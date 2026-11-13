#include "core/app/game.h"

#include "core/ui/decryption_screen.h"

// game_decryption.cpp: DISK DECRYPTION, the Ransomware line's hatch minigame.
//
// A Cryptoshell egg is laid and the payload that came with it is already holding the
// disk: five colours, three slots, five attempts, and two anonymous counts per row to
// deduce from. Crack the key and the incubation clock halves; run out of attempts and
// you lose only that bonus. The Clutch Pick and the Isolation Protocol work the same
// way, so none of the three can lose a pet.
//
// The rules are core/model/disk_decryption.h, with no framebuffer and no Game in them.
// This file is the lifecycle and the payout; the board is core/ui/decryption_screen.cpp.
//
// Entered from Game::startHatchGame the instant the egg is laid, and from the GAMES
// arcade's own cabinet. Leaves to idle, or to the arcade's till.

namespace mal {

void Game::startDecryption(bool allowDuplicates, bool easyHints) {
    rng_ = rng_ * 1664525u + 1013904223u;   // the shared LCG draws the key
    decryption_.reset(rng_, allowDuplicates);
    decryptionEasy_ = easyHints;
    closeGameBrief();   // a stale flag from a previous board must not open paused
    nav_ = Nav::Decryption;
    dirty_ = true;
}

void Game::onDecryption(const ButtonEvent& ev) {
    if (onGameBriefInput(ev)) return;
    if (!decryption_.running()) {
        // Parked on the verdict. B banks and leaves. C is disabled, as on every
        // hatch screen, because there is no pet to go back to.
        if (ev.button == Button::B) finishDecryption();
        return;
    }
    // A cycles the focused slot's colour, B settles it (and the third settle plays the
    // row), C steps back and re-opens the slot it lands on. C is not "cancel" here,
    // which is the deviation the hint band spells out.
    if (ev.button == Button::A) decryption_.cycleColour();
    else if (ev.button == Button::B) {
        const int rows = decryption_.played();
        decryption_.lockIn();
        if (decryption_.cracked()) playSound(Sound::Clear);
        else if (!decryption_.running()) playSound(Sound::Crash);
        else if (decryption_.played() > rows) playSound(Sound::Probe);
    }
    else if (ev.button == Button::C) decryption_.stepBack();
    dirty_ = true;
}

void Game::finishDecryption() {
    if (inArcadeRun()) {
        finishArcadeRun(decryption_.cracked(), decryption_.score(),
                        DiskDecryption::maxScore());
        return;
    }
    // FIRST_BRUTE_FORCE fires only on this board, because it is named for breaking a
    // key. A cracked run is the only thing that earns it.
    if (decryption_.cracked()) {
        unlockAchievement(ach::kFirstBruteForce);
        // Halves what is left of the clock. The board is played when the egg is laid,
        // so that is the whole incubation, and the prize stays correct if the game is
        // ever offered later.
        if (pet_.bootHatchRemainMs > 0) pet_.bootHatchRemainMs /= 2;
    }
    nav_ = Nav::Idle;
    dirty_ = true;
    markSaveDirty();
}

void Game::drawDecryption(Framebuffer& fb) const {
    if (gameBriefOpen_) { drawGameBrief(fb); return; }
    drawDiskDecryption(fb, decryption_,
                       decryptionEasy_ ? DecryptHints::Exact : DecryptHints::Anonymous,
                       inArcadeRun() ? DecryptStakes::Arcade : DecryptStakes::Hatch, beat_);
}

}  // namespace mal
