// home_habitat.h: what the home habitat (the Idle and Cursor screens) animates around the
// resting pet: where it and its ambient copies are standing, and the line's resting
// tricks (a disguise, a harvest, a skim and its lure, a shapeshifter's drift, vanish and
// mimic, a worm's fork and relay). Purely presentational, so none of it is saved.
//
// Stepped once per heartbeat (Game::tickHeartbeat), which hands it what this beat's pet
// is and does; the habitat draw (Game::drawHabitat and its helpers) reads it back.
#pragma once

#include <cstdint>

#include "core/content/content_passives.h"   // kWormReplicaSlots
#include "core/content/defs.h"
#include "core/model/idle_camo.h"
#include "core/model/idle_disguise.h"
#include "core/model/idle_feeding.h"
#include "core/model/idle_fork.h"
#include "core/model/idle_harvest.h"
#include "core/model/idle_lure.h"
#include "core/model/idle_mimic.h"
#include "core/model/idle_relay.h"
#include "core/model/idle_vanish.h"
#include "core/model/idle_wander.h"

namespace mal {

struct SpriteData;

// The art a borrowed-colour drift or a mimic wears, resolved by family slot. A registry
// walk, so the habitat asks once per drift rather than once per beat.
class HomeHost {
public:
    virtual const SpriteData* idleCamoSprite(int slot) const = 0;

protected:
    ~HomeHost() = default;
};

// What this beat's pet is and does.
struct HabitatBeat {
    const CreatureDef* creature = nullptr;   // null: an empty save, so everything parks
    RestingTrick trick = RestingTrick::None;
    int companions = 0;                      // ambient copies live beside the pet
    bool replicates = false;                 // whether it forks at all
    bool wearsBorrowedColours = false;       // a hatched pet whose line drifts colour
};

class HomeHabitat {
public:
    HomeHabitat();

    void step(const HabitatBeat& beat, const HomeHost& host);

    // Where the pet is standing, in logical px off the shelf anchor.
    const IdleWander& wander() const { return wander_; }
    // Where ambient copy `i` is standing, same units. Each keeps its own stream so the
    // group never marches in step.
    const IdleWander& companion(int i) const { return companions_[i]; }
    // The burst of copies past the cap that crawls out and dies now and then, and the
    // byte the copies pass along between forks.
    const IdleFork& fork() const { return fork_; }
    const IdleRelay& relay() const { return relay_; }
    // The prop a disguising line turns into, and the data a harvesting line holds.
    const IdleDisguise& disguise() const { return disguise_; }
    const IdleHarvest& harvest() const { return harvest_; }
    // The mail a skimming line chases down and eats, and the bait it sets between skims.
    const IdleFeeding& feeding() const { return feeding_; }
    const IdleLure& lure() const { return lure_; }
    // A shapeshifting line's colour drift, its fade into the habitat and its wipe into
    // another family's creature. The worn sprites are null whenever nothing is worn.
    const IdleCamo& camo() const { return camo_; }
    const SpriteData* camoWorn() const { return camoWorn_; }
    const IdleVanish& vanish() const { return vanish_; }
    const IdleMimic& mimic() const { return mimic_; }
    const SpriteData* mimicWorn() const { return mimicWorn_; }

private:
    void stepMotion(const HabitatBeat& beat);
    void stepTricks(const HabitatBeat& beat, const HomeHost& host);

    IdleWander wander_;
    IdleWander companions_[kWormReplicaSlots];
    IdleFork fork_;
    IdleRelay relay_;
    IdleDisguise disguise_;
    IdleHarvest harvest_;
    IdleFeeding feeding_;
    IdleLure lure_;
    IdleCamo camo_;
    const SpriteData* camoWorn_ = nullptr;
    IdleVanish vanish_;
    IdleMimic mimic_;
    const SpriteData* mimicWorn_ = nullptr;
};

}  // namespace mal
