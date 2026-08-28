#include "core/render/scenes/draws.h"

#include "core/render/canvas.h"
#include "core/render/framebuffer.h"
#include "core/render/palette.h"

namespace mal {

namespace {

// TRACE CITY — a board read as a skyline, and the prize a hundred thousand steps pays
// out. Chip packages and capacitor towers standing on a horizon, their pin rows lit
// like windows, and copper routed across the ground in front of them.
//
// WHY IT IS NOT A GRID. The other place made of circuitry (grid_horizon.cpp) is a
// receding grid under a sunset, and a second one of those would be the same picture
// twice. What makes a BOARD look like a board is not perspective, it is the ROUTING:
// right angles, chamfered corners, and a via wherever a run stops. So the ground here
// is orthogonal rather than convergent, and the identity is in the silhouette above it.
//
// Pure value, and it has to be: a board's own copper is a colour the palette has no
// token for, and faking one with `warn` is exactly what sceneTint refuses.
constexpr uint8_t kToneHaze = 28;        // the lift off the city, such as it is
constexpr uint8_t kToneDust = 74;        // specks — a board's flux haze, not stars
constexpr uint8_t kToneFar = 42;         // the back rank of packages
constexpr uint8_t kToneNear = 62;        // the front rank
constexpr uint8_t kTonePin = 88;         // the pin rows down a package's side
constexpr uint8_t kToneWindow = 178;     // a lit one
constexpr uint8_t kToneCap = 56;         // an electrolytic tower
constexpr uint8_t kToneCapBand = 110;    // its polarity stripe
constexpr uint8_t kToneMask = 40;        // the solder mask underfoot
constexpr uint8_t kToneSeam = 30;
constexpr uint8_t kToneKerb = 116;
constexpr uint8_t kToneTrace = 96;       // copper routed across it
constexpr uint8_t kToneVia = 172;        // ...and a via where a run ends

// Two ranks of packages, one behind the other, so the skyline has a depth that costs
// nothing but a second table. The BACK rank is even and dense (a bank of identical
// parts); the FRONT is sparse and tall with real gaps in it, and the gaps are where a
// screen's sprite stands.
constexpr uint8_t kBackRank[] = {12, 12, 12, 16, 16, 16, 10, 10, 14, 14, 14, 20, 20,
                                 20, 20, 11, 11, 15, 15, 15, 13, 13, 18, 18, 12, 12,
                                 12, 17, 17, 17, 10, 10};
constexpr uint8_t kFrontRank[] = {0, 0, 34, 34, 34, 34, 0, 0, 0, 22, 22, 22, 0, 0,
                                  0, 0, 0, 0, 28, 28, 28, 28, 28, 0, 0, 0, 40, 40,
                                  40, 40, 0, 0};

// The lit pin rows. A package's windows are its pins, so they come in EVEN PAIRS down
// the two sides of a body rather than scattered across its face — which is the one
// thing that tells a chip from an office block at this size.
struct Package { uint8_t x, w, h, pins; };
constexpr Package kLit[] = {{16, 24, 34, 5}, {64, 20, 22, 3}, {128, 32, 28, 4},
                            {182, 26, 40, 6}};
constexpr int kPinPitch = 6;
constexpr int kPinDrop = 5;              // rows below a package's top the first pin sits

// The capacitor towers: cylinders standing clear of both ranks, taller than anything
// else and only two of them. A cap at this size is a bar with a band near the top, and
// the band is what stops it reading as another package.
struct Cap { uint8_t x, w, h; };
constexpr Cap kCaps[] = {{100, 9, 52}, {168, 7, 44}};

// The flux haze. Thin, and hugging the horizon rather than filling the sky — the band
// above a board is empty, which is what leaves this scene's upper rows to a screen.
constexpr uint8_t kDust[][2] = {
    {28, 44}, {49, 88}, {70, 30}, {96, 66}, {121, 40},
    {143, 96}, {166, 34}, {188, 72}, {206, 50}, {13, 76},
};

// The routing on the near ground: a run goes out from the FOOT of the canvas, turns
// once, and ends in a via. Each entry is where it starts, how far up it goes before the
// corner, and how far across it then runs — a positive width turning right, a negative
// one left. Measured from the foot rather than from the floor line so the runs stay in
// the near band on both grounds, where 40 rows of floor is the tighter of the two.
// Authored, because routing is authored.
struct Run { int16_t x; uint8_t up; int16_t across; };
constexpr Run kRuns[] = {
    {18, 14, 34}, {52, 26, -22}, {88, 9, 40}, {130, 22, 26},
    {168, 12, -30}, {200, 30, 16}, {6, 32, 20},
};
constexpr int kCorner = 2;               // the chamfer a copper corner is cut with

}  // namespace

void drawTraceCityScene(Framebuffer& fb, int beat, const SceneGround& g) {
    fb.clear(palColor(Pal::PAPER));

    sceneSpecks(fb, kDust, static_cast<int>(sizeof(kDust) / sizeof(kDust[0])), g,
                kToneDust);
    sceneGlow(fb, g, /*up=*/26, kToneHaze);

    // The skyline, back rank first so the front stands in front of it.
    sceneSilhouette(fb, kBackRank,
                    static_cast<int>(sizeof(kBackRank) / sizeof(kBackRank[0])),
                    g.horizonY, kToneFar);
    sceneSilhouette(fb, kFrontRank,
                    static_cast<int>(sizeof(kFrontRank) / sizeof(kFrontRank[0])),
                    g.horizonY, kToneNear);

    // The caps, and their bands.
    for (const Cap& c : kCaps) {
        fb.fillRect(c.x, g.horizonY - c.h, c.w, c.h, sceneTone(kToneCap));
        fb.fillRect(c.x, g.horizonY - c.h + 6, c.w, 2, sceneTone(kToneCapBand));
    }

    // The pins. Two columns per package, and one pin in each column is LIT on a slow
    // walk down the row — the same blink the rack lamps keep, and the scene's only
    // motion above the ground.
    const Rgb565 pin = sceneTone(kTonePin);
    const Rgb565 win = sceneTone(kToneWindow);
    for (const Package& p : kLit) {
        const int top = g.horizonY - p.h;
        for (int i = 0; i < p.pins; ++i) {
            const int y = top + kPinDrop + i * kPinPitch;
            if (y + 2 >= g.horizonY) break;
            const bool lit = ((beat + p.x / 8) % p.pins) == i;
            for (int side = 0; side < 2; ++side) {
                const int x = side ? p.x + p.w - 3 : p.x + 1;
                fb.fillRect(x, y, 2, 2, lit ? win : pin);
            }
        }
    }

    sceneMiddle(fb, g, kToneFar);
    sceneFloor(fb, g, /*seamPitch=*/64, kToneMask, kToneSeam, kToneKerb);

    // The routing. Up, chamfer, across, via — in that order, because that is the order
    // a run is drawn on a board and the chamfer is the join between the two legs.
    const Rgb565 copper = sceneTone(kToneTrace);
    const Rgb565 via = sceneTone(kToneVia);
    for (const Run& r : kRuns) {
        const int endY = kActiveH - r.up;
        fb.fillRect(r.x, endY, 2, r.up, copper);
        const int x0 = r.across > 0 ? r.x : r.x + r.across;
        const int w = r.across > 0 ? r.across : -r.across;
        fb.fillRect(x0, endY, w, 2, copper);
        for (int k = 0; k < kCorner; ++k)          // the chamfer, on the inside of the turn
            fb.fillRect(r.x + (r.across > 0 ? 2 + k : -1 - k), endY + 1 + k, 1, 1, copper);
        fb.fillRect(r.x + r.across - 1, endY - 1, 4, 4, via);
    }
}

}  // namespace mal
