// scene_id.h — the name of a place, on its own, so a row can hold one without holding
// the code that draws it.
//
// A backdrop is NAMED the way an area's glyph is named (AreaDef::icon) and for the same
// reasons: keyed off identity rather than off a ladder position, so splicing an area
// into the middle of kAreaList cannot silently re-point a place at its neighbour's
// picture. What it is not is an asset id, because a scene is CODE — a table and a
// handful of primitive calls — and a name that does not resolve should therefore be a
// compile error rather than a blank screen. Hence an enum, and one lookup
// (render/scenes.h) rather than a function pointer sitting on a content row.
//
// It also cannot hang off AreaDef alone: a prize backdrop belongs to no area at all,
// and half the roster below is exactly that. The id is the seam both kinds of owner
// name, and the catalogue is where a name meets its drawing.
//
// `None` is a real answer — a place whose backdrop is not authored yet draws nothing
// and the screen keeps its `paper` field, which is what every screen showed before any
// of this existed.
#pragma once

#include <cstdint>

namespace mal {

enum class SceneId : uint8_t {
    None = 0,
    // The explore ladder, in ladder order.
    CitrusCircuit,
    PirateBayou,
    CastleRapidscare,
    // Prize backdrops, which no area names.
    GridHorizon,
    MainframeRow,
    Count
};

}  // namespace mal
