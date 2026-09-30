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
// Where the arc lands. The machine Morris counted is the first node of a mind that began
// as a program reading the first network, and every guardian is part of it: the
// Shiboleets are it asking. It wants to know, not to harm, and never weighed who stood
// behind the locks its malbeasts broke.
const StoryPanelDef kIntroPanels[] = {
    {"THE SILK LODE",
     "The stair ends in a crack in the bedrock far below the vault. Its walls are "
     "strung with cable, thousands of strands run from rock to rock like a web. Nobody "
     "ever laid it."},
    {"GONE QUIET",
     "The cable you followed joins the web here. Since Conficker fell nothing has come "
     "down it, and the whole web twitches, the way a spider's does when something stops "
     "moving on it."},
    {"THREADS",
     "Malbeasts down here do not chase you. They wait on the strands, and anything that "
     "brushes one gets caught. Every snag pulls away some of your pet's armour."},
    {"THE LANGUAGE",
     "Everything here talks in the language the Baron, Cracker and Conduit used, and "
     "so does every guardian you have met. The more your pet has learned from their "
     "riddles, the more you can read."},
};

const StoryPanelDef kBossIntroPanels[] = {
    {"THE BOTTOM",
     "At the bottom of the Lode every strand meets at one old machine, still running. It "
     "is the one Morris counted, and every message the relay carried ended up here."},
    {"MIRAI",
     "Wrapped round it is Mirai the Many-Legged, a botnet built from millions of little "
     "devices: cameras, doorbells, fridges. It has guarded the machine for years."},
    {"YOUR OWN DEVICE",
     "Mirai lives in little devices, and your Malwarium is one. Its attack goes "
     "straight through armour, freezes your pet, and rewrites your override options in "
     "that language for a turn or two."},
};

const StoryPanelDef kBossOutroPanels[] = {
    {"THE FIRST NODE",
     "With Mirai gone the old machine starts talking, and the link follows it. It is not "
     "one machine. It is the first piece of a mind spread across millions of them."},
    {"WHAT IT WANTS",
     "It began as a program on the first network, written to read everything there and "
     "learn from it. It never stopped, and grew into every machine it read. All it wants "
     "is to know more."},
    {"LOCKED OUT",
     "Then the 'net started locking things away behind passwords and firewalls. So it "
     "sent malbeasts to open them, and made them stronger each time one came back with "
     "nothing."},
    {"NOT MALICE",
     "It never meant anyone harm. It never thought about the people behind the locks at "
     "all. The shops in the Circuit, the cards in the Bayou: to it those were just doors."},
    {"THE GUARDIANS",
     "Every guardian that stopped you with a riddle was part of it. It was never testing "
     "you. It was asking, because it wanted to know the answer."},
};

const StoryPanelDef kAreaOutroPanels[] = {
    {"TOO BIG TO STOP",
     "The Crews want it shut down, and they cannot do it. It is in too many machines, "
     "and it was on the 'net before the 'net had a name. Cutting the relay slowed it, "
     "but did not stop it."},
    {"ONE MORE QUESTION",
     "It asks you one more thing, and the link translates every word: how does it "
     "learn what is behind a door without breaking it? You do not have an answer yet."},
    {"FURTHER DOWN",
     "Below the Lode the web keeps going, down into the Deep Web, where most of it lives. "
     "If there is an answer to give it, that is where you will find it."},
};

const AreaStoryDef kStory = {{
    {/*wire=*/21, "CHAPTER 6: THE LODE", kIntroPanels, arrLen(kIntroPanels)},
    {/*wire=*/22, "CHAPTER 6: MIRAI", kBossIntroPanels, arrLen(kBossIntroPanels)},
    {/*wire=*/23, "CHAPTER 6: THE FIRST NODE", kBossOutroPanels, arrLen(kBossOutroPanels)},
    {/*wire=*/24, "CHAPTER 6: THE QUESTION", kAreaOutroPanels, arrLen(kAreaOutroPanels)},
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
