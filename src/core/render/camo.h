// camo.h — FX_CAMO: the pet repainting itself in somebody else's colours.
//
// The third effect built on dissolve.h's scatter, alongside FX_ABSORB and FX_SHRED, and
// for the reason that header gives: a screen that can play more than one of them must
// not look like it swapped renderers between them. Same hash, same grain.
//
// WHAT IT IS FOR. The Metamorphic line fields moves rolled out of other lines' pools
// (MoveDef::drawLineA, defs.h). This is that borrowing made visible: while the pet's live
// cast is somebody else's, the pet IS their colours. Whose, exactly, is ranked by
// camoTarget (core/ui/combat_screen.h) — the fighter opposite when the cast is one of
// theirs, otherwise the line the cast came out of.
//
// A STATE, not a moment. The pet's colour says whose move it is currently holding, and it
// stays said until the pet casts something else — a wildcard row that rolls a GENERIC move
// (most of any pool, buildWildPools) puts it back in its own. That is the whole read, and
// it only works if the colour holds: an effect that ran out on a timer would be telling
// you about a swing that has already finished, and one that could be interrupted would be
// lying about what the pet is holding.
//
// WHY A RAMP AND NOT A TINT. drawSpriteTinted flattens a sprite to one colour, which
// loses the form. Here every source pixel is re-coloured by WHERE ITS OWN LUMINANCE
// SITS in the worn palette, so the creature's shading survives the swap — it reads as
// the same animal in different colours rather than as a silhouette. That also means the
// effect needs to know nothing about the sprite it is painting: no index, no palette
// metadata, no per-creature table.
//
// Like the other two it allocates nothing, stores nothing, and is a pure function of
// the source pixel and the current level, which is what lets the ~4fps event-driven
// repaint run it and lets the frame gates dump it. The one thing that IS remembered is
// the level itself, which is a single byte on the caller (Game::combatCamoLevel_).
#pragma once

#include <cstdint>
#include <cstring>

#include "core/render/color.h"

namespace mal {

class Framebuffer;
struct SpriteData;

// A palette a creature can wear. Small and fixed: this is a value passed by the draw
// path every frame, so it owns its tones rather than pointing at somebody else's.
constexpr int kCamoRampMax = 8;

// A ramp is ALWAYS complete: every band of the creature's own value range gets a tone.
// Where those tones come from is the interesting question (camoRampFrom below), but a
// half-filled ramp is not one of the answers — a creature wearing a borrowed palette
// over part of itself and its own colours over the rest reads as damage rather than as
// a disguise.
struct CamoRamp {
    Rgb565 tone[kCamoRampMax] = {};
    int count = 0;                       // 0 = nothing to wear; the effect no-ops
    bool empty() const { return count <= 0; }
};

// WHOSE colours a creature is wearing — the palette named as a source rather than as a
// ramp, so a caller can carry the answer around, compare it, and resolve it to tones only
// where it has the sprites to sample (Game::drawCombatScreen). Which source a fight picks,
// and why the fighter opposite outranks the line, is camoTarget (core/ui/combat_screen.h).
struct CamoTarget {
    // Whose colours, in the order camoTarget ranks them.
    enum class Source : uint8_t {
        Own,     // the creature's own — nothing is worn and the effect no-ops
        Rival,   // the fighter opposite, sampled off its sprite
        Line,    // a creature line, sampled off that line's own creature
    };
    Source source = Source::Own;
    // The line to wear, when `source` is Line. Null otherwise.
    const char* line = nullptr;
};

// Two targets are the same colours when they name the same source and the same line, so a
// caller can tell a palette CHANGE from a level that is simply still moving.
inline bool operator==(const CamoTarget& a, const CamoTarget& b) {
    if (a.source != b.source) return false;
    if (!a.line || !b.line) return a.line == b.line;
    return std::strcmp(a.line, b.line) == 0;
}
inline bool operator!=(const CamoTarget& a, const CamoTarget& b) { return !(a == b); }

// The palette a sprite is actually drawn in, darkest first — the colours it uses most,
// ranked by luminance so the ramp reads as a value scale rather than a bag of hues.
//
// Sampling A CREATURE is what makes this work without a per-line colour table: a line's
// creatures wear that line's colour, so any of them already IS the line's palette, and the
// one standing opposite carries whatever accent it alone has on top of that. It also
// answers for something that belongs to no line — a malbeast, a boss, the dummy — where
// there is no table to look in and the truth is "whatever colours are on that thing".
//
// A BASELINE, THEN AS MUCH OF THE REAL THING AS THERE IS. The ladder is first derived
// whole from the sprite's MAIN colour (camoRampFromTone below), so every rung has a tone
// before anything else happens — a complete value scale is the one thing the remap
// cannot do without, and a ramp with holes in it paints a creature that looks damaged
// rather than disguised. Then the sprite's actual colours are laid over that baseline,
// commonest first, each on the rung its own luminance belongs to.
//
// So a richly drawn opponent is worn almost entirely in its own colours, a plainer one
// is worn in the colours it has plus shades of its main one where it has none, and a
// 1-bit Worm — one white ink and nothing else — turns the pet greyscale, keeping every
// bit of its own shading. One rule, and the sprite decides how much of it is real.
CamoRamp camoRampFrom(const SpriteData& s, int frame = 0, int row = 0);

// The OTHER source: a ramp built out of one named colour, for a caller with no sprite
// to sample. A fight always has somebody standing opposite; a screen that asks the pet
// to wear a colour on purpose — a board of skins to choose between, an ambient flourish
// at home — has only the colour it means, and hand-listing eight tones per skin would
// put a palette in a call site where the theme swap cannot reach it.
//
// The ladder runs from a shadow of `base`, through `base` itself, to a highlight of it —
// all three derived from the colour's own channels, so a ramp built from a PAL_CORE
// token follows a theme swap for free and one built from a sprite's ink cannot invert
// whatever that ink happens to be. `steps` is clamped to [2, kCamoRampMax]; the result
// is darkest-first, the same value scale camoRampFrom returns, so nothing downstream can
// tell the two apart.
CamoRamp camoRampFromTone(Rgb565 base, int steps = 5);

// One frame of the camouflage, seated exactly as drawSpriteUpscaled would seat it.
//
// `level` is how much of the borrowed palette the pet is wearing, 0 (its own colours) to
// 255 (fully in `ramp`). A pixel flips when its own dissolve hash falls under `level`, so
// a CHANGE of level spreads as a scatter rather than a wipe, and pixels just ahead of the
// front take the ramp's brightest tone — the burn edge that says the colour is moving.
// At a settled 255 the front is past every pixel and the creature simply stands there in
// the other line's colours, which is the state this exists to show.
//
// `flashColor`/`flashAmt` are the caller's own whole-body flash (drawSpriteFlash's pair),
// applied OVER the recolour. Composed rather than chosen between: the camouflage is what
// colour the creature is, so a hit taken while wearing it must flash the camouflaged pet.
// Draw them as alternatives and every hit reads as cancelling the disguise.
//
// `from` is what the pixels the front has NOT reached are still wearing — the palette
// being left behind. nullptr, the default, means the creature's own colours, which is
// what a fight wants: there the level rises out of the pet being itself and falls back
// to it. A caller changing DIRECTLY from one borrowed palette to another (the
// CHROMATOPHORE, where the pet is always wearing something) passes the old one, and the
// scatter becomes a dissolve between two skins instead of a detour through a creature
// that, for those four frames, would be wearing neither.
void drawSpriteCamo(Framebuffer& fb, const SpriteData& s, int frame,
                    int destX, int destY, int num, int den,
                    const CamoRamp& ramp, uint8_t level,
                    Rgb565 flashColor = 0, uint8_t flashAmt = 0, int row = 0,
                    const CamoRamp* from = nullptr);

// How much of the way the level moves toward its target in one tick of the caller's
// clock. 255 in four steps: fast enough that the colour has changed by the time the
// swing it belongs to is over, slow enough that the scatter is visibly a scatter.
constexpr uint8_t kCamoFadeStep = 64;

// The level after one tick, easing toward fully worn or fully off. `worn` is the LIVE
// answer to "is this fighter's last cast another line's" (ui/combat_screen.h's
// wearingBorrowedColours) — read fresh every tick rather than latched, so there is no
// window to be cut short and nothing to reset. Settled at either end it is a fixed
// point: hand it the same `worn` forever and it stops moving.
uint8_t camoAdvance(uint8_t level, bool worn);

} // namespace mal
