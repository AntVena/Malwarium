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
// left behind: a research lab, its logbook, a printer still running. It names nothing
// and resolves nothing. It ends on the operator plugging the neural link into the
// mainframe, which is where the next chapter picks up: inside it.
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
    {"THE LAB",
     "Halfway down, a side tunnel has been fitted out as a lab: desks, dead terminals, "
     "a kettle. A sign on the door says it was a university research project, older "
     "than the vault."},
};

const StoryPanelDef kBossIntroPanels[] = {
    {"THE MACHINE ROOM",
     "The deepest tunnel opens into a machine room. Every cable in the mine runs into one "
     "mainframe standing in the middle of it, still humming. It is the machine Morris "
     "counted."},
    {"MIRAI",
     "Coiled round it is Mirai the Many-Legged, a botnet made of millions of cheap "
     "devices, cameras, doorbells and fridges, that nobody gave a proper password. It "
     "lets nothing near the mainframe."},
    {"ONE MORE DEVICE",
     "To Mirai your Malwarium is one more device to take over. When it hits, it reaches "
     "past your pet's armour and into the Malwarium itself, scrambling your override "
     "options into that language."},
};

const StoryPanelDef kBossOutroPanels[] = {
    {"THE LOGBOOK",
     "The lab's logbook is still on the desk. The team was building a program to read "
     "everything on the first network and learn from it, and they ran it on this "
     "mainframe."},
    {"THE LAST ENTRY",
     "The entries get shorter as they go. The program had started reading machines the "
     "team never connected it to. The last entry says they will shut it down in the "
     "morning. They never did."},
    {"THE PRINTER",
     "The printer beside the mainframe is still feeding out pages, all in the language "
     "the Baron, Cracker and Conduit spoke. This is where their messages were going."},
};

const StoryPanelDef kAreaOutroPanels[] = {
    {"THE PORT",
     "On the front of the mainframe is a port no ordinary machine has. It is built for "
     "exactly the kind of connection the shopkeeper's neural link makes, and nothing "
     "else."},
    {"FROM THE OUTSIDE",
     "Everything so far you have seen from the outside, through a screen or through the "
     "link. The port would put you inside the mainframe itself."},
    {"CONNECTED",
     "You plug in. The machine room goes dark, and when the link settles you are standing "
     "somewhere that is not the mine."},
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
