// game_onboard.cpp — what the device teaches a first-time player, and how it knows it
// already has.
//
// Nobody is standing beside a new operator, so the device has to say the three things
// the hint bands cannot: what an egg's first minigame is staking, which button opens
// the menu, and which part of the menu the pet needs right now. The first two are
// one-time TIPS, a player-level persisted set (save v66) so a new egg does not re-teach
// the buttons; the third is a standing readout and is never "seen".
#include "core/app/game.h"

#include <cstdio>

#include "core/ui/carousel.h"
#include "core/ui/items_screen.h"
#include "core/ui/prose_page.h"

namespace mal {

// --- The tips set ------------------------------------------------------------

bool Game::tipSeen(Tip t) const {
    const int w = static_cast<int>(t);
    if (w >= kTipWireCap) return false;
    return (tipsSeen_[w / 8] & (1u << (w % 8))) != 0;
}

void Game::markTipSeen(Tip t) {
    const int w = static_cast<int>(t);
    if (w >= kTipWireCap || tipSeen(t)) return;
    tipsSeen_[w / 8] |= static_cast<uint8_t>(1u << (w % 8));
    markSaveDirty();
}

// --- The hatch briefing --------------------------------------------------------

bool Game::hatchGameLive() const {
    if (arcadeRun_ || !inEggPhase()) return false;
    switch (nav_) {
        case Nav::Decryption:
        case Nav::ModalEggPick:
        case Nav::Isolation:
        case Nav::Chroma:
            return true;
        default:
            return false;
    }
}

ProseRow Game::hatchBriefLeadRow() const {
    // Line-agnostic on purpose: every hatch game pays in incubation time and none of
    // them can cost the egg (game_lifecycle.cpp's startHatchGame), so one panel is true
    // of all four. It also names the chord, which is the only way back to this page.
    ProseRow r;
    r.label = "A NEW EGG";
    std::snprintf(r.body.buf, sizeof(r.body.buf), "%s",
                  "Your pet is sealed in this egg. Play well and it hatches sooner. "
                  "Play badly and it still hatches on its own clock - the egg is never "
                  "at risk. A+C brings these rules back mid-game.");
    return r;
}

// --- What the pet needs ---------------------------------------------------------

void Game::careAttention(unsigned& attention, unsigned& urgent) const {
    attention = 0;
    urgent = 0;
    if (!pet_ || inEggPhase()) return;
    // Each vital names the ONE slot that fixes it, so the shelf points at the remedy
    // rather than at a readout: food is in ITEMS, a defrag or AV scan in MAINT, and the
    // arcade is the one place Happiness can be bought back on demand. With no meal in
    // the bag, ITEMS cannot fix hunger, so the mark goes to EXPL, where food is found.
    const auto mark = [&](SubmenuId id, Zone zone, bool force) {
        if (zone == Zone::Ok && !force) return;
        for (int i = 0; i < kCarouselSlots; ++i) {
            if (carouselSlots()[i].id != id) continue;
            attention |= 1u << i;
            if (zone == Zone::Critical) urgent |= 1u << i;
        }
    };
    mark(inventoryHoldsMeal(registry_, inventory_) ? SubmenuId::Items : SubmenuId::Expl,
         model_.hungerZone(), false);
    mark(SubmenuId::Maint, model_.fragZone(), model_.hasGhost());
    mark(SubmenuId::Games, model_.happyZone(), false);
}

bool Game::lockoutFoodHeld() const {
    for (const ItemDef* d : registry_.allItems())
        if (itemResolvesLockout(*d) && inventory_.count(d->id) > 0) return true;
    return false;
}

}  // namespace mal
