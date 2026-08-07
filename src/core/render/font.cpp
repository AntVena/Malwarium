// font.cpp — the renderer over FONT_UI's glyph table (see font.h).
//
// Face-independent by construction: everything here is expressed in kFontW /
// kFontH / kFontAdvance and fontGlyph(), so changing the face is a re-run of
// tools/gen_font.py and nothing in this file. A FontFace only reaches the
// lookup — the pixel loops below never learn which cut they are drawing.
#include "core/render/font.h"

#include <cstring>

#include "core/render/font_glyphs.h"
#include "core/render/framebuffer.h"

namespace mal {

int drawText(Framebuffer& fb, int x, int y, const char* s, Rgb565 color,
             int scale, FontFace face) {
    for (const char* p = s; *p; ++p) {
        const uint8_t* g = fontGlyph(*p, face);
        if (g) {
            for (int row = 0; row < kFontH; ++row) {
                const uint8_t bits = g[row];
                if (!bits) continue;   // blank scanline — most cells have several
                for (int col = 0; col < kFontW; ++col)
                    if (bits & (1u << (kFontW - 1 - col)))
                        fb.fillRect(x + col * scale, y + row * scale, scale, scale,
                                    color);
            }
        }
        x += kFontAdvance * scale;
    }
    return x;
}

int textWidth(const char* s, int scale) {
    return static_cast<int>(std::strlen(s)) * kFontAdvance * scale;
}

// The one word-break rule, shared by drawing and counting so the two can never
// disagree about where a line ends. Calls back per word with the line it landed on
// and the x it starts at; `fb == nullptr` measures without drawing.
namespace {
template <typename OnWord>
int wrapWalk(const char* s, int maxW, OnWord onWord) {
    char word[32];
    int lineW = 0, line = 0, cx = 0;
    for (const char* p = s; *p;) {
        int n = 0;
        while (p[n] && p[n] != ' ' && n < 31) { word[n] = p[n]; ++n; }
        word[n] = '\0';
        const int ww = textWidth(word);
        if (lineW > 0 && lineW + kFontAdvance + ww > maxW) {
            ++line;
            lineW = 0;
            cx = 0;
        }
        const bool trailingSpace = p[n] == ' ';
        if (!onWord(line, cx, word, trailingSpace)) return line + 1;
        lineW += ww;
        cx += ww;
        if (trailingSpace) {
            lineW += kFontAdvance;
            cx += kFontAdvance;
            ++n;
        }
        p += n;
    }
    return line + 1;
}
}  // namespace

int drawTextWrapped(Framebuffer& fb, int x, int y, int maxW, const char* s,
                    Rgb565 color, int lineH, int maxLines, int firstLine,
                    FontFace face) {
    int drawn = 0;
    wrapWalk(s, maxW, [&](int line, int cx, const char* word, bool space) {
        const int rel = line - firstLine;
        if (rel < 0) return true;              // above the scroll window
        if (rel >= maxLines) return false;     // past the room we were given
        const int wy = y + rel * lineH;
        int px = drawText(fb, x + cx, wy, word, color, 1, face);
        if (space) drawText(fb, px, wy, " ", color, 1, face);
        if (rel + 1 > drawn) drawn = rel + 1;
        return true;
    });
    return y + (drawn > 0 ? drawn : 1) * lineH;
}

int textWrapLines(const char* s, int maxW) {
    if (!s || !s[0]) return 0;
    return wrapWalk(s, maxW, [](int, int, const char*, bool) { return true; });
}

} // namespace mal
