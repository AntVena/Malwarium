// Napstorrent Moors — area 2, tier 3: Napster/P2P, a marshy journey toward the
// castle it foreshadows (AREA_NAMING.md). Opens once The Pirate Bayou's
// gauntlet is cleared.
#include "core/content/areas/area_defs.h"

#include "tunables.h"

namespace mal {

namespace {
// Niche-flavour pass mod pool, plus Faraday Cage (tier 3) — the counter to this
// area's own signature boss rider (data_rot, below).
const char* const kModPool[] = {"overclock_chip", "heat_sink", "honeytoken",
                                 "cipher_asic", "faraday_cage", "prowlware",
                                 "meltdown_core", "zero_day_exploit"};

// MOOR-TO-MOOR — the item storefront: this area's own stock/price per item, same
// pattern as the mod storefront below.
// Both Browns are stocked here and nowhere else: meeting a dish is what the Rig Shop
// asks for before it will sell the recipe for it (game_rig_shop.h's requiresItems),
// so this counter is the front door to cooking them.
const ShopListingDef kShopListings[] = {
    {"disk_scrubber", 8, 14},
    {"ambig_usb", 2, 1024},
    {"hashed_browns", 4, 512},
    {"salted_hashed_browns", 4, 512},
};

// MOOR TO MODS — the mod storefront: this area's own tier-3 mods, priced in Bits
// plus an item cost drawn from this area's own item shop above.
const ShopListingDef kModShopListings[] = {
    {"heat_sink", kShopStock, 512, {{"disk_scrubber", 15}}},
    {"honeytoken", kShopStock, 768, {{"disk_scrubber", 30}}},
};
}  // namespace

const AreaDef kAreaNapstorrentMoors = {
    "napstorrent_moors",
    "NAPSTORRENT MOORS",
    /*tier=*/3,
    "MOOR MARAUDER",
    {"SEEDER SHALLOWS", "LEECHER FEN", "THE SHARED BOG", "SPECTRE SWAMP",
     "CASTLE CAUSEWAY"},
    {"SHALLOW SADMINISTRATOR", "LEECH LICH", "BOG NEUMANN", "CASTELLAN CREEPER",
     "HERALD ANNA KOVA"},
    "MORRIS THE WYRM",
    /*apexThreatMoveId=*/"data_rot",  // the signature boss's DoT rider
    {"MOOR-TO-MOOR", kShopListings, arrLen(kShopListings)},
    {"MOOR TO MODS", kModShopListings, arrLen(kModShopListings)},
    kModPool,
    arrLen(kModPool),
};

}  // namespace mal
