// darkweb_crawl/area.h — the DARKWEB CRAWL's endless-scaling constants.
//
// The second endless zone, and the terminal one. It is reached through the portal at THE
// SILK LODE's ZERO DAY SHRINE, and it is what the DeepWeb Dive used to be: the Dive now
// opens at Net-Sea Crossing and is tuned as the mid-game farm (deepweb_dive/area.h), so
// the numbers below are the ones the Dive was measured at before that — no foothold, and
// a full level's worth of stat points on every enemy.
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

// The random stat spread, as a percentage of effective level, and the linear term that
// eventually ends a run. 100 is the point: a Crawl enemy spends a FULL level's worth of
// points, on top of its tier-3 body and the wild challenge buff.
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
