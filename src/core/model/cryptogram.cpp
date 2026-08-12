// cryptogram.cpp — the DECRYPTOGRAM board's rules. See cryptogram.h for the shape and
// the reasoning; this file is the wrap, the opening reveals, and the two-stage pick.
//
// Split off the header (unlike its three sibling minigame models, which are one header
// each) because the wrap alone is longer than all of stacker.h — and every screen that
// includes a model pays for its inline body.

#include "core/model/cryptogram.h"

#include <cstring>

namespace mal {

namespace {

char upper(char c) { return (c >= 'a' && c <= 'z') ? static_cast<char>(c - 32) : c; }

}  // namespace

bool Cryptogram::reset(const char* text, int extraReveals) {
    len_ = 0;
    rowCount_ = 0;
    poolSize_ = 0;
    poolCursor_ = 0;
    cellCursor_ = 0;
    toOpen_ = 0;
    playerOpens_ = 0;
    stage_ = Stage::PickLetter;
    state_ = State::Running;
    wrongLetter_ = '\0';
    wrongCell_ = -1;
    for (bool& b : letterOpen_) b = false;
    std::memset(text_, 0, sizeof(text_));

    if (!text) { state_ = State::Failed; return false; }
    // The face folds case at the glyph lookup, but the RULES cannot: 'e' and 'E' have to
    // be one letter in the pool and one bit in letterOpen_, so the fold happens here.
    int n = 0;
    for (const char* p = text; *p; ++p) {
        if (n >= kCryptogramMaxChars) { state_ = State::Failed; return false; }
        text_[n++] = upper(*p);
    }
    text_[n] = '\0';
    len_ = n;

    layout();
    if (rowCount_ > kCryptogramRows) { state_ = State::Failed; len_ = 0; return false; }

    // Frequency-descending, ties alphabetical: the commonest letters buy the most
    // scaffolding per reveal, which is what makes three of them a fair opening rather
    // than three arbitrary holes. Ties broken alphabetically so the opening is one
    // answer, not an artefact of the sort.
    int freq[26] = {0};
    int distinct = 0;
    for (int i = 0; i < len_; ++i) {
        if (!isLetter(text_[i])) continue;
        if (freq[text_[i] - 'A']++ == 0) ++distinct;
    }
    if (distinct == 0) { state_ = State::Solved; return true; }   // a quote with no letters

    const int reveals = openingReveals(distinct, extraReveals);
    for (int r = 0; r < reveals; ++r) {
        int best = -1;
        for (int c = 0; c < 26; ++c) {
            if (freq[c] == 0 || letterOpen_[c]) continue;
            if (best < 0 || freq[c] > freq[best]) best = c;
        }
        if (best < 0) break;
        letterOpen_[best] = true;
    }

    rebuildPool();
    toOpen_ = poolSize_;
    cellCursor_ = firstClosedCell();
    return true;
}

int Cryptogram::firstClosedCell() const {
    for (int i = 0; i < len_; ++i)
        if (!isOpen(i)) return i;
    return 0;
}

void Cryptogram::layout() {
    // Word-wrapped into kCryptogramCols cells. A space that lands on a break is marked
    // hidden rather than placed: it costs a column otherwise, and the one column at the
    // end of a line is the difference between a quote that fits in five rows and one
    // that doesn't.
    int r = 0, c = 0, i = 0;
    while (i < len_) {
        if (text_[i] == ' ') {
            int j = i;
            while (j < len_ && text_[j] == ' ') ++j;
            int wl = 0;
            while (j + wl < len_ && text_[j + wl] != ' ') ++wl;
            if (c == 0 || c + (j - i) + wl > kCryptogramCols) {
                for (int k = i; k < j; ++k) {
                    rows_[k] = static_cast<uint8_t>(r);
                    cols_[k] = kCryptogramHiddenCol;
                }
                if (c > 0) { ++r; c = 0; }
                i = j;
                continue;
            }
            for (int k = i; k < j; ++k) {
                rows_[k] = static_cast<uint8_t>(r);
                cols_[k] = static_cast<uint8_t>(c++);
            }
            i = j;
            continue;
        }
        int wl = 0;
        while (i + wl < len_ && text_[i + wl] != ' ') ++wl;
        // A single word longer than a row can only be broken mid-word. The content gate
        // never lets one through, so this is a floor and not a feature.
        if (wl > kCryptogramCols) wl = kCryptogramCols;
        if (c > 0 && c + wl > kCryptogramCols) { ++r; c = 0; }
        for (int k = 0; k < wl; ++k) {
            rows_[i + k] = static_cast<uint8_t>(r);
            cols_[i + k] = static_cast<uint8_t>(c++);
        }
        i += wl;
    }
    rowCount_ = len_ > 0 ? r + 1 : 0;
}

int Cryptogram::openingReveals(int distinct, int extra) {
    int n = kCryptogramMinReveal;
    if (distinct - n > kCryptogramPoolMax) n = distinct - kCryptogramPoolMax;
    n += extra > 0 ? extra : 0;
    // Always leave one letter to guess. Without this an easy board on a three-letter
    // quote would open itself and hand over the prize on entry.
    if (n > distinct - 1) n = distinct - 1;
    return n < 0 ? 0 : n;
}

void Cryptogram::rebuildPool() {
    bool present[26] = {false};
    for (int i = 0; i < len_; ++i)
        if (isLetter(text_[i])) present[text_[i] - 'A'] = true;
    poolSize_ = 0;
    for (int c = 0; c < 26; ++c)
        if (present[c] && !letterOpen_[c]) pool_[poolSize_++] = static_cast<char>('A' + c);
    if (poolCursor_ >= poolSize_) poolCursor_ = 0;
}

int Cryptogram::cellsLeft() const {
    int n = 0;
    for (int i = 0; i < len_; ++i)
        if (!isOpen(i)) ++n;
    return n;
}

void Cryptogram::openLetter(char c) {
    if (!isLetter(c) || letterOpen_[c - 'A']) return;
    letterOpen_[c - 'A'] = true;
    ++playerOpens_;
    rebuildPool();
}

// One step of either cursor in either direction. `dir` is +1 or -1; the two public
// verbs are this with the sign flipped, so forwards and backwards can never disagree
// about what counts as a stop.
void Cryptogram::step(int dir) {
    if (!running()) return;
    if (stage_ == Stage::PickLetter) {
        if (poolSize_ > 0) poolCursor_ = (poolCursor_ + poolSize_ + dir) % poolSize_;
        return;
    }
    if (len_ <= 0) return;
    // Walk to the next CLOSED cell, wrapping. Bounded by len_ so a board with nothing
    // closed (which Solved would already have caught) cannot spin.
    for (int n = 1; n <= len_; ++n) {
        const int i = ((cellCursor_ + dir * n) % len_ + len_) % len_;
        if (!isOpen(i)) { cellCursor_ = i; return; }
    }
}

void Cryptogram::cycle() { step(1); }
void Cryptogram::cycleBack() { step(-1); }

void Cryptogram::accept() {
    if (!running() || poolSize_ == 0) return;
    if (stage_ == Stage::PickLetter) {
        stage_ = Stage::PickCell;
        // Land on a closed cell rather than wherever the last placement left it.
        if (cellCursor_ < 0 || cellCursor_ >= len_ || isOpen(cellCursor_))
            cellCursor_ = firstClosedCell();
        return;
    }
    const char want = at(cellCursor_);
    const char got = poolLetter(poolCursor_);
    if (want != got) {
        wrongLetter_ = got;
        wrongCell_ = cellCursor_;
        state_ = State::Failed;
        return;
    }
    openLetter(got);
    if (poolSize_ == 0) { state_ = State::Solved; return; }
    stage_ = Stage::PickLetter;
    // The opened cells took the cursor's cell with them; park it on one that is left.
    if (isOpen(cellCursor_)) cellCursor_ = firstClosedCell();
}

void Cryptogram::dropLetter() {
    if (!running()) return;
    if (stage_ == Stage::PickCell) stage_ = Stage::PickLetter;
}

}  // namespace mal
