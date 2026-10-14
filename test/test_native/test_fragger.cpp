// test_fragger.cpp — native gates for FRAGGER, the played Defrag and its cabinet.
//
// One slice of the native suite; see test_gates.h for the shared fixtures and
// test_main.cpp for the roster that runs these.
#include "test_gates.h"

namespace {

// A run with nothing on the disk, the head in the middle lane. The board starts empty,
// so a test stages exactly the blocks it needs with Fragger::place.
Fragger emptyRun() {
    Fragger f;
    f.reset(7);
    return f;
}

// Shoot `n` fragments through the REAL button path: stage one over the head, fire.
void shootFragments(Game& g, int n) {
    for (int i = 0; i < n; ++i) {
        g.fragger().place(kFraggerRows - 1, g.fragger().lane(), Fragger::Cell::Fragment);
        g.onButton(press(Button::B));
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

    f.place(kFraggerRows - 1, 5, Fragger::Cell::Fragment);
    f.step();
    CHECK(!f.running());
    CHECK(f.crashLane() == 5);
    // A crashed run is frozen: nothing moves, and nothing more can be scored.
    f.place(kFraggerRows - 1, f.lane(), Fragger::Cell::Fragment);
    CHECK(f.fire() == Fragger::Shot::Nothing);
    CHECK(f.hits() == 0);
}

// The head stops at the walls rather than wrapping.
void test_fragger_head_stops_at_the_walls() {
    Fragger f = emptyRun();
    for (int i = 0; i < 2 * kFraggerLanes; ++i) f.moveLeft();
    CHECK(f.lane() == 0);
    for (int i = 0; i < 2 * kFraggerLanes; ++i) f.moveRight();
    CHECK(f.lane() == kFraggerLanes - 1);
}

// The fall quickens with every fragment shot, and stops quickening at its floor — which
// is where the arcade's win line sits, so a run that passes it has seen the whole curve.
void test_fragger_pace_ramps_to_a_floor() {
    Fragger f = emptyRun();
    CHECK(f.stepMs() == kFraggerStartStepMs);
    int last = f.stepMs();
    for (int i = 0; i < kArcadeFraggerWinHits; ++i) {
        f.place(0, f.lane(), Fragger::Cell::Fragment);
        CHECK(f.fire() == Fragger::Shot::Hit);
        CHECK(f.stepMs() <= last);
        last = f.stepMs();
    }
    CHECK(f.stepMs() == kFraggerMinStepMs);
    f.place(0, f.lane(), Fragger::Cell::Fragment);
    f.fire();
    CHECK(f.stepMs() == kFraggerMinStepMs);
    // ...and the floor stays clear of the panel's fastest repaint even on HARD.
    CHECK(kFraggerMinStepMs * kArcadeSpeedPctHard / 100 >= kStackerStepMs);
}

// The spawner never stacks two blocks in adjacent rows of a lane. That gap is what keeps
// the file rule fair — a fragment behind a file always has two rows of fall left when the
// file lands — so it is checked over a long run rather than trusted. Also pins that a
// seed replays exactly.
void test_fragger_spawns_keep_a_gap_and_replay() {
    Fragger a, b;
    a.reset(12345);
    b.reset(12345);
    int spawned = 0;
    for (int s = 0; s < 2000; ++s) {
        // Clear the bottom row first so nothing ever crashes: the spawn is under test,
        // not the player.
        for (int l = 0; l < kFraggerLanes; ++l) {
            a.place(kFraggerRows - 1, l, Fragger::Cell::Empty);
            b.place(kFraggerRows - 1, l, Fragger::Cell::Empty);
        }
        a.step();
        b.step();
        for (int l = 0; l < kFraggerLanes; ++l) {
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

// The MAINT payout: every fragment shot is kFraggerFragPerHit off the disk while it has
// any and kFraggerBitsPerHit into the wallet regardless, so a run that outlasts a clean
// disk is still worth playing. A landed fragment ends it with no penalty and no mistake.
void test_fragger_defrag_pays_per_fragment() {
    Game g{StartMode::Hatched};
    g.model().setFragmentation(5);
    const int bits = g.bits();
    const int mistakes = g.model().careMistakes();
    const int defrags = g.defragCount();
    g.debugStartFraggerDefrag();
    CHECK(g.nav() == Game::Nav::Fragger);
    shootFragments(g, 8);                               // three past a clean disk
    CHECK(g.fragger().hits() == 8);
    crashRun(g);
    CHECK(!g.fragger().running());
    g.onButton(press(Button::A));                       // any press settles the run

    CHECK(g.nav() == Game::Nav::Process);
    CHECK(g.model().fragmentation() == 0);
    CHECK(g.bits() == bits + 8 * kFraggerBitsPerHit);
    CHECK(g.model().careMistakes() == mistakes);
    CHECK(!g.model().hasGhost());
    CHECK(g.defragCount() == defrags + 1);

    // A run that shot nothing changes nothing, and does not count as a defrag.
    Game z{StartMode::Hatched};
    z.model().setFragmentation(kFragCriticalMin + 5);
    z.debugStartFraggerDefrag();
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
    shootFragments(g, kArcadeFraggerWinHits + 5);
    crashRun(g);
    g.onButton(press(Button::B));

    CHECK(g.nav() == Game::Nav::ArcadeResult);
    CHECK(g.model().fragmentation() == 60);
    CHECK(g.bits() == bits + kArcadePlayBits + kArcadeScoreBits);   // past the line: full
    CHECK(g.arcadeWins(row) == 1);
    CHECK(g.arcadeBest(row) == kArcadeFraggerWinHits + 5);   // the best is unclamped
}

// The fall runs on its own real-ms clock, and that clock reads the run's score: the same
// second of play carries a block further down once the run has been shooting. A second
// at either pace is shorter than a block's whole fall, so nothing lands mid-measurement.
void test_fragger_cadence_follows_the_score() {
    auto rowsInOneSecond = [](int hits) {
        Game g{StartMode::Hatched};
        g.model().setFragmentation(50);
        g.debugStartFraggerDefrag();
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
    const int fast = rowsInOneSecond(kArcadeFraggerWinHits);
    CHECK(slow >= 1);
    CHECK(fast > slow);
}
