// absorb.h — FX_ABSORB: a glyph breaking into blocks that stream into the pet.
//
// The device has no GPU, so the dissolve every engine does as a shader is here a
// framebuffer pass at the 128-logical canvas (RENDER_PIPELINE.md, SPRITE_MODS stage).
// It is derived entirely from the source pixel's own coordinates and the sweep
// position: a block's departure order, flight arc and fade are a hash of where it
// started, so the effect stores nothing, allocates nothing, and draws the same frame
// twice for the same progress — which is what lets a 4fps event-driven repaint run it
// and what lets the frame gates dump it.
//
// Consumed by the two screens where the pet takes something in: the feeding modal
// (core/ui/modals.cpp) and the EXPL Wi-Fi event (core/ui/expl_screen.cpp).
#pragma once

#include <cstdint>

#include "core/render/color.h"

namespace mal {

class Framebuffer;
struct SpriteData;

// One frame of the dissolve.
//
// `s`/`frame`/`row` is the glyph being taken in, seated with its top-left at
// (srcX, srcY) in active space and scaled num/den the way drawSpriteUpscaled would
// seat it; (dstX, dstY) is the point every block converges on — the pet's mouth.
// `tint` is the colour a FLAT MASK's blocks take — the ICON_* family has no colour of
// its own, so the caller supplies one from core/ui/theme.h the same way
// drawSpriteTinted's callers do. A full-colour sprite ignores it and comes apart into
// its own pixels, which is what a creature-sized subject needs; the tint still drives
// the pet's swallow flash either way.
//
// TWO NUMBERS, and they are not the same one. `bite` is HOW MUCH of the glyph is
// going: blocks are ranked by how near the mouth they sit, and the nearest `bite`/255
// of them are the ones that ever leave — the rest are still glyph when it is over.
// `progress` is HOW FAR ALONG that bite is, 0 (nothing has moved) to 255 (everything
// that was going has arrived). So a half-bite run finishes with half a glyph standing
// and nothing left hanging in the air, which is a state the Wi-Fi event needs: what
// the pet leaves is as much a statement as what it takes.
void drawAbsorb(Framebuffer& fb, const SpriteData& s, int frame,
                int srcX, int srcY, int num, int den,
                int dstX, int dstY, Rgb565 tint, uint8_t progress, uint8_t bite,
                int row = 0);

// One frame of an absorb, as the numbers a screen hands to the two draws it makes.
struct AbsorbPhase {
    uint8_t progress;   // -> drawAbsorb
    uint8_t bite;       // -> drawAbsorb
    uint8_t flash;      // -> drawSpriteFlash's flashAmt on the pet
};

// The phase for a screen running a `bite`-sized absorb over `beats` heartbeats from
// `startBeat`. Before the window nothing has moved; after it the bite is finished and
// the pet's afterglow decays over the next couple of beats, so a screen that lingers
// past the absorb settles out of it instead of snapping. A bite of 0 is a glyph the
// pet declines: nothing moves and nothing flashes, ever.
AbsorbPhase absorbPhase(int beat, int startBeat, int beats, uint8_t bite = 255);

// The whole beat as one call: the pet seated centred on (petCx, petCy) and flashing as
// it swallows, with `glyph` standing off its left shoulder and coming apart into its
// mouth. Both sprites take the habitat's x1.75 creature scale, so the blocks the glyph
// breaks into are the same size as the pixels of the creature eating them.
//
// This owns the seating so the two screens that show it agree on where a glyph waits
// and where a mouth is; each still chooses its own (petCx, petCy). A null `pet` draws
// only the glyph, a null `glyph` only the pet — the Wi-Fi event has beats for both.
void drawPetAbsorbing(Framebuffer& fb, const SpriteData* pet, int petCx, int petCy,
                      const SpriteData* glyph, Rgb565 tint, AbsorbPhase phase,
                      int beat);

} // namespace mal
