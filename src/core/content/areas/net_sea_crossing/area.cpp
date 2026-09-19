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
const StoryPanelDef kIntroPanels[] = {
    {"OVER THE TRUNK",
     "The trunk you followed out of the Bayou runs along the sea floor and does not "
     "surface for a week. The only way to follow it is the sea-net, laid across the top "
     "of the water."},
    {"WATCH THIS FIRST",
     "Every private network on the route carries you for free, after an advert. Or an "
     "app. Or a toolbar you did not ask for. None of it is new; most of it stopped "
     "working properly years ago."},
    {"THE SLOW LIFE",
     "Nothing out here takes your pet's Health. It takes its turns. A licence screen is "
     "one. The worst thing on the reef holds it for three, and then asks whether you "
     "accept."},
    {"BLUE TEAM",
     "A blue team crew has been clearing this route for months and is barely ahead of "
     "it. They are glad of the help, and hand you a rig for diving the trunk, where the "
     "work never ends either."},
};

const StoryPanelDef kBossIntroPanels[] = {
    {"THE FAR JUNCTION",
     "A week of that and the cable comes up at a junction on the far shore. The glitch "
     "you have been following is worse here than it was at Leech Landing, not better."},
    {"CONVOYS",
     "Nothing crosses the sea-net on its own. It goes in convoys, and every convoy "
     "carries bundles it was not asked to carry. Cracker's list came over on one of "
     "these."},
    {"RED TEAM",
     "The bundles carry red team marks. The blue crew leaves them alone: red is inside "
     "the outfit doing the distributing, and pulling them out now would cost more than "
     "the bundles do."},
    {"ADMIRAL CONDUIT",
     "Conduit is what grew on the traffic. It runs the convoys now, and it does not "
     "damage anything - it holds you, three turns at a time, until whatever it is "
     "carrying has gone past."},
};

const StoryPanelDef kBossOutroPanels[] = {
    {"STILL TALKING",
     "Conduit comes apart slowly, the way everything out here does anything. What it "
     "says on the way down is not aimed at you, and you have heard the shape of it twice "
     "before."},
    {"FURTHER DOWN",
     "There is something a long way down the trunk that talks like that as well. It has "
     "been on this cable longer than the cable has been carrying anything, and it has "
     "never once sent."},
    {"NOT A PLOT",
     "The convoy log is no help. The bundles came aboard from forty networks and none of "
     "them know each other. Whatever is doing this is not organising it. It is already "
     "in the networks."},
};

const StoryPanelDef kAreaOutroPanels[] = {
    {"THE SPLIT",
     "The blue crew keeps the water. You get the shore and whatever the cable does after "
     "it, which is the first time anybody has handed you a share instead of a favour."},
    {"NOBODY ELSE IS GOING",
     "Nobody else is following the cable. The blue crew has a sea to clear and the red "
     "team has a job to keep. The bounty is still up, and it is still for the thing "
     "nobody has found."},
    {"OLD COUNTRY",
     "The cable comes ashore and runs inland into the Napstorrent Moors, which nobody "
     "has maintained in thirty years and nobody has been through in ten."},
};

const AreaStoryDef kStory = {{
    {/*wire=*/9, "CHAPTER 3: THE SEA-NET", kIntroPanels, arrLen(kIntroPanels)},
    {/*wire=*/10, "CHAPTER 3: THE CONVOYS", kBossIntroPanels, arrLen(kBossIntroPanels)},
    {/*wire=*/11, "CHAPTER 3: THE LISTENER", kBossOutroPanels, arrLen(kBossOutroPanels)},
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
