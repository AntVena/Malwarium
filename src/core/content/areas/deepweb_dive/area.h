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
// parity); depth=7 -> +3*kDeepWebDepthLevelPerLog2; depth=63 ->
// +6*kDeepWebDepthLevelPerLog2.
extern const int kDeepWebDepthLevelPerLog2;

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

extern const char* const* const kDeepWebMoveRungs[];  // rung -> that rung's id list
extern const int kDeepWebMoveRungCounts[];            // ...and its length
extern const int kDeepWebMoveRungDepths[];            // ...and the depth it opens at
extern const int kDeepWebMoveRungTotal;               // how many rungs exist
extern const char* const kDeepWebMovesBoss[];         // the deep pool (see the gate above)
extern const int kDeepWebMovesBossCount;

}  // namespace mal
