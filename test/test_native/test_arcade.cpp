// test_arcade.cpp — native gates for the GAMES arcade: the till, the difficulty dial,
// and the promise that a cabinet run never touches what the game's own context owns.
//
// One slice of the native suite; see test_gates.h for the shared fixtures and
// test_main.cpp for the roster that runs these. A save-vNN gate sits with the
// feature whose field it migrates, not in a migrations pile of its own.
#include "test_gates.h"

namespace {

// Roster positions the gates below drive. Read from the table rather than typed, so a
// reordered roster moves the tests with it instead of quietly testing the wrong game.
int arcadeRowOf(const char* id) { return arcadeGameIndexById(id); }

}  // namespace

// The flat half of the payout is for FINISHING, not for winning: a run abandoned on the
// first row still banks it. This is the whole pitch of the arcade, so it is asserted
// against the worst possible run rather than a good one.
void test_arcade_pays_the_attempt() {
    Game g{StartMode::Hatched};
    g.model().setHappiness(40);
    const int bits = g.bits();
    const int row = arcadeRowOf("stacker");
    CHECK(row >= 0);
    CHECK(g.arcadePlays(row) == 0);

    enterArcadeCabinet(g, row, ArcadeDifficulty::Medium);
    g.onButton(press(Button::B));                 // START
    CHECK(g.nav() == Game::Nav::Stacker);
    CHECK(g.inArcadeRun());
    tapC(g);                 // stop on row 0 — nothing banked

    CHECK(g.nav() == Game::Nav::ArcadeResult);
    CHECK(!g.inArcadeRun());
    CHECK(g.bits() == bits + kArcadePlayBits);    // the score bonus is zero, the flat isn't
    CHECK(g.model().happiness() == 50);
    CHECK(g.arcadePlays(row) == 1);
    CHECK(g.arcadeWins(row) == 0);

    // The payout screen is informational: any press returns to the cabinet list, and
    // the run cannot be banked twice by pressing again.
    g.onButton(press(Button::B));
    CHECK(g.nav() == Game::Nav::Submenu);
    CHECK(g.bits() == bits + kArcadePlayBits);
}

// A cleared board takes the whole bonus, and a won run counts on the cabinet's record.
void test_arcade_perfect_board_pays_the_bonus() {
    Game g{StartMode::Hatched};
    const int bits = g.bits();
    const int row = arcadeRowOf("stacker");
    enterArcadeCabinet(g, row, ArcadeDifficulty::Medium);
    g.onButton(press(Button::B));                 // START
    // Column 0 every row: nothing overhangs, so the run reaches the top at full width —
    // the maximum score the cabinet advertises.
    CHECK(playStackerBoard(g, [](int) { return 0; }));
    CHECK(g.stacker().won());
    CHECK(g.stacker().score() == kStackerMaxScore);
    g.onButton(press(Button::B));                 // park -> finish

    CHECK(g.nav() == Game::Nav::ArcadeResult);
    CHECK(g.bits() == bits + kArcadePlayBits + kArcadeScoreBits);
    CHECK(g.arcadeWins(row) == 1);
}

// The contract that makes the arcade safe to reuse a real minigame: a cabinet run is
// off the disk entirely. No Bits charged to enter, no Fragmentation cleaned, no defrag
// tally, and none of the Defrag's achievements — a played board in MAINT does all four.
void test_arcade_stacker_leaves_the_disk_alone() {
    Game g{StartMode::Hatched};
    g.model().setFragmentation(60);
    const int defrags = g.defragCount();
    const int wins = g.stackerWins();

    enterArcadeCabinet(g, arcadeRowOf("stacker"), ArcadeDifficulty::Medium);
    const int bitsAtStart = g.bits();
    g.onButton(press(Button::B));                 // START — costs nothing
    CHECK(g.bits() == bitsAtStart);
    CHECK(playStackerBoard(g, [](int) { return 0; }));
    g.onButton(press(Button::B));

    CHECK(g.model().fragmentation() == 60);       // the disk is untouched
    CHECK(g.defragCount() == defrags);
    CHECK(g.stackerWins() == wins);               // the played-defrag ladder isn't this one
    CHECK(!g.hasAchievement(ach::kPerfectDefrag));
}

// The dial actually moves the paced games: the same elapsed time yields strictly more
// steps on HARD than on MEDIUM, and strictly fewer on EASY. Counted off the run's own
// position, since a step is the only thing that moves it.
void test_arcade_difficulty_paces_the_run() {
    auto stepsInOneSecond = [](ArcadeDifficulty d) {
        Game g{StartMode::Hatched};
        enterArcadeCabinet(g, arcadeRowOf("stacker"), d);
        g.onButton(press(Button::B));
        int steps = 0, last = g.stacker().left();
        for (uint32_t ms = 1; ms <= 1000; ++ms) {
            g.tick(ms);
            if (g.stacker().left() != last) { ++steps; last = g.stacker().left(); }
        }
        return steps;
    };
    const int easy = stepsInOneSecond(ArcadeDifficulty::Easy);
    const int medium = stepsInOneSecond(ArcadeDifficulty::Medium);
    const int hard = stepsInOneSecond(ArcadeDifficulty::Hard);
    CHECK(easy > 0);
    CHECK(easy < medium);
    CHECK(medium < hard);
}

// The Clutch has no clock to speed up, so its dial is how many times the raft halves —
// a narrower survivor, found from less information. The run now resolves the instant
// the live egg falls out of the surviving span (see game_eggpick.cpp), so the dial is
// a CEILING, not a guaranteed length — only a run that keeps the egg in span the whole
// way actually spends every round the dial allows. Asserted on that perfect path, which
// is still the one thing the setting is allowed to change.
void test_arcade_clutch_rounds_follow_the_dial() {
    auto roundsToResolvePerfect = [](ArcadeDifficulty d) {
        Game g{StartMode::Hatched};
        enterArcadeCabinet(g, arcadeRowOf("clutch"), d);
        g.onButton(press(Button::B));             // START
        CHECK(g.nav() == Game::Nav::ModalEggPick);
        // Mirror game_eggpick.cpp's splitsColumns: columns halve first, alternating
        // with rows, except once one axis is down to a single track the other takes
        // every remaining round.
        const int col = g.eggPickTargetSlot() % Game::kEggPickCols;
        const int row = g.eggPickTargetSlot() / Game::kEggPickCols;
        int c0 = 0, cw = Game::kEggPickCols, r0 = 0, rh = Game::kEggPickRows;
        int commits = 0;
        while (!g.eggPickResolved() && commits < 16) {
            const bool byColumn = cw >= 2 && (rh < 2 || commits % 2 == 0);
            bool second;
            if (byColumn) { cw /= 2; second = col >= c0 + cw; if (second) c0 += cw; }
            else          { rh /= 2; second = row >= r0 + rh; if (second) r0 += rh; }
            g.onButton(press(second ? Button::C : Button::A));
            g.onButton(press(Button::B));
            ++commits;
        }
        CHECK(g.eggPickWon());   // the egg was kept in span every round — never lost early
        return commits;
    };
    CHECK(roundsToResolvePerfect(ArcadeDifficulty::Easy) == kArcadeClutchRoundsEasy);
    CHECK(roundsToResolvePerfect(ArcadeDifficulty::Medium) == kArcadeClutchRoundsMedium);
    CHECK(roundsToResolvePerfect(ArcadeDifficulty::Hard) == kArcadeClutchRoundsHard);
}

// An Isolation cabinet run has no egg behind it: it must not shave the incubation clock
// (there is none) and must not fire the hatch's own achievement, however clean it is.
// Driven to a crash, which is the run every player will actually see.
void test_arcade_isolation_is_off_the_clock() {
    Game g{StartMode::Hatched};
    const int bits = g.bits();
    const int row = arcadeRowOf("isolation");
    enterArcadeCabinet(g, row, ArcadeDifficulty::Medium);
    g.onButton(press(Button::B));                 // START
    CHECK(g.nav() == Game::Nav::Isolation);
    CHECK(g.isolation().goal() == kArcadeIsolationGoal);
    CHECK(!g.inEggPhase());                       // a raised pet, playing for fun

    // Steer nothing: the worm starts heading right and walks into the far wall, which
    // is the fastest honest end (turning every step just cycles it forever — see
    // test_isolation_cycle_run_finishes_clean).
    for (int i = 0; i < 400 && g.isolation().running(); ++i)
        g.tick(static_cast<uint32_t>((i + 1) * 400));
    CHECK(!g.isolation().running());
    g.onButton(press(Button::B));                 // park -> finish

    CHECK(g.nav() == Game::Nav::ArcadeResult);
    CHECK(g.bits() >= bits + kArcadePlayBits);
    CHECK(g.arcadePlays(row) == 1);
    CHECK(!g.hasAchievement(ach::kWormWhisperer));
}

// The three ladders read off the same two tallies, so the gate that matters is that
// they read them DIFFERENTLY — a losing run must move plays and losses but not wins.
void test_arcade_ladders_split_plays_wins_losses() {
    Game g{StartMode::Hatched};
    const int row = arcadeRowOf("stacker");
    CHECK(!g.hasAchievement("ARCADE_FIRST"));

    // A run abandoned on the first row: a play and a loss, and not a win.
    enterArcadeCabinet(g, row, ArcadeDifficulty::Medium);
    g.onButton(press(Button::B));
    tapC(g);
    g.onButton(press(Button::B));                 // dismiss the payout
    g.tick(kAchSweepIntervalMs + 1);
    CHECK(g.hasAchievement("ARCADE_FIRST"));      // the plays ladder moved
    CHECK(!g.hasAchievement("TOWER_OF_FRAGGLE")); // the cabinet's win row did not

    // A cleared board on the same cabinet is the win that row is waiting for.
    enterArcadeCabinet(g, row, ArcadeDifficulty::Medium);
    g.onButton(press(Button::B));
    CHECK(playStackerBoard(g, [](int) { return 0; }));
    g.onButton(press(Button::B));
    g.tick(2 * kAchSweepIntervalMs + 2);
    CHECK(g.hasAchievement("TOWER_OF_FRAGGLE"));
}

// The board's own joke, and the one arcade row that is a SHAPE rather than a tally:
// losing the whole hand on the second lock. Fires wherever the board is played, so it
// is asserted through the DEFRAG variant — the context the arcade borrowed it from.
void test_stack_overflow_fires_on_the_second_row() {
    Game g{StartMode::Hatched};
    g.model().setFragmentation(60);
    g.debugStartStackerDefrag();
    // Base row at 0..2, then a lock with no column in common with it: nothing survives,
    // and the run ends on row 1.
    CHECK(playStackerBoard(g, [](int r) { return r == 0 ? 0 : 4; }));
    CHECK(!g.stacker().won() && g.stacker().row() == 1);
    g.onButton(press(Button::B));
    CHECK(g.hasAchievement(ach::kStackOverflow));

    // A board that gets further does not earn it — the row is the second one, exactly.
    Game n{StartMode::Hatched};
    n.model().setFragmentation(60);
    n.debugStartStackerDefrag();
    CHECK(playStackerBoard(n, [](int r) { return r < 2 ? 0 : 4; }));
    CHECK(!n.stacker().won() && n.stacker().row() == 2);
    n.onButton(press(Button::B));
    CHECK(!n.hasAchievement(ach::kStackOverflow));
}

// v47: the per-cabinet tallies survive a reboot, and they are keyed by the cabinet's
// id rather than by its row — so the tally that comes back is the one that went out.
void test_arcade_tallies_persist() {
    MemSaveStore store;
    const int row = arcadeGameIndexById("stacker");
    {
        Game g(StartMode::Hatched, "malbear", &store);
        enterArcadeCabinet(g, row, ArcadeDifficulty::Medium);
        g.onButton(press(Button::B));
        CHECK(playStackerBoard(g, [](int) { return 0; }));
        g.onButton(press(Button::B));             // finish -> banked
        CHECK(g.arcadePlays(row) == 1 && g.arcadeWins(row) == 1);
        g.tick(kSaveAutosaveMs + kHeartbeatMs);   // autosave
    }
    {
        Game g(StartMode::Hatched, "malbear", &store);
        CHECK(g.arcadePlays(row) == 1);
        CHECK(g.arcadeWins(row) == 1);
        // A cabinet nobody played writes no row at all, and reads back as untouched.
        CHECK(g.arcadePlays(arcadeGameIndexById("clutch")) == 0);
    }
}
