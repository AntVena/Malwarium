#include "core/app/game.h"

#include <cstdio>
#include <cstring>

#include "tunables.h"
#include "core/content/content_crews.h"
#include "core/content/content_tournament.h"
#include "core/model/tournament.h"
#include "core/render/canvas.h"
#include "core/render/font.h"
#include "core/render/palette.h"
#include "core/ui/layout.h"
#include "core/ui/theme.h"
#include "core/ui/widgets.h"

// game_tourney.cpp — THE COMPO: eight operators, one bracket, in The Pirate Bayou.
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
// STAKES: none, until the whole thing is taken. A match is Safe (a loss costs no
// Fragmentation and spends no Bandwidth — the arena is not the wild), and a won match
// pays nothing but the next match. Only the TITLE pays, once, in one lump. That is
// what keeps the Compo a place to practise tactics instead of the fastest Bits in the
// game, and it is why a loss ends the run outright: the run is the stake.

namespace mal {

namespace {
// One row per slot, plus the header/context above and the match card + hint band
// below. Eight rows at this pitch is what makes the whole field readable at once —
// the bracket's shape is only legible if none of it is scrolled away.
constexpr int kSlotRowH = 15;
constexpr int kSlotTop = kContextRule + 6;
// The left gutter the round's bracket ties are drawn in, and the text beside it.
constexpr int kGutterX = kMargin;
constexpr int kGutterW = 6;
constexpr int kSlotTextX = kGutterX + kGutterW + 4;
// The status column's width, fixed at the widest word it ever holds — see the note at
// the draw site for why it is not per-row.
inline int slotTagWidth() { return textWidth("NEXT"); }
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
        log_.push(LogEventType::CombatWon, "ENTERED THE COMPO");
        markSaveDirty();
    }
    tourneyCursor_ = tourneySlot();
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
    combatTurnBeat_ = 0;
    combatHitBeat_ = 0;
    combatStatsOpen_ = false;
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
    nav_ = Nav::Tourney;
    dirty_ = true;
    persistSave();
}

void Game::awardTourneyPurse() {
    // The one payout the arena makes, and it makes it once. Bits and XP run through the
    // same Scraping Cluster / Well-Fed bonuses every other combat reward does, so the
    // upgrades an operator bought still apply here; the mod is drawn from The Pirate
    // Bayou's own pool, because that is the water the Compo is held in.
    bits_ += applyCombatBitsBonus(kTourneyWinBits);
    addCombatXp(applyCombatXpBonus(kTourneyWinXp));
    if (const char* id = rollAreaModId(kTourneyAreaIndex)) grantMod(id);
    log_.push(LogEventType::CombatWon, "TOOK THE COMPO");
    markSaveDirty();
}

// --- Input -------------------------------------------------------------------

void Game::onTourney(const ButtonEvent& ev) {
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
        // can see who is left and at what level. Nothing acts on the cursor, which is
        // why B below ignores it and always means "fight my own match".
        tourneyCursor_ = (tourneyCursor_ + 1) % kTourneySlots;
        dirty_ = true;
    } else if (ev.button == Button::B) {
        startTourneyMatch();
    } else if (ev.button == Button::C) {
        // Leaving does NOT forfeit: the run is persisted, and the EXPL row says IN PLAY
        // until it is finished. A bracket is a commitment of attention, not of one
        // sitting.
        nav_ = Nav::Submenu;
        openExplList();
        dirty_ = true;
    }
}

// --- Rendering ---------------------------------------------------------------

void Game::drawTourney(Framebuffer& fb) const {
    drawHeaderBand(fb, "THE COMPO");

    const int me = tourneySlot();
    const int opp = tourneyOpponentSlot();
    // Context line: which round, and how much of the field is left. Both are numbers
    // the operator reads rather than colours, so the header survives grayscale.
    char ctx[32];
    if (tourneyPhase_ == TourneyPhase::Champion)
        std::snprintf(ctx, sizeof(ctx), "CHAMPION");
    else if (tourneyPhase_ == TourneyPhase::Eliminated)
        std::snprintf(ctx, sizeof(ctx), "ELIMINATED");
    else
        std::snprintf(ctx, sizeof(ctx), "ROUND %d/%d", tourneyRound_ + 1, kTourneyRounds);
    drawText(fb, kMargin, kContextY, ctx, palColor(Pal::INK));
    char left[20];
    std::snprintf(left, sizeof(left), "%d LEFT", tourneyAliveCount(tourneyAlive_));
    drawText(fb, kActiveW - kMargin - textWidth(left), kContextY, left,
             palColor(Pal::INK_DIM));
    fb.fillRect(0, kContextRule, kActiveW, 1, palColor(Pal::TRACK));

    // The field. One row per ORIGINAL slot, in bracket order, so the shape of the tree
    // is the shape of the list: neighbours are the pairing, and the ties drawn in the
    // gutter say which pairings this round is actually settling.
    const int blockSize = tourneyRound_ < kTourneyRounds
                              ? tourneyBlockSize(tourneyRound_) : kTourneySlots;
    for (int slot = 0; slot < kTourneySlots; ++slot) {
        const int y = kSlotTop + slot * kSlotRowH;
        const bool alive = (tourneyAlive_ & (1u << slot)) != 0;
        const bool focused = slot == tourneyCursor_;
        if (focused) fb.fillRect(2, y - 2, kActiveW - 4, kSlotRowH - 1,
                                 palColor(Pal::TRACK));
        // The bracket tie: a bar down the gutter spanning this round's block, closed
        // with a stub at each end. Drawn only for a block that still holds two
        // survivors, so a tie always means "this pairing is live".
        const int start = tourneyBlockStart(slot, tourneyRound_ < kTourneyRounds
                                                      ? tourneyRound_ : 0);
        if (slot == start && blockSize > 1) {
            int liveInBlock = 0;
            for (int i = start; i < start + blockSize && i < kTourneySlots; ++i)
                if (tourneyAlive_ & (1u << i)) ++liveInBlock;
            if (liveInBlock >= 2) {
                const int h = blockSize * kSlotRowH - 6;
                fb.fillRect(kGutterX + kGutterW - 1, y, 1, h, palColor(Pal::INK_DIM));
                fb.fillRect(kGutterX + 2, y, kGutterW - 2, 1, palColor(Pal::INK_DIM));
                fb.fillRect(kGutterX + 2, y + h - 1, kGutterW - 2, 1,
                            palColor(Pal::INK_DIM));
            }
        }
        const TourneyCard card = tourneyCard(tourneySeed_, slot);
        // The operator's own row is captioned with their own tag — an entrant list that
        // named seven operators and one "YOU" would be the one row you cannot compare.
        const char* name = slot == me ? hackerTag_ : card.handle;
        const Rgb565 ink = !alive      ? palColor(Pal::INK_DIM)
                           : slot == me ? palColor(Pal::ACCENT)
                                        : palColor(Pal::INK);
        drawText(fb, kSlotTextX, y, name, ink);
        char lvl[12];
        std::snprintf(lvl, sizeof(lvl), "L%d",
                      slot == me ? combatLevel_ : card.level);
        // Status as a WORD, never a colour: OUT for a knocked-out entrant, NEXT on the
        // one being faced right now, YOU on the operator. A dimmed row alone would be
        // invisible in grayscale, which is the one thing this screen cannot afford.
        const char* tagText = !alive     ? "OUT"
                              : slot == opp ? "NEXT"
                              : slot == me  ? "YOU"
                                            : "";
        // Both right-hand columns are FIXED, sized to the widest tag rather than to
        // this row's own — eight levels that each sat wherever their own tag left room
        // is eight levels the eye cannot compare, and comparing them is what the
        // field list is for.
        const int tagX = kActiveW - kMargin - slotTagWidth();
        if (tagText[0])
            drawText(fb, tagX, y, tagText,
                     alive ? palColor(Pal::ACCENT) : palColor(Pal::INK_DIM));
        drawText(fb, tagX - 6 - textWidth(lvl), y, lvl, palColor(Pal::INK_DIM));
    }

    // The match card: who is next, what they ARE, and what they are carrying. This is
    // the tactical half of the screen — an opponent's line and Exploit is what a
    // loadout is chosen against, so it is stated before the fight rather than
    // discovered during it.
    const int cardY = kSlotTop + kTourneySlots * kSlotRowH + 4;
    fb.fillRect(0, cardY - 4, kActiveW, 1, palColor(Pal::TRACK));
    char l1[40] = "";
    char l2[40] = "";
    const char* rightOfL2 = "";
    if (tourneyPhase_ == TourneyPhase::Champion) {
        std::snprintf(l1, sizeof(l1), "THE FIELD IS YOURS");
        std::snprintf(l2, sizeof(l2), "+%d BITS +%d XP +1 MOD", kTourneyWinBits,
                      kTourneyWinXp);
    } else if (tourneyPhase_ == TourneyPhase::Eliminated) {
        std::snprintf(l1, sizeof(l1), "KNOCKED OUT OF THE DRAW");
        std::snprintf(l2, sizeof(l2), "THE BRACKET GOES ON");
    } else if (opp >= 0) {
        const CreatureDef* c = registry_.creature(tourneyOpponent_.spec.creatureId);
        std::snprintf(l1, sizeof(l1), "NEXT  %s  L%d",
                      tourneyCard(tourneySeed_, opp).handle,
                      static_cast<int>(tourneyOpponent_.spec.level));
        std::snprintf(l2, sizeof(l2), "%s", c ? c->displayName : "UNKNOWN");
        // The Exploit as its TAG, not its crew NAME — the short mechanic word every
        // other surface shows it by (crewExploitTag), which is also the only form that
        // reliably fits beside a species. Right-anchored through drawLabelValue, so a
        // long species yields to it instead of overprinting it.
        rightOfL2 = crewExploitTag(tourneyOpponent_.exploit.kind);
    }
    if (l1[0]) drawText(fb, kMargin, cardY, l1, palColor(Pal::INK));
    if (l2[0] && rightOfL2[0])
        drawLabelValue(fb, kMargin, cardY + kLineH, l2, palColor(Pal::INK_DIM),
                       rightOfL2, palColor(Pal::ACCENT), beat_, /*scroll=*/true);
    else if (l2[0])
        drawText(fb, kMargin, cardY + kLineH, l2, palColor(Pal::INK_DIM));

    drawHintBand(fb, tourneyPhase_ != TourneyPhase::Ready
                         ? "ANY KEY"
                         : "A READ   B FIGHT   C BACK");
}

}  // namespace mal
