// test_fragger.cpp — native gates for FRAGGER, as a played MAINT run and a cabinet.
//
// One slice of the native suite; see test_gates.h for the shared fixtures and
// test_main.cpp for the roster that runs these.
#include "test_gates.h"

namespace {

// A run with nothing on the disk, the head in the middle lane, on the widest board. The
// board starts empty, so a test stages exactly the blocks it needs with Fragger::place.
Fragger emptyRun() {
    Fragger f;
    f.reset(7, kFraggerMaxLanes);
    return f;
}

// A played DEFRAG run of FRAGGER, whatever the content table pairs with DEFRAG.
void startDefragRun(Game& g) { startPlayedMaint(g, MaintKind::Defrag, ArcadeGameKind::Fragger); }

// Shoot `n` fragments through the REAL button path: stage one over the head, fire.
void shootFragments(Game& g, int n) {
    for (int i = 0; i < n; ++i) {
        g.fragger().place(kFraggerRows - 1, g.fragger().lane(), Fragger::Cell::Fragment);
        g.onButton(press(Button::B));
    }
}

// Land `n` files straight onto the head — one row of fall each. Kept well under
// kFraggerRows steps in a test, so nothing the spawner drops can land meanwhile.
void catchFiles(Game& g, int n) {
    for (int i = 0; i < n; ++i) {
        g.fragger().place(kFraggerRows - 1, g.fragger().lane(), Fragger::Cell::File);
        g.debugStepFragger();
    }
}

// End the run the only way it ends: a fragment reaching the disk, in a lane the head
// is not covering. Leaves it parked on the result.
void crashRun(Game& g) {
    const int lane = g.fragger().lane() == 0 ? 1 : 0;
    g.fragger().place(kFraggerRows - 1, lane, Fragger::Cell::Fragment);
    g.debugStepFragger();
}

}  // namespace

// The shot meets the LOWEST block in the head's lane: a fragment is destroyed, a file
// stops it with nothing lost, and an empty lane is a miss. The file rule is the one that
// makes this a game rather than a reflex test, so it is asserted beside the hit.
void test_fragger_shot_meets_the_lowest_block() {
    Fragger f = emptyRun();
    const int lane = f.lane();
    int row = 0;
    CHECK(f.fire(&row) == Fragger::Shot::Nothing);
    CHECK(row == -1);

    f.place(3, lane, Fragger::Cell::Fragment);
    f.place(6, lane, Fragger::Cell::File);              // below the fragment: a shield
    CHECK(f.fire(&row) == Fragger::Shot::Blocked);
    CHECK(row == 6);
    CHECK(f.hits() == 0);
    CHECK(f.cell(6, lane) == Fragger::Cell::File);      // the file survives the shot
    CHECK(f.cell(3, lane) == Fragger::Cell::Fragment);

    f.place(6, lane, Fragger::Cell::Empty);             // the file has landed
    CHECK(f.fire(&row) == Fragger::Shot::Hit);
    CHECK(row == 3);
    CHECK(f.hits() == 1);
    CHECK(f.cell(3, lane) == Fragger::Cell::Empty);
}

// A file reaching the disk is written and the run goes on; a fragment reaching it ends
// the run, in the lane it landed in.
void test_fragger_landing_fragment_ends_the_run() {
    Fragger f = emptyRun();
    f.place(kFraggerRows - 1, 2, Fragger::Cell::File);
    f.step();
    CHECK(f.running());
    CHECK(f.written() == 1);

    f.place(kFraggerRows - 1, kFraggerMaxLanes - 1, Fragger::Cell::Fragment);
    f.step();
    CHECK(!f.running());
    CHECK(f.crashLane() == kFraggerMaxLanes - 1);
    // A crashed run is frozen: nothing moves, and nothing more can be scored.
    f.place(kFraggerRows - 1, f.lane(), Fragger::Cell::Fragment);
    CHECK(f.fire() == Fragger::Shot::Nothing);
    CHECK(f.hits() == 0);
}

// The head stops at the walls rather than wrapping.
void test_fragger_head_stops_at_the_walls() {
    Fragger f = emptyRun();
    for (int i = 0; i < 2 * kFraggerMaxLanes; ++i) f.moveLeft();
    CHECK(f.lane() == 0);
    for (int i = 0; i < 2 * kFraggerMaxLanes; ++i) f.moveRight();
    CHECK(f.lane() == kFraggerMaxLanes - 1);
}

// The fall quickens with every fragment shot, and stops quickening at its floor — which
// is where the arcade's win line sits, so a run that passes it has seen the whole curve.
void test_fragger_pace_ramps_to_a_floor() {
    Fragger f = emptyRun();
    CHECK(f.stepMs() == kFraggerStartStepMs);
    int last = f.stepMs();
    for (int i = 0; i < kFraggerWinHits; ++i) {
        f.place(0, f.lane(), Fragger::Cell::Fragment);
        CHECK(f.fire() == Fragger::Shot::Hit);
        CHECK(f.stepMs() <= last);
        last = f.stepMs();
    }
    CHECK(f.stepMs() == kFraggerMinStepMs);
    f.place(0, f.lane(), Fragger::Cell::Fragment);
    f.fire();
    CHECK(f.stepMs() == kFraggerMinStepMs);
    // ...and the floor stays clear of the panel's fastest repaint.
    CHECK(kFraggerMinStepMs >= kStackerStepMs);
}

// The spawner never stacks two blocks in adjacent rows of a lane. That gap is what keeps
// the file rule fair — a fragment behind a file always has two rows of fall left when the
// file lands — so it is checked over a long run rather than trusted. Also pins that a
// seed replays exactly.
void test_fragger_spawns_keep_a_gap_and_replay() {
    Fragger a, b;
    a.reset(12345, kFraggerMaxLanes);
    b.reset(12345, kFraggerMaxLanes);
    int spawned = 0;
    for (int s = 0; s < 2000; ++s) {
        // Clear the bottom row first so nothing ever crashes: the spawn is under test,
        // not the player.
        for (int l = 0; l < kFraggerMaxLanes; ++l) {
            a.place(kFraggerRows - 1, l, Fragger::Cell::Empty);
            b.place(kFraggerRows - 1, l, Fragger::Cell::Empty);
        }
        a.step();
        b.step();
        for (int l = 0; l < kFraggerMaxLanes; ++l) {
            if (a.cell(0, l) != Fragger::Cell::Empty) ++spawned;
            for (int r = 0; r + 1 < kFraggerRows; ++r)
                CHECK(!(a.cell(r, l) != Fragger::Cell::Empty &&
                        a.cell(r + 1, l) != Fragger::Cell::Empty));
            for (int r = 0; r < kFraggerRows; ++r) CHECK(a.cell(r, l) == b.cell(r, l));
        }
    }
    CHECK(a.running());
    CHECK(spawned > 500);                               // the disk is actually busy
}

// The DEFRAG payout splits by what was done: every fragment SHOT is kPlayFragPerUnit off
// the disk while it has any, and every file CAUGHT is kPlayBitsPerUnit into the wallet
// whether or not it does (both on EASY's x1), so a run that outlasts a clean disk is still
// worth playing. A file that lands away from the head pays nothing. A landed fragment
// ends the run with no penalty and no mistake.
void test_fragger_defrag_pays_per_fragment() {
    Game g{StartMode::Hatched};
    g.model().setFragmentation(5);
    const int bits = g.bits();
    const int mistakes = g.model().careMistakes();
    const int defrags = g.defragCount();
    g.debugSetMaintDifficulty(ArcadeDifficulty::Easy);
    startDefragRun(g);
    CHECK(g.nav() == Game::Nav::Fragger);
    shootFragments(g, 8);                               // three past a clean disk
    CHECK(g.fragger().hits() == 8);
    catchFiles(g, 3);
    CHECK(g.fragger().caught() == 3);
    // A file landing in another lane is written, not caught.
    g.fragger().place(kFraggerRows - 1, g.fragger().lane() == 0 ? 1 : 0, Fragger::Cell::File);
    g.debugStepFragger();
    CHECK(g.fragger().caught() == 3 && g.fragger().written() == 4);
    crashRun(g);
    CHECK(!g.fragger().running());
    g.onButton(press(Button::A));                       // any press settles the run

    CHECK(g.nav() == Game::Nav::Process);
    CHECK(g.model().fragmentation() == 0);
    CHECK(g.bits() == bits + 3 * kPlayBitsPerUnit);
    CHECK(g.model().careMistakes() == mistakes);
    CHECK(!g.model().hasGhost());
    CHECK(g.defragCount() == defrags + 1);

    // A run that shot nothing changes nothing, and does not count as a defrag.
    Game z{StartMode::Hatched};
    z.model().setFragmentation(kFragCriticalMin + 5);
    startDefragRun(z);
    const int zBits = z.bits();
    crashRun(z);
    z.onButton(press(Button::B));
    CHECK(z.model().fragmentation() == kFragCriticalMin + 5);
    CHECK(z.bits() == zBits);
    CHECK(z.defragCount() == 0);
    CHECK(!z.model().hasGhost());                       // a played run never forks one
}

// Off a cabinet the run is off the disk entirely, the till pays it, and its hit count is
// the score the cabinet keeps as a high score.
void test_fragger_cabinet_leaves_the_disk_alone() {
    Game g{StartMode::Hatched};
    g.model().setFragmentation(60);
    const int row = arcadeGameIndexById("fragger");
    CHECK(row >= 0);
    enterArcadeCabinet(g, row, ArcadeDifficulty::Medium);
    const int bits = g.bits();
    g.onButton(press(Button::B));                       // START — costs nothing
    CHECK(g.nav() == Game::Nav::Fragger);
    shootFragments(g, kFraggerWinHits + 5);
    crashRun(g);
    g.onButton(press(Button::B));

    CHECK(g.nav() == Game::Nav::ArcadeResult);
    CHECK(g.model().fragmentation() == 60);
    CHECK(g.bits() == bits + kArcadePlayBits + kArcadeScoreBits);   // past the line: full
    CHECK(g.arcadeWins(row) == 1);
    CHECK(g.arcadeBest(row) == kFraggerWinHits + 5);   // the best is unclamped
}

// The fall runs on its own real-ms clock, and that clock reads the run's score: the same
// second of play carries a block further down once the run has been shooting. A second
// at either pace is shorter than a block's whole fall, so nothing lands mid-measurement.
void test_fragger_cadence_follows_the_score() {
    auto rowsInOneSecond = [](int hits) {
        Game g{StartMode::Hatched};
        g.model().setFragmentation(50);
        startDefragRun(g);
        shootFragments(g, hits);
        const int lane = g.fragger().lane();
        g.fragger().place(0, lane, Fragger::Cell::File);
        for (uint32_t ms = 1; ms <= 1000; ++ms) g.tick(ms);
        // The tracked file is the LOWEST one in its lane; a spawn only ever enters above.
        for (int r = kFraggerRows - 1; r >= 0; --r)
            if (g.fragger().cell(r, lane) == Fragger::Cell::File) return r;
        return -1;
    };
    const int slow = rowsInOneSecond(0);
    const int fast = rowsInOneSecond(kFraggerWinHits);
    CHECK(slow >= 1);
    CHECK(fast > slow);
}

// The dial is FRAGGER's lane count — EASY's minimum, and one more per step — in a cabinet
// and a MAINT run alike, since both start it through the same door.
void test_fragger_lanes_follow_the_dial() {
    for (int d = 0; d < kArcadeDifficulties; ++d) {
        Game g{StartMode::Hatched};
        g.model().setFragmentation(50);
        g.debugSetMaintDifficulty(static_cast<ArcadeDifficulty>(d));
        startDefragRun(g);
        CHECK(g.fragger().lanes() == kFraggerMinLanes + d);
        CHECK(g.fragger().lane() == g.fragger().lanes() / 2);
    }
    Game c{StartMode::Hatched};
    enterArcadeCabinet(c, arcadeGameIndexById("fragger"), ArcadeDifficulty::Hard);
    c.onButton(press(Button::B));
    CHECK(c.fragger().lanes() == kFraggerMaxLanes);
}

// A played run's payout DOUBLES per step up the dial: the same five hits and three
// catches pay x1, x2, x4 — in frag and in Bits alike.
void test_played_rewards_double_per_step() {
    int paid[kArcadeDifficulties] = {0};
    for (int d = 0; d < kArcadeDifficulties; ++d) {
        Game g{StartMode::Hatched};
        g.model().setFragmentation(100);
        g.debugSetMaintDifficulty(static_cast<ArcadeDifficulty>(d));
        const int bits = g.bits();
        startDefragRun(g);
        shootFragments(g, 5);
        catchFiles(g, 3);
        crashRun(g);
        g.onButton(press(Button::B));
        paid[d] = g.bits() - bits;
        CHECK(g.model().fragmentation() == 100 - 5 * kPlayFragPerUnit * (1 << d));
    }
    CHECK(paid[0] == 3 * kPlayBitsPerUnit);
    CHECK(paid[1] == 2 * paid[0]);
    CHECK(paid[2] == 2 * paid[1]);
}

// Only a file that lands in the head's OWN lane is caught; one landing anywhere else is
// still written to the disk, and either way a file never ends the run.
void test_fragger_catch_needs_the_head_under_it() {
    Fragger f = emptyRun();
    const int under = f.lane();
    f.place(kFraggerRows - 1, under, Fragger::Cell::File);
    f.place(kFraggerRows - 1, under == 0 ? 1 : 0, Fragger::Cell::File);
    f.step();
    CHECK(f.running());
    CHECK(f.written() == 2);
    CHECK(f.caught() == 1);
}
