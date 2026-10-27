// arcade_screen.h — GAMES: the cabinet list (L2), one cabinet's own page (L3), and
// the payout that closes a run.
//
// Deliberately thin. The arcade draws the MENU around a minigame and nothing of the
// game itself — every run hands off to the screen that already owns it (the Stacker
// board, the clutch panel, the quarantine buffer), so there is no second copy of any
// game's rendering here to drift from the first. What comes back is a score, and this
// file's last screen is what turns that into a number of Bits.
#pragma once

#include "core/content/content_arcade.h"

namespace mal {

class Framebuffer;
class ContentRegistry;

// L2 cabinet list. `rows`/`n` are the roster indices actually OFFERED right now
// (ArcadeUnlock — a locked cabinet is absent, never greyed) in the order they draw;
// `plays` is the lifetime tally array, parallel to the full roster and indexed through
// `rows`. `cursor` is a roster index, not a row position.
void drawArcadeList(Framebuffer& fb, const ContentRegistry& reg, const int* plays,
                    const int* rows, int n, int cursor, int beat);

// One cabinet's lifetime record.
struct ArcadeTally {
    int plays = 0;
    int wins = 0;
    int best = 0;                    // high score; 0 for a cabinet that keeps none
};

// L3 cabinet page: what the game asks, what the dial moves, what a run pays, and the
// START row. The tally's `best` is drawn only where a run HAS a score to keep, which is
// what makes the two endless cabinets legible as endless: their page shows a number to
// beat rather than a finish to reach.
void drawArcadeCabinet(Framebuffer& fb, const ContentRegistry& reg,
                       const ArcadeGameDef& def, ArcadeDifficulty difficulty,
                       const ArcadeTally& tally);

// The payout screen a finished run lands on.
struct ArcadeResultView {
    bool won = false;
    // The run's own numbers, both 0 for a win-or-lose cabinet, which has nothing to show
    // but the verdict.
    int score = 0;
    int scoreMax = 0;
    int best = 0;                    // the cabinet's high score after this run
    bool newBest = false;            // this run is what set it
    int bits = 0;                    // what was actually banked
    int happy = 0;
};
void drawArcadeResult(Framebuffer& fb, const ArcadeGameDef& def, const ArcadeResultView& v);

}  // namespace mal
