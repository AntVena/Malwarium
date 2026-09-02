// game_listnav.cpp — the row-stepping half of the button contract, in one place.
//
// Every list on the device walks its rows the same way, and this unit is why: A steps
// forward, C steps backward, and holding either one keeps going. The per-screen
// handlers (game_items, game_care, game_config, game_hacker, ...) still own what their
// B does and what their rows MEAN; what they no longer each own is a private idea of
// how a cursor moves, which is what let ITEMS wrap while a mis-stepped SHOP row cost a
// full lap of the table.
//
// The gate is Game::listFocus(): it names the list under the cursor right now, or None.
// None is the important half — an inline confirm, a detail page, a modal or a minigame
// all answer None, so C stays the instant tap-to-cancel there and nothing about those
// screens changes. A screen joins the contract by gaining a case here, not by growing
// its own stepping code.

#include "core/app/game.h"
#include "core/app/game_internal.h"

#include "tunables.h"
#include "core/model/loadout.h"
#include "core/ui/cfg_screen.h"
#include "core/ui/expl_screen.h"
#include "core/ui/items_screen.h"
#include "core/ui/mods_screen.h"
#include "core/ui/train_screen.h"

namespace mal {

namespace {

// Step `cur` through `n` slots by `dir`, wrapping, and stop on the first one `ok`
// accepts. Returns `cur` unchanged when nothing qualifies, so a roster with no
// selectable row leaves the cursor where the player last saw it rather than snapping
// it to a row that isn't there. The one walk behind the SHOP's offered rows, the
// ARCADE's offered cabinets and the EXPL ladder's landable rows.
template <typename Ok>
int stepWrapping(int cur, int n, int dir, Ok ok) {
    if (n <= 0) return cur;
    for (int step = 1; step <= n; ++step) {
        const int next = ((cur + dir * step) % n + n) % n;
        if (ok(next)) return next;
    }
    return cur;
}

}  // namespace

Game::ListFocus Game::listFocus() const {
    // The Hacker face first — it reuses Nav::Submenu with its own slot roster, so
    // asking enteredId() there would read a pet submenu that isn't on screen.
    if (face_ == Face::Hacker) {
        if (nav_ != Nav::Submenu) return ListFocus::None;
        switch (enteredHackerId()) {
            case HackerSlotId::Shop: return ListFocus::HackerShop;
            case HackerSlotId::Vault: return ListFocus::HackerVault;
            case HackerSlotId::Peers: return ListFocus::HackerPeers;
            case HackerSlotId::Crew:
                // CREW is four screens behind one slot; only the three that are row
                // lists take the contract. Detail is a prose reader — its A pages the
                // description, which is not a row step.
                switch (crewView_) {
                    case CrewView::Hub: return ListFocus::CrewHub;
                    case CrewView::Team: return ListFocus::CrewTeam;
                    case CrewView::Picker: return ListFocus::CrewPicker;
                    default: return ListFocus::None;
                }
            default: return ListFocus::None;
        }
    }

    if (nav_ == Nav::Submenu) {
        switch (enteredId()) {
            case SubmenuId::Items:
                return itemsScreen_ == ItemsScreen::Picker ? ListFocus::ItemsPicker
                                                           : ListFocus::ItemsList;
            // ARCH is two L2 screens now, the same shape ITEMS is above: the group
            // picker and the rows behind one group. Both are row lists and both want
            // the hold-repeat, so both take the contract — separately, because C's tap
            // means a different thing on each (out to the carousel vs back to the
            // picker) and leaveFocusedList replays exactly that.
            case SubmenuId::Arch:
                return archScreen_ == ArchScreen::Picker ? ListFocus::ArchPicker
                                                         : ListFocus::ArchList;
            case SubmenuId::Cfg: return ListFocus::CfgList;
            case SubmenuId::Maint: return ListFocus::Maint;
            case SubmenuId::Expl: return ListFocus::Expl;
            case SubmenuId::Games: return ListFocus::Arcade;
            case SubmenuId::Mods:
                if (loadoutTab_ == LoadoutTab::Mods) return ListFocus::ModSlots;
                if (loadoutTab_ == LoadoutTab::Hub) return ListFocus::LoadoutHub;
                return ListFocus::None;   // MOVES rests in its L3, never here
            default: return ListFocus::None;
        }
    }

    if (nav_ == Nav::Detail) {
        switch (enteredId()) {
            // The two CFG GROUP screens (DEVICE / RADIO) are row lists wearing an L3's
            // Nav state; every other CFG L3 is a single setting, where A cycles a value
            // and stepping backward would mean nothing.
            case SubmenuId::Cfg:
                return (cfgScreen_ == CfgScreen::Device || cfgScreen_ == CfgScreen::Radio)
                           ? ListFocus::CfgGroup : ListFocus::None;
            case SubmenuId::Mods:
                // An open inline confirm owns A and C (choice + abort), and a detail
                // page is a reader — neither is a list.
                if (loadoutTab_ == LoadoutTab::Mods)
                    return (modConfirm_ || modDetail_) ? ListFocus::None
                                                       : ListFocus::ModPicker;
                if (loadoutTab_ == LoadoutTab::Moves)
                    return (moveConfirm_ || trainScreen_ != TrainScreen::MovePicker)
                               ? ListFocus::None : ListFocus::MovePicker;
                return ListFocus::None;
            default: return ListFocus::None;
        }
    }
    return ListFocus::None;
}

void Game::stepFocusedList(int dir) {
    switch (listFocus()) {
        case ListFocus::None:
            return;

        case ListFocus::ItemsPicker: {
            const int n = static_cast<int>(buildItemPickerRows(registry_, inventory_).size());
            if (n > 0) itemPickRow_ = ((itemPickRow_ + dir) % n + n) % n;
            break;
        }
        case ListFocus::ItemsList: {
            // Section headers are drawn but never landed on, which is exactly what
            // stepSelectableRow walks past — in either direction.
            const auto rows =
                buildInventoryRows(registry_, inventory_, lockoutItemsContext_, itemFilter_);
            if (!rows.empty()) listRow_ = stepSelectableRow(rows, listRow_, dir);
            break;
        }
        case ListFocus::ArchPicker: {
            const int n = archPickRowCount();
            if (n > 0) archPickRow_ = ((archPickRow_ + dir) % n + n) % n;
            break;
        }
        case ListFocus::ArchList: {
            const int n = archRowCount();
            if (n > 0) listRow_ = ((listRow_ + dir) % n + n) % n;
            break;
        }
        case ListFocus::CfgList: {
            const CfgRow* rows = nullptr;
            const int n = cfgRows(rows);
            if (n > 0) listRow_ = ((listRow_ + dir) % n + n) % n;
            break;
        }
        case ListFocus::CfgGroup: {
            const CfgRow* rows = nullptr;
            const int n = cfgGroupRows(cfgScreen_, rows);
            if (n > 0) cfgGroupRow_ = ((cfgGroupRow_ + dir) % n + n) % n;
            break;
        }
        case ListFocus::ModSlots:
            listRow_ = ((listRow_ + dir) % kModSlots + kModSlots) % kModSlots;
            break;
        case ListFocus::LoadoutHub:
            loadoutHubRow_ =
                ((loadoutHubRow_ + dir) % kLoadoutHubRows + kLoadoutHubRows) % kLoadoutHubRows;
            break;
        case ListFocus::ModPicker: {
            const int n = static_cast<int>(
                ownedModList(registry_, loadout_, combatLevel_,
                             pet_ ? pet_->line : nullptr, modSlot_).size());
            if (n > 0) modPick_ = ((modPick_ + dir) % n + n) % n;
            break;
        }
        case ListFocus::MovePicker: {
            // Row 0 is the unequip row, so the walk covers one more than the roster.
            const int n = 1 + static_cast<int>(
                ownedMoveList(registry_, moveLoadout_, slotRequiredKind(moveSlot_),
                              pet_ ? pet_->stage : Stage::BootSector,
                              pet_ ? pet_->line : nullptr, moveSlot_,
                              moveShowAll_).size());
            movePick_ = ((movePick_ + dir) % n + n) % n;
            break;
        }
        case ListFocus::Expl:
            listRow_ = stepWrapping(listRow_, explRowCount(), dir,
                                    [this](int r) { return explRowLandable(r); });
            break;
        case ListFocus::Maint:
            listRow_ ^= 1;            // two rows: either direction is the same toggle
            break;
        case ListFocus::Arcade:
            arcadeRow_ = stepWrapping(arcadeRow_, arcadeGameCount(), dir,
                                      [this](int r) { return arcadeCabinetOffered(r); });
            break;
        case ListFocus::HackerShop:
            hackerShopRow_ = stepWrapping(hackerShopRow_, kRigUpgradeCount, dir,
                                          [this](int r) { return shopRowOffered(r); });
            break;
        case ListFocus::HackerVault: {
            const ItemDef* rows[16];
            const int n = vaultOwnedRows(rows, 16);
            if (n > 0) hackerVaultRow_ = ((hackerVaultRow_ + dir) % n + n) % n;
            break;
        }
        case ListFocus::HackerPeers: {
            PeerRow rows[kPeerMaxRows];
            const int n = peerRows(rows, kPeerMaxRows);
            if (n > 0) peerRow_ = ((peerRow_ + dir) % n + n) % n;
            break;
        }
        case ListFocus::CrewHub:
            crewHubRow_ = ((crewHubRow_ + dir) % kCrewHubRows + kCrewHubRows) % kCrewHubRows;
            break;
        case ListFocus::CrewTeam: {
            const int n = crewTeamCount(crewTeam_);
            if (n > 0) crewTeamRow_ = ((crewTeamRow_ + dir) % n + n) % n;
            break;
        }
        case ListFocus::CrewPicker: {
            CrewNetRow rows[kNetMaxRows];
            const int n = crewNetworkRows(rows, kNetMaxRows);
            if (n > 0) crewNetRow_ = ((crewNetRow_ + dir) % n + n) % n;
            break;
        }
    }
    dirty_ = true;
}

bool Game::listBackStep(const ButtonEvent& ev) {
    if (ev.button != Button::C || listFocus() == ListFocus::None) return false;
    // Arm and wait. C's meaning is decided by how long it is held: released before
    // kListRepeatDelayMs it was a tap and cancels (listBackRelease below), held past it
    // it walks the cursor backward and cancels nothing. Nothing happens on the press
    // itself, which is what lets one button carry both without either guessing.
    cHeld_ = true;
    cDownMs_ = nowMs_;
    listBackStepped_ = false;
    return true;
}

void Game::listBackRelease() {
    if (!cHeld_) return;
    cHeld_ = false;
    // A press that stepped was a hold, and a hold is not a cancel. Only a press that
    // never reached the repeat resolves as the tap it looked like.
    if (listBackStepped_) { listBackStepped_ = false; return; }
    leaveFocusedList();
    lastInputMs_ = nowMs_;
}

bool Game::tickListNav() {
    bool changed = false;

    // A's repeat. Only where A is a plain step: listFocus() is re-read every tick, so a
    // hold that walks the cursor onto a screen change stops with it rather than
    // scrubbing a list the player is no longer looking at.
    if (aHeld_ && listFocus() != ListFocus::None &&
        nowMs_ - aDownMs_ >= kListRepeatDelayMs &&
        nowMs_ - listRepeatLastMs_ >= kListRepeatMs) {
        listRepeatLastMs_ = nowMs_;
        stepFocusedList(+1);
        lastInputMs_ = nowMs_;   // a held step is activity (no defocus mid-scroll)
        changed = true;
    } else if (!aHeld_) {
        listRepeatLastMs_ = nowMs_;   // primed, so the next hold can't burst-catch-up
    }

    // C's backward walk. Crossing kListRepeatDelayMs is what turns the press from a tap
    // into a hold, so the first backward step and the decision "this is not a cancel"
    // are the same event — listBackStepped_ latches it for the release edge.
    if (cHeld_ && listFocus() != ListFocus::None &&
        nowMs_ - cDownMs_ >= kListRepeatDelayMs &&
        nowMs_ - listBackRepeatLastMs_ >= kListRepeatMs) {
        listBackRepeatLastMs_ = nowMs_;
        listBackStepped_ = true;
        stepFocusedList(-1);
        lastInputMs_ = nowMs_;
        changed = true;
    } else if (!cHeld_) {
        listBackRepeatLastMs_ = nowMs_;
    }

    return changed;
}

void Game::leaveFocusedList() {
    // What C's tap used to do on each of these screens, which is what the hold now
    // does. Routed back through the screen's own handler rather than restated here, so
    // a screen that changes where its C goes changes it in one place.
    const ListFocus focus = listFocus();
    if (focus == ListFocus::None) return;
    const ButtonEvent back{Button::C, true, false};
    switch (focus) {
        case ListFocus::ItemsPicker: onItemsPicker(back); break;
        case ListFocus::ItemsList: onItemsList(back); break;
        case ListFocus::ArchPicker: onArchPicker(back); break;
        case ListFocus::ArchList: onArchList(back); break;
        case ListFocus::CfgList: onCfgList(back); break;
        case ListFocus::CfgGroup: onCfgGroup(back); break;
        case ListFocus::ModSlots: onModsList(back); break;
        case ListFocus::LoadoutHub: onLoadoutHub(back); break;
        case ListFocus::ModPicker: onModPicker(back); break;
        case ListFocus::MovePicker: onMovePicker(back); break;
        case ListFocus::Expl: onExplList(back); break;
        case ListFocus::Maint: onMaintList(back); break;
        case ListFocus::Arcade: onArcadeList(back); break;
        case ListFocus::HackerShop: onHackerShop(back); break;
        case ListFocus::HackerVault: onHackerVault(back); break;
        case ListFocus::HackerPeers: onHackerPeers(back); break;
        case ListFocus::CrewHub:
        case ListFocus::CrewTeam:
        case ListFocus::CrewPicker: onHackerCrew(back); break;
        case ListFocus::None: break;
    }
    dirty_ = true;
}

}  // namespace mal
