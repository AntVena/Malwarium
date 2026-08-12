// content_arcade.cpp — the arcade cabinet table. One row per playable minigame; see
// content_arcade.h for the field schema and what adding a cabinet costs.

#include "core/content/content_arcade.h"

#include <cstring>

namespace mal {

namespace {

const ArcadeGameDef kArcadeGames[] = {
    // The played Defrag, off its disk. Nothing about the board changes here — it is
    // the same deterministic slide, so a run that went well in MAINT goes exactly the
    // same way in the arcade.
    {"stacker", "DEFRAG STACKER",
     "LAND THE RUN. THE OVERHANG IS SHAVED OFF.",
     "ICON_MAINT_DEFRAG", "HOW FAST THE RUN SLIDES.",
     ArcadeGameKind::Stacker, ArcadeScoring::Incremental},

    // The Phishing hatch, with no egg riding on it. Its dial is the only one that
    // isn't speed: more halvings is a narrower survivor, so the tell has to be found
    // earlier rather than watched for longer.
    {"clutch", "SPOT THE PHISH",
     "ONE EGG MOVES. HALVE THE RAFT ONTO IT.",
     "ICON_ARCADE_CLUTCH", "HOW MANY TIMES THE RAFT HALVES.",
     ArcadeGameKind::Clutch, ArcadeScoring::WinLose},

    // The Worm hatch, with the clock replaced by a flat target — so a clean run is a
    // clean run whatever the pet is or isn't incubating.
    {"isolation", "ISOLATION PROTOCOL",
     "EAT THE BUFFER WITHOUT EATING YOURSELF.",
     "ICON_LINE_WORM", "HOW FAST THE WORM MOVES.",
     ArcadeGameKind::Isolation, ArcadeScoring::Incremental},

    // The Ransomware hatch, with no egg to unlock. Its dial is the only one that moves
    // the RULES rather than the pace: easy shows which cells a row got exactly right,
    // and hard lets the key repeat a colour, which kills the deduction that every
    // colour ruled out narrows the rest.
    {"decryption", "DISK DECRYPTION",
     "FIVE COLOURS, THREE SLOTS, FIVE GUESSES.",
     "ICON_LINE_RANSOMWARE", "WHAT THE BOARD WILL TELL YOU.",
     ArcadeGameKind::Decryption, ArcadeScoring::Incremental},

    // The VAULT's quote board, with no ticket spent and no prize but Bits — and only
    // over quotes already solved, so the cabinet is a re-run rather than a way to farm
    // the pool for free. That is also why it is the one gated row: there is nothing to
    // re-run until kQuoteArcadeUnlockWins quotes have been cracked the honest way.
    {"cryptogram", "DECRYPTOGRAM",
     "OPEN THE QUOTE. ONE WRONG LETTER ENDS IT.",
     "ICON_ITEM_DECRYPTOGRAM", "HOW MANY LETTERS ARE OPEN.",
     ArcadeGameKind::Cryptogram, ArcadeScoring::Incremental,
     ArcadeUnlock::QuotesSolved},
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

const char* arcadeDifficultyName(ArcadeDifficulty d) {
    switch (d) {
        case ArcadeDifficulty::Easy:   return "EASY";
        case ArcadeDifficulty::Medium: return "MEDIUM";
        case ArcadeDifficulty::Hard:   return "HARD";
    }
    return "MEDIUM";
}

}  // namespace mal
