// deepweb_dive/area.h — the DEEPWEB DIVE's endless-scaling constants.
//
// The dive is an endless zone built off the tier-3 roster, opened by REACHING Net-Sea
// Crossing (kDeepWebUnlockAreaId) and tuned as the mid-game farm — the DARKWEB CRAWL is
// the terminal one. These constants are isolated here rather than in tunables.h since
// nothing outside the dive reads them.
#pragma once

namespace mal {

// Enemy level = petLevel + this (0 = parity, so wildWinXp pays full base XP at depth 0).
// What thickens the body is the stat budget below, not the level.
extern const int kDeepWebEnemyLevelOffset;
// What one Health point of that budget is worth. The other three stats are bought at the
// pet's own per-point rates (kLevelPowerPctPerPoint and friends), so an endless-zone enemy
// is a peer built the way the player was built; Health is the one the zone prices itself.
extern const int kDeepWebHealthPerLevel;

// Depth ramp (the dive's win-streak): without this a dive sits at flat pet-level
// parity forever. floorLog2(depth+1) turns the streak into a bonus "effective
// level" added on top of the pet's own level before the health/speed/XP scaling
// above is computed — logarithmic so early wins ramp fast while deep streaks
// flatten out (an endless zone must not runaway-scale). depth=0 -> +0 (flat
// parity). Depths below are measured from the END of kDeepWebRampFreeDepth's foothold
// (below), not from the first dive: free+7 -> +3*kDeepWebDepthLevelPerLog2; free+63 ->
// +6*kDeepWebDepthLevelPerLog2.
extern const int kDeepWebDepthLevelPerLog2;

// How many wins a dive gets BEFORE the depth ramp starts biting. The dive opens from
// NET-SEA CROSSING rather than from a cleared ladder, so an arriving pet is a Script with
// three move slots — and the log curve is at its steepest early, which is the wrong shape
// for that. This is the flat stretch that gives a shallow dive somewhere to stand. BOTH
// depth terms wait it out (the log level bonus and the linear points term alike), so
// inside it every dive is the same fight.
extern const int kDeepWebRampFreeDepth;

// The enemy's random stat spread as a PERCENTAGE of the pet's SURPLUS over
// kEndlessParLevel (area_defs.h) — not of its whole level. Spending against the whole
// level is what produced both of the zone's failures at once: the arriving pet met an
// endgame body with a full level of points on top of it, and the maxed pet met the same
// body with a budget its own stage growth had long outrun. The surplus is what a peer is.
//
// Measured over the five lines, 60 seeds each, line kits only, no mods, the A+C Exploit
// spent on the pet's best attack. A RUN from depth 0, since one loss ends a dive and a
// per-fight rate hides what that compounds to:
//
//   run length from depth 0        5    10    20    40    60    80
//   Script lv18   even            92%   84%   31%    0%    0%    0%
//   Script lv30   even            85%   74%   26%    0%    0%    0%
//   Daemon lv45   even           100%  100%  100%   87%    5%    1%
//   Daemon lv60   even           100%  100%  100%   96%   10%    2%
//   Daemon lv60   all-Defence     59%   45%   30%   17%    3%    0%
//   Daemon lv60   Power-heavy    100%  100%  100%   94%   12%    3%
//
// 300 runs a cell, so read the shape and not the third digit. The three properties this
// is tuned to hold: an arriving pet clears its first ten dives more often than not and is
// finished well before depth 40; a maxed pet has a failure point that is deep and
// REACHABLE rather than theoretical; and past par the curve is level-INVARIANT — lv45 and
// lv60 meet the same fight at the same depth, which is what makes the streak the score.
extern const int kDeepWebBudgetPct;

// Depth ramp, Bits half: wildWinXp already turns the depth-driven level bonus
// above into more XP (via the level-diff %), but the Bits payout
// (normalBitsReward, keyed to diffPips/stage-rank, not level) doesn't see that
// bonus without this. Mirrors the same logarithmic curve onto Bits directly: pct =
// 100 + floorLog2(depth+1) * kDeepWebDepthBitsPctPerLog2, clamped to
// kDeepWebDepthBitsMaxPct (see deepWebDepthBitsPct, combat.h).
extern const int kDeepWebDepthBitsPctPerLog2;
extern const int kDeepWebDepthBitsMaxPct;  // cap the bonus (endless-zone guard)

// Depth ramp, STAT half: one extra stat point in the dive's random spread
// (applyDeepWebScale) per this many depth PAST the foothold. LINEAR, unlike everything
// above it, and that is the whole job — the logarithmic ramp flattens into a fair fight a
// good build wins forever, so this is the term that ends a run, and the only one that
// does now that the level half answers the surplus rather than the level. Bigger = a
// gentler dive.
extern const int kDeepWebDepthPointsPerN;

// What a dive enemy KNOWS, by depth. The dive is the only zone whose kit is drawn rather
// than authored per enemy, because it is the only one with no roster left to author
// against — so its moves come from the pool the whole ladder already taught, handed back
// in rungs. This is also the answer to a turtle: a stacked wall is beaten by ARMOR PIERCE
// (which cuts the % reduction directly) and by DOT (which bypasses mitigation entirely,
// biting off Health at each turn-start), and the deep rungs are built out of both.
//
// deepWebMoveIds() picks from the rung `depth` has reached; see combat.h.
//
// BOSS SIGNATURES ARE GATED to kDeepWebBossMoveDepth and beyond. A boss is meant to be the
// first place its move is ever seen, and a zone that handed the same move out at depth 12
// would quietly retire the hunt the whole roster is built around. By 256 wins deep the
// player has long since had every boss on offer, and the dive is giving back what it was
// taught rather than front-running it.
extern const int kDeepWebBossMoveDepth;

// The dive's OWN pair — the Attack and the Defend its malbeasts field on top of whatever
// rung they drew, the same thing an AreaDef names in wildAttackMoveId/wildDefendMoveId.
// Named here rather than there for the reason everything else about the dive is: it has no
// row. They ride ALONGSIDE the rungs rather than inside them, so the dive's own two moves
// stay farmable at every depth including past kDeepWebBossMoveDepth, where the boss pool
// replaces the rung outright.
//
// A dive enemy therefore fights with three or four moves, reaching the same ceiling a
// ladder wild does (kMaxMoveSlots) but from its shallowest depth rather than its deepest.
// That ceiling is the zone keeping its own promise: the dive builds a PEER, and four is
// what a Daemon pet's own slots hold (kMoveSlotsByStage).
// The ladder depth the dive's OWN mod pool is authored at (ModDef::powerTier). It used
// to be read as "the deepest area's tier", which was true only while the dive was the
// terminal zone and the ladder stopped at the keep — a sixth area moved it, and every
// dive-only mod would have had to be re-ranked to chase it. It is a fact about the pool,
// so it is stated here rather than derived from a ladder the pool does not sit on.
extern const int kDeepWebModTier;

extern const char* const kDeepWebWildAttackMoveId;
extern const char* const kDeepWebWildDefendMoveId;
// The depth the Defend joins at — the Attack rides from the first dive, so the pair is
// the same shallow-one/deep-both weighting the ladder's areas use. Authored rather than
// derived: it happens to match rung 1's threshold today, and should be free to stop
// matching it without dragging the rung pacing along.
extern const int kDeepWebWildDefendDepth;

extern const char* const* const kDeepWebMoveRungs[];  // rung -> that rung's id list
extern const int kDeepWebMoveRungCounts[];            // ...and its length
extern const int kDeepWebMoveRungDepths[];            // ...and the depth it opens at
extern const int kDeepWebMoveRungTotal;               // how many rungs exist
extern const char* const kDeepWebMovesBoss[];         // the deep pool (see the gate above)
extern const int kDeepWebMovesBossCount;

}  // namespace mal
