// Citrus Circuit — the LimeWire-era download-culture ladder entry
// (AREA_NAMING.md). Sits first in kAreaList, which is what makes it the always-open
// one: every other area gates behind clearing the one before it. Its difficulty
// follows from that position (areaTier), not from anything on the row below.
#include "core/content/areas/area_defs.h"

#include "tunables.h"

namespace mal {

namespace {
// This area's WILD-win drop table — what a won wild encounter here hands over.
// Rows draw at each item's own dropWeight (a bare id is the rule; see
// content_items.cpp). The first four are the staple set every area shares: two
// snacks, the combat shield, and the cleaner — fighting fragments the pet, so a
// wild win is where you top the cleaner up.
// Plus this area's exclusive Merge Hub ingredient, so a wild win here is a second,
// area-flavoured way to farm what the Uncommon cache pool already offers everywhere.
const LootEntry kWildLoot[] = {
    {"airgap_snack"}, {"tortilla_chip"}, {"backup_drive"}, {"disk_scrubber"},
    {"osi_dip"},
};

// Niche-flavour pass mod pool: rolled (weighted by rarity) on this area's
// milestone drops (sub/area-boss clear, Epic cache).
const char* const kModPool[] = {"clock_speed_boost", "packet_sniffer",
                                 "crypto_coprocessor", "canary_trap",
                                 "scratch_disk_buffer"};

// BYTE TO EAT — the item storefront: this area's own stock/price per item, same
// pattern as the mod storefront below.
const ShopListingDef kShopListings[] = {
    {"null_noodles", 10, 5},
};

// CHIP SHOP — the mod storefront: a guaranteed, no-roll buy of this area's own
// starter mods, each priced in Bits plus an item cost (docs/AREA_CONTENT_STANDARD.md
// keeps mod identity/pricing on the owning area, same as the item shop above).
const ShopListingDef kModShopListings[] = {
    {"clock_speed_boost", kShopStock, 32, {{"null_noodles", 20}}},
    {"packet_sniffer", kShopStock, 48, {{"r007_b33r", 10}}},
};
}  // namespace

const AreaDef kAreaCitrusCircuit = {
    "citrus_circuit",
    "CITRUS CIRCUIT",
    "CERTIFIED DOWNLOADER",
    "ICON_SECTOR_CITRUS_CIRCUIT",
    {"FAKE FILE FLATS", "BUFFERING BLUFFS", "99% CACHE", "THE SHARED FOLDER",
     "DIAL-UP DRAW"},
    {"PERVADES NEAR U", "ANOTHER ANIMAL", "VERDANT VIRDEM", "PERM POLTERGEIST",
     "BAUD BANSHEE"},
    "THE BUFFER BARON",
    /*apexThreatMoveId=*/nullptr,  // the entry-level area carries no signature rider
    {"BYTE TO EAT", kShopListings, arrLen(kShopListings)},
    {"CHIP SHOP", kModShopListings, arrLen(kModShopListings)},
    kModPool,
    arrLen(kModPool),
    kWildLoot,
    arrLen(kWildLoot),
};

}  // namespace mal
