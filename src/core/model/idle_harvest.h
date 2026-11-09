// idle_harvest.h: the resting flourish of a line that holds data hostage
// (RestingTrick::Harvest). Every so often streams of bits flow into the pet from around
// the screen, turning to ciphertext as they near it, and then for a moment the pet is a
// silhouette with the data it took falling inside it.
//
// Same split as IdleCamo: this half decides when and how far through, and the habitat
// decides where the streams come from and how the silhouette is drawn.
#pragma once

#include <cstdint>

namespace mal {

// In heartbeats (~4 per second). ~50s apart. The streams run long enough for every
// strand to cross from the bezel into the pet; the hold is a beat on the silhouette.
constexpr int kIdleHarvestRestBeats = 200;
constexpr int kIdleHarvestStreamBeats = 16;
constexpr int kIdleHarvestHoldBeats = 10;

enum class HarvestStage : uint8_t { Rest, Stream, Hold };

class IdleHarvest {
public:
    // Passed every beat for the reason IdleCamo::step gives.
    void step(bool harvester) {
        if (!harvester) { beat_ = 0; return; }
        if (++beat_ >= kCycleBeats) beat_ = 0;
    }

    HarvestStage stage() const {
        if (beat_ < kStreamFrom) return HarvestStage::Rest;
        if (beat_ < kHoldFrom) return HarvestStage::Stream;
        return HarvestStage::Hold;
    }

    // 0-based beat within stage(). Rest's value is meaningless and never read.
    int beatInStage() const {
        switch (stage()) {
            case HarvestStage::Stream: return beat_ - kStreamFrom;
            case HarvestStage::Hold:   return beat_ - kHoldFrom;
            case HarvestStage::Rest:   break;
        }
        return beat_;
    }

private:
    static constexpr int kStreamFrom = kIdleHarvestRestBeats;
    static constexpr int kHoldFrom = kStreamFrom + kIdleHarvestStreamBeats;
    static constexpr int kCycleBeats = kHoldFrom + kIdleHarvestHoldBeats;

    int beat_ = 0;
};

}  // namespace mal
