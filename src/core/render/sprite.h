// sprite.h — sprite descriptor + blit, used by generated assets and the engine.
#pragma once

#include <cstdint>

#include "core/render/color.h"

namespace mal {

class Framebuffer;

// A sprite sheet: a grid of `rows` rows stacked vertically, each row a
// horizontal strip of `frames` cells sized `frameW` x `h`. `rgb`/`a` are
// sheetW(=frameW*frames) x (h*rows), row-major top-to-bottom; `a` is 0..255.
// A single-row sheet (rows=1, the common case) behaves exactly as before —
// `h` has always meant one cell's height, so existing callers that never
// pass `row` are unaffected. `rows` defaults to 1 so a zero-initialized
// SpriteData (tests, placeholders) still addresses row 0 validly.
struct SpriteData {
    int sheetW;
    int h;
    int frameW;
    int frames;
    int rows = 1;
    const uint16_t* rgb;
    const uint8_t* a;
};

// Alpha-composite frame `frame` of row `row` of `s` into `fb` with top-left
// at (x, y), 1:1 (native resolution — used for UI chrome / icons).
void drawSprite(Framebuffer& fb, const SpriteData& s, int frame, int x, int y,
                int row = 0);

// Same, but every pixel takes `tint` instead of its stored colour — the sprite
// supplies only the shape, through its alpha.
//
// Exact for the ICON_*/UI_* masters, which are a single flat `ink` fill on
// transparent: substituting the colour loses nothing, because there was only ever
// one. On a multi-colour sprite this flattens it to a silhouette, which is a
// legitimate effect but not what this is for.
//
// Callers pass a colour from core/ui/theme.h, never a literal — and only where the
// screen already states the same meaning some other way. See that file's rule.
void drawSpriteTinted(Framebuffer& fb, const SpriteData& s, int frame, int x, int y,
                      Rgb565 tint, int row = 0);

// Same, but nearest-neighbour upscaled by num/den (the creature/pixel-art
// path of the hybrid model). (destX, destY) is the top-left in active space.
void drawSpriteUpscaled(Framebuffer& fb, const SpriteData& s, int frame,
                        int destX, int destY, int num, int den, int row = 0);

// Same as drawSpriteUpscaled, but each visible pixel is lerped toward
// `flashColor` by `flashAmt` (0 = unchanged, 255 = solid flashColor) before
// blending — the sprite's own alpha mask still carries the silhouette, so
// this reads as a "hit shader" flash of the creature's shape rather than a
// new animation frame. flashAmt=0 is just drawSpriteUpscaled.
void drawSpriteFlash(Framebuffer& fb, const SpriteData& s, int frame,
                     int destX, int destY, int num, int den,
                     Rgb565 flashColor, uint8_t flashAmt, int row = 0);

// Idle-loop frame for a creature sprite, clamped to what the sheet actually has:
// the breathe alt-frame (1) only with a 2nd frame, an occasional blink (2) only
// with a 3rd. A single-frame creature resolves to a static frame 0 — callers
// still apply the positional idle bob, so it animates without extra frames. This
// decouples "add a creature" from "animate a creature": a richer sheet animates
// automatically as data (no code change), and a 1-frame placeholder never indexes
// a missing frame (which the blitters skip — that would flicker the pet on/off).
inline int idleFrame(const SpriteData& s, int beat) {
    int f = (s.frames > 1 && (beat & 1)) ? 1 : 0;
    if (s.frames > 2 && beat % 12 == 6) f = 2;   // occasional blink
    return f;
}

} // namespace mal
