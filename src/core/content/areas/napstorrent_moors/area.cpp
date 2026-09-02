// Napstorrent Moors — Napster/P2P, a marshy journey toward the castle it
// foreshadows (AREA_NAMING.md). Reached by landing off the Net-Sea and walking
// inland; its CASTLE CAUSEWAY is what walks up to Castle Rapidscare, so the moors
// must stay directly before the keep in kAreaList — that adjacency is the one thing
// about this area's position the fiction actually depends on.
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
    {"dyno_nuggets"}, {"tortilla_chip"}, {"backup_drive"}, {"disk_scrubber"},
};

// Niche-flavour pass mod pool, plus Faraday Cage (tier 3) — the counter to this
// area's own signature boss rider (data_rot, below).
const char* const kModPool[] = {"overclock_chip", "heat_sink", "honeytoken",
                                 "cipher_asic", "faraday_cage", "prowlware",
                                 "meltdown_core", "zero_day_exploit",
                                 // The bulk rung the ladder had been missing since the
                                 // crossing, the opening-probe cut's third, and the
                                 // trickle that gives the first of those a use.
                                 "seedbox_array", "decoy_peer", "trickle_charger",
                                 "rowhammer"};

// MOOR-TO-MOOR — the item storefront: this area's own stock/price per item, same
// pattern as the mod storefront below.
// Both Browns are stocked here and nowhere else: meeting a dish is what a
// Decryptogram asks for before its prize ladder will teach the recipe for it
// (game_internal.h's MergeRecipe::requiresItems), so this counter is the front door to
// cooking them.
//
// The Hypervisor-USB is the one device in its family anyone sells, and it is priced in
// its own family rather than in Bits alone: four Sandbox-USBs, which drop nowhere but the
// DeepWeb Dive. So the deep end of the soak ladder is reached by diving for the rare one
// four times over — the counter is where the four are ASSEMBLED, not where the ladder is
// skipped. Stocked one per visit for the same reason.
const ShopListingDef kShopListings[] = {
    {"disk_scrubber", 8, 14},
    {"ambig_usb", 2, 1024},
    {"hypervisor_usb", 1, 2048, {{"sandbox_usb", 4}}},
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
    "MOOR MARAUDER",
    "ICON_SECTOR_NAPSTORRENT_MOORS",
    SceneId::None,   // the moors' backdrop is not authored yet
    {"SEEDER SHALLOWS", "LEECHER FEN", "THE SHARED BOG", "SPECTRE SWAMP",
     "CASTLE CAUSEWAY"},
    {{"NIMDA OF THE SHALLOWS", {"admin_reversal"}},
     {"MYDOOM LICH", {"mail_storm"}},
     {"BOG NEUMANN", {"self_reference"}},
     {"CASTELLAN CREEPER", {"evade_trace"}},
     {"HERALD ANNA KOVA", {"attachment_bait"}}},
    "MORRIS THE WYRM",
    /*areaBossMoveId=*/"runaway_fork",
    /*apexThreatMoveId=*/"data_rot",  // the signature boss's DoT rider
    // The moors is the mail area, and its wilds carry mail's plainest form: Chain Letter
    // is Mail Storm without the size — no rider, no wind-up, which in a pool built out of
    // riders is its own niche.
    /*wildAttackMoveId=*/"chain_letter",
    /*wildDefendMoveId=*/"private_tracker",
    // Who watches the Moors: the keeper of the listing. Everything out here exists
    // because it is written down, and it is the one doing the writing.
    {"THE INDEX KEEPER",
     {"delisted"},
     // Keeper of the listing. Everything out here exists because it is written down, and
     // it is the one doing the writing — so a pet is a clerical matter until it answers.
     {{"YOU ARE NOT YET WRITTEN DOWN.", "IT LOOKS FOR YOUR NAME."},
      {"ANSWER, AND I WILL ADD YOU.", "IT HOLDS A PLACE OPEN."},
      {"WHAT IS NOT LISTED IS NOT HERE.", "YOU ARE A FILING MATTER."}}},
    {"MOOR-TO-MOOR", kShopListings, arrLen(kShopListings)},
    {"MOOR TO MODS", kModShopListings, arrLen(kModShopListings)},
    kModPool,
    arrLen(kModPool),
    kWildLoot,
    arrLen(kWildLoot),
};

}  // namespace mal
