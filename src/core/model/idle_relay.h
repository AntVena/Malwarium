// idle_relay.h: the replicating line's second resting flourish. The pet spits a byte
// that hops copy to copy in the order the copies were made, each one chomping as it
// catches it, and the last keeps it. It shows the family as a network passing something
// along.
//
// It runs only inside the fork's rest (core/model/idle_fork.h): the habitat steps it
// with 0 copies whenever a fork is out, which resets it, so the two never share the
// shelf.
#pragma once


#include "core/content/content_passives.h"   // kWormReplicaSlots: the most hops a relay has
#include "core/model/idle_fork.h"

namespace mal {

// In heartbeats (~4 per second). A relay starts ~17s into a quiet shelf; a hop is long
// enough to read as an arc at the panel's frame rate, and the chomp is the glyph's two
// attack frames.
constexpr int kIdleRelayRestBeats = 70;
constexpr int kIdleRelayHopBeats = 4;
constexpr int kIdleRelayChompBeats = 2;

class IdleRelay {
public:
    // Passed every beat: how many copies are standing to pass it along. 0 resets.
    void step(int copies) {
        if (copies <= 0) { beat_ = 0; hops_ = 0; return; }
        if (beat_ == kIdleRelayRestBeats - 1) hops_ = copies;   // latched for the whole run
        ++beat_;
        if (beat_ >= kIdleRelayRestBeats &&
            beat_ - kIdleRelayRestBeats >= hops_ * kIdleRelayHopBeats + kIdleRelayChompBeats) {
            beat_ = 0;
            hops_ = 0;
        }
    }

    // Which hop is in the air (0 leaves the parent and lands on copy 0), or -1 when the
    // byte is not flying.
    int hop() const {
        const int t = runBeat();
        if (t < 0 || t >= hops_ * kIdleRelayHopBeats) return -1;
        return t / kIdleRelayHopBeats;
    }

    // Beats into hop(), 0..kIdleRelayHopBeats-1. Read only while hop() >= 0.
    int beatInHop() const { return runBeat() % kIdleRelayHopBeats; }

    // Which copy is chomping, or -1: the one just landed on, for the first
    // kIdleRelayChompBeats of the next hop or of the tail after the last.
    int chomping() const {
        const int t = runBeat();
        if (t < kIdleRelayHopBeats) return -1;
        const int landed = t / kIdleRelayHopBeats - 1;
        if (landed >= hops_ || t % kIdleRelayHopBeats >= kIdleRelayChompBeats) return -1;
        return landed;
    }

private:
    int runBeat() const { return beat_ - kIdleRelayRestBeats; }

    int beat_ = 0;
    int hops_ = 0;
};

constexpr int kIdleRelayMaxBeats = kWormReplicaSlots * kIdleRelayHopBeats + kIdleRelayChompBeats;

// Two full relays fit inside a fork's rest, so neither is ever cut off by a burst.
static_assert(2 * (kIdleRelayRestBeats + kIdleRelayMaxBeats) <= kIdleForkRestBeats,
              "a relay must finish inside the fork's rest");

}  // namespace mal
