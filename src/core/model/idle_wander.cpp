#include "core/model/idle_wander.h"

namespace mal {
namespace {

// What one mover's trip looks like: logical px per heartbeat while under way, and
// heartbeats parked between trips. Only the wander spends these, so the magnitudes
// sit with it rather than in tunables.h. The shape of each row IS the locomotion —
// a walker takes long pauses and never leaves the shelf (stepY 0), a flier is almost
// always gliding somewhere, a swimmer drifts on both axes at a single slow pace, and
// a Ground mover is a walker slowed down: half the pace over a shorter reach, resting
// longer at the end of it, and giving up the shelf bob entirely.
struct Pace {
    int stepX;     // horizontal px per heartbeat
    int stepY;     // vertical px per heartbeat; 0 pins the mover to the shelf
    int restMin;   // heartbeats parked on arrival...
    int restSpan;  // ...plus 0..restSpan-1 more
    int reach;     // how far one trip may carry it sideways
};

constexpr Pace kPaces[] = {
    /* Walk  */ {2, 0, 6, 12, 10},
    /* Fly   */ {2, 1, 1,  4, 12},
    /* Swim  */ {1, 1, 0,  5, 12},
    /* Ground*/ {1, 0, 10, 14, 7},
};
static_assert(static_cast<int>(Locomotion::Ground) + 1 ==
                  static_cast<int>(sizeof(kPaces) / sizeof(kPaces[0])),
              "one Pace row per Locomotion");

// A flier's cruising altitude starts here, so the low end of its band is still well
// clear of the shelf. Its rare low pass (kLowPassOdds) drops under this, and even
// that bottoms out above the floor: something that flies does not land to rest.
constexpr int kCruiseFloor = kWanderRiseMax / 2;
constexpr int kLowPassOdds = 8;    // one trip in this many dips below the cruise
constexpr int kLowPassFloor = 2;   // ...and no lower than this off the shelf
constexpr int kSwimDrift = 6;      // px of depth change one swim trip commits to

int clampTo(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

// One step toward `delta`, capped at `speed` and never overshooting.
int approach(int delta, int speed) {
    if (delta > speed) return speed;
    if (delta < -speed) return -speed;
    return delta;
}

}  // namespace

uint32_t IdleWander::roll(uint32_t span) {
    rng_ = rng_ * 1664525u + 1013904223u;
    return span ? (rng_ >> 16) % span : 0;
}

int IdleWander::hop(int from, int reach, int lo, int hi) {
    // A real distance, never a one-px twitch: a trip the eye can't see is worse than
    // no trip at all. Running into the side of the box turns the trip around instead
    // of clamping it away, which is what stops a mover pressed against a wall from
    // standing there re-rolling moves it cannot take.
    const int least = reach / 3 + 1;
    int dist = least + static_cast<int>(roll(static_cast<uint32_t>(reach - least + 1)));
    if (roll(2)) dist = -dist;
    if (from + dist > hi || from + dist < lo) dist = -dist;
    return clampTo(from + dist, lo, hi);
}

void IdleWander::park() {
    x_ = y_ = 0;
    targetX_ = targetY_ = 0;
    rest_ = 0;
}

void IdleWander::retarget() {
    const Pace& p = kPaces[static_cast<int>(loco_)];

    // Sideways trips are relative to where the creature already is, so it ambles
    // across its box over several moves instead of teleporting the anchor from one
    // random side to the other.
    targetX_ = hop(x_, p.reach, -kWanderHalfSpanX, kWanderHalfSpanX);

    switch (loco_) {
        case Locomotion::Walk:
        case Locomotion::Ground:
            targetY_ = 0;   // the shelf is the whole of a floor-mover's world
            break;
        case Locomotion::Fly:
            targetY_ = roll(kLowPassOdds) == 0
                           ? kLowPassFloor + static_cast<int>(roll(kCruiseFloor))
                           : kCruiseFloor +
                                 static_cast<int>(roll(kWanderRiseMax - kCruiseFloor + 1));
            break;
        case Locomotion::Swim:
            // Nothing pulls a swimmer anywhere, so its depth wanders the same way its
            // x does: from wherever it happens to be.
            targetY_ = hop(y_, kSwimDrift, kLowPassFloor, kWanderRiseMax);
            break;
    }
}

void IdleWander::step(Locomotion loco) {
    if (loco != loco_) {   // a different creature, moving a different way
        loco_ = loco;
        park();
    }
    if (rest_ > 0) { --rest_; return; }

    const Pace& p = kPaces[static_cast<int>(loco_)];
    const int dx = approach(targetX_ - x_, p.stepX);
    const int dy = approach(targetY_ - y_, p.stepY);
    if (dx == 0 && dy == 0) {
        rest_ = p.restMin + static_cast<int>(roll(p.restSpan));
        retarget();
        return;
    }
    x_ = clampTo(x_ + dx, -kWanderHalfSpanX, kWanderHalfSpanX);
    y_ = clampTo(y_ + dy, 0, kWanderRiseMax);
}

}  // namespace mal
