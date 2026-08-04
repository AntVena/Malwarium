// area_defs.h — the EXPL area ladder, one AreaDef per folder under
// src/core/content/areas/.
//
// An AreaDef is everything that makes one explorable area of the linear ladder
// itself: its name/Title, its 5 sub-area names, its 5 sub-area boss names +
// the area-boss banner, its storefront, and its mod-loot pool. Find an area's
// folder (areas/<name>/area.cpp) to change anything about that area; nothing about
// one area's identity is split across another file.
//
// What an AreaDef deliberately does NOT carry is its DIFFICULTY: that is areaTier()
// below, derived from ladder position. Order is the only statement of depth, so the
// list can be reordered or spliced without re-tuning a row.
//
// kAreaList is the ONE place the ladder's order/count is declared: kAreaCount is
// sizeof(kAreaList)/sizeof(...), so adding an area is "write its area.cpp, add one
// line here" — there is no second count anywhere else to fall out of sync with.
// UI (expl_screen.h's kExplSectors) and the model layer (combat.h) both read the
// constants here directly rather than hand-typing their own copies — area identity
// lives in the content layer, which both UI and model depend on without depending
// on each other.
//
// DeepWeb Dive (the terminal endless zone) has no sub-ladder or boss and so isn't
// an AreaDef — see the plain declarations at the bottom, defined in
// areas/deepweb_dive/area.cpp.
#pragma once

namespace mal {

constexpr int kSubAreasPerArea = 5;

// A helper for each area.cpp to size its own mod-pool array without repeating the
// count by hand (same trick as game_internal.h's poolN).
template <int N>
constexpr int arrLen(const char* const (&)[N]) { return N; }
// Generic sibling for arrays of non-`const char*` rows (ShopListingDef, below).
template <typename T, int N>
constexpr int arrLen(const T (&)[N]) { return N; }

// Runtime cap for a storefront's listing count (Game::shopStockLeft_'s sizing,
// game.h) — headroom above any shipped area's listing count. A plain fixed-size
// row list, not a second ladder, so unlike kAreaCount there is no derived count to
// keep in sync — just don't let one area's listings[] grow past this.
constexpr int kMaxShopListings = 6;

// Max additional item costs a single storefront listing can carry, beyond its Bits
// price — headroom so a listing (e.g. RAID Mirror: 50 Backup Drives + 128 Bits)
// doesn't need a variable-length type.
constexpr int kMaxShopCostItems = 3;

// One extra item-cost component on a storefront listing (beyond its bitsPrice) —
// consumed from the player's inventory at purchase (game_explore.cpp's
// buyShopItem). `itemId` nullptr = unused slot.
struct ShopItemCost {
    const char* itemId = nullptr;
    int qty = 0;
};

// One row of a storefront (game_explore.cpp's buyShopItem/shopStatusLine,
// game_render.cpp's drawShopScreen): sells `id` — an ItemDef id in an item shop, a
// ModDef id in a mod shop; the storefront kind (AreaStorefrontDef, below) picks which
// registry resolves it. Stocked to `stock` per visit, priced in Bits (`bitsPrice`)
// plus 0..kMaxShopCostItems additional item costs (`costs`) consumed from the bag on
// purchase. No separate description string — the row's blurb is `id`'s OWN
// ItemDef::effect/ModDef::effect (the same concrete-effect text shown elsewhere),
// resolved through the registry rather than duplicated here. `ItemDef::bitsPrice`
// (defs.h) is a SEPARATE, item-owned reference price (what the 'Pedia lists, and the
// generic "is this item shop-sellable at all" signal) — the two can diverge on
// purpose; nothing keeps them in lockstep.
struct ShopListingDef {
    const char* id;
    int stock;
    int bitsPrice;
    ShopItemCost costs[kMaxShopCostItems] = {};
};

// One storefront: an arbitrary-length list of rows, each pricing/describing itself
// (src/core/content/CONTENT_STANDARD.md rule 2 — no shared 2-row cap to hand-maintain). Shared
// shape for both an area's item shop (e.g. Byte to Eat) and its mod shop — `listings`/
// `listingCount` point at that area's own array in its area.cpp. `stock` refills from
// the row's own ShopListingDef::stock each visit (game_explore.cpp's
// startShopEvent/startModShopEvent) — not persisted.
struct AreaStorefrontDef {
    const char* name;
    const ShopListingDef* listings;
    int listingCount;
};

struct AreaDef {
    const char* id;    // stable id, e.g. "citrus_circuit" (matches the folder name)
    const char* name;  // display name, e.g. "CITRUS CIRCUIT"
    const char* title; // zone-completion Title granted on clearing this area's gauntlet
    // The asset NAME (no path, no extension) of this area's sector glyph. Keyed off the
    // area's own id rather than its rung, because the rung is not an identity: splicing
    // an area into the middle of kAreaList renumbers every area above it, and an
    // index-keyed art family would silently re-point at its neighbour's picture while
    // still resolving. Naming it here is also what keeps the art compiled —
    // tools/check_orphan_assets.py counts a row that names an asset as its consumer.
    const char* icon;
    const char* subAreas[kSubAreasPerArea];      // 5 named stretches
    const char* subBossNames[kSubAreasPerArea];  // 5 sub-area boss names (sub 4 = signature)
    const char* areaBossName;                    // the area gauntlet's overall banner
    // The signature (sub 4) boss's extra rider move, debuting the area's THREAT that
    // its own loot table's counter-mod answers (e.g. Pirate Bayou's system_hang stun,
    // countered by that same area's Watchdog Timer drop). nullptr = no rider.
    const char* apexThreatMoveId;
    AreaStorefrontDef shop;     // item storefront — listings resolve via ContentRegistry::item
    AreaStorefrontDef modShop;  // mod storefront — listings resolve via ContentRegistry::mod
    const char* const* modPoolIds;  // this area's mod-loot table (drop weighted by rarity)
    int modPoolCount;
};

extern const AreaDef kAreaCitrusCircuit;
extern const AreaDef kAreaPirateBayou;
extern const AreaDef kAreaNetSeaCrossing;
extern const AreaDef kAreaNapstorrentMoors;
extern const AreaDef kAreaCastleRapidscare;

// The ladder, in unlock order. Adding an area = write its area.cpp (declare +
// define its AreaDef here-ish) and add one entry below — kAreaCount, kExplSectors,
// kModPowerTiers and every fixed-size save-flag array follow automatically.
//
// Order IS difficulty (areaTier, below), so this list is the only place the ladder's
// shape is stated. Two things are not automatic when an entry lands anywhere but the
// END: a SAVE MIGRATION, because every persisted flag is positional (save.h's
// ladderInserts — add a row there in the same commit), and each mod's `powerTier`
// under the seam, which is a ladder depth spelled out on the row
// (content_mods.cpp).
inline constexpr const AreaDef* const kAreaList[] = {
    &kAreaCitrusCircuit, &kAreaPirateBayou,        &kAreaNetSeaCrossing,
    &kAreaNapstorrentMoors, &kAreaCastleRapidscare,
};
constexpr int kAreaCount = sizeof(kAreaList) / sizeof(kAreaList[0]);

// How many distinct effectiveness ranks a mod can carry (ModDef::powerTier, defs.h).
// A mod's rank IS a ladder depth — rank N is "what area N-1 hands out" — so the
// number of ranks is the number of rungs, and lives here beside the list rather than
// as a hand-typed count in tunables.h that a new area would silently outgrow. The
// equip-level BAND each rank maps to is a balance magnitude and stays a tunable
// (modEquipLevelFloor), but its input no longer has to be maintained by hand.
constexpr int kModPowerTiers = kAreaCount;

// Bounds-clamped accessor — out-of-range clamps to area 0, matching the fallback
// every existing caller already relies on (a defensive clamp for a malformed/stale
// index, distinct from "an area was added incompletely": that failure mode is
// structurally impossible here since kAreaCount IS kAreaList's length).
inline const AreaDef& area(int idx) {
    if (idx < 0 || idx >= kAreaCount) idx = 0;
    return *kAreaList[idx];
}

// An area's difficulty tier — the input to boss scaling (combat.cpp's subAreaBoss)
// and the Bits payout rank. DERIVED from ladder position rather than authored on the
// row, so an area's difficulty is whatever its place in kAreaList says it is: reorder
// the list, or splice one in, and every enemy above the seam re-scales to its new
// depth with no row to re-tune. An authored field would be a second statement of the
// same fact, and the two would disagree the first time the ladder moved.
//
// Tier is 1-based (area 0 is tier 1) because it is a DEPTH the player reads, not an
// index — the EXPL list, the boss level (tier + 1) and the mod equip bands all want
// "how far in is this", where 0 would mean "no depth at all".
inline int areaTier(int idx) {
    if (idx < 0) idx = 0;
    if (idx >= kAreaCount) idx = kAreaCount - 1;
    return idx + 1;
}

// DeepWeb Dive: the always-last endless zone, unlocked once every real area is
// cleared. It has no sub-ladder/boss/shop, just its own mod pool — defined in
// areas/deepweb_dive/area.cpp alongside the endless-scaling constants below (kept
// beside the pool + boss-scaling code they exist for, rather than cross-cutting
// tunables.h, since nothing outside DeepWeb reads them).
extern const char* const kAreaModsDeepWeb[];
extern const int kAreaModsDeepWebCount;

}  // namespace mal
