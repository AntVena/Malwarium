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

}  // namespace mal
