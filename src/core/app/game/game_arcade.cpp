#include "core/app/game.h"

#include <cstdio>

#include "tunables.h"
#include "core/content/content_arcade.h"
#include "core/content/content_quotes.h"
#include "core/ui/prose_page.h"
#include "core/ui/widgets.h"

// game_arcade.cpp (GAMES): the arcade cabinets.
//
// The whole unit is a menu and a till. It starts a minigame that already exists, waits
// for that game to end the way it always ends, and pays for the attempt, so there is
// no arcade copy of any game to keep in step with the original, and changing a hatch
// game changes what the cabinet plays in the same commit.
//
// Two things a run needs that its own context normally supplies, and which the arcade
// therefore has to hand it instead: a GOAL (the Isolation run prices its clean off the
// incubation clock, which a cabinet doesn't have) and a PACE (the difficulty dial). Both
// are parameters at the start call; nothing inside a game knows the arcade exists.
//
// The payout is mostly flat; kArcadePlayBits in tunables.h says why.

namespace mal {

namespace {
// Where the RULES page's prose starts, under drawHeaderBand's title + rule: the same
// offset ROCK THE DOCK's own BRIEFING opens its reader at (kDockBriefTop,
// ui/tourney_screen.h), since both sit under the identical band.
constexpr int kGameBriefReaderTop = 46;
}  // namespace

// --- Mid-play RULES overlay --------------------------------------------------

bool Game::gameBriefAvailable() const {
    switch (nav_) {
        case Nav::Stacker:
        case Nav::Fragger:
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
        case Nav::Fragger:      return ArcadeGameKind::Fragger;
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
    out.reserve(static_cast<size_t>(def->briefCount) + 1);
    // The engine's own rows say nothing about what a run pays (content_arcade.cpp), so
    // a hatch adds the one thing a first-time player most needs before the mechanics:
    // what the egg stakes on this board, which is less than the board makes it look.
    if (hatchGameLive()) out.push_back(hatchBriefLeadRow());
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
    gameBriefIntro_ = false;
    gameBriefScroll_ = 0;
    dirty_ = true;
}

void Game::toggleGameBrief() {
    if (gameBriefOpen_) closeGameBrief();
    else openGameBrief();
}

// Called first by every game engine's input handler. Consumes B (scroll)
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
        // The chord's reader wraps, since it is something to re-read; the hatch's
        // briefing is something to get through, so B off its last page starts play.
        if (gameBriefScroll_ >= total) {
            if (gameBriefIntro_) closeGameBrief();
            else gameBriefScroll_ = 0;
        }
        dirty_ = true;
    } else if (ev.button == Button::C || ev.chordAC) {
        closeGameBrief();
    }
    return true;
}

void Game::drawGameBrief(Framebuffer& fb) const {
    const ArcadeGameDef* def = arcadeGameByKind(gameBriefKind());
    drawHeaderBand(fb, def ? def->displayName : "RULES", "RULES");
    const std::vector<ProseRow> rows = gameBriefRows();
    const char* hint = "B MORE   C BACK";
    if (gameBriefIntro_) {
        const bool last = gameBriefScroll_ +
            proseRowsFitting(rows, gameBriefScroll_, kGameBriefReaderTop) >=
            static_cast<int>(rows.size());
        hint = last ? "B PLAY" : "B NEXT   C SKIP";
    }
    drawProseRows(fb, {rows, gameBriefScroll_, kGameBriefReaderTop, beat_, hint});
}

// --- The cabinet list (L2) -------------------------------------------------
//
// A locked cabinet is ABSENT from the list, not greyed in it. The Rig Shop's rule
// (RigAccount::offered), and for the same reason: a menu should not advertise a door the
// player has no key to. The cursor stays a ROSTER index throughout (ArcadeScreen::row).

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

// --- One cabinet (L3) ------------------------------------------------------

int Game::arcadeBestById(const char* id) const {
    const int row = arcadeGameIndexById(id);
    return row < 0 ? 0 : arcadeBest(row);
}

int Game::playStepMs(int baseMs) const {
    if (!arcade_ && !(maint_ && maint_->boardLive())) return baseMs;
    int pct = kArcadeSpeedPctMedium;
    if (runDifficulty() == ArcadeDifficulty::Easy) pct = kArcadeSpeedPctEasy;
    else if (runDifficulty() == ArcadeDifficulty::Hard) pct = kArcadeSpeedPctHard;
    const int ms = baseMs * pct / 100;
    return ms < 1 ? 1 : ms;
}

void Game::openDetail() { nav_ = Nav::Detail; }
void Game::closeDetail() { nav_ = Nav::Submenu; }

void Game::startArcadeRun(int cabinet, ArcadeDifficulty dial) {
    if (!arcadeCabinetOffered(cabinet)) return;   // range + the unlock, in one ask
    arcade_ = ArcadeRun::started(cabinet, dial);
    const ArcadeGameKind kind = arcadeGames()[arcade_->cabinet].kind;
    switch (kind) {
        case ArcadeGameKind::Stacker:
        case ArcadeGameKind::Fragger:
            // The two engines a MAINT process can also play, so they start the way MAINT
            // starts them. FRAGGER is ENDLESS, like the buffer below: kFraggerWinHits is
            // the line the till pays and scores against.
            beginPlayedGame(kind);
            break;
        case ArcadeGameKind::Clutch: {
            int rounds = kArcadeClutchRoundsMedium;
            if (dial == ArcadeDifficulty::Easy) rounds = kArcadeClutchRoundsEasy;
            else if (dial == ArcadeDifficulty::Hard) rounds = kArcadeClutchRoundsHard;
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
                /*allowDuplicates=*/dial == ArcadeDifficulty::Hard,
                /*easyHints=*/dial == ArcadeDifficulty::Easy);
            break;
        case ArcadeGameKind::Chroma:
            // ENDLESS, like the buffer above: the board runs until the sweep finds it.
            // The dial is the WINDOW, which is the paced games' knob (playStepMs), and
            // HARD also turns switching on, the only rule a cabinet adds.
            startChroma(/*rounds=*/0, playStepMs(kChromaWindowMs),
                        /*switching=*/dial == ArcadeDifficulty::Hard);
            break;
        case ArcadeGameKind::Cryptogram: {
            // The dial is the quote's difficulty ladder, so the cabinet opens the board
            // as the VAULT would and MEDIUM means one thing. Only a SOLVED quote is
            // offered: the cabinet is a re-run, never a free route through the pool.
            const int quote = pickQuote(QuotePick::SolvedOnly);
            if (quote < 0) {
                arcade_.reset();      // nothing to re-run: leave the cabinet page up
                return;
            }
            CryptogramTier t = CryptogramTier::Hard;
            if (dial == ArcadeDifficulty::Medium) t = CryptogramTier::Medium;
            else if (dial == ArcadeDifficulty::Easy) t = CryptogramTier::Easy;
            startCryptogram(quote, cryptogramExtraReveals(t));
            break;
        }
    }
    dirty_ = true;
    playSound(Sound::ArcadeStart);
}

// --- The till --------------------------------------------------------------

void Game::finishArcadeRun(bool won, int score, int scoreMax) {
    // A finish only ever comes from a board a cabinet started.
    const ArcadeRun run = *arcade_;
    arcade_.reset();
    const ArcadeResult result = run.settled(won, score, scoreMax, arcadeBest(run.cabinet));
    arcadeScreen_.showResult(result);
    op_.bits += result.bits;
    pet_.model.setHappiness(pet_.model.happiness() + kArcadePlayHappy);   // clamps at the cap

    if (run.cabinet >= 0 && run.cabinet < kArcadeMaxCabinets) {
        ++op_.tallies.arcadePlays[run.cabinet];
        if (won) ++op_.tallies.arcadeWins[run.cabinet];
        if (result.newBest) op_.tallies.arcadeBest[run.cabinet] = score;
    }
    addBond(pet_.upgrades, BondSource::Games, kBondGameBp);
    markSaveDirty();

    cursor_ = carouselSlotOf(SubmenuId::Games);
    nav_ = Nav::ArcadeResult;
    dirty_ = true;
    playSound(result.newBest ? Sound::NewBest : won ? Sound::GameWin : Sound::GameLose);
}

}  // namespace mal
