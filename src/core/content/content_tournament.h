// content_tournament.h — ROCK THE DOCK: the operator bracket held in The Pirate Bayou.
//
// Everything else the ladder offers is a malbeast: a name, a sprite and a handful of
// moves keyed to depth. The Dock is the one place the pet fights PETWARE — a rolled
// creature off the same roster the player hatches from, carrying a real kit, real
// mods, real per-level stat points and an Exploit of its own. It is the arena for
// tactics rather than for progress, which is why nothing in it drops and only the
// title pays.
//
// This file is the arena's CONTENT: its NAME, the briefing it explains itself with,
// the operator handles its entrants run under, the shape of the bracket, and every
// magnitude the roll reads. The pure derivation that turns a seed into a fighter is
// core/model/tournament.h; the run state machine is game_tourney.cpp. Nothing here
// knows about either.
//
// The arena sits in the Bayou because the Bayou is the warez water
// (areas/AREA_NAMING.md), and a dock is where that water's traffic actually changes
// hands — so a season fought on one is the scene settling something in the open
// instead of over a wire. Its entrants are named to match; see kTourneyHandles.
#pragma once

namespace mal {

// --- The bracket's shape -----------------------------------------------------
// Eight entrants, single elimination, so the operator fights three matches and the
// other four first-round pairings resolve themselves. A power of two by construction:
// the bracket is a fixed binary tree and every round halves it, which is what lets a
// run persist as a survivor BITMASK over these slots rather than as a match list.
constexpr int kTourneySlots = 8;
constexpr int kTourneyRounds = 3;   // log2(kTourneySlots) — quarter, semi, final
static_assert((1 << kTourneyRounds) == kTourneySlots,
              "the bracket is a binary tree: kTourneySlots must be 2^kTourneyRounds");

// --- What an entrant is rolled as --------------------------------------------
// Levels are drawn UNIFORMLY across this band, and the bracket does the sorting: eight
// flat draws mean the operator's first match is an average opponent, their second the
// better of two, their final the best of four. So the difficulty ramp is a consequence
// of the format rather than a curve anyone had to author, and the roster stays a
// genuine spread instead of three rungs wearing different names.
constexpr int kTourneyMinLevel = 1;
constexpr int kTourneyMaxLevel = 60;

// The evolution stage a rolled level fields. An egg is never an entrant (it has no
// kit and no business in a ring), so the bands run Process → Script → Daemon: a level
// under the first is a Process, under the second a Script, and everything above is a
// Daemon. Bands rather than a stage roll of its own, because a level-50 Process would
// read as the arena not knowing what a pet is.
constexpr int kTourneyScriptLevel = 12;
constexpr int kTourneyDaemonLevel = 30;

// The Health fractions an entrant's own Exploit waits for, as a % of its max — rolled
// one per entrant (core/model/tournament.h). 100 is "opens with it": the fighter spends
// its first turn arming rather than swinging, which is a real cost and a readable tell.
// The rest are degrees of "waits until it is in trouble", so the same five crew
// abilities produce opponents that play them at four different moments.
constexpr int kTourneyTriggerPcts[] = {100, 60, 35, 20};
constexpr int kTourneyTriggerCount =
    sizeof(kTourneyTriggerPcts) / sizeof(kTourneyTriggerPcts[0]);

// How many turns a headless AI-vs-AI match is stepped before it is called on Health
// (core/model/tournament.h). Two brace-heavy kits can genuinely stalemate, and the
// bracket has to produce a winner for the operator to face either way — so this is a
// termination guarantee, not a balance number, and it is set far above any fight the
// engine actually resolves.
constexpr int kTourneyMatchTurnCap = 400;

// --- The purse ---------------------------------------------------------------
// Nothing pays until the whole bracket is taken. A won match is worth the next match,
// and the arena's whole reward is the title plus one lump — which is what keeps it a
// place to practise tactics rather than the fastest Bits in the game.
constexpr int kTourneyWinBits = 2048;
constexpr int kTourneyWinXp = 400;

// The kAreaList rung the arena is held on: The Pirate Bayou. It answers two questions
// at once, and deliberately with one number — REACHING that area is what opens the
// arena, and that area's own mod pool is what the purse rolls its prize mod from. An
// arena held in one water that gated on another, or paid out of another's table, would
// be two facts pretending to be one.
constexpr int kTourneyAreaIndex = 1;

// --- What the arena is called, and how it explains itself --------------------
// The event's name, in one place: the EXPL row, the bracket screen's header band, and
// the log lines a run writes all read it from here rather than each spelling it out.
extern const char* const kTourneyName;

// One section of the BRIEFING — the paged explainer the bracket screen opens on the
// A+C chord. A heading and its prose, which is the same NAME + PROSE shape STAT's
// LOADOUT page flows (core/ui/prose_page.h), so the briefing needs no layout of its own.
//
// It is CONTENT and not a comment because it is the only thing that tells an operator
// what they have walked into: an arena whose entrants, stakes and exit rules are all
// different from the ladder's, arriving as one unexplained row on a familiar list.
// Each section's prose is held to EffectText::kMaxProse like every other authored
// string on the device, and `test_tourney_brief_fits_its_page` is the gate.
struct TourneyBriefDef {
    const char* heading;
    const char* text;
};
extern const TourneyBriefDef kTourneyBrief[];
extern const int kTourneyBriefCount;

// --- The entrants' handles ---------------------------------------------------
// Operator handles, scene-flavoured (an entrant is another operator, not a creature,
// so these follow no creature or area standard — they are nicknames). Each fits
// kHackerTagMax, so one drops into a PvpFighter::tag unchanged.
//
// Drawn WITHOUT replacement across a bracket, so eight entrants are eight names; the
// pool is therefore kept comfortably longer than kTourneySlots.
extern const char* const kTourneyHandles[];
extern const int kTourneyHandleCount;

// The handle on row `index`, or "" out of range.
const char* tourneyHandleName(int index);

}  // namespace mal
