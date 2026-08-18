#include "core/render/absorb.h"

#include "core/render/canvas.h"
#include "core/render/dissolve.h"
#include "core/render/framebuffer.h"
#include "core/render/sprite.h"

namespace mal {

namespace {

// How much of a block's departure time comes from where it sits versus the scatter.
// The positional share is what makes the glyph erode from the edge nearest the pet
// rather than evaporating all over at once; the scatter is what stops that edge from
// being a ruled line marching across.
constexpr int kGradientShare = 3;   // of 4

// Departure times are packed into the first part of the sweep so that even the last
// block to leave still has most of a flight ahead of it. Without the headroom a
// late-departing block would teleport, and at ~4fps a teleport is the whole flight.
constexpr int kDepartSpan = 176;    // of 255

// Blocks fan out before they converge — the swell peaks mid-flight and closes to
// nothing on arrival, so the stream reads as a spray being inhaled rather than as
// pixels sliding down a wire.
constexpr int kFanPx = 16;

// The tail of the flight, where a block drops to a single pixel and fades: the last
// thing seen of it is a fleck, not a square winking out at full size.
constexpr uint8_t kTailFrom = 200;
constexpr uint8_t kTailAlpha = 60;

// A block about to go shivers in place for the beat before it leaves. It is the only
// warning the effect gives that the glyph is coming apart.
constexpr uint8_t kShiverLead = 28;

// The pet's swallow: nothing while the glyph is only crumbling, building as the stream
// closes on it, then decaying once it has landed.
constexpr uint8_t kFlashFrom = 150;
constexpr uint8_t kFlashPeak = 105;
constexpr int kGlowBeats = 2;

// The gap the waiting glyph keeps from the creature's drawn edge.
constexpr int kGlyphGap = 10;

// Ease-out: a block leaves fast and settles into the pet, which reads as being pulled
// in rather than thrown.
int easeOut(int q) { return 255 - (255 - q) * (255 - q) / 255; }

}  // namespace

void drawAbsorb(Framebuffer& fb, const SpriteData& s, int frame,
                int srcX, int srcY, int num, int den,
                int dstX, int dstY, Rgb565 tint, uint8_t progress, uint8_t bite,
                int row) {
    if (frame < 0 || frame >= s.frames || row < 0 || row >= s.rows) return;
    if (den <= 0 || num <= 0) return;

    const int block = (num + den - 1) / den;          // one logical px, on screen
    const int cellW = s.frameW, cellH = s.h;
    if (cellW <= 0 || cellH <= 0) return;

    // The pull direction, as the raw vector from the glyph's centre to the mouth. It
    // only ever orders the departures, so it needs no normalising — the projection is
    // scaled by its own span below.
    const int glyphCx = srcX + cellW * num / den / 2;
    const int glyphCy = srcY + cellH * num / den / 2;
    const int pullX = dstX - glyphCx;
    const int pullY = dstY - glyphCy;
    const int span = cellW / 2 * (pullX < 0 ? -pullX : pullX) +
                     cellH / 2 * (pullY < 0 ? -pullY : pullY);

    for (int py = 0; py < cellH; ++py) {
        for (int px = 0; px < cellW; ++px) {
            if (spriteAlphaAt(s, frame * cellW + px, row * cellH + py) < 128) continue;

            // Each per-block quantity below is a slice of this one scatter, so a block
            // keeps its own departure time, fan and shiver for the whole sweep.
            const uint32_t h = dissolveHash(px, py);
            // A flat ICON_* mask has no colour of its own and takes the tint; a
            // full-colour sprite keeps its pixels, so a creature comes apart into its
            // own blocks rather than into a silhouette of itself.
            const Rgb565 col =
                s.bits ? tint : spriteColorAt(s, frame * cellW + px, row * cellH + py);

            // Departure time: how far down the pull this block sits, blended with its
            // own scatter, then compressed into kDepartSpan.
            int grad = 128;
            if (span > 0) {
                const int proj = (px - cellW / 2) * pullX + (py - cellH / 2) * pullY;
                grad = (span - proj) * 255 / (2 * span);   // 0 = nearest the mouth
                if (grad < 0) grad = 0;
                if (grad > 255) grad = 255;
            }
            // The block's RANK in the queue to leave: its place along the pull blended
            // with its own scatter. `bite` cuts that queue — a block ranked past it is
            // simply not part of this mouthful and never moves.
            const int scatter = static_cast<int>(h & 0xFFu);
            const int rank = (grad * kGradientShare + scatter) / (kGradientShare + 1);
            const int depart = rank * kDepartSpan / 255;

            // Where the block sits if it had never moved.
            const int restX = srcX + px * num / den;
            const int restY = srcY + py * num / den;

            if (rank >= bite || progress < depart) {
                // Still part of the glyph. A block that IS going shivers a pixel in the
                // stretch before its turn — the only warning the effect gives that the
                // glyph is coming apart. One staying put holds perfectly still, which
                // is what makes a declined glyph read as declined.
                int jx = 0, jy = 0;
                if (rank < bite && progress + kShiverLead > depart) {
                    jx = static_cast<int>((h >> 8) & 1u) - static_cast<int>((h >> 9) & 1u);
                    jy = static_cast<int>((h >> 10) & 1u) - static_cast<int>((h >> 11) & 1u);
                }
                fb.fillRect(restX + jx, restY + jy, block, block, col);
                continue;
            }

            // In flight. Normalising against the block's own remaining sweep lands the
            // whole stream together at progress 255 — the glyph comes apart raggedly
            // and is swallowed in one beat, which is the shape of a bite.
            const int room = 255 - depart;
            const int q = room > 0 ? (progress - depart) * 255 / room : 255;
            if (q >= 255) continue;                    // eaten
            const int e = easeOut(q);

            const int fan = (q * (255 - q)) / 255;     // 0 at both ends, peak mid-flight
            const int fanX = (static_cast<int>((h >> 12) & 31u) - 16) * fan * kFanPx / (64 * 16);
            const int fanY = (static_cast<int>((h >> 17) & 31u) - 16) * fan * kFanPx / (64 * 16);

            const int x = restX + (dstX - restX) * e / 255 + fanX;
            const int y = restY + (dstY - restY) * e / 255 + fanY;

            const int bs = q > kTailFrom ? 1 : block;
            const uint8_t a =
                q > kTailFrom
                    ? static_cast<uint8_t>(255 - (q - kTailFrom) * (255 - kTailAlpha) /
                                                     (255 - kTailFrom))
                    : 255;
            for (int by = 0; by < bs; ++by)
                for (int bx = 0; bx < bs; ++bx) fb.blendPixel(x + bx, y + by, col, a);
        }
    }
}

void drawPetAbsorbing(Framebuffer& fb, const SpriteData* pet, int petCx, int petCy,
                      const SpriteData* glyph, Rgb565 tint, AbsorbPhase phase,
                      int beat) {
    const int petW = pet ? pet->frameW * kScaleNum / kScaleDen : 0;
    const int petH = pet ? pet->h * kScaleNum / kScaleDen : 0;
    const int petX = petCx - petW / 2;
    const int petY = petCy - petH / 2;

    if (glyph) {
        // The stream aims at the middle of the creature's DRAWING, not of its cell: a
        // 24-wide malbeast in a 56-wide cell would otherwise be fed somewhere off to
        // one side of itself. Horizontally that is the sheet's content span; vertically
        // the cell is drawn to fit, so its own centre is the body's.
        const int bodyX0 =
            pet ? petX + spriteContentX0(*pet) * kScaleNum / kScaleDen : petX;
        const int bodyX1 =
            pet ? petX + spriteContentX1(*pet) * kScaleNum / kScaleDen : petX;
        const int mouthX = (bodyX0 + bodyX1) / 2;
        const int mouthY = petCy;

        // Seated off the drawn left edge, and never past the canvas edge — a glyph
        // pushed off-screen by a wide creature would take its first departures with it.
        const int glyphW = glyph->frameW * kScaleNum / kScaleDen;
        const int glyphH = glyph->h * kScaleNum / kScaleDen;
        int glyphX = bodyX0 - kGlyphGap - glyphW;
        if (glyphX < 2) glyphX = 2;

        // Under the pet, so a block that reaches the body passes behind it and is gone.
        // Landing them on TOP would read as the creature wearing its dinner.
        drawAbsorb(fb, *glyph, 0, glyphX, mouthY - glyphH / 2, kScaleNum, kScaleDen,
                   mouthX, mouthY, tint, phase.progress, phase.bite);
    }

    if (pet)
        drawSpriteFlash(fb, *pet, idleFrame(*pet, beat), petX, petY, kScaleNum,
                        kScaleDen, tint, phase.flash);
}

AbsorbPhase absorbPhase(int beat, int startBeat, int beats, uint8_t bite) {
    if (beats <= 0) return {255, bite, 0};
    const int b = beat - startBeat;
    if (b <= 0) return {0, bite, 0};

    // The swallow is scaled by the size of the mouthful, so a nibble flashes less than
    // a whole glyph and a bite of 0 never lights the pet up at all.
    const int amp = kFlashPeak * bite / 255;

    if (b >= beats) {
        // Squared decay: the beat the stream lands on is the flash, and the one after
        // is an ember. A linear falloff over this few beats leaves the creature still
        // visibly the wrong colour a whole beat later.
        const int after = b - beats;
        const int left = kGlowBeats - after;
        const uint8_t glow =
            left <= 0 ? 0
                      : static_cast<uint8_t>(amp * left * left / (kGlowBeats * kGlowBeats));
        return {255, bite, glow};
    }

    const int p = b * 255 / beats;
    const int f = p < kFlashFrom ? 0 : amp * (p - kFlashFrom) / (255 - kFlashFrom);
    return {static_cast<uint8_t>(p), bite, static_cast<uint8_t>(f)};
}

} // namespace mal
