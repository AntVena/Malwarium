#include "core/render/scenes/draws.h"

#include "core/render/canvas.h"
#include "core/render/framebuffer.h"
#include "core/render/palette.h"

namespace mal {

namespace {

// NEON SUBNET — a city block standing in a wet grid.
//
// Three anchors, split by what they are: the city is the frag ramp, the road under it is
// the neon blue, and the lights are the pink. Warm windows over a cold street is the
// separation the picture was missing when the grid and the lit windows were one hue.
constexpr Pal kFar = Pal::FRAG_LO;
constexpr Pal kNear = Pal::FRAG_HI;
constexpr Pal kRoad = Pal::NEON_LO;

constexpr uint8_t kToneHaze = 34;
constexpr uint8_t kToneStar = 56;
constexpr uint8_t kToneMoon = 118;
constexpr uint8_t kToneBack = 40;
constexpr uint8_t kToneFront = 68;
constexpr uint8_t kToneWindow = 196;
constexpr uint8_t kToneSign = 150;
constexpr uint8_t kToneStreet = 16;
constexpr uint8_t kToneMirror = 62;
constexpr uint8_t kToneMirrorWin = 128;
constexpr uint8_t kToneGrid = 118;
constexpr uint8_t kToneRipple = 8;
constexpr uint8_t kToneAsphalt = 22;
constexpr uint8_t kToneSeam = 30;
constexpr uint8_t kToneKerb = 96;

// The front rank's gaps are where a screen's sprite stands.
constexpr uint8_t kBackRank[] = {18, 18, 18, 24, 24, 24, 14, 14, 20, 20, 20, 28, 28,
                                 28, 28, 16, 16, 22, 22, 22, 19, 19, 26, 26, 17, 17,
                                 17, 23, 23, 23, 15, 15};
constexpr uint8_t kFrontRank[] = {44, 44, 44, 0, 0, 0, 0, 58, 58, 58, 58, 0, 0, 0,
                                  0, 0, 0, 36, 36, 36, 0, 0, 0, 0, 52, 52, 52, 52,
                                  0, 0, 40, 40};
constexpr int kBackN = static_cast<int>(sizeof(kBackRank) / sizeof(kBackRank[0]));
constexpr int kFrontN = static_cast<int>(sizeof(kFrontRank) / sizeof(kFrontRank[0]));

struct Tower { uint8_t x, w, h, cols, rows; };
constexpr Tower kTowers[] = {
    {2, 20, 44, 3, 4}, {50, 28, 58, 4, 5}, {119, 20, 36, 3, 3}, {168, 28, 52, 4, 4},
};
constexpr int kWinPitchX = 6;
constexpr int kWinPitchY = 8;
constexpr int kWinInset = 3;
constexpr int kDarkEveryN = 3;      // ...of a tower's windows, walked by the beat
constexpr int kSignH = 2;           // the parapet strip, on the two tall towers
constexpr int kSignMinH = 50;

constexpr uint8_t kStars[][2] = {
    {14, 210}, {37, 168}, {58, 232}, {83, 190}, {104, 244},
    {129, 176}, {151, 220}, {176, 186}, {198, 238}, {214, 200},
};

constexpr int kMoonX = 152;
constexpr uint8_t kMoonUp = 150;
constexpr int kMoonR = 13;

constexpr int kGridCols = 9;
constexpr int kRipplePitch = 11;
constexpr int kRippleH = 1;
// A reflection is foreshortened rather than mirrored: two fifths keeps it off the foot
// of the canvas on the deeper of the two grounds.
constexpr int kMirrorNum = 2, kMirrorDen = 5;

// What sceneSilhouette does internally to draw a rank; the reflection asks it to mirror
// one, so the two halves stay in step when a table is edited.
int rankHeightAt(const uint8_t* heights, int n, int x) {
    int i = x * n / kActiveW;
    if (i < 0) i = 0;
    if (i >= n) i = n - 1;
    return heights[i];
}

}  // namespace

void drawNeonSubnetScene(Framebuffer& fb, int beat, const SceneGround& g) {
    fb.clear(palColor(Pal::PAPER));

    sceneSpecks(fb, kStars, static_cast<int>(sizeof(kStars) / sizeof(kStars[0])), g,
                kToneStar, kFar);
    sceneDisc(fb, kMoonX, sceneSkyY(g, kMoonUp), kMoonR, kToneMoon, kNear);
    sceneGlow(fb, g, /*up=*/30, kToneHaze, kFar);

    sceneSilhouette(fb, kBackRank, kBackN, g.horizonY, kToneBack, {}, kFar);
    sceneSilhouette(fb, kFrontRank, kFrontN, g.horizonY, kToneFront, {}, kFar);

    const Rgb565 win = sceneTint(kToneWindow, kNear);
    const Rgb565 sign = sceneTint(kToneSign, kNear);
    for (const Tower& t : kTowers) {
        const int top = g.horizonY - t.h;
        if (t.h >= kSignMinH) fb.fillRect(t.x, top, t.w, kSignH, sign);
        for (int r = 0; r < t.rows; ++r) {
            for (int c = 0; c < t.cols; ++c) {
                const int wx = t.x + kWinInset + c * kWinPitchX;
                const int wy = top + kWinInset + kSignH + r * kWinPitchY;
                if (wy + 2 >= g.horizonY || wx + 2 >= t.x + t.w) continue;
                if ((r * t.cols + c + beat + t.x) % kDarkEveryN == 0) continue;
                fb.fillRect(wx, wy, 2, 2, win);
            }
        }
    }

    sceneMiddle(fb, g, kToneStreet, kFar);
    sceneFloor(fb, g, /*seamPitch=*/72, kToneAsphalt, kToneSeam, kToneKerb, kFar);

    const Rgb565 mirror = sceneTint(kToneMirror, kFar);
    for (int x = 0; x < kActiveW; ++x) {
        const int h = rankHeightAt(kFrontRank, kFrontN, x);
        if (h <= 0) continue;
        const int depth = h * kMirrorNum / kMirrorDen;
        for (int d = 0; d < depth; ++d) {
            const int y = g.horizonY + d;
            if (y >= kActiveH) break;
            fb.set(x, y, mirror);
        }
    }

    sceneGrid(fb, g, kGridCols, beat, kToneGrid, kRoad);

    // One streak per column of windows, its length walked by the beat.
    const Rgb565 mirrorWin = sceneTint(kToneMirrorWin, kNear);
    for (const Tower& t : kTowers) {
        const int depth =
            rankHeightAt(kFrontRank, kFrontN, t.x + t.w / 2) * kMirrorNum / kMirrorDen;
        for (int c = 0; c < t.cols; ++c) {
            const int wx = t.x + kWinInset + c * kWinPitchX;
            if (wx + 2 >= t.x + t.w) continue;
            const int len = depth - 2 - ((beat + c + t.x / 8) % 3) * 2;
            for (int d = 2; d < len; ++d) {
                const int y = g.horizonY + d;
                if (y >= kActiveH) break;
                fb.fillRect(wx, y, 2, 1, mirrorWin);
            }
        }
    }


    const Rgb565 ripple = sceneTint(kToneRipple, kFar);
    const int phase = sceneDrift(beat, 0, kRipplePitch);
    for (int y = g.horizonY + 2 + phase; y < kActiveH; y += kRipplePitch)
        fb.fillRect(0, y, kActiveW, kRippleH, ripple);
}

}  // namespace mal
