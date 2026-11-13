#include "core/app/game.h"

#include "tunables.h"
#include "core/ui/maint_screen.h"

// game_played.cpp: the seam between a played minigame and whoever is paying for it.
//
// A game that can be played outside its own context (a MAINT process or a GAMES cabinet)
// starts here and ends here. In between it knows nothing about either: it reports one
// PlayOutcome, and the context decides what that buys. So which engine backs which MAINT
// process is a pair of content constants (content_arcade.h), and rewiring them touches
// nothing in this file, the engines, or the processes.

namespace mal {

bool Game::beginPlayedGame(ArcadeGameKind g) {
    switch (g) {
        case ArcadeGameKind::Stacker: beginStackerBoard(); return true;
        case ArcadeGameKind::Fragger: beginFraggerRun(); return true;
        // The hatch games settle against an egg's clock and their cabinets give them a
        // goal at the start call, so neither door fits a MAINT process.
        case ArcadeGameKind::Clutch:
        case ArcadeGameKind::Isolation:
        case ArcadeGameKind::Decryption:
        case ArcadeGameKind::Cryptogram:
        case ArcadeGameKind::Chroma:
            return false;
    }
    return false;
}

void Game::settlePlayedRun(const PlayOutcome& o) {
    if (inArcadeRun()) {
        finishArcadeRun(o.won, o.score, o.scoreMax);
        return;
    }
    settleMaintPlay(o);
}

int Game::playedFragWorth(int units) const {
    const int worth = units * kPlayFragPerUnit * arcadeRewardMult(runDifficulty());
    const int cap = maint_->kind == MaintKind::Av ? kAvReduction : pet_.model.fragmentation();
    const int capped = worth > cap ? cap : worth;
    return capped > pet_.model.fragmentation() ? pet_.model.fragmentation() : capped;
}

int Game::playedBitsWorth(int bitsUnits) const {
    if (maint_->kind != MaintKind::Defrag) return 0;
    return bitsUnits * kPlayBitsPerUnit * arcadeRewardMult(runDifficulty());
}

PlayHud Game::playHud(const PlayOutcome& sofar) const {
    PlayHud h;
    h.arcade = inArcadeRun();
    if (inArcadeRun()) {
        const int cabinet = arcade_->cabinet;
        h.title = cabinet >= 0 && cabinet < arcadeGameCount()
                      ? arcadeGames()[cabinet].displayName : "";
        h.score = sofar.score;
        h.scoreMax = sofar.scoreMax;
        return h;
    }
    // Neither context: a played board is only ever opened by a cabinet or a MAINT run.
    if (!maint_) return h;
    const MaintKind kind = maint_->kind;
    h.title = kind == MaintKind::Defrag ? "DEFRAGMENTING" : "SCANNING";
    h.fragOff = playedFragWorth(sofar.units);
    h.fragNow = pet_.model.fragmentation() - h.fragOff;
    h.bits = playedBitsWorth(sofar.bitsUnits);
    // The Bits readout only where Bits can be earned: a DEFRAG, by a game with a Bits unit.
    const ArcadeGameDef* g = arcadeGameByKind(maintPlayedGame(kind));
    h.paysBits = kind == MaintKind::Defrag && g && g->bitsUnitName;
    return h;
}

void Game::settleMaintPlay(const PlayOutcome& o) {
    // A played run cannot FAIL; it can only be worth less. So this path deliberately does
    // NOT run resolveMaint(): there is no fail roll to honour, no +kMaintFailPenalty, no
    // care mistake and no Replication Ghost, because the outcome was played rather than
    // gambled. What it does share is the care signal and the dirty save.
    //
    // DEFRAG is paid per unit: cleaning units in Fragmentation while the disk has any,
    // Bits units in Bits either way, so a run that outlasts a clean disk is still worth
    // playing on. AV is a scan:
    // a WON run is a full one, exactly what a passed Quick scan does, and a run that came
    // up short pays its units off the disk up to that same amount but cleans no process.
    //
    // A board outside a cabinet is only ever opened by a played MAINT run.
    MaintRun& run = *maint_;
    if (run.kind == MaintKind::Av && o.won) {
        run.fragRemoved = pet_.model.fragmentation() < kAvReduction ? pet_.model.fragmentation()
                                                                    : kAvReduction;
        run.bitsEarned = 0;
        pet_.model.applyAntivirus(true);
        run.success = true;
    } else {
        run.fragRemoved = playedFragWorth(o.units);
        run.bitsEarned = playedBitsWorth(o.bitsUnits);
        pet_.model.applyPlayedDefrag(run.fragRemoved);
        op_.bits += run.bitsEarned;
        run.success = run.kind == MaintKind::Defrag && run.fragRemoved > 0 &&
                      pet_.model.fragmentation() == 0;
    }
    // The per-pet tally counts disks that got cleaner, so a run worth nothing doesn't.
    if (run.kind == MaintKind::Defrag && run.fragRemoved > 0) ++pet_.defragCount;
    markSaveDirty();
    run.beat = kProcessBeats;                    // the work is done; show the outcome
    run.resolved = true;
    nav_ = Nav::Process;
}

}  // namespace mal
