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
// records: raising a creature that lives there, clearing the area it is, taking
// brackets at ROCK THE DOCK, or holding an achievement. A parallel bitmask would be a
// second copy of facts that are already on the blob, and the failure mode of a second
// copy is that it disagrees with the first. Game::backgroundOwned derives it, so it
// cannot.
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
// Four kinds, because there are four kinds of thing an operator does: they raise
// creatures, they clear areas, they fight in the arena, and they climb the achievement
// ladders — which is the one that scales, since a ladder already exists for every
// system on the device and none of them paid out a place before.
enum class BackgroundSource : uint8_t {
    Start,     // owned from the first boot
    Raise,     // a creature that lives here has been raised
    Clear,     // the area it IS has been cleared
    Bracket,   // taken at ROCK THE DOCK — `rung` brackets in
    Achieve,   // the achievement `earnedById` names has been earned
};

struct BackgroundDef {
    SceneId scene;
    uint8_t wire;              // the save's name for it — never reused, never renumbered
    const char* name;          // what a person reads on the picker
    // The one line under the header while this row is focused. It names the DEED, never
    // the THRESHOLD — an Achieve row says "RUN A WORKING KITCHEN" beside an achievement
    // called Working Kitchen and leaves the count to that achievement's own row, which
    // is the only place it lives. A picker line repeating the number would be a second
    // copy free to go stale, the same reason the achievement triggers template theirs.
    // It matters most here: the device has no achievement browser to send a reader to,
    // so this line is the whole of what the player is told to go and do.
    const char* earnedBy;
    BackgroundSource source;
    int rung = 0;              // Bracket: how many brackets it takes. Others: unused.
    // Achieve: the achievement id that pays it (content_achievements.h's `id`, which is
    // what every engine call site names one by). Others: unused. The gate checks the id
    // resolves, so a renamed achievement is a build failure rather than a background
    // nobody can ever earn.
    const char* earnedById = nullptr;
};

// Listed in the order an operator is likely to meet them: the two they start with, the
// four a raised creature brings, the three an area pays out, the arena's two, then the
// four an achievement does.
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

    // The achievement prizes. Each one is the ROOM its ladder is about — the kitchen
    // the recipes are cooked in, the bench the rig is built on, the mast the spectrum
    // is mapped from, the city the steps add up to — so the reward for climbing a
    // ladder is somewhere to stand that says which one you climbed. Four families that
    // paid Bits and a cache and nothing you could look at, until now.
    {SceneId::TheLine, 12, "THE LINE", "RUN A WORKING KITCHEN",
     BackgroundSource::Achieve, /*rung=*/0, "RECIPES_10"},
    {SceneId::CrtBench, 13, "THE CRT BENCH", "BUILD A FULL RIG",
     BackgroundSource::Achieve, /*rung=*/0, "RIG_ALL"},
    // The one prize behind an opt-in: the radio ladders need AUDIT turned on, so an
    // operator who never consents cannot reach this row. That is the correct shape —
    // a place earned by wardriving should be earned by wardriving — and it is why the
    // other three are all reachable with the radio dark.
    {SceneId::GroundStation, 14, "GROUND STATION", "MAP THE SPECTRUM",
     BackgroundSource::Achieve, /*rung=*/0, "NETS_100"},
    {SceneId::TraceCity, 15, "TRACE CITY", "WALK A MARATHON",
     BackgroundSource::Achieve, /*rung=*/0, "STEPS_100K"},
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
