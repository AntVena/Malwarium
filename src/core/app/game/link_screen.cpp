#include "core/app/game/link_screen.h"

#include <algorithm>
#include <cstdio>

#include "core/app/game/peers_screen.h"   // PeerRow
#include "core/model/pet.h"
#include "core/model/pvp_session.h"
#include "core/render/canvas.h"
#include "core/render/font.h"
#include "core/render/framebuffer.h"
#include "core/render/palette.h"
#include "core/render/sprite.h"
#include "core/ui/layout.h"
#include "core/ui/theme.h"
#include "core/ui/widgets.h"
#include "generated/assets.h"

namespace mal {

namespace {
// A challengeable row is tag, pet and crew: three lines, so the list clears the header
// rule with room for the status band and the hint band underneath.
constexpr int kBlockH = kLineH * 3 + 6;
constexpr int kVisibleBlocks = 3;
constexpr int kMaxRows = 32;

// The live operators, who are the ones a duel can be offered to: a remembered operator
// cannot be challenged, since a duel needs both devices powered, present and on the same
// rendezvous channel. peerRows lists them first.
int challengeable(const LinkHost& host, PeerRow* rows) {
    const int n = host.peerRows(rows, kMaxRows);
    int live = 0;
    while (live < n && rows[live].live) ++live;
    return live;
}
}  // namespace

void LinkScreen::onButton(LinkHost& host, const ButtonEvent& ev) {
    const PvpSession* s = host.pvpSession();
    switch (s ? s->phase : PvpPhase::Idle) {
        case PvpPhase::Invited:
            // The consent gate. B accepts, C refuses; A is inert so a stray cycle press
            // can never answer for the human.
            if (ev.button == Button::B) host.pvpAcceptChallenge();
            else if (ev.button == Button::C) host.pvpDeclineChallenge();
            return;
        case PvpPhase::Inviting:
        case PvpPhase::Arming:
            if (ev.button == Button::C) host.pvpCancel();   // give up waiting
            return;
        case PvpPhase::Result:
            host.clearPvpVerdict();                         // any button
            return;
        case PvpPhase::Fighting:
            return;                                         // the combat screen owns input
        case PvpPhase::Idle:
            onList(host, ev);
            return;
    }
}

void LinkScreen::onList(LinkHost& host, const ButtonEvent& ev) {
    if (ev.button == Button::C) { host.leaveToCarousel(); return; }
    PeerRow rows[kMaxRows];
    const int n = challengeable(host, rows);
    if (n == 0) return;                                     // empty state: A/B inert
    if (row_ >= n) row_ = 0;
    if (ev.button == Button::A) row_ = stepRow(row_, n, +1);
    else if (ev.button == Button::B) host.pvpChallenge(rows[row_].key);
}

void LinkScreen::draw(Framebuffer& fb, const LinkHost& host) const {
    drawHeaderBand(fb, "LINK");
    drawText(fb, kMargin, kContextY, "LINK", palColor(Pal::INK));

    // Name whoever actually holds the radio rather than implying LINK has it. An empty
    // list means something entirely different when the 'Pedia AP owns the radio than
    // when nobody is around.
    const bool starved = host.apEnabled() || host.captureHoldsRadio();
    const char* radioState = host.apEnabled()         ? "AP HAS RADIO"
                             : host.captureHoldsRadio() ? "CAPTURE HAS RADIO"
                                                        : "LINKING";
    drawText(fb, kActiveW - kMargin - textWidth(radioState), kContextY, radioState,
             starved ? palColor(Pal::WARN) : palColor(Pal::INK_DIM));
    fb.fillRect(0, kContextRule, kActiveW, 1, palColor(Pal::TRACK));

    // Every open session takes over the body: what the operator needs to know is the
    // state of the one session, not a list they cannot act on.
    if (const PvpSession* s = host.pvpSession()) drawSession(fb, *s);
    else drawList(fb, host);
}

void LinkScreen::drawSession(Framebuffer& fb, const PvpSession& s) const {
    const char* head = "";
    const char* sub = "";
    const char* secondary = "";
    const char* hint = "";
    switch (s.phase) {
        case PvpPhase::Inviting:
            head = "CHALLENGE SENT"; sub = "WAITING FOR ANSWER"; hint = "C CANCEL";
            break;
        case PvpPhase::Invited:
            head = "CHALLENGES YOU"; sub = "NO STAKES  NO EXPLOITS";
            secondary = "C REFUSE"; hint = "B FIGHT";
            break;
        case PvpPhase::Arming:
            head = "SYNCING FIGHT"; sub = "AGREEING ON THE SEED"; hint = "C CANCEL";
            break;
        case PvpPhase::Fighting:
            head = "DUEL IN PROGRESS";
            break;
        case PvpPhase::Result:
            // The verdict is a WORD, never a colour: it has to survive grayscale.
            head = s.won ? "YOU WON" : "YOU LOST"; hint = "ANY KEY";
            break;
        case PvpPhase::Idle:
            break;
    }
    drawText(fb, kMargin, 56, head, palColor(Pal::INK));
    drawText(fb, kMargin, 56 + kLineH, s.peerTag, palColor(Pal::ACCENT));
    if (sub[0]) drawText(fb, kMargin, 56 + kLineH * 2, sub, palColor(Pal::INK_DIM));
    drawHackerHint(fb, secondary, hint);
}

void LinkScreen::drawList(Framebuffer& fb, const LinkHost& host) const {
    // Whatever went wrong last time, then the challengeable list.
    const char* status = host.pvpStatusText();
    if (status[0]) drawText(fb, kMargin, 50, status, palColor(Pal::WARN));
    const int listTop = status[0] ? 50 + kLineH : 52;

    PeerRow rows[kMaxRows];
    const int n = challengeable(host, rows);
    if (n == 0) {
        // Three very different reasons to be empty: say which. Words, never colour.
        const bool starved = host.apEnabled() || host.captureHoldsRadio();
        drawText(fb, kMargin, listTop, starved ? "LINK CANT RUN" : "NOBODY IN RANGE",
                 palColor(Pal::INK_DIM));
        drawText(fb, kMargin, listTop + kLineH,
                 host.apEnabled()            ? "TURN OFF AP IN CFG"
                 : host.captureHoldsRadio()  ? "CAPTURE IN PROGRESS"
                 : !host.petRecord().creature ? "AND NO PET TO FIGHT"
                                              : "LISTENING...",
                 palColor(Pal::INK_DIM));
    } else {
        const int sel = row_ < n ? row_ : 0;
        int scrollTop = 0;
        if (n > kVisibleBlocks) {
            if (sel >= kVisibleBlocks) scrollTop = sel - kVisibleBlocks + 1;
            scrollTop = std::min(scrollTop, n - kVisibleBlocks);
        }
        for (int v = 0; v < kVisibleBlocks && scrollTop + v < n; ++v) {
            const PeerRow& r = rows[scrollTop + v];
            const int rowY = listTop + v * kBlockH;
            const bool s = scrollTop + v == sel;
            if (s) drawRowCursor(fb, 2, rowY + 1, palColor(Pal::ACCENT));

            drawText(fb, kMargin, rowY, r.tag[0] ? r.tag : "UNKNOWN OP",
                     s ? palColor(Pal::INK) : palColor(Pal::INK_DIM));

            char pet[40];
            if (r.petName[0])
                std::snprintf(pet, sizeof(pet), "%s  %s", r.petName,
                              stageName(static_cast<Stage>(r.stage)));
            else
                std::snprintf(pet, sizeof(pet), "NO PET");
            drawText(fb, kMargin + 6, rowY + kLineH, pet, palColor(Pal::INK_DIM));

            // Who you would be fighting for, the thing that makes a duel a rivalry rather
            // than a stat check. The CREW screen's three channels (the name, the side's
            // glyph and its colour), so an unenlisted peer reads as NO CREW.
            const int crewY = rowY + kLineH * 2;
            if (r.crewName[0]) {
                const Rgb565 side = teamColor(r.crewRed);
                drawSpriteTinted(fb, r.crewRed ? ASSET_ICON_TEAM_RED : ASSET_ICON_TEAM_BLUE,
                                 0, kMargin + 6, crewY - 5, side);
                drawText(fb, kMargin + 28, crewY, r.crewName, side);
            } else {
                drawText(fb, kMargin + 6, crewY, "NO CREW", palColor(Pal::INK_DIM));
            }
        }
    }

    // Ambient discovery borrows the audit scan's radio window, so finding nobody with the
    // scan off is the toggle working, not a fault. Saying so keeps a LINK-on/AUDIT-off
    // device from looking broken.
    if (host.linkAmbientStarved())
        drawText(fb, kMargin, kActiveH - 34, "AMBIENT NEEDS AUDIT SCAN", palColor(Pal::WARN));

    // FIGHT is what this screen is for, so it takes the accent verb (VISUAL_LANGUAGE
    // §4.2) whenever there is someone to fight; with nobody listed, BACK is all there is.
    drawHackerHint(fb, n > 0 ? "A NEXT  C BACK" : "A NEXT", n > 0 ? "B FIGHT" : "C BACK");
}

}  // namespace mal
