// The Pirate Bayou — software-piracy/swamp (AREA_NAMING.md). The bayou is where
// the ladder first reaches water; its BOOTY CACHE is the last dry stretch before
// the open Net-Sea.
#include "core/content/areas/area_defs.h"

#include "tunables.h"

namespace mal {

namespace {
// This area's WILD-win drop table — what a won wild encounter here hands over.
// Rows draw at each item's own dropWeight (a bare id is the rule; see
// content_items.cpp). The first four are the staple set every area shares: two
// snacks, the combat shield, and the cleaner — fighting fragments the pet, so a
// wild win is where you top the cleaner up.
const LootEntry kWildLoot[] = {
    {"airgap_snack"}, {"tortilla_chip"}, {"backup_drive"}, {"disk_scrubber"},
};

// Niche-flavour pass mod pool, plus Watchdog Timer (tier 2) — the counter to this
// area's own signature boss rider (system_hang, below): debuting the threat and its
// counter in the same area's loot table.
const char* const kModPool[] = {"tpm_chip", "solid_state_cache", "firewall_patch",
                                 "watchdog_timer", "botnet_swarm", "airgap_ward",
                                 "tripwire", "cold_storage"};

// PIER-TO-PEER — the item storefront: this area's own stock/price per item, same
// pattern as the mod storefront below.
// The Desalinated C-Salt row is a TRADE, not a sale: no Bits, just the Spoiled Macrol
// nobody else wants. It's the only reliable source of the salt — finding one on a walk
// is deliberately rarer than catching a fish and letting it turn (content_items.cpp).
const ShopListingDef kShopListings[] = {
    {"r007_b33r", 8, 5},
    {"boot_accelerator", 3, 1024},
    {"desalinated_c_salt", 4, 0, {{"spoiled_macrol", 1}}},
};

// PHISHY CHIPS — the mod storefront: this area's own tier-2 mods, each priced in
// Bits plus a Backup Drive cost (this area's own signature item find).
const ShopListingDef kModShopListings[] = {
    {"tpm_chip", kShopStock, 64, {{"backup_drive", 20}}},
    {"firewall_patch", kShopStock, 256, {{"backup_drive", 40}}},
};
}  // namespace

const AreaDef kAreaPirateBayou = {
    "pirate_bayou",
    "THE PIRATE BAYOU",
    "LEECH LORD",
    "ICON_SECTOR_PIRATE_BAYOU",
    {"LEECH LANDING", "TORRENT SWAMP", "THE CRACKED KEYS", "WAREZ MARSH",
     "BOOTY CACHE"},
    {{"NETBUS NIPPER"},
     {"SUPRNOVA SERPENT"},
     {"RAZOR KRAKEN"},
     {"WIGHT OF DRINKORDIE"},
     {"THE SUNKEN SEVENTH"}},
    "CAP'N CRACKER",
    /*apexThreatMoveId=*/"system_hang",  // the signature boss's STUN rider
    {"PIER-TO-PEER", kShopListings, arrLen(kShopListings)},
    {"PHISHY CHIPS", kModShopListings, arrLen(kModShopListings)},
    kModPool,
    arrLen(kModPool),
    kWildLoot,
    arrLen(kWildLoot),
};

}  // namespace mal
