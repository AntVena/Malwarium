// test_crew_peers.cpp — native gates for the Hacker CREW and the PEERS ledger.
//
// One slice of the native suite; see test_gates.h for the shared fixtures and
// test_main.cpp for the roster that runs these. A save-vNN gate sits with the
// feature whose field it migrates, not in a migrations pile of its own.
#include "test_gates.h"

// --- Hacker CREW: membership, the home-network gate, and the crew Exploit -------

// The crew Exploit's negation charges absorb whole attacks, exactly `magnitude` of
// them, and are spent BEFORE a passive one-shot (RAID Mirror) so the passive stays
// held for afterwards.
void test_crew_exploit_negates_next_hits() {
    ContentRegistry r = ContentRegistry::embedded();
    Combatant p = mkCombatant(r, "P", 100, 5, {"quick_jab"});
    p.mods.arm(ModEffect::RaidMirror);          // a passive one-shot armed alongside
    Combatant e = mkCombatant(r, "E", 5000, 10, {"quick_jab"});   // enemy acts first/often
    Combat cb;
    cb.begin(p, e, Combat::Stakes::Safe, 42, /*forceEnemyFirst=*/true,
             /*carryPlayerHealth=*/-1, /*exploitUses=*/1);

    cb.openOverride({}, CrewExploit{"DENIAL OF SERVICE",
                                    CrewExploitKind::NegateNextHits, 3});
    CHECK(cb.overrideCrewRows() == 1);
    const int crewRow = cb.overrideMoveCount();          // no items in this picker
    while (cb.overridePick() != crewRow) cb.cycleOverride();
    cb.commitOverride();
    CHECK(cb.player().crewExploit.charges == 3);
    CHECK(!cb.overrideReady());                          // firing it spends a use

    const int hp0 = cb.player().health;
    int guard = 0;
    while (cb.player().crewExploit.charges > 0 && guard++ < 400) cb.step();
    CHECK(cb.player().health == hp0);                    // all three bounced
    CHECK(cb.player().mods.armed(ModEffect::RaidMirror));  // ...without burning the passive

    // With the charges gone the mirror takes the NEXT hit, and only after that does
    // damage finally reach Health.
    while (cb.player().mods.armed(ModEffect::RaidMirror) && guard++ < 400) cb.step();
    CHECK(cb.player().health == hp0);
    while (cb.player().health == hp0 && guard++ < 400) cb.step();
    CHECK(cb.player().health < hp0);
}

// Enlisting is gated on holding a home network, and membership cannot outlive it.
void test_crew_requires_home_network() {
    Game g{StartMode::Hatched};
    CHECK(g.crewIndex() == -1 && !g.hasHomeNetwork() && g.activeCrew() == nullptr);

    CHECK(!g.joinCrew(0));                        // no home network -> refused
    CHECK(g.crewIndex() == -1);

    g.setHomeNetwork(0x001122334455ull, "HOME_AP");
    CHECK(g.hasHomeNetwork() && std::strcmp(g.homeNetworkName(), "HOME_AP") == 0);
    CHECK(g.joinCrew(0));
    CHECK(g.crewIndex() == 0);
    CHECK(std::strcmp(g.activeCrew()->id, "deniers_of_service") == 0);
    CHECK(!g.joinCrew(kCrewCount));               // out of range -> refused, no change
    CHECK(g.crewIndex() == 0);

    // Clearing the home network drops the crew with it — a member is always somebody's
    // defender.
    g.setHomeNetwork(0, "");
    CHECK(!g.hasHomeNetwork() && g.crewIndex() == -1);
}

// The CREW screen end-to-end on buttons: designate a home network from the ledger
// the pet built by walking, then enlist. The screen renders in both sub-modes.
void test_crew_screen_pick_home_then_enlist() {
    Game g{StartMode::Hatched};
    const uint8_t bssid[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    walkAndCreditNetwork(g, bssid, "Neighbour");   // one known network in the ledger

    // Explore-mode claims A+C for its control overlay, so stop exploring first —
    // the Hacker face is only reachable from a quiet habitat.
    while (g.nav() != Game::Nav::Idle) g.onButton(press(Button::C));
    // A running walk claims the A+C chord for its control overlay, so the Hacker face
    // below is unreachable until the walk is put down.
    if (g.exploreActive()) stopExplore(g);
    CHECK(!g.exploreActive() && g.nav() == Game::Nav::Idle);
    g.onButton({Button::A, true, true});           // A+C -> hacker face
    CHECK(g.face() == Game::Face::Hacker);
    g.onButton(press(Button::A));
    while (hackerCarouselSlots()[g.cursor()].id != HackerSlotId::Crew)
        g.onButton(press(Button::A));
    CHECK(hackerCarouselSlots()[g.cursor()].accessible);
    g.onButton(press(Button::B));
    CHECK(g.nav() == Game::Nav::Submenu);

    Framebuffer fb(kActiveW, kActiveH);
    g.render(fb);
    CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));

    // Row 0 is HOME NET: B opens the picker, B again takes the focused network.
    g.onButton(press(Button::B));
    g.render(fb);
    CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));
    g.onButton(press(Button::B));
    CHECK(g.hasHomeNetwork());
    CHECK(std::strcmp(g.homeNetworkName(), "Neighbour") == 0);

    // A steps onto the crew row; B enlists, and B again resigns.
    g.onButton(press(Button::A));
    g.onButton(press(Button::B));
    CHECK(g.crewIndex() == 0);
    g.onButton(press(Button::B));
    CHECK(g.crewIndex() == -1);

    g.onButton(press(Button::C));                  // C backs out to the hacker carousel
    CHECK(g.nav() == Game::Nav::Cursor);
}

// The picker answers one question — "which of the networks I can hear RIGHT NOW am I
// claiming?" — so every row is a live in-range sighting. History alone is never an
// offer; the ledger only supplies the walked-in tally that tells the familiar network
// apart from its neighbours' APs.
void test_crew_picker_offers_networks_in_range() {
    Game g{StartMode::Hatched};
    g.debugSeedNetworkLedger(0x00AAAAAAAAAAull, "WALKED_PAST", 7);   // history only

    Game::CrewNetRow rows[kNetVisibleCap];
    CHECK(g.crewNetworkRows(rows, kNetVisibleCap) == 0);   // walked past != in range now

    const uint8_t fresh[6] = {0x00, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB};   // in range, unwalked
    CHECK(g.registerNetwork(fresh, "IN_RANGE"));
    const uint8_t both[6] = {0x00, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA};    // in range AND walked
    CHECK(g.registerNetwork(both, "WALKED_PAST"));

    const int n = g.crewNetworkRows(rows, kNetVisibleCap);
    CHECK(n == 2);                                   // one row per in-range network
    // Most-walked first — the tally is evidence for the choice, not a source of rows.
    CHECK(std::strcmp(rows[0].name, "WALKED_PAST") == 0 && rows[0].count == 7);
    CHECK(std::strcmp(rows[1].name, "IN_RANGE") == 0 && rows[1].count == 0);

    // A network in range is a real, claimable offer — no walk required first.
    g.setHomeNetwork(rows[1].key, rows[1].name);
    CHECK(g.hasHomeNetwork() && std::strcmp(g.homeNetworkName(), "IN_RANGE") == 0);
}

// The regression the reward-queue/snapshot split exists to prevent: a pending queue
// saturated by a backlog the player never walked off must not hide a network they are
// standing in front of.
void test_crew_picker_unaffected_by_saturated_reward_queue() {
    Game g{StartMode::Hatched};
    for (int i = 0; i < kPendingNetworkQueueCap; ++i) {
        const uint8_t m[6] = {0x02, 0x03, 0x04, 0x05,
                              static_cast<uint8_t>(i >> 8), static_cast<uint8_t>(i)};
        g.registerNetwork(m, "BACKLOG");
    }
    CHECK(g.pendingNetworks() == kPendingNetworkQueueCap);   // full, nothing resolved

    const uint8_t home[6] = {0x10, 0x20, 0x30, 0x40, 0x50, 0x60};
    g.registerNetwork(home, "HOME_WIFI");

    Game::CrewNetRow rows[kNetVisibleCap];
    const int n = g.crewNetworkRows(rows, kNetVisibleCap);
    bool offered = false;
    for (int i = 0; i < n; ++i)
        if (std::strcmp(rows[i].name, "HOME_WIFI") == 0) offered = true;
    CHECK(offered);
}

// The snapshot is a freshness window, not a lifetime list: a network the radio stops
// hearing ages out of the picker on its own, and is offered again when it comes back.
void test_crew_picker_drops_networks_out_of_range() {
    Game g{StartMode::Hatched};
    uint32_t t = 1000;
    g.tick(t);
    const uint8_t ap[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    g.registerNetwork(ap, "NEIGHBOUR");
    Game::CrewNetRow rows[kNetVisibleCap];
    CHECK(g.crewNetworkRows(rows, kNetVisibleCap) == 1);
    CHECK(g.visibleNetworkCount() == 1);

    g.tick(t += kNetVisibleFreshMs + 1);                    // stopped hearing it
    CHECK(g.crewNetworkRows(rows, kNetVisibleCap) == 0);
    CHECK(g.visibleNetworkCount() == 0);

    g.registerNetwork(ap, "NEIGHBOUR");                     // back in range
    CHECK(g.crewNetworkRows(rows, kNetVisibleCap) == 1);
}

// Build a wire frame for `hello`, the way another device's beacon would arrive.
static size_t makePeerFrame(const PeerHello& hello, uint8_t* frame) {
    return encodePeerHello(hello, frame, kPeerHelloSize);
}

// A beacon repeats about once a second for as long as two devices sit next to each
// other, so the roster's tally would be meaningless if every frame counted. Only
// the ARRIVAL edge is a meeting: a peer that keeps beaconing stays one encounter,
// and only aging out of the snapshot and coming back counts as meeting them again.
void test_register_peer_counts_encounters_not_frames() {
    Game g{StartMode::Hatched};
    uint32_t t = 0;
    const uint8_t mac[6] = {0x02, 0x11, 0x22, 0x33, 0x44, 0x55};

    PeerHello hello;
    std::strncpy(hello.tag, "GHOSTBYTE", sizeof(hello.tag) - 1);
    std::strncpy(hello.petName, "Paypup", sizeof(hello.petName) - 1);
    hello.stage = static_cast<uint8_t>(Stage::Process);
    uint8_t frame[kPeerHelloSize];
    CHECK(makePeerFrame(hello, frame) == kPeerHelloSize);

    CHECK(g.registerPeer(mac, frame, kPeerHelloSize));      // true == a new operator
    CHECK(g.livePeerCount() == 1);
    PeerLedger::Entry e;
    CHECK(g.peerLedger().lookup(packPeerKey(mac), &e));
    CHECK(e.timesMet == 1);

    // Ten more beacons across the encounter: still one meeting, still one row.
    for (int i = 0; i < 10; ++i) {
        g.tick(t += 1000);
        CHECK(!g.registerPeer(mac, frame, kPeerHelloSize));  // false == already known
    }
    CHECK(g.peerLedger().lookup(packPeerKey(mac), &e));
    CHECK(e.timesMet == 1);                                  // ...one encounter
    CHECK(g.peerLedger().size() == 1);

    // They walk away — the snapshot ages them out, so the room is empty again.
    g.tick(t += kPeerVisibleFreshMs + 1);
    CHECK(g.livePeerCount() == 0);

    // ...and come back later. THAT is meeting them a second time.
    CHECK(!g.registerPeer(mac, frame, kPeerHelloSize));      // known, but a new meeting
    CHECK(g.peerLedger().lookup(packPeerKey(mac), &e));
    CHECK(e.timesMet == 2);
    CHECK(g.livePeerCount() == 1);
}

// Anything in range can transmit, so a frame that doesn't decode is background
// noise rather than an error: dropped without touching the snapshot or the roster.
void test_register_peer_ignores_foreign_traffic() {
    Game g{StartMode::Hatched};
    const uint8_t mac[6] = {0x02, 0x11, 0x22, 0x33, 0x44, 0x55};

    uint8_t junk[kPeerHelloSize] = {0};
    CHECK(!g.registerPeer(mac, junk, kPeerHelloSize));      // right length, wrong magic
    CHECK(g.livePeerCount() == 0);
    CHECK(g.peerLedger().size() == 0);

    PeerHello hello;
    std::strncpy(hello.tag, "REAL", sizeof(hello.tag) - 1);
    uint8_t frame[kPeerHelloSize];
    makePeerFrame(hello, frame);
    CHECK(!g.registerPeer(mac, frame, kPeerHelloSize - 2)); // truncated
    CHECK(!g.registerPeer(nullptr, frame, kPeerHelloSize));
    CHECK(g.peerLedger().size() == 0);

    CHECK(g.registerPeer(mac, frame, kPeerHelloSize));      // a real one still lands
    CHECK(g.peerLedger().size() == 1);
}

// The screen merges two sources, and the merge is the interesting part: a live
// operator must appear ONCE, from the fresh snapshot (showing what they're raising
// right now) rather than twice with the roster's older copy underneath.
void test_peer_rows_live_first_and_deduped() {
    Game g{StartMode::Hatched};
    uint32_t t = 0;

    // Someone met before, not here now.
    PeerHello old;
    std::strncpy(old.tag, "ABSENT", sizeof(old.tag) - 1);
    std::strncpy(old.petName, "Pingcub", sizeof(old.petName) - 1);
    g.debugSeedPeer(0xAAAAAAAAAAAAull, old);

    // Someone standing here, whose pet has evolved since we last met them.
    const uint8_t mac[6] = {0x02, 0, 0, 0, 0, 0x01};
    PeerHello then;
    std::strncpy(then.tag, "PRESENT", sizeof(then.tag) - 1);
    std::strncpy(then.petName, "Paypup", sizeof(then.petName) - 1);
    then.stage = static_cast<uint8_t>(Stage::Process);
    g.debugSeedPeer(packPeerKey(mac), then);

    PeerHello now;
    std::strncpy(now.tag, "PRESENT", sizeof(now.tag) - 1);
    std::strncpy(now.petName, "Malbear", sizeof(now.petName) - 1);
    now.stage = static_cast<uint8_t>(Stage::Script);
    uint8_t frame[kPeerHelloSize];
    makePeerFrame(now, frame);
    g.tick(t += 1000);
    g.registerPeer(mac, frame, kPeerHelloSize);

    Game::PeerRow rows[8];
    const int n = g.peerRows(rows, 8);
    CHECK(n == 2);                                       // two operators, not three
    CHECK(rows[0].live);                                 // the live one leads
    CHECK(std::strcmp(rows[0].tag, "PRESENT") == 0);
    // ...and reads from the fresh beacon, not the roster's last-meeting copy.
    CHECK(std::strcmp(rows[0].petName, "Malbear") == 0);
    CHECK(rows[0].stage == static_cast<uint8_t>(Stage::Script));
    CHECK(!rows[1].live);
    CHECK(std::strcmp(rows[1].tag, "ABSENT") == 0);
}

// Release gate: a grayscale screenshot of PEERS must stay fully readable. The one
// status meaning on this screen is "is this operator here right now", and it is
// carried by a WORD (LIVE) against a count (x2) — never by colour — so a live and a
// remembered row must differ in lit pixels with the hue thrown away. Rendering both
// states and diffing the state column is what actually proves that.
void test_peers_screen_grayscale_live_vs_remembered() {
    Game g{StartMode::Hatched};
    uint32_t t = 0;
    const uint8_t mac[6] = {0x02, 0, 0, 0, 0, 0x01};

    PeerHello hello;
    std::strncpy(hello.tag, "GHOSTBYTE", sizeof(hello.tag) - 1);
    std::strncpy(hello.petName, "Malbear", sizeof(hello.petName) - 1);
    std::strncpy(hello.crewName, "Deniers of Service", sizeof(hello.crewName) - 1);
    hello.stage = static_cast<uint8_t>(Stage::Script);
    hello.rank = 4;
    uint8_t frame[kPeerHelloSize];
    encodePeerHello(hello, frame, sizeof(frame));

    while (g.nav() != Game::Nav::Idle) g.onButton(press(Button::C));
    g.onButton({Button::A, true, true});                  // A+C -> hacker face
    g.onButton(press(Button::A));
    while (hackerCarouselSlots()[g.cursor()].id != HackerSlotId::Peers)
        g.onButton(press(Button::A));
    g.onButton(press(Button::B));                         // enter PEERS

    g.tick(t += 1000);
    g.registerPeer(mac, frame, kPeerHelloSize);
    Framebuffer live(kActiveW, kActiveH);
    g.render(live);
    // The row itself reads without colour at all: tag, pet and crew are lit glyphs.
    CHECK(anyLitGray(live, 0, 46, kActiveW, 110));

    // Same operator, no longer in range — the roster row survives, the LIVE tag
    // must not. Compare the right-hand state column between the two renders.
    g.tick(t += kPeerVisibleFreshMs + 1);
    CHECK(g.livePeerCount() == 0);
    Framebuffer remembered(kActiveW, kActiveH);
    g.render(remembered);
    CHECK(anyLitGray(remembered, 0, 46, kActiveW, 110));   // still listed

    int diff = 0;
    for (int y = 46; y < 70; ++y)
        for (int x = kActiveW - 60; x < kActiveW; ++x)
            if (luminance(live.get(x, y)) != luminance(remembered.get(x, y))) ++diff;
    CHECK(diff > 0);   // LIVE vs. the meeting count differ in LUMINANCE, not just hue
}

// Opening PEERS arms the LINK radio on its own — two people holding devices
// together shouldn't wait on a duty cycle — and closing it hands the radio back.
// LINK stays independent of the AUDIT ladder in both directions: announcing is a
// separate consent from listening, so neither toggle may imply the other.
void test_peers_screen_raises_link_without_touching_config() {
    Game g{StartMode::Hatched};
    CHECK(!g.linkEnabled() && !g.linkWanted());           // CFG default: LINK off

    while (g.nav() != Game::Nav::Idle) g.onButton(press(Button::C));
    g.onButton({Button::A, true, true});                  // A+C -> hacker face
    g.onButton(press(Button::A));
    while (hackerCarouselSlots()[g.cursor()].id != HackerSlotId::Peers)
        g.onButton(press(Button::A));
    CHECK(!g.linkWanted());                               // still just the carousel
    g.onButton(press(Button::B));                         // enter PEERS

    CHECK(g.peersScreenOpen());
    CHECK(g.linkWanted());                                // the screen wants the radio
    CHECK(!g.linkEnabled());                              // ...but the opt-in is untouched
    CHECK(!g.radioScanWanted());                          // ...and the audit scan is NOT
    CHECK(g.auditMode() == Game::AuditMode::Off);         //     dragged along with it

    g.onButton(press(Button::C));                         // leave PEERS
    CHECK(!g.linkWanted());                               // config-dictated mode restored

    // The reverse, too: arming the audit scan must never start announcing.
    g.setNetScanEnabled(true);
    CHECK(g.radioScanWanted());
    CHECK(!g.linkWanted());
}

// Opening the CREW screen arms the passive scan on its own so the picker has live
// results, and closing it hands the radio straight back to the config-dictated mode —
// without ever touching the persisted CFG opt-in, and without arming capture.
void test_crew_screen_raises_scan_without_touching_config() {
    Game g{StartMode::Hatched};
    CHECK(!g.netScanEnabled() && !g.radioScanWanted());   // CFG default: AUDIT off

    while (g.nav() != Game::Nav::Idle) g.onButton(press(Button::C));
    g.onButton({Button::A, true, true});                  // A+C -> hacker face
    g.onButton(press(Button::A));
    while (hackerCarouselSlots()[g.cursor()].id != HackerSlotId::Crew)
        g.onButton(press(Button::A));
    CHECK(!g.radioScanWanted());                          // still just the carousel
    g.onButton(press(Button::B));                         // enter CREW

    CHECK(g.radioScanWanted());                           // the screen wants the radio
    CHECK(!g.netScanEnabled());                           // ...but the opt-in is untouched
    CHECK(g.auditMode() == Game::AuditMode::Off);         // ...and capture stays off

    g.onButton(press(Button::C));                         // leave CREW
    CHECK(!g.radioScanWanted());                          // config-dictated mode restored

    // With AUDIT already on, the screen changes nothing — it only ever raises.
    g.setNetScanEnabled(true);
    CHECK(g.radioScanWanted());
    g.onButton(press(Button::B));
    CHECK(g.radioScanWanted() && g.netScanEnabled());
    g.onButton(press(Button::C));
    CHECK(g.radioScanWanted() && g.netScanEnabled());
}

// The CREW screen outlives the global 5s menu-idle collapse. It has to: holding the
// screen open is what arms the scan, so defocusing on the standard budget powered the
// radio down before a sweep could finish — the picker starved itself and stayed empty
// forever. It still collapses eventually, on the kRadioScreenDefocusMs budget.
void test_crew_screen_outlives_the_menu_idle_timer() {
    Game g{StartMode::Hatched};
    uint32_t t = 0;
    while (g.nav() != Game::Nav::Idle) g.onButton(press(Button::C));
    g.onButton({Button::A, true, true});                  // A+C -> hacker face
    g.onButton(press(Button::A));
    while (hackerCarouselSlots()[g.cursor()].id != HackerSlotId::Crew)
        g.onButton(press(Button::A));
    g.onButton(press(Button::B));                         // enter CREW
    CHECK(g.crewScreenOpen() && g.radioScanWanted());

    // Well past the point any other submenu would have collapsed, with no input.
    for (int i = 0; i < 8; ++i) g.tick(t += kAutoDefocusMs);
    CHECK(g.crewScreenOpen());                            // still open
    CHECK(g.radioScanWanted());                           // ...so the radio stays armed

    // But not forever — the longer budget still expires.
    g.tick(t += kRadioScreenDefocusMs);
    CHECK(!g.crewScreenOpen());
    CHECK(!g.radioScanWanted());                          // radio handed back on collapse
}

// PEERS needs the same reprieve as CREW, and for a sharper reason: two devices only
// discover each other while BOTH have their radios up, so a screen that collapses on
// the standard 5s budget can leave an operator reading a list on a device that has
// quietly stopped looking. The device tier holds the panel awake off the same
// predicate (radioScreenOpen), so the screen doesn't go dark underneath them either.
void test_peers_screen_outlives_the_menu_idle_timer() {
    Game g{StartMode::Hatched};
    uint32_t t = 0;
    while (g.nav() != Game::Nav::Idle) g.onButton(press(Button::C));
    g.onButton({Button::A, true, true});                  // A+C -> hacker face
    g.onButton(press(Button::A));
    while (hackerCarouselSlots()[g.cursor()].id != HackerSlotId::Peers)
        g.onButton(press(Button::A));
    g.onButton(press(Button::B));                         // enter PEERS
    CHECK(g.peersScreenOpen() && g.linkWanted());
    CHECK(g.radioScreenOpen());                           // ...and holds the panel awake

    // Well past the point any other submenu would have collapsed, with no input —
    // this is the case that bit on hardware: reading a peer's row takes longer than
    // five seconds.
    for (int i = 0; i < 8; ++i) g.tick(t += kAutoDefocusMs);
    CHECK(g.peersScreenOpen());                           // still open
    CHECK(g.linkWanted());                                // ...so the radio stays armed

    // But not forever — the longer budget still expires and hands the radio back.
    g.tick(t += kRadioScreenDefocusMs);
    CHECK(!g.peersScreenOpen());
    CHECK(!g.linkWanted());
    CHECK(!g.radioScreenOpen());                          // panel sleep resumes too
}
