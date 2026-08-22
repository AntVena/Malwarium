#include "core/render/camo.h"

#include "core/render/dissolve.h"
#include "core/render/framebuffer.h"
#include "core/render/sprite.h"

namespace mal {

namespace {

// How far ahead of the dissolve front the burn edge reaches, in hash units. Wide enough
// to be seen at ~4fps — a one-step edge on a 4-frame sweep would land on at most one
// drawn frame and might land on none.
constexpr uint8_t kEdgeSpan = 26;

// How far the ENDS of a derived ramp (camoRampFromTone) are pulled off the colour it was
// built from — three quarters of the way down toward black and up toward white. That is
// a ramp with real shading in it and still recognisably the colour that was asked for;
// pulled the whole way, every skin would end at the same two tones and the ramps would
// only differ in the middle.
constexpr int kCamoToneEndPct = 75;

// The ends themselves, derived from the colour rather than from two palette tokens. An
// anchored ladder (PAPER at the bottom, INK at the top) reads well for a UI colour, but
// it INVERTS for any tone outside that band — a near-white sprite ink pulled "up" toward
// INK comes out darker than it started, and the ramp stops being a value scale, which is
// the one property everything downstream depends on. Scaling the colour's own channels
// cannot do that whatever it is handed.
Rgb565 shadeDown(Rgb565 c, int pct) {
    return rgb565(static_cast<uint8_t>(r8(c) * (100 - pct) / 100),
                  static_cast<uint8_t>(g8(c) * (100 - pct) / 100),
                  static_cast<uint8_t>(b8(c) * (100 - pct) / 100));
}
Rgb565 liftUp(Rgb565 c, int pct) {
    return rgb565(static_cast<uint8_t>(r8(c) + (255 - r8(c)) * pct / 100),
                  static_cast<uint8_t>(g8(c) + (255 - g8(c)) * pct / 100),
                  static_cast<uint8_t>(b8(c) + (255 - b8(c)) * pct / 100));
}

// A colour's place in a ramp, by luminance. This is the whole trick: the worn palette
// is indexed by the value of the pixel being replaced, so a lit crown stays lit and a
// core shadow stays a shadow. Nothing here knows which creature is being painted.
Rgb565 wornTone(Rgb565 src, const CamoRamp& ramp) {
    int i = static_cast<int>(luminance(src) * ramp.count);
    if (i < 0) i = 0;
    if (i >= ramp.count) i = ramp.count - 1;
    return ramp.tone[i];
}

}  // namespace

CamoRamp camoRampFrom(const SpriteData& s, int frame, int row) {
    CamoRamp out;
    if (frame < 0 || frame >= s.frames || row < 0 || row >= s.rows) return out;
    // A 1-bit mask has exactly one colour by construction, so there is nothing to rank:
    // its ink is both the main colour and the whole palette, and the ladder derived from
    // it is what turns a pet camouflaged as a Worm GREYSCALE rather than flat white.
    if (s.bits) return camoRampFromTone(s.ink, kCamoRampMax);

    // Tally the frame's colours in a fixed table — no allocation, and a sprite that
    // overflows it is one whose rarest tones we can afford to miss. Kept deliberately
    // larger than kCamoRampMax so the ranking below chooses from a real distribution
    // rather than from whatever happened to appear first.
    constexpr int kSeen = 32;
    Rgb565 col[kSeen] = {};
    int cnt[kSeen] = {};
    int used = 0;

    const int fx0 = frame * s.frameW, fy0 = row * s.h;
    for (int y = 0; y < s.h; ++y) {
        for (int x = 0; x < s.frameW; ++x) {
            if (spriteAlphaAt(s, fx0 + x, fy0 + y) < 128) continue;
            const Rgb565 c = spriteColorAt(s, fx0 + x, fy0 + y);
            int i = 0;
            for (; i < used; ++i)
                if (col[i] == c) { ++cnt[i]; break; }
            if (i == used && used < kSeen) { col[used] = c; cnt[used] = 1; ++used; }
        }
    }
    if (used == 0) return out;

    // THE BASELINE. The most-used tone is what the creature reads AS, and a ladder
    // derived from it fills every rung before a single real colour is placed — so the
    // ramp is complete whatever the sprite turns out to have, and the pet is never left
    // half-painted or flattened.
    int main = 0;
    for (int i = 1; i < used; ++i)
        if (cnt[i] > cnt[main]) main = i;
    out = camoRampFromTone(col[main], kCamoRampMax);

    // THEN THE REAL COLOURS, most-used first, each landing on the rung its own luminance
    // belongs to. A rung is claimed once, so the commonest colour at that value wins it
    // and rarer ones at the same value are dropped rather than displacing it — and every
    // rung nobody claims keeps the derived tone underneath. The result wears as much of
    // the other creature as it actually has, and invents only the rest.
    bool claimed[kCamoRampMax] = {false};
    for (int placed = 0; placed < used; ++placed) {
        int pick = -1;
        for (int i = 0; i < used; ++i)
            if (cnt[i] > 0 && (pick < 0 || cnt[i] > cnt[pick])) pick = i;
        if (pick < 0) break;
        cnt[pick] = 0;                       // taken, whether or not it lands
        int rung = static_cast<int>(luminance(col[pick]) * kCamoRampMax);
        if (rung < 0) rung = 0;
        if (rung >= kCamoRampMax) rung = kCamoRampMax - 1;
        if (!claimed[rung]) {
            claimed[rung] = true;
            out.tone[rung] = col[pick];
        }
    }

    // A VALUE SCALE above all else, which the mixing above can nudge out of order at a
    // rung boundary. Sorting is nearly a no-op — every real colour was placed on the rung
    // its own luminance names — and it is what guarantees the one property the remap
    // depends on: a lit crown can never be repainted with a core shadow.
    for (int i = 0; i < out.count; ++i)
        for (int j = i + 1; j < out.count; ++j)
            if (luminance(out.tone[j]) < luminance(out.tone[i])) {
                Rgb565 t = out.tone[i]; out.tone[i] = out.tone[j]; out.tone[j] = t;
            }
    return out;
}

CamoRamp camoRampFromTone(Rgb565 base, int steps) {
    CamoRamp out;
    if (steps < 2) steps = 2;
    if (steps > kCamoRampMax) steps = kCamoRampMax;
    out.count = steps;
    // `base` sits in the MIDDLE of its own ramp rather than at one end, so the colour
    // the caller named is the one the eye reads as the skin — a ladder that only ever
    // darkened it would wear as a different, muddier colour than the swatch beside it.
    // Below it the tones sink into shadow and above they lift toward the light, both
    // derived from the colour itself (shadeDown / liftUp) so the scale can never invert.
    const Rgb565 floorTone = shadeDown(base, kCamoToneEndPct);
    const Rgb565 ceilTone = liftUp(base, kCamoToneEndPct);
    const int mid = (steps - 1) / 2;
    for (int i = 0; i < steps; ++i) {
        if (i == mid) { out.tone[i] = base; continue; }
        if (i < mid) {
            // 0 is the deepest shadow; the step just under `base` barely moves it.
            out.tone[i] = blend(floorTone, base, static_cast<uint8_t>(255 * i / mid));
        } else {
            const int span = steps - 1 - mid;
            out.tone[i] =
                blend(base, ceilTone, static_cast<uint8_t>(255 * (i - mid) / span));
        }
    }
    return out;
}

void drawSpriteCamo(Framebuffer& fb, const SpriteData& s, int frame,
                    int destX, int destY, int num, int den,
                    const CamoRamp& ramp, uint8_t level,
                    Rgb565 flashColor, uint8_t flashAmt, int row,
                    const CamoRamp* from) {
    if (frame < 0 || frame >= s.frames || row < 0 || row >= s.rows) return;
    if (den <= 0 || num <= 0) return;
    if (from && from->empty()) from = nullptr;
    // Not camouflaged at all is the ordinary draw — still carrying the caller's flash,
    // which is its own cue and nothing to do with this one. With a palette to fall back
    // TO, the same case is that palette fully worn, which is where a dissolve between
    // two skins starts and finishes.
    if (ramp.empty() || level == 0) {
        if (from)
            drawSpriteCamo(fb, s, frame, destX, destY, num, den, *from, 255, flashColor,
                           flashAmt, row);
        else
            drawSpriteFlash(fb, s, frame, destX, destY, num, den, flashColor, flashAmt,
                            row);
        return;
    }
    const Rgb565 burn = ramp.tone[ramp.count - 1];
    const int fx0 = frame * s.frameW, fy0 = row * s.h;
    const int dw = s.frameW * num / den, dh = s.h * num / den;
    for (int oy = 0; oy < dh; ++oy) {
        const int py = fy0 + oy * den / num;
        for (int ox = 0; ox < dw; ++ox) {
            const int px = fx0 + ox * den / num;
            const Rgb565 own = spriteColorAt(s, px, py);
            // Hashed on the SOURCE pixel, not the destination one, so the grain is a
            // property of the sprite and does not crawl when the scale changes.
            const uint8_t h = dissolveHash(px, py) & 0xFF;
            Rgb565 c = from ? wornTone(own, *from) : own;
            // Inclusive, so a settled 255 leaves nothing behind: the front has to be
            // able to pass the last hash value, or full camouflage keeps a speckle of
            // burn edge forever and the creature never actually settles.
            if (h <= level)
                c = wornTone(own, ramp);
            else if (h < static_cast<int>(level) + kEdgeSpan)
                c = burn;
            // The flash lands on whatever colour the pixel ended up, so a hit taken in
            // borrowed colours flashes the camouflaged creature instead of undressing it.
            if (flashAmt > 0) c = blend(c, flashColor, flashAmt);
            fb.blendPixel(destX + ox, destY + oy, c, spriteAlphaAt(s, px, py));
        }
    }
}

uint8_t camoAdvance(uint8_t level, bool worn) {
    const int target = worn ? 255 : 0;
    int v = level;
    if (v < target) v = v + kCamoFadeStep > target ? target : v + kCamoFadeStep;
    else if (v > target) v = v - kCamoFadeStep < target ? target : v - kCamoFadeStep;
    return static_cast<uint8_t>(v);
}

}  // namespace mal
