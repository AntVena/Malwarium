#include "core/app/game.h"

#include <cstdio>
#include <cstring>

#include "tunables.h"
#include "core/content/content_crews.h"
#include "core/content/content_tournament.h"
#include "core/model/tournament.h"
#include "core/content/effect_text.h"
#include "core/model/move_loadout.h"
#include "core/model/loadout.h"
#include "core/ui/stat_screen.h"
#include "core/ui/prose_page.h"
#include "core/ui/tourney_screen.h"
#include "core/model/pvp_battle.h"
#include "core/app/game_internal.h"

// game_tourney.cpp — ROCK THE DOCK: eight operators, one bracket, in The Pirate Bayou.
//
// Everywhere else the ladder points the pet at a malbeast. Here it points it at
// PETWARE — a rolled creature off the same roster the player hatches from, at a rolled
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
// is not in are stepped headlessly through the same Combat the screen drives — same
// kits, same Exploits, same maths — and only the winner comes back. Nobody watches
// them, but the bracket that arrives at the final was climbed rather than sorted.
//
// STAKES: none, until the whole thing is taken. A bout is Safe (a loss costs no
// Fragmentation and spends no Bandwidth — the arena is not the wild), and a won bout
// pays nothing but the next bout. Only the TITLE pays, once, in one lump. That is what
// keeps the Dock a place to practise tactics instead of the fastest Bits in the game,
// and it is why a loss ends the run outright: the run is the stake.
//
// ONE SCREEN, THREE READS (TourneyView). The BRACKET is the field; hold-B opens the
// focused entrant's SCOUT sheet, and the chord opens the BRIEFING. Both readers flow
// through core/ui/prose_page.h, and the scout sheet is literally STAT's LOADOUT page
// pointed at somebody else's pet — an opponent whose kit was described by a second
// renderer would be an opponent the operator has to learn to read twice.
//
// This unit is the RUN. What the three reads look like is game_tourney_render.cpp,
// laid out against core/ui/tourney_screen.h; the only thing the two share is this
// file's state and that header's geometry.

namespace mal {

namespace {
// The two READER views (SCOUT, BRIEF) page against ui/tourney_screen.h's tops — the
// scout sheet's is lower, because it seats a portrait. Everything else about how this
// screen is laid out lives there too; this unit is the run.
inline int tourneyReaderTop(Game::TourneyView v) {
    return v == Game::TourneyView::Scout ? kDockScoutTop : kDockBriefTop;
}
}  // namespace

int Game::tourneySlot() const { return tourneyPlayerSlot(tourneySeed_); }

int Game::tourneyOpponentSlot() const {
    if (!tourneyRunning() || tourneyPhase_ != TourneyPhase::Ready) return -1;
    if (tourneyRound_ >= kTourneyRounds) return -1;
    return mal::tourneyOpponentSlot(tourneyAlive_, tourneySlot(), tourneyRound_);
}

void Game::openTourney() {
    // A run in play RESUMES exactly as it stood; otherwise draw a fresh bracket. The
    // seed is the only thing rolled — everything else about the eight entrants falls
    // out of it.
    if (!tourneyRunning()) {
        rng_ = rng_ * 1664525u + 1013904223u;
        tourneySeed_ = rng_ ? rng_ : 1u;
        tourneyAlive_ = static_cast<uint8_t>((1u << kTourneySlots) - 1);
        tourneyRound_ = 0;
        tourneyPhase_ = TourneyPhase::Ready;
        // Resolve the first round's other pairings now, so the field the operator reads
        // before their own opening match is already the true one.
        tourneyResolveRound(registry_, tourneySeed_, tourneyRound_, tourneySlot(),
                            tourneyAlive_);
        char entered[28];
        std::snprintf(entered, sizeof(entered), "ENTERED %s", kTourneyName);
        log_.push(LogEventType::CombatWon, entered);
        markSaveDirty();
    }
    tourneyCursor_ = tourneySlot();
    tourneyView_ = TourneyView::Bracket;
    tourneyScroll_ = 0;
    tourneyScoutedSlot_ = -1;
    // The opponent is rebuilt on every entry rather than persisted: it is derivable,
    // and a resumed run must not be able to disagree with the bracket it came back to.
    if (const int opp = tourneyOpponentSlot(); opp >= 0)
        tourneyOpponent_ = tourneyEntrant(registry_, tourneySeed_, opp);
    else
        tourneyOpponent_ = TourneyFighter{};
    nav_ = Nav::Tourney;
    dirty_ = true;
}

void Game::abandonTourney() {
    tourneySeed_ = 0;
    tourneyAlive_ = 0;
    tourneyRound_ = 0;
    tourneyPhase_ = TourneyPhase::Ready;
    tourneyOpponent_ = TourneyFighter{};
    tourneyView_ = TourneyView::Bracket;
    tourneyScoutedSlot_ = -1;
    markSaveDirty();
}

void Game::startTourneyMatch() {
    if (!pet_ || tourneyPhase_ != TourneyPhase::Ready) return;
    const int opp = tourneyOpponentSlot();
    if (opp < 0) return;
    Combatant p = buildPlayerCombatant();
    // No stakes means no death to save the pet from, so a Rare item's shield is not
    // spent on a drill — the same call PRACTISE makes, for the same reason.
    p.itemShield = false;
    Combatant e = makeTourneyCombatant(registry_, tourneyOpponent_);
    // Facing a real species is the glimpse, exactly as it is in a duel — the arena is
    // the other place a creature the operator has never raised walks on screen.
    markCreatureSeen(tourneyOpponent_.spec.creatureId);
    // The arena is the other place the operator's own species can walk on opposite them,
    // so it fires the mirror on the same terms the duel does.
    noteMirrorMatch(tourneyOpponent_.spec.creatureId);
    // The operator's own Exploit allowance is the normal one. THIS is the difference
    // from a linked duel, which grants zero: there, opening the picker would pause a
    // fight the other device is still stepping. Here both fighters are local, so the
    // arena is the one place a full-kit pet-vs-pet fight can be played with Exploits on
    // both sides — which is the whole tactical point of it.
    combat_.begin(p, e, Combat::Stakes::Safe,
                  tourneyMatchSeed(tourneySeed_, tourneyRound_,
                                   tourneyBlockStart(tourneySlot(), tourneyRound_)),
                  /*forceEnemyFirst=*/false, /*carryPlayerHealth=*/-1,
                  exploitUsesPerBattle());
    combatCaller_ = CombatCaller::Tourney;
    combatBeat_ = 0;
    fxBeat_ = 0;
    combatTurnBeat_ = 0;
    combatHitBeat_ = 0;
    combatStatsPage_ = 0;
    nav_ = Nav::Combat;
    dirty_ = true;
}

void Game::finishTourneyMatch() {
    const int opp = mal::tourneyOpponentSlot(tourneyAlive_, tourneySlot(), tourneyRound_);
    const bool won = combat_.outcome() == Combat::Outcome::Win;
    char line[28];
    if (!won) {
        // A loss — or walking out of the fight, which is the same thing here: there is
        // no fleeing a bracket, only forfeiting it. The run ends and the banner waits.
        tourneyPhase_ = TourneyPhase::Eliminated;
        if (opp >= 0) tourneyAlive_ &= static_cast<uint8_t>(~(1u << tourneySlot()));
        std::snprintf(line, sizeof(line), "OUT TO %s",
                      tourneyCard(tourneySeed_, opp).handle);
        log_.push(LogEventType::CombatLost, line);
    } else {
        if (opp >= 0) tourneyAlive_ &= static_cast<uint8_t>(~(1u << opp));
        std::snprintf(line, sizeof(line), "BEAT %s",
                      tourneyCard(tourneySeed_, opp).handle);
        log_.push(LogEventType::CombatWon, line);
        ++tourneyRound_;
        if (tourneyRound_ >= kTourneyRounds) {
            tourneyPhase_ = TourneyPhase::Champion;
            awardTourneyPurse();
        } else {
            // Open the next round: the other survivors settle their own match before the
            // operator sees who they climbed to.
            tourneyResolveRound(registry_, tourneySeed_, tourneyRound_, tourneySlot(),
                                tourneyAlive_);
        }
    }
    if (const int next = tourneyOpponentSlot(); next >= 0)
        tourneyOpponent_ = tourneyEntrant(registry_, tourneySeed_, next);
    else
        tourneyOpponent_ = TourneyFighter{};
    tourneyCursor_ = tourneySlot();
    tourneyView_ = TourneyView::Bracket;
    tourneyScoutedSlot_ = -1;
    nav_ = Nav::Tourney;
    dirty_ = true;
    persistSave();
}

void Game::awardTourneyPurse() {
    // The one payout the arena makes, and it makes it once. Bits and XP run through the
    // same Scraping Cluster / Well-Fed bonuses every other combat reward does, so the
    // upgrades an operator bought still apply here; the mod is drawn from The Pirate
    // Bayou's own pool, because that is the water the arena is held in.
    bits_ += applyCombatBitsBonus(kTourneyWinBits);
    addCombatXp(applyCombatXpBonus(kTourneyWinXp));
    if (const char* id = rollAreaModId(kTourneyAreaIndex)) grantMod(id);
    ++tourneyWins_;
    // The two rows that describe what the bracket was taken WITH, which the tally above
    // cannot say. Both are read HERE, before the caller clears tourneyOpponent_: the
    // final's opponent is still seated and its level is still the one the seed rolled,
    // so neither needs a byte of save or a flag carried through the run.
    //
    // UNDERDOG — the final was won against an entrant rolled ABOVE the operator's own
    // level. Strictly above: matching the band is the expected fight, and the arena
    // draws levels uniformly, so this is a genuine draw of the short straw rather than
    // a rung that always lands on the third win.
    if (tourneyOpponent_.spec.level > combatLevel_) unlockAchievement(ach::kDockUnderdog);
    // PUNCHING UP — the whole bracket taken by a pet that never reached Daemon. The
    // entrants' stage follows their rolled level (kTourneyScriptLevel /
    // kTourneyDaemonLevel), so the top of a bracket is Daemon country; arriving there
    // as a Process or a Script is the operator's kit doing the work, not their stage.
    if (pet_ && pet_->stage != Stage::Daemon) unlockAchievement(ach::kDockPunchingUp);
    char took[28];
    std::snprintf(took, sizeof(took), "TOOK %s", kTourneyName);
    log_.push(LogEventType::CombatWon, took);
    markSaveDirty();
}

// --- The two readers' row models ---------------------------------------------

std::vector<ProseRow> Game::tourneyScoutRows() const {
    // An entrant's kit, built through the SAME buildLoadoutRows STAT's LOADOUT page
    // uses — the opponent's moves and mods are described by their own content rows,
    // wrapped whole, exactly as the operator's are. Nothing here re-words a move.
    std::vector<ProseRow> out;
    const CreatureDef* pet = registry_.creature(tourneyScouted_.spec.creatureId);
    if (!pet) return out;
    MoveLoadout moves;
    Loadout mods;
    buildPvpLoadouts(registry_, tourneyScouted_.spec, moves, mods);
    out = buildLoadoutRows(registry_, moves, mods, pet->stage, /*isEgg=*/false);

    // ...then the two things a loadout page has no column for, as a section of their
    // own: how the levels were SPENT (the stat spread is half of what an opponent is,
    // and it is invisible in a move list), and the Exploit it is carrying. The trigger
    // is deliberately NOT stated — which moment a rival commits to is the read the
    // arena exists to teach, and printing it would answer the question for them.
    ProseRow stats;
    stats.header = true;
    stats.label = "STATS";
    out.push_back(stats);
    for (int i = 0; i < kLevelStatCount; ++i) {
        ProseRow r;
        r.label = levelStatName(i);
        setProseTag(r, "+%d", tourneyScouted_.spec.statPoints[i]);
        out.push_back(r);
    }
    if (tourneyScouted_.exploit.kind != CrewExploitKind::None) {
        ProseRow head;
        head.header = true;
        head.label = "EXPLOIT";
        out.push_back(head);
        ProseRow r;
        r.label = tourneyScouted_.exploit.label;
        setProseTag(r, "%s", crewExploitTag(tourneyScouted_.exploit.kind));
        // The crew's own description of the ability, templated off its magnitude — the
        // same sentence the CREW screen shows, so an operator who has read one has read
        // the other.
        for (int c = 0; c < kCrewCount; ++c)
            if (crew(c)->exploit.kind == tourneyScouted_.exploit.kind)
                r.body = effectText(crew(c)->exploit);
        out.push_back(r);
    }
    return out;
}

std::vector<ProseRow> Game::tourneyBriefRows() const {
    std::vector<ProseRow> out;
    out.reserve(static_cast<size_t>(kTourneyBriefCount));
    for (int i = 0; i < kTourneyBriefCount; ++i) {
        ProseRow r;
        r.label = kTourneyBrief[i].heading;
        std::snprintf(r.body.buf, sizeof(r.body.buf), "%s", kTourneyBrief[i].text);
        out.push_back(r);
    }
    return out;
}

void Game::openTourneyScout() {
    if (tourneyView_ != TourneyView::Bracket || !tourneyRunning()) return;
    // Building an entrant walks the whole move and mod tables, so it happens HERE, on
    // the press, and the sheet reads the held copy for as long as it is open.
    tourneyScouted_ = tourneyEntrant(registry_, tourneySeed_, tourneyCursor_);
    tourneyScoutedSlot_ = tourneyCursor_;
    tourneyView_ = TourneyView::Scout;
    tourneyScroll_ = 0;
    dirty_ = true;
}

// --- Input -------------------------------------------------------------------

void Game::tourneyReleaseB() {
    // Screen check only — onButton owns bHeld_ (see moveFilterReleaseB). A B that was
    // armed on the bracket and released before kTourneyScoutHoldMs is the TAP: start
    // the operator's own bout.
    if (nav_ != Nav::Tourney || tourneyView_ != TourneyView::Bracket) return;
    startTourneyMatch();
    dirty_ = true;
    lastInputMs_ = nowMs_;
}

void Game::onTourney(const ButtonEvent& ev) {
    // The two READERS first: both are plain scroll-and-back pages, and neither can
    // start a bout, so nothing below them has to know they exist.
    if (tourneyView_ != TourneyView::Bracket) {
        const std::vector<ProseRow> rows = tourneyView_ == TourneyView::Scout
                                               ? tourneyScoutRows()
                                               : tourneyBriefRows();
        const int total = static_cast<int>(rows.size());
        // B SCROLLS by exactly the window that was on screen and wraps at the end —
        // the STAT reader's contract, and the reason proseRowsFitting is exported.
        if (ev.button == Button::B && total > 0) {
            const int shown = proseRowsFitting(rows, tourneyScroll_, tourneyReaderTop(tourneyView_));
            tourneyScroll_ += shown;
            if (tourneyScroll_ >= total) tourneyScroll_ = 0;
        } else if (ev.button == Button::C || ev.chordAC) {
            tourneyView_ = TourneyView::Bracket;
            tourneyScroll_ = 0;
        }
        dirty_ = true;
        return;
    }
    // The BRIEFING is the chord's job here — see game_core.cpp's chord routing for why
    // it is the chord and not a fourth key.
    if (ev.chordAC) {
        tourneyView_ = TourneyView::Brief;
        tourneyScroll_ = 0;
        dirty_ = true;
        return;
    }
    // A terminal banner is a read, not a menu: any of the three keys clears the run and
    // returns to EXPL. Answering all three is deliberate — nothing here is destructive
    // and a verdict the operator has already read should not need the right button.
    if (tourneyPhase_ != TourneyPhase::Ready) {
        abandonTourney();
        nav_ = Nav::Submenu;
        openExplList();
        dirty_ = true;
        return;
    }
    if (ev.button == Button::A) {
        // A READS the field — it steps the cursor over the eight slots so the operator
        // can see who is left and at what level, and it chooses whose SCOUT sheet a
        // held B opens. It never picks who you fight: the bracket does that.
        tourneyCursor_ = (tourneyCursor_ + 1) % kTourneySlots;
        dirty_ = true;
    } else if (ev.button == Button::B) {
        // Tap/hold, resolved on the RELEASE edge: a tap starts the bout
        // (tourneyReleaseB), a hold opens the focused entrant's sheet (tick()).
        bHeld_ = true;
        bDownMs_ = nowMs_;
    } else if (ev.button == Button::C) {
        // Leaving does NOT forfeit: the run is persisted, and the EXPL row says IN PLAY
        // until it is finished. A bracket is a commitment of attention, not of one
        // sitting.
        nav_ = Nav::Submenu;
        openExplList();
        dirty_ = true;
    }
}

}  // namespace mal
