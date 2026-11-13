#include "core/app/game.h"

#include <cstring>

#include "core/ui/train_screen.h"

namespace mal {

void Game::onTrainList(const ButtonEvent& ev) {
    // The MOVES slot list. A steps the unlocked slots (skipping the locked ones), B
    // opens the focused slot's move picker, C backs to the LOADOUT hub. An egg has no
    // unlocked slot at all, so `sel` can be 0. MOVES cannot be reached in that state
    // (slotLocked), but the wrap still has to handle it.
    const Stage st = pet_.creature ? pet_.creature->stage : Stage::BootSector;
    const int sel = loadoutSelectableCount(st);
    if (ev.button == Button::A) {
        trainRow_ = sel > 0 ? (trainRow_ + 1) % sel : 0;
    } else if (ev.button == Button::B) {
        // Tap/hold, resolved on release (moveSlotsReleaseB) or on the hold crossing
        // kMoveFilterHoldMs (moveSlotsHoldB): a tap opens the picker, a hold reads the
        // move in the slot.
        if (sel <= 0) return;
        armHoldB();
    } else if (ev.button == Button::C) {
        leaveLoadoutTab();
    }
}

// An empty slot has nothing to read, so there the hold waits for the release instead.
bool Game::moveSlotsHoldB(uint32_t heldMs) {
    if (!pet_.moves.equipped(trainRow_) || heldMs < kMoveFilterHoldMs) return false;
    moveSlot_ = trainRow_;
    moveDetailEquipped_ = true;
    moveProseScroll_ = 0;
    trainScreen_ = TrainScreen::MoveDetail;
    nav_ = Nav::Detail;
    return true;
}

void Game::moveSlotsReleaseB() {
    if (loadoutSelectableCount(pet_.creature ? pet_.creature->stage : Stage::BootSector) <= 0) return;
    trainScreen_ = TrainScreen::MovePicker;
    moveSlot_ = trainRow_;
    movePick_ = 0;
    moveShowAll_ = false;
    moveConfirm_.close();
    moveDetailEquipped_ = false;
    movePendingId_ = nullptr;
    nav_ = Nav::Detail;
}

void Game::onTrainDetail(const ButtonEvent& ev) {
    if (trainScreen_ == TrainScreen::MoveDetail) onMoveDetail(ev);
    else onMovePicker(ev);
}

const MoveDef* Game::focusedPickerMove() const {
    if (moveDetailEquipped_) {
        const char* id = pet_.moves.equipped(moveSlot_);
        return id ? registry_.move(id) : nullptr;
    }
    const auto owned = ownedMoveList(registry_, pet_.moves, slotRequiredKind(moveSlot_),
                                     pet_.creature ? pet_.creature->stage : Stage::BootSector,
                                     pet_.creature ? pet_.creature->line : nullptr, moveSlot_, moveShowAll_);
    if (movePick_ <= 0 || movePick_ - 1 >= static_cast<int>(owned.size())) return nullptr;
    return owned[movePick_ - 1];
}

void Game::onMoveDetail(const ButtonEvent& ev) {
    // Standard A/B/C: A pages the prose when there is more of it than fits (A cycles.
    // Reading on is the natural "next" here), B equips, C returns to the picker with
    // the cursor where it was.
    const MoveDef* m = focusedPickerMove();
    if (moveDetailEquipped_ && (ev.button == Button::C || !m)) {
        moveDetailEquipped_ = false;
        trainScreen_ = TrainScreen::MovePicker;
        nav_ = Nav::Submenu;
        return;
    }
    if (ev.button == Button::C || !m) {
        trainScreen_ = TrainScreen::MovePicker;
        return;
    }
    if (ev.button == Button::A) {
        const int total = moveProseLines(*m);
        const int shown = moveDetailProseLines();
        if (total <= shown) return;                 // it all fits: nothing to page
        moveProseScroll_ += shown;
        if (moveProseScroll_ >= total) moveProseScroll_ = 0;   // wraps to the top
        return;
    }
    if (ev.button != Button::B || moveDetailEquipped_) return;
    // A move the pet hasn't evolved into yet can't be equipped: the page states the
    // gate, and B is inert so the player stays and reads it.
    if (pet_.creature && !moveUnlockedAtStage(*m, pet_.creature->stage)) return;
    // Line-gating: a defensive backstop. A pet can never equip another line's
    // exclusive move (drops already keep them un-owned, so this only fires if a
    // wrong-line move is ever granted some other way).
    if (pet_.creature && !moveAllowedForLine(*m, pet_.creature->line)) return;
    const char* cur = pet_.moves.equipped(moveSlot_);
    if (cur && std::strcmp(cur, m->id) != 0) {
        // Replacing a DIFFERENT move: hand back to the picker, which owns the inline
        // overwrite confirm (drawMovePicker / onMovePicker) rather than duplicating it.
        moveConfirm_.open();
        movePendingId_ = m->id;
        trainScreen_ = TrainScreen::MovePicker;
        return;
    }
    pet_.moves.equip(moveSlot_, m->id);
    markSaveDirty();
    trainScreen_ = TrainScreen::MovePicker;
    nav_ = Nav::Submenu;
}

void Game::onMovePicker(const ButtonEvent& ev) {
    // Mirrors the MODS picker: owned moves + an unequip option; a move lives in
    // one slot (equipping it elsewhere moves it); overwriting a DIFFERENT move
    // opens the inline confirm. The innate default can't be slotted. The slot is
    // type-locked, so the candidate list is already filtered by kind and B never
    // has to reject one. The default (non- showAll) list also drops anything else
    // the pet can't equip here right now (locked by stage, wrong line, already
    // equipped elsewhere): leaving only the options that can be equipped now
    // (ownedMoveList).
    const auto owned = ownedMoveList(registry_, pet_.moves, slotRequiredKind(moveSlot_),
                                     pet_.creature ? pet_.creature->stage : Stage::BootSector,
                                     pet_.creature ? pet_.creature->line : nullptr, moveSlot_, moveShowAll_);
    const int rows = 1 + static_cast<int>(owned.size());   // [0] = unequip

    if (moveConfirm_.isOpen()) {
        // B answers and goes back to the slots; C backs out to the picker.
        const InlineConfirm::Answer answer = moveConfirm_.onButton(ev);
        if (answer == InlineConfirm::Answer::Pending) return;
        if (answer == InlineConfirm::Answer::Yes && movePendingId_) {
            pet_.moves.equip(moveSlot_, movePendingId_);
            markSaveDirty();
        }
        if (answer != InlineConfirm::Answer::Back) nav_ = Nav::Submenu;
        movePendingId_ = nullptr;
        return;
    }

    if (ev.button == Button::A) {
        movePick_ = (movePick_ + 1) % rows;   // steps, and holding it repeats
    } else if (ev.button == Button::B) {
        // B is the tap/hold pair here (the ITEMS list's shape): arm now and resolve on
        // the RELEASE edge (movePickerReleaseB: a short tap unequips or drills into the
        // focused move) or on the hold crossing kMoveFilterHoldMs, where movePickerHoldB
        // reveals the full roster instead.
        armHoldB();
    } else if (ev.button == Button::C) {
        nav_ = Nav::Submenu;
    }
}

// The row set changes size with the roster, so the cursor re-parks at row 0. No unlock
// gate, unlike the ITEMS filter.
bool Game::movePickerHoldB(uint32_t heldMs) {
    if (moveConfirm_.isOpen() || heldMs < kMoveFilterHoldMs) return false;
    moveShowAll_ = !moveShowAll_;
    movePick_ = 0;
    return true;
}

void Game::movePickerReleaseB() {
    if (moveConfirm_.isOpen()) return;
    if (movePick_ == 0) {                              // unequip this slot
        pet_.moves.unequip(moveSlot_);
        markSaveDirty();
        nav_ = Nav::Submenu;
    } else {
        // A real move row opens its own entry, and equipping happens there: the
        // list -> B -> detail -> B -> act shape ITEMS uses. It's what lets the
        // picker stay a comparison view (the aligned readout) and the detail page
        // be the reader, instead of one screen failing at both.
        trainScreen_ = TrainScreen::MoveDetail;
        moveProseScroll_ = 0;
    }
}

void Game::onSimTier(const ButtonEvent& ev) {
    // PRACTISE's only screen, so C leaves the page and returns to the hub row that
    // opened it.
    if (ev.button == Button::A) simTier_ = (simTier_ + 1) % kSimDummyTiers;
    else if (ev.button == Button::B) startSimBattle();
    else if (ev.button == Button::C) leaveLoadoutTab();
}

}  // namespace mal
