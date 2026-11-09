#include "core/app/game.h"

#include "core/render/canvas.h"
#include "core/render/framebuffer.h"
#include "core/render/palette.h"
#include "core/render/sprite.h"

// game_mimic.cpp: a shapeshifting line wearing another family's creature on the idle
// shelf (core/model/idle_mimic.h). Both stand on the pet's feet, centred on it; a
// scanline comes down over them, and above it is whoever is taking over.

namespace mal {

namespace {

constexpr int kScanlinePx = 2;

int lerp(int a, int b, int num, int den) { return a + (b - a) * num / den; }

}  // namespace

void Game::drawMimic(Framebuffer& fb, const SpriteData& pet, int frame, int row, int petX,
                     int petY, bool mirror) const {
    const SpriteData& worn = *idleMimicWorn_;
    const int petW = pet.frameW * kScaleNum / kScaleDen;
    const int petH = pet.h * kScaleNum / kScaleDen;
    const int wornW = worn.frameW * kScaleNum / kScaleDen;
    const int wornH = worn.h * kScaleNum / kScaleDen;
    const int feet = petY + petH;
    const int wornX = petX + petW / 2 - wornW / 2;
    const int wornY = feet - wornH;
    const bool wornMirror = spriteMirrorToFace(worn, petWander_.headingRight());
    const int wornFrame = idleFrame(worn, beat_);

    const auto drawPet = [&] {
        drawSpriteUpscaled(fb, pet, frame, petX, petY, kScaleNum, kScaleDen, row, mirror);
    };
    const auto drawWorn = [&] {
        drawSpriteUpscaled(fb, worn, wornFrame, wornX, wornY, kScaleNum, kScaleDen, 0,
                           wornMirror);
    };

    const MimicStage stage = petMimic_.stage();
    if (stage == MimicStage::Hold) { drawWorn(); return; }

    const int top = petY < wornY ? petY : wornY;
    const int lineY = lerp(top, feet, petMimic_.wipe(), kIdleMimicWipeBeats);
    const bool takingOver = stage == MimicStage::WipeIn;
    fb.setClip(0, 0, kActiveW, lineY);
    if (takingOver) drawWorn(); else drawPet();
    fb.setClip(0, lineY, kActiveW, kActiveH - lineY);
    if (takingOver) drawPet(); else drawWorn();
    fb.clearClip();
    if (lineY >= feet) return;
    const int x0 = petX < wornX ? petX : wornX;
    const int x1 = petX + petW > wornX + wornW ? petX + petW : wornX + wornW;
    fb.fillRect(x0, lineY, x1 - x0, kScanlinePx, palColor(Pal::ACCENT));
}

}  // namespace mal
