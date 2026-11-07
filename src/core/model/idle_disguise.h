// idle_disguise.h: the resting flourish of a line that disguises itself
// (CreatureLine::disguisesAsProps). Every so often the pet glitches into a harmless prop
// on the shelf, sits still, wobbles, gives itself away with a pair of eyes, and glitches
// back. The props are shared by the whole line (SPR_DISGUISE_*); which one is up turns
// over each time, like IdleCamo's slot.
//
// Same split as IdleCamo: this half decides when and which frame, and the habitat
// decides which prop a slot names and where it sits.
#pragma once

#include <cstdint>

namespace mal {

// In heartbeats (~4 per second). ~50s between disguises; the glitch is two beats each
// way, one with the creature torn and one with the prop torn, so the swap is seen.
constexpr int kIdleDisguiseRestBeats = 200;
constexpr int kIdleDisguiseGlitchBeats = 2;

// The prop's frame on each beat it is on the shelf (0 rest, 1-2 lean, 3 eyes, 4 blink).
// It holds still long enough to pass, wobbles, holds again, then the eyes.
inline constexpr uint8_t kIdleDisguiseScript[] = {
    0, 0, 0, 0, 0, 0, 1, 2, 1, 2, 0, 0, 0, 0, 3, 3, 3, 4, 3, 3, 3, 4, 0, 0, 0,
};
constexpr int kIdleDisguiseHiddenBeats =
    static_cast<int>(sizeof(kIdleDisguiseScript) / sizeof(kIdleDisguiseScript[0]));

enum class DisguiseStage : uint8_t { Rest, GlitchIn, Hidden, GlitchOut };

class IdleDisguise {
public:
    // Passed every beat for the reason IdleCamo::step gives.
    void step(bool disguiser) {
        if (!disguiser) { beat_ = 0; slot_ = 0; return; }
        if (++beat_ >= kCycleBeats) { beat_ = 0; ++slot_; }
    }

    DisguiseStage stage() const {
        if (beat_ < kGlitchInFrom) return DisguiseStage::Rest;
        if (beat_ < kHiddenFrom) return DisguiseStage::GlitchIn;
        if (beat_ < kGlitchOutFrom) return DisguiseStage::Hidden;
        return DisguiseStage::GlitchOut;
    }

    // Is the prop on the shelf this beat, torn or not. False means the creature is.
    bool showsProp() const {
        switch (stage()) {
            case DisguiseStage::Rest:      return false;
            case DisguiseStage::GlitchIn:  return beat_ - kGlitchInFrom >= 1;
            case DisguiseStage::Hidden:    return true;
            case DisguiseStage::GlitchOut: return beat_ - kGlitchOutFrom < 1;
        }
        return false;
    }

    // Is whatever is showing torn by the glitch this beat.
    bool torn() const {
        return stage() == DisguiseStage::GlitchIn || stage() == DisguiseStage::GlitchOut;
    }

    // The prop frame to draw while showsProp(). The rest pose while torn.
    int propFrame() const {
        if (stage() != DisguiseStage::Hidden) return 0;
        return kIdleDisguiseScript[beat_ - kHiddenFrom];
    }

    // Which turn of the prop rotation this is, counting from the first. The habitat
    // takes it modulo the props there are.
    int slot() const { return slot_; }

private:
    static constexpr int kGlitchInFrom = kIdleDisguiseRestBeats;
    static constexpr int kHiddenFrom = kGlitchInFrom + kIdleDisguiseGlitchBeats;
    static constexpr int kGlitchOutFrom = kHiddenFrom + kIdleDisguiseHiddenBeats;
    static constexpr int kCycleBeats = kGlitchOutFrom + kIdleDisguiseGlitchBeats;

    int beat_ = 0;
    int slot_ = 0;
};

}  // namespace mal
