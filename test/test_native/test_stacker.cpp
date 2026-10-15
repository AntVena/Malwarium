// test_stacker.cpp — native gates for the Stacker minigame, played MAINT runs and the
// Replication Ghost.
//
// One slice of the native suite; see test_gates.h for the shared fixtures and
// test_main.cpp for the roster that runs these. A save-vNN gate sits with the
// feature whose field it migrates, not in a migrations pile of its own.
#include "test_gates.h"

// How many blocks are locked in a row.
int stackerRowCount(const Stacker& s, int r) {
    int n = 0;
    for (int c = 0; c < kStackerCols; ++c) if (s.locked(r, c)) ++n;
    return n;
}

// Slide the run until its left edge reaches `col`, then lock it. Returns false if the
// column is unreachable, which would make the test a no-op rather than a failure.
bool stackerDropAt(Stacker& s, int col) {
    for (int guard = 0; guard < 4 * kStackerCols; ++guard) {
        if (s.left() == col) { s.drop(); return true; }
        s.step();
    }
    return false;
}

// Play a bare model the way the Game gates do (playStackerBoard, test_gates.h): lock
// where `stackerHoldLine` says, then let the intruders come.
void stackerHoldTheLine(Stacker& s) {
    for (int k = 0; s.running(); ++k) {
        const int want = stackerHoldLine(s, k);
        if (want < 0) break;
        CHECK(stackerDropAt(s, want));
    }
    for (int i = 0; i < 4 * kStackerThreatBeats * kStackerRows && s.running(); ++i) s.step();
}

void test_stacker_overhang_falls_and_leaves_the_hand() {
    Stacker s(kStackerGateSeed);
    CHECK(s.running());
    CHECK(s.width() == kStackerStartWidth);
    CHECK(s.row() == 0);

    // The first lock lands on open floor, so it keeps everything wherever it lands.
    CHECK(stackerDropAt(s, 1));
    CHECK(stackerRowCount(s, 0) == kStackerStartWidth);
    CHECK(s.locked(0, 1) && s.locked(0, 2) && s.locked(0, 3));
    CHECK(s.row() == 1);
    CHECK(s.width() == kStackerStartWidth);

    // Offset by one: the run lands on the wall, and the block over open floor falls to
    // it — still on the board, but out of the hand, which narrows by exactly that block.
    CHECK(stackerDropAt(s, 2));
    CHECK(stackerRowCount(s, 1) == 2);
    CHECK(s.locked(1, 2) && s.locked(1, 3));
    CHECK(!s.locked(1, 4) && s.locked(0, 4));     // fell to the floor, not shaved away
    CHECK(s.width() == 2);
    CHECK(s.row() == 2);
    CHECK(s.running());
}

// The move a second wall needs: a lock that misses the wall completely lands on the
// floor whole, keeps the full hand, and leaves the first wall where it was.
void test_stacker_complete_miss_starts_a_second_wall() {
    Stacker s(kStackerGateSeed);
    CHECK(stackerDropAt(s, 0));                   // first wall at columns 0..2
    CHECK(stackerDropAt(s, 0));                   // ...two high
    CHECK(stackerDropAt(s, 4));                   // nothing in common with it
    CHECK(s.running());
    CHECK(s.width() == kStackerStartWidth);
    CHECK(s.locked(0, 4) && s.locked(0, 5) && s.locked(0, 6));
    CHECK(s.height(0) == 2 && s.height(4) == 1);
    CHECK(s.row() == 2);                          // the run still slides over the tallest

    // A run over UNEVEN ground lands on the tallest column under it, and the rest fall
    // to their own columns' tops and leave the hand.
    CHECK(stackerDropAt(s, 2));                   // over heights 2, 0 and 1
    CHECK(s.width() == 1);                        // only column 2 landed level
    CHECK(s.height(2) == 3 && s.height(3) == 1 && s.height(4) == 2);
}

// The price of the ground: a lock that lands any block on it calls one more intruder,
// over one of the columns it touched; a lock that lands only on walls calls none.
void test_stacker_the_ground_calls_an_intruder() {
    Stacker s(kStackerGateSeed);
    CHECK(s.threatCount() == kStackerDealtThreats);
    CHECK(stackerDropAt(s, 0));
    CHECK(s.threatCount() == kStackerDealtThreats + 1);
    const int i = kStackerDealtThreats;
    CHECK(s.threatCol(i) >= 0 && s.threatCol(i) < kStackerStartWidth);
    CHECK(s.threatRow(i) == kStackerRows + kStackerCallDelay);   // waits above the well
    CHECK(stackerDropAt(s, 0));                   // on the wall: nothing called
    CHECK(s.threatCount() == kStackerDealtThreats + 1);
    CHECK(stackerDropAt(s, 2));                   // two blocks fall past to the ground
    CHECK(s.threatCount() == kStackerDealtThreats + 2);
    CHECK(s.threatCol(i + 1) == 3 || s.threatCol(i + 1) == 4);
}

// An intruder that reaches a wall is stopped and takes the block it hit; one that
// reaches the floor is the breach.
void test_stacker_an_intruder_takes_the_block_it_hits() {
    Stacker s(kStackerGateSeed);
    CHECK(stackerDropAt(s, 0));                   // one course under the column-0 intruder
    while (s.running() && s.destroyed() == 0) s.step();
    CHECK(s.running());
    CHECK(s.height(0) == 0);                      // its block is gone
    CHECK(s.blocks() == kStackerStartWidth - s.destroyed());
    // The column-4 intruder had nothing under it, and the next drop puts it through.
    for (int i = 0; i < kStackerThreatBeats && s.running(); ++i) s.step();
    CHECK(s.breached());

    // Nothing built at all: the opening intruders reach the floor and that's a breach.
    Stacker none(kStackerGateSeed);
    for (int i = 0; i < kStackerThreatBeats * (kStackerRows + 2) && none.running(); ++i)
        none.step();
    CHECK(none.breached() && none.blocks() == 0);
}

// The goal: every intruder stopped, the called ones included, is a held line.
void test_stacker_stopping_every_intruder_holds_the_line() {
    Stacker s(kStackerGateSeed);
    CHECK(s.threatCol(0) == 0 && s.threatCol(1) == 4);
    stackerHoldTheLine(s);
    CHECK(s.won() && !s.running());
    CHECK(s.width() == kStackerStartWidth);
    CHECK(s.threatCount() == 0);
    CHECK(s.destroyed() == s.freeLosses());       // the opening two, and the two called
    CHECK(s.blocks() == kStackerHoldLocks * kStackerStartWidth - s.destroyed());
    CHECK(s.score() == kStackerMaxScore);

    // A finished run ignores further input rather than resuming somewhere odd.
    const int blocks = s.blocks();
    s.step();
    s.drop();
    CHECK(s.blocks() == blocks && s.won());

    s.reset(kStackerGateSeed);                    // a fresh run starts over cleanly
    CHECK(s.running() && s.row() == 0 && s.width() == kStackerStartWidth);
    CHECK(s.blocks() == 0 && s.threatCount() == kStackerDealtThreats && s.destroyed() == 0);
}

// One wall, however tall, is a breach: the other opening intruder had nothing under it.
void test_stacker_one_wall_is_a_breach() {
    Stacker s(kStackerGateSeed);
    for (int i = 0; i < 5; ++i) CHECK(stackerDropAt(s, 0));
    for (int i = 0; i < 4 * kStackerThreatBeats * kStackerRows && s.running(); ++i) s.step();
    CHECK(s.breached() && !s.won());
    CHECK(s.score() > 0 && s.score() < kStackerHeldFloor);  // built, and still under any hold
}

// A held line is scored on what holding it cost: the losses every board takes are free,
// each one past them costs kStackerLossCost, and the floor keeps any hold above any
// breach.
void test_stacker_a_costly_hold_scores_less() {
    // One extra ground lock between the walls calls a fifth intruder, which takes a
    // fifth block — and the line still holds, for less.
    Stacker s(kStackerGateSeed);
    const int cols[] = {0, 4, 3, 0, 4, 0, 4, 0, 4};
    for (int c : cols) CHECK(stackerDropAt(s, c));
    CHECK(s.width() == 2);                        // the lock at 3 dropped one past a wall
    for (int i = 0; i < 4 * kStackerThreatBeats * kStackerRows && s.running(); ++i) s.step();
    CHECK(s.won());
    CHECK(s.destroyed() == s.freeLosses() + 1);
    CHECK(s.score() == kStackerMaxScore - kStackerLossCost);
    CHECK(s.score() > kStackerHeldFloor);
}

// A board gets kStackerDrops locks and no more; the board plays on without them, so
// what the walls hold when the locks run out is what they have to hold with.
void test_stacker_locks_run_out() {
    Stacker s(kStackerGateSeed);
    CHECK(s.dropsLeft() == kStackerDrops);
    for (int i = 0; i < kStackerDrops; ++i) {
        CHECK(stackerDropAt(s, i % 2 ? 4 : 0));   // two walls, so neither tops out
        CHECK(s.dropsLeft() == kStackerDrops - 1 - i);
    }
    const int blocks = s.blocks();
    CHECK(stackerDropAt(s, 0));                   // aligned, pressed, and nothing locks
    CHECK(s.blocks() == blocks && s.dropsLeft() == 0);
    CHECK(s.running());
}

// What one lock on the ground calls is the board's setting, and the losses a held line
// gets free scale with it.
void test_stacker_ground_calls_follow_the_setting() {
    for (int calls = 1; calls <= 3; ++calls) {
        Stacker s(kStackerGateSeed, calls);
        CHECK(stackerDropAt(s, 0));
        CHECK(s.threatCount() == kStackerDealtThreats + calls);
        CHECK(s.freeLosses() == kStackerDealtThreats * (1 + calls));
    }
    // The dial is what picks it: a HARD cabinet calls the most.
    Game g{StartMode::Hatched};
    enterArcadeCabinet(g, arcadeGameIndexById("stacker"), ArcadeDifficulty::Hard);
    g.onButton(press(Button::B));
    CHECK(g.stacker().calls() == kStackerGroundCalls[2]);
}

// The opening intruders land a full call delay ahead of the ones the opening locks call,
// so the impacts come one after another instead of all on one drop.
void test_stacker_called_intruders_land_after_the_opening_ones() {
    Stacker s(kStackerGateSeed);
    CHECK(stackerDropAt(s, 0));
    CHECK(stackerDropAt(s, 4));
    CHECK(s.threatCount() == 4);
    int firstLoss = -1, lastLoss = -1;
    for (int beat = 0; beat < 4 * kStackerThreatBeats * kStackerRows && s.running(); ++beat) {
        const int before = s.destroyed();
        s.step();
        if (s.destroyed() > before) {
            if (firstLoss < 0) firstLoss = beat;
            lastLoss = beat;
        }
    }
    CHECK(firstLoss >= 0);
    CHECK(lastLoss - firstLoss >= kStackerCallDelay * kStackerThreatBeats);
}

// The fast step moves two columns a beat; the bounce spends one of them, so a run
// reaches every column across a lap instead of only those of its starting parity.
void test_stacker_fast_run_reaches_every_column() {
    for (int narrow = 0; narrow < 2; ++narrow) {
        Stacker s(kStackerGateSeed);
        for (int i = 0; i < kStackerFastFromRow - 1; ++i) CHECK(stackerDropAt(s, 0));
        CHECK(stackerDropAt(s, narrow ? 1 : 0));  // offset by one: a hand of two
        CHECK(s.row() >= kStackerFastFromRow);
        CHECK(s.width() == (narrow ? 2 : kStackerStartWidth));
        const int span = kStackerCols - s.width();
        bool seen[kStackerCols] = {};
        for (int i = 0; i < 4 * kStackerCols; ++i) {
            s.step();
            seen[s.left()] = true;
        }
        for (int c = 0; c <= span; ++c) CHECK(seen[c]);
    }
}

// The seed picks the pair, across every column, and never two a single hand covers.
void test_stacker_seed_spreads_the_intruders() {
    bool seen[kStackerCols] = {};
    for (uint32_t seed = 0; seed < 64; ++seed) {
        const Stacker s(seed);
        CHECK(s.threatCol(1) - s.threatCol(0) >= kStackerThreatSpread);
        seen[s.threatCol(0)] = seen[s.threatCol(1)] = true;
    }
    for (int c = 0; c < kStackerCols; ++c) CHECK(seen[c]);
}

// The run stays ON the board, and every column stays SOLID from the floor — the
// property that lets height() count a column and a falling block land on its top.
void test_stacker_run_stays_on_board_and_columns_stay_solid() {
    Stacker s(kStackerGateSeed);
    for (int i = 0; i < 200; ++i) {
        s.step();
        CHECK(s.left() >= 0);
        CHECK(s.left() + s.width() <= kStackerCols);
    }
    Stacker n(kStackerGateSeed);
    const int cols[] = {0, 2, 4, 1, 3, 0, 4, 2};
    for (int col : cols) if (n.running() && col + n.width() <= kStackerCols) stackerDropAt(n, col);
    for (int c = 0; c < kStackerCols; ++c)
        for (int r = 0; r < kStackerRows; ++r)
            CHECK(n.locked(r, c) == (r < n.height(c)));
}

void test_stacker_row_width_reports_the_row() {
    Stacker s(kStackerGateSeed);
    CHECK(s.rowWidth(0) == 0);
    CHECK(stackerDropAt(s, 1));
    CHECK(s.rowWidth(0) == kStackerStartWidth);
    CHECK(stackerDropAt(s, 2));
    CHECK(s.rowWidth(1) == 2 && s.rowWidth(0) == kStackerStartWidth + 1);
    CHECK(s.rowWidth(-1) == 0 && s.rowWidth(kStackerRows) == 0);   // out of range
}

// score() is a breached board's whole reward curve, so the thing worth pinning is that
// HEIGHT is paid for twice — a block on a high row is worth more than the same block
// low down.
void test_stacker_score_pays_for_height_twice() {
    Stacker s(kStackerGateSeed);
    CHECK(s.score() == 0);
    CHECK(stackerDropAt(s, 0));
    CHECK(s.score() == kStackerStartWidth);
    CHECK(stackerDropAt(s, 0));
    CHECK(s.score() == kStackerStartWidth * 3);
    // The same three blocks on the floor elsewhere are worth only their level.
    CHECK(stackerDropAt(s, 4));
    CHECK(s.score() == kStackerStartWidth * 4);
}

// The two rows a board's SHAPE earns, and the tally underneath them. A perfect board and
// a one-block board are the two ends of the same measurement, so they are asserted
// against each other rather than one at a time — reading the wrong end of it is the
// mistake worth catching.
void test_stacker_win_credits_the_tally_and_the_shape_rows() {
    // Every lock on flat ground: nothing ever falls past a wall, so the line is held at
    // full width. That is the perfect board, and NOT the narrow one.
    Game g{StartMode::Hatched};
    g.model().setFragmentation(60);
    CHECK(g.stackerWins() == 0);
    startPlayedMaint(g, MaintKind::Av, ArcadeGameKind::Stacker);
    CHECK(playStackerBoard(g, [&](int k) { return stackerHoldLine(g.stacker(), k); }));
    CHECK(g.stacker().won());
    CHECK(g.stacker().width() == kStackerStartWidth);
    g.onButton(press(Button::B));                   // park -> finish
    CHECK(g.stackerWins() == 1);
    CHECK(g.hasAchievement(ach::kPerfectDefrag));
    CHECK(!g.hasAchievement(ach::kHangingByABit));

    // Down to a single block early, then held a column at a time: a win, but the
    // narrowest one there is. The tally belongs to the BOARD, so the same win counts on
    // the other process too.
    Game n{StartMode::Hatched};
    n.model().setFragmentation(60);
    startPlayedMaint(n, MaintKind::Defrag, ArcadeGameKind::Stacker);
    CHECK(playStackerBoard(n, [&](int k) { return stackerHoldNarrow(n.stacker(), k); }));
    CHECK(n.stacker().won());
    CHECK(n.stacker().width() == 1);
    n.onButton(press(Button::B));
    CHECK(n.stackerWins() == 1);
    CHECK(n.hasAchievement(ach::kHangingByABit));
    CHECK(!n.hasAchievement(ach::kPerfectDefrag));
}

// What `blocks` are worth off an AV scan at the dial's current setting: per block, up to
// what a full scan takes.
int scanWorth(const Game& g, int blocks, int frag) {
    int worth = blocks * kPlayFragPerUnit * arcadeRewardMult(g.maintDifficulty());
    if (worth > kAvReduction) worth = kAvReduction;
    return worth > frag ? frag : worth;
}

// A breached scan is NOT a failed scan: it pays for the blocks it landed in
// Fragmentation and costs nothing extra — but it cleans no process, because only a won
// run is a full scan. The halves are asserted together because taking only one of them
// is the mistake.
void test_stacker_short_board_pays_what_it_stacked_and_costs_nothing() {
    Game g{StartMode::Hatched};
    g.model().setFragmentation(60);
    g.model().setGhost(true);
    g.model().setDebuffs(2);
    const int mistakes = g.model().careMistakes();
    startPlayedMaint(g, MaintKind::Av, ArcadeGameKind::Stacker);
    // One wall, one course high, and the intruders left to drop: a breach.
    CHECK(playStackerBoard(g, [](int k) { return k == 0 ? 0 : -1; }));
    CHECK(!g.stacker().running() && !g.stacker().won());
    const int worth = scanWorth(g, g.stacker().blocks(), 60);
    CHECK(worth > 0);
    g.onButton(press(Button::B));
    CHECK(g.model().fragmentation() == 60 - worth);     // paid for, just not much
    CHECK(g.model().careMistakes() == mistakes);        // no failure, so no mistake
    CHECK(g.model().hasGhost() && g.model().debuffs() == 2);   // no process cleaned
    CHECK(g.stackerWins() == 0);                        // the tally counts CLEARED boards

    // Building is worth it even breached: the same wall built higher is worth more,
    // up to the cap a full scan takes. On EASY, so the cap is not reached first.
    Game low{StartMode::Hatched};
    low.model().setFragmentation(90);
    low.debugSetMaintDifficulty(ArcadeDifficulty::Easy);
    startPlayedMaint(low, MaintKind::Av, ArcadeGameKind::Stacker);
    CHECK(playStackerBoard(low, [](int k) { return k < 2 ? 0 : -1; }));
    Game high{StartMode::Hatched};
    high.model().setFragmentation(90);
    high.debugSetMaintDifficulty(ArcadeDifficulty::Easy);
    startPlayedMaint(high, MaintKind::Av, ArcadeGameKind::Stacker);
    CHECK(playStackerBoard(high, [](int k) { return k < 6 ? 0 : -1; }));
    low.onButton(press(Button::B));
    high.onButton(press(Button::B));
    CHECK(high.model().fragmentation() < low.model().fragmentation());
    CHECK(low.model().fragmentation() < 90);            // both still cleaned something
    CHECK(high.model().fragmentation() >= 90 - kAvReduction);   // never past a full scan
}

// A won scan is a FULL one that cannot fail: debuffs, the ghost and a Quick scan's worth
// of Fragmentation, all gone — however narrow the hand it was held with.
void test_stacker_cleared_board_is_a_full_scan() {
    Game g{StartMode::Hatched};
    g.model().setFragmentation(95);
    g.model().setGhost(true);
    g.model().setDebuffs(3);
    startPlayedMaint(g, MaintKind::Av, ArcadeGameKind::Stacker);
    CHECK(playStackerBoard(g, [&](int k) { return stackerHoldLine(g.stacker(), k); }));
    CHECK(g.stacker().won());
    g.onButton(press(Button::B));
    CHECK(g.model().fragmentation() == 95 - kAvReduction);
    CHECK(!g.model().hasGhost() && g.model().debuffs() == 0);
    CHECK(g.model().careMistakes() == 0);

    Game n{StartMode::Hatched};
    n.model().setFragmentation(95);
    n.model().setGhost(true);
    startPlayedMaint(n, MaintKind::Av, ArcadeGameKind::Stacker);
    CHECK(playStackerBoard(n, [&](int k) { return stackerHoldNarrow(n.stacker(), k); }));
    CHECK(n.stacker().won() && n.stacker().width() == 1);
    n.onButton(press(Button::B));
    CHECK(n.model().fragmentation() == 95 - kAvReduction);
    CHECK(!n.model().hasGhost());
}

// C stops the run early, and banking is the point: since a board that runs out of blocks
// keeps what it locked, quitting has to keep it too, or C would be a button that throws
// away Fragmentation the player already earned.
void test_stacker_stopping_early_banks_the_board() {
    Game g{StartMode::Hatched};
    g.model().setFragmentation(70);
    startPlayedMaint(g, MaintKind::Av, ArcadeGameKind::Stacker);
    for (int r = 0; r < 4; ++r) {
        for (int guard = 0; guard < 4 * kStackerCols; ++guard) {
            if (g.stacker().left() == 0) break;
            g.debugStepStacker();
        }
        g.onButton(press(Button::B));
    }
    CHECK(g.stacker().running());
    const int worth = scanWorth(g, g.stacker().blocks(), 70);
    CHECK(worth > 0);
    tapC(g);
    CHECK(g.model().fragmentation() == 70 - worth);
    CHECK(g.model().careMistakes() == 0);
    CHECK(g.stackerWins() == 0);
}

// The action screen's A cycle: past the fixed variants onto the PLAYED row at EASY, up
// the dial to HARD, then back to QUICK. B on the played row opens the game wired to that
// process, and a Defrag charges its Bits to do it.
void test_maint_played_variant_walks_the_dial() {
    Game d{StartMode::Hatched};
    d.debugSetBits(1000);
    d.model().setFragmentation(40);
    enterSubmenuId(d, SubmenuId::Maint);
    d.onButton(press(Button::B));                       // open DEFRAG
    for (int i = 0; i < kDefragVariants - 1; ++i) d.onButton(press(Button::A));
    CHECK(d.maintDifficulty() == ArcadeDifficulty::Easy);
    d.onButton(press(Button::A));
    CHECK(d.maintDifficulty() == ArcadeDifficulty::Medium);
    d.onButton(press(Button::A));
    CHECK(d.maintDifficulty() == ArcadeDifficulty::Hard);
    const int before = d.bits();
    d.onButton(press(Button::B));
    CHECK(d.nav() == (kDefragPlayedGame == ArcadeGameKind::Stacker ? Game::Nav::Stacker
                                                                   : Game::Nav::Fragger));
    CHECK(d.bits() < before);                           // the entry was charged

    // One more A from HARD wraps to QUICK, which rolls rather than plays.
    Game q{StartMode::Hatched};
    q.model().setGhost(true);
    enterSubmenuId(q, SubmenuId::Maint);
    q.onButton(press(Button::A));                       // AV row
    q.onButton(press(Button::B));                       // open AV
    for (int i = 0; i < kAvVariants - 1 + 2 + 1; ++i) q.onButton(press(Button::A));
    q.onButton(press(Button::B));
    CHECK(q.nav() == Game::Nav::Process);
}

// The wiring is content, not a rule of either process: point each process at the OTHER
// engine and both still settle on their own terms. A won FRAGGER run as an AV scan is a
// full scan; a Stacker board as a DEFRAG pays its blocks in frag, and — having no Bits
// unit — nothing in Bits.
void test_played_maint_engines_are_interchangeable() {
    Game a{StartMode::Hatched};
    a.model().setFragmentation(50);
    a.model().setGhost(true);
    startPlayedMaint(a, MaintKind::Av, ArcadeGameKind::Fragger);
    CHECK(a.nav() == Game::Nav::Fragger);
    for (int i = 0; i < kFraggerWinHits; ++i) {
        a.fragger().place(kFraggerRows - 1, a.fragger().lane(), Fragger::Cell::Fragment);
        a.onButton(press(Button::B));
    }
    a.fragger().place(kFraggerRows - 1, a.fragger().lane() == 0 ? 1 : 0,
                      Fragger::Cell::Fragment);
    a.debugStepFragger();                               // the run ends; it was won first
    a.onButton(press(Button::B));
    CHECK(a.nav() == Game::Nav::Process);
    CHECK(!a.model().hasGhost());
    CHECK(a.model().fragmentation() == 50 - kAvReduction);

    Game d{StartMode::Hatched};
    d.model().setFragmentation(100);
    d.debugSetMaintDifficulty(ArcadeDifficulty::Easy);
    const int bits = d.bits();
    startPlayedMaint(d, MaintKind::Defrag, ArcadeGameKind::Stacker);
    CHECK(d.nav() == Game::Nav::Stacker);
    // Three rows then a miss: a board short of the top, so no achievement pays Bits of
    // its own into the wallet being measured.
    CHECK(playStackerBoard(d, [](int k) { return k < 3 ? 0 : -1; }));
    CHECK(!d.stacker().won());
    const int blocks = d.stacker().blocks();
    d.onButton(press(Button::B));
    CHECK(d.model().fragmentation() == 100 - blocks * kPlayFragPerUnit);
    CHECK(d.bits() == bits);                            // no Bits unit, so no Bits
}

// The counting ladder rides the ordinary sweep, like every other counted series, and the
// tally is PLAYER-level: it has to survive the pet that set it.
void test_stacker_wins_ladder_sweeps_and_persists() {
    Game g{StartMode::Hatched};
    uint32_t t = 0;
    g.debugAddStackerWins(10);
    g.tick(t += kAchSweepIntervalMs);
    CHECK(g.hasAchievement("DEFRAG_BY_HAND"));
    CHECK(g.hasAchievement("STACK_10"));
    CHECK(!g.hasAchievement("STACK_50"));

    // v44 on the wire, and back into a Game: the tally is the operator's, so it has to
    // come back on a device that has been power-cycled since.
    SaveData a; std::strcpy(a.activeId, "paypup"); a.generation = 1;
    a.stackerWins = 10;
    SaveData out;
    CHECK(deserializeSave(serializeSave(a), out));
    CHECK(out.stackerWins == 10);
    MemSaveStore store; store.save(serializeSave(a));
    Game loaded(StartMode::Hatched, "paypup", &store);
    CHECK(loaded.stackerWins() == 10);
}

// The ROLLED/BOUGHT defrag seam, which the played one deliberately does not share: a
// Quick or Tool run takes a fixed bite or pays a fixed penalty, and the penalty is where
// the care mistake lives. Asserted here so a change to the played variant's own payout
// can't quietly move what the other two are worth.
void test_rolled_defrag_takes_its_fixed_bite() {
    Game g{StartMode::Hatched};
    g.model().setFragmentation(60);
    const int before = g.model().fragmentation();
    g.debugResolveDefrag(true);
    CHECK(g.model().fragmentation() == before - kDefragReduction);

    // ...and a lost board costs what a failed Quick defrag costs: the frag penalty plus
    // the care mistake, not merely "no clean".
    Game g2{StartMode::Hatched};
    g2.model().setFragmentation(60);
    const int mistakes = g2.model().careMistakes();
    const int frag = g2.model().fragmentation();
    g2.debugResolveDefrag(false);
    CHECK(g2.model().fragmentation() == frag + kMaintFailPenalty);
    CHECK(g2.model().careMistakes() == mistakes + 1);
}

// The Replication Ghost's whole lifecycle, which until now had no way to begin: the
// model carried a flag, a setter and a save field that nothing ever set true, so the AV
// screen's GHOST state and the AIR_GAPPED row were both unreachable by play.
// Worm-line only (game_care.cpp's resolveMaint) — a phantom self-copy is that line's
// own failure mode, the way Lockout is Ransomware's, so every raising case here hatches
// "vermicell" (content_evolution.cpp's Worm BootSector egg) rather than the default.
void test_replication_ghost_is_raised_by_a_failed_defrag_on_a_critical_disk() {
    // A failed defrag on a disk that is NOT yet critical is just a failed defrag.
    Game ok{StartMode::Hatched, "vermicell"};
    ok.model().setFragmentation(kFragCriticalMin - kMaintFailPenalty - 1);
    ok.debugResolveDefrag(false);
    CHECK(ok.model().fragmentation() < kFragCriticalMin);
    CHECK(!ok.model().hasGhost());

    // Fail one on an already-critical disk and the write forks a phantom copy.
    Game g{StartMode::Hatched, "vermicell"};
    g.model().setFragmentation(kFragCriticalMin);
    CHECK(!g.model().hasGhost());
    g.debugResolveDefrag(false);
    CHECK(g.model().hasGhost());

    // A SUCCESSFUL defrag never raises one, however bad the disk was.
    Game s{StartMode::Hatched, "vermicell"};
    s.model().setFragmentation(90);
    s.debugResolveDefrag(true);
    CHECK(!s.model().hasGhost());
}

// A Ransomware/Phishing/Trojan disk never forks a ghost, however critical and however
// badly the defrag fails — the same critical-disk failure that raises one on "vermicell"
// above must be a no-op here, or the mechanic isn't actually line-specific.
void test_replication_ghost_never_raised_off_the_worm_line() {
    Game g{StartMode::Hatched, "paypup"};             // Ransomware
    g.model().setFragmentation(kFragCriticalMin);
    g.debugResolveDefrag(false);
    CHECK(!g.model().hasGhost());
}

// The cure, and the achievement that marks it. Unlinkguine stays an ordinary dish when
// there is nothing to cure — which is the common case, and must not unlock anything.
// Dyno Nuggets, the everyday dish beside it, must NOT cure or unlock: that half is the
// point of the split, not incidental coverage.
void test_unlinkguine_cures_the_ghost_and_unlocks() {
    Game g{StartMode::Hatched, "vermicell"};
    g.inventory().add("unlinkguine", 2);

    // Eaten with no ghost: it feeds, and that is all. No unlock.
    g.model().setHunger(20);
    g.debugUseItem("unlinkguine");
    CHECK(g.model().hunger() > 20);                 // still an ordinary dish
    CHECK(!g.hasAchievement(ach::kAirGapped));

    // Raise a ghost the only way there is. The everyday snack does not touch it.
    g.model().setFragmentation(kFragCriticalMin);
    g.debugResolveDefrag(false);
    CHECK(g.model().hasGhost());
    g.inventory().add("dyno_nuggets", 1);
    g.debugUseItem("dyno_nuggets");
    CHECK(g.model().hasGhost());                    // still there — nuggets only feed
    CHECK(!g.hasAchievement(ach::kAirGapped));

    // ...and the dish that names the cure is the one that lands it.
    g.debugUseItem("unlinkguine");
    CHECK(!g.model().hasGhost());
    CHECK(g.hasAchievement(ach::kAirGapped));
}

// The AV scan is the other cure, and the two are deliberately the same shape as the
// defrag variants: the scan is free but can fail, Unlinkguine is an item that cannot. So
// a ghost must be reachable AND clearable through both, or one of the pair is decoration.
void test_ghost_also_clears_through_a_successful_av_scan() {
    Game g{StartMode::Hatched, "vermicell"};
    g.model().setFragmentation(kFragCriticalMin);
    g.debugResolveDefrag(false);
    CHECK(g.model().hasGhost());
    CHECK(!avGated(g.model()));                     // a ghost is work for the AV screen
    CHECK(!g.model().applyAntivirus(true));          // succeeded
    CHECK(!g.model().hasGhost());
    // Curing it this way is NOT the Air-Gapped achievement — that row names Unlinkguine.
    CHECK(!g.hasAchievement(ach::kAirGapped));
}

// Spoilage: a perishable held through a feeding turns into its `spoilsInto` row, and the
// conversion is one-for-one — a stack that rots must not lose or gain count, which is the
// failure a percentage roll would otherwise hide behind "unlucky".
void test_perishable_food_spoils_on_a_feeding() {
    Game g(StartMode::Hatched, "paypup");
    g.inventory().add("fresh_macrol", 40);
    int spoilages = 0, prevFresh = 40;
    for (int i = 0; i < 200; ++i) {
        g.inventory().add("dyno_nuggets", 1);
        g.debugUseItem("dyno_nuggets");   // eat something ELSE — the beat, not the item

        const int fresh = g.inventory().count("fresh_macrol");
        CHECK(fresh + g.inventory().count("spoiled_macrol") == 40);  // conserved either way
        CHECK(fresh >= prevFresh - 1);        // at most one unit per disturbance
        if (fresh < prevFresh) ++spoilages;
        prevFresh = fresh;
    }
    // A 5% roll that never fires across 200 feedings isn't riding the beat at all.
    CHECK(spoilages > 0);

    // An imperishable row never converts, however much the bag is disturbed — and the
    // chain stops after one stage rather than rotting away to nothing.
    Game h(StartMode::Hatched, "paypup");
    h.inventory().add("spoiled_macrol", 3);
    for (int i = 0; i < 200; ++i) {
        h.inventory().add("dyno_nuggets", 1);
        h.debugUseItem("dyno_nuggets");
    }
    CHECK(h.inventory().count("spoiled_macrol") == 3);
}

// Tinting: an icon master is a flat fill, so drawing it in another colour must keep
// its shape exactly and change nothing else. And the theme index has to be the only
// thing a colour lookup depends on — that indirection is what a theme swap rides on.
void test_icon_tint_and_theme_indirection() {
    const SpriteData* icon = embeddedAssets().sprite("ICON_ITEM_SEALED_CACHE");
    CHECK(icon != nullptr);

    Framebuffer plain(kActiveW, kActiveH), tinted(kActiveW, kActiveH);
    const Rgb565 bg = palColor(Pal::PAPER), ink = palColor(Pal::RARITY_EPIC);
    plain.clear(bg);
    tinted.clear(bg);
    drawSprite(plain, *icon, 0, 4, 4);
    drawSpriteTinted(tinted, *icon, 0, 4, 4, ink);

    int shape = 0;
    for (int y = 0; y < kActiveH; ++y) {
        for (int x = 0; x < kActiveW; ++x) {
            const bool onA = plain.get(x, y) != bg;
            const bool onB = tinted.get(x, y) != bg;
            CHECK(onA == onB);                       // identical silhouette
            if (onB) { ++shape; CHECK(tinted.get(x, y) == ink); }
        }
    }
    CHECK(shape > 0);                                // it actually drew something

    // The tier is countable BEFORE it is coloured: the four cache masters differ in
    // how many chevrons they carry, which is why tinting them stays decoration and
    // the ramp survives grayscale. Equal pixel counts would mean the tier had
    // quietly become colour-only.
    const char* tiers[] = {"ICON_ITEM_SEALED_CACHE_COMMON", "ICON_ITEM_SEALED_CACHE_UNCOMMON",
                           "ICON_ITEM_SEALED_CACHE_RARE", "ICON_ITEM_SEALED_CACHE_EPIC"};
    int on[4] = {0, 0, 0, 0};
    for (int i = 0; i < 4; ++i) {
        const SpriteData* s = embeddedAssets().sprite(tiers[i]);
        CHECK(s != nullptr);
        for (int y = 0; y < s->h; ++y)
            for (int x = 0; x < s->sheetW; ++x)
                if (spriteAlphaAt(*s, x, y) > 0) ++on[i];
    }
    for (int i = 0; i < 4; ++i)
        for (int j = i + 1; j < 4; ++j) CHECK(on[i] != on[j]);

    // Every lookup goes through the active theme, so an out-of-range index falls
    // back rather than reading past the table.
    setPalTheme(kPalThemeCount + 5);
    CHECK(palThemeIndex() == 0);
    setPalTheme(-1);
    CHECK(palThemeIndex() == 0);
    CHECK(palThemeName() != nullptr);
}

// The rename table's own invariants, so the rules in save.h are enforced rather than
// merely written down. Each row is a promise about ids that only ever LEAVE the content
// tables, so nothing else can notice when one goes stale.
void test_renamed_ids_table_invariants() {
    int n = 0;
    const RenamedId* rows = renamedIds(n);
    ContentRegistry r = ContentRegistry::embedded();

    for (int i = 0; i < n; ++i) {
        // Retirement: a row whose blobs the codec no longer opens is dead weight, so
        // raising kOldestAcceptedVersion is what tells you to delete it.
        CHECK(rows[i].sinceVersion >= kOldestAcceptedVersion);
        CHECK(rows[i].sinceVersion <= kSaveVersion);

        // The point of the table: `from` is gone from content, `to` is real. A `from`
        // that still resolves means the id was never actually retired.
        //
        // Asked of BOTH id spaces rather than of creatures alone: the table carried
        // nothing but creatures until v54 renamed an item, and a check that knows only
        // one kind would call a live item id a dead one.
        auto resolves = [&r](const char* id) {
            return r.creature(id) != nullptr || r.item(id) != nullptr;
        };
        CHECK(!resolves(rows[i].from));
        CHECK(resolves(rows[i].to));

        // Flattening: distinct `from`s, and no `to` that is itself renamed — the
        // rewrite takes exactly one hop and stops.
        for (int j = i + 1; j < n; ++j) CHECK(std::strcmp(rows[i].from, rows[j].from) != 0);
        for (int j = 0; j < n; ++j) CHECK(std::strcmp(rows[i].to, rows[j].from) != 0);
    }
}

void test_save_v39_raised_tally_roundtrip() {
    // A real evolution, written through the live autosave, read back cold.
    MemSaveStore store;
    { Game g(StartMode::Hatched, "pingcub", &store);
      g.debugTriggerEvolution();
      uint32_t t = 0; advanceToReveal(g, t);
      g.onButton(press(Button::B));
      CHECK(g.pet() && std::strcmp(g.pet()->id, "malbear") == 0);
      g.tick(t += kSaveDebounceMs + kHeartbeatMs); }   // flush the debounced save
    SaveData out;
    CHECK(deserializeSave(store.bytes(), out));
    CHECK(out.raisedCreatures.size() == 2);
    Game g2(StartMode::Hatched, "paypup", &store);   // hatchedCreature ignored
    CHECK(g2.pet() && std::strcmp(g2.pet()->id, "malbear") == 0);
    CHECK(g2.creatureRaised("pingcub"));               // nothing HOLDS pingcub
    CHECK(g2.speciesRaised() == 2);
}

// FULL_PEDIA_L1 wants a whole line RAISED, which is only ever true across time — a
// line is walked one stage at a time, so no player holds all of it at once and the
// possession test this replaced could never fire. Seeded through a save so the
// achievement's own re-check (fired by the evolution below) is what unlocks it.
void test_full_pedia_achievement_reads_the_raised_tally() {
    ContentRegistry r = ContentRegistry::embedded();
    SaveData a; std::strcpy(a.activeId, "pingcub"); a.generation = 1;
    for (const CreatureDef* c : r.allCreatures())
        if (c->line && std::strcmp(c->line, "ransomware") == 0 &&
            std::strcmp(c->id, "malbear") != 0) {      // the one still to be raised
            SaveId s{};
            std::strncpy(s.id, c->id, kSaveIdCap - 1);
            a.raisedCreatures.push_back(s);
        }
    MemSaveStore store; store.save(serializeSave(a));
    Game g(StartMode::Hatched, "pingcub", &store);
    CHECK(!g.hasAchievement("FULL_PEDIA_L1"));
    CHECK(!g.creatureRaised("malbear"));

    g.debugTriggerEvolution();                         // pingcub -> malbear: the last one
    uint32_t t = 0; advanceToReveal(g, t);
    g.onButton(press(Button::B));
    CHECK(g.creatureRaised("malbear"));
    g.tick(t += kAchSweepIntervalMs);      // the sweep is what unlocks it
    CHECK(g.hasAchievement("FULL_PEDIA_L1"));
}
