#include "core/render/scene.h"

#include "core/render/canvas.h"
#include "core/render/framebuffer.h"
#include "core/render/palette.h"

namespace mal {

Rgb565 sceneTone(uint8_t t) {
    return blend(palColor(Pal::PAPER), palColor(Pal::INK_DIM), t);
}

void sceneGlow(Framebuffer& fb, int top, const SceneGround& g, uint8_t tone) {
    const int h = (g.horizonY - top) / 4;
    if (h <= 0) return;
    for (int i = 0; i < 4; ++i)
        fb.fillRect(0, top + i * h, kActiveW, h,
                    sceneTone(static_cast<uint8_t>(tone * (i + 1) / 4)));
}

void sceneSpecks(Framebuffer& fb, const uint8_t (*pts)[2], int n, uint8_t tone) {
    const Rgb565 c = sceneTone(tone);
    for (int i = 0; i < n; ++i) fb.fillRect(pts[i][0], pts[i][1], 1, 1, c);
}

void sceneDisc(Framebuffer& fb, int cx, int cy, int r, uint8_t tone) {
    const Rgb565 c = sceneTone(tone);
    for (int dy = -r + 1; dy < r; ++dy) {
        int w = 0;
        while ((w + 1) * (w + 1) + dy * dy <= r * r) ++w;
        fb.fillRect(cx - w, cy + dy, w * 2 + 1, 1, c);
    }
}

void sceneSilhouette(Framebuffer& fb, const uint8_t* heights, int n,
                     const SceneGround& g, uint8_t tone) {
    if (n <= 0) return;
    const Rgb565 c = sceneTone(tone);
    const int w = kActiveW / n;
    for (int i = 0; i < n; ++i)
        fb.fillRect(i * w, g.horizonY - heights[i], w, heights[i], c);
}

void sceneMiddle(Framebuffer& fb, const SceneGround& g, uint8_t tone) {
    fb.fillRect(0, g.horizonY, kActiveW, g.floorY - g.horizonY, sceneTone(tone));
}

void sceneFloor(Framebuffer& fb, const SceneGround& g, int seamPitch, uint8_t fill,
                uint8_t seam, uint8_t edge) {
    const int h = kActiveH - g.floorY;
    fb.fillRect(0, g.floorY, kActiveW, h, sceneTone(fill));
    fb.fillRect(0, g.floorY, kActiveW, 1, sceneTone(edge));
    if (seamPitch <= 0) return;
    const Rgb565 s = sceneTone(seam);
    for (int x = seamPitch / 2; x < kActiveW; x += seamPitch)
        fb.fillRect(x, g.floorY + 1, 1, h - 1, s);
}

void scenePost(Framebuffer& fb, int x, int h, const SceneGround& g, uint8_t tone) {
    const Rgb565 c = sceneTone(tone);
    fb.fillRect(x, g.floorY - h, 5, h, c);
    fb.fillRect(x - 1, g.floorY - h - 3, 7, 3, c);   // the cap
}

int sceneDrift(int beat, int i, int span) {
    if (span <= 0) return 0;
    // Alternating direction by index, so a field of drifting elements reads as a
    // surface moving rather than as a row of bars all sliding the same way. The phase
    // offset keeps neighbours from arriving at the same mark on the same beat.
    return ((i & 1 ? beat : -beat) + i * 5) % span;
}

}  // namespace mal
