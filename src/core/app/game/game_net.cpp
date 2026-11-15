#include "core/app/game.h"

#include <cstdio>
#include <cstring>

#include "tunables.h"
#include "core/ui/expl_screen.h"

namespace mal {

namespace {
// Weighted Sealed-Cache rarity picks for the real-radio discovery rewards
// (pcap-blowup fix follow-on). `roll` is 0..99 from the shared LCG,
// matching every other weighted roll. Local to this unit,
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
// Packs a 6-byte BSSID into the same 48-bit key every dedup ledger in this unit
// uses (networkLedger_, handshakeLedger_).
uint64_t packBssid(const uint8_t* bssid) {
    uint64_t key = 0;
    for (int i = 0; i < 6; ++i) key = (key << 8) | bssid[i];
    return key;
}
// Display names are capped well under the pending-queue's 32-byte SSID
// headroom so every WifiEvent::discoveryLine template (game_net.cpp resolution
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
    // Real-network discovery resolves FIRST, and what it found is what ROUTES the rest of
    // this event. The two halves of this screen come from one roll. Separate rolls could
    // announce a new network, say the pet wanted one, and then have a friend stop by.
    // Three sentences about three different things. The sighting queue decides instead:
    // something to resolve makes this the discovery beat, and an empty queue is what puts
    // the area's GUARDIAN in front of the pet.
    WifiEvent ev;
    resolveNetworkDiscovery(ev);

    // A dry queue on the cadence beat: the guardian is standing there
    // (game_shiboleet.cpp). Every Nth dry event, never the first: arming a walk resets
    // the streak (game_explore.cpp's startExplore), so summoning on the first miss would
    // put a guardian at the head of every fresh walk and let re-arming farm them.
    // Counting to N first also means a walk has to actually BE somewhere dry for a while
    // before anything comes out to look at it, which is the fiction. The dry events
    // between fall through to the ordinary sub-outcome below, so the walk's event
    // density is the same either way.
    if (ev.discovery == NetDiscovery::None &&
        networkSightings_.emptyStreak() % kNetDiscoveryEmptyGuardianStrikes == 0) {
        startShiboleet();
        return;
    }

    rng_ = rng_ * 1664525u + 1013904223u;
    // broadcasting() is the only path into AuditPhase::Hot (registerHandshake →
    // noteHandshake), so a pcap is the sole thing that can wake a guardian.
    ev.rollOutcome(static_cast<int>((rng_ >> 16) % 100), auditCapture_.broadcasting());
    wifi_ = ev;
    walk_.eventBeat = 0;   // start the hands-off hold clock
    fx_.restart();             // and the dissolve's, which no press restarts
    nav_ = Nav::Wifi;
}

const char* Game::walkSectorName() const { return explSectorName(walk_.sector); }

void Game::resolveWifiOutcome() {
    // Playing the outcome out is the event's end, whatever comes after it.
    const WifiOutcome outcome = wifi_ ? wifi_->outcome : WifiOutcome::SleepingGuardian;
    wifi_.reset();
    switch (outcome) {
        case WifiOutcome::SleepingGuardian:
        case WifiOutcome::OpenCache:
            // Sneak the cache / it's just sitting open: same reward pool as a
            // loot cache ("no new ones").
            grantLootReward(walk_.flavor, sizeof(walk_.flavor));
            returnToExplore();
            break;
        case WifiOutcome::AwakenedGuardian:
            // Straight into the shared wild-combat path ("reuse
            // startWildCombat/Nav::Combat"): no separate Fight/Flee/Sinkhole
            // intro; the guardian is already awake. Any rank-up celebration
            // waits for finishCombat to return here.
            startWildCombat(rollWild(), /*forceEnemyFirst=*/false);
            break;
        case WifiOutcome::FriendlyVisit:
            // v1 substitute for the doc-07 Met-Pets Roster (out of scope):
            // a generic ally buff rather than a named remembered pet.
            walk_.escortBattles = kAllyBuffBattles;
            std::snprintf(walk_.flavor, sizeof(walk_.flavor), "ALLY BUFF X%d",
                         kAllyBuffBattles);
            markSaveDirty();
            returnToExplore();
            break;
    }
}

// Hacker Rank -----------------------------------------

void Game::checkHackerRankUp() {
    const int newRank = hackerRankForNetworksSeen(op_.networksSeen);
    if (newRank <= op_.hackerRank) return;
    // A single scan can hear several new APs at once and cross more than one rank
    // (XP model): pay the flat Bits lump per rank gained. Reward-only: this
    // gates nothing, it's just a celebration. Deliberately NOT logged (
    // rank-up is one of the few beats the closed Hacker-Log vocabulary excludes).
    const int gained = newRank - op_.hackerRank;
    op_.hackerRank = newRank;
    op_.bits += kHackerRankUpBitsReward * gained;
    rankUp_.hold(op_.hackerRank, kHackerRankUpBitsReward * gained);
    markSaveDirty();
}

void Game::applyPendingRankUp() {
    rankUp_.take(walk_.flavor, sizeof(walk_.flavor));
}

// Audit-mode passive discovery scan ----------------------

void Game::setNetScanEnabled(bool on) {
    // Capture depends on the discovery scan, so turning scan OFF also seals/disarms
    // capture: the escalating Off<Scan<Capture ladder can never sit in a
    // scan-off/capture-on state (see AuditMode).
    const bool disarmCapture = !on && auditCapture_.enabled();
    if (!settings_.setNetScan(on) && !disarmCapture) return;
    if (disarmCapture) auditCapture_.setEnabled(false, nowMs_);
    markSaveDirty();   // an authorized-use choice: persist it (save v5/v6)
}

void Game::setAuditCaptureEnabled(bool on) {
    // Capture implies scan: arming capture also arms the scan it feeds off (needed
    // for the SSID names + the NETS breadth the capture rides on).
    const bool armScan = on && !settings_.netScan;
    if (auditCapture_.enabled() == on && !armScan) return;
    if (armScan) settings_.setNetScan(true);
    auditCapture_.setEnabled(on, nowMs_);
    markSaveDirty();   // authorized-use opt-in: persist the toggle (save v5/v6)
}

void Game::setAuditMode(AuditMode m) {
    // Order matters so the intermediate state is always valid: raise scan before
    // arming capture; drop capture-implied scan after capture is off. Both low-level
    // setters enforce the invariant, so any transition lands consistently.
    setNetScanEnabled(m != AuditMode::Off);
    setAuditCaptureEnabled(m == AuditMode::ScanCapture);
}

void Game::setApEnabled(bool on) {
    if (!settings_.setAp(on)) return;
    markSaveDirty();   // a persisted CFG opt-in: survives a reboot (save v20)
}

bool Game::registerNetwork(const uint8_t* bssid, const char* ssid) {
    // Scan-time: refresh the in-range snapshot, then queue the sighting for the EXPL
    // Wi-Fi event to resolve later (resolveNetworkDiscovery, below). No XP/reward/
    // log/SD access here, and no dedup against the SD ledger's full history either
    // (that check is deliberately deferred to explore-time, see game.h).
    const uint64_t key = packBssid(bssid);
    char name[33];
    formatDisplayName(bssid, ssid, name, sizeof(name));

    // --- The in-range snapshot (what the CREW picker reads) -------------------
    // Every sighting lands here, dups included: a network the radio keeps hearing
    // must keep reading as in-range no matter what the reward queue is doing.
    const bool arrived = networkSightings_.noteInRange(key, name, nowMs_);
    if (arrived) {
        // An egg can't explore (no EXPL event ever runs), so a network COMING INTO
        // RANGE is the only hatch-accelerating signal it gets. Keyed off the
        // snapshot rather than the reward queue so a network that simply stays in
        // range accelerates once on arrival, not once per sweep.
        accelerateEggHatch(kBootHatchNetworkAccelMs);
    }

    // --- The reward queue (what an EXPL Wi-Fi event will pay out) -------------
    return networkSightings_.queue(key, name);
}

int Game::visibleNetworkCount() const { return networkSightings_.inRangeCount(nowMs_); }

void Game::loadNetworkLedger(const char* path) {
    networkLedger_.load(path);
}

void Game::loadHandshakeLedger(const char* path) {
    handshakeLedger_.load(path);
}

void Game::resolveNetworkDiscovery(WifiEvent& ev) {
    NetworkSightings::Sighting picked;
    if (!networkSightings_.popPending(&picked)) {
        // No penalty. A dry queue costs the pet nothing. An empty area is not the pet's
        // fault, and a Happiness tax would drain a long unattended walk. The streak is
        // still counted, because startWifiEvent reads it to decide when the guardian
        // appears: the beat that costs nothing is the beat that summons.
        return;
    }


    NetworkLedger::Entry existing;
    if (!networkLedger_.lookup(picked.key, &existing)) {
        // New: the only source of Hacker Rank XP and the Sealed-Cache discovery
        // reward.
        networkLedger_.recordNew(picked.key, picked.name);
        ++op_.networksSeen;
        // (Hatch acceleration already fired when this sighting was queued, in
        // registerNetwork: see its comment.)
        checkHackerRankUp();
        pet_.model.setHappiness(pet_.model.happiness() + kNetDiscoveryFoundHappyBonus);

        rng_ = rng_ * 1664525u + 1013904223u;
        const char* id = rollNetworkDiscoveryCacheId(static_cast<int>((rng_ >> 16) % 100));
        inventory_.add(id, 1);
        const ItemDef* got = registry_.item(id);
        char buf[32];
        std::snprintf(buf, sizeof(buf), "FOUND %s", got ? got->displayName : id);
        log_.push(LogEventType::ItemGained, buf);

        rng_ = rng_ * 1664525u + 1013904223u;
        if ((rng_ >> 16) % 2 == 0)
            std::snprintf(ev.discoveryLine, sizeof(ev.discoveryLine),
                         "ADMIRING %s", picked.name);
        else
            std::snprintf(ev.discoveryLine, sizeof(ev.discoveryLine),
                         "ENVIES %s'S BANDWIDTH", picked.name);
        ev.discovery = NetDiscovery::New;
        markSaveDirty();
        return;
    }

    // Repeat: familiar (outside your top favorites, still feeds the pet) vs.
    // home-turf (inside them: flat, no reward, nudges toward variety). The
    // top-N check must run before recordSeenAgain. This visit's count is about
    // to grow, and checking after would defeat the "was this occasional UNTIL
    // NOW" question for a network sitting exactly at the boundary.
    const bool wasHomeTurf =
        networkLedger_.inTopN(picked.key, kNetDiscoveryTopFavoritesCount);
    networkLedger_.recordSeenAgain(picked.key, picked.name);
    if (wasHomeTurf) {
        std::snprintf(ev.discoveryLine, sizeof(ev.discoveryLine),
                     "TIRED OF %s...", picked.name);
        ev.discovery = NetDiscovery::HomeTurf;
        return;
    }
    addCombatXp(kNetDiscoveryFondXpAmount);
    pet_.model.setHappiness(pet_.model.happiness() + kNetDiscoveryFoundHappyBonus);
    std::snprintf(ev.discoveryLine, sizeof(ev.discoveryLine),
                 "FONDLY REMEMBERS %s", picked.name);
    ev.discovery = NetDiscovery::Fond;
    markSaveDirty();
}

bool Game::handshakeAlreadyCaptured(const uint8_t* bssid) const {
    return handshakeLedger_.contains(packBssid(bssid));
}

bool Game::registerHandshake(const uint8_t* bssid) {
    // A capture DID occur, so drive the policy SM hot/broadcast regardless of
    // whether it's a new network (noteHandshake self-guards on phase). Then dedup
    // the *credit* by BSSID: SHAKES counts distinct handshakes, so re-capturing a
    // known network (walking home past the same AP) never re-credits. At the ledger
    // cap the oldest BSSID rotates out (handshake_ledger.h), so a long walk keeps
    // crediting fresh captures instead of going dead.
    auditCapture_.noteHandshake(nowMs_);
    if (!handshakeLedger_.insert(packBssid(bssid))) return false;  // already credited this AP
    ++op_.cant.handshakesSeen;
    // A new captured handshake is rarer and harder to earn, so it pays a richer
    // Sealed Cache than the plain-discovery reward above (no Common tier).
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
