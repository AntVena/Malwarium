#include "core/render/scenes/draws.h"

#include "core/render/framebuffer.h"
#include "core/render/palette.h"

namespace mal {

namespace {

// LANDFALL AT FIRST LIGHT. A week of open water, and the walk comes ashore at SANDBOX
// BEACH with the dawn behind the convoys it crossed among. The Bayou is the same sea
// by moonlight from a dock, so this one is everything that harbour is not: no far
// shore, no moon, a sky lifting rather than starred, and sand underfoot instead of
// planks.
//
// THE BUOYS ARE THE AREA. The sea-net is a chain of relay buoys, and the one moving
// thing here worth watching is a packet hopping lamp to lamp along that chain toward
// the beach — the cable it rides is the thread the story follows ashore, so the cable
// is drawn too, coming up out of the surf and running off across the sand.
constexpr uint8_t kToneStar = 64;
constexpr uint8_t kToneGlow = 52;
constexpr uint8_t kToneShip = 14;
constexpr uint8_t kToneWater = 30;
constexpr uint8_t kToneSwell = 66;
constexpr uint8_t kToneBuoy = 92;
constexpr uint8_t kToneLampOff = 64;
constexpr uint8_t kToneHalo = 120;
constexpr uint8_t kToneLampOn = 210;
constexpr uint8_t kToneSand = 62;
constexpr uint8_t kToneRipple = 48;
constexpr uint8_t kToneSurf = 128;
constexpr uint8_t kToneCable = 22;

// The stars that are left, all of them high: dawn has already taken the low sky, so
// the field is the top third and thin. (x, up) as sceneSpecks reads them.
constexpr uint8_t kStars[][2] = {{21, 228}, {58, 242}, {94, 214}, {137, 236},
                                 {171, 220}, {204, 246}, {40, 198}, {188, 192}};

// Where the dawn lift starts, as a share of the sky. Higher than the Bayou's, because
// the light is the sky here rather than a band under a treeline — but well under the
// middle of the sky, which is where a screen's copy sits.
constexpr uint8_t kGlowUp = 64;

// The convoys: container ships hull-down on the horizon, a dark profile against the
// lift. Each column is two pixels; the stacks come in threes because nothing on the
// sea-net arrives alone, and the tall block at the stern is the bridge. The gaps
// between stacks drop all the way to the deck — a shallower notch reads as a
// battlement, and the Castle is two rungs further on.
constexpr uint8_t kShipBig[] = {5, 4, 3, 7, 7, 7, 3, 7, 7, 7, 3, 7,
                                7, 7, 3, 3, 11, 11, 10, 3, 2, 2};
constexpr uint8_t kShipSmall[] = {4, 3, 2, 5, 5, 2, 5, 5, 2, 8, 8, 2, 1};
constexpr SceneSpan kShipBigAt = {22, 48};
constexpr SceneSpan kShipSmallAt = {126, 26};

// The sea-net, far to near and left to right: each buoy sits `dy` rows below the
// horizon on the water and carries its lamp `h` rows above that. The near ones are
// wider and taller, which is the whole of the perspective — the middle band is ten
// rows deep and has no room for anything cleverer. The last stands in the surf.
struct Buoy { int x, dy, w, h; };
constexpr Buoy kBuoys[] = {{86, 1, 1, 3}, {118, 3, 2, 5}, {160, 5, 2, 7},
                           {198, 8, 3, 10}};
constexpr int kBuoyCount = static_cast<int>(sizeof(kBuoys) / sizeof(kBuoys[0]));
constexpr int kHopBeats = 2;   // how long the packet rests on each lamp

// The swell: strokes drifting on the heartbeat, `y` rows down from the horizon.
struct Swell { int x, y, w; };
constexpr Swell kSwells[] = {{6, 2, 14}, {44, 6, 20}, {130, 4, 16},
                             {174, 2, 12}, {96, 8, 18}, {12, 8, 10}};
constexpr int kSwellSpan = 12;

// Ripples the tide left in the sand, (x, rows below the surf line, width). Authored
// down to 70 rows so a fighter's taller beach keeps texture all the way to the foot;
// a resting pet's shorter one simply runs out of rows and clips them.
struct Ripple { int x, dy, w; };
constexpr Ripple kRipples[] = {{14, 6, 18},  {70, 9, 12},  {120, 5, 22}, {176, 11, 16},
                               {36, 17, 14}, {150, 20, 20}, {92, 26, 16}, {8, 31, 20},
                               {196, 30, 14}, {58, 39, 22}, {130, 44, 14}, {24, 52, 16},
                               {172, 57, 20}, {84, 64, 18}};

// The surf, lapping: short strokes riding the waterline, sliding back and forth.
constexpr int kSurfPitch = 28;
constexpr int kSurfW = 14;
constexpr int kSurfSpan = 6;

// The cable comes out of the water under the nearest buoy and runs down the beach,
// one column across for every `kCableRun` rows down, toward a relay station somewhere
// past the right edge of the canvas.
constexpr int kCableX = 168;
constexpr int kCableRun = 2;

}  // namespace

void drawNetSeaCrossingScene(Framebuffer& fb, int beat, const SceneGround& g) {
    fb.clear(palColor(Pal::PAPER));

    // The sky: what is left of the night up top, the dawn lifting under it, and the
    // convoys standing on the horizon against that light.
    sceneSpecks(fb, kStars, static_cast<int>(sizeof(kStars) / sizeof(kStars[0])), g,
                kToneStar);
    sceneGlow(fb, g, kGlowUp, kToneGlow);
    sceneSilhouette(fb, kShipBig, static_cast<int>(sizeof(kShipBig)), g.horizonY,
                    kToneShip, kShipBigAt);
    sceneSilhouette(fb, kShipSmall, static_cast<int>(sizeof(kShipSmall)), g.horizonY,
                    kToneShip, kShipSmallAt);

    // Open water, and the swell running across it.
    sceneMiddle(fb, g, kToneWater);
    const Rgb565 swell = sceneTone(kToneSwell);
    for (int i = 0; i < static_cast<int>(sizeof(kSwells) / sizeof(kSwells[0])); ++i)
        fb.fillRect(kSwells[i].x + sceneDrift(beat, i, kSwellSpan),
                    g.horizonY + kSwells[i].y, kSwells[i].w, 1, swell);

    // The sea-net. The packet rests on one lamp at a time and walks the chain toward
    // the shore, so the relaying reads as a direction rather than as a blink.
    const Rgb565 buoy = sceneTone(kToneBuoy);
    const int lit = (beat / kHopBeats) % kBuoyCount;
    for (int i = 0; i < kBuoyCount; ++i) {
        const Buoy& b = kBuoys[i];
        const int water = g.horizonY + b.dy;
        fb.fillRect(b.x - b.w, water - 1, b.w * 2 + 1, 2, buoy);   // the float
        fb.fillRect(b.x, water - b.h, 1, b.h - 1, buoy);           // the mast
        // The lamp, a pixel for the far ones and two square for the near, with a
        // ring of halo round whichever one holds the packet — at this size a single
        // bright pixel among four masts is a pixel, and a lit one has to be a light.
        const int lw = b.w > 1 ? 2 : 1;
        const int lx = b.x - (lw - 1), ly = water - b.h - lw;
        if (i == lit) fb.fillRect(lx - 1, ly - 1, lw + 2, lw + 2, sceneTone(kToneHalo));
        fb.fillRect(lx, ly, lw, lw, sceneTone(i == lit ? kToneLampOn : kToneLampOff));
    }

    // The beach. No seams: sand has none, which is what tells it from the Bayou's deck
    // at a glance. The lit edge is the wet line where the water stops.
    sceneFloor(fb, g, /*seamPitch=*/0, kToneSand, kToneRipple, kToneSurf);
    const Rgb565 ripple = sceneTone(kToneRipple);
    for (const Ripple& r : kRipples) {
        const int y = g.floorY + 2 + r.dy;
        if (y < kActiveH) fb.fillRect(r.x, y, r.w, 1, ripple);
    }
    const Rgb565 surf = sceneTone(kToneSurf);
    for (int i = 0, x = 4; x < kActiveW; ++i, x += kSurfPitch)
        fb.fillRect(x + sceneDrift(beat, i, kSurfSpan), g.floorY - 1, kSurfW, 1, surf);

    // The cable, last, because it lies across everything on the beach. It surfaces in
    // the shallows under the nearest buoy and does not stop at the edge of the canvas.
    const Rgb565 cable = sceneTone(kToneCable);
    for (int y = g.floorY - 2; y < kActiveH; ++y) {
        const int x = kCableX + (y - g.floorY) / kCableRun;
        if (x >= kActiveW) break;
        fb.fillRect(x, y, 2, 1, cable);
    }
}

}  // namespace mal
