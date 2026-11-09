#include "core/app/game.h"

#include <cmath>

#include "core/render/dissolve.h"
#include "core/render/font.h"
#include "core/render/font_glyphs.h"
#include "core/render/framebuffer.h"
#include "core/render/palette.h"

// game_harvest.cpp: the strands of data a harvesting line pulls in on the idle shelf
// (core/model/idle_harvest.h). The silhouette the hold draws is FX_RAIN
// (core/render/rain.h), seated by the habitat in place of the creature.

namespace mal {

namespace {

// The strands come in from above and both sides, never from below the shelf, where the
// carousel track covers them. Degrees, screen y down, so 270 is straight up.
constexpr int kStrandAngles[] = {195, 225, 255, 285, 315, 345};
constexpr int kStrandCount = static_cast<int>(sizeof(kStrandAngles) / sizeof(kStrandAngles[0]));
constexpr int kStrandDigits = 6;
constexpr int kDigitSpacing = 10;
// Strands start this far out (past the bezel on every side) and stagger their starts so
// they arrive one after another rather than as a ring closing.
constexpr int kStrandStartPx = 160;
constexpr int kStrandStaggerBeats = 1;
// Within kTearPx of the pet a digit starts to tear, worse the closer it gets; within
// kAbsorbPx it is gone.
constexpr int kTearPx = 72;
constexpr int kAbsorbPx = 14;
// At the pet's edge a torn scanline shears up to kMaxShearPx and up to kMaxDropPct of a
// glyph's rows drop out.
constexpr int kMaxShearPx = 5;
constexpr int kMaxDropPct = 45;
constexpr uint8_t kPulseLightAlpha = 140;

// One digit, scanline by scanline, each row sheared sideways or dropped by `tear`
// (0 clean, 255 at the pet). The rolls change every beat so a torn digit flickers.
void drawTornDigit(Framebuffer& fb, int x, int y, char digit, Rgb565 color, int tear,
                   uint32_t seed) {
    const uint8_t* g = fontGlyph(digit);
    for (int r = 0; r < kFontH; ++r) {
        const uint32_t h = dissolveHash(static_cast<int>(seed), r);
        if (static_cast<int>(h % 100) * 255 < kMaxDropPct * tear) continue;
        const int shear = static_cast<int>((h >> 8) % (2 * kMaxShearPx + 1)) - kMaxShearPx;
        const int dx = shear * tear / 255;
        for (int c = 0; c < kFontW; ++c)
            if (g[r] & (1u << (kFontW - 1 - c))) fb.blendPixel(x + c + dx, y + r, color, 255);
    }
}

}  // namespace

void Game::drawHarvestStreams(Framebuffer& fb, int petCx, int petCy) const {
    if (petHarvest_.stage() != HarvestStage::Stream) return;
    const int b = petHarvest_.beatInStage();
    // The last strand must have swallowed its last digit by the end of the stage.
    const int travelBeats = kIdleHarvestStreamBeats - kStrandStaggerBeats * (kStrandCount - 1);
    const int travelPx = kStrandStartPx - kAbsorbPx + kStrandDigits * kDigitSpacing;
    const Rgb565 green = palColor(Pal::DECRYPTION_GREEN);
    const Rgb565 light = blend(green, palColor(Pal::INK), kPulseLightAlpha);
    for (int i = 0; i < kStrandCount; ++i) {
        const int age = b - i * kStrandStaggerBeats;
        if (age < 0) continue;
        const float a = kStrandAngles[i] * 3.14159265f / 180.0f;
        const float ca = std::cos(a), sa = std::sin(a);
        const int head = kStrandStartPx - travelPx * age / travelBeats;
        for (int k = 0; k < kStrandDigits; ++k) {
            const int d = head + k * kDigitSpacing;
            if (d <= kAbsorbPx) continue;
            const int tear = d >= kTearPx ? 0 : (kTearPx - d) * 255 / (kTearPx - kAbsorbPx);
            const char digit = (dissolveHash(i, k) & 1) ? '1' : '0';
            const Rgb565 color = ((beat_ + i + k) & 1) ? light : green;
            const int x = petCx + static_cast<int>(ca * d) - kFontW / 2;
            const int y = petCy + static_cast<int>(sa * d) - kFontH / 2;
            drawTornDigit(fb, x, y, digit, color, tear, dissolveHash(i * 17 + k, beat_));
        }
    }
}

}  // namespace mal
