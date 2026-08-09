#include "core/ui/decryption_screen.h"

#include <cstdio>

#include "core/model/disk_decryption.h"
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
constexpr int kFeedbackX = kBoardX + kDecryptionSlots * (kCellW + kCellGap) + 6;
// The rule under the opening row, sat in the gap between rows 0 and 1. The first
// attempt is a PROBE — it cannot be deduced, only guessed, and it is one of the two
// the score floors out (kDecryptionFreeAttempts) — so the board says so by drawing a
// line under it rather than by explaining itself in words it has no room for.
constexpr int kProbeRuleY = kBoardY + kCellH + 4;

constexpr int kHeadingY = 10;
constexpr int kEffectY = 186;
constexpr int kStatusY = 172;

// The revealed key on a lost board: cells at a size that fits beside the verdict.
constexpr int kKeyCellW = 14;
constexpr int kKeyCellH = 10;
constexpr int kKeyGap = 3;

int cellX(int slot) { return kBoardX + slot * (kCellW + kCellGap); }
int rowY(int r) { return kBoardY + r * kRowPitch; }

Rgb565 colourOf(int c) {
    switch (c) {
        case 0: return palColor(Pal::DECRYPTION_GREEN);
        case 1: return palColor(Pal::DECRYPTION_BLUE);
        case 2: return palColor(Pal::DECRYPTION_WHITE);
        case 3: return palColor(Pal::DECRYPTION_ORANGE);
        default: return palColor(Pal::DECRYPTION_PURPLE);
    }
}

void strokeRect(Framebuffer& fb, int x, int y, int w, int h, int t, Rgb565 c) {
    fb.fillRect(x, y, w, t, c);
    fb.fillRect(x, y + h - t, w, t, c);
    fb.fillRect(x, y, t, h, c);
    fb.fillRect(x + w - t, y, t, h, c);
}

uint32_t lcgNext(uint32_t& s) {
    s = s * 1664525u + 1013904223u;
    return s;
}

// Corrupt a cell the way the STAT page corrupts the Fragmentation gauge: the cell is a
// 3x3 sub-grid, and damage is sub-blocks that AREN'T THERE — punched back to paper, not
// tinted. Rolls are allowed to collide, so the damage clumps instead of spreading
// evenly, and a colour eaten down to two blocks reads as a colour under attack rather
// than as a different colour.
//
// This is the board's one piece of ambient feedback: a row's answer is two numbers the
// player has to hold in their head, and the holes turn the same fact into something the
// eye reads without counting. It never says WHICH cell is wrong (that is the easy
// setting's job, below) — a whole row is eaten together.
void punchCorruption(Framebuffer& fb, int x, int y, int rolls, uint32_t& seed) {
    if (rolls <= 0) return;
    const Rgb565 paper = palColor(Pal::PAPER);
    const int sw = kCellW / 3, sh = kCellH / 3;
    for (int k = 0; k < rolls; ++k) {
        const int idx = static_cast<int>(lcgNext(seed) % 9u);
        const int row = idx / 3, col = idx % 3;
        const int bw = (col == 2) ? kCellW - 2 * sw : sw;
        const int bh = (row == 2) ? kCellH - 2 * sh : sh;
        fb.fillRect(x + col * sw, y + row * sh, bw, bh, paper);
    }
}

// The key cells on a lost board, drawn small and right-aligned. Returns the x it
// started at, so the caller can keep the verdict clear of it.
int drawRevealedKey(Framebuffer& fb, const DiskDecryption& d) {
    const int keyW = kDecryptionSlots * kKeyCellW + (kDecryptionSlots - 1) * kKeyGap;
    const int x0 = kActiveW - kMargin - keyW;
    const int y = kStatusY - 1;
    for (int s = 0; s < kDecryptionSlots; ++s)
        fb.fillRect(x0 + s * (kKeyCellW + kKeyGap), y, kKeyCellW, kKeyCellH,
                    colourOf(d.codeAt(s)));
    const int labelX = x0 - 4 - textWidth("KEY");
    drawText(fb, labelX, kStatusY, "KEY", palColor(Pal::INK_DIM));
    return labelX;
}

}  // namespace

void drawDiskDecryption(Framebuffer& fb, const DiskDecryption& d, bool showExactHints,
                        bool arcade) {
    fb.clear(palColor(Pal::PAPER));

    const char* title = "DISK DECRYPTION";
    drawText(fb, (kActiveW - textWidth(title)) / 2, kHeadingY, title, palColor(Pal::INK));

    const int played = d.played();
    const bool over = !d.running();

    for (int r = 0; r < kDecryptionAttempts; ++r) {
        const int y = rowY(r);

        if (r < played) {                                   // a played row + its answer
            const DiskDecryption::Row& row = d.row(r);
            const int corruption = d.corruption(r);
            // One stream per row, seeded off what the row IS rather than off the beat,
            // so the corruption holds still between repaints instead of crawling.
            uint32_t seed = 1u + static_cast<uint32_t>(r) * 7919u +
                            static_cast<uint32_t>(corruption) * 131u +
                            static_cast<uint32_t>(row.slots[0]) * 17u;
            for (int s = 0; s < kDecryptionSlots; ++s) {
                fb.fillRect(cellX(s), y, kCellW, kCellH, colourOf(row.slots[s]));
                // EASY spends the corruption PER CELL, which is what gives the setting
                // away: a cell that was exactly right stays clean, so the overlay stops
                // being ambient damage and starts being an answer. Every other setting
                // spreads the same total across the whole row and tells you nothing
                // about where it came from.
                if (showExactHints)
                    punchCorruption(fb, cellX(s), y, d.exactAt(r, s) ? 0 : 3, seed);
                else
                    punchCorruption(fb, cellX(s), y, corruption, seed);
            }
            char tag[20];
            std::snprintf(tag, sizeof(tag), "%d POSITION%s", row.exact,
                          row.exact == 1 ? "" : "S");
            drawText(fb, kFeedbackX, y + 1, tag,
                     row.exact > 0 ? palColor(Pal::CALM) : palColor(Pal::INK_DIM));
            std::snprintf(tag, sizeof(tag), "%d COLOUR%s", row.colour,
                          row.colour == 1 ? "" : "S");
            drawText(fb, kFeedbackX, y + 1 + kFontH + 3, tag,
                     row.colour > 0 ? palColor(Pal::INK) : palColor(Pal::INK_DIM));
            continue;
        }

        if (r == played && !over) {                          // the row being built
            for (int s = 0; s < kDecryptionSlots; ++s) {
                fb.fillRect(cellX(s), y, kCellW, kCellH, colourOf(d.guess(s)));
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

        for (int s = 0; s < kDecryptionSlots; ++s)           // a row not yet reached
            strokeRect(fb, cellX(s), y, kCellW, kCellH, 1, palColor(Pal::TRACK));
    }

    fb.fillRect(kBoardX, kProbeRuleY, kActiveW - kMargin - kBoardX, 1,
                palColor(Pal::TRACK));

    if (!over) {
        char status[28];
        std::snprintf(status, sizeof(status), "ATTEMPT %d / %d", played + 1,
                      kDecryptionAttempts);
        drawText(fb, kBoardX, kStatusY, status, palColor(Pal::INK_DIM));
        // A and C step and B commits, which is close enough to the standard contract
        // that only the nouns need naming.
        const char* hint = "A COLOUR   B LOCK   C BACK";
        drawText(fb, (kActiveW - textWidth(hint)) / 2, kEffectY + 12,
                 hint, palColor(Pal::INK_DIM));
        return;
    }

    // The verdict, and — on a failed run — the key itself. Showing it is the point: a
    // puzzle that never tells you the answer teaches nothing about the next one, and
    // the last row's own feedback is unreadable without it.
    const bool cracked = d.cracked();
    const char* verdict = cracked ? "KEY RECOVERED" : "PAYLOAD HELD";
    if (cracked) {
        drawText(fb, (kActiveW - textWidth(verdict)) / 2, kStatusY, verdict,
                 palColor(Pal::ACCENT));
    } else {
        drawRevealedKey(fb, d);
        drawText(fb, kBoardX, kStatusY, verdict, palColor(Pal::INK));
    }

    char effect[32];
    if (arcade)
        std::snprintf(effect, sizeof(effect), "SCORE %d / %d", d.score(),
                      DiskDecryption::maxScore());
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
