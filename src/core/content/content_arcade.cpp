// content_arcade.cpp — the arcade cabinet table. One row per playable minigame; see
// content_arcade.h for the field schema and what adding a cabinet costs.

#include "core/content/content_arcade.h"

#include <cstring>

namespace mal {

namespace {

// Each engine's RULES page (GameBriefDef, content_arcade.h) — what A+C opens mid-play,
// on top of the run, whichever door the player came in through (cabinet, MAINT's
// Stacker variant, or an egg's hatch). Kept to the mechanic itself: none of these
// mentions Bits, an incubation clock, or a clean streak, because what a run PAYS is the
// caller's business (arcade till vs. hatch clock vs. Fragmentation) and would be wrong
// copy in two of the three doors it can be read through.
const GameBriefDef kStackerBrief[] = {
    {"THE RUN", "A run of blocks slides back and forth on its own. B locks it where it "
                "stands - there's no clock, but the row above is waiting."},
    {"THE SHAVE", "Locking shaves off anything not resting on a locked block below. "
                  "What survives becomes the next run, narrower, re-entering from the "
                  "left wall."},
    {"THE STAKES", "Lock the top row with at least one block and the board clears. "
                   "Lose every block on a lock and the run is over. C stops early and "
                   "banks whatever is already locked."},
};

const GameBriefDef kClutchBrief[] = {
    {"FIND THE TELL", "One egg in the raft is real - the rest are dead still. Motion "
                      "is the only thing that gives it away."},
    {"HALVE THE RAFT", "A aims at the first half, C at the second. B halves the raft "
                       "onto whichever half you aimed at."},
    {"KEEP NARROWING", "Every round cuts what's left in half. Finish with the live egg "
                       "still inside and it pays out; lose it and only the bonus is "
                       "gone - never the egg."},
};

const GameBriefDef kIsolationBrief[] = {
    {"KEEP MOVING", "The worm never stops. A turns it left, C turns it right - the "
                    "outer two buttons - and there is no straight-ahead."},
    {"EAT THE BYTE", "Swallowing the lit byte grows you and drops another somewhere "
                     "in the buffer. Following your own tail is legal."},
    {"THE WALL, THE BODY", "Run into either and the protocol crashes, banking "
                           "whatever you already ate. Swallow every byte the run "
                           "asks for and it finishes CLEAN."},
};

const GameBriefDef kDecryptionBrief[] = {
    {"THE KEY", "Three slots, five colours, a hidden key. A cycles the focused "
               "slot's colour; B locks it in - locking the third slot plays the row."},
    {"THE ANSWER", "Every played row is scored two numbers: how many slots are the "
                   "right colour in the right place, and how many are the right "
                   "colour in the wrong slot. Never which."},
    {"FIVE GUESSES", "C steps back a slot and reopens it if you change your mind. "
                     "Crack the key inside five rows or the disk stays locked."},
};

const GameBriefDef kCryptogramBrief[] = {
    {"OPEN A LETTER", "A/C walk the pool; B takes the focused letter. A/C then walk "
                      "the closed cells to aim it, and B places it."},
    {"ALL OR NOTHING", "Right, and every instance of that letter opens across the "
                       "whole quote at once. Wrong, and the run ends there - no "
                       "second guess inside a board."},
    {"CHANGE YOUR MIND", "A+C hands a held letter back to the pool before you place "
                         "it - the same chord that opens this page, so it only ever "
                         "drops a letter, never both at once."},
};

const GameBriefDef kChromaBrief[] = {
    {"WEAR THE WATER", "Three colours, three buttons - A, B and C each wear one. The "
                       "water under you takes one of them, and it is never the one you "
                       "have on."},
    {"CHANGING TAKES TIME", "A press starts the repaint; it is a moment before you are "
                            "actually wearing it. Caught halfway counts as caught."},
    {"THE SWEEP", "Something crosses the water on a clock that gets shorter every "
                  "pass. Match it and the pass is yours; miss it and the run ends with "
                  "what you already earned."},
};

const ArcadeGameDef kArcadeGames[] = {
    // The played Defrag, off its disk. Nothing about the board changes here — it is
    // the same deterministic slide, so a run that went well in MAINT goes exactly the
    // same way in the arcade.
    {"stacker", "DEFRAG STACKER",
     "LAND THE RUN. THE OVERHANG IS SHAVED OFF.",
     "ICON_MAINT_DEFRAG", "HOW FAST THE RUN SLIDES.",
     ArcadeGameKind::Stacker, ArcadeScoring::Incremental, ArcadeUnlock::Always,
     kStackerBrief, static_cast<int>(sizeof(kStackerBrief) / sizeof(kStackerBrief[0]))},

    // The Phishing hatch, with no egg riding on it. Its dial is the only one that
    // isn't speed: more halvings is a narrower survivor, so the tell has to be found
    // earlier rather than watched for longer.
    {"clutch", "SPOT THE PHISH",
     "ONE EGG MOVES. HALVE THE RAFT ONTO IT.",
     "ICON_ARCADE_CLUTCH", "HOW MANY TIMES THE RAFT HALVES.",
     ArcadeGameKind::Clutch, ArcadeScoring::WinLose, ArcadeUnlock::Always,
     kClutchBrief, static_cast<int>(sizeof(kClutchBrief) / sizeof(kClutchBrief[0]))},

    // The Worm hatch, with the clock replaced by a flat target — so a clean run is a
    // clean run whatever the pet is or isn't incubating.
    {"isolation", "ISOLATION PROTOCOL",
     "EAT THE BUFFER WITHOUT EATING YOURSELF.",
     "ICON_LINE_WORM", "HOW FAST THE WORM MOVES.",
     ArcadeGameKind::Isolation, ArcadeScoring::Incremental, ArcadeUnlock::Always,
     kIsolationBrief,
     static_cast<int>(sizeof(kIsolationBrief) / sizeof(kIsolationBrief[0]))},

    // The Ransomware hatch, with no egg to unlock. Its dial is the only one that moves
    // the RULES rather than the pace: easy shows which cells a row got exactly right,
    // and hard lets the key repeat a colour, which kills the deduction that every
    // colour ruled out narrows the rest.
    {"decryption", "DISK DECRYPTION",
     "FIVE COLOURS, THREE SLOTS, FIVE GUESSES.",
     "ICON_LINE_RANSOMWARE", "WHAT THE BOARD WILL TELL YOU.",
     ArcadeGameKind::Decryption, ArcadeScoring::Incremental, ArcadeUnlock::Always,
     kDecryptionBrief,
     static_cast<int>(sizeof(kDecryptionBrief) / sizeof(kDecryptionBrief[0]))},

    // The VAULT's quote board, with no ticket spent and no prize but Bits — and only
    // over quotes already solved, so the cabinet is a re-run rather than a way to farm
    // the pool for free. That is also why it is the one gated row: there is nothing to
    // re-run until kQuoteArcadeUnlockWins quotes have been cracked the honest way.
    {"cryptogram", "DECRYPTOGRAM",
     "OPEN THE QUOTE. ONE WRONG LETTER ENDS IT.",
     "ICON_ITEM_DECRYPTOGRAM", "HOW MANY LETTERS ARE OPEN.",
     ArcadeGameKind::Cryptogram, ArcadeScoring::Incremental,
     ArcadeUnlock::QuotesSolved,
     kCryptogramBrief,
     static_cast<int>(sizeof(kCryptogramBrief) / sizeof(kCryptogramBrief[0]))},

    // The Metamorphic hatch, with no egg riding on it — and the one cabinet whose
    // SUBJECT can change: a pet on the family that wears borrowed colours rehearses on
    // the board itself, and anybody else's pet watches the egg do it (Game::chromaSubject).
    // Its dial moves the window like the paced games, and HARD adds the rule the hatch
    // never plays with: the water can change under you once you have committed.
    {"chroma", "CHROMATOPHORE",
     "WEAR THE WATER BEFORE THE SWEEP.",
     "ICON_ARCADE_CHROMA", "HOW LONG YOU GET, AND IF THE WATER MOVES.",
     ArcadeGameKind::Chroma, ArcadeScoring::Incremental, ArcadeUnlock::Always,
     kChromaBrief, static_cast<int>(sizeof(kChromaBrief) / sizeof(kChromaBrief[0]))},
};

static_assert(sizeof(kArcadeGames) / sizeof(kArcadeGames[0]) <= kArcadeMaxCabinets,
              "raise kArcadeMaxCabinets: the save's per-cabinet tallies are that wide");

}  // namespace

const ArcadeGameDef* arcadeGames() { return kArcadeGames; }

int arcadeGameCount() {
    return static_cast<int>(sizeof(kArcadeGames) / sizeof(kArcadeGames[0]));
}

int arcadeGameIndexById(const char* id) {
    if (!id) return -1;
    for (int i = 0; i < arcadeGameCount(); ++i)
        if (std::strcmp(kArcadeGames[i].id, id) == 0) return i;
    return -1;
}

const ArcadeGameDef* arcadeGameByKind(ArcadeGameKind kind) {
    for (int i = 0; i < arcadeGameCount(); ++i)
        if (kArcadeGames[i].kind == kind) return &kArcadeGames[i];
    return nullptr;
}

const char* arcadeDifficultyName(ArcadeDifficulty d) {
    switch (d) {
        case ArcadeDifficulty::Easy:   return "EASY";
        case ArcadeDifficulty::Medium: return "MEDIUM";
        case ArcadeDifficulty::Hard:   return "HARD";
    }
    return "MEDIUM";
}

}  // namespace mal
