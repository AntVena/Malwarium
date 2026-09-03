// test_flock.cpp — the guardians' bodies: the boids model and the swarm that draws it.
//
// Two subjects, one file, for the reason test_shibboleth.cpp gives about its three: the
// model is only interesting because of what the draw makes of it. What the gates here can
// actually hold is the part a screenshot cannot — that the sim stays inside its cell, that
// it never collapses or blows up, and that the moods are DIFFERENT ENOUGH to read. How it
// looks is `tools/screens.sh`'s job, and no gate is a substitute for it.
#include "test_gates.h"

#include "core/model/flock.h"
#include "core/render/swarm.h"
#include "core/ui/shibboleth_screen.h"

namespace {

// The cell every gate here works in — the screen's own, so a bound that holds in a test
// holds on the panel (shibboleth_screen.h).
constexpr int kW = mal::kGuardianCellW;
constexpr int kH = mal::kGuardianCellH;

mal::FlockMood kAllMoods[] = {
    mal::FlockMood::Watching, mal::FlockMood::Attending, mal::FlockMood::Pleased,
    mal::FlockMood::Agitated, mal::FlockMood::Withdrawn, mal::FlockMood::Open,
};

// A flock run to equilibrium. Every mood settles well inside this
// (test_flock_settles_quickly_enough_to_be_seen pins how quickly), so a gate about a
// SHAPE runs to here first rather than measuring a swarm mid-arrival.
mal::Flock settled(mal::FlockMood mood, uint32_t seed, int steps = 120) {
    mal::Flock f;
    f.seed(seed);
    f.reset(mal::kGuardianSwarmMarks, kW, kH);
    for (int i = 0; i < steps; ++i) f.step(mood);
    return f;
}

mal::SwarmView viewOf(const mal::Flock& f) {
    mal::SwarmView v;
    v.n = f.count();
    for (int i = 0; i < v.n; ++i) {
        v.marks[i].x = static_cast<int16_t>(f.x(i));
        v.marks[i].y = static_cast<int16_t>(f.y(i));
        v.marks[i].vx = static_cast<int16_t>(f.vx(i));
        v.marks[i].vy = static_cast<int16_t>(f.vy(i));
    }
    v.cx = f.centreX();
    v.cy = f.centreY();
    v.spread = f.spread();
    return v;
}

}  // namespace

// THE containment gate. A swarm is drawn in a cell with text directly under it, so a boid
// that walks out of the box is a mark on somebody's reading. The model has to hold this by
// itself — the draw's clip is the second line of defence, not the first — and it has to
// hold for every mood, because the moods are exactly what change how hard the sim pushes.
void test_flock_never_leaves_its_cell() {
    for (mal::FlockMood mood : kAllMoods) {
        for (uint32_t seed = 1; seed <= 12; ++seed) {
            mal::Flock f;
            f.seed(seed);
            f.reset(mal::kGuardianSwarmMarks, kW, kH);
            for (int s = 0; s < 400; ++s) {
                f.step(mood);
                for (int i = 0; i < f.count(); ++i) {
                    CHECK(f.x(i) >= 0 && f.x(i) <= kW);
                    CHECK(f.y(i) >= 0 && f.y(i) <= kH);
                }
            }
        }
    }
}

// It neither collapses to a dot nor comes apart. Both failures look like a bug rather than
// a creature, and both are what an integrator with badly-weighted rules actually does: too
// much cohesion and every mark ends up on the same pixel, too much separation and they all
// end up pinned to the walls. The floor is what makes the mesh (core/render/swarm.h) draw
// a body rather than one bright knot.
void test_flock_holds_together_without_collapsing() {
    for (mal::FlockMood mood : kAllMoods) {
        for (uint32_t seed = 1; seed <= 8; ++seed) {
            const mal::Flock f = settled(mood, seed);
            CHECK(f.spread() >= 4);            // a body, not a dot
            CHECK(f.spread() <= kH);           // ...and not the whole box
        }
    }
}

// The moods are legible AS SHAPES, which is the dual-coding gate for an effect whose only
// state is a disposition: a guardian that refused the pet and one that is at ease have to
// be different pictures in grayscale, before either of them is a colour. The two extremes
// are the pair that has to hold — a refusal draws in and an open swarm spreads out.
void test_flock_moods_read_as_different_shapes() {
    int knot = 0, wide = 0;
    for (uint32_t seed = 1; seed <= 8; ++seed) {
        knot += settled(mal::FlockMood::Withdrawn, seed).spread();
        wide += settled(mal::FlockMood::Open, seed).spread();
    }
    // Not merely different: different by enough to see. Half again is the bar, which at
    // these sizes is the difference between a twenty-px knot and a sixty-px cloud.
    CHECK(wide > knot * 3 / 2);

    // ...and the one a fight hangs off is the FAST one. An agitated guardian is the only
    // mood the player has to read in a hurry — the fight is one button away — so it has to
    // be moving visibly faster than a settled one, which is what draws the streaks.
    int agitated = 0, pleased = 0;
    for (uint32_t seed = 1; seed <= 8; ++seed) {
        const mal::Flock a = settled(mal::FlockMood::Agitated, seed);
        const mal::Flock p = settled(mal::FlockMood::Pleased, seed);
        for (int i = 0; i < a.count(); ++i) {
            const int ax = a.vx(i) < 0 ? -a.vx(i) : a.vx(i);
            const int ay = a.vy(i) < 0 ? -a.vy(i) : a.vy(i);
            agitated += ax > ay ? ax : ay;
            const int px = p.vx(i) < 0 ? -p.vx(i) : p.vx(i);
            const int py = p.vy(i) < 0 ? -p.vy(i) : p.vy(i);
            pleased += px > py ? px : py;
        }
    }
    CHECK(agitated > pleased * 3 / 2);
}

// It arrives in time to be SEEN. A guardian's hail holds for kShibbolethHailHoldBeats
// heartbeats and the flock steps four times per heartbeat (the FX clock, kFxAnimMs), so a
// sim that took longer than that to find its shape would spend every meeting mid-arrival
// and the moods above would never reach a player at all.
void test_flock_settles_quickly_enough_to_be_seen() {
    const int stepsOnScreen = kShibbolethHailHoldBeats * (kHeartbeatMs / kFxAnimMs);
    for (mal::FlockMood mood : kAllMoods) {
        const int early = settled(mood, 7, stepsOnScreen / 2).spread();
        const int late = settled(mood, 7, 400).spread();
        // Within a third of where it ends up, by halfway through the shortest hold that
        // ever shows one.
        const int drift = early > late ? early - late : late - early;
        CHECK(drift * 3 <= late + 3);
    }
}

// Two guardians are not the same guardian. The seed is the whole of what tells two flocks
// apart — the same rule IdleWander::seed keeps — and a meeting seeds off the walk position
// (Game::startShibboleth), so a swarm that ignored its seed would be the identical animation
// every time anyone met anything.
void test_two_seeds_are_two_creatures() {
    const mal::Flock a = settled(mal::FlockMood::Watching, 11);
    const mal::Flock b = settled(mal::FlockMood::Watching, 977);
    CHECK(a.count() == b.count());
    bool differs = false;
    for (int i = 0; i < a.count(); ++i)
        if (a.x(i) != b.x(i) || a.y(i) != b.y(i)) differs = true;
    CHECK(differs);

    // ...and the same seed IS the same creature, which is what lets a frame gate dump a
    // swarm by stepping the model to the frame it wants.
    const mal::Flock again = settled(mal::FlockMood::Watching, 11);
    for (int i = 0; i < a.count(); ++i) {
        CHECK(a.x(i) == again.x(i));
        CHECK(a.y(i) == again.y(i));
    }
}

// An empty or degenerate flock draws nothing and crashes nothing. Both are reachable: a
// cell can be asked for before a meeting has reset one, and the count is clamped rather
// than trusted, so the draw has to survive being handed the result.
void test_a_degenerate_flock_is_harmless() {
    mal::Flock f;
    CHECK(f.count() == 0);
    f.step(mal::FlockMood::Watching);          // stepping an empty flock is a no-op
    CHECK(f.count() == 0);
    CHECK(f.spread() == 0);

    // Over the ceiling clamps rather than overruns, and a cell of nothing collapses to a
    // point instead of dividing by one.
    f.reset(mal::kFlockMax + 40, 0, 0);
    CHECK(f.count() == mal::kFlockMax);
    for (int i = 0; i < 20; ++i) f.step(mal::FlockMood::Agitated);

    mal::Framebuffer fb(kActiveW, kActiveH);
    fb.clear(palColor(mal::Pal::PAPER));
    drawSwarm(fb, viewOf(f), 10, 10, kW, kH, palColor(mal::Pal::ACCENT),
              palColor(mal::Pal::INK_DIM));
    mal::SwarmView empty;
    drawSwarm(fb, empty, 10, 10, kW, kH, palColor(mal::Pal::ACCENT),
              palColor(mal::Pal::INK_DIM));
}

// THE draw's containment gate, and the one the model's own cannot cover: a STREAK reaches
// backward past its mark, so a boid sitting legally on the cell's edge still has a tail
// heading out of it. Nothing may reach the row of text under the cell.
void test_the_swarm_draws_only_inside_its_cell() {
    mal::Framebuffer fb(kActiveW, kActiveH);
    const int cx = 40, cy = 40;
    for (mal::FlockMood mood : kAllMoods) {
        const mal::Flock f = settled(mood, 5);
        fb.clear(palColor(mal::Pal::PAPER));
        drawSwarm(fb, viewOf(f), cx, cy, kW, kH, palColor(mal::Pal::ACCENT),
                  palColor(mal::Pal::INK_DIM));
        const mal::Rgb565 paper = palColor(mal::Pal::PAPER);
        for (int y = 0; y < kActiveH; ++y)
            for (int x = 0; x < kActiveW; ++x) {
                const bool inCell = x >= cx && x < cx + kW && y >= cy && y < cy + kH;
                if (!inCell) CHECK(fb.get(x, y) == paper);
            }
    }
}

// It actually PUTS something on the panel, in every mood — the failure this catches is a
// swarm tuned until it is tasteful enough to be invisible, which a containment gate is
// perfectly happy with.
void test_the_swarm_is_visible_in_every_mood() {
    mal::Framebuffer fb(kActiveW, kActiveH);
    for (mal::FlockMood mood : kAllMoods) {
        const mal::Flock f = settled(mood, 3);
        fb.clear(palColor(mal::Pal::PAPER));
        drawSwarm(fb, viewOf(f), 20, 20, kW, kH, palColor(mal::Pal::ACCENT),
                  palColor(mal::Pal::INK_DIM));
        int lit = 0;
        for (int y = 20; y < 20 + kH; ++y)
            for (int x = 20; x < 20 + kW; ++x)
                if (fb.get(x, y) != palColor(mal::Pal::PAPER)) ++lit;
        // More than the marks themselves: the mesh and the haloes are most of what makes
        // this a creature, so a count near kGuardianSwarmMarks means they stopped drawing.
        CHECK(lit > mal::kGuardianSwarmMarks * 4);
    }
}
