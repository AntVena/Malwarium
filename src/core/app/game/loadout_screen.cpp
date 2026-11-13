#include "core/app/game/loadout_screen.h"

#include <cstring>

#include "tunables.h"
#include "core/content/registry.h"
#include "core/model/combat.h"   // kSimDummyTiers
#include "core/model/pet.h"
#include "core/ui/layout.h"
#include "core/ui/mods_screen.h"
#include "core/ui/train_screen.h"

namespace mal {

namespace {
Stage petStage(const Pet& p) { return p.creature ? p.creature->stage : Stage::BootSector; }
const char* petLine(const Pet& p) { return p.creature ? p.creature->line : nullptr; }

// The rows the move picker draws: what fits the slot now, or the full roster.
std::vector<const MoveDef*> moveRows(const LoadoutHost& host, int slot, bool showAll) {
    const Pet& pet = host.petRecord();
    return ownedMoveList(host.content(), pet.moves, host.slotRequiredKind(slot), petStage(pet),
                         petLine(pet), slot, showAll);
}

// The rows the mod picker draws, fittable mods first and locked ones after, so its
// cursor indexes what the player is looking at.
std::vector<const ModDef*> modRows(const LoadoutHost& host, int slot) {
    const Pet& pet = host.petRecord();
    return ownedModList(host.content(), host.modHoldings(), pet.level, petLine(pet), slot);
}
}  // namespace

int LoadoutScreen::modPickerRows(const LoadoutHost& host) const {
    return static_cast<int>(modRows(host, modSlot_).size());
}

int LoadoutScreen::movePickerRows(const LoadoutHost& host) const {
    return 1 + static_cast<int>(moveRows(host, moveSlot_, moveShowAll_).size());
}

// --- L2: the hub and the slot lists -------------------------------------------

void LoadoutScreen::openTab(LoadoutHost& host, LoadoutTab tab) {
    tab_ = tab;
    switch (tab) {
        case LoadoutTab::Mods:
            modRow_ = 0;
            mods_.close();
            modDetailId_ = nullptr;
            host.closeDetail();
            break;
        case LoadoutTab::Moves:
            moveRow_ = 0;
            movePage_ = MovePage::Picker;
            moves_.close();
            host.closeDetail();
            break;
        case LoadoutTab::Practise:
            // The dummy fight has no list of its own. The hub row is its L2, so it opens
            // straight into the tier pick.
            simTier_ = 0;
            host.openDetail();
            break;
        case LoadoutTab::Hub:
            host.closeDetail();
            break;
    }
}

void LoadoutScreen::leaveTab(LoadoutHost& host) {
    tab_ = LoadoutTab::Hub;
    host.closeDetail();
}

void LoadoutScreen::onButton(LoadoutHost& host, const ButtonEvent& ev) {
    switch (tab_) {
        case LoadoutTab::Mods:  onModSlots(host, ev); break;
        case LoadoutTab::Moves: onMoveSlots(host, ev); break;
        default:                onHub(host, ev); break;
    }
}

void LoadoutScreen::stepList(const LoadoutHost&, int dir) {
    if (tab_ == LoadoutTab::Mods) modRow_ = stepRow(modRow_, kModSlots, dir);
    else if (tab_ == LoadoutTab::Hub) hubRow_ = stepRow(hubRow_, kLoadoutHubRows, dir);
}

void LoadoutScreen::onHub(LoadoutHost& host, const ButtonEvent& ev) {
    if (ev.button == Button::A) {
        hubRow_ = stepRow(hubRow_, kLoadoutHubRows, +1);
    } else if (ev.button == Button::B) {
        // Row order is the hub's draw order, which is LoadoutTab's own order past Hub.
        openTab(host, static_cast<LoadoutTab>(hubRow_ + 1));
    } else if (ev.button == Button::C) {
        host.leaveToCarousel();
    }
}

void LoadoutScreen::onModSlots(LoadoutHost& host, const ButtonEvent& ev) {
    if (ev.button == Button::A) {
        modRow_ = stepRow(modRow_, kModSlots, +1);
    } else if (ev.button == Button::B) {                 // open the slot's picker
        modSlot_ = modRow_;
        modPick_ = 0;
        mods_.close();
        modDetailId_ = nullptr;
        host.openDetail();
    } else if (ev.button == Button::C) {
        leaveTab(host);
    }
}

void LoadoutScreen::onMoveSlots(LoadoutHost& host, const ButtonEvent& ev) {
    // A steps the unlocked slots, B is the tap/hold pair (moveSlotsReleaseB,
    // moveSlotsHoldB), C backs to the hub. An egg has no unlocked slot, so the count can
    // be 0; MOVES cannot be reached then, but the wrap still handles it.
    const int sel = loadoutSelectableCount(petStage(host.petRecord()));
    if (ev.button == Button::A) {
        moveRow_ = stepRow(moveRow_, sel, +1);
    } else if (ev.button == Button::B) {
        if (sel > 0) host.armHoldB();
    } else if (ev.button == Button::C) {
        leaveTab(host);
    }
}

// An empty slot has nothing to read, so there the hold waits for the release instead.
bool LoadoutScreen::moveSlotsHoldB(LoadoutHost& host, uint32_t heldMs) {
    if (!host.petRecord().moves.equipped(moveRow_) || heldMs < kMoveFilterHoldMs) return false;
    moveSlot_ = moveRow_;
    moveDetailEquipped_ = true;
    moveProseScroll_ = 0;
    movePage_ = MovePage::Detail;
    host.openDetail();
    return true;
}

void LoadoutScreen::moveSlotsReleaseB(LoadoutHost& host) {
    if (loadoutSelectableCount(petStage(host.petRecord())) <= 0) return;
    movePage_ = MovePage::Picker;
    moveSlot_ = moveRow_;
    movePick_ = 0;
    moveShowAll_ = false;
    moves_.close();
    moveDetailEquipped_ = false;
    host.openDetail();
}

// --- L3: the pickers, the detail pages, the tier ------------------------------

void LoadoutScreen::onDetail(LoadoutHost& host, const ButtonEvent& ev) {
    switch (tab_) {
        case LoadoutTab::Practise: onSimTier(host, ev); break;
        case LoadoutTab::Moves:
            if (movePage_ == MovePage::Detail) onMoveDetail(host, ev);
            else onMovePicker(host, ev);
            break;
        default:
            if (modDetailId_) onModDetail(host, ev);
            else onModPicker(host, ev);
            break;
    }
}

bool LoadoutScreen::detailIsList() const {
    // An open confirm owns A and C, and a detail page is a reader.
    if (tab_ == LoadoutTab::Mods) return !mods_.confirm.isOpen() && !modDetailId_;
    if (tab_ == LoadoutTab::Moves) return !moves_.confirm.isOpen() && movePage_ == MovePage::Picker;
    return false;
}

void LoadoutScreen::stepDetail(const LoadoutHost& host, int dir) {
    if (tab_ == LoadoutTab::Mods) modPick_ = stepRow(modPick_, modPickerRows(host), dir);
    else if (tab_ == LoadoutTab::Moves) movePick_ = stepRow(movePick_, movePickerRows(host), dir);
}

void LoadoutScreen::onModPicker(LoadoutHost& host, const ButtonEvent& ev) {
    const auto owned = modRows(host, modSlot_);
    const int rows = static_cast<int>(owned.size());

    if (mods_.confirm.isOpen()) {
        const InlineConfirm::Answer answer = mods_.confirm.onButton(ev);
        if (answer == InlineConfirm::Answer::Pending) return;
        // B answers and goes back to the slots; C backs out to the picker. The install
        // consumes the new mod and discards the one already in the slot.
        if (answer != InlineConfirm::Answer::Back) {
            if (answer == InlineConfirm::Answer::Yes && mods_.pendingId)
                host.installMod(modSlot_, mods_.pendingId);
            host.closeDetail();
        }
        mods_.pendingId = nullptr;
        return;
    }

    if (ev.button == Button::A) {
        modPick_ = stepRow(modPick_, rows, +1);
    } else if (ev.button == Button::B) {
        if (rows > 0) {                                   // inspect before equipping
            if (modPick_ >= rows) modPick_ = rows - 1;
            modDetailId_ = owned[modPick_]->id;
        }
    } else if (ev.button == Button::C) {
        host.closeDetail();
    }
}

// The mod detail (the ITEMS list -> detail -> use shape): B equips, C backs to the
// picker. An empty slot installs straight away; a slot holding a different mod hands off
// to the picker's overwrite confirm, since the overwrite cannot be taken back; a slot
// already holding this mod is a no-op, so no spare is burned for no change.
void LoadoutScreen::onModDetail(LoadoutHost& host, const ButtonEvent& ev) {
    if (ev.button == Button::C) { modDetailId_ = nullptr; return; }
    if (ev.button != Button::B) return;
    const char* id = modDetailId_;
    // The page already shows why a refused mod cannot go in.
    if (!host.modEquippable(modSlot_, id)) return;
    const char* cur = host.petRecord().mods.equipped(modSlot_);
    modDetailId_ = nullptr;
    if (cur && std::strcmp(cur, id) != 0) {
        mods_.open(id);                                   // the confirm shows on the picker
        return;
    }
    if (!cur) host.installMod(modSlot_, id);
    host.closeDetail();
}

void LoadoutScreen::onMovePicker(LoadoutHost& host, const ButtonEvent& ev) {
    // The MODS picker's shape: owned moves and an unequip row; a move lives in one slot;
    // overwriting a different move opens the confirm. The slot is type-locked, so the
    // rows are already filtered by kind, and the default view also drops what the pet
    // cannot equip here right now (ownedMoveList).
    if (moves_.confirm.isOpen()) {
        // B answers and goes back to the slots; C backs out to the picker.
        const InlineConfirm::Answer answer = moves_.confirm.onButton(ev);
        if (answer == InlineConfirm::Answer::Pending) return;
        if (answer == InlineConfirm::Answer::Yes && moves_.pendingId)
            host.equipMove(moveSlot_, moves_.pendingId);
        if (answer != InlineConfirm::Answer::Back) host.closeDetail();
        moves_.pendingId = nullptr;
        return;
    }

    if (ev.button == Button::A) {
        movePick_ = stepRow(movePick_, movePickerRows(host), +1);
    } else if (ev.button == Button::B) {
        // The ITEMS list's tap/hold pair: the release unequips or reads the focused move
        // (movePickerReleaseB), a hold past kMoveFilterHoldMs shows the full roster.
        host.armHoldB();
    } else if (ev.button == Button::C) {
        host.closeDetail();
    }
}

// The row set changes size with the roster, so the cursor re-parks at row 0.
bool LoadoutScreen::movePickerHoldB(uint32_t heldMs) {
    if (moves_.confirm.isOpen() || heldMs < kMoveFilterHoldMs) return false;
    moveShowAll_ = !moveShowAll_;
    movePick_ = 0;
    return true;
}

void LoadoutScreen::movePickerReleaseB(LoadoutHost& host) {
    if (moves_.confirm.isOpen()) return;
    if (movePick_ == 0) {                              // unequip this slot
        host.unequipMove(moveSlot_);
        host.closeDetail();
    } else {
        // A real move row opens its own page, and equipping happens there, so the
        // picker stays a comparison view and the detail page is the reader.
        movePage_ = MovePage::Detail;
        moveProseScroll_ = 0;
    }
}

const MoveDef* LoadoutScreen::focusedMove(const LoadoutHost& host) const {
    if (moveDetailEquipped_) {
        const char* id = host.petRecord().moves.equipped(moveSlot_);
        return id ? host.content().move(id) : nullptr;
    }
    const auto owned = moveRows(host, moveSlot_, moveShowAll_);
    if (movePick_ <= 0 || movePick_ - 1 >= static_cast<int>(owned.size())) return nullptr;
    return owned[movePick_ - 1];
}

void LoadoutScreen::onMoveDetail(LoadoutHost& host, const ButtonEvent& ev) {
    // A pages the prose when there is more than fits, B equips, C returns to the picker
    // with the cursor where it was.
    const MoveDef* m = focusedMove(host);
    if (moveDetailEquipped_ && (ev.button == Button::C || !m)) {
        moveDetailEquipped_ = false;
        movePage_ = MovePage::Picker;
        host.closeDetail();
        return;
    }
    if (ev.button == Button::C || !m) {
        movePage_ = MovePage::Picker;
        return;
    }
    if (ev.button == Button::A) {
        const int total = moveProseLines(*m);
        const int shown = moveDetailProseLines();
        if (total <= shown) return;                 // it all fits: nothing to page
        moveProseScroll_ = nextWindowTop(moveProseScroll_, shown, total);
        return;
    }
    if (ev.button != Button::B || moveDetailEquipped_) return;
    const Pet& pet = host.petRecord();
    // A move the pet has not evolved into yet cannot be equipped: the page states the
    // gate and B is inert. The line gate is a backstop, since drops already keep another
    // line's exclusive moves un-owned.
    if (pet.creature && !moveUnlockedAtStage(*m, pet.creature->stage)) return;
    if (pet.creature && !moveAllowedForLine(*m, pet.creature->line)) return;
    movePage_ = MovePage::Picker;
    const char* cur = pet.moves.equipped(moveSlot_);
    if (cur && std::strcmp(cur, m->id) != 0) {
        moves_.open(m->id);                          // the picker owns the confirm
        return;
    }
    host.equipMove(moveSlot_, m->id);
    host.closeDetail();
}

void LoadoutScreen::onSimTier(LoadoutHost& host, const ButtonEvent& ev) {
    // PRACTISE's only page, so C returns to the hub row that opened it.
    if (ev.button == Button::A) simTier_ = stepRow(simTier_, kSimDummyTiers, +1);
    else if (ev.button == Button::B) host.startSimBattle(simTier_);
    else if (ev.button == Button::C) leaveTab(host);
}

// --- Drawing ------------------------------------------------------------------

void LoadoutScreen::draw(Framebuffer& fb, const LoadoutHost& host) const {
    const Pet& pet = host.petRecord();
    if (tab_ == LoadoutTab::Mods) {
        drawModsList(fb, host.content(), host.modHoldings(), modRow_, host.screenBeat());
    } else if (tab_ == LoadoutTab::Moves) {
        // Each slot's stamped required kind, for the row tags.
        MoveDef::Kind sk[kMaxMoveSlots];
        for (int i = 0; i < kMaxMoveSlots; ++i) sk[i] = host.slotRequiredKind(i);
        LoadoutView v;
        v.stage = petStage(pet);
        v.cursor = moveRow_;
        v.slotKinds = sk;
        v.beat = host.screenBeat();
        drawLoadout(fb, host.content(), pet.moves, v);
    } else {
        LoadoutHubView v;
        v.stage = petStage(pet);
        v.cursor = hubRow_;
        v.beat = host.screenBeat();
        drawLoadoutHub(fb, host.modHoldings(), pet.moves, v);
    }
}

void LoadoutScreen::drawDetail(Framebuffer& fb, const LoadoutHost& host) const {
    if (tab_ == LoadoutTab::Practise) drawSimTier(fb, simTier_, kSimDummyTiers);
    else if (tab_ == LoadoutTab::Moves) drawMovePage(fb, host);
    else drawModPage(fb, host);
}

void LoadoutScreen::drawModPage(Framebuffer& fb, const LoadoutHost& host) const {
    const Pet& pet = host.petRecord();
    if (const ModDef* md = modDetailId_ ? host.content().mod(modDetailId_) : nullptr) {
        const char* cur = pet.mods.equipped(modSlot_);
        ModDetailView dv;
        dv.slot = modSlot_;
        dv.equippedHere = cur && std::strcmp(cur, modDetailId_) == 0;
        dv.reqLevel = modEquipLevel(*md);
        dv.petLevel = pet.level;
        dv.petLine = petLine(pet);
        dv.storageCap = host.modStorageCap();
        dv.beat = host.screenBeat();
        drawModDetail(fb, host.content(), host.modHoldings(), *md, dv);
        return;
    }
    ModPickerView pv;
    pv.slot = modSlot_;
    pv.pick = modPick_;
    pv.petLevel = pet.level;
    pv.petLine = petLine(pet);
    pv.confirmActive = mods_.confirm.isOpen();
    pv.confirmChoice = mods_.confirm.choice();
    pv.pendingId = mods_.pendingId;
    pv.beat = host.screenBeat();
    drawModPicker(fb, host.content(), host.modHoldings(), pv);
}

void LoadoutScreen::drawMovePage(Framebuffer& fb, const LoadoutHost& host) const {
    const Pet& pet = host.petRecord();
    // A detail whose move has gone falls back to the picker rather than an empty page.
    if (movePage_ == MovePage::Detail) {
        if (const MoveDef* m = focusedMove(host)) {
            const char* eq = pet.moves.equipped(moveSlot_);
            MoveDetailView dv;
            dv.petStage = petStage(pet);
            dv.equippedHere = eq && std::strcmp(eq, m->id) == 0;
            dv.proseScroll = moveProseScroll_;
            dv.beat = host.screenBeat();
            drawMoveDetail(fb, host.content(), *m, dv);
            return;
        }
    }
    MovePickerView v;
    v.slot = moveSlot_;
    v.requiredKind = host.slotRequiredKind(moveSlot_);
    v.petStage = petStage(pet);
    v.petLine = petLine(pet);
    v.pick = movePick_;
    v.showAll = moveShowAll_;
    v.confirmActive = moves_.confirm.isOpen();
    v.confirmChoice = moves_.confirm.choice();
    v.pendingId = moves_.pendingId;
    v.beat = host.screenBeat();
    drawMovePicker(fb, host.content(), pet.moves, v);
}

}  // namespace mal
