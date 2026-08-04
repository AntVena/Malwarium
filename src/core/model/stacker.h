// stacker.h — the rules of the DEFRAG minigame, with no rendering and no Game.
//
// A run of blocks slides across the row, you lock it, and anything not resting on the
// run below is shaved off. Reach the top with at least one block and the disk is
// defragmented; run out of width and it isn't. That is the whole game — the arcade
// Stacker cabinet, which is also what defragmenting a disk looks like when you draw it:
// scattered blocks pulled into one contiguous run, narrowing as the fragments that
// wouldn't align are discarded.
//
// DETERMINISTIC BY DESIGN. There is no RNG anywhere in here, which is the point of the
// variant it backs: the Bits-only defrag pays with luck, the Tool defrag pays with an
// item, and this one pays with skill. A run that ends badly ended badly because of when
// the player pressed, and can be replayed identically.
//
// Kept a plain model (core/model, like pet_model/combat) so the rules can be tested a
// press at a time without a framebuffer or a Game. The screen reads it; it reads
// nothing.
#pragma once

#include <cstdint>

namespace mal {

// The grid. Columns must stay <= 8: a locked row is one byte, which keeps a whole board
// at kStackerRows bytes and lets the whole thing live inside Game without a heap touch.
constexpr int kStackerCols = 7;
constexpr int kStackerRows = 9;
constexpr int kStackerStartWidth = 3;   // blocks in hand on the base row

// The moving run advances this many columns per beat, by row. The narrowing IS the
// classic difficulty curve, but the low rows are wide enough to land half-asleep at the
// shared 4fps heartbeat, so the upper rows step faster instead of merely narrower.
constexpr int kStackerFastFromRow = 5;
constexpr int kStackerSlowStep = 1;
constexpr int kStackerFastStep = 2;

class Stacker {
  public:
    enum class State : uint8_t { Running, Won, Lost };

    Stacker() { reset(); }

    // Start a fresh run: an empty board, a full-width run in hand on the base row.
    void reset() {
        for (int r = 0; r < kStackerRows; ++r) locked_[r] = 0;
        row_ = 0;
        width_ = kStackerStartWidth;
        left_ = 0;
        dir_ = +1;
        state_ = State::Running;
    }

    // One beat of slide. The run bounces off both walls; a run as wide as the board
    // can't move at all, which is a legal (if generous) state rather than a stuck one.
    void step() {
        if (state_ != State::Running) return;
        const int span = kStackerCols - width_;
        if (span <= 0) return;
        const int stride = row_ >= kStackerFastFromRow ? kStackerFastStep : kStackerSlowStep;
        for (int i = 0; i < stride; ++i) {
            if (left_ + dir_ < 0 || left_ + dir_ > span) dir_ = -dir_;
            left_ += dir_;
        }
    }

    // Lock the run where it stands. Every block NOT resting on a locked block below is
    // shaved off, which is what narrows the run; the base row rests on the floor and so
    // keeps everything. Losing every block ends the run, and locking the top row wins it.
    void drop() {
        if (state_ != State::Running) return;
        uint8_t placed = 0;
        for (int c = left_; c < left_ + width_; ++c) placed |= colBit(c);
        if (row_ > 0) placed &= locked_[row_ - 1];     // shave the overhang
        locked_[row_] = placed;

        const int kept = popcount(placed);
        if (kept == 0) { state_ = State::Lost; return; }
        if (row_ + 1 >= kStackerRows) { state_ = State::Won; return; }

        // The next run is the surviving blocks, re-entering from the left wall. The
        // survivors are contiguous (they were a run, masked by a run), so a width and a
        // left edge still describe them.
        width_ = kept;
        row_ += 1;
        left_ = 0;
        dir_ = +1;
    }

    State state() const { return state_; }
    bool running() const { return state_ == State::Running; }
    bool won() const { return state_ == State::Won; }

    int row() const { return row_; }        // the moving run's row, 0 = base
    int left() const { return left_; }      // its leftmost column
    int width() const { return width_; }    // how many blocks are in hand

    // Is (row, col) a LOCKED block? Rows at or above the moving run are always empty —
    // the run in hand is not part of the board until it drops.
    bool locked(int r, int c) const {
        if (r < 0 || r >= kStackerRows || c < 0 || c >= kStackerCols) return false;
        return (locked_[r] & colBit(c)) != 0;
    }

  private:
    // NOT named `bit`: Arduino.h defines a `bit(b)` macro, so that name compiles on the
    // host and is rewritten out from under this class on the device — a break only the
    // S3 gate would ever see.
    static constexpr uint8_t colBit(int c) { return static_cast<uint8_t>(1u << c); }
    static int popcount(uint8_t v) {
        int n = 0;
        while (v) { n += v & 1; v >>= 1; }
        return n;
    }

    uint8_t locked_[kStackerRows] = {0};
    int8_t row_ = 0;
    int8_t left_ = 0;
    int8_t width_ = 0;
    int8_t dir_ = 1;
    State state_ = State::Running;
};

}  // namespace mal
