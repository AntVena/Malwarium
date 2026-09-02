// prose_page.h — the flowed, scrollable page of NAME + PROSE rows.
//
// Four screens now stack rows of the same shape: a name line, an optional short tag
// right-aligned against it, and the row's own authored text wrapped whole underneath.
// STAT's LOADOUT and BUFFS pages, ROCK THE DOCK's opponent SCOUT sheet, and its
// briefing are all that page — so the flow lives here once instead of in each of them.
//
// WHY IT FLOWS RATHER THAN BUDGETS. The text is AUTHORED PROSE, written to be read and
// not sized to the panel, so a fixed per-row line budget can only be wrong in one of
// two directions: too small cuts the sentence (a mod that wraps to four lines under a
// three-line budget), too large runs the last visible row off the foot of the screen
// and over the hint band. So nothing budgets — every row is MEASURED (textWrapLines)
// and takes exactly the height its own text needs, rows pack down from the top, and
// whatever doesn't fit is simply where the next scroll window starts.
//
// The scroll STEP is therefore "however many rows were on screen", which is why
// proseRowsFitting is exported: the engine advancing the window and the renderer
// drawing it must agree about where the next one begins, and the alternative is two
// answers to that question, which reads as skipped or repeated rows.
#pragma once

#include <vector>

#include "core/content/effect_text.h"
#include "core/render/font.h"   // kFontH — the row heights are derived from it
#include "core/render/canvas.h"
#include "core/ui/layout.h"
#include "core/ui/widgets.h"    // kHintBandH — the foot the flow stops at

namespace mal {

class Framebuffer;

// One row of a prose page.
//
// `label` is BORROWED — every producer names either a content row's displayName
// (which outlives any frame) or a string literal, so nothing here owns a copy.
// `tag` is the short right-aligned value beside the name ("DEFAULT", "ATK 18"); it is
// stored rather than borrowed because most producers FORMAT it. Empty = no tag. A HEADER
// row may carry one too — a running total for the group it opens — and draws it dim
// alongside the heading.
// `body` is the prose, already templated (effect_text.h). Empty on a header row.
struct ProseRow {
    bool header = false;        // a section heading (MOVES/MODS) — dim, no prose
    const char* label = "";
    char tag[12] = {0};
    EffectText body;
};

// Prose-flow geometry. Public because the pages that lay their own header out above
// the flow have to know where it starts and where it must stop.
constexpr int kProseW = kActiveW - 2 * kMargin;
constexpr int kProseLineH = kFontH + 2;
constexpr int kProseNameGap = 3;   // name line -> its first prose line
constexpr int kProseRowGap = 8;    // last prose line -> the next row's name
// A section header belongs to the block UNDER it, so it takes its air from ABOVE: a
// wide lead-in fences one group off from the last, and a tight tail keeps the label
// attached to the entry it names. The two have to stay ordered
// kProseHeaderTail < kProseRowGap < kProseGroupLead or the page loses its grouping —
// two pixels of difference cannot say "new section", and the dim heading would be left
// carrying the grouping alone. The lead is suppressed on the FIRST row: the header
// band above it is already the fence.
constexpr int kProseGroupLead = 14;
constexpr int kProseHeaderTail = 3;
constexpr int kProseHeaderH = kFontH + kProseHeaderTail;
// The foot of the flow: the hint band's strip is reserved whether or not a hint is
// drawn, so a page can never discover it after a row is already using the space.
constexpr int kProseBottom = kActiveH - kHintBandH;

// The measured height of one row's block. The lead above a header is part of that
// header's HEIGHT rather than a gap the draw loop adds, so the fit maths and the
// reader see the same page.
std::vector<int> proseRowHeights(const std::vector<ProseRow>& rows);

// How many rows starting at `top` fit between `rowTop` and the foot. Always at least
// one: a row taller than the whole body still draws (clipped) rather than stalling the
// scroll on a window of zero rows, which would never reach the end and wrap.
int proseRowsFitting(const std::vector<ProseRow>& rows, int top, int rowTop);

// Draw the window of `rows` starting at `scrollTop`, from `rowTop` down. When the list
// outruns one screen this also draws the right-edge scrollbar and `hint` in the hint
// band — a page that gives a key meaning has to name it, and the two only appear
// together because there is nothing to scroll otherwise.
//
// `scrollTop` is CLAMPED here, not wrapped: the wrap belongs to whoever owns the
// press, which is the only place that knows the total row count as well.
void drawProseRows(Framebuffer& fb, const std::vector<ProseRow>& rows, int scrollTop,
                   int rowTop, int beat, const char* hint = "B SCROLL");

// Set a row's formatted tag. Truncates rather than overflowing; a caller that needs
// more than this holds is describing something that belongs in the prose instead.
void setProseTag(ProseRow& row, const char* fmt, ...);

}  // namespace mal
