#include "core/app/game.h"

#include <algorithm>
#include <cstdio>

#include "core/render/canvas.h"
#include "core/render/font.h"
#include "core/render/framebuffer.h"
#include "core/render/palette.h"
#include "core/render/sprite.h"
#include "generated/assets.h"

// game_eggpick.cpp: the Clutch Pick, the Phishing line's hatch minigame.
//
// A Phishing or Metamorphic egg is laid into a raft of identical decoys, one of which is
// the live egg; the raft and its draw are EggPickScreen (game/egg_pick_screen.h).
//
// The player then halves the clutch kEggPickRounds times: A aims at the first half
// (left / top), C at the second (right / bottom), B commits. The split alternates axis
// each round, so 8x4 slots narrow 8x4 -> 4x4 -> 4x2 -> 2x2. Finish with the live egg
// still inside and the incubation clock halves (Game::startEggPick's caller keeps the
// full clock otherwise): a miss costs only the bonus, never the pet.
//
// Entered from Game::startHatchGame the instant the egg is laid; leaves to idle.
//
// The unit also owns the HATCH REVEAL (bottom of the file): the on-demand crack that
// lets a line without a decrypt show its shell opening.

namespace mal {

// --- Lifecycle -------------------------------------------------------------

void Game::startEggPick(int rounds) {
    // Hide the live egg: an equal-weight draw over the non-wrapping slots, off the same
    // shared LCG every other content draw uses (rollHatchProcess, the Trojan divert).
    rng_ = rng_ * 1664525u + 1013904223u;
    int pool[kEggPickCols * kEggPickRows];
    int n = 0;
    for (int s = 0; s < kEggPickCols * kEggPickRows; ++s)
        if (EggPickScreen::slotCanHideEgg(s)) pool[n++] = s;
    eggPick_ = EggPick::started(pool[(rng_ >> 16) % n], rounds);
    closeGameBrief();   // a stale flag from a previous clutch must not open paused
    nav_ = Nav::ModalEggPick;
    dirty_ = true;
}

void Game::eggPickAim(bool secondHalf) {
    if (eggPick_ && eggPick_->aim(secondHalf)) dirty_ = true;
}

void Game::eggPickCommit() {
    if (!eggPick_) return;
    if (eggPick_->resolved) {
        const bool won = eggPick_->won;
        eggPick_.reset();
        // An arcade raft has no egg and no clock to halve, so the till takes the
        // verdict. Win-or-lose: a halving either kept the live egg or it didn't, and
        // there is no partial credit to award.
        if (inArcadeRun()) {
            finishArcadeRun(won, 0, 0);
            return;
        }
        // The reveal has been read: bank the result and drop the player at idle with
        // their egg. Winning halves what's left of the incubation clock; that IS the
        // whole prize, since this line never opens the decrypt modal to grind down.
        if (won && pet_.bootHatchRemainMs > 0) pet_.bootHatchRemainMs /= 2;
        nav_ = Nav::Idle;
        dirty_ = true;
        markSaveDirty();
        return;
    }
    eggPick_->commit();
    const EggPick& p = *eggPick_;
    playSound(!p.resolved ? Sound::Point : p.won ? Sound::Clear : Sound::Crash);
    dirty_ = true;
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

    // The shell's own one-shot, played big and centred with nothing else on screen.
    if (const SpriteData* egg = hatchEggSprite()) {
        const int w = egg->frameW * kScaleNum / kScaleDen;
        const int h = egg->h * kScaleNum / kScaleDen;
        drawSpriteUpscaled(fb, *egg, hatchRevealFrame(), (kActiveW - w) / 2,
                           (kActiveH - h) / 2, kScaleNum, kScaleDen);
    }
}

}  // namespace mal
