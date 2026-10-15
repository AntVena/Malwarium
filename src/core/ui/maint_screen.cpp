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

// The disk map both processes animate (ANIM_DEFRAG / ANIM_AV_SWEEP). Its three cell
// states differ in SHAPE — plain track, hollow frame, solid block — so the map reads in
// grayscale without leaning on the hues laid over it.
constexpr int kMapCols = 17;
constexpr int kMapRows = 5;
constexpr int kMapCells = kMapCols * kMapRows;
constexpr int kCellW = 10;
constexpr int kCellH = 8;
constexpr int kCellGap = 2;
constexpr int kMapX = (kActiveW - (kMapCols * (kCellW + kCellGap) - kCellGap)) / 2;
constexpr int kMapY = 64;

// A fixed scatter rather than one off the pet's state: the map is a picture of the
// process, and the numbers that matter are the bar's and the outcome toast's.
uint32_t cellHash(int i) {
    uint32_t h = static_cast<uint32_t>(i) * 2654435761u;
    h ^= h >> 15;
    h *= 2246822519u;
    return h ^ (h >> 13);
}
bool cellUsed(int i) { return cellHash(i) % 100 < 45; }
bool cellInfected(int i) { return cellUsed(i) && (cellHash(i) >> 8) % 7 == 0; }

int cellX(int i) { return kMapX + (i % kMapCols) * (kCellW + kCellGap); }
int cellY(int i) { return kMapY + (i / kMapCols) * (kCellH + kCellGap); }

void drawFreeCell(Framebuffer& fb, int i) {
    fb.fillRect(cellX(i), cellY(i), kCellW, kCellH, palColor(Pal::TRACK));
}
void drawHollowCell(Framebuffer& fb, int i, Rgb565 c) {
    drawFreeCell(fb, i);
    strokeRect(fb, cellX(i), cellY(i), kCellW, kCellH, c);
}
void drawSolidCell(Framebuffer& fb, int i, Rgb565 c) {
    fb.fillRect(cellX(i), cellY(i), kCellW, kCellH, c);
}

// Block-shuffle: the used blocks furthest out are lifted to the front of the disk, so
// at progress `t` the first k cells are one contiguous run and the stragglers are what
// the rest of the bar still has to move. The cell the next block lands in is lit.
void drawDefragMap(Framebuffer& fb, float t) {
    int used[kMapCells];
    int n = 0;
    for (int i = 0; i < kMapCells; ++i)
        if (cellUsed(i)) used[n++] = i;
    const int k = static_cast<int>(t * n);
    const int stillScattered = n - k;
    for (int i = 0; i < kMapCells; ++i) drawFreeCell(fb, i);
    for (int j = 0; j < stillScattered; ++j)
        if (used[j] >= k) drawHollowCell(fb, used[j], palColor(Pal::FRAG_HI));
    for (int i = 0; i < k; ++i) drawSolidCell(fb, i, palColor(Pal::CALM));
    if (k < n) drawSolidCell(fb, k, palColor(Pal::ACCENT));
}

// Scan-sweep: a beam walks the columns left to right. Files ahead of it are unchecked
// (hollow); behind it they are clean (solid), and an infected one is quarantined — its
// cell emptied and crossed out, which is the one mark the scan leaves that a solid or
// hollow cell cannot be mistaken for.
void drawAvMap(Framebuffer& fb, float t) {
    const int beamCol = static_cast<int>(t * kMapCols);
    for (int i = 0; i < kMapCells; ++i) {
        const bool scanned = i % kMapCols < beamCol;
        if (!cellUsed(i)) {
            drawFreeCell(fb, i);
        } else if (!scanned) {
            drawHollowCell(fb, i, palColor(Pal::INK_DIM));
        } else if (cellInfected(i)) {
            drawFreeCell(fb, i);
            const Rgb565 hot = palColor(Pal::HOT);
            for (int d = 0; d < kCellH; ++d) {
                const int x = cellX(i) + 1 + d;
                fb.fillRect(x, cellY(i) + d, 1, 1, hot);
                fb.fillRect(x, cellY(i) + kCellH - 1 - d, 1, 1, hot);
            }
        } else {
            drawSolidCell(fb, i, palColor(Pal::CALM));
        }
    }
    if (beamCol < kMapCols) {
        const int x = kMapX + beamCol * (kCellW + kCellGap) - kCellGap;
        fb.fillRect(x, kMapY - 2, 2,
                    kMapRows * (kCellH + kCellGap) - kCellGap + 4, palColor(Pal::ACCENT));
    }
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
    drawHintBand(fb, "A CYCLE  B OPEN  C BACK");
}

void drawMaintAction(Framebuffer& fb, MaintKind kind, const PetModel& m,
                     int cost, int walletBits, int variant, int toolCount,
                     int defragCount, int failPct, const PlayedRow& played) {
    char line[40];
    // The played row's tag: the dial setting and what it multiplies the payout by, so
    // the stakes and the reward read together before B commits to either.
    const int mult = arcadeRewardMult(played.difficulty);
    char playTag[16];
    std::snprintf(playTag, sizeof(playTag), "%s x%d", arcadeDifficultyName(played.difficulty),
                  mult);
    // What a failed QUICK defrag or AV scan costs, said where the gamble is taken: the
    // screen used to say only MAY FAIL, and the price is a care ERROR as well as the
    // Fragmentation, which is the half a new player would never guess.
    char failCost[40];
    std::snprintf(failCost, sizeof(failCost), "FAIL: +%d FRAG, +1 ERROR", kMaintFailPenalty);
    if (kind == MaintKind::Defrag) {
        drawHeaderBand(fb, "DEFRAGMENTATION");
        // The headline effect is the FOCUSED variant's, because the three don't pay the
        // same: two take a fixed bite, and the played one is paid per unit its game
        // reports. Naming the fixed number on a screen where it can be wrong would be
        // worse than naming nothing.
        if (variant == kDefragVariants - 1) {
            if (played.bitsUnit)
                std::snprintf(line, sizeof(line), "%s -%d FRAG, %s +%d B", played.unit,
                              kPlayFragPerUnit * mult, played.bitsUnit,
                              kPlayBitsPerUnit * mult);
            else
                std::snprintf(line, sizeof(line), "PER %s: -%d FRAG.", played.unit,
                              kPlayFragPerUnit * mult);
            drawText(fb, kMargin, 34, line, palColor(Pal::INK));
        } else {
            std::snprintf(line, sizeof(line), "REDUCES FRAGMENTATION BY %d.",
                          kDefragReduction);
            drawText(fb, kMargin, 34, line, palColor(Pal::INK));
        }
        // Everything under the headline is a READOUT, and dim is what says so — the
        // same job it does on ITEMS and MODS, where the magnitude grid under the name is
        // dim and the prose above it is INK. Colouring these by row instead made dim
        // alternate down the page and mark no group at all, which spent the one
        // separation channel that survives grayscale on texture. The two pairs are
        // fenced by pitch instead: frag facts, then what you have to spend, 16px apart
        // where each pair keeps 12.
        std::snprintf(line, sizeof(line), "CURRENT FRAG: %d", m.fragmentation());
        drawText(fb, kMargin, 50, line, palColor(Pal::INK_DIM));
        // this pet's running defrag tally (persists through freeze/thaw),
        // surfaced here and nowhere else.
        std::snprintf(line, sizeof(line), "DEFRAGS DONE: %d", defragCount);
        drawText(fb, kMargin, 62, line, palColor(Pal::INK_DIM));
        // stage-scaled Bits cost + the wallet, both spelled out so the affordability
        // reads in grayscale (COST N B / HAVE N B). It goes WARN when the wallet is
        // short, which is how a FAILING gate reads on MODS too — dim while it passes,
        // lit the moment it is the thing stopping you.
        std::snprintf(line, sizeof(line), "COST %d B   HAVE %d B", cost, walletBits);
        const bool afford = walletBits >= cost;
        drawText(fb, kMargin, 78, line,
                 afford ? palColor(Pal::INK_DIM) : palColor(Pal::WARN));
        // The tool count is a WALLET line, not a pick: it belongs beside HAVE %d B at
        // the readout pitch, above the list, rather than trailing the list at the list's
        // own pitch — where a dim unindented row still reads as a fourth thing to land
        // the cursor on.
        std::snprintf(line, sizeof(line), "HELD DEFRAG TOOLS: %d", toolCount);
        drawText(fb, kMargin, 90, line, palColor(Pal::INK_DIM));

        // the three payment VARIANTS as a 3-row pick (A switches, B runs the
        // focused one). QUICK = Bits-only, may fail; TOOL = spend a Defrag Tool for a
        // guaranteed clean; the played game = the same Bits, then play for it.
        // Grayscale-safe: the cursor marks the focus, and each row spells its terms in
        // words (n% FAIL / GUARANTEED / NO TOOL / the dial). The 22px above the first row and the 28px
        // below the last are what fence the pick off from the readouts and the action
        // line — both wider than the 18px the rows keep between themselves.
        const int rowY[3] = {112, 130, 148};
        for (int i = 0; i < 3; ++i) {
            const bool focus = variant == i;
            if (focus) drawRowCursor(fb, kMargin, rowY[i], palColor(Pal::ACCENT));
            const Rgb565 col = focus ? palColor(Pal::ACCENT) : palColor(Pal::INK);
            const char* tag;
            Rgb565 tagCol = palColor(Pal::INK_DIM);
            char quickTag[16];
            if (i == 0) {
                std::snprintf(line, sizeof(line), "QUICK  -%d B", cost);
                std::snprintf(quickTag, sizeof(quickTag), "%d%% FAIL", failPct);
                tag = quickTag;
            } else if (i == 1) {
                std::snprintf(line, sizeof(line), "TOOL  -%d B -1", cost);
                tag = toolCount > 0 ? "GUARANTEED" : "NO TOOL";
                tagCol = toolCount > 0 ? palColor(Pal::CALM) : palColor(Pal::WARN);
            } else {
                std::snprintf(line, sizeof(line), "%s  -%d B", played.name, cost);
                tag = playTag;          // never fails; the run sets the size of the clean
            }
            drawLabelValue(fb, kMargin + 12, rowY[i], line, col, tag, tagCol, 0, false);
        }

        // Bottom action line: the gated reason, or the armed action. It names the
        // ACTION, not the buttons — the band below does those, and the AV branch's
        // "SCAN" cursor is the same shape.
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
            drawText(fb, kMargin + 12, 176, "RUN", palColor(Pal::ACCENT));
            // The price of the gamble, as a footnote to the action that takes it — only
            // while QUICK is the armed variant, the one of the three that can fail.
            if (variant == 0)
                drawText(fb, kMargin + 12, 190, failCost, palColor(Pal::INK_DIM));
        }
        drawHintBand(fb, runnable ? "A SWITCH  B RUN  C BACK" : "A SWITCH  C BACK");
    } else {
        drawHeaderBand(fb, "ANTIVIRUS");
        // The prose follows the focused variant, as DEFRAG's headline does: a Quick scan
        // is all-or-nothing, and a played one pays per unit until it is won.
        if (variant == kAvVariants - 1) {
            std::snprintf(line, sizeof(line), "PER %s: -%d FRAG UP TO %d", played.unit,
                          kPlayFragPerUnit * mult, kAvReduction);
            drawText(fb, kMargin, 34, line, palColor(Pal::INK));
            drawText(fb, kMargin, 50, "WIN IT FOR A FULL SCAN:", palColor(Pal::INK));
            drawText(fb, kMargin, 62, "DEBUFFS + GHOST CLEARED.", palColor(Pal::INK));
        } else {
            drawText(fb, kMargin, 34, "SCANS + REMOVES ROGUE PROCS.", palColor(Pal::INK));
            std::snprintf(line, sizeof(line), "-%d FRAG. CLEARS DEBUFFS +", kAvReduction);
            drawText(fb, kMargin, 50, line, palColor(Pal::INK));
            drawText(fb, kMargin, 62, "REPLICATION GHOST.", palColor(Pal::INK));
        }
        char status[16];
        avStatus(m, status, sizeof(status));
        std::snprintf(line, sizeof(line), "STATUS: %s", status);
        drawText(fb, kMargin, 78, line, palColor(Pal::INK_DIM));

        // The two ways to run the same scan, picked the way DEFRAG's three are. Both
        // free: QUICK rolls for it, the played game cannot fail — a run that stops
        // short still pays its units off the disk.
        const int rowY[2] = {112, 130};
        for (int i = 0; i < kAvVariants; ++i) {
            const bool focus = variant == i;
            if (focus) drawRowCursor(fb, kMargin, rowY[i], palColor(Pal::ACCENT));
            const Rgb565 col = focus ? palColor(Pal::ACCENT) : palColor(Pal::INK);
            char quickTag[16];
            const char* tag = playTag;
            if (i == 0) {
                std::snprintf(quickTag, sizeof(quickTag), "%d%% FAIL", failPct);
                tag = quickTag;
            }
            drawLabelValue(fb, kMargin + 12, rowY[i], i == 0 ? "QUICK" : played.name, col,
                           tag, palColor(Pal::INK_DIM), 0, false);
        }

        if (avGated(m)) {
            drawText(fb, kMargin, 176, "- SYSTEM CLEAN -", palColor(Pal::INK_DIM));
        } else {
            drawRowCursor(fb, kMargin, 176, palColor(Pal::ACCENT));
            drawText(fb, kMargin + 12, 176, "SCAN", palColor(Pal::ACCENT));
            if (variant == 0)
                drawText(fb, kMargin + 12, 190, failCost, palColor(Pal::INK_DIM));
        }
        drawHintBand(fb, avGated(m) ? "A SWITCH  C BACK" : "A SWITCH  B SCAN  C BACK");
    }
}

// The right column of a played board's footer: what the run is FOR, the one thing its
// contexts don't share. A cabinet banks a score against its line; a MAINT run counts
// the disk down live, and either the Bits it is earning or the Fragmentation it is worth
// below that. It climbs as the run goes rather than arriving afterwards — that is the
// whole reason to keep playing.
void drawPlayHudWorth(Framebuffer& fb, const PlayHud& hud) {
    char line[24];
    auto right = [&](int y, Rgb565 c) {
        drawText(fb, kActiveW - kMargin - textWidth(line), y, line, c);
    };
    if (hud.arcade) {
        std::snprintf(line, sizeof(line), "GOAL %d", hud.scoreMax);
        right(182, palColor(Pal::INK_DIM));
        std::snprintf(line, sizeof(line), "SCORE %d", hud.score);
        right(196, hud.score > 0 ? palColor(Pal::CALM) : palColor(Pal::INK_DIM));
        return;
    }
    std::snprintf(line, sizeof(line), "FRAG %d", hud.fragNow);
    right(182, hud.fragOff > 0 ? palColor(Pal::CALM) : palColor(Pal::INK_DIM));
    const bool bitsLine = hud.bits > 0 || hud.paysBits;
    if (bitsLine) std::snprintf(line, sizeof(line), "+%d B", hud.bits);
    else std::snprintf(line, sizeof(line), "-%d FRAG", hud.fragOff);
    right(196, (bitsLine ? hud.bits : hud.fragOff) > 0 ? palColor(Pal::CALM)
                                                       : palColor(Pal::INK_DIM));
}

void drawStackerBoard(Framebuffer& fb, const Stacker& s, const PlayHud& hud) {
    drawHeaderBand(fb, hud.title);

    // Drawn bottom-up: row 0 is the base, so it sits at the FOOT of the well and the run
    // climbs toward the header. Cells are WIDE and short — a disk block, not a tile — and
    // their size is derived from the grid rather than typed twice, so changing
    // kStackerCols/kStackerRows can't desync the drawing from the rules. One row more
    // than the grid is laid out: the open row above the well the intruder starts in.
    constexpr int kWellTop = 30;
    constexpr int kWellBottom = 176;
    constexpr int kCellH = (kWellBottom - kWellTop) / (kStackerRows + 1);
    constexpr int kCellW = 28;
    constexpr int kGap = 2;
    constexpr int kX0 = (kActiveW - kStackerCols * kCellW) / 2;
    // Bottom-align the grid in the well, so any rounding in kCellH is slack at the TOP
    // (against the header) rather than a floating stack with a gap under its base row.
    const int y0 = kWellBottom - (kStackerRows + 1) * kCellH;

    auto cellAt = [&](int r, int c, int& x, int& y) {
        x = kX0 + c * kCellW;
        y = y0 + (kStackerRows - r) * kCellH;   // row 0 at the bottom
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

    // Each intruder's drop line: a dotted guide down its column to the top of the wall
    // there, so the gap still to close is read off the board rather than worked out. It
    // goes once the wall reaches the intruder. Dotted rather than solid so it never reads
    // as a block in grayscale; on a breach it is HOT, the hole the intruder went through.
    for (int i = 0; i < kStackerThreats; ++i) {
        const int col = s.threatCol(i);
        int ix, iy;
        cellAt(s.threatRow(), col, ix, iy);
        if (!s.held(i) && (s.running() || s.breached())) {
            int fx, fy;
            cellAt(s.height(col), col, fx, fy);
            const Rgb565 dot = s.running() ? palColor(Pal::INK_DIM) : palColor(Pal::HOT);
            for (int y = iy + kCellH; y < fy + kCellH; y += 3)
                fb.fillRect(ix + kCellW / 2 - 1, y, 2, 1, dot);
        }

        // The intruder itself: a hollow frame crossed out, FRAGGER's fragment mark and
        // the AV map's quarantine mark — the one shape on this board that is a threat.
        const Rgb565 hot = palColor(Pal::HOT);
        const int bw = kCellW - 2 * kGap, bh = kCellH - 2 * kGap;
        const int bx = ix + kGap, by = iy + kGap;
        fb.fillRect(bx, by, bw, bh, palColor(Pal::PAPER));
        strokeRect(fb, bx, by, bw, bh, hot);
        const int cross = bh - 2;
        const int cx = bx + (bw - cross) / 2, cy = by + 1;
        for (int d = 0; d < cross; ++d) {
            fb.fillRect(cx + d, cy + d, 2, 1, hot);
            fb.fillRect(cx + cross - 1 - d, cy + d, 2, 1, hot);
        }
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

    // The status/control pair below the well is a live HUD, not a static hint — its
    // right column changes every drop (SCORE or FRAG) rather than naming a fixed
    // button set, which is why it isn't drawHintBand. But every OTHER screen's
    // footer sits on the same TRACK-filled band, and this one was bare background —
    // the one place in the app that band vanishes the moment play starts.
    fb.fillRect(0, 178, kActiveW, kActiveH - 178, palColor(Pal::TRACK));

    char line[40];
    if (s.running()) {
        // GAPS are the rows of open air under each intruder, left to right: the clock
        // this board runs on, counted because the drop lines only show it.
        static_assert(kStackerThreats == 2, "the GAPS readout names two intruders");
        auto gap = [&](int i) {
            const int g = s.threatRow() - s.height(s.threatCol(i));
            return g > 0 ? g : 0;
        };
        std::snprintf(line, sizeof(line), "GAPS %d %d  HAND %d", gap(0), gap(1),
                      s.width());
        drawText(fb, kMargin, 182, line, palColor(Pal::INK_DIM));
        drawText(fb, kMargin, 196, "B DROP    C STOP", palColor(Pal::ACCENT));
    } else if (s.won()) {
        drawText(fb, kMargin, 182, "LINE HELD.", palColor(Pal::CALM));
        drawText(fb, kMargin, 196, "ANY BUTTON", palColor(Pal::ACCENT));
    } else {
        drawText(fb, kMargin, 182, "BREACH!", palColor(Pal::HOT));
        drawText(fb, kMargin, 196, "ANY BUTTON", palColor(Pal::ACCENT));
    }
    drawPlayHudWorth(fb, hud);
}

void drawFraggerBoard(Framebuffer& fb, const Fragger& f, const PlayHud& hud, int shotLane,
                      int shotRow) {
    drawHeaderBand(fb, hud.title);

    // The lanes of wide, short disk blocks over a scan head, the Stacker well's cell
    // shape turned into a fall. Lanes are as wide as the run's count allows, so fewer
    // of them means bigger targets rather than a narrow strip in the middle. Every state differs by SHAPE so the board reads in
    // grayscale: an empty slot is plain track, a file a solid block, a fragment a hollow
    // frame crossed out — the AV map's quarantine mark, here as the thing to shoot.
    constexpr int kMaxLaneW = 48;
    constexpr int kRowH = 13;
    constexpr int kInset = 2;
    constexpr int kTop = 28;
    constexpr int kHeadY = kTop + kFraggerRows * kRowH + 2;
    constexpr int kHeadH = 8;
    constexpr int kBlockH = kRowH - 2 * kInset;
    constexpr int kCross = kBlockH - 2;
    const int lanes = f.lanes();
    const int laneW = (kActiveW - 2 * kMargin) / lanes < kMaxLaneW
                           ? (kActiveW - 2 * kMargin) / lanes : kMaxLaneW;
    const int x0 = (kActiveW - lanes * laneW) / 2;
    const int blockW = laneW - 2 * kInset;

    auto blockX = [&](int lane) { return x0 + lane * laneW + kInset; };
    auto blockY = [&](int row) { return kTop + row * kRowH + kInset; };

    for (int r = 0; r < kFraggerRows; ++r)
        for (int l = 0; l < lanes; ++l) {
            const int x = blockX(l), y = blockY(r);
            switch (f.cell(r, l)) {
                case Fragger::Cell::Empty:
                    fb.fillRect(x, y, blockW, kBlockH, palColor(Pal::TRACK));
                    break;
                case Fragger::Cell::File:
                    fb.fillRect(x, y, blockW, kBlockH, palColor(Pal::CALM));
                    break;
                case Fragger::Cell::Fragment: {
                    const Rgb565 hot = palColor(Pal::HOT);
                    fb.fillRect(x, y, blockW, kBlockH, palColor(Pal::PAPER));
                    strokeRect(fb, x, y, blockW, kBlockH, hot);
                    // A square cross in the middle of the frame, two pixels wide so it
                    // holds its shape at x1.75 — a cross stretched corner to corner of
                    // a block this flat reads as an envelope.
                    const int cx = x + (blockW - kCross) / 2, cy = y + (kBlockH - kCross) / 2;
                    for (int d = 0; d < kCross; ++d) {
                        fb.fillRect(cx + d, cy + d, 2, 1, hot);
                        fb.fillRect(cx + kCross - 1 - d, cy + d, 2, 1, hot);
                    }
                    break;
                }
            }
        }

    // The beam, from the head up to whatever stopped it — the top of the disk for a shot
    // that met nothing. It stays until the next row falls, which at this cadence is the
    // difference between seeing the shot and having to take it on trust.
    if (shotLane >= 0) {
        const int bx = x0 + shotLane * laneW + laneW / 2 - 1;
        const int top = shotRow >= 0 ? blockY(shotRow) + kBlockH : kTop;
        fb.fillRect(bx, top, 2, kHeadY - top, palColor(Pal::ACCENT));
    }

    // The scan head: a wide base with a muzzle, solid ACCENT — the one thing on the
    // board that moves on a press. A run that crashed marks the lane it crashed in.
    if (f.running()) {
        const int hx = x0 + f.lane() * laneW + kInset;
        fb.fillRect(hx, kHeadY + 3, blockW, kHeadH - 3, palColor(Pal::ACCENT));
        fb.fillRect(hx + blockW / 2 - 3, kHeadY, 6, 3, palColor(Pal::ACCENT));
    } else if (f.crashLane() >= 0) {
        fb.fillRect(x0 + f.crashLane() * laneW + kInset, kHeadY, blockW, kHeadH,
                    palColor(Pal::HOT));
    }

    // The live HUD on the footer band, the Stacker's layout: the run on the left, what
    // it is worth on the right.
    fb.fillRect(0, 178, kActiveW, kActiveH - 178, palColor(Pal::TRACK));
    char line[40];
    if (f.running()) {
        std::snprintf(line, sizeof(line), "HITS %d  CATCH %d", f.hits(), f.caught());
        drawText(fb, kMargin, 182, line, palColor(Pal::INK_DIM));
        drawText(fb, kMargin, 196, "A<  B FIRE  >C", palColor(Pal::ACCENT));
    } else {
        drawText(fb, kMargin, 182, "FRAGMENT LANDED.", palColor(Pal::HOT));
        drawText(fb, kMargin, 196, "ANY BUTTON", palColor(Pal::ACCENT));
    }
    drawPlayHudWorth(fb, hud);
}

void drawMaintProcess(Framebuffer& fb, MaintKind kind, float t) {
    drawHeaderBand(fb, kind == MaintKind::Defrag ? "DEFRAGMENTATION" : "ANTIVIRUS");
    if (kind == MaintKind::Defrag) drawDefragMap(fb, t);
    else drawAvMap(fb, t);
    drawText(fb, kMargin, 126,
             kind == MaintKind::Defrag ? "DEFRAGMENTING..." : "SCANNING...",
             palColor(Pal::INK));
    drawProgressBar(fb, kMargin, 144, kActiveW - 2 * kMargin, 12, t,
                    palColor(Pal::ACCENT));
}

void drawMaintOutcome(Framebuffer& fb, MaintKind kind, MaintOutcome outcome,
                      int fragRemoved, int bitsEarned) {
    const bool defrag = kind == MaintKind::Defrag;
    drawHeaderBand(fb, defrag ? "DEFRAGMENTATION" : "ANTIVIRUS");
    char line[40];
    const char* msg = line;
    Rgb565 col = palColor(Pal::CALM);
    if (outcome == MaintOutcome::Failed) {
        std::snprintf(line, sizeof(line), "%s FAILED  +%d FRAG", defrag ? "DEFRAG" : "AV",
                      kMaintFailPenalty);
        col = palColor(Pal::HOT);
    } else if (outcome == MaintOutcome::Partial) {
        // A played run that came up short still cleaned something, and says so in the
        // same words a full run does — the difference is the adjective and the number,
        // not a failure notice, because nothing failed.
        if (fragRemoved > 0) {
            std::snprintf(line, sizeof(line), "PARTIAL %s  -%d FRAG",
                          defrag ? "DEFRAG" : "SCAN", fragRemoved);
        } else {
            msg = defrag ? "NO FRAGMENTS SHOT.  NO CHANGE." : "NO BLOCKS PLACED.  NO CHANGE.";
            col = palColor(Pal::INK_DIM);
        }
    } else if (!defrag) {
        msg = "AV COMPLETE  SYSTEM CLEAN";
    } else {
        std::snprintf(line, sizeof(line), "DEFRAG COMPLETE  -%d FRAG", fragRemoved);
    }
    drawText(fb, kMargin, 100, msg, col);
    if (bitsEarned > 0) {
        std::snprintf(line, sizeof(line), "+%d BITS", bitsEarned);
        drawText(fb, kMargin, 116, line, palColor(Pal::ACCENT));
    }
}

}  // namespace mal
