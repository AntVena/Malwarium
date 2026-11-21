// game_listnav.cpp: the row-stepping half of the button contract, in one place.
//
// Every list on the device walks its rows the same way, and this unit is why: A steps
// forward, C steps backward, and holding either one keeps going. The per-screen
// handlers (game_items, game_care, game_config, game_hacker, ...) still own what their
// B does and what their rows MEAN, but not how a cursor moves: a private idea of that
// per screen is how one list wraps while a mis-stepped row on its sibling costs a full
// lap of the table.
//
// The gate is Game::listTarget(): the list under the cursor right now, as how it steps and
// what C's tap does there, or an empty target. An inline confirm, a detail page, a modal
// or a minigame answer empty, so C stays an instant cancel on them. A screen joins the
// contract through its ScreenRow's list column (game_screens.cpp), or the menu tree's
// per-slot answer below, and steps with stepRow (core/ui/layout.h) rather than its own
// arithmetic.

#include "core/app/game.h"
#include "core/app/game_internal.h"

#include "tunables.h"
#include "core/ui/cfg_screen.h"
#include "core/ui/expl_screen.h"
#include "core/ui/layout.h"
#include "core/ui/mods_screen.h"
#include "core/ui/stat_pages.h"   // StatSources: what the STAT index steps over
#include "core/ui/train_screen.h"

namespace mal {

Game::ListTarget Game::listTarget() const {
    const auto list = screenRow(nav_).list;
    return list ? list(*this) : ListTarget{};
}

// The menu tree's L2, answered by the entered slot. The Hacker face first: it reuses
// Nav::Submenu with its own slot roster, so asking enteredId() there would read a pet
// submenu that isn't on screen.
Game::ListTarget Game::submenuListTarget() const {
    if (face_ == Face::Hacker) {
        switch (enteredHackerId()) {
            case HackerSlotId::Shop:
                // The service info page is a reader with nothing selected, so it takes
                // no row contract and C stays an instant back.
                if (rigShop_.view() == ShopView::ServiceInfo) return {};
                return screenList<&Game::rigShop_>();
            case HackerSlotId::Vault: return screenList<&Game::vault_>();
            case HackerSlotId::Peers: return screenList<&Game::peers_>();
            case HackerSlotId::Merge: return screenList<&Game::merge_>();
            case HackerSlotId::Link:
                if (!link_.listUp(*this)) return {};
                return screenList<&Game::link_>();
            // CREW's Detail is a prose reader: its A pages the description, which is
            // not a row step.
            case HackerSlotId::Crew:
                if (crew_.view() == CrewView::Detail) return {};
                return screenList<&Game::crew_>();
            default: return {};
        }
    }
    switch (enteredId()) {
        // Only the INDEX is a list. The six pages behind it are readers: nothing is
        // selected, A turns the page, and C leaves as on every reader.
        case SubmenuId::Stat:
            if (stat_.screen() != StatScreen::Index) return {};
            return {[](Game& g, int dir) { g.stat_.stepIndex(g.statSources(), dir); },
                    [](Game& g) { g.stat_.onButton(g, kTapC); }};
        case SubmenuId::Items:
            return screenList<&Game::items_>();
        // ARCH's picker and group rows are both lists; the screen knows which is up.
        case SubmenuId::Arch: return screenList<&Game::arch_>();
        case SubmenuId::Cfg: return screenList<&Game::cfg_>();
        case SubmenuId::Maint:
            return screenList<&Game::maintScreen_>();
        case SubmenuId::Expl: return screenList<&Game::expl_>();
        case SubmenuId::Games:
            return {[](Game& g, int dir) { g.arcadeScreen_.stepList(g, dir); },
                    [](Game& g) { g.arcadeScreen_.onList(g, kTapC); }};
        case SubmenuId::Mods: return screenList<&Game::loadout_>();
        default: return {};
    }
}

Game::ListTarget Game::detailListTarget() const {
    switch (enteredId()) {
        // The two CFG GROUP screens (DEVICE / RADIO) are row lists wearing an L3's Nav
        // state; every other CFG L3 is a single setting, where A cycles a value and
        // stepping backward would mean nothing.
        case SubmenuId::Cfg:
            if (!cfg_.detailIsList()) return {};
            return {[](Game& g, int dir) { g.cfg_.stepGroup(dir); },
                    [](Game& g) { g.cfg_.onDetail(g, kTapC); }};
        case SubmenuId::Mods:
            if (!loadout_.detailIsList()) return {};
            return {[](Game& g, int dir) { g.loadout_.stepDetail(g, dir); },
                    [](Game& g) { g.loadout_.onDetail(g, kTapC); }};
        default: return {};
    }
}

bool Game::listBackStep(const ButtonEvent& ev) {
    if (ev.button != Button::C || !listTarget()) return false;
    // Arm and wait. C's meaning is decided by how long it is held: released before
    // kListRepeatDelayMs it was a tap and cancels (listBackRelease below), held past it
    // it walks the cursor backward and cancels nothing. Nothing happens on the press
    // itself, which is what lets one button carry both without either guessing.
    held_.armListBack(nowMs_);
    return true;
}

void Game::listBackRelease() {
    if (!held_.held(Button::C)) return;
    held_.disarm(Button::C);
    // A press that stepped was a hold, and a hold is not a cancel. Only a press that
    // never reached the repeat resolves as the tap it looked like.
    if (held_.takeBackStepped()) return;
    if (const ListTarget t = listTarget()) {
        t.leave(*this);
        dirty_ = true;
    }
    lastInputMs_ = nowMs_;
}

bool Game::tickListNav() {
    bool changed = false;

    // A's repeat. Only where A is a plain step: listTarget() is re-read every tick, so a
    // hold that walks the cursor onto a screen change stops with it rather than
    // scrubbing a list the player is no longer looking at.
    const ListTarget list = listTarget();
    held_.primeListRepeat(nowMs_);
    if (list && held_.listRepeatDue(Button::A, nowMs_)) {
        list.step(*this, +1);
        lastInputMs_ = nowMs_;   // a held step is activity (no defocus mid-scroll)
        changed = true;
    }

    // C's backward walk. Its first step is also the decision that this press is not a
    // cancel, which HeldButtons latches for the release edge.
    if (list && held_.listRepeatDue(Button::C, nowMs_)) {
        list.step(*this, -1);
        lastInputMs_ = nowMs_;
        changed = true;
    }

    if (changed) dirty_ = true;
    return changed;
}

}  // namespace mal
