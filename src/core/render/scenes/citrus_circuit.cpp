#include "core/render/scenes/draws.h"

#include "core/render/framebuffer.h"
#include "core/render/palette.h"

namespace mal {

namespace {

// DIAL-UP SUBURBIA AT 3AM. The entry area, and the one an operator sees more than any
// other, so it is built out of the plainest thing on the ladder: a flat residential
// street with a phone line over it. No trees, no hills, no distance — the whole place
// is a roofline, four poles and the wire between them.
//
// THE SAG IS THE AREA. Everything else here is furniture; the catenary across the long
// middle span is the one figure worth authoring, and it is the only curve in the set.
constexpr uint8_t kToneRoof = 54;
constexpr uint8_t kToneStreet = 34;
constexpr uint8_t kTonePole = 70;
constexpr uint8_t kToneWire = 88;
constexpr uint8_t kToneWindow = 200;
constexpr uint8_t kToneBlip = 190;
constexpr uint8_t kToneDrive = 72;
constexpr uint8_t kToneSeam = 46;
constexpr uint8_t kToneKerb = 124;

// Seven ranch houses, four columns each: a shallow gable that rises to a ridge and
// drops back. The ridge heights differ by a pixel or two between houses and nothing
// else does, which is what "evenly pitched" looks like from the street at night.
constexpr uint8_t kRoofline[] = {4, 7, 7, 4, 3, 6, 6, 3, 5, 8, 8, 5, 4, 7,
                                 7, 4, 3, 6, 6, 3, 5, 8, 8, 5, 4, 7, 7, 4};

// The one CRT still on, under the ridge of the fourth house. Three pixels square and
// the brightest thing in the scene by a long way — at this size a lit window is a dot,
// and a dot is only a window if nothing else near it is that colour.
constexpr int kWindowX = 118, kWindowW = 3;
constexpr int kWindowDrop = 5;   // rows below the horizon, i.e. down the house front

// The poles. Two pairs pushed to the margins, which leaves the long middle span the
// operator's copy sits over — and the sag under that copy is the shape being sold.
struct Pole { int x, h; };
constexpr Pole kPoles[] = {{12, 30}, {52, 34}, {172, 40}, {212, 32}};
constexpr int kPoleW = 2;
constexpr int kCrossW = 11, kCrossH = 2;   // the crossarm each wire ends on

// The catenary, as nine samples of how far a wire has dropped by that fraction of a
// span — 0 at each pole, deepest in the middle, in 16ths of the span's own depth. A
// table rather than a formula so the curve can be tuned by looking at it.
constexpr uint8_t kSagCurve[] = {0, 7, 12, 15, 16, 15, 12, 7, 0};
constexpr int kSagSteps = 8;      // one fewer than the table, i.e. the gaps between
constexpr int kSagDepthDiv = 5;   // a span sags a fifth of its own width at the middle

// The handshake packet, two pixels wide, running the wire left to right and starting
// over. Slow enough to be a thing crossing rather than a flicker: one span every few
// seconds at the heartbeat this rides.
constexpr int kBlipW = 2;
constexpr int kBlipStep = 6;

// Where a wire sits at column x, given the two poles it hangs between. Returns the row,
// or -1 when x is outside every span — the wire exists only between poles, so the
// corners of the canvas keep their empty sky.
int wireY(int x, const SceneGround& g) {
    const int n = static_cast<int>(sizeof(kPoles) / sizeof(kPoles[0]));
    for (int i = 0; i + 1 < n; ++i) {
        const int x0 = kPoles[i].x, x1 = kPoles[i + 1].x;
        if (x < x0 || x > x1) continue;
        const int y0 = g.horizonY - kPoles[i].h, y1 = g.horizonY - kPoles[i + 1].h;
        const int span = x1 - x0;
        // Where along the span, in table steps, plus the fraction between two samples —
        // a nine-sample curve over a hundred-pixel span would step visibly otherwise.
        const int t = (x - x0) * kSagSteps;
        const int k = t / span, f = t % span;
        const int a = kSagCurve[k], b = kSagCurve[k < kSagSteps ? k + 1 : kSagSteps];
        const int sag = (a + (b - a) * f / span) * span / (kSagDepthDiv * 16);
        return y0 + (y1 - y0) * (x - x0) / span + sag;
    }
    return -1;
}

}  // namespace

void drawCitrusCircuitScene(Framebuffer& fb, int beat, const SceneGround& g) {
    fb.clear(palColor(Pal::PAPER));

    // No stars and no glow: a suburb at 3am has a sodium haze rather than a sky, and
    // an empty field above the roofline is what leaves the wire something to read on.
    sceneSilhouette(fb, kRoofline,
                    static_cast<int>(sizeof(kRoofline) / sizeof(kRoofline[0])),
                    g.horizonY, kToneRoof);
    sceneMiddle(fb, g, kToneStreet);
    fb.fillRect(kWindowX, g.horizonY + kWindowDrop, kWindowW, kWindowW,
                sceneTone(kToneWindow));

    // The poles, then the wire they carry. Drawn after the roofline so a pole stands in
    // front of the house behind it rather than being swallowed by it.
    const Rgb565 pole = sceneTone(kTonePole);
    for (const Pole& p : kPoles) {
        fb.fillRect(p.x, g.horizonY - p.h, kPoleW, p.h, pole);
        fb.fillRect(p.x - kCrossW / 2, g.horizonY - p.h, kCrossW, kCrossH, pole);
    }
    const Rgb565 wire = sceneTone(kToneWire);
    for (int x = kPoles[0].x; x <= kPoles[3].x; ++x) {
        const int y = wireY(x, g);
        if (y >= 0) fb.fillRect(x, y, 1, 1, wire);
    }

    // The packet, riding the wire it travels rather than a straight line across it —
    // which is the whole point of having authored a sag.
    const int run = kPoles[3].x - kPoles[0].x;
    const int blipX = kPoles[0].x + (beat * kBlipStep) % run;
    const int blipY = wireY(blipX, g);
    if (blipY >= 0) fb.fillRect(blipX, blipY - 1, kBlipW, kBlipW, sceneTone(kToneBlip));

    // Cracked driveway concrete: the seams are wide and few, which is what tells a
    // driveway from the Bayou's planking at a glance.
    sceneFloor(fb, g, /*seamPitch=*/74, kToneDrive, kToneSeam, kToneKerb);
}

}  // namespace mal
