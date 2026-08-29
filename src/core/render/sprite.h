// sprite.h — sprite descriptor + blit, used by generated assets and the engine.
#pragma once

#include <cstdint>

#include "core/render/color.h"

namespace mal {

class Framebuffer;

// Which way the DRAWING in a sheet is turned. A fact about the art rather than about
// any creature, which is why it rides on the sheet: a wild malbeast is built from a
// sprite-named spec and carries no CreatureDef at all (model/combat_factory.cpp's
// makeEnemyCombatant), so a facing declared on a content row could never reach one.
// Declared per asset in tools/gen_assets.py.
//
// `None` is the three-quarter, turned-to-the-viewer standing pose that
// assets/CREATURE_VISUAL_RULES.md §2 asks every creature for, plus every icon and
// panel — a drawing with no side to it, or one carrying a detail that must not read
// backwards. It is also the floor for a sheet that has not been looked at yet, and it
// never mirrors, so an undeclared asset draws exactly as it is stored.
//
// `Right`/`Left` are for the sheets that DO have a side: a profile walk row, a fish, a
// creature whose body reads across its cell. A screen that seats a drawing against an
// opponent asks spriteMirrorToFace() below which way to turn it.
enum class Facing : uint8_t { None = 0, Right, Left };

// A sprite sheet: a grid of `rows` rows stacked vertically, each row a
// horizontal strip of `frames` cells sized `frameW` x `h`. `rgb`/`a` are
// sheetW(=frameW*frames) x (h*rows), row-major top-to-bottom; `a` is 0..255.
// A single-row sheet (rows=1, the common case) behaves exactly as before —
// `h` has always meant one cell's height, so existing callers that never
// pass `row` are unaffected. `rows` defaults to 1 so a zero-initialized
// SpriteData (tests, placeholders) still addresses row 0 validly.
//
// THREE STORAGE FORMS, distinguished by `bits` and `pal`:
//
//   FULL COLOUR (`bits == nullptr`) — `rgb` + `a`, one entry each per pixel. The floor,
//   and the only form that can hold more distinct pixel values than an 8-bit index
//   addresses.
//
//   1-BIT MASK (`bits != nullptr`, `pal == nullptr`) — a packed bitmap plus the one
//   colour `ink` that every set bit takes; `rgb`/`a` are null. Most of the ICON_*/UI_*
//   family is a single flat fill on transparent with no partial-alpha pixel anywhere,
//   because dim/bright is engine brightness and colour is applied at draw time
//   (drawSpriteTinted). Stored as RGB565 + alpha, such a glyph spends 24 bits per
//   pixel-bit of real information; as a mask it is exactly its own shape. This form
//   also states "this drawing has no colour of its own", which is what lets camo/shred/
//   absorb substitute a tint for it — ask spriteIsMask(), never `bits`.
//
//   INDEXED (`bits != nullptr`, `pal != nullptr`) — `bpp` bits per pixel into a palette
//   of RGB565 + coverage pairs (`pal`/`palA`). What a creature sheet needs: a sheet's
//   distinct (colour, alpha) pairs number in the single or low double digits against
//   43,008 pixels, so the pixels cost 3-6 bits each instead of 24 and the separate alpha
//   plane stops being stored at all — transparent is simply an entry. Lossless: the
//   palette is DERIVED from the sheet's own pixels, so nothing is quantised and a sheet
//   that gains a colour widens its palette (and, at a power of two, its `bpp`) by
//   itself. A sheet needing more than 256 entries falls back to full colour.
//
// Packing, both bitmap forms: row-major over the SHEET, MSB first, each sheet row padded
// to a whole byte, so a row starts on a byte boundary and the index math needs no carry
// between rows. An indexed sheet's array carries one trailing pad byte so the two-byte
// window spriteIndexAt reads is always in bounds. Read it through spriteAlphaAt/
// spriteColorAt below rather than by hand.
// `contentX0`/`contentX1` are the horizontal band the DRAWING actually occupies inside
// one frame cell — the union across every frame and row, so it is a fixed property of
// the sheet and a pose can never shift it. A cell is usually wider than what is drawn
// in it (a 56-wide cell holding a 24-wide Cachemutt), and a screen that seats two
// sprites against each other has to seat the drawings, not the cells: seating by the
// cell edge stands a well-padded creature a third of its own width away from where it
// looks like it is standing. Read them through spriteContentX0/spriteContentX1 below,
// which fall back to the whole frame for a zero-initialized placeholder.
struct SpriteData {
    int sheetW;
    int h;
    int frameW;
    int frames;
    int rows = 1;
    int contentX0 = 0;
    int contentX1 = 0;
    Facing facing = Facing::None;    // which way the drawing is turned; see above
    const uint16_t* rgb = nullptr;
    const uint8_t* a = nullptr;
    const uint8_t* bits = nullptr;   // non-null = packed bitmap; rgb/a are then null
    uint16_t ink = 0;                // mask only: the colour a set bit takes
    const uint16_t* pal = nullptr;   // indexed only: RGB565 per entry
    const uint8_t* palA = nullptr;   // indexed only: coverage per entry, 0..255
    uint8_t bpp = 0;                 // indexed only: bits per pixel index, 1..8
};

// Which bitmap form `bits` holds. A mask is the one that carries no colour of its own, so
// a pass that substitutes a tint for the stored colour (camo, shred, absorb) keys off
// this rather than off `bits` — an indexed sheet has a palette and must keep it.
inline bool spriteIsMask(const SpriteData& s) { return s.bits && !s.pal; }

// Bytes per sheet row, in the 1-bit and the indexed form.
inline int spriteMaskStride(const SpriteData& s) { return (s.sheetW + 7) >> 3; }
inline int spriteIndexStride(const SpriteData& s) { return (s.sheetW * s.bpp + 7) >> 3; }

// One pixel's palette index, at sheet coordinates. `bpp` is any width 1..8, so an index
// can straddle a byte boundary; reading a two-byte window and shifting covers every case
// without a branch, and the trailing pad byte on the array is what makes the second read
// safe on the last pixel of the last row.
inline uint8_t spriteIndexAt(const SpriteData& s, int px, int py) {
    const int bit = px * s.bpp;
    const uint8_t* row = s.bits + py * spriteIndexStride(s) + (bit >> 3);
    const uint16_t win = static_cast<uint16_t>((row[0] << 8) | row[1]);
    return static_cast<uint8_t>((win >> (16 - s.bpp - (bit & 7))) & ((1u << s.bpp) - 1));
}

// The drawn band inside one frame cell (SpriteData::contentX0/contentX1), as a half-open
// [x0, x1) column range. A sheet that never had the span measured — a zero-initialized
// placeholder in a test — answers with the whole frame, which is what every caller
// assumed before the span existed. Read both through these, never the fields: a caller
// that took one with the fallback and the other without would measure a nonsense band.
// `mirror` asks for the band a MIRRORED draw occupies, which is the same band measured
// from the other cell edge. A screen seating a turned drawing has to seat where it will
// actually land, or a sprite padded to one side of its cell steps that padding's width
// away from its seat the moment it turns round. The WIDTH is the same either way, so a
// caller measuring only the span (seatWidth, ui/combat_screen.cpp) needs no flag.
inline bool spriteHasContentSpan(const SpriteData& s) { return s.contentX1 > s.contentX0; }
inline int spriteContentX0(const SpriteData& s, bool mirror = false) {
    if (!spriteHasContentSpan(s)) return 0;
    return mirror ? s.frameW - s.contentX1 : s.contentX0;
}
inline int spriteContentX1(const SpriteData& s, bool mirror = false) {
    if (!spriteHasContentSpan(s)) return s.frameW;
    return mirror ? s.frameW - s.contentX0 : s.contentX1;
}

// Does drawing `s` so that it looks toward `faceRight` need the horizontal mirror? An
// undeclared sheet (Facing::None) answers false and is drawn exactly as stored, so a
// front-on creature, an icon or a panel is never turned round by a caller that asks.
inline bool spriteMirrorToFace(const SpriteData& s, bool faceRight) {
    if (s.facing == Facing::None) return false;
    return (s.facing == Facing::Right) != faceRight;
}

// The SHEET column a frame's own column `col` reads from. This is the whole of what a
// mirror is: every blitter walks its DESTINATION unchanged and only sources from the
// far side of the cell, so nothing that is keyed to a screen position — a dissolve's
// scatter, a shred row's slide — has to know a sprite turned round.
inline int spriteSrcX(const SpriteData& s, int frame, int col, bool mirror) {
    return frame * s.frameW + (mirror ? s.frameW - 1 - col : col);
}

// One source pixel's coverage and colour, at sheet coordinates. Every form answers the
// same two values the full-colour form would have stored — a mask has no partial coverage
// and no per-pixel colour by construction, and an indexed palette is derived from the
// pixels themselves — which is why neither saving costs anything at the caller.
inline uint8_t spriteAlphaAt(const SpriteData& s, int px, int py) {
    if (s.pal) return s.palA[spriteIndexAt(s, px, py)];
    if (!s.bits) return s.a[py * s.sheetW + px];
    const uint8_t byte = s.bits[py * spriteMaskStride(s) + (px >> 3)];
    return (byte >> (7 - (px & 7))) & 1 ? 255 : 0;
}
inline Rgb565 spriteColorAt(const SpriteData& s, int px, int py) {
    if (s.pal) return s.pal[spriteIndexAt(s, px, py)];
    return s.bits ? s.ink : s.rgb[py * s.sheetW + px];
}

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
//
// `mirror` turns the glyph (spriteSrcX below) and `alpha` scales the coverage it is
// blended at, 255 being the sprite's own. Both exist for the combat screen's strike
// marks (ui/combat_screen.cpp): a mark is drawn as though the blow travels right and is
// turned for one going the other way, and it fades as it crosses the lane.
void drawSpriteTinted(Framebuffer& fb, const SpriteData& s, int frame, int x, int y,
                      Rgb565 tint, int row = 0, bool mirror = false,
                      uint8_t alpha = 255);

// Same, but nearest-neighbour upscaled by num/den (the creature/pixel-art
// path of the hybrid model). (destX, destY) is the top-left in active space.
// `mirror` flips it horizontally inside its own cell — see spriteSrcX above, and
// spriteContentX0 for seating one.
void drawSpriteUpscaled(Framebuffer& fb, const SpriteData& s, int frame,
                        int destX, int destY, int num, int den, int row = 0,
                        bool mirror = false);

// Same as drawSpriteUpscaled, but each visible pixel is lerped toward
// `flashColor` by `flashAmt` (0 = unchanged, 255 = solid flashColor) before
// blending — the sprite's own alpha mask still carries the silhouette, so
// this reads as a "hit shader" flash of the creature's shape rather than a
// new animation frame. flashAmt=0 is just drawSpriteUpscaled.
void drawSpriteFlash(Framebuffer& fb, const SpriteData& s, int frame,
                     int destX, int destY, int num, int den,
                     Rgb565 flashColor, uint8_t flashAmt, int row = 0,
                     bool mirror = false);

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
