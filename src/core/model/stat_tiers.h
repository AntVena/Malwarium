// stat_tiers.h — the investment LADDER, as one table.
//
// Every combat stat unlocks three discrete tiers, at the same three point counts
// (kStatTier1Points / 2 / 3, tunables.h). The MAGNITUDES live in tunables.h beside the
// curves they sit on; what lives here is the other half — which rung belongs to which
// stat, what it is called, and the sentence that explains it — plus the two questions
// every consumer asks of a point count ("how many rungs is that" / "how far to the next").
//
// ONE table, because there are two consumers and they must not be able to disagree. The
// engine resolves a fighter's tiers when it builds a Combatant (combat_factory.cpp), and
// the STAT screen's TIERS page draws the same twelve rows to tell the player what they
// have and what the next one costs. A screen that listed the rungs from its own copy
// would be a second answer to "what does committing to Speed get me", and the two would
// drift the first time a magnitude moved.
//
// Adding a tier to a stat: a row here, its magnitude in tunables.h, the field it sets on
// Combatant, and the place in the fight that reads that field. The STAT page needs no
// edit — it renders whatever this table holds.
#pragma once

#include "core/content/effect_text.h"   // EffectText — the rendered row prose
#include "tunables.h"

namespace mal {

// The stat axis, named. Same order as Game::statPoints_, PetUpgrades::statBonus and
// game_internal.h's levelStatName — 0 power · 1 defense · 2 speed · 3 max-Health — and
// an enum rather than a loose int so a table indexed on it cannot be silently reordered.
enum class LevelStat : uint8_t { Power = 0, Defense = 1, Speed = 2, Health = 3 };

// One rung. `effect` is a TEMPLATE over this row's own two magnitudes, the same
// convention every content description follows (effect_text.h): `{mag}` and `{mag2}`
// substitute, so retuning the number in tunables.h retunes the sentence with it.
struct StatTierDef {
    const char* name;    // the rung's own short name, as the STAT page lists it
    int points;          // the point count that unlocks it
    int mag;             // {mag} — 0 when the row's prose names no number
    int mag2;            // {mag2}
    const char* effect;  // what it does, in one sentence
};

// The whole 4x3 grid, in stat order then rung order. Borrowed — it is a static table.
const StatTierDef& statTier(LevelStat stat, int tier);

// `tier`'s point threshold (tier 0..kStatTierCount-1). Identical on every stat by
// construction; taking the tier index rather than a stat is the type saying so.
int statTierPoints(int tier);

// How many rungs `points` has reached, 0..kStatTierCount. The one function that decides
// whether a tier is live; every applier and the STAT page both go through it, so
// "unlocked" cannot mean one thing in a fight and another on the screen.
int statTiersReached(int points);

// Points still owed on the next rung, or 0 once the last one is reached — what the STAT
// page's "N TO GO" reads, and the reason the ladder is worth drawing at all.
int statTierPointsToNext(int points);

// A rung's prose with its own magnitudes substituted in.
EffectText statTierText(LevelStat stat, int tier);

}  // namespace mal
