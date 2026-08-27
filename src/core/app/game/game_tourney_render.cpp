#include "core/app/game.h"

#include <cstdio>

#include "tunables.h"
#include "core/content/content_crews.h"
#include "core/content/content_tournament.h"
#include "core/model/tournament.h"
#include "core/render/canvas.h"
#include "core/render/font.h"
#include "core/render/palette.h"
#include "core/render/sprite.h"
#include "core/ui/layout.h"
#include "core/ui/prose_page.h"
#include "core/ui/tourney_screen.h"
#include "core/ui/widgets.h"

// game_tourney_render.cpp — ROCK THE DOCK's three views. The run itself (the seed, the
// bracket's state machine, what a bout costs and pays) is game_tourney.cpp; this unit
// only ever reads it.
//
// The layout is ui/tourney_screen.h's, including its one structural idea: the field
// COLLAPSES as entrants go out, and the room that frees up is what stands two real
// fighters on the dock at the foot. So the same screen is a list of eight names in the
// first round and a face-off by the semi-final, without ever being two screens.
//
// WHY THE OPPONENT HAS A FACE AT ALL. The Dock's whole premise is that its entrants
// are PETWARE — a rolled creature off the hatchable roster rather than a malbeast
// (content_tournament.h) — and that premise is unprovable on a screen that only ever
// spells the species out. The sprite is already in flash for the bout that follows, so
// showing it costs nothing and is the only thing that says "this is somebody's pet"
// before the fight starts.

namespace mal {

namespace {

// The status column, fixed at the widest word it ever holds rather than sized per
// row: eight levels that each sat wherever their own tag left room is eight levels
// the eye cannot compare, and comparing them is what the field list is for.
inline int slotTagWidth() { return textWidth("NEXT"); }

// When an entrant fires its Exploit, as the tell the briefing promises the operator
// they can learn ("Some open with it; some wait until they are cornered"). A percentage
// rather than a mood, because it is a number the operator can check against the health
// bar during the bout — and the arena is the one place a tell is worth stating up
// front, since nothing here is a surprise the run can't survive.
void exploitTell(int atHealthPct, char* buf, int n) {
    if (atHealthPct >= 100) std::snprintf(buf, n, "FROM TURN 1");
    else std::snprintf(buf, n, "AT %d%% HEALTH", atHealthPct);
}

}  // namespace

void Game::drawTourney(Framebuffer& fb) const {
    switch (tourneyView_) {
        case TourneyView::Scout: drawTourneyScout(fb); return;
        case TourneyView::Brief: drawTourneyBrief(fb); return;
        case TourneyView::Bracket: break;
    }
    drawTourneyBracket(fb);
}

void Game::drawTourneyBracket(Framebuffer& fb) const {
    // The harbour is this screen's BACKGROUND pass (core/render/RENDER_PIPELINE.md),
    // so it goes down first and the band composes onto it instead of clearing it away.
    drawDockScene(fb, beat_);
    // Which round it is rides the band's right label. It is a NUMBER the operator
    // reads rather than a colour, so the header survives grayscale — and the two
    // verdicts take the same slot, because "CHAMPION" is also just where you are.
    char where[16];
    if (tourneyPhase_ == TourneyPhase::Champion)
        std::snprintf(where, sizeof(where), "CHAMPION");
    else if (tourneyPhase_ == TourneyPhase::Eliminated)
        std::snprintf(where, sizeof(where), "ELIMINATED");
    else
        std::snprintf(where, sizeof(where), "ROUND %d/%d", tourneyRound_ + 1,
                      kTourneyRounds);
    drawHeaderBandOver(fb, kTourneyName, where);

    const int me = tourneySlot();
    const int opp = tourneyOpponentSlot();
    const uint8_t alive = tourneyAlive_;
    const int cur = tourneyCursor_;

    // The tree goes down before the rows, so a row's own highlight can never break a
    // bracket line the row is hanging off.
    const int round = tourneyRound_ < kTourneyRounds ? tourneyRound_ : kTourneyRounds - 1;
    drawDockTies(fb, alive, cur, round, me);

    for (int slot = 0; slot < kTourneySlots; ++slot) {
        const int y = dockRowY(alive, slot, cur);
        const bool live = (alive & (1u << slot)) != 0;
        const bool focused = slot == cur;
        // Out of the draw and not being read: the row collapses to the strike alone.
        // The strikes are countable, and counting them gives the same answer as the
        // "n LEFT" readout — which is what keeps the collapse honest in grayscale.
        if (!live && !focused) {
            fb.fillRect(kDockTextX, y + kDockOutRowH / 2,
                        textWidth(slot == me ? hackerTag_
                                             : tourneyCard(tourneySeed_, slot).handle),
                        1, palColor(Pal::INK_DIM));
            continue;
        }
        if (focused) {
            fb.fillRect(kDockTextX - 3, y - 2, kActiveW - kDockTextX - 1,
                        kDockLiveRowH - 1, palColor(Pal::TRACK));
            drawRowCursor(fb, 2, y, palColor(Pal::ACCENT));
        }
        const TourneyCard card = tourneyCard(tourneySeed_, slot);
        // The operator's own row is captioned with their own tag — an entrant list that
        // named seven operators and one "YOU" would be the one row you cannot compare.
        const char* name = slot == me ? hackerTag_ : card.handle;
        const Rgb565 ink = live ? palColor(Pal::INK) : palColor(Pal::INK_DIM);
        drawText(fb, kDockTextX, y, name, ink);
        if (!live)   // the same strike the collapsed rows carry, through the name
            fb.fillRect(kDockTextX, y + kFontH / 2, textWidth(name), 1,
                        palColor(Pal::INK_DIM));
        char lvl[12];
        std::snprintf(lvl, sizeof(lvl), "L%d", slot == me ? combatLevel_ : card.level);
        // Status as a WORD, never a colour: OUT for a knocked-out entrant, NEXT on the
        // one being faced right now, YOU on the operator. The focus colour is spent on
        // focus and nothing else (PAL_CORE's `accent`), so a tag says what it is.
        const char* tagText = !live       ? "OUT"
                              : slot == opp ? "NEXT"
                              : slot == me  ? "YOU"
                                            : "";
        const int tagX = kActiveW - kMargin - slotTagWidth();
        if (tagText[0]) drawText(fb, tagX, y, tagText, ink);
        drawText(fb, tagX - 6 - textWidth(lvl), y, lvl, palColor(Pal::INK_DIM));
    }

    drawTourneyFoot(fb);

    if (tourneyPhase_ == TourneyPhase::Ready) {
        drawText(fb, kMargin, kDockGestureY, "HOLD B SCOUT  A+C BRIEF",
                 palColor(Pal::INK_DIM));
        drawHintBand(fb, "A NEXT   B BOUT   C BACK");
    } else {
        drawHintBand(fb, "ANY KEY");
    }
}

void Game::drawTourneyFoot(Framebuffer& fb) const {
    const int opp = tourneyOpponentSlot();
    const bool ready = tourneyPhase_ == TourneyPhase::Ready;
    const bool room = dockFaceoffFits(tourneyAlive_);

    // WHOSE drawing stands on the dock. Waiting on a bout it is the OPPONENT, not the
    // pair: the operator's own pet is on every other screen the device has, and the
    // one thing the arena has to show is the stranger — which is also the only way a
    // 96-wide Daemon and a column of copy fit on one 224px screen. Taking the bracket
    // is the exception, and it is the pet's: a title is a picture of your own pet
    // alone on a dock everybody else has gone home from.
    const bool mine = tourneyPhase_ == TourneyPhase::Champion;
    const CreatureDef* c =
        ready && opp >= 0 ? registry_.creature(tourneyOpponent_.spec.creatureId) : nullptr;
    const SpriteData* s = mine ? (pet_ ? registry_.creatureSprite(*pet_) : nullptr)
                        : c    ? registry_.sprite(c->spriteName)
                               : nullptr;
    if (!room) s = nullptr;

    bool mirror = false;
    if (s) {
        // The opponent faces the copy that describes it; a champion faces out.
        mirror = spriteMirrorToFace(*s, /*faceRight=*/mine);
        drawSpriteUpscaled(fb, *s, idleFrame(*s, beat_), dockSeatX(*s, mirror),
                           dockSeatY(*s), 1, 1, /*row=*/0, mirror);
    }
    const int w = dockCardW(s, mirror);
    const int top = s ? kDockCardTop : kDockTextCardTop;

    // Four lines, in the order the question is asked: who is it, what is it, what is
    // it carrying, and when will it use that. The last one is the tell the briefing
    // promises the operator they can learn, stated up front because the arena is the
    // one place where knowing it early costs the run nothing.
    char line[4][32] = {{0}, {0}, {0}, {0}};
    if (tourneyPhase_ == TourneyPhase::Champion) {
        // A receipt, one item to a line. The purse lands in one lump and is the only
        // thing the arena ever pays, so it is itemised rather than summarised — and
        // the band overhead already says CHAMPION, which is the sentence.
        std::snprintf(line[0], sizeof(line[0]), "TITLE TAKEN");
        std::snprintf(line[1], sizeof(line[1]), "+%d BITS", kTourneyWinBits);
        std::snprintf(line[2], sizeof(line[2]), "+%d XP", kTourneyWinXp);
        std::snprintf(line[3], sizeof(line[3]), "+1 MOD");
    } else if (tourneyPhase_ == TourneyPhase::Eliminated) {
        // Nobody is standing on the dock for this one, so the copy has the whole width
        // and gets to say it in the words it was written in.
        std::snprintf(line[0], sizeof(line[0]), "KNOCKED OUT OF THE DRAW");
        std::snprintf(line[1], sizeof(line[1]), "THE BRACKET GOES ON");
    } else if (opp >= 0) {
        std::snprintf(line[0], sizeof(line[0]), "%s",
                      tourneyCard(tourneySeed_, opp).handle);
        std::snprintf(line[1], sizeof(line[1]), "%s  L%d", c ? c->displayName : "UNKNOWN",
                      static_cast<int>(tourneyOpponent_.spec.level));
        // The Exploit as its TAG, not its crew NAME — the short mechanic word every
        // other surface shows it by (crewExploitTag), and the only form that fits.
        std::snprintf(line[2], sizeof(line[2]), "%s",
                      crewExploitTag(tourneyOpponent_.exploit.kind));
        exploitTell(tourneyOpponent_.exploitAtHealthPct, line[3], sizeof(line[3]));
    }
    // The first line is the NAME and the rest describe it, which is the same weight
    // split every row of every list on the device already uses.
    for (int i = 0; i < kDockCardLines; ++i) {
        if (!line[i][0]) continue;
        drawTextMarquee(fb, kMargin, top + i * kDockCardLineH, w, line[i],
                        palColor(i == 0 ? Pal::INK : Pal::INK_DIM), beat_,
                        /*scroll=*/true);
    }
}

void Game::drawTourneyBrief(Framebuffer& fb) const {
    drawHeaderBand(fb, kTourneyName, "BRIEF");
    drawProseRows(fb, tourneyBriefRows(), tourneyScroll_, kDockBriefTop, beat_,
                  "B MORE   C BACK");
}

void Game::drawTourneyScout(Framebuffer& fb) const {
    // The sheet's own header is the only thing that is not the LOADOUT page: WHO this
    // kit belongs to, WHAT it is, and — since this is the one screen with room for it —
    // the drawing. Everything under it is the shared flow.
    const int slot = tourneyScoutedSlot_;
    const bool self = slot == tourneySlot();
    drawHeaderBand(fb, self ? hackerTag_ : tourneyCard(tourneySeed_, slot).handle,
                   "SCOUT");
    const CreatureDef* c = registry_.creature(tourneyScouted_.spec.creatureId);
    const SpriteData* s = c ? registry_.sprite(c->spriteName) : nullptr;
    bool mirror = false;
    if (s) {
        // Seated on the sheet's own foot line, facing the caption that describes it.
        mirror = spriteMirrorToFace(*s, /*faceRight=*/false);
        drawSpriteUpscaled(fb, *s, idleFrame(*s, beat_), dockSeatX(*s, mirror),
                           kDockPortraitFootY - s->h, 1, 1, /*row=*/0, mirror);
    }
    // Stacked beside the portrait rather than run along one line: the three facts are
    // the sheet's caption, and a caption reads down the side of a picture.
    const int w = dockCardW(s, mirror);
    const int top = s ? kDockCardTop : kDockTextCardTop;
    drawTextMarquee(fb, kMargin, kDockSubY, w, c ? c->displayName : "UNKNOWN",
                    palColor(Pal::INK), beat_, /*scroll=*/true);
    char lvl[16];
    std::snprintf(lvl, sizeof(lvl), "LEVEL %d",
                  static_cast<int>(tourneyScouted_.spec.level));
    drawText(fb, kMargin, kDockSubY + kLineH, lvl, palColor(Pal::INK_DIM));
    const char* state = !(tourneyAlive_ & (1u << slot)) ? "OUT OF THE DRAW"
                        : slot == tourneyOpponentSlot() ? "YOU FACE THIS"
                        : self                          ? "THIS IS YOURS"
                                                        : "STILL IN";
    drawTextMarquee(fb, kMargin, kDockSubY + 2 * kLineH, w, state,
                    palColor(Pal::INK_DIM), beat_, /*scroll=*/true);
    fb.fillRect(0, kDockScoutTop - 6, kActiveW, 1, palColor(Pal::TRACK));
    drawProseRows(fb, tourneyScoutRows(), tourneyScroll_, kDockScoutTop, beat_,
                  "B MORE   C BACK");
}

}  // namespace mal
