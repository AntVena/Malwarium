// scene.h — engine-drawn BACKDROPS: the BACKGROUND pass of RENDER_PIPELINE.md §"Canonical
// pass order", for a screen that wants to be somewhere rather than on `paper`.
//
// WHY THESE ARE DRAWN AND NOT SHIPPED AS ART. A backdrop that sits behind TEXT cannot
// hold a colour opinion. `assets/PAL_CORE.json` is the device's one palette and every
// theme (a colourblind set, a high-contrast set) moves the interface by moving tokens —
// so a backdrop authored as pixels is the one surface a theme can never reach, and it is
// under the rows a reader most needs contrast on. Everything here is therefore painted
// out of sceneTone(): one ramp, from `paper` to `ink-dim`, so a scene is dark by
// construction, moves with the palette, and can never out-shout `ink`. It also costs no
// flash, which on a screen already spending a full creature cell is the difference
// between a backdrop and a budget conversation (assets/ASSET_MANIFEST.md §J).
//
// A SCENE IS TWO NUMBERS. Everything below is laid out against a SceneGround: the
// HORIZON, where the far distance stops, and the FLOOR, the surface the subject stands
// on. Both belong to the SCREEN, not to the scene — the screen is the thing that knows
// where its text stops and where its sprite's feet are, and a scene that decided either
// for itself would be a scene that can only ever be used once. ROCK THE DOCK's harbour
// (ui/tourney_screen.h) is the first caller; a combat stage's shelf and an EXPL area's
// own backdrop are the same two numbers.
//
// WHAT IS SHARED IS THE MACHINERY, NOT THE PLACE. A scene's identity — which silhouette,
// where the moon is, what the floor is made of — is a short table and a handful of calls
// that live with whatever owns the place. These are the parts every one of them would
// otherwise copy.
#pragma once

#include <cstdint>

#include "core/render/color.h"

namespace mal {

class Framebuffer;

// One step of the backdrop ramp: 0 is `paper`, 255 is `ink-dim`. Every colour in a
// scene comes from here — that is the whole rule, and it is a function rather than a
// convention so that breaking it requires writing a different call.
Rgb565 sceneTone(uint8_t t);

// The two lines a scene is composed against, in active rows. Above `horizonY` is
// distance; between the two is the middle ground (water, haze, a floor's approach);
// `floorY` is the near surface, and a sprite's feet sit on or just below it.
struct SceneGround {
    int horizonY;
    int floorY;
};

// The sky lifting toward the horizon, in four steps rather than a gradient: at this
// panel's depth a smooth ramp bands anyway, so the bands are placed on purpose.
void sceneGlow(Framebuffer& fb, int top, const SceneGround& g, uint8_t tone);

// A scatter of single pixels — stars, embers, dust, packet noise. `pts` is an authored
// table of (x, y) rather than a roll, and that is load-bearing: a screen's own text is
// drawn straight over this, and a sky that reshuffled between repaints would be the one
// moving thing behind words somebody is trying to read.
void sceneSpecks(Framebuffer& fb, const uint8_t (*pts)[2], int n, uint8_t tone);

// A filled disc, walked out from the radius rather than tabled — a moon, a sun, a
// distant dish. At backdrop sizes a circle is nine or so rows, and nine rows of
// hand-placed pixels is a table nobody can check against the radius it claims.
void sceneDisc(Framebuffer& fb, int cx, int cy, int r, uint8_t tone);

// A ragged strip standing on the horizon: `heights` columns spread evenly across the
// canvas. A treeline, a skyline, a ridge of circuit traces, a run of crenellation — the
// silhouette IS the place, and it is the one thing a new scene really has to author.
void sceneSilhouette(Framebuffer& fb, const uint8_t* heights, int n,
                     const SceneGround& g, uint8_t tone);

// The middle ground: a flat fill from the horizon down to the floor.
void sceneMiddle(Framebuffer& fb, const SceneGround& g, uint8_t tone);

// The near surface, from the floor to the foot of the canvas: the fill, a lit far edge
// where it meets the middle ground, and seams every `seamPitch` across it. The lit edge
// is what stops whatever stands on the floor reading as floating over it.
void sceneFloor(Framebuffer& fb, const SceneGround& g, int seamPitch, uint8_t fill,
                uint8_t seam, uint8_t edge);

// A post standing out of the middle ground onto the floor — a mooring bollard, a fence
// stake, a pylon. `h` is how far it rises above the floor line.
void scenePost(Framebuffer& fb, int x, int h, const SceneGround& g, uint8_t tone);

// The offset one drifting element takes at `beat`. Every scene's motion rides the ~4fps
// heartbeat the pet wanders on, and takes its direction from `i` — so a field of them
// moves as a field rather than as one bar sliding, and two scenes drift alike.
int sceneDrift(int beat, int i, int span);

}  // namespace mal
