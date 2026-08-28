#include "core/render/scenes/draws.h"

#include "core/render/canvas.h"
#include "core/render/framebuffer.h"

namespace mal {

namespace {

// UNDER THE PIER — the Phishing line's own place, and the reason a line gets to override
// how its creatures move. Half this line walks and half swims, so "a swimmer's water"
// would have been the wrong fact about it. What is true of all of it is the OTHER END OF
// THE LINE: something up there is fishing, and the whole scene is drawn from the point
// of view of the thing being fished for.
//
// It shares the underwater inversion with the open water beside it — the column darkens
// downward, so up is where the light is — and then puts a ceiling on it. The pier deck
// is the horizon, seen from beneath, and the pilings come down through the frame.
// As with the open water beside it, the deep never reaches `paper`: the pet stands at
// the bed, and a creature silhouetted against the darkest band on the panel has lost
// its own read.
constexpr uint8_t kToneShallow = 54;
constexpr uint8_t kToneDeep = 28;
constexpr uint8_t kToneUnderside = 26;   // the deck seen from below: darker than water
constexpr uint8_t kTonePiling = 40;
constexpr uint8_t kToneWeed = 56;
constexpr uint8_t kToneLine = 96;
constexpr uint8_t kToneLure = 210;
constexpr uint8_t kToneHook = 130;
constexpr uint8_t kToneBed = 52;
constexpr uint8_t kToneBedLip = 106;

constexpr int kWaterBands = 4;

// How far down the frame the deck's underside reaches — a share of the water column, so
// the pier is overhead on every screen rather than only on the tall one.
constexpr uint8_t kDeckUp = 224;
// The planks, read from underneath: gaps of light between them, which is the one detail
// that says "deck" rather than "lid".
constexpr int kPlankPitch = 13, kPlankGap = 2;

// The pilings, coming down through the whole frame and stopping in the bed. Pushed to
// the margins — a pier's legs are the frame of this picture, and the middle is where a
// screen puts everything it has to say.
struct Piling { int x, w; };
constexpr Piling kPilings[] = {{10, 13}, {32, 9}, {186, 9}, {202, 13}};
// Weed skirting each one at the waterline of its own growth — the fouled band that says
// how long the thing has been standing there.
constexpr int kWeedTop = 44, kWeedH = 26;

// THE LURE. Off-centre, hanging on a line that goes up out of the frame, and the
// brightest thing in the scene by a wide margin — because it is meant to be, and because
// the joke only lands if the eye goes there first.
constexpr int kLureX = 150;
constexpr uint8_t kLureUp = 96;
constexpr int kLureW = 7, kLureH = 4;
constexpr int kLureBob = 3;

}  // namespace

void drawBaitShallowsScene(Framebuffer& fb, int beat, const SceneGround& g) {
    // The column runs to the FLOOR, not to the horizon: this is one body of water and
    // the bed is its bottom, so there is no middle band to leave between them. Every
    // horizon scene has one; a submerged one does not, and calling sceneMiddle here
    // would be drawing a band that isn't there.
    for (int i = 0; i < kWaterBands; ++i) {
        const int y0 = g.floorY * i / kWaterBands;
        const int y1 = g.floorY * (i + 1) / kWaterBands;
        const uint8_t t = static_cast<uint8_t>(
            kToneShallow + (kToneDeep - kToneShallow) * i / (kWaterBands - 1));
        fb.fillRect(0, y0, kActiveW, y1 - y0, sceneTone(t));
    }

    // The deck overhead, planked, with light coming through between the boards. Drawn
    // as a band rather than a silhouette because it is a ceiling: what makes it read is
    // the gaps, and a silhouette table has no way to say "hole".
    const int deckY = sceneSkyY(g, kDeckUp);
    fb.fillRect(0, 0, kActiveW, deckY, sceneTone(kToneUnderside));
    const Rgb565 gap = sceneTone(kToneShallow);
    for (int x = kPlankPitch / 2; x < kActiveW; x += kPlankPitch)
        fb.fillRect(x, 0, kPlankGap, deckY, gap);

    // The pilings, and the weed skirting them.
    const Rgb565 piling = sceneTone(kTonePiling);
    const Rgb565 weed = sceneTone(kToneWeed);
    for (const Piling& p : kPilings) {
        fb.fillRect(p.x, deckY, p.w, g.floorY - deckY, piling);
        // The fouled band sways with the water, one column either way on the beat.
        for (int k = 0; k < kWeedH; ++k)
            fb.fillRect(p.x - 1 + ((beat + k) % 3 - 1), deckY + kWeedTop + k,
                        p.w + 2, 1, weed);
    }

    // The line, and what is on the end of it. The line runs from the top of the frame
    // down to the hook, so it comes from somewhere this scene does not show.
    const int lureY = sceneSkyY(g, kLureUp) + (beat % 2 ? kLureBob : 0);
    fb.fillRect(kLureX + 2, 0, 1, lureY, sceneTone(kToneLine));
    fb.fillRect(kLureX, lureY, kLureW, kLureH, sceneTone(kToneLure));
    // The hook: down out of the lure and back on itself, which is three short runs and
    // the only figure in the scene that has to be read as a shape rather than a mass.
    const Rgb565 hook = sceneTone(kToneHook);
    fb.fillRect(kLureX + 2, lureY + kLureH, 1, 5, hook);
    fb.fillRect(kLureX, lureY + kLureH + 5, 3, 1, hook);
    fb.fillRect(kLureX, lureY + kLureH + 2, 1, 3, hook);

    sceneFloor(fb, g, /*seamPitch=*/0, kToneBed, kToneBed, kToneBedLip);
}

}  // namespace mal
