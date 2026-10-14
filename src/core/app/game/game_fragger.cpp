#include "core/app/game.h"

#include "tunables.h"
#include "core/ui/maint_screen.h"

// game_fragger.cpp — FRAGGER, a played MAINT run and a GAMES cabinet.
//
// The rules are core/model/fragger.h, with no framebuffer and no Game in them; the
// board is maint_screen's drawFraggerBoard. This file is the buttons, the cadence's
// sound and the run's PlayOutcome — what that buys is game_played.cpp's business.

namespace mal {

void Game::beginFraggerRun() {
    // The run, with no opinion about who is paying for it — MAINT has already charged
    // its Bits, the arcade charges nothing.
    // The dial is the LANE count, one more per step up from EASY's minimum.
    rng_ = rng_ * 1664525u + 1013904223u;   // the shared LCG seeds the fall
    fragger_.reset(rng_, kFraggerMinLanes + static_cast<int>(runDifficulty()));
    lastFraggerStepMs_ = nowMs_;
    fraggerShotLane_ = -1;
    fraggerShotRow_ = -1;
    closeGameBrief();   // a stale flag from a previous run must not open paused
    nav_ = Nav::Fragger;
    dirty_ = true;
}

void Game::onFragger(const ButtonEvent& ev) {
    if (onGameBriefInput(ev)) return;
    if (!fragger_.running()) {                  // parked on the result: any press moves on
        finishFragger();
        return;
    }
    // A and C MOVE the head — the outer two buttons, left to left and right to right,
    // the Isolation Protocol's deviation from the standard contract. B fires. There is
    // no stop: the run ends when a fragment lands, and everything shot before that is
    // already banked, so a quit button would only be a slower way to the same toast.
    if (ev.button == Button::A) {
        fragger_.moveLeft();
    } else if (ev.button == Button::C) {
        fragger_.moveRight();
    } else if (ev.button == Button::B) {
        int row = -1;
        const Fragger::Shot shot = fragger_.fire(&row);
        fraggerShotLane_ = fragger_.lane();
        fraggerShotRow_ = row;
        if (shot == Fragger::Shot::Hit) playSound(Sound::Point);
        else if (shot == Fragger::Shot::Blocked) playSound(Sound::Miss);
    }
    dirty_ = true;
}

void Game::stepFragger() {
    if (!fragger_.running()) return;
    fragger_.step();
    fraggerShotLane_ = -1;                      // the beam lasts one row of fall
    if (!fragger_.running()) playSound(Sound::Crash);
    dirty_ = true;
}

void Game::finishFragger() {
    PlayOutcome o;
    o.units = fragger_.hits();
    o.bitsUnits = fragger_.caught();
    o.won = fragger_.hits() >= kFraggerWinHits;
    o.score = fragger_.hits() + fragger_.caught();
    o.scoreMax = kFraggerWinHits;
    settlePlayedRun(o);
}

void Game::drawFragger(Framebuffer& fb) const {
    if (gameBriefOpen_) { drawGameBrief(fb); return; }
    PlayOutcome sofar;
    sofar.units = fragger_.hits();
    sofar.bitsUnits = fragger_.caught();
    sofar.score = fragger_.hits() + fragger_.caught();
    sofar.scoreMax = kFraggerWinHits;
    drawFraggerBoard(fb, fragger_, playHud(sofar), fraggerShotLane_, fraggerShotRow_);
}

}  // namespace mal
