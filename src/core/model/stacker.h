// stacker.h — the rules of the STACKER, with no rendering and no Game.
//
// The FIREWALL. An intruder drops down the well over one column while you raise a wall
// under it: a run of blocks slides across the row, you lock it, and anything not resting
// on the course below is shaved off. When the intruder meets the top of the wall, the
// wall holds if that course covers its column (BLOCKED) and is breached if it doesn't.
// Run out of width before then and the wall collapses. What a held wall buys is the
// caller's business (Game::settlePlayedRun: a MAINT process or the arcade till).
//
// So the base lock aims (shaving only narrows, so the base decides every column the wall
// can ever cover), and every lock after it trades width for height against the drop.
// The intruder's column is seeded per board so the aim has to be read each run rather
// than recited.
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

// The intruder starts in the open row above the top course and drops one row every
// this many beats of the slide, so the difficulty dial (which paces the beat) paces it
// too. Set so a careful hand meets it around two-thirds of the way up, and a hasty one
// can meet it higher but narrower.
constexpr int kStackerThreatBeats = 12;

// The best a board can be worth (see score()): every block kept, all the way up.
constexpr int kStackerMaxScore =
    kStackerStartWidth * kStackerRows * (kStackerRows + 1) / 2;

class Stacker {
  public:
    // Won = the intruder was BLOCKED. Lost = the wall collapsed (a lock kept nothing);
    // Breached = the intruder met a wall that didn't cover its column.
    enum class State : uint8_t { Running, Won, Lost, Breached };

    explicit Stacker(uint32_t seed = 0) { reset(seed); }

    // Start a fresh run: an empty board, a full-width run in hand on the base row, and
    // the intruder over the seed's column in the open row above the wall.
    void reset(uint32_t seed = 0) {
        for (int r = 0; r < kStackerRows; ++r) locked_[r] = 0;
        row_ = 0;
        width_ = kStackerStartWidth;
        left_ = 0;
        dir_ = +1;
        threatCol_ = static_cast<int8_t>(seed % kStackerCols);
        threatRow_ = kStackerRows;
        threatBeat_ = 0;
        state_ = State::Running;
    }

    // One beat: the run slides, and every kStackerThreatBeats-th beat the intruder drops
    // a row. The run bounces off both walls; a run as wide as the board can't move at
    // all, which is a legal (if generous) state rather than a stuck one.
    void step() {
        if (state_ != State::Running) return;
        const int span = kStackerCols - width_;
        const int stride = row_ >= kStackerFastFromRow ? kStackerFastStep : kStackerSlowStep;
        for (int i = 0; i < stride && span > 0; ++i) {
            if (left_ + dir_ < 0 || left_ + dir_ > span) dir_ = -dir_;
            left_ += dir_;
        }
        if (++threatBeat_ >= kStackerThreatBeats) {
            threatBeat_ = 0;
            --threatRow_;
            if (threatRow_ <= row_) contact();
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

        // The next run is the surviving blocks, re-entering from the left wall. The
        // survivors are contiguous (they were a run, masked by a run), so a width and a
        // left edge still describe them. A wall that rises into the intruder's row meets
        // it there, which is also how a wall built all the way up ends.
        width_ = kept;
        row_ += 1;
        left_ = 0;
        dir_ = +1;
        if (row_ >= threatRow_) contact();
    }

    State state() const { return state_; }
    bool running() const { return state_ == State::Running; }
    bool won() const { return state_ == State::Won; }
    bool breached() const { return state_ == State::Breached; }

    // The moving run's row, 0 = base. Once the intruder has been met it is the row above
    // the top course, which is kStackerRows for a wall built all the way up.
    int row() const { return row_; }
    int left() const { return left_; }      // its leftmost column
    int width() const { return width_; }    // how many blocks are in hand

    // Where the intruder is: its column, and its row (kStackerRows = the open row above
    // the top course). Once met, it rests on the top of the wall.
    int threatCol() const { return threatCol_; }
    int threatRow() const { return threatRow_; }

    // The top course's width once the intruder has been met — what the wall held with.
    // 1 is the narrowest a held wall can be and kStackerStartWidth means nothing was ever
    // shaved. Game::finishStacker's shape achievements read it.
    int topWidth() const { return row_ > 0 ? rowWidth(row_ - 1) : 0; }

    // How many blocks a locked row kept.
    int rowWidth(int r) const {
        if (r < 0 || r >= kStackerRows) return 0;
        return popcount(locked_[r]);
    }

    // What the board is WORTH so far: every block that reached a level is worth that
    // level, level 1 being the base row. So height is paid for twice over — once by
    // reaching a row at all, and again by how many blocks survived the climb to it —
    // which is what makes a run that stalled two rows short worth more than one that
    // stalled halfway, without the model knowing what the number buys.
    //
    // Only LOCKED rows count. The run in hand hasn't landed anywhere yet, so it is worth
    // nothing until it does, and a board is scored the same whether the player stopped or
    // ran out of blocks.
    int score() const {
        int total = 0;
        for (int r = 0; r < kStackerRows; ++r) total += popcount(locked_[r]) * (r + 1);
        return total;
    }

    // Every block locked so far, at any height. The unit a MAINT run is paid in: one per
    // block that landed, so a board that stalled still earns what it stacked.
    int blocks() const {
        int total = 0;
        for (int r = 0; r < kStackerRows; ++r) total += popcount(locked_[r]);
        return total;
    }

    // Is (row, col) a LOCKED block? Rows at or above the moving run are always empty —
    // the run in hand is not part of the board until it drops.
    bool locked(int r, int c) const {
        if (r < 0 || r >= kStackerRows || c < 0 || c >= kStackerCols) return false;
        return (locked_[r] & colBit(c)) != 0;
    }

  private:
    // The intruder has reached the top of the wall (or the floor, if there is none).
    void contact() {
        threatRow_ = row_;
        const bool covered = row_ > 0 && (locked_[row_ - 1] & colBit(threatCol_)) != 0;
        state_ = covered ? State::Won : State::Breached;
    }

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
    int8_t threatCol_ = 0;
    int8_t threatRow_ = kStackerRows;
    int8_t threatBeat_ = 0;
    State state_ = State::Running;
};

}  // namespace mal
