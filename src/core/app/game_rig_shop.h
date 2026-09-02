#pragma once

// game_rig_shop.h — the Rig Shop: the Hacker-face SHOP's account-upgrade rows
// (bandwidth pool, explore-frag reducers, ITEMS/VAULT gesture unlocks, rack slots,
// combat/cache Bits %, the Merge Hub). Named "Rig" to stay unambiguous from the
// *explore-event* shops
//
// Private, engine-internal, sibling to game_internal.h — inline const (C++17 inline
// variable, header-only), shared by game_hacker.cpp (buy dispatch + row rendering) and
// game_merge.cpp (which reads the Merge Hub row). Every row is data: adding an upgrade
// is a new `kRigUpgrades[]` entry, never new buy/render logic.
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
#include <cstring>

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
// Values are stable (persisted positionally: rows below kRigRowExtBase have their own
// named save fields, the rest ride the rigLevelsExt tail by position) — never reorder
// existing rows, and append new ones at the END.
//
// Everything here is BOUGHT. The one thing the Hacker face hands out that isn't — a
// MERGE HUB recipe — is not a row at all: recipes are their own content table with
// their own save bits (core/content/content_recipes.h), because a recipe has no price,
// no levels and no place in a storefront list. The hub itself is the last rig row
// cooking needs.
//
// Capacity for the owned-recipe bitset, in MergeRecipe::wire numbers — one bit each,
// carried length-prefixed as SaveData::recipeOwned so the table can outgrow any fixed
// width. It sits HERE rather than beside the recipes so that game.h can size
// Game::recipesOwned_ without pulling a content table into the public engine header.
// Raise it in multiples of 8 (a whole byte) with a save-version note; the native gate
// asserts every wire is under it.
inline constexpr int kMergeRecipeWireCap = 128;
inline constexpr int kMergeRecipeWireBytes = kMergeRecipeWireCap / 8;

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
    kRigRowAutoBackup = 9,
    kRigRowContinuousBackup = 10,
    kRigRowDiskMaintenance = 11,
    kRigRowHungerXpRate = 12,
    kRigRowHungerXpWindow = 13,
    kRigRowCombatXpPct = 14,
    kRigRowCombatXpWindow = 15,
    kRigRowItemPicker = 16,
    kRigRowModStorage = 17,
};

// The first row that persists positionally in SaveData::rigLevelsExt rather than
// through a named field of its own (save.h v32). Every row below it predates that tail
// and keeps its own field; every row from here up is `rigLevelsExt[row - this]`.
inline constexpr int kRigRowExtBase = kRigRowAutoBackup;

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
// No tiers, no computed text — Items Type-Tabs arms the ITEMS hold-B FOOD/BUFFS/QUEST
// filter cycle (kItemFilterHoldMs, tunables.h); Vault Bulk-Open arms the VAULT hold-B
// "open every cache of this rarity" gesture (kBulkOpenHoldMs, tunables.h).
inline constexpr int kShopItemTabsCost = 1024;
inline constexpr int kShopBulkOpenCost = 2048;

// --- ITEMS Type-Picker --------------------------------------------------------
// A one-time unlock that puts a category tile screen (ALL/FOOD/BUFFS/KEYS/TOOLS,
// items_screen.h's ItemPickRow) in FRONT of the ITEMS list: entering ITEMS lands on
// the picker, B drills into that category, C walks back to it. Independent of Items
// Type-Tabs — either row is useful alone — but owning both upgrades the hold-B
// gesture to the picker's finer CATEGORY axis (Game::nextItemFilter's categoryAxis),
// so the two never disagree about what a tab means. The Lockout Open-Items path
// never routes through the picker: a crisis doesn't get an extra screen.
inline constexpr int kShopItemPickerCost = 2048;

// --- Containment Rack Slot -----------------------------------------------------
// +1 permanent ARCH rack slot per purchase (rackSlots() = kRackSlots + level), to a
// ceiling of kRackSlots + this — 64 slots, which is what the ARCH is SIZED for: the
// roster is 35 species, so a full shelf holds one of every one of them with room over
// for duplicates, both halves of a care branch, and whatever families ship later.
//
// Log-stepped rather than doubling, sibling to Enhanced DataMining below. The doubling
// ladder this replaces priced the sixteenth slot at 16.7 MILLION Bits and the whole
// ladder at 33.5M, which is not a Bits sink but a wall: past the first handful the rows
// were unbuyable, so the capacity above them was decoration. The log-step prices a
// 32-slot shelf at ~75K cumulative — a real mid-to-late goal beside the 50K Bits
// achievement — and the full 64 at ~325K, which is a long project rather than a joke.
inline constexpr int kRackSlotUpgradeStart = 512;
inline constexpr int kRackSlotUpgradeMax = 60;

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

// --- f: the Merge Hub ---------------------------------------------------------
// A one-time unlock that makes the MRG carousel slot accessible
// (Game::mergeHubUnlocked). The hub is ALL of cooking that Bits can reach: the recipes
// cooked in it are won off a Decryptogram, one per first solve, and are not rig rows at
// all (game_internal.h's kMergeRecipes, content_quotes.h's prize ladder). So this row
// buys the kitchen and nothing that happens in it.
inline constexpr int kRigMergeHubCost = 4096;

// --- g/h: Auto Backup / Continuous Auto-Backup --------------------------------
// One-time unlocks that spare the player a manual Backup Drive use — they arm the same
// death-save the item does (Game::autoArmBackupShield, free: the effect is applied
// without spending a drive from the Vault). Auto Backup arms it the moment explore-mode
// starts (Game::startExplore/startDeepWebDive) if it isn't already armed; Continuous
// Auto-Backup does the same check at every resolved explore event (Game::
// returnToExplore) — so it re-arms mid-run whenever a fight spends the save or its hour
// lapses. Both are no-ops while the save is already live — never doubles up.
//
// These sit high on the price ladder on purpose: what they automate is not a negated
// hit but a life, so Continuous in particular is close to "the pet does not die
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
// so more heavily-explored) pet. Doubling purchase-price ladder — the shape Rack Slot
// used to share, before its ceiling rose past what a doubling ladder can price. (Future: defragCost() itself may later scale with defragCount() —
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
// (Game::hungerXpThreshold). Doubling price, sibling shape to Disk Maintenance.
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
    Flag = 0,   // no number at all — the label IS the statement ("HOLD B: FILTER")
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
     RigEffectKind::None, 0, "BOUGHT ITEM TABS", {{"HOLD B: FILTER"}}},

    {"bulk_open", "VAULT BULK-OPEN", 1, RigCostCurve::kFixed, kShopBulkOpenCost, 0,
     RigEffectKind::None, 0, "BOUGHT BULK OPEN", {{"HOLD B: OPEN ALL"}}},

    {"rack_slot", "CONTAINMENT RACK SLOT", kRackSlotUpgradeMax, RigCostCurve::kLogStepHalf,
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

// The RigRow whose row carries `id`, or -1. For anything that names a rig row as DATA
// rather than as a cursor position — a quote's prize (content_quotes.h) is the first —
// so a reward table spells "bandwidth" and not the integer 0.
inline int rigRowIndexById(const char* id) {
    if (!id) return -1;
    for (int i = 0; i < kRigUpgradeCount; ++i)
        if (std::strcmp(kRigUpgrades[i].id, id) == 0) return i;
    return -1;
}

}  // namespace mal
