// content_homes.h — where a creature is at home: which engine-drawn backdrop stands
// behind it on the habitat, and behind it in a fight with no place of its own.
//
// An AREA names its backdrop on its own row (AreaDef::scene), because an area IS a
// place. A creature is not, so its home is DERIVED — and from two facts it already
// carries, in this order:
//
//   1. ITS LINE, when that line has earned a place of its own. Phishing is not "a thing
//      that swims", it is the thing on the other end of the line, and the difference is
//      worth a scene. A line with nothing to say here simply isn't in the table.
//   2. ITS LOCOMOTION, which always answers. The habitat already moves a creature the
//      way it gets around (core/model/idle_wander.h) — a swimmer drifts on both axes, a
//      flier holds an altitude, a Ground mover gives up the shelf bob — so this is what
//      keeps the motion and the surroundings agreeing. A swimmer sculling over a plank
//      floor is the specific thing this table exists to prevent.
//
// Deliberately NOT a field on CreatureDef. Thirty-five rows would each be restating
// their line's answer, and the one fact worth authoring per creature — that it swims —
// is on the row already. A creature that some day wants its own place against both of
// these gains a row here, which is the same edit and visible beside its neighbours.
#pragma once

#include <cstring>

#include "core/content/defs.h"
#include "core/render/scene_id.h"

namespace mal {

// The lines with a place of their own. Everything not named here falls through to how
// the creature moves, which is the answer for most of the roster and is not a gap.
struct LineHome {
    const char* line;
    SceneId scene;
};
inline constexpr LineHome kLineHomes[] = {
    // Phishing lives under a pier with a lure hanging in it — the water is the setting,
    // the bait is the point, and half this line walks anyway.
    {"phishing", SceneId::BaitShallows},
    // Ransomware is kept behind someone else's fence, looking at what it has shut.
    {"ransomware", SceneId::RansomLot},
};

// How a creature moves, and therefore what is around it when nothing more specific is
// known. Every Locomotion answers — this is the floor of the chain, not a default that
// might be missing.
inline constexpr SceneId sceneForLocomotion(Locomotion loco) {
    switch (loco) {
        case Locomotion::Swim: return SceneId::KelpDrift;
        case Locomotion::Fly:  return SceneId::CirrusDeck;
        case Locomotion::Ground: return SceneId::StrataBurrow;
        // Walk and Static share the yard. An egg is not going anywhere, and where it
        // sits is where the thing inside it will stand once it can.
        default: return SceneId::ServerYard;
    }
}

// The chain: the creature's line if that line has a place, otherwise how it moves.
inline SceneId sceneForCreature(const CreatureDef& c) {
    if (c.line)
        for (const LineHome& h : kLineHomes)
            if (std::strcmp(h.line, c.line) == 0) return h.scene;
    return sceneForLocomotion(c.locomotion);
}

}  // namespace mal
