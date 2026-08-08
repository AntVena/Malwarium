#include "core/app/game.h"

#include <cstdio>
#include <cstring>

#include "tunables.h"
#include "core/render/canvas.h"
#include "core/render/font.h"
#include "core/render/palette.h"
#include "core/render/sprite.h"
#include "core/ui/carousel.h"
#include "core/ui/combat_screen.h"
#include "core/ui/expl_screen.h"
#include "core/ui/items_screen.h"
#include "core/ui/maint_screen.h"
#include "core/ui/modals.h"
#include "core/ui/stat_screen.h"
#include "core/ui/train_screen.h"
#include "generated/assets.h"

namespace mal {

namespace {
// Weighted Sealed-Cache rarity picks for the real-radio discovery rewards
// (pcap-blowup fix follow-on). `roll` is 0..99 from the shared LCG,
// matching every other weighted roll in game_items.cpp. Local to this unit —
// only resolveNetworkDiscovery()/registerHandshake() below use them.
const char* rollNetworkDiscoveryCacheId(int roll) {
    return roll < kNetDiscoveryCacheCommonPct ? "sealed_cache_common"
                                               : "sealed_cache_uncommon";
}
const char* rollHandshakeCaptureCacheId(int roll) {
    if (roll < kHandshakeCaptureCacheUncommonPct) return "sealed_cache_uncommon";
    if (roll < kHandshakeCaptureCacheUncommonPct + kHandshakeCaptureCacheRarePct)
        return "sealed_cache_rare";
    return "sealed_cache_epic";
}
// Packs a 6-byte BSSID into the same 48-bit key used by every other dedup
// ledger in this unit (seenHandshakeBssids_, and formerly seenBssids_).
uint64_t packBssid(const uint8_t* bssid) {
    uint64_t key = 0;
    for (int i = 0; i < 6; ++i) key = (key << 8) | bssid[i];
    return key;
}
// Display names are capped well under the pending-queue's 32-byte SSID
// headroom so every netDiscoveryFlavor_ template (game_net.cpp resolution
// below) fits its 40-char buffer even for a max-length real SSID.
constexpr size_t kDisplayNameCap = 18;
void formatDisplayName(const uint8_t* bssid, const char* ssid, char* out, size_t cap) {
    const size_t effCap = cap < kDisplayNameCap ? cap : kDisplayNameCap;
    if (ssid && ssid[0] != '\0') {
        std::strncpy(out, ssid, effCap - 1);
        out[effCap - 1] = '\0';
    } else {
        // Hidden/no-SSID network: a short label from the BSSID's last 3 bytes.
        std::snprintf(out, effCap, "NET-%02X%02X%02X", bssid[3], bssid[4], bssid[5]);
    }
}
}  // namespace

// Wi-Fi network event ------------------------------------------

void Game::startWifiEvent() {
    // Real-network discovery resolves first (own flavor line, netDiscoveryFlavor_),
    // independent of the guardian/cache/friendly roll below (own flavor line,
    // wifiFlavor_) — two unrelated beats on one screen, see game.h's comment.
    resolveNetworkDiscovery();

    rng_ = rng_ * 1664525u + 1013904223u;
    const int roll = static_cast<int>((rng_ >> 16) % 100);
    if (roll < kWifiSleepingPct) {
        wifiOutcome_ = WifiOutcome::SleepingGuardian;
        std::snprintf(wifiFlavor_, sizeof(wifiFlavor_), "GUARDIAN'S ASLEEP...");
    } else if (roll < kWifiSleepingPct + kWifiAwakenedPct) {
        // A guardian only WAKES (→ attacks) when the player is "hot" — i.e. actively
        // broadcasting after a real handshake capture. Merely *discovering*
        // a network never flags you hot, so an un-hot player sneaks the cache instead
        // of being auto-attacked. `broadcasting()` is the only path into AuditPhase::Hot
        // (registerHandshake → noteHandshake), so a pcap is the sole trigger.
        if (auditCapture_.broadcasting()) {
            wifiOutcome_ = WifiOutcome::AwakenedGuardian;
            std::snprintf(wifiFlavor_, sizeof(wifiFlavor_), "GUARDIAN AWAKENS!");
        } else {
            wifiOutcome_ = WifiOutcome::SleepingGuardian;
            std::snprintf(wifiFlavor_, sizeof(wifiFlavor_), "GUARDIAN'S ASLEEP...");
        }
    } else if (roll < kWifiSleepingPct + kWifiAwakenedPct + kWifiOpenCachePct) {
        wifiOutcome_ = WifiOutcome::OpenCache;
        std::snprintf(wifiFlavor_, sizeof(wifiFlavor_), "CACHE WIDE OPEN...");
    } else {
        wifiOutcome_ = WifiOutcome::FriendlyVisit;
        std::snprintf(wifiFlavor_, sizeof(wifiFlavor_), "A FRIEND STOPS BY...");
    }
    exploreEventBeat_ = 0;   // start the hands-off hold clock
    nav_ = Nav::Wifi;
}

void Game::onWifi(const ButtonEvent& ev) {
    // B plays out the rolled outcome; A/C are no-ops — it auto-resolves,
    // there's no engage/flee choice here the way a wild encounter has one. Any press
    // restarts the hands-off hold so a watching player isn't rushed.
    exploreEventBeat_ = 0;
    if (ev.button == Button::B) resolveWifiOutcome();
}

void Game::resolveWifiOutcome() {
    switch (wifiOutcome_) {
        case WifiOutcome::SleepingGuardian:
        case WifiOutcome::OpenCache:
            // Sneak the cache / it's just sitting open — same reward pool as a
            // loot cache ("no new ones").
            grantLootReward(exploreFlavor_, sizeof(exploreFlavor_));
            returnToExplore();
            break;
        case WifiOutcome::AwakenedGuardian:
            // Straight into the shared wild-combat path ("reuse
            // startWildCombat/Nav::Combat") — no separate Fight/Flee/Sinkhole
            // intro; the guardian is already awake. Any rank-up celebration
            // waits for finishCombat to return here.
            rng_ = rng_ * 1664525u + 1013904223u;             // roster variant roll
            encounterEnemy_ = wildMalbeast(explSectorTier(exploreSector_), rng_ >> 16);
            applyWildSubAreaRamp(encounterEnemy_, exploreSector_, exploreSub_);  // depth ramp
            startWildCombat(/*forceEnemyFirst=*/false);
            break;
        case WifiOutcome::FriendlyVisit:
            // v1 substitute for the doc-07 Met-Pets Roster (out of scope):
            // a generic ally buff rather than a named remembered pet.
            allyBuffBattlesLeft_ = kAllyBuffBattles;
            std::snprintf(exploreFlavor_, sizeof(exploreFlavor_), "ALLY BUFF X%d",
                         kAllyBuffBattles);
            markSaveDirty();
            returnToExplore();
            break;
    }
}

// Hacker Rank -----------------------------------------

void Game::checkHackerRankUp() {
    const int newRank = hackerRankForNetworksSeen(networksSeen_);
    if (newRank <= hackerRank_) return;
    // A single scan can hear several new APs at once and cross more than one rank
    // (XP model) — pay the flat Bits lump per rank gained. Reward-only: this
    // gates nothing, it's just a celebration. Deliberately NOT logged (
    // rank-up is one of the few beats the closed Hacker-Log vocabulary excludes).
    const int gained = newRank - hackerRank_;
    hackerRank_ = newRank;
    bits_ += kHackerRankUpBitsReward * gained;
    std::snprintf(rankUpFlavor_, sizeof(rankUpFlavor_), "RANK UP R%d +%dB",
                 hackerRank_, kHackerRankUpBitsReward * gained);
    rankUpPending_ = true;
    markSaveDirty();
}

void Game::applyPendingRankUp() {
    if (!rankUpPending_) return;
    std::strncpy(exploreFlavor_, rankUpFlavor_, sizeof(exploreFlavor_) - 1);
    exploreFlavor_[sizeof(exploreFlavor_) - 1] = '\0';
    rankUpPending_ = false;
}

// Audit-mode passive discovery scan ----------------------

void Game::setNetScanEnabled(bool on) {
    // Capture depends on the discovery scan, so turning scan OFF also seals/disarms
    // capture — the escalating Off<Scan<Capture ladder can never sit in a
    // scan-off/capture-on state (see AuditMode).
    const bool disarmCapture = !on && auditCapture_.enabled();
    if (netScanEnabled_ == on && !disarmCapture) return;
    netScanEnabled_ = on;
    if (disarmCapture) auditCapture_.setEnabled(false, nowMs_);
    markSaveDirty();   // an authorized-use choice — persist it (save v5/v6)
}

void Game::setAuditCaptureEnabled(bool on) {
    // Capture implies scan: arming capture also arms the scan it feeds off (needed
    // for the SSID names + the NETS breadth the capture rides on).
    const bool armScan = on && !netScanEnabled_;
    if (auditCapture_.enabled() == on && !armScan) return;
    if (armScan) netScanEnabled_ = true;
    auditCapture_.setEnabled(on, nowMs_);
    markSaveDirty();   // authorized-use opt-in — persist the toggle (save v5/v6)
}

void Game::setAuditMode(AuditMode m) {
    // Order matters so the intermediate state is always valid: raise scan before
    // arming capture; drop capture-implied scan after capture is off. Both low-level
    // setters enforce the invariant, so any transition lands consistently.
    setNetScanEnabled(m != AuditMode::Off);
    setAuditCaptureEnabled(m == AuditMode::ScanCapture);
}

void Game::setApEnabled(bool on) {
    if (apEnabled_ == on) return;
    apEnabled_ = on;
    markSaveDirty();   // a persisted CFG opt-in — survives a reboot (save v20)
}

bool Game::registerNetwork(const uint8_t* bssid, const char* ssid) {
    // Scan-time: refresh the in-range snapshot, then queue the sighting for the EXPL
    // Wi-Fi event to resolve later (resolveNetworkDiscovery, below) — no XP/reward/
    // log/SD access here, and no dedup against the SD ledger's full history either
    // (that check is deliberately deferred to explore-time, see game.h).
    const uint64_t key = packBssid(bssid);
    char name[33];
    formatDisplayName(bssid, ssid, name, sizeof(name));

    // --- The in-range snapshot (what the CREW picker reads) -------------------
    // Every sighting lands here, dups included: a network the radio keeps hearing
    // must keep reading as in-range no matter what the reward queue is doing.
    const bool arrived = noteVisibleNetwork(key, name);
    if (arrived) {
        // An egg can't explore (no EXPL event ever runs), so a network COMING INTO
        // RANGE is the only hatch-accelerating signal it gets. Keyed off the
        // snapshot rather than the reward queue so a network that simply stays in
        // range accelerates once on arrival, not once per sweep.
        accelerateEggHatch(kBootHatchNetworkAccelMs);
    }

    // --- The reward queue (what an EXPL Wi-Fi event will pay out) -------------
    for (int i = 0; i < pendingNetworkCount_; ++i)
        if (pendingNetworks_[i].key == key) return false;  // already owed a reward
    if (pendingNetworkCount_ >= kPendingNetworkQueueCap) {
        // Full: drop the OLDEST unresolved sighting to make room. A player who
        // hasn't walked in a while loses the tail of a backlog they were never
        // going to reach anyway, rather than having the queue go deaf to
        // everything new for the rest of the boot.
        for (int i = 1; i < pendingNetworkCount_; ++i)
            pendingNetworks_[i - 1] = pendingNetworks_[i];
        --pendingNetworkCount_;
    }

    PendingNetwork& slot = pendingNetworks_[pendingNetworkCount_++];
    slot.key = key;
    std::strncpy(slot.name, name, sizeof(slot.name) - 1);
    slot.name[sizeof(slot.name) - 1] = '\0';
    return true;
}

bool Game::noteVisibleNetwork(uint64_t key, const char* name) {
    // Already in the snapshot: refresh its timestamp (and its label — a hidden
    // network that later broadcasts its SSID should stop reading as NET-xxxxxx).
    // "Arrived" means arrived: a stale entry for a network that dropped out and came
    // back counts as a fresh arrival, matching what the player just experienced.
    for (int i = 0; i < visibleNetworkCount_; ++i) {
        VisibleNetwork& v = visibleNetworks_[i];
        if (v.key != key) continue;
        const bool wasStale = !networkVisible(v);
        std::strncpy(v.name, name, sizeof(v.name) - 1);
        v.name[sizeof(v.name) - 1] = '\0';
        v.lastSeenMs = nowMs_;
        return wasStale;
    }

    int at = visibleNetworkCount_;
    if (at >= kNetVisibleCap) {
        // Full: evict the STALEST entry (oldest sighting). Anything still in range is
        // being refreshed every sweep, so what goes is whatever the radio has heard
        // least recently — never something the player is standing in front of.
        at = 0;
        for (int i = 1; i < visibleNetworkCount_; ++i)
            if (visibleNetworks_[i].lastSeenMs < visibleNetworks_[at].lastSeenMs) at = i;
    } else {
        ++visibleNetworkCount_;
    }
    VisibleNetwork& v = visibleNetworks_[at];
    v.key = key;
    std::strncpy(v.name, name, sizeof(v.name) - 1);
    v.name[sizeof(v.name) - 1] = '\0';
    v.lastSeenMs = nowMs_;
    return true;
}

int Game::visibleNetworkCount() const {
    int n = 0;
    for (int i = 0; i < visibleNetworkCount_; ++i)
        if (networkVisible(visibleNetworks_[i])) ++n;
    return n;
}

void Game::loadNetworkLedger(const char* path) {
    networkLedger_.load(path);
}

void Game::resolveNetworkDiscovery() {
    if (pendingNetworkCount_ == 0) {
        ++emptyQueueStreak_;
        netDiscoveryFlavor_[0] = '\0';
        // Only sting on the 1st miss, then every Nth after — a long unmonitored
        // dead-zone walk tapers off instead of grinding Happiness to 0.
        const bool strikes = emptyQueueStreak_ == 1 ||
            (emptyQueueStreak_ - 1) % kNetDiscoveryEmptyCooldownStrikes == 0;
        if (strikes) {
            model_.setHappiness(model_.happiness() - kNetDiscoveryNoneHappyPenalty);
            std::snprintf(netDiscoveryFlavor_, sizeof(netDiscoveryFlavor_),
                         "WANTS NEW NETWORKS...");
        }
        return;
    }

    emptyQueueStreak_ = 0;
    // Pop the oldest pending sighting (FIFO — resolve in scan order).
    const PendingNetwork picked = pendingNetworks_[0];
    for (int i = 1; i < pendingNetworkCount_; ++i) pendingNetworks_[i - 1] = pendingNetworks_[i];
    --pendingNetworkCount_;

    NetworkLedger::Entry existing;
    if (!networkLedger_.lookup(picked.key, &existing)) {
        // Genuinely new — the sole source of Hacker Rank XP + the Sealed-Cache
        // discovery reward (moved here verbatim from the old registerNetwork).
        networkLedger_.recordNew(picked.key, picked.name);
        ++networksSeen_;
        // (Hatch acceleration already fired when this sighting was queued, in
        // registerNetwork — see its comment.)
        checkHackerRankUp();
        model_.setHappiness(model_.happiness() + kNetDiscoveryFoundHappyBonus);

        rng_ = rng_ * 1664525u + 1013904223u;
        const char* id = rollNetworkDiscoveryCacheId(static_cast<int>((rng_ >> 16) % 100));
        inventory_.add(id, 1);
        const ItemDef* got = registry_.item(id);
        char buf[32];
        std::snprintf(buf, sizeof(buf), "FOUND %s", got ? got->displayName : id);
        log_.push(LogEventType::ItemGained, buf);

        rng_ = rng_ * 1664525u + 1013904223u;
        if ((rng_ >> 16) % 2 == 0)
            std::snprintf(netDiscoveryFlavor_, sizeof(netDiscoveryFlavor_),
                         "ADMIRING %s", picked.name);
        else
            std::snprintf(netDiscoveryFlavor_, sizeof(netDiscoveryFlavor_),
                         "ENVIES %s'S BANDWIDTH", picked.name);
        markSaveDirty();
        return;
    }

    // Repeat: familiar (outside your top favorites, still feeds the pet) vs.
    // home-turf (inside them — flat, no reward, nudges toward variety). The
    // top-N check MUST run before recordSeenAgain — this visit's own count is
    // about to grow, and checking after would compare it against a landscape
    // where it just outgrew itself, self-defeating the "was this occasional
    // UNTIL NOW" question for a network sitting exactly at the boundary.
    const bool wasHomeTurf =
        networkLedger_.inTopN(picked.key, kNetDiscoveryTopFavoritesCount);
    networkLedger_.recordSeenAgain(picked.key, picked.name);
    if (wasHomeTurf) {
        std::snprintf(netDiscoveryFlavor_, sizeof(netDiscoveryFlavor_),
                     "TIRED OF %s...", picked.name);
        return;
    }
    addCombatXp(kNetDiscoveryFondXpAmount);
    model_.setHappiness(model_.happiness() + kNetDiscoveryFoundHappyBonus);
    std::snprintf(netDiscoveryFlavor_, sizeof(netDiscoveryFlavor_),
                 "FONDLY REMEMBERS %s", picked.name);
    markSaveDirty();
}

bool Game::handshakeAlreadyCaptured(const uint8_t* bssid) const {
    uint64_t key = 0;
    for (int i = 0; i < 6; ++i) key = (key << 8) | bssid[i];
    for (uint64_t k : seenHandshakeBssids_)
        if (k == key) return true;
    return false;
}

bool Game::registerHandshake(const uint8_t* bssid) {
    // A capture DID occur, so drive the policy SM hot/broadcast regardless of
    // whether it's a new network (noteHandshake self-guards on phase). Then dedup
    // the *credit* by BSSID: SHAKES counts distinct handshakes, so re-capturing a
    // known network (walking home past the same AP) never re-credits.
    auditCapture_.noteHandshake(nowMs_);
    uint64_t key = 0;
    for (int i = 0; i < 6; ++i) key = (key << 8) | bssid[i];
    for (uint64_t k : seenHandshakeBssids_)
        if (k == key) return false;                 // already credited this AP
    if (static_cast<int>(seenHandshakeBssids_.size()) >= kBssidDedupCap)
        return false;                               // dedup set full — don't overcount
    seenHandshakeBssids_.push_back(key);
    ++handshakesSeen_;
    // A genuinely new captured handshake is the rarer, harder-earned event — a
    // richer Sealed Cache than the plain-discovery reward above (no Common tier).
    rng_ = rng_ * 1664525u + 1013904223u;
    {
        const char* id = rollHandshakeCaptureCacheId(static_cast<int>((rng_ >> 16) % 100));
        inventory_.add(id, 1);
        const ItemDef* got = registry_.item(id);
        char buf[32];
        std::snprintf(buf, sizeof(buf), "FOUND %s", got ? got->displayName : id);
        log_.push(LogEventType::ItemGained, buf);
    }
    markSaveDirty();
    return true;
}


}  // namespace mal
