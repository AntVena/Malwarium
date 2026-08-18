#include "core/render/shred.h"

#include "core/render/dissolve.h"
#include "core/render/framebuffer.h"
#include "core/render/sprite.h"

namespace mal {

namespace {

// Rows leave in a stagger rather than all at once, packed into the first part of the
// sweep so the last one still has a slide left in it — the same headroom FX_ABSORB
// keeps for the same reason.
constexpr int kDepartSpan = 150;    // of 255

// How far a row can travel, as a multiple of its own speed roll. At the ×1.75 creature
// scale this carries a sliver clear of a 56-wide cell without throwing it across the
// whole screen, which would read as debris rather than as the thing falling apart.
constexpr int kSlidePx = 40;

// The streak: a departing row smears back toward where it came from, one sample per
// step, so the sliver reads as a LINE rather than as a row of pixels that moved. It
// grows as the row picks up speed and is what separates this from a plain slide.
constexpr int kTrailMax = 7;

// A row thins before it goes, dropping alternate pixels once it is most of the way
// out. The gap pattern is the row's own scatter, so it frays instead of dimming
// uniformly — which is what survives at this palette's contrast, where a fade to 30%
// against PAPER is nearly a fade to nothing.
constexpr uint8_t kFrayFrom = 140;

}  // namespace

void drawShred(Framebuffer& fb, const SpriteData& s, int frame,
               int srcX, int srcY, int num, int den, Rgb565 tint, uint8_t progress,
               int row) {
    if (frame < 0 || frame >= s.frames || row < 0 || row >= s.rows) return;
    if (den <= 0 || num <= 0) return;
    if (progress >= 255) return;                      // nothing left of it

    const int block = (num + den - 1) / den;
    const int cellW = s.frameW, cellH = s.h;
    if (cellW <= 0 || cellH <= 0) return;

    for (int py = 0; py < cellH; ++py) {
        // Everything about a sliver is this one roll on its ROW — so the whole row
        // travels as a piece, which is what makes it a line instead of a smear of
        // independently wandering pixels.
        const uint32_t rh = dissolveHash(py, 0x5EED);
        const int dir = (rh & 1u) ? 1 : -1;
        const int speed = 1 + static_cast<int>((rh >> 1) & 3u);      // 1..4
        const int depart = static_cast<int>((rh >> 8) & 0xFFu) * kDepartSpan / 255;

        // A row that has not left yet is still the sprite: one sample, no offset. The
        // streak only exists once it is moving.
        int slide = 0, trail = 1, rowA = 255;
        bool fraying = false;
        if (progress > depart) {
            const int room = 255 - depart;
            const int q = room > 0 ? (progress - depart) * 255 / room : 255;
            slide = dir * q * speed * kSlidePx / (4 * 255);
            trail = 1 + q * kTrailMax / 255;
            // Fray and fade together over the same tail. The fray is what reads at this
            // palette's contrast; the fade is what guarantees the row reaches nothing
            // by the end of the sweep rather than thinning to a stubborn ghost.
            fraying = q > kFrayFrom;
            if (fraying) rowA = 255 - (q - kFrayFrom) * 255 / (255 - kFrayFrom);
        }

        for (int px = 0; px < cellW; ++px) {
            if (spriteAlphaAt(s, frame * cellW + px, row * cellH + py) < 128) continue;
            // A frayed row drops the pixels its own scatter picks, so the sliver comes
            // apart along its length rather than dimming as a bar.
            if (fraying && (dissolveHash(px, py) & 1u)) continue;

            const Rgb565 col =
                s.bits ? tint : spriteColorAt(s, frame * cellW + px, row * cellH + py);
            const int restX = srcX + px * num / den;
            const int y = srcY + py * num / den;

            // The head, then the streak behind it — drawn back toward the rest position
            // so the trail always points at where the row came from, whichever way it
            // went. Each sample dims, so the line has a direction you can read.
            for (int t = 0; t < trail; ++t) {
                const int x = restX + slide - dir * t * block;
                const uint8_t a =
                    static_cast<uint8_t>((255 - t * 200 / kTrailMax) * rowA / 255);
                for (int by = 0; by < block; ++by)
                    for (int bx = 0; bx < block; ++bx)
                        fb.blendPixel(x + bx, y + by, col, a);
            }
        }
    }
}

} // namespace mal
