#include "core/app/game.h"

#include "core/render/canvas.h"
#include "core/render/palette.h"
#include "core/render/shred.h"
#include "core/render/sprite.h"
#include "generated/assets.h"

// game_feeding.cpp: the envelopes a skimming line chases on the idle shelf
// (core/model/idle_feeding.h). Drawn over the pet, which is usually larger than the
// letter and would otherwise hide it the moment it arrives.

namespace mal {

void Game::drawFeedingMail(Framebuffer& fb) const {
    if (!petFeeding_.feeding()) return;
    const SpriteData& env = ASSET_SPR_MAIL_ENVELOPE;
    const int w = env.frameW * kScaleNum / kScaleDen;
    const int h = env.h * kScaleNum / kScaleDen;
    for (int i = 0; i < kIdleFeedLetters; ++i) {
        const Mail& m = petFeeding_.letter(i);
        if (m.state != MailState::Sinking && m.state != MailState::Bitten) continue;
        const int x = kActiveW / 2 + logicalToActive(m.x) - w / 2;
        const int y = kLivingBottom - logicalToActive(m.y) - h;
        if (m.state == MailState::Bitten) {
            const uint8_t progress =
                static_cast<uint8_t>((m.biteBeat + 1) * 255 / kIdleFeedBiteBeats);
            drawShred(fb, env, 0, x, y, kScaleNum, kScaleDen, palColor(Pal::INK), progress);
            continue;
        }
        // Rocking as it sinks, the way a flake does through water; still once it lands.
        const int frame = m.y == 0 ? 0 : 1 + ((beat_ + i) & 1);
        drawSpriteUpscaled(fb, env, frame, x, y, kScaleNum, kScaleDen);
    }
}

}  // namespace mal
