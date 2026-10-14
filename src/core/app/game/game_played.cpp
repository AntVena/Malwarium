#include "core/app/game.h"

#include "tunables.h"
#include "core/ui/maint_screen.h"

// game_played.cpp — the seam between a played minigame and whoever is paying for it.
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
    if (arcadeRun_) {
        finishArcadeRun(o.won, o.score, o.scoreMax);
        return;
    }
    settleMaintPlay(o);
}

int Game::playedFragWorth(int units) const {
    const int worth = units * kPlayFragPerUnit * arcadeRewardMult(runDifficulty());
    const int cap = maintKind_ == MaintKind::Av ? kAvReduction : model_.fragmentation();
    const int capped = worth > cap ? cap : worth;
    return capped > model_.fragmentation() ? model_.fragmentation() : capped;
}

int Game::playedBitsWorth(int bitsUnits) const {
    if (maintKind_ != MaintKind::Defrag) return 0;
    return bitsUnits * kPlayBitsPerUnit * arcadeRewardMult(runDifficulty());
}

PlayHud Game::playHud(const PlayOutcome& sofar) const {
    PlayHud h;
    h.arcade = arcadeRun_;
    if (arcadeRun_) {
        h.title = arcadeGame_ >= 0 && arcadeGame_ < arcadeGameCount()
                      ? arcadeGames()[arcadeGame_].displayName : "";
        h.score = sofar.score;
        h.scoreMax = sofar.scoreMax;
        return h;
    }
    h.title = maintKind_ == MaintKind::Defrag ? "DEFRAGMENTING" : "SCANNING";
    h.fragOff = playedFragWorth(sofar.units);
    h.fragNow = model_.fragmentation() - h.fragOff;
    h.bits = playedBitsWorth(sofar.bitsUnits);
    // The Bits readout only where Bits can be earned: a DEFRAG, by a game with a Bits unit.
    const ArcadeGameDef* g = arcadeGameByKind(maintPlayedGame(maintKind_));
    h.paysBits = maintKind_ == MaintKind::Defrag && g && g->bitsUnitName;
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
    maintPlayLive_ = false;
    maintPlayed_ = true;
    if (maintKind_ == MaintKind::Av && o.won) {
        playedFragRemoved_ = model_.fragmentation() < kAvReduction ? model_.fragmentation()
                                                                   : kAvReduction;
        playedBitsEarned_ = 0;
        model_.applyAntivirus(true);
        maintSuccess_ = true;
    } else {
        playedFragRemoved_ = playedFragWorth(o.units);
        playedBitsEarned_ = playedBitsWorth(o.bitsUnits);
        model_.applyPlayedDefrag(playedFragRemoved_);
        bits_ += playedBitsEarned_;
        maintSuccess_ = maintKind_ == MaintKind::Defrag && playedFragRemoved_ > 0 &&
                        model_.fragmentation() == 0;
    }
    // The per-pet tally counts disks that got cleaner, so a run worth nothing doesn't.
    if (maintKind_ == MaintKind::Defrag && playedFragRemoved_ > 0) ++defragCount_;
    noteCareSignal(DominantSignal::Maintenance);
    markSaveDirty();
    processBeat_ = kProcessBeats;                // the work is done; show the outcome
    processResolved_ = true;
    nav_ = Nav::Process;
}

}  // namespace mal
