#include "core/app/game.h"

#include <cstdio>

#include "tunables.h"
#include "core/content/areas/area_defs.h"
#include "core/content/areas/darkweb_crawl/area.h"
#include "core/content/areas/deepweb_dive/area.h"
#include "core/content/content_tables.h"
#include "core/content/items/pools.h"
#include "core/ui/expl_screen.h"

namespace mal {

// EXPL / Explore-mode --------------------------------------------

// The durable flag blocks flattened for the shared expl_screen row helpers
// (row-major [area*kExplSubAreas+sub]). Small stack copies: the caller reads them
// once per press or frame and never keeps the pointer.
namespace {
constexpr int kSubFlags = kExplSectors * kExplSubAreas;

// Per-area MOD loot tables ------------------------------------
// Each explore area (+ the endless DeepWeb) has its own mod pool (owned by that
// area's own AreaDef under src/core/content/areas/); a milestone drop (sub/area
// boss clear, DeepWeb win, Epic cache) rolls one id from it, weighted by each
// mod's RARITY (kModRarityWeight). The mod ids' powerTier (defs.h) mirrors the
// area tier (1/2/3/4): a calibration test asserts nothing early out-powers a
// late mod.
struct AreaModTable { const char* const* ids; int count; };

// The table for `areaIdx` (0..kExplSectors-1 or kDeepWebSector). Out-of-range →
// empty (mal::area()'s own clamp-to-0 fallback would silently hand back area 0's
// pool instead, which is wrong here, so this bounds-checks first).
AreaModTable areaModTable(int areaIdx) {
    if (areaIdx == kDeepWebSector) return {kAreaModsDeepWeb, kAreaModsDeepWebCount};
    if (areaIdx == kDarkWebSector) return {kAreaModsDarkWeb, kAreaModsDarkWebCount};
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
            cleared[a * kExplSubAreas + s] = op_.world.subCleared[a][s];
            bossUnlocked[a * kExplSubAreas + s] = op_.world.subBossUnlocked[a][s];
        }
}

bool Game::areaBossReady(int area) const {
    if (area < 0 || area >= kExplSectors) return false;
    if (!explSectorOpen(area, op_.world.sectorCleared) || op_.world.sectorCleared[area])
        return false;
    for (int s = 0; s < kExplSubAreas; ++s)
        if (!op_.world.subCleared[area][s]) return false;
    return true;                                     // all 5 cleared, area not yet
}

// THREE-level EXPL navigation. explCat_ == None is the ACTIVITY PICKER (ui_state.h's
// ExplCat) and `listRow_` indexes its four rows; inside a category `listRow_` indexes
// the ladder's row space and explNavArea_ picks the level: -1 for the category's own
// rows, N for inside area N. `explRowLandable` centralises which LADDER rows a level
// stops on, and explCatLandable does the same job for the picker.
bool Game::explRowLandable(int row) const {
    if (!explRowInLevel(row, explCat_, explNavArea_)) return false;
    bool cleared[kSubFlags], boss[kSubFlags];
    flattenSubFlags(cleared, boss);
    const ExplRowState st = explRowState(row, op_.world.sectorCleared, cleared, boss,
                                         exploreActive_ ? exploreSector_ : -1,
                                         exploreActive_ ? exploreSub_ : -1,
                                         tourneyRunning());
    // An AREA HEADER at STORY's own level is the one row that is landable while not
    // selectable: you enter an open area whether or not its gauntlet is standing yet,
    // so AreaProgress/BossReady/Cleared all drill in and only AreaLocked does not.
    if (explCat_ == ExplCat::Story && explNavArea_ < 0)
        return st != ExplRowState::AreaLocked;
    return explRowSelectable(st);
}

// The picker's own filter: the cursor stops on a category there is something behind.
bool Game::explCatLandable(int catRow) const {
    if (catRow < 0 || catRow >= kExplCatRows) return false;
    return explCatOpen(explCatAt(catRow), explView());
}

// The view struct the picker's lock/detail rules are answered from. Built here for
// both the nav and the renderer, so the categories the cursor skips are the same set
// drawn as "??????".
ExplListView Game::explView() const {
    ExplListView v;
    v.areaCleared = op_.world.sectorCleared;
    v.exploringSector = exploreActive_ ? exploreSector_ : -1;
    v.exploringSub = exploreActive_ ? exploreSub_ : -1;
    v.bestDeepWebDepth = pet_.bestDeepWebDepth;
    v.bestDarkWebDepth = pet_.bestDarkWebDepth;
    v.tourneyRunning = tourneyRunning();
    v.tourneyAlive = tourneyAliveCount(tourneyAlive_);
    v.tourneyRound = tourneyRound_;
    v.storyChapters = storyChapterCount();
    return v;
}

int Game::areaHeaderRow(int area) const {
    // Past the LEAD rows (the two endless zones), then one block per area. Reads the
    // constant rather than a literal, so adding a zone above the ladder moves every area
    // block with it instead of silently pointing this one row short.
    return kExplLeadRows + area * kExplRowsPerArea;
}

void Game::openExplList() {
    // Entering EXPL. An explore-mode already running is almost always why the list is
    // being opened mid-walk (to check the streak, or to move on), so a running walk
    // RESUMES where the pet is, at the level it is standing on: a ladder area drills
    // straight to the armed sub-area, an endless zone parks on its own row. So the
    // ACTIVITY PICKER adds no press to returning to the walk you are on.
    if (exploreActive_) {
        if (exploreSector_ >= 0 && exploreSector_ < kExplSectors) {
            explCat_ = ExplCat::Story;
            explNavArea_ = exploreSector_;
            listRow_ = areaHeaderRow(exploreSector_) + 1 + exploreSub_;
            if (explRowLandable(listRow_)) return;
        } else if (inEndlessZone()) {
            explCat_ = ExplCat::Endless;
            explNavArea_ = -1;
            listRow_ = exploreSector_ == kDeepWebSector ? 0 : 1;
            if (explRowLandable(listRow_)) return;
        }
    }
    // Otherwise the picker opens on STORY, which is never locked.
    explCat_ = ExplCat::None;
    explNavArea_ = -1;
    for (int r = 0; r < kExplCatRows; ++r)
        if (explCatLandable(r)) { listRow_ = r; return; }
    listRow_ = 0;                                    // nothing landable → park at 0
}

void Game::onExplCategories(const ButtonEvent& ev) {
    // EXPL's own top level. A cycles the open categories, B opens the focused one, C
    // leaves EXPL for the carousel. The two categories with rows behind them
    // (explCatHasRows) drop into their level; the other two ARE their action, so B on
    // them goes straight to the bracket or the archive rather than through a list of
    // one.
    if (ev.button == Button::A) {
        for (int i = 1; i <= kExplCatRows; ++i) {
            const int r = (listRow_ + i) % kExplCatRows;
            if (explCatLandable(r)) { listRow_ = r; break; }
        }
    } else if (ev.button == Button::B) {
        if (!explCatLandable(listRow_)) return;
        const ExplCat c = explCatAt(listRow_);
        if (!explCatHasRows(c)) {
            if (c == ExplCat::Arena) openTourney();
            else if (c == ExplCat::Chapters) openStoryArchive();
            return;
        }
        explCat_ = c;
        explNavArea_ = -1;
        for (int r = 0, n = explRowCount(); r < n; ++r)
            if (explRowLandable(r)) { listRow_ = r; return; }
        listRow_ = 0;
    } else if (ev.button == Button::C) {
        nav_ = Nav::Cursor;
    }
}

void Game::onExplList(const ButtonEvent& ev) {
    // No category open -> the press belongs to the ACTIVITY PICKER, which walks its own
    // four rows and not the ladder's row space.
    if (explCat_ == ExplCat::None) { onExplCategories(ev); return; }
    bool cleared[kSubFlags], boss[kSubFlags];
    flattenSubFlags(cleared, boss);
    const int exSec = exploreActive_ ? exploreSector_ : -1;
    const int exSub = exploreActive_ ? exploreSub_ : -1;
    const int n = explRowCount();
    auto stateOf = [&](int r) {
        return explRowState(r, op_.world.sectorCleared, cleared, boss, exSec, exSub,
                            tourneyRunning());
    };
    if (ev.button == Button::A) {
        // Advance to the next row LANDABLE at the current level, wrapping.
        for (int i = 1; i <= n; ++i) {
            const int r = (listRow_ + i) % n;
            if (explRowLandable(r)) { listRow_ = r; break; }
        }
    } else if (ev.button == Button::B) {
        if (listRow_ < 0 || listRow_ >= n || !explRowLandable(listRow_)) return;
        // STORY's own level, on an area header → DRILL IN: drop to that area's first
        // landable row (its boss-ready header, else its first open/farmable sub).
        // ENDLESS's two rows have nothing to drill into and act directly.
        if (explCat_ == ExplCat::Story && explNavArea_ < 0) {
            explNavArea_ = explRowArea(listRow_);
            for (int r = 0; r < n; ++r)
                if (explRowLandable(r)) { listRow_ = r; break; }
            return;
        }
        const ExplRowState st = stateOf(listRow_);
        const int area = explRowArea(listRow_);
        const int sub = explRowSub(listRow_);
        switch (st) {
            case ExplRowState::AreaBossReady:
            case ExplRowState::AreaCleared:   startAreaBoss(area); break;
            case ExplRowState::SubBossReady:  startSubAreaBoss(area, sub); break;
            case ExplRowState::DeepWebOpen:                     // endless zone
            case ExplRowState::DeepWebDiving: offerThenDive(); break;   // bells first
            case ExplRowState::DarkWebOpen:                     // the terminal zone
            case ExplRowState::DarkWebCrawling: startDarkWebCrawl(); break;
            default:                          startExplore(area, sub);  // arm/re-arm
                // The first walk a player arms is the one moment they are certainly
                // watching before it runs hands-off, so it is when the walk is explained.
                if (!tipSeen(Tip::FirstWalk)) openTipCard(Tip::FirstWalk, Nav::Idle);
                break;
        }
    } else if (ev.button == Button::C) {
        // One level out, every time: inside an area → that category's own rows, parked
        // on the area's header; at a category's level → the ACTIVITY PICKER, parked on
        // the category just left. C never skips a level, so the way out of EXPL is the
        // same number of presses as the way in.
        if (explNavArea_ >= 0) {
            const int a = explNavArea_;
            explNavArea_ = -1;
            listRow_ = areaHeaderRow(a);
        } else {
            const ExplCat c = explCat_;
            explCat_ = ExplCat::None;
            listRow_ = explCatRow(c);
        }
    }
}

void Game::startExplore(int sector, int sub) {
    // Arm a sub-area: explore-mode starts and we return to the IDLE
    // habitat. There is no walk screen. The badge appears; the tick auto-steps from
    // Idle. The streak resets on a fresh arm (per-sub-area); durable sub-area
    // clear/boss-unlock persist.
    exploreActive_ = true;
    exploreSector_ = sector;
    exploreSub_ = sub;
    exploreStreak_ = 0;
    exploreSteps_ = 0;
    exploreStepBeat_ = 0;
    exploreFlavor_[0] = '\0';
    // A fresh walk starts with a clean slate for the empty-queue Happiness-penalty
    // throttle too: a drought from an earlier walk must not suppress this walk's
    // first miss (see game.h's comment on emptyQueueStreak_).
    emptyQueueStreak_ = 0;
    nav_ = Nav::Idle;
    autoArmBackupShield(kRigRowAutoBackup);
    markSaveDirty();
}

void Game::startDeepWebDive() {
    // arm the endless terminal zone (unlocked once every area is cleared).
    // Same idle-background model as startExplore, but on the virtual kDeepWebSector:
    // doExploreStep()/startEncounter() special-case it to scale enemies to the pet and
    // never reach the (nonexistent) boss ladder. A loss ends the dive like any wild.
    if (!deepWebUnlocked()) return;
    exploreActive_ = true;
    exploreSector_ = kDeepWebSector;
    exploreSub_ = 0;
    exploreStreak_ = 0;
    // NB: pet_.armed.deepWebDepthMultiplier is deliberately NOT reset here. A Deep-Learning
    // Module/Core is armed BEFORE the dive (ItemDef::Context::Anytime) so it applies
    // to this dive. Clearing it on the way in would waste it. It is cleared where a
    // dive ENDS instead. A Backdoor/Rootkit/Kernel/Checkpoint Bell arms a start depth
    // ahead of time (Game::applyItemEffects); consume it here so a fresh dive begins
    // there instead of at 0. Checkpoint's sentinel resolves to this pet's own best
    // depth NOW (not at Use-time), so it never targets a stale, since-improved record.
    // The bell's DIVE PAY half goes live with it, for this dive only; a dive started
    // with no bell armed pays no start bonus, whatever an earlier dive carried.
    pet_.armed.diveStartBonusPct = 0;
    if (pet_.armed.deepWebStartDepth != -1) {
        exploreStreak_ = pet_.armed.deepWebStartDepth == PetArmed::kStartDepthUseBest
                              ? pet_.bestDeepWebDepth
                              : pet_.armed.deepWebStartDepth;
        pet_.armed.deepWebStartDepth = -1;
        pet_.armed.diveStartBonusPct = pet_.armed.pendingDiveStartBonusPct;
        pet_.armed.pendingDiveStartBonusPct = 0;
    }
    exploreSteps_ = 0;
    exploreStepBeat_ = 0;
    exploreFlavor_[0] = '\0';
    emptyQueueStreak_ = 0;  // see startExplore's comment above
    nav_ = Nav::Idle;
    autoArmBackupShield(kRigRowAutoBackup);
    markSaveDirty();
}

void Game::startDarkWebCrawl() {
    // The terminal endless zone, reached through the portal at THE SILK LODE's ZERO DAY
    // SHRINE. Everything about the walk is startDeepWebDive's (the same idle background,
    // the same depth-as-streak, the same "a loss ends it") on its own virtual sector, so
    // doExploreStep/startEncounter route to the crawl's roll instead of the dive's.
    //
    // What it does NOT share is the dive's start-depth items: a Bell buys a head start in
    // the farm, and the end of the map is not a thing you skip the front of.
    if (!darkWebUnlocked()) return;
    exploreActive_ = true;
    exploreSector_ = kDarkWebSector;
    exploreSub_ = 0;
    exploreStreak_ = 0;
    exploreSteps_ = 0;
    exploreStepBeat_ = 0;
    exploreFlavor_[0] = '\0';
    emptyQueueStreak_ = 0;
    nav_ = Nav::Idle;
    autoArmBackupShield(kRigRowAutoBackup);
    markSaveDirty();
}

void Game::exploreBadgeLabel(char* out, size_t n) const {
    if (!out || n == 0) return;
    if (inDeepWebDive()) { std::snprintf(out, n, "%s", kDeepWebBadge); return; }
    if (inDarkWebCrawl()) { std::snprintf(out, n, "%s", kDarkWebBadge); return; }
    // The area's own short name (AreaDef::badge) + the 1-based sub number, e.g.
    // "CITRUS 3": sized to clear the right-anchored status field, and measured against it
    // by a native gate. A row that names none falls back to the first word of its display
    // name, which fits only by luck; the same gate is what says whether it did.
    const char* badge = explSectorBadge(exploreSector_);
    char word[12];
    size_t w = 0;
    for (; badge[w] && badge[w] != ' ' && w < sizeof(word) - 1; ++w) word[w] = badge[w];
    word[w] = '\0';
    std::snprintf(out, n, "%s %d", word, exploreSub_ + 1);
}

void Game::doExploreStep() {
    // One explore step: re-arm the pacer, then resolve a GUARANTEED event
    // from the sector's weighted table. Wild encounters DOMINATE (the streak
    // needs wins); loot / Wi-Fi / shop / cache / key are the breather + reward beats.
    // The boss is NOT on this table: it's a manual trigger. No "quiet" rolls.
    exploreStepBeat_ = 0;
    ++exploreSteps_;
    // The lifetime tally the STEPS ladder is measured against (AchSeries::StepsWalked)
    // and the PROFILE row reads. exploreSteps_ is this RUN and resets with every walk;
    // this one outlives the pet, so it is the save's counter rather than the screen's.
    // Dirtying per step is safe: the debounce is measured from the last WRITE, not the
    // last change, so a long walk still lands one write every kSaveDebounceMs rather
    // than pushing the write out ahead of itself.
    ++op_.lifetimeSteps;
    markSaveDirty();
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
    autoArmBackupShield(kRigRowContinuousBackup);   // Rig Shop h: mid-run re-arm
    maybeAutoDefrag();                              // Rig Shop i: hands-off upkeep
    autoProgressStep();                             // may take over and re-aim the walk
}

void Game::autoProgressStep() {
    // The one place auto-progress acts. Every hand-back to the walk runs it, so the
    // condition decides when a step happens: the armed sub-area has to have met its win
    // target. Below that, nothing (the player is still grinding it), which is why an
    // unrelated hand-back like a resolved shop is harmless.
    if (!autoProgress_ || !exploreActive_) return;
    if (inEndlessZone()) return;                    // endless: depth IS the progress
    const int a = exploreSector_, s = exploreSub_;
    if (a < 0 || a >= kExplSectors || s < 0 || s >= kExplSubAreas) return;
    if (exploreStreak_ < kExploreStreakToBoss) return;
    // A boss unlocked by that streak and still standing is the step. A sub-area that's
    // already cleared never unlocks one again (game_combat.cpp), so on a re-run of a
    // done area this falls through to the positional advance below, so the loop keeps
    // moving.
    if (op_.world.subBossUnlocked[a][s] && !op_.world.subCleared[a][s]) {
        startSubAreaBoss(a, s);
        return;
    }
    autoProgressAdvance(a, s);
}

void Game::autoProgressAdvance(int area, int sub) {
    // The step rule: the next rung by index, finished or not. That would strand
    // auto-progress the moment the ladder was complete, which is exactly when a
    // hands-off farm loop is worth having. Everything here is derived from the area/sub
    // indices, so an area spliced into the middle of kAreaList takes its place in the
    // rotation with no rule to update.
    if (sub < kExplSubAreas - 1) { startExplore(area, sub + 1); return; }
    // Past the last sub-area the area's own gauntlet is the next rung, while it
    // stands. Arming auto-progress midway through an area leaves earlier sub-areas
    // unbeaten, so there the rotation wraps to the first one and comes back round to
    // the gauntlet once they all are.
    if (areaBossReady(area) || op_.world.sectorCleared[area]) { startAreaBoss(area); return; }
    startExplore(area, 0);
}

int Game::wildWinXpBase(int sector) {
    // The zone's own base, before wildWinXp's level-difference curve: see the contract
    // in game.h. Everything but the crawl pays the flat one.
    return sector == kDarkWebSector ? kWildWinXpReward * kDarkWebXpPct / 100
                                    : kWildWinXpReward;
}

int Game::exploreXpEfficiencyPct() const {
    // Run the flat base through the SAME chain a wild win pays through
    // (game_combat.cpp's wild-win branch → addCombatXp), rather than restating the
    // arithmetic: a rate that stops matching what the walk actually banks is worse than
    // no readout at all. The one term left out is the depth-keyed Bits bonus, which is
    // not XP.
    //
    // The rate is stated as a percentage of the FLAT base every zone is measured against
    // (kWildWinXpReward) while the payout runs on the armed zone's own base, so a zone
    // that pays a multiple of the flat one reads as that multiple instead of flattening
    // back to 100% against itself.
    const int base = kWildWinXpReward;
    const int zoneBase = wildWinXpBase(exploreSector_);
    // The rung's enemy level. A ladder sub-area's is its GLOBAL depth rung
    // (applyWildSubAreaRamp); an endless zone's is the pet's own plus the logarithmic
    // depth bonus the streak has earned (applyDeepWebScale / applyDarkWebScale), which is
    // why a deep run reads well over 100% and a shallow re-farm well under it.
    int enemyLevel;
    // Keyed off the armed sector's IDENTITY rather than inDeepWebDive(), so a walk that
    // has since been stopped still reads as the zone it was aimed at instead of decoding
    // the dive's one-past-the-ladder index as a ladder rung.
    if (exploreSector_ == kDeepWebSector) {
        // The dive's FOOTHOLD is part of its curve: inside it every dive is the same
        // parity fight, so the readout has to wait it out exactly as applyDeepWebScale
        // does or it advertises a ramp the walk is not paying yet.
        const int rampDepth = exploreStreak_ > kDeepWebRampFreeDepth
                            ? exploreStreak_ - kDeepWebRampFreeDepth : 0;
        enemyLevel = pet_.level + kDeepWebEnemyLevelOffset +
                     kDeepWebDepthLevelPerLog2 * floorLog2(rampDepth + 1);
    } else if (exploreSector_ == kDarkWebSector) {
        // ...and the crawl's has no foothold to wait out (applyDarkWebScale), so its
        // streak rides the curve from the first win.
        enemyLevel = pet_.level + kDarkWebEnemyLevelOffset +
                     kDarkWebDepthLevelPerLog2 * floorLog2(exploreStreak_ + 1);
    } else {
        const int a = exploreSector_ < 0 ? 0
                    : exploreSector_ >= kExplSectors ? kExplSectors - 1 : exploreSector_;
        const int sub = exploreSub_ < 0 ? 0
                      : exploreSub_ >= kExplSubAreas ? kExplSubAreas - 1 : exploreSub_;
        enemyLevel = a * kExplSubAreas + sub;
    }
    const int xp = scaleXp(applyCombatXpBonus(wildWinXp(zoneBase, enemyLevel, pet_.level)));
    return (xp * 100 + base / 2) / base;      // rounded, so a small trim still shows
}

int Game::nextOpenArea(int area) const {
    // The rung after `area`, wrapping past any still-locked area to the front of the
    // ladder. `area` is the fallback and is always valid, because a locked area cannot
    // be entered.
    for (int i = 1; i <= kExplSectors; ++i) {
        const int a = (area + i) % kExplSectors;
        if (explSectorOpen(a, op_.world.sectorCleared)) return a;
    }
    return area;
}

void Game::autoArmBackupShield(int gateRow) {
    if (!op_.rig.featureActive(gateRow)) return;   // not owned, or stopped on SHOP > SERVICES
    if (backupShieldArmed()) return;          // already live, never doubles up
    // applyItemEffects, not useItem: the upgrade grants the effect outright and never
    // reaches into the Vault, so owning it doesn't quietly eat the player's drives.
    if (const ItemDef* d = registry_.item(kBackupDriveId)) applyItemEffects(*d);
}

void Game::dismissPostEncounter() {
    // Any button press, or the kPostEncounterMs timer (tick()), calls this: the
    // readout has nothing to confirm or cancel. Hands straight back to the
    // explore habitat, same as any other resolved event.
    returnToExplore();
}

void Game::onExploreControl(const ButtonEvent& ev) {
    // The A+C control overlay is a CURSOR LIST, exactly like combat's Exploit picker:
    // the chord opens it, and inside it A cycles the row, B does the focused thing and
    // C backs out to the habitat with the walk still running. The chord only opens
    // this. Driving the screen with it would be awkward, because A registers on its
    // own first.
    if (ev.button == Button::A) {
        exploreCtlRow_ = (exploreCtlRow_ + 1) % kExploreControlRows;
        return;
    }
    if (ev.button == Button::C) { nav_ = Nav::Idle; return; }   // back: walk continues
    if (ev.button != Button::B) return;
    switch (static_cast<ExploreControlRow>(exploreCtlRow_)) {
        case ExploreControlRow::Ping:
            // Fire the next guaranteed event NOW, skipping the step timer.
            nav_ = Nav::Idle;
            doExploreStep();                        // may re-enter a full-screen event
            break;
        case ExploreControlRow::Warp:
            // Inert with no key to spend: the row stays, and says so.
            if (!heldWarpKeys().empty()) { warpRow_ = 0; nav_ = Nav::WarpPicker; }
            break;
        case ExploreControlRow::AutoProgress:
            // A MODE, not an action: toggling it leaves the overlay open so the ON/OFF
            // it just changed is the thing the player is looking at.
            autoProgress_ = !autoProgress_;
            break;
        case ExploreControlRow::Sound:
            setExploreSound(!exploreSound_);      // a mode too: the overlay stays open
            break;
        case ExploreControlRow::Stop:
            // Read the dive flag BEFORE clearing exploreActive_: inDeepWebDive() is
            // derived from it. Only a DIVE ending spends the depth multiplier; stopping
            // a sector walk leaves an armed Module/Core armed for the dive it was for.
            if (inDeepWebDive()) clearDiveDepthBuffs();
            exploreActive_ = false;
            exploreStreak_ = 0;
            exploreFlavor_[0] = '\0';
            nav_ = Nav::Idle;
            markSaveDirty();
            break;
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
        case ItemDef::WalkWarp::SafeRest: resolveSafeRestEvent(d); break;
        case ItemDef::WalkWarp::None:     returnToExplore(); break;  // unreachable
    }
}

void Game::resolveSafeRestEvent(const ItemDef& d) {
    // A guaranteed-safe rest: no combat, no roll. The pet rests and the Safe-Mode
    // boot lowers Fragmentation (rest de-frags). HOW MUCH is the key's own negative
    // Frag effect, so it applies through applyItemEffects like any other item and a
    // second, deeper rest key is a row rather than a branch here. The flavor line
    // reports the delta actually taken, not the row's magnitude, so a pet already
    // near clean doesn't get told it shed more than it had.
    const int before = pet_.model.fragmentation();
    applyItemEffects(d);
    std::snprintf(exploreFlavor_, sizeof(exploreFlavor_),
                 "SAFE MODE - RESTED (-%d FRAG)", before - pet_.model.fragmentation());
    markSaveDirty();
    returnToExplore();
}

void Game::debugSetExploreFlavor(const char* s) {
    std::snprintf(exploreFlavor_, sizeof(exploreFlavor_), "%s", s ? s : "");
    dirty_ = true;
}

void Game::resolveCacheEvent() {
    // A Sealed Cache find does not interrupt: it drops a locked container into the
    // inventory and explore-mode keeps stepping. No sub-screen. Roll a weighted
    // container so different caches carry different pools (drawn on Open,
    // openSealedCache). The candidates are every item whose row declares a
    // findWeight, so a cache that is only ever GRANTED (an achievement's Commendation
    // Cache, weight 0) never turns up here. Adding a findable one takes only a row.
    // The player opens whatever drops later, from the VAULT.
    int total = 0;
    for (const ItemDef* it : registry_.allItems())
        if (it->use == ItemDef::Use::OpenContainer) total += it->cache.findWeight;
    if (total <= 0) return;                       // nothing findable: no cache event
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
    ++away_.caches;
    // The VAULT, not ITEMS: a sealed cache is opened from the Hacker face, and pet-side
    // ITEMS refuses one with this exact sentence (itemUsable's "OPEN IN VAULT (A+C)").
    // The two say the same words so the walk never sends a player to a screen that
    // will turn them away.
    std::snprintf(exploreFlavor_, sizeof(exploreFlavor_), "%s - OPEN IN VAULT (A+C)",
                 d->displayName);
    markSaveDirty();
}

void Game::resolveKeyItemEvent() {
    // A Key-item find is NON-INTERRUPTING like the Sealed Cache: it drops a
    // random warp key into the inventory and explore-mode keeps stepping. The player
    // spends it later via the A+C control chord's Warp picker to jump to a
    // shop / safe rest. The pool is every row that declares a walkWarp, in source order.
    std::vector<const ItemDef*> pool;
    for (const ItemDef* k : registry_.allItems())
        if (k->walkWarp != ItemDef::WalkWarp::None) pool.push_back(k);
    if (pool.empty()) return;
    rng_ = rng_ * 1664525u + 1013904223u;
    const ItemDef& d = *pool[(rng_ >> 16) % pool.size()];
    inventory_.add(d.id, 1);
    char buf[32];
    std::snprintf(buf, sizeof(buf), "FOUND %s", d.displayName);
    log_.push(LogEventType::ItemGained, buf);
    std::snprintf(exploreFlavor_, sizeof(exploreFlavor_), "%s - WARP KEY (A+C)",
                  d.displayName);
    markSaveDirty();
}

void Game::resolveLootEvent() {
    // A loot-cache event resolves IN PLACE (no sub-activity), then
    // explore-mode keeps stepping.
    grantLootReward(exploreFlavor_, sizeof(exploreFlavor_));
}

void Game::grantLootReward(char* outFlavor, size_t outFlavorSize) {
    // The shared reward-pool draw: flat Bits + a chance of one item.
    // Full drop tables are a later content pass; this seeds the mechanic.
    // Reused by the loot-cache event and the Wi-Fi sleeping-guardian/open-cache
    // sub-outcomes ("no new ones," same pool, same log path).
    op_.bits += applyCacheBitsBonus(kLootBitsReward);   // Enhanced DataMining
    rng_ = rng_ * 1664525u + 1013904223u;
    const bool dropItem =
        static_cast<int>((rng_ >> 16) % 100) < kLootItemChancePct;
    if (dropItem) {
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
    // Global rarity-weighted draw across EVERY mod: the Epic sealed-cache jackpot (it
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

void Game::grantMod(const char* id) {
    if (!id) return;
    const ModDef* m = registry_.mod(id);
    if (!m) return;
    char buf[kSaveTextCap];
    // A pool already holding modStorageCap() of this mod takes no more. The drop is
    // reported, because a milestone reward that vanishes silently looks like a bug. The
    // line names the Rig Shop row that would have kept it.
    if (!op_.modPool.grant(id, modStorageCap())) {
        std::snprintf(buf, sizeof(buf), "%s: NO ROOM", m->displayName);
        log_.push(LogEventType::ItemGained, buf);
        return;
    }
    std::snprintf(buf, sizeof(buf), "MOD %s L%d", m->displayName, modEquipLevel(*m));
    log_.push(LogEventType::ItemGained, buf);
    markSaveDirty();
}

}  // namespace mal
