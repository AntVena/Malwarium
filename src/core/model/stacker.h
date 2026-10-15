// stacker.h — the rules of the STACKER, with no rendering and no Game.
//
// The FIREWALL. Intruders drop down the well over their columns while you raise walls
// under them: a run of blocks slides across the well and B locks it, every block falling
// straight down its column onto whatever is there — a block, or the floor. The run lands
// on the tallest column beneath it; blocks over lower columns fall past and leave the
// hand, which is what narrows it. A run over a flat stretch (open floor included) keeps
// every block, so a second wall can be started anywhere without overlapping the first.
//
// An intruder that reaches a wall is stopped, and takes the block it hit with it. One
// that reaches the floor is a BREACH, and ends the run. Stop every intruder and the line
// is HELD. The catch is the floor: every lock that puts a block on the ground queues
// another intruder over one of the columns it touched, so a new wall — or a block that
// fell past one — costs a block off that wall later, and enough of them wear it through.
// What a held line buys is the caller's business (Game::settlePlayedRun: a MAINT
// process or the arcade till).
//
// The opening intruders' columns and the queued ones' are seeded per board, so the aim
// is read each run rather than recited.
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
// Slow enough that a board is a few seconds of building between impacts, not a sprint.
constexpr int kStackerThreatBeats = 16;

// How many drops a called intruder waits above the open row before entering it, so the
// ones a board's opening locks call land after the opening intruders rather than with
// them.
constexpr int kStackerCallDelay = 2;

// How many intruders a board opens with, and how far apart their columns always are: at
// least a full hand apart, so no single lock covers both and each needs a wall of its
// own. Queued intruders join them up to kStackerMaxThreats in the well at once; a lock
// that touches the ground past that queues nothing.
constexpr int kStackerDealtThreats = 2;
constexpr int kStackerThreatSpread = kStackerStartWidth;
constexpr int kStackerMaxThreats = 8;

// The best a board can be worth (see score()): a full hand's wall under each opening
// intruder, all the way up.
constexpr int kStackerMaxScore =
    kStackerDealtThreats * kStackerStartWidth * kStackerRows * (kStackerRows + 1) / 2;
// A held line scores the maximum less kStackerLossCost per block lost past the ones
// every board loses — each opening intruder's, and one for the intruder each of their
// walls' first lock calls — but never below kStackerHeldFloor. A breached board scores
// what it built, kept under that floor, so holding the line always outscores losing it.
constexpr int kStackerFreeLosses = 2 * kStackerDealtThreats;
constexpr int kStackerLossCost = kStackerMaxScore / 18;
constexpr int kStackerHeldFloor = kStackerMaxScore / 2;

class Stacker {
  public:
    // Won = every intruder stopped; Breached = one reached the floor.
    enum class State : uint8_t { Running, Won, Breached };

    explicit Stacker(uint32_t seed = 0) { reset(seed); }

    // Start a fresh run: an empty board, a full-width run in hand, and the opening
    // intruders over the seed's pair of columns in the open row above the well.
    void reset(uint32_t seed = 0) {
        for (int r = 0; r < kStackerRows; ++r) locked_[r] = 0;
        width_ = kStackerStartWidth;
        left_ = 0;
        dir_ = +1;
        threats_ = 0;
        destroyed_ = 0;
        threatBeat_ = 0;
        rng_ = seed * 2654435761u + 1u;
        // Every pair at least kStackerThreatSpread apart, enumerated so the seed indexes
        // them evenly.
        static_assert(kStackerDealtThreats == 2, "the deal picks a pair");
        int pairs = 0;
        for (int a = 0; a < kStackerCols; ++a)
            for (int b = a + kStackerThreatSpread; b < kStackerCols; ++b) ++pairs;
        int pick = static_cast<int>(seed % static_cast<uint32_t>(pairs));
        for (int a = 0; a < kStackerCols; ++a)
            for (int b = a + kStackerThreatSpread; b < kStackerCols; ++b)
                if (pick-- == 0) { spawn(a, 0); spawn(b, 0); }
        state_ = State::Running;
    }

    // One beat: the run slides, and every kStackerThreatBeats-th beat the intruders
    // drop a row. The run bounces off both walls, and the bounce spends a column of the
    // stride: at the fast step a run that only ever moved two at a time would keep one
    // parity forever, and a single block could never stop on half the columns. A run as
    // wide as the board can't move at all, a legal (if generous) state.
    void step() {
        if (state_ != State::Running) return;
        const int span = kStackerCols - width_;
        const int stride = row() >= kStackerFastFromRow ? kStackerFastStep : kStackerSlowStep;
        for (int i = 0; i < stride && span > 0; ++i) {
            if (left_ + dir_ < 0 || left_ + dir_ > span) dir_ = -dir_;
            else left_ += dir_;
        }
        if (++threatBeat_ < kStackerThreatBeats) return;
        threatBeat_ = 0;
        dropThreats();
    }

    // Lock the run where it stands. Each block falls down its column; the run lands on
    // the tallest column beneath it, and blocks over lower ones fall past and leave the
    // hand. A cell an intruder occupies takes no block, and neither does a full column:
    // those blocks leave the hand too, and a run with nowhere at all to land isn't
    // locked. Any block that lands on the ground queues an intruder.
    void drop() {
        if (state_ != State::Running) return;
        int land = -1;
        for (int c = left_; c < left_ + width_; ++c)
            if (open(c) && height(c) > land) land = height(c);
        int kept = 0;
        int grounded[kStackerCols];
        int nGrounded = 0;
        for (int c = left_; c < left_ + width_; ++c) {
            if (!open(c)) continue;
            const int h = height(c);
            locked_[h] |= colBit(c);
            if (h == land) ++kept;
            if (h == 0) grounded[nGrounded++] = c;
        }
        if (kept > 0) width_ = static_cast<int8_t>(kept);
        if (nGrounded > 0)
            spawn(grounded[next() % static_cast<uint32_t>(nGrounded)], kStackerCallDelay);
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

    // How many blocks a column holds. Blocks always fall to rest and are only ever
    // taken off the top, so a column is solid from the floor and this is also the row
    // its next block lands in.
    int height(int c) const {
        int h = 0;
        while (h < kStackerRows && (locked_[h] & colBit(c))) ++h;
        return h;
    }

    // The intruders still in play, the opening ones and every one queued since. Rows
    // past kStackerRows are queued above the open row, waiting for it to clear in their
    // column; an intruder whose row equals its column's height is resting on it, and
    // takes that block at the next drop.
    int threatCount() const { return threats_; }
    int threatCol(int i) const { return threatCol_[i]; }
    int threatRow(int i) const { return threatRow_[i]; }

    // Blocks intruders have taken off the walls this run.
    int destroyed() const { return destroyed_; }

    // How many blocks a locked row holds.
    int rowWidth(int r) const {
        if (r < 0 || r >= kStackerRows) return 0;
        return popcount(locked_[r]);
    }

    // What the board is WORTH so far: every block is worth its level, level 1 being the
    // floor. So height is paid for twice over — once by reaching a row at all, and again
    // by how many blocks reached it — without the model knowing what the number buys.
    // A held line is scored on what holding it cost instead (kStackerLossCost).
    int score() const {
        if (state_ == State::Won) {
            const int lost = destroyed_ > kStackerFreeLosses ? destroyed_ - kStackerFreeLosses
                                                              : 0;
            const int held = kStackerMaxScore - lost * kStackerLossCost;
            return held > kStackerHeldFloor ? held : kStackerHeldFloor;
        }
        int total = 0;
        for (int r = 0; r < kStackerRows; ++r) total += popcount(locked_[r]) * (r + 1);
        return total < kStackerHeldFloor ? total : kStackerHeldFloor - 1;
    }

    // Every block still standing, at any height. The unit a MAINT run is paid in, so a
    // block an intruder took is a block the run doesn't pay for.
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
    // Queue an intruder over column c, `delay` drops above the open row and behind any
    // already waiting there.
    void spawn(int c, int delay) {
        if (threats_ >= kStackerMaxThreats) return;
        int r = kStackerRows + delay;
        for (int i = 0; i < threats_; ++i)
            if (threatCol_[i] == c && threatRow_[i] >= r) r = threatRow_[i] + 1;
        threatCol_[threats_] = static_cast<int8_t>(c);
        threatRow_[threats_] = static_cast<int8_t>(r);
        ++threats_;
    }

    // Every intruder moves a row, lowest first so one never steps into a cell the one
    // below it is about to leave. One already resting on its column takes the top block
    // and is gone; one resting on the floor is the breach.
    void dropThreats() {
        for (int a = 1; a < threats_; ++a)
            for (int b = a; b > 0 && threatRow_[b] < threatRow_[b - 1]; --b) {
                const int8_t c = threatCol_[b], r = threatRow_[b];
                threatCol_[b] = threatCol_[b - 1]; threatRow_[b] = threatRow_[b - 1];
                threatCol_[b - 1] = c; threatRow_[b - 1] = r;
            }
        int kept = 0;
        bool breach = false;
        for (int i = 0; i < threats_; ++i) {
            const int c = threatCol_[i], h = height(c);
            int8_t r = threatRow_[i];
            if (r == h && h > 0) {
                locked_[h - 1] &= static_cast<uint8_t>(~colBit(c));
                ++destroyed_;
                continue;
            }
            if (r == h) breach = true;   // on the floor: it stays there, to be drawn
            else --r;
            threatCol_[kept] = threatCol_[i];
            threatRow_[kept] = r;
            ++kept;
        }
        threats_ = static_cast<int8_t>(kept);
        if (breach) state_ = State::Breached;
        else if (threats_ == 0) state_ = State::Won;
    }

    // Can a block land in column c? Not past the top of the well, and not into a cell an
    // intruder is sitting in.
    bool open(int c) const {
        const int h = height(c);
        if (h >= kStackerRows) return false;
        for (int i = 0; i < threats_; ++i)
            if (threatCol_[i] == c && threatRow_[i] == h) return false;
        return true;
    }

    uint32_t next() {
        rng_ = rng_ * 1664525u + 1013904223u;
        return rng_ >> 8;
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
    int8_t threatCol_[kStackerMaxThreats] = {};
    int8_t threatRow_[kStackerMaxThreats] = {};
    int8_t threats_ = 0;
    int8_t threatBeat_ = 0;
    int16_t destroyed_ = 0;
    uint32_t rng_ = 1;
    State state_ = State::Running;
};

}  // namespace mal
