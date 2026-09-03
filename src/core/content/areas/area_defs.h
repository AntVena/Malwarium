// area_defs.h — the EXPL area ladder, one AreaDef per folder under
// src/core/content/areas/.
//
// An AreaDef is everything that makes one explorable area of the linear ladder
// itself: its name/Title, its 5 sub-area names, its 5 sub-area bosses (each a banner plus
// the rounds it is fought as) + the area-boss banner, its storefront, and its two loot
// pools (mod + wild-win item).
// Find an area's
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

#include <cstdint>

#include "core/content/defs.h"  // LootEntry — an area's wild-win drop table
#include "core/render/scene_id.h"  // SceneId — an area names its backdrop the way it names its glyph

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

// How many rounds one sub-area boss can be fought as. Headroom over what any shipped row
// uses, not a ladder — unlike kAreaCount there is no derived count to keep in sync, just
// don't let a row's rounds[] grow past it.
constexpr int kMaxSubBossRounds = 3;

// One round of a sub-area boss fight. `rung` is a DELTA on the sub-area's own depth, so an
// escort is authored as "a rung shallower than the boss it guards" rather than as its own
// stat block: 0 (the default) is the boss at full strength, -1 is drawn one sub-area back.
// A magnitude on the row, per src/core/content/CONTENT_STANDARD.md — the SCALE it feeds is
// the shared tunable, in combat_factory.cpp's subBossEnemy.
struct SubBossRound {
    const char* name;
    int rung = 0;
};

// One sub-area boss. `name` is the BANNER — what the EXPL row calls it and what the
// confrontation is announced as. `rounds` is what it is actually FOUGHT as: an ordered
// gauntlet run back-to-back with carried Health, exactly like the area boss. An EMPTY list
// (the common case) means one round named by the banner, so a plain boss stays a one-liner
// and only a row that wants escorts pays for them.
// How many moves one boss carries BEYOND the shared spine. Two, because the ceiling is
// behavioural rather than storage: Combat::chooseMove is uniform over a kit, so every move
// added to a boss is a slice of its turns spent doing that instead of what it did before.
constexpr int kMaxBossTeaches = 2;

struct SubBossDef {
    const char* name;
    // What beating this boss can TEACH — the moves it carries on top of the depth spine
    // subBossEnemy builds. Drops are drawn from the defeated enemy's kit
    // (Game::rollEnemyMoveDrop), so this list is the ONLY thing that makes a move findable:
    // a move no row here names cannot be earned anywhere in the game. Ids resolve against
    // the move registry; the native gate fails a typo rather than shipping an unreachable
    // move, which is the exact bug the list exists to prevent.
    const char* teaches[kMaxBossTeaches] = {};
    SubBossRound rounds[kMaxSubBossRounds] = {};

    // The two readers of `rounds` (combat_factory's subAreaBoss, and the width gate) both
    // want the list already reconciled with the empty-means-one rule, so it is resolved
    // HERE once rather than re-derived — a row that spells no rounds and a row that spells
    // one at rung 0 are the same fight, and nothing downstream should be able to tell them
    // apart.
    int roundCount() const {
        int n = 0;
        while (n < kMaxSubBossRounds && rounds[n].name) ++n;
        return n > 0 ? n : 1;
    }
    SubBossRound round(int i) const {
        if (i < 0 || i >= kMaxSubBossRounds || !rounds[i].name) return {name, 0};
        return rounds[i];
    }
};

// One area's GUARDIAN — the thing that has been watching this network the whole time,
// and the only creature in the game that would rather TALK than fight (the SHIBBOLETH,
// game_shibboleth.cpp). It is not a rung of the EXPL ladder: it is met on the walk, when
// the radio has nothing new to hand the pet, so it has no sub-area and no clear flag.
//
// The row carries only what is this guardian's OWN — its banner and what beating it
// teaches. How much it out-classes the area is the SAME step for every area
// (kGuardian*, tunables.h) and stays central per src/core/content/CONTENT_STANDARD.md
// rule 2, so a guardian is authored as an identity and never as a stat block.
//
// `teaches` is bounded by kMaxBossTeaches for the reason that constant gives: a kit is
// turns spent, not bytes stored. It is also the only thing that makes those moves
// findable at all, exactly as on SubBossDef.
// How many greeting/demeanour pairs one guardian carries. Three, because the pool has to
// be big enough that a guardian is not one-note across a save and small enough that
// giving one a VOICE is an afternoon's writing rather than a project.
constexpr int kGuardianLines = 3;

// One thing a guardian does when it meets a pet, said twice.
//
// `cant` is what it SAYS, in its own language — enciphered with the same mapping as the
// riddle (Game::shibbolethGreeting), so it is gibberish to a pet with no sigils and
// plain speech to a fluent one. `seen` is what the pet OBSERVES, always in plain words,
// because you can read a body without sharing a language.
//
// The two describe the SAME moment, and that is the whole mechanism: at zero sigils the
// demeanour is all a player has, and as the Cant comes in they watch the words arrive
// underneath a gesture they already understood. So the pair teaches — a player who has
// seen "IT HOLDS OUT NOTHING" a dozen times has a standing guess at what the words above
// it mean, which is how anyone learns a language they were not taught.
//
// `seen` is ONE panel line (kRiddleBodyCols) and `cant` is at most two. A stage
// direction that runs long stops being a glance and starts competing with the riddle,
// which is the thing the player is actually supposed to be reading.
struct GuardianLine {
    const char* cant;
    const char* seen;
};

// How a meeting ENDS. A guardian is met on the walk and the walk does not stop there:
// something happens because of what the pet said, and this is the row that says WHAT.
// Indexes GuardianDef::outcomes, in this order.
//
// Pleased and Displeased are the two ways a riddle lands. Affront and Boon are the two
// bands that never ask one at all (Game::startShibboleth's fluency roll) — a guardian
// that will not hear an illiterate pet out still has to say so, and one that simply
// talks to a fluent pet is the payoff the whole ladder climbs to.
enum class GuardianOutcome : uint8_t { Pleased, Displeased, Affront, Boon };
constexpr int kGuardianOutcomes = 4;

struct GuardianDef {
    const char* name;
    const char* teaches[kMaxBossTeaches] = {};
    // What it says and what it looks like saying it — see GuardianLine. One is drawn per
    // encounter, rolled, so a guardian met twice is not word-for-word the same meeting.
    GuardianLine lines[kGuardianLines] = {};
    // How it TAKES what the pet did — one GuardianLine per GuardianOutcome, in that
    // order, and the same two channels for the same reason: a pet that reads nothing of
    // the Cant still has to come away knowing whether it was believed.
    //
    // This is the half that makes a lost riddle legible. The fight that follows a wrong
    // reply is the guardian's answer, and without a row here it arrives as a boss with
    // no stated cause — so `displeased` is what turns "a fight started" into "it did not
    // believe you". Written as ONE moment said twice, exactly like `lines` above.
    GuardianLine outcomes[kGuardianOutcomes] = {};
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
    // This area's engine-drawn BACKDROP, named the same way and for the same reason the
    // glyph above is: keyed off identity, so splicing an area into the middle of
    // kAreaList cannot silently re-point a place at its neighbour's picture. It is an
    // enum rather than an asset name because a scene is CODE — a table and a handful of
    // primitive calls under core/render/scenes/ — so a name that does not resolve
    // should be a build failure and not a blank screen. SceneId::None is a real answer:
    // an area whose place is not authored yet keeps the plain `paper` field.
    SceneId scene;
    const char* subAreas[kSubAreasPerArea];      // 5 named stretches
    SubBossDef subBosses[kSubAreasPerArea];      // 5 sub-area bosses (sub 4 = signature)
    const char* areaBossName;                    // the area gauntlet's overall banner
    // The area boss's OWN move — carried by the final round of its gauntlet and nowhere
    // else, so it is a reward for clearing all five stages rather than for beating whichever
    // sub-area happens to sit last. nullptr = the banner teaches nothing of its own.
    const char* areaBossMoveId;
    // The signature (sub 4) boss's extra rider move, debuting the area's THREAT that
    // its own loot table's counter-mod answers (e.g. Pirate Bayou's system_hang stun,
    // countered by that same area's Watchdog Timer drop). nullptr = no rider.
    const char* apexThreatMoveId;
    // This area's WILD pair — the Attack and the Defend every ordinary malbeast met here
    // fields, handed out by combat_factory's applyWildSubAreaRamp (the Attack at every
    // rung, the Defend from the deeper ones). Named HERE, beside the two boss hooks
    // above, because which move an area's wilds swing is the same kind of fact as which
    // move its banner swings: an area's identity, not a rung of the shared ramp. The ramp
    // itself stays central — it reads these two off the row and applies one rule to every
    // area, so an area declares WHAT it fields and never HOW DEEP.
    //
    // This is also the whole of what makes the pair earnable: a drop comes from the
    // defeated enemy's kit (Game::rollEnemyMoveDrop), so naming a generic move here is
    // what turns walking this area into a way to learn it. nullptr on either = this
    // area's wilds add nothing of their own to the ramp's kit.
    const char* wildAttackMoveId;
    const char* wildDefendMoveId;
    // Who watches this network. Met on the walk rather than on the ladder — see
    // GuardianDef above.
    GuardianDef guardian;
    AreaStorefrontDef shop;     // item storefront — listings resolve via ContentRegistry::item
    AreaStorefrontDef modShop;  // mod storefront — listings resolve via ContentRegistry::mod
    const char* const* modPoolIds;  // this area's mod-loot table (drop weighted by rarity)
    int modPoolCount;
    // This area's WILD-win item drop table — what a won wild encounter here can hand
    // over, drawn by Game::rollLootEntry like every other pool. Authored in full rather
    // than as a delta on a shared base: an area's drop table is a fact about that area,
    // and a reader editing this row should see all of it. Most areas share a staple set
    // and then differ in one entry, which is exactly the difference this makes visible.
    const LootEntry* wildLootPool;
    int wildLootPoolCount;
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

// How many distinct depths a mod can sit at (ModDef::powerTier, defs.h). A mod's rank
// IS a ladder depth — rank N is "what area N-1 hands out" — so the number of ranks is
// the number of rungs, and lives here beside the list rather than as a hand-typed count
// in tunables.h that a new area would silently outgrow. It does NOT decide when a mod may
// be equipped: that gate is authored per row (ModDef::equipLevel, bounded by
// kModEquipLevelMax), since a count of areas can only ever express that many gates.
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
// cleared. It has no sub-ladder/boss/shop, just its own two loot pools — defined in
// areas/deepweb_dive/area.cpp alongside the endless-scaling constants below (kept
// beside the pool + boss-scaling code they exist for, rather than cross-cutting
// tunables.h, since nothing outside DeepWeb reads them).
extern const char* const kAreaModsDeepWeb[];
extern const int kAreaModsDeepWebCount;
extern const LootEntry kWildLootDeepWeb[];   // AreaDef::wildLootPool's stand-in
extern const int kWildLootDeepWebCount;
extern const char* const kDeepWebIcon;   // its EXPL row glyph (AreaDef::icon's stand-in)

// The wild-win drop pool for `areaIdx` (0..kAreaCount-1 or kDeepWebSector), or an
// empty pool if the index names neither. The one place the dive's standalone pool is
// reconciled with the ladder's rows, so a caller draws from a pool without knowing
// which of the two shapes it came from — same job areaModTable does for mods.
struct AreaLootTable { const LootEntry* rows; int count; };

// The sector index that MEANS DeepWeb: one past the last real area, so any
// `0 <= idx < kAreaCount` test excludes it by construction and nothing has to know
// the zone by name to skip it. It sits here with the ladder it indexes past rather
// than in expl_screen.h, because the run state that carries it (Game's active
// sector) outlives any screen — the picker just happens to draw a row for it.
constexpr int kDeepWebSector = kAreaCount;

inline AreaLootTable areaWildLootTable(int areaIdx) {
    if (areaIdx == kDeepWebSector) return {kWildLootDeepWeb, kWildLootDeepWebCount};
    // Bounds-checked rather than leaning on area()'s clamp-to-0: handing back area 0's
    // drop table for an out-of-range sector would be a silent wrong answer, where an
    // empty pool is a visible one (rollLootEntry returns nullptr and nothing drops).
    if (areaIdx < 0 || areaIdx >= kAreaCount) return {nullptr, 0};
    const AreaDef& a = area(areaIdx);
    return {a.wildLootPool, a.wildLootPoolCount};
}

}  // namespace mal
