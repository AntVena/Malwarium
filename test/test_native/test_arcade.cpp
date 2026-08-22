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
    // ENDLESS off a cabinet: no byte count finishes it, so the run is only ever ended by
    // the player. kArcadeIsolationWinBytes is the till's win line, not the board's.
    CHECK(g.isolation().endless() && g.isolation().goal() == 0);
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

// --- The two ENDLESS cabinets ----------------------------------------------

// The button that wears skin `s` on the CHROMATOPHORE, in the order the chips draw.
static Button chromaSkinButton(int skin) {
    return skin == 0 ? Button::A : skin == 1 ? Button::B : Button::C;
}

// A cabinet run of the CHROMATOPHORE has no finish line: the till's line decides what a
// WIN is, and the score is free to leave it behind. What comes back is a high score,
// which is the only thing an endless board can be proud of.
void test_arcade_endless_chroma_records_a_high_score() {
    Game g{StartMode::Hatched};
    const int row = arcadeRowOf("chroma");
    CHECK(g.arcadeBest(row) == 0);
    uint32_t t = 0;

    // A short run: miss the very first pass on purpose.
    enterArcadeCabinet(g, row, ArcadeDifficulty::Medium);
    g.onButton(press(Button::B));                     // START
    CHECK(g.nav() == Game::Nav::Chroma);
    CHECK(g.chroma().endless() && g.chroma().goal() == 0);
    g.onButton(press(chromaSkinButton((g.chroma().plate() + 1) % kChromaSkins)));
    while (g.chroma().running()) g.tick(t += kFxAnimMs);
    CHECK(g.chroma().passes() == 0);
    g.onButton(press(Button::B));                     // bank it
    CHECK(g.nav() == Game::Nav::ArcadeResult);
    CHECK(g.arcadePlays(row) == 1);
    CHECK(g.arcadeWins(row) == 0);                    // nowhere near the line
    CHECK(g.arcadeBest(row) == 0);

    // A real one: play perfectly PAST the win line, then miss.
    g.onButton(press(Button::B));                     // dismiss the payout
    enterArcadeCabinet(g, row, ArcadeDifficulty::Medium);
    g.onButton(press(Button::B));
    while (g.chroma().running() && g.chroma().passes() <= kArcadeChromaWinPasses) {
        g.onButton(press(chromaSkinButton(g.chroma().plate())));
        g.tick(t += kFxAnimMs);
    }
    const int scored = g.chroma().passes();
    CHECK(scored > kArcadeChromaWinPasses);           // it did not stop at the line
    g.onButton(press(chromaSkinButton((g.chroma().plate() + 1) % kChromaSkins)));
    while (g.chroma().running()) g.tick(t += kFxAnimMs);
    g.onButton(press(Button::B));
    CHECK(g.arcadeWins(row) == 1);                    // past the line = a win
    CHECK(g.arcadeBest(row) == scored);               // ...and the best is uncapped
    CHECK(g.arcadeBestById("chroma") == scored);      // reachable by id, as a row keys it
}

// The high score is player-level and survives a power cycle (save v55) — a run with no
// finish line has a number and nothing else to come back for, so the number has to last.
void test_arcade_high_score_survives_a_reboot() {
    MemSaveStore store;
    int scored = 0;
    const int row = arcadeRowOf("chroma");
    {
        Game g{StartMode::Hatched, "paypup", &store};
        uint32_t t = 0;
        enterArcadeCabinet(g, row, ArcadeDifficulty::Medium);
        g.onButton(press(Button::B));
        while (g.chroma().running() && g.chroma().passes() < 3) {
            g.onButton(press(chromaSkinButton(g.chroma().plate())));
            g.tick(t += kFxAnimMs);
        }
        g.onButton(press(chromaSkinButton((g.chroma().plate() + 1) % kChromaSkins)));
        while (g.chroma().running()) g.tick(t += kFxAnimMs);
        scored = g.chroma().passes();
        CHECK(scored >= 3);
        g.onButton(press(Button::B));                 // bank -> the till records it
        CHECK(g.arcadeBest(row) == scored);
        g.tick(t + kSaveAutosaveMs + kHeartbeatMs);   // force the autosave
    }
    Game back{StartMode::Hatched, "paypup", &store};
    CHECK(back.arcadeBest(row) == scored);
    CHECK(back.arcadePlays(row) == 1);
}

// SAVE v55, THE ROLLBACK DIRECTION. A trial-boot that fails puts the PREVIOUS firmware
// back underneath a save the new one has already rewritten, so a v55 blob has to load on
// a build that has never heard of v55. It does, because the tail is written last and read
// behind its own version gate: rewriting the version stamp in place is exactly what an
// older reader sees — every field before the tail intact, and the tail simply not read.
void test_arcade_high_score_tail_is_rollback_safe() {
    MemSaveStore store;
    Game g{StartMode::Hatched, "paypup", &store};
    CHECK(g.setHackerTag("ROLLBACK_9"));
    g.tick(kSaveAutosaveMs + kHeartbeatMs);         // force the write
    std::vector<uint8_t> blob = store.bytes();
    CHECK(!blob.empty());

    SaveData now;
    CHECK(deserializeSave(blob, now));
    CHECK(now.arcadeBest.size() == now.arcadeIds.size());

    // The version stamp is a u16 right after the 4-byte magic (serializeSaveInto).
    CHECK(blob[4] == static_cast<uint8_t>(kSaveVersion & 0xFF));
    blob[4] = static_cast<uint8_t>(54 & 0xFF);
    blob[5] = static_cast<uint8_t>((54 >> 8) & 0xFF);

    SaveData older;
    CHECK(deserializeSave(blob, older));            // the old build still loads it...
    CHECK(older.arcadeBest.empty());                // ...and only loses what v55 added
    CHECK(std::strcmp(older.hackerTag, now.hackerTag) == 0);
    CHECK(older.arcadeIds.size() == now.arcadeIds.size());
    CHECK(older.bits == now.bits);
}
