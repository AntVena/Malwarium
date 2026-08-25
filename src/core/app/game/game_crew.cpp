#include "core/app/game.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "core/app/game_internal.h"   // drawScrollbar — shared with game_peers.cpp
#include "core/content/content_crews.h"
#include "core/content/effect_text.h"  // effectText — the Exploit's description prose
#include "core/render/canvas.h"
#include "core/render/font.h"
#include "core/render/palette.h"
#include "core/render/sprite.h"
#include "core/ui/layout.h"
#include "core/ui/theme.h"
#include "core/ui/widgets.h"
#include "generated/assets.h"

// game_crew.cpp — the Hacker-face CREW slot: enlist in a crew (content_crews.h) and
// designate the HOME NETWORK that membership hangs off.
//
// FOUR VIEWS, not one list (Game::CrewView). Enlisting is the one piece of player
// IDENTITY the device asks you to author — it outlives every pet you raise — and a
// roster stacked into a single scrolling list gives it the same weight as a settings
// row. So the screen spends the space instead:
//
//   Hub     · who you are now (the allegiance card), and the two SIDES to browse.
//   Team    · one side's roster, a row per crew: name at title weight, motto under it,
//             and the Exploit's mechanic word right-aligned so the side is comparable
//             down a column.
//   Detail  · one crew's page — glyph, motto, the Exploit named and described in full,
//             and the single verb (ENLIST / LEAVE) that changes anything.
//   Picker  · the home-network chooser, a modal over the Hub rather than a step past
//             it: designating turf is the PRECONDITION for all of the above, not a
//             stage of the browse.
//
// C walks back up that order, and only leaves the screen from the Hub — so the deeper
// views cost a press to escape and never strand the player. The picker answers exactly
// one question — "which of the networks I can hear right now am I claiming?" — so its
// rows are the live in-range snapshot the passive scan refreshes
// (Game::registerNetwork -> visibleNetworks_), and opening the screen arms that scan on
// its own (Game::radioScanWanted). The SD-backed NetworkLedger
// (core/net/network_ledger.h) supplies each row's walked-in tally as evidence for the
// choice; it never contributes a row of its own.
//
// Membership's payoff in play is the crew's signature Exploit: crewExploitOption()
// hands it to Combat as the A+C picker's last row (Game::onCombat → openOverride).
//
// THE SIDE IS DUAL-CODED EVERYWHERE, in this order of authority: the word (RED/BLUE,
// and the doctrine under it), the glyph (ICON_TEAM_RED/BLUE — different shapes, not one
// shape recoloured), and only then the tint. The tint is the channel that can be lost,
// so nothing on these screens is distinguishable by it alone.

namespace mal {

namespace {
// The picker window. Each row is a name line plus room for the HOME/LIVE sub-tags
// beneath it; six of them clear the body between the header rule and the hint band.
constexpr int kNetRowPitch = kFontH + 14;
constexpr int kNetVisibleRows = 6;
constexpr int kGlyph = 16;   // ICON_TEAM_RED / ICON_TEAM_BLUE are 16x16

// --- The Hub ---------------------------------------------------------------
// The allegiance card: a raised TRACK panel given the top of the body, so the first
// thing the screen says is who you are. Three lines, and each is sized to hold the
// LONGEST row the table can hand it rather than to whatever fits today — the crew name
// gets the full width beside the glyph, the side word and the Exploit's meter share the
// second, and the Exploit's name has the third to itself. Nothing on this card clips.
// Fixed height whether or not you belong to a crew, so enlisting never shifts the two
// side rows out from under the cursor.
constexpr int kCardTop = 50;
constexpr int kCardH = 46;
constexpr int kCardPad = 6;
constexpr int kCardTextTop = kCardTop + 6;
// ...and the two sides under it: glyph, the side at title weight, doctrine + count.
constexpr int kSideTop = kCardTop + kCardH + 12;   // 108
constexpr int kSideH = 38;

// --- The Team roster --------------------------------------------------------
// Three lines a row — name at title weight, the motto, then the Exploit's mechanic word
// — and a row pitch with real air in it. That is the whole point of the side having a
// screen of its own: five crews stacked two-to-a-row was the cramped thing this pass
// exists to undo, and a column of NEGATE / ESCALATE / RALLY is what makes a side a
// choice rather than five paragraphs read in turn.
// Four line-heights per row — name, up to two wrapped tagline lines, then the
// Exploit tag — plus real gap to the next row. Fixed, not measured per-row: the
// tagline's slot is reserved at its worst case (2 lines) whether a given crew's
// motto needs it or not, so a short motto leaves a little air and a long one
// never competes with the row below for the same pixels.
constexpr int kTeamRowH = 60;
constexpr int kTeamVisibleRows = 3;

// --- The Detail page --------------------------------------------------------
// The motto WRAPS rather than travelling: it is the crew's voice and the reason the
// page exists, so it gets two lines of its own instead of a marquee the reader has to
// wait out. Everything below is measured from where that block ends.
constexpr int kDetailMottoY = 28;
// Three, not two: the longest motto in the table needs all three beside the glyph, and
// a motto cut at "...'send it straight over'," has lost the joke it was written for.
constexpr int kDetailMottoLines = 3;
constexpr int kDetailRule = 66;
constexpr int kDetailExploitY = 72;
constexpr int kDetailProseTop = kDetailExploitY + kLineH * 2 + 2;   // 100
constexpr int kDetailVerbY = kActiveH - 46;                         // 178
// Prose gets whatever is left between the Exploit block and the verb row, and pages
// on A when it wants more (the same bargain drawMoveDetail strikes: the derived
// readout is bounded and never cut, the authored prose is elastic and scrolls).
constexpr int kDetailProseLines = (kDetailVerbY - kDetailProseTop) / kLineH;

// A raised panel, the same TRACK fill the combat picker and stat panels use — the
// device's one "this block is a thing, not a row" cue.
void drawPanel(Framebuffer& fb, int x, int y, int w, int h) {
    fb.fillRect(x, y, w, h, palColor(Pal::TRACK));
}

// drawLabelValue's shape, but with the right edge PASSED rather than assumed to be the
// canvas margin. Inside the allegiance card the content stops at the panel's own
// padding, and the shared widget — which measures against kMargin, correctly, for a
// full-width row — hangs the value six pixels off the end of the panel it sits in.
void drawPairIn(Framebuffer& fb, int x, int y, int right, const char* label,
                Rgb565 labelCol, const char* value, Rgb565 valueCol, int beat,
                bool scroll, FontFace face = FontFace::Regular) {
    int room = right - x;
    if (value && value[0]) {
        const int vx = right - textWidth(value);
        drawText(fb, vx, y, value, valueCol);
        room = vx - 8 - x;                 // a glyph's gap between the two
    }
    if (room > 0) drawTextMarquee(fb, x, y, room, label, labelCol, beat, scroll, face);
}
}  // namespace

// --- Membership + home network --------------------------------------------

void Game::setHomeNetwork(uint64_t key, const char* name) {
    homeNetworkKey_ = key;
    std::strncpy(homeNetworkName_, name ? name : "", sizeof(homeNetworkName_) - 1);
    homeNetworkName_[sizeof(homeNetworkName_) - 1] = '\0';
    if (key == 0) {
        homeNetworkName_[0] = '\0';
        crewIndex_ = -1;   // membership can't outlive the network it defends
    }
    markSaveDirty();
    dirty_ = true;
}

bool Game::joinCrew(int index) {
    if (!crew(index)) return false;
    if (!hasHomeNetwork()) return false;   // a crew member is always somebody's defender
    if (crewIndex_ == index) return true;
    crewIndex_ = index;
    // Taking a side at all, once. Fired on the join rather than counted, because a
    // player belongs to at most one crew and may switch freely — there is no tally here
    // that could mean anything, only the first time they stopped being unaffiliated.
    unlockAchievement(ach::kCrewEnlisted);
    log_.push(LogEventType::ItemGained, kCrews[index].displayName);
    markSaveDirty();
    dirty_ = true;
    return true;
}

void Game::leaveCrew() {
    if (crewIndex_ < 0) return;
    crewIndex_ = -1;
    markSaveDirty();
    dirty_ = true;
}

CrewExploit Game::crewExploitOption() const {
    const CrewDef* c = activeCrew();
    if (!c || c->exploit.kind == CrewExploitKind::None) return {};
    return {c->exploit.name, c->exploit.kind, c->exploit.magnitude};
}

void Game::debugSeedNetworkLedger(uint64_t key, const char* name, int count) {
    networkLedger_.recordNew(key, name);
    for (int i = 1; i < count; ++i) networkLedger_.recordSeenAgain(key, name);
    dirty_ = true;
}

int Game::crewNetworkRows(CrewNetRow* rows, int max) const {
    int n = 0;
    // Exactly one source of rows: what the radio can hear right now. The reward
    // queue (pendingNetworks_) is deliberately NOT consulted — its size and its
    // walk-paced drain rate have nothing to do with which networks are in range, and
    // letting them leak in here is what made a saturated backlog hide the network the
    // operator was standing in front of.
    for (int i = 0; i < visibleNetworkCount_ && n < max; ++i) {
        const VisibleNetwork& v = visibleNetworks_[i];
        if (!networkVisible(v)) continue;
        // The ledger contributes evidence, never an offer: how many times this pet has
        // walked this network in, which is what tells the familiar one apart from the
        // six neighbours' APs sitting next to it in the list.
        NetworkLedger::Entry known;
        const uint32_t count = networkLedger_.lookup(v.key, &known) ? known.count : 0;
        rows[n++] = {v.key, v.name, count};
    }
    // Most-walked first — the strongest available signal for which network is actually
    // yours, offered as a choice rather than inferred. Stable within a tie, so the order stays
    // steady between repaints instead of shuffling under the cursor.
    std::stable_sort(rows, rows + n, [](const CrewNetRow& a, const CrewNetRow& b) {
        return a.count > b.count;
    });
    return n;
}

// --- Input ------------------------------------------------------------------

void Game::onHackerCrew(const ButtonEvent& ev) {
    // C is the same key at every depth: step back ONE view, and leave the screen only
    // from the Hub. Written once here rather than per-view so a view added later
    // cannot forget to be escapable.
    if (ev.button == Button::C) {
        switch (crewView_) {
            case CrewView::Detail: crewView_ = CrewView::Team; break;
            case CrewView::Team:
            case CrewView::Picker: crewView_ = CrewView::Hub;  break;
            case CrewView::Hub:    nav_ = Nav::Cursor;         return;
        }
        dirty_ = true;
        return;
    }

    switch (crewView_) {
        case CrewView::Picker: {
            CrewNetRow rows[kNetMaxRows];
            const int n = crewNetworkRows(rows, kNetMaxRows);
            if (n == 0) return;                        // empty state: A/B inert
            if (crewNetRow_ >= n) crewNetRow_ = 0;
            if (ev.button == Button::A) {
                crewNetRow_ = (crewNetRow_ + 1) % n;
            } else if (ev.button == Button::B) {
                setHomeNetwork(rows[crewNetRow_].key, rows[crewNetRow_].name);
                crewView_ = CrewView::Hub;
            }
            return;
        }
        case CrewView::Hub:
            if (ev.button == Button::A) {
                crewHubRow_ = (crewHubRow_ + 1) % kCrewHubRows;
            } else if (ev.button == Button::B) {
                if (crewHubRow_ == 0) {                // HOME NET row -> the modal
                    crewView_ = CrewView::Picker;
                    crewNetRow_ = 0;
                } else {                               // a side -> its roster
                    crewTeam_ = crewHubRow_ == 1 ? CrewTeam::Red : CrewTeam::Blue;
                    crewTeamRow_ = 0;
                    crewView_ = CrewView::Team;
                }
            }
            break;
        case CrewView::Team: {
            const int n = crewTeamCount(crewTeam_);
            if (n == 0) return;                        // a side with no crews yet
            if (crewTeamRow_ >= n) crewTeamRow_ = 0;
            if (ev.button == Button::A) {
                crewTeamRow_ = (crewTeamRow_ + 1) % n;
            } else if (ev.button == Button::B) {
                crewDetail_ = crewByTeam(crewTeam_, crewTeamRow_);
                crewProseScroll_ = 0;
                crewView_ = CrewView::Detail;
            }
            break;
        }
        case CrewView::Detail: {
            const CrewDef* c = crew(crewDetail_);
            if (!c) { crewView_ = CrewView::Team; break; }
            if (ev.button == Button::B) {              // the one verb on this page
                if (crewIndex_ == crewDetail_) leaveCrew();
                else joinCrew(crewDetail_);            // no-op without a home network
            } else if (ev.button == Button::A) {
                // A already means "advance", so paging the description with it needs no
                // exception to the A/B/C contract — and a page that fits never moves,
                // because the wrap count is what bounds the offset.
                const EffectText prose = effectText(c->exploit);
                const int lines = textWrapLines(prose.c_str(), kActiveW - 2 * kMargin);
                if (lines > kDetailProseLines)
                    crewProseScroll_ =
                        (crewProseScroll_ + 1) % (lines - kDetailProseLines + 1);
            }
            break;
        }
    }
    dirty_ = true;
}

// --- Rendering ---------------------------------------------------------------

// The one dispatch, and the only place the header band is chosen. Unlike its sibling
// Hacker sub-screens (whose caller draws a fixed band for them), the CREW title is a
// function of the view — and the deeper two are TINTED by the side they belong to, so
// the band itself carries the Red/Blue axis down with you.
void Game::drawHackerCrew(Framebuffer& fb) const {
    switch (crewView_) {
        case CrewView::Hub:
            drawHeaderBand(fb, "CREW");
            drawCrewHub(fb);
            return;
        case CrewView::Picker:
            drawHeaderBand(fb, "HOME NET");
            drawCrewNetPicker(fb);
            return;
        case CrewView::Team:
            drawHeaderBand(fb, crewTeamName(crewTeam_), crewTeamDoctrine(crewTeam_),
                           palColor(Pal::INK_DIM),
                           teamColor(crewTeam_ == CrewTeam::Red));
            drawCrewTeam(fb);
            return;
        case CrewView::Detail: {
            const CrewDef* c = crew(crewDetail_);
            if (!c) { drawHeaderBand(fb, "CREW"); return; }
            const bool red = c->team == CrewTeam::Red;
            drawHeaderBand(fb, c->displayName, crewTeamName(c->team), teamColor(red),
                           teamColor(red));
            drawCrewDetail(fb);
            return;
        }
    }
}

// --- Hub ----------------------------------------------------------------------

void Game::drawCrewHub(Framebuffer& fb) const {
    // HOME NET keeps the Hacker face's context band (layout.h's kContextY/kContextRule):
    // it is a precondition the rows below it are read against, which is exactly what
    // that band is for. It is also row 0, so it carries the cursor like any other.
    const bool homeSel = crewHubRow_ == 0;
    if (homeSel) drawRowCursor(fb, 2, kContextY + 1, palColor(Pal::ACCENT));
    const char* homeVal = hasHomeNetwork() ? homeNetworkName_ : "NOT SET";
    drawLabelValue(fb, kMargin + 10, kContextY, "HOME NET",
                   homeSel ? palColor(Pal::INK) : palColor(Pal::INK_DIM), homeVal,
                   hasHomeNetwork() ? palColor(Pal::INK) : palColor(Pal::WARN), beat_,
                   homeSel);
    fb.fillRect(0, kContextRule, kActiveW, 1, palColor(Pal::TRACK));

    // The allegiance card — the screen's answer to "who am I", stated before it offers
    // anywhere to go. Not selectable: there is nothing to DO to your own membership
    // from here that leaving through the crew's own page doesn't say better.
    drawPanel(fb, kMargin, kCardTop, kActiveW - 2 * kMargin, kCardH);
    const int cx = kMargin + kCardPad;
    const int cRight = kActiveW - kMargin - kCardPad;
    const CrewDef* mine = activeCrew();
    if (mine) {
        const bool red = mine->team == CrewTeam::Red;
        const Rgb565 side = teamColor(red);
        // The glyph anchors the top two lines the way a detail page's icon does, and
        // the two of them indent past it; the third runs the full width beneath.
        drawSpriteTinted(fb, red ? ASSET_ICON_TEAM_RED : ASSET_ICON_TEAM_BLUE, 0, cx,
                         kCardTextTop - 2, side);
        const int tx = cx + kGlyph + 6;
        // The name at title weight, in the side's colour, with the whole card to run
        // in — the one line on this screen that is purely who you are.
        drawTextMarquee(fb, tx, kCardTextTop, cRight - tx, mine->displayName, side,
                        beat_, /*scroll=*/true, FontFace::Bold);
        // The side spelled out beside its glyph (never the tint alone), against what
        // the Exploit meters.
        char tag[16];
        crewExploitLabel(tag, sizeof(tag), mine->exploit.kind, mine->exploit.magnitude);
        drawPairIn(fb, tx, kCardTextTop + kLineH, cRight, crewTeamName(mine->team), side,
                   tag, palColor(Pal::ACCENT), beat_, /*scroll=*/false);
        // ...and the Exploit you actually carry into a fight, which is what makes the
        // card worth its space rather than a restatement of the header.
        drawTextMarquee(fb, cx, kCardTextTop + kLineH * 2, cRight - cx,
                        mine->exploit.name, palColor(Pal::INK), beat_, /*scroll=*/false);
    } else {
        // The same three lines, so the two side rows never move under the cursor when
        // membership changes — and the empty state says what to do about it, in the
        // order it has to be done.
        drawText(fb, cx, kCardTextTop, "UNAFFILIATED", palColor(Pal::INK_DIM), 1,
                 FontFace::Bold);
        drawText(fb, cx, kCardTextTop + kLineH, "no crew, no exploit",
                 palColor(Pal::INK_DIM));
        drawText(fb, cx, kCardTextTop + kLineH * 2,
                 hasHomeNetwork() ? "PICK A SIDE BELOW" : "SET A HOME NET FIRST",
                 hasHomeNetwork() ? palColor(Pal::INK) : palColor(Pal::WARN));
    }

    // The two sides. A fixed pair rather than a loop over what the roster happens to
    // contain: an empty side still has to be reachable, or the first crew added to it
    // would have nowhere to appear.
    for (int i = 0; i < 2; ++i) {
        const CrewTeam team = i == 0 ? CrewTeam::Red : CrewTeam::Blue;
        const bool red = team == CrewTeam::Red;
        const Rgb565 side = teamColor(red);
        const int y = kSideTop + i * kSideH;
        const bool sel = crewHubRow_ == i + 1;
        if (sel) drawRowCursor(fb, 2, y + 5, palColor(Pal::ACCENT));
        drawSpriteTinted(fb, red ? ASSET_ICON_TEAM_RED : ASSET_ICON_TEAM_BLUE, 0,
                         kMargin + 8, y, side);
        const int nameX = kMargin + 8 + kGlyph + 8;
        // Title weight for the side, because on this screen the side IS the heading —
        // the row under it is the caption.
        drawText(fb, nameX, y + 2, crewTeamName(team), side, 1, FontFace::Bold);
        char sub[32];
        const int n = crewTeamCount(team);
        std::snprintf(sub, sizeof(sub), "%s - %d CREW%s", crewTeamDoctrine(team), n,
                      n == 1 ? "" : "S");
        drawText(fb, nameX, y + 2 + kLineH, sub, palColor(Pal::INK_DIM));
        // Where your allegiance sits, said on the row you'd have to open to change it.
        if (mine && mine->team == team)
            drawText(fb, kActiveW - kMargin - textWidth("YOURS"), y + 2, "YOURS", side);
    }

    fb.fillRect(0, kActiveH - 26, kActiveW, 1, palColor(Pal::TRACK));
    drawText(fb, kMargin, kActiveH - 20, "A NEXT", palColor(Pal::INK_DIM));
    const char* verb = homeSel ? "B PICK NET" : "B OPEN";
    drawText(fb, kActiveW - kMargin - textWidth(verb), kActiveH - 20, verb,
             palColor(Pal::ACCENT));
}

// --- Home-network picker (the modal over the Hub) ------------------------------

void Game::drawCrewNetPicker(Framebuffer& fb) const {
    CrewNetRow rows[kNetMaxRows];
    const int n = crewNetworkRows(rows, kNetMaxRows);
    // The radio has exactly ONE owner (platform RadioArbiter, priority AP >
    // CAPTURE > SCAN), so name whoever actually holds it rather than claiming
    // SCANNING unconditionally: an empty list means something completely
    // different when the 'Pedia AP has the radio than when nothing is in range.
    // Doubles as the notice that entering this screen arms the scan by itself, so
    // a player who deliberately keeps AUDIT off isn't surprised by a live radio.
    const bool starved = apEnabled_ || auditCapture_.capturing();
    const char* radioState = apEnabled_                 ? "AP HAS RADIO"
                             : auditCapture_.capturing() ? "CAPTURE HAS RADIO"
                                                         : "SCANNING";
    // Alone on the context line: "CAPTURE HAS RADIO" is already 17 characters, and the
    // header band above has named the screen, so anything sharing this row overprints
    // it in exactly the case the message matters most.
    drawText(fb, kActiveW - kMargin - textWidth(radioState), kContextY, radioState,
             starved ? palColor(Pal::WARN) : palColor(Pal::INK_DIM));
    fb.fillRect(0, kContextRule, kActiveW, 1, palColor(Pal::TRACK));

    if (n == 0) {
        // Empty for two very different reasons — say which, so a starved radio
        // never reads as "there is nothing around me". Words, never colour alone.
        drawText(fb, kMargin, 56, starved ? "SCAN CANT RUN" : "NO NETWORKS IN RANGE",
                 palColor(Pal::INK_DIM));
        drawText(fb, kMargin, 56 + kLineH,
                 apEnabled_                  ? "TURN OFF AP IN CFG"
                 : auditCapture_.capturing() ? "CAPTURE IN PROGRESS"
                                             : "LISTENING...",
                 palColor(Pal::INK_DIM));
    } else {
        int sel = crewNetRow_;
        if (sel >= n) sel = 0;
        int scrollTop = 0;
        if (n > kNetVisibleRows) {
            if (sel >= kNetVisibleRows) scrollTop = sel - kNetVisibleRows + 1;
            scrollTop = std::min(scrollTop, n - kNetVisibleRows);
        }
        for (int v = 0; v < kNetVisibleRows && scrollTop + v < n; ++v) {
            const CrewNetRow& row = rows[scrollTop + v];
            const int rowY = 52 + v * kNetRowPitch;
            const bool s = scrollTop + v == sel;
            if (s) drawRowCursor(fb, 2, rowY + 1, palColor(Pal::ACCENT));
            // Sighting count doubles as the "is this really home?" evidence, and is
            // the grayscale-safe ordering cue for the sort. A network in range that
            // no walk has resolved yet has no tally — say NEW, not a hollow "x0".
            char seen[16];
            if (row.count == 0) std::snprintf(seen, sizeof(seen), "NEW");
            else std::snprintf(seen, sizeof(seen), "x%u",
                               static_cast<unsigned>(row.count));
            drawLabelValue(fb, kMargin + 10, rowY, row.name,
                           s ? palColor(Pal::INK) : palColor(Pal::INK_DIM), seen,
                           palColor(Pal::INK_DIM), beat_, s);
            // No LIVE tag: every row on this screen is in range by construction, so
            // the tag would mark nothing. HOME still needs saying — it's the one
            // row whose meaning differs. Word, never colour alone.
            if (row.key == homeNetworkKey_)
                drawText(fb, kMargin + 10, rowY + kFontH + 1, "HOME",
                         palColor(Pal::ACCENT));
        }
        drawScrollbar(fb, 52, kNetVisibleRows * kNetRowPitch, n, scrollTop,
                      kNetVisibleRows);
    }

    fb.fillRect(0, kActiveH - 26, kActiveW, 1, palColor(Pal::TRACK));
    drawText(fb, kMargin, kActiveH - 20, "A NEXT", palColor(Pal::INK_DIM));
    drawText(fb, kActiveW - kMargin - textWidth("B SET  C BACK"), kActiveH - 20,
             "B SET  C BACK",
             n > 0 ? palColor(Pal::ACCENT) : palColor(Pal::INK_DIM));
}

// --- Team roster ---------------------------------------------------------------

void Game::drawCrewTeam(Framebuffer& fb) const {
    const bool red = crewTeam_ == CrewTeam::Red;
    const Rgb565 side = teamColor(red);
    const int n = crewTeamCount(crewTeam_);
    if (n == 0) {
        drawText(fb, kMargin, 56, "NO CREWS ON THIS SIDE", palColor(Pal::INK_DIM));
        drawText(fb, kMargin, 56 + kLineH, "YET.", palColor(Pal::INK_DIM));
        drawHintBand(fb, "C BACK");
        return;
    }
    int sel = crewTeamRow_;
    if (sel >= n) sel = 0;
    int scrollTop = 0;
    if (n > kTeamVisibleRows)
        scrollTop = std::min(std::max(0, sel - kTeamVisibleRows + 1),
                             n - kTeamVisibleRows);

    for (int v = 0; v < kTeamVisibleRows && scrollTop + v < n; ++v) {
        const int i = crewByTeam(crewTeam_, scrollTop + v);
        const CrewDef& c = kCrews[i];
        const int y = kRowTop + v * kTeamRowH;
        const bool s = scrollTop + v == sel;
        const bool joined = crewIndex_ == i;
        if (s) drawRowCursor(fb, 2, y + 1, palColor(Pal::ACCENT));
        // One job per line, each with the full row to do it in — no two things on this
        // screen compete for the same pixels, which is what keeps a long crew name from
        // clipping.
        const int tx = kMargin + 10;
        const int w = kActiveW - kMargin - tx;
        // 1 — the name, at title weight and in the side's colour while focused.
        drawTextMarquee(fb, tx, y, w, c.displayName, s ? side : palColor(Pal::INK),
                        beat_, s, FontFace::Bold);
        // 2 — the motto, which is the crew's whole character and the reason to read
        // on. WRAPPED, not marqueed: a marquee only ever ran for the focused row, so
        // an unselected crew with a long motto (e.g. "fail to plan; plan to fail")
        // was silently cut mid-word with no indication there was more. Wrapping
        // shows the whole thing on every row, focused or not.
        drawTextWrapped(fb, tx, y + kLineH, w, c.tagline, palColor(Pal::INK_DIM),
                        kLineH, 2);
        // 3 — the Exploit's mechanic word, and where you already belong. The tag is the
        // comparable thing here: a column of NEGATE / STAT LOCK / RALLY is what makes a
        // side a choice you can read down rather than three pages to visit in turn.
        char tag[16];
        crewExploitLabel(tag, sizeof(tag), c.exploit.kind, c.exploit.magnitude);
        drawText(fb, tx, y + kLineH * 3, tag, palColor(Pal::INK));
        if (joined)
            drawText(fb, kActiveW - kMargin - textWidth("JOINED"), y + kLineH * 3,
                     "JOINED", palColor(Pal::ACCENT));
    }
    if (n > kTeamVisibleRows)
        drawScrollbar(fb, kRowTop, kTeamVisibleRows * kTeamRowH, n, scrollTop,
                      kTeamVisibleRows);
    drawHintBand(fb, "A NEXT   B VIEW   C BACK");
}

// --- Crew detail ---------------------------------------------------------------

void Game::drawCrewDetail(Framebuffer& fb) const {
    const CrewDef* c = crew(crewDetail_);
    if (!c) return;
    const bool red = c->team == CrewTeam::Red;
    const Rgb565 side = teamColor(red);
    const bool joined = crewIndex_ == crewDetail_;

    // Glyph + motto. The name is already the header band's title, so this block is the
    // crew's voice rather than a repeat of its label — and it WRAPS rather than
    // marqueeing, because a motto the reader has to wait out isn't one.
    drawSpriteTinted(fb, red ? ASSET_ICON_TEAM_RED : ASSET_ICON_TEAM_BLUE, 0, kMargin,
                     kDetailMottoY - 3, side);
    const int mottoX = kMargin + kGlyph + 8;
    drawTextWrapped(fb, mottoX, kDetailMottoY, kActiveW - kMargin - mottoX, c->tagline,
                    palColor(Pal::INK_DIM), kLineH, kDetailMottoLines);
    fb.fillRect(kMargin, kDetailRule, kActiveW - 2 * kMargin, 1, palColor(Pal::TRACK));

    // The Exploit, given the page's middle: what it is called, what it meters, and
    // what it does. The heading line carries the derived "<TAG> xN" so the NAME gets
    // the next line whole — the longest of them ("MALBEAST IN THE MIDDLE") is 22
    // characters, and sharing a line with its own meter is what cut it. Name and
    // meter are the bounded half and are never cut; the description under them is the
    // elastic half and pages on A.
    char tag[16];
    crewExploitLabel(tag, sizeof(tag), c->exploit.kind, c->exploit.magnitude);
    drawLabelValue(fb, kMargin, kDetailExploitY, "EXPLOIT", palColor(Pal::INK_DIM), tag,
                   palColor(Pal::INK), beat_, /*scroll=*/false);
    drawTextMarquee(fb, kMargin, kDetailExploitY + kLineH, kActiveW - 2 * kMargin,
                    c->exploit.name, palColor(Pal::ACCENT), beat_, /*scroll=*/true,
                    FontFace::Bold);
    const EffectText prose = effectText(c->exploit);
    const int lines = textWrapLines(prose.c_str(), kActiveW - 2 * kMargin);
    drawTextWrapped(fb, kMargin, kDetailProseTop, kActiveW - 2 * kMargin, prose.c_str(),
                    palColor(Pal::INK), kLineH, kDetailProseLines, crewProseScroll_);

    // The one verb, and the gate on it, in words. A crew member is always somebody's
    // defender, so a player without a home network is told the precondition rather
    // than offered a button that would silently do nothing.
    if (joined) {
        drawRowCursor(fb, kMargin, kDetailVerbY, palColor(Pal::ACCENT));
        drawText(fb, kMargin + 10, kDetailVerbY, "JOINED - B TO LEAVE",
                 palColor(Pal::ACCENT));
    } else if (hasHomeNetwork()) {
        drawRowCursor(fb, kMargin, kDetailVerbY, palColor(Pal::ACCENT));
        drawText(fb, kMargin + 10, kDetailVerbY, "ENLIST", palColor(Pal::ACCENT));
    } else {
        drawText(fb, kMargin, kDetailVerbY, "NEEDS A HOME NET", palColor(Pal::WARN));
    }

    const bool pages = lines > kDetailProseLines;
    const char* hint = joined ? (pages ? "A MORE  B LEAVE  C BACK" : "B LEAVE   C BACK")
                      : hasHomeNetwork()
                          ? (pages ? "A MORE  B ENLIST  C BACK" : "B ENLIST   C BACK")
                          : (pages ? "A MORE   C BACK" : "C BACK");
    drawHintBand(fb, hint);
}

}  // namespace mal
