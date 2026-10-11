// preview_malbeast.cpp — headless style check for the wild-malbeast art spike.
//
// Renders one malbeast at the device's ×1.75 creature scale as four cells: its two stored
// frames clean (idle, attack — exactly what the atlas holds), then the same two through the
// FX_GLITCH shader (core/render/glitch.h), so the drawing and the corruption can be judged
// side by side without a device. Writes a PPM (P6); tools/preview_malbeast.py wraps it into
// a PNG.
//
// `play <dir> <beats>` instead runs a real fight against GlitchHog through the actual combat
// screen and writes one active-canvas frame per combat beat (~8fps) as <dir>/frame_NNN.ppm — the
// sequence tools/preview_malbeast.py assembles into a GIF, so a wild can be watched in motion.
//
// Not a shipping path and not a gate — a look-at-it tool, the sibling of dump_frame for a
// sprite that has no screen to sit in yet. Build: the CMake `preview_malbeast` target.

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "tunables.h"
#include "core/app/game.h"
#include "core/model/combat.h"
#include "core/render/canvas.h"
#include "core/render/color.h"
#include "core/render/framebuffer.h"
#include "core/render/glitch.h"
#include "core/render/sprite.h"
#include "generated/assets.h"

using namespace mal;

static bool writePpm(const Framebuffer& fb, const char* path) {
    FILE* f = std::fopen(path, "wb");
    if (!f) { std::fprintf(stderr, "cannot open %s\n", path); return false; }
    std::fprintf(f, "P6\n%d %d\n255\n", fb.width(), fb.height());
    for (int yy = 0; yy < fb.height(); ++yy)
        for (int xx = 0; xx < fb.width(); ++xx) {
            const Rgb565 c = fb.get(xx, yy);
            const unsigned char rgb[3] = {r8(c), g8(c), b8(c)};
            std::fwrite(rgb, 1, 3, f);
        }
    std::fclose(f);
    return true;
}

// A fight against GlitchHog (tier 1, variant 0), one frame per combat animation beat
// (kCombatAnimMs — the stage repaints at ~8fps, twice the idle heartbeat). The pet is the
// default Paypup the frame dumper also boots; the stakes are Safe, so nothing persists.
static int play(const char* dir, int beats) {
    Game game{StartMode::Hatched, "paypup"};
    for (Game::Tip t : {Game::Tip::HatchDecrypt, Game::Tip::HatchClutch,
                        Game::Tip::HatchIsolation, Game::Tip::HatchChroma})
        game.markTipSeen(t);
    game.debugStartCombatVs(wildMalbeast(1, 0));
    uint32_t t = 0;
    char path[512];
    for (int k = 0; k < beats; ++k) {
        game.tick(t += kCombatAnimMs);
        Framebuffer fb(kActiveW, kActiveH);
        game.render(fb);
        std::snprintf(path, sizeof(path), "%s/frame_%03d.ppm", dir, k);
        if (!writePpm(fb, path)) return 1;
        if (game.nav() != Game::Nav::Combat) { ++k; std::fprintf(stderr, "fight over at beat %d\n", k); break; }
    }
    std::fprintf(stderr, "wrote frames to %s\n", dir);
    return 0;
}

int main(int argc, char** argv) {
    if (argc > 1 && std::strcmp(argv[1], "play") == 0)
        return play(argc > 2 ? argv[2] : ".", argc > 3 ? std::atoi(argv[3]) : 60);
    const char* out = argc > 1 ? argv[1] : "malbeast.ppm";

    const SpriteData& s = ASSET_SPR_MALBEAST_GLITCHHOG;
    const int num = 7, den = 4;                       // the panel's ×1.75 creature scale
    const int cw = s.frameW * num / den;              // one cell, scaled
    const int ch = s.h * num / den;

    const int pad = 10;
    const int cells = 4;                              // idle, attack, idle+glitch, attack+glitch
    const int W = pad + cells * (cw + pad);
    const int H = pad + ch + pad;

    // A dark panel, the way a combat scene reads, so the light line art and the hot red
    // corruption both pop. Body + accent are the shader's two inputs.
    const Rgb565 bg = rgb565(20, 22, 30);
    const Rgb565 body = rgb565(220, 226, 232);        // the line art's ink
    const Rgb565 hot = rgb565(228, 42, 64);           // the glitch accent (malware red)

    Framebuffer fb(W, H);
    fb.clear(bg);

    // Row 0 is the idle, row 1 the attack (tools/gen_malbeast_art.py). The clean cells are
    // the plain upscaled draw in the sheet's own ink — what a wild looks like with no FX.
    const int rows = s.rows < 2 ? 1 : 2;
    int x = pad;
    for (int r = 0; r < 2; ++r, x += cw + pad)
        drawSpriteUpscaled(fb, s, 0, x, pad, num, den, r % rows);
    // Mid-band phases that tear, so the glitched pair shows the effect rather than one of
    // the clock states where nothing happens to land on the body.
    const uint8_t phases[2] = {48, 176};
    for (int r = 0; r < 2; ++r, x += cw + pad)
        drawSpriteGlitch(fb, s, 0, x, pad, num, den, body, hot, phases[r], r % rows);

    if (!writePpm(fb, out)) return 1;
    std::fprintf(stderr, "wrote %s (%dx%d)\n", out, W, H);
    return 0;
}
