#include "core/ui/decypher_screen.h"

#include <cstdio>

#include "core/model/disk_decypher.h"
#include "core/render/canvas.h"
#include "core/render/font.h"
#include "core/render/framebuffer.h"
#include "core/render/palette.h"
#include "core/ui/layout.h"

namespace mal {

namespace {

// The board's geometry, in ACTIVE pixels and drawn at x1 — the Clutch/Isolation
// precedent. Nothing here is art, and whole-pixel cells keep the three columns aligned
// down all five rows, which is the only thing that makes the history readable as a
// table rather than as five separate answers.
constexpr int kCellW = 26;
constexpr int kCellH = 22;
constexpr int kCellGap = 5;
constexpr int kBoardX = 12;
constexpr int kBoardY = 30;
constexpr int kRowPitch = 29;
constexpr int kFeedbackX = kBoardX + kDecypherSlots * (kCellW + kCellGap) + 6;

constexpr int kHeadingY = 10;
constexpr int kEffectY = 186;
constexpr int kStatusY = 172;

int cellX(int slot) { return kBoardX + slot * (kCellW + kCellGap); }
int rowY(int r) { return kBoardY + r * kRowPitch; }

Rgb565 colourOf(int c) {
    switch (c) {
        case 0: return palColor(Pal::DECYPHER_GREEN);
        case 1: return palColor(Pal::DECYPHER_BLUE);
        case 2: return palColor(Pal::DECYPHER_WHITE);
        case 3: return palColor(Pal::DECYPHER_ORANGE);
        default: return palColor(Pal::DECYPHER_PURPLE);
    }
}

void strokeRect(Framebuffer& fb, int x, int y, int w, int h, int t, Rgb565 c) {
    fb.fillRect(x, y, w, t, c);
    fb.fillRect(x, y + h - t, w, t, c);
    fb.fillRect(x, y, t, h, c);
    fb.fillRect(x + w - t, y, t, h, c);
}

// One code cell: the hue, plus the colour's INITIAL punched through it in PAPER. The
// letter is not decoration — it is the whole reason this screen passes the grayscale
// test, so it is drawn for every cell on the board, history and working row alike.
void drawCell(Framebuffer& fb, int x, int y, int colour) {
    fb.fillRect(x, y, kCellW, kCellH, colourOf(colour));
    const char s[2] = {kDecypherInitials[colour % kDecypherColours], '\0'};
    drawText(fb, x + (kCellW - kFontW) / 2, y + (kCellH - kFontH) / 2, s,
             palColor(Pal::PAPER));
}

// An empty slot on a not-yet-played row: an outline, so the board's full height reads
// as five attempts from the first press rather than growing a row at a time.
void drawEmptyCell(Framebuffer& fb, int x, int y) {
    strokeRect(fb, x, y, kCellW, kCellH, 1, palColor(Pal::TRACK));
}

}  // namespace

void drawDiskDecypher(Framebuffer& fb, const DiskDecypher& d, bool showExactHints,
                      bool arcade) {
    fb.clear(palColor(Pal::PAPER));

    const char* title = "DISK DECYPHER";
    drawText(fb, (kActiveW - textWidth(title)) / 2, kHeadingY, title, palColor(Pal::INK));

    const int played = d.played();
    const bool over = !d.running();

    for (int r = 0; r < kDecypherAttempts; ++r) {
        const int y = rowY(r);

        if (r < played) {                                   // a played row + its answer
            const DiskDecypher::Row& row = d.row(r);
            for (int s = 0; s < kDecypherSlots; ++s) {
                drawCell(fb, cellX(s), y, row.slots[s]);
                // EASY only: outline the cells that were exactly right. The standard
                // rules answer in counts and never in positions, so this is the one
                // thing the setting actually gives away.
                if (showExactHints && d.exactAt(r, s))
                    strokeRect(fb, cellX(s) - 2, y - 2, kCellW + 4, kCellH + 4, 2,
                               palColor(Pal::ACCENT));
            }
            char tag[20];
            std::snprintf(tag, sizeof(tag), "%d POS", row.exact);
            drawText(fb, kFeedbackX, y + 1, tag,
                     row.exact > 0 ? palColor(Pal::CALM) : palColor(Pal::INK_DIM));
            std::snprintf(tag, sizeof(tag), "%d COL", row.colour);
            drawText(fb, kFeedbackX, y + 1 + kFontH + 3, tag,
                     row.colour > 0 ? palColor(Pal::INK) : palColor(Pal::INK_DIM));
            continue;
        }

        if (r == played && !over) {                          // the row being built
            for (int s = 0; s < kDecypherSlots; ++s) {
                drawCell(fb, cellX(s), y, d.guess(s));
                // Two states worth telling apart, both by SHAPE: the focused slot
                // carries the cursor box, and a settled one carries a bar under it.
                // The box does NOT blink — a cursor that is invisible half the time is
                // the one piece of state this screen can least afford to hide, since
                // A and C both act on wherever it happens to be.
                if (d.locked(s))
                    fb.fillRect(cellX(s), y + kCellH + 1, kCellW, 2, palColor(Pal::INK));
                if (s == d.cursor())
                    strokeRect(fb, cellX(s) - 3, y - 3, kCellW + 6, kCellH + 6, 2,
                               palColor(Pal::ACCENT));
            }
            continue;
        }

        for (int s = 0; s < kDecypherSlots; ++s) drawEmptyCell(fb, cellX(s), y);
    }

    if (!over) {
        char status[28];
        std::snprintf(status, sizeof(status), "ATTEMPT %d / %d", played + 1,
                      kDecypherAttempts);
        drawText(fb, kBoardX, kStatusY, status, palColor(Pal::INK_DIM));
        // A and C step and B commits, which is close enough to the standard contract
        // that only the nouns need naming.
        const char* hint = "A COLOUR   B LOCK   C BACK";
        drawText(fb, (kActiveW - textWidth(hint)) / 2, kEffectY + 12,
                 hint, palColor(Pal::INK_DIM));
        return;
    }

    // The verdict, and — on a failed run — the key itself, spelled in initials. Showing
    // it is the point: a puzzle that never tells you the answer teaches nothing about
    // the next one, and the last row's feedback is unreadable without it.
    const bool cracked = d.cracked();
    char verdict[36];
    if (cracked) {
        std::snprintf(verdict, sizeof(verdict), "KEY RECOVERED");
    } else {
        char key[kDecypherSlots + 1] = {0};
        for (int s = 0; s < kDecypherSlots; ++s)
            key[s] = kDecypherInitials[d.codeAt(s) % kDecypherColours];
        std::snprintf(verdict, sizeof(verdict), "PAYLOAD HELD - KEY %s", key);
    }
    drawText(fb, (kActiveW - textWidth(verdict)) / 2, kStatusY, verdict,
             cracked ? palColor(Pal::ACCENT) : palColor(Pal::INK));

    char effect[32];
    if (arcade)
        std::snprintf(effect, sizeof(effect), "SCORE %d / %d", d.score(),
                      DiskDecypher::maxScore());
    else if (cracked)
        std::snprintf(effect, sizeof(effect), "INCUBATION HALVED");
    else
        std::snprintf(effect, sizeof(effect), "FULL INCUBATION");
    drawText(fb, (kActiveW - textWidth(effect)) / 2, kEffectY, effect,
             cracked ? palColor(Pal::ACCENT) : palColor(Pal::INK_DIM));

    const char* hint = "B CONTINUE   C DISABLED";
    drawText(fb, (kActiveW - textWidth(hint)) / 2, kEffectY + 14, hint,
             palColor(Pal::INK_DIM));
}

}  // namespace mal
