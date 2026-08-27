// scenes.h — the backdrop CATALOGUE: which SceneId is drawn by which function.
//
// One table, in scenes.cpp, is the whole dispatch. A new place is a file under
// render/scenes/, a row on SceneId (render/scene_id.h) and a row here — and because the
// table is indexed by the enum, an id with no drawing is a build failure rather than a
// screen that comes up empty.
//
// WHO NAMES ONE. An area names its own on AreaDef::scene, beside the sector glyph it
// already names. A prize background names one from wherever prizes are held. Neither
// owner knows anything about drawing; both hold an id and hand it here with the ground
// their screen composes on (core/render/scene.h).
#pragma once

#include "core/render/scene.h"
#include "core/render/scene_id.h"

namespace mal {

class Framebuffer;

// What every scene is. The ground comes LAST because it is the parameter a caller
// varies — the same place drawn under a fighter's feet and under a resting pet.
using SceneDraw = void (*)(Framebuffer& fb, int beat, const SceneGround& g);

// The drawing for an id, or nullptr for SceneId::None — which is the answer for a place
// whose backdrop is not authored yet, not an error.
SceneDraw sceneFor(SceneId id);

// The place's own short name, lower_snake and matching its file under render/scenes/.
// What a tool addresses a backdrop by (tools/dump_frame.cpp) and what a gate names in a
// failure; empty for None.
const char* sceneName(SceneId id);

// The id a name addresses, or None if nothing answers to it.
SceneId sceneByName(const char* name);

// Paint `id` over the whole active canvas, and say whether it drew anything. The one
// call a screen makes: a screen with no backdrop passes None and takes `false` back,
// so nothing on the drawing side has to branch on whether a place exists.
bool drawScene(Framebuffer& fb, SceneId id, int beat, const SceneGround& g);

}  // namespace mal
