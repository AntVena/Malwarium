#include "core/render/scenes/draws.h"

#include "core/render/canvas.h"
#include "core/render/framebuffer.h"
#include "core/render/palette.h"

namespace mal {

namespace {

// THE HARBOUR'S ART DIRECTION. The machinery is core/render/scene.h — the ramp every
// colour comes from, the silhouette, the floor, the drift. What is here is only what
// makes this place THIS place: which tones, which skyline, where the moon is.
//
// The tones are deliberately bunched near the dark end. Eight rows of list text sit on
// this sky wherever ROCK THE DOCK draws it, and the picture is not allowed to cost them
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

// The treeline: one column of cypress per step. A 224px screen holding a list of eight,
// a 96px creature and four lines of copy has no room for a landscape, so there is not
// one — the far shore is a ragged strip at the waterline, and everything else the scene
// spends is spent on the dock itself, which is the part the operator is standing on.
constexpr uint8_t kTreeline[] = {3, 6, 2, 5, 4, 3, 2, 6, 3, 5, 2, 4, 6, 3,
                                 2, 5, 3, 6, 4, 2, 5, 3, 6, 2, 4, 3, 5, 2};

// The night sky, as (x, height up the sky) — the second number a 256th of whatever sky
// the ground leaves, so the field spreads over 174 rows above a resting pet and packs
// into 130 above a fighter without losing a star off the top. Sparse and authored for
// the reason the treeline is authored: rows of text are drawn straight over it. Kept
// well under the brightness of `ink` so a star can never be mistaken for a glyph pixel.
constexpr uint8_t kStars[][2] = {{18, 206},  {47, 230}, {73, 180},  {96, 217},
                                 {119, 196}, {141, 233}, {158, 168}, {177, 211},
                                 {203, 187}, {212, 224}, {31, 159},  {62, 127},
                                 {129, 148}, {186, 133}, {88, 109},  {166, 103}};

// The moon, low over the water: high enough up the sky to clear the copy a screen puts
// on it, low enough that its reflection has water to lie on. On ROCK THE DOCK it lands
// in the gap between the foot of the field and the top of the opponent's card, and it
// is therefore MORE of it the further the bracket goes — every entrant knocked out
// shortens the field above it, which is the collapse paying for something twice.
constexpr int kMoonX = 100;
constexpr uint8_t kMoonUp = 98;
constexpr int kMoonR = 8;

// Where the lift toward the treeline starts, as its own share of the sky.
constexpr uint8_t kGlowUp = 36;

// The light coming off the water. Six strokes, drifting on the heartbeat, kept to the
// two outer thirds: the middle of this band is where a screen's copy is read, and a
// moving line under a word is a word that has to be read twice. `y` counts rows DOWN
// from the waterline, which is a band of fixed depth wherever the ground is put.
struct Glint { int x, y, w; };
constexpr Glint kGlints[] = {{4, 2, 16},  {26, 5, 22}, {150, 7, 20},
                             {186, 3, 26}, {60, 6, 12}, {96, 8, 24}};
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

void drawPirateBayouScene(Framebuffer& fb, int beat, const SceneGround& g) {
    fb.clear(palColor(Pal::PAPER));

    // The sky: stars, a moon low over the water, and the lift toward the treeline.
    sceneSpecks(fb, kStars, static_cast<int>(sizeof(kStars) / sizeof(kStars[0])), g,
                kToneStar);
    const int moonY = sceneSkyY(g, kMoonUp);
    sceneDisc(fb, kMoonX, moonY, kMoonR, kToneMoon);
    // Two maria, which is what stops a light disc at this size reading as a blank
    // token. One tone down, so they are texture rather than a second shape.
    const Rgb565 maria = sceneTone(kToneMoonMaria);
    fb.fillRect(kMoonX - 4, moonY - 3, 4, 3, maria);
    fb.fillRect(kMoonX + 1, moonY + 2, 3, 2, maria);
    sceneGlow(fb, g, kGlowUp, kToneGlow);

    // The far shore, and the water it stands in.
    sceneSilhouette(fb, kTreeline,
                    static_cast<int>(sizeof(kTreeline) / sizeof(kTreeline[0])),
                    g.horizonY, kToneShore);
    sceneMiddle(fb, g, kToneWater);
    const Rgb565 shimmer = sceneTone(kToneShimmer);
    for (int i = 0; i < static_cast<int>(sizeof(kGlints) / sizeof(kGlints[0])); ++i)
        fb.fillRect(kGlints[i].x + sceneDrift(beat, i, kGlintSpan),
                    g.horizonY + kGlints[i].y, kGlints[i].w, 1, shimmer);
    // The moon on the water, directly under it and broken up by the swell. The one
    // piece of the scene that has to agree with another piece, so it is derived from
    // the moon's own x rather than placed.
    for (int y = g.horizonY + 1; y < g.floorY; y += 2)
        fb.fillRect(kMoonX - 3 + ((y + beat) % 3), y, 5, 1, shimmer);

    // The deck: planks running left to right, seamed across, with its far edge lit
    // where it meets the water.
    sceneFloor(fb, g, /*seamPitch=*/26, kToneDeck, kToneSeam, kToneEdge);
    for (const Post& p : kPosts) scenePost(fb, p.x, p.h, g, p.tone);
}

}  // namespace mal
