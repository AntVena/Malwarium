#include "core/render/scene.h"

#include "core/render/canvas.h"
#include "core/render/framebuffer.h"
#include "core/render/palette.h"

namespace mal {

namespace {

// Which tokens a backdrop may be anchored to, as a list of what is ALLOWED rather than
// a list of what is not. A blacklist decides the question for tokens that exist today
// and quietly admits every one added afterwards; this way a new PAL_CORE role has to be
// let in on purpose. `ink-dim` is the value ramp every scene that reads in grey uses,
// the fragmentation pair is the purple-to-pink no interface state has claimed, and
// `track` is the chrome grey — dim enough to sit under text by construction.
constexpr Pal kSceneAnchors[] = {Pal::INK_DIM, Pal::FRAG_LO, Pal::FRAG_HI, Pal::TRACK};

bool anchorAllowed(Pal p) {
    for (Pal a : kSceneAnchors)
        if (a == p) return true;
    return false;
}

// The grid's own two counts. Both are fixed rather than passed: `cols` is the thing an
// author actually varies between a wide aisle and a tight one, and the number of
// horizontals is a property of how far the eye reads depth on a panel this size.
constexpr int kGridRows = 8;
constexpr int kGridPhaseSteps = 8;   // sub-steps per row, so the creep is smooth

}  // namespace

Rgb565 sceneCeiling() {
    Rgb565 top = sceneTint(kSceneWideCeiling, kSceneAnchors[0]);
    for (Pal a : kSceneAnchors) {
        const Rgb565 c = sceneTint(kSceneWideCeiling, a);
        if (luminance(c) > luminance(top)) top = c;
    }
    return top;
}

Rgb565 sceneTint(uint8_t t, Pal anchor) {
    if (!anchorAllowed(anchor)) anchor = Pal::INK_DIM;
    return blend(palColor(Pal::PAPER), palColor(anchor), t);
}

int sceneSkyY(const SceneGround& g, uint8_t up) {
    if (g.horizonY <= 0) return 0;
    return g.horizonY - g.horizonY * up / 256;
}

void sceneGlow(Framebuffer& fb, const SceneGround& g, uint8_t up, uint8_t tone,
               Pal anchor) {
    const int h = (g.horizonY - sceneSkyY(g, up)) / 4;
    if (h <= 0) return;
    const int top = g.horizonY - h * 4;
    for (int i = 0; i < 4; ++i)
        fb.fillRect(0, top + i * h, kActiveW, h,
                    sceneTint(static_cast<uint8_t>(tone * (i + 1) / 4), anchor));
}

void sceneSpecks(Framebuffer& fb, const uint8_t (*pts)[2], int n, const SceneGround& g,
                 uint8_t tone, Pal anchor) {
    const Rgb565 c = sceneTint(tone, anchor);
    for (int i = 0; i < n; ++i) fb.fillRect(pts[i][0], sceneSkyY(g, pts[i][1]), 1, 1, c);
}

void sceneDisc(Framebuffer& fb, int cx, int cy, int r, uint8_t tone, Pal anchor) {
    const Rgb565 c = sceneTint(tone, anchor);
    for (int dy = -r + 1; dy < r; ++dy) {
        int w = 0;
        while ((w + 1) * (w + 1) + dy * dy <= r * r) ++w;
        fb.fillRect(cx - w, cy + dy, w * 2 + 1, 1, c);
    }
}

void sceneSilhouette(Framebuffer& fb, const uint8_t* heights, int n, int baseY,
                     uint8_t tone, SceneSpan span, Pal anchor) {
    if (n <= 0) return;
    const Rgb565 c = sceneTint(tone, anchor);
    // Column edges are taken from the span rather than from a per-column width, so a
    // table of any length tiles it exactly instead of leaving a gap at the right.
    for (int i = 0; i < n; ++i) {
        const int x0 = span.x + i * span.w / n, x1 = span.x + (i + 1) * span.w / n;
        fb.fillRect(x0, baseY - heights[i], x1 - x0, heights[i], c);
    }
}

void sceneOverhang(Framebuffer& fb, const uint8_t* depths, int n, int baseY,
                   uint8_t tone, SceneSpan span, Pal anchor) {
    if (n <= 0) return;
    const Rgb565 c = sceneTint(tone, anchor);
    for (int i = 0; i < n; ++i) {
        const int x0 = span.x + i * span.w / n, x1 = span.x + (i + 1) * span.w / n;
        fb.fillRect(x0, baseY, x1 - x0, depths[i], c);
    }
}

void sceneMiddle(Framebuffer& fb, const SceneGround& g, uint8_t tone, Pal anchor) {
    fb.fillRect(0, g.horizonY, kActiveW, g.floorY - g.horizonY, sceneTint(tone, anchor));
}

void sceneFloor(Framebuffer& fb, const SceneGround& g, int seamPitch, uint8_t fill,
                uint8_t seam, uint8_t edge, Pal anchor) {
    const int h = kActiveH - g.floorY;
    fb.fillRect(0, g.floorY, kActiveW, h, sceneTint(fill, anchor));
    fb.fillRect(0, g.floorY, kActiveW, 1, sceneTint(edge, anchor));
    if (seamPitch <= 0) return;
    const Rgb565 s = sceneTint(seam, anchor);
    for (int x = seamPitch / 2; x < kActiveW; x += seamPitch)
        fb.fillRect(x, g.floorY + 1, 1, h - 1, s);
}

void scenePost(Framebuffer& fb, int x, int h, const SceneGround& g, uint8_t tone,
               Pal anchor) {
    const Rgb565 c = sceneTint(tone, anchor);
    fb.fillRect(x, g.floorY - h, 5, h, c);
    fb.fillRect(x - 1, g.floorY - h - 3, 7, 3, c);   // the cap
}

int sceneConverge(const SceneGround& g, int floorX, int y) {
    const int vanishX = kActiveW / 2;
    const int span = kActiveH - g.horizonY;
    if (span <= 0) return vanishX;
    if (y <= g.horizonY) return vanishX;
    // Straight lines to a single point: the fan is linear in the row, which is what
    // one-point perspective actually is once the picture plane is the panel.
    return vanishX + (floorX - vanishX) * (y - g.horizonY) / span;
}

void sceneGrid(Framebuffer& fb, const SceneGround& g, int cols, int phase, uint8_t tone,
               Pal anchor) {
    const Rgb565 c = sceneTint(tone, anchor);
    const int span = kActiveH - g.horizonY;
    if (span <= 0 || cols < 2) return;

    // The verticals leave the foot of the canvas across twice its width, so the outer
    // few run off the sides on the way down and the fan reads as wider than the screen
    // rather than as a wedge that happens to fit in it.
    //
    // Each row draws the RUN between where the line is and where it will be one row
    // down, not a single pixel: an outer line crosses several columns per row by the
    // time it reaches the foot, and a pixel per row would leave it a dotted trail.
    for (int i = 0; i < cols; ++i) {
        const int floorX = -kActiveW / 2 + i * (kActiveW * 2) / (cols - 1);
        for (int y = g.horizonY; y < kActiveH; ++y) {
            const int a = sceneConverge(g, floorX, y);
            const int b = sceneConverge(g, floorX, y + 1);
            const int x0 = a < b ? a : b, x1 = a < b ? b : a;
            fb.fillRect(x0, y, x1 - x0 + 1, 1, c);
        }
    }

    // The horizontals, tightening toward the horizon: the row is quadratic in the step,
    // which is the cheapest curve that puts most of the lines in the far half. `phase`
    // walks every one of them one sub-step nearer, and the modulo is what makes the
    // nearest line leaving the bottom the same event as a new one leaving the horizon.
    const int steps = kGridRows * kGridPhaseSteps;
    for (int k = 1; k <= kGridRows; ++k) {
        const int u = (k * kGridPhaseSteps + phase) % steps;
        fb.fillRect(0, g.horizonY + span * u * u / (steps * steps), kActiveW, 1, c);
    }
}

int sceneDrift(int beat, int i, int span) {
    if (span <= 0) return 0;
    // Alternating direction by index, so a field of drifting elements reads as a
    // surface moving rather than as a row of bars all sliding the same way. The phase
    // offset keeps neighbours from arriving at the same mark on the same beat.
    return ((i & 1 ? beat : -beat) + i * 5) % span;
}

}  // namespace mal
