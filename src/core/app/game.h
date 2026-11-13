// game.h: top-level engine state machine + render entry point.
//
// Navigation is a shallow stack: the carousel is
// always-on furniture framing the living area. A/C summon the *cursor* onto a
// slot; B enters its submenu (L2). The list/detail layers sit beneath the
// cursor (ITEMS, MAINT) plus the event-overlay modals (Feeding, Lockout) that
// preempt the whole tree. The engine renders the full active (224) canvas: UI
// chrome native, creature upscaled.
#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "tunables.h"
#include "core/app/game_achievements.h"
#include "core/app/game_rig_shop.h"
#include "core/app/operator.h"
#include "core/app/settings.h"
#include "core/app/game/arcade_screen.h"
#include "core/app/game/crew_screen.h"
#include "core/app/game/encounter_screen.h"
#include "core/app/game/fight_screen.h"
#include "core/app/game/inline_confirm.h"
#include "core/app/game/items_menu.h"
#include "core/app/game/rig_shop_screen.h"
#include "core/app/game/stat_reader.h"
#include "core/app/game/vault_screen.h"
#include "core/device/net_status.h"
#include "core/device/update_status.h"
#include "core/device/power_status.h"
#include "core/device/radio_status.h"
#include "core/device/sd_status.h"
#include "core/content/areas/area_defs.h"
#include "core/content/content_arcade.h"    // kArcadeMaxCabinets bounds the arcade tallies
#include "core/content/content_passives.h"  // kWormReplicaSlots sizes companionWander_
#include "core/content/content_quotes.h"    // QuoteReward: the Decryptogram prize
#include "core/content/content_riddles.h"   // kRiddleReplies sizes the shown reply order
#include "core/content/defs.h"
#include "core/content/effect_text.h"
#include "core/content/registry.h"
#include "core/model/cant.h"                // SigilSet / CantCipher: the guardian's tongue
#include "core/model/combat.h"
#include "core/model/cryptogram.h"
#include "core/model/cryptogram_run.h"
#include "core/model/arcade_run.h"
#include "core/model/egg_pick.h"
#include "core/model/explore_walk.h"
#include "core/model/feeding.h"
#include "core/model/maint_run.h"
#include "core/model/fight.h"
#include "core/model/gauntlet_run.h"
#include "core/model/pvp_session.h"
#include "core/model/tourney_run.h"
#include "core/model/disk_decryption.h"
#include "core/model/event_log.h"
#include "core/model/flock.h"          // the guardian's body: a boids swarm, not a sheet
#include "core/model/hacker_rank.h"
#include "core/model/idle_camo.h"
#include "core/model/idle_disguise.h"
#include "core/model/idle_feeding.h"
#include "core/model/idle_lure.h"
#include "core/model/idle_mimic.h"
#include "core/model/idle_vanish.h"
#include "core/model/idle_fork.h"
#include "core/model/idle_harvest.h"
#include "core/model/idle_relay.h"
#include "core/model/idle_wander.h"
#include "core/model/inventory.h"
#include "core/model/loadout.h"
#include "core/model/move_loadout.h"
#include "core/model/pet.h"
#include "core/model/pet_model.h"
#include "core/model/pet_upgrades.h"
#include "core/model/save.h"
#include "core/model/chromatophore.h"
#include "core/model/isolation.h"
#include "core/model/fragger.h"
#include "core/model/stacker.h"
#include "core/model/tournament.h"
#include "core/net/audit_capture.h"
#include "core/net/handshake_ledger.h"
#include "core/net/network_ledger.h"
#include "core/net/peer_ledger.h"
#include "core/net/pvp_link.h"
#include "core/render/scene_id.h"
#include "core/render/camo.h"          // CamoTarget: the FX_CAMO state the tick keeps
#include "core/render/framebuffer.h"
// ui_state.h ONLY, never a core/ui/*_screen.h: Game holds the ids below as members
// but draws nothing itself, and this header is included by every TU in the engine.
// A screen header belongs in the game_*.cpp unit that calls its draw functions.
#include "core/ui/arch_screen.h"   // ArchRow: the ARCH list's row model
#include "core/ui/ui_state.h"
#include "core/device/platform.h"

namespace mal {

struct SpriteData;
// A flowed reader's row (core/ui/prose_page.h). Named here only as the return type of
// the row builders below, per the include note above.
struct ProseRow;

// EXPL's list view (core/ui/expl_screen.h). Named here only as explView()'s return
// type, per the include note above: the screen header stays in the game_*.cpp units
// that draw and navigate with it.
struct ExplListView;
struct PlayHud;

// How a Game starts. FreshHatch = empty save -> Decryption Hatch (the real
// first boot). Hatched = skip straight to a raised pet (the test/dev
// seam that keeps the tests green without going through the egg).
enum class StartMode { FreshHatch, Hatched };

class Game : private RigShopHost, private ItemsHost, private StatReaderHost,
             private FightScreenHost, private EncounterHost, private VaultHost,
             private ArcadeHost, private CrewHost {
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
    //   Idle/Cursor: resting / a slot focused.
    //   Submenu:     L2 (STAT viewer · ITEMS/MAINT list · placeholder).
    //   Detail:      L3 (item detail · MAINT action).
    //   Process:     a running MAINT process (non-interruptible).
    //   ModalFeeding / ModalLockout: event overlays.
    enum class Nav { Idle, Cursor, Submenu, Detail, Process, ModalFeeding, ModalLockout, ModalLineSelect, ModalEggPick, ModalHatchReveal, ModalEvolve, ModalCSF, Combat, ExploreControl, Encounter, Wifi, Shop, ModShop, WarpPicker, RollbackPicker, RepartitionPicker, CacheYield, BulkYield, PostEncounter, Stacker, Fragger, Isolation, Chroma, Decryption, Cryptogram, ArcadeResult, Tourney, ShiboleetHail, Shiboleet, ShiboleetVerdict, Story, StoryArchive, TipCard, AwayDigest, ItemOffer };

    // ARCH's two L2 screens, the same shape ITEMS uses: the group picker, then the rows
    // behind whichever group was opened. This picker is free (the ITEMS one is bought):
    // a 64-slot shelf cannot be navigated without it.
    enum class ArchScreen : uint8_t { Picker, List };

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

    // How a guardian receives the pet, graded on how much of the CANT it can read
    // (cantFluencyPct, core/model/cant.h). Public so a test can steer/inspect the band a
    // fluency landed in without playing a walk out.
    //
    //   Affront: it will not hear an illiterate pet out, and attacks.
    //   Riddle: the full SHIBOLEET, a riddle drawn in the Cant with three replies.
    //   Boon:    fluent enough that the two simply talk; no riddle at all.
    enum class ShiboleetWelcome : uint8_t { Affront, Riddle, Boon };

    // How a riddle ENDED. Answered is the only one that can buy a sigil; the other two
    // both hand the pet to the guardian's fight, and are distinguished only so the
    // flavor line can tell the truth about which happened.
    enum class ShiboleetReply : uint8_t { Pending, Answered, Wrong, Unanswered };

    // Which slice of the DECRYPTOGRAM pool a roll is allowed to land in. The VAULT
    // wants an unsolved quote (a prize to win) or, once there are none, a solved one to
    // re-run; the arcade cabinet only ever wants a solved one. Public so a test can
    // interrogate the pool without playing a board.
    enum class QuotePick : uint8_t { Unsolved, SolvedOnly, Any };

    // The per-pet, per-slot Attack/Defend STAMP. Unset until the slot first
    // unlocks for this pet (stampSlotKinds), then fixed for good (see
    // pet_.slotKinds's declaration). Public so tests/render can read it without
    // reaching into private state.
    using SlotKind = ::mal::SlotKind;

    bool tick(uint32_t nowMs);
    void onButton(const ButtonEvent& ev);
    void render(Framebuffer& fb) const;

    // --- Inspection (tests / debug / host) ---------------------------------
    PetModel& model() { return pet_.model; }
    const PetModel& model() const { return pet_.model; }
    Inventory& inventory() { return inventory_; }
    const Inventory& inventory() const override { return inventory_; }
    const EventLog& log() const { return log_; }
    // Which page of the mid-combat panel is showing (0 = closed). B cycles it.
    int combatStatsPage() const { return fightScreen_.statsPage(); }
    const ModSlots& modSlots() const { return pet_.mods; }
    const ModPool& modPool() const { return op_.modPool; }
    // The two halves the MODS screen reads together.
    ModHoldings modHoldings() const { return {pet_.mods, op_.modPool}; }
    const MoveLoadout& moveLoadout() const { return pet_.moves; }
    // The stamped kind for `slot`, or Unset if it hasn't
    // unlocked for this pet yet (out-of-range also reads Unset).
    SlotKind slotKind(int slot) const {
        return (slot >= 0 && slot < kMaxMoveSlots) ? pet_.slotKinds[slot] : SlotKind::Unset;
    }
    // The MoveDef::Kind an equip into `slot` must match. Unset defensively reads as
    // Attack. Unreachable in normal play: onTrainList only lets the cursor reach an
    // UNLOCKED slot, and stampSlotKinds() runs on every pet install, so every
    // reachable slot is stamped before the player can ever open its picker.
    MoveDef::Kind slotRequiredKind(int slot) const {
        return slotKind(slot) == SlotKind::Defend ? MoveDef::Kind::Defend
                                                   : MoveDef::Kind::Attack;
    }
    // Content lookup (creatures/items/mods/moves): read-only, for anything that
    // needs to enumerate the catalog (e.g. the 'Pedia state export).
    const ContentRegistry& content() const override { return registry_; }
    const Combat& combat() const { return fight_.combat; }
    // The current fight, or the last one after it ends (core/model/fight.h).
    const Fight& fight() const { return fight_; }
    // The boss gauntlet in play, or nullptr between gauntlets.
    const GauntletRun* gauntlet() const { return gauntlet_ ? &*gauntlet_ : nullptr; }
    int combatXp() const { return pet_.xp; }        // XP banked toward the next level
    int combatLevel() const { return pet_.level; }  // 0-based creature level
    // What this pet actually banks from a raw award: the USB soak, then the Epic dish's
    // rate and Bond together. Shared by addCombatXp and the walk's XP preview.
    int scaleXp(int xp) const;
    int defragCount() const { return pet_.defragCount; }  // this pet's defrags
    // XP needed to climb from the current level to the next: the geometric curve
    // round(kLevelXpBase * (kLevelXpGrowthPct/100)^pet_.level).
    int xpToNextLevel() const;
    // The same curve asked about an ARBITRARY level rather than the one the pet is on.
    // Rollback needs it: shedding a level moves the pet onto a SHORTER rung, and the
    // banked XP has to be re-expressed against that rung rather than thrown away.
    int xpForLevel(int level) const;
    // Earned points in combat stat `i` (0 power · 1 defense · 2 speed · 3 max-Health);
    // sum == pet_.level (the level == total earned points invariant). Out-of-range → 0.
    int levelStatPoint(int i) const {
        return (i >= 0 && i < kLevelStatCount) ? pet_.statPoints[i] : 0;
    }
    // The combat stat the most-recent level-up bumped (0..3), or -1 before the first
    // level-up. STAT's LVL n line reads it to show which stat grew. Used by tests to
    // confirm a level-up.
    int lastLevelUpStat() const { return lastLevelUpStat_; }
    int bits() const { return op_.bits; }
    int bandwidth() const { return bandwidth_; }   // farming resource: see below
    // The farming-pool CAP: the base plus every "Increase Bandwidth" upgrade bought on
    // the Rig Shop (a). Replaces the raw kBandwidthMax everywhere the cap is
    // read (regen ceiling, the badge/PROFILE denominator, the debug clamp).
    int bandwidthMax() const {
        return kBandwidthMax + op_.rig.level[kRigRowBandwidth] * kBandwidthUpgradeStep;
    }
    int bwUpgradeCount() const { return op_.rig.level[kRigRowBandwidth]; }   // a purchases made
    // Minutes of real active time per +1 Bandwidth for the ACTIVE pet: the shared
    // interval less whatever this pet has been fed (Tiramisudo), never below the floor.
    // Per-pet, so the same rig regenerates at different speeds under different pets.
    uint32_t bandwidthRegenMinutes() const {
        const uint32_t bonus = static_cast<uint32_t>(pet_.upgrades.bandwidthRegenMin);
        return bonus >= kBandwidthRegenMinutesPerPoint - kBandwidthRegenMinutesFloor
                   ? kBandwidthRegenMinutesFloor
                   : kBandwidthRegenMinutesPerPoint - bonus;
    }
    // Has this pet had its permanent regen upgrade? The STAT BUFFS page's own question,
    // and the reason a second Tiramisudo is a top-up rather than a second upgrade.
    bool bandwidthRegenUpgraded() const { return pet_.upgrades.bandwidthRegenMin > 0; }
    // n (Rig Shop): the Link Aggregator, the one-time unlock that makes a regen tick
    // pay a PERCENTAGE of the hole instead of a flat point (bandwidthRegenAmount).
    bool linkAggregatorUnlocked() const {
        return op_.rig.level[kRigRowLinkAggregator] > 0;
    }
    // How much ONE regen tick restores: the amount half of regen, where
    // bandwidthRegenMinutes() is the interval half. Flat +1 by default; with the
    // Link Aggregator owned, kLinkAggregatorPct% of what is currently SPENT
    // (bandwidthMax() - bandwidth()), never below that same +1. A small pool
    // regenerates at the base rate, and a deep one refills in proportion to how far
    // down it is. The amount is recomputed every tick, so the refill slows as the
    // pool fills.
    //
    // Bandwidth is whole points, so the slice ROUNDS UP: a 150-point hole pays 2, and
    // the +1 floor is then only ever reached by a hole small enough to deserve it.
    int bandwidthRegenAmount() const {
        if (!linkAggregatorUnlocked()) return 1;
        const int spent = bandwidthMax() - bandwidth_;
        const int slice = (spent * kLinkAggregatorPct + 99) / 100;
        return slice > 1 ? slice : 1;
    }
    // Everything an Epic dish has permanently handed this pet, as one value: what STAT's
    // BUFFS page lists and what the rack freezes (core/model/pet_upgrades.h).
    const PetUpgrades& petUpgrades() const { return pet_.upgrades; }
    // ...and those plus the two arming gates, as the one value anything asking "has this
    // pet already taken that row's once-per-life grant" needs (lifetimeGrantSpent,
    // core/model/pet_upgrades.h). The ITEMS list and detail page read it to draw the
    // once-per-life note beside a row that carries one.
    PetLifetimeGates petLifetimeGates() const override {
        return {pet_.upgrades, pet_.grants.yubiSpent, pet_.grants.shieldSpent};
    }
    int xpRateBonusPct() const { return pet_.upgrades.xpRatePct; }
    // Has THIS pet already spent `d`'s once-per-lifetime effect (defs.h's
    // itemIsOncePerPetLifetime)? The three gates are separate fields because they guard
    // different things; this is the one place that answers the question generically, so
    // a reader (the 'Pedia's pet page), never has to know which flag belongs to which
    // item. False for any row that has no such effect: nothing to have spent.
    bool lifetimeItemSpent(const ItemDef& d) const;
    // Player-level Fragmentation-amount tier (b, save v21): shaves the battle-fatigue
    // frag AMOUNT cap (Game::applyBattleFatigue).
    uint8_t fragAmountTier() const { return static_cast<uint8_t>(op_.rig.level[kRigRowFragReduce]); }
    // Player-level Fragmentation-TRIGGER tier (c, save v22): lowers the
    // battle-fatigue trigger CHANCE (sibling to fragAmountTier's AMOUNT cut).
    uint8_t fragTriggerTier() const {
        return static_cast<uint8_t>(op_.rig.level[kRigRowFragTrigger]);
    }
    // d/e (Rig Shop, save v23): one-time, permanent account unlocks. No
    // tiers, just owned or not. itemTabsUnlocked arms the ITEMS hold-B filter cycle;
    // bulkOpenUnlocked arms the VAULT hold-B bulk-open.
    bool itemTabsUnlocked() const override { return op_.rig.level[kRigRowItemTabs] > 0; }
    bool bulkOpenUnlocked() const override { return op_.rig.level[kRigRowBulkOpen] > 0; }
    // ITEMS Type-Picker (Rig Shop): another one-time unlock. The ITEMS submenu
    // opens on the category tile screen (ItemsView::Picker) instead of straight
    // onto the list, and hold-B's cycle steps up to the finer category axis.
    bool itemPickerUnlocked() const override { return op_.rig.level[kRigRowItemPicker] > 0; }
    // f (Rig Shop, save v31): the Merge Hub one-time unlock. Makes the MRG
    // carousel slot accessible (game_merge.cpp). Doesn't grant any recipe by
    // itself; each recipe is won off a Decryptogram (recipeOwned).
    bool mergeHubUnlocked() const { return op_.rig.level[kRigRowMergeHub] > 0; }
    // g/h (Rig Shop): one-time unlocks gating the auto-arm hooks in
    // Game::autoArmBackupShield. Auto Backup fires at explore-start
    // (startExplore/startDeepWebDive), Continuous Auto-Backup at every resolved
    // explore event (returnToExplore).
    bool autoBackupUnlocked() const { return op_.rig.level[kRigRowAutoBackup] > 0; }
    bool continuousBackupUnlocked() const {
        return op_.rig.level[kRigRowContinuousBackup] > 0;
    }
    // i (Rig Shop): Disk Maintenance tier (0 = not bought). Level N's threshold/cost
    // multiplier live on kDiskMaintenanceThreshold/kDiskMaintenanceCostMult
    // (game_rig_shop.h), read by Game::maybeAutoDefrag.
    int diskMaintenanceLevel() const { return op_.rig.level[kRigRowDiskMaintenance]; }

    // --- SERVICES: the owned rows that run on their own, and their switches ---------
    // RigAccount (core/app/operator.h) answers which are owned, running and at ASK.
    using ServiceMode = mal::ServiceMode;   // game_rig_shop.h
    // Flip one owned service, or, on an askable one, step it YES -> ASK -> NO -> YES.
    // A row that isn't an owned service has no switch to throw, so this is inert on it
    // rather than storing a bit nothing reads.
    void toggleRigService(int row) override;
    // The SHOP is three screens behind one slot, the shape CREW's views take: the
    // storefront list, the SERVICES switchboard the head slot opens, and one service's
    // info page (what it does and what a run of it costs), which a HELD B opens from
    // the board, since a switch thrown months after the purchase is the one a player
    // no longer remembers the terms of.
    using ShopView = mal::ShopView;
    ShopView shopView() const { return rigShop_.view(); }
    // The SHOP list's cursor: a RigRow, or kRigSlotServices on the head slot. A slot
    // index. It differs from the row position by every row the list filters out.
    int shopSlot() const { return rigShop_.slot(); }
    int rigServiceRow() const { return rigShop_.serviceRow(); }   // cursor within SERVICES
    // Mod Storage tier -> how many spare copies of ONE mod the pool will hold
    // (modCopyCap, tunables). Every grant runs through it, so a player who wants a
    // bench of a favourite mod buys the room for it rather than accumulating copies
    // nobody counted.
    int modStorageCap() const { return modCopyCap(op_.rig.level[kRigRowModStorage]); }
    // The Operator record, read by const reference: the player across every pet.
    const Operator& operatorRecord() const override { return op_; }
    // The Settings record, read by const reference: the device's preferences.
    const Settings& settingsRecord() const { return settings_; }
    // Price of the NEXT "Increase Bandwidth" upgrade (climbs per purchase, a).
    int bandwidthUpgradeCost() const { return op_.rig.costFor(kRigRowBandwidth); }
    // rig upgrades: player-level, persisted (save v27), survive lifecycles
    // like bwUpgradeCount(). All three price off RigAccount::costFor so balance is a
    // start-price/max-purchase edit on the row (game_rig_shop.h), not new code.
    int rackSlotUpgradeCount() const { return op_.rig.level[kRigRowRackSlot]; }
    int rackSlots() const { return kRackSlots + op_.rig.level[kRigRowRackSlot]; }  // ARCH capacity
    int rackSlotUpgradeCost() const { return op_.rig.costFor(kRigRowRackSlot); }
    int scrapingClusterLevel() const { return op_.rig.level[kRigRowScraping]; }
    int combatBitsBonusPct() const {
        return op_.rig.level[kRigRowScraping] * kScrapingClusterPctPerLevel;
    }
    int scrapingClusterCost() const { return op_.rig.costFor(kRigRowScraping); }
    int dataMiningLevel() const { return op_.rig.level[kRigRowDataMining]; }
    int cacheBitsBonusPct() const {
        return op_.rig.level[kRigRowDataMining] * kDataMiningPctPerLevel;
    }
    int dataMiningCost() const { return op_.rig.costFor(kRigRowDataMining); }
    // Applied at every combat/cache Bits payout site so the % bonus is one call, not
    // duplicated arithmetic per site. Integer `bits * pct / 100` truncates to 0 on
    // small grants at low levels (e.g. a 10-Bit Common cache at 2-8% DataMining never
    // moves: the purchase would look inert on-device even though the tier is live),
    // so any ACTIVE bonus (pct > 0, bits > 0) is floored at +1 Bit rather than +0.
    int applyCombatBitsBonus(int bits) const {
        return applyBonusPct(bits, combatBitsBonusPct());
    }
    int applyCacheBitsBonus(int bits) const {
        return applyBonusPct(bits, cacheBitsBonusPct());
    }
    // j (Rig Shop): Passive XP Farming level. Game::tick reads this directly
    // whenever Hunger decays (game_core.cpp), granting level XP per hunger point
    // lost while Hunger stayed above hungerXpThreshold().
    int hungerXpRateLevel() const { return op_.rig.level[kRigRowHungerXpRate]; }
    // k (Rig Shop): the Hunger threshold row j's passive XP still counts above.
    // Level 0 sits at the row's base threshold; each level lowers it.
    int hungerXpThreshold() const {
        return kHungerXpBaseThreshold - op_.rig.level[kRigRowHungerXpWindow] * kHungerXpWindowStepPct;
    }
    // l (Rig Shop): +kCombatXpPctPerLevel%% combat XP per level, active only
    // while CURRENT Hunger is at or above combatXpWindowThreshold(). A fed and
    // happy pet fights sharper. 0 whenever the pet has let Hunger slip below it.
    int combatXpBonusPct() const {
        if (!pet_.creature || pet_.model.hunger() < combatXpWindowThreshold()) return 0;
        return op_.rig.level[kRigRowCombatXpPct] * kCombatXpPctPerLevel;
    }
    // m (Rig Shop): the Hunger threshold row l's boost stays active above.
    int combatXpWindowThreshold() const {
        return kCombatXpBaseThreshold - op_.rig.level[kRigRowCombatXpWindow] * kCombatXpWindowStepPct;
    }
    // Applied at real-fight XP payout sites (wild win / boss / Sim-Battle). The
    // Sinkhole bypass and Wi-Fi discovery XP grants skip it because neither is
    // a fight. Shares applyBonusPct's floor-at-+1 rounding with
    // applyCombatBitsBonus/applyCacheBitsBonus.
    int applyCombatXpBonus(int xp) const {
        return applyBonusPct(xp, combatXpBonusPct());
    }
    // Which top-level face is showing. A+C flips PET ↔ HACKER at the top level.
    enum class Face { Pet, Hacker };
    Face face() const { return face_; }
    int exploreSteps() const { return walk_.steps; }      // steps this explore run
    bool exploreActive() const { return walk_.active; }   // explore-mode running
    int exploreStreak() const { return walk_.streak; }    // current win-streak
    int exploreSector() const { return walk_.sector; }    // armed area (or last)
    int exploreSub() const { return walk_.sub; }          // armed sub-area
    // The line under the walk's badge: what the last resolved event left behind
    // (drawExploreScreen). Empty between events.
    const char* exploreFlavor() const { return walk_.flavor; }
    bool autoProgress() const { return autoProgress_; }     // hands-off ladder stepping
    // How many more wins auto-progress needs before it steps off the armed rung:
    // kExploreStreakToBoss minus the streak, floored at 0. What the walk badge counts
    // down while a CLEARED sub-area is being re-farmed (ExploreBadgeMode::AutoNext), and
    // meaningless with the mode off, since then nothing steps but the player.
    int winsToNextStage() const {
        const int left = kExploreStreakToBoss - walk_.streak;
        return left > 0 ? left : 0;
    }
    // What a WILD WIN on the armed rung pays right now, as a percentage of the flat base
    // (kWildWinXpReward): 100 = a level-matched fight, under it the pet has outgrown the
    // rung, over it the walk is punching up or a bonus is live. Everything that scales a
    // wild win's XP folds in. The level-difference curve (wildWinXp) against the rung's
    // own depth level, the Well-Fed window (applyCombatXpBonus), an armed evolution soak,
    // and Profilerole's permanent rate, so it is the whole payout in one number. Read by
    // the walk's A+C control overlay (drawExploreControl).
    int exploreXpEfficiencyPct() const;
    // What ONE wild win in `sector` pays before the level-difference curve scales it: the
    // flat base on every rung of the ladder and in the dive, and a multiple of it in the
    // CRAWL, whose runs are too short to ever reach the paying end of a depth curve
    // (kDarkWebXpPct, darkweb_crawl/area.h). Keyed by the sector rather than by
    // inDarkWebCrawl() so a walk that has since been stopped still prices as the zone it
    // was aimed at, the same reason exploreXpEfficiencyPct reads the sector directly.
    static int wildWinXpBase(int sector);
    // the endless DeepWeb Dive (terminal zone) is armed. It's an explore mode
    // on the virtual kDeepWebSector: no sub-area/boss, enemies scale to the pet.
    bool inDeepWebDive() const {
        return walk_.active && walk_.sector == kDeepWebSector;
    }
    // ...and the DARKWEB CRAWL, the terminal one. Same idle-background walk on its own
    // virtual sector, but every fight in it runs with the A+C picker scrambled,
    // because the Silk Lode's apex rider stays on (darkweb_crawl/area.h).
    bool inDarkWebCrawl() const {
        return walk_.active && walk_.sector == kDarkWebSector;
    }
    // Either endless zone: the two share the walk, the depth streak and the "a loss ends
    // the run" rule, and differ in what they field and what they take from you. Callers
    // that mean "no sub-area ladder here" want this rather than one of the two above.
    bool inEndlessZone() const { return inDeepWebDive() || inDarkWebCrawl(); }
    // The crawl's unlock: the area whose shrine holds the portal has been CLEARED.
    bool darkWebUnlocked() const {
        return op_.world.areaCleared(areaIndexById(kDarkWebUnlockAreaId));
    }
    int bestDarkWebDepth() const { return pet_.bestDarkWebDepth; }
    // The DeepWeb Dive's unlock: has the ladder reached kDeepWebUnlockAreaId? The SAME
    // linear gate every area answers to (explSectorOpen over the cleared areas), asked of
    // one named rung, so the dive needs no unlock flag of its own. The arena works the
    // same way. An id that resolves to nothing leaves the zone locked rather than opening
    // it by accident, which is the safe direction for a typo to fail in.
    bool deepWebUnlocked() const {
        const int a = areaIndexById(kDeepWebUnlockAreaId);
        if (a < 0) return false;
        if (a == 0) return true;
        return op_.world.areaCleared(a - 1);
    }
    // is hands-off auto-explore paused because the pet is too fragmented
    // (battle fatigue climbed frag into the danger band)? The mode stays armed; a
    // defrag drops frag back below the gate and stepping resumes.
    bool exploreAutoPausedByFatigue() const {
        return walk_.active && pet_.model.fragmentation() >= kBattleFatigueAutoPauseFrag;
    }
    // Has sub-area (`area`,`sub`)'s boss been UNLOCKED by a 10-win streak? A per-sub
    // flag, persisted (save v13). The streak is separate and not saved.
    bool subBossUnlocked(int area, int sub) const { return op_.world.subBossReady(area, sub); }
    // Has sub-area (`area`,`sub`)'s boss been BEATEN? Durable (save v13).
    bool subCleared(int area, int sub) const { return op_.world.subAreaCleared(area, sub); }
    // Is the AREA boss reachable? All 5 sub-areas cleared, area not yet.
    bool areaBossReady(int area) const;

    // --- ROCK THE DOCK: the operator bracket (game_tourney.cpp) --------------
    // Which of the arena screen's three READS is showing. One Nav, three views,
    // because all three are the same screen answering different questions about the
    // same run, and a Nav per reader would put the run's lifecycle in three places.
    //   Bracket  the field: who is left, at what level, and who is next
    //   Scout    the focused entrant's full kit, on the LOADOUT page's own flow
    //   Brief    the arena explaining itself (content_tournament.h's kTourneyBrief)
    enum class TourneyView : uint8_t { Bracket, Scout, Brief };

    // What the STORY reader hands back to when the chapter it is showing runs out
    // (game_story.cpp). A chapter fires IN FRONT OF something (the encounter that
    // triggered it, the gauntlet it introduces), so the reader stores what it
    // interrupted; the nav state it leaves cannot say.
    //
    //   Walk:      back to the idle habitat, explore-mode still running. What every
    //               OUTRO hands back to, since the thing it followed is already over.
    //   Encounter: the wild encounter whose first-in-this-zone roll opened the chapter.
    //   AreaBoss:  the area gauntlet the chapter was the threshold of (storyThenArea_).
    //   Archive: opened from the CHAPTERS list with nothing happening on the walk. C
    //               goes back to the list.
    enum class StoryThen : uint8_t { Walk, Encounter, AreaBoss, Archive };
    // A bracket is in play (core/model/tourney_run.h), verdict included. With none, the
    // seed reads 0, the bitmask and round 0, and the phase Ready.
    bool tourneyRunning() const { return tourney_.has_value(); }
    uint32_t tourneySeed() const { return tourney_ ? tourney_->seed : 0; }
    uint8_t tourneyAlive() const { return tourney_ ? tourney_->alive : 0; }  // survivors
    int tourneyRound() const { return tourney_ ? tourney_->round : 0; }      // 0-based
    TourneyPhase tourneyPhase() const {
        return tourney_ ? tourney_->phase : TourneyPhase::Ready;
    }
    // The operator's own slot, and the slot they face next (-1 once the run is over).
    int tourneySlot() const;
    int tourneyOpponentSlot() const;
    // The opponent as a built fighter: resolved once when a round opens, so the
    // bracket screen can name its species and Exploit without walking the content
    // tables every repaint.
    const TourneyFighter& tourneyOpponent() const;
    TourneyView tourneyView() const { return tourneyView_; }
    int tourneyCursor() const { return tourneyCursor_; }
    // Is a game engine's mid-play RULES overlay open right now? (game_arcade.cpp)
    bool gameBriefOpen() const { return gameBriefOpen_; }
    // True when the hatch opened it (the first time a line's egg is laid on this
    // device), false when the chord did. That reading leads with what the egg stakes,
    // B walks it forward and off its last page into play, and C skips straight to the
    // board.
    bool gameBriefIntro() const { return gameBriefIntro_; }
    // The overlay's row model (core/ui/prose_page.h), built on demand. Public so a gate
    // can assert what the page SAYS instead of reading it out of pixels.
    std::vector<ProseRow> gameBriefRows() const;

    // --- One-time onboarding tips (game_onboard.cpp) ---------------------------
    // Taught once per operator, so a new egg does not repeat the button lesson.
    using Tip = mal::Tip;   // core/app/operator.h
    bool tipSeen(Tip t) const;
    void markTipSeen(Tip t);
    // Which carousel slots the pet needs visited right now, as drawCarousel's masks
    // (bit i = carouselSlots()[i]): ITEMS while Hunger is out of its OK zone, MAINT
    // while Fragmentation is (or a Replication Ghost needs scanning out), GAMES while
    // Happiness is. `urgent` is the subset in the Critical band. An egg needs nothing.
    void careAttention(unsigned& attention, unsigned& urgent) const;
    // Does the bag hold anything that resolves a Lockout (itemResolvesLockout)? The
    // Lockout opens on its Bits row without, and says where food comes from.
    bool lockoutFoodHeld() const;
    // A pet at 5/5 errors: how much of its kCsfDyingGraceMs window is left, or -1 when it
    // is not failing. The habitat banner and the Lockout both count it down.
    int32_t failingLeftMs() const;
    // Is there a Yubi-Cookie in the bag this pet can still eat (once per pet)? The one
    // item that brings a failing pet back below 5/5.
    bool yubiReady() const;
    // Would eating the Yubi-Cookie SAVE this pet right now? Only while it is failing
    // (5/5: errors cap there, so the cookie's -1 always brings it back under) and only
    // with one this pet can still eat. Anywhere else the once-per-pet cookie would be
    // spent on an error the pet can live with, so nothing offers it.
    bool yubiWouldSave() const { return failingLeftMs() >= 0 && yubiReady(); }
    bool feedYubiToSave();
    // Feed it, through the real Use path (gate, effects, the feeding modal), from the
    // FAILING banner's B or the Lockout's feed row. False if it would not save the pet.
    // The one-time TIP CARD (Nav::TipCard): which tip it is showing, and the page it
    // shows as prose rows. Public so a gate can assert what a card SAYS.
    Tip tipCard() const { return tipCard_; }
    std::vector<ProseRow> tipCardRows() const;
    // CFG > DEVICE > TIPS: the cards this device has shown, in a fixed order, to read
    // again (game_onboard.cpp). A card not yet shown is not listed, so the list never
    // spoils it.
    std::vector<Tip> replayableTips() const;
    static const char* tipTitle(Tip t);
    // WHILE YOU WERE AWAY (game_onboard.cpp): what happened since the last button press,
    // shown on the first press after kAwayDigestMs of powered-on quiet, but only when
    // something did. The tally is runtime-only: a reboot starts a fresh one.
    struct AwayTally {
        int hungry = 0;          // Lockouts fired (FED hit zero)
        int missed = 0;          // ...of them expired unanswered
        int errors = 0;          // care errors charged (addCareMistakeShielded)
        int won = 0, lost = 0;   // wild fights on the walk
        int caches = 0;          // sealed caches found
        const CreatureDef* hatched = nullptr;   // the egg hatched into this
        const CreatureDef* evolved = nullptr;   // the pet evolved into this
    };
    const AwayTally& awayTally() const { return away_; }
    // The tally's rows as label/value pairs, the order they are drawn in. Public so a
    // gate can assert what the summary SAYS.
    struct AwayRow { const char* label; char value[20]; };
    std::vector<AwayRow> awayRows() const;
    // Dev/test seam: the summary is off on the Hatched seam (a gate that ticks hours and
    // then presses a button means the press), and a gate about it turns it back on.
    void debugSetAwayDigest(bool on) { awayDigestOn_ = on; }
    // Dev/test seam: forget every tip, so a gate can meet one on a seam pet.
    void debugClearTips() { for (uint8_t& b : op_.reveals.tipsSeen) b = 0; }
    // The chance, 0..100, that a QUICK defrag or an AV scan fails at the current
    // Fragmentation: what rollMaintSuccess rolls against and MAINT prints.
    int maintFailPct() const;
    // The two readers' row models (core/ui/prose_page.h), built on demand: both walk
    // the content tables, so they are called on a press or a repaint, never held.
    // Public so a gate can assert what a sheet SAYS instead of reading it out of pixels.
    std::vector<ProseRow> tourneyScoutRows() const;
    std::vector<ProseRow> tourneyBriefRows() const;

    // --- STORY (game_story.cpp) --------------------------------------------- The
    // chapter the reader is showing (nullptr = none), and where it is up to. The
    // window pair is what the page's "2/4" is drawn from and what a gate asserts a
    // skip against; the reader's own flow decides both (core/ui/prose_page.h). Named
    // for the state and not for the type, so it does not collide with the content
    // layer's own storyChapter(sector, beat) lookup (core/content/story.h). The two
    // answer different questions, and the compiler would not catch one called for
    // the other.
    const StoryChapterDef* currentStoryChapter() const { return storyChapter_; }
    int storyWindow() const;
    int storyWindows() const;
    // Has this device read `chapter`? The read-set is player-level and persisted, so a
    // chapter fires once per device and a new egg does not replay the premise.
    bool storyRead(const StoryChapterDef* chapter) const;
    // How many chapters the walk has written so far. The CHAPTERS row shows it, and
    // it decides whether that row can be entered.
    int storyChapterCount() const;
    // The chapter flowed as prose rows, built on demand like every other reader's
    // model. Public so a gate can assert what a page SAYS rather than read it out of
    // pixels.
    std::vector<ProseRow> storyRows() const;
    WifiOutcome wifiOutcome() const { return wifiOutcome_; }   // rolled sub-outcome
    // WHICH of the rival's moves beating it could teach this pet: one bit per index
    // into its kit. Every part of the screen that asks uses this. The combat STAT
    // panel's KIT page marks each set bit in its gutter (RivalPrizes,
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
    // The same question the KIT page's prize gutter answers, asked of a PLACE instead of
    // the thing in front of the pet: how many moves this pet could still learn in a zone
    // of the EXPL ladder. The content half is the zone's move pool (subAreaMovePool and
    // friends, core/model/combat.h) and the pet half is moveIsTeachable, the drop roll's
    // own filter, so a count here never promises a move the roll would refuse.
    //
    // What it buys is the answer to "where do I go next", which nothing on the device
    // said: a cleared area is still the only place some of its moves are findable, and a
    // ladder of CLEARED rows gave a player no way to tell that one from the one it has
    // taught everything it knows. The EXPL list draws each as a "+n" lead on its row
    // (core/ui/expl_screen.h's ExplListView).
    //
    // A zone with no pet (an empty rack) counts 0.
    int areaMovesToLearn(int area) const;
    // The AREA GAUNTLET row's own count, which is not the area's: the gauntlet is five
    // boss rounds and the area boss's own move, and a player standing on that row is
    // asking what THAT fight pays, not what the whole zone holds.
    int areaGauntletMovesToLearn(int area) const;
    int subAreaMovesToLearn(int area, int sub) const;
    int deepWebMovesToLearn() const;
    int darkWebMovesToLearn() const;
    // The on-screen line resolveNetworkDiscovery() set on the last Wi-Fi event
    // (new/familiar/home-turf/empty-queue): "" if none has resolved yet.
    const char* netDiscoveryFlavor() const { return netDiscoveryFlavor_; }
    // The same beat as a value, for the screen's picture of it (and for a gate to
    // assert against without reading the sentence).
    NetDiscovery netDiscovery() const { return netDiscovery_; }
    int networksSeen() const { return op_.networksSeen; }         // lifetime, dedup'd
    // Scan-time sightings queued in RAM, not yet resolved into the SD-backed
    // NetworkLedger by an EXPL Wi-Fi event: how close the pet is to
    // kPendingNetworkQueueCap, past which new sightings are dropped.
    int pendingNetworks() const { return pendingNetworkCount_; }
    // Storefront (item shop AND mod shop share this accessor set: shopIsModShop()
    // says which registry a row's `id` resolves through). `i` is bounds-clamped to
    // "" / 0 for an out-of-range row, so a caller can safely loop `0..shopListingCount()`.
    bool shopIsModShop() const { return shopIsModShop_; }       // false = item shop, true = mod shop
    const char* shopStoreName() const { return shopFront_ ? shopFront_->name : ""; }
    int shopListingCount() const { return shopFront_ ? shopFront_->listingCount : 0; }
    int shopCursor() const { return shopCursor_; }              // A-cycled row selection
    const char* shopListingId(int i) const;                     // raw content id (tests/debug)
    const char* shopListingName(int i) const;                   // resolved display name
    // Resolved concrete-effect text: the listing's own description with its
    // magnitudes substituted in (core/content/effect_text.h). Returned by value
    // because the text is rendered from the row.
    EffectText shopListingDescription(int i) const;
    int shopListingStock(int i) const;                          // remaining THIS visit
    int shopListingBitsPrice(int i) const;
    int shopListingCostCount(int i) const;                      // non-empty extra item costs
    const char* shopListingCostId(int i, int k) const;          // raw content id (tests/debug)
    const char* shopListingCostName(int i, int k) const;        // display name of cost item k
    int shopListingCostQty(int i, int k) const;
    int shopListingCostHave(int i, int k) const;                // held count, for the buy gate
    // How many of the row's own goods the player already holds, and the ceiling that
    // count is measured against (0 = no ceiling). A MOD row reads the un-equipped spare
    // pool against modStorageCap(); an ITEM row reads the inventory stack, which has no
    // cap at all. A capped row shows the pair as "HAVE n/cap", so a storefront answers
    // "have I got this already, and can I take another" before the Bits are spent.
    // STORAGE FULL is the same pair given as the reason a buy is refused.
    int shopListingHeld(int i) const;
    int shopListingHeldCap(int i) const;
    // Is this row's buy available right now? The single answer the till (buyShopItem)
    // and the sign (shopStatusLine) both defer to, so a shop can never refuse a
    // purchase it drew as available, or take Bits for one it drew as refused.
    bool shopRowBuyable(int i) const;
    // ...and the same answer in WORDS for the SELECTED row: "SOLD OUT" /
    // "STORAGE FULL" / "NOT ENOUGH BITS" / "NOT ENOUGH ITEMS" / "B TO BUY". Spelled
    // out rather than coloured so the buy reason survives grayscale (the dual-coding
    // rule); the shop screen prints it verbatim.
    const char* shopStatusLine() const;
    // EXPL sector-clear flags: has sector `idx`'s boss/gauntlet
    // been cleared? Drives the linear unlock (sector N+1 opens when N clears).
    bool sectorCleared(int idx) const {
        return op_.world.areaCleared(idx);
    }

    // Power/battery status pushed from the platform tier (device reads the ADC +
    // CHG_STAT; host leaves it absent). Surfaced on the CFG "BATT" line and as the
    // Hacker face's battery glyph.
    //
    // The percentage is smoothed and the glyph's fill level resolved HERE rather than
    // by each screen, so the two readouts can never disagree about the same battery,
    // and a platform that samples more often than a screen repaints pays for it once.
    // A repaint is requested only when something a screen would actually DRAW moved.
    // A sample that lands on the same number requests nothing, so a free-running
    // sampler cannot drive the ~4fps event-driven redraw.
    void setPowerStatus(const PowerStatus& p) {
        PowerStatus next = p;
        const bool readable = p.present && p.percent >= 0;
        if (readable) {
            next.percent = smoothBatteryPercent(
                power_.present && power_.percent >= 0 ? power_.percent : -1, p.percent);
        }
        const int level =
            readable ? batteryLevelStable(next.percent, batteryLevel_) : -1;
        if (next.present != power_.present || next.charging != power_.charging ||
            next.percent != power_.percent || level != batteryLevel_) {
            dirty_ = true;
        }
        power_ = next;
        batteryLevel_ = level;
    }
    const PowerStatus& powerStatus() const { return power_; }
    // Which ICON_SYS_BATTERY frame the current reading draws as, or -1 when there is
    // no reading to draw (host builds, or a board with the monitor disabled).
    int batteryLevel() const { return batteryLevel_; }

    // microSD presence pushed from the platform tier at boot (device mounts the
    // card + round-trips it; host leaves it absent). Surfaced on the CFG "SD" line
    // so it reflects the real card instead of a hardcoded "ABSENT".
    // The largest single allocation the platform could satisfy right now. 0 means
    // "nobody told us": a host build never sets one, so it never blocks a save there.
    //
    // A save is the biggest allocation the engine makes (~16KB in one piece) and it
    // fires off a timer, so it can land at any moment. The audit capture claims
    // ~70KB as it arms, and a save in that window finds no block big enough. The
    // Arduino build has exceptions off, so that is not a failed save: bad_alloc
    // goes to std::terminate and RESETS THE DEVICE. It can only be avoided.
    //
    // A PROBE rather than a pushed reading, because the save allocates in two stages
    // (build the SaveData, then reserve the blob) and a once-a-loop sample is already
    // stale by the second. Captureless, so it decays to a plain function pointer and
    // core/ stays free of <functional>.
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
    // How many writes in a row the STORE has refused: a save that was built, handed
    // over, and refused by the storage medium (a heap failure does not count).
    // Non-zero means the blob on flash is older than what is in RAM right now, which
    // is the one state that survives a reboot as lost progress.
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
    // `out` when Disarmed (capture idle/off). Surfaces the AuditCapture SM phase.
    // "ARM" (armed, listening), "HOT <s>s" (broadcast window), "COOL <m>m" (the
    // ~30-min re-arm cooldown, radio OFF and no capture possible), so the phase is
    // visible instead of inferred from serial. The word carries the meaning, so it
    // reads in grayscale. Host-tested (test_capture_badge_phases).
    bool captureBadge(char* out, unsigned n) const;

    // How many text lines the explore status stack occupies in the idle screen's
    // top-LEFT corner: the EXPL badge, the Bandwidth readout, and the event-flavor
    // tick when there is one. The corner is shared with the SD flash and the
    // armed-buff row, which start below the block this measures, so the two
    // consumers in game_render.cpp stay in step instead of each guessing a height.
    //
    // Not a fixed three: a flavor line names an item it found, and the longest of
    // those needs a second line. Defined in game_render.cpp, beside the column width
    // it measures against and the call that draws the line to the same width.
    int exploreStatusLines() const;

    // Runtime microSD re-check seam (mirrors the netScan opt-in pattern). The
    // device mounts the card once in setup(); a card inserted later is invisible
    // until this one-shot flag asks the device loop to unmount + remount and push
    // a fresh setSdStatus(). CFG sets it (requestSdRecheck); the device tier reads
    // sdRecheckRequested(), acts, then clears it. No-op on host / card-less boards.
    void requestSdRecheck() { sdRecheckRequested_ = true; }
    bool sdRecheckRequested() const { return sdRecheckRequested_; }
    void clearSdRecheck() { sdRecheckRequested_ = false; }

    // FORMAT SD seam (CFG → DEVICE → FORMAT SD). The operator's yes latches Working;
    // the device tier sees sdFormatRequested(), stands down everything with a file
    // open on the card, formats it FAT32, remounts, pushes a fresh setSdStatus() and
    // reports through finishSdFormat(), which lands Done or Failed for the screen.
    // Latched rather than pulsed for the reason travel sleep is: the device tier may
    // decline a pass (an update job is writing to the card) and be asked again.
    // Host and browser tiers have no card, so they answer finishSdFormat(false).
    void requestSdFormat() { sdFormatState_ = SdFormatState::Working; dirty_ = true; }
    bool sdFormatRequested() const { return sdFormatState_ == SdFormatState::Working; }
    void finishSdFormat(bool ok) {
        sdFormatState_ = ok ? SdFormatState::Done : SdFormatState::Failed;
        dirty_ = true;
    }
    SdFormatState sdFormatState() const { return sdFormatState_; }

    // Travel-mode seam (CFG → DEVICE → TRAVEL MODE): a deliberate indefinite pause.
    // Set here, acted on by the device tier, which powers the radio down, lands a save
    // and deep-sleeps the SoC until the wake chord.
    //
    // No clock to freeze on this side. A deep sleep on this board is a reset, so a wake
    // re-enters through the save, and every engine clock is already reboot-safe by one
    // of three routes: rebased against nowMs_ on load (pet_.stageEnteredMs, lastModelMs_),
    // stored as a REMAINING count rather than a deadline (pet_.bootHatchRemainMs), or
    // anchored on lifetimeUptimeMs(), which is awake time (pet_.armed.backupShieldUntilMs,
    // pet_.dyingElapsedMs). Nothing observes wall time (the board carries no RTC), so time
    // spent asleep is never credited.
    //
    // Latched rather than pulsed: the device tier may have to hold it for a moment (an
    // OTA image on trial must commit before a reset is safe). The host tier clears it
    // without sleeping.
    void requestTravelSleep() { travelSleepRequested_ = true; dirty_ = true; }
    bool travelSleepRequested() const { return travelSleepRequested_; }
    void clearTravelSleep() { travelSleepRequested_ = false; dirty_ = true; }

    // Write the current state now, reporting whether it landed. Unconditional: it
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
    int hackerRank() const { return op_.hackerRank; }              // (unbounded)

    // Audit-mode passive Wi-Fi discovery scan. Runtime opt-in, persisted (save
    // v5), default OFF, because authorized use needs an explicit choice.
    //
    // True while the CREW screen is the open L2: the home-network picker's live half
    // depends on the scan actually running, so radioScanWanted() below reads this.
    // Derived from nav state rather than a latch, so it can never be left stuck on.
    bool crewScreenOpen() const {
        return face_ == Face::Hacker && nav_ == Nav::Submenu &&
               enteredHackerId() == HackerSlotId::Crew;
    }
    // What the RADIO should do right now, as opposed to netScanEnabled()'s persisted
    // player opt-in: the passive discovery scan also runs, unconditionally, while the
    // CREW screen is open, so picking a home network never needs a detour into CFG to
    // arm the scan. Closing the screen returns the radio to the mode CFG sets. This
    // raises the SCAN only; the handshake CAPTURE opt-in is untouched and stays
    // strictly explicit. The RadioArbiter still decides who actually owns the radio,
    // so a live AP or capture owner continues to stand this down.
    bool radioScanWanted() const { return netScanEnabled_ || crewScreenOpen(); }
    // The persisted CFG opt-in itself: what the AUDIT row shows and the save stores,
    // and the baseline radioScanWanted() reads. registerNetwork() is how a heard AP
    // feeds back in from the device tier's WiFi.scanNetworks().
    bool netScanEnabled() const { return netScanEnabled_; }
    void setNetScanEnabled(bool on);

    // Pet-to-pet LINK (game_peers.cpp, core/net/peer_link.h). Runtime opt-in,
    // persisted (save v37), default OFF, and separate from the audit scan: that
    // toggle consents to listening, this one to broadcasting the operator's
    // HackerTag, pet and crew to anything in range.
    //
    // True while the PEERS screen is the open L2. Discovery there must be immediate
    // (two people holding their devices together shouldn't wait on a duty cycle), so
    // the screen arms the radio itself, the same way the CREW picker arms the scan.
    // Derived from nav state rather than a latch, so it can never be left stuck on.
    bool peersScreenOpen() const {
        return face_ == Face::Hacker && nav_ == Nav::Submenu &&
               enteredHackerId() == HackerSlotId::Peers;
    }
    // True while the LINK screen is the open L2: the duel surface (game_pvp.cpp).
    // Arms the radio for the same reason PEERS does, and more sharply: two people are
    // standing next to each other trying to start a fight.
    bool linkScreenOpen() const {
        return face_ == Face::Hacker && nav_ == Nav::Submenu &&
               enteredHackerId() == HackerSlotId::Link;
    }
    // What the LINK RADIO should do right now, as opposed to linkEnabled()'s persisted
    // opt-in: announcing also runs while PEERS or LINK is open, then stops on exit, and
    // for as long as a duel session is in flight. The handshake and the finished
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
    // usual timers would power down the radio the operator is waiting on, so the screen
    // would starve itself. A live duel counts for the same reason: the fight is playing
    // out on two screens and neither may sleep mid-round. Derived from nav every frame,
    // so the normal timeouts resume on exit. A running update job counts too, and is the
    // one entry here NOT derived from a screen: it holds the association itself, so the
    // panel must stay up to report a download the operator is watching.
    bool radioScreenOpen() const {
        return crewScreenOpen() || peersScreenOpen() || linkScreenOpen() ||
               pvpSessionLive() || netConnectWanted();
    }
    bool linkEnabled() const { return linkEnabled_; }
    void setLinkEnabled(bool on);

    // Whether a home network is STORED, pushed from the platform tier at boot and
    // again whenever the setup page writes one (platform/esp32/net_credentials.h).
    // Runtime-only and deliberately just a bool: the engine must never hold, render
    // or serialise the credentials themselves. A stored network means the device can
    // connect. It connects only for a job the operator started (see
    // netConnectWanted).
    void setNetProvisioned(bool on) {
        if (netProvisioned_ == on) return;
        netProvisioned_ = on;
        dirty_ = true;
    }
    bool netProvisioned() const { return netProvisioned_; }

    // What platform/esp32/net_sta.h asks each loop, and what the arbiter reads to pick
    // the Update owner. ONE ASKER: the device joins a network for exactly as long as an
    // operator-started job needs it, with no standing permission surviving the job or a
    // reboot. Latched rather than nav-derived, because a fetch that died the moment the
    // menu collapsed could never finish. When it lets go the arbiter re-resolves and the
    // radio falls back to whichever toggle was already on.
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
    // netProvisioned(): the URL itself never enters the engine. A device with no
    // source doesn't get offered a check it cannot perform.
    void setUpdateSourceKnown(bool on) {
        if (updateSourceKnown_ == on) return;
        updateSourceKnown_ = on;
        dirty_ = true;
    }
    bool updateSourceKnown() const { return updateSourceKnown_; }

    // The manifest address itself, pushed from the platform tier
    // (platform/esp32/update_source.h) at boot and again whenever the 'Pedia's
    // user section rewrites it. Display only (the engine never fetches), but it
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
    // only: every "is it newer" comparison is made device-side against the packed
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
    // and somewhere to look. Both are setup. Pressing the button is the consent, so
    // there is no consent bit to arm first. The UPDATES screen reads this to say
    // which piece is missing instead of offering a button that would silently do
    // nothing.
    bool updateCheckReady() const { return netProvisioned_ && updateSourceKnown_; }

    // Ask what is published. Raises the connection itself and seeds the CONNECTING
    // step the screen reports, because the radio comes up only for the job, with
    // nothing to switch on before it or left on after.
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
    // cancelled). Leaves a completed job's result alone, because the screen is
    // showing that verdict to the operator.
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
    // services, so it is latched (leaving the screen does not end it) and must
    // always reach a terminal outcome, since nothing else releases the radio.
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

    // What this device broadcasts about itself: filled from the live pet/crew/tag.
    // The device tier encodes it (encodePeerHello) and beacons it; nothing here
    // touches the radio.
    void buildPeerHello(PeerHello* out) const;

    // Note a HELLO frame heard from `mac`. Decodes and validates it (a malformed or
    // foreign frame is dropped), refreshes the in-range snapshot, and (only on the
    // edge where a peer ARRIVES rather than merely keeps beaconing) counts a meeting
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
    // Fill `rows` with every known operator: the live snapshot merged over the
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
    // snapshot is who is in range this second, and LINK will only duel the latter.
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

    // The open duel session (core/model/pvp_session.h), or Idle when there is none.
    PvpPhase pvpPhase() const { return pvp_ ? pvp_->phase : PvpPhase::Idle; }
    bool pvpActive() const { return pvp_.has_value(); }
    // A session that still NEEDS the air: mid-handshake, or a fight whose host is still
    // repeating its START. Result is deliberately excluded: a verdict left on screen
    // must not pin the radio (and, through radioScreenOpen, the panel) awake for as
    // long as nobody presses a button.
    bool pvpSessionLive() const { return pvp_ && pvp_->phase != PvpPhase::Result; }
    // True while a duel is actually resolving, which is what makes the Combat screen
    // suppress the A+C Exploit picker and the C flee: both would pause or end THIS
    // device's copy of a fight the other device is still playing out.
    bool pvpFighting() const { return pvpPhase() == PvpPhase::Fighting; }
    // Which side of Combat the local pet occupies. The HOST is always player_ on both
    // devices (core/model/pvp_battle.h), so the guest's own pet is the enemy_ row and
    // the combat screen relabels rather than resolving a mirrored fight.
    bool pvpLocalIsHost() const { return pvp_ && pvp_->host; }
    // The same fact in the combat screen's terms (CombatSides::localIsEnemySide): the
    // one fight where the local pet is not Combat's player_ slot is a duel this device
    // guests. Shared by the Combat screen and by anything that has to bind a resolved
    // turn to the LOCAL seat rather than to Combat's own.
    bool combatLocalIsEnemySide() const { return pvpFighting() && !pvpLocalIsHost(); }
    const char* pvpOpponentTag() const { return pvp_ ? pvp_->peerTag : ""; }
    // The finished duel's verdict from the LOCAL operator's point of view. Only
    // meaningful in PvpPhase::Result.
    bool pvpLocalWon() const { return pvp_ && pvp_->won; }
    // Why the last session ended without a fight (empty = it didn't). Rendered by the
    // LINK screen so a refusal never reads as a hang.
    const char* pvpStatusText() const { return pvpStatus_; }

    // What this device offers as its fighter: species, equipped moves/mods, level and
    // stat points. Also what it feeds its OWN side of the fight through, so the two
    // devices cannot diverge via a local shortcut. False when there is no pet to fight
    // with (an unhatched egg).
    bool buildPvpFighter(PvpFighter* out) const;

    // The keys of every operator in range right now, in snapshot order: what the LINK
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
    // the audit scan/capture down, and handing the radio back to whichever of
    // those is still toggled on the moment the AP is switched off.
    bool apEnabled() const override { return apEnabled_; }
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
    // of two independent toggles, so the radio-cost ladder is explicit and
    // "capture needs scan" cannot be violated:
    //   Off:         radio idle (best battery)
    //   Scan:        passive discovery only (feeds NETS / Hacker Rank)
    //   ScanCapture. Scan + passive handshake->pcap (most battery, authorized use)
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
    // once per heard AP per sweep, dups included). Feeds two independent structures:
    // visibleNetworks_, the in-range snapshot the CREW picker reads, and
    // pendingNetworks_, the reward queue the EXPL Wi-Fi event drains one at a time.
    // Nothing is credited here: uniqueness against the ledger, XP and rewards all
    // happen when resolveNetworkDiscovery() pops it.
    //
    // A network newly ARRIVING in range nudges the egg-hatch clock
    // (accelerateEggHatch); an incubating egg cannot explore, so this is its only
    // accelerator. Returns true if the sighting was newly queued, false if it was
    // already pending: the snapshot updates either way. `bssid` is 6 bytes; an empty
    // `ssid` (hidden network) falls back to a BSSID-derived label.
    bool registerNetwork(const uint8_t* bssid, const char* ssid);
    // How many networks the radio can currently hear (snapshot entries still inside
    // kNetVisibleFreshMs). 0 on host and whenever the scan isn't running.
    int visibleNetworkCount() const;
    // Load the SD-backed network ledger (real-network history + sighting counts)
    // from `path`. Called once at boot by the platform tier after the SD card
    // mounts; a missing/unavailable path just leaves the ledger empty (it then
    // tracks in RAM for that boot only; see NetworkLedger).
    void loadNetworkLedger(const char* path);
    // Load the SD-backed SHAKES handshake-dedup ledger from `path`. Called once at
    // boot by the platform tier after the SD card mounts, the same shape as
    // loadNetworkLedger; a missing path leaves the save-blob fallback in place.
    void loadHandshakeLedger(const char* path);
    // Lifetime count of distinct WPA handshakes captured (SHAKES on CFG). Deduped
    // by BSSID so a repeat capture of a known network never re-credits, until the
    // dedup set rotates that BSSID out at its cap (core/net/handshake_ledger.h).
    int handshakesSeen() const { return op_.cant.handshakesSeen; }
    // SHAKES not yet spent on a sigil of the CANT. A captured handshake is what buys
    // a guardian's answer (game_shiboleet.cpp). The lifetime tally above counts
    // every capture and this counts the ones left to spend; they show together as
    // "unspent/lifetime". Never negative: spending is the only thing that moves it,
    // and it checks first.
    int shakesUnspent() const { return op_.cant.shakesUnspent(); }
    // The letters of the Cant this device can read (core/model/cant.h). The one
    // durable half of the whole guardian system.
    SigilSet cantSigils() const { return op_.cant.sigils; }
    int sigilsKnown() const { return sigilCount(op_.cant.sigils); }
    // The guardian encounter's live state, for the screen and for tests: which band the
    // fluency roll landed in, how the riddle ended, and the riddle itself already drawn
    // in the Cant. `shiboleetReplyText` fills `out` with the reply on shown row `row`.
    ShiboleetWelcome shiboleetWelcome() const { return shibWelcome_; }
    ShiboleetReply shiboleetReply() const { return shibReply_; }
    int shiboleetRow() const { return shibRow_; }
    const char* guardianName() const;
    void shiboleetRiddleText(char* out, int cap) const;
    // The two halves of what the guardian is doing. `shiboleetGreeting` is what it SAYS,
    // already drawn in the Cant like the riddle; `guardianDemeanour` is what the pet can
    // SEE, always plain, so a pet with no sigils still learns something (GuardianLine,
    // area_defs.h).
    void shiboleetGreeting(char* out, int cap) const;
    const char* guardianDemeanour() const;
    void shiboleetReplyText(int row, char* out, int cap) const;
    // The two beats that BRACKET the riddle (Nav::ShiboleetHail, Nav::ShiboleetVerdict).
    // A guardian encounter has three parts: a hail, the riddle, and a verdict on what the
    // pet said.
    //
    // `shiboleetOutcome` is how this meeting came out, folded from the fluency band and
    // the reply into the one axis the content is authored against (GuardianOutcome,
    // area_defs.h). `shiboleetOutcomeSpeech` fills `out` with what the guardian SAYS
    // about it, in the same cipher as the riddle; `shiboleetOutcomeSeen` is what the pet
    // can see it doing, always plain. `shiboleetVerdictLine` is the engine's own plain
    // ledger of what the meeting paid or cost ("+6 HAPPY  -4 FRAG"), and
    // `shiboleetFlavor` is the consequence in words ("LEARNED A SIGIL - 3/26").
    GuardianOutcome shiboleetOutcome() const;
    void shiboleetOutcomeSpeech(char* out, int cap) const;
    const char* shiboleetOutcomeSeen() const;
    const char* shiboleetVerdictLine() const { return shibVerdictLine_; }
    const char* shiboleetFlavor() const { return shibFlavor_; }
    // Whether the verdict hands the pet to the guardian's fight rather than back to the
    // walk. The screen names the button with it, so "B CONTINUE" never turns out to have
    // meant a boss.
    bool shiboleetVerdictFights() const;
    // THE GUARDIAN'S BODY (FX_SWARM, core/render/swarm.h). It has no sprite. It is a
    // flock, and this is the live one: reset when a guardian speaks and stepped on the
    // fast FX clock for as long as the pet is looking at it (game_core.cpp).
    //
    // `guardianFlockMood` is what it is DOING, folded from where the meeting has got to
    // and how it came out. It is a standing disposition, as RENDER_PIPELINE.md requires
    // of an effect that describes a state.
    const Flock& guardianFlock() const { return guardianFlock_; }
    FlockMood guardianFlockMood() const;
    // Which shown row carries the true reply. Exposed for the gates only; no screen
    // or press handler calls it.
    int shiboleetTrueRow() const;
    // Register a handshake the device-tier promiscuous capture just observed +
    // wrote to the .pcap. Always drives the AuditCapture SM hot (a capture DID
    // happen), but only counts + persists once per distinct BSSID. Returns true if
    // newly-credited. `bssid` is 6 bytes. PASSIVE capture only: no deauth/inject.
    bool registerHandshake(const uint8_t* bssid);
    // Read-only query (no mutation, no SM drive) of the same ledger
    // registerHandshake() dedups against: a network already fully captured in a
    // prior session. The device-tier capture sniffer (net_capture.h) checks this
    // BEFORE opening a .pcap file for a BSSID, so a repeat walk past a known AP
    // never mints another file (the fix for the pcap-blowup: a file used to
    // be created once per arm/seal radio session regardless of whether it held
    // anything new). `bssid` is 6 bytes.
    bool handshakeAlreadyCaptured(const uint8_t* bssid) const;
    int allyBuffBattlesLeft() const { return allyBuffBattlesLeft_; }  // friendly-visit buff
    bool lockoutActive() const { return pet_.armed.lockoutActive; }
    Nav nav() const { return nav_; }
    CfgScreen cfgScreen() const { return cfgScreen_; }  // active CFG L3 (tests/inspection)
    // Which QR step is showing. The steps run in the order they happen: join the
    // AP, set up the home network (only while none is stored), open the 'Pedia,
    // so the index of the last one depends on netProvisioned() (pediaQrPages).
    int pediaQrPage() const { return pediaQrPage_; }

    // True while either QR page is showing: the 'Pedia AP steps or the USB
    // flasher's address. The device tier reads this to keep the panel AWAKE (a code
    // is read by a phone, which takes longer than the idle sleep window); the engine
    // likewise suspends its 5s menu auto-defocus here. Both resume normally the
    // moment the player exits the page (C).
    bool qrScreenActive() const {
        return nav_ == Nav::Detail && (cfgScreen_ == CfgScreen::PediaQr ||
                                       cfgScreen_ == CfgScreen::UpdateQr);
    }
    // True while the current screen runs an animation cadence FASTER than the shared
    // ~4fps heartbeat (combat sprite motion, the dissolve/swarm sweep, the arcade boards,
    // the held Decryptogram cursor). Every other screen changes at most once per heartbeat.
    // The device tier reads this to scale the CPU clock: a high-frame-rate screen gets the
    // full clock, an idle/menu one a lower one, since composing a 4fps frame leaves generous
    // headroom (platform/esp32/main.cpp). The flag is set by tickAnimClocks from the very
    // gates that drive those cadences, so a screen that animates faster than the heartbeat
    // is covered by construction, with no separate list to keep in sync.
    bool highFrameRate() const { return fastCadenceActive_; }
    // True while the UPDATES screen is showing: its row list, the install confirm,
    // or a job's progress. The screen is a WAIT: a check associates and fetches on
    // the network's clock, and the verdict it leaves behind is the whole reason the
    // operator opened it. On the standard 5s budget the menu collapses out from under
    // a running job, or throws the answer away seconds after it lands, so this takes
    // the long hands-off budget (kRadioScreenDefocusMs) instead.
    //
    // Kept out of radioScreenOpen(), which also arms a radio: only a job the
    // operator started puts this device on a network (netConnectWanted).
    bool updateScreenOpen() const {
        return nav_ == Nav::Detail && cfgScreen_ == CfgScreen::Update;
    }
    // True while the HackerTag arcade editor is open. Composing a tag one A-press
    // per letter has real pauses in it, and the 5s auto-defocus would throw the
    // half-typed buffer away mid-thought, so the editor holds the menu open the
    // same way the QR page does, and releases it on save or back-out.
    bool tagEditorActive() const {
        return nav_ == Nav::Detail && cfgScreen_ == CfgScreen::HackerTag;
    }
    // Cache-yield reveal inspection (tests): what the opened cache produced, while
    // nav() == Nav::CacheYield; 0 with no reveal up.
    int cacheYieldItemCount() const { return cacheReveal_ ? cacheReveal_->itemCount : 0; }
    int cacheYieldBits() const { return cacheReveal_ ? cacheReveal_->bits : 0; }
    // Bits cost of a Defrag at the pet's current stage (0 on the egg).
    int defragCost() const override {
        // An egg has nothing to defragment, so it is never charged for one.
        if (!pet_.creature || pet_.creature->stage == Stage::BootSector) return 0;
        return rigUpgradeCost(kDefragCostStart, pet_.defragCount, RigCostCurve::kLogStep);
    }
    const CreatureDef* pet() const { return pet_.creature; }   // null while the save is empty (Hatch)
    const Pet& petRecord() const { return pet_; }
    // Where the habitat is currently standing its occupant, in logical px off the
    // shelf anchor (core/model/idle_wander.h). drawHabitat draws from this; it is
    // exposed so the resting motion can be inspected without a framebuffer.
    const IdleWander& petWander() const { return petWander_; }
    // How many ambient copies walk the habitat beside the pet right now. Derived and
    // display-only. Combatant::wormReplicas is wiped at the end of every fight, so a
    // worm at home needs its own idle replica count.
    //
    // It is a question the habitat ASKS rather than a line it tests for, so the
    // Worm's exception lives here on the model side and drawHabitat stays a renderer.
    // Everything that isn't a raised worm answers 0.
    int idleCompanionCount() const;
    // Where companion `i` (0..idleCompanionCount()-1) is standing, same units as
    // petWander(). Each keeps its own stream so the group never marches in step.
    const IdleWander& companionWander(int i) const { return companionWander_[i]; }
    // The burst of copies past the cap that crawls out and dies now and then
    // (core/model/idle_fork.h). Rest for every pet that does not replicate.
    const IdleFork& petFork() const { return petFork_; }
    // The prop a disguising line turns into now and then (core/model/idle_disguise.h).
    const IdleDisguise& petDisguise() const { return petDisguise_; }
    // The data a harvesting line pulls in and holds now and then
    // (core/model/idle_harvest.h).
    const IdleHarvest& petHarvest() const { return petHarvest_; }
    // The mail a skimming line chases down and eats now and then
    // (core/model/idle_feeding.h).
    const IdleFeeding& petFeeding() const { return petFeeding_; }
    // The bait the same line sets between skims (core/model/idle_lure.h).
    const IdleLure& petLure() const { return petLure_; }
    // A shapeshifting line's two tricks: fading into the habitat
    // (core/model/idle_vanish.h) and wiping into another family's creature
    // (core/model/idle_mimic.h).
    const IdleVanish& petVanish() const { return petVanish_; }
    const IdleCamo& petCamo() const { return petCamo_; }
    const IdleMimic& petMimic() const { return petMimic_; }
    // The byte the copies pass along between forks (core/model/idle_relay.h).
    const IdleRelay& petRelay() const { return petRelay_; }
    // DISK DECRYPTION (the Ransomware line's hatch minigame): the code-breaking board,
    // played once, the moment the egg is laid, like the Clutch and the Isolation
    // Protocol. Cracking the key halves what is left of the incubation clock; running out
    // of attempts costs only that bonus. Rules: core/model/disk_decryption.h.
    bool inDecryption() const { return nav_ == Nav::Decryption; }
    const DiskDecryption& decryption() const { return decryption_; }
    // THE DECRYPTOGRAM (game_cryptogram.cpp): the quote board, launched by cashing a
    // Decryptogram in at the Hacker VAULT and, once kQuoteArcadeUnlockWins quotes are
    // solved, from its own arcade cabinet. Rules: core/model/cryptogram.h; the pool:
    // core/content/content_quotes.h.
    bool inCryptogram() const { return nav_ == Nav::Cryptogram; }
    // The board in play (core/model/cryptogram_run.h), or an empty one with none.
    const Cryptogram& cryptogram() const;
    // The roster row (content_quotes.h index) the board is playing, or -1.
    int cryptogramQuote() const { return cryptogramRun_ ? cryptogramRun_->quote : -1; }
    // This device's standing with the quote on roster row `i`. Out-of-range, and any
    // row whose wire is past kQuoteWireCap, read Hard, never played.
    CryptogramTier quoteTier(int i) const;
    // Quotes SOLVED, which is what gates the arcade cabinet and what decides whether
    // the VAULT still has a first-solve prize to offer.
    int quotesSolved() const;
    // Whether the pool has an unsolved, gate-passing quote left. False means a cashed
    // Decryptogram replays a solved one for Bits only.
    bool quotesLeftToWin() const;
    // Is this quote offerable at all: its achievement gate earned, and its solved
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
    // rewards: Phishlet only joins the Phishing pool once DEEPWEB_DEPTH_64 is earned.
    bool hatchProcessUnlocked(const CreatureDef* proc) const;
    // Boot-Sector incubation (redesign): the freshly laid egg sits at idle as
    // the Boot-Sector creature, decrypting over kBootHatchMs. `inEggPhase()` is true
    // while it's still an unhatched egg (Boot stage + a live incubation clock);
    // `bootHatchRemainMs()` is the remaining clock (tests/UI).
    bool inEggPhase() const { return pet_.inEggPhase(); }
    // Can the hatch REVEAL be played on demand? The home stretch of any incubation.
    // Every line's minigame is played when the egg is laid, so this last stretch is
    // common to all of them, and the chord cracks the shell here so the player sees the
    // 8-frame hatch.
    bool hatchRevealReady() const {
        return inEggPhase() && pet_.bootHatchRemainMs <= kHatchRevealMs;
    }
    // "The egg can be cracked open now": what the idle prompt advertises with the
    // flashing Exploit pip, and what the chord acts on.
    bool eggCrackable() const { return hatchRevealReady(); }
    int hatchRevealFrame() const;   // the one-shot's current frame (tests/render)
    uint32_t bootHatchRemainMs() const { return pet_.bootHatchRemainMs; }
    // Shaves `ms` off the incubation clock (floors at 0); a no-op outside the egg
    // phase. Walk/network steps use this to advance a laid egg, and it's also the
    // seam dev_config.h's DEV_EGG_TIMER_MS uses to fast-forward a fresh boot.
    void accelerateEggHatch(uint32_t ms);

    // --- Clutch Pick (the Phishing line's hatch minigame) ------------------ A
    // Phishing egg is laid into a raft of identical decoys, and only the live
    // one's swimmer animates: motion is the only tell, so the puzzle survives
    // grayscale. The player halves the clutch kEggPickRounds times (A = left/top,
    // C = right/bottom, B = commit); finishing with the live egg still inside
    // halves the incubation clock, missing it costs nothing but the bonus. The rules
    // are the EggPick run (core/model/egg_pick.h); the raft and its draw live in
    // game_eggpick.cpp. With no pick on screen these read 0 / false.
    bool inEggPick() const { return nav_ == Nav::ModalEggPick; }
    int eggPickRound() const { return eggPick_ ? eggPick_->round : 0; }   // halvings committed
    int eggPickTargetSlot() const { return eggPick_ ? eggPick_->target : 0; }  // (tests)
    bool eggPickResolved() const { return eggPick_ && eggPick_->resolved; }   // reveal is up
    bool eggPickWon() const { return eggPick_ && eggPick_->won; }
    // True while the live egg's slot is inside the surviving span. A miss resolves the
    // pick on the turn it happens, so false here is a pick already lost.
    bool eggPickTargetInSpan() const { return eggPick_ && eggPick_->targetInSpan(); }

    // --- GAMES (the arcade) ------------------------------------------------ The
    // same minigames the hatches and the Defrag run, off their stakes: a cabinet
    // starts one with nothing riding on it and pays a flat participation reward plus
    // a skill bonus (content_arcade.h; game_arcade.cpp is the whole mechanism). The
    // arcade runs the same minigame code. A live ArcadeRun (core/model/arcade_run.h)
    // marks a board as played for fun and changes only its payout and pacing.
    bool inArcadeRun() const { return arcade_.has_value(); }
    // Lifetime tallies per roster row (content_arcade.h's index order). Out-of-range
    // reads 0, so a caller iterating a roster that shrank can't fall off the array.
    int arcadePlays(int i) const override {
        return (i >= 0 && i < kArcadeMaxCabinets) ? op_.tallies.arcadePlays[i] : 0;
    }
    int arcadeWins(int i) const override {
        return (i >= 0 && i < kArcadeMaxCabinets) ? op_.tallies.arcadeWins[i] : 0;
    }
    // The best score a cabinet has ever been played to. Only meaningful where a run
    // HAS a score: a win-or-lose cabinet (the Clutch) reports 0 forever, which is why
    // the cabinet page shows this only when the roster row scores incrementally.
    int arcadeBest(int i) const override {
        return (i >= 0 && i < kArcadeMaxCabinets) ? op_.tallies.arcadeBest[i] : 0;
    }
    // ...by cabinet id, which is what an achievement row keys on (AchSeries::
    // ArcadeCabinetBest). The roster is free to reorder, so a wire-stable question has
    // to be asked by id rather than by index.
    int arcadeBestById(const char* id) const;
    ArcadeDifficulty arcadeDifficulty() const { return arcadeScreen_.difficulty(); }
    // The MAINT action screen's own dial for its played row: reset to EASY each time the
    // screen opens, and separate from the cabinets' so neither moves the other.
    ArcadeDifficulty maintDifficulty() const { return maintDifficulty_; }
    // The MAINT process in play (its board or its process screen), or nullptr.
    const MaintRun* maintRun() const { return maint_ ? &*maint_ : nullptr; }
    // Which minigame a MAINT process's played variant runs (content_arcade.h's defaults).
    ArcadeGameKind maintPlayedGame(MaintKind k) const {
        return k == MaintKind::Defrag ? defragPlayedGame_ : avPlayedGame_;
    }
    // The cabinet list's cursor, as a roster index (content_arcade.h's order). It
    // differs from the row position whenever a cabinet is locked out of the list.
    int arcadeRow() const { return arcadeScreen_.row(); }
    // Is cabinet `i` in the GAMES list right now (ArcadeUnlock)? A locked cabinet is
    // absent, not greyed. Shared by the A-cycle, the render loop and B, so the list can
    // never show a row the cursor skips or start one it never drew.
    bool arcadeCabinetOffered(int i) const override;
    // The roster indices of every offered cabinet, in table order. Returns the count.
    int arcadeOfferedRows(int* out, int max) const override;

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
    // on a shrinking clock. Every pass made in the right skin (and settled, since a
    // repaint takes kChromaSettleMs) is kChromaPassMs off the incubation clock; being
    // caught in the wrong one ends the run with what it already earned. The rules are
    // core/model/chromatophore.h; the screen and the payout are game_chroma.cpp.
    bool inChroma() const { return nav_ == Nav::Chroma; }
    const Chromatophore& chroma() const { return chroma_; }
    // Who the board is actually painting. The pet if its family wears borrowed colours
    // (CreatureLine::wearsBorrowedColours) (which is every route in from a hatch,
    // since the egg on the shelf IS that pet), and otherwise the Metamorphic egg, so
    // an arcade cabinet played by a Ransomware pet still has a plausible subject
    // rehearsing on it. nullptr only if the roster somehow holds neither.
    const CreatureDef* chromaSubject() const;

    int cursor() const { return cursor_; }      // focused carousel slot 0..7
    UiMode uiMode() const { return settings_.uiMode; }
    // How the carousel draws its slots (icons + label / icons / text). Persisted (save
    // v67), so it survives a reboot and an update; setUiMode persists on change.
    void setUiMode(UiMode m);
    void cycleUiMode();
    // CFG > DEVICE > CARE ALERTS: whether the carousel draws its "!" needs-attention
    // marks (Game::careAttention). Persisted beside UI MODE (save v67). Off hides the
    // marks only: the vitals, the lockout and the care cards are unchanged.
    bool careAlerts() const { return settings_.careAlerts; }
    void setCareAlerts(bool on);
    // Screen-brightness level: a 0-based backlight index (0..kBrightnessLevels-1),
    // persisted (save v14). The device tier reads this to drive the backlight PWM; the
    // host tier ignores it. setBrightness clamps + persists.
    int brightness() const { return settings_.brightness; }
    void setBrightness(int level);
    SoundMode soundMode() const { return settings_.soundMode; }
    void setSoundMode(SoundMode m);
    int volume() const { return settings_.volume; }
    void setVolume(int level);
    // The walk's A+C SOUND row: false mutes every Event cue an explore fight would play.
    bool exploreSound() const { return settings_.exploreSound; }
    void setExploreSound(bool on);
    void setSoundOut(ISoundOut* out) { soundOut_ = out; }
    void playSound(Sound s);
    // Ignores the SOUND mode on purpose: the VOLUME picker's sample.
    void previewVolume(int level);
    // The PAL_CORE colour set the interface is drawn in: an index into the generated
    // theme table (0 = base), persisted by NAME (save v64). setThemePick is the ONLY
    // caller of setPalTheme (core/render/palette.h): the applied theme and the stored
    // one move together, so a repaint can never be in a set the save does not name.
    // It refuses a set this device has not unlocked and answers whether it took, the
    // way setBackgroundPick refuses a locked place.
    int themePick() const { return settings_.theme; }
    const char* themeName() const;
    bool setThemePick(int index);
    // Whether row `i` of kThemes (content_themes.h) is unlocked: Start rows always, a
    // Chip row once this device has EVER held its chip. Derived, never stored: see the
    // banner on that header.
    bool themeRowUnlocked(int row) const;
    // Which kThemes row the applied set is: the picker's ACTIVE tag and its opening
    // focus. A lookup, not a cast: the table and the palette are ordered separately.
    int themeRow() const;
    // One bit per kThemes row, for the picker. Built on demand for the same reason the
    // backgrounds' mask is: ownership is derived, so a held copy could only ever go stale.
    uint32_t themesUnlockedMask() const;
    // Has this DEVICE ever held `id`? The ever-held shelf (OperatorShelves), which the
    // 'Pedia already draws from and a palette chip's unlock is read out of.
    bool hasCollectedItem(const char* id) const;
    bool inEvolve() const { return nav_ == Nav::ModalEvolve; }
    // STAT readout (all stages): time remaining until the next evolution boundary.
    // For an unhatched egg the "next evolution" IS the hatch, so this returns the
    // incubation clock; at a terminus (Daemon, no successor) hasNextEvolution() is
    // false and evolveRemainMs() is 0 (STAT shows MAX). Counts real elapsed time.
    bool hasNextEvolution() const;
    uint32_t evolveRemainMs() const;
    bool inCSF() const { return nav_ == Nav::ModalCSF; }
    const std::vector<SaveRecord>& records() const { return op_.arch.records; }
    int recordCount() const { return static_cast<int>(op_.arch.records.size()); }

    // --- Persisted identity / lifetime (STAT footer, ARCH, CFG) -------------
    int generation() const { return pet_.generation; }          // active pet's generation
    int petsRaised() const { return op_.petsRaised; }          // lifetime eggs hatched
    uint32_t lifetimeUptimeMs() const { return op_.uptimeBase + nowMs_; }
    uint32_t lifetimeSteps() const { return op_.lifetimeSteps; }  // exploration
    const char* hackerTag() const { return op_.hackerTag; }
    // The CURRENT serialized save size: what a persistSave() right now would
    // actually write. Diagnostic-only (the boot-line print, main.cpp): lets a
    // dev watch it against the save partition's known capacity as content
    // grows, rather than finding out via a silent NOT_ENOUGH_SPACE write failure.
    size_t saveBlobSizeBytes() const { return serializeSave(captureSave()).size(); }
    // Web 'Pedia rename ("one safe write"): validate `s` is
    // 1..kHackerTagMax chars, every char in [A-Z0-9_] (the same alphabet the
    // on-device arcade editor cycles through), then commit + persist. Returns
    // false (no mutation) on an invalid tag.
    bool setHackerTag(const char* s);
    const std::vector<SaveStoredPet>& rack() const { return op_.arch.rack; }
    int rackCount() const { return static_cast<int>(op_.arch.rack.size()); }
    // Where the ARCH cursor is, for the screens that draw it and the gates that drive
    // it. The same read-only surface ITEMS and GAMES already expose (itemsScreen,
    // arcadeRow): enough to say which shelf is open and what is focused, without
    // exposing the rack.
    ArchScreen archScreen() const { return archScreen_; }
    ArchGroup archGroup() const { return archGroup_; }
    int archPickRow() const { return archPickRow_; }
    int archPickRowCount() const {
        return static_cast<int>(
            buildArchPickerRows(registry_, pet_.creature, op_.arch.rack, op_.arch.records)
                .size());
    }
    int archRowCount() const { return static_cast<int>(archRows().size()); }
    // The species id under the ARCH cursor, or nullptr on an empty group, a record with
    // an id the tables no longer answer to, or the NEW EGG row (which names no pet).
    const char* archFocusedPetId() const {
        const ArchRow r = archFocusedRow();
        return r.def ? r.def->id : nullptr;
    }
    // The SELL sheet for the focused row, in one piece: what the L3 draws, and what a
    // gate reads to see which offers are open without driving the screen.
    ArchSaleSheet archSaleSheet() const;

    // Zone-completion Titles --------------------------------
    // Player-level, persisted (survive a pet reset, like the sector-clear flags).
    // A cleared sector's Title unlocks; one may be equipped for display.
    bool titleUnlocked(int sector) const { return op_.world.titleUnlocked(sector); }
    int titlesUnlockedCount() const;
    int equippedTitle() const { return op_.world.equippedTitle; }   // sector index, or -1
    // The equipped Title's display name, or "NONE" when nothing is equipped.
    const char* equippedTitleName() const;

    // Crew + home network (game_crew.cpp) ----------------------------------
    // Player-level, persisted (survive a pet reset, like the Titles above). The HOME
    // NETWORK is the network the operator has designated as theirs to defend; holding
    // one is the precondition for belonging to a crew (joinCrew refuses without it).
    int crewIndex() const { return op_.crewIndex; }            // kCrews row, or -1
    const CrewDef* activeCrew() const { return crew(op_.crewIndex); }
    // The CREW screen's views (game/crew_screen.h), named here so the gates can say
    // which is up.
    using CrewView = mal::CrewView;
    CrewView crewView() const { return crew_.view(); }
    bool hasHomeNetwork() const { return op_.homeNetworkKey != 0; }
    uint64_t homeNetworkKey() const { return op_.homeNetworkKey; }
    const char* homeNetworkName() const { return op_.homeNetworkName; }
    // Designate `key` (a packed 48-bit BSSID, as Game::registerNetwork builds them)
    // as the home network. An explicit pick, replacing any earlier inference from
    // sighting counts. key 0 clears it (and any crew that depended on it).
    void setHomeNetwork(uint64_t key, const char* name) override;
    // Enlist in kCrews row `index`. Returns false (changing nothing) without a home
    // network, or for an out-of-range row.
    bool joinCrew(int index) override;
    void leaveCrew() override;

    // The home-network picker's offers: every network the radio can hear RIGHT NOW
    // (visibleNetworks_), most-sighted first, capped at `max`. Returns the count. The
    // picker's render and input and the gates all walk the same list.
    using CrewNetRow = mal::CrewNetRow;
    int crewNetworkRows(CrewNetRow* rows, int max) const override;

    // Achievements (save v40) --------------------------------------------------
    // Player-level, surviving a pet reset like the Titles above. The catalogue is data
    // (content_achievements.cpp) and most rows need no code here: a per-tick sweep
    // compares each row's `series` progress (achValue) against its goal. Only
    // AchSeries::Event rows, whose event leaves no counter behind, are fired by hand
    // where they happen, through the `ach::` id constants, so a typo fails to compile.
    //
    // Idempotent, so a trigger site and the sweep can both call freely. Unlocking pays
    // the row's `rewards` and queues the home-screen banner, which is the only way an
    // unlock reaches the player.
    void unlockAchievement(const char* id);
    // The MIRROR: the pet facing its own species. Called from every seam that can seat a
    // real species opposite the player (the LINK duel and the arena), so the mirror rule
    // lives in one place.
    void noteMirrorMatch(const char* opponentCreatureId);
    bool hasAchievement(const char* id) const;
    bool hasAchievement(const AchievementDef& d) const {
        return achBit(op_.reveals.achEarned, d.wire);
    }
    // The player's live progress toward `d` (the numerator of its ladder). Paired with
    // achievementGoal(d) (app/game_achievements.h), which owns the denominator.
    int achValue(const AchievementDef& d) const;

    // The row whose banner is on screen right now, or nullptr. Set only while the pet
    // face is idle, so an announcement counts as seen only when the home screen was
    // showing.
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
    // The three lines the banner shows, composed once. Both the DRAW and the DWELL need
    // them (how long an announcement has to stay up is a question about the copy it is
    // carrying), so they are composed once, here.
    struct AchBannerCopy {
        const char* kicker = "";
        bool held = false;
        char name[40] = {0};
        char reward[40] = {0};
    };
    AchBannerCopy achBannerCopy() const;
    // How long the banner now being armed must stay up: long enough for its longest
    // line to finish one marquee pass, floored at kAchBannerMs. Derived from the
    // name, because only a long name can leave the screen mid-scroll.
    uint32_t achBannerDwellMs() const;
    // Clear a held banner (marking it announced). Any button does this on the home
    // screen, and the press is spent doing it.
    void dismissAchievementBanner();
    // Earned-but-not-yet-announced rows still waiting. Tests read it; so does the burst
    // check that decides between a named banner and a summary.
    int achPendingNotify() const;
    // A creature FACED IN COMBAT but never raised. Seeing is a fight, not a
    // glimpse: nothing a menu, hint or cinematic shows counts. Today the only
    // combatant resolved from a real species is a duel opponent (startPvpBattle):
    // wild malbeasts, sub/area bosses and Sim dummies are name-only CombatEnemy
    // specs from pools disjoint from the roster, and the wild half has its own
    // roster-keyed masks (malbeastSeen below) written on this same fight rule.
    // Idempotent. A creature already raised is never added, because "hatched"
    // outranks "seen" (pedia_state.cpp's pets{} map).
    void markCreatureSeen(const char* id);
    bool creatureSeen(const char* id) const;
    // The device's running tally of species the operator has RAISED: recorded at
    // every point a creature becomes this device's pet (hatch, evolution, an ARCH
    // Deploy) and never removed, so it outlives the pet itself. This is what the
    // 'Pedia's "hatched" reveal and FULL_PEDIA_L1 read: what's currently HELD is a
    // snapshot, and a species evolved past leaves nothing holding it. Idempotent.
    void markCreatureRaised(const char* id);
    bool creatureRaised(const char* id) const { return op_.shelves.creatureRaised(id); }
    // How many DISTINCT species this device has raised (PROFILE's SPECIES row).
    int speciesRaised() const { return static_cast<int>(op_.shelves.raisedCreatures.size()); }
    // The fixed wild-malbeast roster's (combat.h kWildMalbeastIds) seen/defeated
    // state, set by a real wild encounter roll (seen) / a won live wild fight
    // (defeated): see Game::startEncounter / Game::finishCombat.
    bool malbeastSeen(int idx) const {
        return idx >= 0 && idx < kWildMalbeastCount &&
               (op_.reveals.malbeastSeenMask & (1u << idx)) != 0;
    }
    bool malbeastDefeated(int idx) const {
        return idx >= 0 && idx < kWildMalbeastCount &&
               (op_.reveals.malbeastDefeatedMask & (1u << idx)) != 0;
    }

    // Lifetime boss rounds won (save v40). Each round of a gauntlet counts, so a
    // three-boss gauntlet adds three.
    int bossWins() const { return op_.tallies.bossWins; }
    // Lifetime Stacker boards cleared by hand in MAINT (save v44), for any process.
    // Quick and Tool runs do not count because nothing is played.
    int stackerWins() const { return op_.tallies.stackerWins; }
    // The three v56 lifetime tallies, on the same footing as the two above.
    int tourneyWins() const { return op_.tallies.tourneyWins; }   // ROCK THE DOCK brackets taken
    int pvpWins() const { return op_.tallies.pvpWins; }           // LINK duels won
    int mergesCooked() const { return op_.tallies.mergesCooked; } // dishes cooked at the MERGE HUB
    // Has this item EVER been in the bag? (save v40): the collection tally behind the
    // cuisine/rarity achievements. Anything asking what the player has seen reads
    // this; the bag holds only what is left. Written by a sweep over the live
    // inventory, so every path that grants an item is covered by construction.
    bool itemCollected(const char* id) const { return op_.shelves.itemCollected(id); }
    int itemsCollected() const { return static_cast<int>(op_.shelves.collectedItems.size()); }
    // The PET's palate, beside the operator's collection above: has this pet been fed
    // `id`, and how many distinct dishes has it tasted. Cleared with the pet, so each
    // new pet fills the FOODS grid from empty.
    bool petAteFood(const char* id) const;
    int petFoodsEaten() const { return static_cast<int>(pet_.foodsEaten.size()); }
    // Has the METHOD for `outputId` been won? (save v31). The kitchen's other axis:
    // meeting a dish and knowing how to cook it are independent, and a recipe is only
    // ever won off a Decryptogram, never bought. Keyed by the dish because that is the
    // recipe table's own key (content_recipes.h recipeIndexByOutput). Out-of-line
    // (game_merge.cpp): that table is engine-private, so this header cannot see it.
    bool recipeKnown(const char* outputId) const;
    // The deepest DeepWeb Dive this SPECIES has ever reached on this device (save v40),
    // 0 if it never dived. Per-species rather than per-pet, so the record outlives the
    // individual: the pet's own in-progress best is bestDeepWebDepth() above.
    int speciesDeepestDive(const char* creatureId) const {
        return op_.shelves.deepestDive(creatureId);
    }
    // The deepest dive on the device, by anyone: the max over every species record.
    int deepestDiveEver() const { return op_.shelves.deepestDiveEver(); }

    // --- Dev shortcuts (host key / device dev gesture / tests) --------------
    // Wipe back to a fresh empty save and re-enter the Decryption Hatch, so the
    // hatch + raising loop can be retested without reflashing / relaunching.
    void resetToHatch();
    // Clear the player-level state a pet reset deliberately leaves standing: a fresh
    // Operator record, the Titles, world progress, the shelves, the Cant and the radio
    // consents. Paired with resetToHatch() by the
    // Factory Reset's WIPE EVERYTHING scope; nothing else calls it, because every
    // other reset path is scoped to one pet on purpose.
    void wipeDeviceProgress();
    // Force the Evolution boundary now, bypassing the time/care gate (the stub
    // trigger is /TBD). No-op unless the pet has a defined successor.
    void debugTriggerEvolution();
    // Seed the shared LCG (rng_) so rng-driven draws (the hatch pool, the Trojan
    // cross-line divert roll) are deterministic in a test / headless run.
    void debugSeedRng(uint32_t seed) { rng_ = seed; }
    // Inject a frozen pet into the ARCH rack (tests / headless screenshots of a
    // populated rack; players use the Store action). No-op if the rack is full or the
    // id is unknown. `combatLevel` lands as that many Power points, which keeps the
    // level == earned-points invariant a thawed pet is held to.
    void debugSeedRack(const char* creatureId, int combatLevel = 0);
    // Launch a combat directly (tests / headless screenshots / dev). `live` picks
    // the stakes; the Sim-Battle path is the in-game one (startSimBattle, safe).
    // `lethal` builds an unbeatable enemy so a loss is deterministic (stakes test).
    void debugStartCombat(bool live, bool lethal = false);
    // The same, against a chosen enemy spec (e.g. wildMalbeastAt(i)): what a
    // headless preview of one particular wild needs, since a walk rolls whichever one.
    void debugStartCombatVs(const CombatEnemy& spec, bool live = false);
    // Preset the lifetime networks-seen counter + its v1 dedup pool (tests /
    // dump_frame): marks the first `n` pool slots seen and recomputes Hacker
    // Rank silently. Scaffolding to reach a rank/sector-unlock state without
    // grinding real Wi-Fi events, NOT a real rank-up (no celebration/reward).
    void debugSetNetworksSeen(int n);
    // Compose the explore-mode flavor line directly (tests / dump_frame). A walk step's
    // event is random, so a frame that needs the longest line the walk can produce sets
    // it directly. Explore mode still has to be armed for anything to draw.
    void debugSetExploreFlavor(const char* s);
    // Put a guardian in front of the pet NOW (tests / dump_frame), skipping the walk
    // that would eventually route to one. This is the encounter's real entry point and
    // the welcome is still rolled, so a caller that needs a riddle seeds the RNG.
    // Explore mode has to be armed, exactly as it does for the walk path.
    void debugStartShiboleet() { startShiboleet(); }
    // Credit captured handshakes without a radio (tests / headless runs). SHAKES buy
    // sigils of the Cant, and earning one takes a real WPA capture on hardware, which no
    // native run can do.
    void debugAddHandshakes(int n) {
        op_.cant.handshakesSeen += n;
        markSaveDirty();
    }
    // Learn `n` sigils outright, so a test can stand at a fluency rather than winning its
    // way there. Follows the same fixed reveal order the real path does (learnSigil), so
    // "four sigils" means the same four letters here as in play.
    void debugLearnSigils(int n) {
        for (int i = 0; i < n; ++i) op_.cant.sigils = learnSigil(op_.cant.sigils);
        markSaveDirty();
    }
    // Credit boss rounds without fighting them (tests / headless runs): reaching the
    // deeper rungs otherwise means walking a gauntlet ladder per rung.
    void debugAddBossWins(int n) { op_.tallies.bossWins += n; markSaveDirty(); }
    // Credit cleared DEFRAG boards without playing them (tests / headless runs). The
    // deeper rungs are dozens of hand-played runs, each of which also spends Bits.
    void debugAddStackerWins(int n) { op_.tallies.stackerWins += n; markSaveDirty(); }
    // Record a species' DeepWeb depth without diving (tests / headless runs), so the
    // depth ladders can be exercised without a several-hundred-fight run.
    void debugRecordSpeciesDive(const char* creatureId, int depth) {
        recordSpeciesDive(registry_.creature(creatureId), depth);
    }
    // Seed the network ledger directly, bypassing the scan → EXPL-Wi-Fi-event resolve
    // path: the only way a tool or test can populate the CREW home-network picker
    // without walking. `count` is the sighting tally the picker sorts on.
    void debugSeedNetworkLedger(uint64_t key, const char* name, int count);
    // Put a move in the pet's OWNED set (tests / dump_frame). Nothing in play grants
    // one yet. This is how a scene puts a kit in a known state, e.g. to show the
    // outro a rival with no new move gets.
    void debugGrantMove(const char* id) { pet_.moves.grant(id); }
    void debugUnequipMove(int slot) { pet_.moves.unequip(slot); }
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
    // Pin the active pet's (unfound) favourite dish, so a gate can feed it on purpose
    // rather than hunting through the roll.
    void debugSetFavouriteFood(const char* id) {
        pet_.upgrades.favouriteFood = registry_.item(id);
        pet_.upgrades.favouriteFound = false;
    }
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
    // Buy the MERGE HUB slot via the real buy path (tests / dump_frame). Its recipes
    // are won through debugWinRecipe below.
    void debugBuyMergeHub() { buyRigUpgrade(kRigRowMergeHub); }
    // Any rig row by index, through the real buy path, for a test that sweeps the
    // whole storefront.
    void debugBuyRigRow(int row) { buyRigUpgrade(row); }
    // Draw the Bandwidth pool down without staging a fight to spend it, so a test can
    // watch it REGENERATE (which is the only way to observe a pet's regen interval).
    void debugSpendBandwidth(int n) {
        bandwidth_ -= n;
        if (bandwidth_ < 0) bandwidth_ = 0;
    }
    // Buy the g/h Auto Backup / Continuous Auto-Backup unlocks via the real buy path
    // (tests): the two rows that arm the Backup Drive death-save for free.
    void debugBuyAutoBackup() { buyRigUpgrade(kRigRowAutoBackup); }
    void debugBuyContinuousBackup() { buyRigUpgrade(kRigRowContinuousBackup); }
    // Buy the next MOD STORAGE tier via the real buy path (tests): what raises how many
    // spare copies of one mod the pool will hold (modStorageCap).
    void debugBuyModStorage() { buyRigUpgrade(kRigRowModStorage); }
    // Buy the LINK AGGREGATOR via the real buy path (tests): the row that turns a
    // regen tick from a flat +1 into a percentage of the spent pool.
    void debugBuyLinkAggregator() { buyRigUpgrade(kRigRowLinkAggregator); }
    // Hand back to the idle habitat exactly as a resolved explore event does (tests):
    // the Continuous Auto-Backup / auto-defrag seam, without driving a whole random
    // event to reach it.
    void debugReturnToExplore() { returnToExplore(); }
    // `i` indexes kMergeRecipes; the row it GRANTS is that recipe's own rigRow, so this
    // keeps working when either table grows. A grant. Recipes cannot be bought, and the
    // Decryptogram's prize ladder is the only source. Out-of-line (game_merge.cpp)
    // because the recipe table is engine-private.
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
    void debugSetItemFilter(ItemFilter f) { items_.setFilter(f); dirty_ = true; }
    ItemFilter itemFilter() const { return items_.filter(); }
    // The STAT reader, and what its pages read (game_stat.cpp). Public for the gates,
    // which find a page by its title and assert where a jump landed.
    const StatReader& statReader() const { return stat_; }
    StatSources statSources() const override;
    // Which ITEMS L2 screen is showing, the picker's tile cursor and the list's row
    // cursor (tests / render).
    ItemsView itemsView() const { return items_.view(); }
    int itemPickRow() const { return items_.pickRow(); }
    int itemsRow() const { return items_.row(); }
    // The shared list cursor and the MOD picker's own, for the list-navigation gates:
    // A's repeat and C's backward walk are assertions about where a cursor ENDED UP,
    // which is the one thing a rendered frame states least directly.
    int listRow() const { return listRow_; }
    int modPick() const { return modPick_; }
    const char* modDetailId() const { return modDetailId_; }
    // Grant mods in tests without walking a boss drop or a cache roll.
    ModPool& debugModPool() { return op_.modPool; }
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
    // Put `n` dishes on the active pet's palate (tests / tools/dump_frame.cpp), in
    // roster order. The real path is a meal. Game::startFeeding, which needs a bag,
    // a modal and a tick each; this is the same mark that path makes, so a rendered
    // FOODS grid shows what a fed pet's would.
    void debugTasteFoods(int n);
    // Run the e bulk-open path directly on the item `id` (tests): the real
    // trigger is the VAULT hold-B gesture; this exercises openAllCachesOfRarity's
    // aggregation (consume-all-of-rarity + tally + Bits sum) without the hold timing.
    // No-op if `id` is unknown.
    void debugBulkOpenCaches(const char* id) {
        if (const ItemDef* d = registry_.item(id)) openAllCachesOfRarity(*d);
    }
    // The bulk-open reveal (tests), while nav() == Nav::BulkYield; 0 / empty with none.
    int bulkYieldCachesOpened() const { return bulkReveal_ ? bulkReveal_->cachesOpened : 0; }
    int bulkYieldBits() const { return bulkReveal_ ? bulkReveal_->bits : 0; }
    int bulkYieldTallyCount() const {
        return bulkReveal_ ? static_cast<int>(bulkReveal_->tally.size()) : 0;
    }
    // The i'th tally entry's item id + count (tests). Empty id / 0 count if out of range.
    const char* bulkYieldTallyId(int i) const {
        return (i >= 0 && i < bulkYieldTallyCount() && bulkReveal_->tally[i].def)
                   ? bulkReveal_->tally[i].def->id : "";
    }
    int bulkYieldTallyCountAt(int i) const {
        return (i >= 0 && i < bulkYieldTallyCount()) ? bulkReveal_->tally[i].count : 0;
    }
    // Resolve a Defrag with a forced success/failure via the real resolveMaint() path
    // (tests): a deterministic maint outcome (rollMaintSuccess is otherwise RNG-gated),
    // so a failed defrag's shielded +1 care mistake is assertable. `success=false` =
    // a failure (frag penalty + the shielded mistake).
    void debugResolveDefrag(bool success);
    // Advance the Stacker's run one beat without waiting out its real cadence: so tests
    // and dump_frame can place the run exactly.
    void debugStepStacker() { stepStacker(); }
    // Re-deal the board from `seed` (Stacker::reset) at the dial's ground calls, so a
    // gate can aim its column functions at known intruders.
    void debugSeedStacker(uint32_t seed) { stacker_.reset(seed, stacker_.calls()); }
    const Stacker& stacker() const { return stacker_; }
    // The same seam for FRAGGER: one row of fall, on demand. `fragger()` is mutable so a
    // test can stage a lane (Fragger::place) before driving the real buttons at it.
    void debugStepFragger() { stepFragger(); }
    Fragger& fragger() { return fragger_; }
    const Fragger& fragger() const { return fragger_; }
    // Set a quote's standing directly (tests / dump_frame): reach the eight-solve
    // arcade unlock, or a mid-ladder tier, without playing eight boards by hand. The
    // real paths are Game::settleCryptogram's win and its loss ratchet.
    void debugSetQuoteTier(int i, CryptogramTier t) { setQuoteTier(i, t); }
    // Open a SPECIFIC quote's board (tests / dump_frame). Skips only the ticket and the
    // roll. onButton then drives the real board and settle, which decide the ratchet
    // and the prize. Real path: the VAULT cash-in.
    void debugStartCryptogram(int quote, int extraReveals) {
        startCryptogram(quote, extraReveals);
    }
    // Enter `k`'s played run without paying for it or the run being due (tests /
    // dump_frame). From here onButton drives the REAL board, and a finished run lands in
    // the real settlePlayedRun, which is where the process takes its payout.
    void debugStartPlayedMaint(MaintKind k) {
        maintKind_ = k;
        maint_ = MaintRun::started(k, /*played=*/true, maintDifficulty_);
        if (!beginPlayedGame(maintPlayedGame(k))) maint_.reset();
    }
    // Point a process's played variant at a different engine, so a test can swap them
    // without editing the content table.
    void debugSetMaintPlayedGame(MaintKind k, ArcadeGameKind g) {
        (k == MaintKind::Defrag ? defragPlayedGame_ : avPlayedGame_) = g;
    }
    void debugSetMaintDifficulty(ArcadeDifficulty d) { maintDifficulty_ = d; }
    // Set the Bits wallet directly (tests / economy-gated flows like the shop's
    // "not enough Bits" gate). Clamped at 0.
    void debugSetBits(int n);
    // Set a sub-area's durable clear / boss-unlock flag directly (tests / dump_frame):
    // scaffolding to reach a mid-ladder EXPL state without grinding the real
    // streak. No-op for out-of-range indices; the real paths are clearSubArea's streak
    // + FIGHT BOSS. Marks the save dirty so a follow-on persist captures it.
    void debugSetSubCleared(int area, int sub, bool v) {
        if (area >= 0 && area < kAreaCount && sub >= 0 && sub < kSubAreasPerArea) {
            op_.world.subCleared[area][sub] = v; markSaveDirty();
        }
    }
    void debugSetSubBossUnlocked(int area, int sub, bool v) {
        if (area >= 0 && area < kAreaCount && sub >= 0 && sub < kSubAreasPerArea) {
            op_.world.subBossUnlocked[area][sub] = v; markSaveDirty();
        }
    }
    // Set an AREA's cleared flag directly (tests): reach the "all areas cleared" state
    // (the DeepWeb Dive unlock) without grinding every gauntlet. Real path: area boss.
    void debugSetSectorCleared(int area, bool v) {
        if (area >= 0 && area < kAreaCount) { op_.world.sectorCleared[area] = v; markSaveDirty(); }
    }
    // Arm the DeepWeb Dive directly (tests): keeps its own all-areas-cleared guard.
    // Real path: EXPL → B on the "> DIVE" row.
    void debugStartDeepWebDive() { startDeepWebDive(); }
    void debugOfferThenDive() { offerThenDive(); }
    void debugDeclineEggOffer() { pet_.eggOfferMade = true; }
    int debugDiveRewardBonusPct() const { return diveRewardBonusPct(); }
    // The armed Deep-Learning Module/Core depth step (1 = none armed). No production
    // reader outside the engine (the habitat draws it via game_render.cpp), so tests
    // read it here to assert the buff survives the dive it was armed for.
    int debugDepthMultiplier() const { return pet_.armed.deepWebDepthMultiplier; }
    // The depth the next dive would START at (-1 = no bell armed), with the Checkpoint
    // sentinel already resolved: armedDeepWebStartDepth(), which is what the bells'
    // own inert check compares against, so a gate asserts the number the engine acts on
    // rather than the raw field's sentinel.
    int debugArmedStartDepth() const { return armedDeepWebStartDepth(); }
    // Set this pet's best-ever dive depth, the frontier a Checkpoint Bell targets. Real
    // path: winning at that depth (game_combat.cpp). A gate that needs a pet with a
    // record behind it says so directly instead of playing a dive out.
    void debugSetBestDeepWebDepth(int depth) { pet_.bestDeepWebDepth = depth < 0 ? 0 : depth; }
    void debugSetBestDarkWebDepth(int depth) { pet_.bestDarkWebDepth = depth < 0 ? 0 : depth; }
    void debugStartDarkWebCrawl() { startDarkWebCrawl(); }
    // Arm explore-mode on a SPECIFIC sub-area directly (tests): once cleared sub-areas
    // are re-farmable the EXPL "first-selectable" row is ambiguous, so a test
    // that needs a particular frontier arms it here instead of A-cycling. Real path:
    // EXPL → B on the row. No-op for out-of-range indices.
    // Force the arena run to a terminal banner without playing a whole bracket out.
    // The two verdict screens are otherwise reachable only by winning or losing three
    // matches, which is not what a screen gate is asking about.
    void debugEndTourneyRun(bool champion) {
        if (!tourney_) return;
        tourney_->phase = champion ? TourneyPhase::Champion : TourneyPhase::Eliminated;
        if (!champion) tourney_->alive &= static_cast<uint8_t>(~(1u << tourneySlot()));
        // A champion run pays out like the real third win, including the two shape
        // achievements, which read the final's opponent BEFORE it is cleared below. A
        // hook that ended a run in the title but skipped the purse would be a hook the
        // purse could not be tested through.
        if (champion) awardTourneyPurse();
        tourney_->opponent = TourneyFighter{};
        dirty_ = true;
    }
    void debugArmExplore(int area, int sub) {
        if (area >= 0 && area < kAreaCount && sub >= 0 && sub < kSubAreasPerArea)
            startExplore(area, sub);
    }
    // Arm AUTO-PROGRESS, and set the armed sub-area's win streak (tests). Real path:
    // the A+C chord on the explore-control overlay, and ten wild wins. Lets a test
    // of the step rule skip the streak that triggers it; the streak is covered by
    // test_explore_streak_unlocks_boss_then_clears.
    void debugSetAutoProgress(bool v) { autoProgress_ = v; }
    // TEST/DEV: mark every STORY chapter already read, so a gate driving the walk is
    // never interrupted by one (game_story.cpp). The same standing the auto-progress
    // hook above has: a chapter firing mid-walk is real behaviour with gates of its own,
    // and a gate about loot tables or Wi-Fi rolls should not have to page through the
    // premise to get to what it is measuring.
    // TEST/DEV: open a chapter in the reader directly, without walking to the beat that
    // fires it. What the reader DOES with a chapter is independent of which milestone
    // handed it over, and driving a gauntlet to its end to look at a page is a long way
    // round to a screen.
    void debugOpenStory(const StoryChapterDef* chapter) {
        storyNext_ = nullptr;
        storyThenArea_ = -1;
        openStoryChapter(chapter, StoryThen::Walk);
    }
    void debugMarkStoryRead() {
        for (int i = 0, n = storyEntryCount(); i < n; ++i)
            markStoryRead(storyEntryAt(i).chapter);
    }
    void debugSetExploreStreak(int v) { walk_.streak = v < 0 ? 0 : v; }
    // The composed walk-badge label (tests): the only string on that line whose width is
    // content-driven, so the gate that measures the badge's two fields against each other
    // has to read the real one rather than re-derive it and measure its own guess.
    void debugExploreBadgeLabel(char* out, size_t n) const { exploreBadgeLabel(out, n); }
    // Launch a specific sub-area boss directly (tests): same rationale as
    // debugArmExplore. The EXPL "first-selectable" row is ambiguous once cleared subs
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
    // The same award as a fight pays it, which also builds adventure Bond.
    void debugAddFightXp(int xp) { addFightXp(xp); }
    // The two grants a BACKGROUND can come from that a gate should not have to play out
    // in full: clearing an area's gauntlet, and taking a bracket at the arena. Both move
    // exactly the state ownership is derived from and nothing else, so what they exercise
    // is the derivation rather than a flag written past it. The real paths are the area
    // boss's final win and awardTourneyPurse.
    void debugClearSector(int area) {
        if (area < 0 || area >= kAreaCount) return;
        op_.world.sectorCleared[area] = true;
        markSaveDirty();
    }
    void debugAddTourneyWin() { ++op_.tallies.tourneyWins; markSaveDirty(); }
    // MODS earn path (tests): roll an id from an area's mod loot table, and
    // grant a mod through the real earn path (rolls its per-instance equip-level gate).
    // Real path: boss clears / DeepWeb wins / Epic caches. Same helpers, gated there.
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
    // Advance the pet clock directly through the real model (tests): the same
    // path Game::tick() uses for Hunger decay + Passive XP Farming, without
    // needing a full tick() (nav/lockout/autosave etc).
    void debugTickHunger(uint32_t elapsedMs) { tickHungerAndAwardXp(elapsedMs); }
    // The live post-clear re-farm win count for a sub-area (tests).
    int debugSubRefarmCount(int area, int sub) const {
        if (area < 0 || area >= kAreaCount || sub < 0 || sub >= kSubAreasPerArea)
            return 0;
        return pet_.armed.subRefarmCount[area][sub];
    }

    // Is the Backup Drive death-save currently armed (used and not yet spent in a
    // fight, nor lapsed)? Lazily checked against the deadline, like
    // Game::buildPlayerCombatant() itself, so nothing clears it when it expires.
    // Read by the STAT BUFFS page and the idle-habitat buff badge (game_render.cpp).
    bool backupShieldArmed() const {
        return pet_.armed.backupShieldUntilMs != 0 && lifetimeUptimeMs() < pet_.armed.backupShieldUntilMs;
    }

    // The USB family's two armed states, read by the STAT BUFFS page, the idle-habitat
    // buff badges (game_render.cpp) and the gates that decide what else may be plugged
    // in. Both are per-pet and both are spent at the evolution they steer.
    BranchOverride evolveBranchOverride() const { return pet_.usb.branch; }
    int evolveSoakFactor() const { return pet_.usb.soakFactor; }
    bool evolveHoldArmed() const { return pet_.usb.hold; }

    // Is a LOCKING device in the port (defs.h's isLockingUsbEffect)? A soak owns the
    // boundary by stretching it and a hold owns it by refusing to reach it, so while
    // either is armed the port takes nothing but an Eject-USB. The swappable half of the
    // family (the divert and the branch overrides), never locks anything.
    bool usbPortLocked() const { return pet_.usb.soakFactor > 1 || pet_.usb.hold; }

    // Is ANYTHING in the port? What the Eject-USB asks before spending itself, so it is
    // refused on an empty port rather than thrown away on one.
    bool usbPortOccupied() const {
        return usbPortLocked() || pet_.armed.trojanDivert ||
               pet_.usb.branch != BranchOverride::None;
    }

    // Pull everything out of the port and drop its effect (the Eject-USB). This is the
    // one place every slot is listed, so a new device in the family is undone by
    // adding it here.
    void clearUsbPort() {
        pet_.armed.trojanDivert = false;
        pet_.usb.branch = BranchOverride::None;
        pet_.usb.soakFactor = 1;
        pet_.usb.hold = false;
    }

    // The care branch the pet's next BRANCHING evolution actually reads: the care
    // budget, unless a Bad-USB or Signed-USB is in the port to overrule it. The one
    // place the override is applied, so evolutionTargetId, the Trojan divert's own
    // Good/Bad pick and the post-evolution achievements can never disagree about which
    // branch fired. A DYING pet is returned unchanged: 5/5 routes to Critical System
    // Failure rather than to a successor, and no device on the shelf sells a way out
    // of having starved a pet.
    CareBranch effectiveCareBranch() const;

    // The dwell the CURRENT stage's evolution gate is waiting on: the stage's own
    // duration, stretched by an armed Sandbox/Hypervisor-USB soak. Every read of the
    // gate goes through here (evolveEligible, evolveRemainMs), so the clock the screen
    // counts down is always the clock the boundary is actually checking.
    uint32_t evolveDwellMs() const;

    // the non-Bits drop-chance scale (%) for a sub-area with `refarmCount`
    // post-clear wild wins: 100 decaying to a floor. Pure/static so it's unit-tested
    // directly and reused wherever a re-farm reward is rolled.
    static int refarmDropScalePct(int refarmCount);

    // WHERE THE PET IS. Two functions, because the habitat asks where its occupant lives
    // and the stage asks where this fight is happening. Both answer with a SceneId
    // (core/render/scenes.h), and SceneId::None is a real answer: a plain `paper` field,
    // which is what every one of these screens showed before any place existed.
    //
    // Public so other code can ask which backdrop a screen shows. A gate checking that
    // an icon slot is clear compares it against the backdrop.
    SceneId habitatScene() const;
    SceneId stageScene() const;

    // THE BACKGROUND AN OPERATOR OWNS AND CHOOSES. The rows are content
    // (content/content_backgrounds.h); what is here is the rule that decides whether one
    // is theirs yet, and it is DERIVED rather than stored: every background is earned by
    // something the save already records, so a parallel bitmask would be a second copy of
    // facts already on the blob and the failure mode of a second copy is disagreement.
    bool backgroundOwned(SceneId s) const;
    // AUTO is SceneId::None: the pet decides, which is what the habitat did before any
    // of this could be chosen. A pick the operator does not own is refused, so nothing
    // can place them somewhere they have not been.
    SceneId backgroundPick() const { return op_.backgroundPick; }
    bool setBackgroundPick(SceneId s);
    // How many of the arena's backgrounds the brackets taken so far have paid out. The
    // picker's own rung check and the announcement both read this, so "which win pays
    // what" is stated once.
    int backgroundBracketsTaken() const { return op_.tallies.tourneyWins; }

private:
    // RigShopHost: the SHOP screen's way back into the game (game_rigshop.cpp). The
    // first two also serve ItemsHost.
    void leaveToCarousel() override;
    void armHoldB() override;
    void markDirty() override;
    // ItemsHost: the ITEMS screen's way back into the game (game_items.cpp).
    bool itemsFromLockout() const override { return lockoutItemsContext_; }
    void openItemDetail(const ItemDef& d) override;
    void returnToLockout() override;
    // FightScreenHost: the Combat screen's way back into the game (game_combat.cpp).
    // The rest of it is public above (fight, pvpFighting, stageScene, playSound, ...)
    // or the fight's own lifecycle below (advanceCombatTurn, finishCombat, ...).
    Fight& liveFight() override { return fight_; }
    const SpriteData* petSprite() const override;
    int fxBeat() const override { return fxBeat_; }
    int screenBeat() const override { return beat_; }
    SwarmView guardianSwarm() const override;
    void stepGuardianFlock() override;
    void openExploitPicker() override;
    // Rendering.
    void drawHabitat(Framebuffer& fb, int cursor) const;
    // drawHabitat's steps, in the order it draws them (back to front).
    void drawHabitatPet(Framebuffer& fb, const SpriteData& pet) const;
    void drawHabitatPrompt(Framebuffer& fb, const SpriteData* pet) const;
    void drawFailingBanner(Framebuffer& fb) const;
    void drawHabitatStatusColumn(Framebuffer& fb) const;
    void drawAuditBadge(Framebuffer& fb) const;
    void drawExploreStatus(Framebuffer& fb) const;
    void drawHabitatCarousel(Framebuffer& fb, int cursor) const;
    void drawIdleReplicas(Framebuffer& fb, int petCx) const;
    void drawDisguiseProp(Framebuffer& fb, int petCx) const;
    void drawHarvestStreams(Framebuffer& fb, int petCx, int petCy) const;
    void drawFeedingMail(Framebuffer& fb) const;
    void drawSquatLogin(Framebuffer& fb) const;
    void drawLure(Framebuffer& fb, int originX, int originY) const;
    void drawMimic(Framebuffer& fb, const SpriteData& pet, int frame, int row, int petX,
                   int petY, bool mirror) const;
    // A raised pet whose line carries LinePassive::Replication: the one question both
    // the ambient copies and the fork ask.
    bool petReplicates() const;
    // Say that a background has just been come into. The three moments that can grant
    // one call it after the state that grants it has moved.
    void announceBackground(SceneId s);
    // The unlock announcement over the habitat: a no-op unless a banner is up.
    void drawAchievementBanner(Framebuffer& fb) const;
    void drawSubmenu(Framebuffer& fb) const;
    void drawDetail(Framebuffer& fb) const;
    void drawProcess(Framebuffer& fb) const;
    void drawStacker(Framebuffer& fb) const;
    void drawFragger(Framebuffer& fb) const;
    void drawEvolve(Framebuffer& fb) const;
    void drawCSF(Framebuffer& fb) const;          // Critical System Failure
    void drawTrain(Framebuffer& fb) const;        // MOVES L3 (move picker / detail)
    // The ART a CamoTarget names (FX_CAMO), or null for nothing to wear. Rival answers
    // with the sprite standing opposite; Line answers with that line's own creature at
    // the wearer's stage, so a pet channelling Ransomware looks like a Ransomware of its
    // own tier rather than like that line's hatchling. Registry lookups only: the tick
    // calls this every beat, and the pixel scan that turns the answer into a ramp waits
    // for the draw.
    const SpriteData* camoSpriteForTarget(const CamoTarget& t, const char* rivalSprite,
                                          Stage wearer) const;
    // The family the resting pet's `slot`-th drift reaches for, as art (core/model/
    // idle_camo.h). The rotation is every family except the pet's own, in roster order
    // and wrapping, so a pet works through the whole 'net without repeating colours
    // back to back. A family added to the roster joins automatically. Resolved once per
    // drift by the tick, never per repaint.
    const SpriteData* idleCamoSprite(int slot) const;
    void drawWifiScreen(Framebuffer& fb) const;
    void drawShiboleetHailScreen(Framebuffer& fb) const;
    void drawShiboleetScreen(Framebuffer& fb) const;
    void drawShiboleetVerdictScreen(Framebuffer& fb) const;
    void drawShopScreen(Framebuffer& fb) const;
    void drawPostEncounterScreen(Framebuffer& fb) const;

    // Navigation.
    void summonCursor(int slot);
    void enterSubmenu();
    void dropCursor();
    SubmenuId enteredId() const;                   // the slot table's id for cursor_
    // Whether a pet-face carousel slot is inert, for either reason a slot can be: a
    // Boot-Sector egg (GAMES/EXPL/MAINT/MODS have nothing to act on; STAT and ITEMS
    // stay live, ITEMS gating non-quest items separately via itemUsable), or a demo
    // build, where CFG configures a device the web demo does not have (demo_config.h).
    // Drives both the greyed carousel and the blocked submenu entry, so the two can
    // never disagree.
    bool slotLocked(SubmenuId id) const;

    // --- List navigation (game_listnav.cpp) ---------------------------------
    // The row-stepping half of the button contract, shared by every list on the
    // device so the three gestures behave identically wherever the player meets them:
    // A steps forward (held, it repeats), C steps BACKWARD (held, it backs out of the
    // list), and the row set each one walks is whatever the focused screen draws.
    //
    // ListTarget is a list the three gestures can drive: how its cursor steps by `dir` (wrapping, and
    // skipping whatever it considers unselectable), and what a tap of C means on it.
    // listTarget names the list under the cursor right now, and is the single gate all
    // three gestures ask. A screen absent from it keeps the plain A/B/C contract: an
    // inline confirm, a detail page, a modal and a minigame answer an empty target, so C
    // stays the instant tap-to-cancel there.
    struct ListTarget {
        void (*step)(Game&, int dir) = nullptr;
        void (*leave)(Game&) = nullptr;
        explicit operator bool() const { return step != nullptr; }
    };
    ListTarget listTarget() const;
    // The target for a screen type that owns its list: its stepList(host, dir), and its
    // own onButton for C.
    template <auto Screen>
    static ListTarget screenList();
    // listTarget's branches by where the list sits.
    ListTarget submenuListTarget() const;
    ListTarget detailListTarget() const;
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

    // The hold-B gestures (game_holds.cpp), routed the way the list gestures are:
    // holdTarget names the tap/hold screen that is up right now, with its second action
    // (true once its own dwell has passed and it landed) and the tap under a hold that
    // never landed. An empty target has no second action on B.
    struct HoldTarget {
        bool (*hold)(Game&, uint32_t heldMs) = nullptr;
        void (*release)(Game&) = nullptr;
        explicit operator bool() const { return hold != nullptr; }
    };
    HoldTarget holdTarget() const;
    // The target for a screen type that owns its hold: its holdB(host, ms) and releaseB(host).
    template <auto Screen>
    static HoldTarget screenHold();
    // The hold half: the focused screen's second action, once its own dwell has passed.
    // True if it landed, which disarms B.
    bool tickHoldB();
    // The release half: the tap underneath a hold that never landed.
    void releaseHoldB();

    // Per-state input handlers.
    void onItemsDetail(const ButtonEvent& ev);
    void onMaintList(const ButtonEvent& ev);
    void onMaintAction(const ButtonEvent& ev);
    void onStacker(const ButtonEvent& ev);
    void onFragger(const ButtonEvent& ev);
    void onCfgList(const ButtonEvent& ev);
    void onCfgGroup(const ButtonEvent& ev);        // DISPLAY / RADIO row lists
    void onCfgDetail(const ButtonEvent& ev);
    // onCfgDetail's screens. The picker is a template defined beside its only callers in
    // game_config.cpp.
    template <class Step, class Apply>
    void onCfgPicker(const ButtonEvent& ev, Step step, Apply apply);
    void onCfgTravel(const ButtonEvent& ev);
    void onCfgFormatSd(const ButtonEvent& ev);
    void onCfgSysInfo(const ButtonEvent& ev);
    bool cfgHoldB(uint32_t heldMs);   // System Info / Factory Reset's hidden hold
    void onCfgHackerTag(const ButtonEvent& ev);
    void onCfgUpdate(const ButtonEvent& ev);
    void onCfgUpdateJob(const ButtonEvent& ev);
    void onCfgUpdateConfirm(const ButtonEvent& ev);
    void onCfgUpdateRows(const ButtonEvent& ev);
    void onCfgPediaQr(const ButtonEvent& ev);
    void enterCfgScreen(CfgScreen target);         // open an L3, seeding its picker
    void leaveCfgScreen();                         // back to the group, else the list
    void drawCfg(Framebuffer& fb) const;          // L3 dispatch for the CFG screens
    void executeFactoryReset();                    // hold-B commit
    void onArchList(const ButtonEvent& ev);
    void onArchRecord(const ButtonEvent& ev);
    void archStoreActive();                        // active pet → rack → new egg
    void archDeployStored(int storedIdx);          // stored pet → active (slot-neutral)
    void archReleaseStored(int storedIdx);         // stored pet → gone (frees a slot, no reward)
    // The SELL counter (game_arch_sale.cpp). A stored Daemon is traded for ONE offer and
    // leaves a RETIRED record. archSaleBlocker is the single answer to "can this offer be
    // taken for this pet" (nullptr when it can, else the short reason its row shows),
    // so the sheet and the commit can never disagree about it.
    void onArchSale(const ButtonEvent& ev);
    const char* archSaleBlocker(SaleOffer offer, const SaveStoredPet& sold) const;
    void archSellStored(int storedIdx, SaleOffer offer);
    void reimageActivePet();                       // Reimage's effect on the active twin
    bool daemonOnRack() const;                     // anything for the SELL counter to take
    void noteRackDuplicates();                     // two of one species on the shelf → SECOND_INSTANCE
    void onArchPicker(const ButtonEvent& ev);       // L2 group picker
    // The rows behind the open group, and the focused one. Rebuilt on demand rather
    // than cached: the rack changes under this screen (a Store, a Deploy, a Release) and
    // a cached row list is exactly the thing that would then be describing a shelf that
    // no longer exists.
    std::vector<ArchRow> archRows() const;
    // The focused row, or a default-constructed one (kind Active, index -1, no def) when
    // the group is empty: every caller checks `def` before using it, so an empty group
    // reads as "nothing to act on" without a second null check at each site.
    ArchRow archFocusedRow() const;
    bool archRowIsActive() const { return archFocusedRow().kind == ArchRow::Kind::Active; }
    bool archRowIsRecord() const { return archFocusedRow().kind == ArchRow::Kind::Record; }
    // Is the ARCH detail screen the NEW EGG confirm rather than a pet's record?
    bool archOnNewEgg() const { return archGroup_.kind == ArchGroup::Kind::NewEgg; }
    // Commit the NEW EGG row: set the active pet aside (if there is one) and hatch.
    void archHatchNewEgg();
    // The Script a Daemon is reimaged back to: whichever Script's evolution reaches it,
    // by its care branch, its Daemon pool or a Trojan divert. Null for anything else.
    const CreatureDef* reimageScript(const CreatureDef* daemon) const;
    // Leave the line-select modal for the ARCH picker, without laying an egg. Only ever
    // reachable with something on the rack to go back TO: see the modal's own comment.
    void archReturnFromLineSelect();
    // GAMES: the cabinet list (L2) -> one cabinet (L3) -> a real minigame -> payout.
    // Launch the focused cabinet's minigame as an ArcadeRun. The one place a cabinet's
    // ArcadeGameKind turns into behaviour.
    void startArcadeRun(int cabinet, ArcadeDifficulty dial) override;
    void openDetail() override;
    void closeDetail() override;
    // Bank a finished run and open the payout screen. `scoreMax` 0 means the cabinet
    // scores win-or-lose, which is what decides how the bonus half is worked out.
    void finishArcadeRun(bool won, int score, int scoreMax);
    // A paced minigame's step cadence under the dial: in a cabinet or a played MAINT
    // run alike. A hatch game runs at `baseMs` untouched: it has no dial to read.
    int playStepMs(int baseMs) const;
    // The dial the run on screen was started under: the cabinet's, or MAINT's.
    ArcadeDifficulty runDifficulty() const {
        return arcade_ ? arcade_->difficulty : maint_ ? maint_->difficulty : maintDifficulty_;
    }

    // --- Mid-play RULES overlay (ROCK THE DOCK's BRIEFING, generalised) ----------
    // A+C on any of the game engines (Stacker/Fragger/Isolation/Decryption/Cryptogram/
    // Clutch/Chroma) opens a paged explainer of THAT engine's own rules and pauses it
    // underneath. The only route to one for the three that launch straight out of an
    // egg with no menu stop along the way at all (startHatchGame: Decrypt/Clutch/
    // Isolation land on their Nav directly). One flag rather than five: nav_ can only
    // be one of the five at a time, so nothing can ever have two open together, and
    // the content it reads is keyed by ArcadeGameKind rather than by cabinet row so it
    // reads the same whether the engine was reached from its cabinet, a MAINT played
    // variant, or a hatch.
    bool gameBriefOpen_ = false;
    int gameBriefScroll_ = 0;
    bool gameBriefIntro_ = false;            // opened by the hatch, not the chord
    // The tip card on screen (game_onboard.cpp): which tip, the window it is scrolled
    // to, where dismissing it returns, and whether it leads with the "!" explainer
    // (the first CARE card a device ever shows).
    Tip tipCard_ = Tip::MenuOpened;
    int tipCardScroll_ = 0;
    Nav tipCardReturn_ = Nav::Idle;
    bool tipCardMarkRow_ = false;
    void openTipCard(Tip t, Nav returnTo);
    // On a carousel summon: open the one-time card that is due, most urgent first. One
    // error short of failing, then the first error, then each need's CARE card, then
    // the first cache. Returns whether it opened one.
    bool openCareTipIfDue();
    void onTipCard(const ButtonEvent& ev);
    void drawTipCard(Framebuffer& fb) const;
    int cfgTipsRow_ = 0;                     // CFG > TIPS cursor
    AwayTally away_;
    bool awayDigestOn_ = true;
    uint32_t awayQuietMs_ = 0;               // the absence the open summary covers
    void resetAwayTally();
    // On a press: open the summary if the quiet before it was long enough and the tally
    // holds anything; otherwise start a fresh tally. True when it opened (the press is
    // spent on it).
    bool openAwayDigestIfDue(uint32_t quietMs);
    void onAwayDigest(const ButtonEvent& ev);
    void drawAwayDigest(Framebuffer& fb) const;
    void onTipsList(const ButtonEvent& ev);
    void drawTipsList(Framebuffer& fb) const;
    // Is the engine on screen being played for an egg's hatch bonus (rather than at a
    // cabinet, or for MAINT)? That is what the briefing's lead panel is about.
    bool hatchGameLive() const;
    // The briefing's hatch-only lead panel: what the egg stakes on this board.
    ProseRow hatchBriefLeadRow() const;
    bool gameBriefAvailable() const;         // is nav_ one of the five?
    ArcadeGameKind gameBriefKind() const;    // which one, as the content row's key
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
    // The MOVES picker's B: a hold toggles the full roster, a tap unequips (on the
    // first row) or drills into the focused move's detail.
    bool movePickerHoldB(uint32_t heldMs);
    void movePickerReleaseB();
    // The MOVES slot list's B: a hold reads the move in the slot, a tap opens its picker.
    bool moveSlotsHoldB(uint32_t heldMs);
    void moveSlotsReleaseB();
    void onMoveDetail(const ButtonEvent& ev);      // A pages prose, B equips, C backs
    // The move the picker's cursor is on, or nullptr on the unequip row: shared by
    // the drill-down, its input handler and the MoveDetail render.
    const MoveDef* focusedPickerMove() const;
    void onSimTier(const ButtonEvent& ev);
    // The fight's lifecycle. Its screen, the one pet-side screen where A+C is live, is
    // fightScreen_ (game/fight_screen.h).
    void startSimBattle();                          // build combat (safe stakes) + enter
    void finishCombat();                            // apply result, return to caller
    // Feeding-frenzy pacing: resolves one Combat::step(), then updates the
    // same-actor streak counters off who just acted (Combat::playerTurnNext()
    // is stable until step() runs, so it IS "who's about to act"). Shared by the
    // heartbeat auto-pace (game_core.cpp) and the A fast-forward (FightScreen) so
    // skipping a beat doesn't desync the streak read.
    void advanceCombatTurn();
    // The cue for whatever the last step/flee/commit resolved; shared by every path
    // that can move a fight, so a skipped beat sounds the same as an auto-paced one.
    void playCombatTurnSound();
    // An explore-mode wild or boss fight is on screen: the hands-off fights the walk's
    // SOUND row governs, and the ones whose per-swing cues never play.
    bool exploreFightLive() const;
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
    // A FIGHT's XP: the same award, and it also builds the pet's adventure Bond. Passive
    // farming, Sinkholes and Wi-Fi finds go straight to addCombatXp and build none.
    void addFightXp(int xp);
    // The A+C Exploit allowance for a fight. The tunable today; the seam a
    // rare reward item hangs off later (item ships later).
    int exploitUsesPerBattle() const { return kExploitUsesPerBattle; }
    // Combat-usable items from the live inventory (combatHeal>0), for the picker.
    std::vector<OverrideItem> combatItemOptions() const;
    void consumeCombatItem(const char* id);         // decrement + out-of-combat effect

    // EXPL / Explore-mode: the sector list arms explore-mode, which runs as
    // a background mode on the IDLE habitat; auto-stepping guaranteed events (each
    // resolving via the shared event/combat plumbing) with a win-streak that unlocks
    // the sector's boss. onExploreControl drives the A+C control overlay
    // (Nav::ExploreControl → A ping / B warp / C stop).
    void onExplList(const ButtonEvent& ev);
    // ...and the ACTIVITY PICKER it opens on (ui_state.h's ExplCat), which walks its own
    // four rows rather than the ladder's row space. Split out rather than folded into a
    // branch of onExplList because the two levels share no state but `listRow_`.
    void onExplCategories(const ButtonEvent& ev);
    // Flatten the durable per-sub flag blocks to the row-major layout the shared
    // expl_screen row helpers read ([area*kSubAreasPerArea+sub]).
    void flattenSubFlags(bool (&cleared)[kAreaCount * kSubAreasPerArea],
                         bool (&bossUnlocked)[kAreaCount * kSubAreasPerArea]) const;
    // Open EXPL: pick the nav level + cursor row to enter on (resuming a running
    // explore-mode where it left off, else the top level's first landable row).
    void openExplList();
    // THREE-level EXPL nav: is `row` a stop at the CURRENT level (explCat_ +
    // explNavArea_)? A category's own level → its zone rows / open area headers; inside
    // an area → that area's own selectable rows. `explCatLandable` is the same question
    // asked of the picker's four rows, and `areaHeaderRow` is an area's header row
    // index.
    bool explRowLandable(int row) const;
    bool explCatLandable(int catRow) const;
    // The EXPL list's view struct, filled from this Game's own state. The NAV reads it
    // (which categories the cursor stops on) and the RENDERER draws from it, so a
    // category that is skipped and a category drawn as "??????" can never be two
    // different sets. The renderer fills in the per-row blocks it also needs.
    ExplListView explView() const;
    int areaHeaderRow(int area) const;
    // Compose the idle badge label: the armed area's short (first-word) name + the
    // 1-based sub number, e.g. "CITRUS 3". Compact + collision-free.
    void exploreBadgeLabel(char* out, size_t n) const;
    // AUTO-PROGRESS. `autoProgressStep` is the hook every hand-back to the walk runs
    // (returnToExplore): it fires only once the armed sub-area has met its win target,
    // and then either fights the boss standing there or steps on. `autoProgressAdvance`
    // is the step rule itself: POSITIONAL, so it keeps cycling instead of stopping at
    // the frontier. `nextOpenArea` wraps to the next unlocked rung.
    void autoProgressStep();
    void autoProgressAdvance(int area, int sub);
    int nextOpenArea(int area) const;
    void startExplore(int sector, int sub);         // arm a sub-area → idle background mode
    void startDeepWebDive();                         // arm the mid-game endless zone
    void startDarkWebCrawl();                        // ...and the terminal one
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
    // modStorageCap() and logs either way. Drop sites: boss clears, DeepWeb wins,
    // Epic sealed caches.
    const char* rollAreaModId(int area);
    const char* rollAnyModId();
    void grantMod(const char* id);
    // MOVES earn path: you learn a move by BEATING something that knows it. Rolls
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
    // How many of `pool`'s move ids this pet could still learn: moveIsTeachable counted
    // over a zone's list. The one body every movesToLearn accessor above shares, so the
    // ladder cannot start answering the question differently from the dive.
    int movesToLearnIn(const std::vector<const char*>& pool) const;
    // Is the fight on screen one the WORLD lets a pet learn from at all? A duel and an
    // arena bout are another operator's pet and a Sim dummy is a prop, so none of them
    // rolls a move drop, and nothing on screen may offer one. Read by the outro (which
    // adds "and won") and by the mask the KIT page marks its prizes from.
    bool combatCanTeach() const;
    void startEncounter();                          // roll the malbeast, open the intro
    CombatEnemy rollWild();                         // the wild for this zone, scaled
    // EncounterHost: the intro's three ways out, which the walk's auto-resolve takes too.
    const ItemDef* heldEncounterBypass() const override;
    void fightWild(const CombatEnemy& wild) override;
    void resolveFlee(const CombatEnemy& wild) override;
    void resolveSinkhole(const ItemDef& bypass) override;   // spend it for its XP
    // Live-stakes combat against `wild`: an encounter's roll, or an area guardian.
    void startWildCombat(const CombatEnemy& wild, bool forceEnemyFirst);
    // The tail both debug combat hooks share once the player is built: roll the fight
    // open against `spec` and hand the screen to it (debugStartCombat / ...Vs).
    void beginDebugCombat(const Combatant& p, const CombatEnemy& spec, bool live);
    Combatant buildPlayerCombatant();               // + the ally-buff, if active

    // Boss ladder. Two confrontations share the carried-Health round plumbing
    // (build a gauntlet, run its rounds back-to-back with no heal between):
    //   • startSubAreaBoss(area,sub). A single strong malbeast unlocked by a 10-win
    // streak (OperatorWorld::subBossUnlocked); a full clear marks the SUB-AREA cleared.
    //   • startAreaBoss(area): a 5-stage gauntlet of the area's sub-area bosses,
    //     reachable once all 5 sub-areas are cleared; a full clear marks the AREA
    // cleared (OperatorWorld::sectorCleared → next area) + grants the Title.
    // Each opens a GauntletRun (core/model/gauntlet_run.h). startBossRound(carryHealth)
    // begins one round; finishBossRound() advances, clears or fails it.
    void startSubAreaBoss(int area, int sub);
    void startAreaBoss(int area);
    void startBossRound(int carryHealth);
    void finishBossRound();

    // THE STORY (game_story.cpp). A chapter fires ONCE, on the walk's own milestone,
    // and is readable forever after from EXPL's CHAPTERS list.
    //
    // fireStory/fireStoryPair are the gate every beat goes through: they answer false
    // when the beat is unauthored or already read (in which case the caller carries
    // straight on as though no story existed), and true when they have taken the
    // screen, in which case the caller returns and `then` is what happens once the
    // reader is done. That is what lets a fire point be one line at the top of the
    // thing it interrupts.
    bool fireStory(int sector, StoryBeat beat, StoryThen then, int thenArea = -1);
    bool fireStoryPair(int sector, StoryBeat first, StoryBeat second, StoryThen then);
    void openStoryChapter(const StoryChapterDef* chapter, StoryThen then);
    void markStoryRead(const StoryChapterDef* chapter);
    void onStory(const ButtonEvent& ev);
    void advanceStoryPanel();      // B on a page, and what the panel deadline calls
    void finishStoryChapter();     // the queued second chapter, or hand back to `then`
    void armStoryPanel();          // restart the auto-advance clock for this window
    void drawStoryScreen(Framebuffer& fb) const;
    // The CHAPTERS list. `storyArchiveRows` resolves the unlocked chapters into the row
    // models the screen draws and is the ONE place the archive's order and membership
    // are decided: the list, its cursor and the B that opens a row all read it.
    void openStoryArchive();
    void onStoryArchive(const ButtonEvent& ev);
    void drawStoryArchiveScreen(Framebuffer& fb) const;
    std::vector<StoryEntry> storyArchiveEntries() const;

    // THE SHIBOLEET (game_shiboleet.cpp). startShiboleet() is reached from the Wi-Fi
    // event when the sighting queue came up empty and the dry-streak cadence is due
    // (game_net.cpp): it grades the pet's fluency, and either resolves in place (an
    // AFFRONT into the guardian's fight, a BOON straight back to the walk) or opens
    // Nav::Shiboleet with a riddle to answer. onShiboleet() is the press path;
    // answerShiboleet() commits the focused reply and is also what the ~15s hold calls
    // with nothing focused, since silence is an answer here. startGuardianCombat() is
    // the shared way into the fight, from an affront and from a failed riddle alike.
    void startShiboleet();
    void onShiboleetHail(const ButtonEvent& ev);
    void openShiboleetWelcome();
    void onShiboleet(const ButtonEvent& ev);
    void answerShiboleet(bool answered);
    void enterShiboleetVerdict();
    void onShiboleetVerdict(const ButtonEvent& ev);
    void finishShiboleet();
    void grantBoon();
    void startGuardianCombat();
    // Pay one unspent SHAKE for the next sigil of the Cant. Returns whether it bought
    // one: false when the purse is empty (the riddle's other rewards are paid anyway,
    // which is the whole point of the gate sitting HERE and not on the win) or when the
    // Cant is already complete.
    bool buySigil();

    // Wi-Fi network event: a self-contained typed event. First resolves real-
    // network discovery (resolveNetworkDiscovery: pops the pending sighting
    // queue against the NetworkLedger; sets netDiscoveryFlavor_), independent of
    // and always followed by the Sleeping/Awakened-Guardian / Open-Cache /
    // Friendly-Visit roll (sets wifiFlavor_; unrelated flavor/combat system), then
    // B plays that roll's outcome out (loot / wild combat / ally buff).
    void startWifiEvent();
    void onWifi(const ButtonEvent& ev);
    void resolveWifiOutcome();
    // Real-network discovery resolution: the sole place a real network is
    // credited (Hacker Rank XP + Sealed Cache) or an occasional-repeat pet-XP is
    // granted. See game_net.cpp for the four-outcome breakdown (new / familiar
    // repeat / home-turf repeat / empty queue).
    void resolveNetworkDiscovery();

    // Shop event: a storefront explore event, either the current sector's item shop
    // (startShopEvent, Nav::Shop) or its mod shop (startModShopEvent, Nav::ModShop).
    // Both resolve to shopFront_, an AreaStorefrontDef (src/core/content/areas/) whose
    // arbitrary-length `listings` is the ONE place a row's stock/Bits price/item
    // costs live (the area's own price, distinct from an item's own Pedia-facing
    // bitsPrice). A cycles shopCursor_ through every listing (wraps); B buys the
    // SELECTED row (Bits + every item cost -> inventory add / mod grant) while its
    // stock + wallet + item costs allow; C leaves back to the Walk. Stock is
    // per-visit (shopStockLeft_, refilled from each row's own ShopListingDef::stock
    // on entry, not persisted). shopStatusLine() derives the grayscale-safe reason a
    // buy is/isn't available for the selected row.
    void startShopEvent();
    void startModShopEvent();
    void onShop(const ButtonEvent& ev);
    void buyShopItem();
    // Why row `i` cannot be bought, as the spelled-out WORDS the sign shows, or nullptr
    // when it can. One function so the till and the sign are the same answer rather than
    // two lists of conditions that have to be kept in step: shopRowBuyable() is this
    // asked as a yes/no, and shopStatusLine() is it asked about the selected row.
    const char* shopRowRefusal(int i) const;

    // Key warp items. A "Key-item find" walk event drops a
    // random warp key into the inventory (non-interrupting, like the Sealed Cache);
    // then, on the walk, B opens a picker of held warp keys and using one jumps
    // straight to its target event: Access Token → the nearest shop
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

    // The ITEM OFFER (game_offer.cpp): items that only matter at one rare moment are put
    // in front of the player AT that moment, instead of waiting in the bag for someone to
    // remember them. Starting a DeepWeb Dive offers the held bells and Deep-Learning
    // devices; an egg that starts incubating offers the Boot Accelerator. Each offer is
    // a list of the held items that would do something right now (itemUsable), plus a
    // last row that carries on without them. B on an item uses it and keeps the list
    // open (a bell and a device both go in before a dive); B on the last row carries on;
    // C backs out. Nothing is offered when nothing held would help, so the moment
    // passes exactly as it did before.
    enum class OfferFor : uint8_t { Dive, Egg };
    std::vector<const ItemDef*> offerItems() const;
    bool offerRelevant(const ItemDef& d) const;
    void offerThenDive();           // the EXPL list's dive row: offer, else dive now
    void openItemOffer(OfferFor f);
    void onItemOffer(const ButtonEvent& ev);
    void finishItemOffer();         // the last row: carry on with what was paused
    void useOfferedItem(const ItemDef& d);
    void autoUseDiveGear();         // AUTO DIVE GEAR at YES: arm the best of each kind
    void drawItemOfferScreen(Framebuffer& fb) const;

    // Hacker Rank: re-derives rank from op_.networksSeen after
    // every new unique network and arms a pending celebration on a crossing.
    // applyPendingRankUp() surfaces it the next time nav_ lands back on Walk
    // (both the direct Wi-Fi-resolve path and the wild-combat-return path can
    // get there, so both call sites check it).
    void checkHackerRankUp();
    void applyPendingRankUp();

    void enterHackerTagEditor();                   // copy the tag into the edit buffer
    void saveHackerTag();                          // commit the edit buffer to the tag

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
    // Generic Rig Shop buy: looks up `row`'s RigUpgradeDef (game_rig_shop.h), checks
    // offered/afford, deducts Bits, bumps op_.rig.level[row], applies the row's
    // RigEffectKind (only Bandwidth has one. The live-pool grant), logs, persists. One
    // function for every row: adding a row is a table entry, not a new buyX().
    void buyRigUpgrade(int row) override;
    // The OWNING half of that, with no till in it: bump the row `levels` times, apply
    // its RigEffectKind each time, log and persist. Split out because a rig level can
    // now also be WON (a solved Decryptogram's prize, game_cryptogram.cpp): a granted
    // level and a bought one must land identically, and this is the one place either
    // lands. Out-of-range rows and non-positive counts are inert.
    void grantRigLevel(int row, int levels);

    // MERGE HUB (game_merge.cpp): the f Rig Shop unlock combines owned ingredient
    // items into a rarer one. Owning the Hub makes the MRG slot accessible; each
    // RECIPE is won separately off a Decryptogram (recipeOwned, below) and gates
    // whether it can be cooked: knowing the recipe is necessary but not sufficient,
    // the raw ingredients still have to be in the bag.
    void onHackerMerge(const ButtonEvent& ev);     // MERGE HUB list: A cycle · B craft · C back
    void drawHackerMerge(Framebuffer& fb) const;   // MERGE HUB screen, header band included

    // CREW (game_crew.cpp): enlist in a crew (content_crews.h) and designate the
    // HOME NETWORK membership hangs off. Four views (CrewView, above); this pair is
    // the whole screen, and unlike its sibling Hacker sub-screens it draws its OWN
    // header band, because the title changes with the view (CREW / RED / the crew's
    // own name) and only the screen knows which one is up.
    void onHackerPeers(const ButtonEvent& ev);     // PEERS list: A cycle · C back (B unbound)
    void drawHackerPeers(Framebuffer& fb) const;   // PEERS screen, header band included
    bool captureHoldsRadio() const override { return auditCapture_.capturing(); }
    // The crew Exploit row the A+C picker should offer, or a default-constructed
    // (label == nullptr) one when the player belongs to no crew.
    CrewExploit crewExploitOption() const;
    // Cooking, indexed by kMergeRecipes row (game_internal.h). Ownership is a bit per
    // recipe wire in op_.rig.recipesOwned, and this trio is the only code that knows that.
    bool recipeOwned(int recipeIndex) const;       // won off a Decryptogram, never bought
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
    void drawHackerSubmenu(Framebuffer& fb) const;  // L2 dispatch, one case per HackerSlotId
    void drawHackerProfile(Framebuffer& fb) const;
    // VaultHost. A cache opens for a reward draw, a Decryptogram rolls a quote and
    // starts a board: the one place a VAULT row resolves, so the tap and the bulk hold
    // cannot diverge.
    void cashVaultRow(const ItemDef& d) override;

    // Persistence ------------------------------------
    SaveData captureSave() const;                  // snapshot all persisted state
    void applySave(const SaveData& d);             // restore from a loaded snapshot
    // captureSave's sections, each the mirror of the restore* of the same name.
    void captureBag(SaveData& d) const;
    void captureHistory(SaveData& d) const;
    void captureRadio(SaveData& d) const;
    void captureOperator(SaveData& d) const;
    void captureTourneyRun(SaveData& d) const;
    void captureActivePet(SaveData& d) const;
    void captureSettings(SaveData& d) const;
    void restoreBag(const SaveData& d);
    void restoreHistory(const SaveData& d);
    void restoreRadio(const SaveData& d);
    void restoreOperator(const SaveData& d);
    void restoreTourneyRun(const SaveData& d);
    void restoreActivePet(const SaveData& d);
    void restoreSettings(const SaveData& d);
    void settleBackgroundPick();
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
    // Write the Hacker-Log line for a permanent grant an Epic dish has just made
    // (core/model/pet_upgrades.h): "<what> <how much> FOR LIFE". The ordinary "FED <dish>"
    // line says a plate was eaten and nothing more, and a once-per-pet upgrade is the one
    // thing a bite can do that the operator will want to be able to find again.
    void noteLifetimeGrant(const char* what, const char* valueFmt, int magnitude);
    // Open a Sealed Cache: consume the container + draw from its
    // rarity tier's pool, then surface the yield (Nav::CacheYield).
    void openSealedCache(const ItemDef& d);
    // The shared per-cache reward roll (Bits + up to `maxItems` drawn items, plus the
    // Epic-cache bonus-mod roll): factored out of openSealedCache so e's bulk
    // path can call it once per cache opened without duplicating the reward logic.
    // Does NOT consume the container or touch nav_/log_'s per-open "OPENED" line:
    // callers own both (openSealedCache logs+navigates once; the bulk path batches).
    void grantCacheReward(const ItemDef& d, int& bitsOut, const ItemDef** itemsOut,
                          int& itemCountOut, int maxItems);
    // Pick one item from a weighted loot pool: the shared draw every reward pool
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
    void openAllCachesOfRarity(const ItemDef& focused) override;
    // The bulk-yield reveal: A scrolls the tally if it overflows the window, B/C
    // dismisses back to the VAULT list (Nav::Submenu. Mirrors onCacheYield).
    void onBulkYield(const ButtonEvent& ev);
    void drawBulkYieldScreen(Framebuffer& fb) const;
    // Boot Accelerator: consume it + apply its CutIncubationMin to the incubation.
    void useBootAccelerator(const ItemDef& d);
    // Rollback: open the stat picker (nav_ = RollbackPicker) parked on the
    // first eligible stat; onRollbackPicker drives A cycle / B shed / C cancel;
    // rollbackEligible tests a stat has ≥1 earned point (skipped otherwise);
    // drawRollbackPickerScreen renders it.
    void openRollbackPicker();
    void onRollbackPicker(const ButtonEvent& ev);
    bool rollbackEligible(int stat) const {
        return stat >= 0 && stat < kLevelStatCount && pet_.statPoints[stat] > 0;
    }
    int nextEligibleStat(int cur) const;            // A-cycle target (skips 0-point stats)
    void drawRollbackPickerScreen(Framebuffer& fb) const;
    // Repartition: the same stat table, moved rather than shed. One nav state
    // (Nav::RepartitionPicker) over TWO steps, because the two questions are the same
    // question asked twice and a second Nav row would only make the A/B/C contract
    // harder to read: repartitionFrom_ < 0 is step one (pick the stat to take FROM,
    // rollbackEligible again. A point can only be moved off a stat that has one), and
    // once it is set the cursor is picking the stat to move it TO. B advances the step
    // then commits; C walks it back one step rather than straight out, so a mis-picked
    // source costs one press. Nothing is spent until the commit.
    void openRepartitionPicker();
    void onRepartitionPicker(const ButtonEvent& ev);
    // The TO step's A-cycle target: any stat but the source (a move onto the stat the
    // point is already on is a no-op, and the picker should never offer one).
    int nextRepartitionTarget(int cur) const;
    void drawRepartitionPickerScreen(Framebuffer& fb) const;
    void startMaint();
    // What every played game reports when it ends, whoever was paying for it. `units`
    // and `bitsUnits` are what a MAINT process prices (ArcadeGameDef::unitName /
    // bitsUnitName: cleaning, and Bits); `score` / `scoreMax` are what the arcade till
    // prices. One shape for every engine is what lets any of
    // them stand behind either MAINT process.
    struct PlayOutcome {
        int units = 0;
        int bitsUnits = 0;
        bool won = false;
        int score = 0;
        int scoreMax = 0;
    };
    // Open `g`'s board for whichever context is armed (an ArcadeRun or maintKind_). False
    // for an engine that cannot be played outside its own context (a hatch game).
    bool beginPlayedGame(ArcadeGameKind g);
    // Route a finished run to its context: the arcade till, or the MAINT process.
    void settlePlayedRun(const PlayOutcome& o);
    void settleMaintPlay(const PlayOutcome& o);
    // What cleaning `units` / `bitsUnits` are worth on the current process and dial, for
    // the HUD and the payout.
    int playedFragWorth(int units) const;
    int playedBitsWorth(int bitsUnits) const;
    // The footer a played board shows for a run worth this much so far, in whichever
    // context is armed: the one place a board learns what it is for.
    PlayHud playHud(const PlayOutcome& sofar) const;
    void beginStackerBoard();   // reset the board + enter it, whoever is paying for it
    void finishStacker();
    void stepStacker();
    void beginFraggerRun();     // the same, for FRAGGER
    void stepFragger();         // one row of fall, with its sound
    void finishFragger();
    void resolveMaint();
    bool rollMaintSuccess();

    // Care-mistake increment with the v21 Restore-Point shield: a POSITIVE `n` is
    // BLOCKED (and the shield consumed) when pet_.grants.shieldActive is armed; a negative
    // `n` (e.g. Yubi-Cookie's −1 removal) is unaffected. The single DRY seam every
    // Game-level positive-mistake path (went-hungry, Lockout expiry) funnels through.
    void addCareMistakeShielded(int n);

    // Advances the model's Hunger by `elapsedMs` and pays Passive XP Farming
    // (j/k, Rig Shop) on the portion of the resulting decay that fell above
    // hungerXpThreshold(). The one place pet_.model.tick() is called for Hunger
    // decay, so Game::tick() and debugTickHunger() (tests) can't drift apart.
    void tickHungerAndAwardXp(uint32_t elapsedMs);

    // --- The six halves of Game::tick, in the order it runs them ------------------
    // Each returns whether it changed anything the screen would show, and tick() runs
    // all six unconditionally: the order between them is load-bearing, so none of
    // them may be skipped on an early answer. They are separate because they run on
    // separate CLOCKS, not because tick() was long: the heartbeat, four independent
    // sub-cadences, real-ms deadlines and held-button dwells all land in one function
    // and are otherwise only told apart by which timestamp each `if` happens to read.
    //
    // Every one of them is called from tick() and nowhere else.
    bool tickModelClocks(uint32_t nowMs);    // decay, bandwidth, the egg clock, sweeps
    bool tickHeartbeat(uint32_t nowMs);      // the shared ~4fps beat and what rides it
    bool tickAnimClocks(uint32_t nowMs);     // the faster per-screen cadences
    bool tickLifecycle(uint32_t nowMs);      // CSF, evolution, Lockout, the autosave
    bool tickHeldGestures(uint32_t nowMs);   // every hold-B second action, and A/C repeat
    bool tickIdleDefocus(uint32_t nowMs);    // the menu tree collapsing back to Idle
    // tickHeartbeat's steps, one per thing that rides the beat.
    void stepRestingMotion(RestingTrick trick);
    void stepRestingTricks(const CreatureLine* petLine, RestingTrick trick);
    void stepScreenBeat();
    void stepCombatBeat();
    void stepWalkPacer();
    // tickAnimClocks' cadences. Closed: its screen is not up, and its clock is primed.
    // Open: the screen is up and the cadence is live, so the frame rate stays high.
    // Fired: it stepped this tick and the screen needs a repaint.
    enum class Cadence : uint8_t { Closed, Open, Fired };
    Cadence tickCombatAnim(uint32_t nowMs);
    Cadence tickFxSweep(uint32_t nowMs);
    Cadence tickCryptogramRepeat(uint32_t nowMs);
    Cadence tickStackerSlide(uint32_t nowMs);
    Cadence tickFraggerFall(uint32_t nowMs);
    Cadence tickIsolationCrawl(uint32_t nowMs);
    Cadence tickChromaSweep(uint32_t nowMs);

    // Lockout lifecycle.
    void fireLockout();
    void expireLockout();
    void resolveLockout();
    // Carousel index of the slot routing to `id`: for the paths that park the
    // cursor on a submenu they weren't navigated into (a Lockout opening ITEMS, a
    // Sim-Battle returning to the LOADOUT hub). Falls back to 0 if `id` has no slot.
    int carouselSlotOf(SubmenuId id) const;

    // Decryption Hatch lifecycle
    void startHatch();       // empty save -> line-select (>1 line) or lay the egg
    void layEgg(const EggLineDef* line);  // commit a chosen line: lay its Boot-Sector egg
    const CreatureDef* rollHatchProcess(const EggLineDef* line);  // random Process draw (advances rng_)
    // DISK DECRYPTION lifecycle (game_decryption.cpp): see inDecryption() above.
    // `allowDuplicates` lets the key repeat a colour and `easyHints` outlines the cells
    // a played row got exactly right; the hatch passes false to both, and only the
    // arcade's dial moves either.
    void startDecryption(bool allowDuplicates, bool easyHints);
    void onDecryption(const ButtonEvent& ev);
    void finishDecryption();   // bank the crack against the clock (or the till) and leave
    void drawDecryption(Framebuffer& fb) const;

    // DECRYPTOGRAM lifecycle (game_cryptogram.cpp): see inCryptogram() above.
    // `quote` is a content_quotes.h roster row and `extraReveals` the opening gift; the
    // VAULT reads both off the quote's own tier, and the arcade's dial supplies the
    // second. Nothing here rolls the quote: pickQuote does that, once, at the cash-in.
    void startCryptogram(int quote, int extraReveals);
    void onCryptogram(const ButtonEvent& ev);
    // Runs the instant the board stops, not when the player leaves it: the tier ratchet
    // and the prize both have to have LANDED before the verdict screen can name them.
    // A no-op on an arcade run, which never touches a quote's standing.
    void settleCryptogram();
    void finishCryptogram();   // leave the board: to the arcade's till, or to the VAULT
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
    // The unlock's display name for the board's prize line, or nullptr: resolved off
    // whichever content table the reward kind names.
    const char* cryptogramPrizeLabel() const;
    void completeHatch();    // decrypt done -> hatch straight to Process, return to idle
    const SpriteData* hatchEggSprite() const;
    void startHatchGame(const EggLineDef* line);  // route a freshly laid egg to its HatchGame
    void drawLineSelect(Framebuffer& fb) const;  // the line-select modal

    // Clutch Pick lifecycle (game_eggpick.cpp): see kEggPickRounds above. `rounds` is
    // how many halvings this run asks for: the hatch always passes kEggPickRounds, and
    // the arcade passes its difficulty.
    void startEggPick(int rounds);       // hide the live egg in the clutch, open the modal
    void eggPickAim(bool secondHalf);    // A/C: highlight the first/second half
    void eggPickCommit();                // B: halve, or dismiss the reveal
    void drawEggPick(Framebuffer& fb) const;

    // Hatch reveal cinematic (game_eggpick.cpp): the on-demand crack, see
    // hatchRevealReady(). Non-interactive: it runs on the heartbeat and hatches itself.
    void openHatchReveal();
    void drawHatchReveal(Framebuffer& fb) const;

    // Isolation Protocol lifecycle (game_isolation.cpp): see inIsolation() above.
    // `goalDots` is how many bytes make a CLEAN run: the hatch prices it off whatever
    // is left of the incubation clock, the arcade off a flat target.
    void startIsolation(int goalDots);   // turn the worm loose in the buffer, open the screen
    void onIsolation(const ButtonEvent& ev);
    void finishIsolation();              // spend the bytes on the clock and leave
    void drawIsolation(Framebuffer& fb) const;

    // CHROMATOPHORE lifecycle (game_chroma.cpp): see inChroma() above. `windowMs` is
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
    const char* evolutionTargetId() const;  // the successor id (care-picked pool)
    void fireEvolution();          // enter the modal toward the successor
    void completeEvolution();      // commit the swap, restart the in-stage clock, idle

    // Stamp the Attack/Defend kind of every UNLOCKED slot
    // that's still Unset from the CURRENT pet_.creature's CreatureDef::slotKinds, then never
    // touch that slot again, so which creature occupies the evolution path at the
    // moment a slot unlocks decides that slot's kind for the rest of the raise.
    // Also backfills: any unlocked slot still empty gets the strongest owned+
    // unlocked move of its stamped kind not already sitting in another slot, so a
    // pet is never idling a slot on Quick Jab when it's earned something real to
    // put there, including a move unlocked at an earlier stage than the slot
    // itself. Called after every pet install: completeHatch, completeEvolution,
    // applySave, and an ARCH Deploy swap. A no-op if pet_.creature is null.
    void stampSlotKinds();
    // Defensive invariant: unequip any move whose kind no longer matches its
    // slot's stamped kind (a pre-v24 save migration, or any other drift). Called
    // right after stampSlotKinds() wherever a pet is (re)installed from stored
    // state (applySave, Deploy): a fresh hatch/evolve never needs it (a
    // newly-unlocked slot starts empty).
    void enforceSlotKindInvariant();

    // Critical System Failure: the ONLY death path. The 5/5 dying state,
    // once its ageing/recovery window expires, is terminal: the pet becomes a
    // [CORRUPTED] ARCH record and the active save vacates -> the Decryption Hatch.
    bool csfRevealed() const;      // crash FX held long enough that B acknowledges
    void fireCSF();                // enter the terminal modal
    void acknowledgeCSF();         // B: archive the corpse -> record + new-egg Hatch

    ContentRegistry registry_;
    // The active pet (core/model/pet.h).
    Pet pet_;
    // Purely presentational: where the pet is standing on its shelf right now.
    // Stepped once per heartbeat off pet_.creature->locomotion and parked when there is no
    // mover, so it never needs a reset hook on hatch, evolution or a loaded save.
    IdleWander petWander_;
    // The resting pet's ambient FX_CAMO drift: the clock (core/model/idle_camo.h), and
    // the art it is currently sampling. Same split as the Combat screen's camo
    // (game/fight_screen.h): the tick holds a level and a sprite, the draw ranks that
    // sprite into a ladder.
    // Null between drifts, which is also what says nothing is worn.
    IdleCamo petCamo_;
    const SpriteData* idleCamoWorn_ = nullptr;
    IdleVanish petVanish_;
    IdleMimic petMimic_;
    const SpriteData* idleMimicWorn_ = nullptr;
    // ...and where the ambient copies beside it are standing. One per replication
    // slot, of which idleCompanionCount() are live; each is seeded apart in Game's
    // constructor, since identical wanders move identically.
    IdleWander companionWander_[kWormReplicaSlots];
    IdleFork petFork_;
    IdleRelay petRelay_;
    IdleDisguise petDisguise_;
    IdleHarvest petHarvest_;
    IdleFeeding petFeeding_;
    IdleLure petLure_;
    Inventory inventory_;
    EventLog log_;
    // The player across every pet (core/app/operator.h). WIPE EVERYTHING replaces it.
    Operator op_;
    // The device's preferences (core/app/settings.h). Nothing resets it.
    Settings settings_;
    // Repaint and schedule a save after a setting changed.
    void settingChanged() {
        dirty_ = true;
        markSaveDirty();
    }

    Nav nav_ = Nav::Idle;
    int cursor_ = 0;            // focused carousel slot (also the entered slot)

    // L2/L3 state.
    int listRow_ = 0;                      // selected row (ITEMS row idx / MAINT 0..1)
    // EXPL's THREE-level nav. `explCat_` is the activity (ExplCat::None = the picker
    // itself, EXPL's own top level); `explNavArea_` is the level INSIDE one: -1 = the
    // category's own rows, >= 0 = inside that area (its sub-areas). `listRow_` indexes
    // the picker's four rows while explCat_ is None and the ladder's row space
    // otherwise: the same field either way, because only one of the two levels is ever
    // on screen. Runtime-only; openExplList re-derives both every time EXPL is opened.
    ExplCat explCat_ = ExplCat::None;
    int explNavArea_ = -1;
    // AUTO-PROGRESS: the walk steps the ladder by itself (autoProgressStep). Toggled
    // with the A+C chord from inside the explore-control overlay, which is only
    // reachable while explore-mode is running, so the mode is scoped to exploring by
    // construction. Runtime-only, and deliberately NOT persisted: explore-mode itself
    // doesn't survive a reboot, so a restored flag would arm a mode with nothing to act
    // on. It DOES outlive one walk, so cancelling and re-arming explore keeps it set.
    // Defaults ON: hands-off ladder stepping is the expected experience; the chord
    // opts OUT rather than in.
    bool autoProgress_ = true;
    // Focused row of the explore-control overlay (0..kExploreControlRows-1). Reset to
    // the first action each time the chord opens it, so the list never reopens pointing
    // at whatever was last pressed.
    int exploreCtlRow_ = 0;
    const ItemDef* detailItem_ = nullptr;  // ITEMS detail subject
    // The two cache reveals (core/ui/items_screen.h), each held only while it is up.
    std::optional<CacheReveal> cacheReveal_;
    std::optional<BulkReveal> bulkReveal_;
    // The MAINT action page's: which process it has open.
    MaintKind maintKind_ = MaintKind::Defrag;
    // The focused payment VARIANT on the open MAINT action. DEFRAG: 0 = Quick (Bits,
    // may fail) · 1 = Tool (an item, guaranteed) · 2 = Played. AV: 0 = Quick · 1 = Played.
    // The played one is the only one that can come up short without failing.
    int maintVariant_ = 0;
    // Which engine each process's played variant runs (debugSetMaintPlayedGame).
    ArcadeGameKind defragPlayedGame_ = kDefragPlayedGame;
    ArcadeGameKind avPlayedGame_ = kAvPlayedGame;
    // The action page's dial for its PLAYED row; a run takes a copy when it starts.
    ArcadeDifficulty maintDifficulty_ = ArcadeDifficulty::Easy;
    // The MAINT process in play (core/model/maint_run.h): its board or its process
    // screen. Empty otherwise.
    std::optional<MaintRun> maint_;
    Stacker stacker_;
    uint32_t lastStackerStepMs_ = 0;
    Fragger fragger_;
    uint32_t lastFraggerStepMs_ = 0;
    // The last shot, for the beam the screen draws up its lane until the next row falls.
    int fraggerShotLane_ = -1;
    int fraggerShotRow_ = -1;
    // What a DeepWeb Dive win pays on top, in percent: both halves, added. Zero outside
    // a dive, so a sector walk never collects a bonus armed for the dive after it.
    int diveRewardBonusPct() const {
        return inDeepWebDive() ? pet_.armed.diveStartBonusPct + pet_.armed.diveStepBonusPct : 0;
    }
    // Every site that ends a dive (or a pet) spends both live halves in one call, so the
    // bonus can't outlive the run it was paid for at one exit and not another.
    void clearDiveDepthBuffs() {
        pet_.armed.deepWebDepthMultiplier = 1;
        pet_.armed.diveStepBonusPct = 0;
        pet_.armed.diveStartBonusPct = 0;
    }
    // The depth a dive would actually START at right now (-1 = nothing armed), with the
    // Checkpoint sentinel resolved through pet_.bestDeepWebDepth exactly as startDeepWebDive()
    // resolves it. Anything COMPARING bells reads this rather than the raw field: the
    // sentinel is a negative number, so a raw compare sorts a live Checkpoint arming below
    // "nothing armed" and lets a shallower bell overwrite it. The downgrade paid for
    // with an item that the inert check (itemUseIsInert) exists to refuse.
    int armedDeepWebStartDepth() const {
        if (pet_.armed.deepWebStartDepth == PetArmed::kStartDepthUseBest) return pet_.bestDeepWebDepth;
        return pet_.armed.deepWebStartDepth;
    }

    // CFG submenu. cfgScreen_ is the entered L3; cfgGroupRow_ the focused row of a
    // group screen (DEVICE / RADIO), kept apart from listRow_ so backing out of a
    // group lands on the row that opened it; cfgUiPick_ the focused UI Mode option;
    // factoryScope_ the Factory-Reset scope. bHeld_/bDownMs_ track a held B for the
    // hidden hold-to-reveal / hold-to-commit gestures.
    CfgScreen cfgScreen_ = CfgScreen::SysInfo;
    int cfgGroupRow_ = 0;
    int cfgUiPick_ = 0;
    int cfgBrightPick_ = 0;   // Brightness picker focus (0..kBrightnessLevels-1)
    int cfgSoundPick_ = 0;
    int cfgVolumePick_ = 0;
    int cfgThemePick_ = 0;    // Theme picker focus (0..kPalThemeCount-1)
    int cfgAuditPick_ = 0;    // Audit level picker focus (0 OFF, 1 SCAN, 2 SCAN+CAP)
    int cfgTravelPick_ = 0;   // Travel-mode confirm focus (0 = NO, 1 = YES)
    int cfgSdFormatPick_ = 0; // FORMAT SD confirm focus (0 = NO, 1 = YES)
    int cfgApPick_ = 0;       // 'Pedia AP toggle focus (0 = OFF, 1 = ON)
    int cfgLinkPick_ = 0;     // pet-to-pet LINK toggle focus (0 = OFF, 1 = ON)
    int pediaQrPage_ = 0;     // PEDIA QR step (see pediaQrPage())
    int cfgTitlePick_ = 0;    // Titles picker focus (0 = NONE, else 1+sector index)
    // BACKGROUND picker focus: 0 = AUTO, else 1 + the row's index in kBackgrounds.
    // A row index rather than a SceneId, because the picker walks LOCKED rows too and a
    // locked row is a position in a list rather than a place the operator has.
    int cfgBgPick_ = 0;
    int factoryScope_ = 0;
    // Every screen-level hold on the device is a held B: the CFG Factory Reset, the
    // Hacker VAULT's bulk-open, the ITEMS type-filter cycle and the MOVES picker's
    // show-all. They share this one flag because they are guarded to four different
    // screens and so can never be armed at the same time; the release edge
    // (Game::onButton) offers the press to each in turn and at most one claims it.
    bool bHeld_ = false;
    uint32_t bDownMs_ = 0;
    // A's list-step repeat (game_listnav.cpp), plus the DECRYPTOGRAM's own faster one.
    // Its OWN flag, distinct from bHeld_: A and B are independent physical buttons
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
    // HackerTag on-device arcade editor: a working buffer + caret. Caret
    // kHackerTagMax = the ⏎ confirm cell.
    char editTag_[kHackerTagMax + 1] = {0};
    int editCaret_ = 0;

    // ARCH submenu: the focused record action. The rack itself is the Operator's
    // (OperatorArch). archConfirm_ is the inline confirm for Store/Deploy.
    ArchAction archAction_ = ArchAction::Store;
    SaleOffer archSaleOffer_ = SaleOffer::Bits;    // the SELL sheet's focused offer
    InlineConfirm archConfirm_;
    // Which of ARCH's two L2 screens is showing, which group the list is showing, and
    // the picker's own cursor. archGroup_ also survives into L3: the detail screen asks
    // it whether it is drawing the NEW EGG confirm or a pet's record.
    ArchScreen archScreen_ = ArchScreen::Picker;
    ArchGroup archGroup_;
    int archPickRow_ = 0;

    // MODS submenu. pet_.mods is the equip state; modSlot_ is the slot
    // whose picker is open; modPick_ the picker row (0 = unequip). The inline
    // overwrite confirm is a picker sub-state (modConfirm_ and the pending
    // mod id). Mod EFFECTS apply in combat: equip is real.
    int modSlot_ = 0;
    int modPick_ = 0;
    InlineConfirm modConfirm_;
    const char* modPendingId_ = nullptr;
    // selecting a mod in the picker opens its detail (read-then-act, mirrors
    // ITEMS). modDetail_ overlays the picker; modDetailId_ is the inspected mod.
    bool modDetail_ = false;
    const char* modDetailId_ = nullptr;

    // Which half of the LOADOUT hub is open (ui_state.h). Hub is the three-row menu
    // itself; the other three route the SAME Nav::Submenu/Nav::Detail pair to the
    // mod slots, the move slots, or the Sim-Battle tier pick, so C from any of them
    // returns here rather than to the carousel. loadoutHubRow_ is the hub's own
    // cursor, kept across an entry so leaving a page lands back on the row that
    // opened it.
    LoadoutTab loadoutTab_ = LoadoutTab::Hub;
    int loadoutHubRow_ = 0;

    // MOVES + PRACTISE (the LOADOUT hub's other two rows) + the shared combat
    // engine. trainRow_ is the focused MOVES row (always an unlocked slot); the L3
    // Detail is either the move picker or, under PRACTISE, the Sim-Battle tier pick
    // (trainScreen_).
    // The move picker mirrors MODS (slot/pick/confirm + pending). A fight's Health lives
    // in fight_ and is never persisted; pet_.xp/pet_.level persist (save v2).
    int trainRow_ = 0;
    // MoveDetail is the picker's reader half (hold-B): the picker chooses, the detail
    // page explains, so neither has to shrink to fit the other.
    enum class TrainScreen { MovePicker, MoveDetail, SimTier };
    TrainScreen trainScreen_ = TrainScreen::MovePicker;
    int moveSlot_ = 0;
    int movePick_ = 0;
    // First prose line drawn on MoveDetail: B advances it, wrapping past the end
    // (drawMoveDetail / moveProseLines, train_screen.h). Reset on every entry.
    int moveProseScroll_ = 0;
    // Move picker default view: only moves the pet can actually equip into
    // `moveSlot_` right now. Holding A past kMoveFilterHoldMs toggles the full
    // list (tick()); reset false whenever the picker is (re)entered (onTrainList).
    bool moveShowAll_ = false;
    // MoveDetail was opened by holding B on the SLOT LIST, so it reads the move already in
    // `moveSlot_` rather than the picker's cursor, and C returns to the slot list.
    bool moveDetailEquipped_ = false;
    InlineConfirm moveConfirm_;
    const char* movePendingId_ = nullptr;
    int simTier_ = 0;
    // The fight on screen, or the last one (core/model/fight.h).
    Fight fight_;
    // The Combat screen (game/fight_screen.h): its stats panel, stage clock and camo.
    FightScreen fightScreen_;
    // The stat index (0..3) of the most-recent level-up, or -1 if none yet: the
    // "which stat grew" tell (STAT's LVL n is the shipped feedback surface).
    int lastLevelUpStat_ = -1;
    int rollbackRow_ = 0;       // Rollback picker cursor (RollbackPicker nav state)
    // Repartition picker (RepartitionPicker nav state): the row the cursor is on, and
    // the stat the point is being taken from; -1 while that is still the question.
    int repartitionRow_ = 0;
    int repartitionFrom_ = -1;

    // The explore walk (core/model/explore_walk.h), a background mode on the IDLE
    // habitat. The wild-encounter intro and event screens are their own Nav states, past
    // the 3-deep menu budget, and suspend the global auto-defocus.
    ExploreWalk walk_;

    // Bandwidth is the FARMING resource: spent 1 per re-farm win to keep cleared-sub
    // loot full; when empty the decay curve applies until it regenerates on the
    // heartbeat over real time. Session-level, not per-pet, and not persisted.
    // bandwidthAccumMs_ carries the sub-step regen remainder.
    int bandwidth_ = kBandwidthMax;
    uint32_t bandwidthAccumMs_ = 0;
    // THE dissolve clock (FX_ABSORB / FX_SHRED), on the fast kFxAnimMs tick rather than
    // the heartbeat: a sweep is redrawn every frame it is up, and stepping it at 4fps
    // is what made it read as jumping rather than moving. One counter serves all three
    // hosts because only one dissolve is ever on screen: the feeding modal, the Wi-Fi
    // event, and the beaten rival's combat outro. Each resets it as its own sweep opens
    // and gates its advance (game_core.cpp), so it always means beats-since-THIS-sweep.
    //
    // Deliberately NOT ExploreWalk::eventBeat / Fight::resultBeat: a press restarts those, and a
    // dissolve animates something already resolved. Replaying it under a player leaning
    // on A would be a second helping that never happened.
    int fxBeat_ = 0;
    uint32_t lastFxMs_ = 0;
    // Set by tickAnimClocks each tick: true while any cadence it services is live. Read by
    // highFrameRate() (device-tier CPU-clock scaling). Derived from the same gates that run
    // the fast frames, not a parallel list, so the two cannot drift.
    bool fastCadenceActive_ = false;
    EncounterScreen encounter_;   // the wild-encounter intro (game/encounter_screen.h)

    // The boss gauntlet in play (core/model/gauntlet_run.h), empty between gauntlets.
    // Which bosses have been beaten is the Operator's (OperatorWorld).
    std::optional<GauntletRun> gauntlet_;

    // Grant an area's Title (OperatorWorld::titlesUnlocked). Called from the sector-clear
    // path.
    void unlockTitle(int sector);
    // The next selectable Title after `cur` (wraps): NONE (-1) plus each UNLOCKED
    // sector, in order. Locked sectors are skipped. Drives the CFG picker's A-cycle.
    int nextSelectableTitle(int cur) const;

    // --- Achievements (game_pedia.cpp) -----------------------------------------------
    // The earned and notified bitsets are the Operator's (OperatorReveals).
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
    // limited to kAchSweepIntervalMs: nothing here is latency-critical.
    void sweepAchievements();
    uint32_t lastAchSweepMs_ = 0;
    // Pay a freshly-unlocked row's `rewards`. One switch over AchievementReward::Kind:
    // a new kind of reward lands here, never as a new field on the row.
    void applyAchievementRewards(const AchievementDef& d);
    // Advance the home-screen announcement: retire an expired banner (marking it
    // notified) and raise the next pending one while the pet face sits idle.
    void tickAchievementBanner();
    int achBannerWire_ = -1;        // the row on screen, by wire number; -1 = none
    int achBannerCount_ = 0;        // 1 = named row · >1 = a collapsed burst of N
    uint32_t achBannerUntilMs_ = 0; // when it retires
    int achBannerOpenBeat_ = 0;     // the beat it arrived on: drives the wipe-open and
                                    // starts its marquee at the head of the line rather
                                    // than wherever the free-running beat happens to be

    // Record a dish on the active pet's palate. Idempotent, and a no-op for anything
    // that is not a Food: a Buff drunk from the bag is not a meal. Also where a
    // favourite dish is found.
    void markFoodEaten(const ItemDef& d);
    // Roll the active pet's favourite dish if it has none yet. Lazy, at the first meal
    // that needs it, so a pet from an older save or a rack slot that never had one gets
    // one the same way a new egg does. Draws off a hash rather than advancing rng_, so
    // feeding never shifts the shared sequence the spoilage and combat rolls read.
    void ensureFavouriteFood();
    // Fold the live inventory into the collected-items shelf. Called from the sweep
    // rather than wired into each grant site: every path that adds an item necessarily
    // leaves it in the bag for a tick, so one pass over the stacks catches all of them,
    // including the items an upgraded save was already holding.
    void sweepCollectedItems();
    // Record `depth` against the species if it beats that species' standing record.
    void recordSpeciesDive(const CreatureDef* c, int depth) {
        if (op_.shelves.recordDive(c, depth)) markSaveDirty();
    }
    // Seat `c` as the active pet. THE one place pet_.creature is assigned, so that becoming
    // this device's pet and being tallied as raised cannot come apart: every path
    // that installs a pet (a laid egg, a hatch, an evolution, an ARCH Deploy, a save
    // load) goes through here. nullptr clears the pet and tallies nothing.
    void installPet(const CreatureDef* c) {
        pet_.creature = c;
        if (c) markCreatureRaised(c->id);
    }

    // Wi-Fi network event. wifiOutcome_/wifiFlavor_ are set when the
    // guardian/cache/friendly roll happens (startWifiEvent) and consumed on B
    // (resolveWifiOutcome). Operator::networksSeen is the lifetime unique-REAL-network
    // counter, and only resolveNetworkDiscovery() bumps it.
    // allyBuffBattlesLeft_ is the friendly-visit buff consumed one per battle in
    // buildPlayerCombatant.
    WifiOutcome wifiOutcome_ = WifiOutcome::SleepingGuardian;
    char wifiFlavor_[40] = "";
    PowerStatus power_;  // last SMOOTHED reading from the platform tier
    // The battery fill currently on screen, which is the state batteryLevelStable
    // needs to hold a glyph steady across a reading that sits on a band edge. -1 =
    // nothing drawn yet, so the next reading lands without hysteresis.
    int batteryLevel_ = -1;
    int allyBuffBattlesLeft_ = 0;

    // THE SHIBOLEET: the guardian encounter (game_shiboleet.cpp, core/model/cant.h).
    //
    // What the ladder remembers is the Operator's (OperatorCant). Everything here
    // describes ONE encounter and is rebuilt every time a guardian speaks, which is why
    // the cipher in particular is not persisted: a mapping that survived a reboot would
    // be a mapping worth memorising instead of learning.

    ShiboleetWelcome shibWelcome_ = ShiboleetWelcome::Riddle;
    ShiboleetReply shibReply_ = ShiboleetReply::Pending;
    CantCipher shibCipher_;
    int shibRiddle_ = 0;        // index into riddles() (content_riddles.h)
    // Which authored reply sits on each shown row. The pool authors the TRUE reply first
    // (content_riddles.h), so the order shown has to be shuffled somewhere; doing it here
    // rather than in the pool is what keeps "add a riddle" a one-line edit.
    uint8_t shibOrder_[kRiddleReplies] = {0, 1, 2};
    int shibRow_ = 0;           // the cursor
    // Which of the guardian's greeting/demeanour pairs this meeting used
    // (GuardianLine, area_defs.h). Rolled per encounter, so a guardian met twice is not
    // word-for-word the same meeting.
    int shibLine_ = 0;
    // What the meeting came to, in words, for the VERDICT screen and for the walk's line
    // after it. `shibFlavor_` is the consequence ("LEARNED A SIGIL - 3/26", "IS NOT
    // ANSWERED"); `shibVerdictLine_` is the stat ledger beside it ("+6 HAPPY  -4 FRAG").
    // Both are written where the payout happens, so the screen only ever reads them.
    char shibFlavor_[40] = "";
    char shibVerdictLine_[40] = "";
    // The swarm the guardian IS. Volatile like everything else about one meeting: a flock
    // is reset per encounter, so nothing about the body a player watched is persisted.
    // It is the same creature next time and never the same shape.
    Flock guardianFlock_;

    // Real-network discovery (game_net.cpp, core/net/network_ledger.h). Three
    // structures answering three questions:
    //
    //   visibleNetworks_. What the radio can hear right now: a RAM-only snapshot
    //     (kNetVisibleCap) aged out by kNetVisibleFreshMs. Feeds the CREW picker and
    //     nothing else, so no reward bookkeeping can shape what the picker offers.
    //   pendingNetworks_. What still owes the player a reward: a RAM-only FIFO
    //     (kPendingNetworkQueueCap) drained one entry per EXPL Wi-Fi event. Evicts
    //     oldest-first when full, so an unwalked backlog goes stale rather than deaf.
    //   networkLedger_. What this pet has actually walked past: the SD-backed
    //     per-BSSID history deciding new-vs-familiar-vs-home-turf.
    //
    // netDiscoveryFlavor_ is the line resolveNetworkDiscovery() sets, drawn alongside
    // wifiFlavor_. emptyQueueStreak_ throttles the empty-queue Happiness penalty
    // (kNetDiscoveryEmptyCooldownStrikes) and resets whenever a sighting IS found.
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
    // re-heard: registerNetwork keys the egg-hatch nudge off that edge.
    bool noteVisibleNetwork(uint64_t key, const char* name);
    VisibleNetwork visibleNetworks_[kNetVisibleCap];
    int visibleNetworkCount_ = 0;
    PendingNetwork pendingNetworks_[kPendingNetworkQueueCap];
    int pendingNetworkCount_ = 0;
    NetworkLedger networkLedger_;
    char netDiscoveryFlavor_[40] = "";
    NetDiscovery netDiscovery_ = NetDiscovery::None;
    int emptyQueueStreak_ = 0;

    // Pet-to-pet discovery (game_peers.cpp, core/net/peer_link.h): the same
    // snapshot-vs-history split as the networks above. livePeers_ is a RAM-only
    // snapshot (kPeerVisibleCap) aged out by kPeerVisibleFreshMs, holding the FULL last
    // HELLO so a row shows what someone is raising this second. peerLedger_ is the
    // SD-backed roster, updated only on the ARRIVAL edge, which makes timesMet a count
    // of meetings rather than of frames, so the freshness window is the boundary
    // between one encounter and the next.
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
    // alongside them: nothing may persist that lets this device reach the 'net
    // without the operator asking again (see netConnectWanted).
    bool netProvisioned_ = false;
    NetStatus netStatus_;
    // Live largest-allocatable-block probe, wired by the platform tier
    // (see setHeapProbe). Null = nothing reports, which reads as "no reason to hold
    // back" and is the host build's normal state.
    HeapProbe heapProbe_ = nullptr;
    ISoundOut* soundOut_ = nullptr;
    // The one buffer every save is serialized into. Owning it is what keeps a write off
    // the allocator: it is sized once, behind the higher floor, and reused for good, so
    // a save landing while the radio holds the heap has nothing large left to ask for,
    // which is why a capture session does not block persistence.
    std::vector<uint8_t> saveBlob_;
    size_t lastBlobSize_ = 0;            // what the last write measured; sizes the buffer
    // How big that buffer has to be for the next save to fit without resizing: the last
    // blob plus the growth state can put on between saves, never below the reservation.
    // The margin is what keeps the answer stable: a buffer sized to the blob exactly
    // would put every save back on the allocating path the moment the pet gained a row.
    size_t saveBlobTargetBytes() const {
        const size_t want = lastBlobSize_ + kSaveBlobHeadroomBytes;
        return want < kSaveReserveBytes ? kSaveReserveBytes : want;
    }
    int savesDeferred_ = 0;              // consecutive refusals; 0 once one lands
    uint32_t savesDeferredLow_ = 0;      // worst headroom seen while refusing
    // Consecutive writes the STORE refused, which is a different failure from the heap
    // guard above: the save was built and handed over, and the medium turned it down.
    // Counted for the same reason: a retry that works is indistinguishable from one
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
    // job outlives the screen that started it: see netConnectWanted().
    bool updateSourceKnown_ = false;
    // Sized to match UpdateSource::kUrlCap and UpdateArtifact::url: the manifest's
    // address and the addresses inside it come off the same host, so a URL one can
    // hold and another can't would be a trap.
    char updateManifestUrl_[160] = {0};
    char webBundleVersion_[16] = "UNKNOWN";
    bool updateJob_ = false;
    UpdateTarget jobTarget_ = UpdateTarget::None;
    UpdateStatus updateStatus_;
    InstallStatus installStatus_;
    // UPDATES screen: which row the cursor is on, and (once an install row is
    // pressed), which target is sitting at its yes/no confirm. Nav state, not job
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

    // 1v1 duels (game_pvp.cpp): the open session (core/model/pvp_session.h), empty
    // between sessions. pvpStatus_ is the LINK screen's line saying why the last one
    // ended (empty = it didn't), so it outlives the session.
    std::optional<PvpSession> pvp_;
    char pvpStatus_[32] = "";

    // --- ROCK THE DOCK (game_tourney.cpp) -------------------------------------
    // The bracket in play, verdict included (core/model/tourney_run.h); empty between
    // runs. Saved at v53.
    std::optional<TourneyRun> tourney_;
    // The bracket screen's: its read cursor (A steps it, nothing acts on it) and which
    // of its three reads is up.
    int tourneyCursor_ = 0;
    TourneyView tourneyView_ = TourneyView::Bracket;

    // --- THE STORY (game_story.cpp) -----------------------------------------
    // What the reader is showing, and the chapter queued behind it. Two, because a
    // cleared area fires its two outros back to back (fireStoryPair) and nothing else
    // ever queues more than that: a deeper queue would be a story the walk is telling
    // instead of one it is punctuating.
    const StoryChapterDef* storyChapter_ = nullptr;
    const StoryChapterDef* storyNext_ = nullptr;
    // Which window of the chapter is on screen, as the prose row it opens on: the
    // reader steps by "however many rows were shown", exactly as STAT's pages do, so
    // this is a row index and not a panel number (core/ui/prose_page.h).
    int storyScroll_ = 0;
    // When this window turns itself over (against nowMs_). Re-armed by every press, so
    // a reader who is reading is never overtaken by the clock.
    uint32_t storyPanelDeadlineMs_ = 0;
    StoryThen storyThen_ = StoryThen::Walk;
    int storyThenArea_ = -1;                 // StoryThen::AreaBoss's gauntlet
    int storyArchiveRow_ = 0;                // the CHAPTERS list's cursor
    // The scroll position of whichever reader is open: one field, because only one
    // ever is, and a per-view scroll would have to be reset on every switch anyway.
    int tourneyScroll_ = 0;
    // The entrant the SCOUT sheet is showing. Held rather than re-derived per repaint:
    // building one walks the whole move and mod tables, which is not a per-frame cost.
    TourneyFighter tourneyScouted_;
    int tourneyScoutedSlot_ = -1;
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
    // growing) if the outbox is full: every frame here is repeated on a timer, so a
    // drop costs one retry interval, not the session.
    void queuePvpFrame(const uint8_t* mac, const uint8_t* buf, size_t len);
    // Re-send whatever frame is holding the current phase open, on kPvpRetryMs, and
    // fail the session out on timeout. Driven from tick().
    void pvpTick();
    // Encode + queue the frame this phase is waiting on an answer to.
    void sendPvpPending();
    // Drop back to Idle, telling the opponent unless `notify` is false (it already
    // knows: it is the one that ended the session). `why` is what the LINK screen
    // shows; empty means a clean, unremarkable end.
    void endPvpSession(const char* why, bool notify);
    // Both fighters are known and agreed: build the shared fight and enter Nav::Combat.
    void startPvpBattle(uint32_t seed);

    // --- ROCK THE DOCK (game_tourney.cpp) -----------------------------------
    // Open the bracket screen off the EXPL row: draws a fresh bracket when no run is
    // in play, else resumes the one that is.
    void openTourney();
    void onTourney(const ButtonEvent& ev);         // A read · B bout · C back · A+C brief
    // The three views (game_tourney_render.cpp: the arena's own screen unit, laid
    // out against ui/tourney_screen.h).
    void drawTourney(Framebuffer& fb) const;
    void drawTourneyBracket(Framebuffer& fb) const;
    void drawTourneyFoot(Framebuffer& fb) const;   // the match card under the field
    void drawTourneyScout(Framebuffer& fb) const;
    void drawTourneyBrief(Framebuffer& fb) const;
    void openTourneyScout();
    bool tourneyHoldB(uint32_t heldMs);            // the bracket's B: a hold scouts...
    void tourneyReleaseB();                        // ...and a tap fights the own bout
    void startTourneyMatch();                      // the operator's own match, live
    void finishTourneyMatch();                     // advance the bracket, or end the run
    void abandonTourney();                         // forget the run (seed back to 0)
    void awardTourneyPurse();                      // the title's one lump
    // The duel finished: record the verdict, log it, and park on the LINK screen.
    // Deliberately does NOT go through applyCombatResult: no stakes means no XP, no
    // Bits, no Fragmentation, no care signal.
    void finishPvpBattle();

    void onHackerLink(const ButtonEvent& ev);      // LINK list: A cycle · B act · C back
    void drawHackerLink(Framebuffer& fb) const;    // LINK screen + header (game_pvp.cpp)

    // Shop event. shopFront_ points at the CURRENT area's AreaStorefrontDef (its
    // .shop or .modShop, picked by startShopEvent/startModShopEvent): a stable
    // pointer into compiled-in content, valid for the whole visit. shopIsModShop_
    // says which registry resolves a listing's `id` (item vs mod). shopStockLeft_ is
    // the per-visit remaining stock for each listing (index-aligned with
    // shopFront_->listings; refilled from each row's own ShopListingDef::stock on
    // entry, NOT persisted). shopCursor_ is the A-cycled row selection (wraps
    // 0..listingCount-1). All transient run state, like the boss/gauntlet fields
    // above.
    const AreaStorefrontDef* shopFront_ = nullptr;
    bool shopIsModShop_ = false;
    int shopStockLeft_[kMaxShopListings] = {};
    int shopCursor_ = 0;

    // Hacker face (07). face_ is the top-level PET/HACKER mode (transient: a
    // reboot always boots PET; not persisted). It gates the carousel + submenu dispatch
    // and the render path. cursor_/nav_ are SHARED with the pet face (a flip drops to
    // Idle), so the hacker carousel reuses the same cursor cycling.
    Face face_ = Face::Pet;
    VaultScreen vault_;       // the VAULT slot's cursor (game/vault_screen.h)
    RigShopScreen rigShop_;   // the SHOP slot's views and cursors (game/rig_shop_screen.h)
    ItemsMenu items_;         // the ITEMS slot's views, cursors, filter and rows (game/items_menu.h)
    StatReader stat_;         // the STAT slot's pages, scroll and INDEX (game/stat_reader.h)
    int hackerMergeRow_ = 0;  // MERGE HUB list cursor: recipe row
    CrewScreen crew_;         // the CREW slot's views and cursors (game/crew_screen.h)
    // Warp-key picker. warpRow_ is the focused row into the live
    // heldWarpKeys() list while nav_ == Nav::WarpPicker; transient run state (the
    // keys themselves persist in the inventory, but the picker cursor does not).
    int warpRow_ = 0;

    // The ITEM OFFER's cursor, what it is offering for, and where C goes back to.
    // pet_.eggOfferMade is the once-per-egg latch: declining is an answer, so the egg is not
    // asked again (cleared by layEgg; session-volatile, so a reboot asks once more).
    OfferFor offerFor_ = OfferFor::Dive;
    int offerRow_ = 0;
    Nav offerBackNav_ = Nav::Idle;

    // A Hacker Rank crossing (Operator::hackerRank), held until the next Walk-screen
    // render (both the direct Wi-Fi-resolve return and the wild-combat-return path
    // check it).
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
    // Per-BSSID dedup for the SHAKES credit (OperatorCant::handshakesSeen), which
    // registerHandshake() bumps once per new BSSID. SD-backed (loadHandshakeLedger at
    // boot) so the full rotating history lives on the card, with a smaller save-blob
    // fallback (captureRadio) for a card-less reboot. See core/net/handshake_ledger.h.
    HandshakeLedger handshakeLedger_;

    SdStatus sdStatus_;  // last microSD reading from the platform tier (CFG "SD")
    // Idle SD-present reveal: game-ms the card last became present + a guard so the
    // 0-ms sentinel (a boot mount at nowMs_==0) still reveals. sdRecheckRequested_
    // is the one-shot CFG->device re-mount request.
    uint32_t sdIconRevealAtMs_ = 0;
    bool sdIconRevealArmed_ = false;
    bool sdRecheckRequested_ = false;
    // CFG->device FORMAT SD job (requestSdFormat / finishSdFormat). Runtime-only:
    // a reboot mid-format leaves the card for the boot mount to report on.
    SdFormatState sdFormatState_ = SdFormatState::Idle;

    // Latched CFG->device request to enter travel sleep. Runtime-only: what it asks
    // for is a reset, and a device that woke from one is not still asking.
    bool travelSleepRequested_ = false;

    // The arbiter's resolved radio owner (runtime-only; None on host).
    RadioOwner radioOwner_ = RadioOwner::None;


    // The feeding in progress (core/model/feeding.h), held while its modal is up.
    std::optional<Feeding> feeding_;

    // Lockout modal.
    bool lockoutPayOption_ = false;        // false = Open Items · true = Pay Bits
    bool lockoutItemsContext_ = false;     // ITEMS opened from the Lockout modal


    // DISK DECRYPTION (the Ransomware hatch minigame, game_decryption.cpp). Un-persisted for
    // the same reason the Clutch Pick and the Isolation Protocol below are: it is played
    // out in the minute after the egg is laid, so a reboot mid-board forfeits the bonus
    // and leaves a normal egg incubating. decryptionEasy_ is the arcade dial's hint
    // setting, read only by the screen.
    DiskDecryption decryption_;
    bool decryptionEasy_ = false;

    // THE DECRYPTOGRAM (game_cryptogram.cpp): the board in play and its stake
    // (core/model/cryptogram_run.h), empty otherwise. What persists (save v48) is
    // OperatorTallies::quoteStates: two bits per QuoteDef::wire holding the tier ladder,
    // which is at once the loss ratchet and the "already won" record.
    std::optional<CryptogramRun> cryptogramRun_;

    // GAMES / the arcade (game_arcade.cpp). arcade_ is the cabinet run in play
    // (core/model/arcade_run.h), empty between runs: a minigame reads it to know it is
    // being played for fun, which redirects the payout and scales the pacing. The GAMES
    // screens are arcadeScreen_'s. Only the tallies persist (save v47, on the Operator).
    std::optional<ArcadeRun> arcade_;
    ArcadeScreen arcadeScreen_;

    // The Clutch Pick on screen (core/model/egg_pick.h), empty otherwise.
    std::optional<EggPick> eggPick_;

    // Isolation Protocol (the Worm hatch minigame, game_isolation.cpp). Un-persisted for
    // the same reason the Clutch Pick is (core/model/egg_pick.h): it is played out in the
    // minute after the egg is laid, so a reboot mid-run forfeits the bonus and leaves a
    // normal egg incubating. The run's own step clock is real-ms, like the Stacker's.
    Isolation isolation_;
    uint32_t lastIsolationStepMs_ = 0;
    int isolationBanked_ = 0;        // bytes already spent on the clock, so B can't double-pay

    // CHROMATOPHORE (the Metamorphic hatch minigame, game_chroma.cpp). Un-persisted for
    // the reason the two above are: the whole run is over in half a minute after the
    // egg is laid, so a reboot mid-run forfeits the bonus and leaves a normal egg
    // incubating. Its clock is real-ms and DELTAS rather than a step cadence: the
    // board is a stopwatch, not a board that steps.
    Chromatophore chroma_;
    uint32_t lastChromaMs_ = 0;
    int chromaBanked_ = 0;           // passes already spent on the clock, so B can't double-pay

    // Hatch reveal cinematic. Counts heartbeats since the chord cracked the egg, which
    // maps straight onto the shell's hatch frames; completeHatch fires off the end.
    int hatchRevealBeat_ = 0;
    // Line-select modal: the cursor into the unlocked egg lines while
    // choosing which egg to lay. Runtime-only: an empty save re-prompts on reboot
    // (the modal precedes laying/persisting an egg, so there's nothing to persist).
    int lineSelectRow_ = 0;

    // Evolution modal (stub). evolveTo_ is the successor being revealed; the
    // swap onto pet_.creature is deferred to completeEvolution so the cinematic holds the
    // old sprite first. pet_.stageEnteredMs clocks time-in-stage for the trigger.
    const CreatureDef* evolveTo_ = nullptr;
    int evolveBeat_ = 0;

    // Critical System Failure. pet_.dyingArmed latches when the pet enters the
    // 5/5 dying state and pet_.dyingEnteredMs is the anchor it clocks from; csfBeat_
    // paces the crash-FX hold before B acknowledges into an ARCH record (OperatorArch).
    //
    // pet_.dyingElapsedMs is the total burned so far and the ONLY one of the three that
    // persists (save v42), which is what stops a power cycle from refunding the
    // window. The pair splits because the anchor is in monotonic game-ms, meaningless
    // across a boot, while the accumulator is a duration and survives one intact.
    int csfBeat_ = 0;

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
