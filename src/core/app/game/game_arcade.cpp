#include "core/app/game.h"

#include "tunables.h"
#include "core/content/content_arcade.h"
#include "core/ui/arcade_screen.h"

// game_arcade.cpp — GAMES: the arcade cabinets.
//
// The whole unit is a menu and a till. It starts a minigame that already exists, waits
// for that game to end the way it always ends, and pays for the attempt — so there is
// no arcade copy of any game to keep in step with the original, and changing a hatch
// game changes what the cabinet plays in the same commit.
//
// Two things a run needs that its own context normally supplies, and which the arcade
// therefore has to hand it instead: a GOAL (the Isolation run prices its clean off the
// incubation clock, which a cabinet doesn't have) and a PACE (the difficulty dial). Both
// are parameters at the start call; nothing inside a game knows the arcade exists.
//
// The payout is deliberately mostly flat — see kArcadePlayBits in tunables.h for why.

namespace mal {

// --- The cabinet list (L2) -------------------------------------------------

void Game::onArcadeList(const ButtonEvent& ev) {
    const int n = arcadeGameCount();
    if (ev.button == Button::A) {
        arcadeRow_ = n > 0 ? (arcadeRow_ + 1) % n : 0;
    } else if (ev.button == Button::B) {
        if (n <= 0) return;
        nav_ = Nav::Detail;
    } else if (ev.button == Button::C) {
        nav_ = Nav::Cursor;
    }
}

// --- One cabinet (L3) ------------------------------------------------------

void Game::onArcadeCabinet(const ButtonEvent& ev) {
    if (ev.button == Button::A) {
        arcadeDifficulty_ = static_cast<ArcadeDifficulty>(
            (static_cast<int>(arcadeDifficulty_) + 1) % kArcadeDifficulties);
    } else if (ev.button == Button::B) {
        startArcadeRun();
    } else if (ev.button == Button::C) {
        nav_ = Nav::Submenu;
    }
    dirty_ = true;
}

int Game::arcadeStepMs(int baseMs) const {
    if (!arcadeRun_) return baseMs;
    int pct = kArcadeSpeedPctMedium;
    if (arcadeDifficulty_ == ArcadeDifficulty::Easy) pct = kArcadeSpeedPctEasy;
    else if (arcadeDifficulty_ == ArcadeDifficulty::Hard) pct = kArcadeSpeedPctHard;
    const int ms = baseMs * pct / 100;
    return ms < 1 ? 1 : ms;
}

void Game::startArcadeRun() {
    const int n = arcadeGameCount();
    if (arcadeRow_ < 0 || arcadeRow_ >= n) return;
    arcadeGame_ = arcadeRow_;
    arcadeRun_ = true;
    switch (arcadeGames()[arcadeGame_].kind) {
        case ArcadeGameKind::Stacker:
            beginStackerBoard();
            break;
        case ArcadeGameKind::Clutch: {
            int rounds = kArcadeClutchRoundsMedium;
            if (arcadeDifficulty_ == ArcadeDifficulty::Easy) rounds = kArcadeClutchRoundsEasy;
            else if (arcadeDifficulty_ == ArcadeDifficulty::Hard) rounds = kArcadeClutchRoundsHard;
            startEggPick(rounds);
            break;
        }
        case ArcadeGameKind::Isolation:
            startIsolation(kArcadeIsolationGoal);
            break;
    }
    dirty_ = true;
}

// --- The till --------------------------------------------------------------

void Game::finishArcadeRun(bool won, int score, int scoreMax) {
    arcadeRun_ = false;
    arcadeWon_ = won;
    arcadeScore_ = score;
    arcadeScoreMax_ = scoreMax;

    // The flat half is for finishing at all; the skill half is a PROPORTION of the
    // ceiling where there is one, and all-or-nothing where there isn't. Clamped
    // because a game is free to report a score above its own advertised max (a
    // cleared Stacker board can), and a run must never pay more than the cabinet said.
    int bonus = 0;
    if (scoreMax > 0) {
        bonus = kArcadeScoreBits * score / scoreMax;
        if (bonus > kArcadeScoreBits) bonus = kArcadeScoreBits;
        if (bonus < 0) bonus = 0;
    } else if (won) {
        bonus = kArcadeScoreBits;
    }
    arcadeBits_ = kArcadePlayBits + bonus;
    bits_ += arcadeBits_;
    model_.setHappiness(model_.happiness() + kArcadePlayHappy);   // clamps at the cap

    if (arcadeGame_ >= 0 && arcadeGame_ < kArcadeMaxCabinets) {
        ++arcadePlays_[arcadeGame_];
        if (won) ++arcadeWins_[arcadeGame_];
    }
    // The pet enjoyed itself, and PLAY is the signal that says so — the arcade is the
    // one place a player can hand a pet that signal on purpose.
    noteCareSignal(DominantSignal::Play);
    markSaveDirty();

    cursor_ = carouselSlotOf(SubmenuId::Games);
    nav_ = Nav::ArcadeResult;
    dirty_ = true;
}

void Game::onArcadeResult(const ButtonEvent& ev) {
    // Informational: the run is banked already, so any press leaves. C is not "back"
    // here — there is nothing to go back to, only the list to return to.
    (void)ev;
    nav_ = Nav::Submenu;
    dirty_ = true;
}

// --- Render ----------------------------------------------------------------

void Game::drawArcade(Framebuffer& fb) const {
    int plays[kArcadeMaxCabinets] = {0};
    for (int i = 0; i < arcadeGameCount() && i < kArcadeMaxCabinets; ++i)
        plays[i] = arcadePlays_[i];
    drawArcadeList(fb, registry_, plays, arcadeRow_, beat_);
}

void Game::drawArcadeDetail(Framebuffer& fb) const {
    if (arcadeRow_ < 0 || arcadeRow_ >= arcadeGameCount()) return;
    drawArcadeCabinet(fb, registry_, arcadeGames()[arcadeRow_], arcadeDifficulty_,
                      arcadePlays(arcadeRow_), arcadeWins(arcadeRow_));
}

void Game::drawArcadeOutcome(Framebuffer& fb) const {
    if (arcadeGame_ < 0 || arcadeGame_ >= arcadeGameCount()) return;
    drawArcadeResult(fb, arcadeGames()[arcadeGame_], arcadeWon_, arcadeScore_,
                     arcadeScoreMax_, arcadeBits_, kArcadePlayHappy);
}

}  // namespace mal
