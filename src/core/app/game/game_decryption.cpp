#include "core/app/game.h"

#include "tunables.h"
#include "core/ui/decryption_screen.h"

// game_decryption.cpp — DISK DECRYPTION, the Ransomware line's hatch minigame.
//
// A Cryptoshell egg is laid and the payload that came with it is already holding the
// disk: five colours, three slots, five attempts, and two anonymous counts per row to
// deduce from. Crack the key and the incubation clock halves; run out of attempts and
// the run costs only that bonus — the same deal the Clutch Pick and the Isolation
// Protocol both offer, and the reason none of the three can lose you a pet.
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
        // Parked on the verdict. B banks and leaves; C is DISABLED, like every other
        // hatch screen — there is no pet to go back to, only an egg to get on with.
        if (ev.button == Button::B) finishDecryption();
        return;
    }
    // A cycles the focused slot's colour, B settles it (and the third settle plays the
    // row), C steps back and re-opens the slot it lands on. C is not "cancel" here,
    // which is the deviation the hint band spells out.
    if (ev.button == Button::A) decryption_.cycleColour();
    else if (ev.button == Button::B) decryption_.lockIn();
    else if (ev.button == Button::C) decryption_.stepBack();
    dirty_ = true;
}

void Game::finishDecryption() {
    if (arcadeRun_) {
        finishArcadeRun(decryption_.cracked(), decryption_.score(),
                        DiskDecryption::maxScore());
        return;
    }
    // FIRST_BRUTE_FORCE is this board, and nothing else: the achievement is named for
    // breaking a key, and until now it fired on any hatch at all — including the lines
    // that never see a key. A cracked run is the only thing that earns it.
    if (decryption_.cracked()) {
        unlockAchievement(ach::kFirstBruteForce);
        // Halving what is LEFT, not the full clock: the board is played the instant the
        // egg is laid, so "what's left" is the whole incubation anyway — and pricing it
        // this way keeps the prize honest if the game is ever offered later.
        if (bootHatchRemainMs_ > 0) bootHatchRemainMs_ /= 2;
    }
    nav_ = Nav::Idle;
    dirty_ = true;
    markSaveDirty();
}

void Game::drawDecryption(Framebuffer& fb) const {
    if (gameBriefOpen_) { drawGameBrief(fb); return; }
    drawDiskDecryption(fb, decryption_, decryptionEasy_, arcadeRun_, beat_);
}

}  // namespace mal
