#include "core/app/game.h"

// game_decryption.cpp: DISK DECRYPTION, the Ransomware line's hatch minigame.
//
// A Cryptoshell egg is laid and the payload that came with it is already holding the
// disk: five colours, three slots, five attempts, and two anonymous counts per row to
// deduce from. Crack the key and the incubation clock halves; run out of attempts and
// you lose only that bonus. The Clutch Pick and the Isolation Protocol work the same
// way, so none of the three can lose a pet.
//
// The rules are core/model/disk_decryption.h, with no framebuffer and no Game in them.
// This file is the lifecycle and the payout; the board's input is DecryptionScreen
// (game/decryption_screen.h) and its draw is core/ui/decryption_screen.cpp.
//
// Entered from Game::startHatchGame the instant the egg is laid, and from the GAMES
// arcade's own cabinet. Leaves to idle, or to the arcade's till.

namespace mal {

void Game::startDecryption(bool allowDuplicates, bool easyHints) {
    rng_ = rng_ * 1664525u + 1013904223u;   // the shared LCG draws the key
    decryption_.reset(rng_, allowDuplicates);
    decryptionScreen_.open(easyHints ? DecryptHints::Exact : DecryptHints::Anonymous);
    closeGameBrief();   // a stale flag from a previous board must not open paused
    nav_ = Nav::Decryption;
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

}  // namespace mal
