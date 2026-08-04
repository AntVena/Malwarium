#include "core/app/game.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "core/app/game_internal.h"   // drawScrollbar — shared with game_peers.cpp
#include "core/content/content_crews.h"
#include "core/render/canvas.h"
#include "core/render/font5x7.h"
#include "core/render/palette.h"
#include "core/render/sprite.h"
#include "core/ui/theme.h"
#include "core/ui/widgets.h"
#include "generated/assets.h"

// game_crew.cpp — the Hacker-face CREW slot: enlist in a crew (content_crews.h) and
// designate the HOME NETWORK that membership hangs off.
//
// One list, two bands: row 0 is HOME NET, rows 1.. are the crews. B on row 0 opens the
// home-network picker OVER the same body (crewNetPicker_) rather than a new Nav state —
// it's a modal sub-mode of one screen, and C steps back out of it before it exits the
// screen. The picker answers exactly one question — "which of the networks I can hear
// right now am I claiming?" — so its rows are the live in-range snapshot the passive
// scan refreshes (Game::registerNetwork -> visibleNetworks_), and opening the screen
// arms that scan on its own (Game::radioScanWanted). The SD-backed NetworkLedger
// (core/net/network_ledger.h) supplies each row's walked-in tally as evidence for the
// choice; it never contributes a row of its own.
//
// Membership's payoff in play is the crew's signature Exploit: crewExploitOption()
// hands it to Combat as the A+C picker's last row (Game::onCombat → openOverride).

namespace mal {

namespace {
constexpr int kMargin = 10;
constexpr int kLineH = kFontH + 5;
// The picker window. Each row is a name line plus room for the HOME/LIVE sub-tags
// beneath it; six of them clear the body between the header rule and the hint band.
constexpr int kNetRowPitch = kFontH + 14;
constexpr int kNetVisibleRows = 6;
// Sized to hold a full in-range snapshot, so the window on this list is the screen's
// six rows and never a silent truncation of what the radio heard.
constexpr int kNetMaxRows = kNetVisibleCap;
// A crew block is name / motto / Exploit / status; three of them clear the body under
// the HOME NET row. Both lists window rather than assuming their table fits on screen.
constexpr int kCrewBlockH = kLineH * 4 + 6;
constexpr int kCrewVisibleRows = 3;
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
    // Most-walked first — the same signal the old most-seen inference used, now
    // offered as a choice rather than a guess. Stable within a tie, so the order stays
    // steady between repaints instead of shuffling under the cursor.
    std::stable_sort(rows, rows + n, [](const CrewNetRow& a, const CrewNetRow& b) {
        return a.count > b.count;
    });
    return n;
}

// --- Input ------------------------------------------------------------------

void Game::onHackerCrew(const ButtonEvent& ev) {
    if (crewNetPicker_) {
        CrewNetRow rows[kNetMaxRows];
        const int n = crewNetworkRows(rows, kNetMaxRows);
        if (ev.button == Button::C) { crewNetPicker_ = false; return; }
        if (n == 0) return;                            // empty state: A/B inert
        if (crewNetRow_ >= n) crewNetRow_ = 0;
        if (ev.button == Button::A) {
            crewNetRow_ = (crewNetRow_ + 1) % n;
        } else if (ev.button == Button::B) {
            setHomeNetwork(rows[crewNetRow_].key, rows[crewNetRow_].name);
            crewNetPicker_ = false;
        }
        return;
    }

    if (ev.button == Button::C) { nav_ = Nav::Cursor; return; }
    if (ev.button == Button::A) {
        crewRow_ = (crewRow_ + 1) % crewListRows();
    } else if (ev.button == Button::B) {
        if (crewRow_ == 0) {                           // HOME NET row
            crewNetPicker_ = true;
            crewNetRow_ = 0;
            dirty_ = true;
        } else {                                       // a crew row: toggle membership
            const int idx = crewRow_ - 1;
            if (crewIndex_ == idx) leaveCrew();
            else joinCrew(idx);                         // no-op without a home network
        }
    }
}

// --- Rendering ---------------------------------------------------------------

void Game::drawHackerCrew(Framebuffer& fb) const {
    if (crewNetPicker_) {
        CrewNetRow rows[kNetMaxRows];
        const int n = crewNetworkRows(rows, kNetMaxRows);
        drawText(fb, kMargin, 28, "PICK HOME NET", palColor(Pal::INK));
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
        drawText(fb, kActiveW - kMargin - textWidth(radioState), 28, radioState,
                 starved ? palColor(Pal::WARN) : palColor(Pal::INK_DIM));

        if (n == 0) {
            // Empty for two very different reasons — say which, so a starved radio
            // never reads as "there is nothing around me". Words, never colour alone.
            drawText(fb, kMargin, 48,
                     starved ? "SCAN CANT RUN" : "NO NETWORKS IN RANGE",
                     palColor(Pal::INK_DIM));
            drawText(fb, kMargin, 48 + kLineH,
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
                const int rowY = 46 + v * kNetRowPitch;
                const bool s = scrollTop + v == sel;
                if (s) drawRowCursor(fb, 2, rowY + 1, palColor(Pal::ACCENT));
                drawText(fb, kMargin, rowY, row.name,
                         s ? palColor(Pal::INK) : palColor(Pal::INK_DIM));
                // Sighting count doubles as the "is this really home?" evidence, and is
                // the grayscale-safe ordering cue for the sort. A network in range that
                // no walk has resolved yet has no tally — say NEW, not a hollow "x0".
                char seen[16];
                if (row.count == 0) std::snprintf(seen, sizeof(seen), "NEW");
                else std::snprintf(seen, sizeof(seen), "x%u",
                                   static_cast<unsigned>(row.count));
                drawText(fb, kActiveW - kMargin - textWidth(seen), rowY, seen,
                         palColor(Pal::INK_DIM));
                // No LIVE tag: every row on this screen is in range by construction, so
                // the tag would mark nothing. HOME still needs saying — it's the one
                // row whose meaning differs. Word, never colour alone.
                if (row.key == homeNetworkKey_)
                    drawText(fb, kMargin + 8, rowY + kFontH + 1, "HOME",
                             palColor(Pal::ACCENT));
            }
            drawScrollbar(fb, 46, kNetVisibleRows * kNetRowPitch, n, scrollTop,
                          kNetVisibleRows);
        }

        fb.fillRect(0, kActiveH - 26, kActiveW, 1, palColor(Pal::TRACK));
        drawText(fb, kMargin, kActiveH - 20, "A NEXT", palColor(Pal::INK_DIM));
        drawText(fb, kActiveW - kMargin - textWidth("B SET  C BACK"), kActiveH - 20,
                 "B SET  C BACK",
                 n > 0 ? palColor(Pal::ACCENT) : palColor(Pal::INK_DIM));
        return;
    }

    // HOME NET row — the gate everything below reads from, so it leads.
    const bool homeSel = crewRow_ == 0;
    if (homeSel) drawRowCursor(fb, 2, 29, palColor(Pal::ACCENT));
    drawText(fb, kMargin, 28, "HOME NET",
             homeSel ? palColor(Pal::INK) : palColor(Pal::INK_DIM));
    const char* homeVal = hasHomeNetwork() ? homeNetworkName_ : "NOT SET";
    drawText(fb, kActiveW - kMargin - textWidth(homeVal), 28, homeVal,
             hasHomeNetwork() ? palColor(Pal::INK) : palColor(Pal::WARN));
    fb.fillRect(0, 44, kActiveW, 1, palColor(Pal::TRACK));

    // One block per crew: name + team, motto, the Exploit it grants, and the state.
    // Windowed (kCrewVisibleRows) — the roster is a content table free to grow, so the
    // screen scrolls rather than assuming every crew fits.
    const int selCrew = crewRow_ - 1;                    // -1 while HOME NET is focused
    int scrollTop = 0;
    if (kCrewCount > kCrewVisibleRows && selCrew >= kCrewVisibleRows)
        scrollTop = std::min(selCrew - kCrewVisibleRows + 1,
                             kCrewCount - kCrewVisibleRows);
    for (int v = 0; v < kCrewVisibleRows && scrollTop + v < kCrewCount; ++v) {
        const int i = scrollTop + v;
        const CrewDef& c = kCrews[i];
        const int rowY = 52 + v * kCrewBlockH;
        const bool sel = selCrew == i;
        const bool joined = crewIndex_ == i;
        if (sel) drawRowCursor(fb, 2, rowY + 1, palColor(Pal::ACCENT));

        drawText(fb, kMargin, rowY, c.displayName,
                 sel ? palColor(Pal::INK) : palColor(Pal::INK_DIM));
        // Team is triple-coded: the word, its glyph, and the side's own colour —
        // and in that order of authority. The tint (core/ui/theme.h) is the last
        // channel added and the only one that can be lost, so desaturating this
        // row still leaves "BLUE" spelled out beside a glyph shaped only one side
        // uses.
        const bool red = c.team == CrewTeam::Red;
        const Rgb565 side = teamColor(red);
        const char* team = red ? "RED" : "BLUE";
        const int teamX = kActiveW - kMargin - textWidth(team);
        drawText(fb, teamX, rowY, team, side);
        drawSpriteTinted(fb, red ? ASSET_ICON_TEAM_RED : ASSET_ICON_TEAM_BLUE,
                         0, teamX - 20, rowY - 5, side);

        drawText(fb, kMargin + 6, rowY + kLineH, c.tagline, palColor(Pal::INK_DIM));

        // The Exploit the crew grants, with its magnitude spelled out — the reason to
        // join reads off the row without entering a fight to find out.
        char exploit[40];
        std::snprintf(exploit, sizeof(exploit), "%s  %s x%d", c.exploit.name,
                      crewExploitTag(c.exploit.kind), c.exploit.magnitude);
        drawText(fb, kMargin + 6, rowY + kLineH * 2, exploit, palColor(Pal::INK_DIM));

        // Status line: the whole join gate in words.
        const char* status = joined              ? "JOINED"
                             : hasHomeNetwork()  ? "READY TO ENLIST"
                                                 : "NEEDS A HOME NET";
        drawText(fb, kMargin + 6, rowY + kLineH * 3, status,
                 joined ? palColor(Pal::ACCENT)
                        : hasHomeNetwork() ? palColor(Pal::INK) : palColor(Pal::WARN));
    }
    if (kCrewCount > kCrewVisibleRows)
        drawScrollbar(fb, 52, kCrewVisibleRows * kCrewBlockH, kCrewCount, scrollTop,
                      kCrewVisibleRows);

    // Hint band. B's verb depends on the focused row, and dims when it can't fire.
    fb.fillRect(0, kActiveH - 26, kActiveW, 1, palColor(Pal::TRACK));
    drawText(fb, kMargin, kActiveH - 20, "A NEXT", palColor(Pal::INK_DIM));
    const bool joinedSel = !homeSel && crewIndex_ == crewRow_ - 1;
    const char* verb = homeSel ? "B PICK NET" : joinedSel ? "B LEAVE" : "B ENLIST";
    const bool armed = homeSel || joinedSel || hasHomeNetwork();
    drawText(fb, kActiveW - kMargin - textWidth(verb), kActiveH - 20, verb,
             armed ? palColor(Pal::ACCENT) : palColor(Pal::INK_DIM));
}

}  // namespace mal
