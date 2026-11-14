#include "core/app/game/hatch_reveal_screen.h"

#include "tunables.h"
#include "core/render/canvas.h"
#include "core/render/font.h"
#include "core/render/framebuffer.h"
#include "core/render/palette.h"
#include "core/render/sprite.h"

namespace mal {

void HatchRevealScreen::tick(HatchRevealHost& host) {
    const SpriteData* egg = host.hatchEggSprite();
    const int last = (egg ? egg->frames : 1) - 1;
    if (++beat_ >= last + kHatchRevealHoldBeats) host.completeHatch();
}

int HatchRevealScreen::frame(const HatchRevealHost& host) const {
    const SpriteData* egg = host.hatchEggSprite();
    const int frames = egg ? egg->frames : 1;
    int f = beat_;
    if (f >= frames) f = frames - 1;
    if (f < 0) f = 0;
    return f;
}

void HatchRevealScreen::draw(Framebuffer& fb, const HatchRevealHost& host) const {
    fb.clear(palColor(Pal::PAPER));
    const char* title = "HATCHING";
    drawText(fb, (kActiveW - textWidth(title)) / 2, 20, title, palColor(Pal::INK));

    // The shell's own one-shot, played big and centred with nothing else on screen.
    if (const SpriteData* egg = host.hatchEggSprite()) {
        const int w = egg->frameW * kScaleNum / kScaleDen;
        const int h = egg->h * kScaleNum / kScaleDen;
        drawSpriteUpscaled(fb, *egg, frame(host), (kActiveW - w) / 2, (kActiveH - h) / 2,
                           kScaleNum, kScaleDen);
    }
}

}  // namespace mal
