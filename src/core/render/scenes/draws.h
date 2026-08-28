// draws.h — one declaration per authored place, for the catalogue that indexes them.
//
// ADDING A PLACE is four lines and one file: a row on SceneId (render/scene_id.h), a
// declaration here, a row in the catalogue table (render/scenes.cpp), and the file
// beside this one that draws it. The table is length-checked against SceneId::Count, so
// the third of those cannot be forgotten.
//
// WHAT A SCENE FILE LOOKS LIKE. An anonymous namespace of art-direction tables — the
// tones, the silhouette, where the light is — and then a draw function that is mostly
// primitive calls in back-to-front order: specks, disc, glow, silhouette, middle, drift,
// floor, posts. A reader should be able to name the place from the tables alone, before
// reading a single call. The machinery is core/render/scene.h and the house style is
// pirate_bayou.cpp beside this file.
#pragma once

#include "core/render/scene.h"

namespace mal {

class Framebuffer;

void drawCitrusCircuitScene(Framebuffer& fb, int beat, const SceneGround& g);
void drawPirateBayouScene(Framebuffer& fb, int beat, const SceneGround& g);
void drawCastleRapidscareScene(Framebuffer& fb, int beat, const SceneGround& g);
void drawGridHorizonScene(Framebuffer& fb, int beat, const SceneGround& g);
void drawMainframeRowScene(Framebuffer& fb, int beat, const SceneGround& g);
void drawTheLineScene(Framebuffer& fb, int beat, const SceneGround& g);
void drawCrtBenchScene(Framebuffer& fb, int beat, const SceneGround& g);
void drawGroundStationScene(Framebuffer& fb, int beat, const SceneGround& g);
void drawTraceCityScene(Framebuffer& fb, int beat, const SceneGround& g);
void drawKelpDriftScene(Framebuffer& fb, int beat, const SceneGround& g);
void drawBaitShallowsScene(Framebuffer& fb, int beat, const SceneGround& g);
void drawCirrusDeckScene(Framebuffer& fb, int beat, const SceneGround& g);
void drawStrataBurrowScene(Framebuffer& fb, int beat, const SceneGround& g);
void drawServerYardScene(Framebuffer& fb, int beat, const SceneGround& g);
void drawRansomLotScene(Framebuffer& fb, int beat, const SceneGround& g);

}  // namespace mal
