// idle_vanish.h: one of a shapeshifting line's resting flourishes
// (RestingTrick::Shapeshift). Every so often the pet takes on the habitat behind it until
// all that is left is a ripple where it stands and a glint along its edge, holds there,
// and comes back: code that matches its surroundings is code nothing flags.
//
// Same split as IdleCamo: this half decides when and how far gone, and the habitat
// draws it (FX_CLOAK, core/render/cloak.h). It takes turns with the mimic
// (core/model/idle_mimic.h) and the colour drift (core/model/idle_camo.h): the tick steps
// each only while the others are at rest.
#pragma once

#include <cstdint>

namespace mal {

// In heartbeats (~4 per second). ~100s apart, with the mimic halfway between.
constexpr int kIdleVanishRestBeats = 400;
constexpr int kIdleVanishFadeBeats = 6;
constexpr int kIdleVanishHoldBeats = 16;

class IdleVanish {
public:
    void step(bool shifter) {
        if (!shifter) { beat_ = 0; return; }
        if (++beat_ >= kCycleBeats) beat_ = 0;
    }

    bool active() const { return beat_ >= kFadeFrom; }

    // How far gone, 0 (all there) to 255 (only the ripple and the glint).
    uint8_t amount() const {
        if (!active()) return 0;
        const int b = beat_ - kFadeFrom;
        int a;
        if (b < kIdleVanishFadeBeats) a = 255 * (b + 1) / kIdleVanishFadeBeats;
        else if (b < kIdleVanishFadeBeats + kIdleVanishHoldBeats) a = 255;
        else a = 255 * (kCycleBeats - beat_ - 1) / kIdleVanishFadeBeats;
        return static_cast<uint8_t>(a);
    }

private:
    static constexpr int kFadeFrom = kIdleVanishRestBeats;
    static constexpr int kCycleBeats =
        kFadeFrom + 2 * kIdleVanishFadeBeats + kIdleVanishHoldBeats;

    int beat_ = 0;
};

}  // namespace mal
