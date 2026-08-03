#include "core/ui/maint_screen.h"

#include <cstdio>

#include "core/render/canvas.h"
#include "core/render/font5x7.h"
#include "core/render/framebuffer.h"
#include "core/render/palette.h"
#include "core/render/sprite.h"
#include "core/ui/widgets.h"
#include "generated/assets.h"

namespace mal {

namespace {

constexpr int kMargin = 8;
constexpr int kHeaderRule = 22;
constexpr int kRowTop = 26;
constexpr int kRowH = 28;
constexpr int kIcon = 20;

void header(Framebuffer& fb, const char* title) {
    fb.clear(palColor(Pal::PAPER));
    drawText(fb, kMargin, 6, title, palColor(Pal::INK));
    fb.fillRect(0, kHeaderRule, kActiveW, 1, palColor(Pal::TRACK));
}

// AV's status preview: ghost / debuff count / clean.
void avStatus(const PetModel& m, char* out, int n) {
    if (m.hasGhost()) std::snprintf(out, n, "GHOST");
    else if (m.debuffs() > 0) std::snprintf(out, n, "%d DEBUFF", m.debuffs());
    else std::snprintf(out, n, "CLEAN");
}

}  // namespace

void drawMaintList(Framebuffer& fb, const PetModel& m, int cursor) {
    header(fb, "MAINT");

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
        drawSprite(fb, *icons[i], 0, 16, y + (kRowH - kIcon) / 2);
        drawText(fb, 40, y + (kRowH - kFontH) / 2, labels[i], palColor(Pal::INK));
        drawText(fb, kActiveW - kMargin - textWidth(status[i]),
                 y + (kRowH - kFontH) / 2, status[i], palColor(Pal::INK_DIM));
    }
}

void drawMaintAction(Framebuffer& fb, MaintKind kind, const PetModel& m,
                     int cost, int walletBits, int variant, int toolCount,
                     int defragCount) {
    char line[40];
    if (kind == MaintKind::Defrag) {
        header(fb, "DEFRAGMENTATION");
        drawText(fb, kMargin, 34, "REDUCES FRAGMENTATION BY 20.", palColor(Pal::INK));
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

        // the two payment VARIANTS as a 2-row pick (A switches, B runs the
        // focused one). QUICK = Bits-only, may fail; TOOL = spend a Defrag Tool for a
        // guaranteed clean. Grayscale-safe: the cursor marks the focus, and each row
        // spells its terms (MAY FAIL / GUARANTEED / NO TOOL) in words.
        const int rowY[2] = {104, 124};
        for (int i = 0; i < 2; ++i) {
            const bool focus = variant == i;
            if (focus) drawRowCursor(fb, kMargin, rowY[i], palColor(Pal::ACCENT));
            const Rgb565 col = focus ? palColor(Pal::ACCENT) : palColor(Pal::INK);
            if (i == 0) {
                std::snprintf(line, sizeof(line), "QUICK  -%d B", cost);
                drawText(fb, kMargin + 12, rowY[i], line, col);
                drawText(fb, kActiveW - kMargin - textWidth("MAY FAIL"), rowY[i],
                         "MAY FAIL", palColor(Pal::INK_DIM));
            } else {
                std::snprintf(line, sizeof(line), "TOOL   -%d B -1 TOOL", cost);
                drawText(fb, kMargin + 12, rowY[i], line, col);
                const char* tag = toolCount > 0 ? "GUARANTEED" : "NO TOOL";
                drawText(fb, kActiveW - kMargin - textWidth(tag), rowY[i], tag,
                         toolCount > 0 ? palColor(Pal::CALM) : palColor(Pal::WARN));
            }
        }
        std::snprintf(line, sizeof(line), "HELD DEFRAG TOOLS: %d", toolCount);
        drawText(fb, kMargin, 144, line, palColor(Pal::INK_DIM));

        // Bottom action line: the gated reason, or the RUN/SWITCH hint.
        const bool toolReady = toolCount > 0;
        const bool runnable = afford && (variant == 0 || toolReady);
        if (defragGated(m)) {
            drawText(fb, kMargin, 170, "- NOTHING TO DEFRAGMENT -",
                     palColor(Pal::INK_DIM));
        } else if (!afford) {
            drawText(fb, kMargin, 170, "- NOT ENOUGH BITS -", palColor(Pal::WARN));
        } else if (variant == 1 && !toolReady) {
            drawText(fb, kMargin, 170, "- NO DEFRAG TOOL -", palColor(Pal::WARN));
        } else if (runnable) {
            drawRowCursor(fb, kMargin, 170, palColor(Pal::ACCENT));
            drawText(fb, kMargin + 12, 170, "B RUN   A SWITCH", palColor(Pal::ACCENT));
        }
    } else {
        header(fb, "ANTIVIRUS");
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

void drawMaintProcess(Framebuffer& fb, MaintKind kind, float t) {
    header(fb, kind == MaintKind::Defrag ? "DEFRAGMENTATION" : "ANTIVIRUS");
    drawText(fb, kMargin, 90,
             kind == MaintKind::Defrag ? "DEFRAGMENTING..." : "SCANNING...",
             palColor(Pal::INK));
    drawProgressBar(fb, kMargin, 108, kActiveW - 2 * kMargin, 12, t,
                    palColor(Pal::ACCENT));
}

void drawMaintOutcome(Framebuffer& fb, MaintKind kind, bool success) {
    header(fb, kind == MaintKind::Defrag ? "DEFRAGMENTATION" : "ANTIVIRUS");
    const char* msg;
    if (kind == MaintKind::Defrag)
        msg = success ? "DEFRAG COMPLETE  -20 FRAG" : "DEFRAG FAILED  +15 FRAG";
    else
        msg = success ? "AV COMPLETE  SYSTEM CLEAN" : "AV FAILED  +15 FRAG";
    drawText(fb, kMargin, 100, msg,
             success ? palColor(Pal::CALM) : palColor(Pal::HOT));
}

}  // namespace mal
