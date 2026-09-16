#include "core/ui/story_screen.h"

#include <algorithm>
#include <cstdio>

#include "core/render/font.h"
#include "core/render/framebuffer.h"
#include "core/render/palette.h"

namespace mal {

namespace {

// The window counter in the header band's right slot: "2/4". Its own buffer rather than
// a caller's, because the only thing that knows how many windows a chapter has is the
// flow, and the flow is what this screen is built on.
void formatWindows(char* out, size_t n, int window, int windows) {
    std::snprintf(out, n, "%d/%d", window + 1, windows < 1 ? 1 : windows);
}

// The auto-advance countdown: a rule that shortens from the right as the window's
// budget runs out. Drawn in TRACK under INK_DIM so it reads as chrome rather than as a
// gauge of anything the player owns — what it measures is the screen's own patience.
void drawStoryClock(Framebuffer& fb, int remainingPct) {
    const int w = kActiveW - 2 * kMargin;
    const int pct = std::max(0, std::min(100, remainingPct));
    fb.fillRect(kMargin, kStoryClockY, w, kStoryClockH, palColor(Pal::TRACK));
    fb.fillRect(kMargin, kStoryClockY, w * pct / 100, kStoryClockH,
                palColor(Pal::INK_DIM));
}

}  // namespace

void drawStoryPage(Framebuffer& fb, const std::vector<ProseRow>& rows,
                   const StoryPageView& v) {
    char counter[12];
    formatWindows(counter, sizeof(counter), v.window, v.windows);
    drawHeaderBand(fb, "STORY", counter);
    // The chapter title, under the band and above the flow. ACCENT rather than INK
    // because it is the one line on the page that is not the chapter's prose, and a
    // reader coming back to this screen from the archive is looking for exactly it.
    // Marqueed on the same clock everything else on the device travels on, so a long
    // title costs legibility rather than costing the layout.
    drawTextMarquee(fb, kMargin, kStoryTitleY, kActiveW - 2 * kMargin, v.title,
                    palColor(Pal::ACCENT), v.beat, /*scroll=*/true);

    // The flow itself. Its own hint is suppressed (prose_page only draws one when the
    // page overflows, and this screen names both keys on every window whether it does
    // or not) — a chapter that fits on one screen still has to say that B is what ends
    // it, or the only way off the page is to wait the clock out.
    drawProseRows(fb, rows, v.scrollTop, kStoryBodyTop, v.beat, /*hint=*/nullptr);

    drawStoryClock(fb, v.remainingPct);
    // The LAST window's B finishes the chapter rather than turning a page, so the word
    // changes with it: a reader on the final screen is told they are on the final
    // screen by the key that leaves it. C is the same either way — skip the rest.
    const bool last = v.window + 1 >= v.windows;
    drawHintBand(fb, last ? "B DONE   C SKIP" : "B NEXT   C SKIP");
}

void drawStoryArchive(Framebuffer& fb, const StoryArchiveRow* rows, int rowCount,
                      int cursor, int beat) {
    char counter[12];
    std::snprintf(counter, sizeof(counter), "%d", rowCount);
    drawHeaderBand(fb, "CHAPTERS", rowCount > 0 ? counter : nullptr);

    if (rowCount <= 0) {
        // Nothing read yet. Said in words rather than left blank: an empty list and a
        // broken screen look identical, and this one fills itself by being played.
        drawTextWrapped(fb, kMargin, kRowTop, kActiveW - 2 * kMargin,
                        "Nothing yet. Chapters are written as you walk - go and find "
                        "the first one.",
                        palColor(Pal::INK_DIM), kLineH, 4);
        drawHintBand(fb, "C BACK");
        return;
    }

    const int top = listScrollTop(cursor, rowCount, kStoryArchiveRows);
    const int visible = std::min(rowCount, kStoryArchiveRows);
    for (int i = 0; i < visible; ++i) {
        const int idx = top + i;
        if (idx >= rowCount) break;
        const StoryArchiveRow& r = rows[idx];
        const int y = kRowTop + i * kStoryArchiveRowH;
        const bool focused = (idx == cursor);
        if (focused) {
            fb.fillRect(2, y - 2, kActiveW - 4, kStoryArchiveRowH - 2,
                        palColor(Pal::TRACK));
            drawRowCursor(fb, 3, y, palColor(Pal::ACCENT));
        }
        // Title over a dim line naming where and when — the two-line row EXPL's own
        // list uses, so a chapter reads like the zone row it came from.
        drawTextMarquee(fb, kMargin, y, kActiveW - 2 * kMargin, r.title,
                        palColor(Pal::INK), beat, focused);
        char sub[40];
        std::snprintf(sub, sizeof(sub), "%s - %s", r.zone, r.beat);
        drawText(fb, kMargin, y + kFontH + 2, sub, palColor(Pal::INK_DIM));
    }

    if (rowCount > visible) {
        // The same right-edge thumb every other windowed list draws, measured in rows.
        const int barX = kActiveW - 3;
        const int trackTop = kRowTop - 2;
        const int trackH = kActiveH - kHintBandH - trackTop;
        fb.fillRect(barX, trackTop, 2, trackH, palColor(Pal::TRACK));
        const int thumbH = std::max(8, trackH * visible / rowCount);
        fb.fillRect(barX, trackTop + trackH * top / rowCount, 2, thumbH,
                    palColor(Pal::INK_DIM));
    }
    drawHintBand(fb, "A NEXT   B READ   C BACK");
}

}  // namespace mal
