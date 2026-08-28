// content_backgrounds.h — the backgrounds an operator can OWN and choose between, and
// what each one is earned by.
//
// A background is a place (core/render/scenes.h) offered as a possession. This table is
// the half of that which is content: the order they are listed in, what each is CALLED
// where a person reads it, and the one line that says how it is come by. Which pixels
// it is made of stays in the render layer, and the rule that decides whether it is
// owned yet stays in Game — because that rule is a question about a save.
//
// OWNERSHIP IS NOT STORED. Every background is earned by something the save already
// records: raising a creature that lives there, clearing the area it is, or taking
// brackets at ROCK THE DOCK. A parallel bitmask would be a second copy of facts that
// are already on the blob, and the failure mode of a second copy is that it disagrees
// with the first. Game::backgroundOwned derives it, so it cannot.
//
// `wire` IS THE SAVE. Only the operator's CHOICE persists, and it persists as this
// number rather than as a position in the list or a SceneId ordinal — either of which
// would re-point somebody's chosen background at its neighbour the moment a row was
// added in the middle. A wire number is never reused and never renumbered, the same
// contract ModDef and QuoteDef hold theirs to.
#pragma once

#include <cstdint>

#include "core/render/scene_id.h"

namespace mal {

// How the picker says a background is come by, and how Game decides whether it is.
// Three kinds, because there are three kinds of thing an operator does: they raise
// creatures, they clear areas, and they fight in the arena.
enum class BackgroundSource : uint8_t {
    Start,     // owned from the first boot
    Raise,     // a creature that lives here has been raised
    Clear,     // the area it IS has been cleared
    Bracket,   // taken at ROCK THE DOCK — `rung` brackets in
};

struct BackgroundDef {
    SceneId scene;
    uint8_t wire;              // the save's name for it — never reused, never renumbered
    const char* name;          // what a person reads on the picker
    const char* earnedBy;      // the one line under the header while this row is focused
    BackgroundSource source;
    int rung = 0;              // Bracket: how many brackets it takes. Others: unused.
};

// Listed in the order an operator is likely to meet them: the two they start with, the
// four a raised creature brings, the three an area pays out, then the arena's.
inline constexpr BackgroundDef kBackgrounds[] = {
    {SceneId::ServerYard, 1, "THE YARD", "YOURS FROM THE START",
     BackgroundSource::Start},
    // Ransomware is the line every operator starts on, so its place is not something to
    // be earned — it is where the first pet was already standing.
    {SceneId::RansomLot, 2, "THE LOCKED LOT", "YOURS FROM THE START",
     BackgroundSource::Start},

    {SceneId::BaitShallows, 3, "THE SHALLOWS", "RAISE A PHISHING PET",
     BackgroundSource::Raise},
    {SceneId::KelpDrift, 4, "THE KELP", "RAISE A SWIMMER", BackgroundSource::Raise},
    {SceneId::StrataBurrow, 5, "THE BURROW", "RAISE A BURROWER",
     BackgroundSource::Raise},
    {SceneId::CirrusDeck, 6, "THE CLOUD DECK", "RAISE A FLIER",
     BackgroundSource::Raise},

    {SceneId::CitrusCircuit, 7, "CITRUS CIRCUIT", "CLEAR CITRUS CIRCUIT",
     BackgroundSource::Clear},
    {SceneId::PirateBayou, 8, "THE PIRATE BAYOU", "CLEAR THE PIRATE BAYOU",
     BackgroundSource::Clear},
    {SceneId::CastleRapidscare, 9, "CASTLE RAPIDSCARE", "CLEAR CASTLE RAPIDSCARE",
     BackgroundSource::Clear},

    // The arena's two, on a rung each: the first bracket taken pays one and the second
    // pays the other, so a second win is worth something the first already gave.
    {SceneId::GridHorizon, 10, "GRID HORIZON", "TAKE ROCK THE DOCK",
     BackgroundSource::Bracket, /*rung=*/1},
    {SceneId::MainframeRow, 11, "MAINFRAME ROW", "TAKE THE DOCK TWICE",
     BackgroundSource::Bracket, /*rung=*/2},
};
inline constexpr int kBackgroundCount =
    sizeof(kBackgrounds) / sizeof(kBackgrounds[0]);

// The row for a place, or nullptr for a place nobody can own — a scene may exist
// without being a possession, and SceneId::None is one.
inline const BackgroundDef* backgroundFor(SceneId s) {
    if (s == SceneId::None) return nullptr;
    for (const BackgroundDef& b : kBackgrounds)
        if (b.scene == s) return &b;
    return nullptr;
}

// The row a save's stored choice names, or nullptr for 0 (AUTO — the pet decides) and
// for a number this build has no row for, which is what an older build reads when a
// newer one has chosen a background it does not have. Falling back to AUTO there is the
// right answer: the operator gets the place their pet lives in rather than nothing.
inline const BackgroundDef* backgroundByWire(uint8_t wire) {
    if (wire == 0) return nullptr;
    for (const BackgroundDef& b : kBackgrounds)
        if (b.wire == wire) return &b;
    return nullptr;
}

}  // namespace mal
