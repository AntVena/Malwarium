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

}  // namespace mal
