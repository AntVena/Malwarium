// shred.h — FX_SHRED: a sprite coming apart into sliding scanlines.
//
// The other half of the dissolve vocabulary. Where FX_ABSORB (absorb.h) carries a
// glyph SOMEWHERE — into the pet — this one carries it nowhere: the subject shears
// into horizontal slivers that slide apart, stretch into streaks and thin out where
// they stood. Same scatter (dissolve.h), same stateless contract, opposite meaning.
//
// That opposition is the point, and it is the whole reason two effects exist rather
// than one with a flag. A defeated malbeast that SHREDS is gone and gave nothing; one
// that is ABSORBED had something the pet wanted. A player learns which is which once
// and can then read a fight's worth from its last beat.
//
// Consumed by the combat screen's outro (core/ui/combat_screen.cpp).
#pragma once

#include <cstdint>

#include "core/render/color.h"

namespace mal {

class Framebuffer;
struct SpriteData;

// One frame of the shred.
//
// `s`/`frame`/`row` is the subject, seated with its top-left at (srcX, srcY) in active
// space and scaled num/den the way drawSpriteUpscaled would seat it. `progress` sweeps
// 0 (whole, nothing moving) to 255 (gone). A flat mask's slivers take `tint`; a
// full-colour sprite keeps its own pixels, the same split drawAbsorb makes.
//
// No direction is asked for and none is implied: each row picks its own side off the
// scatter, so the subject falls apart rather than being pushed anywhere.
void drawShred(Framebuffer& fb, const SpriteData& s, int frame,
               int srcX, int srcY, int num, int den, Rgb565 tint, uint8_t progress,
               int row = 0);

} // namespace mal
