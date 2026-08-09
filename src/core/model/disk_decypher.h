// disk_decypher.h — the rules of DISK DECYPHER, with no rendering and no Game.
//
// The Ransomware line's hatch minigame, and the one that actually looks like getting
// round a ransomware payload: there is a key, you don't have it, and the only way in is
// to guess and read what the guess tells you. Five colours, three slots, five attempts.
// Lock a row and it answers with two numbers — how many cells are the right colour in
// the right place, and how many are the right colour somewhere else — and nothing about
// WHICH. The board keeps every row you have played, because the deduction is the game
// and it has to be done off what is on screen.
//
// It is a code-breaking board at a size that fits a 224px panel: 5^3 = 125 codes, or 60
// with duplicates barred — cracked comfortably inside five attempts by anyone reading
// the feedback, and essentially never by anyone guessing.
//
// SEEDED, not deterministic — the Isolation precedent rather than the Stacker's. The
// code is the whole secret, so a fixed one would be memorised on the second egg; the
// caller passes a seed (Game hands it one off the shared LCG) so a test can pin a run
// exactly while a real one stays fresh.
//
// Kept a plain model (core/model, like stacker/isolation) so the rules can be tested a
// press at a time without a framebuffer or a Game. The screen reads it; it reads nothing.
#pragma once

#include <cstdint>

namespace mal {

// The board. Three slots and five colours fit one row of legible cells across the
// active canvas with the two feedback numbers beside it; five attempts fill the height
// without scrolling, which matters more here than anywhere else — a row the player
// cannot see is a deduction they cannot make.
constexpr int kDecypherSlots = 3;
constexpr int kDecypherColours = 5;
constexpr int kDecypherAttempts = 5;

// The colour vocabulary, in cycle order. The INITIAL is the dual-coding channel — the
// palette gives each one a hue (PAL_CORE's `decypher` block) and this gives it a letter,
// so the board is playable with the colour stripped out entirely.
inline constexpr char kDecypherInitials[kDecypherColours] = {'G', 'B', 'W', 'O', 'P'};

class DiskDecypher {
  public:
    enum class State : uint8_t {
        Running,
        Cracked,   // the code was guessed — the run's whole prize
        Locked,    // five attempts spent without it
    };

    // One played row: what was guessed, and what it was told in return.
    struct Row {
        uint8_t slots[kDecypherSlots] = {0};
        uint8_t exact = 0;    // right colour, right slot
        uint8_t colour = 0;   // right colour, wrong slot
    };

    DiskDecypher() { reset(1, /*allowDuplicates=*/false); }

    // Draw a fresh code and clear the board. `allowDuplicates` lets the code repeat a
    // colour, which is the hard setting: without it the answer is one of 60 codes and
    // every colour ruled out narrows the rest, and with it there are 125 and no such
    // inference.
    void reset(uint32_t seed, bool allowDuplicates) {
        rng_ = seed ? seed : 1u;
        duplicates_ = allowDuplicates;
        played_ = 0;
        cursor_ = 0;
        state_ = State::Running;
        for (int i = 0; i < kDecypherSlots; ++i) { guess_[i] = 0; locked_[i] = false; }
        for (int s = 0; s < kDecypherSlots; ++s) {
            for (int guard = 0; guard < 64; ++guard) {
                const uint8_t c = static_cast<uint8_t>(next() % kDecypherColours);
                if (!duplicates_ && usedInCode(c, s)) continue;
                code_[s] = c;
                break;
            }
        }
    }

    // A: next colour in the focused slot. A locked slot is settled — step off it first.
    void cycleColour() {
        if (state_ != State::Running || locked_[cursor_]) return;
        guess_[cursor_] = static_cast<uint8_t>((guess_[cursor_] + 1) % kDecypherColours);
    }

    // C: step BACK a slot, cyclically, and re-open it. Going back is the same gesture as
    // changing your mind, so it un-locks what it lands on rather than needing a second
    // button for it.
    void stepBack() {
        if (state_ != State::Running) return;
        cursor_ = static_cast<uint8_t>((cursor_ + kDecypherSlots - 1) % kDecypherSlots);
        locked_[cursor_] = false;
    }

    // B: settle the focused slot and move to the next unsettled one. Locking the LAST
    // one is what plays the row — there is no separate submit, because three locks and a
    // submit is four presses for a decision the third one already made.
    void lockIn() {
        if (state_ != State::Running) return;
        locked_[cursor_] = true;
        for (int i = 1; i <= kDecypherSlots; ++i) {
            const int s = (cursor_ + i) % kDecypherSlots;
            if (!locked_[s]) { cursor_ = static_cast<uint8_t>(s); return; }
        }
        commit();
    }

    State state() const { return state_; }
    bool running() const { return state_ == State::Running; }
    bool cracked() const { return state_ == State::Cracked; }
    bool duplicatesAllowed() const { return duplicates_; }

    int cursor() const { return cursor_; }
    int guess(int slot) const { return inSlot(slot) ? guess_[slot] : 0; }
    bool locked(int slot) const { return inSlot(slot) && locked_[slot]; }

    int played() const { return played_; }                    // rows on the board
    int attemptsLeft() const { return kDecypherAttempts - played_; }
    const Row& row(int i) const {
        static const Row kEmpty;
        return (i >= 0 && i < played_) ? rows_[i] : kEmpty;
    }

    // Was played row `i`'s slot `s` an EXACT hit? Not part of the standard feedback —
    // the counts are deliberately anonymous — so this is the easy setting's hint and
    // nothing else may read it. Kept on the model rather than recomputed by the screen
    // because it needs the code, which nothing outside here is allowed to see.
    bool exactAt(int i, int s) const {
        if (i < 0 || i >= played_ || !inSlot(s)) return false;
        return rows_[i].slots[s] == code_[s];
    }

    // The code itself — for the reveal after a lost run, and for tests. Never for the
    // board while it is running.
    int codeAt(int slot) const { return inSlot(slot) ? code_[slot] : 0; }

    // What the run is WORTH: a crack is worth the attempts it did NOT need, counting the
    // one it landed on — so five for a first-guess crack down to one for a last-gasp
    // one — and a locked-out board is worth nothing. Same shape as the Stacker's score:
    // a number out of a ceiling the caller can price against.
    int score() const {
        return state_ == State::Cracked ? kDecypherAttempts - played_ + 1 : 0;
    }
    static constexpr int maxScore() { return kDecypherAttempts; }

  private:
    static bool inSlot(int s) { return s >= 0 && s < kDecypherSlots; }

    uint32_t next() {
        rng_ = rng_ * 1664525u + 1013904223u;
        return rng_ >> 16;
    }

    bool usedInCode(uint8_t c, int upTo) const {
        for (int i = 0; i < upTo; ++i) if (code_[i] == c) return true;
        return false;
    }

    // Score the working guess and file it as a row. The two counts are the standard
    // exact/elsewhere pair, computed with multiplicity: a colour the code holds twice
    // can be credited twice and no more, which is the only part of this that a duplicate
    // code makes non-obvious.
    void commit() {
        Row r;
        bool codeUsed[kDecypherSlots] = {false};
        bool guessUsed[kDecypherSlots] = {false};
        for (int s = 0; s < kDecypherSlots; ++s) {
            r.slots[s] = guess_[s];
            if (guess_[s] == code_[s]) {
                ++r.exact;
                codeUsed[s] = guessUsed[s] = true;
            }
        }
        for (int g = 0; g < kDecypherSlots; ++g) {
            if (guessUsed[g]) continue;
            for (int c = 0; c < kDecypherSlots; ++c) {
                if (codeUsed[c] || guess_[g] != code_[c]) continue;
                codeUsed[c] = true;
                ++r.colour;
                break;
            }
        }
        rows_[played_++] = r;

        if (r.exact == kDecypherSlots) { state_ = State::Cracked; return; }
        if (played_ >= kDecypherAttempts) { state_ = State::Locked; return; }
        // The next attempt opens on the row just played, not on a blank one: the useful
        // next guess is almost always a small edit of the last, and re-cycling three
        // slots from Green every time would be busywork, not difficulty.
        cursor_ = 0;
        for (int i = 0; i < kDecypherSlots; ++i) locked_[i] = false;
    }

    uint8_t code_[kDecypherSlots] = {0};
    uint8_t guess_[kDecypherSlots] = {0};
    bool locked_[kDecypherSlots] = {false};
    Row rows_[kDecypherAttempts];
    uint8_t cursor_ = 0;
    uint8_t played_ = 0;
    bool duplicates_ = false;
    State state_ = State::Running;
    uint32_t rng_ = 1;
};

}  // namespace mal
