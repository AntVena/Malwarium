#include "core/ui/tourney_screen.h"

#include "core/model/tournament.h"   // tourneyBlockSize / tourneyBlockStart — the tree the ties draw
#include "core/render/canvas.h"
#include "core/render/framebuffer.h"
#include "core/render/sprite.h"
#include "core/render/palette.h"
#include "core/render/scene.h"

namespace mal {

namespace {

// THE HARBOUR'S ART DIRECTION. The machinery is core/render/scene.h — the ramp every
// colour comes from, the silhouette, the floor, the drift. What is here is only what
// makes this place THIS place: which tones, which skyline, where the moon is. A second
// scene (an EXPL area's own backdrop, a duelling stage) is another file this short, not
// a fork of this one.
//
// The tones are deliberately bunched near the dark end. Eight rows of list text sit on
// this sky, and the picture is not allowed to cost them contrast — so the harbour is
// built out of what is left BELOW the text, where the separation between water and
// plank is the whole of the effect.
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

// The two lines the harbour is composed against: the waterline, and the plank the
// fighters stand on. The floor is the LAYOUT's (kDockDeckY) rather than the scene's,
// which is the whole contract in core/render/scene.h — the screen knows where feet go.
constexpr SceneGround kDockGround{/*horizonY=*/174, /*floorY=*/kDockDeckY};
constexpr int kGlowTop = 150;
static_assert(kDockGround.horizonY > kDockCardBottom,
              "the waterline has to start below the copy that sits on the sky");

// The treeline: one column of cypress per step. A 224px screen holding a list of eight,
// a 96px creature and four lines of copy has no room for a landscape, so there is not
// one — the far shore is a ragged strip at the waterline, and everything else the scene
// spends is spent on the dock itself, which is the part the operator is standing on.
constexpr uint8_t kTreeline[] = {3, 6, 2, 5, 4, 3, 2, 6, 3, 5, 2, 4, 6, 3,
                                 2, 5, 3, 6, 4, 2, 5, 3, 6, 2, 4, 3, 5, 2};

// The night sky. Sparse and authored for the reason the treeline is authored: the eight
// rows of the field are drawn straight over it. Kept well under the brightness of `ink`
// so a star can never be mistaken for a pixel of a glyph.
constexpr uint8_t kStars[][2] = {{18, 34},  {47, 18},  {73, 52},  {96, 27},
                                 {119, 41}, {141, 16}, {158, 60}, {177, 31},
                                 {203, 47}, {212, 22}, {31, 66},  {62, 88},
                                 {129, 74}, {186, 84}, {88, 100}, {166, 104}};

// The moon, low over the water, in the one band of sky the layout keeps clear: the gap
// between the foot of the field and the top of the opponent's card. It is therefore
// MORE of it the further the bracket goes — every entrant knocked out shortens the
// field above it, which is the collapse paying for something twice.
constexpr int kMoonX = 100;
constexpr int kMoonY = 108;
constexpr int kMoonR = 8;

// The light coming off the water. Six strokes, drifting on the heartbeat, kept to the
// two outer thirds: the middle of this band is where the opponent's copy is read, and a
// moving line under a word is a word that has to be read twice.
struct Glint { int x, y, w; };
constexpr Glint kGlints[] = {{4, 176, 16},  {26, 179, 22}, {150, 181, 20},
                             {186, 177, 26}, {60, 180, 12}, {96, 182, 24}};
constexpr int kGlintSpan = 14;

// The mooring posts. The two at the canvas edges live in the 8px margin no text ever
// uses, which is what lets the deck have furniture at all on a screen this full; the
// other two stand further out in the water, dimmer, for depth.
struct Post { int x, h; uint8_t tone; };
constexpr Post kPosts[] = {{2, 18, kToneEdge},
                           {kActiveW - 7, 18, kToneEdge},
                           {62, 8, kToneShimmer},
                           {140, 6, kToneShimmer}};

}  // namespace

void drawDockScene(Framebuffer& fb, int beat) {
    fb.clear(palColor(Pal::PAPER));

    // The sky: stars, a moon low over the water, and the lift toward the treeline.
    sceneSpecks(fb, kStars, static_cast<int>(sizeof(kStars) / sizeof(kStars[0])),
                kToneStar);
    sceneDisc(fb, kMoonX, kMoonY, kMoonR, kToneMoon);
    // Two maria, which is what stops a light disc at this size reading as a blank
    // token. One tone down, so they are texture rather than a second shape.
    const Rgb565 maria = sceneTone(kToneMoonMaria);
    fb.fillRect(kMoonX - 4, kMoonY - 3, 4, 3, maria);
    fb.fillRect(kMoonX + 1, kMoonY + 2, 3, 2, maria);
    sceneGlow(fb, kGlowTop, kDockGround, kToneGlow);

    // The far shore, and the water it stands in.
    sceneSilhouette(fb, kTreeline,
                    static_cast<int>(sizeof(kTreeline) / sizeof(kTreeline[0])),
                    kDockGround, kToneShore);
    sceneMiddle(fb, kDockGround, kToneWater);
    const Rgb565 shimmer = sceneTone(kToneShimmer);
    for (int i = 0; i < static_cast<int>(sizeof(kGlints) / sizeof(kGlints[0])); ++i)
        fb.fillRect(kGlints[i].x + sceneDrift(beat, i, kGlintSpan), kGlints[i].y,
                    kGlints[i].w, 1, shimmer);
    // The moon on the water, directly under it and broken up by the swell. The one
    // piece of the scene that has to agree with another piece, so it is derived from
    // the moon's own x rather than placed.
    for (int y = kDockGround.horizonY + 1; y < kDockGround.floorY; y += 2)
        fb.fillRect(kMoonX - 3 + ((y + beat) % 3), y, 5, 1, shimmer);

    // The deck: planks running left to right, seamed across, with its far edge lit
    // where it meets the water.
    sceneFloor(fb, kDockGround, /*seamPitch=*/26, kToneDeck, kToneSeam, kToneEdge);
    for (const Post& p : kPosts) scenePost(fb, p.x, p.h, kDockGround, p.tone);
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
