#include "core/render/scenes/draws.h"

#include "core/render/canvas.h"
#include "core/render/framebuffer.h"
#include "core/render/palette.h"

namespace mal {

namespace {

// THE DEEP WEB, TAKEN AT ITS WORD. The dive goes down under the Net-Sea to where the
// cable the walk has followed since the Bayou lies on the bottom, carrying everything
// nobody indexed — so this is the seabed, with the cable across it and the data
// running along it as light. The sea is inverted from every sky on the ladder: what
// light there is comes from ABOVE and fades going down, so the top of the canvas is
// the lightest water and the bottom is dark.
//
// WHERE THINGS STAND IS DECIDED BY WHO COVERS THEM. A fighter stands on each side of
// the middle and a resting pet in the middle itself, and the habitat's bottom bar hides
// the floor; so the cable runs through the MIDDLE band, where it is seen either side
// of any sprite, and the two landmarks are in the margins — a rock spire carrying a
// repeater on the left, a vent breathing bubbles up the whole height on the right.
constexpr uint8_t kToneSurface = 40;   // the water just under the surface
constexpr uint8_t kToneShaft = 10;     // how much a light shaft adds to the water
constexpr uint8_t kToneSnow = 54;
constexpr uint8_t kToneBubble = 96;
constexpr uint8_t kToneRidge = 20;
constexpr uint8_t kToneSilt = 34;
constexpr uint8_t kToneBed = 38;
constexpr uint8_t kTonePebble = 50;
constexpr uint8_t kToneBedLip = 70;
constexpr uint8_t kToneSpire = 46;
constexpr uint8_t kToneSpireLit = 72;   // its up-facing ledges, catching what light comes down
constexpr uint8_t kToneFish = 0;    // the page's own dark: a fish is a hole in the light
constexpr uint8_t kToneCable = 6;
constexpr uint8_t kToneCableSheen = 64;
constexpr uint8_t kToneRepeater = 84;
constexpr uint8_t kTonePulse = 220;
constexpr uint8_t kTonePulseTail = 120;

// The water column: four bands from the top of the canvas down, each a step darker,
// ending at this share of the way down to the horizon. Below that the water is the
// page's own dark — the depth the dive is at.
constexpr int kBands = 4;
constexpr uint8_t kBandsReach = 224;   // in 256ths of the way from the top to the horizon

// The light shafts: slanting down from the surface, each `w` wide, leaning one column
// right for every `kShaftLean` rows down, and gone by the bottom of the banded water.
struct Shaft { int x, w; };
constexpr Shaft kShafts[] = {{30, 10}, {92, 6}, {150, 12}};
constexpr int kShaftLean = 3;

// Marine snow: specks sinking a row every other beat and wrapping, (x, start row).
// Rows rather than sky fractions because they fall the whole height of the canvas.
constexpr uint8_t kSnow[][2] = {{12, 14},  {41, 90},  {67, 40},  {88, 132}, {103, 8},
                                {131, 70}, {158, 112}, {176, 30}, {197, 150}, {60, 170},
                                {120, 190}, {170, 60}};
constexpr int kSnowSink = 2;   // beats per row

// The vent's bubbles, rising up the right margin from the seabed: each starts `phase`
// rows up its climb and wobbles a column either side as it goes.
constexpr int kVentX = 210;
constexpr int kBubbleRise = 2;   // rows per beat
constexpr uint8_t kBubbles[] = {0, 23, 41, 68, 90, 117, 139, 161};

// The far seabed: a low ragged slope on the horizon, which is the only silhouette —
// down here there is no distance, only a little further down.
constexpr uint8_t kRidge[] = {3, 5, 4, 6, 7, 5, 4, 3, 4, 6, 8, 6, 5, 4,
                              3, 4, 5, 7, 6, 4, 3, 5, 6, 4, 3, 4, 5, 3};

// The rock spire in the left margin, two columns a step, and the repeater bolted to
// it where the cable comes past.
constexpr uint8_t kSpire[] = {10, 26, 40, 48, 44, 30, 14, 6};
constexpr SceneSpan kSpireAt = {0, 32};

// The cable: lying across the middle band, `kCableUp` rows above the floor at the
// edges and sagging `kCableSag` lower in the middle, three rows thick. A repeater sits
// every kRepeaterPitch columns, and the data runs left to right as pulses, `kPulseGap`
// apart, moving `kPulseSpeed` columns a beat.
constexpr int kCableUp = 9;
constexpr int kCableSag = 4;
constexpr int kCableH = 4;
constexpr int kRepeaterPitch = 74;
constexpr int kRepeaterW = 11, kRepeaterH = 7;
constexpr int kPulseGap = 56;
constexpr int kPulseSpeed = 5;
constexpr int kPulseW = 5, kPulseTail = 12;

// A school of fish crossing the mid-water, dark against the light coming down: what
// says "under the sea" before anything else on the canvas does. Each is (x, row as a
// 256th of the way down to the horizon), and the school drifts right a column a beat
// and wraps. A fish is a 4x2 body and a forked tail.
constexpr uint8_t kFish[][2] = {{20, 74}, {32, 66}, {30, 84}, {46, 76},
                                {44, 92}, {58, 82}, {140, 70}, {152, 64}};

// Pebbles on the seabed, (x, rows below the floor line).
constexpr uint8_t kPebbles[][2] = {{20, 6},  {58, 12}, {96, 4},  {134, 9}, {172, 15},
                                   {206, 5}, {40, 22}, {116, 26}, {190, 30}, {76, 38},
                                   {150, 44}, {10, 52}};

// The cable's top row at column x.
int cableY(const SceneGround& g, int x) {
    const int t = x * 2 - kActiveW;   // -W..W across the canvas
    return g.floorY - kCableUp + kCableSag - kCableSag * t * t / (kActiveW * kActiveW);
}

}  // namespace

void drawDeepWebDiveScene(Framebuffer& fb, int beat, const SceneGround& g) {
    fb.clear(palColor(Pal::PAPER));

    // The water column, lightest at the top, and the shafts coming down through it.
    const int bandsEnd = g.horizonY * kBandsReach / 256;
    const int bandH = bandsEnd / kBands;
    for (int i = 0; i < kBands; ++i)
        fb.fillRect(0, i * bandH, kActiveW, bandH,
                    sceneTone(static_cast<uint8_t>(kToneSurface * (kBands - i) / kBands)));
    for (const Shaft& s : kShafts)
        for (int y = 0; y < bandsEnd; ++y) {
            const int band = y / bandH < kBands ? y / bandH : kBands - 1;
            const uint8_t water = static_cast<uint8_t>(kToneSurface * (kBands - band) / kBands);
            fb.fillRect(s.x + y / kShaftLean, y, s.w, 1, sceneTone(water + kToneShaft));
        }

    // The far seabed, and the silt haze between it and the bed.
    sceneSilhouette(fb, kRidge, static_cast<int>(sizeof(kRidge)), g.horizonY, kToneRidge);
    sceneMiddle(fb, g, kToneSilt);

    // The bed itself: no seams, a lit lip where the silt settles, pebbles.
    sceneFloor(fb, g, /*seamPitch=*/0, kToneBed, kToneBed, kToneBedLip);
    const Rgb565 pebble = sceneTone(kTonePebble);
    for (const auto& p : kPebbles) {
        const int y = g.floorY + 2 + p[1];
        if (y < kActiveH) fb.fillRect(p[0], y, 3, 1, pebble);
    }

    // The spire in the left margin, standing on the bed and rising well into the water.
    sceneSilhouette(fb, kSpire, static_cast<int>(sizeof(kSpire)), g.floorY + 2, kToneSpire,
                    kSpireAt);
    const Rgb565 ledge = sceneTone(kToneSpireLit);
    for (int i = 0, n = static_cast<int>(sizeof(kSpire)); i < n; ++i) {
        const int x0 = kSpireAt.x + i * kSpireAt.w / n, x1 = kSpireAt.x + (i + 1) * kSpireAt.w / n;
        fb.fillRect(x0, g.floorY + 2 - kSpire[i], x1 - x0, 1, ledge);
    }

    // The cable, repeaters and all, then the data running along it.
    const Rgb565 cable = sceneTone(kToneCable);
    const Rgb565 sheen = sceneTone(kToneCableSheen);
    for (int x = 0; x < kActiveW; ++x) {
        fb.fillRect(x, cableY(g, x), 1, kCableH, cable);
        fb.fillRect(x, cableY(g, x) - 1, 1, 1, sheen);   // its armour, catching the light
    }
    const Rgb565 repeater = sceneTone(kToneRepeater);
    for (int x = kRepeaterPitch / 3; x < kActiveW; x += kRepeaterPitch)
        fb.fillRect(x - kRepeaterW / 2, cableY(g, x) - 1, kRepeaterW, kRepeaterH, repeater);
    const Rgb565 pulse = sceneTone(kTonePulse), tail = sceneTone(kTonePulseTail);
    for (int head = (beat * kPulseSpeed) % kPulseGap; head < kActiveW + kPulseTail;
         head += kPulseGap) {
        for (int x = head - kPulseTail; x < head; ++x)
            if (x >= 0 && x < kActiveW) fb.fillRect(x, cableY(g, x) + 1, 1, 2, tail);
        for (int x = head; x < head + kPulseW && x < kActiveW; ++x)
            fb.fillRect(x, cableY(g, x) + 1, 1, 2, pulse);
    }

    // The school, drifting across the light.
    const Rgb565 fish = sceneTone(kToneFish);
    for (const auto& f : kFish) {
        const int x = (f[0] + beat) % (kActiveW + 8) - 8;
        const int y = g.horizonY * f[1] / 256;
        fb.fillRect(x + 1, y, 4, 2, fish);   // the body, head to the right
        fb.fillRect(x, y - 1, 1, 1, fish);   // the tail's fork
        fb.fillRect(x, y + 2, 1, 1, fish);
        fb.fillRect(x + 5, y, 1, 1, fish);   // the snout
    }

    // Marine snow, sinking the whole height.
    const Rgb565 snow = sceneTone(kToneSnow);
    for (const auto& s : kSnow)
        fb.fillRect(s[0], (s[1] + beat / kSnowSink) % g.floorY, 1, 1, snow);

    // The vent's bubbles, climbing the right margin from the bed to the surface: a
    // two-pixel ring each, so a bubble is a bubble and not another speck of snow.
    const Rgb565 bubble = sceneTone(kToneBubble);
    for (int i = 0; i < static_cast<int>(sizeof(kBubbles)); ++i) {
        const int y = g.floorY - (kBubbles[i] + beat * kBubbleRise) % g.floorY;
        const int x = kVentX + ((y / 6 + i) % 3) - 1;
        fb.fillRect(x, y - 1, 2, 1, bubble);
        fb.fillRect(x - 1, y, 1, 2, bubble);
        fb.fillRect(x + 2, y, 1, 2, bubble);
        fb.fillRect(x, y + 2, 2, 1, bubble);
    }
}

}  // namespace mal
