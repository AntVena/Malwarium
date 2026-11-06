// idle_fork.h: the resting flourish of a line that replicates (LinePassive::Replication).
//
// Every so often the pet replicates past its cap: a copy crawls out every couple of
// seconds into the seats past the last slot, the overflow looks around, and then every
// extra dies on the same beat with the dissolve a copy killed in a fight plays. Ending all
// at once is what makes it read as the cap enforcing itself.
//
// Same split as IdleCamo: this half decides when and how far through, and the habitat
// decides where and with which glyph.
#pragma once

#include <cstdint>

namespace mal {

// In heartbeats (~4 per second). Bursts ~45s apart and ~7s long. A copy is born every
// kIdleForkSpawnBeats and crawls to its seat in the same span, so one has arrived as the
// next appears. Die is the glyph's two dissolve frames, once each.
constexpr int kIdleForkRestBeats = 180;
constexpr int kIdleForkExtras = 3;
constexpr int kIdleForkSpawnBeats = 6;
constexpr int kIdleForkLookBeats = 8;
constexpr int kIdleForkDieBeats = 2;

enum class ForkStage : uint8_t { Rest, Spawn, Look, Die };

class IdleFork {
public:
    // Passed every beat for the reason IdleCamo::step gives: a hatch, evolution or load
    // just starts arriving with a different answer.
    void step(bool forker) {
        if (!forker) { beat_ = 0; return; }
        if (++beat_ >= kCycleBeats) beat_ = 0;
    }

    ForkStage stage() const {
        if (beat_ < kSpawnFrom) return ForkStage::Rest;
        if (beat_ < kLookFrom) return ForkStage::Spawn;
        if (beat_ < kDieFrom) return ForkStage::Look;
        return ForkStage::Die;
    }

    // 0-based beat within stage(). Rest's value is meaningless and never read.
    int beatInStage() const {
        switch (stage()) {
            case ForkStage::Spawn: return beat_ - kSpawnFrom;
            case ForkStage::Look:  return beat_ - kLookFrom;
            case ForkStage::Die:   return beat_ - kDieFrom;
            case ForkStage::Rest:  break;
        }
        return beat_;
    }

    // How many extras are on the shelf: 0 at rest, rising through Spawn, all of them after.
    int extras() const {
        switch (stage()) {
            case ForkStage::Rest:  return 0;
            case ForkStage::Spawn: return beatInStage() / kIdleForkSpawnBeats + 1;
            case ForkStage::Look:
            case ForkStage::Die:   break;
        }
        return kIdleForkExtras;
    }

    // Beats since extra k was born, capped at kIdleForkSpawnBeats once it has arrived.
    int crawlBeat(int k) const {
        if (stage() != ForkStage::Spawn) return kIdleForkSpawnBeats;
        const int age = beatInStage() - k * kIdleForkSpawnBeats;
        return age < kIdleForkSpawnBeats ? age : kIdleForkSpawnBeats;
    }

private:
    static constexpr int kSpawnFrom = kIdleForkRestBeats;
    static constexpr int kLookFrom = kSpawnFrom + kIdleForkExtras * kIdleForkSpawnBeats;
    static constexpr int kDieFrom = kLookFrom + kIdleForkLookBeats;
    static constexpr int kCycleBeats = kDieFrom + kIdleForkDieBeats;

    int beat_ = 0;
};

}  // namespace mal
