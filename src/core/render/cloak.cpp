#include "core/render/cloak.h"

#include "core/render/dissolve.h"
#include "core/render/framebuffer.h"
#include "core/render/sprite.h"

namespace mal {

namespace {

// The ripple: bands kBandPx tall, each bending the scene kBendPx one way or the other,
// drifting down a band a beat. One rim pixel in kGlintEvery catches the light.
constexpr int kBandPx = 3;
constexpr int kBendPx = 2;
constexpr int kGlintEvery = 3;

struct Seat {
    const SpriteData& s;
    int frame, fy0, num, den;
    bool mirror;
    int dw, dh;

    int srcX(int ox) const { return spriteSrcX(s, frame, ox * den / num, mirror); }
    int srcY(int oy) const { return fy0 + oy * den / num; }
    bool inside(int ox, int oy) const {
        if (ox < 0 || oy < 0 || ox >= dw || oy >= dh) return false;
        return spriteAlphaAt(s, srcX(ox), srcY(oy)) >= 128;
    }
};

}  // namespace

void drawSpriteCloak(Framebuffer& fb, const SpriteData& s, int frame, int destX, int destY,
                     int num, int den, uint8_t amount, Rgb565 glint, int phase, int row,
                     bool mirror) {
    if (frame < 0 || frame >= s.frames || row < 0 || row >= s.rows) return;
    const Seat seat{s, frame, row * s.h, num, den, mirror, s.frameW * num / den,
                    s.h * num / den};
    for (int oy = 0; oy < seat.dh; ++oy) {
        const int band = oy / kBandPx + phase;
        const int bend = (band & 1 ? kBendPx : -kBendPx) * amount / 255;
        // Walk away from the side being read, so every read is of the scene and never of
        // a pixel this pass already wrote.
        const int step = bend > 0 ? 1 : -1;
        const int ox0 = step > 0 ? 0 : seat.dw - 1;
        for (int ox = ox0; ox >= 0 && ox < seat.dw; ox += step) {
            if (!seat.inside(ox, oy)) continue;
            const int x = destX + ox, y = destY + oy;
            const Rgb565 own = spriteColorAt(s, seat.srcX(ox), seat.srcY(oy));
            const Rgb565 behind = fb.get(x + bend, y);
            Rgb565 out = blend(own, behind, amount);
            const bool rim = !seat.inside(ox - 1, oy) || !seat.inside(ox + 1, oy) ||
                             !seat.inside(ox, oy - 1) || !seat.inside(ox, oy + 1);
            if (rim && dissolveHash(ox * 7 + oy, phase) % kGlintEvery == 0)
                out = blend(out, glint, amount / 2);
            fb.set(x, y, out);
        }
    }
}

}  // namespace mal
