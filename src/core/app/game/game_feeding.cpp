#include "core/app/game.h"

#include <cctype>
#include <cstring>

#include "core/render/canvas.h"
#include "core/render/dissolve.h"
#include "core/render/font.h"
#include "core/render/palette.h"
#include "core/render/shred.h"
#include "core/render/sprite.h"
#include "generated/assets.h"

// game_feeding.cpp: what a skimming line does on the idle shelf
// (core/model/idle_feeding.h). The scraps it chases are drawn over the pet, which is
// usually larger than a scrap and would otherwise hide it the moment it arrives. Then
// the lookalike of its name on the caption row, and the login it types it into.

namespace mal {

namespace {

const SpriteData& mailSprite(MailKind k) {
    switch (k) {
        case MailKind::Address:  return ASSET_SPR_MAIL_ADDRESS;
        case MailKind::Password: return ASSET_SPR_MAIL_PASSWORD;
        case MailKind::Letter:   break;
    }
    return ASSET_SPR_MAIL_ENVELOPE;
}

// The swaps a typosquat makes, most convincing first. Never the first letter: that is
// the one a reader checks.
constexpr char kLookalikes[][2] = {
    {'O', '0'}, {'I', '1'}, {'L', 'I'}, {'E', '3'}, {'A', '4'}, {'S', '5'}, {'B', '8'},
};
constexpr int kNameMax = 24;

struct Squat {
    char name[kNameMax];
    int swapAt = -1;
};

Squat squatOf(const char* real) {
    Squat q;
    int n = 0;
    for (; real[n] && n < kNameMax - 1; ++n)
        q.name[n] = static_cast<char>(std::toupper(static_cast<unsigned char>(real[n])));
    q.name[n] = '\0';
    for (const auto& swap : kLookalikes) {
        for (int i = 1; i < n; ++i)
            if (q.name[i] == swap[0]) { q.swapAt = i; break; }
        if (q.swapAt >= 0) break;
    }
    return q;
}

constexpr int kLoginW = 128;
constexpr int kLoginPad = 6;
constexpr int kLoginRowGap = 5;
constexpr int kLoginH = 2 * kFontH + 2 * kLoginPad + kLoginRowGap;
constexpr int kLoginTop = kLivingTop + 18;
constexpr int kDotPx = 4;
constexpr int kTearBandPx = 4;
constexpr int kTearMaxShearPx = 10;

void frameRect(Framebuffer& fb, int x, int y, int w, int h, Rgb565 c) {
    fb.fillRect(x, y, w, 1, c);
    fb.fillRect(x, y + h - 1, w, 1, c);
    fb.fillRect(x, y, 1, h, c);
    fb.fillRect(x + w - 1, y, 1, h, c);
}

}  // namespace

void Game::drawFeedingMail(Framebuffer& fb) const {
    if (!petFeeding_.feeding()) return;
    for (int i = 0; i < kIdleFeedLetters; ++i) {
        const Mail& m = petFeeding_.letter(i);
        if (m.state != MailState::Sinking && m.state != MailState::Bitten) continue;
        const SpriteData& s = mailSprite(m.kind);
        const int w = s.frameW * kScaleNum / kScaleDen;
        const int h = s.h * kScaleNum / kScaleDen;
        const int x = kActiveW / 2 + logicalToActive(m.x) - w / 2;
        const int y = kLivingBottom - logicalToActive(m.y) - h;
        if (m.state == MailState::Bitten) {
            const uint8_t progress =
                static_cast<uint8_t>((m.biteBeat + 1) * 255 / kIdleFeedBiteBeats);
            drawShred(fb, s, 0, x, y, kScaleNum, kScaleDen, palColor(Pal::INK), progress);
            continue;
        }
        // Rocking as it sinks, the way a flake does through water; still once it lands.
        const int frame = m.y == 0 ? 0 : 1 + ((beat_ + i) & 1);
        drawSpriteUpscaled(fb, s, frame, x, y, kScaleNum, kScaleDen);
    }
}

void Game::drawSquatLogin(Framebuffer& fb) const {
    const FeedStage stage = petFeeding_.stage();
    if (!pet_ || (stage != FeedStage::Squat && stage != FeedStage::Login)) return;
    Squat q = squatOf(pet_->displayName);
    const bool squatted = petFeeding_.squatted();
    const bool swapBeat =
        stage == FeedStage::Squat && petFeeding_.beatInStage() == kIdleSquatSwapBeat;
    if (squatted && q.swapAt >= 0) {
        for (const auto& swap : kLookalikes)
            if (q.name[q.swapAt] == swap[0]) { q.name[q.swapAt] = swap[1]; break; }
    }

    // The caption row: its name, and the one letter that changed, caught changing.
    const int nameW = textWidth(q.name);
    const int nx = (kActiveW - nameW) / 2;
    drawText(fb, nx, kLivingTop + 4, q.name, palColor(Pal::INK));
    if (swapBeat && q.swapAt >= 0) {
        const char swapped[2] = {q.name[q.swapAt], '\0'};
        const int gx = nx + q.swapAt * kFontAdvance;
        fb.fillRect(gx, kLivingTop + 4, kFontAdvance, kFontH, palColor(Pal::PAPER));
        drawText(fb, gx, kLivingTop + 4, swapped, palColor(Pal::HOT));
    }
    if (stage != FeedStage::Login) return;

    const int x0 = (kActiveW - kLoginW) / 2;
    const int y0 = kLoginTop;
    const LoginStep step = petFeeding_.loginStep();
    if (step == LoginStep::Torn) {
        // Torn away: the box in bands, each knocked sideways, some gone.
        for (int band = 0; band * kTearBandPx < kLoginH; ++band) {
            const uint32_t h = dissolveHash(band, beat_);
            if (h % 3 == 0) continue;
            const int dx = static_cast<int>((h >> 4) % (2 * kTearMaxShearPx + 1)) -
                           kTearMaxShearPx;
            const int by = y0 + band * kTearBandPx;
            const int bh = by + kTearBandPx > y0 + kLoginH ? y0 + kLoginH - by : kTearBandPx;
            fb.fillRect(x0 + dx, by, kLoginW, bh, palColor(Pal::PAPER));
            const Rgb565 edge = (h >> 9) & 1 ? palColor(Pal::HOT) : palColor(Pal::CALM);
            fb.fillRect(x0 + dx, by, 1, bh, edge);
            fb.fillRect(x0 + dx + kLoginW - 1, by, 1, bh, edge);
        }
        return;
    }

    const bool granted = step == LoginStep::Granted;
    fb.fillRect(x0, y0, kLoginW, kLoginH, palColor(Pal::PAPER));
    frameRect(fb, x0, y0, kLoginW, kLoginH,
              granted ? palColor(Pal::CALM) : palColor(Pal::INK_DIM));
    if (granted) {
        const char* ok = "ACCESS GRANTED";
        drawText(fb, (kActiveW - textWidth(ok)) / 2, y0 + (kLoginH - kFontH) / 2, ok,
                 palColor(Pal::CALM));
        return;
    }

    const int userY = y0 + kLoginPad;
    const int passY = userY + kFontH + kLoginRowGap;
    const int fieldX = x0 + kLoginPad + kFontAdvance + 4;
    drawText(fb, x0 + kLoginPad, userY, "@", palColor(Pal::INK_DIM));
    drawText(fb, x0 + kLoginPad, passY, "*", palColor(Pal::INK_DIM));
    const int len = static_cast<int>(std::strlen(q.name));
    char typed[kNameMax];
    const int n = petFeeding_.typed(len);
    std::memcpy(typed, q.name, static_cast<size_t>(n));
    typed[n] = '\0';
    const int end = drawText(fb, fieldX, userY, typed, palColor(Pal::INK));
    for (int i = 0; i < petFeeding_.dots(); ++i)
        fb.fillRect(fieldX + i * (kDotPx + 4), passY + (kFontH - kDotPx) / 2, kDotPx, kDotPx,
                    palColor(Pal::INK));
    // The caret blinks wherever typing is happening.
    if (beat_ & 1) {
        const int cx = step == LoginStep::User
                           ? end + 1
                           : fieldX + petFeeding_.dots() * (kDotPx + 4);
        fb.fillRect(cx, step == LoginStep::User ? userY : passY, 1, kFontH,
                    palColor(Pal::INK));
    }
}

}  // namespace mal
