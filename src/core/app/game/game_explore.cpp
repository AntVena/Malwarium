#include "core/app/game.h"
#include "core/app/game_internal.h"

#include <cstdio>
#include <cstring>

#include "tunables.h"
#include "core/content/areas/area_defs.h"
#include "core/content/content_tables.h"
#include "core/render/canvas.h"
#include "core/render/font5x7.h"
#include "core/render/palette.h"
#include "core/render/sprite.h"
#include "core/ui/carousel.h"
#include "core/ui/combat_screen.h"
#include "core/ui/items_screen.h"
#include "core/ui/maint_screen.h"
#include "core/ui/modals.h"
#include "core/ui/stat_screen.h"
#include "core/ui/train_screen.h"
#include "generated/assets.h"

namespace mal {

// EXPL / Explore-mode --------------------------------------------

// The durable flag blocks flattened for the shared expl_screen row helpers
// (row-major [area*kExplSubAreas+sub]). Small stack copies — the caller reads them
// once per press/frame, never holds the pointer.
namespace {
constexpr int kSubFlags = kExplSectors * kExplSubAreas;

// Per-area MOD loot tables ------------------------------------
// Each explore area (+ the endless DeepWeb) has its own mod pool (owned by that
// area's own AreaDef under src/core/content/areas/); a milestone drop (sub/area
// boss clear, DeepWeb win, Epic cache) rolls one id from it, weighted by each
// mod's RARITY (kModRarityWeight). The mod ids' powerTier (defs.h) mirrors the
// area tier (1/2/3/4) — a calibration test asserts nothing early out-powers a
// late mod.
struct AreaModTable { const char* const* ids; int count; };

// The table for `areaIdx` (0..kExplSectors-1 or kDeepWebSector). Out-of-range →
// empty (mal::area()'s own clamp-to-0 fallback would silently hand back area 0's
// pool instead, which is wrong here, so this bounds-checks first).
AreaModTable areaModTable(int areaIdx) {
    if (areaIdx == kDeepWebSector) return {kAreaModsDeepWeb, kAreaModsDeepWebCount};
    if (areaIdx < 0 || areaIdx >= kAreaCount) return {nullptr, 0};
    const AreaDef& a = area(areaIdx);
    return {a.modPoolIds, a.modPoolCount};
}

int modRarityWeight(const ContentRegistry& reg, const char* id) {
    const ModDef* m = reg.mod(id);
    if (!m) return 0;
    const int r = static_cast<int>(m->rarity);
    return (r >= 0 && r < 4) ? kModRarityWeight[r] : 0;
}
}  // namespace
void Game::flattenSubFlags(bool (&cleared)[kSubFlags],
                           bool (&bossUnlocked)[kSubFlags]) const {
    for (int a = 0; a < kExplSectors; ++a)
        for (int s = 0; s < kExplSubAreas; ++s) {
            cleared[a * kExplSubAreas + s] = subCleared_[a][s];
            bossUnlocked[a * kExplSubAreas + s] = subBossUnlocked_[a][s];
        }
}

bool Game::areaBossReady(int area) const {
    if (area < 0 || area >= kExplSectors) return false;
    if (!explSectorOpen(area, sectorCleared_) || sectorCleared_[area]) return false;
    for (int s = 0; s < kExplSubAreas; ++s)
        if (!subCleared_[area][s]) return false;
    return true;                                     // all 5 cleared, area not yet
}

// Two-level EXPL navigation: the rendered list is unchanged, but A/B/C
// traverse two levels so a deep sub-area is a few presses instead of cycling the whole
// ladder. explNavArea_ == -1 → TOP level: the cursor lands on the DeepWeb row + area
// HEADERS only. explNavArea_ == N → INSIDE area N: the cursor lands only on area N's own
// selectable rows (its sub-areas + its boss-ready header). `explRowLandable` centralises
// which rows a level stops on.
bool Game::explRowLandable(int row) const {
    bool cleared[kSubFlags], boss[kSubFlags];
    flattenSubFlags(cleared, boss);
    const ExplRowState st = explRowState(row, sectorCleared_, cleared, boss,
                                         exploreActive_ ? exploreSector_ : -1,
                                         exploreActive_ ? exploreSub_ : -1);
    if (explNavArea_ < 0) {                          // TOP level
        if (explRowIsDeepWeb(row)) return explRowSelectable(st);
        // Area headers only, and only for OPEN areas (an open area is enterable even when
        // its header isn't a boss trigger yet — AreaProgress/BossReady/Cleared all enter).
        return explRowSub(row) < 0 && st != ExplRowState::AreaLocked;
    }
    // INSIDE an area: only that area's own selectable rows (its subs + boss-ready header).
    if (explRowIsDeepWeb(row) || explRowArea(row) != explNavArea_) return false;
    return explRowSelectable(st);
}

int Game::areaHeaderRow(int area) const {
    return 1 + area * kExplRowsPerArea;              // row 0 = DeepWeb; area blocks follow
}

int Game::firstSelectableExplRow() const {
    // Called on entering EXPL — always start at the TOP level (explNavArea_ reset by the
    // submenu entry) and park on the first landable row (DeepWeb if unlocked, else area 0).
    const int n = explRowCount();
    for (int r = 0; r < n; ++r)
        if (explRowLandable(r)) return r;
    return 0;                                        // nothing landable → park at 0
}

void Game::onExplList(const ButtonEvent& ev) {
    bool cleared[kSubFlags], boss[kSubFlags];
    flattenSubFlags(cleared, boss);
    const int exSec = exploreActive_ ? exploreSector_ : -1;
    const int exSub = exploreActive_ ? exploreSub_ : -1;
    const int n = explRowCount();
    auto stateOf = [&](int r) {
        return explRowState(r, sectorCleared_, cleared, boss, exSec, exSub);
    };
    if (ev.button == Button::A) {
        // Advance to the next row LANDABLE at the current level, wrapping.
        for (int i = 1; i <= n; ++i) {
            const int r = (listRow_ + i) % n;
            if (explRowLandable(r)) { listRow_ = r; break; }
        }
    } else if (ev.button == Button::B) {
        if (listRow_ < 0 || listRow_ >= n || !explRowLandable(listRow_)) return;
        // TOP level, on an area header → DRILL IN: drop to that area's first landable row
        // (its boss-ready header, else its first open/farmable sub). DeepWeb acts directly.
        if (explNavArea_ < 0 && !explRowIsDeepWeb(listRow_)) {
            explNavArea_ = explRowArea(listRow_);
            for (int r = 0; r < n; ++r)
                if (explRowLandable(r)) { listRow_ = r; break; }
            return;
        }
        const ExplRowState st = stateOf(listRow_);
        const int area = explRowArea(listRow_);
        const int sub = explRowSub(listRow_);
        switch (st) {
            case ExplRowState::AreaBossReady: startAreaBoss(area); break;
            case ExplRowState::SubBossReady:  startSubAreaBoss(area, sub); break;
            case ExplRowState::DeepWebOpen:                     // endless zone
            case ExplRowState::DeepWebDiving: startDeepWebDive(); break;
            default:                          startExplore(area, sub); break;  // arm/re-arm
        }
    } else if (ev.button == Button::C) {
        // INSIDE an area → pop back to the TOP level, parking on that area's header.
        // At the TOP level → leave EXPL for the carousel.
        if (explNavArea_ >= 0) {
            const int a = explNavArea_;
            explNavArea_ = -1;
            listRow_ = areaHeaderRow(a);
        } else {
            nav_ = Nav::Cursor;
        }
    }
}

void Game::startExplore(int sector, int sub) {
    // Arm a sub-area: explore-mode starts and we return to the IDLE
    // habitat — there is no walk screen. The badge appears; the tick auto-steps from
    // Idle. The streak resets on a fresh arm (per-sub-area); durable sub-area
    // clear/boss-unlock persist.
    exploreActive_ = true;
    exploreSector_ = sector;
    exploreSub_ = sub;
    exploreStreak_ = 0;
    deepWebDepthMultiplier_ = 1;
    exploreSteps_ = 0;
    exploreStepBeat_ = 0;
    exploreFlavor_[0] = '\0';
    // A fresh walk starts with a clean slate for the empty-queue Happiness-penalty
    // throttle too — a drought from a much earlier walk should never suppress this
    // walk's first miss (see game.h's comment on emptyQueueStreak_).
    emptyQueueStreak_ = 0;
    nav_ = Nav::Idle;
    autoArmBackupShield(kRigRowAutoBackup);
    markSaveDirty();
}

void Game::startDeepWebDive() {
    // arm the endless terminal zone (unlocked once every area is cleared).
    // Same idle-background model as startExplore, but on the virtual kDeepWebSector —
    // doExploreStep()/startEncounter() special-case it to scale enemies to the pet and
    // never reach the (nonexistent) boss ladder. A loss ends the dive like any wild.
    if (!allSectorsCleared()) return;
    exploreActive_ = true;
    exploreSector_ = kDeepWebSector;
    exploreSub_ = 0;
    exploreStreak_ = 0;
    deepWebDepthMultiplier_ = 1;
    // A Backdoor/Rootkit/Kernel/Zero-Day Bell arms a start depth ahead of time
    // (Game::applyItemEffects); consume it here so a fresh dive begins there instead
    // of at 0. Zero-Day's sentinel resolves to this pet's own best depth NOW (not at
    // Use-time), so it never targets a stale, since-improved record.
    if (pendingDeepWebStartDepth_ != -1) {
        exploreStreak_ = pendingDeepWebStartDepth_ == kDeepWebStartDepthUseBest
                              ? bestDeepWebDepth_
                              : pendingDeepWebStartDepth_;
        pendingDeepWebStartDepth_ = -1;
    }
    exploreSteps_ = 0;
    exploreStepBeat_ = 0;
    exploreFlavor_[0] = '\0';
    emptyQueueStreak_ = 0;  // see startExplore's comment above
    nav_ = Nav::Idle;
    autoArmBackupShield(kRigRowAutoBackup);
    markSaveDirty();
}

void Game::exploreBadgeLabel(char* out, size_t n) const {
    if (!out || n == 0) return;
    if (inDeepWebDive()) { std::snprintf(out, n, "DEEPWEB"); return; }
    // Area's first word (up to the first space) + the 1-based sub number, e.g.
    // "CITRUS 3" — short enough to never collide with the right-anchored WINS field.
    const char* name = explSectorName(exploreSector_);
    char word[12];
    size_t w = 0;
    for (; name[w] && name[w] != ' ' && w < sizeof(word) - 1; ++w) word[w] = name[w];
    word[w] = '\0';
    std::snprintf(out, n, "%s %d", word, exploreSub_ + 1);
}

void Game::doExploreStep() {
    // One explore step: re-arm the pacer, then resolve a GUARANTEED event
    // from the sector's weighted table. Wild encounters DOMINATE (the streak
    // needs wins); loot / Wi-Fi / shop / cache / key are the breather + reward beats.
    // The boss is NOT on this table — it's a manual trigger. No "quiet" rolls.
    exploreStepBeat_ = 0;
    ++exploreSteps_;
    rng_ = rng_ * 1664525u + 1013904223u;
    const int roll = static_cast<int>((rng_ >> 16) % 100);
    int t = kExploreLootPct;
    if (roll < t)                    { resolveLootEvent();    return; }
    t += kExploreWifiPct;  if (roll < t) { startWifiEvent();     return; }
    t += kExploreShopPct;  if (roll < t) { startShopEvent();     return; }
    t += kExploreModShopPct; if (roll < t) { startModShopEvent(); return; }
    t += kExploreCachePct; if (roll < t) { resolveCacheEvent();  return; }
    t += kExploreKeyPct;   if (roll < t) { resolveKeyItemEvent(); return; }
    startEncounter();      // remainder = wild encounter (the majority slot)
}

void Game::returnToExplore() {
    // A resolved event / fight hands back to the IDLE habitat; explore-mode keeps
    // running. Re-arm the step pacer so the next auto-step waits a full beat,
    // and surface any pending Hacker Rank-up on the flavor line.
    nav_ = Nav::Idle;
    exploreStepBeat_ = 0;
    applyPendingRankUp();
    autoArmBackupShield(kRigRowContinuousBackup);   // Rig Shop h — mid-run re-arm
    maybeAutoDefrag();                              // Rig Shop i — hands-off upkeep
}

void Game::autoArmBackupShield(int gateRow) {
    if (rigLevel_[gateRow] <= 0) return;      // upgrade not owned
    if (backupShieldArmed()) return;          // already live — never doubles up
    // applyItemEffects, not useItem: the upgrade grants the effect outright and never
    // reaches into the Vault, so owning it doesn't quietly eat the player's drives.
    if (const ItemDef* d = registry_.item(kBackupDriveId)) applyItemEffects(*d);
}

void Game::dismissPostEncounter() {
    // Any button press, or the kPostEncounterMs timer (tick()), calls this — the
    // readout is informational only, nothing to confirm/cancel. Hands straight
    // back to the explore habitat, same as any other resolved event.
    returnToExplore();
}

void Game::onExploreControl(const ButtonEvent& ev) {
    // The A+C control overlay: A = Network Ping (fire the next guaranteed
    // event NOW, skipping the ~3s timer); B = Warp (open the held-key picker, else
    // inert); C = Stop explore (cancel the mode; the streak resets). Standard
    // A/B/C returns, exactly like combat's override picker.
    if (ev.button == Button::A) {
        nav_ = Nav::Idle;
        doExploreStep();                // may re-enter a full-screen event
    } else if (ev.button == Button::B) {
        if (!heldWarpKeys().empty()) { warpRow_ = 0; nav_ = Nav::WarpPicker; }
        // else inert — no key to spend; stay in the overlay.
    } else if (ev.button == Button::C) {
        exploreActive_ = false;
        exploreStreak_ = 0;
        deepWebDepthMultiplier_ = 1;
        exploreFlavor_[0] = '\0';
        nav_ = Nav::Idle;
        markSaveDirty();
    }
}

std::vector<const ItemDef*> Game::heldWarpKeys() const {
    // The warp keys currently in the inventory, in inventory order so the
    // picker rows are stable/testable. A key = an ItemDef whose walkWarp != None.
    std::vector<const ItemDef*> out;
    for (const auto& s : inventory_.stacks()) {
        if (s.qty <= 0) continue;
        const ItemDef* d = registry_.item(s.id);
        if (d && d->walkWarp != ItemDef::WalkWarp::None) out.push_back(d);
    }
    return out;
}

void Game::onWarpPicker(const ButtonEvent& ev) {
    // A flat list of held warp keys: A cycles the row, B spends the focused key
    // (warp), C backs out to the idle habitat (explore-mode continues). The list is
    // rebuilt each press so a key that ran out (spent) can't leave a stale cursor.
    auto keys = heldWarpKeys();
    if (keys.empty()) { returnToExplore(); return; }
    if (warpRow_ >= static_cast<int>(keys.size())) warpRow_ = 0;
    if (ev.button == Button::A) {
        warpRow_ = (warpRow_ + 1) % static_cast<int>(keys.size());
    } else if (ev.button == Button::B) {
        useWarpKey(*keys[warpRow_]);
    } else if (ev.button == Button::C) {
        returnToExplore();
    }
}

void Game::useWarpKey(const ItemDef& d) {
    // Spend the key and jump straight to its target event: Access Token → the
    // nearest storefront, Safe-Mode Key → a safe rest. startShopEvent() sets
    // Nav::Shop; resolveSafeRestEvent() resolves in place and returns to the habitat.
    inventory_.remove(d.id, 1);
    char buf[28];
    std::snprintf(buf, sizeof(buf), "USED %s", d.displayName);
    log_.push(LogEventType::ItemUsed, buf);
    markSaveDirty();
    switch (d.walkWarp) {
        case ItemDef::WalkWarp::Shop:     startShopEvent(); break;
        case ItemDef::WalkWarp::SafeRest: resolveSafeRestEvent(); break;
        case ItemDef::WalkWarp::None:     returnToExplore(); break;  // unreachable
    }
}

void Game::resolveSafeRestEvent() {
    // A guaranteed-safe rest: no combat, no roll — the pet rests and the
    // Safe-Mode boot lowers Fragmentation (rest de-frags). Resolves in place and
    // hands back to the idle habitat with a flavor line.
    model_.setFragmentation(model_.fragmentation() - kSafeRestDefrag);
    std::snprintf(exploreFlavor_, sizeof(exploreFlavor_),
                 "SAFE MODE - RESTED (-%d FRAG)", kSafeRestDefrag);
    markSaveDirty();
    returnToExplore();
}

void Game::resolveCacheEvent() {
    // A Sealed Cache find is NON-INTERRUPTING: it drops a locked
    // container into the inventory and explore-mode keeps stepping — no sub-screen.
    // Roll a weighted container so different caches carry different pools (drawn on
    // Open, openSealedCache). The candidates are every item whose row declares a
    // findWeight — so a cache that is only ever GRANTED (an achievement's Commendation
    // Cache, weight 0) can never turn up here, and adding a findable one is a row.
    // The player opens whatever drops later, from the VAULT.
    int total = 0;
    for (const ItemDef* it : registry_.allItems())
        if (it->use == ItemDef::Use::OpenContainer) total += it->cache.findWeight;
    if (total <= 0) return;                       // nothing findable — no cache event
    rng_ = rng_ * 1664525u + 1013904223u;
    int roll = static_cast<int>((rng_ >> 16) % static_cast<unsigned>(total));
    const ItemDef* d = nullptr;
    for (const ItemDef* it : registry_.allItems()) {
        if (it->use != ItemDef::Use::OpenContainer || it->cache.findWeight <= 0) continue;
        if (roll < it->cache.findWeight) { d = it; break; }
        roll -= it->cache.findWeight;
    }
    if (!d) return;
    inventory_.add(d->id, 1);
    char buf[32];
    std::snprintf(buf, sizeof(buf), "FOUND %s", d->displayName);
    log_.push(LogEventType::ItemGained, buf);
    std::snprintf(exploreFlavor_, sizeof(exploreFlavor_), "%s - OPEN IN ITEMS",
                 d->displayName);
    markSaveDirty();
}

void Game::resolveKeyItemEvent() {
    // A Key-item find is NON-INTERRUPTING like the Sealed Cache: it drops a
    // random warp key into the inventory and explore-mode keeps stepping. The player
    // spends it later via the A+C control chord's Warp picker to jump to a
    // shop / safe rest.
    static const char* kKeyPool[] = {"access_token", "safe_mode_key"};
    rng_ = rng_ * 1664525u + 1013904223u;
    const char* id = kKeyPool[(rng_ >> 16) % 2];
    inventory_.add(id, 1);
    const ItemDef* d = registry_.item(id);
    char buf[32];
    std::snprintf(buf, sizeof(buf), "FOUND %s", d ? d->displayName : id);
    log_.push(LogEventType::ItemGained, buf);
    std::snprintf(exploreFlavor_, sizeof(exploreFlavor_), "%s - WARP KEY (A+C)",
                 d ? d->displayName : id);
    markSaveDirty();
}

void Game::resolveLootEvent() {
    // A loot-cache event resolves IN PLACE — no sub-activity — then
    // explore-mode keeps stepping.
    grantLootReward(exploreFlavor_, sizeof(exploreFlavor_));
}

void Game::grantLootReward(char* outFlavor, size_t outFlavorSize) {
    // The shared reward-pool draw — flat Bits + a chance of one item.
    // Full drop tables are a later content pass; this seeds the mechanic.
    // Reused by the loot-cache event and the Wi-Fi sleeping-guardian/open-cache
    // sub-outcomes ("no new ones," same pool, same log path).
    bits_ += applyCacheBitsBonus(kLootBitsReward);   // Enhanced DataMining
    rng_ = rng_ * 1664525u + 1013904223u;
    const bool dropItem =
        static_cast<int>((rng_ >> 16) % 100) < kLootItemChancePct;
    if (dropItem) {
        // kLootPool is content (content_items.cpp, declared in content_tables.h) — it
        // travels with its own count, since sizeof only works in the defining unit.
        // Weighted through the same draw a container uses (Game::drawCacheItem), so a
        // staple that is common in the caches is common on the walk too.
        drawCacheItem(kLootPool, kLootPoolCount);
        std::snprintf(outFlavor, outFlavorSize, "+%d BITS + ITEM",
                     kLootBitsReward);
    } else {
        log_.push(LogEventType::ItemGained, "+BITS FROM CACHE");
        std::snprintf(outFlavor, outFlavorSize, "+%d BITS",
                     kLootBitsReward);
    }
    markSaveDirty();
}

// MODS earn path ----------------------------------------------

const char* Game::rollAreaModId(int area) {
    // Rarity-weighted draw from the area's mod loot table (rarer = scarcer). Advances
    // the shared LCG so the pick stays deterministic under a fixed seed.
    const AreaModTable t = areaModTable(area);
    if (!t.ids || t.count <= 0) return nullptr;
    int total = 0;
    for (int i = 0; i < t.count; ++i) total += modRarityWeight(registry_, t.ids[i]);
    if (total <= 0) return nullptr;
    rng_ = rng_ * 1664525u + 1013904223u;
    int roll = static_cast<int>((rng_ >> 16) % static_cast<unsigned>(total));
    for (int i = 0; i < t.count; ++i) {
        const int w = modRarityWeight(registry_, t.ids[i]);
        if (roll < w) return t.ids[i];
        roll -= w;
    }
    return t.ids[t.count - 1];
}

const char* Game::rollAnyModId() {
    // Global rarity-weighted draw across EVERY mod — the Epic sealed-cache jackpot (it
    // can surface even a tier-4 mod, gated only by the rolled equip level).
    const std::vector<const ModDef*> all = registry_.allMods();
    int total = 0;
    for (const ModDef* m : all) {
        const int r = static_cast<int>(m->rarity);
        if (r >= 0 && r < 4) total += kModRarityWeight[r];
    }
    if (total <= 0) return nullptr;
    rng_ = rng_ * 1664525u + 1013904223u;
    int roll = static_cast<int>((rng_ >> 16) % static_cast<unsigned>(total));
    for (const ModDef* m : all) {
        const int r = static_cast<int>(m->rarity);
        const int w = (r >= 0 && r < 4) ? kModRarityWeight[r] : 0;
        if (roll < w) return m->id;
        roll -= w;
    }
    return all.empty() ? nullptr : all.back()->id;
}

void Game::grantRolledMod(const char* id) {
    if (!id) return;
    const ModDef* m = registry_.mod(id);
    if (!m) return;
    // Roll the per-instance equip-LEVEL gate within the mod's power-tier band:
    // floor(tier) ± kModEquipLevelVariance, clamped >= 0. A lucky low roll lets a lower-
    // level pet field a stronger mod; the spare is held permanently until then.
    int tier = m->powerTier;
    if (tier < 1) tier = 1;
    if (tier > kModPowerTiers) tier = kModPowerTiers;
    const int floor = modEquipLevelFloor(tier);
    rng_ = rng_ * 1664525u + 1013904223u;
    const int span = kModEquipLevelVariance * 2 + 1;                 // [-var, +var]
    const int delta = static_cast<int>((rng_ >> 16) % static_cast<unsigned>(span))
                      - kModEquipLevelVariance;
    int req = floor + delta;
    if (req < 0) req = 0;
    loadout_.grant(id, req);
    char buf[kSaveTextCap];
    std::snprintf(buf, sizeof(buf), "MOD %s L%d", m->displayName, req);
    log_.push(LogEventType::ItemGained, buf);
    markSaveDirty();
}

// Boss ladder ----------------------------------------------------

void Game::startSubAreaBoss(int area, int sub) {
    // A sub-area boss: a single strong malbeast, manually triggered from EXPL
    // once a 10-win streak unlocked it. A full clear marks the SUB-AREA cleared.
    if (!pet_) return;
    bossSector_ = area;
    bossSub_ = sub;                                 // >=0 → record a sub-area clear
    bossGauntlet_ = subAreaBoss(area, sub);
    bossRound_ = 0;
    bossBitsAccrued_ = 0;
    startBossRound(/*carryHealth=*/-1);             // round 0 starts at full Health
}

void Game::startAreaBoss(int area) {
    // The area boss: the 5 sub-area bosses fought back-to-back with carried
    // Health (no heal). Reachable once all 5 sub-areas are cleared; a full clear
    // clears the AREA (→ next area) + grants its Title.
    if (!pet_) return;
    bossSector_ = area;
    bossSub_ = -1;                                  // -1 → record an AREA clear
    bossGauntlet_ = areaBoss(area);
    bossRound_ = 0;
    bossBitsAccrued_ = 0;
    startBossRound(/*carryHealth=*/-1);
}

void Game::startBossRound(int carryHealth) {
    if (bossRound_ >= static_cast<int>(bossGauntlet_.rounds.size())) return;
    Combatant p = buildPlayerCombatant();
    Combatant e = makeEnemyCombatant(registry_, bossGauntlet_.rounds[bossRound_]);
    rng_ = rng_ * 1664525u + 1013904223u;
    // Live stakes; carryHealth < 0 on round 0 (full), else the wounded Health the
    // pet limped out of the previous round with (no heal between).
    combat_.begin(p, e, Combat::Stakes::Live, rng_, /*forceEnemyFirst=*/false,
                 carryHealth, exploitUsesPerBattle());
    combatCaller_ = CombatCaller::Boss;
    combatBeat_ = 0;
    combatTurnBeat_ = 0;
    nav_ = Nav::Combat;
    dirty_ = true;
}

void Game::finishBossRound() {
    // Settled every round, not once at the end: a gauntlet rebuilds the player Combatant
    // (buildPlayerCombatant) between rounds, so a drive spent in one round must neither
    // re-arm for the next nor lose the achievement it earned.
    settleBackupDrive();
    if (combat_.outcome() == Combat::Outcome::Win) {
        // Bank this round's boss roll (a gauntlet pays the lump at the end).
        bossBitsAccrued_ += bossBitsReward(bossGauntlet_.stageRank, rng_);
        const int surviving = combat_.player().health;
        ++bossRound_;
        ++bossWins_;   // per ROUND, so a three-boss gauntlet counts as three bosses beaten
        if (bossRound_ < static_cast<int>(bossGauntlet_.rounds.size())) {
            startBossRound(surviving);              // next round, no heal — stays in Combat
            return;
        }
        // Boss beaten: pay the lump, record the clear. A SUB-AREA boss
        // (bossSub_ >= 0) marks just that sub-area cleared; the AREA boss (bossSub_ ==
        // 1) clears the whole area → the next area unlocks + the Title is granted.
        bits_ += applyCombatBitsBonus(bossBitsAccrued_);   // Scraping Cluster Expansion
        addCombatXp(applyCombatXpBonus(kWildWinXpReward));  // Well-Fed XP Boost
        if (bossSub_ >= 0 && bossSub_ < kExplSubAreas) {
            const bool firstClear = !subCleared_[bossSector_][bossSub_];
            subCleared_[bossSector_][bossSub_] = true;
            log_.push(LogEventType::CombatWon, "SUB-AREA CLEARED");
            // a sub-area boss's FIRST clear has a chance to drop one of the
            // area's mods (rarity-weighted). Re-clears never re-roll — permanent items
            // aren't farmable. The rolled equip-level gate decides when it's fieldable.
            if (firstClear) {
                rng_ = rng_ * 1664525u + 1013904223u;
                if (static_cast<int>((rng_ >> 16) % 100) < kModSubBossDropPct)
                    if (const char* id = rollAreaModId(bossSector_))
                        grantRolledMod(id);
            }
        } else {
            const bool firstClear = !sectorCleared_[bossSector_];
            sectorCleared_[bossSector_] = true;
            unlockTitle(bossSector_);       // clearing the area grants its Title
            log_.push(LogEventType::CombatWon, "AREA CLEARED");
            // the AREA boss's first clear GUARANTEES a mod from the area table —
            // the marquee reward for beating the 5-stage gauntlet.
            if (firstClear)
                if (const char* id = rollAreaModId(bossSector_))
                    grantRolledMod(id);
        }
    } else {
        // Any non-win loses the whole gauntlet. A loss takes the standard
        // live-stakes Frag hit once (×Bad-branch multiplier); a flee just
        // bails (no penalty, same as a wild flee). No sector clear either way. A boss
        // loss ALSO cancels explore-mode + resets the streak — but the
        // boss-unlock PERSISTS, so the player re-triggers it rather than re-grinding.
        if (combat_.outcome() == Combat::Outcome::Lose) {
            const int frag = kWildLossFrag * combat_.player().fragMultPct / 100;
            model_.setFragmentation(model_.fragmentation() + frag);
            log_.push(LogEventType::CombatLost, "GAUNTLET FAILED");
        }
        exploreActive_ = false;
        exploreStreak_ = 0;
    }
    returnToExplore();                               // back to the idle habitat
    dirty_ = true;
    markSaveDirty();
    persistSave();
}

// Shop event ------------------------------------------------

namespace {
// Bounds-clamped: an out-of-range row (or no storefront entered yet) reads as
// empty/zero rather than reading past the array — callers loop 0..listingCount()
// which already stays in range, but the accessors stay safe on their own.
bool shopRowInRange(const AreaStorefrontDef* front, int i) {
    return front && i >= 0 && i < front->listingCount;
}
bool shopCostInRange(const AreaStorefrontDef* front, int i, int k) {
    return shopRowInRange(front, i) && k >= 0 && k < kMaxShopCostItems &&
           front->listings[i].costs[k].itemId != nullptr;
}
}  // namespace

void Game::startShopEvent() {
    // The current sector's ITEM storefront (BYTE TO EAT / PIER-TO-PEER /
    // MOOR-TO-MOOR). Refill this visit's per-listing stock from the area's OWN rows.
    shopFront_ = &area(exploreSector_).shop;
    shopIsModShop_ = false;
    for (int i = 0; i < shopFront_->listingCount && i < kMaxShopListings; ++i)
        shopStockLeft_[i] = shopFront_->listings[i].stock;
    shopCursor_ = 0;
    exploreEventBeat_ = 0;   // start the hands-off decision hold
    nav_ = Nav::Shop;
}

void Game::startModShopEvent() {
    // The current sector's MOD storefront (CHIP SHOP / PLUNDER PORT / MOOR TO MODS)
    // — same shape as startShopEvent, just resolving through the area's modShop and
    // the mod registry (shopIsModShop_ picks that at render/buy time).
    shopFront_ = &area(exploreSector_).modShop;
    shopIsModShop_ = true;
    for (int i = 0; i < shopFront_->listingCount && i < kMaxShopListings; ++i)
        shopStockLeft_[i] = shopFront_->listings[i].stock;
    shopCursor_ = 0;
    exploreEventBeat_ = 0;
    nav_ = Nav::ModShop;
}

const char* Game::shopListingId(int i) const {
    return shopRowInRange(shopFront_, i) ? shopFront_->listings[i].id : "";
}

const char* Game::shopListingName(int i) const {
    if (!shopRowInRange(shopFront_, i)) return "";
    const char* id = shopFront_->listings[i].id;
    if (shopIsModShop_) {
        const ModDef* m = registry_.mod(id);
        return m ? m->displayName : id;
    }
    const ItemDef* it = registry_.item(id);
    return it ? it->displayName : id;
}

EffectText Game::shopListingDescription(int i) const {
    if (!shopRowInRange(shopFront_, i)) return {};
    const char* id = shopFront_->listings[i].id;
    if (shopIsModShop_) {
        const ModDef* m = registry_.mod(id);
        return m ? effectText(*m) : EffectText{};
    }
    const ItemDef* it = registry_.item(id);
    return it ? effectText(*it) : EffectText{};
}

int Game::shopListingStock(int i) const {
    return shopRowInRange(shopFront_, i) ? shopStockLeft_[i] : 0;
}
int Game::shopListingBitsPrice(int i) const {
    return shopRowInRange(shopFront_, i) ? shopFront_->listings[i].bitsPrice : 0;
}
int Game::shopListingCostCount(int i) const {
    if (!shopRowInRange(shopFront_, i)) return 0;
    int n = 0;
    for (const auto& c : shopFront_->listings[i].costs) if (c.itemId) ++n;
    return n;
}
const char* Game::shopListingCostId(int i, int k) const {
    return shopCostInRange(shopFront_, i, k) ? shopFront_->listings[i].costs[k].itemId : "";
}
const char* Game::shopListingCostName(int i, int k) const {
    if (!shopCostInRange(shopFront_, i, k)) return "";
    const ItemDef* it = registry_.item(shopFront_->listings[i].costs[k].itemId);
    return it ? it->displayName : shopFront_->listings[i].costs[k].itemId;
}
int Game::shopListingCostQty(int i, int k) const {
    return shopCostInRange(shopFront_, i, k) ? shopFront_->listings[i].costs[k].qty : 0;
}
int Game::shopListingCostHave(int i, int k) const {
    return shopCostInRange(shopFront_, i, k)
               ? inventory_.count(shopFront_->listings[i].costs[k].itemId)
               : 0;
}

const char* Game::shopStatusLine() const {
    // Spelled out so the buy-availability reason survives grayscale (dual-coding).
    if (!shopRowInRange(shopFront_, shopCursor_)) return "";
    const ShopListingDef& def = shopFront_->listings[shopCursor_];
    if (shopStockLeft_[shopCursor_] <= 0) return "SOLD OUT";
    if (bits_ < def.bitsPrice) return "NOT ENOUGH BITS";
    for (const auto& c : def.costs)
        if (c.itemId && inventory_.count(c.itemId) < c.qty) return "NOT ENOUGH ITEMS";
    return "B TO BUY";
}

void Game::buyShopItem() {
    if (!shopRowInRange(shopFront_, shopCursor_)) return;
    const int i = shopCursor_;
    if (shopStockLeft_[i] <= 0) return;
    const ShopListingDef& def = shopFront_->listings[i];
    if (bits_ < def.bitsPrice) return;
    for (const auto& c : def.costs)
        if (c.itemId && inventory_.count(c.itemId) < c.qty) return;  // inert — a cost is short

    bits_ -= def.bitsPrice;
    for (const auto& c : def.costs) if (c.itemId) inventory_.remove(c.itemId, c.qty);
    --shopStockLeft_[i];

    char buf[28];
    if (shopIsModShop_) {
        // A shop buy is a guaranteed acquisition, not a lucky drop — grant at
        // reqLevel 0 (freely equippable) rather than rolling grantRolledMod's
        // per-instance gate.
        const ModDef* m = registry_.mod(def.id);
        loadout_.grant(def.id, /*reqLevel=*/0);
        std::snprintf(buf, sizeof(buf), "BOUGHT %s", m ? m->displayName : def.id);
    } else {
        inventory_.add(def.id, 1);
        const ItemDef* it = registry_.item(def.id);
        std::snprintf(buf, sizeof(buf), "BOUGHT %s", it ? it->displayName : def.id);
    }
    log_.push(LogEventType::ItemGained, buf);
    markSaveDirty();
}

void Game::onShop(const ButtonEvent& ev) {
    // B buys the SELECTED row (while stock + wallet + item costs allow); C leaves
    // back to the idle habitat. A cycles the selection through every listing
    // (wraps) — a no-op, not an exit, on a single-listing storefront so a stray
    // "Next" press can't drop the player out mid-shop. Any press restarts the
    // hands-off hold so an engaged shopper isn't auto-left.
    exploreEventBeat_ = 0;
    if (!shopFront_) return;
    if (ev.button == Button::A) {
        if (shopFront_->listingCount > 0)
            shopCursor_ = (shopCursor_ + 1) % shopFront_->listingCount;
    } else if (ev.button == Button::B) {
        buyShopItem();
    } else if (ev.button == Button::C) {
        std::snprintf(exploreFlavor_, sizeof(exploreFlavor_), "LEFT THE SHOP");
        returnToExplore();
    }
}





}  // namespace mal
