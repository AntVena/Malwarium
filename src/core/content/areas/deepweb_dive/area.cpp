#include "core/content/areas/deepweb_dive/area.h"

#include "core/content/areas/area_defs.h"

namespace mal {

const int kDeepWebEnemyLevelOffset = 0;
const int kDeepWebHealthPerLevel = 6;
const int kDeepWebSpeedPerNLevels = 6;
const int kDeepWebDepthLevelPerLog2 = 2;
const int kDeepWebDepthBitsPctPerLog2 = 32;
const int kDeepWebDepthBitsMaxPct = 512;
const int kDeepWebDepthPointsPerN = 8;
const int kDeepWebBossMoveDepth = 256;

// The dive's move rungs. Each is what an enemy may KNOW from that depth on, drawn from
// (not added to) the pool — the rung replaces the roster kit rather than extending it, so
// a deep enemy fights like the depth it was found at and not like a tier-3 wild carrying
// souvenirs.
//
// The shape of the ramp is a widening answer to defence. Rung 0 is the roster's own kit.
// Rung 1 adds the plain heavy hitters, which a wall still absorbs. Rung 2 introduces
// PIERCE, the first thing that cares about the wall's size. Rung 3 introduces DOT, which
// does not care at all. Past that it is all three at once, which is where a build that
// answered only one of them stops working.
//
// Rungs are indexed by an authored DEPTH THRESHOLD each, not by the log curve the zone's
// level and Bits ramps share. Measured, not assumed: on the log curve the stun-and-rot
// rung landed at depth 7, and a rung that takes the pet's turns away is worth far more
// than the handful of stat points depth 7 also buys — a good build went from winning most
// dives to winning almost none across a single step. What a rung does to a fight is not
// proportional to what a stat point does, so it gets its own pacing.
const char* const kDeepWebMovesR0[] = {"packet_storm", "fork_bomb"};
const char* const kDeepWebMovesR1[] = {"packet_storm", "fork_bomb", "buffer_overflow",
                                       "rootkit_strike"};
const char* const kDeepWebMovesR2[] = {"buffer_overflow", "rootkit_strike", "fork_bomb",
                                       "system_hang", "decoy_download"};
const char* const kDeepWebMovesR3[] = {"buffer_overflow", "rootkit_strike", "system_hang",
                                       "data_rot", "nag_screen"};
const char* const kDeepWebMovesR4[] = {"rootkit_strike", "data_rot", "nag_screen",
                                       "system_hang", "decoy_download"};
// From kDeepWebBossMoveDepth on: everything the ladder's bosses teach, which is where the
// dive stops being a harder version of the same fight. Pierce that ignores the wall
// outright, rot that never touched it, control, and the two wind-up nukes.
const char* const kDeepWebMovesBoss[] = {
    "crack_the_keys", "backdoor_knock", "wild_card",      "domain_flux",
    "mail_merge",     "runaway_fork",   "mail_storm",     "attachment_bait",
    "popup_storm",    "admin_reversal", "false_positive", "nuked_release",
};

const char* const* const kDeepWebMoveRungs[] = {
    kDeepWebMovesR0, kDeepWebMovesR1, kDeepWebMovesR2, kDeepWebMovesR3, kDeepWebMovesR4,
};
// The depth each rung opens at. Rung 0 from the first dive; the rest spaced so a run has
// room to settle into one answer before the next thing it doesn't answer shows up.
//
// What this pacing buys, measured at pet level 60 over 80 seeded fights per depth —
// win rate by build, which is the shape the zone is FOR:
//
//            depth      0    32    64   128   256   700  1200  2400
//   all-Defence        10%    0%    0%    0%    0%    0%    0%    0%
//   even spread        77%   21%    5%    0%    0%    0%    0%    0%
//   spread + boss kit 100%  100%   62%   50%   33%   12%    3%    0%
//
// Two things that had to be true are: the turtle is the WORST of the three rather than
// the best, and the loadout is worth roughly twenty times more depth than the stat spread
// is. Every column ends at zero, which is the zone keeping its promise.
const int kDeepWebMoveRungDepths[] = {0, 12, 40, 100, 180};
const int kDeepWebMoveRungCounts[] = {
    arrLen(kDeepWebMovesR0), arrLen(kDeepWebMovesR1), arrLen(kDeepWebMovesR2),
    arrLen(kDeepWebMovesR3), arrLen(kDeepWebMovesR4),
};
const int kDeepWebMoveRungTotal = arrLen(kDeepWebMoveRungs);
const int kDeepWebMovesBossCount = arrLen(kDeepWebMovesBoss);

// The dive's EXPL row glyph. The terminal zone is no AreaDef, so it names its own
// art here rather than inheriting the ICON_SECTOR_<AREA_ID> convention off a row it
// doesn't have — same reason its pool and scaling constants live here.
const char* const kDeepWebIcon = "ICON_SECTOR_DEEPWEB_DIVE";

// The dive's mod pool — earned from milestone wins in the endless zone, the
// defensive/build-around endgame Epics.
const char* const kAreaModsDeepWeb[] = {
    "deadman_switch", "raid_mirror",  "ecc_memory",       "load_balancer",
    // One hard-gated line build-around per line — the dive is where all four live, so
    // reaching it is what opens the deep end of every line at once.
    "phishing_rod",    "extortion_ledger", "ring_zero_shim", "replication_bus",
    "backup_uplink",
};
const int kAreaModsDeepWebCount = arrLen(kAreaModsDeepWeb);

// The dive's WILD-win drop table. Same staple set every area's row carries, plus the
// dive's own exclusive Merge Hub ingredient — the terminal zone is no AreaDef, so its
// pool is named here for the same reason its glyph and scaling constants are.
const LootEntry kWildLootDeepWeb[] = {
    {"airgap_snack"}, {"tortilla_chip"}, {"backup_drive"}, {"disk_scrubber"},
    {"pwnzu_sauce"},
};
const int kWildLootDeepWebCount = arrLen(kWildLootDeepWeb);

}  // namespace mal
