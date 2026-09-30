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
                                 "rowhammer",
                                 // The four lines the Cipher ASIC above left out — the
                                 // deep end of the soft-affinity pattern is the whole
                                 // roster's now, not one line's.
                                 "whale_hook", "logic_bomb", "reseed_loop",
                                 "signature_churn"};

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

// The Moors' CHAPTERS (core/content/story.h), wires 13-16.
// The Moors says outright what the ape, Cracker and Conduit were talking to: the host
// Morris counted that no one built. Nothing later has to decode it.
const StoryPanelDef kIntroPanels[] = {
    {"OFF THE GRID",
     "The Napstorrent Moors are wet, empty country: a few farms and villages, and no "
     "proper network. Nobody ever ran a line out here, so every device just talks to "
     "whichever neighbour is in range."},
    {"PEER TO PEER",
     "Files get passed along the same way, house to house and farm to farm, each copy "
     "made from the last. Through your implant you can see it: thin threads of traffic "
     "strung across the bog."},
    {"WORM COUNTRY",
     "That is perfect ground for worms. A worm copies itself into every file it touches, "
     "so one bad file reaches the whole moor in a day. Soon your Malwarium is full of "
     "infected chain letters."},
    {"INFECTIONS",
     "Most malbeasts here carry a worm, so a bite is never just a bite. It leaves an "
     "infection that keeps hurting your pet for a few turns after the hit, even when the "
     "fight is going your way."},
};

const StoryPanelDef kBossIntroPanels[] = {
    {"THE CAUSEWAY",
     "The cable you have been following crosses the moor on an old causeway. Every worm "
     "you beat out here fled along it, back to the one worm they were all copied from."},
    {"MORRIS THE WYRM",
     "Morris the Wyrm is the oldest worm there is, older than most of the 'net. The moor's "
     "patchwork network never had anything to clear it out, so it has lived here ever "
     "since."},
    {"WHAT IT WAS FOR",
     "Morris was built to count the machines on the network. It did that by copying "
     "itself into each one, so fast that it crashed most of them. It is still counting."},
    {"THE WIND-UP",
     "Morris spends three turns reinfecting everything it already has. Then it strikes, "
     "and that damage keeps coming for four turns more. Hit it hard before it finishes."},
};

const StoryPanelDef kBossOutroPanels[] = {
    {"THE COUNT",
     "With Morris down, its count spills out, and your implant lets you read it: every "
     "machine it ever got into, back to the first computers anyone ever networked."},
    {"ONE TOO MANY",
     "The first network had a few thousand machines on it. Morris counted one more. That "
     "one has no address, and it was already connected before Morris was written."},
    {"THE SOURCE",
     "That extra machine is what the ape, Cracker and Conduit were all talking to. It did "
     "not get into the 'net from outside. It has been part of it from the very start."},
};

const StoryPanelDef kAreaOutroPanels[] = {
    {"CLEAN FILES",
     "With Morris gone the chain letters stop, and people on the moor start getting their "
     "files back clean. One real letter arrives, from a Crew that has followed you since "
     "the Circuit."},
    {"THE OFFER",
     "They cannot reach the Moors themselves, but they back operators who can. They are "
     "offering you a place with them, whenever you want to take it."},
    {"THE KEEP",
     "Past the causeway stands Castle Rapidscare, a huge old vault that stored other "
     "people's files for a fee. It never shut down, and the cable runs straight in under "
     "its gate."},
};

const AreaStoryDef kStory = {{
    {/*wire=*/13, "CHAPTER 4: THE MOORS", kIntroPanels, arrLen(kIntroPanels)},
    {/*wire=*/14, "CHAPTER 4: THE WYRM", kBossIntroPanels, arrLen(kBossIntroPanels)},
    {/*wire=*/15, "CHAPTER 4: THE COUNT", kBossOutroPanels, arrLen(kBossOutroPanels)},
    {/*wire=*/16, "CHAPTER 4: THE CAUSEWAY", kAreaOutroPanels, arrLen(kAreaOutroPanels)},
}};}  // namespace

const AreaDef kAreaNapstorrentMoors = {
    "napstorrent_moors",
    "NAPSTORRENT MOORS",
    /*badge=*/"MOORS",
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
      {"WHAT IS NOT LISTED IS NOT HERE.", "YOU ARE A FILING MATTER."}},
     // How it takes the answer — pleased, displeased, affront, boon. A clerk to the
     // last: nothing it does to a pet is worse than what it does to the listing.
     {{"YOU ARE WRITTEN DOWN NOW.", "A LINE IS ADDED FOR YOU."},
      {"I AM STRIKING YOU OUT.", "YOUR ENTRY IS SCRATCHED."},
      {"UNLISTED THINGS ARE NOT ASKED.", "IT SHUTS THE LEDGER."},
      {"YOUR PAGE IS ALREADY OPEN.", "IT READS YOU YOUR ENTRY."}}},
    {"MOOR-TO-MOOR", kShopListings, arrLen(kShopListings)},
    {"MOOR TO MODS", kModShopListings, arrLen(kModShopListings)},
    kModPool,
    arrLen(kModPool),
    kWildLoot,
    arrLen(kWildLoot),
    kStory,
};

}  // namespace mal
