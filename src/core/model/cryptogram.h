// cryptogram.h — the rules of the DECRYPTOGRAM board, with no rendering and no Game.
//
// A quote is on screen with its letters struck out: punctuation, digits and spaces show
// as themselves, and every A-Z sits on an empty rule. The player picks a LETTER from the
// pool and then a CELL to put it in. Right, and every instance of that letter in the
// quote opens at once — the letter is the unit, not the cell, which is what makes one
// good deduction worth a whole column of the puzzle. Wrong, and the run is over: there
// is no second chance inside a board, only an easier board next time (the per-quote
// difficulty ratchet, which lives on Game rather than here).
//
// Some letters start open, because a wall of empty rules is a guess and not a puzzle.
// See openingReveals() for how many and which.
//
// DETERMINISTIC, not seeded — the Stacker precedent rather than Disk Decryption's. The
// secret is the QUOTE, and which quote gets played is the caller's roll; once it is
// chosen there is nothing left to randomise, and a fixed opening is what lets the
// difficulty ratchet mean "the same board, one more letter open".
//
// Kept a plain model (core/model, like stacker/isolation/disk_decryption) so the rules
// can be tested a press at a time without a framebuffer or a Game. The screen
// (core/ui/cryptogram_screen.h) reads it; it reads nothing.
#pragma once

#include <cstdint>

namespace mal {

// --- The board's shape, which is the panel's shape -------------------------
//
// Cells are one 8px font cell wide and laid out on the same margin the rest of the UI
// uses, so a row is exactly (kActiveW - 2*kMargin) / kFontAdvance cells. Five rows of
// them clear the pool and both bands with room for the attribution line. These are the
// numbers content is written against: tools/check + the native content gate hold every
// quote to a text that WRAPS inside them, which is a stronger check than a character
// count because wrapping wastes the tail of most lines.
constexpr int kCryptogramCols = 26;
constexpr int kCryptogramRows = 5;

// The longest text the board will hold, terminator excluded — a buffer bound, not the
// rule content is written to. The rule is "does it WRAP into kCryptogramRows", which a
// punctuation-heavy 96 can fail and a well-broken 100 can pass; this is simply larger
// than any text that can pass it, so reset() never rejects a quote the gate allowed.
// (The +kCryptogramRows is the spaces a wrap swallows: they occupy no column, so a text
// that fits the grid can still be that many characters longer than the grid is wide.)
constexpr int kCryptogramMaxChars = kCryptogramRows * kCryptogramCols + kCryptogramRows;

// Pool tiles, as two rows of 13 across the same margin. The whole alphabet fits, so the
// pool never actually forces an extra opening reveal today — openingReveals() keeps the
// clamp anyway, because a narrower pool is a layout change and not a rules change.
constexpr int kCryptogramPoolMax = 26;

// Never fewer than this many letters open at the start, whatever the quote.
constexpr int kCryptogramMinReveal = 3;

// col() answers this for a character the wrap dropped — the space that became a line
// break. It has a row, so a caller sweeping the text still knows where it fell, but
// nothing draws it.
constexpr uint8_t kCryptogramHiddenCol = 0xFF;

// A quote's standing with this device, and the whole of what the save keeps about it
// (two bits, save v48). The loss ratchet and the win are ONE axis on purpose: losing a
// board is how a quote gets easier, so "how many times have I lost this" and "have I
// beaten it" are the same question asked twice.
//
// Hard is where every quote starts. There is no rung below Easy — a quote that has
// beaten you twice stops getting easier, because the next rung down opens the whole
// board.
enum class CryptogramTier : uint8_t { Hard = 0, Medium = 1, Easy = 2, Solved = 3 };

// The tier's gift, as the `extraReveals` reset() takes. A Solved quote replayed for
// Bits is offered at Hard: the prize is already spent, so the only thing left to play
// for is the puzzle, and handing over an easy one is not that.
constexpr int cryptogramExtraReveals(CryptogramTier t) {
    switch (t) {
        case CryptogramTier::Medium: return 1;
        case CryptogramTier::Easy:   return 3;
        case CryptogramTier::Hard:
        case CryptogramTier::Solved: break;
    }
    return 0;
}

// The word for the tier — the grayscale-safe channel for the setting, the way
// arcadeDifficultyName is for the cabinets.
constexpr const char* cryptogramTierName(CryptogramTier t) {
    switch (t) {
        case CryptogramTier::Medium: return "MEDIUM";
        case CryptogramTier::Easy:   return "EASY";
        case CryptogramTier::Solved: return "SOLVED";
        case CryptogramTier::Hard:   break;
    }
    return "HARD";
}

class Cryptogram {
  public:
    enum class State : uint8_t {
        Running,
        Solved,   // every letter opened without a miss — the run's whole prize
        Failed,   // one wrong placement, which is all it takes
    };

    // Which half of the two-stage pick the buttons are driving. A press means different
    // things in each, and the hint band says which — see Game::onCryptogram.
    enum class Stage : uint8_t {
        PickLetter,   // A cycles the pool, B takes the focused letter
        PickCell,     // A cycles the open cells, B places, C goes back to the pool
    };

    Cryptogram() { reset("", 0); }

    // Load `text` and open the starting letters. `extraReveals` is the difficulty
    // ratchet's gift on top of the baseline (0 hard / 1 medium / 3 easy).
    //
    // Returns false — and leaves the board Failed and empty — when the text will not
    // wrap into kCryptogramRows or is longer than the buffer. A caller never has to
    // handle that at runtime: the content gate rejects such a quote at build time, and
    // this return is what the gate tests.
    bool reset(const char* text, int extraReveals);

    State state() const { return state_; }
    bool running() const { return state_ == State::Running; }
    bool solved() const { return state_ == State::Solved; }
    Stage stage() const { return stage_; }

    // --- The text ----------------------------------------------------------
    int length() const { return len_; }
    // The plaintext character at `i`, whether or not it is open. The SCREEN must ask
    // isOpen(i) before drawing one; this is the answer, not the display.
    char at(int i) const { return (i >= 0 && i < len_) ? text_[i] : '\0'; }
    // Where the wrap put `i`: its row, and its column or kCryptogramHiddenCol for a
    // space the break swallowed.
    int row(int i) const { return (i >= 0 && i < len_) ? rows_[i] : 0; }
    uint8_t col(int i) const {
        return (i >= 0 && i < len_) ? cols_[i] : kCryptogramHiddenCol;
    }
    int rowCount() const { return rowCount_; }

    static bool isLetter(char c) { return c >= 'A' && c <= 'Z'; }
    // Everything that isn't A-Z — punctuation, digits, spaces — is never hidden: it is
    // the SHAPE the player reads the quote's rhythm off, and hiding it would turn a
    // cryptogram into a word count.
    bool isOpen(int i) const {
        const char c = at(i);
        return !isLetter(c) || letterOpen_[c - 'A'];
    }
    // The letters the player still owes, which is also what Solved is waiting on.
    int cellsLeft() const;

    // --- The pool ----------------------------------------------------------
    // Alphabetical, and only the letters still owed — a letter leaves the pool the
    // instant it opens, so the pool IS the remaining vocabulary.
    int poolSize() const { return poolSize_; }
    char poolLetter(int i) const {
        return (i >= 0 && i < poolSize_) ? pool_[i] : '\0';
    }
    int poolCursor() const { return poolCursor_; }

    // --- The cell cursor ---------------------------------------------------
    // Only ever on a closed letter cell — the open ones and the punctuation are not
    // places a guess can go, so stepping past them is the cursor's job, not the
    // player's.
    int cellCursor() const { return cellCursor_; }

    // --- Play --------------------------------------------------------------
    void cycle();     // A — next pool letter, or next closed cell
    void accept();    // B — take the letter, or place it (which decides the run)
    void stepBack();  // C — from a cell back to the pool; inert in PickLetter

    // --- The verdict -------------------------------------------------------
    // What went wrong, for the screen to point at: the letter that was placed and the
    // cell it was placed in. Both -1/'\0' while the run is alive.
    char wrongLetter() const { return wrongLetter_; }
    int wrongCell() const { return wrongCell_; }

    // How much of the puzzle the PLAYER opened, against how much was left to them —
    // the arcade's partial credit (ArcadeScoring::Incremental). The starting reveals
    // are in neither number: they were never the player's to earn.
    int score() const { return playerOpens_; }
    int maxScore() const { return toOpen_; }

  private:
    void layout();
    void openLetter(char c);
    void rebuildPool();
    int firstClosedCell() const;   // where the cell cursor parks; 0 if nothing is closed
    // How many letters open before the first press: kCryptogramMinReveal, raised if the
    // pool would otherwise overflow, plus the difficulty's `extra` — and always at least
    // one letter short of the whole alphabet the quote uses, because a board that opens
    // itself is not a board.
    static int openingReveals(int distinct, int extra);

    char text_[kCryptogramMaxChars + 1] = {0};
    uint8_t rows_[kCryptogramMaxChars] = {0};
    uint8_t cols_[kCryptogramMaxChars] = {0};
    int len_ = 0;
    int rowCount_ = 0;

    bool letterOpen_[26] = {false};
    char pool_[26] = {0};
    int poolSize_ = 0;
    int poolCursor_ = 0;
    int cellCursor_ = 0;

    int toOpen_ = 0;       // letters left to the player at reset
    int playerOpens_ = 0;  // ...and how many of them they have opened

    State state_ = State::Running;
    Stage stage_ = Stage::PickLetter;
    char wrongLetter_ = '\0';
    int wrongCell_ = -1;
};

}  // namespace mal
