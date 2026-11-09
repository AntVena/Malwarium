#include "core/app/game.h"

#include <cmath>

#include "core/render/dissolve.h"
#include "core/render/font.h"
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
// A digit within kCipherPx of the pet is already encrypted; within kAbsorbPx it is gone.
constexpr int kCipherPx = 56;
constexpr int kAbsorbPx = 14;

constexpr char kCipherGlyphs[] = "0123456789ABCDEF";

}  // namespace

void Game::drawHarvestStreams(Framebuffer& fb, int petCx, int petCy) const {
    if (petHarvest_.stage() != HarvestStage::Stream) return;
    const int b = petHarvest_.beatInStage();
    // The last strand must have swallowed its last digit by the end of the stage.
    const int travelBeats = kIdleHarvestStreamBeats - kStrandStaggerBeats * (kStrandCount - 1);
    const int travelPx = kStrandStartPx - kAbsorbPx + kStrandDigits * kDigitSpacing;
    const Rgb565 plain = palColor(Pal::INK);
    const Rgb565 cipher = palColor(Pal::DECRYPTION_GREEN);
    for (int i = 0; i < kStrandCount; ++i) {
        const int age = b - i * kStrandStaggerBeats;
        if (age < 0) continue;
        const float a = kStrandAngles[i] * 3.14159265f / 180.0f;
        const float ca = std::cos(a), sa = std::sin(a);
        const int head = kStrandStartPx - travelPx * age / travelBeats;
        for (int k = 0; k < kStrandDigits; ++k) {
            const int d = head + k * kDigitSpacing;
            if (d <= kAbsorbPx) continue;
            const bool encrypted = d < kCipherPx;
            const char glyph[2] = {
                encrypted ? kCipherGlyphs[dissolveHash(i * 17 + k, beat_) & 15]
                          : static_cast<char>((dissolveHash(i, k) & 1) ? '1' : '0'),
                '\0'};
            const int x = petCx + static_cast<int>(ca * d) - kFontW / 2;
            const int y = petCy + static_cast<int>(sa * d) - kFontH / 2;
            drawText(fb, x, y, glyph, encrypted ? cipher : plain);
        }
    }
}

}  // namespace mal
