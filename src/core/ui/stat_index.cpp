#include "core/ui/stat_screen.h"

#include <cstdarg>
#include <cstdio>

#include "core/render/framebuffer.h"
#include "core/render/palette.h"
#include "core/ui/layout.h"
#include "core/ui/widgets.h"

namespace mal {

namespace {

// --- The INDEX's own grid ---------------------------------------------------
//
// Tighter than the list grid's kRowH: the index earns its keep by showing as much of
// the reader's map at once as it can, and a row here is a name and a number rather than
// the icon-plus-label a carousel row carries.
//
// It still WINDOWS rather than shrinking to fit: the roster grows with the page count
// and with each page's sections, and a pitch chased down to hold all of it would end up
// unreadable to buy an ability the list contract already provides (the cursor drives the
// window, listScrollTop, and A/hold-C walk it).
constexpr int kIndexRowTop = kRowTop;
constexpr int kIndexRowH = 15;
constexpr int kIndexLabelX = 20;   // clear of the cursor marker at the left margin
constexpr int kIndexSubX = 32;     // a section INSIDE a page, indented under it
constexpr int kIndexVisible = (kProseBottom - kIndexRowTop) / kIndexRowH;

StatIndexRow indexRow(const StatPage& page, const char* label, int anchor, const char* fmt,
                      va_list ap) {
    StatIndexRow r{};
    r.label = label;
    r.page = &page;
    r.anchor = anchor;
    std::vsnprintf(r.value, sizeof(r.value), fmt, ap);
    return r;
}

}  // namespace

StatIndexRow pageIndexRow(const StatPage& page, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    const StatIndexRow r = indexRow(page, page.title(), 0, fmt, ap);
    va_end(ap);
    return r;
}

StatIndexRow sectionIndexRow(const StatPage& page, const char* label, int anchor,
                             const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    StatIndexRow r = indexRow(page, label, anchor, fmt, ap);
    va_end(ap);
    r.sub = true;
    return r;
}

int StatPage::nextScroll(const StatSources& /*s*/, int scrollTop) const { return scrollTop; }

void drawStatIndex(Framebuffer& fb, const std::vector<StatIndexRow>& rows, int cursor,
                   int beat) {
    drawHeaderBand(fb, "STAT", "INDEX");

    const int n = static_cast<int>(rows.size());
    // One window, derived from the cursor the way every scrolling list on the device
    // derives its own (layout.h), so the rows drawn and the thumb below can never
    // disagree about where the list is.
    const int top = listScrollTop(cursor, n, kIndexVisible);
    for (int v = 0; v < kIndexVisible && top + v < n; ++v) {
        const int i = top + v;
        const StatIndexRow& r = rows[i];
        const int y = kIndexRowTop + v * kIndexRowH;
        if (i == cursor) {
            fb.fillRect(4, y - 2, kActiveW - 8, kIndexRowH - 1, palColor(Pal::TRACK));
            drawRowCursor(fb, 6, y, palColor(Pal::ACCENT));
        }
        // A section reads as part of the page above it: dimmer, and indented under it.
        // The indent is the grayscale channel for that nesting: the dimming alone
        // would not survive the gate.
        const Rgb565 ink = r.sub ? palColor(Pal::INK_DIM) : palColor(Pal::INK);
        const int x = r.sub ? kIndexSubX : kIndexLabelX;
        drawLabelValue(fb, x, y, r.label, ink, r.value, palColor(Pal::INK_DIM), beat,
                       scrollWhen(i == cursor));
    }

    drawScrollbar(fb, kIndexRowTop, kIndexVisible * kIndexRowH, n, top, kIndexVisible);

    drawHintBand(fb, "A NEXT  B OPEN  C BACK");
}


} // namespace mal
