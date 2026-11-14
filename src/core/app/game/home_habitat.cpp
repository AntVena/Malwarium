#include "core/app/game/home_habitat.h"

namespace mal {

HomeHabitat::HomeHabitat() {
    // Identical wanders walk identical paths, so the seed is what tells the copies apart.
    for (int i = 0; i < kWormReplicaSlots; ++i)
        companions_[i].seed(0x2545f491u + 0x9e3779b9u * static_cast<uint32_t>(i + 1));
}

void HomeHabitat::step(const HabitatBeat& beat, const HomeHost& host) {
    stepMotion(beat);
    stepTricks(beat, host);
}

// Resting motion: the pet drifts a little way around the shelf anchor, the way its
// species moves (core/model/idle_wander.h). An egg moves the way its row says too, which
// for almost every egg is Locomotion::Static, so it sits where it was laid under its
// incubation readout; an egg adrift in water says Swim and drifts. An empty save parks.
// A worm's copies walk the same shelf on the same beat, each off its own stream; the ones
// past the live count park, so a shrinking family leaves nothing mid-stride to walk back
// in. A pet passing itself off as a prop holds still, as does an angler with its bait
// out, and a skimming mail goes where the scrap it is after is.
void HomeHabitat::stepMotion(const HabitatBeat& beat) {
    // The skim and the lure take turns: each runs only while the other rests.
    const bool skims = beat.trick == RestingTrick::Skim;
    if (!lure_.active()) feeding_.step(skims, wander_.offsetX(), wander_.offsetY());
    if (!feeding_.active()) lure_.step(skims);
    if (!beat.creature) {
        wander_.park();
    } else if (const Mail* m = feeding_.target()) {
        wander_.chase(beat.creature->locomotion, IdleFeeding::standX(*m),
                      IdleFeeding::standY(*m), IdleFeeding::faceRight(*m));
    } else if (disguise_.stage() == DisguiseStage::Rest && !lure_.active()) {
        wander_.step(beat.creature->locomotion);
    }
    for (int i = 0; i < kWormReplicaSlots; ++i) {
        if (i < beat.companions) companions_[i].step(beat.creature->locomotion);
        else companions_[i].park();
    }
    fork_.step(beat.replicates);
    relay_.step(fork_.stage() == ForkStage::Rest ? beat.companions : 0);
}

// The line's resting trick, and the resting colour. A line that wears borrowed colours
// drifts into another family's palette now and then and comes back
// (core/model/idle_camo.h).
void HomeHabitat::stepTricks(const HabitatBeat& beat, const HomeHost& host) {
    disguise_.step(beat.trick == RestingTrick::Disguise);
    harvest_.step(beat.trick == RestingTrick::Harvest);
    // A shapeshifter's colour drift, vanish and mimic take turns: each runs only while
    // the others rest. The creature a mimic wears is resolved as it begins, like the
    // drift's.
    const bool shifts = beat.trick == RestingTrick::Shapeshift;
    const bool drifting = camo_.level() > 0;
    if (!drifting && !mimic_.active()) vanish_.step(shifts);
    if (!drifting && !vanish_.active()) mimic_.step(shifts);
    if (!mimic_.active()) mimicWorn_ = nullptr;
    else if (!mimicWorn_) mimicWorn_ = host.idleCamoSprite(mimic_.slot());
    if (!vanish_.active() && !mimic_.active()) camo_.step(beat.wearsBorrowedColours);
    // The art is resolved once per drift, on the beat the level leaves zero: the level
    // moving is not a change of who is being sampled.
    if (camo_.level() == 0) camoWorn_ = nullptr;
    else if (!camoWorn_) camoWorn_ = host.idleCamoSprite(camo_.slot());
}

}  // namespace mal
