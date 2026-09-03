// flock.h — a BOIDS flock: what a guardian's body actually is.
//
// A guardian is the thing that has been watching an area's networks for a very long
// time, and it is not shaped like the creatures the pet has met. It has no sprite: it is
// a SWARM, drawn from this — a few dozen marks steering by Reynolds' three rules
// (separation, alignment, cohesion) plus a pull toward the point the flock is trying to
// occupy. What reads as "alien" is that nothing in it is authored: the body has no
// outline, only a density, and it is never the same shape twice.
//
// WHY A MODEL AND NOT A PROCEDURAL PASS. Every other effect on the device is derived from
// its own inputs and stores nothing (core/render/absorb.h says why: a 4fps event-driven
// repaint has to be able to draw the same frame twice). A flock cannot be — each frame is
// the last one steered, and that INTEGRATION is the whole of why it looks alive rather
// than like a field of sine waves. So it lives here beside IdleWander, which is the same
// shape of thing: state advanced one step at a time, no framebuffer, testable without
// one, and reproducible from a seed and a step count.
//
// INTEGER FIXED POINT throughout, at kFlockShift. The S3 has an FPU but nothing else in
// the engine's model layer spends one, and a flock's arithmetic is sums and clamps —
// fractions exist here only so that a boid moving a third of a pixel per frame moves at
// all, which at this scale is most of them.
//
// COST. The three rules are O(n^2) in the flock's own size, which is the reason kFlockMax
// is small: at 28 boids one step is ~780 pair tests of a few integer ops, on a clock that
// ticks 16 times a second. That is the budget the swarm is designed against — a real
// particle system is not available here and this is not one.
//
// The stream is its own, never Game::rng_ — the same rule IdleWander keeps, and for the
// same reason: a swarm advances on a clock the player controls by standing still, and
// drawing from the gameplay LCG would make a loot roll depend on how long someone looked
// at a guardian.
#pragma once

#include <cstdint>

namespace mal {

// Positions and velocities are in 1/16 px. Small enough that a boid can drift slower than
// a pixel a frame, large enough that the whole sim stays inside 16-bit ranges over a cell
// no bigger than the canvas.
constexpr int kFlockShift = 4;
constexpr int kFlockOne = 1 << kFlockShift;

// The ceiling on one swarm. Sized by the O(n^2) note in the banner rather than by taste:
// this is the number a 16fps step can afford everywhere it might run, so a caller asking
// for more gets this instead of a frame budget it cannot see.
constexpr int kFlockMax = 28;

// How a swarm is BEHAVING — the standing description of a guardian's disposition, which
// is what the flock is a picture of. Not a beat and not a countdown: a mood holds until
// the thing it describes changes (RENDER_PIPELINE.md's moment-vs-state rule), and the
// motion it produces is the model integrating under it.
//
// The rows are in core/model/flock.cpp beside the weights they select, because the whole
// content of a mood IS its weights — there is nothing else to a disposition here.
enum class FlockMood : uint8_t {
    Watching,    // met, and deciding: a loose body holding station, turning over
    Attending,   // it has been answered and is considering the answer — drawn in tight
    Pleased,     // settled: slow, close, orbiting its own centre
    Agitated,    // it did not like that: fast, flying apart and snapping back
    Withdrawn,   // it is not interested: pulled away, tight and receding
    Open,        // fluent enough to be spoken with: wide, slow, unguarded
};

// One swarm.
//
// Lives in a CELL — a box in whatever space the caller draws in, given at reset() and
// never stored in active-canvas coordinates. The model has no idea where on a panel it
// is, which is what lets one flock be drawn into a hail's figure band and a fight's
// fighter seat without knowing the difference.
class Flock {
public:
    // Scatter `n` boids across a `w` x `h` cell and point them at random headings. The
    // count is clamped to kFlockMax; a cell smaller than a few px collapses to a point,
    // which is a swarm arriving rather than an error.
    void reset(int n, int w, int h);

    // Advance one step under `mood`. The cell is whatever reset() was given — the mood
    // moves the LURE inside it (see the .cpp), so a swarm never has to be told where to
    // go by its caller.
    void step(FlockMood mood);

    int count() const { return n_; }

    // Where boid `i` is, in CELL px. Out-of-range reads clamp to the first boid rather
    // than off the end: the draw walks count() and a gate walks whatever it likes.
    int x(int i) const { return px_[idx(i)] >> kFlockShift; }
    int y(int i) const { return py_[idx(i)] >> kFlockShift; }

    // Where it is HEADING, in 1/16 px per step. The draw uses this for the streak behind
    // a boid — a swarm with no headings drawn reads as static noise however fast it
    // actually moves, because at these sizes one mark moving two pixels is invisible and
    // fifty marks all leaning the same way is not.
    int vx(int i) const { return vx_[idx(i)]; }
    int vy(int i) const { return vy_[idx(i)]; }

    // The flock's centre of mass, in cell px — where the BODY is, as distinct from where
    // any of it is. The draw ranks boids against it so the middle of a swarm is denser
    // than its edge, which is the only thing that makes a scatter read as one creature.
    int centreX() const;
    int centreY() const;

    // How far the flock is spread, in cell px: the mean distance from the centre. A
    // second, non-colour channel for the mood — a tight body and a scattered one are
    // different pictures in grayscale, which is what the dual-coding gate wants of an
    // effect whose whole state is "how it is feeling".
    int spread() const;

    // Re-point this flock's own stream. Two flocks reset the same way are identical, so
    // anything drawing more than one seeds them apart — the same thing IdleWander::seed
    // exists for.
    void seed(uint32_t s) { rng_ = s ? s : 1u; }

private:
    int idx(int i) const { return (i < 0 || i >= n_) ? 0 : i; }
    uint32_t roll(uint32_t span);

    int px_[kFlockMax] = {};   // 1/16 px inside the cell
    int py_[kFlockMax] = {};
    int vx_[kFlockMax] = {};   // 1/16 px per step
    int vy_[kFlockMax] = {};
    int n_ = 0;
    int w_ = 0, h_ = 0;        // the cell, in 1/16 px
    int lureX_ = 0, lureY_ = 0;
    int step_ = 0;             // steps taken, which is what walks the lure
    uint32_t rng_ = 0x9e3779b9u;
};

}  // namespace mal
