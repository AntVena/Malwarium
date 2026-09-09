#include "core/render/scenes/draws.h"

#include "core/render/canvas.h"
#include "core/render/framebuffer.h"
#include "core/render/palette.h"

namespace mal {

namespace {

// NULL POINTE — a headland at sunset, seen from the water.
//
// The middle band is ten rows on both screens (sceneGround) and a scene cannot widen its
// own, so the sea is the NEAR band and the land is the far strip: the sun's road needs
// somewhere to run, and that is the whole composition.

constexpr Pal kSky = Pal::FRAG_LO;
constexpr Pal kHot = Pal::FRAG_HI;      // the sun, and everything it lights
constexpr Pal kSea = Pal::NEON_LO;
constexpr Pal kFoam = Pal::NEON_HI;

constexpr uint8_t kToneHaze = 32;
constexpr uint8_t kToneStar = 48;
constexpr uint8_t kToneSun = 132;
constexpr uint8_t kToneHead = 48;       // the headland — a landform, not a smudge
constexpr uint8_t kToneWater = 34;
constexpr uint8_t kToneSwell = 58;
constexpr uint8_t kTonePath = 118;      // the sun's road down the water
constexpr uint8_t kToneSurf = 54;       // the lip where the water meets the land

constexpr uint8_t kStars[][2] = {
    {18, 232}, {44, 190}, {70, 246}, {97, 204}, {118, 170},
    {140, 238}, {164, 196}, {188, 226}, {206, 182}, {220, 214},
};

// On the horizon, and cut by the headland in front of it.
constexpr int kSunX = 128;
constexpr uint8_t kSunUp = 26;
constexpr int kSunR = 30;
constexpr int kSunBites = 4;
constexpr int kSunBiteTop = 1;
constexpr int kSunBiteGrow = 1;
constexpr int kSunBiteGap = 5;

// A headland running off the left and a second, lower one to the right of the sun.
constexpr uint8_t kHeadland[] = {30, 38, 46, 40, 30, 24, 18, 12, 8, 5, 3, 0,
                                 0,  0,  0,  0,  0,  0,  4,  9, 14, 18, 14, 11};
constexpr int kHeadN = static_cast<int>(sizeof(kHeadland) / sizeof(kHeadland[0]));

constexpr int kSwellPitch = 4;
constexpr int kPathSpread = 3;          // how fast the sun's road widens, in 8ths
constexpr int kPathPitch = 3;
constexpr int kSurfDash = 5;            // the lip is broken, so it is a shore not a rule

}  // namespace

void drawNullPointeScene(Framebuffer& fb, int beat, const SceneGround& g) {
    fb.clear(palColor(Pal::PAPER));

    sceneSpecks(fb, kStars, static_cast<int>(sizeof(kStars) / sizeof(kStars[0])), g,
                kToneStar, kSky);

    const Rgb565 cut = palColor(Pal::PAPER);
    const int sunY = sceneSkyY(g, kSunUp);
    sceneDisc(fb, kSunX, sunY, kSunR, kToneSun, kHot);
    int y = sunY - kSunR + kSunBiteGap;
    for (int i = 0; i < kSunBites && y < sunY + kSunR; ++i) {
        const int h = kSunBiteTop + i * kSunBiteGrow;
        fb.fillRect(kSunX - kSunR, y, kSunR * 2, h, cut);
        y += h + kSunBiteGap;
    }
    if (g.horizonY < kActiveH)
        fb.fillRect(0, g.horizonY, kActiveW, kActiveH - g.horizonY, cut);

    // Two lifts rather than one: the sky over a sunset is banded, and one band of glow
    // above a big empty field reads as a gradient nobody finished.
    sceneGlow(fb, g, /*up=*/34, kToneHaze, kSky);
    sceneGlow(fb, g, /*up=*/14, kToneHaze + 14, kHot);
    sceneSilhouette(fb, kHeadland, kHeadN, g.horizonY, kToneHead, {}, kSky);

    const Rgb565 water = sceneTint(kToneWater, kSea);
    const Rgb565 swell = sceneTint(kToneSwell, kSea);
    const Rgb565 path = sceneTint(kTonePath, kHot);
    const int waterBottom = kActiveH;

    fb.fillRect(0, g.horizonY, kActiveW, waterBottom - g.horizonY, water);
    for (int wy = g.horizonY + 1; wy < waterBottom; wy += kSwellPitch) {
        const int drift = sceneDrift(beat, wy, kSwellPitch * 2);
        for (int x = drift % 6; x < kActiveW; x += 6) fb.fillRect(x, wy, 3, 1, swell);
    }

    // The sun's road: a column of broken light widening toward the viewer.
    for (int py = g.horizonY + 2; py < waterBottom; py += kPathPitch) {
        const int half = 4 + (py - g.horizonY) * kPathSpread / 8;
        const int x0 = kSunX - half, x1 = kSunX + half;
        const int step = 3 + ((py / kPathPitch + beat) % 3);
        for (int x = x0; x < x1; x += step)
            fb.fillRect(x < 0 ? 0 : x, py, 2, 1, path);
    }

    // The lip where the water meets the land, broken rather than ruled.
    const Rgb565 surf = sceneTint(kToneSurf, kSea);
    for (int x = sceneDrift(beat, 1, kSurfDash * 2) % kSurfDash; x < kActiveW;
         x += kSurfDash)
        fb.fillRect(x, g.horizonY, 3, 1, surf);
}

}  // namespace mal
