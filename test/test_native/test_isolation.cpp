// test_isolation.cpp — native gates for the Isolation Protocol, the Worm egg's hatch
// minigame, and the SECOND_INSTANCE gate that puts the line on the menu at all.
//
// One slice of the native suite; see test_gates.h for the shared fixtures and
// test_main.cpp for the roster that runs these. A save-vNN gate sits with the
// feature whose field it migrates, not in a migrations pile of its own.
#include "test_gates.h"

#include "core/model/isolation.h"

// --- The rules model (core/model/isolation.h) ------------------------------

// Steer the worm until it is heading `want`, one quarter-turn per step. Returns false
// if it crashed on the way, which would make whatever follows a no-op rather than a
// failure.
static bool isoFace(Isolation& iso, int want) {
    for (int guard = 0; guard < 4; ++guard) {
        if (iso.dir() == want) return true;
        iso.turnRight();
        iso.step();
        if (!iso.running()) return false;
    }
    return iso.dir() == want;
}

// The worm opens on the middle row heading right, three cells long, with one byte
// somewhere else in the buffer.
void test_isolation_opens_on_the_middle_row() {
    Isolation iso;
    iso.reset(12345u, 30);
    CHECK(iso.running());
    CHECK(iso.length() == kIsolationStartLength);
    CHECK(iso.dir() == 0);                          // heading right
    CHECK(iso.goal() == 30 && iso.dots() == 0);
    const int row = kIsolationRows / 2;
    CHECK(iso.head() == row * kIsolationCols + 2);  // head at column 2
    CHECK(iso.segment(iso.length() - 1) == row * kIsolationCols);   // tail at the wall
    CHECK(!iso.bodyAt(iso.dot()));                  // the byte is never under the worm
}

// A plain step moves one cell and keeps the length: the tail vacates as the head
// advances, which is what makes a tight coil playable.
void test_isolation_step_moves_without_growing() {
    Isolation iso;
    iso.reset(7u, 30);
    const int head0 = iso.head();
    const int tail0 = iso.segment(iso.length() - 1);
    iso.step();
    CHECK(iso.running());
    CHECK(iso.head() == head0 + 1);                 // one cell right
    CHECK(iso.length() == kIsolationStartLength);   // no growth without a byte
    CHECK(!iso.bodyAt(tail0));                      // the old tail cell is free again
}

// Two of the same turn inside one step is ONE quarter-turn, not a U-turn into its own
// neck — a mistimed double-press costs an input, never the run. A left and a right in
// the same interval cancel to straight.
void test_isolation_double_turn_is_one_quarter() {
    Isolation iso;
    iso.reset(7u, 30);
    iso.turnLeft();
    iso.turnLeft();                                 // would be a reversal if it stacked
    iso.step();
    CHECK(iso.running());                           // ...and would have crashed instantly
    CHECK(iso.dir() == 3);                          // exactly one quarter-turn: up

    Isolation flat;
    flat.reset(7u, 30);
    flat.turnLeft();
    flat.turnRight();
    flat.step();
    CHECK(flat.dir() == 0);                         // cancelled out: still heading right
}

// The buffer wall is lethal, and the run banks whatever it had already eaten.
void test_isolation_wall_ends_the_run() {
    Isolation iso;
    iso.reset(7u, 30);
    iso.turnLeft();                                 // face up, five rows below the wall
    for (int i = 0; i < kIsolationRows; ++i) iso.step();
    CHECK(!iso.running());
    CHECK(iso.state() == Isolation::State::Crashed);
    CHECK(!iso.clean());
}

// The successor of `cell` on a Hamiltonian cycle over the whole buffer: row 0 is the
// return corridor, and every column below it is walked down (even) or up (odd). Every
// cell is on it exactly once, so a worm that follows it eats every byte placed and can
// never meet itself — which is what makes a CLEAN run reachable in a test at all.
static int isoCycleNext(int cell) {
    const int col = cell % kIsolationCols, row = cell / kIsolationCols;
    if (row == 0) return col > 0 ? cell - 1 : kIsolationCols;      // left, then drop in
    if (col == kIsolationCols - 1 && row == 1) return col;         // up into the corridor
    const bool down = (col % 2) == 0;
    if (down) return row < kIsolationRows - 1 ? cell + kIsolationCols : cell + 1;
    return row > 1 ? cell - kIsolationCols : cell + 1;
}

// The turn (-1 left, +1 right, 0 straight) that puts a worm at `cell` heading `dir` onto
// the cycle's next cell.
static int isoCycleTurn(int cell, int dir) {
    const int next = isoCycleNext(cell);
    const int d = next - cell;
    int want = 0;
    if (d == 1) want = 0;
    else if (d == kIsolationCols) want = 1;
    else if (d == -1) want = 2;
    else want = 3;
    const int delta = (want - dir + 4) % 4;
    return delta == 3 ? -1 : delta;                 // 3 quarter-turns right == one left
}

// Walk the cycle until the worm has eaten `n` bytes, so a gate that needs a LONG worm
// can have one without caring where the buffer put them.
static void isoEat(Isolation& iso, int n) {
    for (int guard = 0; guard < 20000 && iso.running() && iso.dots() < n; ++guard) {
        const int turn = isoCycleTurn(iso.head(), iso.dir());
        if (turn < 0) iso.turnLeft();
        else if (turn > 0) iso.turnRight();
        iso.step();
    }
}

// Its own body is lethal too — which only means anything once the worm is long enough
// to reach itself. Three cells cannot: the tightest loop a worm can turn is four steps
// wide, and the cell it started on has vacated by the time the head comes back round.
// Fed up to seven, the same loop closes on its own neck.
void test_isolation_self_collision_ends_the_run() {
    Isolation iso;
    iso.reset(7u, 30);
    for (int i = 0; i < 4 && iso.running(); ++i) {   // length 3: the loop is survivable
        iso.turnRight();
        iso.step();
    }
    CHECK(iso.running());
    CHECK(iso.length() == kIsolationStartLength);

    Isolation fed;
    fed.reset(7u, 30);
    isoEat(fed, 2);
    CHECK(fed.running() && fed.length() >= 5);
    for (int i = 0; i < 4 && fed.running(); ++i) {
        fed.turnRight();
        fed.step();
    }
    CHECK(!fed.running());
    CHECK(fed.state() == Isolation::State::Crashed);
    // A finished run holds still: stepping it again changes nothing.
    const int len = fed.length();
    fed.step();
    CHECK(fed.length() == len && fed.state() == Isolation::State::Crashed);
}

// A worm that follows the cycle eats every byte the buffer places and finishes CLEAN,
// which is the outcome the whole minigame is priced around.
void test_isolation_cycle_run_finishes_clean() {
    Isolation iso;
    iso.reset(99u, 30);
    isoEat(iso, 30);
    CHECK(iso.state() == Isolation::State::Clean);
    CHECK(iso.dots() == 30);
    // Every byte is worth kIsolationGrowth cells on top of the three it started with —
    // except the last, which ends the run on the step that eats it, so the tail it was
    // still owed never gets walked out.
    CHECK(iso.length() ==
          kIsolationStartLength + 30 * kIsolationGrowth - (kIsolationGrowth - 1));
    // A third of the buffer, which is the difficulty the economy in tunables.h claims:
    // a clean protocol is an arcade run, not a formality.
    CHECK(iso.length() > kIsolationCells / 3);
}

// --- The line gate (SECOND_INSTANCE) ---------------------------------------

// Enter ARCH and freeze the active pet into the rack, leaving a fresh egg behind.
static void archStore(Game& g) {
    enterArchNewEgg(g);                // ARCH's NEW EGG row: store the active, then hatch
}

// The Worm line is not on the menu until the rack has held two of one species at once —
// the line about replication asks you to replicate something first.
void test_worm_line_gated_on_second_instance() {
    Game g;
    pickFirstEggLine(g);
    CHECK(!g.hasAchievement(ach::kSecondInstance));
    for (const EggLineDef* line : g.availableEggLines())
        CHECK(std::strcmp(line->id, "worm") != 0);

    archStore(g);                      // one CryptoShell on the shelf
    pickFirstEggLine(g);               // still no Worm row to cycle to
    CHECK(g.rackCount() == 1);
    CHECK(!g.hasAchievement(ach::kSecondInstance));

    archStore(g);                      // a SECOND CryptoShell: two of one species
    CHECK(g.rackCount() == 2);
    CHECK(g.hasAchievement(ach::kSecondInstance));
    // Earned before the line-select it triggered, so the very freeze that unlocks the
    // line is the one whose menu already offers it.
    CHECK(g.inLineSelect());
    bool worm = false;
    for (const EggLineDef* line : g.availableEggLines())
        if (std::strcmp(line->id, "worm") == 0) worm = true;
    CHECK(worm);
}

// --- The screen (game_isolation.cpp) ---------------------------------------

// Lay a Worm egg, which drops straight into the Isolation Protocol.
static Game wormEgg() {
    Game g;
    g.unlockAchievement(ach::kSecondInstance);   // unlocks the Worm line
    g.resetToHatch();
    g.onButton(press(Button::A));                // Ransomware -> Worm
    g.onButton(press(Button::B));                // lay it
    return g;
}

// The egg is laid and the run opens at once, priced at the WHOLE incubation clock: eat
// that many bytes and the protocol is clean.
void test_isolation_opens_on_a_laid_worm_egg() {
    Game g = wormEgg();
    CHECK(g.pet() && std::strcmp(g.pet()->id, "vermicell") == 0);
    CHECK(g.inEggPhase());
    CHECK(g.nav() == Game::Nav::Isolation);
    CHECK(g.bootHatchRemainMs() == kBootHatchMs);
    CHECK(g.isolation().running());
    CHECK(g.isolation().goal() ==
          static_cast<int>(kBootHatchMs / kIsolationDotMs));
}

// The worm walks its own real-ms cadence, not the shared heartbeat — and the incubation
// clock is frozen underneath it, so the goal it quoted stays the goal it is judged by.
void test_isolation_steps_on_its_own_cadence() {
    Game g = wormEgg();
    uint32_t t = 0;
    const int head0 = g.isolation().head();
    g.tick(t += kIsolationStepMs - 10);
    CHECK(g.isolation().head() == head0);        // not yet
    g.tick(t += 20);
    CHECK(g.isolation().head() != head0);        // stepped
    CHECK(g.bootHatchRemainMs() == kBootHatchMs);   // clock frozen inside the run
}

// A crashed run banks a minute per byte and drops the player back at the egg. A and B
// steer rather than step/accept, so C is inert throughout.
void test_isolation_crash_banks_a_minute_per_byte() {
    Game g = wormEgg();
    uint32_t t = 0;
    g.onButton(press(Button::A));                // turn up, toward the near wall
    for (int i = 0; i < 4 * kIsolationRows && g.isolation().running(); ++i)
        g.tick(t += kIsolationStepMs);
    CHECK(!g.isolation().running());
    const int dots = g.isolation().dots();
    CHECK(g.bootHatchRemainMs() == kBootHatchMs);  // nothing spent until it is banked
    tapC(g);                  // C is disabled on the verdict
    CHECK(g.nav() == Game::Nav::Isolation);
    g.onButton(press(Button::B));
    CHECK(g.nav() == Game::Nav::Idle);
    CHECK(g.inEggPhase());                         // a bad run costs the bonus, not the pet
    CHECK(g.bootHatchRemainMs() ==
          kBootHatchMs - static_cast<uint32_t>(dots) * kIsolationDotMs);
    CHECK(!g.hasAchievement(ach::kWormWhisperer));
}

// A CLEAN protocol eats the whole clock: the egg hatches out of the minigame rather
// than waiting one out, and WORM_WHISPERER — which asks for exactly this run — fires.
void test_isolation_clean_run_hatches_and_unlocks() {
    Game g = wormEgg();
    CHECK(!g.hasAchievement(ach::kWormWhisperer));
    uint32_t t = 0;
    for (int guard = 0; guard < 20000 && g.isolation().running(); ++guard) {
        const int turn = isoCycleTurn(g.isolation().head(), g.isolation().dir());
        if (turn < 0) g.onButton(press(Button::A));
        else if (turn > 0) tapC(g);
        g.tick(t += kIsolationStepMs);
    }
    CHECK(g.isolation().clean());
    CHECK(g.nav() == Game::Nav::Isolation);        // parked on the verdict, waiting on B
    g.onButton(press(Button::B));
    CHECK(g.hasAchievement(ach::kWormWhisperer));
    CHECK(g.bootHatchRemainMs() == 0);
    CHECK(!g.inEggPhase());
    CHECK(g.nav() == Game::Nav::Idle);
    // Hatched straight to a Process pet on its OWN line, not left as a Boot-Sector
    // shell with a spent clock.
    CHECK(g.pet() && g.pet()->stage == Stage::Process);
    CHECK(g.pet()->line && std::strcmp(g.pet()->line, "worm") == 0);
}

// Off a CABINET the buffer is endless: no byte count finishes the run, so it goes until
// the worm crashes (or fills the buffer, which is the only clean an endless run has).
// kArcadeIsolationWinBytes is the till's win line, not a wall the board stops at.
void test_isolation_endless_run_has_no_finish() {
    Isolation iso;
    iso.reset(7u, /*goal=*/0);
    CHECK(iso.endless() && iso.goal() == 0);
    // Walk the buffer's Hamiltonian cycle (the same route the steps: dump uses) far
    // enough to pass the old finish line, and check nothing stops it there.
    for (int i = 0; i < 4000 && iso.running(); ++i) {
        const int cell = iso.head();
        const int col = cell % kIsolationCols, row = cell / kIsolationCols;
        int next;
        if (row == 0) next = col > 0 ? cell - 1 : kIsolationCols;
        else if (col == kIsolationCols - 1 && row == 1) next = col;
        else if (col % 2 == 0)
            next = row < kIsolationRows - 1 ? cell + kIsolationCols : cell + 1;
        else next = row > 1 ? cell - kIsolationCols : cell + 1;
        const int d = next - cell;
        const int want = d == 1 ? 0 : d == kIsolationCols ? 1 : d == -1 ? 2 : 3;
        const int turn = (want - iso.dir() + 4) % 4;
        if (turn == 3) iso.turnLeft();
        else if (turn == 1) iso.turnRight();
        iso.step();
        if (iso.dots() > kArcadeIsolationWinBytes) break;
    }
    CHECK(iso.dots() > kArcadeIsolationWinBytes);   // straight past the old ending
    CHECK(iso.running() && !iso.clean());
}
