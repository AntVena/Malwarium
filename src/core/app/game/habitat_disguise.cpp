#include "core/app/game/home_habitat.h"

#include "core/render/framebuffer.h"
#include "core/render/canvas.h"
#include "core/render/glitch.h"
#include "core/render/palette.h"
#include "core/render/sprite.h"
#include "generated/assets.h"

// habitat_disguise.cpp: the prop a disguising line's pet turns into on the idle shelf
// (core/model/idle_disguise.h). The habitat draws this in place of the creature for as
// long as IdleDisguise::showsProp() holds.

namespace mal {

namespace {

const SpriteData* const kDisguiseProps[] = {
    &ASSET_SPR_DISGUISE_FILE,
    &ASSET_SPR_DISGUISE_FLOPPY,
    &ASSET_SPR_DISGUISE_USB,
    &ASSET_SPR_DISGUISE_MONITOR,
    &ASSET_SPR_DISGUISE_MALWARIUM,
};
constexpr int kDisguisePropCount =
    static_cast<int>(sizeof(kDisguiseProps) / sizeof(kDisguiseProps[0]));

}  // namespace

void HomeHabitat::drawDisguiseProp(Framebuffer& fb, int petCx, int beat) const {
    const SpriteData& prop = *kDisguiseProps[disguise_.slot() % kDisguisePropCount];
    const int w = prop.frameW * kScaleNum / kScaleDen;
    const int h = prop.h * kScaleNum / kScaleDen;
    const int x = petCx - w / 2;
    const int y = kLivingBottom - h;
    if (disguise_.torn())
        drawSpriteGlitch(fb, prop, 0, x, y, kScaleNum, kScaleDen, palColor(Pal::INK),
                         palColor(Pal::HOT), static_cast<uint8_t>(beat * 32));
    else
        drawSpriteUpscaled(fb, prop, disguise_.propFrame(), x, y, kScaleNum, kScaleDen);
}

}  // namespace mal
