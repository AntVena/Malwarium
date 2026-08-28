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

// The side as words: the short NAME every surface shows beside the glyph ("RED"), and
// the DOCTRINE the side's own screen leads with ("OPERATORS"). Both live here rather
// than in the screen that happens to draw them, because the axis is content — CREW,
// LINK and PEERS all show a side, and three private copies of the vocabulary is three
// places for it to drift.
const char* crewTeamName(CrewTeam team);
const char* crewTeamDoctrine(CrewTeam team);

// A side's roster, as a FILTER over kCrews rather than an ordering constraint on it:
// how many crews sit on `team`, and the kCrews index of the `nth` of them (-1 past the
// end). So a new crew goes wherever it reads best in the table below and the side's
// screen still finds it, with no grouping to keep in sync.
int crewTeamCount(CrewTeam team);
int crewByTeam(CrewTeam team, int nth);

// The effect vocabulary for a crew's signature Exploit. One entry per mechanic;
// Combat::applyCrewExploit is the single applier, and each kind meters itself out of
// the shared CrewExploitState counters (charges / turns) rather than its own field.
//
// Three metering shapes, and every kind is one of them: CHARGE-metered (spends itself
// on events), TURN-metered (spends itself on turns), or STICKY (fires once and then
// simply holds for the rest of the fight, counting nothing — see crewExploitIsSticky).
enum class CrewExploitKind : uint8_t {
    None,
    // CHARGE-metered: fully negate the next `magnitude` incoming attacks, one charge
    // each — the same seam a RAID Mirror / Backup Drive one-shot uses
    // (Combat::applyEffect), so a negated hit also drops the attack's stun/DoT rider.
    NegateNextHits,
    // CHARGE-metered: each of the next `magnitude` LANDED attacks banks its own final
    // damage as Power for the rest of the fight. Uncapped and compounding — the bigger
    // swing one charge buys is what the next charge banks.
    PowerByDamageDealt,
    // STICKY: snap the live stat LEANS (attack power, speed, damage cut) back to their
    // fight-start baselines, then floor them there — nothing may lower them again.
    ResetStatsAndFloor,
    // STICKY: every self-buff the ENEMY casts (brace, shield pool, Lockout/Cipher
    // stack) is copied onto you as it lands.
    MirrorEnemyBuffs,
    // TURN-metered one-shot: while it has turns left, a blow that would put the pet
    // under 0 instead leaves it at half of max Health and pays the overkill back as
    // Power, scaled by the turns still on the clock. Firing consumes it outright.
    DeathSaveRally,
    // TURN-metered: while it has turns left, every attack this fighter LANDS takes
    // kCrewRakePct of its own final damage back as Health. Metered on the holder's own
    // turns rather than its opponent's (Combat::tickCrewExploitClock), because what is
    // sold is a stretch of the holder's OWN swings — a turn that lands nothing simply
    // pays nothing and the clock runs anyway.
    LeechOnHit,
    // CHARGE-metered: each of the next `magnitude` casts resolves a SECOND time, against
    // the same target, immediately (Combat::resolveTurn's tail). One charge is one
    // duplicated cast, not one duplicated turn: the turn-start ticks, the tempo refund
    // and the chain hand-off all belong to the turn and happen once.
    //
    // Line-agnostic on purpose, and it is what makes the ability reach further than it
    // reads. A cast is where every LinePassive actually fires (defs.h), so a second cast
    // arms a second Trojan trap, rolls a second Worm replica and re-rolls a Metamorphic
    // wildcard — without the turn engine ever asking which line it is standing in front
    // of, which is the rule that set names its own hook.
    SpareFailover,
};

// The share of a landed hit's final damage that LeechOnHit takes back as Health. A
// property of the KIND and not of any crew — every crew fielding a rake takes the same
// cut — so it lives beside the vocabulary rather than on a crew's row, which carries
// only that crew's own number (the turn count).
constexpr int kCrewRakePct = 40;

// The short mechanic word for a kind, used everywhere the effect is surfaced (the
// picker row's tag, the commit popup, the mid-combat stat panel) so all three can never
// drift. It is the grayscale-safe channel for the effect — never a colour.
const char* crewExploitTag(CrewExploitKind kind);

// Whether a kind, once fired, simply HOLDS for the rest of the fight. A sticky kind
// meters out of neither counter, so the armed `kind` alone is its whole state and there
// is no number to print beside its tag.
bool crewExploitIsSticky(CrewExploitKind kind);

// The one readout formatter for an Exploit: "<TAG> xN", or a bare "<TAG>" when the
// count is 0 (a sticky kind has nothing to count). Every surface that shows an Exploit
// — the CREW row, the combat picker row, the commit popup, the mid-combat stat panel —
// goes through this, so none of them can word it differently from the others.
void crewExploitLabel(char* out, int cap, CrewExploitKind kind, int count);

// A crew's signature Exploit. Nested on CrewDef rather than inlined as `exploit*`
// fields so a crew granting more than one becomes an array here, not a rename
// everywhere. `magnitude` is the kind's one number and lives on this row, not in
// tunables.h — nothing outside this crew reads it. A STICKY kind counts nothing, so
// its magnitude is 0 and the readout drops the "xN" rather than printing a hollow one.
struct CrewExploitDef {
    const char* name;           // picker row label ("DENIAL OF SERVICE")
    CrewExploitKind kind;
    int magnitude;
    // What it does, in a sentence, for the CREW detail page — a TEMPLATE over this row
    // (effect_text.h): write `{mag}` where the magnitude goes, never a digit, so
    // retuning the number retunes the sentence. A kind with no number to cite writes
    // pure prose; the derived "<TAG> xN" readout beside it carries the arithmetic.
    const char* desc;
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
