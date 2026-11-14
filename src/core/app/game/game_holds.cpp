// game_holds.cpp: the hold-B gestures' routing. B is a tap/hold pair on several screens:
// the press only arms it (armHoldB), a hold that outlasts the screen's own dwell fires
// the screen's second action, and a release before that is the tap underneath. Both
// halves go to the screen holdTarget names (its ScreenRow's hold column, game_screens.cpp),
// the way the list gestures go to listTarget's (game_listnav.cpp), so a B armed on one
// screen can never resolve on another.
#include "core/app/game.h"
#include "core/app/game_internal.h"

#include "core/ui/stat_pages.h"   // StatSources: what the STAT reader's index opens over

namespace mal {

Game::HoldTarget Game::holdTarget() const {
    const auto hold = screenRow(nav_).hold;
    return hold ? hold(*this) : HoldTarget{};
}

// The menu tree's L2, answered by the entered slot. The Hacker face reuses
// Nav::Submenu with its own slot roster, so it is asked first: enteredId() there would
// read a pet submenu that isn't on screen.
Game::HoldTarget Game::submenuHoldTarget() const {
    if (face_ == Face::Hacker) {
        switch (enteredHackerId()) {
            case HackerSlotId::Shop: return screenHold<&Game::rigShop_>();
            case HackerSlotId::Vault: return screenHold<&Game::vault_>();
            default: return {};
        }
    }
    switch (enteredId()) {
        case SubmenuId::Items: return screenHold<&Game::items_>();
        case SubmenuId::Stat:
            return {[](Game& g, uint32_t held) { return g.stat_.holdB(g.statSources(), held); },
                    [](Game& g) { g.stat_.releaseB(g); }};
        case SubmenuId::Mods:
            if (loadout_.tab() != LoadoutTab::Moves) return {};
            return {[](Game& g, uint32_t held) { return g.loadout_.moveSlotsHoldB(g, held); },
                    [](Game& g) { g.loadout_.moveSlotsReleaseB(g); }};
        default: return {};
    }
}

// The menu tree's L3, answered by the entered slot. Only the pet face has L3 pages.
Game::HoldTarget Game::detailHoldTarget() const {
    switch (enteredId()) {
        // Letting go early abandons the reveal or the commit, so the release is empty.
        case SubmenuId::Cfg:
            return screenHold<&Game::cfg_>();
        case SubmenuId::Mods:
            if (!loadout_.movePickerUp()) return {};
            return {[](Game& g, uint32_t held) { return g.loadout_.movePickerHoldB(held); },
                    [](Game& g) { g.loadout_.movePickerReleaseB(g); }};
        default: return {};
    }
}

bool Game::tickHoldB() {
    if (!held_.held(Button::B)) return false;
    const HoldTarget t = holdTarget();
    if (!t || !t.hold(*this, held_.heldMs(Button::B, nowMs_))) return false;
    // Disarmed as it lands, so the release resolves as nothing rather than as the tap.
    held_.disarm(Button::B);
    lastInputMs_ = nowMs_;   // the hold counts as activity (no defocus race)
    return true;
}

void Game::releaseHoldB() {
    const HoldTarget t = holdTarget();
    if (!t) return;
    t.release(*this);
    dirty_ = true;
    lastInputMs_ = nowMs_;
}

// A board whose A and C step a cursor repeats the step while either is held (the
// tick reads these, kCryptogramRepeatMs).
void Game::armHoldRepeat(Button b) {
    if (b != Button::B) held_.arm(b, nowMs_);
}

}  // namespace mal
