// Castle Rapidscare — RapidShare's file-locker keep (AREA_NAMING.md), the castle
// the Napstorrent Moors' CASTLE CAUSEWAY walks up to. Last in kAreaList, which is
// what makes it the deepest named area before the endless DeepWeb Dive — and the
// reason its pool is the endgame one. Its five stretches are held by a card/chess
// court — pawns at the gate, a Red Queen in the mirrors, a Knave selling fixes for
// problems he invented, a Joker in the decoys, the King in the catacombs — all flying
// COUNT CONFICKER's banner: the keep the copyright war never actually took.
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

// The endgame pool: this area's own Ghost Process, the classic tier-4 Epics, and a
// SECOND source of Watchdog Timer + Faraday Cage. Earlier areas debut one threat each
// and pay out its counter in the same loot table; the keep's apex wields BOTH riders at
// once (nag_screen, below), so it re-stocks both counters for a player whose Bayou/Moors
// rolls never turned one up.
const char* const kModPool[] = {"ghost_process",  "deadman_switch", "raid_mirror",
                                "backup_uplink",  "watchdog_timer", "faraday_cage"};

// SPAM & SCRAM — the item storefront: the free tier's endless lunch, and the two ways
// out of a fight you didn't want (skip it, or patch through the middle of it). The
// Sinkhole Traps are also what the mod counter below charges for Ghost Process.
const ShopListingDef kShopListings[] = {
    {"spam", 12, 4},
    {"sinkhole_trap", 6, 40},
    {"airgap_snack", 4, 96},
};

// THE GHOST IN THE MACHINE — the mod storefront: this area's own tier-4 Epics, priced in
// Bits plus a stack of whichever consumable the mod is made out of. Ghost Process is paid
// for in Sinkhole Traps from the item shop above. RAID Mirror is paid for in Backup
// Drives, which is the trade the keep is really offering: a drawer full of one-shot
// restores, melted down into a redundancy the pet carries permanently. It costs a serious
// pile of them because that is the point — the drives are the cheap, repeatable version
// of surviving, and this is the expensive, permanent one.
const ShopListingDef kModShopListings[] = {
    {"ghost_process", kShopStock, 1024, {{"sinkhole_trap", 12}}},
    {"raid_mirror", kShopStock, 1536, {{"backup_drive", 24}}},
};
}  // namespace

const AreaDef kAreaCastleRapidscare = {
    "castle_rapidscare",
    "CASTLE RAPIDSCARE",
    "KING OF THE KEEP",
    "ICON_SECTOR_CASTLE_RAPIDSCARE",
    {"404 DRAWBRIDGE", "MIRROR MAZE", "OPT-OUT OBSCURA", "DECOY DUNGEON",
     "COMMENT CATACOMBS"},
    // Two of the court are fought as gauntlets rather than as one malbeast: the pawns come
    // in two ranks, and the Joker's fall is what lets the gate's pawns back in behind you.
    {{"THE EIGHT PWNS", {{"THE FRONT RANK", -1}, {"THE BACK RANK"}}},
     {"RED QUEEN MELISSA"},
     {"KNAVE WINFIXER"},
     {"JOKER VIRUS", {{"JOKER VIRUS"}, {"THE EIGHT PWNS", -1}}},
     {"KING KIMBLE"}},
    "COUNT CONFICKER",
    /*apexThreatMoveId=*/"nag_screen",  // the signature boss's FREEZE + DoT rider
    {"SPAM & SCRAM", kShopListings, arrLen(kShopListings)},
    {"THE GHOST IN THE MACHINE", kModShopListings, arrLen(kModShopListings)},
    kModPool,
    arrLen(kModPool),
    kWildLoot,
    arrLen(kWildLoot),
};

}  // namespace mal
