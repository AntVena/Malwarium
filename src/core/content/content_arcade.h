// content_arcade.h — the GAMES arcade roster: which minigames have a cabinet.
//
// The arcade does not own a single rule of a single game. Every cabinet here points at
// a minigame that already exists for its own reason — the Worm's hatch, the Phishing
// line's hatch, the played Defrag — and the arcade's whole job is to start one with no
// stakes attached and pay for the attempt. So changing a game changes its cabinet, and
// a game can never drift from its arcade copy, because there is no copy.
//
// A row is therefore mostly COPY: the name on the cabinet, the line that says what the
// game is, and what its difficulty dial actually moves (the games don't share a knob —
// two are paced and one is precision). The two fields that aren't copy are `kind`, which
// Game::startArcadeRun switches on, and `scoring`, which decides how the bonus half of
// the payout is worked out.
//
// ADDING ONE IS ONE ROW PLUS ONE CASE. The row goes in content_arcade.cpp and the case
// in Game::startArcadeRun — the same shape HatchGame uses (content/defs.h), and for the
// same reason: a minigame is engine behaviour, so an id alone can't launch it.
//
// Compiled-in and INDEX-addressed, like the crews (content_crews.h) and unlike the
// registry types: every call site is "the cabinet on row i" of the GAMES list. The SAVE
// stores each row's `id` string, so rows may be reordered or inserted without
// invalidating a save.
#pragma once

#include <cstdint>

namespace mal {

// Which minigame a cabinet starts. One entry per game; the applier is
// Game::startArcadeRun (game_arcade.cpp), which is the map of every route into a
// no-stakes run — never an `if (id == "...")` at a call site.
enum class ArcadeGameKind : uint8_t {
    Stacker,     // the played Defrag board (core/model/stacker.h)
    Clutch,      // the Phishing line's Spot the Phish (game_eggpick.cpp)
    Isolation,   // the Worm line's quarantine buffer (core/model/isolation.h)
    Decryption,    // the Ransomware line's code board (core/model/disk_decryption.h)
    Cryptogram,  // the VAULT's quote board (core/model/cryptogram.h)
};

// What has to be true before a cabinet appears in the GAMES list at all. An ABSENT row,
// not a greyed one — the same rule the Rig Shop's requiresRow follows, and for the same
// reason: a list should never advertise something the player has no route to.
//
// One entry per condition, resolved in one place (Game::arcadeCabinetOffered). Most
// cabinets are Always: their game already exists somewhere else in the device, so the
// arcade is only a second door to it.
enum class ArcadeUnlock : uint8_t {
    Always,
    // ...once kQuoteArcadeUnlockWins quotes are solved (content_quotes.h). The cabinet
    // replays a SOLVED quote for Bits, so before there is a back catalogue there is
    // nothing for it to offer.
    QuotesSolved,
};

// How a run is scored, which is the whole difference between the two payout shapes.
// Incremental runs bank a score out of a ceiling and are paid a proportion of the
// bonus; a WinLose run has no partial credit and takes all of it or none.
enum class ArcadeScoring : uint8_t { Incremental, WinLose };

// The difficulty dial, shared by every cabinet. Medium is the game exactly as its own
// (hatch or Defrag) context plays it, so the arcade never quietly rebalances the
// original — Easy and Hard are the only settings that change anything.
enum class ArcadeDifficulty : uint8_t { Easy, Medium, Hard };
inline constexpr int kArcadeDifficulties = 3;

// EASY / MEDIUM / HARD. The word is the grayscale-safe channel for the setting.
const char* arcadeDifficultyName(ArcadeDifficulty d);

// One section of a game's RULES page — the paged explainer A+C opens mid-play,
// pausing the engine underneath it (game.h's gameBriefOpen_). A heading and its
// prose, the same NAME + PROSE shape ROCK THE DOCK's own BRIEFING uses
// (content_tournament.h's TourneyBriefDef), so it flows through the same reader
// (core/ui/prose_page.h) with no layout of its own.
//
// Kept SHORT on purpose, same as the arena's: a section that runs past half the panel
// leaves the rest of the screen empty rather than seating a second one. Two or three
// short sections read as a rules card; a wall with a scrollbar does not.
struct GameBriefDef {
    const char* heading;
    const char* text;
};

struct ArcadeGameDef {
    const char* id;            // save + achievement key, e.g. "stacker"
    const char* displayName;   // the name on the cabinet
    const char* blurb;         // one line: what the game asks of the player
    const char* iconName;      // ICON_* row glyph, resolved through the registry
    const char* difficultyBlurb;  // what the dial moves HERE, in this game's terms
    ArcadeGameKind kind;
    ArcadeScoring scoring;
    ArcadeUnlock unlock = ArcadeUnlock::Always;   // what has to be true to see the row
    const GameBriefDef* brief = nullptr;   // this engine's RULES page, in reading order
    int briefCount = 0;
};

// Compile-time ceiling on the roster, so the per-cabinet save tallies can be a plain
// array on Game rather than a heap allocation. content_arcade.cpp static_asserts the
// real count against it — raise it there and here together, never silently.
inline constexpr int kArcadeMaxCabinets = 8;

const ArcadeGameDef* arcadeGames();
int arcadeGameCount();
// Row index for `id`, or -1 — how a saved play tally finds its cabinet again after
// the rows have been reordered.
int arcadeGameIndexById(const char* id);
// The row for `kind`, or nullptr. Keyed by the ENGINE rather than by cabinet row,
// because Stacker and Isolation are also reached from MAINT and an egg's hatch with no
// arcade row in play at all — a RULES page opened from either still needs a row.
const ArcadeGameDef* arcadeGameByKind(ArcadeGameKind kind);

}  // namespace mal
