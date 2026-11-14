#include "core/content/content_backgrounds.h"   // the arena pays a place per bracket rung
#include "core/app/game.h"

#include <cstdio>
#include <cstring>

#include "tunables.h"
#include "core/content/content_crews.h"
#include "core/content/content_tournament.h"
#include "core/model/tournament.h"
#include "core/ui/prose_page.h"
#include "core/model/pvp_battle.h"
#include "core/app/game_internal.h"

// game_tourney.cpp (ROCK THE DOCK): eight operators, one bracket, in The Pirate Bayou.
//
// Everywhere else the ladder points the pet at a malbeast. Here it points it at
// PETWARE: a rolled creature off the same roster the player hatches from, at a rolled
// level up to the arena's cap, carrying a real move kit, real mods, the per-level stat
// points a raise would have bought it, and an Exploit it will fire on its own. So the
// arena is where a loadout is tested against the whole shape of the game rather than
// against one sector's difficulty rung, which is the only reason it exists.
//
// THE SEED IS THE RUN. Every entrant is derived from (seed, slot) rather than stored
// (core/model/tournament.h), so this unit persists four bytes plus a survivor bitmask
// and a verdict. Resuming after a reboot rebuilds the identical field.
//
// THE OTHER MATCHES ARE REALLY FOUGHT. When a round opens, the pairings the operator
// is not in are stepped headlessly through the same Combat the screen drives (same
// kits, same Exploits, same maths), and only the winner comes back. Nobody watches
// them, but the bracket that arrives at the final was climbed rather than sorted.
//
// STAKES: none, until the whole thing is taken. A bout is Safe (a loss costs no
// Fragmentation and spends no Bandwidth), and a won bout pays only the next bout. Only
// the TITLE pays, once, in one lump. That is what keeps the Dock a place to practise
// tactics instead of the fastest Bits in the game, and it is why a loss ends the run
// outright: the run is the stake.
//
// ONE SCREEN, THREE READS (TourneyView). The BRACKET is the field; hold-B opens the
// focused entrant's SCOUT sheet, and the chord opens the BRIEFING. Both readers flow
// through core/ui/prose_page.h, and the scout sheet is STAT's LOADOUT page pointed
// at another pet, so an opponent's kit reads exactly like the operator's own.
//
// This unit is the RUN. The screen over it is ArenaScreen (game/arena_screen.h).

namespace mal {

int Game::tourneySlot() const { return tourneyPlayerSlot(tourneySeed()); }

int Game::tourneyOpponentSlot() const { return tourney_ ? tourney_->opponentSlot() : -1; }

const TourneyFighter& Game::tourneyOpponent() const {
    static const TourneyFighter kNone{};
    return tourney_ ? tourney_->opponent : kNone;
}

void Game::openTourney() {
    // A run in play RESUMES exactly as it stood; otherwise draw a fresh bracket.
    // Only the seed is rolled; the eight entrants follow from it.
    if (!tourney_) {
        rng_ = rng_ * 1664525u + 1013904223u;
        tourney_ = TourneyRun::drawn(rng_);
        // Resolve the first round's other pairings now, so the field the operator reads
        // before their own opening match is already the true one.
        tourneyResolveRound(registry_, tourney_->seed, tourney_->round,
                            tourney_->playerSlot(), tourney_->alive);
        char entered[28];
        std::snprintf(entered, sizeof(entered), "ENTERED %s", kTourneyName);
        log_.push(LogEventType::CombatWon, entered);
        markSaveDirty();
    }
    arena_.open(tourneySlot());
    // The opponent is reseated on every entry rather than persisted: it is derivable,
    // and a resumed run must not be able to disagree with the bracket it came back to.
    tourney_->seatOpponent(registry_);
    nav_ = Nav::Tourney;
    dirty_ = true;
}

void Game::abandonTourney() {
    tourney_.reset();
    markSaveDirty();
}

void Game::startTourneyMatch() {
    if (!pet_.creature || !tourney_ || tourney_->opponentSlot() < 0) return;
    const TourneyRun& run = *tourney_;
    Combatant p = buildPlayerCombatant();
    // With no stakes there is no death to prevent, so a Rare item's shield is not
    // spent here. PRACTISE does the same.
    p.itemShield = false;
    Combatant e = makeTourneyCombatant(registry_, run.opponent);
    // Facing a real species counts as seeing it, as in a duel. The arena is the
    // other place an unraised creature appears.
    markCreatureSeen(run.opponent.spec.creatureId);
    // The arena is the other place the operator's own species can walk on opposite them,
    // so it fires the mirror on the same terms the duel does.
    noteMirrorMatch(run.opponent.spec.creatureId);
    // The operator's own Exploit allowance is the normal one. THIS is the difference
    // from a linked duel, which grants zero: there, opening the picker would pause a
    // fight the other device is still stepping. Here both fighters are local, so the
    // arena is the only full-kit pet-vs-pet fight with Exploits on both sides.
    fight_ = Fight::started(FightCaller::Tourney);
    fight_.combat.begin(p, e, Combat::Stakes::Safe,
                        tourneyMatchSeed(run.seed, run.round,
                                         tourneyBlockStart(run.playerSlot(), run.round)),
                        /*forceEnemyFirst=*/false, /*carryPlayerHealth=*/-1,
                        exploitUsesPerBattle());
    fxBeat_ = 0;
    fightScreen_.shutPanel();
    nav_ = Nav::Combat;
    dirty_ = true;
    playSound(Sound::CombatStart);
}

void Game::finishTourneyMatch() {
    // An arena bout is only ever opened by startTourneyMatch, which needs a run.
    TourneyRun& run = *tourney_;
    const int me = run.playerSlot();
    const int opp = mal::tourneyOpponentSlot(run.alive, me, run.round);
    const bool won = fight_.combat.outcome() == Combat::Outcome::Win;
    char line[28];
    if (!won) {
        // A loss, a walk-out (there is no fleeing a bracket, only forfeiting), or a
        // draw all count as a loss. The run ends and the banner waits.
        run.phase = TourneyPhase::Eliminated;
        if (opp >= 0) run.alive &= static_cast<uint8_t>(~(1u << me));
        std::snprintf(line, sizeof(line), "OUT TO %s", tourneyCard(run.seed, opp).handle);
        log_.push(LogEventType::CombatLost, line);
    } else {
        if (opp >= 0) run.alive &= static_cast<uint8_t>(~(1u << opp));
        std::snprintf(line, sizeof(line), "BEAT %s", tourneyCard(run.seed, opp).handle);
        log_.push(LogEventType::CombatWon, line);
        ++run.round;
        if (run.round >= kTourneyRounds) {
            run.phase = TourneyPhase::Champion;
            awardTourneyPurse();
        } else {
            // Open the next round: the other survivors settle their own match before the
            // operator sees who they climbed to.
            tourneyResolveRound(registry_, run.seed, run.round, me, run.alive);
        }
    }
    run.seatOpponent(registry_);
    arena_.open(me);
    nav_ = Nav::Tourney;
    dirty_ = true;
    persistSave();
}

void Game::awardTourneyPurse() {
    // The one payout the arena makes, and it makes it once. Bits and XP run through the
    // same Scraping Cluster / Well-Fed bonuses every other combat reward does, so the
    // upgrades an operator bought still apply here; the mod is drawn from The Pirate
    // Bayou's own pool, because that is the water the arena is held in.
    op_.bits += applyCombatBitsBonus(kTourneyWinBits);
    addFightXp(applyCombatXpBonus(kTourneyWinXp));
    if (const char* id = rollAreaModId(kTourneyAreaIndex)) grantMod(id);
    ++op_.tallies.tourneyWins;
    // ...and the rung this win lands on pays a BACKGROUND, if one is authored for it.
    // Read off the table rather than spelled out here, so adding a third arena prize is
    // a row rather than an edit to the purse.
    for (const BackgroundDef& b : kBackgrounds)
        if (b.source == BackgroundSource::Bracket && b.rung == op_.tallies.tourneyWins)
            announceBackground(b.scene);
    // The two rows that describe what the bracket was taken WITH, which the tally above
    // cannot say. Both are read HERE, before the caller clears the run's opponent: the
    // final's opponent is still seated and its level is still the one the seed rolled,
    // so neither needs a byte of save or a flag carried through the run.
    //
    // UNDERDOG: the final was won against an entrant rolled ABOVE the operator's own
    // level. Strictly above: matching the band is the expected fight, and the arena
    // draws levels uniformly, so this is a genuine draw of the short straw rather than
    // a rung that always lands on the third win.
    if (tourneyOpponent().spec.level > pet_.level) unlockAchievement(ach::kDockUnderdog);
    // PUNCHING UP: the whole bracket taken by a pet that never reached Daemon. The
    // entrants' stage follows their rolled level (kTourneyScriptLevel /
    // kTourneyDaemonLevel), so the top of a bracket is Daemon country; arriving there
    // as a Process or a Script is the operator's kit doing the work, not their stage.
    if (pet_.creature && pet_.creature->stage != Stage::Daemon) unlockAchievement(ach::kDockPunchingUp);
    char took[28];
    std::snprintf(took, sizeof(took), "TOOK %s", kTourneyName);
    log_.push(LogEventType::CombatWon, took);
    markSaveDirty();
}


std::vector<ProseRow> Game::tourneyScoutRows() const { return arena_.scoutRows(*this); }
std::vector<ProseRow> Game::tourneyBriefRows() const { return ArenaScreen::briefRows(); }

void Game::leaveArena() {
    // Leaving does NOT forfeit: the run is persisted, and the EXPL row says IN PLAY
    // until it is finished. A bracket is a commitment of attention, not of one sitting.
    nav_ = Nav::Submenu;
    openExplList();
}

void Game::abandonArena() {
    abandonTourney();
    leaveArena();
}

void Game::endTourneyRun(TourneyPhase end) {
    if (!tourney_) return;
    tourney_->phase = end;
    if (end == TourneyPhase::Eliminated)
        tourney_->alive &= static_cast<uint8_t>(~(1u << tourneySlot()));
    // The purse reads the final's opponent for its two shape achievements, so it is paid
    // before the opponent is cleared below.
    else awardTourneyPurse();
    tourney_->opponent = TourneyFighter{};
    dirty_ = true;
}

}  // namespace mal
