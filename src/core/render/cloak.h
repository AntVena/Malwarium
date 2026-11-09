// cloak.h (FX_CLOAK): a sprite fading into whatever is drawn behind it. Where the
// creature was, the scene shows through bent sideways in bands, so it reads as something
// standing there rather than nothing; a glint runs along its edge. The Metamorphic line's
// vanish (core/model/idle_vanish.h).
//
// Stateless like the dissolve family: a pure function of the sprite's alpha, what is on
// the canvas under it, `amount` and `phase`.
#pragma once

#include <cstdint>

#include "core/render/color.h"

namespace mal {

class Framebuffer;
struct SpriteData;

// Seated top-left at (destX, destY) and scaled num/den the way drawSpriteUpscaled would
// seat it. `amount` 0 is the plain sprite and 255 is gone but for the ripple and `glint`
// along the rim. Draw it after the scene and before anything meant to sit in front.
void drawSpriteCloak(Framebuffer& fb, const SpriteData& s, int frame, int destX, int destY,
                     int num, int den, uint8_t amount, Rgb565 glint, int phase, int row = 0,
                     bool mirror = false);

}  // namespace mal
