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
    {"dyno_nuggets"}, {"tortilla_chip"}, {"backup_drive"}, {"disk_scrubber"},
    {"osi_dip"},
};

// Niche-flavour pass mod pool: rolled (weighted by rarity) on this area's
// milestone drops (sub/area-boss clear, Epic cache).
const char* const kModPool[] = {"clock_speed_boost", "packet_sniffer",
                                 "crypto_coprocessor", "canary_trap",
                                 "scratch_disk_buffer", "spare_ram_stick",
                                 "capacitor_bank",
                                 // The two families the starter band was missing: the
                                 // fatigue tax a pet with no Bits cannot buy its way
                                 // out of, and attack power behind a threshold, which
                                 // is the only shape of it worth a slot this shallow.
                                 "thermal_paste", "brownout_boost",
                                 // The line set, at the shallowest gates on the ladder:
                                 // a pet's own family starts paying in the area it
                                 // hatches into rather than a band later
                                 // (content_mods.cpp).
                                 "lure_page", "locked_sector", "autorun_stub",
                                 "chain_letter", "nop_sled"};

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

// The Circuit's CHAPTERS (core/content/story.h), wires 1-4.
// No chapter names the Title: `title` is retunable, and the device grants the real one
// on the departure beat anyway.
const StoryPanelDef kIntroPanels[] = {
    {"THE BEEP",
     "You set out with your new Malwarium to walk the network around Citrus Circuit, "
     "your usual stomping ground. Two steps in, it beeps. That is odd."},
    {"THE COUNT",
     "It does not stop beeping. The count climbs the whole way down the street. There "
     "are more malbeasts in the Circuit today than you have seen here in a year, and "
     "none of them are leaving."},
    {"NOT BROKEN",
     "The doors are shut too. Not broken - locked, by something that had the keys. "
     "Every screen on the block shows the same grinning purple ape, and it wants paying "
     "before anything opens."},
    {"LEND A HAND",
     "Install some mods. Check your pet's moves. Then lend a hand until a Crew can get "
     "here. It is a good chance to work on your teamwork."},
};

const StoryPanelDef kBossIntroPanels[] = {
    {"DOWNHILL",
     "You cleared five stretches of the Circuit. Every keeper you beat broke off and ran "
     "the same way, downhill, the way the Draw runs. Whatever they answer to is waiting "
     "down there."},
    {"BARON BONZI",
     "A ransomware cartel holds the Circuit's locks. The ape on the screens is what it "
     "puts in front, and what it tells the whole block is: pay, and it will kindly put "
     "everything back."},
    {"THE OPENING",
     "Five of them, back to back, no rest between. Your Malwarium can force an opening "
     "when your petware is cornered, but firing one costs a turn, and you do not get "
     "many."},
};

const StoryPanelDef kBossOutroPanels[] = {
    {"UNINSTALLED",
     "The ape comes apart mid-sentence. What it was saying was not aimed at you, and not "
     "in any language the Circuit speaks."},
    {"ON THE WAY OUT",
     "Every lock on the block lets go. Doors open the length of the Flats and people come "
     "out to look. You check your Bits on the way past. Nobody hired you, and it still "
     "paid."},
    {"TWO STREETS OVER",
     "Your Malwarium does not stop counting. Two streets over the screens are still lit, "
     "still the same ape, still asking to be paid. Whatever installed it here installed "
     "it there too."},
};

const StoryPanelDef kAreaOutroPanels[] = {
    {"WORD GETS ROUND",
     "Nobody knows your name. By the end of the week the block has one for you anyway. "
     "You did not pick it, and it has already travelled further than you have."},
    {"WHAT DO YOU CHARGE?",
     "People start asking what you charge. You have not been charging anything. Your rig "
     "is half configured and your bag is full of sealed caches you have no way to open."},
    {"THE PIER",
     "Dial-Up Draw runs downhill out of the Circuit and comes out on water. There is a "
     "Malwarium shop on the pier at the bottom. You start walking, and the Crew still "
     "has not arrived."},
};

const AreaStoryDef kStory = {{
    {/*wire=*/1, "CHAPTER 1: DAY ZERO", kIntroPanels, arrLen(kIntroPanels)},
    {/*wire=*/2, "CHAPTER 1: THE BARON", kBossIntroPanels, arrLen(kBossIntroPanels)},
    {/*wire=*/3, "CHAPTER 1: UNINSTALLED", kBossOutroPanels, arrLen(kBossOutroPanels)},
    {/*wire=*/4, "CHAPTER 1: THE PIER", kAreaOutroPanels, arrLen(kAreaOutroPanels)},
}};
}  // namespace

const AreaDef kAreaCitrusCircuit = {
    "citrus_circuit",
    "CITRUS CIRCUIT",
    /*badge=*/"CITRUS",
    "CERTIFIED DOWNLOADER",
    "ICON_SECTOR_CITRUS_CIRCUIT",
    SceneId::CitrusCircuit,
    {"FAKE FILE FLATS", "BUFFERING BLUFFS", "99% CACHE", "THE SHARED FOLDER",
     "DIAL-UP DRAW"},
    // The fake-file boss teaches the fake AND the checksum that catches it — which is why
    // Checksum Guard, one of the two generic braces the roster places by hand, is here
    // rather than on whichever boss has a free slot.
    {{"BENJAMIN THE FALSE", {"fake_seed", "checksum_guard"}},
     {"MORPHEUS THE MIRAGE", {"stall_loop"}},
     {"TURING THE UNHALTED", {"infinite_loop"}},
     {"SHARMAN OF THE FOLD", {"shared_folder"}},
     {"TONELOC THE TOLLTAKER", {"toll_charge"}}},
    "BARON BONZI",
    /*areaBossMoveId=*/"helper_monkey",
    /*apexThreatMoveId=*/nullptr,  // the entry-level area carries no signature rider
    // Nothing in the Circuit ever finishes: the wilds hand out the rot the boss pool's
    // Infinite Loop is the endless version of, and the brace that is the 99% cache
    // itself — a hit that finds nothing where the file was supposed to be.
    /*wildAttackMoveId=*/"partial_download",
    /*wildDefendMoveId=*/"cache_miss",
    // Who watches the Circuit: a seeder that never went offline, still serving a
    // swarm that dispersed years ago. It asks before it acts, which nothing else here
    // does.
    {"THE LONG SEEDER",
     {"ratio_debt"},
     // Still serving a swarm that dispersed years ago, and has not been told. Everything
     // it says is an offer nobody has accepted in a very long time.
     {{"YOU ARE LATE. THEY WERE ALL LATE.", "IT HAS WAITED SO LONG."},
      {"I STILL HAVE EVERY FILE. ASK ME.", "IT HOLDS OUT NOTHING."},
      {"STAY. THE OTHERS DID NOT STAY.", "IT HOPES YOU WILL STAY."}},
     // How it takes the answer — pleased, displeased, affront, boon. A seeder measures
     // everything as a ratio, so what a pet gives back is the only thing it is grading.
     {{"THEN THE SWARM IS NOT DEAD.", "IT SEEDS TO YOU AT LAST."},
      {"YOU LEECH LIKE ALL OF THEM.", "YOUR RATIO IS NOTED."},
      {"I DO NOT SEED TO STRANGERS.", "IT CHOKES THE STREAM OFF."},
      {"SIT. THE TRANSFER IS SLOW.", "IT SHARES WHAT IT KEPT."}}},
    {"BYTE TO EAT", kShopListings, arrLen(kShopListings)},
    {"CHIP SHOP", kModShopListings, arrLen(kModShopListings)},
    kModPool,
    arrLen(kModPool),
    kWildLoot,
    arrLen(kWildLoot),
    kStory,
};

}  // namespace mal
