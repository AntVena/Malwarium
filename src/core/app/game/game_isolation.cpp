#include "core/app/game.h"

#include "tunables.h"

// game_isolation.cpp: the Isolation Protocol, the Worm line's hatch minigame.
//
// A Vermicell egg is laid and the worm inside it is turned straight loose in a
// quarantine buffer: it never stops moving, A steers it left, C steers it right, and
// every byte it swallows is kIsolationDotMs off the incubation clock. Crash into a wall
// or into its own body and the run banks what it earned; swallow the whole clock and the
// protocol finished CLEAN: the egg hatches out of the minigame and WORM_WHISPERER
// fires. A bad run costs only the bonus, never the pet, which is the same deal the
// Clutch Pick and the defrag Stacker both offer.
//
// The rules are core/model/isolation.h, with no framebuffer and no Game in them, and the
// buffer's input and draw are IsolationScreen (game/isolation_screen.h). This file is
// the start and the payout.
//
// Entered from Game::startHatchGame the instant the egg is laid; leaves to idle, or
// straight into the hatch when the clock is gone.

namespace mal {


// --- Lifecycle -------------------------------------------------------------

void Game::startIsolation(int goalDots) {
    rng_ = rng_ * 1664525u + 1013904223u;   // the shared LCG seeds the byte sequence
    isolation_.reset(rng_, goalDots);
    isolationBanked_ = 0;
    lastIsolationStepMs_ = nowMs_;
    closeGameBrief();   // a stale flag from a previous run must not open paused
    nav_ = Nav::Isolation;
    dirty_ = true;
}

void Game::finishIsolation() {
    // An arcade run has no clock to spend the bytes on, and no achievement riding on a
    // clean one: the till takes the score and that is the whole settlement. The cabinet
    // run is endless, so `clean` means a run that filled the whole buffer. A win is
    // reaching kArcadeIsolationWinBytes, and the score can go past it.
    if (inArcadeRun()) {
        finishArcadeRun(isolation_.dots() >= kArcadeIsolationWinBytes, isolation_.dots(),
                        kArcadeIsolationWinBytes);
        return;
    }
    // Spend the run. Banked against isolationBanked_ rather than paid straight out, so a
    // second press on the result screen can't buy the same bytes twice.
    const int owed = isolation_.dots() - isolationBanked_;
    if (owed > 0) {
        isolationBanked_ = isolation_.dots();
        accelerateEggHatch(static_cast<uint32_t>(owed) * kIsolationDotMs);
    }
    // WORM_WHISPERER asks for a CLEAN protocol, and this is the one place that exists:
    // the run that ate the entire clock, not the one that ran out of buffer first.
    if (isolation_.clean()) unlockAchievement(ach::kWormWhisperer);
    if (pet_.bootHatchRemainMs == 0) {
        completeHatch();   // nothing left to incubate: hatch here, not a tick later
        return;
    }
    nav_ = Nav::Idle;
    dirty_ = true;
    markSaveDirty();
}

}  // namespace mal
