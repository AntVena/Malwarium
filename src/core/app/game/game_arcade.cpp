#include "core/app/game.h"

#include <cstdio>

#include "tunables.h"
#include "core/content/content_arcade.h"
#include "core/content/content_quotes.h"
#include "core/ui/arcade_screen.h"
#include "core/ui/prose_page.h"
#include "core/ui/widgets.h"

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

namespace {
// Where the RULES page's prose starts, under drawHeaderBand's title + rule — the same
// offset ROCK THE DOCK's own BRIEFING opens its reader at (kTourneyReaderTop,
// game_tourney.cpp), since both sit under the identical band.
constexpr int kGameBriefReaderTop = 46;
}  // namespace

// --- Mid-play RULES overlay --------------------------------------------------

bool Game::gameBriefAvailable() const {
    switch (nav_) {
        case Nav::Stacker:
        case Nav::Isolation:
        case Nav::Decryption:
        case Nav::Cryptogram:
        case Nav::ModalEggPick:
        case Nav::Chroma:
            return true;
        default:
            return false;
    }
}

ArcadeGameKind Game::gameBriefKind() const {
    switch (nav_) {
        case Nav::Isolation:    return ArcadeGameKind::Isolation;
        case Nav::Decryption:   return ArcadeGameKind::Decryption;
        case Nav::Cryptogram:   return ArcadeGameKind::Cryptogram;
        case Nav::ModalEggPick: return ArcadeGameKind::Clutch;
        case Nav::Chroma:       return ArcadeGameKind::Chroma;
        default:                return ArcadeGameKind::Stacker;
    }
}

std::vector<ProseRow> Game::gameBriefRows() const {
    std::vector<ProseRow> out;
    const ArcadeGameDef* def = arcadeGameByKind(gameBriefKind());
    if (!def) return out;
    out.reserve(static_cast<size_t>(def->briefCount));
    for (int i = 0; i < def->briefCount; ++i) {
        ProseRow r;
        r.label = def->brief[i].heading;
        std::snprintf(r.body.buf, sizeof(r.body.buf), "%s", def->brief[i].text);
        out.push_back(r);
    }
    return out;
}

void Game::openGameBrief() {
    if (!gameBriefAvailable()) return;
    gameBriefOpen_ = true;
    gameBriefScroll_ = 0;
    dirty_ = true;
}

void Game::closeGameBrief() {
    gameBriefOpen_ = false;
    gameBriefScroll_ = 0;
    dirty_ = true;
}

void Game::toggleGameBrief() {
    if (gameBriefOpen_) closeGameBrief();
    else openGameBrief();
}

// Called first by every one of the five engines' input handlers. Consumes B (scroll)
// and C (close) while the overlay is up and reports so via the return, so the caller
// returns immediately instead of also feeding the same press to the paused game
// underneath; reports false the moment the overlay isn't open, so a normal press falls
// through untouched.
bool Game::onGameBriefInput(const ButtonEvent& ev) {
    if (!gameBriefOpen_) return false;
    const std::vector<ProseRow> rows = gameBriefRows();
    const int total = static_cast<int>(rows.size());
    if (ev.button == Button::B && total > 0) {
        const int shown = proseRowsFitting(rows, gameBriefScroll_, kGameBriefReaderTop);
        gameBriefScroll_ += shown;
        if (gameBriefScroll_ >= total) gameBriefScroll_ = 0;
        dirty_ = true;
    } else if (ev.button == Button::C || ev.chordAC) {
        closeGameBrief();
    }
    return true;
}

void Game::drawGameBrief(Framebuffer& fb) const {
    const ArcadeGameDef* def = arcadeGameByKind(gameBriefKind());
    drawHeaderBand(fb, def ? def->displayName : "RULES", "RULES");
    drawProseRows(fb, gameBriefRows(), gameBriefScroll_, kGameBriefReaderTop, beat_,
                  "B MORE   C BACK");
}

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

int Game::arcadeBestById(const char* id) const {
    const int row = arcadeGameIndexById(id);
    return row < 0 ? 0 : arcadeBest(row);
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
            // ENDLESS off a cabinet: no byte count ends the run, so it goes until the
            // worm crashes (or fills the buffer). kArcadeIsolationWinBytes is the win
            // line the till pays and scores against, not a finish the board stops at.
            startIsolation(/*goalDots=*/0);
            break;
        case ArcadeGameKind::Decryption:
            // One of the two cabinets whose dial moves the RULES: easy outlines the
            // cells a row got exactly right, hard lets the key repeat a colour.
            startDecryption(
                /*allowDuplicates=*/arcadeDifficulty_ == ArcadeDifficulty::Hard,
                /*easyHints=*/arcadeDifficulty_ == ArcadeDifficulty::Easy);
            break;
        case ArcadeGameKind::Chroma:
            // ENDLESS, like the buffer above: the board runs until the sweep finds it.
            // The dial is the WINDOW, which is the paced games' knob (arcadeStepMs) —
            // and hard alone turns switching on, which is the only rule any cabinet
            // adds to a game rather than re-times.
            startChroma(/*rounds=*/0, arcadeStepMs(kChromaWindowMs),
                        /*switching=*/arcadeDifficulty_ == ArcadeDifficulty::Hard);
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
        // The HIGH SCORE, which is the only one of the three tallies a run can leave
        // worse than it found: it is kept unclamped, deliberately. The bits a run pays
        // stop at the win line, but the number remembered here does not, because on an
        // endless cabinet that number IS the game.
        arcadeNewBest_ = score > arcadeBest_[arcadeGame_] && scoreMax > 0;
        if (arcadeNewBest_) arcadeBest_[arcadeGame_] = score;
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
                      arcadePlays(arcadeRow_), arcadeWins(arcadeRow_),
                      arcadeBest(arcadeRow_));
}

void Game::drawArcadeOutcome(Framebuffer& fb) const {
    if (arcadeGame_ < 0 || arcadeGame_ >= arcadeGameCount()) return;
    drawArcadeResult(fb, arcadeGames()[arcadeGame_], arcadeWon_, arcadeScore_,
                     arcadeScoreMax_, arcadeBest(arcadeGame_), arcadeNewBest_,
                     arcadeBits_, kArcadePlayHappy);
}

}  // namespace mal
