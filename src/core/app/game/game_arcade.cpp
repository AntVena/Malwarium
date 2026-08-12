#include "core/app/game.h"

#include "tunables.h"
#include "core/content/content_arcade.h"
#include "core/content/content_quotes.h"
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
//
// A locked cabinet is ABSENT from the list, not greyed in it — the Rig Shop's rule
// (shopRowOffered), and for the same reason: a menu should not advertise a door the
// player has no key to. arcadeRow_ stays a ROSTER index throughout, so the tallies and
// the START call index the same space whether or not anything is hidden.

bool Game::arcadeCabinetOffered(int i) const {
    if (i < 0 || i >= arcadeGameCount()) return false;
    switch (arcadeGames()[i].unlock) {
        case ArcadeUnlock::Always: return true;
        case ArcadeUnlock::QuotesSolved: return quotesSolved() >= kQuoteArcadeUnlockWins;
    }
    return true;
}

int Game::arcadeOfferedRows(int* out, int max) const {
    int n = 0;
    for (int i = 0; i < arcadeGameCount() && n < max; ++i)
        if (arcadeCabinetOffered(i)) out[n++] = i;
    return n;
}

void Game::onArcadeList(const ButtonEvent& ev) {
    const int total = arcadeGameCount();
    if (ev.button == Button::A) {
        // Step through raw table order to the next offered row, wrapping; a roster with
        // nothing offered leaves the cursor where it is.
        for (int step = 1; step <= total; ++step) {
            const int next = (arcadeRow_ + step) % total;
            if (arcadeCabinetOffered(next)) { arcadeRow_ = next; break; }
        }
    } else if (ev.button == Button::B) {
        if (!arcadeCabinetOffered(arcadeRow_)) return;
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
    if (!arcadeCabinetOffered(arcadeRow_)) return;   // range + the unlock, in one ask
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
        case ArcadeGameKind::Decryption:
            // One of the two cabinets whose dial moves the RULES: easy outlines the
            // cells a row got exactly right, hard lets the key repeat a colour.
            startDecryption(
                /*allowDuplicates=*/arcadeDifficulty_ == ArcadeDifficulty::Hard,
                /*easyHints=*/arcadeDifficulty_ == ArcadeDifficulty::Easy);
            break;
        case ArcadeGameKind::Cryptogram: {
            // The dial IS the quote's difficulty ladder, so the cabinet reuses the same
            // opening the VAULT would give — no second answer to what MEDIUM means on
            // a quote board. Only a SOLVED quote is offered: the cabinet is a re-run,
            // never a free route through the pool.
            const int quote = pickQuote(QuotePick::SolvedOnly);
            if (quote < 0) {
                arcadeRun_ = false;   // nothing to re-run — leave the cabinet page up
                return;
            }
            CryptogramTier t = CryptogramTier::Hard;
            if (arcadeDifficulty_ == ArcadeDifficulty::Medium) t = CryptogramTier::Medium;
            else if (arcadeDifficulty_ == ArcadeDifficulty::Easy) t = CryptogramTier::Easy;
            startCryptogram(quote, cryptogramExtraReveals(t));
            break;
        }
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
    int rows[kArcadeMaxCabinets] = {0};
    const int n = arcadeOfferedRows(rows, kArcadeMaxCabinets);
    drawArcadeList(fb, registry_, plays, rows, n, arcadeRow_, beat_);
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
