// swarm_view.h: a core/model Flock as the swarm renderer sees it (core/render/swarm.h).
//
// The copy is deliberate: core/render never reaches into core/model, so a renderer is
// handed a value that owns its marks, the same call CamoRamp makes. It is a couple of
// hundred bytes on a repaint that is already composing a whole panel. The guardian's hail
// and verdict and the fight's rival seat all project the meeting's flock through here.
#pragma once

#include "core/model/flock.h"
#include "core/render/swarm.h"

namespace mal {

SwarmView swarmViewOf(const Flock& f);

}  // namespace mal
