// test_holds.cpp: the hold-B routing every tap/hold screen shares (Game::holdFocus,
// game_core.cpp). A B press arms one gesture; the hold and the release both go to the
// screen that is up, and to no other.
#include "test_gates.h"

// The MOVES picker's B is a tap/hold pair. Its state outlives the screen when the menu
// collapses on the idle timer, so a later B on another screen must not reach it: a tap on
// CFG's System Info is CFG's, and must neither unequip a move nor leave the page, and the
// full hold still reveals the Factory Reset.
void test_hold_b_reaches_only_the_screen_that_is_up() {
    Game g{StartMode::Hatched, "malbear"};
    CHECK(g.moveLoadout().equipped(0) != nullptr);
    uint32_t t = 1000;
    g.tick(t);
    enterLoadoutTab(g, 1);                        // the MOVES slot list
    tapB(g);                                      // its picker, on the UNEQUIP row
    CHECK(g.nav() == Game::Nav::Detail);
    g.onButton(lift(Button::A));                  // a held A would keep the tree open
    for (uint32_t end = t + 3 * kAutoDefocusMs; t < end && g.nav() != Game::Nav::Idle;)
        g.tick(t += kHeartbeatMs);                // the menu collapses under it
    CHECK(g.nav() == Game::Nav::Idle);
    if (g.nav() != Game::Nav::Idle) return;       // the walk below needs the carousel

    enterSubmenuId(g, SubmenuId::Cfg);            // listRow 0 == System Info
    g.onButton(press(Button::B));
    CHECK(g.nav() == Game::Nav::Detail);
    CHECK(g.cfgScreen() == CfgScreen::SysInfo);
    tapB(g);
    CHECK(g.nav() == Game::Nav::Detail);
    CHECK(g.cfgScreen() == CfgScreen::SysInfo);
    CHECK(g.moveLoadout().equipped(0) != nullptr);

    g.tick(t += kHeartbeatMs);
    g.onButton(press(Button::B));
    g.tick(t += kFactoryRevealMs + kHeartbeatMs);
    CHECK(g.cfgScreen() == CfgScreen::FactoryReset);
    g.onButton(lift(Button::B));
}
