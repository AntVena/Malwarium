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
// The one keeper on the ladder that does NOT talk like the ape: Morris is older than
// whatever it is the others have been talking to, and its tally is where that shows.
const StoryPanelDef kIntroPanels[] = {
    {"SEEDER SHALLOWS",
     "The cable comes ashore and goes under the peat. Every share out here is still up, "
     "still seeding to peers that logged off decades ago. None of them have noticed."},
    {"YOU HAVE MAIL",
     "Two steps in, your Malwarium starts taking mail. Chain letters, forwarded from "
     "addresses nobody has answered at in twenty years, each one asking you to send it on."},
    {"IT KEEPS LANDING",
     "Nothing here hits harder than the Crossing did. What lands just keeps landing. A "
     "bite in the Moors goes on working through your pet for turns after it is over."},
    {"THE LAST NAME",
     "The listing at the edge of the fen has one name in it from the last ten years, and "
     "nothing written after it. The cable runs off the same way they must have gone."},
};

const StoryPanelDef kBossIntroPanels[] = {
    {"GOING HOME",
     "Every keeper out here broke off toward the causeway, and none of them were running. "
     "Everything in the Moors copies itself, and all of it was copied from one thing."},
    {"MORRIS THE WYRM",
     "Morris is the oldest thing on the 'net that still moves. It was only ever meant to "
     "count the network. It counted by getting into every machine it found, and never "
     "learnt to stop."},
    {"THE WIND-UP",
     "It is slow to start. Morris spends three turns getting back into everything it "
     "already has, and whatever it takes after that, it goes on taking for four more."},
};

const StoryPanelDef kBossOutroPanels[] = {
    {"NOT A WORD",
     "Morris goes down without a word in any language. It is the first thing since the "
     "Circuit that has not talked like the ape. It only ever had the one job, and it "
     "leaves you the tally."},
    // The extra host is the thing the ape, Cracker and Conduit were talking to. Nothing
    // here says so; the Crossing already said it was in the networks, not organising them.
    {"ONE MORE",
     "The tally is every machine the first network had, in the order Morris got into "
     "them. The last line is one more. No address, no name, and the time beside it is "
     "before Morris ever ran."},
    {"COUNTED, NOT CARRIED",
     "So Morris never brought it anywhere. Whatever was there to be counted was on the "
     "first network before anybody switched that network on. Morris only counted it."},
};

const StoryPanelDef kAreaOutroPanels[] = {
    {"RETURN ADDRESS",
     "The chain letters stop when Morris does. One more comes in after, and it is not a "
     "chain letter. It has a return address, a real one, and a Crew's mark at the bottom."},
    {"THE DOOR IS OPEN",
     "They have been reading your walk since the Circuit. They cannot get this far out, "
     "but they stand behind anyone who can. There is a place for you when you want it."},
    {"THE KEEP",
     "The causeway runs out of the fen and up to Castle Rapidscare, which the war over "
     "what it hoards never took. Every light in it is on. The cable goes in under the "
     "gate."},
};

const AreaStoryDef kStory = {{
    {/*wire=*/13, "CHAPTER 4: THE MOORS", kIntroPanels, arrLen(kIntroPanels)},
    {/*wire=*/14, "CHAPTER 4: THE WYRM", kBossIntroPanels, arrLen(kBossIntroPanels)},
    {/*wire=*/15, "CHAPTER 4: THE TALLY", kBossOutroPanels, arrLen(kBossOutroPanels)},
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
