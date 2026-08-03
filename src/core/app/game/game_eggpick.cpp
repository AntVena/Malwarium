#include "core/app/game.h"

#include <cstdio>

#include "core/render/canvas.h"
#include "core/render/font5x7.h"
#include "core/render/framebuffer.h"
#include "core/render/palette.h"
#include "core/render/sprite.h"
#include "generated/assets.h"

// game_eggpick.cpp — the Clutch Pick, the Phishing line's hatch minigame.
//
// A Phishing egg is laid into a raft of identical decoys (BG_EGG_CLUTCH), one of which
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

void Game::startEggPick() {
    // Hide the live egg: an equal-weight draw over the non-wrapping slots, off the same
    // shared LCG every other content draw uses (rollHatchProcess, the Trojan divert).
    rng_ = rng_ * 1664525u + 1013904223u;
    int pool[kSlots];
    int n = 0;
    for (int s = 0; s < kSlots; ++s)
        if (!slotWraps(s)) pool[n++] = s;
    eggPickTarget_ = static_cast<uint8_t>(pool[(rng_ >> 16) % n]);

    eggPickRound_ = 0;
    eggPickCol_ = 0;
    eggPickColSpan_ = kEggPickCols;
    eggPickRow_ = 0;
    eggPickRowSpan_ = kEggPickRows;
    eggPickSecondHalf_ = false;
    eggPickResolved_ = false;
    eggPickWon_ = false;
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

    if (eggPickRound_ >= kEggPickRounds) {
        eggPickResolved_ = true;
        eggPickWon_ = eggPickTargetInSpan();
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

// Wash a slot's cell in PAPER at `amt`, dimming whatever the backdrop baked there. A
// wrapped cell is painted at both of its positions so the two visible halves match.
void drawCellScrim(Framebuffer& fb, int slot, uint8_t amt) {
    const Rgb565 c = palColor(Pal::PAPER);
    for (int pass = 0; pass < (slotWraps(slot) ? 2 : 1); ++pass) {
        const int x0 = kPanelX + (slotX(slot) - pass * kClutchW) * kZoom;
        const int y0 = kPanelY + slotY(slot) * kZoom;
        for (int y = 0; y < kCell * kZoom; ++y)
            for (int x = 0; x < kCell * kZoom; ++x)
                fb.blendPixel(x0 + x, y0 + y, c, amt);
    }
}

}  // namespace

void Game::drawEggPick(Framebuffer& fb) const {
    fb.clear(palColor(Pal::PAPER));

    const char* title = "SPOT THE PHISH";
    drawText(fb, (kActiveW - textWidth(title)) / 2, 14, title, palColor(Pal::INK));

    // The raft of decoys, then the one live egg swapped in over its own cell. The tile
    // is opaque and cell-sized, so it replaces a decoy exactly; alternating its two
    // frames on the heartbeat is the only motion on screen.
    drawSpriteUpscaled(fb, ASSET_BG_EGG_CLUTCH, 0, kPanelX, kPanelY, kZoom, 1);
    drawSpriteUpscaled(fb, ASSET_SPR_EGG_PHISH_MICRO, beat_ & 1,
                       kPanelX + slotX(eggPickTarget_) * kZoom,
                       kPanelY + slotY(eggPickTarget_) * kZoom, kZoom, 1);

    const bool byColumn =
        splitsColumns(eggPickRound_, eggPickColSpan_, eggPickRowSpan_);
    // Where this round's cut falls inside the surviving span; only meaningful while
    // rounds remain, and unread once eggPickResolved_.
    const int cutCol = eggPickCol_ + eggPickColSpan_ / 2;
    const int cutRow = eggPickRow_ + eggPickRowSpan_ / 2;

    for (int s = 0; s < kSlots; ++s) {
        const int c = slotCol(s), r = slotRow(s);
        const bool alive = c >= eggPickCol_ && c < eggPickCol_ + eggPickColSpan_ &&
                           r >= eggPickRow_ && r < eggPickRow_ + eggPickRowSpan_;
        // The reveal buries every cell but the live egg's, so wherever the run went the
        // answer is the one thing still lit on screen. The verdict line adjudicates
        // it in words — drawing the committed span too would only add a second bright
        // shape to compare against, and its bounding box can't be honest anyway once
        // the odd-row shift straddles a column edge.
        if (eggPickResolved_) {
            if (s != eggPickTarget_) drawCellScrim(fb, s, kScrimDead);
            continue;
        }
        if (!alive) {
            drawCellScrim(fb, s, kScrimDead);
            continue;
        }
        const bool inSecondHalf = byColumn ? (c >= cutCol) : (r >= cutRow);
        if (inSecondHalf != eggPickSecondHalf_) drawCellScrim(fb, s, kScrimIdle);
    }

    if (eggPickResolved_) {
        const char* verdict = eggPickWon_ ? "LIVE EGG FOUND" : "DECOY - IT GOT AWAY";
        const char* effect = eggPickWon_ ? "INCUBATION HALVED" : "FULL INCUBATION";
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
                  kEggPickRounds);
    drawText(fb, (kActiveW - textWidth(round)) / 2, 30, round, palColor(Pal::INK_DIM));

    // A solid bar down the outer edge of the aimed half. The scrim already carries the
    // choice in luminance; this repeats it as pure SHAPE and position, so the aim reads
    // at a glance and doesn't lean on the dimming alone.
    constexpr int kEdge = 4;
    const Rgb565 aim = palColor(Pal::ACCENT);
    const int panelH = kClutchH * kZoom;
    if (byColumn) {
        fb.fillRect(eggPickSecondHalf_ ? kActiveW - kEdge : 0, kPanelY, kEdge, panelH,
                    aim);
    } else {
        fb.fillRect(0, eggPickSecondHalf_ ? kPanelY + panelH - kEdge : kPanelY,
                    kActiveW, kEdge, aim);
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
