// test_glitch.cpp — native gates for FX_GLITCH, the wild-malbeast corruption shader
// (core/render/glitch.h), and the burst schedule the combat screen drives it on.
//
// One slice of the native suite; see test_gates.h for the shared fixtures and
// test_main.cpp for the roster that runs these.
#include "test_gates.h"

#include "core/render/glitch.h"

// --- Gate: the glitch comes in bursts, a fixed length each window ---
//
// The schedule is what keeps a wild READABLE: mostly whole, torn for a moment. So every
// window holds exactly kGlitchBurstLen torn beats in one unbroken run — never none (the
// corruption would vanish for good), never two runs (a stutter), never more (the body
// stops reading). Before the clock starts there is nothing to tear.
void test_glitch_bursts_once_per_window() {
    for (int w = 0; w < 64; ++w) {
        int on = 0, runs = 0;
        bool prev = false;
        for (int b = 0; b < kGlitchBurstPeriod; ++b) {
            const bool now = glitchBurst(w * kGlitchBurstPeriod + b);
            on += now ? 1 : 0;
            runs += (now && !prev) ? 1 : 0;
            prev = now;
        }
        CHECK(on == kGlitchBurstLen);
        CHECK(runs == 1);
    }
    CHECK(!glitchBurst(-1));
}

// --- Gate: a two-tone wild keeps its own colours through the shader ---
//
// `base` is for a one-ink mask; a malbeast sheet carries a line ink and a body fill of its
// own, and flattening them to `base` would undo the filled body exactly when it tears. So
// over a whole burst, every pixel the shader lays is one of the sheet's own colours or the
// `hot` accent — never `base`.
void test_glitch_keeps_a_two_tone_sheet_s_colours() {
    const SpriteData& s = ASSET_SPR_MALBEAST_GLITCHHOG;
    CHECK(!spriteIsMask(s));
    const Rgb565 bg = rgb565(0, 0, 255), base = rgb565(0, 255, 0), hot = rgb565(255, 0, 0);
    for (int phase = 0; phase < 256; phase += 32) {
        Framebuffer fb(s.frameW * 2, s.h * 2);
        fb.clear(bg);
        drawSpriteGlitch(fb, s, 0, 0, 0, 1, 1, base, hot, static_cast<uint8_t>(phase));
        bool sawSprite = false;
        for (int y = 0; y < fb.height(); ++y)
            for (int x = 0; x < fb.width(); ++x) {
                const Rgb565 c = fb.get(x, y);
                CHECK(c != base);
                if (c != bg && c != hot) sawSprite = true;
            }
        CHECK(sawSprite);
    }
}
