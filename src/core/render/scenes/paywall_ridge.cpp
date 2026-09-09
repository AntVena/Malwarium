#include "core/render/scenes/draws.h"

#include "core/render/canvas.h"
#include "core/render/framebuffer.h"
#include "core/render/palette.h"

namespace mal {

namespace {

// PAYWALL RIDGE — a wild wireframe range under a slit sun.
//
// Cool land, hot sky: the range and the grid are the same blue-green surface, and the
// one warm thing in the picture is the sun behind it.
constexpr Pal kLand = Pal::NEON_HI;     // acid green — the lines
constexpr Pal kDeep = Pal::NEON_LO;     // electric blue — the faces between them
constexpr Pal kSun = Pal::FRAG_HI;

constexpr uint8_t kToneHaze = 26;
constexpr uint8_t kToneStar = 54;
constexpr uint8_t kToneSun = 128;
constexpr uint8_t kToneFace = 30;       // under the ridge, so nothing shows through it
constexpr uint8_t kToneRidge = 148;
constexpr uint8_t kToneContour = 96;
constexpr uint8_t kToneSpur = 66;
constexpr uint8_t kToneGrid = 124;

// Control points the ridge is interpolated between — x, then height above the horizon.
// Asymmetric on purpose; the low run at 96..126 is the pass.
struct Peak { int16_t x; uint8_t h; };
constexpr Peak kRidge[] = {
    {0, 34},   {14, 74},  {26, 44},  {44, 122}, {58, 52},  {72, 90},
    {84, 38},  {96, 26},  {112, 20}, {126, 30}, {138, 108}, {150, 46},
    {166, 136}, {180, 58}, {196, 96}, {210, 50}, {223, 82},
};
constexpr int kRidgeN = static_cast<int>(sizeof(kRidge) / sizeof(kRidge[0]));

// Contours are the ridge profile again at a fraction of its height, so they run WITH
// the terrain. Ruled straight across, they would be graph paper behind a silhouette.
constexpr int kContours = 5;
constexpr int kRidgeH = 2;

constexpr uint8_t kStars[][2] = {
    {12, 228}, {40, 186}, {63, 244}, {88, 202}, {109, 168},
    {131, 236}, {153, 194}, {182, 222}, {202, 178}, {217, 210},
};

// Above the range, and cut by the range in front of it.
constexpr int kSunX = 112;
constexpr uint8_t kSunUp = 82;
constexpr int kSunR = 26;
constexpr int kSunBites = 4;
constexpr int kSunBiteTop = 1;          // rows cut out of the highest bite
constexpr int kSunBiteGrow = 1;         // ...and one more per bite going down
constexpr int kSunBiteGap = 5;

constexpr int kGridCols = 9;

// The ridge height over column `x`, interpolated between the control points either side.
int ridgeH(int x) {
    int i = 0;
    while (i + 1 < kRidgeN - 1 && kRidge[i + 1].x <= x) ++i;
    const Peak& a = kRidge[i];
    const Peak& b = kRidge[i + 1];
    const int span = b.x - a.x;
    return span <= 0 ? a.h : a.h + (b.h - a.h) * (x - a.x) / span;
}

}  // namespace

void drawPaywallRidgeScene(Framebuffer& fb, int beat, const SceneGround& g) {
    fb.clear(palColor(Pal::PAPER));

    sceneSpecks(fb, kStars, static_cast<int>(sizeof(kStars) / sizeof(kStars[0])), g,
                kToneStar, kDeep);

    // The sun, drawn whole and then striped: the bites widen downward, which is the
    // venetian read — even ones would be a beach ball.
    const Rgb565 cut = palColor(Pal::PAPER);
    const int sunY = sceneSkyY(g, kSunUp);
    sceneDisc(fb, kSunX, sunY, kSunR, kToneSun, kSun);
    int y = sunY - kSunR + kSunBiteGap;
    for (int i = 0; i < kSunBites && y < sunY + kSunR; ++i) {
        const int h = kSunBiteTop + i * kSunBiteGrow;
        fb.fillRect(kSunX - kSunR, y, kSunR * 2, h, cut);
        y += h + kSunBiteGap;
    }
    if (g.horizonY < kActiveH)
        fb.fillRect(0, g.horizonY, kActiveW, kActiveH - g.horizonY, cut);

    sceneGlow(fb, g, /*up=*/26, kToneHaze, kDeep);

    // The faces first, so the range is opaque, then the mesh over them.
    const Rgb565 face = sceneTint(kToneFace, kDeep);
    for (int x = 0; x < kActiveW; ++x) {
        const int h = ridgeH(x);
        if (h > 0) fb.fillRect(x, g.horizonY - h, 1, h, face);
    }

    const Rgb565 contour = sceneTint(kToneContour, kDeep);
    for (int k = 1; k <= kContours; ++k) {
        int was = -1;
        for (int x = 0; x < kActiveW; ++x) {
            const int cy = g.horizonY - ridgeH(x) * (kContours - k + 1) / (kContours + 1);
            const int lo = was < 0 || cy < was ? cy : was;
            const int hi = was < 0 || cy > was ? cy : was;
            fb.fillRect(x, lo, 1, hi - lo + 1, contour);
            was = cy;
        }
    }

    // A spur down from every peak and valley: the edge where two faces meet, which is
    // what makes the range polygonal rather than striped.
    const Rgb565 spur = sceneTint(kToneSpur, kLand);
    for (const Peak& p : kRidge) {
        const int x = p.x >= kActiveW ? kActiveW - 1 : p.x;
        if (p.h > 0) fb.fillRect(x, g.horizonY - p.h, 1, p.h, spur);
    }

    const Rgb565 ridge = sceneTint(kToneRidge, kLand);
    for (int x = 0; x < kActiveW; ++x) {
        const int y0 = g.horizonY - ridgeH(x);
        const int was = x == 0 ? y0 : g.horizonY - ridgeH(x - 1);
        const int lo = y0 < was ? y0 : was, hi = y0 < was ? was : y0;
        fb.fillRect(x, lo, 1, hi - lo + kRidgeH, ridge);
    }

    sceneGrid(fb, g, kGridCols, beat, kToneGrid, kLand);
}

}  // namespace mal
