// scene.h — engine-drawn BACKDROPS: the BACKGROUND pass of RENDER_PIPELINE.md §"Canonical
// pass order", for a screen that wants to be somewhere rather than on `paper`.
//
// WHY THESE ARE DRAWN AND NOT SHIPPED AS ART. A backdrop that sits behind TEXT cannot
// hold a colour opinion. `assets/PAL_CORE.json` is the device's one palette and every
// theme (a colourblind set, a high-contrast set) moves the interface by moving tokens —
// so a backdrop authored as pixels is the one surface a theme can never reach, and it is
// under the rows a reader most needs contrast on. Everything here is therefore painted
// out of sceneTint(): one ramp, from `paper` to a named token, so a scene is dark by
// construction, moves with the palette, and can never out-shout `ink`. It also costs no
// flash, which on a screen already spending a full creature cell is the difference
// between a backdrop and a budget conversation (assets/ASSET_MANIFEST.md §J).
//
// A SCENE IS TWO NUMBERS. Everything below is laid out against a SceneGround: the
// HORIZON, where the far distance stops, and the FLOOR, the surface the subject stands
// on. Both belong to the SCREEN, not to the scene — the screen is the thing that knows
// where its text stops and where its sprite's feet are, and a scene that decided either
// for itself would be a scene that can only ever be used once. The floors in play are
// far apart: fighters stand at kCombatSpriteShelf and a resting pet at kLivingBottom.
//
// WHICH BAND IS MEASURED IN WHAT. At or below the horizon, a figure is an offset in
// ROWS — those bands are the same size wherever the ground is put, so a post 18 rows
// tall is 18 rows tall on every screen. ABOVE it, a figure is a FRACTION of the sky the
// screen actually has (sceneSkyY), because that band is the one whose size really
// changes: 174 rows of it under a pet and 130 under a fighter. A star placed a fixed
// number of rows up clears the bezel on one screen and is off the top of the other.
// Neither band takes an absolute row, ever — that is what makes a place portable, and
// sceneSpecks/sceneGlow take the ground rather than a row so that breaking the rule
// requires writing a different call.
//
// WHAT IS SHARED IS THE MACHINERY, NOT THE PLACE. A scene's identity — which silhouette,
// where the moon is, what the floor is made of — is a short table and a handful of calls
// in its own file under render/scenes/, named by a SceneId (render/scene_id.h) and
// reached through the catalogue in render/scenes.h. These are the parts every one of
// them would otherwise copy.
#pragma once

#include <cstdint>

#include "core/render/canvas.h"   // kActiveW — a figure spans the canvas unless it says otherwise
#include "core/render/color.h"
#include "generated/pal_core.h"   // Pal — the token a scene's ramp is anchored to

namespace mal {

class Framebuffer;

// One step of a backdrop ramp: 0 is `paper`, 255 is `anchor`. Every colour in a scene
// comes from here — that is the whole rule, and it is a function rather than a
// convention so that breaking it requires writing a different call.
//
// THE ANCHOR IS GUARDED, not merely advised. `accent` means FOCUS and a status hue
// means a state; a backdrop wearing either says something about the screen that is not
// true, and says it under the rows a reader is scanning. Asking for one of those here
// gets `ink-dim` instead, so the rail holds in the build rather than in review. A tint
// is for PRIZE backdrops — a place somebody chose — and `frag-lo`/`frag-hi` is the ramp
// PAL_CORE already carries that no interface state has claimed.
//
// CEILINGS, which the scene gate enforces in aggregate: kSceneWideCeiling for anything
// wide, and t up to 220 only for accents a few pixels across (a lit window, a via, a
// buoy lamp).
Rgb565 sceneTint(uint8_t t, Pal anchor);

// The ramp in its default anchoring — what a scene that reads in pure value uses.
inline Rgb565 sceneTone(uint8_t t) { return sceneTint(t, Pal::INK_DIM); }

// The step nothing WIDE may pass. A backdrop is under the rows a reader is scanning,
// and the separation between it and `ink` is the whole of why any of this is a ramp.
constexpr uint8_t kSceneWideCeiling = 150;

// That step on the BRIGHTEST anchor the rail admits — the one bar every scene can be
// held to, whichever ramp it chose. A tinted place at the ceiling is genuinely lighter
// than a value place at the same step, so a gate comparing every scene against the
// value ramp would be measuring the tint rather than the rule.
Rgb565 sceneCeiling();

// The two lines a scene is composed against, in active rows. Above `horizonY` is
// distance; between the two is the middle ground (water, haze, a floor's approach);
// `floorY` is the near surface, and a sprite's feet sit on or just below it.
struct SceneGround {
    int horizonY;
    int floorY;
};

// How wide the middle band is for a screen with no opinion about it. The Dock's harbour
// is the shipped example and it keeps ten rows of water between the far shore and the
// planks — enough for the horizon to be a band rather than a line, and little enough
// that the sky, which is where a scene's identity lives, keeps the rest.
constexpr int kSceneMiddleH = 10;

// The ground a screen hands a scene when the floor is all it has to say. A screen with
// a real opinion — open water wants a wide middle, a keep has no distance at all —
// writes its own SceneGround instead; this is the default, not the rule.
constexpr SceneGround sceneGround(int floorY) {
    return {floorY - kSceneMiddleH, floorY};
}

// A sky element's row: `up` is how far above the horizon it sits, in 256ths of the sky
// this ground actually leaves. 0 is on the horizon, 255 the top of the canvas. See the
// banner — this is the half of the composition that has to scale rather than translate.
int sceneSkyY(const SceneGround& g, uint8_t up);

// The sky lifting toward the horizon, in four steps rather than a gradient: at this
// panel's depth a smooth ramp bands anyway, so the bands are placed on purpose. `up` is
// where the lift starts, as a fraction of the sky (sceneSkyY).
void sceneGlow(Framebuffer& fb, const SceneGround& g, uint8_t up, uint8_t tone,
               Pal anchor = Pal::INK_DIM);

// A scatter of single pixels — stars, embers, dust, packet noise. Each row is (x, up),
// the second being a sky fraction rather than a row. `pts` is an authored table rather
// than a roll, and that is load-bearing: a screen's own text is drawn straight over
// this, and a sky that reshuffled between repaints would be the one moving thing behind
// words somebody is trying to read.
void sceneSpecks(Framebuffer& fb, const uint8_t (*pts)[2], int n, const SceneGround& g,
                 uint8_t tone, Pal anchor = Pal::INK_DIM);

// A filled disc, walked out from the radius rather than tabled — a moon, a sun, a
// distant dish. At backdrop sizes a circle is nine or so rows, and nine rows of
// hand-placed pixels is a table nobody can check against the radius it claims.
void sceneDisc(Framebuffer& fb, int cx, int cy, int r, uint8_t tone,
               Pal anchor = Pal::INK_DIM);

// The columns a figure is spread across. Defaulted to the whole canvas, which is what a
// horizon-spanning silhouette wants; a figure that occupies PART of the width — a keep
// far off on the right, teeth inside one gate's opening — names its own span instead of
// being padded out to the edges with zeroes nobody can read.
struct SceneSpan {
    int x = 0;
    int w = kActiveW;
};

// A ragged strip STANDING on `baseY`: `heights` columns spread evenly across `span`. A
// treeline, a skyline, a ridge of circuit traces, a run of crenellation — the
// silhouette IS the place, and it is the one thing a new scene really has to author.
// `baseY` is g.horizonY for a scene with a horizon and a row derived from the ground
// for one without: a keep's wall is a silhouette that stands nowhere near the distance.
void sceneSilhouette(Framebuffer& fb, const uint8_t* heights, int n, int baseY,
                     uint8_t tone, SceneSpan span = {}, Pal anchor = Pal::INK_DIM);

// The same strip HANGING from `baseY` — portcullis teeth, a soffit, stalactites. Its
// own call rather than a sign on the one above, because the two make different pictures
// out of the same table and a scene should say which it meant.
void sceneOverhang(Framebuffer& fb, const uint8_t* depths, int n, int baseY,
                   uint8_t tone, SceneSpan span = {}, Pal anchor = Pal::INK_DIM);

// The middle ground: a flat fill from the horizon down to the floor.
void sceneMiddle(Framebuffer& fb, const SceneGround& g, uint8_t tone,
                 Pal anchor = Pal::INK_DIM);

// The near surface, from the floor to the foot of the canvas: the fill, a lit far edge
// where it meets the middle ground, and seams every `seamPitch` across it. The lit edge
// is what stops whatever stands on the floor reading as floating over it.
void sceneFloor(Framebuffer& fb, const SceneGround& g, int seamPitch, uint8_t fill,
                uint8_t seam, uint8_t edge, Pal anchor = Pal::INK_DIM);

// A post standing out of the middle ground onto the floor — a mooring bollard, a fence
// stake, a pylon. `h` is how far it rises above the floor line.
void scenePost(Framebuffer& fb, int x, int h, const SceneGround& g, uint8_t tone,
               Pal anchor = Pal::INK_DIM);

// ONE-POINT PERSPECTIVE. The vanishing point is the centre of the canvas on the
// horizon, and sceneConverge is the whole of the maths: where a line that leaves the
// floor at `floorX` has got to by row `y`. An aisle of cabinet faces and a receding
// grid are the same two calls with different things hung off them, which is why this is
// a primitive and not a detail inside sceneGrid.
int sceneConverge(const SceneGround& g, int floorX, int y);

// A receding grid over the floor band: `cols` verticals converging on the vanishing
// point, and horizontals whose spacing tightens toward it. `phase` walks the
// horizontals toward the viewer — pass the beat to travel the grid, or a constant to
// hold it still.
void sceneGrid(Framebuffer& fb, const SceneGround& g, int cols, int phase, uint8_t tone,
               Pal anchor = Pal::INK_DIM);

// The offset one drifting element takes at `beat`. Every scene's motion rides the ~4fps
// heartbeat the pet wanders on, and takes its direction from `i` — so a field of them
// moves as a field rather than as one bar sliding, and two scenes drift alike.
int sceneDrift(int beat, int i, int span);

}  // namespace mal
