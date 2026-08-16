// Net-Sea Crossing — CNET/Download.com, sounded backwards into open water
// ("see-net" → "net-sea", AREA_NAMING.md §1.1). The stretch of the ladder you SAIL:
// out past the Pirate Bayou's last dry cache, across the shipping lanes where every
// download arrived wrapped in three things you didn't ask for, and ashore again at
// SANDBOX BEACH — which is where the walk inland to the Napstorrent Moors starts.
// That landfall is why the crossing sits between the two, and why its signature
// stretch is a beach rather than the trench: the wall here is arriving, not drowning.
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

// The crossing's own pool — seamanship over sabotage: hold the hull, hear what's
// coming, strip the junk off what you hauled aboard. Plus Watchdog Timer, the counter
// to this area's own apex rider (decoy_download, below): the same debut-the-threat-
// and-its-counter-together rule the Bayou and the Moors follow, except the rider here
// is a longer version of one already in play, so this restocks that counter rather
// than paying out a third.
const char* const kModPool[] = {"hardened_shell", "bundle_stripper", "ballast_cache",
                                "sonar_ping",     "salvage_rig",     "watchdog_timer"};

// FLOATING POINT — the item storefront, tied up where the water is calm enough to
// trade. Restore Point is the joke and the stock in one: the only thing worth buying
// from a floating point is a fixed one.
const ShopListingDef kShopListings[] = {
    {"tortilla_chip", 10, 6},
    {"restore_point", 3, 640},
    {"fresh_macrol", 6, 18},
};

// THE HARDENED SHELL — the mod storefront: this area's own two hull mods, priced in
// Bits plus a stack of the Backup Drives the water keeps handing out.
const ShopListingDef kModShopListings[] = {
    {"hardened_shell", kShopStock, 384, {{"backup_drive", 28}}},
    {"ballast_cache", kShopStock, 448, {{"backup_drive", 32}}},
};
}  // namespace

const AreaDef kAreaNetSeaCrossing = {
    "net_sea_crossing",
    "NET-SEA CROSSING",
    "BUNDLE BREAKER",
    "ICON_SECTOR_NET_SEA_CROSSING",
    {"UNINSTALL UNDERTOW", "POPUP WHIRLPOOL", "TRACKER TRENCH", "CODEC REEF",
     "SANDBOX BEACH"},
    {{"THE CANDY SIREN"},
     {"VUNDO THE UNENDING"},
     {"THE SUPERFISH"},
     {"ZLOB CONGER"},
     {"THE GREEN BUTTON"}},
    "ADMIRAL CONDUIT",
    /*apexThreatMoveId=*/"decoy_download",  // the signature boss's long-STUN rider
    {"FLOATING POINT", kShopListings, arrLen(kShopListings)},
    {"THE HARDENED SHELL", kModShopListings, arrLen(kModShopListings)},
    kModPool,
    arrLen(kModPool),
    kWildLoot,
    arrLen(kWildLoot),
};

}  // namespace mal
