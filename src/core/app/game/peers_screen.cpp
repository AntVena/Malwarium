#include "core/app/game/peers_screen.h"

#include <algorithm>
#include <cstdio>

#include "core/app/game_internal.h"   // kPeerMaxRows, drawScrollbar
#include "core/render/canvas.h"
#include "core/render/font.h"
#include "core/render/framebuffer.h"
#include "core/render/palette.h"
#include "core/render/sprite.h"
#include "core/ui/layout.h"
#include "core/ui/widgets.h"
#include "generated/assets.h"

namespace mal {

namespace {
// A peer block is tag / pet / crew: three of them clear the body under the header
// rule, leaving room for the hint band.
constexpr int kPeerBlockH = kLineH * 3 + 6;
constexpr int kPeerVisibleRows = 3;
}  // namespace

void PeersScreen::stepList(const PeersHost& host, int dir) {
    PeerRow rows[kPeerMaxRows];
    row_ = stepRow(row_, host.peerRows(rows, kPeerMaxRows), dir);
}

void PeersScreen::onButton(PeersHost& host, const ButtonEvent& ev) {
    if (ev.button == Button::C) { host.leaveToCarousel(); return; }
    PeerRow rows[kPeerMaxRows];
    const int n = host.peerRows(rows, kPeerMaxRows);
    if (n == 0) return;                        // empty state: A inert
    if (row_ >= n) row_ = 0;                   // the roster shrank under the cursor
    if (ev.button == Button::A) row_ = stepRow(row_, n, +1);
}

void PeersScreen::draw(Framebuffer& fb, const PeersHost& host) const {
    drawHeaderBand(fb, "PEERS");
    PeerRow rows[kPeerMaxRows];
    const int n = host.peerRows(rows, kPeerMaxRows);

    drawText(fb, kMargin, kContextY, "PEERS MET", palColor(Pal::INK));

    // Name whoever holds the radio. An empty list means something different when the
    // 'Pedia AP has the radio than when nobody is around. Doubles as the notice that
    // this screen announces on its own, so an operator who deliberately keeps LINK
    // off is never surprised into broadcasting.
    const bool starved = host.apEnabled() || host.captureHoldsRadio();
    const char* radioState = host.apEnabled()                  ? "AP HAS RADIO"
                             : host.captureHoldsRadio() ? "CAPTURE HAS RADIO"
                                                         : "LINKING";
    drawText(fb, kActiveW - kMargin - textWidth(radioState), kContextY, radioState,
             starved ? palColor(Pal::WARN) : palColor(Pal::INK_DIM));
    fb.fillRect(0, kContextRule, kActiveW, 1, palColor(Pal::TRACK));

    if (n == 0) {
        // Empty for three very different reasons: say which. Words, never colour.
        drawText(fb, kMargin, 54,
                 starved ? "LINK CANT RUN" : "NOBODY MET YET", palColor(Pal::INK_DIM));
        drawText(fb, kMargin, 54 + kLineH,
                 host.apEnabled()                  ? "TURN OFF AP IN CFG"
                 : host.captureHoldsRadio() ? "CAPTURE IN PROGRESS"
                                             : "LISTENING...",
                 palColor(Pal::INK_DIM));
    } else {
        int sel = row_;
        if (sel >= n) sel = 0;
        int scrollTop = 0;
        if (n > kPeerVisibleRows) {
            if (sel >= kPeerVisibleRows) scrollTop = sel - kPeerVisibleRows + 1;
            scrollTop = std::min(scrollTop, n - kPeerVisibleRows);
        }
        for (int v = 0; v < kPeerVisibleRows && scrollTop + v < n; ++v) {
            const PeerRow& row = rows[scrollTop + v];
            const int rowY = 52 + v * kPeerBlockH;
            const bool s = scrollTop + v == sel;
            if (s) drawRowCursor(fb, 2, rowY + 1, palColor(Pal::ACCENT));

            drawText(fb, kMargin, rowY, row.tag[0] ? row.tag : "UNKNOWN OP",
                     s ? palColor(Pal::INK) : palColor(Pal::INK_DIM));

            // LIVE vs. how many times we've met is the one thing that must survive
            // grayscale, so it's a word and a count, never a colour.
            char state[16];
            if (row.live) std::snprintf(state, sizeof(state), "LIVE");
            else std::snprintf(state, sizeof(state), "x%u",
                               static_cast<unsigned>(row.timesMet));
            drawText(fb, kActiveW - kMargin - textWidth(state), rowY, state,
                     row.live ? palColor(Pal::ACCENT) : palColor(Pal::INK_DIM));

            // What they're raising. An operator between pets broadcasts nothing here.
            char pet[40];
            if (row.petName[0])
                std::snprintf(pet, sizeof(pet), "%s  %s", row.petName,
                              stageName(static_cast<Stage>(row.stage)));
            else
                std::snprintf(pet, sizeof(pet), "NO PET");
            drawText(fb, kMargin + 6, rowY + kLineH, pet, palColor(Pal::INK_DIM));

            // Crew + rank. Team is dual-coded by its glyph beside the name, never by
            // the colour of it.
            char crew[40];
            std::snprintf(crew, sizeof(crew), "%s  RANK %u",
                          row.crewName[0] ? row.crewName : "NO CREW",
                          static_cast<unsigned>(row.rank));
            drawText(fb, kMargin + 6, rowY + kLineH * 2, crew, palColor(Pal::INK_DIM));
            if (row.crewName[0])
                drawSprite(fb,
                           row.crewRed ? ASSET_ICON_TEAM_RED : ASSET_ICON_TEAM_BLUE,
                           0, kActiveW - kMargin - 20, rowY + kLineH * 2 - 5);
        }
        drawScrollbar(fb, 52, kPeerVisibleRows * kPeerBlockH, n, scrollTop,
                      kPeerVisibleRows);
    }

    // Ambient discovery borrows the audit scan's radio window, so with the scan off
    // this screen is the ONLY place meetings happen. Saying so keeps a
    // LINK-on/AUDIT-off device from looking broken: the toggle works but has no
    // ambient airtime.
    if (host.linkAmbientStarved())
        drawText(fb, kMargin, kActiveH - 34, "AMBIENT NEEDS AUDIT SCAN",
                 palColor(Pal::WARN));

    drawHackerHint(fb, "A NEXT", "C BACK");
}

}  // namespace mal
