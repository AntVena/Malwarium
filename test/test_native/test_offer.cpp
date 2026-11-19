// test_offer.cpp: the ITEM OFFER and DIVE PREP (game_offer.cpp), AUTO DIVE GEAR, AUTO-REDIVE
// and the DIVE PAY
// bonus a bell or Deep-Learning device carries.
#include "test_gates.h"

using namespace mal;

namespace {
Game divingReadyGame(MemSaveStore* store = nullptr) {
    Game g{StartMode::Hatched, "bruinforce", store};
    for (int a = 0; a < kExplSectors; ++a) g.liveOperator().world.sectorCleared[a] = true;
    return g;
}
}  // namespace

// A bell and a device armed together pay their DIVE PAY added, for the dive they were
// armed for and only while it runs: a dive started with nothing armed pays nothing.
void test_dive_gear_pays_a_bonus() {
    Game g = divingReadyGame();
    g.inventory().add("kernel_bell", 1);           // +100
    g.inventory().add("deep_learning_module", 1);  // +50
    useItem(g, "kernel_bell");
    useItem(g, "deep_learning_module");
    CHECK(g.diveRewardBonusPct() == 0);       // armed, but nothing to pay yet
    g.startDeepWebDive();
    CHECK(g.inDeepWebDive());
    CHECK(g.diveRewardBonusPct() == 150);
    stopExplore(g);
    CHECK(g.diveRewardBonusPct() == 0);
    g.startDeepWebDive();                     // a fresh dive, nothing re-armed
    CHECK(g.diveRewardBonusPct() == 0);
}

// Without AUTO DIVE GEAR, starting a dive opens DIVE PREP on DIVE NOW, whatever the bag
// holds: four rows, so stocking more dive items adds nothing to press past. The default
// loadout arms nothing, so one B dives with nothing spent. C backs out with no dive.
void test_dive_prep_offer() {
    {
        Game g = divingReadyGame();
        g.inventory().add("backdoor_bell", 1);
        g.inventory().add("rootkit_bell", 1);
        g.inventory().add("kernel_bell", 1);
        g.inventory().add("deep_learning_module", 1);
        g.offerThenDive();
        CHECK(g.nav() == Game::Nav::DivePrep);
        Framebuffer fb(kActiveW, kActiveH);
        g.render(fb);
        CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));
        for (int i = 0; i < kDivePrepRows; ++i) g.onButton(press(Button::A));
        tapB(g);                                   // wrapped back round to DIVE NOW
        CHECK(g.inDeepWebDive());
        CHECK(g.inventory().count("kernel_bell") == 1);
        CHECK(g.diveRewardBonusPct() == 0);
    }
    {
        Game g = divingReadyGame();
        g.inventory().add("backdoor_bell", 1);
        g.offerThenDive();
        tapC(g);
        CHECK(g.nav() != Game::Nav::DivePrep);
        CHECK(!g.inDeepWebDive());
        CHECK(g.inventory().count("backdoor_bell") == 1);
    }
    {
        // Nothing held: the screen still opens, since AUTO-REDIVE lives on it.
        Game g = divingReadyGame();
        g.offerThenDive();
        CHECK(g.nav() == Game::Nav::DivePrep);
        tapB(g);
        CHECK(g.inDeepWebDive());
    }
}

// The loadout stands: set once, it arms every later dive and survives a reboot. BEST is
// the deepest bell held; an item pick arms that item while it lasts and nothing after.
void test_dive_loadout_stands() {
    MemSaveStore store;
    {
        Game g = divingReadyGame(&store);
        g.inventory().add("backdoor_bell", 1);
        g.inventory().add("kernel_bell", 2);
        g.inventory().add("deep_learning_module", 1);
        g.offerThenDive();
        g.onButton(press(Button::A));              // BELL
        tapB(g);                                   // NONE -> BEST
        CHECK(g.diveLoadout().bell.kind == DivePickKind::Best);
        g.onButton(press(Button::A));              // DEVICE
        tapB(g);                                   // NONE -> BEST
        tapB(g);                                   // BEST -> the module
        CHECK(g.diveLoadout().device.kind == DivePickKind::Item);
        CHECK(std::strcmp(g.diveLoadout().device.item->id, "deep_learning_module") == 0);
        tapC(g);                                   // backing out keeps the picks
        CHECK(!g.inDeepWebDive());
        CHECK(g.inventory().count("kernel_bell") == 2);

        g.offerThenDive();
        CHECK(g.nav() == Game::Nav::DivePrep);
        tapB(g);                                   // lands on DIVE NOW again
        CHECK(g.inDeepWebDive());
        CHECK(g.inventory().count("kernel_bell") == 1);
        CHECK(g.inventory().count("backdoor_bell") == 1);
        CHECK(g.inventory().count("deep_learning_module") == 0);
        CHECK(g.diveRewardBonusPct() == 100 + 50);
        stopExplore(g);

        g.offerThenDive();                         // the module is gone: the bell alone
        tapB(g);
        CHECK(g.inventory().count("kernel_bell") == 0);
        CHECK(g.diveRewardBonusPct() == 100);
        stopExplore(g);
        g.tick(kSaveAutosaveMs + kHeartbeatMs);
    }
    Game g2(StartMode::Hatched, "bruinforce", &store);
    CHECK(g2.diveLoadout().bell.kind == DivePickKind::Best);
    CHECK(g2.diveLoadout().device.kind == DivePickKind::Item);
    CHECK(std::strcmp(g2.diveLoadout().device.item->id, "deep_learning_module") == 0);
}

// AUTO-REDIVE: a dive lost with it on starts the next one, geared by the loadout, as the
// loss's readout closes. Off, the loss ends it. A start far below the pet's depth is the
// deterministic loss. The walk overlay's AUTO row is the same switch while diving.
void test_auto_redive() {
    for (const bool on : {true, false}) {
        Game g{StartMode::Hatched, "phishlet"};
        g.seedRng(1);
        markEveryStoryRead(g);
        for (int a = 0; a < kExplSectors; ++a) g.liveOperator().world.sectorCleared[a] = true;
        if (on) {
            g.offerThenDive();
            for (int i = 0; i < 3; ++i) g.onButton(press(Button::A));
            tapB(g);                               // AUTO-REDIVE on
            CHECK(g.diveLoadout().autoRedive);
            tapC(g);
        }
        g.livePet().armed.deepWebStartDepth = 4096;
        g.startDeepWebDive();
        CHECK(g.exploreStreak() == 4096);
        for (int i = 0; i < 400 && g.nav() != Game::Nav::Combat; ++i) {
            if (g.nav() == Game::Nav::Idle) pingExplore(g);
            else if (g.nav() == Game::Nav::Shop || g.nav() == Game::Nav::ModShop) tapC(g);
            else g.onButton(press(Button::B));
        }
        CHECK(g.nav() == Game::Nav::Combat);
        uint32_t t = 0;
        for (int i = 0; i < 400 && g.combat().outcome() == Combat::Outcome::Ongoing; ++i)
            g.tick(t += kHeartbeatMs);
        for (int i = 0; i < 200 && g.nav() == Game::Nav::Combat; ++i) g.tick(t += kHeartbeatMs);
        CHECK(g.combat().lostOrDrawn());
        CHECK(g.nav() == Game::Nav::PostEncounter);
        CHECK(!g.exploreActive());
        g.onButton(press(Button::B));              // close the readout
        CHECK(g.nav() == Game::Nav::Idle);
        CHECK(g.inDeepWebDive() == on);
        if (on) CHECK(g.exploreStreak() == 0);     // a fresh dive, no bell armed
    }
    {
        Game g = divingReadyGame();
        g.startDeepWebDive();
        CHECK(!g.diveLoadout().autoRedive);
        g.toggleAutoRedive();
        CHECK(g.diveLoadout().autoRedive);
        CHECK(g.autoProgress());                   // the walk's own AUTO is untouched
    }
}

// AUTO DIVE GEAR's three positions. YES arms the deepest bell and the strongest device
// with no screen, leaving the shallower bell in the bag; ASK is DIVE PREP; NO dives
// with nothing armed. The position survives a reboot.
void test_auto_dive_gear_modes() {
    MemSaveStore store;
    {
        Game g = divingReadyGame(&store);
        g.liveOperator().bits = kRigAutoDiveGearCost;
        g.buyRigUpgrade(kRigRowAutoDiveGear);
        CHECK(g.operatorRecord().rig.serviceMode(kRigRowAutoDiveGear) == Game::ServiceMode::On);
        CHECK(std::strcmp(g.operatorRecord().rig.serviceWord(kRigRowAutoDiveGear), "YES") == 0);
        g.inventory().add("backdoor_bell", 1);
        g.inventory().add("kernel_bell", 1);
        g.inventory().add("deep_learning_module", 1);

        g.offerThenDive();                    // YES
        CHECK(g.inDeepWebDive());
        CHECK(g.inventory().count("kernel_bell") == 0);
        CHECK(g.inventory().count("backdoor_bell") == 1);
        CHECK(g.inventory().count("deep_learning_module") == 0);
        CHECK(g.diveRewardBonusPct() == 100 + 50);
        stopExplore(g);

        g.toggleRigService(kRigRowAutoDiveGear);   // -> ASK
        CHECK(g.operatorRecord().rig.serviceMode(kRigRowAutoDiveGear) == Game::ServiceMode::Ask);
        g.offerThenDive();
        CHECK(g.nav() == Game::Nav::DivePrep);
        tapC(g);

        g.toggleRigService(kRigRowAutoDiveGear);   // -> NO
        CHECK(g.operatorRecord().rig.serviceMode(kRigRowAutoDiveGear) == Game::ServiceMode::Off);
        g.offerThenDive();
        CHECK(g.inDeepWebDive());
        CHECK(g.inventory().count("backdoor_bell") == 1);
        CHECK(g.diveRewardBonusPct() == 0);
        stopExplore(g);

        g.toggleRigService(kRigRowAutoDiveGear);   // NO -> YES
        CHECK(g.operatorRecord().rig.serviceMode(kRigRowAutoDiveGear) == Game::ServiceMode::On);
        g.toggleRigService(kRigRowAutoDiveGear);   // -> ASK, and keep it there
        g.tick(kSaveAutosaveMs + kHeartbeatMs);
    }
    Game g2(StartMode::Hatched, "bruinforce", &store);
    CHECK(g2.operatorRecord().rig.serviceMode(kRigRowAutoDiveGear) == Game::ServiceMode::Ask);
}

// An egg at rest offers the Boot Accelerator once: B spends it and cuts the row's own
// minutes off the clock; C is "not now", and the egg doesn't ask again.
void test_egg_offers_the_accelerator() {
    {
        Game g;
        if (g.inLineSelect()) g.onButton(press(Button::B));
        settleDecryption(g);
        CHECK(g.inventory().count("boot_accelerator") == 1);   // the starting kit
        g.tick(1000);
        CHECK(g.nav() == Game::Nav::ItemOffer);
        const uint32_t before = g.bootHatchRemainMs();
        tapB(g);
        CHECK(g.inventory().count("boot_accelerator") == 0);
        CHECK(g.bootHatchRemainMs() == before - 10u * 60u * 1000u);
        CHECK(g.nav() == Game::Nav::Idle);
    }
    {
        Game g;
        if (g.inLineSelect()) g.onButton(press(Button::B));
        settleDecryption(g);
        g.tick(1000);
        CHECK(g.nav() == Game::Nav::ItemOffer);
        tapC(g);
        CHECK(g.nav() == Game::Nav::Idle);
        g.tick(2000);
        CHECK(g.nav() == Game::Nav::Idle);
        CHECK(g.inventory().count("boot_accelerator") == 1);
    }
}
