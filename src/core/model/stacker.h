// stacker.h — the rules of the STACKER, with no rendering and no Game.
//
// The FIREWALL. Intruders drop down the well over their columns while you raise walls
// under them: a run of blocks slides across the well and B locks it, every block falling
// straight down its column onto whatever is there — a block, or the floor. The run lands
// on the tallest column beneath it; blocks over lower columns fall past and leave the
// hand, which is what narrows it. A run over a flat stretch (open floor included) keeps
// every block, so a second wall can be started anywhere without overlapping the first.
//
// An intruder is HELD once the wall under it reaches its row. At each drop of the
// intruders, any one held stops them all, and that is contact: every intruder held is
// BLOCKED, any other is a BREACH. So the walls race the drop and have to be level under
// every intruder when it comes — and an intruder that reaches the floor unheld is a
// breach on its own. What a held line buys is the caller's business
// (Game::settlePlayedRun: a MAINT process or the arcade till).
//
// The intruders' columns are seeded per board, so the aim is read each run rather than
// recited.
#pragma once

#include <cstdint>

namespace mal {

// The grid. Columns must stay <= 8: a locked row is one byte, which keeps a whole board
// at kStackerRows bytes and lets the whole thing live inside Game without a heap touch.
constexpr int kStackerCols = 7;
constexpr int kStackerRows = 9;
constexpr int kStackerStartWidth = 3;   // blocks in hand at the start of a run

// The moving run advances this many columns per beat, by the row it slides in. The low
// rows are wide enough to land half-asleep at the shared 4fps heartbeat, so the upper
// rows step faster instead of merely narrower.
constexpr int kStackerFastFromRow = 5;
constexpr int kStackerSlowStep = 1;
constexpr int kStackerFastStep = 2;

// The intruders start in the open row above the well and drop one row every this many
// beats of the slide, so the difficulty dial (which paces the beat) paces them too.
constexpr int kStackerThreatBeats = 12;

// How many intruders a board deals, and how far apart their columns always are: at least
// a full hand apart, so no single lock covers both and each needs a wall of its own.
constexpr int kStackerThreats = 2;
constexpr int kStackerThreatSpread = kStackerStartWidth;

// The best a board can be worth (see score()): what a held line scores, and the cap on
// a breached one — a full hand's wall under each intruder, all the way up.
constexpr int kStackerMaxScore =
    kStackerThreats * kStackerStartWidth * kStackerRows * (kStackerRows + 1) / 2;

class Stacker {
  public:
    // Won = every intruder BLOCKED; Breached = at least one got through.
    enum class State : uint8_t { Running, Won, Breached };

    explicit Stacker(uint32_t seed = 0) { reset(seed); }

    // Start a fresh run: an empty board, a full-width run in hand, and the intruders over
    // the seed's pair of columns in the open row above the well.
    void reset(uint32_t seed = 0) {
        for (int r = 0; r < kStackerRows; ++r) locked_[r] = 0;
        width_ = kStackerStartWidth;
        left_ = 0;
        dir_ = +1;
        // Every pair at least kStackerThreatSpread apart, enumerated so the seed indexes
        // them evenly.
        static_assert(kStackerThreats == 2, "the deal picks a pair");
        int pairs = 0;
        for (int a = 0; a < kStackerCols; ++a)
            for (int b = a + kStackerThreatSpread; b < kStackerCols; ++b) ++pairs;
        int pick = static_cast<int>(seed % static_cast<uint32_t>(pairs));
        for (int a = 0; a < kStackerCols; ++a)
            for (int b = a + kStackerThreatSpread; b < kStackerCols; ++b)
                if (pick-- == 0) { threatCol_[0] = a; threatCol_[1] = b; }
        threatRow_ = kStackerRows;
        threatBeat_ = 0;
        state_ = State::Running;
    }

    // One beat: the run slides, and every kStackerThreatBeats-th beat the intruders try
    // to drop a row — which is when contact is decided. The run bounces off both walls;
    // a run as wide as the board can't move at all, a legal (if generous) state.
    void step() {
        if (state_ != State::Running) return;
        const int span = kStackerCols - width_;
        const int stride = row() >= kStackerFastFromRow ? kStackerFastStep : kStackerSlowStep;
        for (int i = 0; i < stride && span > 0; ++i) {
            if (left_ + dir_ < 0 || left_ + dir_ > span) dir_ = -dir_;
            left_ += dir_;
        }
        if (++threatBeat_ < kStackerThreatBeats) return;
        threatBeat_ = 0;
        for (int i = 0; i < kStackerThreats; ++i)
            if (held(i)) { contact(); return; }
        if (--threatRow_ == 0) contact();   // on the floor, and nothing held them
    }

    // Lock the run where it stands. Each block falls down its column; the run lands on
    // the tallest column beneath it, and blocks over lower ones fall past and leave the
    // hand. A cell an intruder occupies takes no block, and neither does a full column:
    // those blocks leave the hand too, and a run with nowhere at all to land isn't
    // locked.
    void drop() {
        if (state_ != State::Running) return;
        int land = -1;
        for (int c = left_; c < left_ + width_; ++c)
            if (open(c) && height(c) > land) land = height(c);
        int kept = 0;
        for (int c = left_; c < left_ + width_; ++c) {
            if (!open(c)) continue;
            const int h = height(c);
            locked_[h] |= colBit(c);
            if (h == land) ++kept;
        }
        if (kept > 0) width_ = static_cast<int8_t>(kept);
        left_ = 0;
        dir_ = +1;
    }

    State state() const { return state_; }
    bool running() const { return state_ == State::Running; }
    bool won() const { return state_ == State::Won; }
    bool breached() const { return state_ == State::Breached; }

    // The row the run slides in: just above the tallest column, kept inside the well.
    int row() const {
        int top = 0;
        for (int c = 0; c < kStackerCols; ++c) if (height(c) > top) top = height(c);
        return top < kStackerRows ? top : kStackerRows - 1;
    }
    int left() const { return left_; }      // the run's leftmost column
    int width() const { return width_; }    // how many blocks are in hand

    // How many blocks a column holds. Blocks always fall to rest, so a column is solid
    // from the floor and this is also the row its next block lands in.
    int height(int c) const {
        int h = 0;
        while (h < kStackerRows && (locked_[h] & colBit(c))) ++h;
        return h;
    }

    // The intruders: their columns, and the row they share (kStackerRows = the open row
    // above the well). Held = the wall under it reaches it, so the next drop is contact.
    int threatCol(int i) const { return threatCol_[i]; }
    int threatRow() const { return threatRow_; }
    bool held(int i) const {
        const int h = height(threatCol_[i]);
        return h > 0 && h >= threatRow_;
    }

    // How many blocks a locked row holds.
    int rowWidth(int r) const {
        if (r < 0 || r >= kStackerRows) return 0;
        return popcount(locked_[r]);
    }

    // What the board is WORTH so far: every block is worth its level, level 1 being the
    // floor. So height is paid for twice over — once by reaching a row at all, and again
    // by how many blocks reached it — without the model knowing what the number buys.
    // A held line is worth the whole board, however low it was held: holding it is the
    // goal, and height is only what a breached board is paid for.
    int score() const {
        if (state_ == State::Won) return kStackerMaxScore;
        int total = 0;
        for (int r = 0; r < kStackerRows; ++r) total += popcount(locked_[r]) * (r + 1);
        return total < kStackerMaxScore ? total : kStackerMaxScore;
    }

    // Every block locked so far, at any height. The unit a MAINT run is paid in.
    int blocks() const {
        int total = 0;
        for (int r = 0; r < kStackerRows; ++r) total += popcount(locked_[r]);
        return total;
    }

    // Is (row, col) a LOCKED block? The run in hand is not part of the board until it
    // drops.
    bool locked(int r, int c) const {
        if (r < 0 || r >= kStackerRows || c < 0 || c >= kStackerCols) return false;
        return (locked_[r] & colBit(c)) != 0;
    }

  private:
    void contact() {
        bool all = true;
        for (int i = 0; i < kStackerThreats; ++i) all = all && held(i);
        state_ = all ? State::Won : State::Breached;
    }

    // Can a block land in column c? Not past the top of the well, and not into the row
    // an intruder above it is sitting in.
    bool open(int c) const {
        const int h = height(c);
        if (h >= kStackerRows) return false;
        for (int i = 0; i < kStackerThreats; ++i)
            if (threatCol_[i] == c && h >= threatRow_) return false;
        return true;
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
    int8_t left_ = 0;
    int8_t width_ = 0;
    int8_t dir_ = 1;
    int8_t threatCol_[kStackerThreats] = {0, kStackerThreatSpread};
    int8_t threatRow_ = kStackerRows;
    int8_t threatBeat_ = 0;
    State state_ = State::Running;
};

}  // namespace mal
