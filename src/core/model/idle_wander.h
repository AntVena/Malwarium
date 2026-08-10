// idle_wander.h — where a resting creature stands inside the idle habitat.
//
// The habitat pins its occupant to the middle of the shelf; this walks that anchor
// a little way around the living box, so a pet at rest is somewhere slightly
// different each time the screen is looked at. One move is a short trip to a nearby
// point followed by a pause, and how the trip reads is the creature's Locomotion
// (content/defs.h): a walker's feet stay on the shelf, a flier holds an altitude
// above it, a swimmer ignores the shelf entirely and drifts on both axes at once,
// and a Ground mover is a slow walker that also gives up the shelf bob.
//
// Offsets are LOGICAL px from the anchor — +x right, +y UP off the shelf. The
// habitat (game_render.cpp drawHabitat) scales them into the active canvas and
// draws the sprite there; nothing else about the pose changes, so this animates a
// single-frame creature as much as a fully authored sheet.
//
// The stream is its own, never Game::rng_. A wander advances on every heartbeat the
// engine takes, so drawing from the gameplay LCG would make a hatch pool or a loot
// roll depend on how long the player left the pet sitting on its shelf.
#pragma once

#include <cstdint>

#include "core/content/defs.h"   // Locomotion

namespace mal {

// The box the anchor may stray into, in logical px. A pet cell is 56x48 logical on
// a 128-wide canvas, so 36px of slack exists to either side; the rest is margin, so
// even a full-width sprite stays clear of the bezel. The ceiling is short of the
// living zone's full height for the same reason at the top — the carousel caption
// band and the hunger alert both sit in the air above the shelf.
constexpr int kWanderHalfSpanX = 24;
constexpr int kWanderRiseMax = 22;

class IdleWander {
public:
    // Advance one heartbeat for a creature that moves this way. The locomotion is
    // passed every beat rather than latched at a reset, so the habitat needs exactly
    // one call site: a hatch, an evolution or a loaded save simply starts arriving
    // with a different value, and the wander re-homes onto the new mover's rules.
    void step(Locomotion loco);

    // Back onto the anchor, with nowhere to be. What the habitat holds a creature
    // that is not moving under its own power on — an unhatched egg sits where it was
    // laid, with its incubation readout drawn directly above it.
    void park();

    int offsetX() const { return x_; }   // logical px, + right of the anchor
    int offsetY() const { return y_; }   // logical px above the shelf, never below it

    // Mid-trip rather than parked between them — what a creature with an authored walk
    // clip poses on, so the legs move exactly on the beats the anchor does. BOTH terms
    // are load-bearing: step() retargets the moment a trip lands, so there is distance
    // left to the next target for the whole of the rest that follows, and distance alone
    // would walk a parked creature on the spot until it set off again.
    bool travelling() const {
        return rest_ == 0 && (x_ != targetX_ || y_ != targetY_);
    }

    // Whether this mover's pose carries the shelf bob on top of the drift. Two movers
    // skip it, for opposite reasons: a swimmer moves continuously already, so a bob
    // over that reads as jitter rather than breathing, while a Ground mover is defined
    // by never breaking contact with the floor — a 2px lift, even for one beat, is the
    // one thing it must not do (Locomotion, content/defs.h).
    static bool bobs(Locomotion loco) {
        return loco != Locomotion::Swim && loco != Locomotion::Ground;
    }

    // Re-point this wander's own stream. Two wanders start life identical, so several
    // on one screen — a worm and the ambient copies beside it — would otherwise march
    // in lockstep and read as one animation drawn three times instead of as separate
    // creatures. Seeding is the whole of what tells them apart.
    void seed(uint32_t s) { rng_ = s; }

private:
    void retarget();
    // Somewhere new on one axis: `reach` px at most from `from`, inside [lo,hi].
    int hop(int from, int reach, int lo, int hi);
    uint32_t roll(uint32_t span);   // 0..span-1, off this component's own stream

    int x_ = 0, y_ = 0;             // where the anchor is now
    int targetX_ = 0, targetY_ = 0; // ...and where this trip is taking it
    int rest_ = 0;                  // heartbeats left parked before the next trip
    Locomotion loco_ = Locomotion::Walk;
    uint32_t rng_ = 0x2545f491u;
};

}  // namespace mal
