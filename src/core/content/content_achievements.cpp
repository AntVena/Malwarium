// content_achievements.cpp — the achievement table (see content_achievements.h).
//
// One content table. Edit rows here; the engine sweeps them through Game::achProgress
// and the web 'Pedia publishes them through tools/dump_content.cpp — neither needs an
// edit to carry a new row.
//
// TWO RULES WHEN ADDING A ROW:
//   1. `wire` is the next unused number. Never reuse or renumber one — it is the save's
//      bitset index (the header banner has the why). Rows are grouped by THEME below,
//      which is exactly what wire numbers buy: the reading order and the wire order are
//      allowed to disagree.
//   2. Write `{n}` where the threshold goes, never a digit — effect_text.h substitutes
//      the effective goal, so a kGoalAll row's prose grows with the set it counts over
//      and a retuned rung retunes its own sentence.
//
// REWARD CONVENTION (keep a new ladder in step with the ones here):
//   entry rung  ~25 Bits · mid rung ~60 Bits + a Common Cache · deep rung ~150 Bits + a
//   Rare Cache · capstone ~400 Bits + a Commendation Cache (the earned-only container,
//   content_items.cpp). A one-off Event row pays like the rung it feels like.
//
// Icons reuse the shipped item/slot/line glyphs — no achievement-specific art exists yet
// (assets/ASSET_MANIFEST.md), and the 'Pedia is the only surface that draws
// one, so a reused glyph costs nothing on the device.
#include "core/content/content_achievements.h"

#include <cstring>

#include "core/content/defs.h"   // ItemDef::Rarity, for the RarityCollected rows' param

namespace mal {

namespace {
using AR = AchievementReward;
constexpr AR bits(int n) { return {AR::Kind::Bits, n, nullptr}; }
constexpr AR item(const char* id) { return {AR::Kind::Item, 1, id}; }

// Icon shorthands, so a row's art choice reads as one word and a re-point is one edit.

constexpr int kRarityRare = static_cast<int>(ItemDef::Rarity::Rare);
constexpr int kRarityEpic = static_cast<int>(ItemDef::Rarity::Epic);
}  // namespace

const AchievementDef kAchievements[] = {
    // --- Care & lifecycle: the one-off moments, fired at the event ----------------
    // These leave no counter behind to test for, so each is an explicit
    // Game::unlockAchievement() at the site where it happens (AchSeries::Event).
    {/*wire=*/0, "FIRST_BRUTE_FORCE", "First Brute Force",
     "Break a DISK DECRYPTION key on a Ransomware egg.", "ICON_ACH_FIRST_BRUTE_FORCE",
     AchSeries::Event, /*goal=*/0, nullptr, 0, {bits(25)}},
    {/*wire=*/1, "SURVIVED_LOCKOUT", "Survived Lockout",
     "Resolve a Lockout Timer before it expires.", "ICON_ACH_SURVIVED_LOCKOUT",
     AchSeries::Event, /*goal=*/0, nullptr, 0, {bits(40)}},
    {/*wire=*/2, "FLAWLESS_RUN", "Flawless Run",
     "Reach Daemon with zero care mistakes.", "ICON_ACH_FLAWLESS_RUN",
     AchSeries::Event, /*goal=*/0, nullptr, 0, {bits(400), item("commend_cache")}},
    {/*wire=*/3, "GONE_ROGUE", "Gone Rogue",
     "Raise a pet all the way to a Bad-branch Daemon.", "ICON_ACH_GONE_ROGUE",
     AchSeries::Event, /*goal=*/0, nullptr, 0, {bits(150), item("sealed_cache_rare")}},
    // WORM_WHISPERER still has no firing site, and the reason is not a missing unlock
    // call: the Worm line has no content rows at all, so there is nothing to hatch. It
    // needs a creature line before it needs code. The row exists so the 'Pedia's map is
    // complete — wire the unlock where the line's hatch lands.
    {/*wire=*/4, "WORM_WHISPERER", "Worm Whisperer",
     "Hatch the Worm line through a clean Isolation Protocol.", "ICON_ACH_WORM_WHISPERER",
     AchSeries::Event, /*goal=*/0, nullptr, 0, {bits(150), item("sealed_cache_rare")}},
    // The same shape one line over: a hatch board played to the end, which on the
    // CHROMATOPHORE means every pass made in the right skin — and since those passes
    // price out at the whole incubation clock, the egg hatches out of the run itself.
    {/*wire=*/97, "NEVER_SEEN", "Never Seen",
     "Hatch the Metamorphic line without being spotted once.", "ICON_ACH_NEVER_SEEN",
     AchSeries::Event, /*goal=*/0, nullptr, 0, {bits(150), item("sealed_cache_rare")}},
    // Fires from Unlinkguine's own ClearReplicationGhost effect
    // (Game::applyItemEffects), and only when there was a ghost to cut loose — eating the
    // dish as ordinary food is the common case and unlocks nothing. A ghost is raised by
    // a defrag that FAILS on an already-Critical disk (Game::resolveMaint), Worm line only.
    //
    // The `wire` and the KEY stay AIR_GAPPED though the dish that earns it is now named
    // for unlink(): the key is exchanged verbatim in the peer/PvP JSON, so it is a wire
    // value rather than a label, and renaming it would desync against every device still
    // on an older build. The display name below is the part players read.
    {/*wire=*/7, "AIR_GAPPED", "Unlinked",
     "Cure a Replication Ghost with Unlinkguine.", "ICON_ACH_AIR_GAPPED",
     AchSeries::Event, /*goal=*/0, nullptr, 0, {bits(60), item("sealed_cache_common")}},
    {/*wire=*/13, "TROJAN_UNLEASHED", "Trojan Unleashed",
     "Have a pet defect into the Trojan family mid-evolution. Unlocks the line.", "ICON_ACH_TROJAN_UNLEASHED",
     AchSeries::Event, /*goal=*/0, nullptr, 0, {bits(400), item("commend_cache")}},
    // The Worm line's unlock, and the second row (with TROJAN_UNLEASHED above) that a
    // line-select gate reads rather than a 'Pedia page. Fires from Game::archStoreActive
    // the moment a freeze leaves the rack holding two of one species — the player has
    // replicated something, which is the only credential the replication line asks for.
    // Sticky by construction: it is an earned bit, so releasing one of the pair later
    // cannot take the line back off the menu.
    {/*wire=*/68, "SECOND_INSTANCE", "Second Instance",
     "Hold two of the same species in the ARCH rack at once. Unlocks the Worm line.",
     "ICON_ACH_SECOND_INSTANCE",
     AchSeries::Event, /*goal=*/0, nullptr, 0, {bits(200), item("sealed_cache_rare")}},
    // The mirror match. Both fighters resolve to one species, which is the one thing a
    // roster of distinct silhouettes is not supposed to be able to show you — so it
    // earns the line that is ABOUT being indistinguishable from something else.
    {/*wire=*/94, "HASH_COLLISION", "Hash Collision",
     "Face a pet of your own species, over the LINK or in the arena. "
     "Unlocks the Metamorphic line.", "ICON_ACH_HASH_COLLISION",
     AchSeries::Event, /*goal=*/0, nullptr, 0, {bits(200), item("sealed_cache_rare")}},
    {/*wire=*/14, "FIRST_DUEL", "Packet Exchange",
     "Fight another operator's pet over the LINK.", "ICON_ACH_FIRST_DUEL",
     AchSeries::Event, /*goal=*/0, nullptr, 0, {bits(60), item("sealed_cache_common")}},
    // The Backup Drive's three endings (Game::settleBackupDrive). One row per way the
    // fight can go after the drive is spent, so between them they cover every outcome
    // the player can actually watch — the save working and holding, the save working and
    // not being enough, and the save not working at all.
    {/*wire=*/60, "BACK_UP_AND_DRIVEN", "Back-Up & Driven",
     "Win a fight your pet only survived because a Backup Drive restored it.",
     "ICON_ACH_BACK_UP_AND_DRIVEN",
     AchSeries::Event, /*goal=*/0, nullptr, 0, {bits(150), item("sealed_cache_rare")}},
    {/*wire=*/61, "NEEDED_MORE_BACKUP", "Needed More Backup",
     "Lose a fight after a Backup Drive had already brought your pet back.",
     "ICON_ACH_NEEDED_MORE_BACKUP",
     AchSeries::Event, /*goal=*/0, nullptr, 0, {bits(60), item("sealed_cache_common")}},
    {/*wire=*/62, "SHATTERED_PLATTER", "Shattered Platter",
     "Go down so hard that a Backup Drive's restore couldn't lift your pet back up.",
     "ICON_ACH_SHATTERED_PLATTER",
     AchSeries::Event, /*goal=*/0, nullptr, 0, {bits(150), item("sealed_cache_rare")}},

    // Hidden: the 'Pedia masks the trigger, because working it out IS the achievement.
    {/*wire=*/9, "DEVTOOLS_INTRUDER", "DevTools Intruder",
     "????????", "ICON_ACH_DEVTOOLS_INTRUDER",
     AchSeries::Event, /*goal=*/0, nullptr, 0, {bits(150)}, /*hidden=*/true},

    // --- Generations & the roster --------------------------------------------------
    {/*wire=*/8, "GENERATION_X", "Generation X",
     "Raise {n} pets across lifecycles.", "ICON_ACH_GENERATION_X",
     AchSeries::PetsRaised, /*goal=*/10, nullptr, 0,
     {bits(150), item("sealed_cache_rare")}},
    {/*wire=*/15, "GENERATION_25", "Generation XXV",
     "Raise {n} pets across lifecycles.", "ICON_ACH_GENERATION_25",
     AchSeries::PetsRaised, /*goal=*/25, nullptr, 0,
     {bits(400), item("commend_cache")}},
    {/*wire=*/16, "SPECIES_3", "Menagerie",
     "Raise {n} different species.", "ICON_ACH_SPECIES_3",
     AchSeries::SpeciesRaised, /*goal=*/3, nullptr, 0, {bits(25)}},
    {/*wire=*/17, "SPECIES_6", "Habitat Curator",
     "Raise {n} different species.", "ICON_ACH_SPECIES_6",
     AchSeries::SpeciesRaised, /*goal=*/6, nullptr, 0,
     {bits(60), item("sealed_cache_common")}},
    {/*wire=*/18, "SPECIES_12", "Taxonomist",
     "Raise {n} different species.", "ICON_ACH_SPECIES_12",
     AchSeries::SpeciesRaised, /*goal=*/12, nullptr, 0,
     {bits(150), item("sealed_cache_rare")}},
    // A whole line raised, rung by rung — kGoalAll counts over that line's own roster, so
    // adding a creature to a line re-opens its row instead of silently mis-stating it.
    {/*wire=*/5, "FULL_PEDIA_L1", "Full 'Pedia: L1",
     "Raise every one of the {n} creatures in the {key} line.", "ICON_ACH_FULL_PEDIA_L1",
     AchSeries::LineRaised, kGoalAll, "ransomware", 0,
     {bits(400), item("commend_cache")}},
    {/*wire=*/19, "FULL_LINE_PHISHING", "Full 'Pedia: Phish",
     "Raise every one of the {n} creatures in the {key} line.", "ICON_ACH_FULL_LINE_PHISHING",
     AchSeries::LineRaised, kGoalAll, "phishing", 0,
     {bits(400), item("commend_cache")}},
    {/*wire=*/20, "FULL_LINE_TROJAN", "Full 'Pedia: Trojan",
     "Raise every one of the {n} creatures in the {key} line.", "ICON_ACH_FULL_LINE_TROJAN",
     AchSeries::LineRaised, kGoalAll, "trojan", 0,
     {bits(400), item("commend_cache")}},
    // The Worm line reads the same as its three siblings, Rootgrub's Trojan divert
    // included: a divert is a chance taken at one boundary, not a road closed, so a
    // raise that loses Shenloop and Threadbore to it is a raise to do again — exactly
    // what Phishlet's divert already means for the Phishing row above.
    {/*wire=*/93, "FULL_LINE_WORM", "Full 'Pedia: Worm",
     "Raise every one of the {n} creatures in the {key} line.", "ICON_ACH_FULL_LINE_WORM",
     AchSeries::LineRaised, kGoalAll, "worm", 0,
     {bits(400), item("commend_cache")}},

    // --- The DeepWeb Dive ---------------------------------------------------------
    // The device-wide ladder. Depth 8 and 64 are CONTENT GATES as well as badges (the
    // Phishing egg line, then Phishlet in its hatch pool) — see content_achievements.h.
    {/*wire=*/10, "DEEPWEB_DEPTH_8", "Down the Rabbit Hole",
     "Reach depth {n} in a DeepWeb Dive. Unlocks the Phishing egg line.", "ICON_ACH_DEEPWEB_DEPTH_8",
     AchSeries::DeepWebDepth, /*goal=*/8, nullptr, 0, {bits(25)}},
    {/*wire=*/11, "DEEPWEB_DEPTH_64", "Deep Diver",
     "Reach depth {n} in a DeepWeb Dive.", "ICON_ACH_DEEPWEB_DEPTH_64",
     AchSeries::DeepWebDepth, /*goal=*/64, nullptr, 0,
     {bits(60), item("sealed_cache_common")}},
    {/*wire=*/12, "DEEPWEB_DEPTH_256", "Abyssal",
     "Reach depth {n} in a DeepWeb Dive.", "ICON_ACH_DEEPWEB_DEPTH_256",
     AchSeries::DeepWebDepth, /*goal=*/256, nullptr, 0,
     {bits(150), item("sealed_cache_rare")}},
    {/*wire=*/21, "DEEPWEB_DEPTH_512", "Bathypelagic",
     "Reach depth {n} in a DeepWeb Dive.", "ICON_ACH_DEEPWEB_DEPTH_512",
     AchSeries::DeepWebDepth, /*goal=*/512, nullptr, 0,
     {bits(250), item("sealed_cache_rare")}},
    {/*wire=*/22, "DEEPWEB_DEPTH_999", "Bottom of the Stack",
     "Reach depth {n} in a DeepWeb Dive.", "ICON_ACH_DEEPWEB_DEPTH_999",
     AchSeries::DeepWebDepth, /*goal=*/999, nullptr, 0,
     {bits(999), item("commend_cache")}},
    // Per-LINE depth: the same dive, credited to whichever line the diving pet belongs
    // to, so a deep run is worth repeating with a different family.
    {/*wire=*/23, "DEEP_LINE_RANSOMWARE", "Ransom Depths",
     "Reach depth {n} with a {key} pet.", "ICON_ACH_DEEP_LINE_RANSOMWARE",
     AchSeries::DeepWebDepthLine, /*goal=*/256, "ransomware", 0,
     {bits(250), item("sealed_cache_rare")}},
    {/*wire=*/24, "DEEP_LINE_PHISHING", "Phish Depths",
     "Reach depth {n} with a {key} pet.", "ICON_ACH_DEEP_LINE_PHISHING",
     AchSeries::DeepWebDepthLine, /*goal=*/256, "phishing", 0,
     {bits(250), item("sealed_cache_rare")}},
    {/*wire=*/25, "DEEP_LINE_TROJAN", "Trojan Depths",
     "Reach depth {n} with a {key} pet.", "ICON_ACH_DEEP_LINE_TROJAN",
     AchSeries::DeepWebDepthLine, /*goal=*/256, "trojan", 0,
     {bits(250), item("sealed_cache_rare")}},
    // ... and per-SPECIES, counted as a bench rather than a row each: every species keeps
    // its own deepest dive, and these ask how many of them got respectably deep.
    {/*wire=*/26, "DEEP_BENCH_3", "Deep Bench",
     "Take {n} different species to depth {p}.", "ICON_ACH_DEEP_BENCH_3",
     AchSeries::SpeciesAtDepth, /*goal=*/3, nullptr, /*param=*/64,
     {bits(150), item("sealed_cache_rare")}},
    {/*wire=*/27, "DEEP_BENCH_6", "Deeper Bench",
     "Take {n} different species to depth {p}.", "ICON_ACH_DEEP_BENCH_6",
     AchSeries::SpeciesAtDepth, /*goal=*/6, nullptr, /*param=*/64,
     {bits(400), item("commend_cache")}},

    // --- Bosses, areas & the bestiary ---------------------------------------------
    {/*wire=*/28, "BOSS_FIRST", "Root Access",
     "Defeat {n} boss.", "ICON_ACH_BOSS_FIRST",
     AchSeries::BossWins, /*goal=*/1, nullptr, 0, {bits(25)}},
    {/*wire=*/29, "BOSS_10", "Boss Sweep",
     "Defeat {n} bosses.", "ICON_ACH_BOSS_10",
     AchSeries::BossWins, /*goal=*/10, nullptr, 0,
     {bits(60), item("sealed_cache_common")}},
    {/*wire=*/30, "BOSS_25", "Privilege Escalated",
     "Defeat {n} bosses.", "ICON_ACH_BOSS_25",
     AchSeries::BossWins, /*goal=*/25, nullptr, 0,
     {bits(150), item("sealed_cache_rare")}},
    {/*wire=*/31, "BOSS_50", "Boss Rush",
     "Defeat {n} bosses.", "ICON_ACH_BOSS_50",
     AchSeries::BossWins, /*goal=*/50, nullptr, 0,
     {bits(250), item("sealed_cache_rare")}},
    {/*wire=*/32, "BOSS_100", "Boss Centurion",
     "Defeat {n} bosses.", "ICON_ACH_BOSS_100",
     AchSeries::BossWins, /*goal=*/100, nullptr, 0,
     {bits(400), item("commend_cache")}},
    {/*wire=*/33, "SUBS_5", "Sector Sweeper",
     "Clear {n} sub-areas of the 'net.", "ICON_ACH_SUBS_5",
     AchSeries::SubAreasCleared, /*goal=*/5, nullptr, 0, {bits(25)}},
    {/*wire=*/34, "SUBS_10", "Cartographer",
     "Clear {n} sub-areas of the 'net.", "ICON_ACH_SUBS_10",
     AchSeries::SubAreasCleared, /*goal=*/10, nullptr, 0,
     {bits(60), item("sealed_cache_common")}},
    {/*wire=*/35, "SUBS_ALL", "Every Last Node",
     "Clear all {n} sub-areas of the 'net.", "ICON_ACH_SUBS_ALL",
     AchSeries::SubAreasCleared, kGoalAll, nullptr, 0,
     {bits(400), item("commend_cache")}},
    {/*wire=*/36, "AREA_FIRST", "Zone Cleared",
     "Beat the area boss of {n} area.", "ICON_ACH_AREA_FIRST",
     AchSeries::AreasCleared, /*goal=*/1, nullptr, 0,
     {bits(60), item("sealed_cache_common")}},
    {/*wire=*/37, "AREAS_ALL", "Net Runner",
     "Beat the area boss of all {n} areas.", "ICON_ACH_AREAS_ALL",
     AchSeries::AreasCleared, kGoalAll, nullptr, 0,
     {bits(400), item("commend_cache")}},
    {/*wire=*/38, "MALBEAST_3", "Beast Wrangler",
     "Defeat {n} different wild malbeasts.", "ICON_ACH_MALBEAST_3",
     AchSeries::MalbeastsDefeated, /*goal=*/3, nullptr, 0,
     {bits(60), item("sealed_cache_common")}},
    {/*wire=*/39, "MALBEAST_ALL", "Full Bestiary",
     "Defeat all {n} wild malbeasts.", "ICON_ACH_MALBEAST_ALL",
     AchSeries::MalbeastsDefeated, kGoalAll, nullptr, 0,
     {bits(400), item("commend_cache")}},

    // --- Cuisine & the collection -------------------------------------------------
    // "Held", not "eaten": the tally is written the moment an item reaches the bag, so a
    // food counts whether it was fed to the pet, banked, or cooked into something else.
    {/*wire=*/40, "CUISINE_3", "Taste Tester",
     "Get hold of {n} different foods.", "ICON_ACH_CUISINE_3",
     AchSeries::FoodsCollected, /*goal=*/3, nullptr, 0, {bits(25)}},
    {/*wire=*/41, "CUISINE_6", "Gourmand",
     "Get hold of {n} different foods.", "ICON_ACH_CUISINE_6",
     AchSeries::FoodsCollected, /*goal=*/6, nullptr, 0,
     {bits(60), item("sealed_cache_common")}},
    {/*wire=*/42, "CUISINE_ALL", "Full Buffet",
     "Get hold of all {n} foods, the cooked ones included.", "ICON_ACH_CUISINE_ALL",
     AchSeries::FoodsCollected, kGoalAll, nullptr, 0,
     {bits(400), item("commend_cache")}},
    // The two dishes nothing drops — the Merge Hub is the only way to hold one, so these
    // are the standouts of the cuisine group rather than another counting rung.
    {/*wire=*/43, "COOK_NOODLES", "Short Order Cook",
     "Cook Pwnzu-Patched Noodles at the Merge Hub.", "ICON_ACH_COOK_NOODLES",
     AchSeries::ItemCollected, /*goal=*/1, "pwnzu_patched_noodles", 0,
     {bits(150), item("sealed_cache_rare")}},
    {/*wire=*/44, "COOK_NACHOS", "Fully Stacked",
     "Cook Fully-Stacked Nachos at the Merge Hub.", "ICON_ACH_COOK_NACHOS",
     AchSeries::ItemCollected, /*goal=*/1, "fully_stacked_nachos", 0,
     {bits(150), item("sealed_cache_rare")}},
    // The rungs between "you have eaten a few things" and the whole shelf. The pantry is
    // now the biggest table in the game, so Taste Tester to Full Buffet without these is
    // three foods and then two hundred — a ladder with no middle is a ladder nobody
    // climbs. Icons are shared with the rungs above and below on purpose: this is one
    // series getting more steps, not four new kinds of thing to recognise.
    {/*wire=*/82, "CUISINE_25", "Line Cook",
     "Get hold of {n} different foods.", "ICON_ACH_CUISINE_6",
     AchSeries::FoodsCollected, /*goal=*/25, nullptr, 0,
     {bits(120), item("sealed_cache_uncommon")}},
    {/*wire=*/83, "CUISINE_60", "Sous Chef",
     "Get hold of {n} different foods.", "ICON_ACH_CUISINE_6",
     AchSeries::FoodsCollected, /*goal=*/60, nullptr, 0,
     {bits(200), item("sealed_cache_rare")}},
    {/*wire=*/84, "CUISINE_120", "Head Chef",
     "Get hold of {n} different foods.", "ICON_ACH_CUISINE_ALL",
     AchSeries::FoodsCollected, /*goal=*/120, nullptr, 0,
     {bits(300), item("sealed_cache_epic")}},

    // What a player actually climbs: not dishes HELD but methods KNOWN. Every rung here
    // is a Decryptogram somebody cracked, since no shop sells a recipe at any price — so
    // this series is the one place the board's whole prize ladder is measured back.
    {/*wire=*/85, "RECIPES_1", "First Method",
     "Learn your first Merge Hub recipe.", "ICON_ACH_COOK_NOODLES",
     AchSeries::RecipesKnown, /*goal=*/1, nullptr, 0, {bits(50)}},
    {/*wire=*/86, "RECIPES_10", "Working Kitchen",
     "Know {n} Merge Hub recipes.", "ICON_ACH_COOK_NOODLES",
     AchSeries::RecipesKnown, /*goal=*/10, nullptr, 0,
     {bits(150), item("sealed_cache_uncommon")}},
    {/*wire=*/87, "RECIPES_30", "The Book",
     "Know {n} Merge Hub recipes.", "ICON_ACH_COOK_NACHOS",
     AchSeries::RecipesKnown, /*goal=*/30, nullptr, 0,
     {bits(300), item("sealed_cache_rare")}},
    {/*wire=*/88, "RECIPES_60", "Brigade",
     "Know {n} Merge Hub recipes.", "ICON_ACH_COOK_NACHOS",
     AchSeries::RecipesKnown, /*goal=*/60, nullptr, 0,
     {bits(500), item("sealed_cache_epic")}},
    {/*wire=*/89, "RECIPES_ALL", "Every Method",
     "Learn all {n} Merge Hub recipes.", "ICON_ACH_CUISINE_ALL",
     AchSeries::RecipesKnown, kGoalAll, nullptr, 0,
     {bits(800), item("commend_cache")}},

    // Three dishes worth their own row, for what cooking one PROVES rather than for the
    // plate: Tiramisudo is the only food that permanently upgrades the pet, Portridge is
    // the method that changes nothing, and Recursive Turducken wants three of one bird.
    {/*wire=*/90, "COOK_TIRAMISUDO", "Ask It Nicely",
     "Cook a Tiramisudo at the Merge Hub.", "ICON_ACH_COOK_NACHOS",
     AchSeries::ItemCollected, /*goal=*/1, "tiramisudo", 0,
     {bits(250), item("sealed_cache_rare")}},
    {/*wire=*/91, "COOK_PORTRIDGE", "Runs Anywhere",
     "Cook a Portridge, and change absolutely nothing.", "ICON_ACH_COOK_NOODLES",
     AchSeries::ItemCollected, /*goal=*/1, "portridge", 0, {bits(75)}},
    {/*wire=*/92, "COOK_TURDUCKEN", "Base Case",
     "Cook a Recursive Turducken at the Merge Hub.", "ICON_ACH_COOK_NACHOS",
     AchSeries::ItemCollected, /*goal=*/1, "recursive_turducken", 0,
     {bits(250), item("sealed_cache_rare")}},

    // Rarity as a whole: the standout for the item collection.
    {/*wire=*/45, "COLLECT_RARE", "Rare Collector",
     "Get hold of all {n} Rare items.", "ICON_ACH_COLLECT_RARE",
     AchSeries::RarityCollected, kGoalAll, nullptr, kRarityRare,
     {bits(250), item("sealed_cache_rare")}},
    {/*wire=*/46, "COLLECT_EPIC", "Epic Collector",
     "Get hold of all {n} Epic items.", "ICON_ACH_COLLECT_EPIC",
     AchSeries::RarityCollected, kGoalAll, nullptr, kRarityEpic,
     {bits(400), item("commend_cache")}},

    // --- The rig ------------------------------------------------------------------
    {/*wire=*/47, "RIG_FIRST", "First Upgrade",
     "Buy {n} rig upgrade from the Hacker SHOP.", "ICON_ACH_RIG_FIRST",
     AchSeries::RigRowsOwned, /*goal=*/1, nullptr, 0, {bits(25)}},
    {/*wire=*/48, "RIG_5", "Rig Builder",
     "Own {n} different rig upgrades.", "ICON_ACH_RIG_5",
     AchSeries::RigRowsOwned, /*goal=*/5, nullptr, 0,
     {bits(60), item("sealed_cache_common")}},
    {/*wire=*/49, "RIG_10", "Overclocked Rig",
     "Own {n} different rig upgrades.", "ICON_ACH_RIG_10",
     AchSeries::RigRowsOwned, /*goal=*/10, nullptr, 0,
     {bits(150), item("sealed_cache_rare")}},
    {/*wire=*/50, "RIG_ALL", "Full Rig",
     "Own at least one level of all {n} rig upgrades.", "ICON_ACH_RIG_ALL",
     AchSeries::RigRowsOwned, kGoalAll, nullptr, 0,
     {bits(400), item("commend_cache")}},
    // Bandwidth is uncapped, so it sits outside this one by design — see RigCappedLevels.
    {/*wire=*/51, "RIG_MAXED", "Maxed Out",
     "Buy all {n} capped rig upgrade levels there are.", "ICON_ACH_RIG_MAXED",
     AchSeries::RigCappedLevels, kGoalAll, nullptr, 0,
     {bits(999), item("commend_cache")}},

    // --- The radio ----------------------------------------------------------------
    {/*wire=*/52, "NETS_10", "Wardriver",
     "Resolve {n} unique Wi-Fi networks on the walk.", "ICON_ACH_NETS_10",
     AchSeries::NetworksSeen, /*goal=*/10, nullptr, 0, {bits(25)}},
    {/*wire=*/53, "NETS_50", "Signal Hunter",
     "Resolve {n} unique Wi-Fi networks on the walk.", "ICON_ACH_NETS_50",
     AchSeries::NetworksSeen, /*goal=*/50, nullptr, 0,
     {bits(60), item("sealed_cache_common")}},
    {/*wire=*/54, "NETS_100", "Spectrum Mapper",
     "Resolve {n} unique Wi-Fi networks on the walk.", "ICON_ACH_NETS_100",
     AchSeries::NetworksSeen, /*goal=*/100, nullptr, 0,
     {bits(150), item("sealed_cache_rare")}},
    {/*wire=*/55, "NETS_290", "Airwave Authority",
     "Resolve {n} unique Wi-Fi networks on the walk.", "ICON_ACH_NETS_290",
     AchSeries::NetworksSeen, /*goal=*/290, nullptr, 0,
     {bits(400), item("commend_cache")}},
    {/*wire=*/56, "SHAKE_1", "First Handshake",
     "Capture {n} unique WPA handshake in Audit mode.", "ICON_ACH_SHAKE_1",
     AchSeries::Handshakes, /*goal=*/1, nullptr, 0, {bits(25)}},
    {/*wire=*/57, "SHAKE_10", "Handshake Habit",
     "Capture {n} unique WPA handshakes in Audit mode.", "ICON_ACH_SHAKE_10",
     AchSeries::Handshakes, /*goal=*/10, nullptr, 0,
     {bits(60), item("sealed_cache_common")}},
    {/*wire=*/58, "SHAKE_50", "Handshake Hoarder",
     "Capture {n} unique WPA handshakes in Audit mode.", "ICON_ACH_SHAKE_50",
     AchSeries::Handshakes, /*goal=*/50, nullptr, 0,
     {bits(400), item("commend_cache")}},

    // --- The DEFRAG minigame --------------------------------------------------------
    // The Stacker variant is the one maintenance path that pays with SKILL rather than
    // luck or an item (core/model/stacker.h), and it is deterministic, so these are the
    // only rows on the board that a player can practise their way to rather than grind.
    // The ladder counts cleared boards; the two below it read the width the run reached
    // the top with, which is the whole difficulty curve in one number.
    {/*wire=*/63, "DEFRAG_BY_HAND", "Defragged by Hand",
     "Clear {n} disk by hand in the DEFRAG minigame.", "ICON_ACH_DEFRAG_BY_HAND",
     AchSeries::StackerWins, /*goal=*/1, nullptr, 0, {bits(25)}},
    {/*wire=*/64, "STACK_10", "Sector Stacker",
     "Clear {n} disks by hand in the DEFRAG minigame.", "ICON_ACH_STACK_10",
     AchSeries::StackerWins, /*goal=*/10, nullptr, 0,
     {bits(60), item("sealed_cache_common")}},
    {/*wire=*/65, "STACK_50", "Contiguous",
     "Clear {n} disks by hand in the DEFRAG minigame.", "ICON_ACH_STACK_50",
     AchSeries::StackerWins, /*goal=*/50, nullptr, 0,
     {bits(250), item("sealed_cache_rare")}},
    // Reaching the top having never once overhung the row below — nine perfect locks,
    // the last four of them at the doubled step. Priced as a capstone because it is one:
    // no amount of playing makes it likelier, only getting better at it does.
    {/*wire=*/66, "PERFECT_DEFRAG", "Zero Fragmentation",
     "Clear a DEFRAG board without shaving off a single block.", "ICON_ACH_PERFECT_DEFRAG",
     AchSeries::Event, /*goal=*/0, nullptr, 0, {bits(400), item("commend_cache")}},
    {/*wire=*/67, "HANGING_BY_A_BIT", "Hanging by a Bit",
     "Clear a DEFRAG board with a single block left standing.", "ICON_ACH_HANGING_BY_A_BIT",
     AchSeries::Event, /*goal=*/0, nullptr, 0, {bits(150), item("sealed_cache_rare")}},

    // --- The GAMES arcade ----------------------------------------------------------
    // Counted off the cabinets' own tallies (content_arcade.h), so every cabinet feeds
    // the same three ladders and a new one needs no row here. The arcade pays for the
    // attempt, and so do these: the plays ladder is the long one, and the losses row
    // exists so a run of bad luck is still going somewhere.
    {/*wire=*/75, "ARCADE_FIRST", "Insert Bit",
     "Finish {n} run in the GAMES arcade.", "ICON_ACH_ARCADE_FIRST",
     AchSeries::ArcadePlays, /*goal=*/1, nullptr, 0, {bits(25)}},
    {/*wire=*/76, "ARCADE_25", "Idle Cycles",
     "Finish {n} runs in the GAMES arcade.", "ICON_ACH_ARCADE_25",
     AchSeries::ArcadePlays, /*goal=*/25, nullptr, 0,
     {bits(60), item("sealed_cache_common")}},
    {/*wire=*/77, "ARCADE_100", "Thrashing",
     "Finish {n} runs in the GAMES arcade.", "ICON_ACH_ARCADE_100",
     AchSeries::ArcadePlays, /*goal=*/100, nullptr, 0,
     {bits(250), item("sealed_cache_rare")}},
    {/*wire=*/78, "ARCADE_WINS_10", "High Score",
     "Win {n} arcade runs.", "ICON_ACH_ARCADE_WINS_10",
     AchSeries::ArcadeWins, /*goal=*/10, nullptr, 0,
     {bits(60), item("sealed_cache_common")}},
    // Losing is a tally too, and it is the one a bad player fills fastest — which is
    // exactly why it pays: the arcade should never be a place effort goes nowhere.
    {/*wire=*/79, "SUNK_COST", "Sunk Cost",
     "Come away from {n} arcade runs without a win.", "ICON_ACH_SUNK_COST",
     AchSeries::ArcadeLosses, /*goal=*/25, nullptr, 0,
     {bits(60), item("sealed_cache_common")}},
    // Cabinet-specific, keyed on the row's own id — the arcade Stacker, not the DEFRAG
    // one, which has its own rows above.
    {/*wire=*/80, "TOWER_OF_FRAGGLE", "Tower of Fraggle",
     "Clear the DEFRAG STACKER cabinet in the GAMES arcade.", "ICON_ACH_TOWER_OF_FRAGGLE",
     AchSeries::ArcadeCabinetWins, /*goal=*/1, "stacker", 0,
     {bits(60), item("sealed_cache_common")}},
    // Hidden: the joke only lands if the player wasn't aiming for it.
    {/*wire=*/81, "STACK_OVERFLOW", "Stack Overflow",
     "Lose a Stacker board on the second row.", "ICON_ACH_STACK_OVERFLOW",
     AchSeries::Event, /*goal=*/0, nullptr, 0, {bits(40)}, /*hidden=*/true},
    // The two ENDLESS cabinets, which are the only rows on the board that ask HOW FAR
    // rather than how often. Both goals sit well past the till's win line — passing that
    // is what the wins ladder already counts, so a high-score row that landed on the
    // same run would be paying twice for one thing. These are for the run after that.
    {/*wire=*/95, "DEEP_COVER", "Deep Cover",
     "Reach {n} passes in one CHROMATOPHORE cabinet run.", "ICON_ACH_DEEP_COVER",
     AchSeries::ArcadeCabinetBest, /*goal=*/20, "chroma", 0,
     {bits(150), item("sealed_cache_rare")}},
    {/*wire=*/96, "BUFFER_GLUTTON", "Buffer Glutton",
     "Eat {n} bytes in one ISOLATION PROTOCOL cabinet run.", "ICON_ACH_BUFFER_GLUTTON",
     AchSeries::ArcadeCabinetBest, /*goal=*/60, "isolation", 0,
     {bits(150), item("sealed_cache_rare")}},

    // --- The wallet ---------------------------------------------------------------
    {/*wire=*/6, "BIT_BARON", "Bit Baron",
     "Hold {n} Bits at once.", "ICON_ACH_BIT_BARON",
     AchSeries::BitsHeld, /*goal=*/10000, nullptr, 0,
     {bits(150), item("sealed_cache_rare")}},
    {/*wire=*/59, "BITS_50K", "Bit Tycoon",
     "Hold {n} Bits at once.", "ICON_ACH_BITS_50K",
     AchSeries::BitsHeld, /*goal=*/50000, nullptr, 0,
     {bits(400), item("commend_cache")}},

    // --- ROCK THE DOCK ------------------------------------------------------------
    // The arena in The Pirate Bayou (content_tournament.h), and the only place the pet
    // fights PETWARE rather than a malbeast. The tally counts BRACKETS, not matches:
    // the format is single elimination, so a run either ends in the title or ends, and
    // "took a bracket without dropping a match" would be this ladder's first rung
    // wearing a second name. The two rows under it ask the question the tally cannot —
    // what the bracket was taken WITH — and both are read at the purse, off state the
    // run already has (see ach::kDockUnderdog).
    //
    // Priced a rung above the equivalent boss row throughout: an entrant carries a real
    // kit, real mods, a real stat spread and an Exploit it fires on its own trigger, so
    // three of them in a row is not the same afternoon as three bosses.
    {/*wire=*/98, "DOCK_FIRST", "Took the Dock",
     "Take {n} ROCK THE DOCK bracket.", "ICON_ACH_DOCK_FIRST",
     AchSeries::TourneyWins, /*goal=*/1, nullptr, 0,
     {bits(150), item("sealed_cache_rare")}},
    {/*wire=*/99, "DOCK_5", "Dock Regular",
     "Take {n} ROCK THE DOCK brackets.", "ICON_ACH_DOCK_FIRST",
     AchSeries::TourneyWins, /*goal=*/5, nullptr, 0,
     {bits(250), item("sealed_cache_rare")}},
    {/*wire=*/100, "DOCK_25", "Harbourmaster",
     "Take {n} ROCK THE DOCK brackets.", "ICON_ACH_DOCK_FIRST",
     AchSeries::TourneyWins, /*goal=*/25, nullptr, 0,
     {bits(400), item("commend_cache")}},
    {/*wire=*/101, "DOCK_UNDERDOG", "Underdog",
     "Take the bracket against a higher-level operator in the final.",
     "ICON_ACH_DOCK_UNDERDOG",
     AchSeries::Event, /*goal=*/0, nullptr, 0, {bits(150), item("sealed_cache_rare")}},
    // A capstone, and priced as one: entrant stage follows entrant level, so the top of
    // a bracket is Daemon country by construction. Arriving there without being one is
    // the kit and the reads doing the work.
    {/*wire=*/102, "DOCK_PUNCHING_UP", "Punching Up",
     "Take the bracket with a pet that has not reached Daemon.",
     "ICON_ACH_DOCK_PUNCHING_UP",
     AchSeries::Event, /*goal=*/0, nullptr, 0, {bits(400), item("commend_cache")}},

    // --- The LINK duel ------------------------------------------------------------
    // FIRST_DUEL above counts the MEETING and fires when a duel starts; these count the
    // verdict. Two rows over one event on purpose: a duel that desyncs or is walked out
    // of was still a duel fought, and it still shouldn't count as one won. They share
    // the duel's icon because they are one subject, not four.
    {/*wire=*/103, "DUEL_WIN_1", "Ack",
     "Win {n} duel over the LINK.", "ICON_ACH_FIRST_DUEL",
     AchSeries::PvpWins, /*goal=*/1, nullptr, 0,
     {bits(60), item("sealed_cache_common")}},
    {/*wire=*/104, "DUEL_WIN_10", "Packet Loss",
     "Win {n} duels over the LINK.", "ICON_ACH_FIRST_DUEL",
     AchSeries::PvpWins, /*goal=*/10, nullptr, 0,
     {bits(150), item("sealed_cache_rare")}},
    {/*wire=*/105, "DUEL_WIN_50", "Uplink Dominant",
     "Win {n} duels over the LINK.", "ICON_ACH_FIRST_DUEL",
     AchSeries::PvpWins, /*goal=*/50, nullptr, 0,
     {bits(400), item("commend_cache")}},

    // --- The MERGE HUB's stove ----------------------------------------------------
    // The third axis on the kitchen, beside dishes HELD (CUISINE) and methods KNOWN
    // (RECIPES): dishes actually COOKED. Knowing a method is one Decryptogram cracked
    // and never happens again; cooking is what the player does with it afterwards, and
    // nothing was counting it.
    {/*wire=*/106, "SERVICE_1", "Mise en Place",
     "Cook {n} dish at the MERGE HUB.", "ICON_ACH_CUISINE_3",
     AchSeries::MergesCooked, /*goal=*/1, nullptr, 0, {bits(25)}},
    {/*wire=*/107, "SERVICE_25", "Dinner Service",
     "Cook {n} dishes at the MERGE HUB.", "ICON_ACH_CUISINE_3",
     AchSeries::MergesCooked, /*goal=*/25, nullptr, 0,
     {bits(60), item("sealed_cache_common")}},
    {/*wire=*/108, "SERVICE_100", "The Pass",
     "Cook {n} dishes at the MERGE HUB.", "ICON_ACH_CUISINE_3",
     AchSeries::MergesCooked, /*goal=*/100, nullptr, 0,
     {bits(250), item("sealed_cache_rare")}},

    // --- The DECRYPTOGRAM board ---------------------------------------------------
    // The VAULT's quote board, which until now was the one whole subsystem the
    // achievement table said nothing about — despite gating both a Rig Shop-free recipe
    // economy and the arcade's cryptogram cabinet. kGoalAll counts over the shipped
    // quote roster, so the capstone grows with the pool rather than mis-stating it.
    {/*wire=*/109, "QUOTES_1", "First Crack",
     "Solve {n} Decryptogram.", "ICON_ITEM_DECRYPTOGRAM",
     AchSeries::QuotesSolved, /*goal=*/1, nullptr, 0, {bits(25)}},
    {/*wire=*/110, "QUOTES_10", "Cryptanalyst",
     "Solve {n} Decryptograms.", "ICON_ITEM_DECRYPTOGRAM",
     AchSeries::QuotesSolved, /*goal=*/10, nullptr, 0,
     {bits(60), item("sealed_cache_common")}},
    {/*wire=*/111, "QUOTES_50", "Known Plaintext",
     "Solve {n} Decryptograms.", "ICON_ITEM_DECRYPTOGRAM",
     AchSeries::QuotesSolved, /*goal=*/50, nullptr, 0,
     {bits(250), item("sealed_cache_rare")}},
    {/*wire=*/112, "QUOTES_ALL", "Full Disclosure",
     "Solve all {n} Decryptograms.", "ICON_ITEM_DECRYPTOGRAM",
     AchSeries::QuotesSolved, kGoalAll, nullptr, 0,
     {bits(400), item("commend_cache")}},

    // --- Titles, crew & the other operators ---------------------------------------
    // A Title is a cleared area's receipt, so these share the AREA rows' art: same
    // achievement, read off the shelf it left behind rather than off the fight.
    {/*wire=*/113, "TITLE_FIRST", "Titled",
     "Earn {n} zone Title.", "ICON_ACH_AREA_FIRST",
     AchSeries::TitlesUnlocked, /*goal=*/1, nullptr, 0, {bits(25)}},
    {/*wire=*/114, "TITLES_ALL", "Fully Decorated",
     "Earn all {n} zone Titles.", "ICON_ACH_AREAS_ALL",
     AchSeries::TitlesUnlocked, kGoalAll, nullptr, 0,
     {bits(400), item("commend_cache")}},
    // Operators MET over the radio — the peer ledger, which is a different set from the
    // ones duelled: meeting is passive and costs neither side a consent they haven't
    // already given. Radio art, because that is what earns it.
    {/*wire=*/115, "PEERS_1", "Not Alone",
     "Meet {n} other operator over the radio.", "ICON_ACH_NETS_10",
     AchSeries::PeersMet, /*goal=*/1, nullptr, 0, {bits(25)}},
    {/*wire=*/116, "PEERS_10", "Local Traffic",
     "Meet {n} other operators over the radio.", "ICON_ACH_NETS_10",
     AchSeries::PeersMet, /*goal=*/10, nullptr, 0,
     {bits(60), item("sealed_cache_common")}},
    {/*wire=*/117, "PEERS_50", "Peer Reviewed",
     "Meet {n} other operators over the radio.", "ICON_ACH_NETS_50",
     AchSeries::PeersMet, /*goal=*/50, nullptr, 0,
     {bits(400), item("commend_cache")}},
    {/*wire=*/118, "CREW_ENLISTED", "Enlisted",
     "Take a side and enlist in a crew.", "ICON_ACH_CREW_ENLISTED",
     AchSeries::Event, /*goal=*/0, nullptr, 0, {bits(60), item("sealed_cache_common")}},

    // --- The bestiary's third axis: SEEN ------------------------------------------
    // Beside SpeciesRaised (what you brought up) and MalbeastsDefeated (what you beat),
    // this is what you have merely LAID EYES ON — moved by a duel, an arena entrant and
    // a branch-sibling reveal as well as by raising. The widest of the three by
    // construction: a device can meet far more of the roster than it will ever raise,
    // which is exactly why the capstone here is worth less than FULL_ROSTER below.
    {/*wire=*/119, "SEEN_12", "Field Notes",
     "Lay eyes on {n} different species.", "ICON_ACH_SPECIES_3",
     AchSeries::SpeciesSeen, /*goal=*/12, nullptr, 0,
     {bits(60), item("sealed_cache_common")}},
    {/*wire=*/120, "SEEN_ALL", "Full Sightings",
     "Lay eyes on all {n} species.", "ICON_ACH_SPECIES_12",
     AchSeries::SpeciesSeen, kGoalAll, nullptr, 0,
     {bits(250), item("sealed_cache_rare")}},
    // ...and the raised roster's own capstone, which SPECIES_12 above stopped short of.
    // Priced above SEEN_ALL: raising one is the whole lifecycle, seeing one is a glance.
    {/*wire=*/121, "SPECIES_ALL", "Full Roster",
     "Raise all {n} species.", "ICON_ACH_SPECIES_12",
     AchSeries::SpeciesRaised, kGoalAll, nullptr, 0,
     {bits(400), item("commend_cache")}},

    // --- The ARCH rack and the walk -----------------------------------------------
    // Pets held FROZEN at once. Fixed rungs rather than kGoalAll: the rack's ceiling is
    // a rig upgrade, so it is player state, and achievementSeriesTotal has to stay pure
    // (game_achievements.h). SECOND_INSTANCE's art, since that is the rack's other row.
    {/*wire=*/122, "RACK_3", "Cold Storage",
     "Hold {n} pets in the ARCH rack at once.", "ICON_ACH_SECOND_INSTANCE",
     AchSeries::RackHeld, /*goal=*/3, nullptr, 0,
     {bits(60), item("sealed_cache_common")}},
    {/*wire=*/123, "RACK_6", "Server Room",
     "Hold {n} pets in the ARCH rack at once.", "ICON_ACH_SECOND_INSTANCE",
     AchSeries::RackHeld, /*goal=*/6, nullptr, 0,
     {bits(250), item("sealed_cache_rare")}},
    // Distance walked, which every other explore row measures a RESULT of (a boss, a
    // sub-area, a network) and none of them measures directly.
    {/*wire=*/124, "STEPS_1K", "Legwork",
     "Take {n} steps on the 'net.", "ICON_ACH_SUBS_5",
     AchSeries::StepsWalked, /*goal=*/1000, nullptr, 0, {bits(25)}},
    {/*wire=*/125, "STEPS_10K", "Long Haul",
     "Take {n} steps on the 'net.", "ICON_ACH_SUBS_5",
     AchSeries::StepsWalked, /*goal=*/10000, nullptr, 0,
     {bits(60), item("sealed_cache_common")}},
    {/*wire=*/126, "STEPS_100K", "Marathon",
     "Take {n} steps on the 'net.", "ICON_ACH_SUBS_5",
     AchSeries::StepsWalked, /*goal=*/100000, nullptr, 0,
     {bits(250), item("sealed_cache_rare")}},

    // --- The arcade's remaining cabinets ------------------------------------------
    // ArcadeCabinetWins already existed and already had a tally being written for every
    // cabinet; only DEFRAG STACKER had claimed a row off it (TOWER_OF_FRAGGLE). These
    // are the other five, priced identically to it, each wearing its own cabinet's
    // glyph from content_arcade.cpp so the row and the cabinet read as one thing.
    {/*wire=*/127, "CAB_CLUTCH", "Phish Spotter",
     "Clear the SPOT THE PHISH cabinet in the GAMES arcade.", "ICON_ARCADE_CLUTCH",
     AchSeries::ArcadeCabinetWins, /*goal=*/1, "clutch", 0,
     {bits(60), item("sealed_cache_common")}},
    {/*wire=*/128, "CAB_ISOLATION", "Contained",
     "Clear the ISOLATION PROTOCOL cabinet in the GAMES arcade.", "ICON_LINE_WORM",
     AchSeries::ArcadeCabinetWins, /*goal=*/1, "isolation", 0,
     {bits(60), item("sealed_cache_common")}},
    {/*wire=*/129, "CAB_DECRYPTION", "Key Breaker",
     "Clear the DISK DECRYPTION cabinet in the GAMES arcade.", "ICON_LINE_RANSOMWARE",
     AchSeries::ArcadeCabinetWins, /*goal=*/1, "decryption", 0,
     {bits(60), item("sealed_cache_common")}},
    {/*wire=*/130, "CAB_CRYPTOGRAM", "Replay Value",
     "Clear the DECRYPTOGRAM cabinet in the GAMES arcade.", "ICON_ITEM_DECRYPTOGRAM",
     AchSeries::ArcadeCabinetWins, /*goal=*/1, "cryptogram", 0,
     {bits(60), item("sealed_cache_common")}},
    {/*wire=*/131, "CAB_CHROMA", "Local Colour",
     "Clear the CHROMATOPHORE cabinet in the GAMES arcade.", "ICON_ARCADE_CHROMA",
     AchSeries::ArcadeCabinetWins, /*goal=*/1, "chroma", 0,
     {bits(60), item("sealed_cache_common")}},

    // --- The two lines the older ladders were written before -----------------------
    // Worm and Metamorphic shipped after the per-line depth and full-line rows were
    // authored, so both families were climbing ladders that had no rung with their name
    // on it. Same goals and same prices as their three siblings above — these are the
    // missing rungs of an existing ladder, not a new one.
    {/*wire=*/132, "DEEP_LINE_WORM", "Worm Depths",
     "Reach depth {n} with a {key} pet.", "ICON_LINE_WORM",
     AchSeries::DeepWebDepthLine, /*goal=*/256, "worm", 0,
     {bits(250), item("sealed_cache_rare")}},
    {/*wire=*/133, "DEEP_LINE_METAMORPHIC", "Shifting Depths",
     "Reach depth {n} with a {key} pet.", "ICON_ACH_DEEP_LINE_METAMORPHIC",
     AchSeries::DeepWebDepthLine, /*goal=*/256, "metamorphic", 0,
     {bits(250), item("sealed_cache_rare")}},
    {/*wire=*/134, "FULL_LINE_METAMORPHIC", "Full 'Pedia: Meta",
     "Raise every one of the {n} creatures in the {key} line.",
     "ICON_ACH_FULL_LINE_METAMORPHIC",
     AchSeries::LineRaised, kGoalAll, "metamorphic", 0,
     {bits(400), item("commend_cache")}},

    // The DEFRAG ladder's capstone, which STACK_50 stopped one rung short of.
    {/*wire=*/135, "STACK_100", "Solid State",
     "Clear {n} disks by hand in the DEFRAG minigame.", "ICON_ACH_STACK_50",
     AchSeries::StackerWins, /*goal=*/100, nullptr, 0,
     {bits(400), item("commend_cache")}},
};

const int kAchievementCount =
    static_cast<int>(sizeof(kAchievements) / sizeof(kAchievements[0]));

const AchievementDef* achievementById(const char* id) {
    if (!id) return nullptr;
    for (int i = 0; i < kAchievementCount; ++i)
        if (std::strcmp(kAchievements[i].id, id) == 0) return &kAchievements[i];
    return nullptr;
}

const AchievementDef* achievementAt(int index) {
    return (index >= 0 && index < kAchievementCount) ? &kAchievements[index] : nullptr;
}

}  // namespace mal
