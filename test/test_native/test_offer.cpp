// test_offer.cpp — the ITEM OFFER (game_offer.cpp), AUTO DIVE GEAR, and the DIVE PAY
// bonus a bell or Deep-Learning device carries.
#include "test_gates.h"

using namespace mal;

namespace {
Game divingReadyGame(MemSaveStore* store = nullptr) {
    Game g{StartMode::Hatched, "bruinforce", store};
    for (int a = 0; a < kExplSectors; ++a) g.debugSetSectorCleared(a, true);
    return g;
}
}  // namespace

// A bell and a device armed together pay their DIVE PAY added, for the dive they were
// armed for and only while it runs — a dive started with nothing armed pays nothing.
void test_dive_gear_pays_a_bonus() {
    Game g = divingReadyGame();
    g.inventory().add("kernel_bell", 1);           // +100
    g.inventory().add("deep_learning_module", 1);  // +50
    g.debugUseItem("kernel_bell");
    g.debugUseItem("deep_learning_module");
    CHECK(g.debugDiveRewardBonusPct() == 0);       // armed, but nothing to pay yet
    g.debugStartDeepWebDive();
    CHECK(g.inDeepWebDive());
    CHECK(g.debugDiveRewardBonusPct() == 150);
    stopExplore(g);
    CHECK(g.debugDiveRewardBonusPct() == 0);
    g.debugStartDeepWebDive();                     // a fresh dive, nothing re-armed
    CHECK(g.debugDiveRewardBonusPct() == 0);
}

// Without AUTO DIVE GEAR, starting a dive with dive items held opens DIVE PREP. B uses
// the focused item and keeps the list up while anything is left; when nothing is, the
// dive starts. C backs out with no dive and nothing spent.
void test_dive_prep_offer() {
    {
        Game g = divingReadyGame();
        g.inventory().add("backdoor_bell", 1);
        g.inventory().add("deep_learning_module", 1);
        g.debugOfferThenDive();
        CHECK(g.nav() == Game::Nav::ItemOffer);
        Framebuffer fb(kActiveW, kActiveH);
        g.render(fb);
        CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));
        tapB(g);                                   // the bell (inventory order)
        CHECK(g.inventory().count("backdoor_bell") == 0);
        CHECK(g.nav() == Game::Nav::ItemOffer);    // the module is still on offer
        CHECK(!g.inDeepWebDive());
        tapB(g);                                   // the module; nothing left -> dive
        CHECK(g.inventory().count("deep_learning_module") == 0);
        CHECK(g.inDeepWebDive());
        CHECK(g.debugDiveRewardBonusPct() == 25 + 50);
    }
    {
        Game g = divingReadyGame();
        g.inventory().add("backdoor_bell", 1);
        g.debugOfferThenDive();
        CHECK(g.nav() == Game::Nav::ItemOffer);
        tapC(g);
        CHECK(g.nav() != Game::Nav::ItemOffer);
        CHECK(!g.inDeepWebDive());
        CHECK(g.inventory().count("backdoor_bell") == 1);
    }
    {
        // Nothing held that a dive can use: no screen, straight in.
        Game g = divingReadyGame();
        g.debugOfferThenDive();
        CHECK(g.inDeepWebDive());
    }
}

// AUTO DIVE GEAR's three positions. YES arms the deepest bell and the strongest device
// with no screen, leaving the shallower bell in the bag; ASK is DIVE PREP; NO dives
// with nothing armed. The position survives a reboot.
void test_auto_dive_gear_modes() {
    MemSaveStore store;
    {
        Game g = divingReadyGame(&store);
        g.debugSetBits(kRigAutoDiveGearCost);
        g.debugBuyRigRow(kRigRowAutoDiveGear);
        CHECK(g.rigServiceMode(kRigRowAutoDiveGear) == Game::ServiceMode::On);
        CHECK(std::strcmp(g.rigServiceWord(kRigRowAutoDiveGear), "YES") == 0);
        g.inventory().add("backdoor_bell", 1);
        g.inventory().add("kernel_bell", 1);
        g.inventory().add("deep_learning_module", 1);

        g.debugOfferThenDive();                    // YES
        CHECK(g.inDeepWebDive());
        CHECK(g.inventory().count("kernel_bell") == 0);
        CHECK(g.inventory().count("backdoor_bell") == 1);
        CHECK(g.inventory().count("deep_learning_module") == 0);
        CHECK(g.debugDiveRewardBonusPct() == 100 + 50);
        stopExplore(g);

        g.toggleRigService(kRigRowAutoDiveGear);   // -> ASK
        CHECK(g.rigServiceMode(kRigRowAutoDiveGear) == Game::ServiceMode::Ask);
        g.debugOfferThenDive();
        CHECK(g.nav() == Game::Nav::ItemOffer);
        tapC(g);

        g.toggleRigService(kRigRowAutoDiveGear);   // -> NO
        CHECK(g.rigServiceMode(kRigRowAutoDiveGear) == Game::ServiceMode::Off);
        g.debugOfferThenDive();
        CHECK(g.inDeepWebDive());
        CHECK(g.inventory().count("backdoor_bell") == 1);
        CHECK(g.debugDiveRewardBonusPct() == 0);
        stopExplore(g);

        g.toggleRigService(kRigRowAutoDiveGear);   // NO -> YES
        CHECK(g.rigServiceMode(kRigRowAutoDiveGear) == Game::ServiceMode::On);
        g.toggleRigService(kRigRowAutoDiveGear);   // -> ASK, and keep it there
        g.tick(kSaveAutosaveMs + kHeartbeatMs);
    }
    Game g2(StartMode::Hatched, "bruinforce", &store);
    CHECK(g2.rigServiceMode(kRigRowAutoDiveGear) == Game::ServiceMode::Ask);
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
