// tournament.h — THE COMPO, the pure half: a seed in, a bracket out.
//
// A Compo run is ONE 32-BIT SEED plus a survivor bitmask. Every entrant — its handle,
// species, level, stat spread, move kit, mods and the Exploit it will fire — is
// DERIVED from (seed, slot) rather than stored, so a whole bracket costs four bytes to
// persist and a run reloads byte-identical after a reboot. That is the same trick a
// linked duel uses to avoid broadcasting turns (core/model/pvp_battle.h), pointed at
// storage instead of at the radio.
//
// It also decides the matches the operator never watches. The other first-round
// pairings are resolved by STEPPING THE REAL ENGINE headlessly (tourneyResolveMatch) —
// not by comparing stat totals — so the bracket the operator climbs was genuinely won
// by the fighters in it, Exploits and all. Determinism is what makes that safe to do
// lazily: re-resolving the same round from the same seed always names the same winner.
//
// PURE + HOST-TESTABLE: no Game, no screen, no radio. Feed it a registry and a seed.
#pragma once

#include <cstdint>

#include "core/content/content_tournament.h"
#include "core/model/combat.h"
#include "core/net/pvp_link.h"   // PvpFighter — the fighter RECIPE both this and a duel build from

namespace mal {

class ContentRegistry;

// What the bracket list can show without touching the registry: an entrant's handle
// and the level it was rolled at. Derived from the same stream the full fighter is, so
// the two can never disagree about who slot 3 is — the builder below calls this for
// its own first two fields rather than re-rolling them.
struct TourneyCard {
    const char* handle = "";
    int level = 0;
};

// One entrant, in full. `spec` is deliberately the DUEL's fighter recipe: species,
// equipped moves, equipped mods, level and the four per-level stat points, resolved
// against the local registry by the one shared builder (makePvpCombatant). Reusing it
// means a Compo opponent and a real operator's pet are built by identical code, so an
// arena fight can never quietly apply a rule the rest of the game doesn't.
struct TourneyFighter {
    PvpFighter spec;
    CrewExploit exploit;              // the Exploit this entrant fires on its own
    int exploitAtHealthPct = 100;     // ...at this % of its max Health (100 = opens with it)
};

// --- Deriving the bracket ----------------------------------------------------

// Which slot the OPERATOR occupies, derived from the seed like everything else — so
// the draw is a fact about the bracket rather than a separate thing to persist.
int tourneyPlayerSlot(uint32_t seed);

// Entrant `slot`'s handle + level. Cheap (no registry, no allocation) — this is what
// the bracket screen walks every repaint.
TourneyCard tourneyCard(uint32_t seed, int slot);

// Entrant `slot` in full, resolved against `reg`. Allocates (it walks the content
// tables), so build one when a match needs it rather than per frame.
TourneyFighter tourneyEntrant(const ContentRegistry& reg, uint32_t seed, int slot);

// Build the fighting form of an entrant: the shared duel build, plus the Exploit it
// will arm itself with (Combatant::autoExploit). The one place those two halves are
// joined, so the operator's live opponent and a headless AI-vs-AI winner are the same
// fighter — an opponent that only used its Exploit when somebody was watching would be
// a different opponent from the one that won its way up the bracket.
Combatant makeTourneyCombatant(const ContentRegistry& reg, const TourneyFighter& f);

// --- Resolving matches -------------------------------------------------------

// Step a full match between two entrants with nobody at the buttons and report whether
// `a` won. Neither side gets an override allowance (there is no human to open a
// picker), but both still fire their own Exploits on their own triggers.
//
// A match that has not resolved by kTourneyMatchTurnCap is CALLED on the Health
// fraction, and a dead tie goes to `a`. Two brace-heavy kits can genuinely stalemate
// and the bracket owes the operator an opponent either way — so this terminates by
// construction rather than by trusting every kit pairing to be decisive.
bool tourneyResolveMatch(const ContentRegistry& reg, const TourneyFighter& a,
                         const TourneyFighter& b, uint32_t seed);

// --- The bracket tree --------------------------------------------------------
// Single elimination over kTourneySlots fixed slots. Round r pairs the survivors
// within each block of 2^(r+1) slots: exactly one is alive in each half of a block, so
// a block names a match without any pairing having to be stored. `alive` is a bitmask
// over the original slots, and it is the whole of a run's mutable state.

// The slots one match spans at `round` (2 in the first round, 4 in the second, …).
inline int tourneyBlockSize(int round) { return 1 << (round + 1); }

// The first slot of the block `slot` sits in at `round`.
inline int tourneyBlockStart(int slot, int round) {
    const int n = tourneyBlockSize(round);
    return (slot / n) * n;
}

// The alive slot `slot` faces at `round`, or -1 if its block holds no other survivor
// (a malformed bitmask, or a round past the final).
int tourneyOpponentSlot(uint8_t alive, int slot, int round);

// The seed for the match in `round` whose block starts at `blockStart`. Stable, so
// resolving a round twice always reaches the same winners.
uint32_t tourneyMatchSeed(uint32_t seed, int round, int blockStart);

// Resolve every match of `round` EXCEPT the operator's own, clearing each loser's bit
// from `alive`. The block holding `playerSlot` is left untouched — that one is fought
// on the screen. Called once as a round opens, so the list the operator reads is the
// list of who is actually still standing.
void tourneyResolveRound(const ContentRegistry& reg, uint32_t seed, int round,
                         int playerSlot, uint8_t& alive);

// How many slots are still alive in `alive` (the bracket list's "n LEFT" readout).
int tourneyAliveCount(uint8_t alive);

}  // namespace mal
