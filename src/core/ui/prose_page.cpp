#include "core/ui/prose_page.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>

#include "core/render/framebuffer.h"
#include "core/render/palette.h"

namespace mal {

namespace {

int proseBodyH(const char* body) {
    const int lines = (body && body[0]) ? textWrapLines(body, kProseW) : 0;
    return kFontH + (lines > 0 ? kProseNameGap + lines * kProseLineH : 0) + kProseRowGap;
}

int headerLead(int index) { return index == 0 ? 0 : kProseGroupLead; }

// UI_SCROLLBAR for a flowed page. Measured in ROWS, not pixels — the thumb reports
// position in the LIST, which is what the reader is tracking; a pixel-proportional
// thumb on rows of wildly different heights jumps unevenly for no reason the reader
// can see.
void drawScrollThumb(Framebuffer& fb, int rowTop, int top, int shown, int total) {
    const int barX = kActiveW - 3;
    const int trackH = kProseBottom - rowTop;
    fb.fillRect(barX, rowTop, 2, trackH, palColor(Pal::TRACK));
    const int thumbH = std::max(8, trackH * shown / total);
    const int thumbY = rowTop + trackH * top / total;
    fb.fillRect(barX, thumbY, 2, thumbH, palColor(Pal::INK_DIM));
}

}  // namespace

std::vector<int> proseRowHeights(const std::vector<ProseRow>& rows) {
    std::vector<int> h;
    h.reserve(rows.size());
    for (int i = 0; i < static_cast<int>(rows.size()); ++i)
        h.push_back(rows[i].header ? headerLead(i) + kProseHeaderH
                                   : proseBodyH(rows[i].body.c_str()));
    return h;
}

int proseRowsFitting(const std::vector<ProseRow>& rows, int top, int rowTop) {
    const std::vector<int> heights = proseRowHeights(rows);
    int y = rowTop;
    int n = 0;
    for (int i = top; i < static_cast<int>(heights.size()); ++i) {
        if (n > 0 && y + heights[i] > kProseBottom) break;
        y += heights[i];
        ++n;
    }
    return n;
}

void drawProseRows(Framebuffer& fb, const std::vector<ProseRow>& rows, int scrollTop,
                   int rowTop, int beat, const char* hint) {
    if (rows.empty()) return;
    const std::vector<int> heights = proseRowHeights(rows);
    const int total = static_cast<int>(rows.size());
    const bool overflow = proseRowsFitting(rows, 0, rowTop) < total;
    const int top = overflow ? std::max(0, std::min(scrollTop, total - 1)) : 0;
    const int shown = proseRowsFitting(rows, top, rowTop);

    int y = rowTop;
    for (int v = 0; v < shown; ++v) {
        const ProseRow& r = rows[top + v];
        if (r.header) {
            drawText(fb, kMargin, y + headerLead(top + v), r.label,
                     palColor(Pal::INK_DIM));
        } else {
            // Name and tag as a PAIR (drawLabelValue), so a long name yields to the tag
            // instead of running underneath it — the failure two independent drawText
            // calls produce every time the copy grows.
            drawLabelValue(fb, kMargin, y, r.label, palColor(Pal::INK), r.tag,
                           palColor(Pal::INK_DIM), beat, /*scroll=*/false);
            if (!r.body.empty())
                drawTextWrapped(fb, kMargin, y + kFontH + kProseNameGap, kProseW,
                                r.body.c_str(), palColor(Pal::INK_DIM), kProseLineH,
                                textWrapLines(r.body.c_str(), kProseW));
        }
        y += heights[top + v];
    }

    if (overflow) {
        drawScrollThumb(fb, rowTop, top, shown, total);
        if (hint && hint[0]) drawHintBand(fb, hint);
    }
}

void setProseTag(ProseRow& row, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(row.tag, sizeof(row.tag), fmt, ap);
    va_end(ap);
}

}  // namespace mal
