#include "core/render/scenes/draws.h"

#include "core/render/canvas.h"
#include "core/render/framebuffer.h"
#include "core/render/palette.h"

namespace mal {

namespace {

// A DATACENTRE AISLE — a prize backdrop that reads in pure value, and should. Nothing
// about a rack room is a colour; it is a corridor of identical dark faces with lights
// on them, and the identity is entirely in the perspective and the blinking.
//
// It borrows sceneGrid's VANISHING POINT and none of its grid. A rank of cabinets is
// placed by picking the row where it MEETS THE FLOOR — near the foot of the canvas for
// the one you could touch, close to the horizon for the one at the end — and everything
// else follows from that row: sceneConverge says how far in the aisle has narrowed by
// then, and the same fraction shortens the cabinet. Two scenes needing that one call is
// exactly why it is a primitive rather than a detail inside the grid.
constexpr uint8_t kToneFar = 20;       // the dark at the end of the aisle
constexpr uint8_t kToneFace = 60;      // a cabinet face
constexpr uint8_t kToneEdge = 108;     // the lit leading edge of one
constexpr uint8_t kToneLed = 205;
constexpr uint8_t kToneLedOff = 42;
constexpr uint8_t kToneTile = 70;
constexpr uint8_t kToneSeam = 46;
constexpr uint8_t kToneLip = 128;
constexpr uint8_t kToneCeiling = 40;   // faint, and it has to be: a screen's copy sits on it

// The aisle: how wide the walkway is where it leaves the foot of the canvas, and how
// many ranks stand along it. Six a side is as many as read before they merge into one
// wall, and the walkway is wide because the centre columns are where a screen puts its
// fighters and its copy.
constexpr int kAisleHalf = 54;
constexpr int kRanks = 6;

// A cabinet's height where it is nearest, in rows above the floor it stands on. Every
// rank further down the aisle is shortened in the same proportion the aisle has closed
// by, which is the only thing that makes a corridor out of a row of rectangles.
constexpr int kCabH = 96;

// The lamps. Four to a face, blinking on a pattern derived from the beat and the rank's
// own index — a blink, not a slide, and the only blinking in the whole set. Arithmetic
// rather than a roll for the reason every scene's tables are authored: a screen's rows
// are read over this, and a sky that reshuffles is the one moving thing behind them.
constexpr int kLedRows = 4;
constexpr int kLedPeriod = 7;
constexpr int kLedDuty = 3;

}  // namespace

void drawMainframeRowScene(Framebuffer& fb, int beat, const SceneGround& g) {
    fb.clear(palColor(Pal::PAPER));

    // The walkway, all the way down. The far half is simply darker than the near — an
    // aisle does not open onto anything, so what is at the end is the absence of light
    // rather than a wall — and the raised-floor tiles take over where it is close
    // enough to see them. Both go down before the ranks, so a cabinet stands on the
    // floor rather than the floor being painted across its feet.
    fb.fillRect(0, g.horizonY, kActiveW, kActiveH - g.horizonY, sceneTone(kToneFar));
    sceneFloor(fb, g, /*seamPitch=*/28, kToneTile, kToneSeam, kToneLip);
    const Rgb565 seam = sceneTone(kToneSeam);
    for (int y = g.floorY + 14; y < kActiveH; y += 14)
        fb.fillRect(0, y, kActiveW, 1, seam);

    // Two LINES running back from the top corners to the same vanishing point, and
    // nothing between them: enough ceiling that the aisle is a corridor rather than a
    // pit, and little enough that the sky a screen puts its rows on stays `paper`. A
    // filled ceiling would be the whole upper half of the panel behind that copy.
    const Rgb565 ceiling = sceneTone(kToneCeiling);
    for (int y = 0; y + 1 < g.horizonY; ++y) {
        const int a = kActiveW * y / (2 * g.horizonY);
        const int b = kActiveW * (y + 1) / (2 * g.horizonY);
        fb.fillRect(a, y, b - a + 1, 1, ceiling);
        fb.fillRect(kActiveW - b - 1, y, b - a + 1, 1, ceiling);
    }

    const Rgb565 face = sceneTone(kToneFace);
    const Rgb565 edge = sceneTone(kToneEdge);
    const Rgb565 led = sceneTone(kToneLed);
    const Rgb565 dark = sceneTone(kToneLedOff);
    const int span = kActiveH - g.horizonY;

    // Far to near, so a nearer rank stands in front of the one behind it — which is
    // what turns a set of rectangles into a corridor with depth in it.
    for (int r = kRanks - 1; r >= 0; --r) {
        const int baseY = g.horizonY + span * (kRanks - r) / kRanks;
        // How much of the way toward the viewer this rank is, and therefore how much of
        // its full size it keeps. The two are the same number by construction.
        const int nearN = baseY - g.horizonY, nearD = span;
        const int h = kCabH * nearN / nearD;
        for (int side = -1; side <= 1; side += 2) {
            const int inner = sceneConverge(g, kActiveW / 2 + side * kAisleHalf, baseY);
            const int outer = side < 0 ? 0 : kActiveW;
            const int x0 = inner < outer ? inner : outer;
            fb.fillRect(x0, baseY - h, (inner < outer ? outer - inner : inner - outer),
                        h, face);
            fb.fillRect(inner, baseY - h, 1, h, edge);

            // The lamps, on the face and shrinking with it. A rank whose face has
            // closed to nothing gets none, which is the honest end of the aisle.
            const int lamp = h > kCabH / 2 ? 2 : 1;
            const int lx = inner + side * (4 + lamp);
            for (int i = 0; i < kLedRows; ++i) {
                const bool on = ((beat + r * 3 + i * 5) % kLedPeriod) < kLedDuty;
                const int ly = baseY - h + (i + 1) * h / (kLedRows + 1);
                if (h > 6) fb.fillRect(lx, ly, lamp, lamp, on ? led : dark);
            }
        }
    }

}

}  // namespace mal
