#include "core/ui/tourney_screen.h"

#include "core/model/tournament.h"   // tourneyBlockSize / tourneyBlockStart — the tree the ties draw
#include "core/render/canvas.h"
#include "core/render/framebuffer.h"
#include "core/render/sprite.h"
#include "core/render/palette.h"

namespace mal {

namespace {

// The scene's ramp, in the order it recedes. Named rather than spelled at each draw
// site, because what makes a handful of tones read as depth is that a tone means one
// DISTANCE everywhere it appears. They are deliberately bunched near the dark end:
// eight rows of list text sit on the sky, and the picture is not allowed to cost them
// contrast — so the harbour is built out of what is left BELOW the text, where the
// separation between water and plank is the whole of the effect.
constexpr uint8_t kToneStar = 92;
constexpr uint8_t kToneGlow = 16;
constexpr uint8_t kToneShore = 52;
constexpr uint8_t kToneWater = 40;
constexpr uint8_t kToneShimmer = 104;
constexpr uint8_t kToneDeck = 78;
constexpr uint8_t kToneSeam = 56;
constexpr uint8_t kToneEdge = 150;
constexpr uint8_t kToneMoon = 132;
constexpr uint8_t kToneMoonMaria = 96;

// The horizon, in active rows. A 224px screen holding a 96px creature and a column of
// copy has no room for a landscape, so there is not one: the far shore is a ragged
// strip at the waterline and everything else the scene spends is spent on the dock
// itself, which is the part the operator is actually standing on.
constexpr int kShoreY = 174;
constexpr int kGlowTop = 150;
static_assert(kShoreY > kDockCardBottom,
              "the waterline has to start below the copy that sits on the sky");

// The treeline: one column of cypress per step, at heights that never repeat inside a
// span the eye can hold. Authored as a table rather than rolled, so the same harbour
// is the same harbour every time it is drawn — a shore that reshuffled itself between
// repaints would be the one moving thing on a screen that repaints on every keypress.
constexpr uint8_t kTreeline[] = {3, 6, 2, 5, 4, 3, 2, 6, 3, 5, 2, 4, 6, 3,
                                 2, 5, 3, 6, 4, 2, 5, 3, 6, 2, 4, 3, 5, 2};

// The night sky, as x/y pairs. Sparse and authored rather than rolled, for the reason
// the treeline is: the eight rows of the field are drawn straight over it, and a sky
// that reshuffled between repaints would be the one moving thing behind text somebody
// is trying to read. Kept well under the brightness of `ink` so a star can never be
// mistaken for a pixel of a glyph.
constexpr uint8_t kStars[][2] = {{18, 34},  {47, 18},  {73, 52},  {96, 27},
                                 {119, 41}, {141, 16}, {158, 60}, {177, 31},
                                 {203, 47}, {212, 22}, {31, 66},  {62, 88},
                                 {129, 74}, {186, 84}, {88, 100}, {166, 104}};

// The moon, low over the water, in the one band of sky the layout keeps clear: the
// gap between the foot of the field and the top of the opponent's card. It is
// therefore MORE of it the further the bracket goes — every entrant knocked out
// shortens the field above it, which is the collapse paying for something twice.
constexpr int kMoonX = 100;
constexpr int kMoonY = 108;
constexpr int kMoonR = 8;

// A mooring post standing out of the water. The two at the canvas edges live in the
// 8px margin no text ever uses, which is what lets the deck have furniture at all on
// a screen this full.
void drawBollard(Framebuffer& fb, int x, int h, Rgb565 c) {
    fb.fillRect(x, kDockDeckY - h, 5, h, c);
    fb.fillRect(x - 1, kDockDeckY - h - 3, 7, 3, c);
}

}  // namespace

Rgb565 dockTone(uint8_t t) {
    return blend(palColor(Pal::PAPER), palColor(Pal::INK_DIM), t);
}

void drawDockScene(Framebuffer& fb, int beat) {
    fb.clear(palColor(Pal::PAPER));

    // The sky: stars, then a moon low over the water, then the lift toward the
    // treeline — in four steps rather than a gradient, because at this panel's depth a
    // smooth ramp bands anyway, so the bands are placed on purpose.
    const Rgb565 star = dockTone(kToneStar);
    for (const auto& p : kStars) fb.fillRect(p[0], p[1], 1, 1, star);
    const Rgb565 moon = dockTone(kToneMoon);
    for (int dy = -kMoonR + 1; dy < kMoonR; ++dy) {
        // Widths walked out rather than tabled: at this radius the disc is nine rows,
        // and nine rows of a hand-placed circle is a table nobody can check against the
        // radius it claims to be drawn at.
        int w = 0;
        while ((w + 1) * (w + 1) + dy * dy <= kMoonR * kMoonR) ++w;
        fb.fillRect(kMoonX - w, kMoonY + dy, w * 2 + 1, 1, moon);
    }
    // Two maria, which is what stops a light disc at this size reading as a blank
    // token. One tone down, so they are texture rather than a second shape.
    const Rgb565 maria = dockTone(kToneMoonMaria);
    fb.fillRect(kMoonX - 4, kMoonY - 3, 4, 3, maria);
    fb.fillRect(kMoonX + 1, kMoonY + 2, 3, 2, maria);
    constexpr int kGlowH = (kShoreY - kGlowTop) / 4;
    for (int i = 0; i < 4; ++i)
        fb.fillRect(0, kGlowTop + i * kGlowH, kActiveW, kGlowH,
                    dockTone(static_cast<uint8_t>(kToneGlow * (i + 1) / 4)));

    // The far shore, and the water it stands in.
    const Rgb565 shore = dockTone(kToneShore);
    constexpr int kTrees = static_cast<int>(sizeof(kTreeline) / sizeof(kTreeline[0]));
    for (int i = 0; i < kTrees; ++i) {
        const int w = kActiveW / kTrees;
        fb.fillRect(i * w, kShoreY - kTreeline[i], w, kTreeline[i], shore);
    }
    fb.fillRect(0, kShoreY, kActiveW, kDockDeckY - kShoreY, dockTone(kToneWater));

    // The light coming off the water walks with the beat, so the harbour is doing
    // something on a screen where nothing else moves between presses — the same
    // heartbeat the pet wanders on. Kept to the two outer thirds: the middle of this
    // band is where the opponent's copy is read, and a moving line under a word is a
    // word that has to be read twice.
    const Rgb565 shimmer = dockTone(kToneShimmer);
    constexpr int kStrokeY[] = {176, 179, 181, 177, 180, 182};
    constexpr int kStrokeX[] = {4, 26, 150, 186, 60, 96};
    constexpr int kStrokeW[] = {16, 22, 20, 26, 12, 24};
    for (int i = 0; i < 6; ++i) {
        const int drift = ((i & 1 ? beat : -beat) + i * 5) % 14;
        fb.fillRect(kStrokeX[i] + drift, kStrokeY[i], kStrokeW[i], 1, shimmer);
    }

    // The deck: planks running left to right, seamed across, with its far edge lit
    // where it meets the water. The edge is the line the fighters stand behind, and
    // drawing it is what stops them reading as floating over the harbour.
    fb.fillRect(0, kDockDeckY, kActiveW, kActiveH - kDockDeckY, dockTone(kToneDeck));
    fb.fillRect(0, kDockDeckY, kActiveW, 1, dockTone(kToneEdge));
    const Rgb565 seam = dockTone(kToneSeam);
    for (int x = 12; x < kActiveW; x += 26)
        fb.fillRect(x, kDockDeckY + 1, 1, kActiveH - kDockDeckY - 1, seam);
    const Rgb565 post = dockTone(kToneEdge);
    drawBollard(fb, 2, 18, post);
    drawBollard(fb, kActiveW - 7, 18, post);
    drawBollard(fb, 62, 8, dockTone(kToneShimmer));    // two further out, in the water
    drawBollard(fb, 140, 6, dockTone(kToneShimmer));
    // The moon on the water, directly under it and broken up by the swell. The one
    // piece of the scene that has to agree with another piece, so it is derived from
    // the moon's own x rather than placed.
    for (int y = kShoreY + 1; y < kDockDeckY; y += 2)
        fb.fillRect(kMoonX - 3 + ((y + beat) % 3), y, 5, 1, shimmer);
}

void drawDockTies(Framebuffer& fb, uint8_t alive, int cursor, int round,
                  int playerSlot) {
    for (int r = 0; r < kTourneyRounds; ++r) {
        const int n = tourneyBlockSize(r);
        const int x = dockTieX(r);
        const Rgb565 c = r == round ? palColor(Pal::INK) : palColor(Pal::INK_DIM);
        for (int start = 0; start < kTourneySlots; start += n) {
            const int yA = dockBlockAnchorY(alive, start, n / 2, cursor);
            const int yB = dockBlockAnchorY(alive, start + n / 2, n / 2, cursor);
            const int t =
                playerSlot >= 0 && tourneyBlockStart(playerSlot, r) == start ? 2 : 1;
            fb.fillRect(x, yA, t, yB - yA + t, c);       // the spine
            fb.fillRect(x, yA, kDockTieColW, t, c);      // an arm into each half
            fb.fillRect(x, yB, kDockTieColW, t, c);
        }
    }
}

int dockFieldBottomMax(uint8_t alive) {
    // The tallest the field can get is with the cursor expanding one out row. Found by
    // asking, rather than by reasoning about which slot it is: the answer is one loop
    // over eight, and a rule derived twice is a rule that eventually disagrees.
    int worst = dockFieldBottom(alive, -1);
    for (int slot = 0; slot < kTourneySlots; ++slot) {
        const int b = dockFieldBottom(alive, slot);
        if (b > worst) worst = b;
    }
    return worst;
}

bool dockFaceoffFits(uint8_t alive) {
    return dockFieldBottomMax(alive) + 4 <= kDockFighterY;
}

int dockSeatX(const SpriteData& s, bool mirror) {
    return kActiveW - kMargin - spriteContentX1(s, mirror);
}

int dockSeatY(const SpriteData& s) { return kDockFeetY - s.h; }

int dockCardW(const SpriteData* s, bool mirror) {
    const int left = s ? dockSeatX(*s, mirror) + spriteContentX0(*s, mirror) - 4
                       : kActiveW - kMargin;
    return left - kMargin;
}

}  // namespace mal
