#include "core/app/game.h"

#include <cstring>

#include "core/ui/train_screen.h"

namespace mal {

void Game::onTrainList(const ButtonEvent& ev) {
    // The MOVES slot list. A steps the unlocked slots (skipping the locked ones), B
    // opens the focused slot's move picker, C backs to the LOADOUT hub. An egg has no
    // unlocked slot at all, so `sel` can be 0. MOVES cannot be reached in that state
    // (slotLocked), but the wrap still has to handle it.
    const Stage st = pet_ ? pet_->stage : Stage::BootSector;
    const int sel = loadoutSelectableCount(st);
    if (ev.button == Button::A) {
        trainRow_ = sel > 0 ? (trainRow_ + 1) % sel : 0;
    } else if (ev.button == Button::B) {
        // Tap/hold, resolved on release (trainListReleaseB) or on the hold crossing
        // kMoveFilterHoldMs (tick): a tap opens the picker, a hold reads the move in the slot.
        if (sel <= 0) return;
        bHeld_ = true;
        bDownMs_ = nowMs_;
    } else if (ev.button == Button::C) {
        leaveLoadoutTab();
    }
}

void Game::trainListReleaseB() {
    if (!(nav_ == Nav::Submenu && enteredId() == SubmenuId::Mods &&
          loadoutTab_ == LoadoutTab::Moves))
        return;
    if (loadoutSelectableCount(pet_ ? pet_->stage : Stage::BootSector) <= 0) return;
    trainScreen_ = TrainScreen::MovePicker;
    moveSlot_ = trainRow_;
    movePick_ = 0;
    moveShowAll_ = false;
    moveConfirm_ = false;
    moveDetailEquipped_ = false;
    movePendingId_ = nullptr;
    nav_ = Nav::Detail;
    dirty_ = true;
    lastInputMs_ = nowMs_;
}

void Game::onTrainDetail(const ButtonEvent& ev) {
    if (trainScreen_ == TrainScreen::MoveDetail) onMoveDetail(ev);
    else onMovePicker(ev);
}

const MoveDef* Game::focusedPickerMove() const {
    if (moveDetailEquipped_) {
        const char* id = moveLoadout_.equipped(moveSlot_);
        return id ? registry_.move(id) : nullptr;
    }
    const auto owned = ownedMoveList(registry_, moveLoadout_, slotRequiredKind(moveSlot_),
                                     pet_ ? pet_->stage : Stage::BootSector,
                                     pet_ ? pet_->line : nullptr, moveSlot_, moveShowAll_);
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
    if (pet_ && !moveUnlockedAtStage(*m, pet_->stage)) return;
    // Line-gating: a defensive backstop. A pet can never equip another line's
    // exclusive move (drops already keep them un-owned, so this only fires if a
    // wrong-line move is ever granted some other way).
    if (pet_ && !moveAllowedForLine(*m, pet_->line)) return;
    const char* cur = moveLoadout_.equipped(moveSlot_);
    if (cur && std::strcmp(cur, m->id) != 0) {
        // Replacing a DIFFERENT move: hand back to the picker, which owns the inline
        // overwrite confirm (drawMovePicker / onMovePicker) rather than duplicating it.
        moveConfirm_ = true;
        moveConfirmChoice_ = 0;
        movePendingId_ = m->id;
        trainScreen_ = TrainScreen::MovePicker;
        return;
    }
    moveLoadout_.equip(moveSlot_, m->id);
    noteCareSignal(DominantSignal::Training);
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
    const auto owned = ownedMoveList(registry_, moveLoadout_, slotRequiredKind(moveSlot_),
                                     pet_ ? pet_->stage : Stage::BootSector,
                                     pet_ ? pet_->line : nullptr, moveSlot_, moveShowAll_);
    const int rows = 1 + static_cast<int>(owned.size());   // [0] = unequip

    if (moveConfirm_) {
        if (ev.button == Button::A) moveConfirmChoice_ ^= 1;
        else if (ev.button == Button::B) {
            if (moveConfirmChoice_ == 1 && movePendingId_) {
                moveLoadout_.equip(moveSlot_, movePendingId_);
                noteCareSignal(DominantSignal::Training);
                markSaveDirty();
            }
            moveConfirm_ = false;
            movePendingId_ = nullptr;
            nav_ = Nav::Submenu;
        } else if (ev.button == Button::C) {
            moveConfirm_ = false;
            movePendingId_ = nullptr;
        }
        return;
    }

    if (ev.button == Button::A) {
        movePick_ = (movePick_ + 1) % rows;   // steps, and holding it repeats
    } else if (ev.button == Button::B) {
        // B is the tap/hold pair here (the ITEMS list's shape): arm now and resolve on
        // the RELEASE edge (moveFilterReleaseB. A short tap unequips or drills into
        // the focused move) or on the hold crossing kMoveFilterHoldMs, where tick()
        // reveals the full roster instead.
        bHeld_ = true;
        bDownMs_ = nowMs_;
    } else if (ev.button == Button::C) {
        nav_ = Nav::Submenu;
    }
}

void Game::moveFilterReleaseB() {
    // Screen check only: onButton owns bHeld_ (see itemFilterReleaseB).
    if (!(nav_ == Nav::Detail && loadoutTab_ == LoadoutTab::Moves &&
          trainScreen_ == TrainScreen::MovePicker && !moveConfirm_))
        return;   // bHeld_ was armed elsewhere (the ITEMS / VAULT / CFG holds): no-op
    if (movePick_ == 0) {                              // unequip this slot
        moveLoadout_.unequip(moveSlot_);
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
    dirty_ = true;
    lastInputMs_ = nowMs_;
}

void Game::onSimTier(const ButtonEvent& ev) {
    // PRACTISE's only screen, so C leaves the page and returns to the hub row that
    // opened it.
    if (ev.button == Button::A) simTier_ = (simTier_ + 1) % kSimDummyTiers;
    else if (ev.button == Button::B) startSimBattle();
    else if (ev.button == Button::C) leaveLoadoutTab();
}

}  // namespace mal
