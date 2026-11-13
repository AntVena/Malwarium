#include "core/app/game/arena_screen.h"

#include <cstdio>

#include "tunables.h"
#include "core/app/game_internal.h"   // levelStatName
#include "core/app/operator.h"
#include "core/content/content_crews.h"
#include "core/content/content_tournament.h"
#include "core/content/effect_text.h"
#include "core/content/registry.h"
#include "core/model/loadout.h"
#include "core/model/move_loadout.h"
#include "core/model/pet.h"
#include "core/model/pvp_battle.h"
#include "core/model/tournament.h"
#include "core/model/tourney_run.h"
#include "core/render/canvas.h"
#include "core/render/framebuffer.h"
#include "core/render/font.h"
#include "core/render/palette.h"
#include "core/render/scenes.h"
#include "core/render/sprite.h"
#include "core/ui/layout.h"
#include "core/ui/prose_page.h"
#include "core/ui/stat_screen.h"
#include "core/ui/tourney_screen.h"
#include "core/ui/widgets.h"

// What the three reads look like is laid out against core/ui/tourney_screen.h; the run
// they read is game_tourney.cpp's.

namespace mal {

namespace {
// The screen is only ever up over a run, so a missing one draws as an empty bracket
// rather than being checked at every read.
const TourneyRun& runOf(const ArenaHost& host) {
    static const TourneyRun kNone{};
    const TourneyRun* r = host.tourneyRun();
    return r ? *r : kNone;
}

// The two READER views (SCOUT, BRIEF) page against ui/tourney_screen.h's tops: the
// scout sheet's is lower, because it seats a portrait.
inline int tourneyReaderTop(TourneyView v) {
    return v == TourneyView::Scout ? kDockScoutTop : kDockBriefTop;
}
}  // namespace

namespace {

// The status column, fixed at the widest word it ever holds rather than sized per
// row: eight levels that each sat wherever their own tag left room is eight levels
// the eye cannot compare, and comparing them is what the field list is for.
inline int slotTagWidth() { return textWidth("NEXT"); }

// When an entrant fires its Exploit, as the tell the briefing promises the operator
// they can learn ("Some open with it; some wait until they are cornered"). A
// percentage, so the operator can check it against the health bar during the bout. The
// arena states it up front because no surprise here can end a run.
void exploitTell(int atHealthPct, bool eitherSide, char* buf, int n) {
    if (atHealthPct >= 100) std::snprintf(buf, n, "FROM TURN 1");
    else if (eitherSide) std::snprintf(buf, n, "ANYONE AT %d%%", atHealthPct);
    else std::snprintf(buf, n, "AT %d%% HEALTH", atHealthPct);
}

}  // namespace

void ArenaScreen::draw(Framebuffer& fb, const ArenaHost& host) const {
    switch (view_) {
        case TourneyView::Scout: drawScout(fb, host); return;
        case TourneyView::Brief: drawBrief(fb, host); return;
        case TourneyView::Bracket: break;
    }
    drawBracket(fb, host);
}

void ArenaScreen::drawBracket(Framebuffer& fb, const ArenaHost& host) const {
    // The harbour is this screen's BACKGROUND pass (core/render/RENDER_PIPELINE.md),
    // so it goes down first and the band composes onto it instead of clearing it away.
    drawScene(fb, SceneId::PirateBayou, host.screenBeat(), kDockGround);
    // Which round it is rides the band's right label. It is a number, so the
    // header survives grayscale, and the two verdicts take the same slot because
    // CHAMPION is also a position in the bracket.
    char where[16];
    if (runOf(host).phase == TourneyPhase::Champion)
        std::snprintf(where, sizeof(where), "CHAMPION");
    else if (runOf(host).phase == TourneyPhase::Eliminated)
        std::snprintf(where, sizeof(where), "ELIMINATED");
    else
        std::snprintf(where, sizeof(where), "ROUND %d/%d", runOf(host).round + 1,
                      kTourneyRounds);
    drawHeaderBandOver(fb, kTourneyName, where);

    const int me = runOf(host).playerSlot();
    const int opp = runOf(host).opponentSlot();
    const uint8_t alive = runOf(host).alive;
    const int cur = cursor_;

    // The tree goes down before the rows, so a row's own highlight can never break a
    // bracket line the row is hanging off.
    const int round = runOf(host).round < kTourneyRounds ? runOf(host).round : kTourneyRounds - 1;
    drawDockTies(fb, alive, cur, round, me);

    for (int slot = 0; slot < kTourneySlots; ++slot) {
        const int y = dockRowY(alive, slot, cur);
        const bool live = (alive & (1u << slot)) != 0;
        const bool focused = slot == cur;
        // Out of the draw and not being read: the row collapses to the strike alone.
        // The strikes are countable, and counting them gives the same answer as the
        // "n LEFT" readout, so the collapse reads correctly in grayscale.
        if (!live && !focused) {
            fb.fillRect(kDockTextX, y + kDockOutRowH / 2,
                        textWidth(slot == me ? host.operatorRecord().hackerTag
                                             : tourneyCard(runOf(host).seed, slot).handle),
                        1, palColor(Pal::INK_DIM));
            continue;
        }
        if (focused) {
            fb.fillRect(kDockTextX - 3, y - 2, kActiveW - kDockTextX - 1,
                        kDockLiveRowH - 1, palColor(Pal::TRACK));
            drawRowCursor(fb, 2, y, palColor(Pal::ACCENT));
        }
        const TourneyCard card = tourneyCard(runOf(host).seed, slot);
        // The operator's own row shows their tag like every other entrant, so all
        // eight rows compare the same way.
        const char* name = slot == me ? host.operatorRecord().hackerTag : card.handle;
        const Rgb565 ink = live ? palColor(Pal::INK) : palColor(Pal::INK_DIM);
        drawText(fb, kDockTextX, y, name, ink);
        if (!live)   // the same strike the collapsed rows carry, through the name
            fb.fillRect(kDockTextX, y + kFontH / 2, textWidth(name), 1,
                        palColor(Pal::INK_DIM));
        char lvl[12];
        std::snprintf(lvl, sizeof(lvl), "L%d", slot == me ? host.petRecord().level : card.level);
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

    drawFoot(fb, host);

    if (runOf(host).phase == TourneyPhase::Ready) {
        drawText(fb, kMargin, kDockGestureY, "HOLD B SCOUT  A+C BRIEF",
                 palColor(Pal::INK_DIM));
        drawHintBand(fb, "A NEXT   B BOUT   C BACK");
    } else {
        drawHintBand(fb, "ANY KEY");
    }
}

void ArenaScreen::drawFoot(Framebuffer& fb, const ArenaHost& host) const {
    const int opp = runOf(host).opponentSlot();
    const bool ready = runOf(host).phase == TourneyPhase::Ready;
    // WHOSE drawing stands on the dock. Waiting on a bout it shows only the opponent,
    // because the operator's pet appears everywhere else. It is also the only way a
    // 96-wide Daemon and a column of copy fit on one 224px screen. Taking the bracket
    // is the exception, and it is the pet's: a title is a picture of your own pet
    // alone on a dock everybody else has gone home from. Being knocked out draws
    // nobody, and an empty dock is the right picture for that.
    const bool mine = runOf(host).phase == TourneyPhase::Champion;
    const CreatureDef* c =
        ready && opp >= 0 ? host.content().creature(runOf(host).opponent.spec.creatureId) : nullptr;
    // A field too full to seat anyone takes the same exit as a verdict: no drawing, and
    // the copy moves to the foot and takes the whole width.
    const SpriteData* s =
        !dockFaceoffFits(runOf(host).alive) ? nullptr
        : mine ? (host.petRecord().creature ? host.content().creatureSprite(*host.petRecord().creature) : nullptr)
        : c    ? host.content().sprite(c->spriteName)
               : nullptr;
    // The opponent faces the copy that describes it; a champion faces out.
    const bool mirror = s && spriteMirrorToFace(*s, /*faceRight=*/mine);
    if (s)
        drawSpriteUpscaled(fb, *s, idleFrame(*s, host.screenBeat()), dockSeatX(*s, mirror),
                           dockSeatY(*s), 1, 1, /*row=*/0, mirror);
    const int w = dockCardW(s, mirror);
    const int top = s ? kDockCardTop : kDockTextCardTop;

    // Four lines, in the order the question is asked: who is it, what is it, what is
    // it carrying, and when will it use that. The last one is the tell the briefing
    // promises the operator they can learn, stated up front because the arena is the
    // one place where knowing it early costs the run nothing.
    char line[kDockCardLines][32] = {};
    if (runOf(host).phase == TourneyPhase::Champion) {
        // A receipt, one item to a line. The purse lands in one lump and is the only
        // thing the arena ever pays, so it is itemised. The band above already says
        // CHAMPION.
        std::snprintf(line[0], sizeof(line[0]), "TITLE TAKEN");
        std::snprintf(line[1], sizeof(line[1]), "+%d BITS", kTourneyWinBits);
        std::snprintf(line[2], sizeof(line[2]), "+%d XP", kTourneyWinXp);
        std::snprintf(line[3], sizeof(line[3]), "+1 MOD");
    } else if (runOf(host).phase == TourneyPhase::Eliminated) {
        // Nobody is standing on the dock for this one, so the copy has the whole width
        // and gets to say it in the words it was written in.
        std::snprintf(line[0], sizeof(line[0]), "KNOCKED OUT OF THE DRAW");
        std::snprintf(line[1], sizeof(line[1]), "THE BRACKET GOES ON");
    } else if (opp >= 0) {
        std::snprintf(line[0], sizeof(line[0]), "%s",
                      tourneyCard(runOf(host).seed, opp).handle);
        std::snprintf(line[1], sizeof(line[1]), "%s  L%d", c ? c->displayName : "UNKNOWN",
                      static_cast<int>(runOf(host).opponent.spec.level));
        // The Exploit's tag (crewExploitTag), the short word other screens use and
        // the only form that fits.
        std::snprintf(line[2], sizeof(line[2]), "%s",
                      crewExploitTag(runOf(host).opponent.exploit.kind));
        exploitTell(runOf(host).opponent.exploitAtHealthPct, runOf(host).opponent.exploitOnEitherHealth,
                    line[3], sizeof(line[3]));
    }
    // The first line is the NAME and the rest describe it, which is the same weight
    // split every row of every list on the device already uses.
    for (int i = 0; i < kDockCardLines; ++i) {
        if (!line[i][0]) continue;
        drawTextMarquee(fb, kMargin, top + i * kDockCardLineH, w, line[i],
                        palColor(i == 0 ? Pal::INK : Pal::INK_DIM), host.screenBeat(),
                        Overflow::Scroll);
    }
}

void ArenaScreen::drawBrief(Framebuffer& fb, const ArenaHost& host) const {
    drawHeaderBand(fb, kTourneyName, "BRIEF");
    drawProseRows(fb, {briefRows(), scroll_, kDockBriefTop, host.screenBeat(),
                       "B MORE   C BACK"});
}

void ArenaScreen::drawScout(Framebuffer& fb, const ArenaHost& host) const {
    // Only the sheet's header differs from the LOADOUT page: WHO this kit belongs to,
    // WHAT it is, and (since this is the one screen with room for it) the drawing.
    // Everything under it is the shared flow.
    const int slot = scoutedSlot_;
    const bool self = slot == runOf(host).playerSlot();
    drawHeaderBand(fb, self ? host.operatorRecord().hackerTag : tourneyCard(runOf(host).seed, slot).handle,
                   "SCOUT");
    const CreatureDef* c = host.content().creature(scouted_.spec.creatureId);
    const SpriteData* s = c ? host.content().sprite(c->spriteName) : nullptr;
    bool mirror = false;
    if (s) {
        // Seated on the sheet's own foot line, facing the caption that describes it.
        mirror = spriteMirrorToFace(*s, /*faceRight=*/false);
        drawSpriteUpscaled(fb, *s, idleFrame(*s, host.screenBeat()), dockSeatX(*s, mirror),
                           kDockPortraitFootY - s->h, 1, 1, /*row=*/0, mirror);
    }
    // Stacked beside the portrait rather than run along one line: the three facts are
    // the sheet's caption, and a caption reads down the side of a picture.
    const int w = dockCardW(s, mirror);
    const int top = s ? kDockCardTop : kDockTextCardTop;
    drawTextMarquee(fb, kMargin, kDockSubY, w, c ? c->displayName : "UNKNOWN",
                    palColor(Pal::INK), host.screenBeat(), Overflow::Scroll);
    char lvl[16];
    std::snprintf(lvl, sizeof(lvl), "LEVEL %d",
                  static_cast<int>(scouted_.spec.level));
    drawText(fb, kMargin, kDockSubY + kLineH, lvl, palColor(Pal::INK_DIM));
    const char* state = !(runOf(host).alive & (1u << slot)) ? "OUT OF THE DRAW"
                        : slot == runOf(host).opponentSlot() ? "YOU FACE THIS"
                        : self                          ? "THIS IS YOURS"
                                                        : "STILL IN";
    drawTextMarquee(fb, kMargin, kDockSubY + 2 * kLineH, w, state,
                    palColor(Pal::INK_DIM), host.screenBeat(), Overflow::Scroll);
    fb.fillRect(0, kDockScoutTop - 6, kActiveW, 1, palColor(Pal::TRACK));
    drawProseRows(fb, {scoutRows(host), scroll_, kDockScoutTop, host.screenBeat(),
                       "B MORE   C BACK"});
}


// --- The two readers' row models ---------------------------------------------

std::vector<ProseRow> ArenaScreen::scoutRows(const ArenaHost& host) const {
    // An entrant's kit, built through the SAME buildLoadoutRows STAT's LOADOUT page
    // uses: the opponent's moves and mods are described by their own content rows,
    // wrapped whole, as the operator's are. Nothing here re-words a move.
    std::vector<ProseRow> out;
    const CreatureDef* pet = host.content().creature(scouted_.spec.creatureId);
    if (!pet) return out;
    MoveLoadout moves;
    ModSlots mods;
    buildPvpLoadouts(host.content(), scouted_.spec, moves, mods);
    out = buildLoadoutRows(host.content(), moves, mods, pet->stage, /*isEgg=*/false);

    // ...then the two things a loadout page has no column for, as a section of their
    // own: how the levels were SPENT (the stat spread is half of what an opponent is,
    // and it is invisible in a move list), and the Exploit it is carrying. The trigger
    // is deliberately NOT stated, which moment a rival commits to is the read the
    // arena exists to teach, and printing it would answer the question for them.
    ProseRow stats;
    stats.header = true;
    stats.label = "STATS";
    out.push_back(stats);
    for (int i = 0; i < kLevelStatCount; ++i) {
        ProseRow r;
        r.label = levelStatName(i);
        setProseTag(r, "+%d", scouted_.spec.statPoints[i]);
        out.push_back(r);
    }
    if (scouted_.exploit.kind != CrewExploitKind::None) {
        ProseRow head;
        head.header = true;
        head.label = "EXPLOIT";
        out.push_back(head);
        ProseRow r;
        r.label = scouted_.exploit.label;
        setProseTag(r, "%s", crewExploitTag(scouted_.exploit.kind));
        // The crew's own description of the ability, templated off its magnitude: the
        // same sentence the CREW screen shows.
        for (int c = 0; c < kCrewCount; ++c)
            if (crew(c)->exploit.kind == scouted_.exploit.kind)
                r.body = effectText(crew(c)->exploit);
        out.push_back(r);
    }
    return out;
}

std::vector<ProseRow> ArenaScreen::briefRows() {
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

void ArenaScreen::openScout(const ArenaHost& host) {
    const TourneyRun* run = host.tourneyRun();
    if (view_ != TourneyView::Bracket || !run) return;
    // Building an entrant walks the whole move and mod tables, so it happens HERE, on
    // the press, and the sheet reads the held copy for as long as it is open.
    scouted_ = tourneyEntrant(host.content(), run->seed, cursor_);
    scoutedSlot_ = cursor_;
    view_ = TourneyView::Scout;
    scroll_ = 0;
}

// --- Input -------------------------------------------------------------------

// Held on the bracket past kTourneyScoutHoldMs, B opens the focused entrant's SCOUT
// sheet: its full kit, read on the very page the operator reads their own kit on. The
// tap underneath (tourneyReleaseB) fights the operator's own bout, so both engage with
// the fighter under the cursor, which is what keeps the hold related to the tap.
bool ArenaScreen::holdB(ArenaHost& host, uint32_t heldMs) {
    if (view_ != TourneyView::Bracket || heldMs < kTourneyScoutHoldMs) return false;
    openScout(host);
    return true;
}

void ArenaScreen::releaseB(ArenaHost& host) {
    if (view_ != TourneyView::Bracket) return;
    host.startTourneyMatch();
}

void ArenaScreen::onButton(ArenaHost& host, const ButtonEvent& ev) {
    // The two READERS first: both are plain scroll-and-back pages, and neither can
    // start a bout, so nothing below them has to know they exist.
    if (view_ != TourneyView::Bracket) {
        const std::vector<ProseRow> rows = view_ == TourneyView::Scout
                                               ? scoutRows(host)
                                               : briefRows();
        const int total = static_cast<int>(rows.size());
        // B scrolls by the window that was on screen and wraps at the end, as the
        // STAT reader does.
        if (ev.button == Button::B && total > 0) {
            const int shown = proseRowsFitting(rows, scroll_, tourneyReaderTop(view_));
            scroll_ = nextWindowTop(scroll_, shown, total);
        } else if (ev.button == Button::C || ev.chordAC) {
            view_ = TourneyView::Bracket;
            scroll_ = 0;
        }
        return;
    }
    // The BRIEFING is the chord's job here: see game_core.cpp's chord routing for why
    // it is the chord and not a fourth key.
    if (ev.chordAC) {
        view_ = TourneyView::Brief;
        scroll_ = 0;
        return;
    }
    // A terminal banner is a read, not a menu: any of the three keys clears the run and
    // returns to EXPL. All three answer, because nothing here is destructive and a
    // verdict already read should not need a specific button.
    if (runOf(host).phase != TourneyPhase::Ready) {
        host.abandonArena();
        return;
    }
    if (ev.button == Button::A) {
        // A READS the field: it steps the cursor over the eight slots so the operator
        // can see who is left and at what level, and it chooses whose SCOUT sheet a
        // held B opens. It never picks who you fight: the bracket does that.
        cursor_ = stepRow(cursor_, kTourneySlots, +1);
    } else if (ev.button == Button::B) {
        // Tap/hold, resolved on the RELEASE edge: a tap starts the bout
        // (releaseB), a hold opens the focused entrant's sheet (holdB).
        host.armHoldB();
    } else if (ev.button == Button::C) {
        // Leaving does NOT forfeit: the run is persisted, and the EXPL row says IN PLAY
        // until it is finished. A bracket is a commitment of attention, not of one
        // sitting.
        host.leaveArena();
    }
}


void ArenaScreen::open(int ownSlot) {
    view_ = TourneyView::Bracket;
    cursor_ = ownSlot;
    scroll_ = 0;
    scoutedSlot_ = -1;
}

}  // namespace mal
