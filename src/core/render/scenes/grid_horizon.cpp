#include "core/render/scenes/draws.h"

#include "core/render/canvas.h"
#include "core/render/framebuffer.h"
#include "core/render/palette.h"

namespace mal {

namespace {

// THE VAPORWAVE PRIMARY — a prize backdrop, and the one scene here that is not painted
// in pure value. Magenta, cyan and sunset are what the look is made of and the palette
// law forbids all three as authored hues, so the place is built the only legal way: the
// same ramp every other scene uses, anchored on a PAL_CORE token instead of `ink-dim`.
//
// TWO ANCHORS, WHICH IS THE RAMP. `frag-lo` to `frag-hi` is already a purple-to-pink
// spectrum in the palette and no interface state has claimed it, so the far half of the
// picture takes the violet end and the near half the pink — the gradient is expressed
// by WHICH element gets which token rather than by a second colour being invented for
// it. Neither is `accent`, which means focus, and neither is a status hue.
constexpr Pal kFar = Pal::FRAG_LO;
constexpr Pal kNear = Pal::FRAG_HI;

// Kept under the ceiling everything wide is kept under: a prize backdrop is still a
// backdrop, and a screen's own rows are read on top of this one like any other.
constexpr uint8_t kToneSun = 140;
constexpr uint8_t kToneHaze = 30;
constexpr uint8_t kTonePalm = 96;
constexpr uint8_t kToneGrid = 120;

// The slit sun, sitting ON the horizon so half of it is already below the world. The
// bites are horizontal and WIDEN downward, which is the whole of the venetian-blind
// read — even bites would be a striped circle, and a striped circle is a beach ball.
constexpr int kSunR = 34;
constexpr int kSunBites = 4;
constexpr int kSunBiteTop = 1;     // rows cut out of the highest bite
constexpr int kSunBiteGrow = 1;    // and one more row per bite going down
constexpr int kSunBiteGap = 6;     // solid rows between two of them

// Three palms, two left and one right, well clear of the columns a screen keeps for
// fighters and copy. A palm at this size is a stick with a splash on it, and the splash
// is the whole of the read: the fronds have to ARC DOWN, because a crown of straight
// bars is a street lamp and a street lamp is the wrong decade.
struct Palm { int x, h, frond; };
constexpr Palm kPalms[] = {{20, 48, 15}, {48, 36, 12}, {198, 44, 14}};
constexpr int kFronds = 4;
// How far a frond's TIP has fallen below the crown, as a fraction of the frond's own
// length. Half is the whole difference between a palm and an umbrella.
constexpr int kFrondFallDiv = 2;

// The grid. Nine verticals is wide enough that the fan reaches the canvas edges well
// before the foot and few enough that they do not collapse into a smear where they meet
// the vanishing point. The phase is the beat itself: one sub-step per heartbeat pulls
// the horizontals toward the viewer at about the speed the pet breathes.
constexpr int kGridCols = 9;

}  // namespace

void drawGridHorizonScene(Framebuffer& fb, int beat, const SceneGround& g) {
    fb.clear(palColor(Pal::PAPER));

    // The sun, sitting ON the horizon: drawn whole, then everything below the world cut
    // back to `paper`, so what is left is the half a sun at dusk actually shows. The
    // bites are horizontal and WIDEN downward — even bites would be a striped circle,
    // and a striped circle is a beach ball.
    const Rgb565 cut = palColor(Pal::PAPER);
    sceneDisc(fb, kActiveW / 2, g.horizonY, kSunR, kToneSun, kNear);
    int y = g.horizonY - kSunR + kSunBiteGap;
    for (int i = 0; i < kSunBites && y < g.horizonY; ++i) {
        const int h = kSunBiteTop + i * kSunBiteGrow;
        fb.fillRect(kActiveW / 2 - kSunR, y, kSunR * 2, h, cut);
        y += h + kSunBiteGap;
    }
    fb.fillRect(0, g.horizonY, kActiveW, kActiveH - g.horizonY, cut);
    sceneGlow(fb, g, /*up=*/40, kToneHaze, kFar);

    // The palms, standing on the horizon in the far token. Each frond is walked out one
    // pixel at a time and allowed to fall as it goes, three to a side at different
    // lengths — which is what makes a crown rather than a cross.
    const Rgb565 palm = sceneTint(kTonePalm, kFar);
    for (const Palm& p : kPalms) {
        const int top = g.horizonY - p.h;
        fb.fillRect(p.x, top, 2, p.h, palm);
        for (int f = 0; f < kFronds; ++f) {
            const int dir = (f & 1) ? 1 : -1;
            const int len = p.frond - (f / 2) * 4;   // the outer pair is the longest
            const int fall = len / kFrondFallDiv;
            int was = 0;
            for (int k = 1; k <= len; ++k) {
                // The drop is quadratic in the distance out, and each step fills down
                // to the last one — a frond that skipped rows would be a dotted line,
                // which at this size reads as spray rather than as a leaf.
                const int drop = fall * k * k / (len * len);
                fb.fillRect(p.x + (dir < 0 ? -k : 1 + k), top + was, 1,
                            drop - was + 1, palm);
                was = drop;
            }
        }
    }

    // No middle band and no planked floor: below the horizon there is only the grid,
    // which is this place's whole answer to "what is the ground made of".
    sceneGrid(fb, g, kGridCols, beat, kToneGrid, kNear);
}

}  // namespace mal
