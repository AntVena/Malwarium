// swarm.h — FX_SWARM: a guardian, drawn as the flock it is.
//
// The SPRITE_BASE pass of RENDER_PIPELINE.md for one subject only: an area's GUARDIAN,
// which has no sheet and is not going to get one. Every other creature on the device is
// authored pixels; this one is a few dozen marks steering by boids rules
// (core/model/flock.h), and the point of it is that a player can tell at a glance that
// the thing across from the pet is not the same KIND of thing as the pet.
//
// WHAT MAKES IT READ AS A BODY rather than as dust, which is the whole problem with
// drawing a swarm at this size:
//
//   * DENSITY, not outline. A mark near the flock's centre of mass is drawn as a block
//     in the core colour; one out at the fringe is a single dim pixel. So the creature
//     has a middle and an edge without anything ever drawing either, and it is a
//     different shape every frame while staying recognisably one thing.
//   * STREAKS. At these sizes a mark that moved two pixels since the last frame is
//     indistinguishable from one that did not, so each mark trails along its own heading.
//     A flock all leaning one way is the only way "it turned to look at you" is visible.
//   * The spread is the SECOND CHANNEL. A tight swarm and a scattered one are different
//     pictures in grayscale, which is what the dual-coding gate asks of an effect whose
//     entire state is a disposition (assets/VISUAL_LANGUAGE.md).
//
// A STATE, not a moment (RENDER_PIPELINE.md's rule). What it is a function of is the
// guardian's DISPOSITION — FlockMood, set from where the meeting has got to and how it
// came out — and never a beat: a swarm does not run out, it is doing something until the
// guardian is doing something else. The motion is the model integrating under that mood,
// which is why the one thing remembered lives on the caller (Game::guardianFlock_), the
// same way FX_CAMO's level does.
//
// This draw itself allocates nothing and stores nothing: it is a pure function of the
// SwarmView handed to it, so a frame gate can dump any step of a swarm by stepping the
// model to it first.
#pragma once

#include <cstdint>

#include "core/render/color.h"

namespace mal {

class Framebuffer;

// One mark, in CELL px — the draw is told where the cell is and never asks the model.
struct SwarmMark {
    int16_t x = 0, y = 0;
    int16_t vx = 0, vy = 0;   // heading, in 1/16 px per step (kFlockShift)
};

// A flock as the draw path sees it, and it OWNS its marks rather than pointing at the
// model's — the same call CamoRamp makes, and for the same reason: this is a value passed
// every frame by a renderer that must not reach into core/model.
//
// `cx`/`cy` is the body's centre of mass and `spread` the mean distance from it, both in
// cell px. Both are ranked against rather than drawn, so a caller that has them cheaply
// hands them over instead of this walking the marks again.
constexpr int kSwarmMarksMax = 28;

struct SwarmView {
    SwarmMark marks[kSwarmMarksMax] = {};
    int n = 0;
    int cx = 0, cy = 0;
    int spread = 0;
};

// Draw `v` into the cell at (x, y, w, h) in active px, clipped to it — a swarm sitting
// beside text must not be able to put a pixel on the text, whatever the model does.
//
// `core` is what the dense middle is drawn in and `fringe` the outer marks; a caller
// picks both from core/ui/theme.h the way drawSpriteTinted's do. They are separate rather
// than a ramp because there is nothing between them to shade: a mark is in the body or it
// is out on the edge, and that IS the picture.
void drawSwarm(Framebuffer& fb, const SwarmView& v, int x, int y, int w, int h,
               Rgb565 core, Rgb565 fringe);

}  // namespace mal
