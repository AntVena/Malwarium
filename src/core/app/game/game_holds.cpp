// game_holds.cpp: the hold-B gestures' routing. B is a tap/hold pair on several screens:
// the press only arms it (armHoldB), a hold that outlasts the screen's own dwell fires
// the screen's second action, and a release before that is the tap underneath. Both
// halves go to the screen holdFocus names, the way the list gestures go to listFocus's
// (game_listnav.cpp), so a B armed on one screen can never resolve on another.
#include "core/app/game.h"

#include "core/ui/stat_pages.h"   // StatSources: what the STAT reader's index opens over

namespace mal {

Game::HoldFocus Game::holdFocus() const {
    if (nav_ == Nav::Tourney) return HoldFocus::Tourney;
    if (face_ == Face::Hacker) {
        if (nav_ != Nav::Submenu) return HoldFocus::None;
        switch (enteredHackerId()) {
            case HackerSlotId::Shop: return HoldFocus::Shop;
            case HackerSlotId::Vault: return HoldFocus::Vault;
            default: return HoldFocus::None;
        }
    }
    const bool movesTab = loadoutTab_ == LoadoutTab::Moves;
    if (nav_ == Nav::Submenu) {
        switch (enteredId()) {
            case SubmenuId::Items: return HoldFocus::Items;
            case SubmenuId::Stat: return HoldFocus::Stat;
            case SubmenuId::Mods: return movesTab ? HoldFocus::MoveSlots : HoldFocus::None;
            default: return HoldFocus::None;
        }
    }
    if (nav_ == Nav::Detail) {
        switch (enteredId()) {
            case SubmenuId::Cfg: return HoldFocus::Cfg;
            // trainScreen_ keeps its value across a hub page change, so the MODS picker
            // sits at the same Nav::Detail with a stale MovePicker beside it.
            case SubmenuId::Mods:
                return movesTab && trainScreen_ == TrainScreen::MovePicker
                           ? HoldFocus::MovePicker
                           : HoldFocus::None;
            default: return HoldFocus::None;
        }
    }
    return HoldFocus::None;
}

bool Game::tickHoldB() {
    if (!bHeld_) return false;
    const uint32_t held = nowMs_ - bDownMs_;
    bool landed = false;
    switch (holdFocus()) {
        case HoldFocus::None: break;
        case HoldFocus::Cfg: landed = cfgHoldB(held); break;
        case HoldFocus::Items: landed = items_.holdB(*this, held); break;
        case HoldFocus::Stat: landed = stat_.holdB(statSources(), held); break;
        case HoldFocus::MoveSlots: landed = moveSlotsHoldB(held); break;
        case HoldFocus::MovePicker: landed = movePickerHoldB(held); break;
        case HoldFocus::Tourney: landed = tourneyHoldB(held); break;
        case HoldFocus::Shop: landed = rigShop_.holdB(*this, held); break;
        case HoldFocus::Vault: landed = vaultHoldB(held); break;
    }
    if (!landed) return false;
    // Disarmed as it lands, so the release resolves as nothing rather than as the tap.
    bHeld_ = false;
    lastInputMs_ = nowMs_;   // the hold counts as activity (no defocus race)
    return true;
}

void Game::releaseHoldB() {
    switch (holdFocus()) {
        case HoldFocus::None: return;
        case HoldFocus::Cfg: break;   // letting go early abandons the reveal or the commit
        case HoldFocus::Items: items_.releaseB(*this); break;
        case HoldFocus::Stat: stat_.releaseB(*this); break;
        case HoldFocus::MoveSlots: moveSlotsReleaseB(); break;
        case HoldFocus::MovePicker: movePickerReleaseB(); break;
        case HoldFocus::Tourney: tourneyReleaseB(); break;
        case HoldFocus::Shop: rigShop_.releaseB(*this); break;
        case HoldFocus::Vault: vaultReleaseB(); break;
    }
    dirty_ = true;
    lastInputMs_ = nowMs_;
}

}  // namespace mal
