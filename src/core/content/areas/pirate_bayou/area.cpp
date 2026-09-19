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
// One panel here and one in the Circuit's outro end on a line the speaker should not
// know, and quote nothing: the quotes wait on CantCipher (core/model/cant.h).
const StoryPanelDef kIntroPanels[] = {
    {"LEECH LANDING",
     "The shop is the last building on Leech Landing, half of it out over the water. The "
     "clerk looks at you, then at his screen, and asks which of the eleven you have come "
     "to collect."},
    {"NOTHING WAS ORDERED",
     "You have never been in this shop. Neither had the four before you, and he shipped "
     "their crates too. He is out the cost of every one, and the Crew that handles this "
     "is months away."},
    {"HIGHER SYNC",
     "So he goes in the back and fits you a neural link. It runs above the sync a "
     "civilian rig is certified for. He will not take Bits. He wants this stopped before "
     "he goes under."},
    {"EVERY BAD NETWORK",
     "Outside you feel every bad network on the Landing, and every sealed cache you have "
     "carried since the Circuit reads open. Then the Malwarium beeps at the water. "
     "Something here is phishy."},
};

const StoryPanelDef kBossIntroPanels[] = {
    {"THE CABLE",
     "The link puts the cable under the boards in front of you, glitching where nothing "
     "should, malbeast sign the whole length. You follow it up past Torrent Swamp to the "
     "dock stacks."},
    {"THE JUNCTION",
     "The Bayou's lines drop away here and join the undersea trunks. Every crate billed "
     "to your card went out through this junction, and none of it went to the people who "
     "paid."},
    {"THEY ALL CARRY KEYS",
     "Nothing in the Keys bothers with a lock. They all carry a key instead. Your pet's "
     "armour buys it one round against that and then stops counting for anything."},
    {"CAP'N CRACKER",
     "Cracker is not big. He works a password list, answers as whoever is on it, and "
     "ships what they never ordered. He moved onto your water the week the Circuit "
     "locked down."},
};

const StoryPanelDef kBossOutroPanels[] = {
    {"THE LEDGER",
     "Cracker goes down next to an open ledger. He did not put the list together. He "
     "bought it outright for eight thousand Bits, more than this stretch of water turns "
     "over in a year."},
    // Nothing later chases the ape. The implication is the whole of it.
    {"A PURPLE APE",
     "The entry says who sold it to him. Not a name. A description: a purple ape, four "
     "weeks ago, cash up front. Every card locked down in the Circuit was on the "
     "manifest."},
    {"A FIRM HANDSHAKE",
     "He only got onto your water, he says, on a firm handshake. Then he says something "
     "else, and it is not in a language a Bayou operator has any business knowing."},
};

const StoryPanelDef kAreaOutroPanels[] = {
    {"THE BACK ROOM",
     "The clerk has started hearing people talk about you. He opens the back of the shop, "
     "where the stock that never reaches the boards is kept, and tells you all of it is "
     "for sale to you now."},
    {"THE BOUNTY",
     "There is a bounty up. Not for Cracker - nobody is paying for Cracker. It is for "
     "whatever started all of this, and every Crew on the 'net is short enough to sign "
     "anyone who can help."},
    {"OUT OR DOWN",
     "Two ways off the Landing. Out across the Net-Sea, which sends more of Cracker's "
     "sort than anyone counts, and which is where the Crews are looking. Or straight "
     "down, off the edge of the map."},
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
