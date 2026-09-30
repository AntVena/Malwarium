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
    {"dyno_nuggets"}, {"tortilla_chip"}, {"backup_drive"}, {"disk_scrubber"},
};

// Niche-flavour pass mod pool, plus Watchdog Timer (tier 2) — the counter to this
// area's own signature boss rider (system_hang, below): debuting the threat and its
// counter in the same area's loot table.
const char* const kModPool[] = {"tpm_chip", "solid_state_cache", "firewall_patch",
                                 "watchdog_timer", "botnet_swarm", "airgap_ward",
                                 "tripwire", "cold_storage",
                                 // The soft line-affinity mods, one per line — the
                                 // band where a pet's LINE goes from a first taste to a
                                 // real slot (content_mods.cpp explains the pattern, and
                                 // every other band now carries its own set of these).
                                 "spoof_header", "escrow_buffer", "dropper_payload",
                                 "fork_spur", "junk_padding",
                                 // The pierce family opens here, one band after the
                                 // first flat power mod — the pair is the point.
                                 "drm_stripper"};

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

// The Bayou's CHAPTERS (core/content/story.h), wires 5-8.
// Where the operator gets the neural link, so the arrival says what it does. Cracker's
// last words are the second of three sent to the machine Morris counted (the Moors'
// chapters), in a language nothing on the device translates.
const StoryPanelDef kIntroPanels[] = {
    {"THE BAYOU",
     "The Pirate Bayou is a swampy harbour town where half the stalls sell cracked "
     "software and nobody asks where it came from. The Malwarium shop sits at the end "
     "of its pier, Leech Landing."},
    {"IN YOUR NAME",
     "The shopkeeper knows your name already. Someone has been ordering his stock with "
     "card details stolen in the Circuit, yours included. He shipped every order and "
     "never got paid."},
    {"THE DEAL",
     "Every Crew is too busy with the surge to take his case. So he offers you a deal: "
     "stop whoever is doing it, and he will fit you with a neural link, tuned far past "
     "the civilian limit."},
    {"THE LINK",
     "With it in, you can sense the networks around you without looking at a screen. The "
     "Bayou's are thick with malbeasts, and the sealed caches in your bag suddenly read "
     "open."},
    {"CRACKERS",
     "Cracked software is the Bayou's trade, so its malbeasts are crackers too: breaking "
     "through protection is what they do. Your pet's armour stops less of every hit "
     "here."},
};

const StoryPanelDef kBossIntroPanels[] = {
    {"THE CABLE",
     "Through the link you can see a thick data cable running under the boardwalk, "
     "glitching and crawling with malbeasts. You follow it past Torrent Swamp to the "
     "docks."},
    {"THE JUNCTION",
     "At the docks the Bayou's lines join the undersea cables. Every order placed on a "
     "stolen card went out from here, and the thief is sitting right on the junction."},
    {"CAP'N CRACKER",
     "Cap'n Cracker works through a list of stolen passwords, logs in as each person on "
     "it, and orders stock in their name. He set up here the week the Circuit was "
     "locked."},
    {"NO LOCK HOLDS",
     "Cracker has broken into so much that armour means nothing to him. His strongest "
     "attack goes straight through it, however much your pet is wearing."},
};

const StoryPanelDef kBossOutroPanels[] = {
    {"THE LEDGER",
     "Cracker goes down beside his ledger. He did not steal the password list himself. "
     "He bought it for eight thousand Bits, more than this whole stretch of water makes "
     "in a year."},
    {"A PURPLE APE",
     "The seller is written down as a purple ape. The Baron's gang did not just lock the "
     "Circuit: they copied every card on it while they were inside, then sold the list "
     "on."},
    {"THE SAME LANGUAGE",
     "As he goes, Cracker sends a message in the same language the Baron used. The "
     "link follows it out along the cable, under the docks, and away towards the open "
     "sea."},
};

const StoryPanelDef kAreaOutroPanels[] = {
    {"THE BACK ROOM",
     "The shopkeeper has heard what you did before you get back. He opens the back room, "
     "where the stock he never puts out is kept, and tells you it is all for sale to you "
     "now."},
    {"THE BOUNTY",
     "The Crews have put up a bounty for whoever is behind the surge. Nobody knows who "
     "that is yet, and every Crew is so short of people they will take help from anyone."},
    {"OUT OR DOWN",
     "The cable runs out to sea across the Net-Sea, and that is where the Crews are "
     "looking. The other way is down, into the Deep Web, which nobody has ever mapped."},
};

const AreaStoryDef kStory = {{
    {/*wire=*/5, "CHAPTER 2: THE LINK", kIntroPanels, arrLen(kIntroPanels)},
    {/*wire=*/6, "CHAPTER 2: THE CRACKER", kBossIntroPanels, arrLen(kBossIntroPanels)},
    {/*wire=*/7, "CHAPTER 2: THE LIST", kBossOutroPanels, arrLen(kBossOutroPanels)},
    {/*wire=*/8, "CHAPTER 2: THE ACCOUNT", kAreaOutroPanels, arrLen(kAreaOutroPanels)},
}};
}  // namespace

const AreaDef kAreaPirateBayou = {
    "pirate_bayou",
    "THE PIRATE BAYOU",
    /*badge=*/"BAYOU",
    "LEECH LORD",
    "ICON_SECTOR_PIRATE_BAYOU",
    SceneId::PirateBayou,
    {"LEECH LANDING", "TORRENT SWAMP", "THE CRACKED KEYS", "WAREZ MARSH",
     "BOOTY CACHE"},
    // The cracking area, so ARMOR PIERCE is the family its bosses teach — 25%, 50%, and
    // both of the generic pool's two full-pierce moves. A player who wants to stop caring
    // about walls farms this water.
    {{"NETBUS NIPPER", {"remote_handle"}},
     {"SUPRNOVA SERPENT", {"seed_leech"}},
     {"RAZOR KRAKEN", {"keygen_cut"}},
     {"WIGHT OF DRINKORDIE", {"nuked_release"}},
     {"THE SUNKEN SEVENTH", {"backdoor_knock"}}},
    "CAP'N CRACKER",
    /*areaBossMoveId=*/"crack_the_keys",
    /*apexThreatMoveId=*/"system_hang",  // the signature boss's STUN rider
    // The wilds pierce too, just less: Keygen Hum is the quarter-strength version of the
    // wall-opening this water's bosses teach outright, so a player meets the family long
    // before THE SUNKEN SEVENTH shows them the whole of it.
    /*wildAttackMoveId=*/"keygen_hum",
    /*wildDefendMoveId=*/"rar_password",
    // Who watches the Bayou: the thing that decides which door opens, and has never
    // once explained the sequence.
    {"THE PORT WARDEN",
     {"port_knock"},
     // Decides which door opens and has never once explained the sequence. Officious
     // rather than cruel: there IS a right answer, and it is simply not posted.
     {{"STATE YOUR BUSINESS. CORRECTLY.", "IT BLOCKS THE WAY."},
      {"THE SEQUENCE IS NOT POSTED.", "IT WANTS SOMETHING EXACT."},
      {"MANY KNOCK. FEW ARE ADMITTED.", "IT COUNTS YOU."}},
     // How it takes the answer — pleased, displeased, affront, boon. Officious to the
     // end: every outcome is a door doing something, because that is all it controls.
     {{"THE SEQUENCE WAS CORRECT.", "A DOOR COMES UNLOCKED."},
      {"THAT KNOCK WAS NOT MINE.", "EVERY PORT SHUTS AT ONCE."},
      {"YOU DID NOT KNOCK AT ALL.", "IT NEVER OPENED ITS BOOK."},
      {"COME THROUGH. I KNOW YOU.", "IT STANDS ASIDE FOR YOU."}}},
    {"PIER-TO-PEER", kShopListings, arrLen(kShopListings)},
    {"PHISHY CHIPS", kModShopListings, arrLen(kModShopListings)},
    kModPool,
    arrLen(kModPool),
    kWildLoot,
    arrLen(kWildLoot),
    kStory,
};

}  // namespace mal
