// fragger.h — the rules of FRAGGER, with no rendering and no Game.
//
// Blocks fall down three to five lanes toward the scan head at the foot of the disk. A
// FRAGMENT (drawn crossed out) has to be shot before it lands — one that reaches the disk
// ends the run. A FILE is a good block, and the shot cannot get past it: it lands
// harmlessly, and whatever was queued behind it in that lane can only be shot once it
// has. A file that lands in the head's own lane is CAUGHT, which is the run's second
// score: so every moment asks whether to stand under a file or go and shoot a fragment,
// and a fragment shielded by a file has to be met where it will be, not where it is.
//
// ENDLESS. There is no board to clear: every fragment shot and every file caught is worth
// the same, the fall quickens a little with each shot, and the run goes until a fragment
// lands. So the score IS the game, and what it buys is the caller's business (Game::settlePlayedRun:
// a MAINT process or the arcade till).
//
// Kept a plain model (core/model, like stacker/isolation) so the rules can be tested a
// step at a time without a framebuffer or a Game. The screen reads it; it reads nothing.
// Callers pass a seed (Game hands it one off the shared LCG), so a test can pin a run
// exactly while a real one stays fresh.
#pragma once

#include <cstdint>

namespace mal {

// The lane count IS the difficulty dial: every lane added is one more the head has to
// cover, with the same number of blocks falling into them. kFraggerMaxLanes sizes the
// board's storage; a run plays on any count from the minimum up to it.
constexpr int kFraggerMinLanes = 3;
constexpr int kFraggerMaxLanes = 5;
constexpr int kFraggerRows = 10;   // row 0 enters at the top; the last row is over the head

// One row in two of fall drops a block at all, and of those roughly one in three is a
// file — the same density on any lane count, so a wider disk is harder only because the
// head has further to go.
constexpr int kFraggerSpawnPct = 50;
constexpr int kFraggerFilePct = 35;

// The fall's pace in ms per row, quickening by kFraggerRampMs for every fragment shot.
// At the start a fragment takes five seconds to cross the disk; kFraggerWinHits in, it
// reaches the floor, under two — still well above the 110ms the panel is known to
// sustain (kStackerStepMs), so no row of the fall is ever skipped on screen.
constexpr int kFraggerStartStepMs = 500;
constexpr int kFraggerMinStepMs = 180;
constexpr int kFraggerRampMs = 8;
// The run counts as WON once it has shot this many: the fall has reached its floor by
// then, so the game stops getting harder and starts being a test of how long you hold it.
constexpr int kFraggerWinHits = (kFraggerStartStepMs - kFraggerMinStepMs) / kFraggerRampMs;

class Fragger {
  public:
    enum class Cell : uint8_t { Empty, Fragment, File };
    enum class State : uint8_t { Running, Crashed };
    // What a press of fire met: the lowest block in the head's lane.
    enum class Shot : uint8_t { Nothing, Hit, Blocked };

    Fragger() { reset(1, kFraggerMinLanes); }

    void reset(uint32_t seed, int lanes) {
        lanes_ = lanes < kFraggerMinLanes ? kFraggerMinLanes
               : lanes > kFraggerMaxLanes ? kFraggerMaxLanes : lanes;
        for (int r = 0; r < kFraggerRows; ++r)
            for (int l = 0; l < kFraggerMaxLanes; ++l) cells_[r][l] = Cell::Empty;
        rng_ = seed ? seed : 1;
        lane_ = lanes_ / 2;
        hits_ = 0;
        written_ = 0;
        caught_ = 0;
        crashLane_ = -1;
        state_ = State::Running;
    }

    // One row of fall. The bottom row leaves first — a file is written (and CAUGHT if
    // it lands in the head's lane), a fragment
    // crashes the run — then everything moves down and a block may enter at the top.
    void step() {
        if (state_ != State::Running) return;
        for (int l = 0; l < lanes_; ++l) {
            const Cell landing = cells_[kFraggerRows - 1][l];
            if (landing == Cell::File) {
                ++written_;
                if (l == lane_) ++caught_;
            }
            else if (landing == Cell::Fragment && crashLane_ < 0) crashLane_ = l;
        }
        if (crashLane_ >= 0) {
            state_ = State::Crashed;
            return;
        }
        for (int r = kFraggerRows - 1; r > 0; --r)
            for (int l = 0; l < lanes_; ++l) cells_[r][l] = cells_[r - 1][l];
        for (int l = 0; l < lanes_; ++l) cells_[0][l] = Cell::Empty;
        spawn();
    }

    // The head stops at the walls rather than wrapping: a wrap would make the far lane
    // the nearest one, and the disk would stop reading as a row.
    void moveLeft() {
        if (state_ == State::Running && lane_ > 0) --lane_;
    }
    void moveRight() {
        if (state_ == State::Running && lane_ < lanes_ - 1) ++lane_;
    }

    // Fire up the head's lane. The shot meets the LOWEST block there: a fragment is
    // destroyed, a file stops it with nothing lost, and an empty lane is a miss.
    // `row` reports where the shot stopped (-1 for a miss) so the screen can draw it.
    Shot fire(int* row = nullptr) {
        if (row) *row = -1;
        if (state_ != State::Running) return Shot::Nothing;
        for (int r = kFraggerRows - 1; r >= 0; --r) {
            Cell& c = cells_[r][lane_];
            if (c == Cell::Empty) continue;
            if (row) *row = r;
            if (c == Cell::File) return Shot::Blocked;
            c = Cell::Empty;
            ++hits_;
            return Shot::Hit;
        }
        return Shot::Nothing;
    }

    // Place a block directly, for a test that needs a lane in a known state.
    void place(int row, int lane, Cell c) {
        if (row >= 0 && row < kFraggerRows && lane >= 0 && lane < lanes_)
            cells_[row][lane] = c;
    }

    // The fall's current interval, before any arcade dial scales it.
    int stepMs() const {
        const int ms = kFraggerStartStepMs - hits_ * kFraggerRampMs;
        return ms < kFraggerMinStepMs ? kFraggerMinStepMs : ms;
    }

    State state() const { return state_; }
    bool running() const { return state_ == State::Running; }
    int lanes() const { return lanes_; }
    int lane() const { return lane_; }
    int hits() const { return hits_; }           // fragments shot: the score
    int written() const { return written_; }     // files that landed anywhere
    int caught() const { return caught_; }       // ...of them, under the head
    int crashLane() const { return crashLane_; } // where the run ended, -1 while running
    Cell cell(int row, int lane) const {
        if (row < 0 || row >= kFraggerRows || lane < 0 || lane >= lanes_)
            return Cell::Empty;
        return cells_[row][lane];
    }

  private:
    uint32_t next() {
        rng_ = rng_ * 1664525u + 1013904223u;
        return rng_ >> 8;
    }

    // At most one block enters per row of fall, and never directly on top of one still
    // in row 1, so every lane keeps at least one empty row between its blocks. That gap
    // is what keeps the file rule fair: a fragment queued behind a file is never closer
    // than two rows of fall from the head when the file lands.
    void spawn() {
        if (static_cast<int>(next() % 100) >= kFraggerSpawnPct) return;
        const int lane = static_cast<int>(next() % lanes_);
        if (cells_[1][lane] != Cell::Empty) return;
        cells_[0][lane] = static_cast<int>(next() % 100) < kFraggerFilePct ? Cell::File
                                                                           : Cell::Fragment;
    }

    Cell cells_[kFraggerRows][kFraggerMaxLanes];
    uint32_t rng_ = 1;
    int8_t lanes_ = kFraggerMinLanes;
    int8_t lane_ = 0;
    int16_t hits_ = 0;
    int16_t written_ = 0;
    int16_t caught_ = 0;
    int8_t crashLane_ = -1;
    State state_ = State::Running;
};

}  // namespace mal
