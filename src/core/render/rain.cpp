#include "core/render/rain.h"

#include "core/render/dissolve.h"
#include "core/render/font.h"
#include "core/render/framebuffer.h"
#include "core/render/sprite.h"

namespace mal {

namespace {

// How many glyphs trail a column's leading digit, and how far apart the columns' drops
// start, in rows. A column whose head has run off the bottom waits this gap before it
// comes back over the top.
constexpr int kRainTrail = 5;
constexpr int kRainGap = 2;
constexpr uint8_t kTrailAlpha = 140;
// Every glyph cell not in a column's run still shows its digit this faintly, so the
// silhouette reads as full of data rather than as an empty shape with a few drops in it.
constexpr uint8_t kRestAlpha = 55;

struct Seat {
    const SpriteData& s;
    int frame, fy0, num, den;
    bool mirror;
    int dw, dh;

    bool inside(int ox, int oy) const {
        if (ox < 0 || oy < 0 || ox >= dw || oy >= dh) return false;
        const int px = spriteSrcX(s, frame, ox * den / num, mirror);
        return spriteAlphaAt(s, px, fy0 + oy * den / num) >= 128;
    }
};

// How far below its column's leading digit glyph row `gr` is: 0 is the head, 1..trail
// the trail behind it, anything else dark.
int rainDepth(int col, int gr, int rows, int phase) {
    const int cycle = rows + kRainGap;
    const int head = static_cast<int>((static_cast<uint32_t>(phase) +
                                       dissolveHash(col, 0x5a17) % cycle) %
                                      cycle);
    return head - gr;
}

}  // namespace

void drawSpriteRain(Framebuffer& fb, const SpriteData& s, int frame, int destX, int destY,
                    int num, int den, Rgb565 body, Rgb565 lit, int phase, int row,
                    bool mirror) {
    if (frame < 0 || frame >= s.frames || row < 0 || row >= s.rows) return;
    const Seat seat{s, frame, row * s.h, num, den, mirror, s.frameW * num / den,
                    s.h * num / den};
    const int rows = (seat.dh + kFontH - 1) / kFontH;
    for (int oy = 0; oy < seat.dh; ++oy) {
        for (int ox = 0; ox < seat.dw; ++ox) {
            if (!seat.inside(ox, oy)) continue;
            const int x = destX + ox, y = destY + oy;
            if (!seat.inside(ox - 1, oy) || !seat.inside(ox + 1, oy) ||
                !seat.inside(ox, oy - 1) || !seat.inside(ox, oy + 1)) {
                fb.blendPixel(x, y, lit, 255);
                continue;
            }
            fb.blendPixel(x, y, body, 255);
            const int col = ox / kFontAdvance, gr = oy / kFontH;
            const int depth = rainDepth(col, gr, rows, phase);
            const bool inRun = depth >= 0 && depth <= kRainTrail;
            const char digit =
                (dissolveHash(col * 131 + gr, inRun ? phase - depth : 0) & 1) ? '1' : '0';
            const uint8_t bits = fontGlyph(digit)[oy % kFontH];
            if (!(bits & (1u << (kFontW - 1 - ox % kFontAdvance)))) continue;
            const uint8_t a = !inRun ? kRestAlpha : depth == 0 ? 255 : kTrailAlpha;
            fb.blendPixel(x, y, lit, a);
        }
    }
}

}  // namespace mal
