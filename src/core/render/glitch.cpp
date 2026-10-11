#include "core/render/glitch.h"

#include "core/render/dissolve.h"
#include "core/render/framebuffer.h"
#include "core/render/sprite.h"

namespace mal {

namespace {

// The glitch quantum, in SOURCE pixels — the malbeast nibble (tools/gen_malbeast_art.py's
// NIB). Every tell works in whole blocks of this so the corruption is as blocky as the body;
// a tear or a lit cell that landed off this grid would read as a finer, cleaner renderer than
// the thing it is corrupting.
constexpr int kBlock = 4;

// A torn band shears by at most this many blocks to either side. Two blocks (8 source px, 14
// dest px at the ×1.75 creature scale) is far enough to read as broken without throwing a
// slice clear of the body — past that it reads as debris rather than as a bad scanline.
constexpr int kMaxTearBlocks = 2;

// Rolls per phase. A band tears, or a cell corrupts, only when its hash clears a threshold,
// so most of the sprite is intact most of the time and the glitch reads as intermittent — a
// signal breaking up, not static. Out of 256.
constexpr uint32_t kTearChance = 70;    // ~27% of bands shear on a given phase
constexpr uint32_t kHotChance = 22;     // ~9% of interior blocks light hot
constexpr uint32_t kDropChance = 10;    // ~4% drop out to a hole

// Phase only re-rolls the corruption every few steps, so it reads as a handful of distinct
// broken frames rather than per-tick noise (which greys out into mush). The hashes seed off
// this coarse clock.
inline uint32_t glitchClock(uint8_t phase) { return phase >> 5; }   // 8 states over 0..255

// Dest-space horizontal shear for the band a source row sits in, this phase. Most bands
// return 0; a torn band returns a whole number of dest blocks, its direction and magnitude
// fixed by the band+clock hash.
int bandTear(int bandRow, uint32_t clock, int block) {
    const uint32_t h = dissolveHash(bandRow, static_cast<int>(clock) ^ 0x6CA9);
    if ((h & 0xFFu) >= kTearChance) return 0;
    const int mag = 1 + static_cast<int>((h >> 8) & 1u);     // 1..kMaxTearBlocks blocks
    const int dir = (h >> 10) & 1u ? 1 : -1;
    return dir * mag * block * kBlock;                       // blocks -> source px -> dest px
}

}  // namespace

void drawSpriteGlitch(Framebuffer& fb, const SpriteData& s, int frame,
                      int destX, int destY, int num, int den,
                      Rgb565 base, Rgb565 hot, uint8_t phase,
                      int row, bool mirror) {
    if (frame < 0 || frame >= s.frames || row < 0 || row >= s.rows) return;
    if (den <= 0 || num <= 0) return;

    const int block = (num + den - 1) / den;          // dest pixels per source pixel
    const int cellW = s.frameW, cellH = s.h;
    if (cellW <= 0 || cellH <= 0) return;

    const uint32_t clock = glitchClock(phase);

    for (int py = 0; py < cellH; ++py) {
        const int bandRow = py / kBlock;
        const int tear = bandTear(bandRow, clock, block);
        // A sheared band carries a one-block hot fringe on the side it is travelling toward —
        // the mis-clocked-scanline colour split. 0 for an untorn band.
        const int ghostDir = tear > 0 ? 1 : (tear < 0 ? -1 : 0);

        for (int px = 0; px < cellW; ++px) {
            const int sx = spriteSrcX(s, frame, px, mirror);
            if (spriteAlphaAt(s, sx, row * cellH + py) < 128) continue;

            const int bx = px / kBlock, by = py / kBlock;
            const uint32_t bh = dissolveHash(bx * 131 + by,
                                             static_cast<int>(clock) ^ 0x1F93);

            // Dropout: this block is a hole in the frame this phase. Checked first so a
            // dropped block shows neither body nor fringe.
            if ((bh & 0xFFu) < kDropChance) continue;

            const bool corrupt = ((bh >> 8) & 0xFFu) < kHotChance;
            const Rgb565 col = corrupt ? hot : base;

            const int dx = destX + px * num / den + tear;
            const int dy = destY + py * num / den;

            // The edge ghost: where a torn band's leading column meets open space, lay a hot
            // block just past it. Cheap tell, only on torn bands, only at the tear edge.
            if (ghostDir != 0 && (px % kBlock == 0)) {
                const int gx = dx + ghostDir * block * kBlock;
                for (int by2 = 0; by2 < block; ++by2)
                    for (int bx2 = 0; bx2 < block; ++bx2)
                        fb.blendPixel(gx + bx2, dy + by2, hot, 90);
            }

            for (int by2 = 0; by2 < block; ++by2)
                for (int bx2 = 0; bx2 < block; ++bx2)
                    fb.blendPixel(dx + bx2, dy + by2, col, 255);
        }
    }
}

}  // namespace mal
