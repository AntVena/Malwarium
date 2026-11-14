// test_rigshop.cpp: native gates for the Hacker SHOP rig upgrades and the ITEMS type-tabs/picker.
//
// One slice of the native suite; see test_gates.h for the shared fixtures and
// test_main.cpp for the roster that runs these. A save-vNN gate sits with the
// feature whose field it migrates, not in a migrations pile of its own.
#include "test_gates.h"

#include "core/ui/layout.h"   // kLineH: the MERGE HUB window mirrors the list metrics

// An armed Backup Drive save rides into a REAL fight (Game::buildPlayerCombatant, not
// the raw makePlayerCombatant the combat-model tests use): the wiring that matters,
// since the Sim Battle it's stripped from is the only fight that doesn't carry it.
void test_backup_drive_save_armed_into_wild_combat() {
    Game g{StartMode::Hatched, "paypup"};
    useItem(g, "backup_drive");
    CHECK(g.backupShieldArmed());
    // Consuming the last Backup Drive drops useItem() into Nav::Submenu (the ITEMS
    // "item left the list" case): back out to the carousel before the EXPL walk,
    // which assumes it's starting from the carousel/idle layer.
    tapC(g);
    walkToEncounter(g);
    CHECK(g.nav() == Game::Nav::Combat);
    CHECK(g.combat().player().itemShield);
}

// Auto Backup (Rig Shop g): the purchased upgrade arms the death-save for free the
// moment explore-mode starts, and never reaches into the Vault for an actual drive.
void test_rig_auto_backup_arms_save_on_explore() {
    Game g{StartMode::Hatched, "paypup"};
    const int drives = g.inventory().count("backup_drive");
    enterWalk(g);
    CHECK(!g.backupShieldArmed());            // nothing free without the upgrade
    g.liveOperator().bits = kRigAutoBackupCost;
    g.buyRigUpgrade(kRigRowAutoBackup);
    CHECK(!g.backupShieldArmed());            // buying alone arms nothing...
    enterWalk(g);                             // ...arming a sub-area does
    CHECK(g.backupShieldArmed());
    CHECK(g.inventory().count("backup_drive") == drives);   // and it cost no item
}

// Continuous Auto-Backup (Rig Shop h) re-arms mid-run: every resolved explore event
// puts a fresh save up, so one purchase covers a whole walk rather than its first fight.
void test_rig_continuous_backup_rearms_mid_run() {
    Game g{StartMode::Hatched, "paypup"};
    const int drives = g.inventory().count("backup_drive");
    enterWalk(g);
    g.returnToExplore();                 // a resolved event hands back to the habitat
    CHECK(!g.backupShieldArmed());            // ...which arms nothing on its own
    g.liveOperator().bits = kRigContinuousBackupCost;
    g.buyRigUpgrade(kRigRowContinuousBackup);
    g.returnToExplore();
    CHECK(g.backupShieldArmed());             // now the next event re-arms it, free
    CHECK(g.inventory().count("backup_drive") == drives);
}

// Arming explore-mode drops back to the IDLE habitat with the mode running:
// exploreActive is true, the sector is armed, the streak starts at 0, and the badge
// renders. Summoning the carousel pauses stepping; C in EXPL doesn't cancel the mode.
void test_explore_arm_returns_to_idle() {
    Game g{StartMode::Hatched};
    enterWalk(g);
    CHECK(g.nav() == Game::Nav::Idle);
    CHECK(g.exploreActive());
    CHECK(g.exploreSector() == 0);
    CHECK(g.exploreStreak() == 0);
    Framebuffer fb(kActiveW, kActiveH);
    g.render(fb);
    CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));   // the badge draws over the habitat
    // The A+C control chord opens the explore-control overlay; STOP EXPLORE is its
    // last row, reached by cycling with A and done with B.
    g.onButton(chordAC());
    CHECK(g.nav() == Game::Nav::ExploreControl);
    tapC(g);                       // C backs out, walk untouched
    CHECK(g.nav() == Game::Nav::Idle && g.exploreActive());
    stopExplore(g);                                     // ...the STOP row does cancel it
    CHECK(g.nav() == Game::Nav::Idle);
    CHECK(!g.exploreActive());
    CHECK(g.exploreStreak() == 0);
}

// A guaranteed explore step types a wild encounter among its outcomes. In
// the hands-off auto mode there's no human at a Fight/Flee intro, so the encounter
// AUTO-STARTS the fight: live wild combat, no decision screen.
void test_walk_event_roll_reaches_encounter() {
    Game g{StartMode::Hatched};
    walkToEncounter(g);
    CHECK(g.nav() == Game::Nav::Combat);
    CHECK(g.combat().stakes() == Combat::Stakes::Live);
    Framebuffer fb(kActiveW, kActiveH);
    g.render(fb);
    CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));
}

// re-farming an already-CLEARED sub-area gives diminishing non-Bits
// rewards. Three contracts:
//   (1) the pure decay refarmDropScalePct (100 → floor, monotonic, guarded);
//   (2) a wild win in a CLEARED sub-area advances that sub's re-farm count while Bits
//       still flow;
//   (3) a win in an UNCLEARED sub-area leaves the count at 0 (full drops).
static void winAutoExploreFight(Game& g) {   // resolve + auto-dismiss the wild fight
    uint32_t t = 0;
    for (int i = 0; i < 400 && g.combat().outcome() == Combat::Outcome::Ongoing; ++i)
        g.tick(t += kHeartbeatMs);
    CHECK(g.combat().outcome() == Combat::Outcome::Win);
    // Bounded only so a stuck fight fails as a test rather than hanging: the loop
    // exits the moment the nav leaves Combat. The hold is the reveal hold PLUS the
    // beaten rival's dissolve, which the auto-dismiss waits out (combatDissolveRunning).
    for (int i = 0; i < 200 && g.nav() == Game::Nav::Combat; ++i)
        g.tick(t += kHeartbeatMs);
}
void test_refarm_diminishing_rewards() {
    // (1) The pure decay.
    CHECK(Game::refarmDropScalePct(0) == 100);
    CHECK(Game::refarmDropScalePct(1) == 100 - kRefarmDropDecayStep);
    CHECK(Game::refarmDropScalePct(2) < Game::refarmDropScalePct(1));   // monotonic down
    CHECK(Game::refarmDropScalePct(10000) == kRefarmDropFloorPct);      // floored
    CHECK(Game::refarmDropScalePct(-3) == 100);                         // guarded
    // (2) A cleared sub-area accrues a re-farm win ONLY once Bandwidth is spent:
    //     with Bandwidth drained to 0 the decay curve advances the count; Bits still flow.
    {
        Game g{StartMode::Hatched};
        walkToEncounter(g);
        const int S = g.exploreSector(), U = g.exploreSub();
        g.liveOperator().world.subCleared[S][U] = true;          // this sub is a re-farm ground now
        g.setBandwidth(0);                    // ...and Bandwidth is spent (decay path)
        CHECK(g.petRecord().armed.subRefarmCount[S][U] == 0);
        const int bits0 = g.bits();
        winAutoExploreFight(g);
        CHECK(g.petRecord().armed.subRefarmCount[S][U] == 1);   // the re-farm win advanced the count
        CHECK(g.bits() > bits0);                    // Bits never decay
    }
    // (3) An uncleared sub-area never accrues (it's a first clear, not a re-farm).
    {
        Game g{StartMode::Hatched};
        walkToEncounter(g);
        const int S = g.exploreSector(), U = g.exploreSub();
        winAutoExploreFight(g);
        CHECK(g.petRecord().armed.subRefarmCount[S][U] == 0);
    }
    // (4). The curve is PER-PET, not per-device: a farmed-out count resets
    //     when a new egg is laid, so a fresh pet finds the area undepleted again.
    {
        Game g{StartMode::Hatched};
        walkToEncounter(g);
        const int S = g.exploreSector(), U = g.exploreSub();
        g.liveOperator().world.subCleared[S][U] = true;
        g.setBandwidth(0);                        // decay path
        winAutoExploreFight(g);
        CHECK(g.petRecord().armed.subRefarmCount[S][U] == 1);       // this pet depleted it a notch
        g.resetToHatch();                              // a new egg = a new pet
        pickFirstEggLine(g);                           // reset re-enters line-select
        CHECK(g.petRecord().armed.subRefarmCount[S][U] == 0);       // ...starts the area fresh
    }
}

// Bandwidth is a per-fight FRAGMENTATION SHIELD:
// ANY resolved exploration fight (first-clear, re-farm, or DeepWeb) spends 1 charge to
// skip the corruption tax; when the pool is dry the tax bites. Contracts:
//   (1) a cleared-sub farm win SPENDS 1 charge, keeps loot full, and FREEZES the
// decay count (while a charge covered the fight);
// (2) with Bandwidth at 0, the decay curve applies + the count advances;
//   (3) Bandwidth REGENERATES over real elapsed time on the heartbeat;
//   (4) a FIRST-CLEAR (uncleared-sub) win now ALSO spends a charge (the shield covers
//       every fight, not just farming);
//   (5) a DeepWeb Dive win ALSO spends a charge (it's prime farming: protect the pet).
void test_bandwidth_farming_resource() {
    // (1) A charge is spent, loot stays full, the decay count is frozen.
    {
        Game g{StartMode::Hatched};
        walkToEncounter(g);
        const int S = g.exploreSector(), U = g.exploreSub();
        g.liveOperator().world.subCleared[S][U] = true;
        const int bw0 = g.bandwidth();
        CHECK(bw0 == kBandwidthMax);                   // fresh pool
        winAutoExploreFight(g);
        CHECK(g.bandwidth() == bw0 - 1);               // one farm win = one charge spent
        CHECK(g.petRecord().armed.subRefarmCount[S][U] == 0);       // ...and the decay count is frozen
    }
    // (2) Once depleted, the decay resumes (count advances, Bandwidth stays 0).
    {
        Game g{StartMode::Hatched};
        walkToEncounter(g);
        const int S = g.exploreSector(), U = g.exploreSub();
        g.liveOperator().world.subCleared[S][U] = true;
        g.setBandwidth(0);
        winAutoExploreFight(g);
        CHECK(g.bandwidth() == 0);                     // stays empty
        CHECK(g.petRecord().armed.subRefarmCount[S][U] == 1);       // decay path advances the count
    }
    // (3) Regeneration over real time: a depleted pool trickles back on the heartbeat.
    {
        Game g{StartMode::Hatched};
        g.setBandwidth(0);
        // Advance well past kBandwidthRegenMinutesPerPoint so at least one point returns.
        const uint32_t ms = (kBandwidthRegenMinutesPerPoint + 1) * 60u * 1000u;
        g.tick(ms);
        CHECK(g.bandwidth() >= 1);
        CHECK(g.bandwidth() <= kBandwidthMax);         // never over the cap
    }
    // (4) A first-clear (UNCLEARED sub) win now ALSO spends a charge (the shield covers
    //     every fight), while leaving the re-farm count untouched (it's not a re-farm).
    {
        Game g{StartMode::Hatched};
        walkToEncounter(g);
        const int S = g.exploreSector(), U = g.exploreSub();
        const int bw0 = g.bandwidth();
        winAutoExploreFight(g);                        // sub is NOT cleared → shield spend
        CHECK(g.bandwidth() == bw0 - 1);
        CHECK(g.petRecord().armed.subRefarmCount[S][U] == 0);       // ...not a re-farm, count stays 0
    }
    // (5) A DeepWeb Dive win ALSO spends a charge (prime farming: protect the pet).
    {
        Game g{StartMode::Hatched, "bruinforce"};
        for (int a = 0; a < kExplSectors; ++a) g.liveOperator().world.sectorCleared[a] = true;
        g.startDeepWebDive();
        CHECK(g.inDeepWebDive());
        const int bw0 = g.bandwidth();
        uint32_t t = 0; int guard = 0;
        while (g.nav() != Game::Nav::Combat && guard++ < 400) {
            switch (g.nav()) {
                case Game::Nav::Idle: if (g.inDeepWebDive()) pingExplore(g); else guard = 400; break;
                case Game::Nav::Wifi: g.onButton(press(Button::B)); break;
                case Game::Nav::Shop: tapC(g); break;
                case Game::Nav::ModShop: tapC(g); break;
                default: tapC(g); break;
            }
            (void)t;
        }
        CHECK(g.nav() == Game::Nav::Combat);
        winAutoExploreFight(g);
        CHECK(g.bandwidth() == bw0 - 1);               // DeepWeb spends the shield too
    }
    // (6) Corruption shield: ANY won exploration fight with Bandwidth left never rolls the
    // fragmentation (corruption) tax. The spent charge keeps the pet clean.
    //     So a bigger pool makes exploring safer, not just farming. (First-clear here.)
    {
        Game g{StartMode::Hatched};
        walkToEncounter(g);
        CHECK(g.bandwidth() > 0);
        const int frag0 = g.model().fragmentation();
        winAutoExploreFight(g);                        // first-clear win, Bandwidth shielded
        CHECK(g.model().fragmentation() == frag0);     // ...no corruption accrued
    }
    // (7) With Bandwidth exhausted, the shield is gone: the tax can now bite
    //     (a first-clear win at 0 charges leaves the pet exposed; fragmentation may rise).
    {
        Game g{StartMode::Hatched};
        walkToEncounter(g);
        g.setBandwidth(0);
        winAutoExploreFight(g);
        CHECK(g.bandwidth() == 0);                      // nothing to spend, stays empty
    }
}

// A+C flips PET ↔ HACKER at the top level, reusing the carousel/submenu
// contracts. The flip only fires from Idle/Cursor; on the HACKER face B enters a live
// slot (PROFILE/SHOP) but is inert on an inaccessible one; A+C always returns.
void test_hacker_face_toggle() {
    Game g{StartMode::Hatched};
    CHECK(g.face() == Game::Face::Pet && g.nav() == Game::Nav::Idle);

    // A+C from idle → the Hacker face home (still Nav::Idle, but the hacker face now).
    g.onButton({Button::A, true, true});
    CHECK(g.face() == Game::Face::Hacker && g.nav() == Game::Nav::Idle);

    // toggleFace() already lands the cursor on the first ACCESSIBLE slot (PROFILE);
    // walk (by id, not index: order is a display detail) to PROFILE and B enters it.
    g.onButton(press(Button::A));
    while (hackerCarouselSlots()[g.cursor()].id != HackerSlotId::Profile)
        g.onButton(press(Button::A));
    g.onButton(press(Button::B));
    CHECK(g.nav() == Game::Nav::Submenu);               // PROFILE opened
    tapC(g);
    CHECK(g.nav() == Game::Nav::Cursor);                // C backs to the hacker carousel

    // B on an inaccessible slot (SCAN) is inert: stays on the carousel.
    while (hackerCarouselSlots()[g.cursor()].id != HackerSlotId::Scan)
        g.onButton(press(Button::A));
    CHECK(!hackerCarouselSlots()[g.cursor()].accessible);
    g.onButton(press(Button::B));
    CHECK(g.nav() == Game::Nav::Cursor);                // no entry

    // A+C from the hacker carousel returns to the pet face (idle habitat).
    g.onButton({Button::A, true, true});
    CHECK(g.face() == Game::Face::Pet && g.nav() == Game::Nav::Idle);
}

// a. The Hacker SHOP "Increase Bandwidth" upgrade: a buy spends Bits, raises
// the farming-pool CAP (bandwidthMax) by one step, persists (bwUpgradeCount), and the
// price climbs per purchase. A buy is inert when the wallet can't afford it.
void test_hacker_shop_bandwidth_upgrade() {
    Game g{StartMode::Hatched};
    const int cap0 = g.bandwidthMax();
    const int cost0 = g.bandwidthUpgradeCost();
    CHECK(cost0 == kBandwidthUpgradeBaseCost);          // first purchase = base price
    g.liveOperator().bits = cost0 + 10;

    g.buyRigUpgrade(kRigRowBandwidth);
    CHECK(g.bwUpgradeCount() == 1);
    CHECK(g.bandwidthMax() == cap0 + kBandwidthUpgradeStep);
    CHECK(g.bits() == 10);                              // Bits deducted
    CHECK(g.bandwidthUpgradeCost() == cost0 + kBandwidthUpgradeCostStep);  // price climbs

    // Now broke → a second buy is inert (no cap change, no Bits change).
    const int cap1 = g.bandwidthMax();
    g.buyRigUpgrade(kRigRowBandwidth);
    CHECK(g.bwUpgradeCount() == 1 && g.bandwidthMax() == cap1 && g.bits() == 10);
}

// The Link Aggregator (Rig Shop n): a regen tick pays a percentage of the SPENT pool
// instead of a flat point, rounded UP to whole Bandwidth. Nothing changes for a rig
// whose hole is under 100 (the slice rounds to the +1 every rig gets); a deeply-
// upgraded pool climbs out in proportion to how empty it is.
void test_rig_link_aggregator_regen() {
    Game g{StartMode::Hatched};
    // The row's whole audience: a pool bought up 240 times, which is what makes 1% of
    // the hole worth more than the flat point.
    g.liveOperator().bits = 4000000;
    for (int i = 0; i < 240; ++i) g.buyRigUpgrade(kRigRowBandwidth);
    const int cap = g.bandwidthMax();
    CHECK(cap == kBandwidthMax + 240 * kBandwidthUpgradeStep);

    const uint32_t tick = g.bandwidthRegenMinutes() * 60u * 1000u;
    uint32_t now = 0;

    // Unbought, even that pool trickles back one point a tick.
    g.setBandwidth(cap - 200);
    CHECK(g.bandwidthRegenAmount() == 1);
    g.tick(now += tick);
    CHECK(g.bandwidth() == cap - 199);

    // Bought: 200 spent pays 2 a tick, and the row is a one-time unlock.
    g.liveOperator().bits = kLinkAggregatorCost + 7;
    g.buyRigUpgrade(kRigRowLinkAggregator);
    CHECK(g.linkAggregatorUnlocked());
    CHECK(g.bits() == 7);                                    // Bits deducted
    CHECK(!g.operatorRecord().rig.offered(kRigRowLinkAggregator));         // ...and nothing left to buy
    g.setBandwidth(cap - 200);
    CHECK(g.bandwidthRegenAmount() == 2);
    g.tick(now += tick);
    CHECK(g.bandwidth() == cap - 198);

    // Whole points, so a part-slice rounds UP rather than away: 150 spent pays 2, and
    // the price of the row is a power of two like every other sticker in the shop.
    g.setBandwidth(cap - 150);
    CHECK(g.bandwidthRegenAmount() == 2);
    CHECK((kLinkAggregatorCost & (kLinkAggregatorCost - 1)) == 0);

    // The slice is measured per tick, so it decays as the hole closes: a 50-point hole
    // is back to the flat +1 floor, and the pool still never overfills.
    g.setBandwidth(cap - 50);
    CHECK(g.bandwidthRegenAmount() == 1);
    g.tick(now += tick);
    CHECK(g.bandwidth() == cap - 49);
    g.setBandwidth(cap - 1);
    g.tick(now += tick * 4);
    CHECK(g.bandwidth() == cap);
}

namespace {
// The Hacker face owns the carousel and a running walk owns the A+C chord, so anything
// that wants the PET face back has to put both down first. Private to this unit: the
// SERVICES gate is the only one that crosses between the two mid-test.
void backToPetFace(Game& g) {
    if (g.exploreActive() && g.nav() == Game::Nav::Idle) stopExplore(g);
    // C backs out of a submenu, but on a CAROUSEL it only steps the cursor, so the
    // way off the Hacker face is the same A+C chord that reached it, which lands on
    // the pet face at idle.
    while (g.nav() != Game::Nav::Idle && g.nav() != Game::Nav::Cursor) tapC(g);
    if (g.face() == Game::Face::Hacker) g.onButton(chordAC());
}

// Open SHOP > SERVICES from wherever the test is, landing on its first row.
void openServices(Game& g) {
    backToPetFace(g);
    enterHackerSlot(g, HackerSlotId::Shop);
    for (int i = 0; i <= kRigUpgradeCount && g.rigShopScreen().slot() != kRigSlotServices; ++i)
        g.onButton(press(Button::A));
    CHECK(g.rigShopScreen().slot() == kRigSlotServices);
    tapB(g);
    CHECK(g.rigShopScreen().view() == Game::ShopView::Services);
}
}  // namespace

// SHOP > SERVICES (the list's head slot): the switchboard for the rows that act on
// their own. It appears only once one is owned, B on it opens the board, B on a row
// stops and starts that service, and a stopped one does nothing until it is started
// again. The mask persists: a service the player switched off stays off across a boot.
void test_rig_services_switchboard() {
    MemSaveStore store;
    {
        Game g{StartMode::Hatched, "paypup", &store};
        // Nothing that runs by itself is owned, so the list carries no head slot: a lap
        // of the whole storefront never lands on one.
        CHECK(!g.operatorRecord().rig.servicesOffered());
        enterHackerSlot(g, HackerSlotId::Shop);
        CHECK(g.nav() == Game::Nav::Submenu && g.rigShopScreen().view() == Game::ShopView::Rows);
        for (int i = 0; i <= kRigUpgradeCount; ++i) {
            CHECK(g.rigShopScreen().slot() != kRigSlotServices);
            g.onButton(press(Button::A));
        }

        // Buy the two services this gate drives. Both leave the storefront list the
        // moment they are bought and show up on the switchboard instead.
        g.liveOperator().bits = kRigAutoBackupCost + kDiskMaintenanceBuyStart;
        g.buyRigUpgrade(kRigRowAutoBackup);
        g.buyRigUpgrade(kRigRowDiskMaintenance);
        CHECK(g.operatorRecord().rig.servicesOffered() && g.operatorRecord().rig.servicesRunning() == 2);

        // The head slot now draws at the top of the storefront, and the board behind it
        // draws too: both are new screens, so both are rendered at least once here.
        Framebuffer fb(kActiveW, kActiveH);
        g.render(fb);
        CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));

        // Row 0 is AUTO BACKUP (table order). B stops it: owned, but no longer running,
        // and explore-mode arms nothing for free any more.
        openServices(g);
        g.render(fb);
        CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));
        int rows[kRigUpgradeCount];
        CHECK(g.operatorRecord().rig.serviceRows(rows, kRigUpgradeCount) == 2);
        CHECK(rows[0] == kRigRowAutoBackup && rows[1] == kRigRowDiskMaintenance);
        tapB(g);
        CHECK(!g.operatorRecord().rig.featureActive(kRigRowAutoBackup));
        CHECK(g.autoBackupUnlocked());              // still bought: a switch isn't a refund
        CHECK(g.operatorRecord().rig.servicesRunning() == 1);
        backToPetFace(g);
        enterWalk(g);
        CHECK(!g.backupShieldArmed());

        // Started again, the same walk arms the free save.
        openServices(g);
        tapB(g);
        CHECK(g.operatorRecord().rig.featureActive(kRigRowAutoBackup));
        backToPetFace(g);
        enterWalk(g);
        CHECK(g.backupShieldArmed());

        // Stop the auto-defrag instead: a pet well past the tier-1 threshold is left
        // dirty, and the wallet is untouched, because a stopped service spends nothing.
        openServices(g);
        g.onButton(press(Button::A));               // cursor -> DISK MAINTENANCE
        CHECK(g.rigShopScreen().serviceRow() == 1);
        tapB(g);
        CHECK(!g.operatorRecord().rig.featureActive(kRigRowDiskMaintenance));
        tapC(g);                                    // C backs to the storefront, not out
        CHECK(g.rigShopScreen().view() == Game::ShopView::Rows && g.nav() == Game::Nav::Submenu);

        g.liveOperator().bits = 9999;
        g.model().setFragmentation(100);
        const int bits0 = g.bits();
        g.returnToExplore();
        CHECK(g.model().fragmentation() == 100 && g.bits() == bits0);
        g.tick(kSaveAutosaveMs + kHeartbeatMs);     // autosave the mask
    }
    // The switch survives the reboot, and the service it stopped is still stopped.
    Game g2(StartMode::Hatched, "paypup", &store);
    CHECK(g2.diskMaintenanceLevel() == 1);
    CHECK(!g2.operatorRecord().rig.featureActive(kRigRowDiskMaintenance));
    CHECK(g2.operatorRecord().rig.featureActive(kRigRowAutoBackup));
}

// SHOP > SERVICES, the other half of B: a HELD B reads the focused service out
// (what it does and what a run of it costs), instead of switching it, so a switch
// thrown long after the purchase is thrown knowingly. A tap still switches.
void test_rig_service_info_on_hold_b() {
    Game g{StartMode::Hatched};
    g.liveOperator().bits = kRigAutoBackupCost + kDiskMaintenanceBuyStart;
    g.buyRigUpgrade(kRigRowAutoBackup);
    g.buyRigUpgrade(kRigRowDiskMaintenance);
    openServices(g);
    uint32_t t = 0;
    g.tick(t);

    // Held past the threshold: the info page opens and the service is untouched.
    g.onButton(press(Button::B));                       // arm: nothing flips yet
    CHECK(g.rigShopScreen().view() == Game::ShopView::Services);
    g.tick(t += kServiceInfoHoldMs + kHeartbeatMs);
    CHECK(g.rigShopScreen().view() == Game::ShopView::ServiceInfo);
    CHECK(g.operatorRecord().rig.featureActive(kRigRowAutoBackup));       // ...still running
    g.onButton(lift(Button::B));                        // release after the fire: no-op
    CHECK(g.operatorRecord().rig.featureActive(kRigRowAutoBackup));

    // Every service carries the description that page is for, and it draws.
    int rows[kRigUpgradeCount];
    const int n = g.operatorRecord().rig.serviceRows(rows, kRigUpgradeCount);
    for (int i = 0; i < n; ++i) CHECK(kRigUpgrades[rows[i]].serviceInfo != nullptr);
    Framebuffer fb(kActiveW, kActiveH);
    g.render(fb);
    CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));

    // C returns to the board, where a short tap of B still flips the switch.
    tapC(g);
    CHECK(g.rigShopScreen().view() == Game::ShopView::Services);
    g.onButton(press(Button::B));
    g.tick(t += kServiceInfoHoldMs / 2);                // well under the threshold
    g.onButton(lift(Button::B));
    CHECK(g.rigShopScreen().view() == Game::ShopView::Services);    // no page opened...
    CHECK(!g.operatorRecord().rig.featureActive(kRigRowAutoBackup));      // ...the tap switched it off

    // The page reads out the service under the cursor, whichever that is: step to
    // DISK MAINTENANCE and hold there. Its live per-run price is the one thing the
    // content table cannot state, so the page is where a player can find it.
    g.onButton(press(Button::A));
    CHECK(g.rigShopScreen().serviceRow() == 1);
    g.onButton(press(Button::B));
    g.tick(t += kServiceInfoHoldMs + kHeartbeatMs);
    CHECK(g.rigShopScreen().view() == Game::ShopView::ServiceInfo);
    g.onButton(lift(Button::B));
    g.render(fb);
    CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));
    tapB(g);                                            // B leaves the reader too
    CHECK(g.rigShopScreen().view() == Game::ShopView::Services);
    CHECK(g.operatorRecord().rig.featureActive(kRigRowDiskMaintenance));  // and left the switch alone
}

// Disk Maintenance is an emergency backstop, not a maid: every tier sits inside the
// CRITICAL Fragmentation band, and a run bills a multiple of the pet's own defrag price.
void test_rig_disk_maintenance_thresholds() {
    // The whole ladder is above the Critical line: a rig with the deepest tier still
    // explores the entire Caution band on its own.
    for (int t = 0; t < kDiskMaintenanceMaxTier; ++t)
        CHECK(kDiskMaintenanceThreshold[t] >= kFragCriticalMin);

    Game g{StartMode::Hatched};
    g.liveOperator().bits = kDiskMaintenanceBuyStart;
    g.buyRigUpgrade(kRigRowDiskMaintenance);
    CHECK(g.diskMaintenanceLevel() == 1);

    // One under the tier's threshold: nothing runs, nothing is charged.
    g.liveOperator().bits = 9999;
    g.model().setFragmentation(kDiskMaintenanceThreshold[0] - 1);
    int bits0 = g.bits();
    g.returnToExplore();
    CHECK(g.model().fragmentation() == kDiskMaintenanceThreshold[0] - 1);
    CHECK(g.bits() == bits0);

    // At it, the auto-defrag runs: guaranteed clean, billed at the tier's multiple of
    // this pet's own defrag price, and counted on the same tally that price climbs with.
    g.model().setFragmentation(kDiskMaintenanceThreshold[0]);
    const int runCost = g.defragCost() * kDiskMaintenanceCostMult[0];
    bits0 = g.bits();
    const int defrags0 = g.defragCount();
    g.returnToExplore();
    CHECK(g.model().fragmentation() == kDiskMaintenanceThreshold[0] - kDefragReduction);
    CHECK(g.bits() == bits0 - runCost);
    CHECK(g.defragCount() == defrags0 + 1);
}

// the shared rigUpgradeCost curve engine (tunables.h). Pure-function
// check of all six named curves at their defining boundary points, so the balance
// knobs (start price) can be retuned without re-deriving the shape by hand.
void test_rig_cost_curve_formulas() {
    // Fixed: Cost(n) = start, regardless of n (one-time-unlock rows).
    CHECK(rigUpgradeCost(1024, 0, RigCostCurve::kFixed) == 1024);
    CHECK(rigUpgradeCost(1024, 5, RigCostCurve::kFixed) == 1024);

    // Linear: Cost(n) = start + step*n.
    CHECK(rigUpgradeCost(128, 0, RigCostCurve::kLinear, 64) == 128);
    CHECK(rigUpgradeCost(128, 1, RigCostCurve::kLinear, 64) == 192);
    CHECK(rigUpgradeCost(128, 3, RigCostCurve::kLinear, 64) == 320);

    // Doubling: Cost(n) = start * 2^n.
    CHECK(rigUpgradeCost(512, 0, RigCostCurve::kDoubling) == 512);
    CHECK(rigUpgradeCost(512, 1, RigCostCurve::kDoubling) == 1024);
    CHECK(rigUpgradeCost(512, 4, RigCostCurve::kDoubling) == 8192);

    // Halving: Cost(n) = start / 2^n, floored at 1.
    CHECK(rigUpgradeCost(2048, 0, RigCostCurve::kHalving) == 2048);
    CHECK(rigUpgradeCost(2048, 1, RigCostCurve::kHalving) == 1024);
    CHECK(rigUpgradeCost(2048, 2, RigCostCurve::kHalving) == 512);

    // LogStep: Cost(n) = start * 2^floor(log2(n)), n clamped to >=1 (plateaus at n=0,1).
    CHECK(rigUpgradeCost(128, 0, RigCostCurve::kLogStep) == 128);
    CHECK(rigUpgradeCost(128, 1, RigCostCurve::kLogStep) == 128);
    CHECK(rigUpgradeCost(128, 2, RigCostCurve::kLogStep) == 256);
    CHECK(rigUpgradeCost(128, 3, RigCostCurve::kLogStep) == 256);
    CHECK(rigUpgradeCost(128, 4, RigCostCurve::kLogStep) == 512);

    // LogStepHalf: Cost(n) = start * 2^(floor(log2(n+1))-1). Same shape, one tier down.
    CHECK(rigUpgradeCost(128, 0, RigCostCurve::kLogStepHalf) == 64);
    CHECK(rigUpgradeCost(128, 1, RigCostCurve::kLogStepHalf) == 128);
    CHECK(rigUpgradeCost(128, 2, RigCostCurve::kLogStepHalf) == 128);
    CHECK(rigUpgradeCost(128, 3, RigCostCurve::kLogStepHalf) == 256);
}

// Containment Rack Slot: broke → inert, an affordable buy raises rackSlots by 1 and
// gates ARCH Store at the new capacity, the price climbs on the log-stepped ladder, and
// buy-to-cap MAXES out (inert past kRackSlotUpgradeMax).
//
// The ladder is LOG-STEPPED, not doubling. That matters enough to assert: the shelf now
// reaches 64 slots (one of every species on the roster, with room over), and a doubling
// ladder over that many rows prices the last one past a trillion Bits, which is a wall
// rather than a sink. Prices are read off rigUpgradeCost rather than restated, so a
// retune of the curve or the sticker is a one-line edit there and not a test rewrite.
void test_hacker_shop_rack_slot_upgrade() {
    Game g{StartMode::Hatched};
    const int base = g.rackSlots();
    const int first = rigUpgradeCost(kRackSlotUpgradeStart, 0, RigCostCurve::kLogStepHalf);
    CHECK(g.rackSlotUpgradeCost() == first);
    g.liveOperator().bits = 0;
    g.buyRigUpgrade(kRigRowRackSlot);
    CHECK(g.rackSlots() == base && g.bits() == 0);          // broke → inert

    g.liveOperator().bits = first + 5;
    g.buyRigUpgrade(kRigRowRackSlot);
    CHECK(g.rackSlots() == base + 1 && g.bits() == 5);      // +1 slot, Bits deducted
    CHECK(g.rackSlotUpgradeCost() ==
          rigUpgradeCost(kRackSlotUpgradeStart, 1, RigCostCurve::kLogStepHalf));

    // The shelf a full ladder buys is what the ARCH is SIZED for, and the cumulative
    // price of it is the thing a wall would hide: assert both, so raising the ceiling
    // without repricing the rows can't pass.
    int cumulative = 0;
    for (int n = 0; n < kRackSlotUpgradeMax; ++n)
        cumulative += rigUpgradeCost(kRackSlotUpgradeStart, n, RigCostCurve::kLogStepHalf);
    CHECK(base + kRackSlotUpgradeMax == 64);
    CHECK(cumulative < 400000);      // a long project, not an unreachable one

    g.liveOperator().bits = 100000000;
    for (int i = 0; i < kRackSlotUpgradeMax + 2; ++i) g.buyRigUpgrade(kRigRowRackSlot);
    CHECK(g.rackSlotUpgradeCount() == kRackSlotUpgradeMax);       // capped (MAXED)
    CHECK(g.rackSlots() == base + kRackSlotUpgradeMax);
}

// Scraping Cluster Expansion (+combat Bits %) and Enhanced DataMining
// (+cache Bits %): each level lifts applyCombatBitsBonus()/applyCacheBitsBonus() by its
// per-level %, and the two bonuses are independent of each other.
void test_hacker_shop_scraping_and_datamining_bonus() {
    Game g{StartMode::Hatched};
    CHECK(g.combatBitsBonusPct() == 0 && g.cacheBitsBonusPct() == 0);
    CHECK(g.applyCombatBitsBonus(100) == 100 && g.applyCacheBitsBonus(100) == 100);

    g.liveOperator().bits = 1000000;
    for (int i = 0; i < 3; ++i) g.buyRigUpgrade(kRigRowScraping);
    CHECK(g.scrapingClusterLevel() == 3);
    CHECK(g.combatBitsBonusPct() == 3 * kScrapingClusterPctPerLevel);
    CHECK(g.applyCombatBitsBonus(100) == 100 + 100 * g.combatBitsBonusPct() / 100);
    CHECK(g.cacheBitsBonusPct() == 0);                      // untouched by the other track
    // Regression: on-device, buying 1-2 DataMining levels looked inert on a Common
    // cache (10 Bits). `bits * pct / 100` truncates to 0 below the 100-Bits-worth-of-
    // percent threshold. Any ACTIVE bonus must add at least 1 Bit, even on a tiny grant.
    CHECK(g.applyCombatBitsBonus(10) == 10 + 1);            // 10 * 6% = 0.6 -> would floor to 0

    for (int i = 0; i < 2; ++i) g.buyRigUpgrade(kRigRowDataMining);
    CHECK(g.dataMiningLevel() == 2);
    CHECK(g.cacheBitsBonusPct() == 2 * kDataMiningPctPerLevel);
    CHECK(g.applyCacheBitsBonus(100) == 100 + 100 * g.cacheBitsBonusPct() / 100);
    const int commonBits = ContentRegistry::embedded().item("sealed_cache_common")->cache.bits;
    CHECK(g.applyCacheBitsBonus(commonBits) == commonBits + 1);  // same bug, real tier
    CHECK(g.combatBitsBonusPct() == 3 * kScrapingClusterPctPerLevel);  // still independent

    // A single level is the shallowest on-device case: still must not truncate to 0.
    Game g2{StartMode::Hatched};
    g2.liveOperator().bits = 1000000;
    g2.buyRigUpgrade(kRigRowDataMining);
    CHECK(g2.dataMiningLevel() == 1 && g2.cacheBitsBonusPct() == kDataMiningPctPerLevel);
    CHECK(g2.applyCacheBitsBonus(commonBits) == commonBits + 1);
}

// b: the Reduce-Explore-Frag hacker-shop upgrade. Buying a tier follows the
// descending price ladder, is gated by Bits, caps at kFragReducerMaxTier, and lowers the
// effective battle-fatigue frag AMOUNT cap (kBattleFatigueFragMax − fragAmountTier).
void test_hacker_shop_frag_reducer() {
    Game g{StartMode::Hatched};
    CHECK(g.fragAmountTier() == 0);
    g.liveOperator().bits = 0;
    g.buyRigUpgrade(kRigRowFragReduce);
    CHECK(g.fragAmountTier() == 0 && g.bits() == 0);        // broke → inert
    g.liveOperator().bits = kFragReducerStart + 5;
    g.buyRigUpgrade(kRigRowFragReduce);
    CHECK(g.fragAmountTier() == 1 && g.bits() == 5);        // tier up, Bits deducted
    g.liveOperator().bits = 100000;
    for (int i = 0; i < 10; ++i) g.buyRigUpgrade(kRigRowFragReduce);
    CHECK(g.fragAmountTier() == kFragReducerMaxTier);       // capped (MAXED)
    const int spent = 100000 - g.bits();
    g.buyRigUpgrade(kRigRowFragReduce);                                // MAXED → inert, no spend
    CHECK(100000 - g.bits() == spent);
    // The wired combat effect: the effective frag cap is reduced by the tier.
    CHECK(kBattleFatigueFragMax - static_cast<int>(g.fragAmountTier()) < kBattleFatigueFragMax);
}

// j/k. Passive XP Farming + its XP Farming Window: buying the rate row pays
// hungerXpRateLevel() XP per hunger point lost while Hunger stays above
// hungerXpThreshold(), and buying the window row lowers that threshold so more
// of a decay step counts. Every case below stays well under kLevelXpBase (100)
// so combatXp() is a direct, un-rolled-over read. The two tracks are
// independent Rig Shop rows.
void test_hacker_shop_hunger_xp_farming() {
    Game g{StartMode::Hatched};
    CHECK(g.hungerXpRateLevel() == 0 && g.hungerXpThreshold() == kHungerXpBaseThreshold);

    // No rate bought yet: hunger decay pays nothing, even a lot of it.
    g.model().setHunger(100);
    g.tickHungerAndAwardXp(60u * 60u * 1000u * 20u);           // plenty of decay steps
    CHECK(g.combatXp() == 0 && g.combatLevel() == 0);     // rate 0 → no XP

    // Buy 2 levels of the rate row: hunger 100 -> 95 (5 points) all sit above the
    // default 90%% threshold, so all 5 count at 2 XP/point = 10 XP.
    Game g2{StartMode::Hatched};
    g2.liveOperator().bits = 1000000;
    for (int i = 0; i < 2; ++i) g2.buyRigUpgrade(kRigRowHungerXpRate);
    CHECK(g2.hungerXpRateLevel() == 2);
    g2.model().setHunger(100);
    g2.tickHungerAndAwardXp(kHungerMinutesPerPoint * 60u * 1000u * 5u);  // 5 hunger points lost
    CHECK(g2.combatXp() == 5 * 2 && g2.combatLevel() == 0);

    // Drop hunger to sit right at the threshold band: only points ABOVE 90 count.
    Game g3{StartMode::Hatched};
    g3.liveOperator().bits = 1000000;
    g3.buyRigUpgrade(kRigRowHungerXpRate);                              // rate level 1
    g3.model().setHunger(93);
    g3.tickHungerAndAwardXp(kHungerMinutesPerPoint * 60u * 1000u * 10u);  // 93 -> 83
    // Only 93..91 (3 points) sit above the 90 threshold.
    CHECK(g3.combatXp() == 3 * 1 && g3.model().hunger() == 83);

    // Buying the window row lowers the threshold: more of the same decay counts.
    Game g4{StartMode::Hatched};
    g4.liveOperator().bits = 1000000;
    g4.buyRigUpgrade(kRigRowHungerXpRate);                               // rate level 1
    g4.buyRigUpgrade(kRigRowHungerXpWindow);                              // threshold 90 -> 85
    CHECK(g4.hungerXpThreshold() == kHungerXpBaseThreshold - kHungerXpWindowStepPct);
    g4.model().setHunger(93);
    g4.tickHungerAndAwardXp(kHungerMinutesPerPoint * 60u * 1000u * 10u);  // 93 -> 83
    // max(hungerAfter=83, threshold=85) = 85, so 93-85 = 8 points count: more
    // than g3's 3 (whose threshold, 90, sat above hungerAfter).
    CHECK(g4.combatXp() == 8 * 1);
}

// l/m. Well-Fed XP Boost + its XP Boost Window: the %% bonus only applies while
// CURRENT Hunger is at or above combatXpWindowThreshold(); buying the window row
// lowers that bar independently of the hunger-XP-farming window (k) above.
void test_hacker_shop_combat_xp_boost() {
    Game g{StartMode::Hatched};
    CHECK(g.combatXpBonusPct() == 0 && g.applyCombatXpBonus(100) == 100);

    g.liveOperator().bits = 1000000;
    for (int i = 0; i < 5; ++i) g.buyRigUpgrade(kRigRowCombatXpPct);
    CHECK(g.combatXpWindowThreshold() == kCombatXpBaseThreshold);  // window not bought yet

    // Hunger below the 90%% base threshold: the bonus is inert even though the
    // rate is bought.
    g.model().setHunger(kCombatXpBaseThreshold - 1);
    CHECK(g.combatXpBonusPct() == 0 && g.applyCombatXpBonus(100) == 100);

    // Hunger at/above the threshold: the bonus is live.
    g.model().setHunger(kCombatXpBaseThreshold);
    CHECK(g.combatXpBonusPct() == 5 * kCombatXpPctPerLevel);
    CHECK(g.applyCombatXpBonus(100) == 100 + 100 * g.combatXpBonusPct() / 100);
    // Small-value floor, same rounding rule as the Bits bonuses.
    CHECK(g.applyCombatXpBonus(10) == 10 + 1);

    // Buying the window row widens the qualifying range without touching the
    // hunger-XP-farming window (k): the two thresholds are independent.
    g.buyRigUpgrade(kRigRowCombatXpWindow);
    CHECK(g.combatXpWindowThreshold() == kCombatXpBaseThreshold - kCombatXpWindowStepPct);
    CHECK(g.hungerXpThreshold() == kHungerXpBaseThreshold);   // untouched by sibling upgrade
    g.model().setHunger(kCombatXpBaseThreshold - kCombatXpWindowStepPct);
    CHECK(g.combatXpBonusPct() == 5 * kCombatXpPctPerLevel);  // now qualifies

    // Wired at a real payout site: Sim-Battle XP scales with the active bonus.
    // The heartbeat loop below covers well under kHungerMinutesPerPoint of game
    // time, so Hunger doesn't decay out of the window mid-fight.
    Game g2{StartMode::Hatched, "paypup"};
    g2.liveOperator().bits = 1000000;
    for (int i = 0; i < 3; ++i) g2.buyRigUpgrade(kRigRowCombatXpPct);
    g2.model().setHunger(100);
    enterSimBattle(g2);
    uint32_t t = 0;
    for (int i = 0; i < 400 && g2.combat().outcome() == Combat::Outcome::Ongoing; ++i)
        g2.tick(t += kHeartbeatMs);
    CHECK(g2.combat().outcome() == Combat::Outcome::Win);
    g2.onButton(press(Button::B));                            // dismiss -> reward granted
    const int expectedXp = kSimXpReward + std::max(1, kSimXpReward * (3 * kCombatXpPctPerLevel) / 100);
    CHECK(g2.combatXp() == expectedXp);
}

// c. The Hacker-SHOP "Reduce Explore Frag TRIGGER %" upgrade: broke → inert,
// affording a buy → tier up + Bits deducted, buy to cap → MAXED/inert, and the effective
// battle-fatigue TRIGGER chance drops below the baseline. Sibling to the AMOUNT reducer.
void test_hacker_shop_frag_trigger() {
    Game g{StartMode::Hatched};
    CHECK(g.fragTriggerTier() == 0);
    g.liveOperator().bits = 0;
    g.buyRigUpgrade(kRigRowFragTrigger);
    CHECK(g.fragTriggerTier() == 0 && g.bits() == 0);      // broke → inert
    g.liveOperator().bits = kFragTriggerStart + 5;
    g.buyRigUpgrade(kRigRowFragTrigger);
    CHECK(g.fragTriggerTier() == 1 && g.bits() == 5);      // tier up, Bits deducted
    // The wired combat effect: the effective trigger chance now drops below baseline.
    CHECK(kFragTriggerReducedPct[g.fragTriggerTier() - 1] < kBattleFatigueChancePct);
    g.liveOperator().bits = 1000000;
    for (int i = 0; i < 20; ++i) g.buyRigUpgrade(kRigRowFragTrigger);
    CHECK(g.fragTriggerTier() == kFragTriggerMaxTier);     // capped (MAXED)
    const int spent = 1000000 - g.bits();
    g.buyRigUpgrade(kRigRowFragTrigger);                               // MAXED → inert, no spend
    CHECK(1000000 - g.bits() == spent);
}

// d. The Hacker SHOP "ITEMS Type-Tabs" one-time unlock: a buy spends Bits
// and sets itemTabsUnlocked() (no tiers. A second buy is inert/OWNED, and an
// unaffordable buy leaves both Bits and the flag untouched).
void test_hacker_shop_item_tabs_buy() {
    Game g{StartMode::Hatched};
    CHECK(!g.itemTabsUnlocked());
    g.liveOperator().bits = kShopItemTabsCost - 1;
    g.buyRigUpgrade(kRigRowItemTabs);
    CHECK(!g.itemTabsUnlocked() && g.bits() == kShopItemTabsCost - 1);   // broke -> inert
    g.liveOperator().bits = kShopItemTabsCost + 7;
    g.buyRigUpgrade(kRigRowItemTabs);
    CHECK(g.itemTabsUnlocked() && g.bits() == 7);            // bought, Bits deducted
    g.liveOperator().bits = 9999;
    g.buyRigUpgrade(kRigRowItemTabs);                                    // already OWNED -> inert
    CHECK(g.bits() == 9999);
}

// e. The Hacker SHOP "VAULT Bulk-Open" one-time unlock: same shape as
// d (buy / broke-inert / OWNED-inert), on its own flag + cost.
void test_hacker_shop_bulk_open_buy() {
    Game g{StartMode::Hatched};
    CHECK(!g.bulkOpenUnlocked());
    g.liveOperator().bits = kShopBulkOpenCost - 1;
    g.buyRigUpgrade(kRigRowBulkOpen);
    CHECK(!g.bulkOpenUnlocked() && g.bits() == kShopBulkOpenCost - 1);
    g.liveOperator().bits = kShopBulkOpenCost + 3;
    g.buyRigUpgrade(kRigRowBulkOpen);
    CHECK(g.bulkOpenUnlocked() && g.bits() == 3);
    g.liveOperator().bits = 9999;
    g.buyRigUpgrade(kRigRowBulkOpen);                                    // already OWNED -> inert
    CHECK(g.bits() == 9999);
}

// d; buildInventoryRows(filter) actually narrows the list: Food/Buffs/Quest
// each return only that type's rows (+ its one header); All matches today's full
// grouped list. Inventory::starting() carries 2 Food + 3 Buffs + 2 Quest (7 items,
// 4 headers, per test_inventory_rows_grouped): Tor-Tilla Chip is Food AND a Nachos
// ingredient (content_recipes.cpp), so the Food filter still splits into its own
// FOOD/INGREDIENTS pair even though the filter itself doesn't narrow by that axis.
// Backup Drive is a Buff (its combat shield), not a Quest item.
void test_item_filter_narrows_rows() {
    ContentRegistry r = ContentRegistry::embedded();
    Inventory inv = Inventory::starting();
    auto all   = buildInventoryRows(r, inv, ItemOrder::ByGroup, ItemFilter::All);
    auto food  = buildInventoryRows(r, inv, ItemOrder::ByGroup, ItemFilter::Food);
    auto buffs = buildInventoryRows(r, inv, ItemOrder::ByGroup, ItemFilter::Buffs);
    auto quest = buildInventoryRows(r, inv, ItemOrder::ByGroup, ItemFilter::Quest);

    auto headerCount = [](const std::vector<InvRow>& rows) {
        int n = 0; for (const auto& row : rows) if (isHeading(row.kind)) ++n; return n;
    };
    auto itemCount = [](const std::vector<InvRow>& rows) {
        int n = 0; for (const auto& row : rows) if (!isHeading(row.kind)) ++n; return n;
    };
    CHECK(headerCount(all) == 4 && itemCount(all) == 6);
    CHECK(headerCount(food) == 2 && itemCount(food) == 2);
    CHECK(headerCount(buffs) == 1 && itemCount(buffs) == 2);
    CHECK(headerCount(quest) == 1 && itemCount(quest) == 2);
    for (const auto& row : food)  if (!isHeading(row.kind)) CHECK(row.def->type == ItemDef::Type::Food);
    for (const auto& row : buffs) if (!isHeading(row.kind)) CHECK(row.def->type == ItemDef::Type::Buff);
    for (const auto& row : quest) if (!isHeading(row.kind)) CHECK(row.def->type == ItemDef::Type::Quest);
}

// The ITEMS hold-B gesture. Unowned: B opens the focused row's detail immediately on
// PRESS (zero regression. No hold, no release wait). Owned: a short tap (release
// before kItemFilterHoldMs) still opens it, on the release edge; holding past the
// threshold cycles the filter instead (ALL -> FOOD), narrowing the list, and the
// eventual release is a no-op (bHeld_ already consumed by the hold-fire). A is the
// plain step throughout: the gesture never touches it.
void test_item_hold_b_cycles_filter_tap_opens() {
    // (a) Unowned: B opens the focused row on PRESS alone (no release needed).
    {
        Game g{StartMode::Hatched};
        CHECK(!g.itemTabsUnlocked());
        enterSubmenuId(g, SubmenuId::Items);          // row 0 = Dyno Nuggets (FOOD)
        g.onButton(press(Button::A));                 // A still steps immediately
        const int tc0 = g.inventory().count("tortilla_chip");
        g.onButton(press(Button::B));                 // open the now-focused row's detail
        g.onButton(press(Button::B));                 // Use
        CHECK(g.inventory().count("tortilla_chip") == tc0 - 1);  // row 1 (Tor-Tilla Chip, FOOD)
    }
    // (b) Owned, short tap: release before the hold threshold still opens the row.
    {
        Game g{StartMode::Hatched};
        g.liveOperator().bits = kShopItemTabsCost;
        g.buyRigUpgrade(kRigRowItemTabs);
        CHECK(g.itemTabsUnlocked());
        enterSubmenuId(g, SubmenuId::Items);
        CHECK(g.itemsMenu().filter() == ItemFilter::All);
        uint32_t t = 0;
        g.tick(t);
        g.onButton(press(Button::A));                 // A steps to row 1 on press
        g.onButton(lift(Button::A));
        const int tc0 = g.inventory().count("tortilla_chip");
        g.onButton(press(Button::B));                 // arm the hold: nothing opens yet
        g.tick(t += kItemFilterHoldMs / 2);           // well under the threshold
        g.onButton(lift(Button::B));                  // release -> short tap -> opens
        CHECK(g.itemsMenu().filter() == ItemFilter::All);     // a tap never touches the filter
        g.onButton(press(Button::B));                 // Use
        CHECK(g.inventory().count("tortilla_chip") == tc0 - 1);  // row 1, same as unowned
    }
    // (c) Owned, a hold past the threshold cycles the filter instead of opening.
    {
        Game g{StartMode::Hatched};
        g.liveOperator().bits = kShopItemTabsCost;
        g.buyRigUpgrade(kRigRowItemTabs);
        enterSubmenuId(g, SubmenuId::Items);
        uint32_t t = 0;
        g.tick(t);
        g.onButton(press(Button::B));                  // arm the hold
        g.tick(t += kItemFilterHoldMs + kHeartbeatMs);  // cross the threshold -> cycles
        CHECK(g.itemsMenu().filter() == ItemFilter::Food);      // ALL -> FOOD
        CHECK(g.nav() == Game::Nav::Submenu);           // and never opened a detail
        g.onButton(lift(Button::B));                    // release AFTER the fire -> no-op
        CHECK(g.itemsMenu().filter() == ItemFilter::Food);      // unchanged by the release
        CHECK(g.nav() == Game::Nav::Submenu);
        // The list is now narrowed to FOOD: B opens the first (only) food row.
        const int as0 = g.inventory().count("dyno_nuggets");
        g.onButton(press(Button::B));
        g.onButton(lift(Button::B));                    // tap -> opens the detail
        g.onButton(press(Button::B));                   // Use
        CHECK(g.inventory().count("dyno_nuggets") == as0 - 1);
    }
}

// The Rig Shop "ITEMS Type-Picker" one-time unlock: same buy shape as the two
// gesture unlocks above (broke-inert / bought / OWNED-inert), on its own flag + cost.
void test_hacker_shop_item_picker_buy() {
    Game g{StartMode::Hatched};
    CHECK(!g.itemPickerUnlocked());
    g.liveOperator().bits = kShopItemPickerCost - 1;
    g.buyRigUpgrade(kRigRowItemPicker);
    CHECK(!g.itemPickerUnlocked() && g.bits() == kShopItemPickerCost - 1);
    g.liveOperator().bits = kShopItemPickerCost + 5;
    g.buyRigUpgrade(kRigRowItemPicker);
    CHECK(g.itemPickerUnlocked() && g.bits() == 5);
    g.liveOperator().bits = 9999;
    g.buyRigUpgrade(kRigRowItemPicker);                                  // already OWNED -> inert
    CHECK(g.bits() == 9999);
}

// The category axis splits QUEST into KEYS + TOOLS without touching the type axis:
// the starting shelf's 2 quest items are both Keys (Decryption Key, Boot Accelerator),
// so a fresh bag has an EMPTY Tools category until a Defrag Tool lands in it. The
// filtered list's one header names the CATEGORY, not the type it sits under.
void test_item_category_filters_split_quest() {
    ContentRegistry r = ContentRegistry::embedded();
    Inventory inv = Inventory::starting();
    auto itemCount = [](const std::vector<InvRow>& rows) {
        int n = 0; for (const auto& row : rows) if (!isHeading(row.kind)) ++n; return n;
    };

    auto quest = buildInventoryRows(r, inv, ItemOrder::ByGroup, ItemFilter::Quest);
    auto keys  = buildInventoryRows(r, inv, ItemOrder::ByGroup, ItemFilter::Keys);
    auto tools = buildInventoryRows(r, inv, ItemOrder::ByGroup, ItemFilter::Tools);
    CHECK(itemCount(quest) == 2 && itemCount(keys) == 2);   // every quest item is a Key
    CHECK(tools.empty());                                   // nothing carried yet
    CHECK(std::strcmp(keys[0].label, "KEYS") == 0);         // header names the category
    for (const auto& row : keys)
        if (!isHeading(row.kind)) CHECK(itemCategory(*row.def) == ItemDef::Category::Keys);

    inv.add("disk_scrubber", 2);                            // a Defrag Tool is a TOOL
    tools = buildInventoryRows(r, inv, ItemOrder::ByGroup, ItemFilter::Tools);
    keys  = buildInventoryRows(r, inv, ItemOrder::ByGroup, ItemFilter::Keys);
    CHECK(itemCount(tools) == 1 && std::strcmp(tools[0].label, "TOOLS") == 0);
    CHECK(itemCount(keys) == 2);                            // Keys untouched by it
}

// Grayscale gate: on the type-picker, the focused tile's cursor marker reads without
// colour.
void test_item_picker_grayscale() {
    Framebuffer fb(kActiveW, kActiveH);
    drawItemTypePicker(fb, 0);

    constexpr int kPickRowH = 30;   // mirrors drawItemTypePicker's pitch
    const int focusY = kItemsRowTop + 0 * kPickRowH + kPickRowH / 2;  // ALL (selected)
    const int plainY = kItemsRowTop + 1 * kPickRowH + kPickRowH / 2;  // FOOD (not)
    CHECK(luminance(fb.get(9, focusY)) - luminance(fb.get(9, plainY)) > 0.3f);
}

// The picker's nav contract. Unowned: ITEMS opens straight on the list, exactly as
// before. Owned: ITEMS opens on the picker, A steps the tile, B commits that tile's
// filter and drills into its list, C on the list walks BACK to the picker, and C on
// the picker leaves ITEMS entirely.
void test_item_picker_nav_drills_in_and_back() {
    {   // (a) Unowned: no picker layer anywhere.
        Game g{StartMode::Hatched};
        enterSubmenuId(g, SubmenuId::Items);
        CHECK(g.itemsMenu().view() == ItemsView::List);
        tapC(g);                       // straight back out
        CHECK(g.nav() == Game::Nav::Cursor);
    }
    // (b) Owned: the picker fronts the list.
    Game g{StartMode::Hatched};
    g.liveOperator().bits = kShopItemPickerCost;
    g.buyRigUpgrade(kRigRowItemPicker);
    enterSubmenuId(g, SubmenuId::Items);
    CHECK(g.itemsMenu().view() == ItemsView::Picker && g.itemsMenu().pickRow() == 0);

    g.onButton(press(Button::A));                           // ALL -> FOOD
    g.onButton(press(Button::A));                           // FOOD -> INGREDIENTS
    g.onButton(press(Button::A));                           // INGREDIENTS -> BUFFS
    g.onButton(press(Button::A));                           // BUFFS -> KEYS
    CHECK(g.itemsMenu().pickRow() == 4);
    g.onButton(press(Button::B));                           // drill into KEYS
    CHECK(g.itemsMenu().view() == ItemsView::List);
    CHECK(g.itemsMenu().filter() == ItemFilter::Keys);
    CHECK(g.nav() == Game::Nav::Submenu);                   // still L2, new screen

    tapC(g);                           // back to the tiles
    CHECK(g.itemsMenu().view() == ItemsView::Picker && g.nav() == Game::Nav::Submenu);
    CHECK(g.itemsMenu().pickRow() == 4);                            // the tile is where we left it
    tapC(g);                           // out of ITEMS
    CHECK(g.nav() == Game::Nav::Cursor);

    // Re-entering resets both the tile cursor and the filter: a visit never
    // inherits the last one's state.
    enterSubmenuId(g, SubmenuId::Items);
    CHECK(g.itemsMenu().pickRow() == 0 && g.itemsMenu().filter() == ItemFilter::All);

    // An EMPTY category still opens; the list is simply empty and C walks back.
    g.onButton(press(Button::A)); g.onButton(press(Button::A));
    g.onButton(press(Button::A)); g.onButton(press(Button::A));
    g.onButton(press(Button::A));                                // -> TOOLS (x0)
    g.onButton(press(Button::B));
    CHECK(g.itemsMenu().view() == ItemsView::List && g.itemsMenu().filter() == ItemFilter::Tools);
    CHECK(buildInventoryRows(ContentRegistry::embedded(), g.inventory(), ItemOrder::ByGroup,
                             g.itemsMenu().filter()).empty());
    g.onButton(press(Button::B));                           // B on an empty list is inert
    CHECK(g.nav() == Game::Nav::Submenu);
    tapC(g);
    CHECK(g.itemsMenu().view() == ItemsView::Picker);
}

// The Lockout Open-Items escape hatch never routes through the picker, even when
// it's owned: a crisis doesn't get an extra screen between the player and a meal,
// and C from that list still returns to the Lockout modal rather than to tiles.
void test_item_picker_skipped_in_lockout() {
    Game g{StartMode::Hatched};
    g.liveOperator().bits = kShopItemPickerCost;
    g.buyRigUpgrade(kRigRowItemPicker);
    g.model().setHunger(0);
    g.tick(kHeartbeatMs);
    CHECK(g.nav() == Game::Nav::ModalLockout);
    g.onButton(press(Button::B));                    // "Open Items" (lockout ctx)
    CHECK(g.nav() == Game::Nav::Submenu);
    CHECK(g.itemsMenu().view() == ItemsView::List);   // straight onto the list
    tapC(g);                    // C goes back to the crisis
    CHECK(g.nav() == Game::Nav::ModalLockout);
}

// Owning the picker upgrades the hold-B tab cycle to the finer CATEGORY axis, so
// the gesture and the tiles can never disagree about what a tab means. Type-Tabs
// alone still walks the type axis (its QUEST tab), and holding B on the PICKER
// screen does nothing at all, only the list arms the gesture.
void test_item_hold_b_follows_picker_axis() {
    auto holdB = [](Game& g, uint32_t& t) {
        g.onButton(press(Button::B));
        g.tick(t += kItemFilterHoldMs + kHeartbeatMs);
        g.onButton(lift(Button::B));
    };
    {   // (a) Type-Tabs only: ALL -> FOOD -> BUFFS -> TOOLS -> QUEST. TOOLS is a band
        // on this axis like the others, so the coarse cycle stops at it too: a filter
        // that skipped it would leave 14 rows reachable only by owning the picker.
        Game g{StartMode::Hatched};
        g.liveOperator().bits = kShopItemTabsCost;
        g.buyRigUpgrade(kRigRowItemTabs);
        enterSubmenuId(g, SubmenuId::Items);
        uint32_t t = 0;
        g.tick(t);
        holdB(g, t); holdB(g, t);
        CHECK(g.itemsMenu().filter() == ItemFilter::Buffs);
        holdB(g, t);
        CHECK(g.itemsMenu().filter() == ItemFilter::Tools);
        holdB(g, t);
        CHECK(g.itemsMenu().filter() == ItemFilter::Quest);
    }
    {   // (b) Both owned: ALL -> FOOD -> INGREDIENTS -> BUFFS -> KEYS -> TOOLS -> ALL.
        Game g{StartMode::Hatched};
        g.liveOperator().bits = kShopItemTabsCost + kShopItemPickerCost;
        g.buyRigUpgrade(kRigRowItemTabs);
        g.buyRigUpgrade(kRigRowItemPicker);
        enterSubmenuId(g, SubmenuId::Items);
        uint32_t t = 0;
        g.tick(t);
        // On the PICKER, B commits the focused tile: the hold gesture is never armed
        // there, so holding it drills in exactly as a tap would.
        holdB(g, t);
        CHECK(g.itemsMenu().filter() == ItemFilter::All);
        CHECK(g.itemsMenu().view() == ItemsView::List);
        holdB(g, t); holdB(g, t); holdB(g, t); holdB(g, t);
        CHECK(g.itemsMenu().filter() == ItemFilter::Keys);
        holdB(g, t);
        CHECK(g.itemsMenu().filter() == ItemFilter::Tools);
        holdB(g, t);
        CHECK(g.itemsMenu().filter() == ItemFilter::All);    // wraps, never lands on QUEST
    }
}

// e; openAllCachesOfRarity's aggregation: grant N caches of one rarity plus
// caches of a DIFFERENT rarity, invoke the bulk path directly (bulkOpenCaches),
// and assert ALL of the target rarity are consumed in one action, the aggregate
// tally + Bits sum across every open, and the other rarity is left untouched.
void test_vault_bulk_open_consumes_all_of_rarity() {
    Game g{StartMode::Hatched};
    g.inventory().add("sealed_cache_common", 4);
    g.inventory().add("sealed_cache_uncommon", 2);    // different rarity: must survive
    const int bits0 = g.bits();

    bulkOpenCaches(g, "sealed_cache_common");

    CHECK(g.inventory().count("sealed_cache_common") == 0);     // ALL of that rarity gone
    CHECK(g.inventory().count("sealed_cache_uncommon") == 2);   // other rarity untouched
    CHECK(g.vaultScreen().bulkReveal()->cachesOpened == 4);
    CHECK(g.vaultScreen().bulkReveal()->bits == 4 * ContentRegistry::embedded().item("sealed_cache_common")->cache.bits);  // aggregate Bits sum
    CHECK(g.bits() == bits0 + g.vaultScreen().bulkReveal()->bits);
    CHECK(g.nav() == Game::Nav::BulkYield);

    // Each common cache draws exactly 1 item (its row's cache.draws == 1): the tally
    // sums to 4 across however many distinct item ids were drawn.
    int tallied = 0;
    for (const auto& e : g.vaultScreen().bulkReveal()->tally) tallied += e.count;
    CHECK(tallied == 4);
}

// e. The VAULT hold-B gesture, end to end through the real Hacker-face
// navigation: a short tap still opens just the focused cache (unchanged, single-open
// reveal); holding past kBulkOpenHoldMs bulk-opens everything sharing its rarity in
// one action (the aggregate reveal), and the eventual release after the fire is a
// no-op (bHeld_ already consumed).
void test_vault_hold_b_bulk_opens_tap_opens_one() {
    Game g{StartMode::Hatched};
    g.inventory().add("sealed_cache_common", 3);
    g.liveOperator().bits = kShopBulkOpenCost;
    g.buyRigUpgrade(kRigRowBulkOpen);
    CHECK(g.bulkOpenUnlocked());

    g.onButton({Button::A, true, true});              // PET -> HACKER
    g.onButton(press(Button::A));                      // summon the hacker cursor
    while (hackerCarouselSlots()[g.cursor()].id != HackerSlotId::Vault)
        g.onButton(press(Button::A));
    g.onButton(press(Button::B));                       // enter VAULT
    CHECK(g.nav() == Game::Nav::Submenu);

    // A short tap opens just the one (unchanged).
    uint32_t t = 0;
    g.tick(t);
    g.onButton(press(Button::B));                        // arm the hold
    g.tick(t += kBulkOpenHoldMs / 2);                     // well under the threshold
    g.onButton({Button::B, false, false});                // release -> short tap -> open one
    CHECK(g.inventory().count("sealed_cache_common") == 2);   // exactly one consumed
    CHECK(g.nav() == Game::Nav::CacheYield);                  // single-open reveal
    tapC(g);                              // dismiss -> back to VAULT
    CHECK(g.nav() == Game::Nav::Submenu);

    // Holding past the threshold bulk-opens the rest in one action.
    g.onButton(press(Button::B));                         // arm the hold again
    g.tick(t += kBulkOpenHoldMs + kHeartbeatMs);           // cross the threshold -> bulk
    CHECK(g.inventory().count("sealed_cache_common") == 0);    // the remaining 2, together
    CHECK(g.vaultScreen().bulkReveal()->cachesOpened == 2);
    CHECK(g.nav() == Game::Nav::BulkYield);
    g.onButton({Button::B, false, false});                 // release AFTER the fire -> no-op
    CHECK(g.nav() == Game::Nav::BulkYield);                 // unchanged
}

// re-home: sealed caches decrypt from the Hacker VAULT, not pet-side ITEMS. Using
// an openable from ITEMS is inert (gated "OPEN IN VAULT (A+C)") and does NOT consume it or
// pay out; the VAULT path (openCache) is the one that decrypts.
void test_cache_not_openable_from_items() {
    Game g{StartMode::Hatched};
    g.inventory().add("sealed_cache_epic", 1);
    const int bits0 = g.bits();
    useItem(g, "sealed_cache_epic");                   // pet-side ITEMS: now inert
    CHECK(g.inventory().count("sealed_cache_epic") == 1);  // not consumed
    CHECK(g.bits() == bits0);                              // no reward paid
    CHECK(g.nav() != Game::Nav::CacheYield);               // no reveal
    openCache(g, "sealed_cache_epic");                 // VAULT path decrypts it
    CHECK(g.inventory().count("sealed_cache_epic") == 0);  // consumed
    CHECK(g.nav() == Game::Nav::CacheYield);
}

// THE MERGE HUB'S ROSTER OUTGREW ITS PANEL, which is why the list windows
// (kMergeVisibleRows). Two things have to hold whatever the recipe table grows to: the
// tallest screen the window can produce still clears the A NEXT / B MERGE footer, and
// the focused row is always one of the rows drawn. A cursor the player can't see is
// worse than a list that runs off the bottom, because it moves invisibly.
void test_merge_hub_windows_its_roster() {
    // Mirrors MergeScreen::draw's own layout: rows start at 28, each folded row costs a
    // line plus the gap, and the focused one also unfolds up to kMaxRecipeInputs
    // ingredient lines. The footer rule is the budget.
    constexpr int kRowTop = 28, kRowGap = 6;
    const int tallest = kRowTop + (kMergeVisibleRows - 1) * (kLineH + kRowGap) +
                        (kLineH + kMaxRecipeInputs * kLineH + kRowGap);
    CHECK(tallest <= kActiveH - 26);

    Game g{StartMode::Hatched};
    g.liveOperator().bits = 99999;
    g.buyRigUpgrade(kRigRowMergeHub);
    enterHackerSlot(g, HackerSlotId::Merge);

    // Every row in turn, including the last: the clamp's own edge.
    for (int i = 0; i < kMergeRecipeCount; ++i) {
        Framebuffer fb(kActiveW, kActiveH);
        g.render(fb);
        // The focused row's cursor marker lives in the left gutter, above the footer.
        CHECK(anyNonPaper(fb, 0, kRowTop - 4, 6, kActiveH - 26));
        g.onButton(press(Button::A));
    }
}
