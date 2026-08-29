// test_audit.cpp — native gates for Hacker Rank, the ledgers and the audit/pcap capture path.
//
// One slice of the native suite; see test_gates.h for the shared fixtures and
// test_main.cpp for the roster that runs these. A save-vNN gate sits with the
// feature whose field it migrates, not in a migrations pile of its own.
#include "test_gates.h"

// Hacker Rank XP model: every network = kHackerRankXpPerNetwork
// XP; kHackerRankXpPerRank XP per rank. Rank is UNBOUNDED (no tier cap) and
// monotonic; XP-into-rank wraps cleanly; titles clamp to the named tiers.
void test_hacker_rank_xp_model() {
    const int perRank = kHackerRankXpPerRank / kHackerRankXpPerNetwork;  // networks/rank

    CHECK(hackerRankForNetworksSeen(0) == 0);
    CHECK(hackerRankXp(0) == 0);
    CHECK(hackerRankXpIntoRank(0) == 0);

    // The XP math holds over many networks — walk a big range and check the
    // rank/xp identities exactly (the "XP bounds test").
    for (int n = 0; n <= 5000; ++n) {
        CHECK(hackerRankXp(n) == n * kHackerRankXpPerNetwork);
        CHECK(hackerRankForNetworksSeen(n) == n / perRank);
        CHECK(hackerRankXpIntoRank(n) ==
              (n * kHackerRankXpPerNetwork) % kHackerRankXpPerRank);
    }
    // Just-below / just-at a rank boundary tips exactly one rank.
    CHECK(hackerRankForNetworksSeen(perRank - 1) == 0);
    CHECK(hackerRankForNetworksSeen(perRank) == 1);
    CHECK(hackerRankForNetworksSeen(2 * perRank) == 2);
    // Unbounded: far past the named-tier count still climbs.
    CHECK(hackerRankForNetworksSeen(1000 * perRank) == 1000);

    // Titles: a growable, threshold-keyed ladder — hackerRankTitle(rank) is the
    // nearest tier at or beneath the numeric rank. The count is DERIVED (no fixed
    // constant), so appending a title just extends the ladder.
    const int tiers = hackerRankTierCount();
    CHECK(tiers >= 1);
    CHECK(std::strcmp(hackerRankTitle(0), "PACKET RAT") == 0);
    CHECK(std::strcmp(hackerRankTitle(-1), "PACKET RAT") == 0);   // below first → first
    // Every named tier resolves to itself at its unlock rank, and each tier's title
    // holds until the next tier's threshold (the "nearest at/beneath" contract).
    for (int i = 0; i < tiers; ++i) {
        const int at = hackerRankTierUnlock(i);
        CHECK(at >= 0);
        CHECK(std::strcmp(hackerRankTitle(at), hackerRankTitle(at)) == 0);   // stable
        if (i + 1 < tiers)
            CHECK(std::strcmp(hackerRankTitle(hackerRankTierUnlock(i + 1) - 1),
                              hackerRankTitle(at)) == 0);
    }
    // A rank far past the top tier keeps the top (highest) title — the numeric rank
    // keeps climbing while the title caps at the end of the ladder.
    const char* top = hackerRankTitle(hackerRankTierUnlock(tiers - 1));
    CHECK(std::strcmp(hackerRankTitle(9999), top) == 0);
}

// registerNetwork is the scan-time seam: dedup a sighting against the pending
// queue ONLY (never the full ledger, never a credit) and hold it for the EXPL
// Wi-Fi event to resolve — networksSeen stays 0 through all of this; crediting
// is resolveNetworkDiscovery's job, exercised via walkAndCreditNetwork in the
// tests below.
void test_register_network_queue_dedup() {
    Game g{StartMode::Hatched};
    CHECK(g.networksSeen() == 0);

    uint8_t a[6]  = {0xAA, 0xBB, 0xCC, 0x00, 0x00, 0x01};
    uint8_t a2[6] = {0xAA, 0xBB, 0xCC, 0x00, 0x00, 0x02};
    CHECK(g.registerNetwork(a, "A"));        // new -> queued
    CHECK(g.networksSeen() == 0);            // queuing never credits
    CHECK(!g.registerNetwork(a, "A"));       // already pending -> not re-queued
    CHECK(g.registerNetwork(a2, "A2"));      // differs in one byte -> queued
    CHECK(g.networksSeen() == 0);

    // The pending queue is capped, and a FULL queue makes room by evicting its
    // OLDEST unresolved sighting rather than refusing the new one — a backlog the
    // player hasn't walked off must never stop fresh sightings being recorded.
    for (int i = 0; i < kPendingNetworkQueueCap - 2; ++i) {   // 2 already queued above
        uint8_t m[6] = {0x02, 0x03, 0x04, 0x05,
                        static_cast<uint8_t>(i >> 8), static_cast<uint8_t>(i)};
        CHECK(g.registerNetwork(m, "M"));
    }
    CHECK(g.pendingNetworks() == kPendingNetworkQueueCap);
    uint8_t overflow[6] = {0x99, 0x99, 0x99, 0x99, 0x99, 0x99};
    CHECK(g.registerNetwork(overflow, "Overflow"));    // queue full -> admitted anyway
    CHECK(g.pendingNetworks() == kPendingNetworkQueueCap);   // ...by eviction, not growth
    // `a` was the oldest, so it's what got evicted: it re-queues as a new sighting.
    CHECK(g.registerNetwork(a, "A"));
    CHECK(g.networksSeen() == 0);                      // none of this ever credits
}

// Crossing a rank via a real credited network (XP model): a flat Bits lump PER
// rank gained, no Hacker-Log entry for the celebration itself (rank-up excluded
// from the closed log vocabulary). Reward-ONLY: rank gates nothing anymore (all
// EXPL sectors open regardless of rank — sector gating is sectorCleared[]).
void test_hacker_rank_up_grants_reward() {
    const int perRank = kHackerRankXpPerRank / kHackerRankXpPerNetwork;
    Game g{StartMode::Hatched};
    CHECK(g.hackerRank() == 0);

    // Credit exactly one rank's worth of distinct networks -> one rank up.
    // Each walkToWifiEvent search can pass through OTHER incidental bits-
    // granting explore events on the way to landing on Wifi (loot/cache steps,
    // sinkholed wild encounters) — noise unrelated to the rank-up reward, and it
    // accumulates across iterations, so comparing a running total against a
    // pre-loop baseline would be unreliable. Instead isolate the reward at its
    // source: track the Bits delta across ONLY the specific credit call where
    // hackerRank() actually changes (before vs. right when that event is
    // reached — resolveNetworkDiscovery's own effects only, still before the
    // unrelated guardian/cache/friendly roll that always follows resolves).
    int rankUpBitsGrant = 0;
    int prevRank = g.hackerRank();
    for (int i = 0; i < perRank; ++i) {
        uint8_t m[6] = {0x10, 0, 0, 0, 0, static_cast<uint8_t>(i)};
        const int bitsBefore = g.bits();
        queueAndReachWifiEvent(g, m, "R");
        if (g.hackerRank() != prevRank) {
            rankUpBitsGrant += g.bits() - bitsBefore;
            prevRank = g.hackerRank();
        }
        resolveWifiEventToIdle(g);
    }
    CHECK(g.networksSeen() == perRank);
    CHECK(g.hackerRank() == 1);
    // Reward-only: climbing rank does NOT unlock sectors — sector[1] stays LOCKED
    // (gating is sectorCleared[], not rank). Only sector 0 is open.
    { bool fl[kExplSectors] = {g.sectorCleared(0), g.sectorCleared(1)};
      CHECK(explSectorOpen(0, fl));
      CHECK(!explSectorOpen(1, fl)); }
    // >= not == : the isolated delta above still spans this ONE credit call's own
    // walkToWifiEvent search, which (rarely) may ALSO pass through an unrelated
    // bits-granting step (a loot cache) before landing on Wifi — noise can only
    // ADD, so a lower bound still proves the reward itself fires.
    CHECK(rankUpBitsGrant >= kHackerRankUpBitsReward);
    // The rank-up celebration ITSELF still isn't logged — but each of the
    // `perRank` new-network credits now also grants + logs a discovery Sealed
    // Cache (the pcap-blowup follow-on reward), so log size DOES grow (capped
    // at EventLog::kCapacity). Assert the property directly: no entry mentions
    // the rank-up celebration text, rather than asserting "size didn't move"
    // (the reward logging writes an entry, so that would be wrong).
    for (int i = 0; i < g.log().size(); ++i)
        CHECK(std::strstr(g.log().at(i).text, "RANK") == nullptr);

    // A batch that crosses several ranks at once pays per rank gained — same
    // per-credit isolation, summed across every iteration that actually crosses.
    const int rank1 = g.hackerRank();
    int totalRankUpBits = 0;
    int prevRank2 = rank1;
    for (int i = 0; i < 2 * perRank; ++i) {
        uint8_t m[6] = {0x20, 0, 0, 0, static_cast<uint8_t>(i >> 8),
                        static_cast<uint8_t>(i)};
        const int bitsBefore = g.bits();
        queueAndReachWifiEvent(g, m, "R2");
        if (g.hackerRank() != prevRank2) {
            totalRankUpBits += g.bits() - bitsBefore;
            prevRank2 = g.hackerRank();
        }
        resolveWifiEventToIdle(g);
    }
    const int gained = g.hackerRank() - rank1;
    CHECK(gained == 2);
    CHECK(totalRankUpBits >= kHackerRankUpBitsReward * gained);   // see the >= note above
}

// NetworkLedger (core/net/network_ledger.h) — pure logic, no file I/O exercised
// here (native tests don't touch real SD/file access, matching net_capture.h's
// own documented host/device testing boundary). recordNew seeds count 1;
// recordSeenAgain bumps it and can upgrade a name from empty (e.g. a hidden
// network scanned before its SSID was ever known) once a real one arrives.
void test_network_ledger_new_and_repeat() {
    NetworkLedger ledger;
    NetworkLedger::Entry e;
    CHECK(!ledger.lookup(1, &e));

    ledger.recordNew(1, "Alpha");
    CHECK(ledger.lookup(1, &e));
    CHECK(e.count == 1);
    CHECK(std::strcmp(e.name, "Alpha") == 0);

    ledger.recordSeenAgain(1, "Alpha");
    CHECK(ledger.lookup(1, &e));
    CHECK(e.count == 2);

    ledger.recordNew(2, "");           // e.g. a hidden network's BSSID fallback
    CHECK(ledger.lookup(2, &e));
    CHECK(e.name[0] == '\0');
    ledger.recordSeenAgain(2, "NowKnown");   // its SSID resolves on a later scan
    CHECK(ledger.lookup(2, &e));
    CHECK(std::strcmp(e.name, "NowKnown") == 0);
}

// PeerHello codec (core/net/peer_link.h) — the pet-to-pet beacon's wire format.
// A round-trip must preserve every field, and the encoded frame must be exactly
// one fixed size so the device tier can send it as a single ESP-NOW payload.
void test_peer_hello_round_trip() {
    PeerHello out;
    std::strncpy(out.tag, "GHOSTBYTE", sizeof(out.tag) - 1);
    std::strncpy(out.petName, "Paypup", sizeof(out.petName) - 1);
    std::strncpy(out.crewName, "Deniers of Service", sizeof(out.crewName) - 1);
    out.stage = static_cast<uint8_t>(Stage::Script);
    out.rank = 7;
    out.crewRed = false;

    uint8_t frame[kPeerHelloSize];
    CHECK(encodePeerHello(out, frame, sizeof(frame)) == kPeerHelloSize);

    PeerHello in;
    CHECK(decodePeerHello(frame, kPeerHelloSize, &in));
    CHECK(std::strcmp(in.tag, "GHOSTBYTE") == 0);
    CHECK(std::strcmp(in.petName, "Paypup") == 0);
    CHECK(std::strcmp(in.crewName, "Deniers of Service") == 0);
    CHECK(in.stage == static_cast<uint8_t>(Stage::Script));
    CHECK(in.rank == 7);
    CHECK(!in.crewRed);

    // The team bit is carried independently of the crew name.
    out.crewRed = true;
    CHECK(encodePeerHello(out, frame, sizeof(frame)) == kPeerHelloSize);
    CHECK(decodePeerHello(frame, kPeerHelloSize, &in));
    CHECK(in.crewRed);

    // An unenlisted operator broadcasts an empty crew — that's the "no crew"
    // signal the PEERS screen reads, not a separate flag.
    PeerHello solo;
    std::strncpy(solo.tag, "LONEWOLF", sizeof(solo.tag) - 1);
    CHECK(encodePeerHello(solo, frame, sizeof(frame)) == kPeerHelloSize);
    CHECK(decodePeerHello(frame, kPeerHelloSize, &in));
    CHECK(in.crewName[0] == '\0');
}

// Everything a decode accepts gets rendered and written to SD, so the frame is a
// trust boundary: a wrong length, foreign magic, or a protocol version this build
// can't lay out must be refused outright rather than parsed on a guess.
void test_peer_hello_rejects_malformed() {
    PeerHello src;
    std::strncpy(src.tag, "VALID", sizeof(src.tag) - 1);
    uint8_t frame[kPeerHelloSize];
    CHECK(encodePeerHello(src, frame, sizeof(frame)) == kPeerHelloSize);

    PeerHello out;
    CHECK(decodePeerHello(frame, kPeerHelloSize, &out));      // baseline: accepted

    CHECK(!decodePeerHello(frame, kPeerHelloSize - 1, &out)); // short frame
    CHECK(!decodePeerHello(frame, kPeerHelloSize + 1, &out)); // long frame
    CHECK(!decodePeerHello(nullptr, kPeerHelloSize, &out));
    CHECK(!decodePeerHello(frame, kPeerHelloSize, nullptr));

    uint8_t bad[kPeerHelloSize];
    std::memcpy(bad, frame, sizeof(bad));
    bad[0] = 'X';                                             // not our magic
    CHECK(!decodePeerHello(bad, kPeerHelloSize, &out));

    std::memcpy(bad, frame, sizeof(bad));
    bad[4] = kPeerProtoVersion + 1;                           // a newer peer
    CHECK(!decodePeerHello(bad, kPeerHelloSize, &out));

    // Too small a buffer must fail the encode rather than write past it.
    uint8_t tiny[4];
    CHECK(encodePeerHello(src, tiny, sizeof(tiny)) == 0);
    CHECK(encodePeerHello(src, nullptr, kPeerHelloSize) == 0);
}

// A HELLO's strings are attacker-controlled and land in the framebuffer and the
// roster file, so decode keeps printable ASCII only. Offending bytes are dropped
// (not frame-rejected), so a mangled name still yields a usable shorter row — and
// an unterminated wire field can never escape as an unterminated C string.
void test_peer_hello_sanitizes_hostile_fields() {
    uint8_t frame[kPeerHelloSize];
    PeerHello src;
    std::strncpy(src.tag, "OK", sizeof(src.tag) - 1);
    CHECK(encodePeerHello(src, frame, sizeof(frame)) == kPeerHelloSize);

    // Overwrite the pet-name slot with control bytes, a high-bit byte, and no
    // terminator at all — the worst a sender could put on the wire.
    const size_t petOff = 9 + kPeerTagCap;
    for (size_t i = 0; i < kPeerPetCap; ++i) frame[petOff + i] = 0xFF;
    frame[petOff + 0] = 'A';
    frame[petOff + 1] = 0x07;   // BEL
    frame[petOff + 2] = 'B';
    frame[petOff + 3] = 0x1B;   // ESC

    PeerHello out;
    CHECK(decodePeerHello(frame, kPeerHelloSize, &out));
    CHECK(std::strcmp(out.petName, "AB") == 0);   // control + high-bit bytes dropped
    CHECK(out.petName[kPeerPetCap - 1] == '\0');  // terminated regardless of the wire
}

// PeerLedger (core/net/peer_ledger.h) — pure logic, no file I/O exercised here
// (native tests don't touch real SD, matching the NetworkLedger tests above). The
// distinction from that ledger is the point: an operator's pet/crew/rank CHANGE
// between meetings, so a repeat sighting refreshes them while the tally accrues.
void test_peer_ledger_new_and_refresh() {
    PeerLedger ledger;
    PeerLedger::Entry e;
    CHECK(!ledger.lookup(0xAABBCCDDEEFFull, &e));

    PeerHello first;
    std::strncpy(first.tag, "GHOSTBYTE", sizeof(first.tag) - 1);
    std::strncpy(first.petName, "Paypup", sizeof(first.petName) - 1);
    first.stage = static_cast<uint8_t>(Stage::Process);
    first.rank = 2;

    CHECK(ledger.record(0xAABBCCDDEEFFull, first));            // true == new operator
    CHECK(ledger.size() == 1);
    CHECK(ledger.lookup(0xAABBCCDDEEFFull, &e));
    CHECK(e.timesMet == 1);
    CHECK(std::strcmp(e.petName, "Paypup") == 0);
    CHECK(e.crewName[0] == '\0');

    // Met again, later: same operator, but they've evolved their pet, enlisted,
    // and ranked up. The row must show who they are NOW.
    PeerHello later;
    std::strncpy(later.tag, "GHOSTBYTE", sizeof(later.tag) - 1);
    std::strncpy(later.petName, "Malbear", sizeof(later.petName) - 1);
    std::strncpy(later.crewName, "Deniers of Service", sizeof(later.crewName) - 1);
    later.stage = static_cast<uint8_t>(Stage::Script);
    later.rank = 5;
    later.crewRed = false;

    CHECK(!ledger.record(0xAABBCCDDEEFFull, later));           // false == already known
    CHECK(ledger.size() == 1);                                 // refreshed, not appended
    CHECK(ledger.lookup(0xAABBCCDDEEFFull, &e));
    CHECK(e.timesMet == 2);
    CHECK(std::strcmp(e.petName, "Malbear") == 0);
    CHECK(std::strcmp(e.crewName, "Deniers of Service") == 0);
    CHECK(e.stage == static_cast<uint8_t>(Stage::Script));
    CHECK(e.rank == 5);

    // A different radio MAC is a different operator even with an identical tag —
    // the key is the identity, the tag is just what they call themselves.
    CHECK(ledger.record(0x112233445566ull, later));
    CHECK(ledger.size() == 2);
}

// inTopN backs the "home turf vs. occasional" split resolveNetworkDiscovery
// uses: fewer than N OTHER entries with a strictly higher count qualifies
// (ties all qualify — with few entries total, everything reads as "top N"
// until enough variety exists to actually rank below others).
void test_network_ledger_in_top_n() {
    NetworkLedger ledger;
    for (uint64_t k = 1; k <= 8; ++k) {
        ledger.recordNew(k, "N");
        for (int i = 0; i < 4; ++i) ledger.recordSeenAgain(k, "N");   // count 5
    }
    ledger.recordNew(9, "Rare");                                      // count 1
    for (uint64_t k = 1; k <= 8; ++k) CHECK(ledger.inTopN(k, 8));
    CHECK(!ledger.inTopN(9, 8));      // the 9th, lower-count network isn't top-8
    CHECK(!ledger.inTopN(999, 8));    // unknown key -> false
}

// A repeat sighting outside the top-8 by count (an "occasional" network — your
// town, not your daily-driver routers) still feeds the pet: pet combat XP + the
// same small Happiness bump a new network gets, no Hacker Rank XP. A repeat
// INSIDE the top-8 ("home turf") stays flat — no XP, no Happiness change.
void test_network_discovery_repeat_familiar_vs_home_turf() {
    Game g{StartMode::Hatched};
    uint8_t common[8][6];
    for (int i = 0; i < 8; ++i) {
        common[i][0] = 0x70; common[i][1] = 0; common[i][2] = 0; common[i][3] = 0;
        common[i][4] = 0; common[i][5] = static_cast<uint8_t>(i);
        walkAndCreditNetwork(g, common[i], "Common");       // count 1 each
    }
    uint8_t rare[6] = {0x71, 0, 0, 0, 0, 0x01};
    walkAndCreditNetwork(g, rare, "Rare");                  // count 1, new -> credited
    CHECK(g.networksSeen() == 9);

    // Bump each of the 8 "common" networks to count 2 — a repeat while every
    // count is still tied at 1 reads as home-turf (see test_network_ledger_in_top_n's
    // comment), so this loop itself grants nothing; it's just setup.
    for (int i = 0; i < 8; ++i) walkAndCreditNetwork(g, common[i], "Common");
    CHECK(g.networksSeen() == 9);   // still just the 9 originals — no new credit

    // Now `rare` (count 1) sits strictly below all 8 "common" networks (count 2) —
    // a repeat resolves it as familiar-but-occasional: pet XP + Happiness, no
    // Hacker Rank XP. Verified via netDiscoveryFlavor_ (set exclusively by
    // resolveNetworkDiscovery) rather than combatXp/combatLevel deltas — a
    // walkToWifiEvent search can incidentally pass through an UNRELATED sinkholed
    // wild encounter (walkToWifiEvent stocks Sinkhole Traps so any Wild roll
    // along the way auto-resolves via resolveSinkhole, which grants its OWN
    // combat XP), so a raw XP delta can't reliably isolate MY grant across a
    // real walk. Same reasoning for hackerRank/networksSeen — those two ARE
    // exclusively mine (nothing else touches them), so they stay precise checks.
    const int rank0 = g.hackerRank();
    queueAndReachWifiEvent(g, rare, "Rare");
    CHECK(g.networksSeen() == 9);                 // no Hacker Rank credit
    CHECK(g.hackerRank() == rank0);
    CHECK(std::strstr(g.netDiscoveryFlavor(), "FONDLY REMEMBERS") != nullptr);
    resolveWifiEventToIdle(g);

    // A repeat of one of the "common" (home-turf) networks stays flat: the
    // flavor reads "TIRED OF", not "FONDLY REMEMBERS" — no pet-XP branch taken,
    // no further Hacker Rank movement.
    queueAndReachWifiEvent(g, common[0], "Common");
    CHECK(std::strstr(g.netDiscoveryFlavor(), "TIRED OF") != nullptr);
    CHECK(g.hackerRank() == rank0);
    resolveWifiEventToIdle(g);
}

// A dry sighting queue costs the pet NOTHING, and on the cadence beat it summons the
// area's guardian instead (game_net.cpp routes it, game_shibboleth.cpp runs it).
//
// This is the inverse of what this seam used to do. Walking a dead zone used to take
// kNetDiscoveryNoneHappyPenalty off Happiness on the 1st miss and every Nth after, which
// on a ~37s Wi-Fi cadence ground a ten-minute unmonitored walk down by ~30 for the crime
// of being somewhere with no new networks. The streak is still counted on exactly the
// same rhythm — it is what paces the guardian — so the beat that used to sting is the
// beat that puts something in front of the pet.
void test_network_discovery_empty_queue_costs_nothing_and_summons_a_guardian() {
    Game g{StartMode::Hatched};
    g.model().setHappiness(80);   // headroom: a penalty would be visible rather than
                                   // clamped, so its ABSENCE is a real observation
    g.inventory().add("sinkhole_trap", 20);   // bypass any Wild roll along the way, free
    enterWalk(g);   // ONCE — walkToWifiEvent re-enters via enterWalk on every call, which
                     // re-arms the sub-area and resets emptyQueueStreak_
                     // (game_explore.cpp's startExplore), defeating the whole point of
                     // this test (observing the streak persist ACROSS misses).
                     // Step with pingExplore instead, staying in the same session.
    bool sawGuardian = false;
    int happyDrops = 0;
    const int before = g.model().happiness();
    for (int i = 0; i < 400; ++i) {
        if (g.nav() == Game::Nav::Shibboleth) {
            // The guardian, standing there because nothing was queued. Answer it and
            // move on — which of the three replies is right is not this test's business.
            sawGuardian = true;
            g.onButton(press(Button::B));
        } else if (g.nav() == Game::Nav::Wifi) {
            // A dry Wi-Fi beat BETWEEN guardians. Its sub-outcome still pays out, so the
            // only thing asserted here is that nothing was taken for the empty queue.
            if (g.model().happiness() < before) ++happyDrops;
            g.onButton(press(Button::B));
        } else if (g.nav() == Game::Nav::Encounter) {
            g.onButton(press(Button::A));   // Fight -> Flee
            g.onButton(press(Button::A));   // Flee -> Sinkhole
            g.onButton(press(Button::B));   // confirm -> back to idle
        } else if (g.nav() == Game::Nav::Shop || g.nav() == Game::Nav::ModShop) {
            tapC(g);   // leave the shop -> back to idle
        } else if (g.nav() == Game::Nav::Combat) {
            uint32_t t = 0;
            for (int j = 0; j < 800 &&
                            g.combat().outcome() == Combat::Outcome::Ongoing; ++j)
                g.tick(t += kHeartbeatMs);
            g.onButton(press(Button::B));
        } else if (g.nav() == Game::Nav::PostEncounter) {
            g.onButton(press(Button::B));
        } else if (g.nav() == Game::Nav::Idle) {
            if (!g.exploreActive()) break;   // a lost fight ended the walk — done looking
            pingExplore(g);
        } else {
            g.onButton(press(Button::B));
        }
        if (sawGuardian) break;
    }
    CHECK(sawGuardian);        // a dry queue DOES reach the Cant
    CHECK(happyDrops == 0);    // ...and never charged the pet for the drought
}

// The runtime Audit-scan toggle persists across a reboot (save
// v5) — an authorized-use choice must survive, and default OFF must be honored.
void test_audit_scan_toggle_persists() {
    MemSaveStore store;
    {
        Game g(StartMode::Hatched, "paypup", &store);
        CHECK(!g.netScanEnabled());          // default OFF (authorized-use opt-in)
        g.setNetScanEnabled(true);
        g.tick(kSaveAutosaveMs + kHeartbeatMs);   // flush the debounced write
    }
    {
        Game g(StartMode::Hatched, "paypup", &store);
        CHECK(g.netScanEnabled());           // survived the reboot
    }
}

// Audit handshake capture —.pcap writer + capture policy SM -------

// A memory PcapSink: the native seam standing in for the device's SD FILE. Every
// byte the writer emits accumulates here so a gate can assert the exact stream.
class MemPcapSink : public PcapSink {
public:
    bool write(const uint8_t* data, size_t n) override {
        bytes_.insert(bytes_.end(), data, data + n);
        return true;
    }
    const std::vector<uint8_t>& bytes() const { return bytes_; }
private:
    std::vector<uint8_t> bytes_;
};

// Little-endian u32 read helper for asserting header fields.
static uint32_t leU32(const std::vector<uint8_t>& b, size_t off) {
    return static_cast<uint32_t>(b[off]) | (static_cast<uint32_t>(b[off + 1]) << 8) |
           (static_cast<uint32_t>(b[off + 2]) << 16) |
           (static_cast<uint32_t>(b[off + 3]) << 24);
}

// The 24-byte global header is byte-exact classic libpcap (magic a1b2c3d4,
// version 2.4, snaplen, DLT_IEEE802_11=105). A reader keys off these exactly.
void test_pcap_global_header() {
    uint8_t h[kPcapGlobalHeaderLen];
    writePcapGlobalHeader(h);
    // Magic bytes are little-endian a1b2c3d4 -> d4 c3 b2 a1 on disk.
    CHECK(h[0] == 0xd4 && h[1] == 0xc3 && h[2] == 0xb2 && h[3] == 0xa1);
    CHECK(h[4] == 2 && h[5] == 0);           // version_major = 2
    CHECK(h[6] == 4 && h[7] == 0);           // version_minor = 4
    std::vector<uint8_t> v(h, h + sizeof(h));
    CHECK(leU32(v, 8) == 0);                 // thiszone
    CHECK(leU32(v, 12) == 0);                // sigfigs
    CHECK(leU32(v, 16) == kPcapSnaplen);     // snaplen
    CHECK(leU32(v, 20) == kPcapLinkTypeIEEE80211);  // 105
}

// The writer streams a valid file: one global header, then a record header +
// frame per writeFrame, with the byte/frame counters and snaplen truncation
// (incl_len capped, orig_len = true length) all correct.
void test_pcap_writer_stream() {
    MemPcapSink sink;
    PcapWriter w;
    CHECK(!w.begun());
    CHECK(w.begin(sink));
    CHECK(w.begun());
    CHECK(w.begin(sink));                    // idempotent: no second header
    CHECK(sink.bytes().size() == static_cast<size_t>(kPcapGlobalHeaderLen));

    // A small EAPOL-sized frame writes whole (incl == orig).
    const uint8_t frame[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    CHECK(w.writeFrame(sink, frame, sizeof(frame), 0x11223344u, 500u));
    CHECK(w.frames() == 1);
    const size_t recOff = kPcapGlobalHeaderLen;
    const auto& b = sink.bytes();
    CHECK(b.size() == static_cast<size_t>(kPcapGlobalHeaderLen + kPcapRecordHeaderLen + 8));
    CHECK(leU32(b, recOff + 0) == 0x11223344u);   // ts_sec
    CHECK(leU32(b, recOff + 4) == 500u);          // ts_usec
    CHECK(leU32(b, recOff + 8) == 8u);            // incl_len
    CHECK(leU32(b, recOff + 12) == 8u);           // orig_len
    CHECK(b[recOff + 16] == 1 && b[recOff + 16 + 7] == 8);  // frame payload

    // An oversized frame is truncated to snaplen in the file but records its true
    // length in orig_len (standard libpcap semantics).
    std::vector<uint8_t> big(kPcapSnaplen + 100, 0xAB);
    const size_t before = sink.bytes().size();
    CHECK(w.writeFrame(sink, big.data(), static_cast<uint32_t>(big.size()), 1u, 2u));
    CHECK(w.frames() == 2);
    const size_t rec2 = before;
    CHECK(leU32(sink.bytes(), rec2 + 8) == kPcapSnaplen);           // incl capped
    CHECK(leU32(sink.bytes(), rec2 + 12) == kPcapSnaplen + 100u);   // orig true
    CHECK(sink.bytes().size() == before + kPcapRecordHeaderLen + kPcapSnaplen);

    CHECK(w.bytesWritten() == sink.bytes().size());
}

// writeFrame before begin() must fail without emitting or counting (the global
// header is a hard precondition for a valid file).
void test_pcap_writer_requires_begin() {
    MemPcapSink sink;
    PcapWriter w;
    const uint8_t f[4] = {9, 9, 9, 9};
    CHECK(!w.writeFrame(sink, f, sizeof(f), 0, 0));
    CHECK(w.frames() == 0);
    CHECK(sink.bytes().empty());
}

// The capture policy SM: arm -> capture -> hot broadcast ->
// seal -> re-arm cooldown -> auto re-arm. Pure, driven by a fake game-ms clock.
void test_audit_capture_state_machine() {
    AuditCapture ac;
    CHECK(ac.phase() == AuditPhase::Disarmed);
    CHECK(!ac.enabled());
    CHECK(!ac.capturing());

    // Turning the toggle on arms immediately (never sealed -> canArm true).
    ac.setEnabled(true, 0);
    CHECK(ac.enabled());
    CHECK(ac.phase() == AuditPhase::Armed);
    CHECK(ac.capturing());
    CHECK(!ac.broadcasting());

    // A handshake capture enters the hot-broadcast window.
    ac.noteHandshake(1000);
    CHECK(ac.phase() == AuditPhase::Hot);
    CHECK(ac.broadcasting());
    CHECK(ac.hotRemainingMs(1000) == kAuditHotBroadcastMs);

    // Mid-window: still hot, still broadcasting.
    ac.tick(1000 + kAuditHotBroadcastMs / 2);
    CHECK(ac.phase() == AuditPhase::Hot);

    // The window expires -> seal -> cooldown (broadcast stops).
    const uint32_t sealAt = 1000 + kAuditHotBroadcastMs;
    ac.tick(sealAt);
    CHECK(ac.phase() == AuditPhase::Cooldown);
    CHECK(!ac.broadcasting());
    CHECK(!ac.canArm(sealAt));
    CHECK(ac.cooldownRemainingMs(sealAt) == kAuditRearmCooldownMs);

    // Cooling: still can't arm; a handshake is ignored.
    ac.noteHandshake(sealAt + 5000);
    CHECK(ac.phase() == AuditPhase::Cooldown);

    // Cooldown elapses with the toggle still on -> auto re-arm.
    const uint32_t rearmAt = sealAt + kAuditRearmCooldownMs;
    CHECK(ac.canArm(rearmAt));
    ac.tick(rearmAt);
    CHECK(ac.phase() == AuditPhase::Armed);
    CHECK(ac.cooldownRemainingMs(rearmAt) == 0);
}

// Turning the toggle OFF seals a live broadcast immediately and starts cooldown;
// after cooldown, with the toggle off, it lands Disarmed (not re-armed). And
// leaving the event (softSeal) stops a hot broadcast without touching the toggle.
void test_audit_capture_seal_paths() {
    AuditCapture ac;
    ac.setEnabled(true, 0);
    ac.noteHandshake(100);
    CHECK(ac.broadcasting());
    // Toggle off mid-broadcast: immediate seal.
    ac.setEnabled(false, 200);
    CHECK(!ac.enabled());
    CHECK(ac.phase() == AuditPhase::Cooldown);
    CHECK(!ac.broadcasting());
    // After cooldown, toggle still off -> Disarmed (no silent re-arm).
    ac.tick(200 + kAuditRearmCooldownMs);
    CHECK(ac.phase() == AuditPhase::Disarmed);

    // softSeal path: leaving the audit event stops a hot broadcast (-> cooldown)
    // but the toggle intent stays on, so it re-arms after the cooldown.
    AuditCapture ac2;
    ac2.setEnabled(true, 0);
    ac2.noteHandshake(10);
    ac2.softSeal(20);
    CHECK(ac2.phase() == AuditPhase::Cooldown);
    CHECK(ac2.enabled());
    ac2.tick(20 + kAuditRearmCooldownMs);
    CHECK(ac2.phase() == AuditPhase::Armed);
}

// The Audit-CAPTURE toggle (save v6) persists across a reboot, default OFF.
void test_audit_capture_toggle_persists() {
    MemSaveStore store;
    {
        Game g(StartMode::Hatched, "paypup", &store);
        CHECK(!g.auditCaptureEnabled());     // default OFF (authorized-use opt-in)
        g.setAuditCaptureEnabled(true);
        g.tick(kSaveAutosaveMs + kHeartbeatMs);   // flush the debounced write
    }
    {
        Game g(StartMode::Hatched, "paypup", &store);
        CHECK(g.auditCaptureEnabled());      // survived the reboot
    }
}

// The escalating AuditMode ties scan + capture into one ordered dial: capture can
// never run without the discovery scan, whichever setter you go through.
void test_audit_mode_enforces_scan_dependency() {
    MemSaveStore store;
    Game g(StartMode::Hatched, "paypup", &store);
    CHECK(g.auditMode() == Game::AuditMode::Off);
    CHECK(!g.netScanEnabled());
    CHECK(!g.auditCaptureEnabled());

    // ScanCapture arms both; Scan keeps scan but disarms capture.
    g.setAuditMode(Game::AuditMode::ScanCapture);
    CHECK(g.auditMode() == Game::AuditMode::ScanCapture);
    CHECK(g.netScanEnabled() && g.auditCaptureEnabled());
    g.setAuditMode(Game::AuditMode::Scan);
    CHECK(g.netScanEnabled() && !g.auditCaptureEnabled());

    // Turning scan OFF via the low-level setter also disarms capture (invariant).
    g.setAuditMode(Game::AuditMode::ScanCapture);
    g.setNetScanEnabled(false);
    CHECK(!g.netScanEnabled() && !g.auditCaptureEnabled());
    CHECK(g.auditMode() == Game::AuditMode::Off);

    // Enabling capture via the low-level setter pulls scan up to match.
    g.setAuditCaptureEnabled(true);
    CHECK(g.netScanEnabled() && g.auditCaptureEnabled());
    CHECK(g.auditMode() == Game::AuditMode::ScanCapture);
}

// A legacy save from before the single control could hold capture-ON with
// scan-OFF (the two toggles were independent). On load, scan is pulled up so the
// invariant holds — capture never runs without its discovery feed.
void test_audit_legacy_capture_without_scan_normalizes_on_load() {
    SaveData a;
    std::strcpy(a.activeId, "paypup");
    a.netScanEnabled = 0;
    a.auditCaptureEnabled = 1;               // the inconsistent legacy state
    MemSaveStore store;
    store.save(serializeSave(a));
    Game g(StartMode::Hatched, "paypup", &store);
    CHECK(g.netScanEnabled());               // pulled up to match capture
    CHECK(g.auditCaptureEnabled());
    CHECK(g.auditMode() == Game::AuditMode::ScanCapture);
}

// RF half — pure EAPOL classifier + handshake tracker -------------

// Build a raw 802.11 EAPOL-Key MPDU (no radiotap — the DLT_IEEE802_11 bytes the
// promiscuous callback hands us). fromDs=true models an AP->STA message (M1/M3,
// BSSID in addr2); fromDs=false a STA->AP message (M2/M4, ToDS, BSSID in addr1).
// Returns the frame length. Layout: 24B MAC hdr + 8B LLC/SNAP + 4B 802.1X hdr +
// EAPOL-Key body (descriptor type + 2B Key Information + a little tail).
static uint32_t buildEapolKey(uint8_t out[64], bool fromDs, const uint8_t bssid[6],
                              uint16_t keyInfo) {
    std::memset(out, 0, 64);
    out[0] = 0x08;                       // data frame, subtype 0
    out[1] = fromDs ? 0x02 : 0x01;       // FromDS (AP->STA) or ToDS (STA->AP)
    // Addresses: put the BSSID in the field the DS bits select.
    std::memcpy(out + (fromDs ? 10 : 4), bssid, 6);
    uint8_t* llc = out + 24;
    const uint8_t snap[8] = {0xAA, 0xAA, 0x03, 0x00, 0x00, 0x00, 0x88, 0x8E};
    std::memcpy(llc, snap, 8);
    uint8_t* eapol = llc + 8;
    eapol[0] = 0x02;                     // 802.1X version
    eapol[1] = 0x03;                     // packet type = EAPOL-Key
    eapol[2] = 0x00; eapol[3] = 0x5f;    // body length (illustrative)
    uint8_t* body = eapol + 4;
    body[0] = 0x02;                      // descriptor type = RSN
    body[1] = static_cast<uint8_t>(keyInfo >> 8);   // Key Information (big-endian)
    body[2] = static_cast<uint8_t>(keyInfo & 0xFF);
    return 24 + 8 + 4 + 3 + 8;           // + a small key-data tail
}

// Key Information flag combos for each 4-way message (pairwise 0x08 + version 2).
static constexpr uint16_t kKiM1 = 0x0088;  // pairwise + Ack
static constexpr uint16_t kKiM2 = 0x0108;  // pairwise + MIC
static constexpr uint16_t kKiM3 = 0x03c8;  // pairwise + Install + Ack + MIC + Secure
static constexpr uint16_t kKiM4 = 0x0308;  // pairwise + MIC + Secure

// parseEapolKey classifies each 4-way message and pulls the right BSSID out of
// the address field the DS bits select; non-EAPOL / malformed frames are inert.
void test_eapol_parse_classifies_messages() {
    uint8_t buf[64];
    const uint8_t ap[6] = {0x02, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE};

    auto m1 = parseEapolKey(buf, buildEapolKey(buf, true, ap, kKiM1));
    CHECK(m1.isEapolKey && m1.msg == 1);
    CHECK(std::memcmp(m1.bssid, ap, 6) == 0);          // FromDS -> addr2

    auto m2 = parseEapolKey(buf, buildEapolKey(buf, false, ap, kKiM2));
    CHECK(m2.isEapolKey && m2.msg == 2);
    CHECK(std::memcmp(m2.bssid, ap, 6) == 0);          // ToDS -> addr1

    auto m3 = parseEapolKey(buf, buildEapolKey(buf, true, ap, kKiM3));
    CHECK(m3.isEapolKey && m3.msg == 3);

    auto m4 = parseEapolKey(buf, buildEapolKey(buf, false, ap, kKiM4));
    CHECK(m4.isEapolKey && m4.msg == 4);

    // A group-rekey EAPOL-Key (no pairwise bit) parses but isn't a 4-way msg.
    auto grp = parseEapolKey(buf, buildEapolKey(buf, true, ap, 0x0380));  // no 0x08
    CHECK(grp.isEapolKey && grp.msg == 0);
}

// Frames that must NOT classify as EAPOL: a non-EAPOL ethertype, a management
// frame, and a buffer truncated before the EAPOL header. None may overrun `len`.
void test_eapol_parse_rejects_non_eapol() {
    uint8_t buf[64];
    const uint8_t ap[6] = {0x02, 0, 0, 0, 0, 1};
    // Valid EAPOL frame, then stomp the ethertype to IPv4 (0x0800).
    uint32_t n = buildEapolKey(buf, true, ap, kKiM1);
    buf[24 + 6] = 0x08; buf[24 + 7] = 0x00;
    CHECK(!parseEapolKey(buf, n).isEapolKey);

    // Management frame (type 0) is never EAPOL.
    n = buildEapolKey(buf, true, ap, kKiM1);
    buf[0] = 0x00;                        // type/subtype -> mgmt beacon-ish
    CHECK(!parseEapolKey(buf, n).isEapolKey);

    // Truncated to the MAC header only: no LLC/EAPOL to read.
    CHECK(!parseEapolKey(buf, 24).isEapolKey);
    // Empty / null are safe.
    CHECK(!parseEapolKey(buf, 0).isEapolKey);
    CHECK(!parseEapolKey(nullptr, 64).isEapolKey);
}

// HandshakeTracker fires exactly once per BSSID, on the transition where the
// capture first becomes crackable (M2 + a neighbour), and never for a lone
// message or a repeat. Distinct APs are tracked independently.
void test_handshake_tracker_first_crackable() {
    HandshakeTracker t;
    const uint8_t a[6] = {0x02, 0, 0, 0, 0, 0xA1};
    const uint8_t b[6] = {0x02, 0, 0, 0, 0, 0xB2};

    CHECK(!t.observe(a, 1));              // M1 alone: not yet crackable
    CHECK(t.observe(a, 2));               // M1+M2: first crackable -> true
    CHECK(!t.observe(a, 3));              // already credited -> never again
    CHECK(!t.observe(a, 2));

    // A different AP needs its own pair; M2 alone isn't enough.
    CHECK(!t.observe(b, 2));
    CHECK(t.observe(b, 3));               // M2+M3 also crackable
    CHECK(t.trackedAps() == 2);

    // A msg of 0 (non-4-way) is inert.
    CHECK(!t.observe(a, 0));

    t.reset();
    CHECK(t.trackedAps() == 0);
    CHECK(!t.observe(a, 2));              // fresh state: M2 alone not crackable
    CHECK(t.observe(a, 1));               // now crackable again
}

// Bounded arming (RF power budget): an Armed session that never captures
// self-seals after kAuditArmWindowMs down the normal seal->cooldown path, and
// re-arms afterwards because the toggle intent is untouched (a listen/sleep duty
// cycle instead of an open-ended radio drain).
void test_audit_capture_arm_window_self_seals() {
    AuditCapture ac;
    ac.setEnabled(true, 0);
    CHECK(ac.phase() == AuditPhase::Armed);
    ac.tick(kAuditArmWindowMs / 2);           // mid-window: still listening
    CHECK(ac.phase() == AuditPhase::Armed);
    ac.tick(kAuditArmWindowMs);                // window elapsed -> seal
    CHECK(ac.phase() == AuditPhase::Cooldown);
    CHECK(ac.enabled());                       // toggle intent preserved
    // Re-arms once the cooldown clears (toggle still on) -> a NEW listen window.
    const uint32_t rearm = kAuditArmWindowMs + kAuditRearmCooldownMs;
    ac.tick(rearm);
    CHECK(ac.phase() == AuditPhase::Armed);
    ac.tick(rearm + kAuditArmWindowMs - 1);    // window measured from the re-arm
    CHECK(ac.phase() == AuditPhase::Armed);
    ac.tick(rearm + kAuditArmWindowMs);
    CHECK(ac.phase() == AuditPhase::Cooldown);
}

// The low-battery guard seam: sealActive() seals from EITHER Armed or Hot while
// keeping the toggle intent (unlike toggle-off), so a pack that recovers re-arms.
void test_audit_capture_seal_active_from_armed() {
    AuditCapture ac;
    ac.setEnabled(true, 0);
    CHECK(ac.phase() == AuditPhase::Armed);
    ac.sealActive(500);                        // battery dipped while merely armed
    CHECK(ac.phase() == AuditPhase::Cooldown);
    CHECK(ac.enabled());                       // intent kept -> re-arms after cooldown
    ac.tick(500 + kAuditRearmCooldownMs);
    CHECK(ac.phase() == AuditPhase::Armed);

    // Also seals a Hot broadcast; a no-op when already disarmed/cooling.
    AuditCapture ac2;
    ac2.sealActive(0);                         // Disarmed -> nothing to seal
    CHECK(ac2.phase() == AuditPhase::Disarmed);
    ac2.setEnabled(true, 0);
    ac2.noteHandshake(100);
    CHECK(ac2.broadcasting());
    ac2.sealActive(200);
    CHECK(ac2.phase() == AuditPhase::Cooldown && !ac2.broadcasting());
}

// registerHandshake is the device-capture seam: SHAKES counts DISTINCT
// handshakes, so one bump per new BSSID and none for a repeat (walking past a
// known AP again). Mirrors registerNetwork's dedup, on a separate ledger.
void test_register_handshake_dedup() {
    Game g{StartMode::Hatched};
    CHECK(g.handshakesSeen() == 0);

    uint8_t a[6]  = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01};
    uint8_t a2[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x02};
    CHECK(g.registerHandshake(a));       // new AP -> credited
    CHECK(g.handshakesSeen() == 1);
    CHECK(!g.registerHandshake(a));      // same AP -> not re-credited
    CHECK(g.handshakesSeen() == 1);
    CHECK(g.registerHandshake(a2));      // different AP -> credited
    CHECK(g.handshakesSeen() == 2);

    // The handshake ledger is independent of the NETS ledger.
    CHECK(g.networksSeen() == 0);
}

// The core "leave the house and come home" requirement, for SHAKES: the
// handshake dedup ledger PERSISTS (save v7), so a reboot never re-credits a
// handshake already captured. (NETS/registerNetwork no longer has an equivalent
// save-blob guarantee — real-network dedup/history moved to the SD-backed
// NetworkLedger, which is file-backed, not save-blob-backed; registerNetwork's
// pending queue is deliberately ephemeral, see game.h's comment on it. Native
// tests don't exercise real SD file I/O — see forgeLegacyNetworkBytes's sibling
// note and net_capture.h's own documented host/device testing boundary.)
void test_audit_ledgers_persist_no_recredit() {
    MemSaveStore store;
    uint8_t shake[6] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x22};
    {
        Game g(StartMode::Hatched, "paypup", &store);
        g.setAuditCaptureEnabled(true);
        CHECK(g.registerHandshake(shake));
        CHECK(g.handshakesSeen() == 1);
        g.tick(kSaveAutosaveMs + kHeartbeatMs);   // flush the debounced write
    }
    {
        Game g(StartMode::Hatched, "paypup", &store);
        // The counter survived the reboot...
        CHECK(g.handshakesSeen() == 1);
        // ...and the dedup ledger came back with it: coming home past the same
        // AP grants NO new credit.
        CHECK(!g.registerHandshake(shake));
        CHECK(g.handshakesSeen() == 1);
    }
}

// v7 round-trip: the SHAKES ledger + handshake count serialize and restore
// byte-exact (the persistence contract behind test_audit_ledgers_persist).
void test_save_v7_ledger_roundtrip() {
    SaveData a;
    std::strcpy(a.activeId, "paypup");
    a.handshakesSeen = 3;
    a.seenHandshakeBssids = {0xABCDEFULL, 0x010203040506ULL};
    auto blob = serializeSave(a);

    SaveData out;
    CHECK(deserializeSave(blob, out));
    CHECK(out.handshakesSeen == 3);
    CHECK(out.seenHandshakeBssids == a.seenHandshakeBssids);
}

// pcap-blowup fix: filename derivation + discovery rewards --------

// buildPcapFilename (pcap_naming.h) is the pure piece net_capture.h uses to name
// a session after the network it captured, instead of a sequential audit_N
// counter (the counter scheme is what minted a fresh file every arm/seal radio
// cycle regardless of whether anything new was heard). A sanitized SSID is
// preferred; the promiscuous EAPOL-only capture path never actually sees one
// (DATA frames only, no beacons), so v1 always falls back to the BSSID hex —
// exercise both paths since the helper supports both.
void test_pcap_naming_sanitizes_and_falls_back_to_bssid() {
    char name[24];
    // No SSID -> BSSID hex fallback, deterministic + collision-free per network.
    CHECK(sanitizeForFilename(nullptr, name, sizeof(name), 16) == 0);
    CHECK(sanitizeForFilename("", name, sizeof(name), 16) == 0);

    uint8_t bssid[6] = {0xAA, 0xBB, 0xCC, 0x00, 0x11, 0x22};
    char path[48];
    buildPcapFilename(path, sizeof(path), "/sdcard/audit_", bssid, nullptr);
    CHECK(std::strcmp(path, "/sdcard/audit_aabbcc001122.pcap") == 0);

    // A different BSSID -> a different filename (the whole point: named by
    // network, not a counter, so distinct networks never collide/overwrite).
    uint8_t bssid2[6] = {0xAA, 0xBB, 0xCC, 0x00, 0x11, 0x23};
    char path2[48];
    buildPcapFilename(path2, sizeof(path2), "/sdcard/audit_", bssid2, nullptr);
    CHECK(std::strcmp(path2, "/sdcard/audit_aabbcc001123.pcap") == 0);
    CHECK(std::strcmp(path, path2) != 0);

    // Sanitized SSID path: named after the network, suffixed with the last 3
    // BSSID octets so two APs sharing an SSID stay on distinct files.
    char ssidName[24];
    CHECK(sanitizeForFilename("My Home AP!", ssidName, sizeof(ssidName), 16) > 0);
    CHECK(std::strcmp(ssidName, "My_Home_AP_") == 0);
    char ssidPath[48];
    buildPcapFilename(ssidPath, sizeof(ssidPath), "/sdcard/audit_", bssid, "My Home AP!");
    CHECK(std::strcmp(ssidPath, "/sdcard/audit_My_Home_AP__001122.pcap") == 0);

    // Same SSID, different BSSID -> different file (the suffix disambiguates).
    char ssidPath2[48];
    buildPcapFilename(ssidPath2, sizeof(ssidPath2), "/sdcard/audit_", bssid2, "My Home AP!");
    CHECK(std::strcmp(ssidPath2, "/sdcard/audit_My_Home_AP__001123.pcap") == 0);
    CHECK(std::strcmp(ssidPath, ssidPath2) != 0);

    // A too-long SSID truncates rather than overflowing the output buffer.
    char longName[8];
    const size_t n = sanitizeForFilename("ThisIsWayTooLongForTheBuffer", longName,
                                         sizeof(longName), 16);
    CHECK(n == sizeof(longName) - 1);      // clamped by outCap, not maxLen
    CHECK(std::strlen(longName) == n);
}

// Game::handshakeAlreadyCaptured is the read-only query net_capture.h checks
// BEFORE opening a file for a BSSID — it must mirror registerHandshake's dedup
// ledger exactly (same networks report captured/not-captured) and never mutate
// state (repeated queries don't grant credit or rewards on their own).
void test_handshake_already_captured_query() {
    Game g{StartMode::Hatched};
    uint8_t a[6] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    uint8_t b[6] = {0x11, 0x12, 0x13, 0x14, 0x15, 0x16};

    CHECK(!g.handshakeAlreadyCaptured(a));
    CHECK(!g.handshakeAlreadyCaptured(b));
    CHECK(g.handshakesSeen() == 0);

    CHECK(g.registerHandshake(a));             // credits `a`
    CHECK(g.handshakeAlreadyCaptured(a));       // now reports captured
    CHECK(!g.handshakeAlreadyCaptured(b));      // `b` still not captured

    // Querying is side-effect-free: repeating it doesn't change SHAKES or the
    // inventory (no reward re-fires from a query alone).
    const int shakes = g.handshakesSeen();
    const int cacheBefore = g.inventory().count("sealed_cache_uncommon") +
                            g.inventory().count("sealed_cache_rare") +
                            g.inventory().count("sealed_cache_epic");
    for (int i = 0; i < 5; ++i) CHECK(g.handshakeAlreadyCaptured(a));
    CHECK(g.handshakesSeen() == shakes);
    CHECK(g.inventory().count("sealed_cache_uncommon") +
          g.inventory().count("sealed_cache_rare") +
          g.inventory().count("sealed_cache_epic") == cacheBefore);
}

// A genuinely new network reward fires EXACTLY ONCE per BSSID (the ledger's
// new-vs-repeat split gates it, resolveNetworkDiscovery), never on a repeat
// sighting. Verified via netDiscoveryFlavor_ (set ONLY by resolveNetworkDiscovery,
// so it's immune to a confound every other numeric signal here has: a
// walkToWifiEvent search can incidentally pass through OTHER explore steps
// that grant the SAME item ids — a "Sealed Cache find" EXPL event
// (resolveCacheEvent, unrelated to network discovery) also drops
// sealed_cache_common/uncommon via its own weighted roll, so a raw inventory
// count can't reliably isolate MY reward across a real walk. The cache-count
// check below is intentionally a lower bound (>=), not exact, for the same
// reason — it still catches a reward that never fires at all.
void test_network_discovery_reward_fires_once_per_network() {
    Game g{StartMode::Hatched};
    auto cacheTotal = [&]() {
        return g.inventory().count("sealed_cache_common") +
               g.inventory().count("sealed_cache_uncommon");
    };
    CHECK(cacheTotal() == 0);

    uint8_t a[6] = {0x40, 0, 0, 0, 0, 0x01};
    queueAndReachWifiEvent(g, a, "A");
    CHECK(std::strstr(g.netDiscoveryFlavor(), "ADMIRING") ||
          std::strstr(g.netDiscoveryFlavor(), "ENVIES"));   // genuinely-new flavor
    CHECK(cacheTotal() >= 1);                                // reward granted
    resolveWifiEventToIdle(g);

    // Repeat sightings of the same BSSID resolve as familiar/home-turf (only
    // one entry exists so far, so it trivially reads as "top favorites" —
    // see test_network_ledger_in_top_n's comment) — never a second NEW-network
    // reward, confirmed by the flavor never reading as genuinely-new again.
    for (int i = 0; i < 3; ++i) {
        queueAndReachWifiEvent(g, a, "A");
        CHECK(std::strstr(g.netDiscoveryFlavor(), "TIRED OF") != nullptr);
        resolveWifiEventToIdle(g);
    }

    uint8_t b[6] = {0x40, 0, 0, 0, 0, 0x02};
    const int cacheBeforeB = cacheTotal();
    queueAndReachWifiEvent(g, b, "B");
    CHECK(std::strstr(g.netDiscoveryFlavor(), "ADMIRING") ||
          std::strstr(g.netDiscoveryFlavor(), "ENVIES"));   // a second, DIFFERENT network
    CHECK(cacheTotal() >= cacheBeforeB + 1);
    resolveWifiEventToIdle(g);
}

// The network-discovery reward rolls 70% common / 30% uncommon
// (kNetDiscoveryCacheCommonPct/…UncommonPct) — both tiers must actually be
// reachable (catches a wiring bug that always picks one tier). A precise
// percentage check isn't reliable here: a walkToWifiEvent search can
// incidentally pass through the unrelated "Sealed Cache find" EXPL event
// (resolveCacheEvent), which grants the SAME sealed_cache_common/uncommon ids
// via its own weighted roll, skewing any raw inventory-count ratio measured
// across many real walks (see test_network_discovery_reward_fires_once_per_network's
// comment for the same confound). Classify each sample by its flavor line
// instead — set exclusively by resolveNetworkDiscovery, immune to that noise.
void test_network_discovery_reward_rarity_ratio() {
    Game g{StartMode::Hatched};
    const int kSamples = 60;
    int common = 0, uncommon = 0;
    for (int i = 0; i < kSamples; ++i) {
        uint8_t bssid[6] = {0x50, static_cast<uint8_t>(i >> 8), static_cast<uint8_t>(i),
                            0, 0, 0};
        const int commonBefore = g.inventory().count("sealed_cache_common");
        const int uncommonBefore = g.inventory().count("sealed_cache_uncommon");
        queueAndReachWifiEvent(g, bssid, "S");
        CHECK(std::strstr(g.netDiscoveryFlavor(), "ADMIRING") ||
              std::strstr(g.netDiscoveryFlavor(), "ENVIES"));
        if (g.inventory().count("sealed_cache_common") > commonBefore) ++common;
        else if (g.inventory().count("sealed_cache_uncommon") > uncommonBefore) ++uncommon;
        resolveWifiEventToIdle(g);
    }
    CHECK(common > 0 && uncommon > 0);   // both tiers reachable
    CHECK(common + uncommon == kSamples);
}

// A genuinely new CAPTURED HANDSHAKE reward fires exactly once per BSSID, mirrors
// the network-discovery test above but on registerHandshake()'s ledger (SHAKES).
void test_handshake_capture_reward_fires_once_per_handshake() {
    Game g{StartMode::Hatched};
    auto cacheTotal = [&]() {
        return g.inventory().count("sealed_cache_uncommon") +
               g.inventory().count("sealed_cache_rare") +
               g.inventory().count("sealed_cache_epic");
    };
    CHECK(cacheTotal() == 0);

    uint8_t a[6] = {0x60, 0, 0, 0, 0, 0x01};
    CHECK(g.registerHandshake(a));
    CHECK(cacheTotal() == 1);
    for (int i = 0; i < 10; ++i) CHECK(!g.registerHandshake(a));   // repeats: no-op
    CHECK(cacheTotal() == 1);

    uint8_t b[6] = {0x60, 0, 0, 0, 0, 0x02};
    CHECK(g.registerHandshake(b));
    CHECK(cacheTotal() == 2);

    // Never grants the plain Common tier — a captured handshake is the rarer,
    // harder-earned event than a bare network sighting.
    CHECK(g.inventory().count("sealed_cache_common") == 0);
}

// The handshake-capture reward rolls 50% uncommon / 30% rare / 20% epic
// (kHandshakeCaptureCacheUncommonPct/…RarePct/…EpicPct).
void test_handshake_capture_reward_rarity_ratio() {
    Game g{StartMode::Hatched};
    // Stays comfortably under kBssidDedupCap (256), same reasoning as the
    // network-discovery ratio test above.
    const int kSamples = 240;
    for (int i = 0; i < kSamples; ++i) {
        uint8_t bssid[6] = {0x70, static_cast<uint8_t>(i >> 16),
                            static_cast<uint8_t>(i >> 8), static_cast<uint8_t>(i),
                            0, 0};
        CHECK(g.registerHandshake(bssid));
    }
    const int uncommon = g.inventory().count("sealed_cache_uncommon");
    const int rare = g.inventory().count("sealed_cache_rare");
    const int epic = g.inventory().count("sealed_cache_epic");
    CHECK(uncommon + rare + epic == kSamples);
    const double uncommonPct = 100.0 * uncommon / kSamples;
    const double rarePct = 100.0 * rare / kSamples;
    const double epicPct = 100.0 * epic / kSamples;
    CHECK(uncommonPct > 40.0 && uncommonPct < 60.0);
    CHECK(rarePct > 20.0 && rarePct < 40.0);
    CHECK(epicPct > 10.0 && epicPct < 30.0);
}

// Move drop tables + malbeast roster depth -------
