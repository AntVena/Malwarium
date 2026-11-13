// game_holds.cpp: the hold-B gestures' routing. B is a tap/hold pair on several screens:
// the press only arms it (armHoldB), a hold that outlasts the screen's own dwell fires
// the screen's second action, and a release before that is the tap underneath. Both
// halves go to the screen holdTarget names, the way the list gestures go to listTarget's
// (game_listnav.cpp), so a B armed on one screen can never resolve on another.
#include "core/app/game.h"

#include "core/ui/stat_pages.h"   // StatSources: what the STAT reader's index opens over

namespace mal {

template <auto Screen>
Game::HoldTarget Game::screenHold() {
    return {[](Game& g, uint32_t held) { return (g.*Screen).holdB(g, held); },
            [](Game& g) { (g.*Screen).releaseB(g); }};
}

Game::HoldTarget Game::holdTarget() const {
    if (nav_ == Nav::Tourney)
        return {[](Game& g, uint32_t held) { return g.tourneyHoldB(held); },
                [](Game& g) { g.tourneyReleaseB(); }};
    if (face_ == Face::Hacker) {
        if (nav_ != Nav::Submenu) return {};
        switch (enteredHackerId()) {
            case HackerSlotId::Shop: return screenHold<&Game::rigShop_>();
            case HackerSlotId::Vault:
                return {[](Game& g, uint32_t held) { return g.vaultHoldB(held); },
                        [](Game& g) { g.vaultReleaseB(); }};
            default: return {};
        }
    }
    const bool movesTab = loadoutTab_ == LoadoutTab::Moves;
    if (nav_ == Nav::Submenu) {
        switch (enteredId()) {
            case SubmenuId::Items: return screenHold<&Game::items_>();
            case SubmenuId::Stat:
                return {[](Game& g, uint32_t held) { return g.stat_.holdB(g.statSources(), held); },
                        [](Game& g) { g.stat_.releaseB(g); }};
            case SubmenuId::Mods:
                if (!movesTab) return {};
                return {[](Game& g, uint32_t held) { return g.moveSlotsHoldB(held); },
                        [](Game& g) { g.moveSlotsReleaseB(); }};
            default: return {};
        }
    }
    if (nav_ == Nav::Detail) {
        switch (enteredId()) {
            // Letting go early abandons the reveal or the commit, so the release is empty.
            case SubmenuId::Cfg:
                return {[](Game& g, uint32_t held) { return g.cfgHoldB(held); },
                        [](Game&) {}};
            // trainScreen_ keeps its value across a hub page change, so the MODS picker
            // sits at the same Nav::Detail with a stale MovePicker beside it.
            case SubmenuId::Mods:
                if (!movesTab || trainScreen_ != TrainScreen::MovePicker) return {};
                return {[](Game& g, uint32_t held) { return g.movePickerHoldB(held); },
                        [](Game& g) { g.movePickerReleaseB(); }};
            default: return {};
        }
    }
    return {};
}

bool Game::tickHoldB() {
    if (!bHeld_) return false;
    const HoldTarget t = holdTarget();
    if (!t || !t.hold(*this, nowMs_ - bDownMs_)) return false;
    // Disarmed as it lands, so the release resolves as nothing rather than as the tap.
    bHeld_ = false;
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

}  // namespace mal
