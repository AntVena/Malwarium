// test_pvp.cpp — native gates for 1v1 duels over LINK.
//
// One slice of the native suite; see test_gates.h for the shared fixtures and
// test_main.cpp for the roster that runs these. A save-vNN gate sits with the
// feature whose field it migrates, not in a migrations pile of its own.
#include "test_gates.h"

// --- 1v1 duels over LINK (core/net/pvp_link.h, core/model/pvp_battle.h) -------

// Every frame type survives the wire, and a malformed one is rejected outright rather
// than half-parsed. The fighter spec is the interesting payload: it is what both
// devices rebuild their combatants from, so a field that doesn't survive encoding
// would desync a duel rather than merely look wrong.
void test_pvp_frame_round_trip() {
    PvpFighter f;
    std::strncpy(f.tag, "NETRUNNER_99", sizeof(f.tag) - 1);
    std::strncpy(f.creatureId, "paypup", sizeof(f.creatureId) - 1);
    std::strncpy(f.moveIds[0], "packet_storm", kPvpIdCap - 1);
    std::strncpy(f.moveIds[1], "checksum_guard", kPvpIdCap - 1);
    std::strncpy(f.modIds[0], "firewall_patch", kPvpIdCap - 1);
    f.level = 300;                                    // past a byte, to catch a narrow field
    f.statPoints[0] = 4; f.statPoints[3] = 9;

    uint8_t buf[kPvpFrameCap];
    PvpFrame out;

    CHECK(encodePvpInvite(0xDEADBEEF, f, buf, sizeof(buf)) == kPvpInviteSize);
    CHECK(decodePvpFrame(buf, kPvpInviteSize, &out));
    CHECK(out.type == PvpFrameType::Invite);
    CHECK(out.session == 0xDEADBEEF);
    CHECK(out.rules == kPvpRulesVersion);
    CHECK(std::strcmp(out.fighter.tag, "NETRUNNER_99") == 0);
    CHECK(std::strcmp(out.fighter.creatureId, "paypup") == 0);
    CHECK(std::strcmp(out.fighter.moveIds[1], "checksum_guard") == 0);
    CHECK(std::strcmp(out.fighter.modIds[0], "firewall_patch") == 0);
    CHECK(out.fighter.moveIds[2][0] == '\0');         // an empty slot stays empty
    CHECK(out.fighter.level == 300);
    CHECK(out.fighter.statPoints[0] == 4 && out.fighter.statPoints[3] == 9);

    CHECK(encodePvpAccept(7, f, buf, sizeof(buf)) == kPvpAcceptSize);
    CHECK(decodePvpFrame(buf, kPvpAcceptSize, &out) && out.type == PvpFrameType::Accept);

    CHECK(encodePvpDecline(7, PvpDeclineReason::Busy, buf, sizeof(buf)) == kPvpDeclineSize);
    CHECK(decodePvpFrame(buf, kPvpDeclineSize, &out));
    CHECK(out.type == PvpFrameType::Decline && out.reason == PvpDeclineReason::Busy);

    CHECK(encodePvpStart(7, 12345, 999, buf, sizeof(buf)) == kPvpStartSize);
    CHECK(decodePvpFrame(buf, kPvpStartSize, &out));
    CHECK(out.type == PvpFrameType::Start && out.seed == 12345 && out.commit == 999);

    CHECK(encodePvpBye(7, buf, sizeof(buf)) == kPvpByeSize);
    CHECK(decodePvpFrame(buf, kPvpByeSize, &out) && out.type == PvpFrameType::Bye);

    // Malformed: wrong length for the declared type, a corrupted magic, and a
    // protocol version this build can't lay out.
    CHECK(encodePvpStart(7, 1, 2, buf, sizeof(buf)) == kPvpStartSize);
    CHECK(!decodePvpFrame(buf, kPvpStartSize - 1, &out));
    CHECK(!decodePvpFrame(buf, kPvpStartSize + 1, &out));
    CHECK(!decodePvpFrame(nullptr, kPvpStartSize, &out));
    uint8_t bad[kPvpFrameCap];
    std::memcpy(bad, buf, kPvpStartSize);
    bad[0] = 'X';
    CHECK(!decodePvpFrame(bad, kPvpStartSize, &out));
    std::memcpy(bad, buf, kPvpStartSize);
    bad[4] = kPvpProtoVersion + 1;
    CHECK(!decodePvpFrame(bad, kPvpStartSize, &out));
    std::memcpy(bad, buf, kPvpStartSize);
    bad[5] = 99;                                      // unknown frame type
    CHECK(!decodePvpFrame(bad, kPvpStartSize, &out));

    // A HELLO beacon must never be mistaken for a duel frame, or the reverse — they
    // share one receive path on the device (platform/esp32/net_link.h).
    PeerHello hello;
    std::strncpy(hello.tag, "SOMEONE", sizeof(hello.tag) - 1);
    uint8_t beacon[kPeerHelloSize];
    CHECK(encodePeerHello(hello, beacon, sizeof(beacon)) == kPeerHelloSize);
    CHECK(!decodePvpFrame(beacon, kPeerHelloSize, &out));
    PeerHello back;
    CHECK(encodePvpInvite(1, f, buf, sizeof(buf)) == kPvpInviteSize);
    CHECK(!decodePeerHello(buf, kPvpInviteSize, &back));
}

// Both devices must independently reach the SAME answer about who hosts, from
// information they both already have. Lower MAC wins, and it is a total order — no
// tie is possible because two devices cannot share a MAC.
void test_pvp_host_election_agrees_on_both_sides() {
    const uint8_t lowMac[6] = {0x02, 0, 0, 0, 0, 0x01};
    const uint8_t highMac[6] = {0x02, 0, 0, 0, 0, 0x02};
    const uint64_t low = packPeerKey(lowMac);
    const uint64_t high = packPeerKey(highMac);
    CHECK(low < high);
    CHECK(pvpHostIsLocal(low, high));           // the low device says "I host"
    CHECK(!pvpHostIsLocal(high, low));          // ...and the high device agrees

    // A packed key round-trips back to the same six bytes, which is what lets a screen
    // turn a selected roster row into an address to send to.
    uint8_t back[6] = {0};
    unpackPeerKey(high, back);
    CHECK(std::memcmp(back, highMac, 6) == 0);
}

// START's commit hash is the divergence guard: it must change if ANY of the three
// things it covers changes, so a guest that would have played a different fight
// refuses instead of playing it.
void test_pvp_commit_hash_catches_divergence() {
    PvpFighter a, b;
    std::strncpy(a.creatureId, "paypup", sizeof(a.creatureId) - 1);
    std::strncpy(b.creatureId, "pingcub", sizeof(b.creatureId) - 1);

    const uint32_t base = pvpCommitHash(42, a, b);
    CHECK(pvpCommitHash(42, a, b) == base);          // stable
    CHECK(pvpCommitHash(43, a, b) != base);          // a different seed
    CHECK(pvpCommitHash(42, b, a) != base);          // ...or the fighters seated swapped
    PvpFighter a2 = a;
    a2.statPoints[0] = 1;
    CHECK(pvpCommitHash(42, a2, b) != base);         // ...or one stat point of difference
}

// The heart of the feature: two devices, no turn-by-turn traffic, one fight. Feeding
// the same two specs and the same seed to two independent Combats must produce the
// same fight hit for hit — that is the entire reason a duel can play out on both
// screens at once without broadcasting anything.
void test_pvp_same_seed_plays_the_same_fight() {
    ContentRegistry reg;
    reg.addSource(embeddedContent());
    reg.setAssets(embeddedAssets());

    PvpFighter host, guest;
    std::strncpy(host.creatureId, "paypup", sizeof(host.creatureId) - 1);
    std::strncpy(host.moveIds[0], "packet_storm", kPvpIdCap - 1);
    host.statPoints[0] = 3;
    std::strncpy(guest.creatureId, "pingcub", sizeof(guest.creatureId) - 1);
    std::strncpy(guest.moveIds[0], "checksum_guard", kPvpIdCap - 1);
    guest.statPoints[3] = 5;
    CHECK(canBuildPvpCombatant(reg, host) && canBuildPvpCombatant(reg, guest));

    Combat left, right;
    beginPvpBattle(left, reg, host, guest, 0xC0FFEE);
    beginPvpBattle(right, reg, host, guest, 0xC0FFEE);

    // Different fighters, so a seating mistake would show up as a mismatch below
    // rather than hiding behind a mirror match.
    CHECK(std::strcmp(left.player().name, right.player().name) == 0);
    CHECK(std::strcmp(left.player().name, left.enemy().name) != 0);

    int steps = 0;
    while (left.outcome() == Combat::Outcome::Ongoing && steps < 500) {
        CHECK(left.step() == right.step());
        CHECK(left.player().health == right.player().health);
        CHECK(left.enemy().health == right.enemy().health);
        CHECK(left.playerTurnNext() == right.playerTurnNext());
        ++steps;
    }
    CHECK(steps > 0 && steps < 500);                 // it actually resolved
    CHECK(left.outcome() == right.outcome());
    CHECK(left.outcome() != Combat::Outcome::Ongoing);

    // A different seed is a different fight — otherwise the agreement above would be
    // proving nothing about the seed at all.
    Combat other;
    beginPvpBattle(other, reg, host, guest, 0xBADBAD);
    int otherSteps = 0;
    while (other.outcome() == Combat::Outcome::Ongoing && otherSteps < 500) {
        other.step();
        ++otherSteps;
    }
    CHECK(otherSteps != steps || other.outcome() != left.outcome());
}

// Seating is fixed to HOST-as-player_ precisely because Combat is not symmetric:
// swapping the two arguments produces a different fight, so both devices agreeing on
// the order is load-bearing, not cosmetic.
void test_pvp_seating_order_is_load_bearing() {
    ContentRegistry reg;
    reg.addSource(embeddedContent());
    reg.setAssets(embeddedAssets());

    PvpFighter a, b;
    std::strncpy(a.creatureId, "paypup", sizeof(a.creatureId) - 1);
    std::strncpy(b.creatureId, "pingcub", sizeof(b.creatureId) - 1);

    Combat forward, swapped;
    beginPvpBattle(forward, reg, a, b, 99);
    beginPvpBattle(swapped, reg, b, a, 99);
    CHECK(std::strcmp(forward.player().name, swapped.enemy().name) == 0);
    CHECK(std::strcmp(forward.enemy().name, swapped.player().name) == 0);

    // No Exploit is offered on either side: the A+C picker pauses the fight for a
    // human, and there is no way to pause the other device's copy of it.
    CHECK(forward.overrideUsesTotal() == 0 && !forward.overrideReady());
    // And no stakes — a loss must not reach Fragmentation.
    CHECK(forward.stakes() == Combat::Stakes::Safe);
}

// Move every frame `from` has queued FOR `to` into `to`, as a unicast radio would.
// Honouring the destination MAC matters: a device mid-duel is queueing frames for its
// opponent, and delivering those to an unrelated third device would be a fiction the
// radio never performs. Frames addressed elsewhere are dropped, as they would be here.
static int relayPvp(Game& from, const uint8_t* fromMac, Game& to, const uint8_t* toMac) {
    uint8_t mac[6];
    uint8_t buf[kPvpFrameCap];
    size_t len = 0;
    int n = 0;
    while (from.takePvpOut(mac, buf, sizeof(buf), &len)) {
        if (std::memcmp(mac, toMac, 6) != 0) continue;
        to.onPvpFrame(fromMac, buf, len);
        ++n;
    }
    return n;
}

// Let `a` hear `b`'s beacon so `b` shows up as a live, challengeable peer.
static void hearBeacon(Game& listener, const uint8_t* senderMac, const char* tag) {
    PeerHello hello;
    std::strncpy(hello.tag, tag, sizeof(hello.tag) - 1);
    std::strncpy(hello.petName, "Paypup", sizeof(hello.petName) - 1);
    hello.stage = static_cast<uint8_t>(Stage::Process);
    uint8_t frame[kPeerHelloSize];
    encodePeerHello(hello, frame, sizeof(frame));
    listener.registerPeer(senderMac, frame, kPeerHelloSize);
}

// The whole handshake, end to end, between two engines: challenge, human consent,
// seed agreement, and then both devices playing the same fight to the same verdict —
// stated from each operator's own point of view.
void test_pvp_two_devices_duel_end_to_end() {
    const uint8_t macA[6] = {0x02, 0, 0, 0, 0, 0x01};   // lower -> hosts
    const uint8_t macB[6] = {0x02, 0, 0, 0, 0, 0x02};

    Game a{StartMode::Hatched, "paypup"};
    Game b{StartMode::Hatched, "pingcub"};
    a.setLocalRadioMac(macA);
    b.setLocalRadioMac(macB);
    uint32_t t = 0;
    a.tick(t += 1000);
    b.tick(t);

    // Both operators are on the LINK screen — that IS the consent to be challenged.
    enterHackerSlot(a, HackerSlotId::Link);
    enterHackerSlot(b, HackerSlotId::Link);
    CHECK(a.linkScreenOpen() && b.linkScreenOpen());
    CHECK(a.linkWanted() && !a.linkEnabled());          // the screen arms the radio,
    CHECK(a.radioScreenOpen());                          // holds the panel awake,
    CHECK(!a.netScanEnabled());                          // and never arms the audit scan

    hearBeacon(a, macB, "OPERATOR_B");
    hearBeacon(b, macA, "OPERATOR_A");

    // A challenges B.
    uint64_t keys[8];
    CHECK(a.pvpChallengeableKeys(keys, 8) == 1);
    CHECK(a.pvpChallenge(keys[0]));
    CHECK(a.pvpPhase() == Game::PvpPhase::Inviting);
    CHECK(a.pvpLocalIsHost() && !b.pvpLocalIsHost());   // ...before B has even heard

    CHECK(relayPvp(a, macA, b, macB) == 1);                   // the INVITE lands
    CHECK(b.pvpPhase() == Game::PvpPhase::Invited);
    CHECK(std::strcmp(b.pvpOpponentTag(), "NETRUNNER_99") == 0);

    // B's human accepts. B is the guest, so it sends its fighter and waits.
    b.pvpAcceptChallenge();
    CHECK(b.pvpPhase() == Game::PvpPhase::Arming);
    CHECK(relayPvp(b, macB, a, macA) >= 1);                   // the ACCEPT lands
    CHECK(a.pvpFighting());                             // the host starts at once
    CHECK(a.nav() == Game::Nav::Combat);

    CHECK(relayPvp(a, macA, b, macB) >= 1);                   // the START lands
    CHECK(b.pvpFighting());
    CHECK(b.nav() == Game::Nav::Combat);

    // Same two fighters, seated the same way round on both screens.
    CHECK(std::strcmp(a.combat().player().name, b.combat().player().name) == 0);
    CHECK(std::strcmp(a.combat().enemy().name, b.combat().enemy().name) == 0);
    CHECK(std::strcmp(a.combat().player().name, a.combat().enemy().name) != 0);

    // Neither screen can pause or bail: A+C is inert and C does not flee.
    a.onButton({Button::A, true, true});
    CHECK(!a.combat().overrideOpen());
    tapC(a);
    CHECK(a.combat().outcome() == Combat::Outcome::Ongoing);

    // Play both out. Each device is stepping its OWN copy; they must stay identical.
    int steps = 0;
    while (a.combat().outcome() == Combat::Outcome::Ongoing && steps < 500) {
        a.onButton(press(Button::A));
        b.onButton(press(Button::A));
        CHECK(a.combat().player().health == b.combat().player().health);
        CHECK(a.combat().enemy().health == b.combat().enemy().health);
        ++steps;
    }
    CHECK(steps < 500);
    CHECK(a.combat().outcome() == b.combat().outcome());

    const int bitsBefore = a.bits();
    const int fragBefore = a.model().fragmentation();
    const int levelBefore = a.combatLevel();

    a.onButton(press(Button::B));                       // dismiss the result
    b.onButton(press(Button::B));
    CHECK(a.pvpPhase() == Game::PvpPhase::Result);
    CHECK(b.pvpPhase() == Game::PvpPhase::Result);
    // Exactly one of them won, and each was told so from its own side.
    CHECK(a.pvpLocalWon() != b.pvpLocalWon());

    // No stakes: nothing was earned, lost, or corrupted by fighting.
    CHECK(a.bits() == bitsBefore);
    CHECK(a.model().fragmentation() == fragBefore);
    CHECK(a.combatLevel() == levelBefore);

    // A verdict left on screen must not pin the radio awake — only a session that
    // still needs the air does that.
    CHECK(a.pvpActive() && !a.pvpSessionLive());

    // Any key clears the verdict back to the challengeable list.
    a.onButton(press(Button::A));
    CHECK(a.pvpPhase() == Game::PvpPhase::Idle);
    CHECK(a.linkScreenOpen());
}

// A duel is the only fight whose opponent is a real species rather than a name-only
// malbeast/boss/dummy spec, so it is the only thing that can move a creature from
// "locked" to "seen". Both devices record the other's species, and neither records
// its own — you raised that one.
void test_pvp_duel_marks_the_opponent_species_seen() {
    const uint8_t macA[6] = {0x02, 0, 0, 0, 0, 0x01};
    const uint8_t macB[6] = {0x02, 0, 0, 0, 0, 0x02};

    Game a{StartMode::Hatched, "paypup"};
    Game b{StartMode::Hatched, "pingcub"};
    a.setLocalRadioMac(macA);
    b.setLocalRadioMac(macB);
    uint32_t t = 0;
    a.tick(t += 1000);
    b.tick(t);
    enterHackerSlot(a, HackerSlotId::Link);
    enterHackerSlot(b, HackerSlotId::Link);
    hearBeacon(a, macB, "OPERATOR_B");
    hearBeacon(b, macA, "OPERATOR_A");

    CHECK(!a.creatureSeen("pingcub") && !b.creatureSeen("paypup"));

    uint64_t keys[8];
    CHECK(a.pvpChallengeableKeys(keys, 8) == 1);
    CHECK(a.pvpChallenge(keys[0]));
    CHECK(relayPvp(a, macA, b, macB) == 1);        // INVITE
    b.pvpAcceptChallenge();
    CHECK(relayPvp(b, macB, a, macA) >= 1);        // ACCEPT -> the host starts
    CHECK(a.pvpFighting());
    CHECK(relayPvp(a, macA, b, macB) >= 1);        // START -> the guest starts
    CHECK(b.pvpFighting());

    // Recorded at the START, before either fight resolves — facing the species is
    // the glimpse, not beating it.
    CHECK(a.creatureSeen("pingcub"));
    CHECK(b.creatureSeen("paypup"));
    CHECK(!a.creatureSeen("paypup"));              // its own pet: raised, never "seen"
    CHECK(!b.creatureSeen("pingcub"));
    CHECK(a.creatureRaised("paypup") && !a.creatureRaised("pingcub"));

    // ...and that is exactly the tier the 'Pedia reports for it.
    const std::string json = buildPediaStateJson(a);
    CHECK(json.find("\"pingcub\":\"seen\"") != std::string::npos);
    CHECK(json.find("\"paypup\":\"hatched\"") != std::string::npos);
}

// A human takes SECONDS to press accept, and the challenger repeats its INVITE every
// kPvpRetryMs the whole time. So the target sees the same invite many times over while
// its own prompt is on screen, and must ignore those repeats: answering one with BUSY
// declines the very challenge it is displaying, and the challenger gives up before its
// opponent can possibly say yes.
//
// This is the case the end-to-end test missed by relaying each frame exactly once —
// on hardware it failed 100% of the time.
void test_pvp_invite_retries_do_not_decline_the_pending_challenge() {
    const uint8_t macA[6] = {0x02, 0, 0, 0, 0, 0x01};
    const uint8_t macB[6] = {0x02, 0, 0, 0, 0, 0x02};

    Game a{StartMode::Hatched, "paypup"};
    Game b{StartMode::Hatched, "pingcub"};
    a.setLocalRadioMac(macA);
    b.setLocalRadioMac(macB);
    uint32_t t = 0;
    a.tick(t += 1000);
    b.tick(t);
    enterHackerSlot(a, HackerSlotId::Link);
    enterHackerSlot(b, HackerSlotId::Link);
    hearBeacon(a, macB, "OPERATOR_B");
    hearBeacon(b, macA, "OPERATOR_A");

    uint64_t keys[8];
    CHECK(a.pvpChallengeableKeys(keys, 8) == 1);
    CHECK(a.pvpChallenge(keys[0]));
    CHECK(relayPvp(a, macA, b, macB) == 1);
    CHECK(b.pvpPhase() == Game::PvpPhase::Invited);

    // B's human dithers for several seconds while A keeps retrying. Every retry must
    // land on a still-Invited B and produce NOTHING back.
    for (int i = 0; i < kPvpMaxRetries + 2; ++i) {
        a.tick(t += kPvpRetryMs);
        b.tick(t);
        relayPvp(a, macA, b, macB);
        CHECK(b.pvpPhase() == Game::PvpPhase::Invited);   // prompt still up
        CHECK(relayPvp(b, macB, a, macA) == 0);                 // ...and B said nothing back
        CHECK(a.pvpPhase() == Game::PvpPhase::Inviting);  // so A is still waiting
    }

    // Now the human finally accepts, and the duel proceeds normally.
    b.pvpAcceptChallenge();
    relayPvp(b, macB, a, macA);
    relayPvp(a, macA, b, macB);
    CHECK(a.pvpFighting() && b.pvpFighting());
    CHECK(std::strcmp(a.combat().player().name, b.combat().player().name) == 0);

    // A genuinely DIFFERENT session from a device already in a duel still gets BUSY —
    // ignoring retries must not have made the busy answer unreachable.
    const uint8_t macC[6] = {0x02, 0, 0, 0, 0, 0x03};
    Game c{StartMode::Hatched, "paypup"};
    c.setLocalRadioMac(macC);
    c.tick(t += 1000);
    enterHackerSlot(c, HackerSlotId::Link);
    hearBeacon(c, macB, "OPERATOR_B");
    uint64_t ckeys[8];
    CHECK(c.pvpChallengeableKeys(ckeys, 8) == 1);
    CHECK(c.pvpChallenge(ckeys[0]));
    relayPvp(c, macC, b, macB);
    CHECK(relayPvp(b, macB, c, macC) >= 1);                     // B answered the newcomer
    CHECK(c.pvpPhase() == Game::PvpPhase::Idle);
    CHECK(std::strcmp(c.pvpStatusText(), "OPERATOR BUSY") == 0);
    CHECK(a.pvpFighting() && b.pvpFighting());            // ...without disturbing the duel
}

// A challenge is only ever considered by a human looking at the LINK screen. Anywhere
// else the device answers BUSY rather than interrupting someone who isn't looking for
// a fight — and the challenger is told which, so a refusal never reads as a hang.
void test_pvp_challenge_needs_a_human_on_the_link_screen() {
    const uint8_t macA[6] = {0x02, 0, 0, 0, 0, 0x01};
    const uint8_t macB[6] = {0x02, 0, 0, 0, 0, 0x02};

    Game a{StartMode::Hatched, "paypup"};
    Game b{StartMode::Hatched, "paypup"};
    a.setLocalRadioMac(macA);
    b.setLocalRadioMac(macB);
    uint32_t t = 0;
    a.tick(t += 1000);
    b.tick(t);

    enterHackerSlot(a, HackerSlotId::Link);
    hearBeacon(a, macB, "OPERATOR_B");
    uint64_t keys[8];
    CHECK(a.pvpChallengeableKeys(keys, 8) == 1);
    CHECK(a.pvpChallenge(keys[0]));

    // B is sitting on its idle habitat, not the LINK screen.
    CHECK(!b.linkScreenOpen());
    relayPvp(a, macA, b, macB);
    CHECK(b.pvpPhase() == Game::PvpPhase::Idle);        // never even prompted
    relayPvp(b, macB, a, macA);                                // ...but it did answer
    CHECK(a.pvpPhase() == Game::PvpPhase::Idle);
    CHECK(std::strcmp(a.pvpStatusText(), "OPERATOR BUSY") == 0);

    // And a refusal by the human reads differently from being busy.
    enterHackerSlot(b, HackerSlotId::Link);
    CHECK(a.pvpChallenge(keys[0]));
    relayPvp(a, macA, b, macB);
    CHECK(b.pvpPhase() == Game::PvpPhase::Invited);
    b.pvpDeclineChallenge();
    CHECK(b.pvpPhase() == Game::PvpPhase::Idle);
    relayPvp(b, macB, a, macA);
    CHECK(a.pvpPhase() == Game::PvpPhase::Idle);
    CHECK(std::strcmp(a.pvpStatusText(), "DECLINED") == 0);
}

// An unanswered challenge must not strand the screen: it times out, says so, and hands
// the radio back.
void test_pvp_unanswered_challenge_times_out() {
    const uint8_t macA[6] = {0x02, 0, 0, 0, 0, 0x01};
    const uint8_t macB[6] = {0x02, 0, 0, 0, 0, 0x02};
    Game a{StartMode::Hatched, "paypup"};
    a.setLocalRadioMac(macA);
    uint32_t t = 0;
    a.tick(t += 1000);
    enterHackerSlot(a, HackerSlotId::Link);
    hearBeacon(a, macB, "OPERATOR_B");

    uint64_t keys[8];
    CHECK(a.pvpChallengeableKeys(keys, 8) == 1);
    CHECK(a.pvpChallenge(keys[0]));
    CHECK(a.pvpActive() && a.linkWanted());

    a.tick(t += kPvpInviteTimeoutMs + 1);
    CHECK(a.pvpPhase() == Game::PvpPhase::Idle);
    CHECK(std::strcmp(a.pvpStatusText(), "NO ANSWER") == 0);
}

// The bottom row owns the zoned Health gauge with its Critical pulse — the "my pet is
// in trouble" read — so each duellist must see their OWN pet there. The guest holds
// Combat's enemy_ slot, so its screen swaps the rows for display. Rendering one shared
// fight from both sides and diffing proves the swap is actually wired: without it the
// two screens would be pixel-identical and the guest would be watching its rival's
// health pulse at it.
void test_pvp_guest_sees_its_own_pet_on_the_bottom_gauge() {
    ContentRegistry reg;
    reg.addSource(embeddedContent());
    reg.setAssets(embeddedAssets());

    PvpFighter host, guest;
    std::strncpy(host.creatureId, "paypup", sizeof(host.creatureId) - 1);
    std::strncpy(guest.creatureId, "pingcub", sizeof(guest.creatureId) - 1);
    guest.statPoints[3] = 9;                 // a different max Health, so the gauges differ

    Combat c;
    beginPvpBattle(c, reg, host, guest, 0xC0FFEE);
    for (int i = 0; i < 3 && c.outcome() == Combat::Outcome::Ongoing; ++i) c.step();

    const SpriteData* ps = reg.sprite(c.player().spriteName);
    const SpriteData* es = reg.sprite(c.enemy().spriteName);

    CombatSides hostSides;                   // host: the ordinary reading
    hostSides.rivalLabel = "RIVAL";
    hostSides.localLabel = "YOU";
    CombatSides guestSides = hostSides;
    guestSides.localIsEnemySide = true;      // guest: roles swapped for display

    Framebuffer hostView(kActiveW, kActiveH), guestView(kActiveW, kActiveH);
    drawCombat(hostView, c, ps, es, 0, 0, 0, false, hostSides);
    drawCombat(guestView, c, ps, es, 0, 0, 0, false, guestSides);

    int diff = 0;
    for (int y = 0; y < kActiveH; ++y)
        for (int x = 0; x < kActiveW; ++x)
            if (hostView.get(x, y) != guestView.get(x, y)) ++diff;
    CHECK(diff > 0);                         // the two screens are NOT the same picture

    // ...and the difference survives grayscale, since it is geometry and digits rather
    // than hue: the release gate is that a colourless screenshot still tells you whose
    // pet is whose.
    int grayDiff = 0;
    for (int y = 0; y < kActiveH; ++y)
        for (int x = 0; x < kActiveW; ++x)
            if (luminance(hostView.get(x, y)) != luminance(guestView.get(x, y))) ++grayDiff;
    CHECK(grayDiff > 0);
}

// The local pet fights from the LEFT stage seat, its rival from the RIGHT — in every
// fight, PVE and duel alike. The seat follows the local/rival ROLE, not Combat's
// player_/enemy_ slot, so a duel guest (which holds enemy_) still finds its own pet on
// the left. Rendering with ONE sprite supplied at a time pins each seat to a role:
// whichever combatant is the local one lights the left box and leaves the right dark.
void test_combat_seats_local_pet_on_the_left() {
    // The band the two seats are bottom-anchored in (combat_screen.cpp). Nothing but the
    // sprites draws here while a fight is Ongoing and neither side is winding up.
    constexpr int kBandY0 = 81, kBandY1 = 165;

    ContentRegistry reg;
    reg.addSource(embeddedContent());
    reg.setAssets(embeddedAssets());

    PvpFighter host, guest;
    std::strncpy(host.creatureId, "paypup", sizeof(host.creatureId) - 1);
    std::strncpy(guest.creatureId, "pingcub", sizeof(guest.creatureId) - 1);

    Combat c;
    beginPvpBattle(c, reg, host, guest, 0xC0FFEE);   // left Ongoing: no result banner
    const SpriteData* ps = reg.sprite(c.player().spriteName);
    const SpriteData* es = reg.sprite(c.enemy().spriteName);

    auto seatOf = [&](const SpriteData* p, const SpriteData* e, bool localIsEnemySide) {
        CombatSides sides;
        sides.localIsEnemySide = localIsEnemySide;
        // The seats move with the two creatures' drawn widths, so the boxes to look in
        // come from the same seating the draw used rather than from copied numbers.
        const CombatStage st = combatStage(localIsEnemySide ? e : p,
                                           localIsEnemySide ? p : e);
        Framebuffer fb(kActiveW, kActiveH);
        drawCombat(fb, c, p, e, 0, 0, 0, false, sides);
        const bool left = anyLitGray(fb, st.localX, kBandY0,
                                     st.localX + st.localW, kBandY1);
        const bool right = anyLitGray(fb, st.rivalX, kBandY0,
                                      st.rivalX + st.rivalW, kBandY1);
        CHECK(left != right);                        // exactly one seat is occupied
        return left;
    };

    CHECK(seatOf(ps, nullptr, false));               // PVE: the pet is Combat's player_
    CHECK(!seatOf(nullptr, es, false));              // PVE: the wild malbeast sits right
    CHECK(seatOf(nullptr, es, true));                // duel guest: enemy_ IS its own pet
    CHECK(!seatOf(ps, nullptr, true));               // ...so the host's pet is its rival
}

// Release gate: NO two creatures in the roster may overlap on the combat stage, the
// clash lane between them is always on canvas and never narrower than the strike mark
// drawn in it, and — since the stage gained a second SHOT to fall back on — no pairing
// in the roster is cropped at all. Swept over every pairing rather than a sample,
// because the pairs that break it are exactly the rare ones: two Daemon cells whose art
// runs to both edges, which together want 336 of the 224 px there are.
void test_combat_stage_seats_never_overlap() {
    ContentRegistry reg = ContentRegistry::embedded();
    std::vector<const SpriteData*> sprites{nullptr};      // a fighter with no art too
    for (const CreatureDef* c : reg.allCreatures())
        if (const SpriteData* s = reg.creatureSprite(*c)) sprites.push_back(s);
    CHECK(sprites.size() > 8);                            // the sweep found a roster

    int wide = 0;
    for (const SpriteData* l : sprites) {
        for (const SpriteData* r : sprites) {
            const CombatStage st = combatStage(l, r);
            CHECK(st.localX + st.localW == st.laneX);     // the lane starts where mine ends
            CHECK(st.laneX + st.laneW == st.rivalX);      // ...and ends where theirs starts
            CHECK(st.laneW >= 18);                        // room for the strike mark
            CHECK(st.laneX >= 0 && st.laneX + st.laneW <= kActiveW);
            // The shot is one of the two rungs and nothing between them, because every
            // ratio between resamples the art (CombatStage, ui/combat_screen.h).
            const bool standing = st.num == kScaleNum && st.den == kScaleDen;
            const bool wideShot = st.num == 1 && st.den == 1;
            CHECK(standing || wideShot);
            if (wideShot) ++wide;
            // No creature in the roster is cut off. This is what the wide shot BUYS, and
            // stating it as a gate is what stops a future creature from quietly getting
            // its face cropped again: art too wide for the standing shot is meant to pull
            // the camera back, and on the day it cannot, this fails rather than the panel
            // silently losing a head.
            CHECK(st.localX >= 0);
            CHECK(st.rivalX + st.rivalW <= kActiveW);
        }
    }
    // ...and the wide shot is REACHED. A fallback no pairing ever takes has never run,
    // so the sweep asserts the roster still holds a pair too wide for the standing shot
    // rather than trusting that it does.
    CHECK(wide > 0);
}

// Release gate: the shot is a property of the PAIRING, so it is the same from either
// seat — which is what makes it the same on both devices in a duel. A stage that framed
// itself off "my pet" would put the two players in different cameras on one fight.
void test_combat_stage_shot_is_symmetric() {
    ContentRegistry reg = ContentRegistry::embedded();
    std::vector<const SpriteData*> sprites{nullptr};
    for (const CreatureDef* c : reg.allCreatures())
        if (const SpriteData* s = reg.creatureSprite(*c)) sprites.push_back(s);
    for (const SpriteData* l : sprites)
        for (const SpriteData* r : sprites) {
            const CombatStage a = combatStage(l, r), b = combatStage(r, l);
            CHECK(a.num == b.num && a.den == b.den);
        }
}

// Release gate: the wide shot RESAMPLES NOTHING. 1/1 is the artist's own grid, so a
// creature framed in it lands on the panel as the exact pixels its sheet carries — that
// is the whole reason the ladder skips the ratios between (CombatStage). Asked of the
// widest pairing in the roster, which is the one that takes the wide shot.
void test_combat_wide_shot_draws_authored_pixels() {
    ContentRegistry reg = ContentRegistry::embedded();
    const SpriteData* wide = nullptr;
    for (const CreatureDef* c : reg.allCreatures())
        if (const SpriteData* s = reg.creatureSprite(*c))
            if (!wide || spriteContentX1(*s) - spriteContentX0(*s) >
                             spriteContentX1(*wide) - spriteContentX0(*wide))
                wide = s;
    CHECK(wide != nullptr);
    const CombatStage st = combatStage(wide, wide);
    CHECK(st.num == 1 && st.den == 1);                    // the pair that pulls back
    // Its band is its own content width — one panel pixel per authored pixel.
    CHECK(st.localW == spriteContentX1(*wide) - spriteContentX0(*wide));
}

// Release gate: the strike mark answers WHO IS HITTING WHOM without colour. It lives in
// the lane for the swing window only, and it TRAVELS toward the fighter being hit — so
// the two directions are different pictures, and a single frozen frame still carries the
// answer through the mark's taper.
void test_combat_strike_mark_travels_toward_its_target() {
    ContentRegistry reg = ContentRegistry::embedded();
    const SpriteData* ps = reg.sprite("SPR_PET_PAYPUP");
    const SpriteData* es = reg.sprite("SPR_PET_PINGCUB");
    const CombatStage st = combatStage(ps, es);

    // The mark's centre of mass along the lane, in grayscale. Nothing else draws here
    // while a fight is Ongoing: the lane is the gap the seating reserved.
    auto laneCentroid = [&](const Framebuffer& fb, int& lit) {
        long sum = 0;
        lit = 0;
        for (int y = 81; y < 165; ++y)
            for (int x = st.laneX; x < st.laneX + st.laneW; ++x)
                if (luminance(fb.get(x, y)) > 0.12f) { sum += x; ++lit; }
        return lit ? static_cast<int>(sum / lit) : -1;
    };
    // Run a real fight forward until the wanted side lands a swing, then hold that turn
    // and step the render's own hit clock across the swing window.
    auto shotsFor = [&](bool byPlayer, int beat, Framebuffer& fb) {
        Combatant p = mkCombatant(reg, "P", 400, 20, {"quick_jab"});
        Combatant e = mkCombatant(reg, "E", 400, 1, {"quick_jab"});
        Combat c;
        c.begin(p, e, Combat::Stakes::Safe, 7);
        for (int i = 0; i < 40; ++i) {
            c.step();
            if (c.lastWasStrike() && c.lastByPlayer() == byPlayer) break;
        }
        CHECK(c.lastWasStrike() && c.lastByPlayer() == byPlayer);
        drawCombat(fb, c, ps, es, 0, 0, beat, false, CombatSides{});
    };

    int litFirst = 0, litLast = 0, litAfter = 0;
    Framebuffer byUsFirst(kActiveW, kActiveH), byUsLast(kActiveW, kActiveH);
    Framebuffer byThemFirst(kActiveW, kActiveH), byThemLast(kActiveW, kActiveH);
    Framebuffer settled(kActiveW, kActiveH);
    shotsFor(/*byPlayer=*/true, 0, byUsFirst);
    shotsFor(/*byPlayer=*/true, 3, byUsLast);
    shotsFor(/*byPlayer=*/false, 0, byThemFirst);
    shotsFor(/*byPlayer=*/false, 3, byThemLast);
    shotsFor(/*byPlayer=*/true, 12, settled);             // long past the swing window

    const int usFrom = laneCentroid(byUsFirst, litFirst);
    const int usTo = laneCentroid(byUsLast, litLast);
    CHECK(litFirst > 0 && litLast > 0);                   // the mark is drawn, in grayscale
    CHECK(usTo > usFrom);                                 // our swing travels rightward...

    const int themFrom = laneCentroid(byThemFirst, litFirst);
    const int themTo = laneCentroid(byThemLast, litLast);
    CHECK(litFirst > 0 && litLast > 0);
    CHECK(themTo < themFrom);                             // ...and theirs the other way

    laneCentroid(settled, litAfter);
    CHECK(litAfter == 0);                                 // and the lane clears afterwards
}

// Release gate: a wind-up must not read as a hit landing. The two cues are separated on
// three channels at once — the wind-up BUILDS where an impact DECAYS, it carries a
// countdown meter over the charging fighter's head that an impact has no equivalent of,
// and both survive grayscale, which is where a hue difference alone would not.
void test_combat_windup_reads_apart_from_impact() {
    ContentRegistry reg = ContentRegistry::embedded();
    const SpriteData* ps = reg.sprite("SPR_PET_PAYPUP");
    const SpriteData* es = reg.sprite("SPR_PET_PINGCUB");
    const CombatStage st = combatStage(ps, es);

    // Total grayscale brightness over one fighter's seat — the flash's own channel.
    auto seatGray = [&](const Framebuffer& fb, int x0, int w) {
        float sum = 0;
        for (int y = 81; y < 165; ++y)
            for (int x = std::max(0, x0); x < std::min(kActiveW, x0 + w); ++x)
                sum += luminance(fb.get(x, y));
        return sum;
    };
    // The charging pet is the one that CAN charge: a kit without a channelled move can
    // never wind up, which is what makes the impact frames a clean control.
    auto render = [&](bool channel, int animBeat, int hitBeat, Framebuffer& fb) {
        Combatant p = channel ? mkCombatant(reg, "P", 400, 20, {"runaway_fork"})
                              : mkCombatant(reg, "P", 400, 20, {"quick_jab"});
        Combatant e = mkCombatant(reg, "E", 400, 1, {"quick_jab"});
        Combat c;
        c.begin(p, e, Combat::Stakes::Safe, 7);
        if (channel) {
            for (int i = 0; i < 40 && c.player().channelMoveIdx < 0; ++i) c.step();
            CHECK(c.player().channelMoveIdx >= 0);        // the pet really is charging
        } else {
            for (int i = 0; i < 40; ++i) {
                c.step();
                if (c.lastWasStrike() && !c.lastByPlayer()) break;
            }
            CHECK(c.lastWasStrike() && !c.lastByPlayer());  // the pet really was hit
        }
        drawCombat(fb, c, ps, es, 0, animBeat, hitBeat, false, CombatSides{});
    };

    // The ramp DIRECTION, read off the pet's own seat across the cue's early frames.
    Framebuffer windEarly(kActiveW, kActiveH), windLate(kActiveW, kActiveH);
    render(/*channel=*/true, 0, -1, windEarly);
    render(/*channel=*/true, 6, -1, windLate);
    CHECK(seatGray(windLate, st.localX, st.localW) >
          seatGray(windEarly, st.localX, st.localW));     // a charge climbs

    Framebuffer hitEarly(kActiveW, kActiveH), hitLate(kActiveW, kActiveH);
    render(/*channel=*/false, 0, 0, hitEarly);
    render(/*channel=*/false, 0, 3, hitLate);
    CHECK(seatGray(hitEarly, st.localX, st.localW) >
          seatGray(hitLate, st.localX, st.localW));       // an impact fades

    // The countdown meter: over the charging fighter's head, and over nobody else's.
    // It is the countable channel, and the one that holds when the flash is at its dimmest.
    Framebuffer none(kActiveW, kActiveH);
    render(/*channel=*/false, 0, -1, none);
    const int markX0 = std::max(0, st.localX), markW = st.localW;
    CHECK(anyLitGray(windEarly, markX0, 30, markX0 + markW, 78));
    CHECK(!anyLitGray(none, markX0, 30, markX0 + markW, 78));
}

// Release gate: the ransom pool (combat.h ransomPool) must stay readable in grayscale.
// Both of its meanings ride on a non-colour channel — HOW MUCH is the green gauge's fill
// width, HOW LONG is the count of lit blips beside it — so throwing the hue away has to
// leave a bigger pool wider and a longer countdown brighter.
void test_combat_ransom_pool_grayscale() {
    ContentRegistry r = ContentRegistry::embedded();

    // The pool row sits directly above the local Health gauge (combat_screen.cpp): the
    // gauge spans phX..phX+phW at phY-7, the kRansomHoldTurns blips the strip to its right.
    const int phX = 8 + textWidth("YOU") + 6, phW = 110;
    const int rowY = 175 - 7, rowH = 5;
    const int blipX = phX + phW + 6, blipW = kRansomHoldTurns * 7;
    // Summed LUMINANCE, not a lit-pixel count: the blips code their state as calm-green
    // (0.67) against ink-dim (0.47), both of which read as "lit" — brightness is the
    // channel that separates them once the hue is gone.
    auto grayIn = [&](const Framebuffer& fb, int x0, int w) {
        float sum = 0;
        for (int y = rowY; y < rowY + rowH; ++y)
            for (int x = x0; x < x0 + w; ++x) sum += luminance(fb.get(x, y));
        return sum;
    };
    auto renderPool = [&](int pool, int turnsLeft, Framebuffer& fb) {
        Combatant p = mkCombatant(r, "P", 100, 10, {"quick_jab"});
        p.ransomPool = pool;                             // begin() carries both through
        p.ransomTurnsLeft = turnsLeft;
        Combatant e = mkCombatant(r, "E", 100, 10, {"quick_jab"});
        Combat c; c.begin(p, e, Combat::Stakes::Safe, 1);   // Ongoing: no result banner
        drawCombat(fb, c, nullptr, nullptr, 0, 0, 0, false, CombatSides{});
    };

    Framebuffer none(kActiveW, kActiveH), small(kActiveW, kActiveH), big(kActiveW, kActiveH);
    renderPool(0, 0, none);
    renderPool(4, 1, small);
    renderPool(400, kRansomHoldTurns, big);

    // Against bare paper (nothing drawn while there's no pool), each step up in pool size
    // and in turns owed has to be brighter than the last.
    CHECK(grayIn(small, phX, phW) > grayIn(none, phX, phW));
    CHECK(grayIn(big, phX, phW) > grayIn(small, phX, phW));         // fill width = how much
    CHECK(grayIn(small, blipX, blipW) > grayIn(none, blipX, blipW));
    CHECK(grayIn(big, blipX, blipW) > grayIn(small, blipX, blipW)); // lit blips = how long
}

// Release gate: a grayscale screenshot of LINK must stay fully readable. The verdict
// is the one status meaning on this screen, and it is carried by a WORD — so a win and
// a loss must differ in lit pixels with the hue thrown away.
void test_pvp_link_screen_verdict_grayscale() {
    const uint8_t macA[6] = {0x02, 0, 0, 0, 0, 0x01};
    const uint8_t macB[6] = {0x02, 0, 0, 0, 0, 0x02};

    auto renderVerdict = [&](bool asWinner, Framebuffer& fb) {
        Game a{StartMode::Hatched, "paypup"};
        Game b{StartMode::Hatched, "pingcub"};
        a.setLocalRadioMac(macA);
        b.setLocalRadioMac(macB);
        uint32_t t = 0;
        a.tick(t += 1000);
        b.tick(t);
        enterHackerSlot(a, HackerSlotId::Link);
        enterHackerSlot(b, HackerSlotId::Link);
        hearBeacon(a, macB, "OPERATOR_B");
        hearBeacon(b, macA, "OPERATOR_A");
        uint64_t keys[8];
        a.pvpChallengeableKeys(keys, 8);
        a.pvpChallenge(keys[0]);
        relayPvp(a, macA, b, macB);
        b.pvpAcceptChallenge();
        relayPvp(b, macB, a, macA);
        relayPvp(a, macA, b, macB);
        int steps = 0;
        while (a.combat().outcome() == Combat::Outcome::Ongoing && steps++ < 500) {
            a.onButton(press(Button::A));
            b.onButton(press(Button::A));
        }
        a.onButton(press(Button::B));
        b.onButton(press(Button::B));
        // Whichever engine holds the outcome we were asked for renders the panel.
        Game& shown = a.pvpLocalWon() == asWinner ? a : b;
        CHECK(shown.pvpLocalWon() == asWinner);
        shown.render(fb);
    };

    Framebuffer won(kActiveW, kActiveH), lost(kActiveW, kActiveH);
    renderVerdict(true, won);
    renderVerdict(false, lost);

    // Desaturated, the two panels must still be distinguishable — if the only
    // difference were hue, these would match.
    int diff = 0;
    for (int y = 0; y < kActiveH; ++y)
        for (int x = 0; x < kActiveW; ++x)
            if (luminance(won.get(x, y)) != luminance(lost.get(x, y))) ++diff;
    CHECK(diff > 0);
}

// In a live fight, the A+C picker only offers the crew row while the player belongs
// to a crew, and committing it arms that crew's charges.
void test_crew_exploit_in_combat_picker() {
    Game g{StartMode::Hatched, "paypup"};
    g.debugStartCombat(/*live=*/true);
    CHECK(g.nav() == Game::Nav::Combat);

    g.onButton({Button::A, true, true});           // A+C -> picker
    CHECK(g.combat().overrideOpen() && g.combat().overrideCrewRows() == 0);
    tapC(g);                  // cancel -> no spend
    CHECK(g.combat().overrideReady());

    g.setHomeNetwork(0xAABBCCDDEEFFull, "HOME");
    CHECK(g.joinCrew(0));
    g.onButton({Button::A, true, true});
    CHECK(g.combat().overrideCrewRows() == 1);
    const int crewRow = g.combat().overrideMoveCount() +
                        static_cast<int>(g.combat().overrideItems().size());
    while (g.combat().overridePick() != crewRow) g.onButton(press(Button::A));
    g.onButton(press(Button::B));
    CHECK(g.combat().player().crewExploit.charges == kCrews[0].exploit.magnitude);
}

// v36: the crew id + home network round-trip, and an unknown crew id loads as
// unaffiliated.
void test_save_v36_crew_roundtrip() {
    SaveData a; std::strcpy(a.activeId, "paypup"); a.generation = 1;
    std::strcpy(a.crewId, "deniers_of_service");
    a.homeNetworkKey = 0x0123456789ABull;
    std::strcpy(a.homeNetworkName, "HOME_AP");
    SaveData out;
    CHECK(deserializeSave(serializeSave(a), out));
    CHECK(std::strcmp(out.crewId, "deniers_of_service") == 0);
    CHECK(out.homeNetworkKey == 0x0123456789ABull);
    CHECK(std::strcmp(out.homeNetworkName, "HOME_AP") == 0);

    MemSaveStore store; store.save(serializeSave(a));
    Game g(StartMode::Hatched, "paypup", &store);
    CHECK(g.crewIndex() == 0 && g.hasHomeNetwork());
    CHECK(std::strcmp(g.homeNetworkName(), "HOME_AP") == 0);

    // A crew id no longer in the table loads as unaffiliated, never as whatever now
    // occupies that row.
    SaveData b = a; std::strcpy(b.crewId, "no_such_crew");
    MemSaveStore store2; store2.save(serializeSave(b));
    Game g2(StartMode::Hatched, "paypup", &store2);
    CHECK(g2.crewIndex() == -1 && g2.hasHomeNetwork());
}

// buildPediaStateJson: pets{} distinguishes "seen" from "hatched"/"locked";
// malbeasts{} reflects defeated > seen > locked; achievements{} reflects the
// unlocked bits. Seeded via a v25 save load (applySave) rather than the runtime
// triggers, so this isolates the JSON-shape half from the trigger-wiring half
// (covered separately below).
void test_pedia_state_json_reveal_states() {
    SaveData a; std::strcpy(a.activeId, "pingcub"); a.generation = 1;
    a.seenCreatures.push_back(SaveId{"malbear"});   // glimpsed, not hatched
    a.malbeastSeen = 1 << 1;                        // Segfault Pup — seen only
    a.malbeastDefeated = 1 << 0;                    // GlitchHog — defeated
    MemSaveStore store; store.save(serializeSave(a));
    Game g(StartMode::Hatched, "paypup", &store);   // hatchedCreature ignored: store wins

    const std::string json = buildPediaStateJson(g);
    CHECK(json.find("\"pingcub\":\"hatched\"") != std::string::npos);
    CHECK(json.find("\"malbear\":\"seen\"") != std::string::npos);
    CHECK(json.find("\"glitchhog\":\"defeated\"") != std::string::npos);
    CHECK(json.find("\"segfault_pup\":\"seen\"") != std::string::npos);
    CHECK(json.find("\"cache_ghoul\":\"locked\"") != std::string::npos);
    CHECK(json.find("\"AIR_GAPPED\":\"incomplete\"") != std::string::npos);

    g.unlockAchievement(ach::kAirGapped);
    const std::string json2 = buildPediaStateJson(g);
    CHECK(json2.find("\"AIR_GAPPED\":\"complete\"") != std::string::npos);
    CHECK(json2.find("\"WORM_WHISPERER\":\"incomplete\"") != std::string::npos);
}

// The raised tally is what the 'Pedia reveals from, so a species stays revealed
// after the pet stops being it. Raise a line by real evolutions and every rung
// behind the live pet must still read "hatched" — the possession-only test this
// replaced re-encrypted each one the moment its successor stamped over it.
void test_pedia_raised_tally_survives_evolution() {
    Game g{StartMode::Hatched, "pingcub"};
    CHECK(g.creatureRaised("pingcub"));
    CHECK(g.speciesRaised() == 1);

    g.debugTriggerEvolution();                        // pingcub -> malbear
    uint32_t t = 0; advanceToReveal(g, t);
    g.onButton(press(Button::B));
    CHECK(g.pet() && std::strcmp(g.pet()->id, "malbear") == 0);
    CHECK(g.creatureRaised("pingcub"));               // the species it USED to be
    CHECK(g.creatureRaised("malbear"));
    CHECK(g.speciesRaised() == 2);

    g.model().setCareMistakes(0);
    g.debugTriggerEvolution();                        // malbear -> bruinforce
    t = 0; advanceToReveal(g, t);
    g.onButton(press(Button::B));
    CHECK(g.speciesRaised() == 3);

    const std::string json = buildPediaStateJson(g);
    CHECK(json.find("\"pingcub\":\"hatched\"") != std::string::npos);
    CHECK(json.find("\"malbear\":\"hatched\"") != std::string::npos);
    CHECK(json.find("\"bruinforce\":\"hatched\"") != std::string::npos);
    CHECK(json.find("\"species_raised\":3") != std::string::npos);
    // The not-taken branch sibling was neither raised nor fought, so it stays locked —
    // walking a line reveals the rungs you walked, not the one you didn't.
    CHECK(!g.creatureRaised("berserkernel"));
    CHECK(json.find("\"berserkernel\":\"locked\"") != std::string::npos);
}

// The splice table's own invariants, so save.h's rules are enforced rather than merely
// written down. A stale row here re-shifts a save that was already correct, which no
// other test would notice.
void test_ladder_inserts_table_invariants() {
    int n = 0;
    const LadderInsert* rows = ladderInserts(n);
    for (int i = 0; i < n; ++i) {
        // Retirement: a row whose blobs the codec no longer opens is dead weight, so
        // raising kOldestAcceptedVersion is what tells you to delete it.
        CHECK(rows[i].sinceVersion > kOldestAcceptedVersion);
        CHECK(rows[i].sinceVersion <= kSaveVersion);
        // A splice lands ON the ladder. Past its end it would be an append, which needs
        // no row at all — so such a row is a mistake, not a no-op.
        CHECK(rows[i].atIndex >= 0);
        CHECK(rows[i].atIndex < kExplSectors);
        // Oldest-first, which is what lets each atIndex read against the ladder its
        // predecessors built instead of being restated whenever a later splice lands.
        if (i > 0) CHECK(rows[i - 1].sinceVersion <= rows[i].sinceVersion);
    }
}

// An area's sector glyph is keyed by its own id, never by its rung. That is the whole
// reason the name lives on the row, and it is exactly the sort of thing a copy-pasted
// area.cpp breaks silently: two areas sharing a glyph still resolves, still draws, and
// shows the wrong picture. Derived here from the id rather than compared against a list,
// so a new area is covered the moment it joins the ladder.
void test_area_icons_are_keyed_by_area_id() {
    for (int i = 0; i < kAreaCount; ++i) {
        const AreaDef& a = area(i);
        CHECK(a.icon && a.icon[0]);
        char want[64];
        std::snprintf(want, sizeof(want), "ICON_SECTOR_%s", a.id);
        for (char* p = want; *p; ++p)
            *p = static_cast<char>(std::toupper(static_cast<unsigned char>(*p)));
        if (std::strcmp(a.icon, want) != 0)
            std::printf("  AREA ICON OFF ITS ID: %s names %s, wanted %s\n", a.id, a.icon,
                        want);
        CHECK(std::strcmp(a.icon, want) == 0);
        // ...and no two areas can end up pointing at one picture.
        for (int j = i + 1; j < kAreaCount; ++j)
            CHECK(std::strcmp(area(j).icon, a.icon) != 0);
    }
}

// The DEFRAG minigame's rules (core/model/stacker.h), driven a press at a time. The model
// is deterministic on purpose — no RNG anywhere — which is what lets a run be replayed
// exactly here, and what makes the variant it backs a test of skill rather than luck.
namespace {
}  // namespace
