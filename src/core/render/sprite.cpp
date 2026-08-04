// sprite.cpp — alpha-composite a sprite frame into the framebuffer.
//
// Every blitter walks SHEET COORDINATES and reads through spriteAlphaAt/spriteColorAt
// (sprite.h) rather than indexing rgb[]/a[] directly, so the same four loops serve both
// the full-colour and the 1-bit-mask storage forms.
#include "core/render/sprite.h"

#include "core/render/framebuffer.h"

namespace mal {

void drawSprite(Framebuffer& fb, const SpriteData& s, int frame, int x, int y,
                int row) {
    if (frame < 0 || frame >= s.frames || row < 0 || row >= s.rows) return;
    const int fx0 = frame * s.frameW;
    const int fy0 = row * s.h;
    for (int r = 0; r < s.h; ++r) {
        for (int col = 0; col < s.frameW; ++col) {
            const int px = fx0 + col, py = fy0 + r;
            fb.blendPixel(x + col, y + r, spriteColorAt(s, px, py),
                          spriteAlphaAt(s, px, py));
        }
    }
}

void drawSpriteTinted(Framebuffer& fb, const SpriteData& s, int frame, int x, int y,
                      Rgb565 tint, int row) {
    if (frame < 0 || frame >= s.frames || row < 0 || row >= s.rows) return;
    const int fx0 = frame * s.frameW;
    const int fy0 = row * s.h;
    for (int r = 0; r < s.h; ++r) {
        for (int col = 0; col < s.frameW; ++col) {
            // shape from alpha, colour from the caller
            fb.blendPixel(x + col, y + r, tint, spriteAlphaAt(s, fx0 + col, fy0 + r));
        }
    }
}

void drawSpriteUpscaled(Framebuffer& fb, const SpriteData& s, int frame,
                        int destX, int destY, int num, int den, int row) {
    if (frame < 0 || frame >= s.frames || row < 0 || row >= s.rows) return;
    const int fx0 = frame * s.frameW;
    const int fy0 = row * s.h;
    const int dw = s.frameW * num / den;
    const int dh = s.h * num / den;
    for (int oy = 0; oy < dh; ++oy) {
        const int py = fy0 + oy * den / num;
        for (int ox = 0; ox < dw; ++ox) {
            const int px = fx0 + ox * den / num;
            fb.blendPixel(destX + ox, destY + oy, spriteColorAt(s, px, py),
                          spriteAlphaAt(s, px, py));
        }
    }
}

void drawSpriteFlash(Framebuffer& fb, const SpriteData& s, int frame,
                     int destX, int destY, int num, int den,
                     Rgb565 flashColor, uint8_t flashAmt, int row) {
    if (frame < 0 || frame >= s.frames || row < 0 || row >= s.rows) return;
    if (flashAmt == 0) {
        drawSpriteUpscaled(fb, s, frame, destX, destY, num, den, row);
        return;
    }
    const int fx0 = frame * s.frameW;
    const int fy0 = row * s.h;
    const int dw = s.frameW * num / den;
    const int dh = s.h * num / den;
    for (int oy = 0; oy < dh; ++oy) {
        const int py = fy0 + oy * den / num;
        for (int ox = 0; ox < dw; ++ox) {
            const int px = fx0 + ox * den / num;
            const Rgb565 c = blend(spriteColorAt(s, px, py), flashColor, flashAmt);
            fb.blendPixel(destX + ox, destY + oy, c, spriteAlphaAt(s, px, py));
        }
    }
}

} // namespace mal
