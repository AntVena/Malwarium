#include "core/ui/maint_screen.h"

#include <cstdio>

#include "tunables.h"
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


// A 1px outline. Local because the Stacker's in-hand run is the only thing that wants
// one — a filled block is a placed block everywhere else, so "outlined" reading as "not
// committed yet" is this screen's own vocabulary, not a general widget.
void strokeRect(Framebuffer& fb, int x, int y, int w, int h, Rgb565 c) {
    fb.fillRect(x, y, w, 1, c);
    fb.fillRect(x, y + h - 1, w, 1, c);
    fb.fillRect(x, y, 1, h, c);
    fb.fillRect(x + w - 1, y, 1, h, c);
}

// AV's status preview: ghost / debuff count / clean.
void avStatus(const PetModel& m, char* out, int n) {
    if (m.hasGhost()) std::snprintf(out, n, "GHOST");
    else if (m.debuffs() > 0) std::snprintf(out, n, "%d DEBUFF", m.debuffs());
    else std::snprintf(out, n, "CLEAN");
}

}  // namespace

void drawMaintList(Framebuffer& fb, const PetModel& m, int cursor, int beat) {
    drawHeaderBand(fb, "MAINT");

    const SpriteData* icons[2] = {&ASSET_ICON_MAINT_DEFRAG, &ASSET_ICON_MAINT_AV};
    const char* labels[2] = {"DEFRAGMENTATION", "ANTIVIRUS"};
    char status[2][16];
    std::snprintf(status[0], sizeof(status[0]), "FRAG %d", m.fragmentation());
    avStatus(m, status[1], sizeof(status[1]));

    for (int i = 0; i < 2; ++i) {
        const int y = kRowTop + i * kRowH;
        if (i == cursor) {
            fb.fillRect(4, y + 2, kActiveW - 8, kRowH - 4, palColor(Pal::TRACK));
            drawRowCursor(fb, 8, y + (kRowH - 7) / 2, palColor(Pal::ACCENT));
        }
        drawSprite(fb, *icons[i], 0, 16, y + (kRowH - kRowIcon) / 2);
        const int textY = y + (kRowH - kFontH) / 2;
        const int statusX = kActiveW - kMargin - textWidth(status[i]);
        drawText(fb, statusX, textY, status[i], palColor(Pal::INK_DIM));
        // The status owns the right end, so the process name yields to it and scrolls
        // when focused (widgets.h) — DEFRAGMENTATION spelled out is the whole point of
        // the row, so it is not the half that gets cut.
        drawTextMarquee(fb, 40, textY, statusX - kMargin - 40, labels[i],
                        palColor(Pal::INK), beat, i == cursor);
    }
}

void drawMaintAction(Framebuffer& fb, MaintKind kind, const PetModel& m,
                     int cost, int walletBits, int variant, int toolCount,
                     int defragCount) {
    char line[40];
    if (kind == MaintKind::Defrag) {
        drawHeaderBand(fb, "DEFRAGMENTATION");
        // The headline effect is the FOCUSED variant's, because the three don't pay the
        // same: two take a fixed bite, and the played one is the only route to a clean
        // disk. Naming the fixed number on a screen where it can be wrong would be worse
        // than naming nothing.
        if (variant == kDefragVariantStacker) {
            drawText(fb, kMargin, 34, "CLEAR THE BOARD: FRAG TO 0.", palColor(Pal::INK));
        } else {
            std::snprintf(line, sizeof(line), "REDUCES FRAGMENTATION BY %d.",
                          kDefragReduction);
            drawText(fb, kMargin, 34, line, palColor(Pal::INK));
        }
        std::snprintf(line, sizeof(line), "CURRENT FRAG: %d", m.fragmentation());
        drawText(fb, kMargin, 50, line, palColor(Pal::INK));
        // this pet's running defrag tally (persists through freeze/thaw),
        // surfaced here and nowhere else.
        std::snprintf(line, sizeof(line), "DEFRAGS DONE: %d", defragCount);
        drawText(fb, kMargin, 62, line, palColor(Pal::INK_DIM));
        // stage-scaled Bits cost + the wallet, both spelled out so the
        // affordability reads in grayscale (COST N B / HAVE N B).
        std::snprintf(line, sizeof(line), "COST %d B   HAVE %d B", cost, walletBits);
        const bool afford = walletBits >= cost;
        drawText(fb, kMargin, 78, line,
                 afford ? palColor(Pal::INK) : palColor(Pal::WARN));
        // The tool count is a WALLET line, not a pick: it belongs beside HAVE %d B at
        // the readout pitch, above the list, rather than trailing the list at the list's
        // own pitch — where a dim unindented row still reads as a fourth thing to land
        // the cursor on.
        std::snprintf(line, sizeof(line), "HELD DEFRAG TOOLS: %d", toolCount);
        drawText(fb, kMargin, 90, line, palColor(Pal::INK_DIM));

        // the three payment VARIANTS as a 3-row pick (A switches, B runs the
        // focused one). QUICK = Bits-only, may fail; TOOL = spend a Defrag Tool for a
        // guaranteed clean; STACKER = the same Bits, then play for it. Grayscale-safe:
        // the cursor marks the focus, and each row spells its terms in words (MAY FAIL /
        // GUARANTEED / NO TOOL / YOUR AIM). The 22px above the first row and the 28px
        // below the last are what fence the pick off from the readouts and the action
        // line — both wider than the 18px the rows keep between themselves.
        const int rowY[3] = {112, 130, 148};
        for (int i = 0; i < 3; ++i) {
            const bool focus = variant == i;
            if (focus) drawRowCursor(fb, kMargin, rowY[i], palColor(Pal::ACCENT));
            const Rgb565 col = focus ? palColor(Pal::ACCENT) : palColor(Pal::INK);
            const char* tag;
            Rgb565 tagCol = palColor(Pal::INK_DIM);
            if (i == 0) {
                std::snprintf(line, sizeof(line), "QUICK  -%d B", cost);
                tag = "MAY FAIL";
            } else if (i == 1) {
                std::snprintf(line, sizeof(line), "TOOL  -%d B -1", cost);
                tag = toolCount > 0 ? "GUARANTEED" : "NO TOOL";
                tagCol = toolCount > 0 ? palColor(Pal::CALM) : palColor(Pal::WARN);
            } else {
                std::snprintf(line, sizeof(line), "STACKER  -%d B", cost);
                tag = "ROWS PAY";       // never fails; the board sets the size of the clean
            }
            drawLabelValue(fb, kMargin + 12, rowY[i], line, col, tag, tagCol, 0, false);
        }

        // Bottom action line: the gated reason, or the RUN/SWITCH hint.
        const bool toolReady = toolCount > 0;
        const bool runnable = afford && (variant != 1 || toolReady);
        if (defragGated(m)) {
            drawText(fb, kMargin, 176, "- NOTHING TO DEFRAGMENT -",
                     palColor(Pal::INK_DIM));
        } else if (!afford) {
            drawText(fb, kMargin, 176, "- NOT ENOUGH BITS -", palColor(Pal::WARN));
        } else if (variant == 1 && !toolReady) {
            drawText(fb, kMargin, 176, "- NO DEFRAG TOOL -", palColor(Pal::WARN));
        } else if (runnable) {
            drawRowCursor(fb, kMargin, 176, palColor(Pal::ACCENT));
            drawText(fb, kMargin + 12, 176, "B RUN   A SWITCH", palColor(Pal::ACCENT));
        }
    } else {
        drawHeaderBand(fb, "ANTIVIRUS");
        drawText(fb, kMargin, 36, "SCANS + REMOVES ROGUE PROCS.", palColor(Pal::INK));
        drawText(fb, kMargin, 52, "-10 FRAG. CLEARS DEBUFFS +", palColor(Pal::INK));
        drawText(fb, kMargin, 64, "REPLICATION GHOST.", palColor(Pal::INK));
        char status[16];
        avStatus(m, status, sizeof(status));
        std::snprintf(line, sizeof(line), "STATUS: %s", status);
        drawText(fb, kMargin, 84, line, palColor(Pal::INK_DIM));
        if (avGated(m)) {
            drawText(fb, kMargin, 170, "- SYSTEM CLEAN -", palColor(Pal::INK_DIM));
        } else {
            drawRowCursor(fb, kMargin, 170, palColor(Pal::ACCENT));
            drawText(fb, kMargin + 10, 170, "SCAN", palColor(Pal::ACCENT));
        }
    }
}

void drawStackerBoard(Framebuffer& fb, const Stacker& s, int frag, bool arcade) {
    drawHeaderBand(fb, arcade ? "DEFRAG STACKER" : "DEFRAGMENTING");

    // Drawn bottom-up: row 0 is the base, so it sits at the FOOT of the well and the run
    // climbs toward the header. Cells are WIDE and short — a disk block, not a tile — and
    // their size is derived from the grid rather than typed twice, so changing
    // kStackerCols/kStackerRows can't desync the drawing from the rules.
    constexpr int kWellTop = 30;
    constexpr int kWellBottom = 176;
    constexpr int kCellH = (kWellBottom - kWellTop) / kStackerRows;
    constexpr int kCellW = 28;
    constexpr int kGap = 2;
    constexpr int kX0 = (kActiveW - kStackerCols * kCellW) / 2;
    // Bottom-align the grid in the well, so any rounding in kCellH is slack at the TOP
    // (against the header) rather than a floating stack with a gap under its base row.
    const int y0 = kWellBottom - kStackerRows * kCellH;

    auto cellAt = [&](int r, int c, int& x, int& y) {
        x = kX0 + c * kCellW;
        y = y0 + (kStackerRows - 1 - r) * kCellH;   // row 0 at the bottom
    };

    for (int r = 0; r < kStackerRows; ++r)
        for (int c = 0; c < kStackerCols; ++c) {
            int x, y;
            cellAt(r, c, x, y);
            // The well floor: empty space still reads as somewhere blocks GO.
            fb.fillRect(x + kGap, y + kGap, kCellW - 2 * kGap, kCellH - 2 * kGap,
                        palColor(Pal::TRACK));
            if (!s.locked(r, c)) continue;
            fb.fillRect(x + kGap, y + kGap, kCellW - 2 * kGap, kCellH - 2 * kGap,
                        palColor(Pal::ACCENT));
        }

    // The run in hand: an OUTLINE, not a fill. Filled means committed everywhere else on
    // this board, so the distinction survives grayscale without needing a second colour,
    // and its own motion is what says the run is live — no blink, which at this cadence
    // would only read as flicker.
    if (s.running())
        for (int c = s.left(); c < s.left() + s.width(); ++c) {
            int x, y;
            cellAt(s.row(), c, x, y);
            strokeRect(fb, x + kGap, y + kGap, kCellW - 2 * kGap, kCellH - 2 * kGap,
                       palColor(Pal::INK));
        }

    char line[40];
    if (s.running()) {
        std::snprintf(line, sizeof(line), "ROW %d/%d  BLOCKS %d",
                      s.row() + 1, kStackerRows, s.width());
        drawText(fb, kMargin, 182, line, palColor(Pal::INK_DIM));
        drawText(fb, kMargin, 196, "B DROP    C STOP", palColor(Pal::ACCENT));
    } else if (s.won()) {
        drawText(fb, kMargin, 182, "DISK ALIGNED.", palColor(Pal::CALM));
        drawText(fb, kMargin, 196, "ANY BUTTON", palColor(Pal::ACCENT));
    } else {
        std::snprintf(line, sizeof(line), "STALLED AT ROW %d.", s.row() + 1);
        drawText(fb, kMargin, 182, line, palColor(Pal::HOT));
        drawText(fb, kMargin, 196, "ANY BUTTON", palColor(Pal::ACCENT));
    }
    // The right column is what the run is FOR, which is the one thing the two contexts
    // don't share: a Defrag is buying Fragmentation off a disk, and a cabinet run is
    // banking a score. Either way it climbs live rather than arriving afterwards —
    // that is the whole reason to keep locking rows.
    if (arcade) {
        std::snprintf(line, sizeof(line), "MAX %d", kStackerMaxScore);
        drawText(fb, kActiveW - kMargin - textWidth(line), 182, line,
                 palColor(Pal::INK_DIM));
        std::snprintf(line, sizeof(line), "SCORE %d", s.score());
        drawText(fb, kActiveW - kMargin - textWidth(line), 196, line,
                 s.score() > 0 ? palColor(Pal::CALM) : palColor(Pal::INK_DIM));
        return;
    }
    std::snprintf(line, sizeof(line), "FRAG %d", frag);
    drawText(fb, kActiveW - kMargin - textWidth(line), 182, line, palColor(Pal::INK_DIM));
    // The banked clean stays a number of FRAG rather than a score, because a score
    // would be a second currency the player would have to learn the exchange rate for.
    const int worth = stackerFragWorth(s, frag);
    std::snprintf(line, sizeof(line), "-%d FRAG", worth);
    drawText(fb, kActiveW - kMargin - textWidth(line), 196, line,
             worth > 0 ? palColor(Pal::CALM) : palColor(Pal::INK_DIM));
}

int stackerFragWorth(const Stacker& s, int frag) {
    // A cleared board is a FULL defrag, so it is worth the whole disk however bad it
    // was; short of that, the blocks pay for themselves at the tunable rate.
    if (s.won()) return frag;
    return s.score() / kStackerScorePerFrag;
}

void drawMaintProcess(Framebuffer& fb, MaintKind kind, float t) {
    drawHeaderBand(fb, kind == MaintKind::Defrag ? "DEFRAGMENTATION" : "ANTIVIRUS");
    drawText(fb, kMargin, 90,
             kind == MaintKind::Defrag ? "DEFRAGMENTING..." : "SCANNING...",
             palColor(Pal::INK));
    drawProgressBar(fb, kMargin, 108, kActiveW - 2 * kMargin, 12, t,
                    palColor(Pal::ACCENT));
}

void drawMaintOutcome(Framebuffer& fb, MaintKind kind, MaintOutcome outcome,
                      int fragRemoved) {
    drawHeaderBand(fb, kind == MaintKind::Defrag ? "DEFRAGMENTATION" : "ANTIVIRUS");
    char line[40];
    const char* msg = line;
    Rgb565 col = palColor(Pal::CALM);
    if (outcome == MaintOutcome::Failed) {
        std::snprintf(line, sizeof(line), "%s FAILED  +%d FRAG",
                      kind == MaintKind::Defrag ? "DEFRAG" : "AV", kMaintFailPenalty);
        col = palColor(Pal::HOT);
    } else if (kind == MaintKind::Av) {
        msg = "AV COMPLETE  SYSTEM CLEAN";
    } else if (outcome == MaintOutcome::Partial) {
        // A board that stopped short still cleaned something, and says so in the same
        // words a full run does — the difference is the adjective and the number, not a
        // failure notice, because nothing failed.
        if (fragRemoved > 0) {
            std::snprintf(line, sizeof(line), "PARTIAL DEFRAG  -%d FRAG", fragRemoved);
        } else {
            msg = "NO BLOCKS PLACED.  NO CHANGE.";
            col = palColor(Pal::INK_DIM);
        }
    } else {
        std::snprintf(line, sizeof(line), "DEFRAG COMPLETE  -%d FRAG", fragRemoved);
    }
    drawText(fb, kMargin, 100, msg, col);
}

}  // namespace mal
