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
// contract by gaining one entry there, and steps with stepRow (core/ui/layout.h) rather
// than its own arithmetic.

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

namespace {
constexpr ButtonEvent kTapC{Button::C, true, false};
}  // namespace

template <auto Screen>
Game::ListTarget Game::screenList() {
    return {[](Game& g, int dir) { (g.*Screen).stepList(g, dir); },
            [](Game& g) { (g.*Screen).onButton(g, kTapC); }};
}

Game::ListTarget Game::listTarget() const {
    // The Hacker face first: it reuses Nav::Submenu with its own slot roster, so
    // asking enteredId() there would read a pet submenu that isn't on screen.
    if (face_ == Face::Hacker) {
        if (nav_ != Nav::Submenu) return {};
        switch (enteredHackerId()) {
            case HackerSlotId::Shop:
                // The service info page is a reader with nothing selected, so it takes
                // no row contract and C stays an instant back.
                if (rigShop_.view() == ShopView::ServiceInfo) return {};
                return screenList<&Game::rigShop_>();
            case HackerSlotId::Vault: return screenList<&Game::vault_>();
            case HackerSlotId::Peers: return screenList<&Game::peers_>();
            // CREW's Detail is a prose reader: its A pages the description, which is
            // not a row step.
            case HackerSlotId::Crew:
                if (crew_.view() == CrewView::Detail) return {};
                return screenList<&Game::crew_>();
            default: return {};
        }
    }
    if (nav_ == Nav::Submenu) return submenuListTarget();
    if (nav_ == Nav::Detail) return detailListTarget();

    // The CHAPTERS archive is a row list with its own Nav. It grows with the ladder,
    // so it takes the shared cursor and its hold-repeat. The chapter READER beside
    // it does not: nothing on that page is selected, its B is "next window" rather
    // than a row step, and its C is the tap-to-leave every other reader's is.
    if (nav_ == Nav::StoryArchive)
        return screenList<&Game::chapters_>();
    return {};
}

Game::ListTarget Game::submenuListTarget() const {
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
            return {[](Game& g, int dir) { g.listRow_ = stepRow(g.listRow_, 2, dir); },
                    [](Game& g) { g.onMaintList(kTapC); }};
        case SubmenuId::Expl:
            // TWO cursor spaces behind one list (see game.h's explCat_): the ACTIVITY
            // PICKER's rows at EXPL's own top level, and the ladder's rows inside an open
            // category. Both walk the same way; what differs is how long the list is and
            // which rows the cursor stops on.
            return {[](Game& g, int dir) {
                        if (g.explCat_ == ExplCat::None)
                            g.listRow_ = stepRow(g.listRow_, kExplCatRows, dir,
                                                 [&g](int r) { return g.explCatLandable(r); });
                        else
                            g.listRow_ = stepRow(g.listRow_, explRowCount(), dir,
                                                 [&g](int r) { return g.explRowLandable(r); });
                    },
                    [](Game& g) { g.onExplList(kTapC); }};
        case SubmenuId::Games:
            return {[](Game& g, int dir) { g.arcadeScreen_.stepList(g, dir); },
                    [](Game& g) { g.arcadeScreen_.onList(g, kTapC); }};
        case SubmenuId::Mods:
            if (loadoutTab_ == LoadoutTab::Mods)
                return {[](Game& g, int dir) { g.listRow_ = stepRow(g.listRow_, kModSlots, dir); },
                        [](Game& g) { g.onModsList(kTapC); }};
            if (loadoutTab_ == LoadoutTab::Hub)
                return {[](Game& g, int dir) {
                            g.loadoutHubRow_ = stepRow(g.loadoutHubRow_, kLoadoutHubRows, dir);
                        },
                        [](Game& g) { g.onLoadoutHub(kTapC); }};
            return {};   // MOVES rests in its L3, never here
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
        // An open inline confirm owns A and C (choice and abort), and a detail page is a
        // reader: neither is a list.
        case SubmenuId::Mods:
            if (loadoutTab_ == LoadoutTab::Mods) {
                if (modConfirm_.isOpen() || modDetail_) return {};
                return {[](Game& g, int dir) {
                            const int n = static_cast<int>(
                                ownedModList(g.registry_, g.modHoldings(), g.pet_.level,
                                             g.pet_.creature ? g.pet_.creature->line : nullptr,
                                             g.modSlot_).size());
                            g.modPick_ = stepRow(g.modPick_, n, dir);
                        },
                        [](Game& g) { g.onModPicker(kTapC); }};
            }
            if (loadoutTab_ == LoadoutTab::Moves) {
                if (moveConfirm_.isOpen() || trainScreen_ != TrainScreen::MovePicker) return {};
                return {[](Game& g, int dir) {
                            // Row 0 is the unequip row, so the walk covers one more than
                            // the roster.
                            const auto* c = g.pet_.creature;
                            const int n = 1 + static_cast<int>(
                                ownedMoveList(g.registry_, g.pet_.moves,
                                              g.slotRequiredKind(g.moveSlot_),
                                              c ? c->stage : Stage::BootSector,
                                              c ? c->line : nullptr, g.moveSlot_,
                                              g.moveShowAll_).size());
                            g.movePick_ = stepRow(g.movePick_, n, dir);
                        },
                        [](Game& g) { g.onMovePicker(kTapC); }};
            }
            return {};
        default: return {};
    }
}

bool Game::listBackStep(const ButtonEvent& ev) {
    if (ev.button != Button::C || !listTarget()) return false;
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
    if (aHeld_ && list && nowMs_ - aDownMs_ >= kListRepeatDelayMs &&
        nowMs_ - listRepeatLastMs_ >= kListRepeatMs) {
        listRepeatLastMs_ = nowMs_;
        list.step(*this, +1);
        lastInputMs_ = nowMs_;   // a held step is activity (no defocus mid-scroll)
        changed = true;
    } else if (!aHeld_) {
        listRepeatLastMs_ = nowMs_;   // primed, so the next hold can't burst-catch-up
    }

    // C's backward walk. Crossing kListRepeatDelayMs is what turns the press from a tap
    // into a hold, so the first backward step and the decision that this is not a
    // cancel happen together: listBackStepped_ latches it for the release edge.
    if (cHeld_ && list && nowMs_ - cDownMs_ >= kListRepeatDelayMs &&
        nowMs_ - listBackRepeatLastMs_ >= kListRepeatMs) {
        listBackRepeatLastMs_ = nowMs_;
        listBackStepped_ = true;
        list.step(*this, -1);
        lastInputMs_ = nowMs_;
        changed = true;
    } else if (!cHeld_) {
        listBackRepeatLastMs_ = nowMs_;
    }

    if (changed) dirty_ = true;
    return changed;
}

}  // namespace mal
