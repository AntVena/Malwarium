#include "core/app/game.h"

#include <algorithm>
#include <cstdio>

#include "core/render/canvas.h"
#include "core/render/font.h"
#include "core/render/framebuffer.h"
#include "core/render/palette.h"
#include "core/render/sprite.h"
#include "generated/assets.h"

// game_eggpick.cpp — the Clutch Pick, the Phishing line's hatch minigame.
//
// A Phishing or Metamorphic egg is laid into a raft of identical decoys (BG_EGG_CLUTCH), one of which
// is quietly swapped for the live egg (SPR_EGG_PHISH_MICRO). Every decoy is baked into
// the backdrop and dead still; only the live tile animates, so MOTION is the only tell
// and the puzzle reads with the colour stripped out.
//
// The player then halves the clutch Game::kEggPickRounds times: A aims at the first half
// (left / top), C at the second (right / bottom), B commits. The split alternates axis
// each round, so 8x4 slots narrow 8x4 -> 4x4 -> 4x2 -> 2x2. Finish with the live egg
// still inside and the incubation clock halves (Game::startEggPick's caller keeps the
// full clock otherwise) — a miss costs only the bonus, never the pet.
//
// Entered from Game::startHatchGame the instant the egg is laid; leaves to idle.
//
// The unit also owns the HATCH REVEAL (bottom of the file) — the on-demand crack that
// gives a line without a decrypt somewhere to actually watch its shell open, rather
// than having the egg quietly become a pet while nobody is looking.

namespace mal {

namespace {

// The clutch panel's own geometry, baked into BG_EGG_CLUTCH: a 112x56 field of 14px
// cells, 8 across and 4 down, with odd rows shifted half a cell so the raft doesn't
// read as a grid. That shift pushes the last odd-row cell off the right edge, where it
// wraps around to the left — see kSlotWraps below.
constexpr int kClutchW = 112;
constexpr int kClutchH = 56;
constexpr int kCell = 14;
constexpr int kOddRowShift = kCell / 2;
constexpr int kSlots = Game::kEggPickCols * Game::kEggPickRows;

// The panel draws at x2, not the engine's usual x1.75: 112 doubles to exactly the
// 224-wide active canvas, and every cell/shift stays whole-pixel, so the live tile lands
// dead-on the decoy it replaces. At x1.75 a 14px cell is 24.5px and the tile would sit
// half a pixel off its own cell.
constexpr int kZoom = 2;
constexpr int kPanelX = (kActiveW - kClutchW * kZoom) / 2;
constexpr int kPanelY = (kActiveH - kClutchH * kZoom) / 2;

// Scrim strengths over an eliminated / not-currently-aimed-at cell, giving three
// luminance steps (aimed > in play > eliminated) that survive grayscale. Pitched for a
// DIM panel, where a subtle wash closes up to nothing — the aimed half has to separate
// at a glance in bad light, not just measurably.
// kScrimIdle is still the restrained one, because an unaimed cell is exactly where the
// player has to keep hunting for the animated tell before committing; it reads as "not
// selected" while leaving the motion legible (the tell is a frame SWAP, which survives
// the wash far better than a brightness difference does). An eliminated cell has
// nothing left to tell, so kScrimDead can bury it.
constexpr uint8_t kScrimDead = 216;   // halved away in an earlier round
constexpr uint8_t kScrimIdle = 124;   // still in play, but not the aimed half

int slotCol(int slot) { return slot % Game::kEggPickCols; }
int slotRow(int slot) { return slot / Game::kEggPickCols; }

// Top-left of a slot's cell in panel space. Odd rows are shifted, which can push a cell
// past the right edge; it wraps modulo the panel width and is ALSO drawn one panel to
// the left so the two halves line up (see drawCellRect).
int slotX(int slot) {
    return (slotCol(slot) * kCell + (slotRow(slot) % 2) * kOddRowShift) % kClutchW;
}
int slotY(int slot) { return slotRow(slot) * kCell; }

// A wrapped cell straddles both edges of the panel, so it can't be honestly said to be
// in the left half or the right half. Decoys there are fine (they're wallpaper), but the
// LIVE egg never hides in one — hence the target draw is single-position, and a player
// is never asked to pick a side for an egg that's on both.
bool slotWraps(int slot) { return slotX(slot) + kCell > kClutchW; }

}  // namespace

// --- Lifecycle -------------------------------------------------------------

void Game::startEggPick(int rounds) {
    // Hide the live egg: an equal-weight draw over the non-wrapping slots, off the same
    // shared LCG every other content draw uses (rollHatchProcess, the Trojan divert).
    rng_ = rng_ * 1664525u + 1013904223u;
    int pool[kSlots];
    int n = 0;
    for (int s = 0; s < kSlots; ++s)
        if (!slotWraps(s)) pool[n++] = s;
    eggPickTarget_ = static_cast<uint8_t>(pool[(rng_ >> 16) % n]);

    eggPickRounds_ = static_cast<uint8_t>(rounds < 1 ? 1 : rounds);
    eggPickRound_ = 0;
    eggPickCol_ = 0;
    eggPickColSpan_ = kEggPickCols;
    eggPickRow_ = 0;
    eggPickRowSpan_ = kEggPickRows;
    eggPickSecondHalf_ = false;
    eggPickResolved_ = false;
    eggPickWon_ = false;
    closeGameBrief();   // a stale flag from a previous clutch must not open paused
    nav_ = Nav::ModalEggPick;
    dirty_ = true;
}

bool Game::eggPickTargetInSpan() const {
    const int c = slotCol(eggPickTarget_), r = slotRow(eggPickTarget_);
    return c >= eggPickCol_ && c < eggPickCol_ + eggPickColSpan_ &&
           r >= eggPickRow_ && r < eggPickRow_ + eggPickRowSpan_;
}

namespace {

// Which axis this round halves. Rounds alternate columns/rows so the surviving span
// stays as square as it can, falling back to the other axis if one is already down to a
// single track (a guard, not a case the shipped 8x4/3-round shape reaches).
bool splitsColumns(int round, int colSpan, int rowSpan) {
    if (colSpan < 2) return false;
    if (rowSpan < 2) return true;
    return (round % 2) == 0;
}

}  // namespace

void Game::eggPickAim(bool secondHalf) {
    if (eggPickResolved_) return;   // the reveal is a read-only screen; only B leaves it
    if (eggPickSecondHalf_ == secondHalf) return;
    eggPickSecondHalf_ = secondHalf;
    dirty_ = true;
}

void Game::eggPickCommit() {
    if (eggPickResolved_) {
        // An arcade raft has no egg behind it, so there is no clock to halve — the
        // till takes the verdict instead. Win-or-lose: a halving either kept the live
        // egg or it didn't, and there is no partial credit to award.
        if (arcadeRun_) {
            finishArcadeRun(eggPickWon_, 0, 0);
            return;
        }
        // The reveal has been read: bank the result and drop the player at idle with
        // their egg. Winning halves what's left of the incubation clock; that IS the
        // whole prize, since this line never opens the decrypt modal to grind down.
        if (eggPickWon_ && bootHatchRemainMs_ > 0) bootHatchRemainMs_ /= 2;
        nav_ = Nav::Idle;
        dirty_ = true;
        markSaveDirty();
        return;
    }

    if (splitsColumns(eggPickRound_, eggPickColSpan_, eggPickRowSpan_)) {
        const uint8_t half = eggPickColSpan_ / 2;
        if (eggPickSecondHalf_) eggPickCol_ = static_cast<uint8_t>(eggPickCol_ + half);
        eggPickColSpan_ = half;
    } else {
        const uint8_t half = eggPickRowSpan_ / 2;
        if (eggPickSecondHalf_) eggPickRow_ = static_cast<uint8_t>(eggPickRow_ + half);
        eggPickRowSpan_ = half;
    }
    ++eggPickRound_;
    eggPickSecondHalf_ = false;   // every round re-opens aimed at its first half

    // The moment the live egg falls out of the surviving span, the run is already
    // decided — playing out the rounds that are left would just be asking the player
    // to keep halving a raft that can no longer contain the answer. Resolve here,
    // on the turn that actually lost it, rather than at the scheduled last round.
    if (!eggPickTargetInSpan()) {
        eggPickResolved_ = true;
        eggPickWon_ = false;
    } else if (eggPickRound_ >= eggPickRounds_) {
        eggPickResolved_ = true;
        eggPickWon_ = true;   // still in span at the last round = the live egg survived
    }
    dirty_ = true;
}

// --- Render ----------------------------------------------------------------

namespace {

// A solid triangle on the 5x7 text grid, so an arrow can sit inline with a button
// letter. The built-in font has no arrow glyphs (the same gap drawRowCursor fills for
// the focused-row marker), and the direction here IS the information — it names which
// way the half this button claims lies.
enum class Arrow { Left, Right, Up, Down };

void drawArrow(Framebuffer& fb, int x, int y, Arrow dir, Rgb565 c) {
    for (int r = 0; r < kFontH; ++r) {
        for (int col = 0; col < kFontW; ++col) {
            // Distance from the tip along the arrow's axis; the triangle widens by one
            // pixel per step away from it.
            int along, across, span;
            switch (dir) {
                case Arrow::Left:  along = col;             across = r;   span = kFontH; break;
                case Arrow::Right: along = kFontW - 1 - col; across = r;  span = kFontH; break;
                case Arrow::Up:    along = r;               across = col; span = kFontW; break;
                case Arrow::Down:  along = kFontH - 1 - r;  across = col; span = kFontW; break;
            }
            const int half = span / 2;
            if (across >= half - along && across <= half + along) fb.set(x + col, y + r, c);
        }
    }
}

// How big a crop of `colSpan` x `rowSpan` CELLS reads at, in whole-pixel zoom. Shrinks
// less than one pass in from the full raft (there's nowhere smaller to grow into yet)
// and grows as the span narrows, so the surviving batch keeps filling roughly the same
// stage instead of shrinking to a speck in the middle of an empty panel.
int cropZoom(int colSpan, int rowSpan) {
    constexpr int kAvailW = 200, kAvailH = 120;
    const int zw = kAvailW / (colSpan * kCell);
    const int zh = kAvailH / (rowSpan * kCell);
    const int z = zw < zh ? zw : zh;
    return z < 2 ? 2 : z;
}

// Blit a SHEET-space rectangle of the clutch backdrop, upscaled by `zoom`. The x axis
// samples modulo the sheet width, the same wrap slotX() itself uses — a crop that
// includes an odd row's half-cell shift needs it (see kOddRowShift), and it costs
// nothing when the crop never reaches the wrap.
void drawClutchCrop(Framebuffer& fb, int srcX0, int srcY0, int srcW, int srcH,
                    int destX, int destY, int zoom) {
    const SpriteData& s = ASSET_BG_EGG_CLUTCH;
    for (int oy = 0; oy < srcH * zoom; ++oy) {
        const int py = srcY0 + oy / zoom;
        for (int ox = 0; ox < srcW * zoom; ++ox) {
            const int px = (srcX0 + ox / zoom) % kClutchW;
            fb.blendPixel(destX + ox, destY + oy, spriteColorAt(s, px, py),
                          spriteAlphaAt(s, px, py));
        }
    }
}

}  // namespace

void Game::drawEggPick(Framebuffer& fb) const {
    if (gameBriefOpen_) { drawGameBrief(fb); return; }
    fb.clear(palColor(Pal::PAPER));

    const char* title = "SPOT THE PHISH";
    drawText(fb, (kActiveW - textWidth(title)) / 2, 14, title, palColor(Pal::INK));

    // Crop to the batch the player can still actually pick — eliminated cells aren't
    // drawn at all, so "the buttons' effect" is never left ambiguous by a wash over a
    // raft that's still fully on screen. Running, that's the surviving span (both row
    // parities' half-shift included, since that's the real shape of those cells, not
    // neighbour bleed — see kOddRowShift). Resolved, it narrows to the live egg's own
    // cell alone, blown up as large as the panel allows, so a miss shows the actual
    // tile that got away rather than asking the player to spot it in the full raft.
    int srcX0, srcY0, srcW, srcH, zoom;
    if (eggPickResolved_) {
        srcX0 = slotX(eggPickTarget_);
        srcY0 = slotY(eggPickTarget_);
        srcW = kCell;
        srcH = kCell;
        zoom = cropZoom(1, 1);
    } else {
        srcX0 = eggPickCol_ * kCell;
        srcY0 = eggPickRow_ * kCell;
        // The shift only ever widens the box; a span already covering every column
        // wraps back onto itself rather than actually needing the extra width, so cap
        // it at the sheet's own size instead of overhanging both panel edges.
        srcW = std::min(eggPickColSpan_ * kCell + kOddRowShift, kClutchW);
        srcH = eggPickRowSpan_ * kCell;
        zoom = cropZoom(eggPickColSpan_, eggPickRowSpan_);
    }
    const int destX = kActiveW / 2 - (srcW * zoom) / 2;
    const int destY = kActiveH / 2 - (srcH * zoom) / 2;
    drawClutchCrop(fb, srcX0, srcY0, srcW, srcH, destX, destY, zoom);

    // The live egg, in that same cropped/zoomed space. It's always inside the crop —
    // running, because eggPickCommit resolves the run the instant it isn't; resolved,
    // because the crop IS its cell — so its offset from the crop's own origin is what
    // places it, wrapping the same way slotX() does.
    {
        const int tx = slotX(eggPickTarget_), ty = slotY(eggPickTarget_);
        int dx = tx - srcX0;
        if (dx < 0) dx += kClutchW;
        const int dy = ty - srcY0;
        drawSpriteUpscaled(fb, ASSET_SPR_EGG_PHISH_MICRO, beat_ & 1, destX + dx * zoom,
                           destY + dy * zoom, zoom, 1);
    }

    if (eggPickResolved_) {
        const char* verdict = eggPickWon_ ? "LIVE EGG FOUND" : "DECOY - IT GOT AWAY";
        // The effect line is the one thing an arcade raft can't inherit: there is no
        // egg behind it, so it names the payout screen that comes next instead of a
        // clock it didn't touch.
        const char* effect = arcadeRun_ ? (eggPickWon_ ? "CABINET CLEARED" : "NO PRIZE")
                             : eggPickWon_ ? "INCUBATION HALVED" : "FULL INCUBATION";
        drawText(fb, (kActiveW - textWidth(verdict)) / 2, 30, verdict,
                 palColor(Pal::INK));
        drawText(fb, (kActiveW - textWidth(effect)) / 2, 186, effect,
                 eggPickWon_ ? palColor(Pal::ACCENT) : palColor(Pal::INK_DIM));
        const char* hint = "B CONTINUE   C DISABLED";
        drawText(fb, (kActiveW - textWidth(hint)) / 2, 204, hint, palColor(Pal::INK_DIM));
        return;
    }

    char round[24];
    std::snprintf(round, sizeof(round), "ROUND %d / %d", eggPickRound_ + 1,
                  eggPickRounds_);
    drawText(fb, (kActiveW - textWidth(round)) / 2, 30, round, palColor(Pal::INK_DIM));

    const bool byColumn = splitsColumns(eggPickRound_, eggPickColSpan_, eggPickRowSpan_);

    // A solid bar down the outer edge of the aimed half, hugging the crop itself now
    // that the eliminated cells aren't drawn at all — the aim reads as pure shape and
    // position against the batch that's actually still in play.
    constexpr int kEdge = 4;
    const Rgb565 aim = palColor(Pal::ACCENT);
    const int dw = srcW * zoom, dh = srcH * zoom;
    if (byColumn) {
        fb.fillRect(eggPickSecondHalf_ ? destX + dw - kEdge : destX, destY, kEdge, dh,
                    aim);
    } else {
        fb.fillRect(destX, eggPickSecondHalf_ ? destY + dh - kEdge : destY, dw, kEdge,
                    aim);
    }

    const char* aimed = byColumn ? (eggPickSecondHalf_ ? "RIGHT HALF" : "LEFT HALF")
                                 : (eggPickSecondHalf_ ? "BOTTOM HALF" : "TOP HALF");
    drawText(fb, (kActiveW - textWidth(aimed)) / 2, 186, aimed, palColor(Pal::INK));

    // UI_HINT_BAND. A and C aim rather than step/cancel here, so the deviation from the
    // standard A/B/C contract is spelled out — and laid out the way the buttons are, A
    // left, B middle, C right, each arrow pointing at the half that button claims. The
    // aimed one is lit and the other dim, so the band shows the choice a second way.
    constexpr int kHintY = 204;
    constexpr int kPad = 10;
    const Rgb565 lit = palColor(Pal::ACCENT), dim = palColor(Pal::INK_DIM);
    const Rgb565 aCol = eggPickSecondHalf_ ? dim : lit;
    const Rgb565 cCol = eggPickSecondHalf_ ? lit : dim;
    const Arrow aDir = byColumn ? Arrow::Left : Arrow::Up;
    const Arrow cDir = byColumn ? Arrow::Right : Arrow::Down;

    drawArrow(fb, kPad, kHintY, aDir, aCol);
    drawText(fb, kPad + kFontW + 3, kHintY, "A", aCol);

    const char* mid = "B PICK";
    drawText(fb, (kActiveW - textWidth(mid)) / 2, kHintY, mid, palColor(Pal::INK));

    const int cx = kActiveW - kPad - kFontW;
    drawText(fb, cx - kFontW - 3, kHintY, "C", cCol);
    drawArrow(fb, cx, kHintY, cDir, cCol);
}

// --- Hatch reveal ----------------------------------------------------------

void Game::openHatchReveal() {
    if (!hatchRevealReady()) return;
    hatchRevealBeat_ = 0;
    nav_ = Nav::ModalHatchReveal;
    dirty_ = true;
}

int Game::hatchRevealFrame() const {
    const SpriteData* egg = hatchEggSprite();
    const int frames = egg ? egg->frames : 1;
    int f = hatchRevealBeat_;
    if (f >= frames) f = frames - 1;
    if (f < 0) f = 0;
    return f;
}

void Game::drawHatchReveal(Framebuffer& fb) const {
    fb.clear(palColor(Pal::PAPER));
    const char* title = "HATCHING";
    drawText(fb, (kActiveW - textWidth(title)) / 2, 20, title, palColor(Pal::INK));

    // The shell's own one-shot, played big and centred with nothing competing for the
    // eye — this screen exists purely so the animation gets watched.
    if (const SpriteData* egg = hatchEggSprite()) {
        const int w = egg->frameW * kScaleNum / kScaleDen;
        const int h = egg->h * kScaleNum / kScaleDen;
        drawSpriteUpscaled(fb, *egg, hatchRevealFrame(), (kActiveW - w) / 2,
                           (kActiveH - h) / 2, kScaleNum, kScaleDen);
    }
}

}  // namespace mal
