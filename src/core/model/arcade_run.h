// arcade_run.h: one cabinet run in GAMES, a run (CONTRIBUTING, "Where state lives").
// From START on a cabinet until its minigame reports a finish to the till
// (Game::finishArcadeRun). The minigame's board is its own engine; this is what makes a
// board a cabinet run rather than a hatch or a MAINT one: which cabinet is being played,
// and the dial it was started under. Runtime-only and never saved; only the tallies the
// till writes onto the Operator are.
//
// Game holds it only while a run is live, and drops it at the till. What the payout screen
// shows afterwards is the ArcadeResult the till settled it into.
#pragma once

#include "core/content/content_arcade.h"
#include "tunables.h"

namespace mal {

// A finished run, frozen for the payout screen: the till has already paid it by the time
// the player reads it, and the minigame's own state resets the moment the next run starts.
struct ArcadeResult {
    int cabinet = 0;      // roster index (content_arcade.h's order)
    bool won = false;
    int score = 0;
    int scoreMax = 0;     // 0 = a win-or-lose cabinet, with no score to show
    int bits = 0;         // what the run actually paid
    // Did this run SET the cabinet's best? The payout says NEW BEST instead of quoting a
    // number the player already had.
    bool newBest = false;
};

struct ArcadeRun {
    int cabinet = 0;      // roster index of what is running
    ArcadeDifficulty difficulty = ArcadeDifficulty::Medium;

    static ArcadeRun started(int cabinet, ArcadeDifficulty difficulty) {
        ArcadeRun r;
        r.cabinet = cabinet;
        r.difficulty = difficulty;
        return r;
    }

    // What the till pays for this finish, and whether it beats the cabinet's `best`. The
    // flat half is for finishing at all; the skill half is a PROPORTION of the ceiling
    // where there is one, and all-or-nothing where there isn't. Clamped because a game is
    // free to report a score above its own advertised max (a cleared Stacker board can),
    // and a run must never pay more than the cabinet said. The best is kept unclamped:
    // on an endless cabinet that number IS the game.
    ArcadeResult settled(bool won, int score, int scoreMax, int best) const {
        int bonus = 0;
        if (scoreMax > 0) {
            bonus = kArcadeScoreBits * score / scoreMax;
            if (bonus > kArcadeScoreBits) bonus = kArcadeScoreBits;
            if (bonus < 0) bonus = 0;
        } else if (won) {
            bonus = kArcadeScoreBits;
        }
        ArcadeResult r;
        r.cabinet = cabinet;
        r.won = won;
        r.score = score;
        r.scoreMax = scoreMax;
        r.bits = kArcadePlayBits + bonus;
        r.newBest = score > best && scoreMax > 0;
        return r;
    }
};

}  // namespace mal
