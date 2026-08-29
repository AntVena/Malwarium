// content_riddles.h — the SHIBBOLETH pool: what a guardian can put to a pet.
//
// A row is a riddle and three replies. The guardian draws all four in the CANT
// (core/model/cant.h), so what the player actually sees is legible only as far as their
// sigils reach — the pool is written in plain words and enciphered at the moment it is
// asked. Nothing here knows about the cipher, the same way content_quotes.h knows
// nothing about how a letter opens.
//
// ADDING ONE IS ONE ROW, and that is the design goal:
//
//     {"I WAIT FOREVER FOR ONE WHO WAITS FOR ME.", {"DEADLOCK", "TIMEOUT", "RACE"}},
//
// THE FIRST REPLY IS THE TRUE ONE, always. The three are SHUFFLED when the riddle is
// asked (Game::startShibboleth), so authoring position carries no information and an
// author cannot accidentally teach the pool a habit — "it's usually the second one" is
// not a thing a player can learn here.
//
// TWO RULES A NEW ROW MUST PASS, both checked by the native content gate rather than by
// eye (test/test_native/test_shibboleth.cpp):
//   * The riddle WRAPS into kRiddleBodyLines lines at kRiddleBodyW, and every reply fits
//     kRiddleReplyW on one line. A character count is the wrong check — wrapping wastes
//     the tail of most lines — so write it and run the gate.
//   * Its text is A-Z, digits, spaces and plain ASCII punctuation. FONT_UI is ASCII
//     32..126 (tools/gen_font.py) and anything outside it renders as a hole. Curly
//     quotes and en-dashes are the usual offenders; normalise them.
//
// A riddle carries NO wire number, unlike a Decryptogram quote: the save keeps which
// SIGILS are known and nothing per-riddle, so a row may be rewritten, reordered or
// dropped freely. What a guardian asked last time is not a fact worth persisting — the
// reward was the sigil, and the sigil is what the save has.
//
// Compiled-in and index-addressed, like the quotes and the crews.
#pragma once

namespace mal {

// Three replies: enough that a blind pick is a real risk and few enough that the panel
// can show all of them under a wrapped riddle without scrolling. This number is the
// whole difficulty curve at zero sigils, so it is a constant and not a per-row field.
constexpr int kRiddleReplies = 3;

// The panel the pool is written against, in CELLS rather than pixels — the same shape
// core/model/cryptogram.h states its board in, and for the same two reasons. Content
// must not depend on the UI layer (a header that reached for core/ui/layout.h would put
// every screen constant into every unit that includes a Game), and a column count is
// what an author is actually writing against anyway. The screen multiplies these by the
// font advance; nothing here needs to know what that is.
//
// 26 columns is (224 - 2*8) / 8: the active canvas less the standard margin, on the 8px
// mono grid. A reply is indented two cells past its cursor.
constexpr int kRiddleBodyCols = 26;
constexpr int kRiddleBodyLines = 4;
constexpr int kRiddleReplyIndentCols = 2;
constexpr int kRiddleReplyCols = kRiddleBodyCols - kRiddleReplyIndentCols;

struct RiddleDef {
    const char* text;
    // replies[0] is the TRUE one. See the banner — the order shown is shuffled.
    const char* replies[kRiddleReplies];
};

const RiddleDef* riddles();
int riddleCount();

}  // namespace mal
