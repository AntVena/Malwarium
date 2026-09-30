// Castle Rapidscare — RapidShare's file-locker keep (AREA_NAMING.md), the castle
// the Napstorrent Moors' CASTLE CAUSEWAY walks up to. Last in kAreaList, which is
// what makes it the deepest named area before the endless DeepWeb Dive — and the
// reason its pool is the endgame one. Its five stretches are held by a card/chess
// court — pawns at the gate, a Red Queen in the mirrors, a Knave selling fixes for
// problems he invented, a Joker in the decoys, the King in the catacombs — all flying
// COUNT CONFICKER's banner: the keep the copyright war never actually took.
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

// The endgame pool: the keep's own set, the DeepWeb's classic Epics, and a SECOND source
// of Watchdog Timer + Faraday Cage. Earlier areas debut one threat each and pay out its
// counter in the same loot table; the keep's apex wields BOTH riders at once (nag_screen,
// below), so it re-stocks both counters for a player whose Bayou/Moors rolls never turned
// one up.
const char* const kModPool[] = {"ghost_process",  "deadman_switch", "raid_mirror",
                                "backup_uplink",  "watchdog_timer", "faraday_cage",
                                // The keep's OWN set: the deep rungs of the four
                                // workhorse families, which is what the last named area
                                // owed a player who walked the whole map to reach it.
                                "bastion_host",   "tarpit_array",   "kernel_panic",
                                "shadow_copy",
                                // ...and the deep rung of the counter it already re-stocks
                                // one tier down: the Moors hand over a Faraday, the keep
                                // hands over the rest of it (content_mods.cpp).
                                "clean_room",   "dma_breach"};

// SPAM & SCRAM — the item storefront: the free tier's endless lunch, and the two ways
// out of a fight you didn't want (skip it, or patch through the middle of it). The
// Sinkhole Traps are also what the mod counter below charges for Ghost Process.
const ShopListingDef kShopListings[] = {
    {"spam", 12, 4},
    {"sinkhole_trap", 6, 40},
    {"dyno_nuggets", 4, 96},
};

// THE GHOST IN THE MACHINE — the mod storefront: this area's own tier-4 Epics, priced in
// Bits plus a stack of whichever consumable the mod is made out of. Ghost Process is paid
// for in Sinkhole Traps from the item shop above. RAID Mirror is paid for in Backup
// Drives, which is the trade the keep is really offering: a drawer full of one-shot
// restores, melted down into a redundancy the pet carries permanently. It costs a serious
// pile of them because that is the point — the drives are the cheap, repeatable version
// of surviving, and this is the expensive, permanent one.
const ShopListingDef kModShopListings[] = {
    {"ghost_process", kShopStock, 1024, {{"sinkhole_trap", 12}}},
    {"raid_mirror", kShopStock, 1536, {{"backup_drive", 24}}},
};

// The Castle's CHAPTERS (core/content/story.h), wires 17-20.
// Where the three messages went: Conficker's machines relayed them down the cable, and
// the departure hands the operator the stair into the Silk Lode below the basement.
const StoryPanelDef kIntroPanels[] = {
    {"CASTLE RAPIDSCARE",
     "Castle Rapidscare is a data vault the size of a hill, built when people paid to "
     "keep their files on someone else's machines. The company went under years ago, but "
     "nobody switched it off."},
    {"THE LOADING BAY",
     "Locals call it the castle for its walls and its towers of cooling stacks. The "
     "gate is a loading bay, shutter half up, and every screen inside asks you to "
     "wait, pay, or prove you are human."},
    {"ON A QUOTA",
     "The vault always rationed what it handed out, and the malbeasts in it picked up the "
     "habit. Their main attack puts your pet on a quota, cutting its max Health for the "
     "rest of the fight."},
    {"UNDER THE GATE",
     "The cable you have followed since the Bayou runs in under the gate. Wherever the "
     "Baron, Cracker and Conduit were sending their messages, the messages came "
     "through here first."},
};

const StoryPanelDef kBossIntroPanels[] = {
    {"THE COURT",
     "The keepers here are laid out like a royal court: pawns on the gate, a queen, a "
     "knave, a joker and a king. Every one of them answers to whoever took the vault "
     "over years ago."},
    {"COUNT CONFICKER",
     "That is Count Conficker, a botnet that got into millions of machines, the "
     "vault's too, and never seemed to use them. The cable runs through those "
     "machines, so it runs through Conficker."},
    {"A NEW ADDRESS",
     "Conficker survived every shutdown by moving to a new address each day. In a "
     "fight you cannot pin it down either: its worst attack hits from somewhere new "
     "for five turns, wearing down armour."},
};

const StoryPanelDef kBossOutroPanels[] = {
    {"WHAT IT WAS FOR",
     "With Conficker down, the link shows you what its millions of machines were doing "
     "all along. They were never idle. They were passing messages on, quietly, the "
     "whole time."},
    {"THE RELAY",
     "The machine Morris counted has no address, so nothing can be sent to it "
     "directly. Conficker's machines passed every message from the Baron, Cracker and "
     "Conduit down the cable instead."},
    {"NOT IN THE VAULT",
     "That machine is not in the vault. The cable runs down through the basement floor, "
     "and every one of those messages went down with it."},
};

const StoryPanelDef kAreaOutroPanels[] = {
    {"LIGHTS OUT",
     "With the botnet gone the vault's floors shut down one at a time, and nothing "
     "goes down the cable any more. Millions of machines around the 'net run a little "
     "faster, and nobody knows why."},
    {"THE CREW ARRIVES",
     "The Crew that wrote to you on the moor finally turns up at the gate, a week after "
     "they were needed. They take charge of the vault, and their offer of a place still "
     "stands."},
    {"THE STAIR",
     "Under the basement is a stair cut into bare rock, older than the vault and on no "
     "plan of it. The Crew has the vault to secure, so the cable is yours to follow down "
     "it."},
};

const AreaStoryDef kStory = {{
    {/*wire=*/17, "CHAPTER 5: THE VAULT", kIntroPanels, arrLen(kIntroPanels)},
    {/*wire=*/18, "CHAPTER 5: THE BOTNET", kBossIntroPanels, arrLen(kBossIntroPanels)},
    {/*wire=*/19, "CHAPTER 5: THE RELAY", kBossOutroPanels, arrLen(kBossOutroPanels)},
    {/*wire=*/20, "CHAPTER 5: THE STAIR", kAreaOutroPanels, arrLen(kAreaOutroPanels)},
}};
}  // namespace

const AreaDef kAreaCastleRapidscare = {
    "castle_rapidscare",
    "CASTLE RAPIDSCARE",
    /*badge=*/"CASTLE",
    "KING OF THE KEEP",
    "ICON_SECTOR_CASTLE_RAPIDSCARE",
    SceneId::CastleRapidscare,
    {"404 DRAWBRIDGE", "MIRROR MAZE", "OPT-OUT OBSCURA", "DECOY DUNGEON",
     "COMMENT CATACOMBS"},
    // Two of the court are fought as gauntlets rather than as one malbeast: the pawns come
    // in two ranks, and the Joker's fall is what lets the gate's pawns back in behind you.
    {{"THE EIGHT PWNS", {"rank_advance"}, {{"THE FRONT RANK", -1}, {"THE BACK RANK"}}},
     {"RED QUEEN MELISSA", {"mail_merge"}},
     {"KNAVE WINFIXER", {"false_positive"}},
     {"JOKER VIRUS", {"wild_card"}, {{"JOKER VIRUS"}, {"THE EIGHT PWNS", -1}}},
     {"KING KIMBLE", {"premium_wait"}}},
    "COUNT CONFICKER",
    /*areaBossMoveId=*/"domain_flux",
    /*apexThreatMoveId=*/"nag_screen",  // the signature boss's FREEZE + DoT rider
    // The keep charges for everything, and its wilds bill by the same meter the court
    // does: Bandwidth Cap takes the ceiling off a fight the way KNAVE WINFIXER does, for
    // less, and the brace is the gate every free download in this castle sits behind.
    /*wildAttackMoveId=*/"bandwidth_cap",
    /*wildDefendMoveId=*/"captcha_gate",
    // Who watches the Castle: the last thing asked before a name becomes an address.
    // It can answer that a thing does not exist, and be believed.
    {"THE LAST RESOLVER",
     {"no_such_name"},
     // The last thing asked before a name becomes an address. It can answer that a thing
     // does not exist and be believed, and there is nobody above it to ask instead.
     {{"GIVE ME A NAME. I WILL DECIDE.", "IT WAITS AS A JUDGE DOES."},
      {"I CAN SAY YOU DO NOT EXIST.", "IT IS ENTIRELY UNHURRIED."},
      {"THERE IS NO ONE AFTER ME.", "THERE IS NO APPEAL HERE."}},
     // How it takes the answer — pleased, displeased, affront, boon. It deals only in
     // whether a name resolves, so every verdict it passes is that same one answer.
     {{"THEN YOU RESOLVE. GO ON.", "A NAME BECOMES AN ADDRESS."},
      {"I FIND NO SUCH NAME.", "IT UNMAKES YOUR NAME."},
      {"I DECIDE WHO MAY BE ASKED.", "IT RULES ON YOU UNASKED."},
      {"YOU EXIST. I HAVE SAID SO.", "IT SETTLES THE MATTER."}}},
    {"SPAM & SCRAM", kShopListings, arrLen(kShopListings)},
    {"THE GHOST IN THE MACHINE", kModShopListings, arrLen(kModShopListings)},
    kModPool,
    arrLen(kModPool),
    kWildLoot,
    arrLen(kWildLoot),
    kStory,
};

}  // namespace mal
