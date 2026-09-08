// darkweb_crawl/area.h — the DARKWEB CRAWL's endless-scaling constants.
//
// The second endless zone, and the terminal one. It is reached through the portal at THE
// SILK LODE's ZERO DAY SHRINE. The Dive opens at Net-Sea Crossing and is tuned as the
// mid-game farm (deepweb_dive/area.h); this is where the map ends.
//
// You DIVE the deep web, because down is the only direction it has. You CRAWL the dark
// web, because you are on a thread and a crawler is what walks one.
//
// What makes it the Lode's own rather than a harder Dive is that its fights run with the
// A+C picker permanently scrambled — the area's apex rider (MoveDef::scrambleTurns)
// never lifted. Down here nothing has a name you can read unless you learned to read it,
// so the Cant stops being a guardian's minigame and becomes the entry ticket.
#pragma once

#include "core/content/defs.h"  // LootEntry — the crawl's wild-win drop table

namespace mal {

// Enemy level = petLevel + this, then the depth ramp on top. 0 = parity on the first
// fight, exactly as the Dive opens.
extern const int kDarkWebEnemyLevelOffset;

// Depth ramp: floorLog2(depth + 1) * this, added as a bonus "effective level". No
// foothold term, unlike the Dive — the Crawl is entered by a pet that has cleared the
// whole ladder, and a place that hands one of those a flat stretch to stand on is not
// the terminal zone, it is a second farm.
extern const int kDarkWebDepthLevelPerLog2;

// The random stat spread, as a percentage of the pet's SURPLUS over kEndlessParLevel
// (area_defs.h, the frame both endless zones roll their tier-3 body against), and the
// linear depth term that ends a run. The crawl's harshness is not in these — they are the
// dive's own — it is in having no foothold and in spending the linear term twice as fast.
//
// Measured the way the dive's table is, over the five lines with the picker SCRAMBLED and
// no sigils, which is what the zone actually hands a player:
//
//   run length from depth 0        5    10    20    40    60    80
//   Daemon lv45   even            81%   55%   22%    8%    4%    0%
//   Daemon lv60   even            82%   61%   31%   11%    3%    0%
//   Daemon lv60   all-Defence     12%    6%    1%    0%    0%    0%
//   Daemon lv60   Power-heavy     86%   66%   34%   16%    6%    3%
//
// ...against the dive's 100% / 100% / 100% / 96% for the same lv60 pet: the crawl is
// harsher at every depth and its median run is roughly a third of the dive's reach.
//
// WHAT THE SCRAMBLE IS WORTH, since the zone's whole claim is that fluency is power. The
// same lv60 even build with a READABLE picker runs 89% / 75% / 46% / 10% — so knowing the
// Cant is worth about half again as much depth, and rather more than that to a build that
// lives on forcing one big hit (the Power-heavy row goes 34% -> 66% at twenty).
extern const int kDarkWebBudgetPct;
extern const int kDarkWebDepthPointsPerN;

// Bits half of the depth ramp, and its ceiling — the Crawl pays deeper than the Dive on
// the same curve shape, because a zone that takes the override away has to be worth
// entering without one.
extern const int kDarkWebDepthBitsPctPerLog2;
extern const int kDarkWebDepthBitsMaxPct;

// A SIGIL every this-many depth, the reason the zone exists beyond its mood. A sigil
// otherwise costs a captured WPA handshake (Game::buySigil), which needs the radio on and
// a network in range — so on a device that never sees one, the Cant cannot be advanced at
// all. This is the second door onto it, and a fitting one: the zone that demands fluency
// is the zone that teaches it.
extern const int kDarkWebSigilEveryN;

// Its EXPL row glyph. No AreaDef, so it names its own art (the Dive's kDeepWebIcon does
// the same, and for the same reason).
extern const char* const kDarkWebIcon;

// Its two pools, the stand-ins for AreaDef::modPoolIds and ::wildLootPool.
extern const char* const kAreaModsDarkWeb[];
extern const int kAreaModsDarkWebCount;
extern const LootEntry kWildLootDarkWeb[];
extern const int kWildLootDarkWebCount;

}  // namespace mal
