// The Silk Lode — Silk Road punned Road → Lode (AREA_NAMING.md), the seam the Castle's
// COMMENT CATACOMBS bottom out onto. A lode is a vein running through rock; this one is
// not ore. Every word this hobby uses for the 'net is already arachnid — a web, a
// crawler, a spider that walks it — and thirty years of calling them dead metaphors is
// what this area is about being wrong.
//
// The five stretches are ordered as a DESCENT rather than as a terrain family: a stair
// (built), a funnel and a fissure (not), a gullet (a passage something HAS), and then a
// shrine — which is the reveal that somebody put a building down here and it was not us.
// Its court is hacker groups and botnets, two of them fought as more than one round
// because a plural banner has to deliver a crowd (AREA_NAMING.md §3.2).
#include "core/content/areas/area_defs.h"

#include "tunables.h"

namespace mal {

namespace {
// The staple set every area's wild-win table carries: two snacks, the combat shield and
// the cleaner. Fighting fragments the pet, so a wild win is where the cleaner tops up.
const LootEntry kWildLoot[] = {
    {"dyno_nuggets"}, {"tortilla_chip"}, {"backup_drive"}, {"disk_scrubber"},
};

// Rank 6. Crib Sheet is the one the area OWES: an area debuts a threat and pays out its
// counter in the same table (the Bayou's stun and its Watchdog Timer), and this area's
// threat is the one that takes the picker. The rest is the deep end of the workhorse
// families, which is what the last named area owes a player who walked the whole map.
const char* const kModPool[] = {"crib_sheet",     "vault_door",    "spoof_relay",
                                "fork_farm",      "false_flag",    "recompiler",
                                "ghost_process",  "deadman_switch", "raid_mirror",
                                "bastion_host",   "tarpit_array",  "kernel_panic",
                                "shadow_copy",    "clean_room",    "dma_breach"};

// THE FLY TRAP — a food shop, in a lode full of spiders, called that. The joke is the
// whole storefront, and the Sinkhole Traps are also what the mod counter below charges.
const ShopListingDef kShopListings[] = {
    {"spam", 12, 4},
    {"sinkhole_trap", 6, 44},
    {"dyno_nuggets", 4, 104},
};

// WHAT THE WEB CAUGHT — it did not stock anything, it just has things. Crib Sheet is
// priced in Bits plus a stack of the traps from the shop above: the trade is a drawer of
// one-shot escapes melted down into never losing your grip on the fight in the first
// place.
const ShopListingDef kModShopListings[] = {
    {"crib_sheet", kShopStock, 1280, {{"sinkhole_trap", 10}}},
};

// The Lode's CHAPTERS (core/content/story.h), wires 21-24.
// Nobody is down here to explain anything, so the Lode tells its story through what was
// left behind: a university team's signs, its logbook, the printed messages. It resolves
// nothing; the one thing it hands over is a name, from the logbook: the port is the
// Shiboleet's interface. It ends on the operator plugging the neural link into it, which
// is where the next chapter picks up: inside the mainframe.
const StoryPanelDef kIntroPanels[] = {
    {"THE OLD MINE",
     "The stair leads down into an old mine under the hill, dug long before the vault was "
     "built on top of it. The miners called it the Silk Lode, after the pale seam that "
     "runs through the rock."},
    {"THE CABLES",
     "The tunnels are strung wall to wall with cable, thousands of lines spliced into one "
     "another like a web. Through the link you can see traffic moving along all of them "
     "at once."},
    {"TANGLED",
     "The malbeasts here live on the cables and barely move. They wait where the lines "
     "cross, and a pet that walks into one gets tangled up, losing some of its armour "
     "with every snag."},
    {"SIGNS OF PEOPLE",
     "Somebody worked down here once. Lights are bolted along the tunnels, there are "
     "safety notices from a university, and arrows painted on the rock all point "
     "further down."},
};

const StoryPanelDef kBossIntroPanels[] = {
    {"THE MACHINE ROOM",
     "The arrows end in a machine room with a desk and one mainframe, still humming. "
     "Every cable in the mine runs into it, and the link knows it at once: the machine "
     "Morris counted."},
    {"MIRAI",
     "Coiled round it is Mirai the Many-Legged, a botnet made of millions of cheap "
     "devices, cameras, doorbells and fridges, that nobody gave a proper password. It "
     "lets nothing near the mainframe."},
    {"ONE MORE DEVICE",
     "To Mirai your Malwarium is one more device to take over. When it hits, it "
     "reaches past your pet's armour and into the Malwarium itself, scrambling your "
     "Exploits into that language."},
};

const StoryPanelDef kBossOutroPanels[] = {
    {"THE LOGBOOK",
     "The team's logbook is still on the desk. They were building a program to read "
     "everything on the first network and learn from it, and they ran it on this "
     "mainframe."},
    {"THE LAST ENTRY",
     "The entries get shorter as they go. The program had started reading machines the "
     "team never connected it to. The last entry says they will shut it down in the "
     "morning. They never did."},
    {"THE PRINTER",
     "Under the printer is a heap of pages in the language the Baron, Cracker and "
     "Conduit spoke: their messages, printed as they arrived. The last one came the "
     "day Conficker fell."},
};

const StoryPanelDef kAreaOutroPanels[] = {
    {"THE PORT",
     "Your link reacts before you see why: the mainframe has a port no ordinary "
     "machine has, and it is reaching for the link. It was built for the same kind of "
     "connection the link makes."},
    {"THE INTERFACE",
     "The logbook calls the port the Shiboleet's interface. The team plugged into it "
     "to go inside the mainframe and work with their program directly, instead of "
     "watching it from a screen."},
    {"CONNECTED",
     "You plug in. The machine room goes dark. When the link settles you are standing "
     "in it again, except the lights are on, every terminal is running, and there is a "
     "warm kettle on the desk."},
};

const AreaStoryDef kStory = {{
    {/*wire=*/21, "CHAPTER 6: THE LODE", kIntroPanels, arrLen(kIntroPanels)},
    {/*wire=*/22, "CHAPTER 6: MIRAI", kBossIntroPanels, arrLen(kBossIntroPanels)},
    {/*wire=*/23, "CHAPTER 6: THE LOGBOOK", kBossOutroPanels, arrLen(kBossOutroPanels)},
    {/*wire=*/24, "CHAPTER 6: THE PORT", kAreaOutroPanels, arrLen(kAreaOutroPanels)},
}};
}  // namespace

const AreaDef kAreaSilkLode = {
    "silk_lode",
    "THE SILK LODE",
    /*badge=*/"SILK",
    "THREADCUTTER",
    "ICON_SECTOR_SILK_LODE",
    SceneId::SilkLode,
    {"DEADLINK STAIR", "BOTNET FUNNEL", "BITROT FISSURE", "ZOMBIE GULLET",
     "ZERO DAY SHRINE"},
    // Two of them arrive as more than one malbeast: a cult is a congregation, and a
    // coven is three. Both are drawn a rung shallower on the way in, so the escorts are
    // the same curve evaluated a step back rather than a second stat block.
    {{"CULT OF THE DEAD CODE",
      {"dead_link"},
      {{"THE FIRST RITE", -1}, {"CULT OF THE DEAD CODE"}}},
     {"MARIPOSA OF THE WEAVE", {"funnel_web"}},
     {"CIH THE UNWRITER", {"unwrite"}},
     {"NECURS THE RAISER",
      {"raise_host"},
      {{"THE FIRST HOST", -1}, {"NECURS THE RAISER"}}},
     // The signature is a SINGLE round on purpose: subBossEnemy hands the apex rider to
     // sub 4 only, and an escort is that boss drawn a rung shallower — so a gauntlet here
     // would put the area's tell on a doorway instead of on the wall. A hierophant is the
     // one who shows the sacred, and is singular, which a coven is not.
     {"THE 29A HIEROPHANT", {"polymorph"}}},
    "MIRAI THE MANY-LEGGED",
    /*areaBossMoveId=*/"legion",
    /*apexThreatMoveId=*/"c2_hijack",  // the signature boss's SCRAMBLE rider
    // The wilds bill in the same currency the court does: a thread you walked into, and
    // the flattest brace anywhere. Nothing down here chases.
    /*wildAttackMoveId=*/"snag_line",
    /*wildDefendMoveId=*/"sheet_web",
    // Who watches this network: the thing that has never chased anything in its life. It
    // put out a line, and everything arrives eventually, and it is in no hurry about you.
    {"THE PATIENT WEAVER",
     {"held_thread"},
     // Its whole register is ARRIVAL — it does not come to you, and it does not have to.
     {{"I DID NOT COME TO YOU.", "IT HAS NOT MOVED AT ALL."},
      {"YOU WALKED ONTO THE LINE.", "SOMETHING IS ALREADY TAUT."},
      {"EVERYTHING ARRIVES EVENTUALLY.", "IT IS IN NO HURRY AT ALL."}},
     // Pleased, displeased, affront, boon — all four graded as slack or tension on a
     // thread, because that is the only thing it has ever measured anything in.
     {{"THEN I WILL LET THIS ONE GO.", "ONE THREAD GOES SLACK."},
      {"THE LINE WAS ALWAYS MINE.", "EVERY THREAD DRAWS IN."},
      {"I DO NOT PARLEY WITH FOOD.", "IT SIMPLY KEEPS WAITING."},
      {"SIT ON THE LINE A WHILE.", "IT MAKES ROOM ON THE WEB."}}},
    {"THE FLY TRAP", kShopListings, arrLen(kShopListings)},
    {"WHAT THE WEB CAUGHT", kModShopListings, arrLen(kModShopListings)},
    kModPool,
    arrLen(kModPool),
    kWildLoot,
    arrLen(kWildLoot),
    kStory,
};

}  // namespace mal
