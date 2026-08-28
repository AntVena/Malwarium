#include "core/ui/widgets.h"

#include "tunables.h"
#include "core/render/canvas.h"
#include "core/render/font.h"
#include "core/render/framebuffer.h"
#include "core/render/palette.h"
#include "core/ui/layout.h"

namespace mal {

void drawHeaderBand(Framebuffer& fb, const char* title, const char* right,
                    Rgb565 rightColor, Rgb565 titleColor) {
    fb.clear(palColor(Pal::PAPER));
    drawHeaderBandOver(fb, title, right, rightColor, titleColor);
}

void drawHeaderBandOver(Framebuffer& fb, const char* title, const char* right,
                        Rgb565 rightColor, Rgb565 titleColor) {
    // The band's title is the one piece of copy that says where you are, so it
    // carries the bold cut and nothing else on the screen does. Same cell and
    // advance as the regular face (font_glyphs.h), so the band does not move.
    drawText(fb, kMargin, kTitleY, title, titleColor, 1, FontFace::Bold);
    if (right && right[0])
        drawText(fb, kActiveW - kMargin - textWidth(right), kTitleY, right,
                 rightColor);
    fb.fillRect(0, kHeaderRule, kActiveW, 1, palColor(Pal::TRACK));
}

void drawTextMarquee(Framebuffer& fb, int x, int y, int w, const char* s,
                     Rgb565 color, int beat, bool scroll, FontFace face) {
    const int overflow = textWidth(s) - w;
    if (overflow <= 0) { drawText(fb, x, y, s, color, 1, face); return; }

    int offset = 0;
    if (scroll) {
        // Beats, not milliseconds: this rides the same heartbeat as the pet's
        // wander and the carousel's spin, so the whole screen moves on one clock.
        constexpr int kHoldBeats = 6;    // ~1.5s at each end — time to read it
        constexpr int kPxPerBeat = 2;    // ~8px/s, about a character every second
        const int travel = (overflow + kPxPerBeat - 1) / kPxPerBeat;
        const int phase = beat % (2 * kHoldBeats + travel);
        if (phase >= kHoldBeats)
            offset = (phase - kHoldBeats) * kPxPerBeat;
        if (offset > overflow) offset = overflow;   // the hold at the tail
    }

    fb.setClip(x, y, w, kFontH);
    drawText(fb, x - offset, y, s, color, 1, face);
    fb.clearClip();
}

void drawLabelValue(Framebuffer& fb, int x, int y, const char* label, Rgb565 labelCol,
                    const char* value, Rgb565 valueCol, int beat, bool scroll) {
    int room = kActiveW - kMargin - x;
    if (value && value[0]) {
        const int vx = kActiveW - kMargin - textWidth(value);
        drawText(fb, vx, y, value, valueCol);
        room = vx - kMargin - x;          // a margin's gap between the two
    }
    if (room > 0) drawTextMarquee(fb, x, y, room, label, labelCol, beat, scroll);
}

namespace {

Rgb565 zoneColor(Zone z) {
    switch (z) {
        case Zone::Ok: return palColor(Pal::CALM);
        case Zone::Caution: return palColor(Pal::WARN);
        case Zone::Critical: return palColor(Pal::HOT);
    }
    return palColor(Pal::CALM);
}

// Dim an RGB565 toward black for the Critical "off" pulse phase.
Rgb565 dim(Rgb565 c, int pct) {
    return rgb565(static_cast<uint8_t>(r8(c) * pct / 100),
                  static_cast<uint8_t>(g8(c) * pct / 100),
                  static_cast<uint8_t>(b8(c) * pct / 100));
}

// Small inline LCG for the frontier-cell dither — the same one-liner-per-site
// convention Combat::rng uses (core/model/combat.cpp).
uint32_t lcgNext(uint32_t& s) {
    s = s * 1664525u + 1013904223u;
    return s;
}

} // namespace

int fragDitherPattern(bool lit[9], int draws, uint32_t seed) {
    for (int j = 0; j < 9; ++j) lit[j] = false;
    uint32_t s = seed;
    int distinct = 0;
    for (int k = 0; k < draws; ++k) {
        const int idx = static_cast<int>(lcgNext(s) % 9u);
        if (!lit[idx]) ++distinct;
        lit[idx] = true;
    }
    return draws - distinct;      // collisions -> spillover debt
}

void drawSubGrid(Framebuffer& fb, int x, int y, int w, int h, const bool lit[9],
                 Rgb565 c) {
    const int sw = w / 3;
    const int sh = h / 3;
    for (int j = 0; j < 9; ++j) {
        if (!lit[j]) continue;
        const int row = j / 3, col = j % 3;
        const int sx = x + col * sw;
        const int sy = y + row * sh;
        const int fw = (col == 2) ? (w - col * sw) : sw;  // absorb rounding at the far edge
        const int fh = (row == 2) ? (h - row * sh) : sh;
        fb.fillRect(sx, sy, fw, fh, c);
    }
}

void drawGauge(Framebuffer& fb, int x, int y, int w, int h, int value,
               Zone zone, bool fragRamp, bool pulseOn, int beat, const Rgb565* fill) {
    const int cells = 10;
    const int full = value / 10;           // floor: 10 solid cells = 10% each
    const int cellW = w / cells;
    const Rgb565 dimCol = palColor(Pal::INK_DIM);
    const bool pulseDown = (zone == Zone::Critical) && !pulseOn;

    // Fragmentation's glitchy frontier (note in widgets.h): the leftover
    // value%10 breaks the next cell into a 3x3 roll instead of just dropping
    // it, and a roll collision "debt" can punch a few holes in the previous
    // solid cell so the corruption edge bleeds backward.
    bool frontierLit[9] = {};
    bool bleedHoles[9] = {};
    bool hasFrontier = false;
    bool hasBleed = false;
    if (fragRamp) {
        const int frac10 = value - full * 10;   // 0..9 leftover within the frontier cell
        if (frac10 > 0 && full < cells) {
            hasFrontier = true;
            const int draws = (frac10 * 9 + 5) / 10;  // round frac10/10 * 9 sub-cells
            const uint32_t s1 = static_cast<uint32_t>(beat) * 977u + 101u +
                                static_cast<uint32_t>(value) * 7u + 1u;
            const int debt = fragDitherPattern(frontierLit, draws, s1);
            if (debt > 0 && full - 1 >= 0) {
                hasBleed = true;
                uint32_t s2 = static_cast<uint32_t>(beat) * 131u + 202u +
                              static_cast<uint32_t>(value) * 13u + 1u;
                for (int k = 0; k < debt; ++k)
                    bleedHoles[lcgNext(s2) % 9u] = true;
            }
        }
    }

    for (int i = 0; i < cells; ++i) {
        const int cx = x + i * cellW;
        const int iw = cellW - 1;          // 1px gap keeps cells crisp upscaled
        Rgb565 c = fragRamp ? mal::fragRamp(static_cast<float>(i) / (cells - 1))
                   : fill ? *fill
                          : zoneColor(zone);
        if (pulseDown) c = dim(c, 45);

        if (hasBleed && i == full - 1) {
            bool keep[9];
            for (int j = 0; j < 9; ++j) keep[j] = !bleedHoles[j];
            drawSubGrid(fb, cx, y, iw, h, keep, c);
        } else if (hasFrontier && i == full) {
            drawSubGrid(fb, cx, y, iw, h, frontierLit, c);
        } else if (i < full) {
            fb.fillRect(cx, y, iw, h, c);
        } else {
            // Empty cell: thin outline so the count reads without colour.
            fb.fillRect(cx, y, iw, 1, dimCol);
            fb.fillRect(cx, y + h - 1, iw, 1, dimCol);
            fb.fillRect(cx, y, 1, h, dimCol);
            fb.fillRect(cx + iw - 1, y, 1, h, dimCol);
        }
    }
}

void drawCarePips(Framebuffer& fb, int x, int y, int mistakes, bool pulseOn) {
    const int pip = 9, gap = 3;
    const Rgb565 calm = palColor(Pal::CALM);
    const Rgb565 hot = palColor(Pal::HOT);
    const Rgb565 dimCol = palColor(Pal::INK_DIM);
    const bool dying = mistakes >= kCareDying;

    int cx = x;
    for (int i = 0; i < 5; ++i) {
        if (i == kCareGoodMax) {           // gate divider between Good and Bad
            fb.fillRect(cx, y - 1, 1, pip + 2, palColor(Pal::INK));
            cx += gap + 1;
        }
        const bool filled = i < mistakes;
        Rgb565 c = dimCol;
        if (filled) {
            c = (i < kCareGoodMax) ? calm : hot;
            if (dying && !pulseOn) c = rgb565(r8(c) / 2, g8(c) / 2, b8(c) / 2);
        }
        if (filled) {
            fb.fillRect(cx, y, pip, pip, c);
        } else {                            // empty pip: outline (shape carries it)
            fb.fillRect(cx, y, pip, 1, c);
            fb.fillRect(cx, y + pip - 1, pip, 1, c);
            fb.fillRect(cx, y, 1, pip, c);
            fb.fillRect(cx + pip - 1, y, 1, pip, c);
        }
        cx += pip + gap;
    }
}

void drawProgressBar(Framebuffer& fb, int x, int y, int w, int h, float t,
                     Rgb565 c, bool churn, int beat) {
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    const Rgb565 frame = palColor(Pal::INK_DIM);
    // Outline frame — the empty track stays readable in grayscale.
    fb.fillRect(x, y, w, 1, frame);
    fb.fillRect(x, y + h - 1, w, 1, frame);
    fb.fillRect(x, y, 1, h, frame);
    fb.fillRect(x + w - 1, y, 1, h, frame);
    // Fill from the left, inset 1px inside the frame.
    const int fillW = static_cast<int>((w - 2) * t);
    if (fillW > 0) fb.fillRect(x + 1, y + 1, fillW, h - 2, c);

    // Overflow churn: the same "value can't fit past its own cap" cue as the
    // Fragmentation frontier glitch (drawGauge above) — a seeded dither of ink
    // flecks at the fill's leading edge, reseeded per `beat` so it shimmers in
    // place rather than animating a fill level the bar has no room to show.
    if (churn && fillW > 0) {
        const int innerH = h - 2;
        const int churnW = fillW < 6 ? fillW : 6;
        const Rgb565 fleck = palColor(Pal::INK);
        const int edgeX = x + 1 + fillW - churnW;
        uint32_t s = static_cast<uint32_t>(beat) * 733u + 91u +
                     static_cast<uint32_t>(x) * 17u + 1u;
        for (int cy = 0; cy < innerH; ++cy) {
            for (int cx = 0; cx < churnW; ++cx) {
                if ((lcgNext(s) & 1u) != 0u)
                    fb.set(edgeX + cx, y + 1 + cy, fleck);
            }
        }
    }
}

void drawRowCursor(Framebuffer& fb, int x, int y, Rgb565 c) {
    for (int row = 0; row < 7; ++row) {
        const int w = (row <= 3 ? row : 6 - row) + 1;  // 1,2,3,4,3,2,1
        fb.fillRect(x, y + row, w, 1, c);
    }
}

void drawHintBand(Framebuffer& fb, const char* hint) {
    fb.fillRect(0, kActiveH - kHintBandH, kActiveW, kHintBandH, palColor(Pal::TRACK));
    drawText(fb, (kActiveW - textWidth(hint)) / 2, kActiveH - kHintBandH + 4, hint,
             palColor(Pal::INK));
}

void drawStageIndicator(Framebuffer& fb, int x, int y, int w, Stage current) {
    const char* labels[4] = {"BOOT", "PROC", "SCR", "DMN"};
    const int cur = stageIndex(current);
    const int node = 7;
    const int step = w / 4;
    for (int i = 0; i < 4; ++i) {
        const int nx = x + i * step;
        Rgb565 c = (i <= cur) ? palColor(Pal::INK) : palColor(Pal::INK_DIM);
        if (i < 3)  // connector
            fb.fillRect(nx + node, y + node / 2, step - node, 1,
                        palColor(Pal::INK_DIM));
        if (i == cur) {  // current: accent box + label
            fb.fillRect(nx - 1, y - 1, node + 2, node + 2, palColor(Pal::ACCENT));
            fb.fillRect(nx, y, node, node, c);
            drawText(fb, nx, y + node + 2, labels[i], palColor(Pal::INK));
        } else {
            fb.fillRect(nx, y, node, node, c);
        }
    }
}

namespace {

// A gap between the two columns, so a right-aligned value never touches the next
// column's label.
constexpr int kGridGutter = kFontAdvance;

// True if this row fits a half-width column (label + at least one space + value).
// A flag row always takes the full width — its label IS the whole statement, so
// splitting it across a column boundary would read as a truncation.
bool fitsHalf(const SpecRow& r, int colW) {
    if (r.flag()) return false;
    return textWidth(r.label) + kFontAdvance + textWidth(r.value) <= colW;
}

}  // namespace

int gridLines(int w, const SpecRow* rows, int n) {
    const int colW = (w - kGridGutter) / 2;
    int lines = 0;
    bool halfPending = false;
    for (int i = 0; i < n; ++i) {
        if (fitsHalf(rows[i], colW)) {
            if (halfPending) halfPending = false;   // fills the open right cell
            else { ++lines; halfPending = true; }   // opens a new line
        } else {
            if (halfPending) halfPending = false;   // the open line closes short
            ++lines;
        }
    }
    return lines;
}

int drawSpecGrid(Framebuffer& fb, int x, int y, int w, const SpecRow* rows, int n,
                 int lineH, Rgb565 labelColor, Rgb565 valueColor) {
    const int colW = (w - kGridGutter) / 2;
    // Draw one cell: label at the left edge, value pushed to the right edge, so the
    // digits of successive rows line up under each other.
    auto cell = [&](int cx, int cy, int cw, const SpecRow& r) {
        drawText(fb, cx, cy, r.label, labelColor);
        if (!r.flag())
            drawText(fb, cx + cw - textWidth(r.value), cy, r.value, valueColor);
    };
    int lineY = y;
    bool halfPending = false;
    for (int i = 0; i < n; ++i) {
        if (fitsHalf(rows[i], colW)) {
            if (halfPending) {                       // right cell of the open line
                cell(x + colW + kGridGutter, lineY, colW, rows[i]);
                lineY += lineH;
                halfPending = false;
            } else {                                 // left cell of a new line
                cell(x, lineY, colW, rows[i]);
                halfPending = true;
            }
        } else {
            if (halfPending) { lineY += lineH; halfPending = false; }
            cell(x, lineY, w, rows[i]);
            lineY += lineH;
        }
    }
    if (halfPending) lineY += lineH;                 // close a half-filled last line
    return lineY;
}

} // namespace mal
