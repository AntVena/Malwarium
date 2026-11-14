#include "core/app/game.h"


// game_fragger.cpp: FRAGGER, a played MAINT run and a GAMES cabinet.
//
// The rules are core/model/fragger.h, with no framebuffer and no Game in them; the
// buttons and the board are FraggerScreen (game/fragger_screen.h). This file is the
// run's start, the cadence's sound and its PlayOutcome; game_played.cpp decides what
// the outcome buys.

namespace mal {

void Game::beginFraggerRun() {
    // The run, independent of who pays for it: MAINT has already charged its Bits,
    // the arcade charges nothing. The dial is the LANE count, one more per step up
    // from EASY's minimum.
    rng_ = rng_ * 1664525u + 1013904223u;   // the shared LCG seeds the fall
    fragger_.reset(rng_, kFraggerMinLanes + static_cast<int>(runDifficulty()));
    lastFraggerStepMs_ = nowMs_;
    fraggerScreen_.clearShot();
    closeGameBrief();   // a stale flag from a previous run must not open paused
    nav_ = Nav::Fragger;
    dirty_ = true;
}

void Game::stepFragger() {
    if (!fragger_.running()) return;
    const int caught = fragger_.caught();
    fragger_.step();
    fraggerScreen_.clearBeamLane();             // the beam lasts one row of fall
    // A catch is money arriving, so it sounds like it, unless the same row ended the
    // run, which is the news that matters.
    if (!fragger_.running()) playSound(Sound::Crash);
    else if (fragger_.caught() > caught) playSound(Sound::Coin);
    dirty_ = true;
}

void Game::finishFragger() {
    if (fragger_.hits() >= ach::kSafeHandsMinHits && fragger_.caught() > fragger_.hits())
        unlockAchievement(ach::kSafeHands);
    PlayOutcome o;
    o.units = fragger_.hits();
    o.bitsUnits = fragger_.caught();
    o.won = fragger_.hits() >= kFraggerWinHits;
    o.score = fragger_.hits() + fragger_.caught();
    o.scoreMax = kFraggerWinHits;
    settlePlayedRun(o);
}

}  // namespace mal
