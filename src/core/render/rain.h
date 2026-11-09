// rain.h (FX_RAIN): a sprite turned into a dark silhouette with columns of digits falling
// inside it, clipped to its shape, and a lit rim so the shape still reads against a dark
// scene. The Ransomware line's harvest hold (core/model/idle_harvest.h): the creature
// full of the data it took.
//
// Stateless like the dissolve family: a pure function of the sprite's alpha and `phase`,
// so the event-driven repaint and the frame gates draw the same frame for the same beat.
#pragma once

#include "core/render/color.h"

namespace mal {

class Framebuffer;
struct SpriteData;

// Seated top-left at (destX, destY) and scaled num/den the way drawSpriteUpscaled would
// seat it. `body` fills the silhouette, `lit` is the rim and each column's leading digit,
// and the trail behind a leading digit is `lit` blended over `body`. Each step of `phase`
// drops every column one glyph.
void drawSpriteRain(Framebuffer& fb, const SpriteData& s, int frame, int destX, int destY,
                    int num, int den, Rgb565 body, Rgb565 lit, int phase, int row = 0,
                    bool mirror = false);

}  // namespace mal
