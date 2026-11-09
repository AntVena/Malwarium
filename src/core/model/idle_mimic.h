// idle_mimic.h: one of a shapeshifting line's resting flourishes
// (RestingTrick::Shapeshift). Every so often a scanline wipes down the pet and leaves
// another family's creature standing in its place; it holds the form, and a second wipe
// gives the pet back. Not the Trojan's disguise: that is malware passing as something
// harmless, and this is malware passing as other malware.
//
// Same split as IdleCamo: this half decides when, how far the wipe is, and which slot of
// the rotation is up; the habitat resolves the slot to a creature (Game::idleCamoSprite)
// and draws the wipe. It takes turns with the vanish (core/model/idle_vanish.h) and the
// colour drift, and starts its clock half a rest ahead so it falls between vanishes.
#pragma once

#include <cstdint>

namespace mal {

// In heartbeats (~4 per second).
constexpr int kIdleMimicRestBeats = 400;
constexpr int kIdleMimicWipeBeats = 4;
constexpr int kIdleMimicHoldBeats = 16;

enum class MimicStage : uint8_t { Rest, WipeIn, Hold, WipeOut };

class IdleMimic {
public:
    void step(bool shifter) {
        if (!shifter) { beat_ = kFirstBeat; slot_ = 0; return; }
        if (++beat_ >= kCycleBeats) beat_ = 0;
        if (beat_ == kWipeInFrom) ++slot_;
    }

    MimicStage stage() const {
        if (beat_ < kWipeInFrom) return MimicStage::Rest;
        if (beat_ < kHoldFrom) return MimicStage::WipeIn;
        if (beat_ < kWipeOutFrom) return MimicStage::Hold;
        return MimicStage::WipeOut;
    }
    bool active() const { return stage() != MimicStage::Rest; }

    // How far down the pet a wipe has come, 1..kIdleMimicWipeBeats of kIdleMimicWipeBeats.
    // Meaningless outside the wipes.
    int wipe() const {
        return beat_ - (stage() == MimicStage::WipeIn ? kWipeInFrom : kWipeOutFrom) + 1;
    }

    // Which turn of the rotation is up, counting from the first mimic. The habitat takes
    // it modulo the families there are.
    int slot() const { return slot_; }

private:
    static constexpr int kWipeInFrom = kIdleMimicRestBeats;
    static constexpr int kHoldFrom = kWipeInFrom + kIdleMimicWipeBeats;
    static constexpr int kWipeOutFrom = kHoldFrom + kIdleMimicHoldBeats;
    static constexpr int kCycleBeats = kWipeOutFrom + kIdleMimicWipeBeats;
    static constexpr int kFirstBeat = kIdleMimicRestBeats / 2;

    int beat_ = kFirstBeat;
    int slot_ = 0;
};

}  // namespace mal
