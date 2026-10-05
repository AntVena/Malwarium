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
constexpr uint8_t kToneHalo = 140;
constexpr uint8_t kToneBeam = 96;
constexpr uint8_t kToneLampOn = 220;
constexpr uint8_t kToneTower = 84;
constexpr uint8_t kToneHousing = 24;
constexpr uint8_t kToneBridge = 104;
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
//
// The big one sits hard left, in the columns no fighter and no resting pet ever stands
// in, and it is big on purpose: at this panel's scale a ship has to be a shape before
// it can be a ship, and a dozen rows of profile is what that costs.
constexpr uint8_t kShipBig[] = {9, 8, 7, 6, 13, 13, 13, 6, 13, 13, 13, 6,
                                13, 13, 13, 6, 6, 20, 20, 20, 17, 6, 5, 4};
constexpr uint8_t kShipSmall[] = {5, 4, 3, 7, 7, 3, 7, 7, 3, 11, 11, 3, 2};
constexpr SceneSpan kShipBigAt = {0, 72};
constexpr SceneSpan kShipSmallAt = {94, 30};

// The bridge windows, lit: one strip across each ship's bridge, `up` rows above the
// horizon (the bridges are 20 and 11 tall). A row of lit windows high at one end of a
// dark hull is what makes the shape a ship under way and not a block of warehouses.
struct Bridge { int x, w, up; };
constexpr Bridge kBridges[] = {{52, 10, 16}, {115, 4, 8}};

// The sea-net, far to near and left to right: each buoy sits `dy` rows below the
// horizon on the water and carries its lamp `h` rows above that. The near ones are
// wider and taller, which is the whole of the perspective — the middle band is ten
// rows deep and has no room for anything cleverer.
struct Buoy { int x, dy, w, h; };
constexpr Buoy kBuoys[] = {{84, 1, 1, 4}, {124, 3, 2, 6}, {164, 6, 2, 9}};
constexpr int kBuoyCount = static_cast<int>(sizeof(kBuoys) / sizeof(kBuoys[0]));
constexpr int kHopBeats = 2;   // how long the packet rests on each lamp

// The last buoy in the chain, standing in the surf: the station the packet is
// arriving at. It is the scene's landmark, so it stands in the right-hand margin —
// the one strip a fighter and a resting pet both leave clear — and it is tall enough
// to reach well above any sprite's head, which is where it is seen on every screen.
// A lattice tower: two legs closing from `kTowerBase` wide at the float to
// `kTowerTop` at the lamp, braced every `kBracePitch` rows.
constexpr int kTowerX = 212;          // its centre column
constexpr int kTowerH = 52;           // floor to lamp housing
constexpr int kTowerBase = 9, kTowerTop = 3;
constexpr int kBracePitch = 7;
constexpr int kFloatW = 15, kFloatH = 4;
constexpr int kLampW = 3;
// The flash, when the packet lands: rays either side of the lamp. A backdrop's
// brightest step is still far under `ink`, so a lamp cannot announce itself by being
// bright — it does it by shape, which is what the rays are for.
constexpr int kBeamW = 12;

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

// The cable comes ashore at the tower's foot and runs inland down the beach, one
// column left for every `kCableRun` rows down — toward the relay station the story
// sends the walk to next, somewhere off the bottom of the canvas.
constexpr int kCableX = kTowerX - 6;
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
    for (const Bridge& b : kBridges)
        fb.fillRect(b.x, g.horizonY - b.up, b.w, 1, sceneTone(kToneBridge));

    // Open water, and the swell running across it.
    sceneMiddle(fb, g, kToneWater);
    const Rgb565 swell = sceneTone(kToneSwell);
    for (int i = 0; i < static_cast<int>(sizeof(kSwells) / sizeof(kSwells[0])); ++i)
        fb.fillRect(kSwells[i].x + sceneDrift(beat, i, kSwellSpan),
                    g.horizonY + kSwells[i].y, kSwells[i].w, 1, swell);

    // The sea-net. The packet rests on one lamp at a time and walks the chain toward
    // the shore, so the relaying reads as a direction rather than as a blink. The
    // tower in the surf is the chain's last stop, so it counts as one more lamp.
    const Rgb565 buoy = sceneTone(kToneBuoy);
    const int lit = (beat / kHopBeats) % (kBuoyCount + 1);
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

    // The tower in the surf, over the beach's edge: float, legs, braces, then the lamp
    // and — when the packet is home — its halo.
    const Rgb565 tower = sceneTone(kToneTower);
    const int foot = g.floorY + 1, top = g.floorY - kTowerH;
    fb.fillRect(kTowerX - kFloatW / 2, foot - kFloatH, kFloatW, kFloatH, tower);
    for (int y = top; y < foot - kFloatH; ++y) {
        const int half = (kTowerTop + (kTowerBase - kTowerTop) * (y - top) / kTowerH) / 2;
        fb.fillRect(kTowerX - half, y, 1, 1, tower);
        fb.fillRect(kTowerX + half, y, 1, 1, tower);
        if ((y - top) % kBracePitch == 0) fb.fillRect(kTowerX - half, y, half * 2 + 1, 1, tower);
    }
    const bool home = lit == kBuoyCount;
    const int lampY = top - kLampW - 1;
    if (home) {
        const Rgb565 beam = sceneTone(kToneBeam);
        fb.fillRect(kTowerX - 4 - kBeamW, lampY + 1, kBeamW, 1, beam);
        fb.fillRect(kTowerX + 5, lampY + 1, kBeamW, 1, beam);
    }
    fb.fillRect(kTowerX - 3, lampY - 1, 7, kLampW + 2, sceneTone(home ? kToneHalo
                                                                   : kToneHousing));
    fb.fillRect(kTowerX - 1, lampY, kLampW, kLampW,
                sceneTone(home ? kToneLampOn : kToneLampOff));

    // The cable, last, because it lies across everything on the beach. It comes out
    // from under the tower's float and does not stop at the edge of the canvas.
    const Rgb565 cable = sceneTone(kToneCable);
    for (int y = g.floorY - 2; y < kActiveH; ++y) {
        const int x = kCableX - (y - g.floorY) / kCableRun;
        if (x < 0) break;
        fb.fillRect(x, y, 2, 1, cable);
    }
}

}  // namespace mal
