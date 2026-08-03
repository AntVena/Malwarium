// content_crews.h — the Hacker-face CREW roster: the crews an operator can enlist in.
//
// A crew is a PLAYER-level allegiance: it outlives any one pet, like the HackerTag and
// the earned Titles. Enlisting requires a HOME NETWORK — the network the operator has
// designated as theirs (Game::setHomeNetwork, picked from the SD-backed NetworkLedger in
// the CREW screen) — so a crew member is always somebody's defender. Game::joinCrew
// enforces that gate; Game::hasHomeNetwork is the check.
//
// Each crew carries a signature combat Exploit, offered as an extra row in the A+C
// Exploit picker while the player belongs to it (Combat::CrewExploit, combat.h). Its
// behaviour is an effect KIND + a magnitude on its own nested row — a new crew ability
// is one enum entry, one case in Combat::applyCrewExploit, and one tag in
// crewExploitTag(), never a per-crew branch in combat.
//
// GROWTH SHAPE — read before adding a dozen crews:
//   * A crew is ~6 lines, so they all live in ONE content_crews.cpp. Split it only if it
//     actually gets unskimmable (the same ~600-line instinct as the game_*.cpp units);
//     an area got its own folder because an area is ~60 lines, a crew is not.
//   * The Exploit is NESTED (CrewExploitDef), not three `exploit*` fields inline, so a
//     crew that one day grants two of them is an array on this row rather than a rename
//     across every row.
//   * Per-fight state for an armed Exploit lives in ONE CrewExploitState block on the
//     Combatant (combat.h), NOT a fresh int per ability — a charge-metered or
//     turn-metered kind reuses the counters already there, so twenty crews add zero
//     fields to Combatant.
//
// Compiled-in and INDEX-addressed, like the EXPL areas (content/areas/area_defs.h) and
// unlike the id-keyed registry types: every call site is "the crew on row i" of the CREW
// list, so the table plus a crew(i) accessor is the whole mechanism. The SAVE stores the
// crew's `id` string (resolved back through crewIndexById on load), so rows may be
// reordered or inserted freely without invalidating a save.
#pragma once

#include <cstdint>

namespace mal {

// Which side of the Red/Blue split a crew sits on. Blue = Guardians (defensive,
// coordinated); Red = Operators (offensive). Dual-coded in the UI by the word plus
// ICON_TEAM_BLUE / ICON_TEAM_RED, never colour alone.
enum class CrewTeam : uint8_t { Blue, Red };

// The effect vocabulary for a crew's signature Exploit. One entry per mechanic;
// Combat::applyCrewExploit is the single applier, and each kind meters itself out of
// the shared CrewExploitState counters (charges / turns) rather than its own field.
enum class CrewExploitKind : uint8_t {
    None,
    // CHARGE-metered: fully negate the next `magnitude` incoming attacks, one charge
    // each — the same seam a RAID Mirror / Backup Drive one-shot uses
    // (Combat::applyEffect), so a negated hit also drops the attack's stun/DoT rider.
    NegateNextHits,
};

// The short mechanic word for a kind, used everywhere the effect is surfaced (the
// picker row's tag, the commit popup, the mid-combat stat panel) so all three can never
// drift. It is the grayscale-safe channel for the effect — never a colour.
const char* crewExploitTag(CrewExploitKind kind);

// A crew's signature Exploit. Nested on CrewDef rather than inlined as `exploit*`
// fields so a crew granting more than one becomes an array here, not a rename
// everywhere. `magnitude` is the kind's one number and lives on this row, not in
// tunables.h — nothing outside this crew reads it.
struct CrewExploitDef {
    const char* name;           // picker row label ("DENIAL OF SERVICE")
    CrewExploitKind kind;
    int magnitude;
};

struct CrewDef {
    const char* id;             // stable save id ("deniers_of_service")
    const char* displayName;    // CREW list heading
    const char* tagline;        // the crew's motto, shown under the name
    CrewTeam team;
    CrewExploitDef exploit;
};

extern const CrewDef kCrews[];
extern const int kCrewCount;

// The crew on row `index`, or nullptr when out of range (so "no crew" callers can
// pass -1 straight through).
const CrewDef* crew(int index);

// Row index for a save id; -1 for an empty/unknown id (a save written against a crew
// that no longer exists loads as "no crew" rather than a wrong one).
int crewIndexById(const char* id);

}  // namespace mal
