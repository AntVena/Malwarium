#include "core/model/flock.h"

namespace mal {
namespace {

// The three rules' weights, plus what a mood does to the box the flock is kept in. All
// weights are numerators over kW — integers, so one step is sums and shifts.
//
// Only the flock spends these, so the magnitudes sit here rather than in tunables.h,
// exactly as IdleWander's paces sit with the wander. What a row IS is a disposition:
// reading down a column tells you how a guardian carries itself when it is pleased
// against when it is not, and that is the whole of the content.
//
//   sep   how hard a boid shoves off a neighbour inside sepR. The thing that stops a
//         swarm collapsing to one bright dot, so no row may set it to zero.
//   ali   how far it turns toward what its neighbours are doing. High is a shoal moving
//         as one sheet; low is a cloud of individuals.
//   coh   how hard it pulls toward the local centre. What makes a BODY out of marks.
//   lure  how hard the whole flock is pulled at the point it is trying to occupy — the
//         rule that is not Reynolds', and the one that keeps a swarm inside a cell that
//         has text either side of it.
//   speed the cap, in 1/16 px per step. THE mood tell: a flock that is upset is fast
//         before it is anything else.
//   sway  how far the lure itself wanders off the cell's centre, as 1/256ths of the cell.
//         Zero pins the body dead centre, which reads as something braced; a wide sway
//         is something drifting around looking at you.
//   sepR  the room each mark insists on, in whole px, and so the SIZE of the body: the
//         flock settles where separation balances cohesion, which is roughly this
//         spacing. It is on the row rather than shared because how much space a thing
//         takes up IS a disposition — something withdrawn draws into a knot and something
//         at ease spreads out, and that is the same creature both times.
//   churn a random shove on every mark every step, in 1/16 px. THE reason the swarm looks
//         alive rather than manufactured: separation and alignment together have a stable
//         solution — an evenly spaced lattice all pointing one way — and a flock that
//         reaches it stops being a creature and becomes a dot grid. Churn is what it
//         never quite settles out of, so no row may set it to zero.
constexpr int kW = 64;

struct MoodRow {
    int sep, ali, coh, lure, speed, sway, sepR, churn;
};

// One row per FlockMood, in that order.
//
// SPEED is the column to read first. A mark has to cover more than a pixel a step before
// it draws a streak at all (core/render/swarm.h), so a row under ~24 is a body that hangs
// in the air and one over ~50 is all strokes — which is the difference between something
// watching you and something that has decided about you.
constexpr MoodRow kMoods[] = {
    /* Watching  */ {30,  8,  7,  5, 40,  84, 13,  8},
    /* Attending */ {26, 10, 13,  9, 34,  30, 10,  9},
    /* Pleased   */ {24,  9, 14,  8, 30,  56, 12, 10},
    /* Agitated  */ {44,  4,  6, 11, 82, 110, 16, 14},
    /* Withdrawn */ {20, 11, 22, 16, 36,  22,  8,  9},
    /* Open      */ {34,  8,  5,  4, 28, 120, 18, 10},
};
static_assert(static_cast<int>(FlockMood::Open) + 1 ==
                  static_cast<int>(sizeof(kMoods) / sizeof(kMoods[0])),
              "one MoodRow per FlockMood");

const MoodRow& moodRow(FlockMood m) {
    const int i = static_cast<int>(m);
    return kMoods[(i < 0 || i >= static_cast<int>(sizeof(kMoods) / sizeof(kMoods[0])))
                      ? 0 : i];
}

// How far a mark can SENSE, in 1/16 px. Fixed rather than per-mood, unlike the separation
// radius beside it: this is a fact about the creature's perception, and a flock whose
// neighbourhood shrank with its temper would be several smaller creatures rather than one
// in a different mood. Comfortably wider than any row's sepR, so the body always has
// enough of itself in view to hold together.
constexpr int kNeiR = 34 * kFlockOne;

int clampTo(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

int absv(int v) { return v < 0 ? -v : v; }

// Chebyshev distance — max(|dx|,|dy|). A true length wants a square root per pair, which
// is the one thing an O(n^2) inner loop on this device must not spend; at boid scale the
// difference between a circle and a square of influence is a pixel, and it is a pixel
// nobody can see in a cloud that is moving.
int dist(int dx, int dy) { const int a = absv(dx), b = absv(dy); return a > b ? a : b; }

// A vector capped at `limit` without a square root: shrink by the ratio of the longer
// component, which is the same approximation the distance above already commits to.
void capTo(int& x, int& y, int limit) {
    const int d = dist(x, y);
    if (d <= limit || d == 0) return;
    x = x * limit / d;
    y = y * limit / d;
}

}  // namespace

uint32_t Flock::roll(uint32_t span) {
    // The same LCG every other roll in the engine advances on, off this flock's own seed.
    rng_ = rng_ * 1664525u + 1013904223u;
    return span ? ((rng_ >> 16) % span) : 0u;
}

void Flock::reset(int n, int w, int h) {
    n_ = clampTo(n, 0, kFlockMax);
    w_ = (w > 1 ? w : 1) << kFlockShift;
    h_ = (h > 1 ? h : 1) << kFlockShift;
    step_ = 0;
    lureX_ = w_ / 2;
    lureY_ = h_ / 2;
    for (int i = 0; i < n_; ++i) {
        // Scattered over the middle of the cell rather than the whole of it, so a swarm
        // ARRIVES as a body and spreads into its shape under the rules — starting it
        // filling the box means the first thing the player sees is the sim contracting.
        px_[i] = w_ / 4 + static_cast<int>(roll(static_cast<uint32_t>(w_ / 2)));
        py_[i] = h_ / 4 + static_cast<int>(roll(static_cast<uint32_t>(h_ / 2)));
        vx_[i] = static_cast<int>(roll(2 * kFlockOne)) - kFlockOne;
        vy_[i] = static_cast<int>(roll(2 * kFlockOne)) - kFlockOne;
    }
}

void Flock::step(FlockMood mood) {
    if (n_ <= 0) return;
    const MoodRow& m = moodRow(mood);
    const int sepR = m.sepR * kFlockOne;
    ++step_;

    // WHERE THE BODY IS TRYING TO BE. Two slow circles of different periods, which is
    // enough to never repeat inside the seconds a screen is up and costs no table: a lure
    // that sat still makes a swarm that hovers, and a swarm that hovers reads as a
    // particle emitter rather than as something with somewhere to be.
    //
    // Integer sine by way of a triangle wave — the shape is a drift, and nothing about it
    // is improved by being round.
    auto tri = [](int t, int period) {
        const int p = period > 0 ? period : 1;
        const int u = ((t % p) + p) % p;                 // 0..p-1
        const int half = p / 2;
        return (u < half ? u : p - u) * 512 / (half ? half : 1) - 256;   // -256..256
    };
    // The roam is measured against the room the BODY leaves, not against the cell: a
    // flock is about sepR*2 across at equilibrium, so swaying by a fraction of the half
    // cell walks a wide creature into the wall and pins it there. Against the remainder,
    // a tight swarm gets the run of the box and a wide one barely moves — which is also
    // the right reading, since the wide ones are the moods that are not going anywhere.
    const int body = 2 * m.sepR * kFlockOne;
    const int roomX = w_ / 2 > body ? w_ / 2 - body : 0;
    const int roomY = h_ / 2 > body ? h_ / 2 - body : 0;
    lureX_ = w_ / 2 + tri(step_, 197) * roomX * m.sway / (256 * 256);
    lureY_ = h_ / 2 + tri(step_, 131) * roomY * m.sway / (256 * 256);

    for (int i = 0; i < n_; ++i) {
        int sepX = 0, sepY = 0;      // away from anyone too close
        int aliX = 0, aliY = 0;      // the sum of neighbours' headings
        int cohX = 0, cohY = 0;      // ...and of their positions
        int neighbours = 0;

        for (int j = 0; j < n_; ++j) {
            if (j == i) continue;
            const int dx = px_[j] - px_[i];
            const int dy = py_[j] - py_[i];
            const int d = dist(dx, dy);
            if (d > kNeiR) continue;
            aliX += vx_[j]; aliY += vy_[j];
            cohX += px_[j]; cohY += py_[j];
            ++neighbours;
            if (d < sepR) {
                // Shove weighted by how close it got: a boid on top of another is pushed
                // hard, one at the edge of the radius barely at all. Without the weight
                // separation is a step function and the swarm buzzes.
                const int push = sepR - d;
                sepX -= dx * push / sepR;
                sepY -= dy * push / sepR;
            }
        }

        int ax = 0, ay = 0;
        ax += sepX * m.sep / kW;
        ay += sepY * m.sep / kW;
        if (neighbours > 0) {
            ax += ((aliX / neighbours) - vx_[i]) * m.ali / kW;
            ay += ((aliY / neighbours) - vy_[i]) * m.ali / kW;
            ax += ((cohX / neighbours) - px_[i]) * m.coh / kW / 8;
            ay += ((cohY / neighbours) - py_[i]) * m.coh / kW / 8;
        }
        // The lure. Divided down hard because it acts over the whole cell rather than
        // over a neighbourhood — undivided it out-pulls the three rules and the flock
        // becomes a ball on a string.
        ax += (lureX_ - px_[i]) * m.lure / kW / 32;
        ay += (lureY_ - py_[i]) * m.lure / kW / 32;

        // The churn (see the table). Sized per mood rather than fixed, because how
        // UNSETTLED a thing is is most of what a mood looks like from outside — and
        // because without it the three rules have a stable solution and the swarm walks
        // straight into it and stops being a creature.
        const int churn = 2 * m.churn + 1;
        ax += static_cast<int>(roll(static_cast<uint32_t>(churn))) - m.churn;
        ay += static_cast<int>(roll(static_cast<uint32_t>(churn))) - m.churn;

        vx_[i] += ax;
        vy_[i] += ay;
        capTo(vx_[i], vy_[i], m.speed);
        px_[i] += vx_[i];
        py_[i] += vy_[i];

        // The cell wall. A BOUNCE and not a wrap: a boid crossing the edge to reappear
        // opposite tears the body in half, and the body is the only reason the marks read
        // as a creature. The lure keeps this rare — it is the floor, not the shape.
        if (px_[i] < 0)   { px_[i] = 0;   vx_[i] = -vx_[i] / 2; }
        if (px_[i] > w_)  { px_[i] = w_;  vx_[i] = -vx_[i] / 2; }
        if (py_[i] < 0)   { py_[i] = 0;   vy_[i] = -vy_[i] / 2; }
        if (py_[i] > h_)  { py_[i] = h_;  vy_[i] = -vy_[i] / 2; }
    }
}

int Flock::centreX() const {
    if (n_ <= 0) return 0;
    int s = 0;
    for (int i = 0; i < n_; ++i) s += px_[i];
    return (s / n_) >> kFlockShift;
}

int Flock::centreY() const {
    if (n_ <= 0) return 0;
    int s = 0;
    for (int i = 0; i < n_; ++i) s += py_[i];
    return (s / n_) >> kFlockShift;
}

int Flock::spread() const {
    if (n_ <= 0) return 0;
    const int cx = centreX() << kFlockShift;
    const int cy = centreY() << kFlockShift;
    int s = 0;
    for (int i = 0; i < n_; ++i) s += dist(px_[i] - cx, py_[i] - cy);
    return (s / n_) >> kFlockShift;
}

}  // namespace mal
