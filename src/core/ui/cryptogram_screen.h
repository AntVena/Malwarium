// cryptogram_screen.h — THE DECRYPTOGRAM, the quote board.
//
// One screen for the whole game: the struck-out quote, the letter pool under it, and
// the verdict. Everything the player needs is on it at once, because the deduction is
// "which letter fits that gap" and both halves of that have to be visible together.
//
// The rules are core/model/cryptogram.h; the pool is core/content/content_quotes.h.
// This file is the board and the copy.
#pragma once

#include "core/model/cryptogram.h"

namespace mal {

class Framebuffer;

// The board. `attribution` is drawn only once the quote is solved — it is the reward
// for finishing, never something to guess at — so it may be anything before then and
// the screen will not have shown it.
//
// `tier` is the quote's standing, which the header states as the difficulty word: it is
// also the promise that a lost board comes back easier, and that promise is worth more
// on screen than off. Ignored when `arcade` is set, where the cabinet's own dial set the
// opening and the quote's standing is not what is being played.
//
// `prizeBits` and `prizeLabel` are what THIS board is playing for, set when it starts
// so the stake is on screen throughout and not just once it is won; `prizeLabel` is the
// account unlock beside the Bits, already resolved to its display name (nullptr for a
// replay, which pays Bits alone). Resolving it is the caller's job precisely so this
// screen needs to know nothing about the Rig Shop. `arcade` swaps the prize line for the
// cabinet's, which the till pays on the way out.
//
// `beat` drives the cursor's blink — the only motion on the board, and the thing that
// says which of the two picks the buttons are currently driving.
void drawCryptogramBoard(Framebuffer& fb, const Cryptogram& c, const char* attribution,
                         CryptogramTier tier, int prizeBits, const char* prizeLabel,
                         bool arcade, int beat);

}  // namespace mal
