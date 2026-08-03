// font5x7.h — minimal built-in 5x7 bitmap font for the tracer UI.
//
// Placeholder for FONT_UI (Pixel Operator Mono), which the spec ships as the
// real face but is not in assets/ yet. Covers A-Z, 0-9 and the punctuation the
// UI needs. Monospace: 6px advance (5 + 1 gap), 7px tall. Digits are tabular.
#pragma once

#include "core/render/color.h"

namespace mal {

class Framebuffer;

constexpr int kFontW = 5;
constexpr int kFontH = 7;
constexpr int kFontAdvance = 6;

// Draw uppercase text. Lowercase is upcased; unknown glyphs render as blank.
// `scale` multiplies pixel size. Returns the x just past the drawn text.
int drawText(Framebuffer& fb, int x, int y, const char* s, Rgb565 color,
             int scale = 1);

// Width in pixels a string would occupy at `scale`.
int textWidth(const char* s, int scale = 1);

// Draw `s` word-wrapped into `maxW` pixels, `lineH` apart: skips the first
// `firstLine` wrapped lines (a scroll offset) and then draws at most `maxLines`.
// Returns the y just past the last line, so a caller can flow the next block
// underneath it.
//
// Every screen that draws a content row's description reaches for this: an
// item/mod/move `effect` string is prose written to be read, not sized to the
// 208px a submenu gives it, so drawing one with plain drawText runs it off the
// right edge. Cap `maxLines` at whatever the surrounding layout can spare —
// a fixed-height list row gets 2, a detail page gets the rest of the screen.
int drawTextWrapped(Framebuffer& fb, int x, int y, int maxW, const char* s,
                    Rgb565 color, int lineH, int maxLines, int firstLine = 0);

// How many lines `s` wraps to at `maxW` — for a caller sizing a block or clamping
// a scroll offset without drawing first. Same break rule as drawTextWrapped.
int textWrapLines(const char* s, int maxW);

} // namespace mal
