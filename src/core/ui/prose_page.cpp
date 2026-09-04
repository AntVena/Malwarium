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

// A heading's lead-in is the fence between it and the group before it, so the heading
// that OPENS a window has nothing to be fenced from — the header band above it already
// is the fence — and takes none. `top` is the window's first row, which is why the
// heights are measured per window rather than once for the page.
int headerLead(int index, int top) { return index == top ? 0 : kProseGroupLead; }

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

std::vector<int> proseRowHeights(const std::vector<ProseRow>& rows, int top) {
    std::vector<int> h;
    h.reserve(rows.size());
    for (int i = 0; i < static_cast<int>(rows.size()); ++i)
        h.push_back(rows[i].header ? headerLead(i, top) + kProseHeaderH
                                   : proseBodyH(rows[i].body.c_str()));
    return h;
}

int proseRowsFitting(const std::vector<ProseRow>& rows, int top, int rowTop) {
    const std::vector<int> heights = proseRowHeights(rows, top);
    int y = rowTop;
    int n = 0;
    for (int i = top; i < static_cast<int>(heights.size()); ++i) {
        // A SECTION WANTS A WINDOW OF ITS OWN. The heading is what says which group the
        // rows under it belong to, so a window that opens midway through a section has
        // thrown that away, and one that ends ON a heading has stranded it from its own
        // rows. Stopping the fit at the next heading fixes both, and is what makes each
        // press of the scroll key land on the next GROUP.
        //
        // Unless it would buy that with an almost empty screen. A section longer than
        // one window leaves a short tail behind it, and breaking after a tail of one row
        // spends a whole screen saying very little — so the break is taken only once the
        // window already holds a screen's worth to break AFTER. Half the body is the
        // line: above it the window reads as full and the heading below reads as the
        // next thing; under it the heading is better off packed in behind the tail.
        if (n > 0 && rows[i].header && y - rowTop >= (kProseBottom - rowTop) / 2) break;
        if (n > 0 && y + heights[i] > kProseBottom) break;
        y += heights[i];
        ++n;
    }
    return n;
}

int proseWindowCount(const std::vector<ProseRow>& rows, int rowTop) {
    int n = 0;
    for (int top = 0; top < static_cast<int>(rows.size()); ++n) {
        const int shown = proseRowsFitting(rows, top, rowTop);
        if (shown <= 0) break;   // unreachable (the fit floors at one row), not a loop
        top += shown;
    }
    return n;
}

int proseWindowIndex(const std::vector<ProseRow>& rows, int scrollTop, int rowTop) {
    int n = 0;
    for (int top = 0; top < static_cast<int>(rows.size()); ++n) {
        if (top >= scrollTop) return n;
        const int shown = proseRowsFitting(rows, top, rowTop);
        if (shown <= 0) break;
        top += shown;
    }
    return n;
}

void drawProseRows(Framebuffer& fb, const std::vector<ProseRow>& rows, int scrollTop,
                   int rowTop, int beat, const char* hint) {
    if (rows.empty()) return;
    const int total = static_cast<int>(rows.size());
    const bool overflow = proseRowsFitting(rows, 0, rowTop) < total;
    const int top = overflow ? std::max(0, std::min(scrollTop, total - 1)) : 0;
    const std::vector<int> heights = proseRowHeights(rows, top);
    const int shown = proseRowsFitting(rows, top, rowTop);

    int y = rowTop;
    for (int v = 0; v < shown; ++v) {
        const ProseRow& r = rows[top + v];
        if (r.header) {
            // A heading may carry a tag of its own — a running total for the group under
            // it (STAT's TIERS page). Paired with the label rather than placed
            // independently for the same reason the entry rows are: whichever of the two
            // grows, they yield to each other instead of overlapping. Both dim, because a
            // heading is a fence and must not out-shout the rows it fences.
            const int hy = y + headerLead(top + v, top);
            if (r.tag[0])
                drawLabelValue(fb, kMargin, hy, r.label, palColor(Pal::INK_DIM), r.tag,
                               palColor(Pal::INK_DIM), beat, /*scroll=*/false);
            else
                drawText(fb, kMargin, hy, r.label, palColor(Pal::INK_DIM));
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
