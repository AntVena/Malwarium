#include "core/render/scenes.h"

#include <cstring>

#include "core/render/framebuffer.h"
#include "core/render/scenes/draws.h"

namespace mal {

namespace {

struct SceneRow {
    const char* name;
    SceneDraw draw;
};

// The catalogue, in SceneId order — the enum indexes this directly, so a row out of
// order points a name at another place's drawing. The static_assert below is what
// stops a new id from being added without one.
constexpr SceneRow kScenes[] = {
    {"", nullptr},                                          // None
    {"citrus_circuit", &drawCitrusCircuitScene},
    {"pirate_bayou", &drawPirateBayouScene},
    {"castle_rapidscare", &drawCastleRapidscareScene},
    {"grid_horizon", &drawGridHorizonScene},
    {"mainframe_row", &drawMainframeRowScene},
};
static_assert(sizeof(kScenes) / sizeof(kScenes[0]) == static_cast<int>(SceneId::Count),
              "every SceneId needs a catalogue row, and only its own");

const SceneRow& row(SceneId id) {
    const int i = static_cast<int>(id);
    return kScenes[(i < 0 || i >= static_cast<int>(SceneId::Count)) ? 0 : i];
}

}  // namespace

SceneDraw sceneFor(SceneId id) { return row(id).draw; }

const char* sceneName(SceneId id) { return row(id).name; }

SceneId sceneByName(const char* name) {
    if (!name || !*name) return SceneId::None;
    for (int i = 1; i < static_cast<int>(SceneId::Count); ++i)
        if (std::strcmp(kScenes[i].name, name) == 0) return static_cast<SceneId>(i);
    return SceneId::None;
}

bool drawScene(Framebuffer& fb, SceneId id, int beat, const SceneGround& g) {
    const SceneDraw d = sceneFor(id);
    if (!d) return false;
    d(fb, beat, g);
    return true;
}

}  // namespace mal
