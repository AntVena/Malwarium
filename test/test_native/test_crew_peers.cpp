// test_crew_peers.cpp — native gates for the Hacker CREW and the PEERS ledger.
//
// One slice of the native suite; see test_gates.h for the shared fixtures and
// test_main.cpp for the roster that runs these. A save-vNN gate sits with the
// feature whose field it migrates, not in a migrations pile of its own.
#include "test_gates.h"

#include "core/content/effect_text.h"   // effectText — a crew Exploit's page prose
#include "core/ui/layout.h"             // kMargin — the width that prose is wrapped to

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

// Every crew row is a well-formed use of the effect vocabulary: a kind that has a tag
// to print, and a magnitude that agrees with how that kind meters itself. A STICKY kind
// counts nothing, so a number on its row would be one the readout has to drop.
void test_crew_roster_exploits_are_well_formed() {
    CHECK(kCrewCount > 0);
    for (int i = 0; i < kCrewCount; ++i) {
        const CrewExploitDef& x = kCrews[i].exploit;
        CHECK(x.kind != CrewExploitKind::None);
        CHECK(crewExploitTag(x.kind)[0] != '\0');
        if (crewExploitIsSticky(x.kind)) CHECK(x.magnitude == 0);
        else                             CHECK(x.magnitude > 0);
        // ...and the label the three surfaces share says exactly one of the two things.
        char label[16];
        crewExploitLabel(label, sizeof(label), x.kind, x.magnitude);
        CHECK(std::strstr(label, crewExploitTag(x.kind)) == label);
        CHECK((std::strchr(label, 'x') != nullptr) == (x.magnitude > 0));
    }
}

// Escalation banks each landed hit's FINAL damage as Power, for exactly `magnitude`
// hits, and compounds: every charge is worth more than the one before it because it is
// banking a swing the previous charge paid for.
void test_crew_escalation_banks_damage_as_power() {
    ContentRegistry r = ContentRegistry::embedded();
    // One move each, so chooseMove is deterministic and every player turn is the same
    // 24-power swing scaled by whatever Power has been banked so far.
    Combatant p = mkCombatant(r, "P", 100, 100, {"rootkit_strike"});
    Combatant e = mkCombatant(r, "E", 5000, 1, {"checksum_guard"});
    Combat cb;
    cb.begin(p, e, Combat::Stakes::Safe, 7);

    cb.openOverride({}, CrewExploit{"ESCALATION",
                                    CrewExploitKind::PowerByDamageDealt, 3});
    while (cb.overridePick() != cb.overrideMoveCount()) cb.cycleOverride();
    cb.commitOverride();
    CHECK(cb.player().crewExploit.charges == 3);
    CHECK(cb.player().stackPowerBonus == 0);   // arming alone banks nothing

    int guard = 0;
    while (cb.player().crewExploit.charges > 0 && guard++ < 400) cb.step();
    // 24 at 100% -> +24; 24 at 124% = 29 -> +29; 24 at 153% = 36 -> +36.
    CHECK(cb.player().stackPowerBonus == 24 + 29 + 36);

    // Spent: the fourth swing lands harder than any of the three, and banks nothing.
    const int banked = cb.player().stackPowerBonus;
    for (int i = 0; i < 6 && guard++ < 400; ++i) cb.step();
    CHECK(cb.player().stackPowerBonus == banked);
}

// Net Neutrality snaps the live stat leans back to what the pet walked in with, and
// then holds them there: a siphon that lands afterwards takes nothing — and, because a
// steal is a transfer, pays the thief nothing either.
void test_crew_net_neutrality_resets_then_floors_the_leans() {
    ContentRegistry r = ContentRegistry::embedded();
    Combatant p = mkCombatant(r, "P", 5000, 5, {"checksum_guard"});
    Combatant e = mkCombatant(r, "E", 5000, 50, {"spear_strike"});
    e.shieldHp = 500;   // the bubble the volatile half of the steal track is gated on
    Combat cb;
    cb.begin(p, e, Combat::Stakes::Safe, 11, /*forceEnemyFirst=*/true);
    const int pow0 = cb.player().powerMultPct;
    const float spd0 = cb.player().speed;

    int guard = 0;
    while (cb.player().powerMultPct == pow0 && guard++ < 400) cb.step();
    CHECK(cb.player().powerMultPct < pow0);      // Power siphoned...
    CHECK(cb.player().speed < spd0);             // ...and speed with it
    const int thiefPow = cb.enemy().powerMultPct;

    cb.openOverride({}, CrewExploit{"NET NEUTRALITY",
                                    CrewExploitKind::ResetStatsAndFloor, 0});
    while (cb.overridePick() != cb.overrideMoveCount()) cb.cycleOverride();
    cb.commitOverride();
    CHECK(cb.player().powerMultPct == pow0);     // snapped back to the fight-start lean
    CHECK(cb.player().speed == spd0);

    // ...and it stays there through every hit that follows, with nothing crossing over.
    for (int i = 0; i < 40 && guard++ < 400; ++i) cb.step();
    CHECK(cb.player().powerMultPct == pow0);
    CHECK(cb.player().speed == spd0);
    CHECK(cb.enemy().powerMultPct == thiefPow);  // the siphon comes back empty
}

// Malbeast In The Middle copies the enemy's own self-buffs onto the player as they
// land — and only once it has been fired.
void test_crew_mitm_copies_enemy_buffs() {
    ContentRegistry r = ContentRegistry::embedded();
    Combatant p = mkCombatant(r, "P", 5000, 1, {"quick_jab"});
    // RSA Vault: a 20-power brace plus +12% Cipher-track Defense on cast.
    Combatant e = mkCombatant(r, "E", 5000, 20, {"rsa_vault"});
    Combat cb;
    cb.begin(p, e, Combat::Stakes::Safe, 5, /*forceEnemyFirst=*/true);

    cb.step();                                   // an enemy cast before the Exploit
    CHECK(cb.enemy().guard == 20 && cb.enemy().stackDefenseBonus == 12);
    CHECK(cb.player().guard == 0 && cb.player().stackDefenseBonus == 0);

    cb.openOverride({}, CrewExploit{"MALBEAST IN THE MIDDLE",
                                    CrewExploitKind::MirrorEnemyBuffs, 0});
    while (cb.overridePick() != cb.overrideMoveCount()) cb.cycleOverride();
    cb.commitOverride();
    CHECK(cb.player().guard == 0);               // firing copies nothing already cast

    int guard = 0;
    while (cb.player().guard == 0 && guard++ < 400) cb.step();
    CHECK(cb.player().guard == 20);              // the next brace is copied whole...
    CHECK(cb.player().stackDefenseBonus == 12);  // ...and so is the stack it carried
}

// Backup Plan B catches the blow that would have ended the fight: half of max back,
// the overkill paid out as Power scaled by the turns still on the clock, one shot.
void test_crew_backup_plan_b_saves_and_rallies() {
    ContentRegistry r = ContentRegistry::embedded();
    Combatant p = mkCombatant(r, "P", 100, 1, {"quick_jab"});
    Combatant e = mkCombatant(r, "E", 5000, 50, {"rootkit_strike"});   // 24 a swing
    Combat cb;
    // Carried in at 10 Health, so the first swing buries the pet 14 past 0.
    cb.begin(p, e, Combat::Stakes::Safe, 3, /*forceEnemyFirst=*/true,
             /*carryPlayerHealth=*/10);
    cb.openOverride({}, CrewExploit{"BACKUP PLAN B",
                                    CrewExploitKind::DeathSaveRally, 3});
    while (cb.overridePick() != cb.overrideMoveCount()) cb.cycleOverride();
    cb.commitOverride();
    CHECK(cb.player().crewExploit.turns == 3);

    cb.step();
    CHECK(cb.outcome() == Combat::Outcome::Ongoing);
    CHECK(cb.player().health == 50);                  // restored TO half of max...
    CHECK(cb.player().stackPowerBonus == 14 * 3);     // ...and paid the overkill back
    CHECK(cb.player().crewExploit.kind == CrewExploitKind::None);   // consumed
    CHECK(!cb.player().crewExploit.live());
}

// ...and the clock is the incoming turns it covers. Three of the malbeast's turns pass
// without it being needed, and the blow that comes after is simply fatal.
void test_crew_backup_plan_b_clock_runs_out() {
    ContentRegistry r = ContentRegistry::embedded();
    Combatant p = mkCombatant(r, "P", 200, 1, {"quick_jab"});
    Combatant e = mkCombatant(r, "E", 5000, 50, {"rootkit_strike"});   // 9 swings to KO
    Combat cb;
    cb.begin(p, e, Combat::Stakes::Safe, 3, /*forceEnemyFirst=*/true);
    cb.openOverride({}, CrewExploit{"BACKUP PLAN B",
                                    CrewExploitKind::DeathSaveRally, 3});
    while (cb.overridePick() != cb.overrideMoveCount()) cb.cycleOverride();
    cb.commitOverride();

    cb.step();
    CHECK(cb.player().crewExploit.turns == 2);   // one enemy turn, one off the clock
    int guard = 0;
    while (cb.player().crewExploit.turns > 0 && guard++ < 400) cb.step();
    CHECK(!cb.player().crewExploit.live());      // expired, unfired
    CHECK(cb.player().health > 0 && cb.outcome() == Combat::Outcome::Ongoing);

    while (cb.outcome() == Combat::Outcome::Ongoing && guard++ < 400) cb.step();
    CHECK(cb.outcome() == Combat::Outcome::Lose);
    CHECK(cb.player().stackPowerBonus == 0);     // it never got to rally
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

// The CREW screen end-to-end on buttons, all four views: claim a home network from the
// ledger the pet built by walking, browse a SIDE, open a crew's page, and enlist from
// it. Every view renders, and C walks back up them one at a time rather than dropping
// out of the screen from wherever it is pressed.
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
    CHECK(g.crewView() == Game::CrewView::Hub);    // entry is always the top view
    g.render(fb);
    CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));

    // Hub row 0 is HOME NET: B opens the picker over it, B again takes the focused
    // network and drops back to the Hub.
    g.onButton(press(Button::B));
    CHECK(g.crewView() == Game::CrewView::Picker);
    g.render(fb);
    CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));
    g.onButton(press(Button::B));
    CHECK(g.crewView() == Game::CrewView::Hub);
    CHECK(g.hasHomeNetwork());
    CHECK(std::strcmp(g.homeNetworkName(), "Neighbour") == 0);

    // Hub rows 1 and 2 are the two sides. Crew 0 is Blue, so step past RED to it.
    CHECK(kCrews[0].team == CrewTeam::Blue);
    g.onButton(press(Button::A));
    g.onButton(press(Button::A));
    g.onButton(press(Button::B));
    CHECK(g.crewView() == Game::CrewView::Team);
    g.render(fb);
    CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));

    // The side's first row is its first crew in table order, and B opens its page.
    g.onButton(press(Button::B));
    CHECK(g.crewView() == Game::CrewView::Detail);
    g.render(fb);
    CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));

    // B is the page's one verb: enlist, then resign.
    g.onButton(press(Button::B));
    CHECK(g.crewIndex() == 0);
    g.render(fb);                                  // ...and the page redraws as JOINED
    CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));
    g.onButton(press(Button::B));
    CHECK(g.crewIndex() == -1);

    // C steps back up one view at a time, and only leaves the screen from the Hub.
    g.onButton(press(Button::C));
    CHECK(g.crewView() == Game::CrewView::Team && g.nav() == Game::Nav::Submenu);
    g.onButton(press(Button::C));
    CHECK(g.crewView() == Game::CrewView::Hub && g.nav() == Game::Nav::Submenu);
    g.onButton(press(Button::C));
    CHECK(g.nav() == Game::Nav::Cursor);
}

// The Hub's two side rows are the fixed Red/Blue pair, and each opens its OWN side —
// the roster is filtered, not reordered, so a crew is found wherever it sits in the
// table. Enlisting from the Red screen cannot land you in a Blue crew.
void test_crew_sides_filter_the_roster() {
    // The filter is a pure function of the table, so assert it there first: the two
    // sides partition kCrews exactly, with nothing counted twice or dropped.
    CHECK(crewTeamCount(CrewTeam::Red) + crewTeamCount(CrewTeam::Blue) == kCrewCount);
    for (int t = 0; t < 2; ++t) {
        const CrewTeam team = t == 0 ? CrewTeam::Red : CrewTeam::Blue;
        const int n = crewTeamCount(team);
        CHECK(crewByTeam(team, n) == -1);            // one past the end is nothing
        int last = -1;
        for (int i = 0; i < n; ++i) {
            const int idx = crewByTeam(team, i);
            CHECK(idx > last);                       // table order, no repeats
            CHECK(kCrews[idx].team == team);
            last = idx;
        }
    }

    // ...and end to end: the Red row opens Red, and every crew it can reach is Red.
    Game g{StartMode::Hatched};
    g.setHomeNetwork(0x001122334455ull, "HOME_AP");
    while (g.nav() != Game::Nav::Idle) g.onButton(press(Button::C));
    g.onButton({Button::A, true, true});
    g.onButton(press(Button::A));
    while (hackerCarouselSlots()[g.cursor()].id != HackerSlotId::Crew)
        g.onButton(press(Button::A));
    g.onButton(press(Button::B));

    g.onButton(press(Button::A));                    // Hub row 1 = RED
    g.onButton(press(Button::B));
    CHECK(g.crewView() == Game::CrewView::Team);
    Framebuffer fb(kActiveW, kActiveH);
    for (int i = 0; i < crewTeamCount(CrewTeam::Red); ++i) {
        g.onButton(press(Button::B));                // open row i's page
        CHECK(g.crewView() == Game::CrewView::Detail);
        g.render(fb);
        CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));
        g.onButton(press(Button::B));                // enlist from it
        CHECK(g.activeCrew() && g.activeCrew()->team == CrewTeam::Red);
        g.onButton(press(Button::B));                // resign
        CHECK(g.crewIndex() == -1);
        g.onButton(press(Button::C));                // back to the side
        g.onButton(press(Button::A));                // next row
    }
}

// A crew page can't offer a button that would silently do nothing: without a home
// network the verb is the precondition, and B changes nothing.
void test_crew_detail_gates_enlist_on_the_home_net() {
    Game g{StartMode::Hatched};
    while (g.nav() != Game::Nav::Idle) g.onButton(press(Button::C));
    g.onButton({Button::A, true, true});
    g.onButton(press(Button::A));
    while (hackerCarouselSlots()[g.cursor()].id != HackerSlotId::Crew)
        g.onButton(press(Button::A));
    g.onButton(press(Button::B));
    CHECK(!g.hasHomeNetwork());

    g.onButton(press(Button::A));                    // past HOME NET to a side
    g.onButton(press(Button::B));
    g.onButton(press(Button::B));                    // ...and into a crew's page
    CHECK(g.crewView() == Game::CrewView::Detail);
    Framebuffer fb(kActiveW, kActiveH);
    g.render(fb);                                    // renders the gated verb
    CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));

    g.onButton(press(Button::B));                    // the verb is inert
    CHECK(g.crewIndex() == -1);
    CHECK(g.crewView() == Game::CrewView::Detail);   // ...and doesn't bounce you out
}

// Every crew's page has a description and it resolves its template — the same gate
// test_effect_text_templates_resolve holds the other content types to, extended to the
// one table that now authors prose of its own.
//
// The line bound is an AUTHORING budget, not a correctness one: the detail page pages
// its description on A rather than truncating it, so a longer row would still be
// readable — it would just cost a press nobody should have to spend on four sentences.
void test_crew_exploit_descriptions_resolve_and_fit() {
    for (int i = 0; i < kCrewCount; ++i) {
        const EffectText prose = effectText(kCrews[i].exploit);
        CHECK(!prose.empty());
        CHECK(!prose.atCap());
        CHECK(std::strchr(prose.c_str(), '{') == nullptr);
        CHECK(std::strchr(prose.c_str(), '}') == nullptr);
        CHECK(textWrapLines(prose.c_str(), kActiveW - 2 * kMargin) <= 6);
        // ...and the motto, which the page WRAPS beside the team glyph rather than
        // marqueeing. Three lines is what it reserves; a fourth would be cut, and a
        // motto is exactly the kind of copy nobody notices losing the end of.
        CHECK(textWrapLines(kCrews[i].tagline,
                            kActiveW - kMargin - (kMargin + 16 + 8)) <= 3);
    }
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
