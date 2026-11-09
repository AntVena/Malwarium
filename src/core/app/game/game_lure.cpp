#include "core/app/game.h"

#include "core/render/canvas.h"
#include "core/render/font.h"
#include "core/render/palette.h"
#include "core/render/shred.h"
#include "core/render/sprite.h"
#include "generated/assets.h"

// game_lure.cpp: the bait a skimming line sets between skims (core/model/idle_lure.h).
// Prize tags on lines out of the pet's head, the cursor that comes in for one, and the
// reel back to the pet's mouth. Lines start at the origin the habitat passes, the top of
// the pet's body.

namespace mal {

namespace {

// Where each tag hangs off the origin, active px, and what it promises.
struct TagSeat {
    int dx, dy;
    const char* prize;
};
constexpr TagSeat kTagSeats[kIdleLureTags] = {
    {-74, -10, "WIN!"},
    {0, -40, "FREE"},
    {74, -10, "$$$"},
};
// The tag's body (past its point and eyelet) in logical px, where the prize is written;
// the eyelet the line ties to.
constexpr int kTagBodyX0 = 6, kTagBodyX1 = 26;
constexpr int kTagEyeX = 4, kTagEyeY = 5;
constexpr int kLineDash = 3;
constexpr int kLureTopClear = kLivingTop + 4 + kFontH + 4;   // under the caption row

int lerp(int a, int b, int num, int den) { return a + (b - a) * num / den; }

int clampTo(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

void dottedLine(Framebuffer& fb, int x0, int y0, int x1, int y1, Rgb565 c) {
    const int dx = x1 - x0, dy = y1 - y0;
    const int steps = (dx < 0 ? -dx : dx) > (dy < 0 ? -dy : dy) ? (dx < 0 ? -dx : dx)
                                                                : (dy < 0 ? -dy : dy);
    for (int i = 0; i <= steps; ++i) {
        if ((i / kLineDash) & 1) continue;
        const int x = steps ? x0 + dx * i / steps : x0;
        const int y = steps ? y0 + dy * i / steps : y0;
        fb.blendPixel(x, y, c, 255);
    }
}

}  // namespace

void Game::drawLure(Framebuffer& fb, int originX, int originY) const {
    const LureStage stage = petLure_.stage();
    if (stage == LureStage::Rest) return;
    const int b = petLure_.beatInStage();
    const int len = IdleLure::stageBeats(stage);
    const SpriteData& tag = ASSET_SPR_LURE_TAG;
    const int tagW = tag.frameW * kScaleNum / kScaleDen;
    const int tagH = tag.h * kScaleNum / kScaleDen;
    const int eyeX = logicalToActive(kTagEyeX), eyeY = logicalToActive(kTagEyeY);

    // Every tag's seat, kept on screen and under the caption row.
    int seatX[kIdleLureTags], seatY[kIdleLureTags];
    for (int i = 0; i < kIdleLureTags; ++i) {
        seatX[i] = clampTo(originX + kTagSeats[i].dx - tagW / 2, 2, kActiveW - tagW - 2);
        seatY[i] = clampTo(originY + kTagSeats[i].dy - tagH / 2, kLureTopClear,
                           kLivingBottom - tagH);
    }

    // Where the tags are this beat: flying out on the cast, bobbing on their lines, then
    // all reeled home to the origin.
    int tx[kIdleLureTags], ty[kIdleLureTags];
    const int oX = originX - eyeX, oY = originY - eyeY;
    for (int i = 0; i < kIdleLureTags; ++i) {
        if (stage == LureStage::Cast) {
            tx[i] = lerp(oX, seatX[i], b + 1, len);
            ty[i] = lerp(oY, seatY[i], b + 1, len);
        } else if (stage == LureStage::Reel) {
            tx[i] = lerp(seatX[i], oX, b + 1, len);
            ty[i] = lerp(seatY[i], oY, b + 1, len);
        } else {
            tx[i] = seatX[i];
            ty[i] = seatY[i] + (((beat_ + i) & 1) ? 1 : 0);
        }
    }

    const int target = petLure_.target();
    if (stage != LureStage::Eat) {
        for (int i = 0; i < kIdleLureTags; ++i)
            dottedLine(fb, originX, originY, tx[i] + eyeX, ty[i] + eyeY,
                       palColor(Pal::INK_DIM));
        for (int i = 0; i < kIdleLureTags; ++i) {
            const bool hit = i == target && stage == LureStage::Click && (beat_ & 1) == 0;
            const int bx0 = tx[i] + logicalToActive(kTagBodyX0);
            const int bx1 = tx[i] + logicalToActive(kTagBodyX1);
            fb.fillRect(bx0, ty[i] + 2, bx1 - bx0, tagH - 4,
                        hit ? palColor(Pal::HOT) : palColor(Pal::PAPER));
            drawSpriteUpscaled(fb, tag, 0, tx[i], ty[i], kScaleNum, kScaleDen);
            if (stage == LureStage::Cast || stage == LureStage::Reel) continue;
            const char* prize = kTagSeats[i].prize;
            drawText(fb, bx0 + (bx1 - bx0 - textWidth(prize)) / 2 + 1,
                     ty[i] + (tagH - kFontH) / 2, prize,
                     hit ? palColor(Pal::PAPER) : palColor(Pal::INK));
        }
    }

    // The cursor: in from the edge to the middle of its tag, a click, then dragged home
    // hooked to the tag, and eaten there.
    if (stage < LureStage::Approach) return;
    const SpriteData& cur = ASSET_SPR_LURE_CURSOR;
    const int curW = cur.frameW * kScaleNum / kScaleDen;
    // The arrow's tip sits a few px into its cell (tools/gen_worm_art.py's cursor).
    const int tipX = logicalToActive(3), tipY = logicalToActive(2);
    const int aimX = tx[target] + (logicalToActive(kTagBodyX0) + logicalToActive(kTagBodyX1)) / 2;
    const int aimY = ty[target] + tagH / 2;
    int cx = aimX, cy = aimY;
    if (stage == LureStage::Approach) {
        const int fromX = petLure_.edge() < 0 ? -curW : kActiveW;
        cx = lerp(fromX, aimX, b + 1, len);
    } else if (stage == LureStage::Eat) {
        cx = originX;
        cy = originY;
    }
    if (stage == LureStage::Eat) {
        const uint8_t progress = static_cast<uint8_t>((b + 1) * 255 / len);
        drawShred(fb, cur, 0, cx - tipX, cy - tipY, kScaleNum, kScaleDen, palColor(Pal::INK),
                  progress);
        return;
    }
    const int frame = stage == LureStage::Click ? 1 : 0;
    drawSpriteUpscaled(fb, cur, frame, cx - tipX, cy - tipY, kScaleNum, kScaleDen);
}

}  // namespace mal
