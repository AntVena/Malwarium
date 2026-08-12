#include "core/ui/cryptogram_screen.h"

#include <cstdio>

#include "core/render/canvas.h"
#include "core/render/font.h"
#include "core/render/framebuffer.h"
#include "core/render/palette.h"
#include "core/ui/layout.h"
#include "core/ui/widgets.h"

namespace mal {

namespace {

// The board's geometry, in ACTIVE pixels and drawn at x1 — the Clutch/Isolation/Disk
// Decryption precedent. Nothing here is art.
//
// The quote grid is the panel: a cell is one font cell wide and sits on the same margin
// every other screen uses, which is where kCryptogramCols' 26 comes from. Everything
// below it is placed against the FIXED five-row grid rather than against the quote's
// own height, so the pool does not move between a two-row quote and a five-row one —
// the pool is where the player's thumb lives, and it moving would be a worse cost than
// the whitespace over a short quote.
constexpr int kGridX = kMargin;
constexpr int kGridY = 30;
constexpr int kCellW = kFontAdvance;   // 8 — the grid IS the font's advance
constexpr int kRowPitch = 15;          // glyph (8) + the rule under it + leading
constexpr int kRuleDY = 10;            // the empty rule's offset below the cell top
constexpr int kRuleW = 6;              // inset a pixel each side, so cells stay separate

// The verdict/prompt line, then the stake on two lines under it: the Bits, and the
// account unlock beside them. Two lines rather than one because a Rig Shop row's name
// is a phrase, and "256 BITS + INCREASE BANDWIDTH" does not fit a 26-cell row — a
// marquee would put the half that matters most in permanent motion.
constexpr int kStatusY = 116;
constexpr int kPrizeY = 130;
constexpr int kPrizeY2 = 142;

// Two rows of 13 tiles across the same margin, which holds the whole alphabet
// (kCryptogramPoolMax). A tile is 14 wide on a 16 pitch, so the 2px gutter is what
// separates one letter's box from the next.
constexpr int kPoolCols = 13;
constexpr int kPoolX = kMargin;
constexpr int kPoolY = 158;
constexpr int kTileW = 14;
constexpr int kTileH = 14;
constexpr int kTilePitchX = 16;
constexpr int kTilePitchY = 20;

int cellX(int col) { return kGridX + col * kCellW; }
int cellY(int row) { return kGridY + row * kRowPitch; }

void strokeRect(Framebuffer& fb, int x, int y, int w, int h, int t, Rgb565 c) {
    fb.fillRect(x, y, w, t, c);
    fb.fillRect(x, y + h - t, w, t, c);
    fb.fillRect(x, y, t, h, c);
    fb.fillRect(x + w - t, y, t, h, c);
}

// One character, since drawText takes a string and every glyph on this board is placed
// individually rather than run together.
void drawChar(Framebuffer& fb, int x, int y, char ch, Rgb565 c) {
    const char s[2] = {ch, '\0'};
    drawText(fb, x, y, s, c);
}

// --- The quote grid ---------------------------------------------------------
//
// Three kinds of cell, and the difference between them is the whole read:
//   * an OPEN letter is ink, and is the answer;
//   * a CLOSED letter is an empty rule, and is the question;
//   * everything else — punctuation, digits, the spaces between words — is dim ink and
//     is neither. It is never hidden: the rhythm of the sentence is what a cryptogram is
//     solved off, and hiding a comma would only make the board a word count.
void drawGrid(Framebuffer& fb, const Cryptogram& c, bool failed) {
    for (int i = 0; i < c.length(); ++i) {
        const uint8_t col = c.col(i);
        if (col == kCryptogramHiddenCol) continue;   // a space the wrap swallowed
        const int x = cellX(col), y = cellY(c.row(i));
        const char ch = c.at(i);
        if (!Cryptogram::isLetter(ch)) {
            if (ch != ' ') drawChar(fb, x, y, ch, palColor(Pal::INK_DIM));
            continue;
        }
        if (c.isOpen(i)) {
            drawChar(fb, x, y, ch, palColor(Pal::INK));
            continue;
        }
        // A lost board shows the misplacement and nothing else. Revealing the answer
        // would be a kindness that costs the retry everything — the same quote comes
        // back one rung easier, and it is not a puzzle if it has already been read.
        if (failed && i == c.wrongCell()) {
            fb.fillRect(x, y - 1, kCellW, kFontH + 2, palColor(Pal::HOT));
            drawChar(fb, x, y, c.wrongLetter(), palColor(Pal::PAPER));
            continue;
        }
        fb.fillRect(x + 1, y + kRuleDY, kRuleW, 1, palColor(Pal::INK_DIM));
    }
}

// The cell cursor: a solid bar under the gap the letter in hand would go into, plus a
// caret over it. Two channels, because the bar alone is the same shape as the empty
// rule it sits on and a grayscale screenshot has to tell them apart.
void drawCellCursor(Framebuffer& fb, const Cryptogram& c, int beat) {
    const int i = c.cellCursor();
    if (i < 0 || i >= c.length()) return;
    const uint8_t col = c.col(i);
    if (col == kCryptogramHiddenCol) return;
    const int x = cellX(col), y = cellY(c.row(i));
    fb.fillRect(x + 1, y + kRuleDY, kRuleW, 2, palColor(Pal::ACCENT));
    if (beat & 1) fb.fillRect(x + 3, y + kRuleDY + 3, 2, 2, palColor(Pal::ACCENT));
}

// --- The pool ---------------------------------------------------------------
//
// The letters still owed, and only those — a letter leaves the moment it opens, so the
// pool shrinking IS the progress bar. The focused tile is FILLED while the player is
// choosing and merely OUTLINED once they have taken it: fill versus outline is a shape
// channel, so which of the two picks is live survives the grayscale gate.
void drawPool(Framebuffer& fb, const Cryptogram& c, bool running) {
    const bool inHand = c.stage() == Cryptogram::Stage::PickCell;
    for (int i = 0; i < c.poolSize() && i < kCryptogramPoolMax; ++i) {
        const int x = kPoolX + (i % kPoolCols) * kTilePitchX;
        const int y = kPoolY + (i / kPoolCols) * kTilePitchY;
        const bool focused = running && i == c.poolCursor();
        Rgb565 ink = palColor(Pal::INK);
        if (focused && !inHand) {
            fb.fillRect(x, y, kTileW, kTileH, palColor(Pal::ACCENT));
            ink = palColor(Pal::PAPER);
        } else if (focused) {
            strokeRect(fb, x, y, kTileW, kTileH, 1, palColor(Pal::ACCENT));
        } else {
            strokeRect(fb, x, y, kTileW, kTileH, 1, palColor(Pal::TRACK));
            if (!running) ink = palColor(Pal::INK_DIM);
        }
        drawChar(fb, x + 3, y + 3, c.poolLetter(i), ink);
    }
}

}  // namespace

void drawCryptogramBoard(Framebuffer& fb, const Cryptogram& c, const char* attribution,
                         CryptogramTier tier, int prizeBits, const char* prizeLabel,
                         bool arcade, int beat) {
    const bool running = c.running();
    const bool solved = c.solved();
    const bool failed = !running && !solved;

    // The header states the difficulty because it is also the promise: a board lost at
    // HARD comes back at MEDIUM. On a cabinet the dial set the opening instead, and the
    // quote's own standing is not what is being played.
    drawHeaderBand(fb, "DECRYPTOGRAM", arcade ? "ARCADE" : cryptogramTierName(tier),
                   palColor(Pal::INK_DIM));

    drawGrid(fb, c, failed);
    if (running && c.stage() == Cryptogram::Stage::PickCell) drawCellCursor(fb, c, beat);

    // The attribution, under the quote's own last row rather than the grid's — a short
    // quote should not float above a hole. Solved boards only: never guessed, and never
    // given away by a loss either.
    if (solved && attribution && attribution[0]) {
        char line[40];
        std::snprintf(line, sizeof(line), "- %s", attribution);
        const int w = textWidth(line);
        const int x = kActiveW - kMargin - w;
        drawText(fb, x < kMargin ? kMargin : x, cellY(c.rowCount()) + 2, line,
                 palColor(Pal::INK_DIM));
    }

    // --- The status line: what the buttons are asking for, or how it ended ---
    char status[32];
    Rgb565 statusCol = palColor(Pal::INK);
    if (solved) {
        std::snprintf(status, sizeof(status), "DECRYPTED");
        statusCol = palColor(Pal::CALM);
    } else if (failed) {
        std::snprintf(status, sizeof(status), "MISPLACED - RUN OVER");
        statusCol = palColor(Pal::HOT);
    } else if (c.stage() == Cryptogram::Stage::PickCell) {
        std::snprintf(status, sizeof(status), "PLACE %c", c.poolLetter(c.poolCursor()));
        statusCol = palColor(Pal::ACCENT);
    } else {
        std::snprintf(status, sizeof(status), "PICK A LETTER  %d LEFT", c.poolSize());
    }
    drawText(fb, kMargin, kStatusY, status, statusCol, 1, FontFace::Bold);

    // --- The stake, stated the whole way through ------------------------------
    // A player deciding whether to risk a placement is deciding against a number, so
    // the number is on screen before the risk and not only after it. A LOST board
    // states the consolation in its place: the run bought nothing except the rung the
    // quote just dropped to, which is the only reason losing is survivable.
    char line1[36];
    Rgb565 lineCol = palColor(Pal::INK_DIM);
    if (arcade) std::snprintf(line1, sizeof(line1), "CABINET PAYS ON EXIT");
    else if (failed) std::snprintf(line1, sizeof(line1), "NOTHING BANKED");
    else if (solved) {
        std::snprintf(line1, sizeof(line1), "WON %d BITS", prizeBits);
        lineCol = palColor(Pal::CALM);
    } else {
        std::snprintf(line1, sizeof(line1), "PRIZE %d BITS", prizeBits);
    }
    drawText(fb, kMargin, kPrizeY, line1, lineCol);

    char line2[36] = {0};
    Rgb565 line2Col = lineCol;
    if (failed && !arcade && tier != CryptogramTier::Solved) {
        std::snprintf(line2, sizeof(line2), "COMES BACK %s", cryptogramTierName(tier));
        line2Col = palColor(Pal::ACCENT);
    } else if (!failed && !arcade && prizeLabel && prizeLabel[0]) {
        std::snprintf(line2, sizeof(line2), "+ %s", prizeLabel);
    }
    if (line2[0])
        drawTextMarquee(fb, kMargin, kPrizeY2, kActiveW - 2 * kMargin, line2, line2Col,
                        beat, /*scroll=*/true);

    drawPool(fb, c, running);

    if (!running) drawHintBand(fb, "B DONE");
    else if (c.stage() == Cryptogram::Stage::PickCell)
        drawHintBand(fb, "A MOVE  B PLACE  C DROP");
    else
        drawHintBand(fb, "A NEXT  B TAKE");
}

}  // namespace mal
