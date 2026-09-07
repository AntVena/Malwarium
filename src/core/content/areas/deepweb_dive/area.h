// deepweb_dive/area.h — the DEEPWEB DIVE's endless-scaling constants.
//
// The dive is the always-last EXPL row, unlocked once every real area is cleared:
// an endless, level-scaling terminal zone built off the tier-3 roster, then
// thickened per pet level so it never trivialises no matter how strong the pet
// gets. These constants are isolated here (rather than tunables.h) since nothing
// outside the dive reads them — the endgame grind rate can be tuned without
// touching normal explore.
#pragma once

namespace mal {

// Enemy level = petLevel + this (0 = parity, so wildWinXp pays full base XP at
// depth 0). Health/speed then thicken per pet level so the fight tracks the pet's
// own stat growth instead of trivialising as it levels.
extern const int kDeepWebEnemyLevelOffset;
extern const int kDeepWebHealthPerLevel;    // +Health per pet level
extern const int kDeepWebSpeedPerNLevels;   // +1 enemy speed every N pet levels

// Depth ramp (the dive's win-streak): without this a dive sits at flat pet-level
// parity forever. floorLog2(depth+1) turns the streak into a bonus "effective
// level" added on top of the pet's own level before the health/speed/XP scaling
// above is computed — logarithmic so early wins ramp fast while deep streaks
// flatten out (an endless zone must not runaway-scale). depth=0 -> +0 (flat
// parity). Depths below are measured from the END of kDeepWebRampFreeDepth's foothold
// (below), not from the first dive: free+7 -> +3*kDeepWebDepthLevelPerLog2; free+63 ->
// +6*kDeepWebDepthLevelPerLog2.
extern const int kDeepWebDepthLevelPerLog2;

// How many wins a dive gets BEFORE the depth ramp above starts biting. The dive opens
// from NET-SEA CROSSING rather than from a cleared ladder, so an arriving pet is a
// Script with three move slots rather than a Daemon with a full kit — and the log curve
// is at its STEEPEST early, which is exactly the wrong shape for that. This is the flat
// stretch that gives a shallow dive somewhere to stand.
//
// Measured with the constant below, over the five lines (60 seeds each, line kits only,
// no mods) — a RUN from depth 0, since one loss ends a dive and a per-fight rate hides
// what that compounds to. The arriving pet is the case being fixed:
//
//   run length from depth 0        5    10    20    40
//   Script lv18   before          44%   12%    0%    0%
//   Script lv18   after           85%   74%   19%    0%
//   Daemon lv60   before          98%   97%   91%   72%
//   Daemon lv60   after          100%  100%   99%   99%
//
// 300 runs a cell (five lines x 60 seeds), so a couple of points either way is sampling
// noise and not a curve — read the shape, not the third digit.
//
// So an arriving pet now clears its first ten fights more often than not, is still
// finished well before depth 40, and per-fight win rate at pet level 60 still falls to
// ~22% by depth 1000 — the zone ends the way it always did. The endgame column going
// flat is the cost, and it is deliberate: the dive stops being the terminal zone once it
// opens this early, and being an unthreatening farm is the job it is left with.
extern const int kDeepWebRampFreeDepth;

// The enemy's random stat spread as a PERCENTAGE of its effective level. A dive enemy
// already brings a tier-3 body and the wild challenge buff on top of its points, so
// spending a full level's worth of them made it a peer PLUS two advantages. Under 100 is
// what makes a shallow dive a fight the arriving pet is favoured in; the depth term
// beside it is untouched, so the zone still ends the way it always did.
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
// (applyDeepWebScale) per this many depth. LINEAR, unlike everything above it, and that
// is the whole job — the logarithmic ramp flattens into a fair fight a good build wins
// forever, so this is the term that eventually ends a run. Bigger = a gentler dive.
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
