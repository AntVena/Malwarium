// font.h — FONT_UI text rendering: the four calls every screen draws through.
//
// The face is Pixel Operator Mono at its own 8px cut (assets/fonts/, CC0),
// rasterised into font_glyphs.cpp by tools/gen_font.py. Mono is what makes the
// digits tabular, so a gauge value or a countdown never reflows as it changes,
// and the cut is what keeps it crisp — the generator refuses a size the face
// would be antialiased at (VISUAL_LANGUAGE.md §2.1).
//
// Every glyph is one 8x8 cell and advances 8px, ink inset a column from the
// left with the last row reserved for descenders. Copy renders in caps: the
// fold lives in fontGlyph (font_glyphs.h), not in the call sites.
//
// FontFace (font_glyphs.h) picks the cut, and every call below defaults to
// Regular. Both cuts share the cell and the advance, so a face is never a
// reason to measure or lay anything out differently.
#pragma once

#include "core/render/color.h"
#include "core/render/font_glyphs.h"

namespace mal {

class Framebuffer;

constexpr int kFontW = 8;
constexpr int kFontH = 8;
constexpr int kFontAdvance = 8;

// Draw text at (x,y) — y is the cell top. Unknown glyphs render as blank.
// `scale` multiplies pixel size. Returns the x just past the drawn text.
int drawText(Framebuffer& fb, int x, int y, const char* s, Rgb565 color,
             int scale = 1, FontFace face = FontFace::Regular);

// Width in pixels a string would occupy at `scale`. Exact, not an estimate —
// the face is monospaced. Takes NO face: every cut shares kFontAdvance, so
// measuring is face-invariant and a parameter here would be one the function
// ignores. A cut that ever advanced differently would have to appear here.
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
                    Rgb565 color, int lineH, int maxLines, int firstLine = 0,
                    FontFace face = FontFace::Regular);

// How many lines `s` wraps to at `maxW` — for a caller sizing a block or clamping
// a scroll offset without drawing first. Same break rule as drawTextWrapped, and
// face-invariant for the same reason textWidth is.
int textWrapLines(const char* s, int maxW);

} // namespace mal
