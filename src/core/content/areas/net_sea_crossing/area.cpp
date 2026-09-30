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
    {"dyno_nuggets"}, {"tortilla_chip"}, {"backup_drive"}, {"disk_scrubber"},
};

// The crossing's own pool — seamanship over sabotage: hold the hull, hear what's
// coming, strip the junk off what you hauled aboard. Plus Watchdog Timer, the counter
// to this area's own apex rider (decoy_download, below): the same debut-the-threat-
// and-its-counter-together rule the Bayou and the Moors follow, except the rider here
// is a longer version of one already in play, so this restocks that counter rather
// than paying out a third.
const char* const kModPool[] = {"hardened_shell", "bundle_stripper", "ballast_cache",
                                "sonar_ping",     "salvage_rig",     "watchdog_timer",
                                "bilge_pump",     "barnacle_plating",
                                "harpoon_mount",  "distress_beacon",
                                // Second rungs: the snare the Bayou opened, and the
                                // COUNT pair, which stays a pair here for the reason it
                                // arrived as one (content_mods.cpp).
                                "depth_charge_rack", "convoy_escort", "broadside_array",
                                "hull_auger",
                                // ...and the crossing's own line set: the band a pet
                                // crosses between its first line mod and its last had
                                // handed it nothing that spoke to what it is.
                                "lookalike_cert", "ransom_locker", "signed_driver",
                                "mass_mailer", "entropy_seed"};

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

// The Crossing's CHAPTERS (core/content/story.h), wires 9-12.
// The freeze belongs to the wilds and THE GREEN BUTTON, not to Conduit: Toolbar Convoy is
// the stack (damage over time, armour, max Health), and its panel says so.
const StoryPanelDef kIntroPanels[] = {
    {"THE NET-SEA",
     "The Net-Sea is open water, a week across by ferry. You take the slow boat out of the "
     "Bayou, because the cable you are following runs along the sea floor right under its "
     "route."},
    {"THE SEA-NET",
     "Out here the only connection is the sea-net: a chain of relay buoys, each run by "
     "whichever company put it there. Using one is free, as long as you accept whatever "
     "it installs first."},
    {"BUNDLES",
     "Every buoy the ferry passes pushes something onto your Malwarium: an advert, a "
     "toolbar, an app nobody asked for. Through the link you feel each one land, and most "
     "of them are malware."},
    {"LOST TURNS",
     "The malbeasts out here are made of the same junk and fight the same way: they make "
     "your pet sit through an install or a licence screen before it can act. They cost it "
     "turns, not Health."},
};

const StoryPanelDef kBossIntroPanels[] = {
    {"THE FAR SHORE",
     "A week later the ferry reaches the far shore, where the cable comes up into a relay "
     "station. The glitch in it is worse here than it was at the Bayou."},
    {"THE BLUE CREW",
     "A blue Crew, the kind that defends networks for a living, has been clearing junk off "
     "the sea-net for months and is barely keeping up. They are glad of the help."},
    {"ADMIRAL CONDUIT",
     "Everything on the sea-net travels in convoys, and the one running them is Admiral "
     "Conduit, adware grown fat on years of traffic. Cracker's stolen orders crossed on "
     "its convoys."},
    {"NEVER ALONE",
     "Nothing on the sea-net arrives alone, and neither do Conduit's attacks. Each one "
     "drops a stack of junk on your pet that hurts for a few turns, weakens its armour "
     "and cuts its max Health."},
};

const StoryPanelDef kBossOutroPanels[] = {
    {"THE SAME LANGUAGE",
     "Conduit comes apart slowly. As it goes, it is sending to something else on the "
     "network, in the same language the Baron and Cracker used."},
    {"THE CONVOY LOG",
     "Its convoy log shows where the junk came from: forty different networks with no "
     "link to each other, and every one of them carrying the same kind of malbeast."},
    {"NOBODY IN CHARGE",
     "Nobody is organising this. Malbeasts all over the 'net are turning bad on their "
     "own, in places that have never been in touch. The one thing they share is who they "
     "talk to."},
};

const StoryPanelDef kAreaOutroPanels[] = {
    {"THE SPLIT",
     "The blue Crew keeps the sea-net and gives you a share of what they recover. It is "
     "the first time anyone has paid you as one of their own instead of thanking you for "
     "a favour."},
    {"ON FOOT",
     "They cannot spare anyone to follow the cable further. It runs inland from here, and "
     "if you want to know what is at the end of it, you will have to walk."},
    {"INLAND",
     "Inland lie the Napstorrent Moors, a stretch of bog and farmland that nobody on the "
     "coast has much reason to visit. The cable heads straight into them."},
};

const AreaStoryDef kStory = {{
    {/*wire=*/9, "CHAPTER 3: THE SEA-NET", kIntroPanels, arrLen(kIntroPanels)},
    {/*wire=*/10, "CHAPTER 3: THE CONVOYS", kBossIntroPanels, arrLen(kBossIntroPanels)},
    {/*wire=*/11, "CHAPTER 3: THE CONVOY LOG", kBossOutroPanels, arrLen(kBossOutroPanels)},
    {/*wire=*/12, "CHAPTER 3: THE SHORE", kAreaOutroPanels, arrLen(kAreaOutroPanels)},
}};
}  // namespace

const AreaDef kAreaNetSeaCrossing = {
    "net_sea_crossing",
    "NET-SEA CROSSING",
    /*badge=*/"NET-SEA",
    "BUNDLE BREAKER",
    "ICON_SECTOR_NET_SEA_CROSSING",
    SceneId::None,   // the crossing's backdrop is not authored yet
    {"UNINSTALL UNDERTOW", "POPUP WHIRLPOOL", "TRACKER TRENCH", "CODEC REEF",
     "SANDBOX BEACH"},
    // Null Route — "reroutes the next hit to nowhere" — rides with the pop-up boss, which
    // is the joke working twice: null-routing the ad domains is how that era actually
    // killed them. It is the second of the two generic braces the roster places by hand,
    // the other being Checksum Guard on Citrus Circuit's fake-file boss.
    {{"THE CANDY SIREN", {"bundle_wrap"}},
     {"VUNDO THE UNENDING", {"popup_storm", "null_route"}},
     {"THE SUPERFISH", {"cert_spoof"}},
     {"ZLOB CONGER", {"fake_codec"}},
     {"THE GREEN BUTTON", {"mirror_click"}}},
    "ADMIRAL CONDUIT",
    /*areaBossMoveId=*/"toolbar_convoy",
    /*apexThreatMoveId=*/"decoy_download",  // the signature boss's long-STUN rider
    // Everything in the crossing costs you a turn rather than Health, all the way down to
    // the wilds: Install Wizard is one turn where THE GREEN BUTTON takes three, and the
    // brace is the licence screen the whole area is really made of.
    /*wildAttackMoveId=*/"install_wizard",
    /*wildDefendMoveId=*/"eula_wall",
    // Who watches the crossing: something far down the cable that has heard every
    // packet that ever went over it and has never sent one of its own.
    {"THE DEEP LISTENER",
     {"deep_listen"},
     // Far down the cable, and has heard every packet that ever crossed it without
     // sending one of its own. Speaking at all is an event for it.
     {{"I HAVE HEARD YOU BEFORE.", "IT DOES NOT MOVE AT ALL."},
      {"SAY IT AGAIN. I AM CERTAIN NOW.", "IT LEANS WITHOUT MOVING."},
      {"I SPEAK SO RARELY. FORGIVE ME.", "SOMETHING OLD IS AWAKE."}},
     // How it takes the answer — pleased, displeased, affront, boon. It has spent its
     // whole existence telling signal from noise, so that is the judgement it passes.
     {{"I HEARD THAT CLEARLY.", "SOMETHING FAR OFF AGREES."},
      {"THAT IS NOISE. I KNOW NOISE.", "THE WATER GOES COLD."},
      {"I WILL NOT WASTE A WORD.", "IT SINKS BACK TO SILENCE."},
      {"STAY. THE CABLE IS QUIET.", "IT LISTENS WITH YOU."}}},
    {"FLOATING POINT", kShopListings, arrLen(kShopListings)},
    {"THE HARDENED SHELL", kModShopListings, arrLen(kModShopListings)},
    kModPool,
    arrLen(kModPool),
    kWildLoot,
    arrLen(kWildLoot),
    kStory,
};

}  // namespace mal
