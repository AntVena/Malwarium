// idle_camo.h — the ambient colour drift a resting pet plays in the idle habitat.
//
// The Metamorphic line's whole argument is that it wears somebody else's colours
// (CreatureLine::wearsBorrowedColours, content/defs.h). A fight makes that visible the
// moment the pet's live cast is a borrowed one (FX_CAMO, core/render/camo.h) and the
// CHROMATOPHORE hatch board scores it — but the habitat is where a player actually
// watches their pet, and there is no cast on that screen to read the colours off. So the
// resting creature rehearses the trick: every so often it drifts into another family's
// palette, holds it a few seconds, and comes back.
//
// A CADENCE, not a reaction. Nothing on the idle screen sets this off and nothing
// interrupts it — it is the animal doing what the animal does, on a clock long enough
// that catching it reads as having caught it AT something rather than as the panel
// glitching. Idle for the rest of the time it costs one counter.
//
// This half only decides WHEN, and which slot of the rotation is up; whose colours a
// slot names is content, resolved by the habitat (Game::idleCamoSprite) into the art
// being sampled. Combat splits the same way and for the same reason — a level plus a
// sprite is standing state, and turning a sprite into tones is a scan the draw already
// makes once per repaint.
//
// The clock is the shared heartbeat, so a pet drifts at the same rate whatever else the
// device is doing, and the counter runs whether or not the habitat is the screen up —
// the pet is not performing for the viewer.
#pragma once

#include <cstdint>

#include "core/render/camo.h"   // camoAdvance — one grain of scatter for every FX_CAMO

namespace mal {

// The two halves of one cycle, in heartbeats (kHeartbeatMs, ~4 per second): how long the
// pet holds its own colours, then how long a drift lasts. ~40s apart and ~6s long — rare
// enough that the borrowed palette never reads as the creature's own, and long enough
// that the scatter arriving (four ticks of kCamoFadeStep) is the edge of the thing seen
// rather than the whole of it.
constexpr int kIdleCamoRestBeats = 160;
constexpr int kIdleCamoWearBeats = 24;

class IdleCamo {
public:
    // Advance one heartbeat. `wearer` is whether the creature on the shelf is one that
    // does this at all, passed every beat rather than latched at a reset for the reason
    // IdleWander::step gives: a hatch, an evolution or a loaded save simply starts
    // arriving with a different answer, and there is one call site either way.
    void step(bool wearer) {
        if (!wearer) { beat_ = 0; slot_ = 0; level_ = 0; return; }
        const bool wasWorn = worn();
        if (++beat_ >= kIdleCamoRestBeats + kIdleCamoWearBeats) beat_ = 0;
        // The slot turns over as a drift BEGINS, never as one ends: the level takes four
        // more ticks to fall back out of the palette it was wearing, and a slot that
        // moved on at the top of the rest would spend them dissolving out of colours the
        // pet was never in.
        if (worn() && !wasWorn) ++slot_;
        level_ = camoAdvance(level_, worn());
    }

    // How much of the borrowed palette the pet is wearing, 0 (its own colours) to 255 —
    // drawSpriteCamo's own level, eased on the same kCamoFadeStep grain as the fight's.
    uint8_t level() const { return level_; }

    // Which turn of the rotation is up, counting from the pet's first drift. Only ever
    // read while something is worn, and free to run past the roster: the habitat takes
    // it modulo the families there actually are, so nothing here knows the count.
    int slot() const { return slot_; }

private:
    // Inside the WEAR half of the cycle. The level follows this rather than being
    // scheduled itself, so the rise and the fall are the same four ticks every other
    // FX_CAMO caller gets.
    bool worn() const { return beat_ >= kIdleCamoRestBeats; }

    int beat_ = 0;
    int slot_ = 0;
    uint8_t level_ = 0;
};

}  // namespace mal
