// preview_malbeast.cpp — headless style check for the wild-malbeast art spike.
//
// Renders one malbeast sprite at the device's ×1.75 creature scale: the raw one-ink
// silhouette on the left (what the atlas stores), then the eight distinct glitch states the
// FX_GLITCH shader (core/render/glitch.h) rolls through, so the corruption can be judged as a
// strip without a device. Writes a PPM (P6); tools/preview_malbeast.py wraps it into a PNG.
//
// Not a shipping path and not a gate — a look-at-it tool, the sibling of dump_frame for a
// sprite that has no screen to sit in yet. Build: the CMake `preview_malbeast` target.

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "core/render/color.h"
#include "core/render/framebuffer.h"
#include "core/render/glitch.h"
#include "core/render/sprite.h"
#include "generated/assets.h"

using namespace mal;

int main(int argc, char** argv) {
    const char* out = argc > 1 ? argv[1] : "malbeast.ppm";

    const SpriteData& s = ASSET_SPR_MALBEAST_GLITCHHOG;
    const int num = 7, den = 4;                       // the panel's ×1.75 creature scale
    const int cw = s.frameW * num / den;              // one cell, scaled
    const int ch = s.h * num / den;

    const int pad = 10;
    const int cells = 9;                              // raw + 8 glitch states
    const int W = pad + cells * (cw + pad);
    const int H = pad + ch + pad;

    // A dark panel, the way a combat scene reads, so a lit metallic body and the hot red
    // corruption both pop. Body + accent are the shader's two inputs.
    const Rgb565 bg = rgb565(20, 22, 30);
    const Rgb565 body = rgb565(150, 160, 172);        // the corrupted-steel silhouette
    const Rgb565 hot = rgb565(228, 42, 64);           // the glitch accent (malware red)

    Framebuffer fb(W, H);
    fb.clear(bg);

    int x = pad;
    const int y = pad;

    // Cell 0: the raw silhouette (drawSpriteUpscaled would tint a mask with its ink; we want
    // the body colour, so tint it to `body` at full scale via the glitch shader with phase 0
    // and the hot colour set to the body too — i.e. no corruption to show the clean shape).
    drawSpriteGlitch(fb, s, 0, x, y, num, den, body, body, /*phase=*/0);
    x += cw + pad;

    // Cells 1..8: the eight glitch clock states (phase >> 5), spread across 0..255.
    for (int i = 0; i < 8; ++i) {
        const uint8_t phase = static_cast<uint8_t>(i * 32 + 16);   // mid of each clock band
        drawSpriteGlitch(fb, s, 0, x, y, num, den, body, hot, phase);
        x += cw + pad;
    }

    FILE* f = std::fopen(out, "wb");
    if (!f) { std::fprintf(stderr, "cannot open %s\n", out); return 1; }
    std::fprintf(f, "P6\n%d %d\n255\n", W, H);
    for (int yy = 0; yy < H; ++yy)
        for (int xx = 0; xx < W; ++xx) {
            const Rgb565 c = fb.get(xx, yy);
            const unsigned char rgb[3] = {r8(c), g8(c), b8(c)};
            std::fwrite(rgb, 1, 3, f);
        }
    std::fclose(f);
    std::fprintf(stderr, "wrote %s (%dx%d)\n", out, W, H);
    return 0;
}
