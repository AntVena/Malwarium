#pragma once

// game_rig_shop.h — the Rig Shop: the Hacker-face SHOP's account-upgrade rows
// (bandwidth pool, explore-frag reducers, ITEMS/VAULT gesture unlocks, rack slots,
// combat/cache Bits %, Merge Hub + its two recipe unlocks). Named "Rig" to stay
// unambiguous from the *explore-event* shops 
//
// Private, engine-internal, sibling to game_internal.h — inline const (C++17 inline
// variable, header-only), shared by game_hacker.cpp (buy dispatch + row rendering) and
// game_merge.cpp (the Merge Hub + recipe rows reuse the same table/engine). Every row is
// data: adding an upgrade is a new `kRigUpgrades[]` entry, never new buy/render logic.
//
// Magnitudes live on the row (src/core/content/CONTENT_STANDARD.md rule 2) — each constant below is
// used by exactly one row, so none of it lives in tunables.h. The one exception is
// kFragTriggerReducedPct (tunables.h), which the Reduce-Frag-Trigger row's readout
// shares with Game::applyBattleFatigue's combat calc — a genuinely dual-consumed table.
//
// A row's READOUT is data too, not a formatter: `RigReadout` says where the number
// comes from (a linear ramp off a base, or a tier table) and how to print it, and
// rigSpec() turns that into the same SpecRows every content row produces
// (content/effect_text.h). So the Rig Shop renders through the shared aligned grid
// (ui/widgets.h's drawSpecGrid) instead of hand-formatting "NOW -> NEXT" into a fixed
// char buffer that had to be eyeballed against the row's price for overlap.

#include <cstdio>
#include <cstdint>

#include "core/content/effect_text.h"   // SpecRows/SpecBuilder — the shared readout
#include "tunables.h"

namespace mal {

// A rig row's purchase state is a single int LEVEL (0 = never bought): a tiered row's
// purchase count, or 0/1 for a one-time unlock. `Game::rigLevel_` is sized to this.
inline constexpr int kMaxRigUpgrades = 24;   // headroom for future rows
// Bandwidth has no purchase cap (unlike every other row) — a maxLevel this large is
// never actually reached, so the generic "MAXED" check is permanently false for it.
inline constexpr int kRigLevelUnlimited = 1 << 20;

// Rig row indices — the SHOP list's cursor space and Game::rigLevel_'s index space.
// Values are stable (persisted positionally via the legacy save fields in
// game_persist.cpp) — never reorder existing rows; append new ones after
// kRigRowRecipeNachos.
enum RigRow {
    kRigRowBandwidth = 0,
    kRigRowFragReduce = 1,
    kRigRowFragTrigger = 2,
    kRigRowItemTabs = 3,
    kRigRowBulkOpen = 4,
    kRigRowRackSlot = 5,
    kRigRowScraping = 6,
    kRigRowDataMining = 7,
    kRigRowMergeHub = 8,
    kRigRowRecipeNoodles = 9,
    kRigRowRecipeNachos = 10,
    kRigRowAutoBackup = 11,
    kRigRowContinuousBackup = 12,
    kRigRowDiskMaintenance = 13,
    kRigRowHungerXpRate = 14,
    kRigRowHungerXpWindow = 15,
    kRigRowCombatXpPct = 16,
    kRigRowCombatXpWindow = 17,
    kRigRowItemPicker = 18,
    kRigRowRecipeHashedBrowns = 19,
    kRigRowRecipeSaltedBrowns = 20,
    kRigRowModStorage = 21,
};

// --- a: Increase Bandwidth --------------------------------------------------
// Each purchase adds kBandwidthUpgradeStep to the farming-pool cap (kBandwidthMax,
// tunables.h) AND immediately grants the same step to the LIVE pool (RigEffectKind::
// GrantBandwidth) so the freshly-bought capacity is usable right away. Price climbs
// linearly: cost(n) = base + step*n. No purchase cap.
inline constexpr int kBandwidthUpgradeStep = 1;
inline constexpr int kBandwidthUpgradeBaseCost = 128;
inline constexpr int kBandwidthUpgradeCostStep = 64;

// --- b: Reduce Explore Frag --------------------------------------------------
// 3 tiers, each shaving 1 off the battle-fatigue frag AMOUNT cap (kBattleFatigueFragMax
// - level, read at Game::applyBattleFatigue). Descending price ladder (rewards the
// first tiers most): cost(n) = start >> n.
inline constexpr int kFragReducerStart = 2048;
inline constexpr int kFragReducerMaxTier = 3;

// --- c: Reduce Frag Trigger %% ------------------------------------------------
// 6 tiers, each lowering the battle-fatigue TRIGGER chance via kFragTriggerReducedPct
// (tunables.h; tier 0 = the unchanged kBattleFatigueChancePct baseline). Doubling
// price ladder: cost(n) = start * 2^n. Sibling to b (which cuts the AMOUNT).
inline constexpr int kFragTriggerStart = 256;
inline constexpr int kFragTriggerMaxTier = 6;

// --- d/e: one-time gesture unlocks -------------------------------------------
// No tiers, no computed text — Items Type-Tabs arms the ITEMS hold-A FOOD/BUFFS/QUEST
// filter cycle (kItemFilterHoldMs, tunables.h); Vault Bulk-Open arms the VAULT hold-B
// "open every cache of this rarity" gesture (kBulkOpenHoldMs, tunables.h).
inline constexpr int kShopItemTabsCost = 1024;
inline constexpr int kShopBulkOpenCost = 2048;

// --- ITEMS Type-Picker --------------------------------------------------------
// A one-time unlock that puts a category tile screen (ALL/FOOD/BUFFS/KEYS/TOOLS,
// items_screen.h's ItemPickRow) in FRONT of the ITEMS list: entering ITEMS lands on
// the picker, B drills into that category, C walks back to it. Independent of Items
// Type-Tabs — either row is useful alone — but owning both upgrades the hold-A
// gesture to the picker's finer CATEGORY axis (Game::nextItemFilter's categoryAxis),
// so the two never disagree about what a tab means. The Lockout Open-Items path
// never routes through the picker: a crisis doesn't get an extra screen.
inline constexpr int kShopItemPickerCost = 2048;

// --- Containment Rack Slot -----------------------------------------------------
// +1 permanent ARCH rack slot per purchase (rackSlots() = kRackSlots + level). Cost
// doubles every purchase — a real Bits sink.
inline constexpr int kRackSlotUpgradeStart = 512;
inline constexpr int kRackSlotUpgradeMax = 16;

// --- Scraping Cluster Expansion --------------------------------------------------
// +1% Bits earned from COMBAT per level (applied at every combat Bits payout via
// Game::applyCombatBitsBonus). Cost steps up every power-of-2 purchases.
inline constexpr int kScrapingClusterStart = 128;
inline constexpr int kScrapingClusterMax = 128;
inline constexpr int kScrapingClusterPctPerLevel = 1;

// --- Enhanced DataMining -----------------------------------------------------------
// +kDataMiningPctPerLevel% Bits earned from CACHES per level (Game::applyCacheBitsBonus). 
// Same log-stepped shape as Scraping Cluster, offset a tier down.
inline constexpr int kDataMiningStart = 128;
inline constexpr int kDataMiningMax = 256;
inline constexpr int kDataMiningPctPerLevel = 4;

// --- f: Merge Hub + its two recipe unlocks ------------------------------------
// Merge Hub is a one-time unlock that makes the MRG carousel slot accessible
// (Game::mergeHubUnlocked). Each recipe is a SEPARATE one-time unlock gating whether
// game_merge.cpp's craftRecipe can run — owning a recipe is necessary but not
// sufficient, the raw ingredients still have to be in the bag. The recipe rows' input/
// output ids/quantities live in game_internal.h's MergeRecipe table (the craft
// mechanic); these rows only own "is it unlocked + what does it cost".
inline constexpr int kRigMergeHubCost = 4096;
inline constexpr int kRigRecipeUnlockCost = 512;

// --- The two Browns recipes ---------------------------------------------------
// Same shape as the two recipe rows above, with one addition: they are also gated on
// having COLLECTED both dishes (requiresItems). Moor-to-Moor stocks both, so the
// intended route is buy one, eat it, decide you'd rather cook them — the Rig Shop
// won't sell you the method for a dish you've never met. `itemCollected` is an
// ever-held record, not current possession, so eating the evidence doesn't revoke it.
inline constexpr int kRigBrownsRecipeCost = 512;

// --- g/h: Auto Backup / Continuous Auto-Backup --------------------------------
// One-time unlocks that spare the player a manual Backup Drive use — they arm the same
// death-save the item does (Game::autoArmBackupShield, free: the effect is applied
// without spending a drive from the Vault). Auto Backup arms it the moment explore-mode
// starts (Game::startExplore/startDeepWebDive) if it isn't already armed; Continuous
// Auto-Backup does the same check at every resolved explore event (Game::
// returnToExplore) — so it re-arms mid-run whenever a fight spends the save or its hour
// lapses. Both are no-ops while the save is already live — never doubles up.
//
// These sit high on the price ladder on purpose: what they automate is no longer a
// negated hit but a life, so Continuous in particular is close to "the pet does not die
// while exploring" and has to read as an endgame purchase.
inline constexpr int kRigAutoBackupCost = 4096;
inline constexpr int kRigContinuousBackupCost = 8192;

// --- i: Disk Maintenance -----------------------------------------------------
// 4 tiers. Each owned tier silently runs a guaranteed-clean defrag between explore
// events (Game::maybeAutoDefrag, called from Game::returnToExplore) once
// Fragmentation reaches that tier's threshold. Buying deeper into the ladder both
// LOWERS the trigger threshold (catches Fragmentation earlier) and RAISES the
// per-run Bits cost (a multiple of the normal defragCost()) — tighter upkeep costs
// more, which is the point: it buys more safety margin for an older, stronger (and
// so more heavily-explored) pet. Doubling purchase-price ladder, sibling to Rack
// Slot's shape. (Future: defragCost() itself may later scale with defragCount() —
// out of scope here, but this table doesn't hardcode around that.)
inline constexpr int kDiskMaintenanceMaxTier = 4;
inline constexpr int kDiskMaintenanceBuyStart = 4096;
inline constexpr int kDiskMaintenanceThreshold[kDiskMaintenanceMaxTier] = {80, 60, 40, 20};
inline constexpr int kDiskMaintenanceCostMult[kDiskMaintenanceMaxTier] = {2, 4, 8, 16};

// --- j: Passive XP Farming ------------------------------------------------------
// +1 combat XP per hunger point the pet loses to passive decay per level, but only
// counting points lost while hunger stayed ABOVE the row k window threshold
// (Game::hungerXpThreshold) — read at the model tick that decays Hunger
// (Game::tick, game_core.cpp). A well-fed pet quietly levels; letting hunger run
// low earns nothing past the threshold. Log-stepped price, sibling shape to
// Scraping Cluster.
inline constexpr int kHungerXpRateStart = 256;
inline constexpr int kHungerXpRateMax = 20;

// --- k: XP Farming Window ---------------------------------------------------------
// Widens row j's qualifying hunger range: each level lowers the threshold Hunger
// must stay above by kHungerXpWindowStepPct, from kHungerXpBaseThreshold down
// (Game::hungerXpThreshold). Doubling price, sibling shape to Rack Slot.
inline constexpr int kHungerXpWindowStart = 1024;
inline constexpr int kHungerXpWindowMaxTier = 8;
inline constexpr int kHungerXpBaseThreshold = 90;
inline constexpr int kHungerXpWindowStepPct = 5;

// --- l: Well-Fed XP Boost -----------------------------------------------------
// +kCombatXpPctPerLevel%% combat XP per level (Game::applyCombatXpBonus), active only
// while current Hunger is at or above row m's window threshold (Game::
// combatXpWindowThreshold) — a fed-and-happy pet fights sharper. Log-stepped
// price, sibling shape to Enhanced DataMining.
inline constexpr int kCombatXpPctStart = 256;
inline constexpr int kCombatXpPctMax = 20;
inline constexpr int kCombatXpPctPerLevel = 1;

// --- m: XP Boost Window -----------------------------------------------------------
// Widens row l's qualifying hunger range, same shape as row k (independent
// threshold — buying one window doesn't move the other).
inline constexpr int kCombatXpWindowStart = 1024;
inline constexpr int kCombatXpWindowMaxTier = 8;
inline constexpr int kCombatXpBaseThreshold = 90;
inline constexpr int kCombatXpWindowStepPct = 5;

struct RigUpgradeDef;  // fwd decl for the table below

enum class RigEffectKind : uint8_t {
    None = 0,          // nothing mutated at buy time — other code reads the level directly
    GrantBandwidth,     // magnitude added to the LIVE bandwidth_ pool, capped at the new max
};

// Where a readout's number comes from at a given purchase level.
enum class RigValueCurve : uint8_t {
    Flag = 0,   // no number at all — the label IS the statement ("HOLD A: FILTER")
    Ramp,       // base + level*step (step may be negative: a reducer counts down)
    Tiers,      // level 0 => base, else table[level-1] — for a hand-picked ladder
};

// One line of a rig row's readout. `label` names it, `valueFmt` prints one number
// (carrying its own sign/unit — "+%d%%", "%d+", "%d"), and rigSpec renders the pair
// the player is deciding between as "NOW -> NEXT", collapsing to just NOW once the row
// is maxed. `zeroText` swaps a word in for a 0 value where 0 means "off" rather than
// "zero of something"; a 0 value with no zeroText drops the line entirely, which is how
// a tier row hides its upkeep cost before the first tier is bought.
struct RigReadout {
    const char* label = nullptr;          // nullptr = unused slot
    RigValueCurve curve = RigValueCurve::Flag;
    int base = 0;
    int step = 0;                          // Ramp only
    const int* tiers = nullptr;            // Tiers only
    int tierCount = 0;
    const char* valueFmt = "%d";
    const char* zeroText = nullptr;
};

// Up to two readouts per row — the second is for a row whose purchase moves a
// side-effect too (Disk Maintenance buys a lower trigger AND a higher upkeep), the
// same shape as ModDef's magnitude/magnitude2 pair.
inline constexpr int kMaxRigReadouts = 2;

// Max items a row's "you must have met these first" gate can name
// (RigUpgradeDef::requiresItems).
inline constexpr int kMaxRigRequiredItems = 2;

struct RigUpgradeDef {
    const char* id;
    const char* displayName;      // SHOP row name
    int maxLevel;                  // 1 = one-time unlock; N = tiered cap; kRigLevelUnlimited = uncapped
    RigCostCurve curve;
    int costStart;
    int costStep;                  // only kLinear reads this
    RigEffectKind effectKind;
    int effectMagnitude;
    const char* logText;            // Hacker-Log line on purchase
    RigReadout readouts[kMaxRigReadouts];
    // Another RigRow that must be OWNED before this one is offered at all — the
    // row is absent from the SHOP, not greyed, so the list never advertises a
    // purchase whose home doesn't exist yet (a recipe with no MERGE HUB to cook
    // in). -1 (the default every unconditional row carries) = always offered.
    // Enforced in one place, shopRowOffered (game_hacker.cpp), which the A-cycle,
    // the render loop and buyRigUpgrade all share.
    int requiresRow = -1;
    // Item ids that must have been COLLECTED (ever held — Game::itemCollected, not
    // current possession) before this row is offered. The second gate axis alongside
    // requiresRow: that one asks what you've BOUGHT, this one asks what you've MET.
    // A nullptr slot ends the list; an all-null array (what every other row carries)
    // = no item gate. Enforced in the same one place, shopRowOffered.
    const char* requiresItems[kMaxRigRequiredItems] = {};
};

// This readout's value at purchase level `lvl`.
inline int rigValueAt(const RigReadout& r, int lvl) {
    switch (r.curve) {
        case RigValueCurve::Flag: return 0;
        case RigValueCurve::Ramp: return r.base + lvl * r.step;
        case RigValueCurve::Tiers:
            if (lvl <= 0) return r.base;
            return r.tiers[(lvl < r.tierCount ? lvl : r.tierCount) - 1];
    }
    return 0;
}

// A rig row's readout as the same SpecRows every content row produces, so the SHOP
// list and any panel showing one render through the shared grid (drawSpecGrid) rather
// than a bespoke formatter. `level` is the row's current purchase count
// (Game::rigLevel_).
inline SpecRows rigSpec(const RigUpgradeDef& d, int level) {
    SpecBuilder s;
    const bool maxed = level >= d.maxLevel;
    for (const RigReadout& r : d.readouts) {
        if (!r.label) continue;
        if (r.curve == RigValueCurve::Flag) {
            s.flag(r.label);
            continue;
        }
        const int now = rigValueAt(r, level);
        if (now == 0 && !r.zeroText) continue;      // nothing owned on this axis yet
        char nowText[12];
        if (now == 0) std::snprintf(nowText, sizeof(nowText), "%s", r.zeroText);
        else          std::snprintf(nowText, sizeof(nowText), r.valueFmt, now);
        if (maxed) {
            s.add(r.label, "%s", nowText);
            continue;
        }
        char nextText[12];
        std::snprintf(nextText, sizeof(nextText), r.valueFmt, rigValueAt(r, level + 1));
        s.add(r.label, "%s -> %s", nowText, nextText);
    }
    return s.out;
}

inline const RigUpgradeDef kRigUpgrades[] = {
    {"bandwidth", "INCREASE BANDWIDTH", kRigLevelUnlimited, RigCostCurve::kLinear,
     kBandwidthUpgradeBaseCost, kBandwidthUpgradeCostStep, RigEffectKind::GrantBandwidth,
     kBandwidthUpgradeStep, "BOUGHT +BANDWIDTH",
     {{"POOL", RigValueCurve::Ramp, kBandwidthMax, kBandwidthUpgradeStep}}},

    {"frag_reduce", "REDUCE EXPLORE FRAG", kFragReducerMaxTier, RigCostCurve::kHalving,
     kFragReducerStart, 0, RigEffectKind::None, 0, "BOUGHT -EXPL FRAG",
     {{"FRAG", RigValueCurve::Ramp, kBattleFatigueFragMax, -1}}},

    {"frag_trigger", "REDUCE FRAG TRIGGER", kFragTriggerMaxTier, RigCostCurve::kDoubling,
     kFragTriggerStart, 0, RigEffectKind::None, 0, "BOUGHT -FRAG TRIG",
     {{"FRAG CHANCE", RigValueCurve::Tiers, kBattleFatigueChancePct, 0,
       kFragTriggerReducedPct, kFragTriggerMaxTier, "%d%%"}}},

    {"item_tabs", "ITEMS TYPE-TABS", 1, RigCostCurve::kFixed, kShopItemTabsCost, 0,
     RigEffectKind::None, 0, "BOUGHT ITEM TABS", {{"HOLD A: FILTER"}}},

    {"bulk_open", "VAULT BULK-OPEN", 1, RigCostCurve::kFixed, kShopBulkOpenCost, 0,
     RigEffectKind::None, 0, "BOUGHT BULK OPEN", {{"HOLD B: OPEN ALL"}}},

    {"rack_slot", "CONTAINMENT RACK SLOT", kRackSlotUpgradeMax, RigCostCurve::kDoubling,
     kRackSlotUpgradeStart, 0, RigEffectKind::None, 0, "BOUGHT +RACK SLOT",
     {{"SLOTS", RigValueCurve::Ramp, kRackSlots, 1}}},

    {"scraping_cluster", "SCRAPING CLUSTER EXP.", kScrapingClusterMax, RigCostCurve::kLogStep,
     kScrapingClusterStart, 0, RigEffectKind::None, 0, "BOUGHT SCRAPING LVL",
     {{"COMBAT BITS", RigValueCurve::Ramp, 0, kScrapingClusterPctPerLevel, nullptr, 0,
       "+%d%%", "+0%"}}},

    {"data_mining", "ENHANCED DATAMINING", kDataMiningMax, RigCostCurve::kLogStepHalf,
     kDataMiningStart, 0, RigEffectKind::None, 0, "BOUGHT DATAMINE LVL",
     {{"CACHES", RigValueCurve::Ramp, 0, kDataMiningPctPerLevel, nullptr, 0,
       "+%d%%", "+0%"}}},

    {"merge_hub", "MERGE HUB", 1, RigCostCurve::kFixed, kRigMergeHubCost, 0,
     RigEffectKind::None, 0, "BOUGHT MERGE HUB", {{"COMBINE ITEMS"}}},

    // Both recipes wait on the hub itself: a recipe is only cookable in the MERGE
    // HUB, so offering one first sells a key to a room the player can't enter.
    {"recipe_noodles", "RECIPE: PATCHED NOODLES", 1, RigCostCurve::kFixed,
     kRigRecipeUnlockCost, 0, RigEffectKind::None, 0, "BOUGHT NOODLES RECIPE",
     {{"NOODLES + SAUCE"}}, kRigRowMergeHub},

    {"recipe_nachos", "RECIPE: STACKED NACHOS", 1, RigCostCurve::kFixed,
     kRigRecipeUnlockCost, 0, RigEffectKind::None, 0, "BOUGHT NACHOS RECIPE",
     {{"CHIP + DIP"}}, kRigRowMergeHub},

    {"auto_backup", "AUTO BACKUP", 1, RigCostCurve::kFixed, kRigAutoBackupCost, 0,
     RigEffectKind::None, 0, "BOUGHT AUTO BACKUP", {{"DEATH SAVE ON EXPLORE"}}},

    {"continuous_backup", "CONTINUOUS AUTO-BACKUP", 1, RigCostCurve::kFixed,
     kRigContinuousBackupCost, 0, RigEffectKind::None, 0, "BOUGHT CONT. BACKUP",
     {{"RE-ARM SAVE MID-RUN"}}},

    // Two readouts: buying a tier both lowers the Fragmentation it steps in at and
    // raises the per-run upkeep. Before tier 1 the trigger reads OFF and the upkeep
    // line is absent entirely (a 0 with no zeroText drops out of rigSpec).
    {"disk_maintenance", "DISK MAINTENANCE", kDiskMaintenanceMaxTier, RigCostCurve::kDoubling,
     kDiskMaintenanceBuyStart, 0, RigEffectKind::None, 0, "BOUGHT DISK MAINT LVL",
     {{"FRAG AT", RigValueCurve::Tiers, 0, 0, kDiskMaintenanceThreshold,
       kDiskMaintenanceMaxTier, "%d+", "OFF"},
      {"UPKEEP", RigValueCurve::Tiers, 0, 0, kDiskMaintenanceCostMult,
       kDiskMaintenanceMaxTier, "x%d"}}},

    {"hunger_xp_rate", "PASSIVE XP FARMING", kHungerXpRateMax, RigCostCurve::kLogStep,
     kHungerXpRateStart, 0, RigEffectKind::None, 0, "BOUGHT XP FARMING LVL",
     {{"XP/PT", RigValueCurve::Ramp, 0, 1, nullptr, 0, "%d", "0"}}},

    {"hunger_xp_window", "XP FARMING WINDOW", kHungerXpWindowMaxTier, RigCostCurve::kDoubling,
     kHungerXpWindowStart, 0, RigEffectKind::None, 0, "BOUGHT XP FARM WINDOW",
     {{"ABOVE", RigValueCurve::Ramp, kHungerXpBaseThreshold, -kHungerXpWindowStepPct,
       nullptr, 0, "%d%%"}}},

    {"combat_xp_pct", "WELL-FED XP BOOST", kCombatXpPctMax, RigCostCurve::kLogStep,
     kCombatXpPctStart, 0, RigEffectKind::None, 0, "BOUGHT XP BOOST LVL",
     {{"COMBAT XP", RigValueCurve::Ramp, 0, kCombatXpPctPerLevel, nullptr, 0,
       "+%d%%", "+0%"}}},

    {"combat_xp_window", "XP BOOST WINDOW", kCombatXpWindowMaxTier, RigCostCurve::kDoubling,
     kCombatXpWindowStart, 0, RigEffectKind::None, 0, "BOUGHT XP BOOST WINDOW",
     {{"ABOVE", RigValueCurve::Ramp, kCombatXpBaseThreshold, -kCombatXpWindowStepPct,
       nullptr, 0, "%d%%"}}},

    {"item_picker", "ITEMS TYPE-PICKER", 1, RigCostCurve::kFixed, kShopItemPickerCost, 0,
     RigEffectKind::None, 0, "BOUGHT TYPE PICKER", {{"B: PICK A TYPE"}}},

    // Both Browns recipes want the same two things first: somewhere to cook (the hub)
    // and first-hand knowledge of BOTH dishes. Moor-to-Moor sells them, so the gate is
    // reachable without ever owning a recipe — which is the point of gating on the
    // dishes rather than on each other.
    {"recipe_hashed_browns", "RECIPE: HASHED BROWNS", 1, RigCostCurve::kFixed,
     kRigBrownsRecipeCost, 0, RigEffectKind::None, 0, "BOUGHT BROWNS RECIPE",
     {{"4-INGREDIENT COOK"}}, kRigRowMergeHub,
     {"hashed_browns", "salted_hashed_browns"}},

    {"recipe_salted_browns", "RECIPE: SALTED&HASHED", 1, RigCostCurve::kFixed,
     kRigBrownsRecipeCost, 0, RigEffectKind::None, 0, "BOUGHT SALTED RECIPE",
     {{"SALT + BROWNS"}}, kRigRowMergeHub,
     {"hashed_browns", "salted_hashed_browns"}},

    // --- Mod storage: how many spares of ONE mod the pool will hold ---------------
    // The counterpart to kModCopyCapBase (tunables): the base cap is deliberately tight
    // enough that a player who wants a bench of a favourite mod has something to spend
    // Bits on, and the tiers stop well short of the save's own 15-per-mod ceiling.
    {"mod_storage", "MOD STORAGE", kModStorageMaxTier, RigCostCurve::kDoubling,
     kModStorageStart, 0, RigEffectKind::None, 0, "BOUGHT +MOD STORAGE",
     {{"PER MOD", RigValueCurve::Tiers, kModCopyCapBase, 0, kModStorageCapByTier,
       kModStorageMaxTier, "x%d"}}},
};
inline constexpr int kRigUpgradeCount =
    static_cast<int>(sizeof(kRigUpgrades) / sizeof(kRigUpgrades[0]));

}  // namespace mal
