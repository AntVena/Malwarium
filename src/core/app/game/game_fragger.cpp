#include "core/app/game.h"

#include "tunables.h"
#include "core/ui/maint_screen.h"

// game_fragger.cpp — FRAGGER, the played Defrag (and its GAMES cabinet).
//
// The rules are core/model/fragger.h, with no framebuffer and no Game in them; the
// board is maint_screen's drawFraggerBoard. This file is the buttons, the cadence's
// sound and the settlement: a run off MAINT pays per fragment shot in Fragmentation AND
// Bits (kFraggerFragPerHit / kFraggerBitsPerHit), and a cabinet run hands its score to
// the arcade till.

namespace mal {

void Game::beginFraggerRun() {
    // The run, with no opinion about who is paying for it — MAINT has already charged
    // its Bits, the arcade charges nothing.
    rng_ = rng_ * 1664525u + 1013904223u;   // the shared LCG seeds the fall
    fragger_.reset(rng_);
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
    if (arcadeRun_) {
        finishArcadeRun(fragger_.hits() >= kArcadeFraggerWinHits, fragger_.hits(),
                        kArcadeFraggerWinHits);
        return;
    }
    // A played defrag cannot FAIL; it can only be worth less. Every fragment shot takes
    // its frag off the disk while there is any left and pays its Bits either way, so a
    // run past a clean disk is still worth playing on. Like the firewall, this path does
    // NOT run resolveMaint(): no fail roll, no +kMaintFailPenalty, no care mistake and no
    // Replication Ghost, because the outcome was played rather than gambled.
    maintKind_ = MaintKind::Defrag;
    maintPlayed_ = true;
    playedFragRemoved_ = fraggerFragWorth(fragger_, model_.fragmentation());
    playedBitsEarned_ = fragger_.hits() * kFraggerBitsPerHit;
    model_.applyPlayedDefrag(playedFragRemoved_);
    bits_ += playedBitsEarned_;
    maintSuccess_ = playedFragRemoved_ > 0 && model_.fragmentation() == 0;
    // The tally counts disks that got cleaner, so a run that shot nothing doesn't score one.
    if (playedFragRemoved_ > 0) ++defragCount_;
    noteCareSignal(DominantSignal::Maintenance);
    markSaveDirty();
    processBeat_ = kProcessBeats;                // the work is done; show the outcome
    processResolved_ = true;
    nav_ = Nav::Process;
}

void Game::drawFragger(Framebuffer& fb) const {
    if (gameBriefOpen_) { drawGameBrief(fb); return; }
    FraggerView v;
    v.arcade = arcadeRun_;
    v.frag = model_.fragmentation();
    v.shotLane = fraggerShotLane_;
    v.shotRow = fraggerShotRow_;
    drawFraggerBoard(fb, fragger_, v);
}

}  // namespace mal
