// sprite.cpp — alpha-composite a sprite frame into the framebuffer.
#include "core/render/sprite.h"

#include "core/render/framebuffer.h"

namespace mal {

void drawSprite(Framebuffer& fb, const SpriteData& s, int frame, int x, int y,
                int row) {
    if (frame < 0 || frame >= s.frames || row < 0 || row >= s.rows) return;
    const int fx0 = frame * s.frameW;
    const int fy0 = row * s.h;
    for (int r = 0; r < s.h; ++r) {
        const int srcBase = (fy0 + r) * s.sheetW + fx0;
        for (int col = 0; col < s.frameW; ++col) {
            const int idx = srcBase + col;
            fb.blendPixel(x + col, y + r, s.rgb[idx], s.a[idx]);
        }
    }
}

void drawSpriteTinted(Framebuffer& fb, const SpriteData& s, int frame, int x, int y,
                      Rgb565 tint, int row) {
    if (frame < 0 || frame >= s.frames || row < 0 || row >= s.rows) return;
    const int fx0 = frame * s.frameW;
    const int fy0 = row * s.h;
    for (int r = 0; r < s.h; ++r) {
        const int srcBase = (fy0 + r) * s.sheetW + fx0;
        for (int col = 0; col < s.frameW; ++col) {
            const int idx = srcBase + col;
            fb.blendPixel(x + col, y + r, tint, s.a[idx]);   // shape from alpha, colour from the caller
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
        const int srow = (fy0 + oy * den / num) * s.sheetW + fx0;
        for (int ox = 0; ox < dw; ++ox) {
            const int idx = srow + (ox * den / num);
            fb.blendPixel(destX + ox, destY + oy, s.rgb[idx], s.a[idx]);
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
        const int srow = (fy0 + oy * den / num) * s.sheetW + fx0;
        for (int ox = 0; ox < dw; ++ox) {
            const int idx = srow + (ox * den / num);
            const Rgb565 c = blend(s.rgb[idx], flashColor, flashAmt);
            fb.blendPixel(destX + ox, destY + oy, c, s.a[idx]);
        }
    }
}

} // namespace mal
