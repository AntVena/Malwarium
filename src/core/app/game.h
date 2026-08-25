// game.h — top-level engine state machine + render entry point.
//
// Navigation is a shallow stack: the carousel is
// always-on furniture framing the living area. A/C summon the *cursor* onto a
// slot; B enters its submenu (L2). The list/detail layers sit beneath the
// cursor (ITEMS, MAINT) plus the event-overlay modals (Feeding, Lockout) that
// preempt the whole tree. The engine renders the full active (224) canvas — UI
// chrome native, creature upscaled.
#pragma once

#include <cstdint>
#include <vector>

#include "tunables.h"
#include "core/app/game_achievements.h"
#include "core/app/game_rig_shop.h"
#include "core/app/net_status.h"
#include "core/app/update_status.h"
#include "core/app/power_status.h"
#include "core/app/radio_status.h"
#include "core/app/sd_status.h"
#include "core/content/areas/area_defs.h"
#include "core/content/content_arcade.h"    // kArcadeMaxCabinets sizes the arcade tallies
#include "core/content/content_passives.h"  // kWormReplicaSlots sizes companionWander_
#include "core/content/content_quotes.h"    // kQuoteStateBytes sizes the per-quote states
#include "core/content/defs.h"
#include "core/content/effect_text.h"
#include "core/content/registry.h"
#include "core/model/combat.h"
#include "core/model/cryptogram.h"
#include "core/model/disk_decryption.h"
#include "core/model/event_log.h"
#include "core/model/hacker_rank.h"
#include "core/model/idle_wander.h"
#include "core/model/inventory.h"
#include "core/model/loadout.h"
#include "core/model/move_loadout.h"
#include "core/model/pet_model.h"
#include "core/model/save.h"
#include "core/model/chromatophore.h"
#include "core/model/isolation.h"
#include "core/model/stacker.h"
#include "core/model/tournament.h"
#include "core/net/audit_capture.h"
#include "core/net/network_ledger.h"
#include "core/net/peer_ledger.h"
#include "core/net/pvp_link.h"
#include "core/render/camo.h"          // CamoTarget — the FX_CAMO state the tick keeps
#include "core/render/framebuffer.h"
// ui_state.h ONLY, never a core/ui/*_screen.h: Game holds the ids below as members
// but draws nothing itself, and this header is included by every TU in the engine.
// A screen header belongs in the game_*.cpp unit that calls its draw functions.
#include "core/ui/ui_state.h"
#include "platform/platform.h"

namespace mal {

struct SpriteData;
// STAT's two flowed prose pages (core/ui/stat_screen.h). Named here only as the
// return type of the row builders below — the screen header itself stays in
// game_render.cpp, per the include note above.
struct ProseRow;
struct BuffRow;

// How a Game starts. FreshHatch = empty save -> Decryption Hatch (the real
// first boot). Hatched = skip straight to a raised pet (the test/dev
// seam that keeps the tests green without going through the egg).
enum class StartMode { FreshHatch, Hatched };

class Game {
public:
    // `hatchedCreature` is the pet installed when mode == Hatched (the dev/test
    // seam); ignored for FreshHatch, which installs the egg-line creature.
    // `store` (optional) persists the save across boots: when it holds a non-empty
    // save the game boots straight onto the raised pet (ignoring `mode`); an empty
    // store falls back to `mode`. nullptr = no persistence (the bare test/dev seam).
    explicit Game(StartMode mode = StartMode::FreshHatch,
                  const char* hatchedCreature = "paypup",
                  ISaveStore* store = nullptr);

    // Navigation layers. The carousel is always drawn at Idle/Cursor.
    //   Idle/Cursor — resting / a slot focused.
    //   Submenu     — L2 (STAT viewer · ITEMS/MAINT list · placeholder).
    //   Detail      — L3 (item detail · MAINT action).
    //   Process     — a running MAINT process (non-interruptible).
    //   ModalFeeding / ModalLockout — event overlays.
    enum class Nav { Idle, Cursor, Submenu, Detail, Process, ModalFeeding, ModalLockout, ModalLineSelect, ModalEggPick, ModalHatchReveal, ModalEvolve, ModalCSF, Combat, ExploreControl, Encounter, Wifi, Shop, ModShop, WarpPicker, RollbackPicker, CacheYield, BulkYield, PostEncounter, Stacker, Isolation, Chroma, Decryption, Cryptogram, ArcadeResult, Tourney };

    // Which L2 screen the ITEMS submenu is showing. Picker (the category tile
    // screen) only ever appears when itemPickerUnlocked(); every other path — no
    // upgrade, and the Lockout Open-Items entry — lands straight on List.
    enum class ItemsScreen : uint8_t { Picker, List };

    // Wi-Fi network event sub-outcomes. Public so tests/dev tools can
    // steer/inspect which outcome the step-roll landed on.
    enum class WifiOutcome { SleepingGuardian, AwakenedGuardian, OpenCache, FriendlyVisit };

    // What the last Wi-Fi event's real-network discovery actually was, as a value
    // rather than only as the sentence resolveNetworkDiscovery() wrote. The Wi-Fi
    // screen draws the pet taking the network in (FX_ABSORB), and how much of the
    // glyph it gets through is the picture of the same reward the line states in
    // words: a new network is eaten whole, one it is merely fond of is nibbled, its
    // own home turf is left standing, and an empty queue has nothing to show at all.
    enum class NetDiscovery : uint8_t { None, New, Fond, HomeTurf };

    // Which slice of the DECRYPTOGRAM pool a roll is allowed to land in. The VAULT
    // wants an unsolved quote (a prize to win) or, once there are none, a solved one to
    // re-run; the arcade cabinet only ever wants a solved one. Public so a test can
    // interrogate the pool without playing a board.
    enum class QuotePick : uint8_t { Unsolved, SolvedOnly, Any };

    // Move-slot rework #12: the per-pet, per-slot Attack/Defend STAMP. Unset until
    // the slot first unlocks for this pet (stampSlotKinds), then locked forever —
    // see slotKinds_'s declaration for the full contract. Public so tests/render
    // can read it without reaching into private state.
    enum class SlotKind : uint8_t { Unset, Attack, Defend };

    bool tick(uint32_t nowMs);
    void onButton(const ButtonEvent& ev);
    void render(Framebuffer& fb) const;

    // --- Inspection (tests / debug / host) ---------------------------------
    PetModel& model() { return model_; }
    const PetModel& model() const { return model_; }
    Inventory& inventory() { return inventory_; }
    const Inventory& inventory() const { return inventory_; }
    const EventLog& log() const { return log_; }
    // Which page of the mid-combat panel is showing (0 = closed). B cycles it.
    int combatStatsPage() const { return combatStatsPage_; }
    const Loadout& loadout() const { return loadout_; }
    const MoveLoadout& moveLoadout() const { return moveLoadout_; }
    // Move-slot rework #12: the stamped kind for `slot`, or Unset if it hasn't
    // unlocked for this pet yet (out-of-range also reads Unset).
    SlotKind slotKind(int slot) const {
        return (slot >= 0 && slot < kMaxMoveSlots) ? slotKinds_[slot] : SlotKind::Unset;
    }
    // The MoveDef::Kind an equip into `slot` must match. Unset defensively reads as
    // Attack — unreachable in normal play: onTrainList only lets the cursor reach an
    // UNLOCKED slot, and stampSlotKinds() runs on every pet install, so every
    // reachable slot is stamped before the player can ever open its picker.
    MoveDef::Kind slotRequiredKind(int slot) const {
        return slotKind(slot) == SlotKind::Defend ? MoveDef::Kind::Defend
                                                   : MoveDef::Kind::Attack;
    }
    // Content lookup (creatures/items/mods/moves) — read-only, for anything that
    // needs to enumerate the catalog (e.g. the 'Pedia state export).
    const ContentRegistry& content() const { return registry_; }
    const Combat& combat() const { return combat_; }
    int combatXp() const { return combatXp_; }        // XP banked toward the next level
    int combatLevel() const { return combatLevel_; }  // 0-based creature level
    int defragCount() const { return defragCount_; }  // this pet's defrags
    // XP needed to climb from the current level to the next — the geometric curve
    // round(kLevelXpBase * (kLevelXpGrowthPct/100)^combatLevel_).
    int xpToNextLevel() const;
    // Earned points in combat stat `i` (0 power · 1 defense · 2 speed · 3 max-Health);
    // sum == combatLevel_ (the level == total earned points invariant). Out-of-range → 0.
    int levelStatPoint(int i) const {
        return (i >= 0 && i < kLevelStatCount) ? statPoints_[i] : 0;
    }
    // The combat stat the most-recent level-up bumped (0..3), or -1 if never — the
    // "which stat grew" tell (the inline combat-overlay flourish is deferred
    // polish; STAT's LVL n is the shipped feedback). Used by tests to confirm a level-up.
    int lastLevelUpStat() const { return lastLevelUpStat_; }
    int bits() const { return bits_; }
    int bandwidth() const { return bandwidth_; }   // farming resource (D2) — see below
    // The farming-pool CAP: the base plus every "Increase Bandwidth" upgrade bought on
    // the Rig Shop (a). Replaces the raw kBandwidthMax everywhere the cap is
    // read (regen ceiling, the badge/PROFILE denominator, the debug clamp).
    int bandwidthMax() const {
        return kBandwidthMax + rigLevel_[kRigRowBandwidth] * kBandwidthUpgradeStep;
    }
    int bwUpgradeCount() const { return rigLevel_[kRigRowBandwidth]; }   // a purchases made
    // Minutes of real active time per +1 Bandwidth for the ACTIVE pet: the shared
    // interval less whatever this pet has been fed (Tiramisudo), never below the floor.
    // Per-pet, so the same rig regenerates at different speeds under different pets.
    uint32_t bandwidthRegenMinutes() const {
        const uint32_t bonus = static_cast<uint32_t>(bandwidthRegenBonusMin_);
        return bonus >= kBandwidthRegenMinutesPerPoint - kBandwidthRegenMinutesFloor
                   ? kBandwidthRegenMinutesFloor
                   : kBandwidthRegenMinutesPerPoint - bonus;
    }
    // Has this pet had its permanent regen upgrade? The STAT BUFFS page's own question,
    // and the reason a second Tiramisudo is a top-up rather than a second upgrade.
    bool bandwidthRegenUpgraded() const { return bandwidthRegenBonusMin_ > 0; }
    // Has THIS pet already spent `d`'s once-per-lifetime effect (defs.h's
    // itemIsOncePerPetLifetime)? The three gates are separate fields because they guard
    // different things; this is the one place that answers the question generically, so
    // a reader — the 'Pedia's pet page — never has to know which flag belongs to which
    // item. False for any row that has no such effect: nothing to have spent.
    bool lifetimeItemSpent(const ItemDef& d) const;
    // Player-level Fragmentation-amount tier (b, save v21): shaves the battle-fatigue
    // frag AMOUNT cap (Game::applyBattleFatigue).
    uint8_t fragAmountTier() const { return static_cast<uint8_t>(rigLevel_[kRigRowFragReduce]); }
    // Player-level Fragmentation-TRIGGER tier (c, save v22): lowers the
    // battle-fatigue trigger CHANCE (sibling to fragAmountTier's AMOUNT cut).
    uint8_t fragTriggerTier() const {
        return static_cast<uint8_t>(rigLevel_[kRigRowFragTrigger]);
    }
    // d/e (Rig Shop, save v23): one-time, permanent account unlocks — no
    // tiers, just owned or not. itemTabsUnlocked arms the ITEMS hold-B filter cycle;
    // bulkOpenUnlocked arms the VAULT hold-B bulk-open.
    bool itemTabsUnlocked() const { return rigLevel_[kRigRowItemTabs] > 0; }
    bool bulkOpenUnlocked() const { return rigLevel_[kRigRowBulkOpen] > 0; }
    // ITEMS Type-Picker (Rig Shop): another one-time unlock — the ITEMS submenu
    // opens on the category tile screen (ItemsScreen::Picker) instead of straight
    // onto the list, and hold-B's cycle steps up to the finer category axis.
    bool itemPickerUnlocked() const { return rigLevel_[kRigRowItemPicker] > 0; }
    // f (Rig Shop, save v31): the Merge Hub one-time unlock — makes the MRG
    // carousel slot accessible (game_merge.cpp). Doesn't grant any recipe by
    // itself; each recipe is its own one-time Rig Shop row (rigUpgradeOwned).
    bool mergeHubUnlocked() const { return rigLevel_[kRigRowMergeHub] > 0; }
    // g/h (Rig Shop): one-time unlocks gating the auto-arm hooks in
    // Game::autoArmBackupShield — Auto Backup fires at explore-start
    // (startExplore/startDeepWebDive), Continuous Auto-Backup at every resolved
    // explore event (returnToExplore).
    bool autoBackupUnlocked() const { return rigLevel_[kRigRowAutoBackup] > 0; }
    bool continuousBackupUnlocked() const {
        return rigLevel_[kRigRowContinuousBackup] > 0;
    }
    // i (Rig Shop): Disk Maintenance tier (0 = not bought). Level N's threshold/cost
    // multiplier live on kDiskMaintenanceThreshold/kDiskMaintenanceCostMult
    // (game_rig_shop.h), read by Game::maybeAutoDefrag.
    int diskMaintenanceLevel() const { return rigLevel_[kRigRowDiskMaintenance]; }
    // Mod Storage tier -> how many spare copies of ONE mod the pool will hold
    // (modCopyCap, tunables). Every grant runs through it, so a player who wants a
    // bench of a favourite mod buys the room for it rather than accumulating copies
    // nobody counted.
    int modStorageCap() const { return modCopyCap(rigLevel_[kRigRowModStorage]); }
    // Is `row` on sale right now — anything left to buy on it? The single answer the
    // SHOP's A-cycle, its render loop and buyRigUpgrade all defer to, so the list can't
    // show a row the cursor skips.
    bool shopRowOffered(int row) const;
    // Price of the NEXT purchase at a Rig Shop row (game_rig_shop.h) — every row
    // resolves its own cost from its own costStart/curve/costStep, so this is the one
    // place that math happens; every named *Cost() below is a thin wrapper over it.
    int rigUpgradeCostFor(int row) const {
        const RigUpgradeDef& d = kRigUpgrades[row];
        return rigUpgradeCost(d.costStart, rigLevel_[row], d.curve, d.costStep);
    }
    // Price of the NEXT "Increase Bandwidth" upgrade (climbs per purchase, a).
    int bandwidthUpgradeCost() const { return rigUpgradeCostFor(kRigRowBandwidth); }
    // rig upgrades — player-level, persisted (save v27), survive lifecycles
    // like bwUpgradeCount(). All three price off rigUpgradeCostFor() so balance is a
    // start-price/max-purchase edit on the row (game_rig_shop.h), not new code.
    int rackSlotUpgradeCount() const { return rigLevel_[kRigRowRackSlot]; }
    int rackSlots() const { return kRackSlots + rigLevel_[kRigRowRackSlot]; }  // ARCH capacity
    int rackSlotUpgradeCost() const { return rigUpgradeCostFor(kRigRowRackSlot); }
    int scrapingClusterLevel() const { return rigLevel_[kRigRowScraping]; }
    int combatBitsBonusPct() const {
        return rigLevel_[kRigRowScraping] * kScrapingClusterPctPerLevel;
    }
    int scrapingClusterCost() const { return rigUpgradeCostFor(kRigRowScraping); }
    int dataMiningLevel() const { return rigLevel_[kRigRowDataMining]; }
    int cacheBitsBonusPct() const {
        return rigLevel_[kRigRowDataMining] * kDataMiningPctPerLevel;
    }
    int dataMiningCost() const { return rigUpgradeCostFor(kRigRowDataMining); }
    // Applied at every combat/cache Bits payout site so the % bonus is one call, not
    // duplicated arithmetic per site. Integer `bits * pct / 100` truncates to 0 on
    // small grants at low levels (e.g. a 10-Bit Common cache at 2-8% DataMining never
    // moves — the purchase would look inert on-device even though the tier is live),
    // so any ACTIVE bonus (pct > 0, bits > 0) is floored at +1 Bit rather than +0.
    int applyCombatBitsBonus(int bits) const {
        return applyBonusPct(bits, combatBitsBonusPct());
    }
    int applyCacheBitsBonus(int bits) const {
        return applyBonusPct(bits, cacheBitsBonusPct());
    }
    // j (Rig Shop): Passive XP Farming level — Game::tick reads this directly
    // whenever Hunger decays (game_core.cpp), granting level XP per hunger point
    // lost while Hunger stayed above hungerXpThreshold().
    int hungerXpRateLevel() const { return rigLevel_[kRigRowHungerXpRate]; }
    // k (Rig Shop): the Hunger threshold row j's passive XP still counts above.
    // Level 0 sits at the row's base threshold; each level lowers it.
    int hungerXpThreshold() const {
        return kHungerXpBaseThreshold - rigLevel_[kRigRowHungerXpWindow] * kHungerXpWindowStepPct;
    }
    // l (Rig Shop): +kCombatXpPctPerLevel%% combat XP per level, active only
    // while CURRENT Hunger is at or above combatXpWindowThreshold() — a fed and
    // happy pet fights sharper. 0 whenever the pet has let Hunger slip below it.
    int combatXpBonusPct() const {
        if (!pet_ || model_.hunger() < combatXpWindowThreshold()) return 0;
        return rigLevel_[kRigRowCombatXpPct] * kCombatXpPctPerLevel;
    }
    // m (Rig Shop): the Hunger threshold row l's boost stays active above.
    int combatXpWindowThreshold() const {
        return kCombatXpBaseThreshold - rigLevel_[kRigRowCombatXpWindow] * kCombatXpWindowStepPct;
    }
    // Applied at real-fight XP payout sites (wild win / boss / Sim-Battle) — the
    // Sinkhole bypass and Wi-Fi discovery XP grants are deliberately left
    // unwrapped, since neither is an actual fight. Shares applyBonusPct's
    // floor-at-+1 rounding with applyCombatBitsBonus/applyCacheBitsBonus.
    int applyCombatXpBonus(int xp) const {
        return applyBonusPct(xp, combatXpBonusPct());
    }
    // Which top-level face is showing. A+C flips PET ↔ HACKER at the top level.
    enum class Face { Pet, Hacker };
    Face face() const { return face_; }
    int exploreSteps() const { return exploreSteps_; }  // steps this explore run
    bool exploreActive() const { return exploreActive_; }   // explore-mode running
    int exploreStreak() const { return exploreStreak_; }    // current win-streak
    int exploreSector() const { return exploreSector_; }    // armed area (or last)
    int exploreSub() const { return exploreSub_; }          // armed sub-area
    bool autoProgress() const { return autoProgress_; }     // hands-off ladder stepping
    // the endless DeepWeb Dive (terminal zone) is armed. It's an explore mode
    // on the virtual kDeepWebSector — no sub-area/boss, enemies scale to the pet.
    bool inDeepWebDive() const {
        return exploreActive_ && exploreSector_ == kDeepWebSector;
    }
    // Every real area's gauntlet is cleared — the DeepWeb Dive unlock ("beat the
    // exploration game"). Derived, so it needs no separate persisted flag.
    bool allSectorsCleared() const {
        for (int a = 0; a < kAreaCount; ++a) if (!sectorCleared_[a]) return false;
        return true;
    }
    // is hands-off auto-explore paused because the pet is too fragmented
    // (battle fatigue climbed frag into the danger band)? The mode stays armed; a
    // defrag drops frag back below the gate and stepping resumes.
    bool exploreAutoPausedByFatigue() const {
        return exploreActive_ && model_.fragmentation() >= kBattleFatigueAutoPauseFrag;
    }
    // Has sub-area (`area`,`sub`)'s boss been UNLOCKED by a 10-win streak? A
    // durable per-sub flag (persisted, save v13) — distinct from the volatile streak.
    bool subBossUnlocked(int area, int sub) const {
        return area >= 0 && area < kAreaCount && sub >= 0 &&
               sub < kSubAreasPerArea && subBossUnlocked_[area][sub];
    }
    // Has sub-area (`area`,`sub`)'s boss been BEATEN? Durable (save v13).
    bool subCleared(int area, int sub) const {
        return area >= 0 && area < kAreaCount && sub >= 0 &&
               sub < kSubAreasPerArea && subCleared_[area][sub];
    }
    // Is the AREA boss reachable? All 5 sub-areas cleared, area not yet.
    bool areaBossReady(int area) const;

    // --- ROCK THE DOCK: the operator bracket (game_tourney.cpp) --------------
    // How a run ENDS, which is also what the bracket screen is showing. Ready is a
    // run still being fought (or none at all); the other two are terminal banners the
    // operator has yet to dismiss, and they persist — a device put down on the verdict
    // is still showing it after a reboot.
    enum class TourneyPhase : uint8_t { Ready, Eliminated, Champion };
    // Which of the arena screen's three READS is showing. One Nav, three views,
    // because all three are the same screen answering different questions about the
    // same run — and a Nav per reader would put the run's lifecycle in three places.
    //   Bracket  the field: who is left, at what level, and who is next
    //   Scout    the focused entrant's full kit, on the LOADOUT page's own flow
    //   Brief    the arena explaining itself (content_tournament.h's kTourneyBrief)
    enum class TourneyView : uint8_t { Bracket, Scout, Brief };
    // A bracket is in play. The seed IS the run (core/model/tournament.h), so a zero
    // seed means "no run" and nothing else has to be checked.
    bool tourneyRunning() const { return tourneySeed_ != 0; }
    uint32_t tourneySeed() const { return tourneySeed_; }
    uint8_t tourneyAlive() const { return tourneyAlive_; }   // survivor bitmask
    int tourneyRound() const { return tourneyRound_; }       // 0-based
    TourneyPhase tourneyPhase() const { return tourneyPhase_; }
    // The operator's own slot, and the slot they face next (-1 once the run is over).
    int tourneySlot() const;
    int tourneyOpponentSlot() const;
    // The opponent as a built fighter — resolved once when a round opens, so the
    // bracket screen can name its species and Exploit without walking the content
    // tables every repaint.
    const TourneyFighter& tourneyOpponent() const { return tourneyOpponent_; }
    TourneyView tourneyView() const { return tourneyView_; }
    int tourneyCursor() const { return tourneyCursor_; }
    // Is a game engine's mid-play RULES overlay open right now? (game_arcade.cpp)
    bool gameBriefOpen() const { return gameBriefOpen_; }
    // The two readers' row models (core/ui/prose_page.h), built on demand — both walk
    // the content tables, so they are called on a press or a repaint, never held.
    // Public so a gate can assert what a sheet SAYS instead of reading it out of pixels.
    std::vector<ProseRow> tourneyScoutRows() const;
    std::vector<ProseRow> tourneyBriefRows() const;
    WifiOutcome wifiOutcome() const { return wifiOutcome_; }   // rolled sub-outcome
    // WHICH of the rival's moves beating it could teach this pet — one bit per index
    // into its kit, and the single answer to that question anywhere on the screen. The
    // combat STAT panel's KIT page marks each set bit in its gutter (RivalPrizes,
    // core/ui/combat_screen.h) and the outro turns the whole mask into which dissolve
    // plays, so the promise the last beat makes and the list the panel showed are the
    // same fact rather than two readings of it. Empty in any fight that cannot teach.
    uint32_t rivalTeachableMoveMask() const;
    // ...and that mask as the yes/no the dissolve needs. Public so a gate can assert the
    // answer rather than inspect pixels.
    bool rivalFieldsUnknownMove() const { return rivalTeachableMoveMask() != 0; }
    // Does the fight on screen close with a dissolve over the beaten rival, and is that
    // dissolve still running? The render side picks WHICH one plays (CombatOutro); this
    // is the half the tick needs, because the result beat's hands-off auto-dismiss must
    // not start counting until the effect has cleared the verdict banner's own space.
    bool combatOutroEligible() const;
    bool combatDissolveRunning() const;
    // The on-screen line resolveNetworkDiscovery() set on the last Wi-Fi event
    // (new/familiar/home-turf/empty-queue) — "" if none has resolved yet.
    const char* netDiscoveryFlavor() const { return netDiscoveryFlavor_; }
    // The same beat as a value, for the screen's picture of it (and for a gate to
    // assert against without reading the sentence).
    NetDiscovery netDiscovery() const { return netDiscovery_; }
    int networksSeen() const { return networksSeen_; }         // lifetime, dedup'd
    // Scan-time sightings queued in RAM, not yet resolved into the SD-backed
    // NetworkLedger by an EXPL Wi-Fi event — how close the pet is to
    // kPendingNetworkQueueCap, past which new sightings are dropped.
    int pendingNetworks() const { return pendingNetworkCount_; }
    // Storefront (item shop AND mod shop share this accessor set — shopIsModShop()
    // says which registry a row's `id` resolves through). `i` is bounds-clamped to
    // "" / 0 for an out-of-range row, so a caller can safely loop `0..shopListingCount()`.
    bool shopIsModShop() const { return shopIsModShop_; }       // false = item shop, true = mod shop
    const char* shopStoreName() const { return shopFront_ ? shopFront_->name : ""; }
    int shopListingCount() const { return shopFront_ ? shopFront_->listingCount : 0; }
    int shopCursor() const { return shopCursor_; }              // A-cycled row selection
    const char* shopListingId(int i) const;                     // raw content id (tests/debug)
    const char* shopListingName(int i) const;                   // resolved display name
    // Resolved concrete-effect text: the listing's own description with its
    // magnitudes substituted in (core/content/effect_text.h). Returned by value —
    // the text is rendered from the row, not borrowed from flash.
    EffectText shopListingDescription(int i) const;
    int shopListingStock(int i) const;                          // remaining THIS visit
    int shopListingBitsPrice(int i) const;
    int shopListingCostCount(int i) const;                      // non-empty extra item costs
    const char* shopListingCostId(int i, int k) const;          // raw content id (tests/debug)
    const char* shopListingCostName(int i, int k) const;        // display name of cost item k
    int shopListingCostQty(int i, int k) const;
    int shopListingCostHave(int i, int k) const;                // held count, for the buy gate
    // EXPL sector-clear flags: has sector `idx`'s boss/gauntlet
    // been cleared? Drives the linear unlock (sector N+1 opens when N clears).
    bool sectorCleared(int idx) const {
        return idx >= 0 && idx < kAreaCount && sectorCleared_[idx];
    }

    // Power/battery status pushed from the platform tier (device reads the ADC +
    // CHG_STAT; host leaves it absent). Surfaced on the CFG "BATT" line.
    void setPowerStatus(const PowerStatus& p) { power_ = p; }
    const PowerStatus& powerStatus() const { return power_; }

    // microSD presence pushed from the platform tier at boot (device mounts the
    // card + round-trips it; host leaves it absent). Surfaced on the CFG "SD" line
    // so it reflects the real card instead of a hardcoded "ABSENT".
    // The largest single allocation the platform could satisfy right now, pushed in
    // each loop like the SD and power readings. Runtime-only, and 0 means "nobody
    // told us" — a host build never pushes one, so it never blocks a save there.
    //
    // This exists because a save is the biggest allocation the engine makes (~16KB
    // in one piece) and it fires off a timer, so it lands at moments nothing chose:
    // on device the audit capture claims ~70KB as it arms and keeps taking more for
    // seconds afterwards, and a save landing in that window found no block big
    // enough. The Arduino build has exceptions off, so that is not a failed save —
    // operator new's bad_alloc goes to std::terminate and RESETS THE DEVICE, losing
    // the very state the save was trying to keep. It cannot be caught, only avoided.
    // A PROBE, not a pushed reading, because the thing it guards allocates in two
    // stages: building the SaveData, then reserving the blob. A value sampled once a
    // loop is already stale by the second stage — the first one spent it — so the
    // engine has to be able to ask again mid-flight. Captureless, so the platform's
    // lambda decays to a plain function pointer and core/ stays free of <functional>.
    using HeapProbe = uint32_t (*)();
    void setHeapProbe(HeapProbe p) { heapProbe_ = p; }
    uint32_t heapHeadroom() const { return heapProbe_ ? heapProbe_() : 0; }

    // Whether a save can be attempted right now without risking that reset. No probe
    // (host, or a platform that doesn't report) always says yes.
    bool saveHasHeadroom() const {
        const uint32_t h = heapHeadroom();
        return h == 0 || h >= saveHeapFloor();
    }

    // What that answer is measured against, which depends on what THIS save would have
    // to allocate. One writing into the blob buffer Game already owns only needs
    // working room for the SaveData; one that still has to size that buffer needs the
    // whole reservation in a single piece, and is held to the far higher bar for it.
    uint32_t saveHeapFloor() const {
        return saveBlob_.capacity() >= saveBlobTargetBytes() ? kSaveHeapFloorBytes
                                                            : kSaveGrowHeapFloorBytes;
    }

    // How many save attempts in a row the guard has turned away, and the smallest
    // headroom it saw doing it. Both reset when a write finally lands, so this reads
    // as "what is happening right now", not a lifetime tally. Exists because the
    // guard is otherwise invisible: it fires on a heap state that is gone by the time
    // anything samples it, so without a count there is no way to tell a working guard
    // apart from a device that happened to have room.
    int savesDeferred() const { return savesDeferred_; }
    uint32_t savesDeferredLowMark() const { return savesDeferredLow_; }
    // How many writes in a row the STORE has refused — a save that was built, handed
    // over, and turned down by the medium rather than held back by the heap. Non-zero
    // means the blob on flash is older than what is in RAM right now, which is the one
    // state that survives a reboot as lost progress.
    int saveWritesFailed() const { return saveWritesFailed_; }

    void setSdStatus(const SdStatus& s) {
        // A card just becoming present (boot mount OR a runtime re-check) arms the
        // idle-screen SD-present reveal (sdIconVisible()); a fresh reading of an
        // already-present card doesn't re-flash it.
        if (s.present && !sdStatus_.present) {
            sdIconRevealAtMs_ = nowMs_;
            sdIconRevealArmed_ = true;
        }
        sdStatus_ = s;
        dirty_ = true;
    }
    const SdStatus& sdStatus() const { return sdStatus_; }

    // Idle-screen status icons (host-testable). sdIconVisible() is true for
    // kSdIconRevealMs after the card became present (a transient "card mounted"
    // flash), dual-coded by icon shape + position so the idle screen stays
    // readable in grayscale.
    bool sdIconVisible() const {
        return sdStatus_.present && sdIconRevealArmed_ &&
               (nowMs_ - sdIconRevealAtMs_) < kSdIconRevealMs;
    }

    // Idle-screen Audit capture-phase badge (top-right). Writes a short tag into
    // `out` and returns true when a badge should show; returns false + empties
    // `out` when Disarmed (capture idle/off). Surfaces the AuditCapture SM phase —
    // "ARM" (armed, listening), "HOT <s>s" (broadcast window), "COOL <m>m" (the
    // ~30-min re-arm cooldown, radio OFF — no capture possible) — so the phase is
    // visible instead of inferred from serial. The word carries the meaning, so it
    // reads in grayscale. Host-tested (test_capture_badge_phases).
    bool captureBadge(char* out, unsigned n) const;

    // How many text lines the explore status stack occupies in the idle screen's
    // top-LEFT corner: the EXPL badge, the Bandwidth readout, and the event-flavor
    // tick when there is one. The corner is shared with the SD flash and the
    // armed-buff row, which start below the block this measures, so the two
    // consumers in game_render.cpp stay in step instead of each guessing a height.
    int exploreStatusLines() const {
        if (!exploreActive_) return 0;
        return exploreFlavor_[0] ? 3 : 2;
    }

    // Runtime microSD re-check seam (mirrors the netScan opt-in pattern). The
    // device mounts the card once in setup(); a card inserted later is invisible
    // until this one-shot flag asks the device loop to unmount + remount and push
    // a fresh setSdStatus(). CFG sets it (requestSdRecheck); the device tier reads
    // sdRecheckRequested(), acts, then clears it. No-op on host / card-less boards.
    void requestSdRecheck() { sdRecheckRequested_ = true; }
    bool sdRecheckRequested() const { return sdRecheckRequested_; }
    void clearSdRecheck() { sdRecheckRequested_ = false; }

    // Travel-mode seam (CFG → DEVICE → TRAVEL MODE), a deliberate indefinite pause
    // for an operator who is going away and does not want to babysit a pet. Set
    // here, acted on by the device tier: it powers the radio down, lands a save, and
    // deep-sleeps the SoC until the wake chord.
    //
    // There is deliberately NO clock to freeze on this side. A deep sleep on this
    // board is a reset, so a wake re-enters through the save, and every engine clock
    // is already reboot-safe by one of three routes: rebased against nowMs_ on load
    // (stageEnteredMs_, lastModelMs_), stored as a REMAINING count rather than a
    // deadline (bootHatchRemainMs_), or anchored on lifetimeUptimeMs(), which is
    // awake time and does not advance while the device is off (backupShieldUntilMs_,
    // dyingElapsedMs_). Nothing observes wall time, because the board carries no RTC
    // (config.h HAS_HARDWARE_RTC) — so the sleep gap is uncreditable by construction
    // rather than by bookkeeping.
    //
    // Latched, not a pulse: the device tier may have to hold it for a moment (an OTA
    // image still on trial must commit before a reset is safe), so it stays set until
    // clearTravelSleep(). The host tier clears it without sleeping.
    void requestTravelSleep() { travelSleepRequested_ = true; dirty_ = true; }
    bool travelSleepRequested() const { return travelSleepRequested_; }
    void clearTravelSleep() { travelSleepRequested_ = false; dirty_ = true; }

    // Write the current state now, reporting whether it landed. Unconditional — it
    // does not ask whether anything is dirty, because passive decay moves the model
    // without marking itself and this is called when the state is about to stop
    // existing in RAM. Travel sleep is the caller that needs that guarantee: a deep
    // sleep is a reset, so anything still inside the kSaveDebounceMs window is simply
    // lost. False means the heap guard turned the write away (savesDeferred), and the
    // caller must stay awake and ask again rather than treat the state as written.
    bool saveNow();

    // Which consent actually holds the radio, pushed from the arbiter whenever it
    // hands ownership over (platform/esp32/radio_arbiter.h). Runtime-only: it is a
    // reading of what the hardware is doing right now, not a preference to persist.
    // The CFG RADIO group and System Info report it, because four toggles resolving
    // to one owner is otherwise invisible from the screens that set them.
    void setRadioOwner(RadioOwner o) {
        if (radioOwner_ == o) return;
        radioOwner_ = o;
        dirty_ = true;
    }
    RadioOwner radioOwner() const { return radioOwner_; }
    int hackerRank() const { return hackerRank_; }              // (unbounded)

    // Audit-mode passive Wi-Fi discovery scan. Runtime opt-in,
    // persisted (save v5), default OFF — an authorized-use affirmative choice.
    //
    // True while the CREW screen is the open L2 — the home-network picker's live half
    // depends on the scan actually running, so radioScanWanted() below reads this.
    // Derived from nav state rather than a latch, so it can never be left stuck on.
    bool crewScreenOpen() const {
        return face_ == Face::Hacker && nav_ == Nav::Submenu &&
               enteredHackerId() == HackerSlotId::Crew;
    }
    // What the RADIO should do right now, as opposed to netScanEnabled()'s persisted
    // player opt-in: the passive discovery scan also runs, unconditionally, while the
    // CREW screen is open, so picking a home network never requires a detour into CFG
    // to arm the scan (and never leaves it armed afterwards — closing the screen
    // returns the radio to the config-dictated mode). This raises the SCAN only; the
    // handshake CAPTURE opt-in is untouched and stays strictly explicit. The
    // RadioArbiter still decides who actually owns the radio, so a live AP or capture
    // owner continues to stand this down.
    bool radioScanWanted() const { return netScanEnabled_ || crewScreenOpen(); }
    // The persisted CFG opt-in itself — what the AUDIT row shows and the save stores,
    // and the baseline radioScanWanted() reads. registerNetwork() is how a heard AP
    // feeds back in from the device tier's WiFi.scanNetworks().
    bool netScanEnabled() const { return netScanEnabled_; }
    void setNetScanEnabled(bool on);

    // Pet-to-pet LINK (game_peers.cpp, core/net/peer_link.h). Runtime opt-in,
    // persisted (save v37), default OFF — and deliberately NOT implied by the audit
    // scan: that toggle consents to LISTENING, this one consents to BROADCASTING the
    // operator's HackerTag, pet and crew to anything in range.
    //
    // True while the PEERS screen is the open L2. Discovery there must be immediate —
    // two people holding their devices together shouldn't wait on a duty cycle — so
    // the screen arms the radio itself, the same way the CREW picker arms the scan.
    // Derived from nav state rather than a latch, so it can never be left stuck on.
    bool peersScreenOpen() const {
        return face_ == Face::Hacker && nav_ == Nav::Submenu &&
               enteredHackerId() == HackerSlotId::Peers;
    }
    // True while the LINK screen is the open L2 — the duel surface (game_pvp.cpp).
    // Arms the radio for the same reason PEERS does, and more sharply: two people are
    // standing next to each other trying to start a fight.
    bool linkScreenOpen() const {
        return face_ == Face::Hacker && nav_ == Nav::Submenu &&
               enteredHackerId() == HackerSlotId::Link;
    }
    // What the LINK RADIO should do right now, as opposed to linkEnabled()'s persisted
    // opt-in: announcing also runs while PEERS or LINK is open, then stops on exit, and
    // for as long as a duel session is in flight — the handshake and the finished
    // fight's BYE both need the air. The RadioArbiter still decides who actually owns
    // the radio.
    bool linkWanted() const {
        return linkEnabled_ || peersScreenOpen() || linkScreenOpen() || pvpSessionLive();
    }
    // Whether LINK should hold the radio in the FOREGROUND (the arbiter's Link owner)
    // rather than riding the scan's ambient window. True exactly when a screen or a
    // session is waiting on frames right now. Read by platform/esp32/net_link.h.
    bool linkForegroundWanted() const {
        return peersScreenOpen() || linkScreenOpen() || pvpSessionLive();
    }

    // A screen whose being-open is what arms a radio, and which the operator reads
    // hands-off while it fills in. Both the menu's idle collapse (kRadioScreenDefocusMs)
    // and the device tier's panel sleep read this ONE predicate: on these screens the
    // usual timers would power down the radio the operator is waiting on, so the
    // screen would starve itself. A live duel counts for the same reason — the fight
    // is playing out on two screens and neither may sleep mid-round. Derived per-frame
    // from nav, never a sticky flag — the normal timeouts resume on exit.
    // A running update job counts too, and is the one entry here NOT derived from a
    // screen: it holds the association itself, so the panel must stay up to report a
    // download the operator is watching.
    bool radioScreenOpen() const {
        return crewScreenOpen() || peersScreenOpen() || linkScreenOpen() ||
               pvpSessionLive() || netConnectWanted();
    }
    bool linkEnabled() const { return linkEnabled_; }
    void setLinkEnabled(bool on);

    // Whether a home network is STORED, pushed from the platform tier at boot and
    // again whenever the setup page writes one (platform/esp32/net_credentials.h).
    // Runtime-only and deliberately just a bool: the engine must never hold, render
    // or serialise the credentials themselves. This is CAPABILITY, not permission —
    // a stored network is somewhere the device COULD go, and only a job the operator
    // started ever takes it there (see netConnectWanted).
    void setNetProvisioned(bool on) {
        if (netProvisioned_ == on) return;
        netProvisioned_ = on;
        dirty_ = true;
    }
    bool netProvisioned() const { return netProvisioned_; }

    // What platform/esp32/net_sta.h asks each loop, and what the arbiter reads to
    // pick the Update owner.
    //
    // ONE ASKER. This device joins a network for exactly as long as a job the
    // operator started needs it to, and not one loop longer — there is no standing
    // permission that survives the job, and none that survives a reboot. Pressing
    // CHECK NOW (or confirming an install) IS the directive to go online; the job
    // runs to a terminal outcome and releases the radio itself, which is why it is
    // latched rather than nav-derived — a fetch that died the moment the menu
    // collapsed could never finish. When it lets go, the arbiter re-resolves and the
    // radio falls back to whichever toggle was already on, so an update puts the
    // radio back the way it found it without anyone bookkeeping that.
    bool netConnectWanted() const { return netProvisioned_ && updateJobLive(); }
    // The association outcome, pushed back by the device tier and reported by the
    // UPDATES screen as the first step of a job (CONNECTING → CHECKING → the verdict).
    // A terminal FAILURE ends the job and fails it as NoNetwork: there is nothing to
    // fetch over a connection that never came up, a dead attempt must not keep the
    // radio's top owner holding the AP down while nothing happens, and the screen
    // has to say which step gave up rather than sitting on CHECKING forever.
    void setNetStatus(const NetStatus& s) {
        netStatus_ = s;
        if (s.state == NetState::Failed && updateJob_) {
            if (jobTarget_ == UpdateTarget::None) {
                updateStatus_.state = UpdateState::Failed;
                updateStatus_.fail = UpdateFail::NoNetwork;
            } else {
                installStatus_.state = InstallState::Failed;
                installStatus_.fail = InstallFail::NoNetwork;
            }
            endJob();
        }
        dirty_ = true;
    }
    const NetStatus& netStatus() const { return netStatus_; }

    // Whether this device knows WHERE to check, pushed from the platform tier at
    // boot (platform/esp32/update_source.h). Runtime-only and just a bool, like
    // netProvisioned() — the URL itself never enters the engine. A device with no
    // source doesn't get offered a check it cannot perform.
    void setUpdateSourceKnown(bool on) {
        if (updateSourceKnown_ == on) return;
        updateSourceKnown_ = on;
        dirty_ = true;
    }
    bool updateSourceKnown() const { return updateSourceKnown_; }

    // The manifest address itself, pushed from the platform tier
    // (platform/esp32/update_source.h) at boot and again whenever the 'Pedia's
    // user section rewrites it. Display only — the engine never fetches — but it
    // has to be readable so that field can show what it is about to replace.
    // Empty means this device has no source at all.
    void setUpdateManifestUrl(const char* url) {
        const char* src = url ? url : "";
        if (std::strcmp(updateManifestUrl_, src) == 0) return;
        std::strncpy(updateManifestUrl_, src, sizeof(updateManifestUrl_) - 1);
        updateManifestUrl_[sizeof(updateManifestUrl_) - 1] = '\0';
        updateSourceKnown_ = updateManifestUrl_[0] != '\0';
        dirty_ = true;
    }
    const char* updateManifestUrl() const { return updateManifestUrl_; }

    // What web 'Pedia bundle is on the card, read off its marker by the platform
    // tier (platform/esp32/web_bundle.h) and refreshed after an install. Display
    // only — every "is it newer" comparison is made device-side against the packed
    // code, which never enters the engine. Empty means no card, or no marker.
    void setWebBundleVersion(const char* v) {
        const char* src = (v && v[0]) ? v : "UNKNOWN";
        if (std::strcmp(webBundleVersion_, src) == 0) return;
        std::strncpy(webBundleVersion_, src, sizeof(webBundleVersion_) - 1);
        webBundleVersion_[sizeof(webBundleVersion_) - 1] = '\0';
        dirty_ = true;
    }
    const char* webBundleVersion() const { return webBundleVersion_; }

    // Everything a check needs before it can even start: a network to reach through
    // and somewhere to look. Both are SETUP, not permission — there is no consent
    // bit to pre-arm, because pressing the button is the permission. The UPDATES
    // screen reads this to say which piece is missing instead of offering a button
    // that would silently do nothing.
    bool updateCheckReady() const { return netProvisioned_ && updateSourceKnown_; }

    // Ask what is published. Raises the connection itself and seeds the CONNECTING
    // step the screen reports, because the job is the reason the radio comes up at
    // all — there is nothing to switch on beforehand and nothing left switched on
    // afterwards.
    void requestUpdateCheck() {
        if (!updateCheckReady()) return;
        if (updateJob_) return;                    // already running; don't restart it
        updateJob_ = true;
        jobTarget_ = UpdateTarget::None;           // None == this job is a check
        updateStatus_ = UpdateStatus{};
        updateStatus_.state = UpdateState::Checking;
        netStatus_ = NetStatus{};
        netStatus_.state = NetState::Connecting;
        dirty_ = true;
    }

    // THE SECOND YES. A check only ever fills in what is available; this is the
    // separate, explicit act that writes something. It refuses any target the last
    // check didn't actually report as newer, so an install can never run against a
    // stale screen or a version the device never confirmed exists.
    void requestUpdateInstall(UpdateTarget t) {
        if (!updateCheckReady() || updateJob_) return;
        if (t == UpdateTarget::Firmware && !updateStatus_.firmwareNewer) return;
        else if (t == UpdateTarget::Web && !updateStatus_.webNewer) return;
        else if (t == UpdateTarget::None) return;
        updateJob_ = true;
        jobTarget_ = t;
        installStatus_ = InstallStatus{};
        installStatus_.state = InstallState::Downloading;
        installStatus_.target = t;
        // Its own connection, raised and dropped with this job. An install after a
        // check reconnects rather than riding the check's association: the window is
        // shorter, the lifetime is the same rule every job follows, and re-joining a
        // known network costs seconds.
        netStatus_ = NetStatus{};
        netStatus_.state = NetState::Connecting;
        dirty_ = true;
    }

    // End a running job without an answer (consent withdrawn, or the operator
    // cancelled). Leaves a completed job's RESULT alone — a finished check's
    // verdict is what the screen is showing, and clearing it on the way past would
    // wipe the very thing the operator asked for.
    void cancelUpdateJob() {
        if (!updateJob_) return;
        updateJob_ = false;
        if (jobTarget_ == UpdateTarget::None) updateStatus_ = UpdateStatus{};
        else installStatus_ = InstallStatus{};
        jobTarget_ = UpdateTarget::None;
        dirty_ = true;
    }

    // True while a check OR an install is actually in flight. This is what holds
    // the radio (see netConnectWanted) and what platform/esp32/net_update.h
    // services, so it must be latched rather than nav-derived — and must always
    // reach a terminal outcome, since nothing else will take the radio back off it.
    bool updateJobLive() const { return updateJob_; }
    // What the live job is doing. None means a check; anything else is an install,
    // which is also how the screen knows which of the two to render.
    UpdateTarget updateJobTarget() const { return jobTarget_; }

    // The check's outcome, pushed back by the device tier. Any terminal state ends
    // the job and hands the radio back; Checking is the device saying "still
    // working" and keeps it.
    void setUpdateStatus(const UpdateStatus& s) {
        updateStatus_ = s;
        if (s.state != UpdateState::Checking) endJob();
        dirty_ = true;
    }
    const UpdateStatus& updateStatus() const { return updateStatus_; }

    // The install's progress, same contract: Done and Failed are terminal and hand
    // the radio back, everything else keeps it. A finished install's status is left
    // standing so the screen can report what happened.
    void setInstallStatus(const InstallStatus& s) {
        installStatus_ = s;
        if (s.state == InstallState::Done || s.state == InstallState::Failed) endJob();
        dirty_ = true;
    }
    const InstallStatus& installStatus() const { return installStatus_; }

    // True when LINK is on but has no ambient airtime to use. Ambient discovery
    // borrows the audit scan's radio-up window rather than powering the radio on its
    // own schedule (platform/esp32/net_link.h), so with the scan off, LINK only works
    // while the PEERS screen is open. The dependency is REPORTED, never silently
    // fixed: arming the scan from here would grant a listening consent the operator
    // didn't give. The LINK and PEERS screens say so instead.
    bool linkAmbientStarved() const { return linkEnabled_ && !netScanEnabled_; }

    // What this device broadcasts about itself — filled from the live pet/crew/tag.
    // The device tier encodes it (encodePeerHello) and beacons it; nothing here
    // touches the radio.
    void buildPeerHello(PeerHello* out) const;

    // Note a HELLO frame heard from `mac`. Decodes and validates it (a malformed or
    // foreign frame is dropped), refreshes the in-range snapshot, and — only on the
    // edge where a peer ARRIVES rather than merely keeps beaconing — counts a meeting
    // in the SD-backed roster. That edge is what stops a device sitting next to you
    // for an hour from logging 3600 meetings. Returns true when a genuinely NEW
    // operator was met, which is what the caller turns into the log line.
    bool registerPeer(const uint8_t* mac, const uint8_t* payload, size_t len);

    // One row of the PEERS screen: an operator, live or remembered.
    struct PeerRow {
        uint64_t key = 0;
        const char* tag = "";
        const char* petName = "";
        const char* crewName = "";
        uint8_t stage = 0;
        uint8_t rank = 0;
        bool crewRed = false;
        bool live = false;      // in range right now (vs. remembered from before)
        uint32_t timesMet = 0;
    };
    // Fill `rows` with every known operator — the live snapshot merged over the
    // roster, live ones first. Returns how many were written (capped at `max`).
    int peerRows(PeerRow* rows, int max) const;
    // How many operators are in range right now. 0 on host and whenever LINK is idle.
    int livePeerCount() const;
    // Load the SD-backed met-list. Mirrors loadNetworkLedger: a missing file (first
    // boot, no card) just starts the roster empty.
    void loadPeerLedger(const char* path);
    const PeerLedger& peerLedger() const { return peerLedger_; }
    // Seed a peer without a radio, for tests and the host build. The two halves the
    // screens read from are separate on purpose, so each has its own seam: the LEDGER
    // is everyone ever met (an absent peer is a ledger-only row), while the LIVE
    // snapshot is who is in range this second — and LINK will only duel the latter.
    void debugSeedPeer(uint64_t key, const PeerHello& hello);
    void debugSeedLivePeer(uint64_t key, const PeerHello& hello);

    // --- 1v1 duels over LINK (game_pvp.cpp, core/net/pvp_link.h) -----------
    //
    // The LINK slot is where a peer PEERS merely reports becomes an opponent. A duel is
    // three frames of handshake and then nothing: both devices rebuild the same two
    // fighters and step the same seeded Combat locally, so the fight plays out on both
    // screens at once without a single turn crossing the air (core/model/pvp_battle.h).
    //
    // No stakes, by design and not just by omission: nothing here is persisted, no XP,
    // Bits, Fragmentation or care signal moves, and the wire format is unauthenticated
    // on the strength of exactly that. Adding a reward means adding signing first.

    // Where the local device is in a duel. Idle is "no session"; every other phase
    // holds pvpPeerKey_/pvpSession_ and keeps the LINK radio up.
    enum class PvpPhase : uint8_t {
        Idle,        // nothing in flight
        Inviting,    // we challenged; waiting for their accept/decline
        Invited,     // they challenged us; waiting on THIS human to accept or refuse
        Arming,      // both fighters known; host is sending / guest awaiting START
        Fighting,    // the shared fight is running (nav_ == Nav::Combat)
        Result,      // it finished; the LINK screen is showing who won
    };

    PvpPhase pvpPhase() const { return pvpPhase_; }
    bool pvpActive() const { return pvpPhase_ != PvpPhase::Idle; }
    // A session that still NEEDS the air: mid-handshake, or a fight whose host is still
    // repeating its START. Result is deliberately excluded — a verdict left on screen
    // must not pin the radio (and, through radioScreenOpen, the panel) awake for as
    // long as nobody presses a button.
    bool pvpSessionLive() const {
        return pvpPhase_ != PvpPhase::Idle && pvpPhase_ != PvpPhase::Result;
    }
    // True while a duel is actually resolving, which is what makes onCombat suppress
    // the A+C Exploit picker and the C flee: both would pause or end THIS device's copy
    // of a fight the other device is still playing out.
    bool pvpFighting() const { return pvpPhase_ == PvpPhase::Fighting; }
    // Which side of Combat the local pet occupies. The HOST is always player_ on both
    // devices (core/model/pvp_battle.h), so the guest's own pet is the enemy_ row and
    // the combat screen relabels rather than resolving a mirrored fight.
    bool pvpLocalIsHost() const { return pvpHost_; }
    // The same fact in the combat screen's terms (CombatSides::localIsEnemySide): the
    // one fight where the local pet is not Combat's player_ slot is a duel this device
    // guests. Shared by drawCombatScreen and by anything that has to bind a resolved
    // turn to the LOCAL seat rather than to Combat's own.
    bool combatLocalIsEnemySide() const { return pvpFighting() && !pvpLocalIsHost(); }
    const char* pvpOpponentTag() const { return pvpPeerTag_; }
    // The finished duel's verdict from the LOCAL operator's point of view. Only
    // meaningful in PvpPhase::Result.
    bool pvpLocalWon() const { return pvpWon_; }
    // Why the last session ended without a fight (empty = it didn't). Rendered by the
    // LINK screen so a refusal never reads as a hang.
    const char* pvpStatusText() const { return pvpStatus_; }

    // What this device offers as its fighter — species, equipped moves/mods, level and
    // stat points. Also what it feeds its OWN side of the fight through, so the two
    // devices cannot diverge via a local shortcut. False when there is no pet to fight
    // with (an unhatched egg).
    bool buildPvpFighter(PvpFighter* out) const;

    // The keys of every operator in range right now, in snapshot order — what the LINK
    // screen lists and what B challenges. Live only: a remembered operator is a memory,
    // not an opponent, since a duel needs both devices powered and present. Returns how
    // many were written (capped at `max`).
    int pvpChallengeableKeys(uint64_t* out, int max) const;

    // Challenge the live peer at `key` (the LINK screen's B). Inert unless the local
    // device is idle, has a pet, and that peer is currently in range.
    bool pvpChallenge(uint64_t key);
    // The local human's answer to an incoming challenge (the LINK screen's B/C).
    void pvpAcceptChallenge();
    void pvpDeclineChallenge();
    // Abandon whatever session is in flight, telling the other device so its screen
    // doesn't sit waiting. Safe to call from any phase.
    void pvpCancel();

    // --- Device-tier seams (platform/esp32/net_link.h) ---------------------

    // Note a duel frame heard from `mac`. Decodes and validates it; a malformed frame,
    // or one for a session this device isn't in, is dropped. Returns true if it moved
    // the state machine.
    bool onPvpFrame(const uint8_t* mac, const uint8_t* payload, size_t len);

    // Drain one queued outbound frame into `buf`, writing its destination MAC into
    // `mac` (6 bytes) and its length into `len`. Returns false when the outbox is
    // empty. The engine never touches the radio; this is the whole send path.
    bool takePvpOut(uint8_t* mac, uint8_t* buf, size_t cap, size_t* len);

    // This device's own radio MAC, needed to decide who hosts (pvpHostIsLocal). The
    // device tier hands it over once at boot; on host it stays 0 and duels are inert.
    void setLocalRadioMac(const uint8_t* mac);
    uint64_t localRadioKey() const { return localRadioKey_; }

    // 'Pedia local Access Point. Runtime opt-in, persisted
    // (save v20), default OFF. The device tier reads apEnabled() each loop; when
    // on it is the radio's TOP owner (SoftAP + the HTTP 'Pedia server), standing
    // the audit scan/capture down — and handing the radio back to whichever of
    // those is still toggled on the moment the AP is switched off.
    bool apEnabled() const { return apEnabled_; }
    void setApEnabled(bool on);

    // Audit-mode handshake CAPTURE. A sibling authorized-use opt-in
    // (persisted, save v6, default OFF) driving the passive WPA-handshake -> .pcap
    // path. The policy/timing lives in the AuditCapture SM; the device tier reads
    // auditCapture().capturing()/broadcasting() to drive the radio and calls
    // auditCapture().noteHandshake() when its sniffer sees a real 4-way handshake.
    bool auditCaptureEnabled() const { return auditCapture_.enabled(); }
    void setAuditCaptureEnabled(bool on);
    const AuditCapture& auditCapture() const { return auditCapture_; }
    AuditCapture& auditCapture() { return auditCapture_; }

    // The escalating audit control (single CFG setting). One ordered dial instead
    // of two independent toggles, so the radio-cost ladder is explicit and the
    // "capture needs scan" dependency is structural, not a state you can violate:
    //   Off         — radio idle (best battery)
    //   Scan        — passive discovery only (feeds NETS / Hacker Rank)
    //   ScanCapture — scan + passive handshake->pcap (most battery, authorized use)
    // Derived from the two persisted opt-ins; setAuditMode() sets them consistently
    // (and the low-level setters above enforce the same invariant on their own).
    enum class AuditMode : uint8_t { Off = 0, Scan = 1, ScanCapture = 2 };
    AuditMode auditMode() const {
        return auditCapture_.enabled() ? AuditMode::ScanCapture
             : netScanEnabled_         ? AuditMode::Scan
                                       : AuditMode::Off;
    }
    void setAuditMode(AuditMode m);
    // Note a network observed by the device-tier passive scan (net_sniff.h calls this
    // once per heard AP on every sweep, dups included). It feeds TWO independent
    // structures, and the split is the point:
    //   * visibleNetworks_ — the in-range snapshot, refreshed on every sighting. This
    //     is what the CREW home-network picker reads, so "which networks can I see
    //     right now" is answered by the radio, never by a backlog.
    //   * pendingNetworks_ — the reward queue the EXPL Wi-Fi event drains one sighting
    //     at a time. NOT credited here: uniqueness-checking against the SD-backed
    //     ledger, XP, and rewards all happen when resolveNetworkDiscovery() pops it.
    // A network arriving in range (new to the snapshot, not merely re-heard) nudges
    // the egg-hatch clock immediately (accelerateEggHatch) — an incubating egg can
    // never explore, so this is its only hatch accelerator, deliberately decoupled
    // from Hacker Rank credit. Returns true if the sighting was newly QUEUED for
    // reward; false if it was already pending — the snapshot is updated either way.
    // `bssid` is 6 bytes; `ssid` may be empty (hidden network) — a BSSID-derived
    // fallback label is used instead. PASSIVE discovery only — no capture/injection.
    bool registerNetwork(const uint8_t* bssid, const char* ssid);
    // How many networks the radio can currently hear (snapshot entries still inside
    // kNetVisibleFreshMs). 0 on host and whenever the scan isn't running.
    int visibleNetworkCount() const;
    // Load the SD-backed network ledger (real-network history + sighting counts)
    // from `path`. Called once at boot by the platform tier after the SD card
    // mounts; a missing/unavailable path just leaves the ledger empty (works as a
    // session-only RAM tracker for that boot — see NetworkLedger's own comment).
    void loadNetworkLedger(const char* path);
    // Lifetime count of distinct WPA handshakes captured (SHAKES on CFG). Deduped
    // by BSSID so a repeat capture of a known network never re-credits.
    int handshakesSeen() const { return handshakesSeen_; }
    // Register a handshake the device-tier promiscuous capture just observed +
    // wrote to the .pcap. Always drives the AuditCapture SM hot (a capture DID
    // happen), but only counts + persists once per distinct BSSID. Returns true if
    // newly-credited. `bssid` is 6 bytes. PASSIVE capture only — no deauth/inject.
    bool registerHandshake(const uint8_t* bssid);
    // Read-only query (no mutation, no SM drive) of the same ledger
    // registerHandshake() dedups against — a network already fully captured in a
    // prior session. The device-tier capture sniffer (net_capture.h) checks this
    // BEFORE opening a .pcap file for a BSSID, so a repeat walk past a known AP
    // never mints another file (the fix for the pcap-blowup: a file used to
    // be created once per arm/seal radio session regardless of whether it held
    // anything new). `bssid` is 6 bytes.
    bool handshakeAlreadyCaptured(const uint8_t* bssid) const;
    int allyBuffBattlesLeft() const { return allyBuffBattlesLeft_; }  // friendly-visit buff
    bool lockoutActive() const { return lockoutActive_; }
    Nav nav() const { return nav_; }
    CfgScreen cfgScreen() const { return cfgScreen_; }  // active CFG L3 (tests/inspection)
    // Which QR step is showing. The steps run in the order they happen: join the
    // AP, set up the home network (only while none is stored), open the 'Pedia —
    // so the index of the last one depends on netProvisioned() (pediaQrPages).
    int pediaQrPage() const { return pediaQrPage_; }

    // True while either QR page is showing — the 'Pedia AP steps or the USB
    // flasher's address. The device tier reads this to keep the panel AWAKE (a code
    // is read by a phone, which takes longer than the idle sleep window); the engine
    // likewise suspends its 5s menu auto-defocus here. Both resume normally the
    // moment the player exits the page (C).
    bool qrScreenActive() const {
        return nav_ == Nav::Detail && (cfgScreen_ == CfgScreen::PediaQr ||
                                       cfgScreen_ == CfgScreen::UpdateQr);
    }
    // True while the UPDATES screen is showing — its row list, the install confirm,
    // or a job's progress. The screen is a WAIT: a check associates and fetches on
    // the network's clock, and the verdict it leaves behind is the whole reason the
    // operator opened it. On the standard 5s budget the menu collapses out from under
    // a running job, or throws the answer away seconds after it lands, so this takes
    // the long hands-off budget (kRadioScreenDefocusMs) instead.
    //
    // Deliberately NOT part of radioScreenOpen(): that predicate also ARMS a radio,
    // and opening a screen must never put this device on a network — only a job the
    // operator started does that (netConnectWanted).
    bool updateScreenOpen() const {
        return nav_ == Nav::Detail && cfgScreen_ == CfgScreen::Update;
    }
    // True while the HackerTag arcade editor is open. Composing a tag one A-press
    // per letter has real pauses in it, and the 5s auto-defocus would throw the
    // half-typed buffer away mid-thought — so the editor holds the menu open the
    // same way the QR page does, and releases it on save or back-out.
    bool tagEditorActive() const {
        return nav_ == Nav::Detail && cfgScreen_ == CfgScreen::HackerTag;
    }
    // Cache-yield reveal inspection (tests): what the last-opened cache
    // produced, valid while nav() == Nav::CacheYield.
    int cacheYieldItemCount() const { return cacheYieldItemCount_; }
    int cacheYieldBits() const { return cacheYieldBits_; }
    // Bits cost of a Defrag at the pet's current stage (0 on the egg).
    int defragCost() const {
        return pet_ ? kDefragCostByStage[static_cast<int>(pet_->stage)] : 0;
    }
    const CreatureDef* pet() const { return pet_; }   // null while the save is empty (Hatch)
    // Where the habitat is currently standing its occupant, in logical px off the
    // shelf anchor (core/model/idle_wander.h). drawHabitat draws from this; it is
    // exposed so the resting motion can be inspected without a framebuffer.
    const IdleWander& petWander() const { return petWander_; }
    // How many ambient copies walk the habitat beside the pet right now. Purely
    // presentational and entirely derived — Combatant::wormReplicas is combat state,
    // wiped at the end of every fight, so a worm at home has no replicas to draw and
    // needs its own idle notion of "there is never just the one".
    //
    // It is a question the habitat ASKS rather than a line it tests for, so the
    // Worm's exception lives here on the model side and drawHabitat stays a renderer.
    // Everything that isn't a raised worm answers 0.
    int idleCompanionCount() const;
    // Where companion `i` (0..idleCompanionCount()-1) is standing, same units as
    // petWander(). Each keeps its own stream so the group never marches in step.
    const IdleWander& companionWander(int i) const { return companionWander_[i]; }
    // DISK DECRYPTION (the Ransomware line's hatch minigame) — the code-breaking board,
    // played once the instant the egg is laid, exactly as the Clutch and the Isolation
    // Protocol are. Cracking the key halves what is left of the incubation clock;
    // running out of attempts costs only that bonus. Rules: core/model/disk_decryption.h.
    bool inDecryption() const { return nav_ == Nav::Decryption; }
    const DiskDecryption& decryption() const { return decryption_; }
    // THE DECRYPTOGRAM (game_cryptogram.cpp) — the quote board, launched by cashing a
    // Decryptogram in at the Hacker VAULT and, once kQuoteArcadeUnlockWins quotes are
    // solved, from its own arcade cabinet. Rules: core/model/cryptogram.h; the pool:
    // core/content/content_quotes.h.
    bool inCryptogram() const { return nav_ == Nav::Cryptogram; }
    const Cryptogram& cryptogram() const { return cryptogram_; }
    // The roster row (content_quotes.h index) the board is playing, or -1.
    int cryptogramQuote() const { return cryptogramQuote_; }
    // This device's standing with the quote on roster row `i`. Out-of-range, and any
    // row whose wire is past kQuoteWireCap, read Hard — never played.
    CryptogramTier quoteTier(int i) const;
    // Quotes SOLVED, which is what gates the arcade cabinet and what decides whether
    // the VAULT still has a first-solve prize to offer.
    int quotesSolved() const;
    // Whether the pool has an unsolved, gate-passing quote left. False means a cashed
    // Decryptogram replays a solved one for Bits only.
    bool quotesLeftToWin() const;
    // Is this quote offerable at all — its achievement gate earned, and its solved
    // standing matching what the caller is after. The one place both eligibility axes
    // are answered, so the VAULT and the cabinet can never disagree about the pool.
    bool quoteEligible(int i, QuotePick pick) const;
    // Line-select modal: true while the player is choosing which egg line
    // to lay (fires at an empty save when >1 line is unlocked). `lineSelectRow()` is
    // the current cursor into the unlocked egg lines.
    bool inLineSelect() const { return nav_ == Nav::ModalLineSelect; }
    int lineSelectRow() const { return lineSelectRow_; }
    // The egg lines offered at line-select right now: every content line whose unlock
    // gate is satisfied (eggLineUnlocked). Ransomware is always on; Phishing unlocks
    // once DEEPWEB_DEPTH_8 is earned. The line-select modal (input/render/startHatch)
    // reads THIS, not the raw registry list, so a locked line never appears.
    std::vector<const EggLineDef*> availableEggLines() const;
    bool eggLineUnlocked(const EggLineDef* line) const;
    // Per-Process deep-dive gate on the egg's random hatch pool (rollHatchProcess).
    // Most Process pets are always in their line's pool; a few are DeepWeb-depth
    // rewards — Phishlet only joins the Phishing pool once DEEPWEB_DEPTH_64 is earned.
    bool hatchProcessUnlocked(const CreatureDef* proc) const;
    // Boot-Sector incubation (redesign): the freshly laid egg sits at idle as
    // the Boot-Sector creature, decrypting over kBootHatchMs. `inEggPhase()` is true
    // while it's still an unhatched egg (Boot stage + a live incubation clock);
    // `bootHatchRemainMs()` is the remaining clock (tests/UI).
    bool inEggPhase() const {
        return pet_ && pet_->stage == Stage::BootSector && bootHatchRemainMs_ > 0;
    }
    // Can the hatch REVEAL be played on demand? The home stretch of ANY incubation —
    // every line's minigame is played at lay-time now, so the last stretch of the clock
    // is the one thing they all share, and the chord cracks the shell there and then so
    // the 8-frame hatch one-shot is something the player watches rather than something
    // that happens off-screen.
    bool hatchRevealReady() const {
        return inEggPhase() && bootHatchRemainMs_ <= kHatchRevealMs;
    }
    // "The egg can be cracked open now" — what the idle prompt advertises with the
    // flashing Exploit pip, and what the chord acts on.
    bool eggCrackable() const { return hatchRevealReady(); }
    int hatchRevealFrame() const;   // the one-shot's current frame (tests/render)
    uint32_t bootHatchRemainMs() const { return bootHatchRemainMs_; }
    // Shaves `ms` off the incubation clock (floors at 0); a no-op outside the egg
    // phase. Walk/network steps use this to advance a laid egg, and it's also the
    // seam dev_config.h's DEV_EGG_TIMER_MS uses to fast-forward a fresh boot.
    void accelerateEggHatch(uint32_t ms);

    // --- Clutch Pick (the Phishing line's hatch minigame) ------------------
    // A Phishing egg is laid into a raft of identical decoys, and only the live
    // one's swimmer animates — motion is the whole tell, so the puzzle survives
    // grayscale. The player halves the clutch kEggPickRounds times (A = left/top,
    // C = right/bottom, B = commit); finishing with the live egg still inside
    // halves the incubation clock, missing it costs nothing but the bonus.
    // Geometry, logic and draw all live in game_eggpick.cpp.
    static constexpr int kEggPickRounds = 3;
    static constexpr int kEggPickCols = 8;   // the clutch grid BG_EGG_CLUTCH bakes
    static constexpr int kEggPickRows = 4;
    bool inEggPick() const { return nav_ == Nav::ModalEggPick; }
    int eggPickRound() const { return eggPickRound_; }        // halvings committed, 0..kEggPickRounds
    int eggPickTargetSlot() const { return eggPickTarget_; }  // the live egg's slot (tests)
    bool eggPickResolved() const { return eggPickResolved_; } // all rounds in -> the reveal is up
    bool eggPickWon() const { return eggPickWon_; }
    // True once the live egg's slot is outside the surviving span — i.e. the run is
    // already lost, even though the remaining rounds still play out.
    bool eggPickTargetInSpan() const;

    // --- GAMES (the arcade) ------------------------------------------------
    // The same minigames the hatches and the Defrag run, off their stakes: a cabinet
    // starts one with nothing riding on it and pays a flat participation reward plus
    // a skill bonus (content_arcade.h; game_arcade.cpp is the whole mechanism). The
    // arcade never reimplements a game — arcadeRun_ is the only thing that tells a
    // running minigame it is being played for fun, and it changes the PAYOUT and the
    // pacing, never the rules.
    bool inArcadeRun() const { return arcadeRun_; }
    // Lifetime tallies per roster row (content_arcade.h's index order). Out-of-range
    // reads 0, so a caller iterating a roster that shrank can't fall off the array.
    int arcadePlays(int i) const {
        return (i >= 0 && i < kArcadeMaxCabinets) ? arcadePlays_[i] : 0;
    }
    int arcadeWins(int i) const {
        return (i >= 0 && i < kArcadeMaxCabinets) ? arcadeWins_[i] : 0;
    }
    // The best score a cabinet has ever been played to. Only meaningful where a run
    // HAS a score — a win-or-lose cabinet (the Clutch) reports 0 forever, which is why
    // the cabinet page shows this only when the roster row scores incrementally.
    int arcadeBest(int i) const {
        return (i >= 0 && i < kArcadeMaxCabinets) ? arcadeBest_[i] : 0;
    }
    // ...by cabinet id, which is what an achievement row keys on (AchSeries::
    // ArcadeCabinetBest) — the roster is free to reorder, so a wire-stable question has
    // to be asked by id rather than by index.
    int arcadeBestById(const char* id) const;
    ArcadeDifficulty arcadeDifficulty() const { return arcadeDifficulty_; }
    // The cabinet list's cursor, as a ROSTER index (content_arcade.h's order) rather
    // than a row position — the two differ whenever a cabinet is locked out of the list.
    int arcadeRow() const { return arcadeRow_; }
    // Is cabinet `i` in the GAMES list right now (ArcadeUnlock)? A locked cabinet is
    // absent, not greyed. Shared by the A-cycle, the render loop and B, so the list can
    // never show a row the cursor skips or start one it never drew.
    bool arcadeCabinetOffered(int i) const;
    // The roster indices of every offered cabinet, in table order. Returns the count.
    int arcadeOfferedRows(int* out, int max) const;

    // --- Isolation Protocol (the Worm line's hatch minigame) ---------------
    // A Vermicell egg turns the worm inside it loose in a quarantine buffer: A steers
    // left, C steers right, and every byte it swallows is kIsolationDotMs off the
    // incubation clock. Crashing banks what it earned; eating the WHOLE clock finishes
    // the protocol clean, which hatches on the spot and fires WORM_WHISPERER. The rules
    // are core/model/isolation.h; the screen and the payout are game_isolation.cpp.
    bool inIsolation() const { return nav_ == Nav::Isolation; }
    const Isolation& isolation() const { return isolation_; }

    // --- CHROMATOPHORE (the Metamorphic line's hatch minigame) -------------
    // A Polystaria egg rehearses the only defence its line has: the water under it
    // takes one of kChromaSkins colours, A/B/C wear one skin each, and a sweep crosses
    // on a shrinking clock. Every pass made in the right skin — and settled, since a
    // repaint takes kChromaSettleMs — is kChromaPassMs off the incubation clock; being
    // caught in the wrong one ends the run with what it already earned. The rules are
    // core/model/chromatophore.h; the screen and the payout are game_chroma.cpp.
    bool inChroma() const { return nav_ == Nav::Chroma; }
    const Chromatophore& chroma() const { return chroma_; }
    // Who the board is actually painting. The pet if its family wears borrowed colours
    // (CreatureLine::wearsBorrowedColours) — which is every route in from a hatch,
    // since the egg on the shelf IS that pet — and otherwise the Metamorphic egg, so
    // an arcade cabinet played by a Ransomware pet still has a plausible subject
    // rehearsing on it. nullptr only if the roster somehow holds neither.
    const CreatureDef* chromaSubject() const;

    int cursor() const { return cursor_; }      // focused carousel slot 0..7
    UiMode uiMode() const { return uiMode_; }
    void setUiMode(UiMode m) { uiMode_ = m; dirty_ = true; }
    void cycleUiMode();
    // Screen-brightness level: a 0-based backlight index (0..kBrightnessLevels-1),
    // persisted (save v14). The device tier reads this to drive the backlight PWM; the
    // host tier ignores it. setBrightness clamps + persists.
    int brightness() const { return brightness_; }
    void setBrightness(int level);
    bool inEvolve() const { return nav_ == Nav::ModalEvolve; }
    // STAT readout (all stages): time remaining until the next evolution boundary.
    // For an unhatched egg the "next evolution" IS the hatch, so this returns the
    // incubation clock; at a terminus (Daemon, no successor) hasNextEvolution() is
    // false and evolveRemainMs() is 0 (STAT shows MAX). Counts real elapsed time.
    bool hasNextEvolution() const;
    uint32_t evolveRemainMs() const;
    // The care signal dominating this stage — the key into the evolution routing
    // tables. Balanced on a tie / no interactions yet.
    DominantSignal dominantSignal() const;
    bool inCSF() const { return nav_ == Nav::ModalCSF; }
    const std::vector<SaveRecord>& records() const { return records_; }
    int recordCount() const { return static_cast<int>(records_.size()); }

    // --- Persisted identity / lifetime (STAT footer, ARCH, CFG) -------------
    int generation() const { return generation_; }          // active pet's generation
    int petsRaised() const { return petsRaised_; }          // lifetime eggs hatched
    uint32_t lifetimeUptimeMs() const { return uptimeBase_ + nowMs_; }
    uint32_t lifetimeSteps() const { return lifetimeSteps_; }  // exploration
    const char* hackerTag() const { return hackerTag_; }
    // The CURRENT serialized save size — what a persistSave() right now would
    // actually write. Diagnostic-only (the boot-line print, main.cpp): lets a
    // dev watch it against the save partition's known capacity as content
    // grows, rather than finding out via a silent NOT_ENOUGH_SPACE write failure.
    size_t saveBlobSizeBytes() const { return serializeSave(captureSave()).size(); }
    // Web 'Pedia rename ("one safe write"): validate `s` is
    // 1..kHackerTagMax chars, every char in [A-Z0-9_] (the same alphabet the
    // on-device arcade editor cycles through), then commit + persist. Returns
    // false (no mutation) on an invalid tag.
    bool setHackerTag(const char* s);
    const std::vector<SaveStoredPet>& rack() const { return rack_; }
    int rackCount() const { return static_cast<int>(rack_.size()); }

    // Zone-completion Titles --------------------------------
    // Player-level, persisted (survive a pet reset, like the sector-clear flags).
    // A cleared sector's Title unlocks; one may be equipped for display.
    bool titleUnlocked(int sector) const {
        return sector >= 0 && sector < kAreaCount &&
               (titlesUnlocked_ & (1u << sector)) != 0;
    }
    int titlesUnlockedCount() const;
    int equippedTitle() const { return equippedTitle_; }   // sector index, or -1
    // The equipped Title's display name, or "NONE" when nothing is equipped.
    const char* equippedTitleName() const;

    // Crew + home network (game_crew.cpp) ----------------------------------
    // Player-level, persisted (survive a pet reset, like the Titles above). The HOME
    // NETWORK is the network the operator has designated as theirs to defend; holding
    // one is the precondition for belonging to a crew (joinCrew refuses without it).
    int crewIndex() const { return crewIndex_; }            // kCrews row, or -1
    const CrewDef* activeCrew() const { return crew(crewIndex_); }
    // The CREW screen's four views, deepest last. Enlisting is the one piece of player
    // IDENTITY this device asks you to author, so the screen gives it room rather than
    // stacking a roster into one list: the Hub states who you are and offers the two
    // sides, a side opens its own roster, and a crew opens a page of its own. `Picker`
    // is the home-network chooser — a modal over the Hub rather than a step past it,
    // because designating turf is a precondition for all of this and not part of the
    // browse. Public so the render path and the gates can name a view; the transitions
    // are Game::onHackerCrew's alone.
    enum class CrewView : uint8_t { Hub, Picker, Team, Detail };
    CrewView crewView() const { return crewView_; }
    bool hasHomeNetwork() const { return homeNetworkKey_ != 0; }
    uint64_t homeNetworkKey() const { return homeNetworkKey_; }
    const char* homeNetworkName() const { return homeNetworkName_; }
    // Designate `key` (a packed 48-bit BSSID, as Game::registerNetwork builds them)
    // as the home network. An explicit pick, replacing any earlier inference from
    // sighting counts. key 0 clears it (and any crew that depended on it).
    void setHomeNetwork(uint64_t key, const char* name);
    // Enlist in kCrews row `index`. Returns false — changing nothing — without a home
    // network, or for an out-of-range row.
    bool joinCrew(int index);
    void leaveCrew();

    // One offer in the home-network picker: a network the operator can claim. Every
    // row is a network the radio can hear RIGHT NOW (visibleNetworks_) — you designate
    // the turf you're standing on, so a network you can't currently see is not an
    // offer. The SD-backed ledger contributes no rows; it only supplies `count`, the
    // evidence that says which of the networks in front of you is the familiar one.
    struct CrewNetRow {
        uint64_t key = 0;
        const char* name = "";
        uint32_t count = 0;   // ledger sighting tally; 0 = never resolved by a walk yet
    };
    // Fills `rows` with the in-range offers, most-sighted first, capped at `max`.
    // Returns the count written. Read-only — the picker's render + input and tests all
    // walk the same list.
    int crewNetworkRows(CrewNetRow* rows, int max) const;

    // Achievements (save v40) --------------------------------------------------
    // Player-level (survive a pet reset, like the Titles above). The catalogue is data —
    // content_achievements.cpp — and MOST rows need no code here at all: a sweep compares
    // each row's `series` progress (achValue) against its goal every tick. Only the rows
    // whose event leaves no counter behind (AchSeries::Event) are fired by hand, from the
    // site where they happen, by id: see the `ach::` id constants in
    // content_achievements.h so a typo is a compile error rather than a silent no-op.
    //
    // Idempotent: setting an already-earned row does nothing, so a trigger site (and the
    // per-tick sweep) can call freely. Unlocking pays the row's `rewards` and queues the
    // home-screen banner (achBanner below) — the only way an unlock reaches the player,
    // since the device has no achievement browser.
    void unlockAchievement(const char* id);
    // The MIRROR: the pet facing its own species. Called from every seam that can seat a
    // real species opposite the player — the LINK duel and the arena — so the rule for
    // what counts as a mirror lives in one place rather than once per screen.
    void noteMirrorMatch(const char* opponentCreatureId);
    bool hasAchievement(const char* id) const;
    bool hasAchievement(const AchievementDef& d) const { return achBit(achEarned_, d.wire); }
    // The player's live progress toward `d` (the numerator of its ladder). Paired with
    // achievementGoal(d) (app/game_achievements.h), which owns the denominator.
    int achValue(const AchievementDef& d) const;

    // The row whose banner is on screen right now, or nullptr. Set only while the pet
    // face is idle — the player has to actually be looking at the home screen for the
    // announcement to count as seen, so nothing is missed by being elsewhere.
    const AchievementDef* achBanner() const;
    // How many unlocks the CURRENT banner is speaking for. 1 = the named row above; >1 =
    // a burst too long to parade one at a time (a firmware update that retro-awards a
    // whole back catalogue), collapsed into one "N ACHIEVEMENTS" banner.
    int achBannerCount() const { return achBannerCount_; }
    // The egg line `d` UNLOCKS, or nullptr for a row that unlocks none. Content-derived
    // rather than a flag on the achievement: an egg line already names the achievement
    // that earns it (EggLineDef::gatedBy), so asking the roster the question backwards
    // means a line added later gets the treatment below for free and no row can be
    // marked wrong. The one caller that matters is the banner: an unlock that has put a
    // NEW KIND OF EGG in the hatch menu is the only announcement on the device with
    // something for the player to go and DO, so it is the only one that waits for them.
    const EggLineDef* achEggLineUnlocked(const AchievementDef& d) const;
    // Is the banner on screen one of those, i.e. holding until a button clears it? A
    // timed banner retires itself on the heartbeat; this one cannot, so input has to.
    bool achBannerHeld() const;
    // Clear a held banner (marking it announced). Any button does this on the home
    // screen, and the press is spent doing it.
    void dismissAchievementBanner();
    // Earned-but-not-yet-announced rows still waiting. Tests read it; so does the burst
    // check that decides between a named banner and a summary.
    int achPendingNotify() const;
    // A creature FACED IN COMBAT but never raised. Seeing is a fight, not a
    // glimpse: nothing a menu, hint or cinematic shows counts. Today the only
    // combatant resolved from a real species is a duel opponent (startPvpBattle) —
    // wild malbeasts, sub/area bosses and Sim dummies are name-only CombatEnemy
    // specs from pools disjoint from the roster, and the wild half has its own
    // roster-keyed masks (malbeastSeen below) written on this same fight rule.
    // Idempotent; a creature already RAISED is never added — "hatched" always wins
    // over "seen" (pedia_state.cpp's pets{} map).
    void markCreatureSeen(const char* id);
    bool creatureSeen(const char* id) const;
    // The device's running tally of species the operator has RAISED — recorded at
    // every point a creature becomes this device's pet (hatch, evolution, an ARCH
    // Deploy) and never removed, so it outlives the pet itself. This is what the
    // 'Pedia's "hatched" reveal and FULL_PEDIA_L1 read: what's currently HELD is a
    // snapshot, and a species evolved past leaves nothing holding it. Idempotent.
    void markCreatureRaised(const char* id);
    bool creatureRaised(const char* id) const;
    // How many DISTINCT species this device has raised (PROFILE's SPECIES row).
    int speciesRaised() const { return static_cast<int>(raisedCreatures_.size()); }
    // The fixed wild-malbeast roster's (combat.h kWildMalbeastIds) seen/defeated
    // state, set by a real wild encounter roll (seen) / a won live wild fight
    // (defeated) — see Game::startEncounter / Game::finishCombat.
    bool malbeastSeen(int idx) const {
        return idx >= 0 && idx < kWildMalbeastCount &&
               (malbeastSeenMask_ & (1u << idx)) != 0;
    }
    bool malbeastDefeated(int idx) const {
        return idx >= 0 && idx < kWildMalbeastCount &&
               (malbeastDefeatedMask_ & (1u << idx)) != 0;
    }

    // Lifetime boss rounds won (save v40). Each round of a gauntlet counts — beating a
    // three-boss gauntlet is three bosses defeated, which is what it felt like.
    int bossWins() const { return bossWins_; }
    // Lifetime DEFRAG minigame boards cleared (save v44). Only the Stacker variant
    // counts — a Quick or Tool defrag is bought, not played.
    int stackerWins() const { return stackerWins_; }
    // The three v56 lifetime tallies, on the same footing as the two above.
    int tourneyWins() const { return tourneyWins_; }   // ROCK THE DOCK brackets taken
    int pvpWins() const { return pvpWins_; }           // LINK duels won
    int mergesCooked() const { return mergesCooked_; } // dishes cooked at the MERGE HUB
    // Has this item EVER been in the bag? (save v40) — the collection tally behind the
    // cuisine/rarity achievements, and a truer answer than "is it in the bag now" for
    // anything asking what the player has seen. Written by a sweep over the live
    // inventory, so every path that grants an item is covered by construction.
    bool itemCollected(const char* id) const;
    int itemsCollected() const { return static_cast<int>(collectedItems_.size()); }
    // Has the METHOD for `outputId` been won? (save v31) — the kitchen's other axis:
    // meeting a dish and knowing how to cook it are independent, and a recipe is only
    // ever won off a Decryptogram, never bought. Keyed by the dish because that is the
    // recipe table's own key (content_recipes.h recipeIndexByOutput). Out-of-line
    // (game_merge.cpp): that table is engine-private, so this header cannot see it.
    bool recipeKnown(const char* outputId) const;
    // The deepest DeepWeb Dive this SPECIES has ever reached on this device (save v40),
    // 0 if it never dived. Per-species rather than per-pet, so the record outlives the
    // individual: the pet's own in-progress best is bestDeepWebDepth() above.
    int speciesDeepestDive(const char* creatureId) const;
    // The deepest dive on the device, by anyone — the max over every species record.
    int deepestDiveEver() const;

    // --- Dev shortcuts (host key / device dev gesture / tests) --------------
    // Wipe back to a fresh empty save and re-enter the Decryption Hatch, so the
    // hatch + raising loop can be retested without reflashing / relaunching.
    void resetToHatch();
    // Clear the player-level state a pet reset deliberately leaves standing — the
    // 'Pedia reveal tiers, rank/discovery, Titles, world progress, Rig Shop
    // upgrades, crew, and the radio consents. Paired with resetToHatch() by the
    // Factory Reset's WIPE EVERYTHING scope; nothing else calls it, because every
    // other reset path is scoped to one pet on purpose.
    void wipeDeviceProgress();
    // Force the Evolution boundary now, bypassing the time/care gate (the stub
    // trigger is /TBD). No-op unless the pet has a defined successor.
    void debugTriggerEvolution();
    // Seed the shared LCG (rng_) so rng-driven draws — the hatch pool, the Trojan
    // cross-line divert roll — are deterministic in a test / headless run.
    void debugSeedRng(uint32_t seed) { rng_ = seed; }
    // Inject a frozen pet into the ARCH rack (tests / headless screenshots of a
    // populated rack — the real path is the Store action). No-op if the rack is
    // full or the id is unknown.
    void debugSeedRack(const char* creatureId);
    // Launch a combat directly (tests / headless screenshots / dev). `live` picks
    // the stakes; the Sim-Battle path is the in-game one (startSimBattle, safe).
    // `lethal` builds an unbeatable enemy so a loss is deterministic (stakes test).
    void debugStartCombat(bool live, bool lethal = false);
    // Preset the lifetime networks-seen counter + its v1 dedup pool (tests /
    // dump_frame): marks the first `n` pool slots seen and recomputes Hacker
    // Rank silently — scaffolding to reach a rank/sector-unlock state without
    // grinding real Wi-Fi events, NOT a real rank-up (no celebration/reward).
    void debugSetNetworksSeen(int n);
    // Credit boss rounds without fighting them (tests / headless runs) — reaching the
    // deeper boss rungs honestly would mean walking a whole gauntlet ladder per rung.
    void debugAddBossWins(int n) { bossWins_ += n; markSaveDirty(); }
    // Credit cleared DEFRAG boards without playing them (tests / headless runs). The
    // deeper rungs are dozens of hand-played runs, each of which also spends Bits.
    void debugAddStackerWins(int n) { stackerWins_ += n; markSaveDirty(); }
    // Record a species' DeepWeb depth without diving (tests / headless runs), so the
    // depth ladders can be exercised without a several-hundred-fight run.
    void debugRecordSpeciesDive(const char* creatureId, int depth) {
        recordSpeciesDive(registry_.creature(creatureId), depth);
    }
    // Seed the network ledger directly, bypassing the scan → EXPL-Wi-Fi-event resolve
    // path — the only way a tool or test can populate the CREW home-network picker
    // without walking. `count` is the sighting tally the picker sorts on.
    void debugSeedNetworkLedger(uint64_t key, const char* name, int count);
    // Put a move in the pet's OWNED set (tests / dump_frame): the real earn path is
    // whatever eventually grants one, and there is none yet — this is how a scene puts
    // a kit in a known state, e.g. to show the outro a rival with no new move gets.
    void debugGrantMove(const char* id) { moveLoadout_.grant(id); }
    // Unlock a zone-completion Title without clearing its sector (tests /
    // dump_frame): the real path is a sector clear (unlockTitle). Auto-equips the
    // first Title, exactly like the real grant. No-op for an out-of-range sector.
    void debugUnlockTitle(int sector) { unlockTitle(sector); }
    // The Factory Reset scope A cycles on its screen (0 = Reset Pet, 1 = Wipe
    // Everything), so a gate can exercise executeFactoryReset() without holding B.
    void debugSetFactoryScope(int scope) { factoryScope_ = scope; }
    // Use the inventory item with `id` as if selected in ITEMS (tests): runs the
    // real useItem() path so its full effect + consume + log fire. No-op if the
    // id is unknown. Add the item to inventory() first.
    void debugUseItem(const char* id);
    // Decrypt a sealed cache directly via the real openSealedCache reward path (tests):
    // caches now decrypt from the Hacker VAULT, not pet-side ITEMS. No-op on unknown id.
    void debugOpenCache(const char* id);
    // Buy the next b Reduce-Explore-Frag tier via the real buy path (tests).
    void debugBuyFragReducer() { buyRigUpgrade(kRigRowFragReduce); }
    // Buy the next c Reduce-Explore-Frag-TRIGGER tier via the real buy path (tests).
    void debugBuyFragTrigger() { buyRigUpgrade(kRigRowFragTrigger); }
    // Buy the d ITEMS type-tabs / e VAULT bulk-open upgrades via the
    // real buy path (tests).
    void debugBuyItemTabs() { buyRigUpgrade(kRigRowItemTabs); }
    void debugBuyBulkOpen() { buyRigUpgrade(kRigRowBulkOpen); }
    // Buy the ITEMS type-picker upgrade via the real buy path (tests).
    void debugBuyItemPicker() { buyRigUpgrade(kRigRowItemPicker); }
    // Buy the MERGE HUB slot via the real buy path (tests / dump_frame). The recipes
    // cooked in it are won, not bought — debugWinRecipe below.
    void debugBuyMergeHub() { buyRigUpgrade(kRigRowMergeHub); }
    // Any rig row by index, through the real buy path — for a test that sweeps the
    // whole storefront rather than naming one row.
    void debugBuyRigRow(int row) { buyRigUpgrade(row); }
    // Draw the Bandwidth pool down without staging a fight to spend it, so a test can
    // watch it REGENERATE (which is the only way to observe a pet's regen interval).
    void debugSpendBandwidth(int n) {
        bandwidth_ -= n;
        if (bandwidth_ < 0) bandwidth_ = 0;
    }
    // Buy the g/h Auto Backup / Continuous Auto-Backup unlocks via the real buy path
    // (tests) — the two rows that arm the Backup Drive death-save for free.
    void debugBuyAutoBackup() { buyRigUpgrade(kRigRowAutoBackup); }
    void debugBuyContinuousBackup() { buyRigUpgrade(kRigRowContinuousBackup); }
    // Buy the next MOD STORAGE tier via the real buy path (tests) — what raises how many
    // spare copies of one mod the pool will hold (modStorageCap).
    void debugBuyModStorage() { buyRigUpgrade(kRigRowModStorage); }
    // Hand back to the idle habitat exactly as a resolved explore event does (tests) —
    // the Continuous Auto-Backup / auto-defrag seam, without driving a whole random
    // event to reach it.
    void debugReturnToExplore() { returnToExplore(); }
    // `i` indexes kMergeRecipes; the row it GRANTS is that recipe's own rigRow, so this
    // keeps working when either table grows. A grant, not a purchase — no Bits path
    // reaches a recipe any more; the Decryptogram's prize ladder is the only one.
    // Out-of-line (game_merge.cpp) because the recipe table is engine-private.
    void debugWinRecipe(int i);
    // The MERGE HUB craft, driven without the screen (tests / dump_frame). Same two
    // calls B on the hub makes: ask, then do.
    bool debugRecipeCraftable(int i) const { return recipeCraftable(i); }
    bool debugRecipeOwned(int i) const { return recipeOwned(i); }
    bool debugRecipeGrantable(int i) const { return recipeGrantable(i); }
    void debugCraftRecipe(int i) { craftRecipe(i); }
    // Set the ITEMS type filter directly (tests): the real path is the ITEMS hold-B
    // gesture (nextItemFilter); this exercises buildInventoryRows/drawItemsList
    // filtering without driving the tap/hold timing.
    void debugSetItemFilter(ItemFilter f) { itemFilter_ = f; dirty_ = true; }
    ItemFilter itemFilter() const { return itemFilter_; }
    // Which ITEMS L2 screen is showing, and the picker's tile cursor (tests / render).
    ItemsScreen itemsScreen() const { return itemsScreen_; }
    int itemPickRow() const { return itemPickRow_; }
    // The shared list cursor and the MOD picker's own, for the list-navigation gates:
    // A's repeat and C's backward walk are assertions about where a cursor ENDED UP,
    // which is the one thing a rendered frame states least directly.
    int listRow() const { return listRow_; }
    int modPick() const { return modPick_; }
    const char* modDetailId() const { return modDetailId_; }
    // Grant mods in tests without walking a boss drop or a cache roll.
    Loadout& debugLoadout() { return loadout_; }
    // Set the move picker's showAll flag directly (tests): the real path is its
    // own hold-B gesture; this exercises ownedMoveList's filtering without driving
    // the tap/hold timing.
    void debugSetMoveShowAll(bool v) { moveShowAll_ = v; dirty_ = true; }
    bool moveShowAll() const { return moveShowAll_; }
    // Fill every unlocked move slot and every mod slot (tests / tools/dump_frame.cpp).
    // A Daemon carrying a full kit is the state STAT's LOADOUT page has to WINDOW
    // rather than run down the screen, and it is otherwise only reachable by playing
    // a pet to the end of its line. Moves come from what the pet's own line owns,
    // matched to each slot's stamped kind; mods are placed without consuming a spare,
    // since what this stages is the display and not the economy.
    void debugFillLoadout();
    // Run the e bulk-open path directly on the item `id` (tests): the real
    // trigger is the VAULT hold-B gesture; this exercises openAllCachesOfRarity's
    // aggregation (consume-all-of-rarity + tally + Bits sum) without the hold timing.
    // No-op if `id` is unknown.
    void debugBulkOpenCaches(const char* id) {
        if (const ItemDef* d = registry_.item(id)) openAllCachesOfRarity(*d);
    }
    int bulkYieldCachesOpened() const { return bulkYieldCachesOpened_; }
    int bulkYieldBits() const { return bulkYieldBits_; }
    int bulkYieldTallyCount() const { return static_cast<int>(bulkYieldTally_.size()); }
    // The i'th tally entry's item id + count (tests). Empty id / 0 count if out of range.
    const char* bulkYieldTallyId(int i) const {
        return (i >= 0 && i < static_cast<int>(bulkYieldTally_.size()) &&
                bulkYieldTally_[i].def)
                   ? bulkYieldTally_[i].def->id : "";
    }
    int bulkYieldTallyCountAt(int i) const {
        return (i >= 0 && i < static_cast<int>(bulkYieldTally_.size()))
                   ? bulkYieldTally_[i].count : 0;
    }
    // Resolve a Defrag with a forced success/failure via the real resolveMaint() path
    // (tests) — a deterministic maint outcome (rollMaintSuccess is otherwise RNG-gated),
    // so a failed defrag's shielded +1 care mistake is assertable. `success=false` =
    // a failure (frag penalty + the shielded mistake).
    void debugResolveDefrag(bool success);
    // Advance the Stacker's run one beat without waiting out its real cadence — the seam
    // tests and dump_frame use to place the run deliberately instead of by stopwatch.
    void debugStepStacker() { stacker_.step(); }
    const Stacker& stacker() const { return stacker_; }
    // Set a quote's standing directly (tests / dump_frame): reach the eight-solve
    // arcade unlock, or a mid-ladder tier, without playing eight boards by hand. The
    // real paths are Game::settleCryptogram's win and its loss ratchet.
    void debugSetQuoteTier(int i, CryptogramTier t) { setQuoteTier(i, t); }
    // Open a SPECIFIC quote's board (tests / dump_frame). Skips only the ticket and the
    // roll — from here onButton drives the real board and the real settle, which is
    // where the ratchet and the prize are decided. Real path: the VAULT cash-in.
    void debugStartCryptogram(int quote, int extraReveals) {
        startCryptogram(quote, extraReveals);
    }
    // Enter a fresh Stacker board without paying for it (tests / dump_frame). Skips only
    // the purchase — from here onButton drives the REAL board and a finished run lands in
    // the real finishStacker, which is where the played-defrag tally and its two
    // shape-of-the-board achievements are decided.
    void debugStartStackerDefrag() { startStackerDefrag(); }
    // Set the Bits wallet directly (tests / economy-gated flows like the shop's
    // "not enough Bits" gate). Clamped at 0.
    void debugSetBits(int n);
    // Set a sub-area's durable clear / boss-unlock flag directly (tests / dump_frame):
    // scaffolding to reach a mid-ladder EXPL state without grinding the real
    // streak. No-op for out-of-range indices; the real paths are clearSubArea's streak
    // + FIGHT BOSS. Marks the save dirty so a follow-on persist captures it.
    void debugSetSubCleared(int area, int sub, bool v) {
        if (area >= 0 && area < kAreaCount && sub >= 0 && sub < kSubAreasPerArea) {
            subCleared_[area][sub] = v; markSaveDirty();
        }
    }
    void debugSetSubBossUnlocked(int area, int sub, bool v) {
        if (area >= 0 && area < kAreaCount && sub >= 0 && sub < kSubAreasPerArea) {
            subBossUnlocked_[area][sub] = v; markSaveDirty();
        }
    }
    // Set an AREA's cleared flag directly (tests): reach the "all areas cleared" state
    // (the DeepWeb Dive unlock) without grinding every gauntlet. Real path: area boss.
    void debugSetSectorCleared(int area, bool v) {
        if (area >= 0 && area < kAreaCount) { sectorCleared_[area] = v; markSaveDirty(); }
    }
    // The level stamped on the last rolled wild encounter (DeepWeb scaling test).
    int debugEncounterEnemyLevel() const { return encounterEnemy_.level; }
    // Arm the DeepWeb Dive directly (tests) — keeps its own all-areas-cleared guard.
    // Real path: EXPL → B on the "> DIVE" row.
    void debugStartDeepWebDive() { startDeepWebDive(); }
    // The armed Deep-Learning Module/Core depth step (1 = none armed). No production
    // reader outside the engine — the habitat draws it via game_render.cpp — so tests
    // read it here to assert the buff survives the dive it was armed for.
    int debugDepthMultiplier() const { return deepWebDepthMultiplier_; }
    // Arm explore-mode on a SPECIFIC sub-area directly (tests): once cleared sub-areas
    // are re-farmable the EXPL "first-selectable" row is ambiguous, so a test
    // that needs a particular frontier arms it here instead of A-cycling. Real path:
    // EXPL → B on the row. No-op for out-of-range indices.
    // Force the arena run to a terminal banner without playing a whole bracket out.
    // The two verdict screens are otherwise reachable only by winning or losing three
    // matches, which is not what a screen gate is asking about.
    void debugEndTourneyRun(bool champion) {
        if (!tourneyRunning()) return;
        tourneyPhase_ = champion ? TourneyPhase::Champion : TourneyPhase::Eliminated;
        if (!champion) tourneyAlive_ &= static_cast<uint8_t>(~(1u << tourneySlot()));
        // A champion run pays, exactly as the real third win does — including the two
        // shape achievements, which read the final's opponent BEFORE it is cleared
        // below. A hook that ended a run in the title but skipped the purse would be a
        // hook the purse could not be tested through.
        if (champion) awardTourneyPurse();
        tourneyOpponent_ = TourneyFighter{};
        dirty_ = true;
    }
    void debugArmExplore(int area, int sub) {
        if (area >= 0 && area < kAreaCount && sub >= 0 && sub < kSubAreasPerArea)
            startExplore(area, sub);
    }
    // Arm AUTO-PROGRESS, and set the armed sub-area's win streak (tests). Real path:
    // the A+C chord on the explore-control overlay, and ten wild wins. A test of the
    // STEP rule shouldn't have to grind the streak that triggers it — that grind is
    // covered on its own by test_explore_streak_unlocks_boss_then_clears.
    void debugSetAutoProgress(bool v) { autoProgress_ = v; }
    void debugSetExploreStreak(int v) { exploreStreak_ = v < 0 ? 0 : v; }
    // Launch a specific sub-area boss directly (tests): same rationale as
    // debugArmExplore — the EXPL "first-selectable" row is ambiguous once cleared subs
    // are re-farmable, so a test reaching the boss of an as-yet-uncleared sub triggers
    // it here. Real path: EXPL → B on a "> FIGHT BOSS" row.
    void debugFightSubBoss(int area, int sub) {
        if (area >= 0 && area < kAreaCount && sub >= 0 && sub < kSubAreasPerArea)
            startSubAreaBoss(area, sub);
    }
    // Grant combat XP directly through the real addCombatXp path (tests): banks the
    // XP and fires any level-ups (+1 random stat each), so the level/stat/curve logic
    // can be exercised without grinding real fights. Not a real reward source.
    void debugAddCombatXp(int xp) { addCombatXp(xp); }
    // MODS earn path (tests): roll an id from an area's mod loot table, and
    // grant a mod through the real earn path (rolls its per-instance equip-level gate).
    // Real path: boss clears / DeepWeb wins / Epic caches — same helpers, gated there.
    const char* debugRollAreaModId(int area) { return rollAreaModId(area); }
    void debugGrantMod(const char* id) { grantMod(id); }
    // Set the Bandwidth farming pool directly (tests): reach the depleted state
    // that switches farming from full-loot to the diminishing-returns curve without
    // grinding kBandwidthMax wins. Clamped to [0, bandwidthMax()].
    void debugSetBandwidth(int n) {
        bandwidth_ = n < 0 ? 0 : (n > bandwidthMax() ? bandwidthMax() : n);
    }
    // Buy a Rig Shop upgrade directly through the real path (tests / dump_frame).
    void debugBuyBandwidthUpgrade() { buyRigUpgrade(kRigRowBandwidth); }
    void debugBuyRackSlotUpgrade() { buyRigUpgrade(kRigRowRackSlot); }
    void debugBuyScrapingCluster() { buyRigUpgrade(kRigRowScraping); }
    void debugBuyDataMining() { buyRigUpgrade(kRigRowDataMining); }
    void debugBuyHungerXpRate() { buyRigUpgrade(kRigRowHungerXpRate); }
    void debugBuyHungerXpWindow() { buyRigUpgrade(kRigRowHungerXpWindow); }
    void debugBuyCombatXpPct() { buyRigUpgrade(kRigRowCombatXpPct); }
    void debugBuyCombatXpWindow() { buyRigUpgrade(kRigRowCombatXpWindow); }
    // Advance the pet clock directly through the real model (tests) — the same
    // path Game::tick() uses for Hunger decay + Passive XP Farming, without
    // needing a full tick() (nav/lockout/autosave etc).
    void debugTickHunger(uint32_t elapsedMs) { tickHungerAndAwardXp(elapsedMs); }
    // The live post-clear re-farm win count for a sub-area (tests).
    int debugSubRefarmCount(int area, int sub) const {
        if (area < 0 || area >= kAreaCount || sub < 0 || sub >= kSubAreasPerArea)
            return 0;
        return subRefarmCount_[area][sub];
    }

    // Post-encounter status readout inspection (tests): the
    // bandwidth/fragmentation before/after snapshot + shield flag applyCombatResult()
    // captured for the fight that just parked on Nav::PostEncounter.
    int debugPostEncBwBefore() const { return postEncBwBefore_; }
    int debugPostEncBwAfter() const { return postEncBwAfter_; }
    int debugPostEncFragBefore() const { return postEncFragBefore_; }
    int debugPostEncFragAfter() const { return postEncFragAfter_; }
    bool debugPostEncShielded() const { return postEncShielded_; }

    // Is the Backup Drive death-save currently armed (used and not yet spent in a
    // fight, nor lapsed)? Lazily checked against the deadline, like
    // Game::buildPlayerCombatant() itself — nothing actively clears it on expiry.
    // Read by the STAT BUFFS page and the idle-habitat buff badge (game_render.cpp).
    bool backupShieldArmed() const {
        return backupShieldUntilMs_ != 0 && lifetimeUptimeMs() < backupShieldUntilMs_;
    }

    // the non-Bits drop-chance scale (%) for a sub-area with `refarmCount`
    // post-clear wild wins — 100 decaying to a floor. Pure/static so it's unit-tested
    // directly and reused wherever a re-farm reward is rolled.
    static int refarmDropScalePct(int refarmCount);

private:
    // Rendering.
    void drawHabitat(Framebuffer& fb, int cursor) const;
    // The unlock announcement over the habitat — a no-op unless a banner is up.
    void drawAchievementBanner(Framebuffer& fb) const;
    void drawSubmenu(Framebuffer& fb) const;
    void drawDetail(Framebuffer& fb) const;
    void drawProcess(Framebuffer& fb) const;
    void drawStacker(Framebuffer& fb) const;
    void drawEvolve(Framebuffer& fb) const;
    void drawCSF(Framebuffer& fb) const;          // Critical System Failure
    void drawTrain(Framebuffer& fb) const;        // MOVES L3 (move picker / detail)
    void drawCombatScreen(Framebuffer& fb) const;
    // The ART a CamoTarget names (FX_CAMO), or null for nothing to wear. Rival answers
    // with the sprite standing opposite; Line answers with that line's own creature at
    // the wearer's stage, so a pet channelling Ransomware looks like a Ransomware of its
    // own tier rather than like that line's hatchling. Registry lookups only — the tick
    // calls this every beat, and the pixel scan that turns the answer into a ramp waits
    // for the draw.
    const SpriteData* camoSpriteForTarget(const CamoTarget& t, const Combatant& rival,
                                          Stage wearer) const;
    void drawEncounterScreen(Framebuffer& fb) const;
    void drawWifiScreen(Framebuffer& fb) const;
    void drawShopScreen(Framebuffer& fb) const;
    void drawPostEncounterScreen(Framebuffer& fb) const;

    // Navigation.
    void summonCursor(int slot);
    void enterSubmenu();
    void dropCursor();
    SubmenuId enteredId() const;                   // the slot table's id for cursor_
    // Whether a pet-face carousel slot is inert, for either reason a slot can be:
    //
    //   * Boot-Sector egg: while the pet is still an unhatched egg the care/combat
    //     slots (GAMES/EXPL/MAINT/MODS) are inert — there's nothing to equip,
    //     explore, maintain, or play with on an egg (the egg's own hatch minigame is
    //     not the arcade's). STAT + ITEMS stay live (ITEMS gates non-quest items
    //     separately, see itemUsable).
    //   * Demo build: CFG configures a device the web demo does not have
    //     (include/demo_config.h).
    //
    // Drives both the greyed carousel and the blocked submenu entry, so the two can
    // never disagree about what a slot does.
    bool slotLocked(SubmenuId id) const;

    // --- List navigation (game_listnav.cpp) ---------------------------------
    // The row-stepping half of the button contract, shared by every list on the
    // device so the three gestures behave identically wherever the player meets them:
    // A steps forward (held, it repeats), C steps BACKWARD (held, it backs out of the
    // list), and the row set each one walks is whatever the focused screen draws.
    //
    // listFocus reports which list — if any — is under the cursor right now, and is
    // the single gate all three gestures ask. A screen absent from it keeps the plain
    // A/B/C contract untouched: an inline confirm, a detail page, a modal and a
    // minigame all answer None, so C stays the instant tap-to-cancel there.
    enum class ListFocus {
        None, ItemsPicker, ItemsList, Arch, CfgList, CfgGroup, ModSlots, LoadoutHub,
        ModPicker, MovePicker, Expl, Maint, Arcade, HackerShop, HackerVault,
        HackerPeers, CrewHub, CrewTeam, CrewPicker,
    };
    ListFocus listFocus() const;
    // Move the focused list's cursor by `dir` (+1/-1), wrapping, skipping whatever
    // that list considers unselectable (ITEMS headers, unoffered SHOP rows, unlandable
    // EXPL rows). Inert when listFocus() is None.
    void stepFocusedList(int dir);
    // The A-repeat and the C-hold back-out, both driven off the held-button clocks.
    // Returns true if either fired and the screen needs a repaint.
    bool tickListNav();
    // Arm C's tap/hold on a list. Returns true if it claimed the PRESS (nothing visible
    // happens yet), false on a screen with no list, where C cancels on the press edge
    // exactly as it always has.
    bool listBackStep(const ButtonEvent& ev);
    // Resolve that press on the release edge: a tap cancels, a hold that already walked
    // the cursor backward does not.
    void listBackRelease();
    // The focused list's own C case, replayed — what a tap of C on it means.
    void leaveFocusedList();

    // Per-state input handlers.
    // The ITEMS type-picker (ItemsScreen::Picker): A steps the tile, B commits that
    // tile's filter and drills into the list, C leaves ITEMS entirely.
    void onItemsPicker(const ButtonEvent& ev);
    void onItemsList(const ButtonEvent& ev);
    void onItemsDetail(const ButtonEvent& ev);
    // Resolves the ITEMS hold-B gesture's SHORT-press half — called from onButton on
    // every B release. No-op unless bHeld_ is still armed (i.e. the hold never crossed
    // kItemFilterHoldMs, which fires the filter cycle itself via tick() and clears
    // bHeld_ first) AND we're still resting on an owned ITEMS list.
    void itemFilterReleaseB();
    // Drill into the focused row's item detail. Shared by B's immediate press (no
    // type-tabs upgrade) and by the tap half of the hold gesture above, so the two
    // paths can't drift on what "open this row" means. Builds the list itself rather
    // than taking it: an InvRow is items_screen.h's, and game.h names no renderer type
    // (the split ui_state.h's header comment describes).
    void openFocusedItem();
    void onMaintList(const ButtonEvent& ev);
    void onMaintAction(const ButtonEvent& ev);
    void onStacker(const ButtonEvent& ev);
    void onCfgList(const ButtonEvent& ev);
    void onCfgGroup(const ButtonEvent& ev);        // DISPLAY / RADIO row lists
    void onCfgDetail(const ButtonEvent& ev);
    void enterCfgScreen(CfgScreen target);         // open an L3, seeding its picker
    void leaveCfgScreen();                         // back to the group, else the list
    void drawCfg(Framebuffer& fb) const;          // L3 dispatch for the CFG screens
    void executeFactoryReset();                    // hold-B commit
    void onArchList(const ButtonEvent& ev);
    void onArchRecord(const ButtonEvent& ev);
    void archStoreActive();                        // active pet → rack → new egg
    void archDeployStored(int storedIdx);          // stored pet → active (slot-neutral)
    void archReleaseStored(int storedIdx);         // stored pet → gone (frees a slot, no reward)
    void noteRackDuplicates();                     // two of one species on the shelf → SECOND_INSTANCE
    bool archRowIsActive() const { return listRow_ == 0; }
    // Records (RETIRED/CORRUPTED) trail the active + rack rows and have no slot.
    bool archRowIsRecord() const { return listRow_ > static_cast<int>(rack_.size()); }
    // GAMES: the cabinet list (L2) -> one cabinet (L3) -> a real minigame -> payout.
    void onArcadeList(const ButtonEvent& ev);
    void onArcadeCabinet(const ButtonEvent& ev);
    void onArcadeResult(const ButtonEvent& ev);
    // Launch the focused cabinet's minigame with arcadeRun_ armed. The one place a
    // cabinet's ArcadeGameKind turns into behaviour.
    void startArcadeRun();
    // Bank a finished run and open the payout screen. `scoreMax` 0 means the cabinet
    // scores win-or-lose, which is what decides how the bonus half is worked out.
    void finishArcadeRun(bool won, int score, int scoreMax);
    // A paced minigame's step cadence under the current difficulty — `baseMs` scaled
    // by the dial, or `baseMs` untouched outside a cabinet run.
    int arcadeStepMs(int baseMs) const;
    void drawArcade(Framebuffer& fb) const;         // L2 cabinet list
    void drawArcadeDetail(Framebuffer& fb) const;   // L3 the focused cabinet
    void drawArcadeOutcome(Framebuffer& fb) const;  // the payout screen

    // --- Mid-play RULES overlay (ROCK THE DOCK's BRIEFING, generalised) ----------
    // A+C on any of the five game engines (Stacker/Isolation/Decryption/Cryptogram/
    // Clutch) opens a paged explainer of THAT engine's own rules and pauses it
    // underneath — the only route to one for the three that launch straight out of an
    // egg with no menu stop along the way at all (startHatchGame: Decrypt/Clutch/
    // Isolation land on their Nav directly). One flag rather than five: nav_ can only
    // be one of the five at a time, so nothing can ever have two open together, and
    // the content it reads is keyed by ArcadeGameKind rather than by cabinet row so it
    // reads the same whether the engine was reached from its cabinet, MAINT's Stacker
    // variant, or a hatch.
    bool gameBriefOpen_ = false;
    int gameBriefScroll_ = 0;
    bool gameBriefAvailable() const;         // is nav_ one of the five?
    ArcadeGameKind gameBriefKind() const;    // which one, as the content row's key
    std::vector<ProseRow> gameBriefRows() const;
    void openGameBrief();
    void closeGameBrief();
    void toggleGameBrief();   // the chord's job when it isn't Cryptogram's drop-letter
    // Handles B (scroll) / C or the chord (close) while the overlay is open and
    // returns true, so the caller returns immediately rather than also feeding the
    // same press to the game underneath. Returns false when the overlay isn't open.
    bool onGameBriefInput(const ButtonEvent& ev);
    void drawGameBrief(Framebuffer& fb) const;

    // MODS: the LOADOUT hub (L2) and the mod-slot half it fronts.
    void onLoadoutHub(const ButtonEvent& ev);
    void openLoadoutTab(LoadoutTab tab);        // hub row -> its page, seeding the page
    void leaveLoadoutTab();                     // any page -> back to the hub
    void onModsList(const ButtonEvent& ev);
    void onModPicker(const ButtonEvent& ev);
    void onModDetail(const ButtonEvent& ev);   // mod detail (read-then-act)
    void commitModEquip();                      // shared equip-or-confirm for a mod

    // MOVES: slot list (L2) -> move picker / Sim-Battle tier (L3) -> combat.
    void onTrainList(const ButtonEvent& ev);
    void onTrainDetail(const ButtonEvent& ev);     // dispatch MovePicker / MoveDetail / SimTier
    void onMovePicker(const ButtonEvent& ev);
    // The MOVES picker's own hold-B short-press half (the twin of itemFilterReleaseB):
    // a tap drills into the focused move's detail, and only a hold that never reached
    // kMoveFilterHoldMs gets here. No-op outside the picker.
    void moveFilterReleaseB();
    void onMoveDetail(const ButtonEvent& ev);      // A pages prose, B equips, C backs
    // The move the picker's cursor is on, or nullptr on the unequip row — shared by
    // the drill-down, its input handler and the MoveDetail render.
    const MoveDef* focusedPickerMove() const;
    void onSimTier(const ButtonEvent& ev);
    // Combat activity: the one pet-side screen where A+C is live.
    void onCombat(const ButtonEvent& ev);
    void startSimBattle();                          // build combat (safe stakes) + enter
    void finishCombat();                            // apply result, return to caller
    // Feeding-frenzy pacing: resolves one combat_.step(), then updates the
    // same-actor streak counters off who just acted (combat_.playerTurnNext()
    // is stable until step() runs, so it IS "who's about to act"). Shared by the
    // heartbeat auto-pace (game_core.cpp) and the A fast-forward (onCombat) so
    // skipping a beat doesn't desync the streak read.
    void advanceCombatTurn();
    int combatBeatsForTurn() const;                 // heartbeats to wait before the next step()
    void applyCombatResult();                       // rewards (win) / +Frag (live loss)
    // Post-fight bookkeeping for a spent Backup Drive: burn its remaining window and
    // award whichever of the three drive achievements the ending earned
    // (backupDriveAchievement, below). Called from applyCombatResult (wild/Sim) and
    // finishBossRound (every gauntlet round).
    void settleBackupDrive();
    void applyBattleFatigue();                      // per-wild-fight frag tax
    // Post-encounter status readout: a fought WILD battle (win or
    // loss) parks on Nav::PostEncounter first, showing the BANDWIDTH/FRAG delta
    // applyCombatResult() just caused. Any button press, or the kPostEncounterMs
    // timer (tick()), dismisses it back to the explore habitat (returnToExplore()).
    void dismissPostEncounter();
    void addCombatXp(int xp);                       // bank XP → level-up (+1 random stat)
    // The A+C Exploit allowance for a fight. The tunable today; the seam a
    // rare reward item hangs off later (item ships later).
    int exploitUsesPerBattle() const { return kExploitUsesPerBattle; }
    // Combat-usable items from the live inventory (combatHeal>0), for the picker.
    std::vector<OverrideItem> combatItemOptions() const;
    void consumeCombatItem(const char* id);         // decrement + out-of-combat effect

    // EXPL / Explore-mode: the sector list arms explore-mode, which runs as
    // a background mode on the IDLE habitat — auto-stepping guaranteed events (each
    // resolving via the shared event/combat plumbing) with a win-streak that unlocks
    // the sector's boss. onExploreControl drives the A+C control overlay
    // (Nav::ExploreControl → A ping / B warp / C stop).
    void onExplList(const ButtonEvent& ev);
    // Flatten the durable per-sub flag blocks to the row-major layout the shared
    // expl_screen row helpers read ([area*kSubAreasPerArea+sub]).
    void flattenSubFlags(bool (&cleared)[kAreaCount * kSubAreasPerArea],
                         bool (&bossUnlocked)[kAreaCount * kSubAreasPerArea]) const;
    // Open EXPL: pick the nav level + cursor row to enter on (resuming a running
    // explore-mode where it left off, else the top level's first landable row).
    void openExplList();
    // Two-level EXPL nav is `row` a stop at the CURRENT level
    // (explNavArea_)? Top level → DeepWeb + open area headers; inside an area → that
    // area's own selectable rows. `areaHeaderRow` is an area's header row index.
    bool explRowLandable(int row) const;
    int areaHeaderRow(int area) const;
    // Compose the idle badge label — the armed area's short (first-word) name + the
    // 1-based sub number, e.g. "CITRUS 3". Compact + collision-free.
    void exploreBadgeLabel(char* out, size_t n) const;
    // AUTO-PROGRESS. `autoProgressStep` is the hook every hand-back to the walk runs
    // (returnToExplore): it fires only once the armed sub-area has met its win target,
    // and then either fights the boss standing there or steps on. `autoProgressAdvance`
    // is the step rule itself — POSITIONAL, so it keeps cycling instead of stopping at
    // the frontier. `nextOpenArea` wraps to the next unlocked rung.
    void autoProgressStep();
    void autoProgressAdvance(int area, int sub);
    int nextOpenArea(int area) const;
    void startExplore(int sector, int sub);         // arm a sub-area → idle background mode
    void startDeepWebDive();                         // arm the endless terminal zone
    void doExploreStep();                           // one guaranteed-event step
    void returnToExplore();                         // event/fight → back to idle, mode continues
    // Auto Backup / Continuous Auto-Backup (Rig Shop g/h): re-arm the Backup
    // Drive death-save for free if `gateRow`'s upgrade is owned and it isn't already
    // armed. Called from startExplore/startDeepWebDive (Auto Backup) and
    // returnToExplore (Continuous Auto-Backup).
    void autoArmBackupShield(int gateRow);
    // Disk Maintenance (Rig Shop i): between explore events, silently run a
    // guaranteed-clean defrag once Fragmentation reaches the owned tier's
    // threshold, charged at that tier's Bits multiple of defragCost(). Called
    // from returnToExplore; a no-op if the upgrade isn't owned or Bits can't
    // cover the run.
    void maybeAutoDefrag();
    void onExploreControl(const ButtonEvent& ev);   // A+C overlay: ping / warp / stop
    void resolveLootEvent();                        // reward-pool draw, in place
    void resolveCacheEvent();                       // drop a Sealed Cache (non-interrupting)
    void grantLootReward(char* outFlavor, size_t outFlavorSize);  // shared reward-pool draw
    // MODS earn path. rollAreaModId draws a rarity-weighted id from an area's
    // loot table (area 0..kAreaCount-1 or kDeepWebSector); rollAnyModId draws globally
    // (Epic cache jackpot); grantMod adds the spare if there is room for it under
    // modStorageCap() and logs either way. Drop sites: boss clears + DeepWeb wins
    // (game_explore/game_combat), Epic sealed caches (game_items).
    const char* rollAreaModId(int area);
    const char* rollAnyModId();
    void grantMod(const char* id);
    // MOVES earn path — you learn a move by BEATING something that knows it. Rolls
    // `dropPct` and, on a hit, teaches one move off `from`'s KIT (everything it could
    // have used, not what it happened to cast) that this pet lacks and could actually
    // field. The one implementation both earn sites share (wild wins in
    // applyCombatResult, boss rounds in finishBossRound), so the filters that decide what
    // is teachable cannot drift apart between them. Returns whether anything was learned.
    bool rollEnemyMoveDrop(const Combatant& from, int dropPct);
    // Could beating something that knows `m` actually teach it to THIS pet? The three
    // filters rollEnemyMoveDrop applies, as one predicate, because the combat outro's
    // absorb (rivalFieldsUnknownMove) is a PROMISE that the roll has something to give:
    // if the two ever disagreed the screen would advertise a drop that cannot happen.
    bool moveIsTeachable(const MoveDef* m) const;
    // Is the fight on screen one the WORLD lets a pet learn from at all? A duel and an
    // arena bout are another operator's pet and a Sim dummy is a prop, so none of them
    // rolls a move drop — and nothing on screen may offer one. Read by the outro (which
    // adds "and won") and by the mask the KIT page marks its prizes from.
    bool combatCanTeach() const;
    void startEncounter();                          // roll the malbeast, open the intro
    void onEncounter(const ButtonEvent& ev);        // Fight / Flee / Sinkhole
    void resolveFlee();                             // pre-fight escape roll
    void resolveSinkhole();                         // consume Sinkhole Trap -> XP lump
    void startWildCombat(bool forceEnemyFirst);     // live-stakes combat from an encounter
    Combatant buildPlayerCombatant();               // + the ally-buff, if active

    // Boss ladder. Two confrontations share the carried-Health round plumbing
    // (build a gauntlet, run its rounds back-to-back with no heal between):
    //   • startSubAreaBoss(area,sub) — a single strong malbeast unlocked by a 10-win
    // streak (subBossUnlocked_); a full clear marks the SUB-AREA cleared.
    //   • startAreaBoss(area) — a 5-stage gauntlet of the area's sub-area bosses,
    //     reachable once all 5 sub-areas are cleared; a full clear marks the AREA
    // cleared (sectorCleared_ → next area) + grants the Title.
    // bossSub_ (>=0 = a sub-area boss for that sub · -1 = the area boss) tells
    // finishBossRound() which clear to record. startBossRound(carryHealth) begins one
    // round; finishBossRound() advances/clears/fails it. Always a MANUAL trigger.
    void startSubAreaBoss(int area, int sub);
    void startAreaBoss(int area);
    void startBossRound(int carryHealth);
    void finishBossRound();

    // Wi-Fi network event: a self-contained typed event. First resolves real-
    // network discovery (resolveNetworkDiscovery — pops the pending sighting
    // queue against the NetworkLedger; sets netDiscoveryFlavor_), independent of
    // and always followed by the Sleeping/Awakened-Guardian / Open-Cache /
    // Friendly-Visit roll (sets wifiFlavor_; unrelated flavor/combat system), then
    // B plays that roll's outcome out (loot / wild combat / ally buff).
    void startWifiEvent();
    void onWifi(const ButtonEvent& ev);
    void resolveWifiOutcome();
    // Real-network discovery resolution — the sole place a real network is
    // credited (Hacker Rank XP + Sealed Cache) or an occasional-repeat pet-XP is
    // granted. See game_net.cpp for the four-outcome breakdown (new / familiar
    // repeat / home-turf repeat / empty queue).
    void resolveNetworkDiscovery();

    // Shop event: a storefront explore event, either the current sector's item shop
    // (startShopEvent, Nav::Shop) or its mod shop (startModShopEvent, Nav::ModShop) —
    // both resolve to shopFront_, an AreaStorefrontDef (src/core/content/areas/) whose
    // arbitrary-length `listings` is the ONE place a row's stock/Bits price/item
    // costs live (the area's own price, distinct from an item's own Pedia-facing
    // bitsPrice). A cycles shopCursor_ through every listing (wraps); B buys the
    // SELECTED row (Bits + every item cost -> inventory add / mod grant) while its
    // stock + wallet + item costs allow; C leaves back to the Walk. Stock is
    // per-visit (shopStockLeft_, refilled from each row's own ShopListingDef::stock
    // on entry — not persisted). shopStatusLine() derives the grayscale-safe reason a
    // buy is/isn't available for the selected row.
    void startShopEvent();
    void startModShopEvent();
    void onShop(const ButtonEvent& ev);
    void buyShopItem();
    const char* shopStatusLine() const;

    // Key warp items. A "Key-item find" walk event drops a
    // random warp key into the inventory (non-interrupting, like the Sealed Cache);
    // then, on the walk, B opens a picker of held warp keys and using one jumps
    // straight to its target event — Access Token → the nearest shop
    // (startShopEvent), Safe-Mode Key → a safe rest (resolveSafeRestEvent). The keys
    // are inert from the ITEMS use path ("USE ON THE WALK") so they can only be spent
    // on a walk. heldWarpKeys() enumerates the currently-held keys (for the picker +
    // draw); resolveSafeRestEvent() applies the key's own effects (a negative Frag
    // magnitude on its row) and returns to the walk.
    void resolveKeyItemEvent();
    std::vector<const ItemDef*> heldWarpKeys() const;
    void onWarpPicker(const ButtonEvent& ev);
    void useWarpKey(const ItemDef& d);
    void resolveSafeRestEvent(const ItemDef& d);
    void drawWarpPickerScreen(Framebuffer& fb) const;

    // Hacker Rank: re-derives rank from networksSeen_ after
    // every new unique network and arms a pending celebration on a crossing.
    // applyPendingRankUp() surfaces it the next time nav_ lands back on Walk
    // (both the direct Wi-Fi-resolve path and the wild-combat-return path can
    // get there, so both call sites check it).
    void checkHackerRankUp();
    void applyPendingRankUp();

    void enterHackerTagEditor();                   // copy the tag into the edit buffer
    void saveHackerTag();                          // commit the edit buffer → hackerTag_

    // Hacker face (07) ----------------------------------------
    // The parallel operator interface reached by A+C at the top level. It reuses the
    // pet-side carousel + submenu contracts wholesale (same L1/L2 shapes, same A/B/C
    // table); only the centre canvas + slot roster differ. toggleFace() flips PET↔HACKER
    // from Idle/Cursor (dropping any cursor + hacker sub-state). The hacker carousel is
    // navigated with the SHARED Nav::Cursor cycling (8 slots, same as pet-side); B enters
    // a hacker slot's L2 (enterHackerSubmenu); onHackerSubmenu drives the entered slot.
    // Only PROFILE + SHOP are live; the rest render the inaccessible marker and
    // B is inert on them (hackerSlotAccessible).
    void toggleFace();
    HackerSlotId enteredHackerId() const;          // hacker slot under the cursor (L2 subject)
    bool hackerSlotAccessible(int slot) const;     // false = inaccessible (B inert)
    void enterHackerSubmenu();                      // Cursor B → the focused hacker slot's L2
    void onHackerSubmenu(const ButtonEvent& ev);   // L2 dispatch (PROFILE viewer / SHOP / VAULT)
    void onHackerShop(const ButtonEvent& ev);      // SHOP list: A cycle · B buy · C back
    void onHackerVault(const ButtonEvent& ev);     // VAULT list: A cycle · B decrypt · C back
    // Generic Rig Shop buy: looks up `row`'s RigUpgradeDef (game_rig_shop.h), checks
    // offered/afford, deducts Bits, bumps rigLevel_[row], applies the row's
    // RigEffectKind (only Bandwidth has one — the live-pool grant), logs, persists. One
    // function for every row — adding a row is a table entry, not a new buyX().
    void buyRigUpgrade(int row);
    // The OWNING half of that, with no till in it: bump the row `levels` times, apply
    // its RigEffectKind each time, log and persist. Split out because a rig level can
    // now also be WON (a solved Decryptogram's prize, game_cryptogram.cpp) — a granted
    // level and a bought one must land identically, and this is the one place either
    // lands. Out-of-range rows and non-positive counts are inert.
    void grantRigLevel(int row, int levels);
    bool rigUpgradeOwned(int row) const { return rigLevel_[row] > 0; }  // level > 0

    // MERGE HUB (game_merge.cpp) — the f Rig Shop unlock combines owned ingredient
    // items into a rarer one. Owning the Hub makes the MRG slot accessible; each
    // RECIPE is won separately off a Decryptogram (recipeOwned, below) and gates
    // whether it can be cooked — knowing the recipe is necessary but not sufficient,
    // the raw ingredients still have to be in the bag.
    void onHackerMerge(const ButtonEvent& ev);     // MERGE HUB list: A cycle · B craft · C back
    void drawHackerMerge(Framebuffer& fb) const;   // MERGE HUB screen body (below the shared header)

    // CREW (game_crew.cpp) — enlist in a crew (content_crews.h) and designate the
    // HOME NETWORK membership hangs off. Four views (CrewView, above); this pair is
    // the whole screen, and unlike its sibling Hacker sub-screens it draws its OWN
    // header band, because the title changes with the view (CREW / RED / the crew's
    // own name) and only the screen knows which one is up.
    void onHackerCrew(const ButtonEvent& ev);      // A cycle · B enter/act · C up a view
    void drawHackerCrew(Framebuffer& fb) const;    // CREW screen, header band included
    void onHackerPeers(const ButtonEvent& ev);     // PEERS list: A cycle · C back (B unbound)
    void drawHackerPeers(Framebuffer& fb) const;   // PEERS screen body (below the shared header)
    // Per-view helpers, one pair each, so no single function carries all four layouts.
    void drawCrewHub(Framebuffer& fb) const;
    void drawCrewNetPicker(Framebuffer& fb) const;
    void drawCrewTeam(Framebuffer& fb) const;
    void drawCrewDetail(Framebuffer& fb) const;
    // Rows on the Hub: HOME NET, then one per side. The sides are the fixed pair, not
    // a count over the roster — an empty side still has to be visitable, or a crew
    // added to it would have nowhere to appear.
    static constexpr int kCrewHubRows = 3;
    // The crew Exploit row the A+C picker should offer, or a default-constructed
    // (label == nullptr) one when the player belongs to no crew.
    CrewExploit crewExploitOption() const;
    // Cooking, indexed by kMergeRecipes row (game_internal.h). Ownership is a bit per
    // recipe wire in recipesOwned_, and this trio is the only code that knows that.
    bool recipeOwned(int recipeIndex) const;       // won off a Decryptogram — never bought
    void grantRecipe(int recipeIndex);             // the win: light the bit, persist
    // May the prize ladder hand this one over right now? Not owned, MERGE HUB built,
    // and any dish the recipe wants MET has been held at least once.
    bool recipeGrantable(int recipeIndex) const;
    bool recipeCraftable(int recipeIndex) const;   // owned + every ingredient in the bag
    void craftRecipe(int recipeIndex);             // consume the ingredients → grant the output
    // Shared by applyCombatBitsBonus/applyCacheBitsBonus/applyCombatXpBonus: floors
    // the % bonus at +1 whenever it's active, so a truncated-to-0 grant (a small
    // value * a low pct) never makes a purchased upgrade look inert.
    static int applyBonusPct(int value, int pct) {
        if (pct <= 0 || value <= 0) return value;
        int bonus = value * pct / 100;
        if (bonus < 1) bonus = 1;
        return value + bonus;
    }
    void drawHackerHome(Framebuffer& fb, int cursor) const;  // terminal centre + identity + carousel
    void drawHackerSubmenu(Framebuffer& fb) const;  // L2 dispatch (PROFILE / SHOP / VAULT)
    // VAULT's owned-openable row list (shared by input + render + the bulk-open
    // trigger), capped at `max`. Returns the count written into `rows`.
    int vaultOwnedRows(const ItemDef** rows, int max) const;
    // Spend the focused VAULT row on whatever it is for — a cache opens for a reward
    // draw, a Decryptogram rolls a quote and starts a board. The one place a VAULT B
    // resolves, so the tap path and the bulk-hold's release path cannot diverge.
    void cashVaultRow(const ItemDef& d);
    // e: resolves the VAULT hold-B gesture's SHORT-press half — called from
    // onButton on every B release. No-op unless the hold was armed here (not by the
    // unrelated CFG Factory-Reset hold, which also uses bHeld_/bDownMs_) AND never
    // crossed kBulkOpenHoldMs (that fires the bulk-open itself via tick()).
    void vaultBulkReleaseB();

    // Persistence ------------------------------------
    SaveData captureSave() const;                  // snapshot all persisted state
    void applySave(const SaveData& d);             // restore from a loaded snapshot
    void persistSave();                            // serialize + write via store_
    void markSaveDirty() { saveDirty_ = true; }    // schedule a debounced write

    // Raising-loop actions.
    bool itemUsable(const ItemDef& d, const char*& gateMsg) const;
    // Would consuming this row change nothing? An arming effect whose state is
    // ALREADY set spends the item and buys the pet nothing, so itemUsable turns
    // that into a stated gate instead of a silent evaporation. Reads the effect
    // vocabulary (ItemEffect::Kind), never a per-id branch, so a new arming
    // mechanic joins by adding its case beside its applier in applyItemEffects.
    bool itemUseIsInert(const ItemDef& d, const char*& why) const;
    void useItem();
    void startFeeding(const ItemDef& d, bool fromLockout);
    // Roll every held perishable (ItemDef::spoil) once. Called on the beats that already
    // disturb the bag rather than from a clock of its own; advances rng_.
    void rollPantrySpoilage();
    void endFeeding();
    // Non-vitals item side effects: Fragmentation delta + the Null
    // Noodles "empty feeling" Happiness pull toward 50%. Applied on consume for
    // both the feeding flow and the buff-use path.
    void applyItemEffects(const ItemDef& d);   // apply an item's on-Use pet levers (effects[])
    // Open a Sealed Cache: consume the container + draw from its
    // rarity tier's pool, then surface the yield (Nav::CacheYield).
    void openSealedCache(const ItemDef& d);
    // The shared per-cache reward roll (Bits + up to `maxItems` drawn items, plus the
    // Epic-cache bonus-mod roll) — factored out of openSealedCache so e's bulk
    // path can call it once per cache opened without duplicating the reward logic.
    // Does NOT consume the container or touch nav_/log_'s per-open "OPENED" line —
    // callers own both (openSealedCache logs+navigates once; the bulk path batches).
    void grantCacheReward(const ItemDef& d, int& bitsOut, const ItemDef** itemsOut,
                          int& itemCountOut, int maxItems);
    // Pick one item from a weighted loot pool — the shared draw every reward pool
    // resolves through (LootEntry::weight, else the item's own itemDropWeight).
    // Advances rng_; grants nothing on its own.
    const ItemDef* rollLootEntry(const LootEntry* pool, int poolSize);
    // Draw one item from a cache pool (adds to inventory + logs); returns the def.
    const ItemDef* drawCacheItem(const LootEntry* pool, int poolSize);
    // The cache-yield reveal: B/C dismisses back to the ITEMS list.
    void onCacheYield(const ButtonEvent& ev);
    void drawCacheYieldScreen(Framebuffer& fb) const;
    // e: bulk-open every OWNED cache sharing `focused`'s rarity tier in one
    // action (grantCacheReward per cache, tallied), then surface the aggregate
    // reveal (Nav::BulkYield). `focused` is the VAULT row the hold-B fired on.
    void openAllCachesOfRarity(const ItemDef& focused);
    // The bulk-yield reveal: A scrolls the tally if it overflows the window, B/C
    // dismisses back to the VAULT list (Nav::Submenu — mirrors onCacheYield).
    void onBulkYield(const ButtonEvent& ev);
    void drawBulkYieldScreen(Framebuffer& fb) const;
    // Boot Accelerator: consume it + take kBootAcceleratorCutMs off the incubation.
    void useBootAccelerator(const ItemDef& d);
    // Rollback: open the stat picker (nav_ = RollbackPicker) parked on the
    // first eligible stat; onRollbackPicker drives A cycle / B shed / C cancel;
    // rollbackEligible tests a stat has ≥1 earned point (skipped otherwise);
    // drawRollbackPickerScreen renders it.
    void openRollbackPicker();
    void onRollbackPicker(const ButtonEvent& ev);
    bool rollbackEligible(int stat) const {
        return stat >= 0 && stat < kLevelStatCount && statPoints_[stat] > 0;
    }
    int nextEligibleStat(int cur) const;            // A-cycle target (skips 0-point stats)
    void drawRollbackPickerScreen(Framebuffer& fb) const;
    void startMaint();
    void startStackerDefrag();
    void beginStackerBoard();   // reset the board + enter it, whoever is paying for it
    void finishStacker();
    void resolveMaint();
    bool rollMaintSuccess();

    // Care-mistake increment with the v21 Restore-Point shield: a POSITIVE `n` is
    // BLOCKED (and the shield consumed) when mistakeShieldActive_ is armed; a negative
    // `n` (e.g. Yubi-Cookie's −1 removal) is unaffected. The single DRY seam every
    // Game-level positive-mistake path (went-hungry, Lockout expiry) funnels through.
    void addCareMistakeShielded(int n);

    // Advances the model's Hunger by `elapsedMs` and pays Passive XP Farming
    // (j/k, Rig Shop) on the portion of the resulting decay that fell above
    // hungerXpThreshold(). The one place model_.tick() is called for Hunger
    // decay, so Game::tick() and debugTickHunger() (tests) can't drift apart.
    void tickHungerAndAwardXp(uint32_t elapsedMs);

    // Lockout lifecycle.
    void fireLockout();
    void expireLockout();
    void resolveLockout();
    // Carousel index of the slot routing to `id` — for the paths that park the
    // cursor on a submenu they weren't navigated into (a Lockout opening ITEMS, a
    // Sim-Battle returning to the LOADOUT hub). Falls back to 0 if `id` has no slot.
    int carouselSlotOf(SubmenuId id) const;

    // Decryption Hatch lifecycle
    void startHatch();       // empty save -> line-select (>1 line) or lay the egg
    void layEgg(const EggLineDef* line);  // commit a chosen line: lay its Boot-Sector egg
    const CreatureDef* rollHatchProcess(const EggLineDef* line);  // random Process draw (advances rng_)
    // DISK DECRYPTION lifecycle (game_decryption.cpp) — see inDecryption() above.
    // `allowDuplicates` lets the key repeat a colour and `easyHints` outlines the cells
    // a played row got exactly right; the hatch passes false to both, and only the
    // arcade's dial moves either.
    void startDecryption(bool allowDuplicates, bool easyHints);
    void onDecryption(const ButtonEvent& ev);
    void finishDecryption();   // bank the crack against the clock (or the till) and leave
    void drawDecryption(Framebuffer& fb) const;

    // DECRYPTOGRAM lifecycle (game_cryptogram.cpp) — see inCryptogram() above.
    // `quote` is a content_quotes.h roster row and `extraReveals` the opening gift; the
    // VAULT reads both off the quote's own tier, and the arcade's dial supplies the
    // second. Nothing here rolls the quote: pickQuote does that, once, at the cash-in.
    void startCryptogram(int quote, int extraReveals);
    void onCryptogram(const ButtonEvent& ev);
    // Runs the instant the board stops, not when the player leaves it: the tier ratchet
    // and the prize both have to have LANDED before the verdict screen can name them.
    // A no-op on an arcade run, which never touches a quote's standing.
    void settleCryptogram();
    void finishCryptogram();   // leave the board — to the arcade's till, or to the VAULT
    void drawCryptogram(Framebuffer& fb) const;
    // A roster row to play, drawn off the shared LCG from whatever is eligible, or -1
    // when nothing is. Advances rng_.
    int pickQuote(QuotePick pick);
    void setQuoteTier(int i, CryptogramTier t);
    // What a FIRST solve is playing for: the first recipe on content_quotes.h's prize
    // ladder whose gates are met and which isn't owned yet, or the Bandwidth fallback
    // once the kitchen is complete. Read at board START, so the stake can be printed
    // while the player is still deciding to place a letter.
    QuoteReward quoteFirstSolvePrize() const;
    void applyQuoteReward();   // the win: Bits + the one account unlock
    // The unlock's display name for the board's prize line, or nullptr — resolved off
    // whichever content table the reward kind names.
    const char* cryptogramPrizeLabel() const;
    void completeHatch();    // decrypt done -> hatch straight to Process, return to idle
    const SpriteData* hatchEggSprite() const;
    void startHatchGame(const EggLineDef* line);  // route a freshly laid egg to its HatchGame
    void drawLineSelect(Framebuffer& fb) const;  // the line-select modal

    // Clutch Pick lifecycle (game_eggpick.cpp) — see kEggPickRounds above. `rounds` is
    // how many halvings this run asks for: the hatch always passes kEggPickRounds, and
    // the arcade passes its difficulty.
    void startEggPick(int rounds);       // hide the live egg in the clutch, open the modal
    void eggPickAim(bool secondHalf);    // A/C: highlight the first/second half
    void eggPickCommit();                // B: halve, or dismiss the reveal
    void drawEggPick(Framebuffer& fb) const;

    // Hatch reveal cinematic (game_eggpick.cpp) — the on-demand crack, see
    // hatchRevealReady(). Non-interactive: it runs on the heartbeat and hatches itself.
    void openHatchReveal();
    void drawHatchReveal(Framebuffer& fb) const;

    // Isolation Protocol lifecycle (game_isolation.cpp) — see inIsolation() above.
    // `goalDots` is how many bytes make a CLEAN run: the hatch prices it off whatever
    // is left of the incubation clock, the arcade off a flat target.
    void startIsolation(int goalDots);   // turn the worm loose in the buffer, open the screen
    void onIsolation(const ButtonEvent& ev);
    void finishIsolation();              // spend the bytes on the clock and leave
    void drawIsolation(Framebuffer& fb) const;

    // CHROMATOPHORE lifecycle (game_chroma.cpp) — see inChroma() above. `windowMs` is
    // the opening window before the sweep arrives (the pace dial, scaled by the arcade)
    // and `switching` lets a round change the water mid-window, which only the hardest
    // cabinet setting turns on.
    void startChroma(int rounds, int windowMs, bool switching);
    void onChroma(const ButtonEvent& ev);
    void finishChroma();                 // spend the passes on the clock and leave
    void drawChroma(Framebuffer& fb) const;

    // Evolution boundary lifecycle (stub).
    bool evolveEligible() const;   // time-in-stage + care gate + a defined successor
    bool evolveRevealed() const;   // cinematic past hold+flash -> the reveal (B active)
    const char* evolutionTargetId() const;  // the successor id (signal route / care-picked pool)
    void noteCareSignal(DominantSignal s);   // tally a care interaction toward the dominant signal
    void fireEvolution();          // enter the modal toward the successor
    void completeEvolution();      // commit the swap, restart the in-stage clock, idle

    // Move-slot rework #12: stamp the Attack/Defend kind of every UNLOCKED slot
    // that's still Unset from the CURRENT pet_'s CreatureDef::slotKinds, then never
    // touch that slot again — so which creature occupies the evolution path at the
    // moment a slot unlocks decides that slot's kind for the rest of the raise.
    // Also backfills: any unlocked slot still empty gets the strongest owned+
    // unlocked move of its stamped kind not already sitting in another slot, so a
    // pet is never idling a slot on Quick Jab when it's earned something real to
    // put there — including a move unlocked at an earlier stage than the slot
    // itself. Called after every pet install: completeHatch, completeEvolution,
    // applySave, and an ARCH Deploy swap. A no-op if pet_ is null.
    void stampSlotKinds();
    // Defensive invariant: unequip any move whose kind no longer matches its
    // slot's stamped kind (a pre-v24 save migration, or any other drift). Called
    // right after stampSlotKinds() wherever a pet is (re)installed from stored
    // state (applySave, Deploy) — a fresh hatch/evolve never needs it (a
    // newly-unlocked slot starts empty).
    void enforceSlotKindInvariant();

    // Critical System Failure — the ONLY death path. The 5/5 dying state,
    // once its ageing/recovery window expires, is terminal: the pet becomes a
    // [CORRUPTED] ARCH record and the active save vacates -> the Decryption Hatch.
    bool csfRevealed() const;      // crash FX held long enough that B acknowledges
    void fireCSF();                // enter the terminal modal
    void acknowledgeCSF();         // B: archive the corpse -> record + new-egg Hatch

    ContentRegistry registry_;
    const CreatureDef* pet_ = nullptr;
    PetModel model_;
    // Purely presentational: where the pet is standing on its shelf right now.
    // Stepped once per heartbeat off pet_->locomotion and parked when there is no
    // mover, so it never needs a reset hook on hatch, evolution or a loaded save.
    IdleWander petWander_;
    // ...and where the ambient copies beside it are standing. One per replication
    // slot, of which idleCompanionCount() are live; each is seeded apart in Game's
    // constructor, since identical wanders move identically.
    IdleWander companionWander_[kWormReplicaSlots];
    // Per-stage tally of care interactions, indexed by DominantSignal, feeding
    // the evolution routing tables. Reset when a new stage begins
    // (hatch / evolution). Runtime-only scaffold: not persisted yet — a reboot
    // mid-stage restarts the tally, behaviour-neutral while every signal routes
    // to the same successor. Weighting (1 per action) is a first cut (tunable).
    int signalTally_[kNumDominantSignals] = {0};
    Inventory inventory_;
    EventLog log_;
    int bits_;

    Nav nav_ = Nav::Idle;
    int cursor_ = 0;            // focused carousel slot (also the entered slot)
    UiMode uiMode_ = UiMode::IconsLabel;
    int brightness_ = kBrightnessDefault;  // backlight level (persisted, v14)

    // L2/L3 state.
    int listRow_ = 0;                      // selected row (ITEMS row idx / MAINT 0..1)
    // EXPL two-level nav cursor level: -1 = top level (area headers +
    // DeepWeb), >= 0 = inside that area (its sub-areas). Runtime-only; reset to -1 each
    // time the EXPL submenu is opened.
    int explNavArea_ = -1;
    // AUTO-PROGRESS: the walk steps the ladder by itself (autoProgressStep). Toggled
    // with the A+C chord from inside the explore-control overlay, which is only
    // reachable while explore-mode is running — so the mode is scoped to exploring by
    // construction. Runtime-only, and deliberately NOT persisted: explore-mode itself
    // doesn't survive a reboot, so a restored flag would arm a mode with nothing to act
    // on. It DOES outlive one walk, so cancelling and re-arming explore keeps it set.
    // Defaults ON — hands-off ladder stepping is the expected experience; the chord
    // opts OUT rather than in.
    bool autoProgress_ = true;
    // Focused row of the explore-control overlay (0..kExploreControlRows-1). Reset to
    // the first action each time the chord opens it, so the list never reopens pointing
    // at whatever was last pressed.
    int exploreCtlRow_ = 0;
    const ItemDef* detailItem_ = nullptr;  // ITEMS detail subject
    // d: the active ITEMS type-tab filter. Runtime-only (never persisted) —
    // enterSubmenu() (and the Lockout Open-Items path) reset it to All every time
    // ITEMS is (re-)entered, so it never "sticks" across a visit.
    ItemFilter itemFilter_ = ItemFilter::All;
    // Which of the ITEMS submenu's two L2 screens is showing (ItemsScreen, public).
    // Runtime-only, reset on every entry alongside itemFilter_; itemPickRow_ is the
    // picker's tile cursor.
    ItemsScreen itemsScreen_ = ItemsScreen::List;
    int itemPickRow_ = 0;
    // Cache-yield reveal: what the just-opened cache produced, held for
    // the Nav::CacheYield overlay. The ItemDef* point into the static registry (stable
    // after the container leaves the bag). cacheYieldCache_ = the opened container.
    const ItemDef* cacheYieldCache_ = nullptr;
    const ItemDef* cacheYieldItems_[2] = {nullptr, nullptr};
    int cacheYieldItemCount_ = 0;
    int cacheYieldBits_ = 0;
    // e VAULT bulk-open reveal (Nav::BulkYield): the rarity group + totals
    // from the just-run openAllCachesOfRarity, held for the reveal screen. Mirrors
    // the cacheYield* fields above but tallies across N caches instead of one.
    const ItemDef* bulkYieldCache_ = nullptr;
    int bulkYieldCachesOpened_ = 0;
    int bulkYieldBits_ = 0;
    struct BulkTallyEntry { const ItemDef* def = nullptr; int count = 0; };
    std::vector<BulkTallyEntry> bulkYieldTally_;
    int bulkYieldRow_ = 0;   // A-scroll cursor into bulkYieldTally_
    MaintKind maintKind_ = MaintKind::Defrag;
    // 0 = Quick (Bits, may fail) · 1 = Tool (an item, guaranteed) · 2 = Stacker (the
    // minigame, worth exactly what the board earns). Three ways to pay for the same
    // clean — luck, an item, or skill — which is why the third one needs no new
    // currency: it is the only one that can wipe a disk outright, and the only one that
    // can come up short without failing.
    int defragVariant_ = 0;
    Stacker stacker_;                      // the variant-2 run in flight (never saved:
                                           // a run interrupted by a power cycle forfeits
                                           // its Bits, board and all)
    uint32_t lastStackerStepMs_ = 0;
    // What the last played board took off the disk (finishStacker). Held only so the
    // outcome toast can print the number the board earned, which — unlike the fixed
    // kDefragReduction a rolled or bought run reports — isn't knowable from the variant.
    int stackerFragRemoved_ = 0;
    int defragCount_ = 0;                  // this pet's successful defrags —
                                           // persists through ARCH freeze/thaw (save v16)
    // Care-mistake SHIELD (save v21) — per-pet, reset on a new egg (startHatch). A
    // Restore-Point buff arms mistakeShieldActive_, which blocks the NEXT positive care
    // mistake (consumed on interception, addCareMistake). shieldItemConsumed_ and
    // yubiConsumed_ are once-per-lifetime gates on the Restore Point and the Yubi-Cookie.
    bool mistakeShieldActive_ = false;
    bool shieldItemConsumed_ = false;
    bool yubiConsumed_ = false;
    // Tiramisudo's permanent upgrade (save v50) — MINUTES shaved off this pet's
    // Bandwidth regen interval (bandwidthRegenMinutes(), read by Game::tick). Per-pet
    // and granted once: the first helping roots the pet, every one after it is just
    // food. Unlike the two once-per-lifetime gates above it FREEZES WITH THE PET
    // (SaveStoredPet), because it is a property of the creature rather than of the run
    // — storing a pet in the rack must not cost it an upgrade it earned.
    int bandwidthRegenBonusMin_ = 0;
    // Ambig-USB's armed effect (save v28) — per-pet, reset on a new egg (layEgg).
    // Guarantees the pet's next Process->Script Trojan divert (Game::fireEvolution)
    // instead of leaving it to the kTrojanDivertPct roll; consumed there regardless
    // of whether the pet actually has a Trojan divert target.
    bool forceTrojanDivert_ = false;
    // Backup Drive's timed death-save (save v30) — per-pet, reset on a new egg.
    // The lifetimeUptimeMs() deadline the save is armed until; 0 = inactive.
    // Game::buildPlayerCombatant() arms Combatant::itemShield while lifetimeUptimeMs()
    // is under this deadline; a fight that spends it (Combatant::itemShieldFired)
    // clears this back to 0 early, regardless of time remaining. Re-using the item
    // just overwrites the deadline (a fresh 1h window). A Sim Battle deliberately
    // fights without it (Game::startSimBattle) — no stakes, so nothing to save.
    uint32_t backupShieldUntilMs_ = 0;
    // DeepWeb Dive depth state — see area_defs' area.h for the depth ramp itself.
    // bestDeepWebDepth_ (save v35) is per-pet, reset on a new egg like the fields
    // above: the highest exploreStreak_ this pet has ever reached in the dive,
    // read back by the Zero-Day Bell (SetDeepWebStartDepthToBest) so it targets
    // THIS pet's own frontier, never a device-wide max. deepWebDepthMultiplier_ and
    // pendingDeepWebStartDepth_ are session-volatile like exploreStreak_ itself (not
    // persisted): the multiplier (Deep-Learning Module/Core, x2/x4) is reset to 1 at every
    // site that resets exploreStreak_ to 0, and doesn't stack — arming a new one just
    // overwrites it. pendingDeepWebStartDepth_ is armed by a Backdoor/Rootkit/Kernel/
    // Zero-Day Bell and consumed the next time startDeepWebDive() begins a fresh dive;
    // -1 = none armed (starts at depth 0 as normal).
    int bestDeepWebDepth_ = 0;
    int deepWebDepthMultiplier_ = 1;
    int pendingDeepWebStartDepth_ = -1;
    static constexpr int kDeepWebStartDepthUseBest = -2;  // pendingDeepWebStartDepth_
                                                          // sentinel for the Zero-Day Bell
    // How many stacking foods (ItemEffect::Kind::HungerStacking — Polltatoes) the pet
    // has eaten since it last lost a Hunger point to decay. Session-volatile like the
    // dive knobs above: the run it measures is a sitting at the bag, and the next
    // decay tick (tickHungerAndAwardXp) ends it anyway, so a reboot mid-run costing
    // the player their stack is a forgiving default rather than a save field.
    int stackingFoodRun_ = 0;

    int statPage_ = 0;   // STAT: 0 vitals · 1 loadout · 2 buffs · 3 species · 4 audit log
    // Row-window scroll offset for whichever of STAT's two flowed prose pages is
    // open (LOADOUT and BUFFS) — B advances it, wrapping, when that page's rows
    // outrun one screen. ONE offset serves both because a page change resets it
    // anyway, alongside C backing out and every other statPage_ reset site.
    int statScroll_ = 0;
    // The rows those two pages show. Game owns the state they report, stat_screen.h
    // owns the shape, and these are the marshalling between the two — shared by the
    // page draw (game_render.cpp) and the B-scroll, which has to advance by the same
    // window the page drew or it skips and repeats rows.
    std::vector<ProseRow> statLoadoutRows() const;
    std::vector<BuffRow> statBuffRows() const;
    // {rows on screen starting at statScroll_, rows in total} for the open STAT page;
    // {0, 0} for a page that doesn't flow rows and so doesn't scroll.
    struct StatScrollSpan { int shown; int total; };
    StatScrollSpan statScrollSpan() const;

    // CFG submenu. cfgScreen_ is the entered L3; cfgGroupRow_ the focused row of a
    // group screen (DISPLAY / RADIO), kept apart from listRow_ so backing out of a
    // group lands on the row that opened it; cfgUiPick_ the focused UI Mode option;
    // factoryScope_ the Factory-Reset scope. bHeld_/bDownMs_ track a held B for the
    // hidden hold-to-reveal / hold-to-commit gestures.
    CfgScreen cfgScreen_ = CfgScreen::SysInfo;
    int cfgGroupRow_ = 0;
    int cfgUiPick_ = 0;
    int cfgBrightPick_ = 0;   // Brightness picker focus (0..kBrightnessLevels-1)
    int cfgAuditPick_ = 0;    // Audit level picker focus (0 OFF, 1 SCAN, 2 SCAN+CAP)
    int cfgTravelPick_ = 0;   // Travel-mode confirm focus (0 = NO, 1 = YES)
    int cfgApPick_ = 0;       // 'Pedia AP toggle focus (0 = OFF, 1 = ON)
    int cfgLinkPick_ = 0;     // pet-to-pet LINK toggle focus (0 = OFF, 1 = ON)
    int pediaQrPage_ = 0;     // PEDIA QR step (see pediaQrPage())
    int cfgTitlePick_ = 0;    // Titles picker focus (0 = NONE, else 1+sector index)
    int factoryScope_ = 0;
    // Every screen-level hold on the device is a held B: the CFG Factory Reset, the
    // Hacker VAULT's bulk-open, the ITEMS type-filter cycle and the MOVES picker's
    // show-all. They share this one flag because they are guarded to four different
    // screens and so can never be armed at the same time; the release edge
    // (Game::onButton) offers the press to each in turn and at most one claims it.
    bool bHeld_ = false;
    uint32_t bDownMs_ = 0;
    // A's list-step repeat (game_listnav.cpp), plus the DECRYPTOGRAM's own faster one.
    // Its OWN flag, distinct from bHeld_ — A and B are independent physical buttons
    // that could in principle both be mid-hold at once (a step repeating while the
    // other thumb starts a screen gesture).
    bool aHeld_ = false;
    uint32_t aDownMs_ = 0;
    // C's two holds: a list's backward walk (game_listnav.cpp) and the DECRYPTOGRAM
    // board's cursor repeat (kCryptogramRepeatMs). The board's repeat runs on its OWN
    // cadence in tick(), like combat's sprites, because stepping a thirty-cell cursor
    // on the 4fps heartbeat would take ten seconds to lap. cryptoRepeatLastMs_ is when
    // the last repeated step fired, kept primed while nothing is held so entering the
    // board can't burst-catch-up.
    bool cHeld_ = false;
    uint32_t cDownMs_ = 0;
    uint32_t cryptoRepeatLastMs_ = 0;
    // The list step's own two repeat clocks (game_listnav.cpp), one per direction, kept
    // primed while nothing is held for the same reason cryptoRepeatLastMs_ is. They are
    // separate because A and C are independent physical buttons: sharing one clock would
    // let a release-and-press of the opposite direction inherit the other's cadence.
    uint32_t listRepeatLastMs_ = 0;
    uint32_t listBackRepeatLastMs_ = 0;
    // Latched the moment a held C walks the cursor backward, and read on C's release to
    // tell a hold from a tap: a press that moved the cursor has done its job and must
    // not ALSO cancel out of the list on the way up.
    bool listBackStepped_ = false;
    char hackerTag_[kHackerTagMax + 1] = "NETRUNNER_99";  // persisted
    // HackerTag on-device arcade editor: a working buffer + caret. Caret
    // kHackerTagMax = the ⏎ confirm cell.
    char editTag_[kHackerTagMax + 1] = {0};
    int editCaret_ = 0;

    // ARCH submenu: the focused record action + the rack of frozen stored
    // pets (persisted). archConfirm_ is the inline confirm for Store/Deploy.
    ArchAction archAction_ = ArchAction::Store;
    std::vector<SaveStoredPet> rack_;
    bool archConfirm_ = false;
    int archConfirmChoice_ = 0;          // 0 Cancel · 1 Confirm

    // MODS submenu. loadout_ is the equip state; modSlot_ is the slot
    // whose picker is open; modPick_ the picker row (0 = unequip). The inline
    // overwrite confirm is a picker sub-state (modConfirm_ + the choice +
    // the pending mod id). Mod EFFECTS apply in combat — equip is real.
    Loadout loadout_;
    int modSlot_ = 0;
    int modPick_ = 0;
    bool modConfirm_ = false;
    int modConfirmChoice_ = 0;          // 0 Cancel · 1 Confirm
    const char* modPendingId_ = nullptr;
    // selecting a mod in the picker opens its detail (read-then-act, mirrors
    // ITEMS). modDetail_ overlays the picker; modDetailId_ is the inspected mod.
    bool modDetail_ = false;
    const char* modDetailId_ = nullptr;

    // Which half of the LOADOUT hub is open (ui_state.h). Hub is the three-row menu
    // itself; the other three route the SAME Nav::Submenu/Nav::Detail pair to the
    // mod slots, the move slots, or the Sim-Battle tier pick — so C from any of them
    // returns here rather than to the carousel. loadoutHubRow_ is the hub's own
    // cursor, kept across an entry so leaving a page lands back on the row that
    // opened it.
    LoadoutTab loadoutTab_ = LoadoutTab::Hub;
    int loadoutHubRow_ = 0;

    // MOVES + PRACTISE (the LOADOUT hub's other two rows) + the shared combat
    // engine. trainRow_ is the focused MOVES row (always an unlocked slot); the L3
    // Detail is either the move picker or, under PRACTISE, the Sim-Battle tier pick
    // (trainScreen_).
    // The move picker mirrors MODS (slot/pick/confirm + pending). combat_ runs the
    // battle; combatBeat_ paces the auto-resolution; Health lives in combat_ and is
    // NEVER persisted (transient). combatXp_/combatLevel_ persist (save v2).
    MoveLoadout moveLoadout_;
    // Move-slot rework #12: the per-pet stamped Attack/Defend kind for each move
    // slot (Unset until stampSlotKinds() first fills it, then locked). Reset to
    // all-Unset on every new egg (Game::startHatch, the single new-egg chokepoint
    // that also resets statPoints_/combatLevel_/moveLoadout_); restored verbatim on
    // an ARCH Deploy swap alongside the rest of the incoming pet's creature-level
    // state (combatLevel_/statPoints_, save v26) and its own move+mod loadout
    // (save v29) — a Deploy backfills any still-Unset slot for the newly active
    // pet. Persisted (save v24).
    SlotKind slotKinds_[kMaxMoveSlots] = {SlotKind::Unset, SlotKind::Unset,
                                          SlotKind::Unset, SlotKind::Unset};
    int trainRow_ = 0;
    // MoveDetail is the picker's reader half (hold-B): the picker chooses, the detail
    // page explains, so neither has to shrink to fit the other.
    enum class TrainScreen { MovePicker, MoveDetail, SimTier };
    TrainScreen trainScreen_ = TrainScreen::MovePicker;
    int moveSlot_ = 0;
    int movePick_ = 0;
    // First prose line drawn on MoveDetail — B advances it, wrapping past the end
    // (drawMoveDetail / moveProseLines, train_screen.h). Reset on every entry.
    int moveProseScroll_ = 0;
    // Move picker default view: only moves the pet can actually equip into
    // `moveSlot_` right now. Holding A past kMoveFilterHoldMs toggles the full
    // list (tick()); reset false whenever the picker is (re)entered (onTrainList).
    bool moveShowAll_ = false;
    bool moveConfirm_ = false;
    int moveConfirmChoice_ = 0;          // 0 Cancel · 1 Confirm
    const char* movePendingId_ = nullptr;
    int simTier_ = 0;
    Combat combat_;
    int combatBeat_ = 0;
    int combatTurnBeat_ = 0;   // heartbeats since the last autonomous turn (paces auto-battle)
    // Combat sprite motion runs on its own faster tick (kCombatAnimMs) instead of the
    // shared beat_ (which paces turn timing/idle/everything else) — see game_core.cpp's
    // Nav::Combat branch. combatHitBeat_ counts animBeat ticks since the last
    // combat_.step() and drives drawCombat's post-hit impact punch/flash.
    int combatAnimBeat_ = 0;
    uint32_t lastCombatAnimMs_ = 0;
    int combatHitBeat_ = 0;
    // Whose colours the pet is wearing (FX_CAMO), as the ART being sampled plus a LEVEL —
    // neither is a beat. The tick reads the pet's live cast (camoTarget), resolves it to
    // the sprite that answers it (camoSpriteForTarget) and eases the level toward it
    // (camoAdvance), so the colours hold while the pet holds the move and no other cue on
    // the combat screen can move them.
    //
    // The SPRITE is what is held, rather than the target that named it, because it is the
    // sprite that decides whether the palette is actually changing: a cast that starts
    // copying the fighter opposite when the pet is already wearing that fighter's line —
    // and it is the same creature either way — must sit on the colour it has rather than
    // dissolve into itself. Turning one into tones is a scan of its pixels, so that waits
    // for the draw. `combatCamoLeaving_` is the art a swap is coming off, held until the
    // dissolve between two palettes finishes. All three are cleared on leaving the fight,
    // the one place a disguise stops meaning anything.
    const SpriteData* combatCamoWorn_ = nullptr;
    const SpriteData* combatCamoLeaving_ = nullptr;
    uint8_t combatCamoLevel_ = 0;
    // Which page of the mid-combat panel is showing: 0 closed, else 1..kCombatStatPages
    // (combat_screen.h). B CYCLES it — the panel grew a second page when opponents
    // started arriving with real loadouts, and a second key to reach it would have had
    // to come out of the three combat already spends.
    int combatStatsPage_ = 0;
    int combatXp_ = 0;          // XP banked toward the NEXT level (0-based model)
    int combatLevel_ = 0;       // creature level (== sum of statPoints_, the invariant)
    // Per-pet earned combat-stat points: 0 power · 1 defense · 2 speed · 3
    // max-Health. A level-up bumps one at random; Rollback sheds one. Applied into
    // the player Combatant in buildPlayerCombatant. Persists across evolution; a new
    // egg resets to all-0 (save v11).
    int statPoints_[kLevelStatCount] = {0};
    // The stat index (0..3) of the most-recent level-up, or -1 if none yet — the
    // "which stat grew" tell (STAT's LVL n is the shipped feedback surface).
    int lastLevelUpStat_ = -1;
    int rollbackRow_ = 0;       // Rollback picker cursor (RollbackPicker nav state)

    // Explore-mode. Exploration is a background mode on the IDLE habitat:
    // exploreActive_ arms it, exploreSector_ is the armed sector, exploreStreak_ the
    // current win-streak, exploreSteps_ a run step counter (tests/telemetry),
    // exploreStepBeat_ the auto-step pacer (heartbeats since the last step). The mode
    // is RUNTIME-ONLY (a reboot stops it; the streak resets — default). The
    // wild-encounter intro / event screens are their own Nav states, beyond the
    // 3-deep menu budget, suspending the global auto-defocus (analogy).
    // combatCaller_ tells finishCombat() which caller to return to: the LOADOUT hub
    // (Sim-Battle) or the explore habitat (a wild encounter, — never the carousel).
    // (D2): Bandwidth is the FARMING resource — spent 1/re-farm win to keep
    // cleared-sub loot full (freezing the decay); when empty, the decay curve
    // applies until it regenerates on the heartbeat over real time (tick()). Device/
    // session-level (NOT per-pet — it's the player's exploration stamina); not persisted
    // (a reboot refills). bandwidthAccumMs_ carries the sub-step regen remainder.
    int bandwidth_ = kBandwidthMax;
    uint32_t bandwidthAccumMs_ = 0;
    bool exploreActive_ = false;
    int exploreSector_ = 0;
    int exploreSub_ = 0;       // armed sub-area within exploreSector_
    int exploreStreak_ = 0;    // per-sub-area win-streak (volatile, resets on any stop)
    int exploreSteps_ = 0;
    int exploreStepBeat_ = 0;  // heartbeats since the last auto-step (paces explore)
    int exploreEventBeat_ = 0; // heartbeats a full-screen explore event (Wi-Fi/Shop) has
                               // been held hands-off; auto-continues past its hold
    // THE dissolve clock (FX_ABSORB / FX_SHRED), on the fast kFxAnimMs tick rather than
    // the heartbeat — a sweep is redrawn every frame it is up, and stepping it at 4fps
    // is what made it read as jumping rather than moving. One counter serves all three
    // hosts because only one dissolve is ever on screen: the feeding modal, the Wi-Fi
    // event, and the beaten rival's combat outro. Each resets it as its own sweep opens
    // and gates its advance (game_core.cpp), so it always means beats-since-THIS-sweep.
    //
    // Deliberately NOT exploreEventBeat_ / combatBeat_: a press restarts those, and a
    // dissolve animates something already resolved — replaying it under a player leaning
    // on A would be a second helping that never happened.
    int fxBeat_ = 0;
    uint32_t lastFxMs_ = 0;
    char exploreFlavor_[32] = "";
    CombatEnemy encounterEnemy_;
    int encounterChoice_ = 0;
    bool encounterSinkhole_ = false;
    enum class CombatCaller { Sim, Wild, Boss, Tourney };
    CombatCaller combatCaller_ = CombatCaller::Sim;

    // Post-encounter status readout (Nav::PostEncounter). Captured
    // by applyCombatResult() at the moment a WILD (non-boss, non-Sim) fight resolves:
    // the bandwidth/fragmentation values just BEFORE and just AFTER this fight's
    // rewards/tax were applied, plus whether 's corruption shield covered it.
    // Runtime-only (never persisted — it describes one just-finished fight, not
    // durable state); postEncounterDeadlineMs_ is the kPostEncounterMs auto-dismiss
    // deadline (a real-ms window, the kSdIconRevealMs pattern, not a beat count).
    int postEncBwBefore_ = 0;
    int postEncBwAfter_ = 0;
    int postEncFragBefore_ = 0;
    int postEncFragAfter_ = 0;
    bool postEncShielded_ = false;
    uint32_t postEncounterDeadlineMs_ = 0;
    // Per-stat points gained from any level-up(s) THIS encounter + the level
    // reached, so the post-encounter readout can name the stat delta ("POWER +1")
    // instead of leaving a level-up silent. applyCombatResult snapshots statPoints_
    // before the reward path and diffs it after. Runtime-only, like the rest here.
    int postEncStatGain_[kLevelStatCount] = {0};
    int postEncLevelAfter_ = 0;

    // Boss ladder state. Durable + persisted (save v8/v13):
    //   sectorCleared_[area]        — the AREA's 5-stage gauntlet beaten (gates the
    // linear area ladder + the Title, v8).
    //   subCleared_[area][sub]      — that SUB-AREA's boss beaten (v13).
    //   subBossUnlocked_[area][sub] — a 10-win streak unlocked that sub-area's manual
    //                                 boss trigger (v13; distinct from exploreStreak_).
    // The rest is transient run state: bossGauntlet_ is the active gauntlet's rounds,
    // bossRound_ the current round, bossSector_ which area it belongs to, bossSub_ the
    // sub-area (>=0) or -1 for the area boss (tells finishBossRound which clear to
    // record), bossBitsAccrued_ the banked lump paid on a full clear.
    bool sectorCleared_[kAreaCount] = {};
    bool subCleared_[kAreaCount][kSubAreasPerArea] = {};
    bool subBossUnlocked_[kAreaCount][kSubAreasPerArea] = {};
    // post-clear wild-win count per sub-area — the decay input for its
    // diminishing non-Bits drop chances (refarmDropScalePct). PER-PET: it
    // persists across a reboot for the active pet (save v15) but RESETS on a new egg
    // (startHatch) and on a pet-swap (archDeployStored), so every pet finds a cleared
    // area an undepleted training ground — the point of re-farm (not per-device).
    uint16_t subRefarmCount_[kAreaCount][kSubAreasPerArea] = {};
    BossGauntlet bossGauntlet_{"", 0, {}};
    int bossRound_ = 0;
    int bossSector_ = 0;
    int bossSub_ = -1;
    int bossBitsAccrued_ = 0;

    // Zone-completion Titles. Player-level, persisted (save v10) so
    // they survive a pet reset like sectorCleared_. titlesUnlocked_ is a bitmask
    // (bit i = sector i's Title earned); equippedTitle_ is the shown Title's sector
    // index, or -1 for none. unlockTitle() is called from the sector-clear path.
    uint32_t titlesUnlocked_ = 0;
    int equippedTitle_ = -1;
    void unlockTitle(int sector);
    // The next selectable Title after `cur` (wraps): NONE (-1) plus each UNLOCKED
    // sector, in order — locked sectors are skipped. Drives the CFG picker's A-cycle.
    int nextSelectableTitle(int cur) const;

    // Web-'Pedia reveal state (save v25, game_pedia.cpp). seenCreatures_ holds
    // glimpsed-but-not-hatched creature pointers — borrowed, stable registry
    // pointers (same pattern rack_/records_ use, resolved through the registry on
    // load). The two malbeast masks are plain bitfields.
    std::vector<const CreatureDef*> seenCreatures_;
    // The raised tally (save v39) — same borrowed-registry-pointer shape as
    // seenCreatures_ above, and player-level in the same way: nothing clears it.
    std::vector<const CreatureDef*> raisedCreatures_;
    uint16_t malbeastSeenMask_ = 0;
    uint16_t malbeastDefeatedMask_ = 0;

    // --- Achievement state (save v40, game_pedia.cpp) -------------------------------
    // Two bitsets indexed by AchievementDef::wire, NOT by table position — that is what
    // lets content_achievements.cpp group its rows by theme and still load an old save.
    // `earned` is the achievement; `notified` is whether its banner has been shown, kept
    // separate so a firmware update that retro-awards a back catalogue announces every
    // one of them instead of silently absorbing them.
    static constexpr int kAchBytes = kAchievementWireCap / 8;
    uint8_t achEarned_[kAchBytes] = {0};
    uint8_t achNotified_[kAchBytes] = {0};
    bool achBit(const uint8_t* bits, int wire) const {
        return wire >= 0 && wire < kAchievementWireCap &&
               (bits[wire / 8] & (1u << (wire % 8))) != 0;
    }
    static void setAchBit(uint8_t* bits, int wire) {
        if (wire >= 0 && wire < kAchievementWireCap) bits[wire / 8] |= 1u << (wire % 8);
    }
    // Compare every unearned countable row against its goal and unlock what has landed.
    // Cheap by construction (an earned row is skipped before its progress is resolved,
    // and adjacent rungs of one ladder share a memoised progress value), but still rate-
    // limited to kAchSweepIntervalMs — nothing here is latency-critical.
    void sweepAchievements();
    uint32_t lastAchSweepMs_ = 0;
    // Pay a freshly-unlocked row's `rewards`. One switch over AchievementReward::Kind —
    // a new kind of reward lands here, never as a new field on the row.
    void applyAchievementRewards(const AchievementDef& d);
    // Advance the home-screen announcement: retire an expired banner (marking it
    // notified) and raise the next pending one while the pet face sits idle.
    void tickAchievementBanner();
    int achBannerWire_ = -1;        // the row on screen, by wire number; -1 = none
    int achBannerCount_ = 0;        // 1 = named row · >1 = a collapsed burst of N
    uint32_t achBannerUntilMs_ = 0; // when it retires

    // Every item id that has ever been in the bag (save v40) — borrowed registry
    // pointers, like seenCreatures_. Player-level: spending an item never un-collects it.
    std::vector<const ItemDef*> collectedItems_;
    // Fold the live inventory into collectedItems_. Called from the sweep rather than
    // wired into each grant site: every path that adds an item necessarily leaves it in
    // the bag for a tick, so one pass over the stacks catches all of them — including
    // the items an upgraded save was already holding.
    void sweepCollectedItems();
    // Lifetime boss rounds won (save v40) — counted in Game::finishBossRound.
    int bossWins_ = 0;
    // Lifetime DEFRAG minigame boards CLEARED (save v44) — counted in
    // Game::finishStacker. Player-level, unlike defragCount_ beside it, which is the
    // active pet's own tally of defrags of every variant: this one is a record of skill
    // and so belongs to the operator, not to whichever pet happened to be on the shelf.
    int stackerWins_ = 0;
    // --- v56: three more lifetime tallies, all player-level like the two above ------
    // ROCK THE DOCK brackets TAKEN (Game::awardTourneyPurse). Brackets, not matches:
    // the arena is single elimination, so the run is the unit — see AchSeries::TourneyWins.
    int tourneyWins_ = 0;
    // LINK duels WON (Game::settlePvpBattle). The FIRST_DUEL achievement counts the
    // meeting and fires at the START of a duel, because a duel that desyncs was still
    // fought; this counts the verdict, so it only moves on a fight that finished.
    int pvpWins_ = 0;
    // Dishes cooked at the MERGE HUB (Game::craftRecipe). Distinct from the recipes
    // KNOWN mask beside it: knowing a method is a Decryptogram cracked once, cooking is
    // the thing the player does with it over and over.
    int mergesCooked_ = 0;
    // The deepest DeepWeb Dive reached BY SPECIES (save v40). A per-species record rather
    // than one device high-water mark, so "take three different species deep" is
    // answerable and a line's own record is the max over its members. Borrowed registry
    // pointers, like seenCreatures_.
    struct SpeciesDive {
        const CreatureDef* def = nullptr;
        int depth = 0;
    };
    std::vector<SpeciesDive> speciesDives_;
    // Record `depth` against the species if it beats that species' standing record.
    void recordSpeciesDive(const CreatureDef* c, int depth);
    // Seat `c` as the active pet. THE one place pet_ is assigned, so that becoming
    // this device's pet and being tallied as raised cannot come apart — every path
    // that installs a pet (a laid egg, a hatch, an evolution, an ARCH Deploy, a save
    // load) goes through here. nullptr clears the pet and tallies nothing.
    void installPet(const CreatureDef* c) {
        pet_ = c;
        if (c) markCreatureRaised(c->id);
    }

    // Wi-Fi network event. wifiOutcome_/wifiFlavor_ are set when the
    // guardian/cache/friendly roll happens (startWifiEvent) and consumed on B
    // (resolveWifiOutcome). networksSeen_ is the lifetime unique-REAL-network
    // counter (persists, save v4) — only resolveNetworkDiscovery() bumps it now.
    // allyBuffBattlesLeft_ is the friendly-visit buff consumed one per battle in
    // buildPlayerCombatant.
    WifiOutcome wifiOutcome_ = WifiOutcome::SleepingGuardian;
    char wifiFlavor_[40] = "";
    int networksSeen_ = 0;
    PowerStatus power_;  // last reading from the platform tier (CFG "BATT" line)
    int allyBuffBattlesLeft_ = 0;

    // Real-network discovery (game_net.cpp: registerNetwork/resolveNetworkDiscovery,
    // core/net/network_ledger.h). Three structures, three different questions:
    //
    //   visibleNetworks_ — "what can the radio hear right now": a RAM-only snapshot
    //     (cap kNetVisibleCap) refreshed by every sighting and aged out by
    //     kNetVisibleFreshMs. Feeds the CREW picker (crewNetworkRows) and nothing
    //     else, so no reward bookkeeping can ever shape what the picker offers.
    //   pendingNetworks_ — "what still owes the player a reward": a RAM-only FIFO
    //     (cap kPendingNetworkQueueCap) drained one entry per EXPL Wi-Fi event. Full
    //     evicts oldest-first rather than refusing new sightings, so an unwalked
    //     backlog goes stale instead of going deaf. Not persisted — a reboot losing
    //     a few unresolved sightings is fine, they'll be re-scanned.
    //   networkLedger_ — "what has this pet actually walked past": the SD-backed
    //     history (per-BSSID display name + sighting count) that decides
    //     new-vs-familiar-vs-home-turf, and supplies the picker's tally column.
    //
    // netDiscoveryFlavor_ is the on-screen line resolveNetworkDiscovery() sets, drawn
    // alongside wifiFlavor_ (drawWifiEvent). emptyQueueStreak_ throttles the
    // empty-queue Happiness penalty (see kNetDiscoveryEmptyCooldownStrikes) — reset
    // whenever a sighting IS found, and on a fresh startExplore().
    struct PendingNetwork {
        uint64_t key = 0;
        char name[33] = {0};
    };
    struct VisibleNetwork {
        uint64_t key = 0;
        char name[33] = {0};
        uint32_t lastSeenMs = 0;
    };
    // True while `v` is still inside the freshness window. Unsigned subtraction, so a
    // nowMs_ wrap behaves like every other elapsed-time check in the engine.
    bool networkVisible(const VisibleNetwork& v) const {
        return nowMs_ - v.lastSeenMs <= kNetVisibleFreshMs;
    }
    // Record/refresh one sighting in the in-range snapshot. Returns true when the
    // network ARRIVED (absent, or present but aged out) rather than merely being
    // re-heard — registerNetwork keys the egg-hatch nudge off that edge.
    bool noteVisibleNetwork(uint64_t key, const char* name);
    VisibleNetwork visibleNetworks_[kNetVisibleCap];
    int visibleNetworkCount_ = 0;
    PendingNetwork pendingNetworks_[kPendingNetworkQueueCap];
    int pendingNetworkCount_ = 0;
    NetworkLedger networkLedger_;
    char netDiscoveryFlavor_[40] = "";
    NetDiscovery netDiscovery_ = NetDiscovery::None;
    int emptyQueueStreak_ = 0;

    // Pet-to-pet discovery (game_peers.cpp, core/net/peer_link.h). The same
    // snapshot-vs-history split as the networks above, for the same reason:
    //
    //   livePeers_ — "who can I hear right now": a RAM-only snapshot (cap
    //     kPeerVisibleCap) refreshed by every beacon and aged out by
    //     kPeerVisibleFreshMs. Holds the FULL last HELLO, so a live row shows what
    //     someone is raising this second rather than what they were last time.
    //   peerLedger_ — "who have I ever met": the SD-backed roster, updated only on
    //     the arrival edge, which is what makes timesMet a count of meetings rather
    //     than of frames.
    //
    // A peer aging out of the snapshot and returning is therefore a new meeting —
    // the freshness window IS the boundary between one encounter and the next.
    struct LivePeer {
        uint64_t key = 0;
        PeerHello hello;
        uint32_t lastSeenMs = 0;
    };
    // True while `p` is still inside the freshness window. Unsigned subtraction, so a
    // nowMs_ wrap behaves like every other elapsed-time check in the engine.
    bool peerLive(const LivePeer& p) const {
        return nowMs_ - p.lastSeenMs <= kPeerVisibleFreshMs;
    }
    // Record/refresh one sighting in the snapshot. Returns true when the peer
    // ARRIVED (absent, or present but aged out) rather than merely being re-heard.
    bool noteLivePeer(uint64_t key, const PeerHello& hello);
    LivePeer livePeers_[kPeerVisibleCap];
    int livePeerCount_ = 0;
    PeerLedger peerLedger_;
    bool linkEnabled_ = false;   // persisted CFG opt-in (save v37)
    // Runtime-only, never saved: whether the platform tier has a stored network, and
    // how the current job's association went. A credential store is device-tier
    // (platform/esp32/net_credentials.h) and the outcome is worthless after a reboot,
    // so neither belongs in the save blob. There is deliberately no saved bit
    // alongside them — nothing may persist that lets this device reach the 'net
    // without the operator asking again (see netConnectWanted).
    bool netProvisioned_ = false;
    NetStatus netStatus_;
    // Live largest-allocatable-block probe, wired by the platform tier
    // (see setHeapProbe). Null = nothing reports, which reads as "no reason to hold
    // back" and is the host build's normal state.
    HeapProbe heapProbe_ = nullptr;
    // The one buffer every save is serialized into. Owning it is what keeps a write off
    // the allocator: it is sized once, behind the higher floor, and reused for good — so
    // a save landing while the radio holds the heap has nothing large left to ask for,
    // which is the whole reason a capture session no longer blocks persistence.
    std::vector<uint8_t> saveBlob_;
    size_t lastBlobSize_ = 0;            // what the last write measured; sizes the buffer
    // How big that buffer has to be for the next save to fit without resizing: the last
    // blob plus the growth state can put on between saves, never below the reservation.
    // The margin is what keeps the answer stable — a buffer sized to the blob exactly
    // would put every save back on the allocating path the moment the pet gained a row.
    size_t saveBlobTargetBytes() const {
        const size_t want = lastBlobSize_ + kSaveBlobHeadroomBytes;
        return want < kSaveReserveBytes ? kSaveReserveBytes : want;
    }
    int savesDeferred_ = 0;              // consecutive refusals; 0 once one lands
    uint32_t savesDeferredLow_ = 0;      // worst headroom seen while refusing
    // Consecutive writes the STORE refused, which is a different failure from the heap
    // guard above: the save was built and handed over, and the medium turned it down.
    // Counted for the same reason — a retry that works is indistinguishable from one
    // that never had to happen unless something keeps score.
    int saveWritesFailed_ = 0;
    void noteSaveDeferred() {
        const uint32_t h = heapHeadroom();
        if (savesDeferred_ == 0 || h < savesDeferredLow_) savesDeferredLow_ = h;
        ++savesDeferred_;
    }

    // Update checking, same runtime-only reasoning: where to look is device-tier
    // (platform/esp32/update_source.h) and a check's answer is stale the moment
    // the device is off. `updateJob_` is latched rather than derived, because a
    // job outlives the screen that started it — see netConnectWanted().
    bool updateSourceKnown_ = false;
    // Sized to match UpdateSource::kUrlCap and UpdateArtifact::url — the manifest's
    // address and the addresses inside it come off the same host, so a URL one can
    // hold and another can't would be a trap.
    char updateManifestUrl_[160] = {0};
    char webBundleVersion_[16] = "UNKNOWN";
    bool updateJob_ = false;
    UpdateTarget jobTarget_ = UpdateTarget::None;
    UpdateStatus updateStatus_;
    InstallStatus installStatus_;
    // UPDATES screen: which row the cursor is on, and — once an install row is
    // pressed — which target is sitting at its yes/no confirm. Nav state, not job
    // state: the confirm clears on the way out, the job doesn't.
    int updateRow_ = 0;
    UpdateTarget updateConfirm_ = UpdateTarget::None;
    int updateConfirmPick_ = 0;   // 0 = NO, and NO is always where it starts

    // One job ends the same way whatever it was. Keeps the two setters from
    // drifting on which fields a finished job leaves behind.
    void endJob() {
        updateJob_ = false;
        jobTarget_ = UpdateTarget::None;
    }

    // PEERS screen cursor (game_peers.cpp).
    int peerRow_ = 0;

    // 1v1 duels (game_pvp.cpp). One session at a time, held entirely in RAM: a duel is
    // worth nothing and survives nothing, so none of this is saved. pvpSession_ is the
    // id every frame of one duel carries — it is what makes a stale or foreign frame
    // inert, and it is NOT a credential (core/net/pvp_link.h).
    PvpPhase pvpPhase_ = PvpPhase::Idle;
    uint32_t pvpSession_ = 0;
    uint64_t pvpPeerKey_ = 0;            // opponent's packed radio MAC
    uint8_t pvpPeerMac_[6] = {0};        // ...and the raw form, for addressing replies
    char pvpPeerTag_[kPvpTagCap] = {0};  // their HackerTag, for every line the UI draws
    PvpFighter pvpLocalFighter_;         // our own spec — encoded, then built FROM
    PvpFighter pvpRemoteFighter_;        // theirs, as decoded off the wire
    bool pvpChallenger_ = false;         // we opened the session, so we already sent our
                                          // fighter inside the INVITE and owe no ACCEPT
    bool pvpHost_ = false;               // we hold the player_ slot and pick the seed
    bool pvpWon_ = false;                // the finished duel's verdict, locally framed
    uint32_t pvpSeed_ = 0;
    char pvpStatus_[32] = "";            // why the last session ended (empty = it didn't)

    // --- ROCK THE DOCK's run state (game_tourney.cpp) -----------------------
    // Four bytes plus a byte of verdict is the WHOLE bracket: every entrant — handle,
    // species, level, stat spread, kit, mods, Exploit — is derived from the seed
    // (core/model/tournament.h) rather than stored, and `tourneyAlive_` is one bit per
    // original slot. So a run costs almost nothing to persist and reloads identical.
    // Persisted at save v53.
    uint32_t tourneySeed_ = 0;           // 0 = no run in play
    uint8_t tourneyAlive_ = 0;           // survivor bitmask over kTourneySlots
    uint8_t tourneyRound_ = 0;           // 0..kTourneyRounds-1
    TourneyPhase tourneyPhase_ = TourneyPhase::Ready;
    // Runtime only. The bracket screen's read cursor (A steps it, nothing acts on it)
    // and the built opponent behind the round's match card — rebuilt whenever a round
    // opens or a run is resumed, never persisted, because both are derivable.
    int tourneyCursor_ = 0;
    TourneyFighter tourneyOpponent_;
    TourneyView tourneyView_ = TourneyView::Bracket;
    // The scroll position of whichever reader is open — one field, because only one
    // ever is, and a per-view scroll would have to be reset on every switch anyway.
    int tourneyScroll_ = 0;
    // The entrant the SCOUT sheet is showing. Held rather than re-derived per repaint:
    // building one walks the whole move and mod tables, which is not a per-frame cost.
    TourneyFighter tourneyScouted_;
    int tourneyScoutedSlot_ = -1;
    uint32_t pvpPhaseStartMs_ = 0;       // for the phase timeout
    uint32_t pvpLastSendMs_ = 0;         // retry pacing for the frame holding the session
    int pvpRetries_ = 0;
    uint64_t localRadioKey_ = 0;         // this device's own MAC, packed (0 = host build)

    // Outbound frames waiting for the device tier to drain (takePvpOut). A tiny ring
    // rather than a callback so the engine stays free of the radio, exactly like the
    // rest of core/.
    struct PvpOut {
        uint8_t mac[6] = {0};
        uint8_t buf[kPvpFrameCap] = {0};
        size_t len = 0;
    };
    PvpOut pvpOutbox_[kPvpOutboxCap];
    int pvpOutCount_ = 0;

    // LINK screen cursor.
    int pvpRow_ = 0;

    // Queue one encoded frame for `mac`. Drops the frame (rather than blocking or
    // growing) if the outbox is full — every frame here is repeated on a timer, so a
    // drop costs one retry interval, not the session.
    void queuePvpFrame(const uint8_t* mac, const uint8_t* buf, size_t len);
    // Re-send whatever frame is holding the current phase open, on kPvpRetryMs, and
    // fail the session out on timeout. Driven from tick().
    void pvpTick();
    // Encode + queue the frame this phase is waiting on an answer to.
    void sendPvpPending();
    // Drop back to Idle, telling the opponent unless `notify` is false (it already
    // knows — it is the one that ended the session). `why` is what the LINK screen
    // shows; empty means a clean, unremarkable end.
    void endPvpSession(const char* why, bool notify);
    // Both fighters are known and agreed: build the shared fight and enter Nav::Combat.
    void startPvpBattle(uint32_t seed);

    // --- ROCK THE DOCK (game_tourney.cpp) -----------------------------------
    // Open the bracket screen off the EXPL row: draws a fresh bracket when no run is
    // in play, else resumes the one that is.
    void openTourney();
    void onTourney(const ButtonEvent& ev);         // A read · B bout · C back · A+C brief
    void drawTourney(Framebuffer& fb) const;
    void drawTourneyBracket(Framebuffer& fb) const;
    void drawTourneyScout(Framebuffer& fb) const;
    void drawTourneyBrief(Framebuffer& fb) const;
    void openTourneyScout();                       // hold-B on the bracket
    void tourneyReleaseB();                        // the tap half of that gesture
    void startTourneyMatch();                      // the operator's own match, live
    void finishTourneyMatch();                     // advance the bracket, or end the run
    void abandonTourney();                         // forget the run (seed back to 0)
    void awardTourneyPurse();                      // the title's one lump
    // The duel finished — record the verdict, log it, and park on the LINK screen.
    // Deliberately does NOT go through applyCombatResult: no stakes means no XP, no
    // Bits, no Fragmentation, no care signal.
    void finishPvpBattle();

    void onHackerLink(const ButtonEvent& ev);      // LINK list: A cycle · B act · C back
    void drawHackerLink(Framebuffer& fb) const;    // LINK screen body (game_pvp.cpp)

    // Crew allegiance + the HOME NETWORK it hangs off (game_crew.cpp). Both are
    // player-level — they survive a pet reset, like the HackerTag and the Titles — and
    // both persist (save v36). crewIndex_ is a kCrews row (-1 = unaffiliated); the SAVE
    // stores the crew's id string, so the table can be reordered. homeNetworkKey_ is a
    // packed 48-bit BSSID (0 = none designated) with homeNetworkName_ caching the label
    // so the UI reads correctly even on a boot where the ledger file is unavailable
    // (no SD card).
    int crewIndex_ = -1;
    uint64_t homeNetworkKey_ = 0;
    char homeNetworkName_[33] = {0};

    // Shop event. shopFront_ points at the CURRENT area's AreaStorefrontDef (its
    // .shop or .modShop, picked by startShopEvent/startModShopEvent) — a stable
    // pointer into compiled-in content, valid for the whole visit. shopIsModShop_
    // says which registry resolves a listing's `id` (item vs mod). shopStockLeft_ is
    // the per-visit remaining stock for each listing (index-aligned with
    // shopFront_->listings; refilled from each row's own ShopListingDef::stock on
    // entry — NOT persisted). shopCursor_ is the A-cycled row selection (wraps
    // 0..listingCount-1). All transient run state, like the boss/gauntlet fields
    // above.
    const AreaStorefrontDef* shopFront_ = nullptr;
    bool shopIsModShop_ = false;
    int shopStockLeft_[kMaxShopListings] = {};
    int shopCursor_ = 0;

    // Hacker face (07). face_ is the top-level PET/HACKER mode (transient — a
    // reboot always boots PET; not persisted). It gates the carousel + submenu dispatch
    // and the render path. cursor_/nav_ are SHARED with the pet face (a flip drops to
    // Idle), so the hacker carousel reuses the same cursor cycling. hackerShopRow_ is the
    // SHOP list cursor.
    Face face_ = Face::Pet;
    int hackerShopRow_ = 0;   // SHOP list cursor
    int hackerVaultRow_ = 0;  // VAULT list cursor — owned sealed-cache row
    int hackerMergeRow_ = 0;  // MERGE HUB list cursor — recipe row
    // CREW screen state. The screen is four views and `crewView_` IS the navigation:
    // Hub (the home network and the two sides), Team (one side's roster), Detail (one
    // crew's page), plus Picker — the home-network chooser, a modal over the Hub. C
    // walks back up that order and only leaves the screen from the Hub. All transient
    // UI state; the membership + home network below are what persist.
    CrewView crewView_ = CrewView::Hub;
    int crewHubRow_ = 0;                   // 0 = HOME NET, 1 = RED, 2 = BLUE
    CrewTeam crewTeam_ = CrewTeam::Red;    // the side Team/Detail are showing
    int crewTeamRow_ = 0;                  // cursor within that side's roster
    int crewDetail_ = -1;                  // the kCrews row Detail is showing
    int crewProseScroll_ = 0;              // Detail: first prose line drawn (A pages it)
    int crewNetRow_ = 0;
    // Rig Shop purchase levels, one int per RigRow (game_rig_shop.h) — 0 = never
    // bought; a tiered row's purchase count, or 0/1 for a one-time unlock. Player-level,
    // survives lifecycles. Persistence (game_persist.cpp) maps rows below
    // kRigRowExtBase onto the legacy per-row SaveGame fields (bwUpgradeCount/
    // fragAmountTier/etc., save v19-v31, unchanged wire format) and every row from
    // there up into the forward-compatible `rigLevelsExt` vector (save v32) — so a new
    // row never needs a save.h edit.
    int rigLevel_[kMaxRigUpgrades] = {};

    // Which MERGE HUB recipes are known — one bit per MergeRecipe::wire
    // (game_internal.h), read and written only through recipeOwned/grantRecipe.
    // Player-level, survives lifecycles; persisted whole as SaveData::recipeOwned
    // (save v51), with its first four bytes also riding the legacy u32 a v31..v50
    // reader understands. A recipe is never bought — the Decryptogram's prize ladder
    // is the only writer.
    uint8_t recipesOwned_[kMergeRecipeWireBytes] = {};

    // Warp-key picker. warpRow_ is the focused row into the live
    // heldWarpKeys() list while nav_ == Nav::WarpPicker; transient run state (the
    // keys themselves persist in the inventory, but the picker cursor does not).
    int warpRow_ = 0;

    // Hacker Rank — a player/device-level tier derived from
    // networksSeen_ (persists across pets, save v4). rankUpPending_/
    // rankUpFlavor_ hold a crossing until the next Walk-screen render (both the
    // direct Wi-Fi-resolve return and the wild-combat-return path check it).
    int hackerRank_ = 0;
    bool rankUpPending_ = false;
    char rankUpFlavor_[32] = "";

    // Audit-mode passive discovery scan. netScanEnabled_ is the
    // persisted runtime opt-in (save v5, default OFF). Real-radio BSSID dedup lives in
    // the SD-backed networkLedger_ above (see its comment), not here: this scan only
    // queues a sighting via registerNetwork, and never dedups or credits directly.
    bool netScanEnabled_ = false;
    // 'Pedia local-AP runtime opt-in (save v20, default OFF). Read by the device
    // tier's RadioArbiter (top radio owner while on). See apEnabled() above.
    bool apEnabled_ = false;

    // Audit-mode handshake-capture policy. Runtime timing brain
    // (hot-broadcast window + re-arm cooldown); only its enabled() toggle intent is
    // persisted (save v6). Ticked each beat; the device tier feeds it handshakes.
    AuditCapture auditCapture_;
    // SHAKES: the lifetime count of distinct captured handshakes + its per-BSSID
    // dedup set, both PERSISTED (save v7) so unique-credit survives a reboot.
    // registerHandshake() bumps these once per new BSSID. Unrelated to (and
    // unaffected by) the network-discovery rework above — a different ledger.
    int handshakesSeen_ = 0;
    std::vector<uint64_t> seenHandshakeBssids_;

    SdStatus sdStatus_;  // last microSD reading from the platform tier (CFG "SD")
    // Idle SD-present reveal: game-ms the card last became present + a guard so the
    // 0-ms sentinel (a boot mount at nowMs_==0) still reveals. sdRecheckRequested_
    // is the one-shot CFG->device re-mount request.
    uint32_t sdIconRevealAtMs_ = 0;
    bool sdIconRevealArmed_ = false;
    bool sdRecheckRequested_ = false;

    // Latched CFG->device request to enter travel sleep. Runtime-only: what it asks
    // for is a reset, and a device that woke from one is not still asking.
    bool travelSleepRequested_ = false;

    // The arbiter's resolved radio owner (runtime-only; None on host).
    RadioOwner radioOwner_ = RadioOwner::None;

    // MAINT process.
    bool maintSuccess_ = false;
    int processBeat_ = 0;
    bool processResolved_ = false;         // true once running -> outcome

    // Feeding modal. feedBefore_ is the vitals snapshot taken the instant before
    // the food is applied, so the modal can report the change it actually made
    // rather than the change its magnitudes asked for (ui/modals.h).
    const ItemDef* feedItem_ = nullptr;
    FeedVitals feedBefore_{};
    int feedBeat_ = 0;
    bool feedFromLockout_ = false;

    // Lockout modal.
    bool lockoutActive_ = false;
    uint32_t lockoutDeadlineMs_ = 0;
    bool lockoutPayOption_ = false;        // false = Open Items · true = Pay Bits
    bool lockoutItemsContext_ = false;     // ITEMS opened from the Lockout modal

    // The egg being incubated: which line laid it, and the Boot-Sector INCUBATION clock
    // (egg-at-idle) counting down while it sits there. The clock is what every hatch
    // minigame's prize is spent against, and what hatchRevealReady() gates the on-demand
    // crack on; persisted (save v9) so an egg survives a reboot mid-incubation.
    const EggLineDef* hatchLine_ = nullptr;
    uint32_t bootHatchRemainMs_ = 0;

    // DISK DECRYPTION (the Ransomware hatch minigame, game_decryption.cpp). Un-persisted for
    // the same reason the Clutch Pick and the Isolation Protocol below are: it is played
    // out in the minute after the egg is laid, so a reboot mid-board forfeits the bonus
    // and leaves a normal egg incubating. decryptionEasy_ is the arcade dial's hint
    // setting, read only by the screen.
    DiskDecryption decryption_;
    bool decryptionEasy_ = false;

    // THE DECRYPTOGRAM (game_cryptogram.cpp). The BOARD is un-persisted like the three
    // above — a reboot mid-quote forfeits the Decryptogram, which is the same deal
    // every other minigame offers. What does persist (save v48) is quoteStates_: two
    // bits per QuoteDef::wire holding the tier ladder, which is simultaneously the loss
    // ratchet and the "already won" record. Player-level, so it outlives a pet.
    //
    // cryptogramPrize_ is the finished run frozen for the verdict screen, for the same
    // reason the arcade's result block is: the payout has already landed on bits_ and
    // the rig by the time the player reads it.
    Cryptogram cryptogram_;
    int cryptogramQuote_ = -1;        // roster row being played, or -1
    int cryptogramPrizeBits_ = 0;     // what the solve actually paid
    QuoteReward cryptogramPrize_ = {};  // ...and the unlock beside it (None = replay)
    uint8_t quoteStates_[kQuoteStateBytes] = {0};

    // GAMES / the arcade (game_arcade.cpp). arcadeRow_ is the cabinet list's cursor and
    // arcadeGame_ the roster index of whatever is RUNNING — the two part company as
    // soon as a run starts, since the list stays where the player left it. arcadeRun_
    // is what a minigame reads to know it is being played for fun: it redirects the
    // payout and scales the pacing, and it is cleared by finishArcadeRun and by every
    // reset that could strand a run (a wipe, a fresh egg).
    //
    // The result block is the finished run frozen for the payout screen, because the
    // minigame's own state is free to be reset the instant the next one starts.
    //
    // Only the two TALLIES persist (save v47), keyed on the wire by each row's id so
    // the roster can be reordered.
    int arcadeRow_ = 0;
    int arcadeGame_ = 0;
    bool arcadeRun_ = false;
    ArcadeDifficulty arcadeDifficulty_ = ArcadeDifficulty::Medium;
    bool arcadeWon_ = false;
    int arcadeScore_ = 0;
    int arcadeScoreMax_ = 0;          // 0 = a win-or-lose cabinet, with no score to show
    int arcadeBits_ = 0;              // what the run actually paid
    int arcadePlays_[kArcadeMaxCabinets] = {0};
    int arcadeWins_[kArcadeMaxCabinets] = {0};
    int arcadeBest_[kArcadeMaxCabinets] = {0};   // high score per cabinet (save v55)
    // Did the run just finished SET that best? Runtime-only and about one screen: the
    // payout says NEW BEST instead of quoting a number the player already had.
    bool arcadeNewBest_ = false;

    // Clutch Pick modal (the Phishing hatch minigame, game_eggpick.cpp). Runtime-only
    // and deliberately un-persisted: the whole game is played in the seconds after the
    // egg is laid, so a reboot mid-pick simply forfeits the bonus and leaves a normal
    // egg incubating — no save field, no schema bump.
    uint8_t eggPickTarget_ = 0;      // slot 0..kEggPickCols*kEggPickRows-1 holding the live egg
    uint8_t eggPickRound_ = 0;       // halvings committed so far
    uint8_t eggPickCol_ = 0;         // surviving column span [eggPickCol_, +eggPickColSpan_)
    uint8_t eggPickColSpan_ = 0;
    uint8_t eggPickRow_ = 0;         // surviving row span
    uint8_t eggPickRowSpan_ = 0;
    bool eggPickSecondHalf_ = false; // C-half (right/bottom) aimed at, vs the A-half
    bool eggPickResolved_ = false;   // rounds done -> the reveal, waiting on B
    bool eggPickWon_ = false;
    // How many halvings THIS run asks for. The hatch always plays kEggPickRounds; the
    // arcade's difficulty dial is this number, since a narrower survivor is the only
    // axis a puzzle with no clock has.
    uint8_t eggPickRounds_ = kEggPickRounds;

    // Isolation Protocol (the Worm hatch minigame, game_isolation.cpp). Un-persisted for
    // the same reason the Clutch Pick above is: it is played out in the minute after the
    // egg is laid, so a reboot mid-run forfeits the bonus and leaves a normal egg
    // incubating. The run's own step clock is real-ms, like the Stacker's.
    Isolation isolation_;
    uint32_t lastIsolationStepMs_ = 0;
    int isolationBanked_ = 0;        // bytes already spent on the clock, so B can't double-pay

    // CHROMATOPHORE (the Metamorphic hatch minigame, game_chroma.cpp). Un-persisted for
    // the reason the two above are: the whole run is over in half a minute after the
    // egg is laid, so a reboot mid-run forfeits the bonus and leaves a normal egg
    // incubating. Its clock is real-ms and DELTAS rather than a step cadence — the
    // board is a stopwatch, not a board that steps.
    Chromatophore chroma_;
    uint32_t lastChromaMs_ = 0;
    int chromaBanked_ = 0;           // passes already spent on the clock, so B can't double-pay

    // Hatch reveal cinematic. Counts heartbeats since the chord cracked the egg, which
    // maps straight onto the shell's hatch frames; completeHatch fires off the end.
    int hatchRevealBeat_ = 0;
    // Line-select modal: the cursor into the unlocked egg lines while
    // choosing which egg to lay. Runtime-only — an empty save re-prompts on reboot
    // (the modal precedes laying/persisting an egg, so there's nothing to persist).
    int lineSelectRow_ = 0;

    // Evolution modal (stub). evolveTo_ is the successor being revealed; the
    // swap onto pet_ is deferred to completeEvolution so the cinematic holds the
    // old sprite first. stageEnteredMs_ clocks time-in-stage for the trigger.
    const CreatureDef* evolveTo_ = nullptr;
    int evolveBeat_ = 0;
    uint32_t stageEnteredMs_ = 0;

    // Critical System Failure. dyingArmed_ latches when the pet enters the
    // 5/5 dying state and dyingEnteredMs_ is the anchor it clocks from; csfBeat_
    // paces the crash-FX hold before B acknowledges. records_ (persisted, v3) is the
    // ARCH rack of RETIRED/CORRUPTED records — greyed, read-only, no live slot.
    //
    // dyingElapsedMs_ is the total burned so far and the ONLY one of the three that
    // persists (save v42), which is what stops a power cycle from refunding the
    // window. The pair splits because the anchor is in monotonic game-ms, meaningless
    // across a boot, while the accumulator is a duration and survives one intact.
    bool dyingArmed_ = false;
    uint32_t dyingEnteredMs_ = 0;
    uint32_t dyingElapsedMs_ = 0;
    int csfBeat_ = 0;
    std::vector<SaveRecord> records_;

    // Persisted identity / lifetime counters. generation_ is
    // the active pet's generation; petsRaised_ the lifetime egg count; uptimeBase_
    // the lifetime uptime carried from prior boots (lifetime = base + nowMs_).
    int generation_ = 0;
    int petsRaised_ = 0;
    uint32_t uptimeBase_ = 0;
    uint32_t lifetimeSteps_ = 0;

    // Persistence plumbing. store_ is the platform save sink (nullptr = none).
    // saveDirty_ schedules a debounced write; lastSaveMs_ paces autosaves.
    ISaveStore* store_ = nullptr;
    bool saveDirty_ = false;
    uint32_t lastSaveMs_ = 0;

    // Timing.
    uint32_t nowMs_ = 0;
    uint32_t lastInputMs_ = 0;
    uint32_t lastBeatMs_ = 0;
    uint32_t lastModelMs_ = 0;
    int beat_ = 0;
    bool dirty_ = true;
    uint32_t rng_ = 0x9e3779b9u;
};

} // namespace mal
