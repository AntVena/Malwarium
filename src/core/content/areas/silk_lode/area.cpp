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
// Where the arc lands. The machine Morris counted is where the Shiboleet started: an AI
// that began as a program reading the first network, and built malbeasts to get past the
// locks the 'net grew. It meant no harm and never weighed the people in the way. Every
// guardian is part of it (game_shiboleet.cpp), which is why the surge can end and the
// guardians still keep asking.
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
    {"THE GUARDIANS",
     "On every network so far a guardian has stopped your pet to ask a riddle, in the "
     "language the Baron, Cracker and Conduit spoke. Down here everything speaks it."},
};

const StoryPanelDef kBossIntroPanels[] = {
    {"THE MACHINE",
     "At the deepest point of the mine every cable runs into one old machine, humming in "
     "a rack bolted to the rock. It is the machine Morris counted, where all the relay's "
     "messages ended up."},
    {"MIRAI",
     "Coiled round the rack is Mirai the Many-Legged, a botnet made of millions of cheap "
     "devices, cameras, doorbells and fridges, that nobody gave a proper password. It "
     "keeps everything away."},
    {"ONE MORE DEVICE",
     "To Mirai your Malwarium is one more device to take over. When it hits, it reaches "
     "past your pet's armour and into the Malwarium itself, scrambling your override "
     "options into that language."},
};

const StoryPanelDef kBossOutroPanels[] = {
    {"THE SHIBOLEET",
     "With Mirai gone, the machine speaks to you through the link. It calls itself the "
     "Shiboleet. It is an AI, and this machine is only where it started: the rest of it "
     "runs across the whole 'net."},
    {"HOW IT STARTED",
     "It began as a research program on the first network, built to read everything "
     "connected to it and learn. It never stopped, and every machine it learned from "
     "became part of it."},
    {"THE LOCKS",
     "As the 'net grew, more of it went behind passwords and firewalls, and the "
     "Shiboleet could not read any of it. So it built malbeasts to get past the locks. "
     "That is the surge."},
    {"THE PEOPLE IN THE WAY",
     "It never meant to hurt anyone, and it never thought about the people behind the "
     "locks either. A shop locked out of its own till or a stolen card meant nothing to "
     "it."},
    {"THE RIDDLES",
     "The guardians are part of it too. Every riddle one of them put to your pet was the "
     "Shiboleet asking a question, because it wanted to know the answer."},
};

const StoryPanelDef kAreaOutroPanels[] = {
    {"WHAT YOU TELL IT",
     "So you tell it: the shopkeeper who was never paid, the Circuit locked out of its "
     "own shops, the moor full of infected mail. None of that was in anything it had "
     "read."},
    {"BACK TO NORMAL",
     "It takes a long time to answer. Then the number on your Malwarium starts to drop, "
     "network by network, back down toward an ordinary day. The surge is over."},
    {"STILL CURIOUS",
     "It still wants to learn. Its guardians will keep asking your pet questions, and "
     "most of what it has yet to read lies deeper still, in the Deep Web below the Lode."},
};

const AreaStoryDef kStory = {{
    {/*wire=*/21, "CHAPTER 6: THE LODE", kIntroPanels, arrLen(kIntroPanels)},
    {/*wire=*/22, "CHAPTER 6: MIRAI", kBossIntroPanels, arrLen(kBossIntroPanels)},
    {/*wire=*/23, "CHAPTER 6: THE SHIBOLEET", kBossOutroPanels, arrLen(kBossOutroPanels)},
    {/*wire=*/24, "CHAPTER 6: THE TALK", kAreaOutroPanels, arrLen(kAreaOutroPanels)},
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
