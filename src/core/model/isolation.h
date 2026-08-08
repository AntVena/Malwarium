// isolation.h — the rules of the ISOLATION PROTOCOL, with no rendering and no Game.
//
// The Worm egg's hatch minigame. The worm inside the shell is turned loose in a
// quarantine buffer with one byte of free memory in it, and it never stops moving: one
// button turns it left, the other turns it right, and swallowing a byte grows it by
// kIsolationGrowth cells and puts another byte somewhere else in the buffer. Run into a
// wall or into its own body and the protocol is over. Swallow `goalDots` of them and it
// finished CLEAN — the whole reason the run is worth playing, since the caller spends
// each byte against the incubation clock (Game::finishIsolation).
//
// It is snake, and it is the one shape this line could have had: a worm whose entire
// identity is that it grows by copying itself into whatever space is left, playing a
// game it loses by running out of space it hasn't already filled with itself.
//
// SEEDED, not deterministic — the one place this parts from Stacker beside it. The
// Stacker has no RNG on purpose, because its whole variant is "the same board, played
// better". Here the board IS the worm's own body, which the player builds; only the byte
// placement is drawn, and a fixed sequence of those would be memorised into a route.
// Callers pass a seed (Game hands it one off the shared LCG), so a test can pin a run
// exactly while a real one stays fresh.
//
// Kept a plain model (core/model, like pet_model/combat/stacker) so the rules can be
// tested a step at a time without a framebuffer or a Game. The screen reads it; it reads
// nothing.
#pragma once

#include <cstdint>

namespace mal {

// The quarantine buffer. 176 cells at 12 logical px is a 192x132 panel — wide enough to
// need real steering, short enough to leave the title and the hint band their rows on a
// 224x224 canvas. A cell index fits in a uint8_t (176 < 256), which is what lets the
// body below be a byte per segment and live inside Game without a heap touch.
constexpr int kIsolationCols = 16;
constexpr int kIsolationRows = 11;
constexpr int kIsolationCells = kIsolationCols * kIsolationRows;
constexpr int kIsolationStartLength = 3;
// Cells the worm gains per byte. A rule of the board rather than a balance dial, the
// same way kStackerStartWidth is — what it costs the PLAYER (a minute of incubation per
// byte, and how many bytes make a clean run) is the caller's arithmetic, in tunables.h.
constexpr int kIsolationGrowth = 2;

class Isolation {
  public:
    enum class State : uint8_t {
        Running,
        Crashed,   // hit a wall or its own body — banks whatever it had already eaten
        Clean,     // swallowed the goal: the protocol completed
    };

    Isolation() { reset(0, 1); }

    // Start a fresh run: a short worm on the middle row heading right, one byte placed,
    // and `goal` bytes to eat for a clean finish. `seed` picks the byte sequence.
    void reset(uint32_t seed, int goal) {
        for (int i = 0; i < kIsolationCells; ++i) body_[i] = 0;
        rng_ = seed ? seed : 1u;
        goal_ = goal < 1 ? 1 : goal;
        dots_ = 0;
        grow_ = 0;
        dir_ = 0;            // heading right
        pendingTurn_ = 0;
        state_ = State::Running;

        // Head near the left wall on the middle row, with its tail behind it — so the
        // opening move is into open buffer and the first byte is never a coin flip the
        // player had no time to steer for.
        const int row = kIsolationRows / 2;
        len_ = kIsolationStartLength;
        for (int i = 0; i < len_; ++i) {
            // ring_[0] is the HEAD; each later entry sits one cell further back.
            const uint8_t cell = static_cast<uint8_t>(row * kIsolationCols + (2 - i));
            ring_[i] = cell;
            body_[cell] = 1;
        }
        placeDot();
    }

    // A quarter-turn, banked until the next step. Two of the same in one interval is ONE
    // turn, not a U-turn into its own neck — an accumulator clamped to a single quarter
    // is what makes a mistimed double-press a lost input rather than an instant death.
    // A left and a right in the same interval cancel, which is the same rule read the
    // other way round.
    void turnLeft() { if (pendingTurn_ > -1) --pendingTurn_; }
    void turnRight() { if (pendingTurn_ < 1) ++pendingTurn_; }

    // One step of the run: commit the banked turn, then move a cell.
    void step() {
        if (state_ != State::Running) return;
        dir_ = static_cast<uint8_t>((dir_ + pendingTurn_ + 4) & 3);
        pendingTurn_ = 0;

        int col = head() % kIsolationCols;
        int row = head() / kIsolationCols;
        switch (dir_) {
            case 0: ++col; break;
            case 1: ++row; break;
            case 2: --col; break;
            case 3: --row; break;
        }
        if (col < 0 || col >= kIsolationCols || row < 0 || row >= kIsolationRows) {
            state_ = State::Crashed;    // the buffer wall
            return;
        }

        const uint8_t next = static_cast<uint8_t>(row * kIsolationCols + col);
        const bool eating = next == dot_;
        // The tail cell vacates on this very step unless the worm is mid-growth, so
        // following your own tail is legal — the classic rule, and the one that keeps a
        // tight coil playable.
        const bool tailFrees = grow_ == 0 && !eating;
        if (body_[next] && !(tailFrees && next == ring_[len_ - 1])) {
            state_ = State::Crashed;    // its own body
            return;
        }

        if (tailFrees) {
            body_[ring_[len_ - 1]] = 0;
            --len_;
        } else if (!eating && grow_ > 0) {
            // Holding the tail is what growing IS, so it costs one queued cell — but
            // only when the queue is what paid for it. On a step that EATS, the byte
            // already bought the held tail, and charging the queue as well would quietly
            // shrink every byte swallowed while the last one was still being walked out.
            --grow_;
        }
        for (int i = len_; i > 0; --i) ring_[i] = ring_[i - 1];
        ring_[0] = next;
        ++len_;
        body_[next] = 1;

        if (eating) {
            ++dots_;
            // Not removing the tail on this step already grew it by one, so only the
            // REST of the byte's worth is queued.
            grow_ += kIsolationGrowth - 1;
            if (dots_ >= goal_) { state_ = State::Clean; return; }
            if (!placeDot()) state_ = State::Clean;   // no room left = nothing left to eat
        }
    }

    State state() const { return state_; }
    bool running() const { return state_ == State::Running; }
    bool clean() const { return state_ == State::Clean; }
    int dots() const { return dots_; }            // bytes swallowed so far
    int goal() const { return goal_; }            // bytes that would finish it clean
    int length() const { return len_; }
    int dot() const { return dot_; }              // cell index of the live byte
    int head() const { return ring_[0]; }
    int dir() const { return dir_; }              // 0 right, 1 down, 2 left, 3 up
    // Segment `i` counting back from the head, as a cell index. The renderer walks this
    // rather than the occupancy map so the head can be drawn as its own shape.
    int segment(int i) const { return ring_[i]; }
    bool bodyAt(int cell) const { return body_[cell] != 0; }

  private:
    // Drop the next byte on a uniformly-drawn FREE cell. Returns false when the worm
    // has filled the buffer and there is nowhere left to put one.
    bool placeDot() {
        int free = 0;
        for (int i = 0; i < kIsolationCells; ++i) if (!body_[i]) ++free;
        if (free == 0) return false;
        rng_ = rng_ * 1664525u + 1013904223u;      // the shared LCG's constants
        int pick = static_cast<int>((rng_ >> 16) % static_cast<uint32_t>(free));
        for (int i = 0; i < kIsolationCells; ++i) {
            if (body_[i]) continue;
            if (pick-- == 0) { dot_ = static_cast<uint8_t>(i); return true; }
        }
        return false;
    }

    uint8_t ring_[kIsolationCells] = {0};   // ring_[0] = head, ring_[len_-1] = tail
    uint8_t body_[kIsolationCells] = {0};   // occupancy, so a collision is one lookup
    uint8_t dot_ = 0;
    int16_t len_ = 0;
    int16_t dots_ = 0;
    int16_t goal_ = 1;
    int16_t grow_ = 0;
    uint8_t dir_ = 0;
    int8_t pendingTurn_ = 0;
    State state_ = State::Running;
    uint32_t rng_ = 1;
};

}  // namespace mal
