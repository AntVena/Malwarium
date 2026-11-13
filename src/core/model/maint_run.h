// maint_run.h: one MAINT process, a run (CONTRIBUTING, "Where state lives"). From B on
// the MAINT action page until its outcome is dismissed. A QUICK or TOOL run fills the
// process bar and then resolves its roll (or its guarantee); a PLAYED run hands off to the
// process's minigame board and settles from what the board was worth. Runtime-only and
// never saved: a run interrupted by a power cycle forfeits its entry, board and all.
//
// Game holds it only while the board or the process screen is up. The action page's
// selection (which process, which variant, the dial) is the page's, and a run copies what
// it needs when it starts.
#pragma once

#include "core/content/content_arcade.h"

namespace mal {

// The two MAINT processes.
enum class MaintKind { Defrag, Av };

struct MaintRun {
    MaintKind kind = MaintKind::Defrag;
    bool played = false;      // the PLAYED variant: a board, not a roll or a purchase
    // The dial a played run's board paces and pays by (Game::runDifficulty).
    ArcadeDifficulty difficulty = ArcadeDifficulty::Easy;
    int beat = 0;             // heartbeats the process bar has run (kProcessBeats fills it)
    bool resolved = false;    // the outcome is showing
    bool success = false;     // cleaned, against a failed roll or a run that came up short
    // What a played run took off the disk and paid. The outcome prints them, because
    // unlike a rolled or bought run's fixed reduction they depend on the board.
    int fragRemoved = 0;
    int bitsEarned = 0;

    static MaintRun started(MaintKind kind, bool played, ArcadeDifficulty difficulty) {
        MaintRun r;
        r.kind = kind;
        r.played = played;
        r.difficulty = difficulty;
        return r;
    }
    // A played run's board is on screen: it has not settled yet.
    bool boardLive() const { return played && !resolved; }
};

}  // namespace mal
