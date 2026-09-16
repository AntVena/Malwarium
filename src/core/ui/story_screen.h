// story_screen.h — the two STORY screens: the CHAPTER READER the walk interrupts you
// with, and the ARCHIVE you go back through afterwards.
//
// THE READER IS A PROSE PAGE (core/ui/prose_page.h), the same flow STAT's LOADOUT and
// ROCK THE DOCK's briefing are drawn with. A story panel is a heading and its prose,
// which is exactly a ProseRow, so the break points, the scrollbar and the window
// stepping are all the ones a player has already learned on those screens. What this
// file adds on top is the two things a chapter has and a reference page does not: a
// TITLE above the flow, and a DEADLINE under it.
//
// WHY THE DEADLINE IS DRAWN. A chapter advances on its own after kStoryPanelMs so a
// hands-off walk (auto-progress, EXPL running in the background) is never parked on a
// page waiting for a thumb. A screen that moves by itself with nothing saying it is
// about to has read as a crash in every playtest of anything that does it, so the
// remaining time is a thin rule above the hint band — a second channel for what the
// hint band already says in words, and the reason the page is allowed to move at all.
//
// THE ARCHIVE IS A PLAIN LIST. Two-line rows, exactly the shape EXPL's own list uses:
// the chapter title over a dim line naming the zone and which of its four beats this
// was. Only chapters the walk has already fired are listed, which is what makes it an
// archive rather than a table of contents — it can spoil nothing, because everything in
// it has been read once already.
#pragma once

#include "core/content/story.h"
#include "core/render/canvas.h"
#include "core/ui/layout.h"
#include "core/ui/prose_page.h"
#include "core/ui/widgets.h"

namespace mal {

class Framebuffer;

// --- The reader --------------------------------------------------------------

// Where the flow starts: under the header band and the chapter title that sits below
// it. Public because the engine advancing the scroll window and the renderer drawing it
// have to agree about where the page begins, the same contract prose_page.h states for
// ROCK THE DOCK's two readers.
constexpr int kStoryTitleY = 28;
constexpr int kStoryBodyTop = 44;
// The countdown rule's own strip, immediately above the hint band.
constexpr int kStoryClockH = 2;
constexpr int kStoryClockY = kActiveH - kHintBandH - kStoryClockH - 2;
static_assert(kStoryClockY > kStoryBodyTop,
              "the countdown rule has to sit below the first line of prose");

// One chapter page. `rows` is the whole chapter flowed (every panel, in order);
// `scrollTop` is the row the visible window opens on, `window`/`windows` are which
// screenful of it this is and how many there are, and `remainingPct` is how much of
// this window's auto-advance budget is left (100 = just arrived, 0 = about to turn).
//
// `windows` is passed rather than re-derived so the page and whoever owns the press can
// never disagree about how long the chapter is — the same reason proseRowsFitting is
// exported at all.
struct StoryPageView {
    const char* title = "";
    int scrollTop = 0;
    int window = 0;
    int windows = 1;
    int remainingPct = 100;
    int beat = 0;
};
void drawStoryPage(Framebuffer& fb, const std::vector<ProseRow>& rows,
                   const StoryPageView& v);

// --- The archive -------------------------------------------------------------

// One listed chapter, already resolved to the strings the row draws — the screen stays
// content-agnostic exactly as ShopRowView keeps the storefront renderer registry-
// agnostic (expl_screen.h).
struct StoryArchiveRow {
    const char* title = "";
    const char* zone = "";   // the zone's short badge (storyZoneBadge)
    const char* beat = "";   // which of its four (storyBeatWord)
};

// The archive's row pitch and how many fit under the header band. Two lines each, so
// the pitch is tighter than layout.h's kRowH and the count is derived from it rather
// than from kVisibleRows — a list of titles is read by scanning, and a row that spends
// a whole kRowH on two short lines wastes a third of the screen.
constexpr int kStoryArchiveRowH = 22;
constexpr int kStoryArchiveRows =
    (kActiveH - kHintBandH - kRowTop) / kStoryArchiveRowH;

// The unlocked-chapter list, windowed on `cursor` (listScrollTop) with the right-edge
// scrollbar every other list on the device draws. An EMPTY archive draws its own line
// saying so rather than a bare screen: a player who opens this before the walk has
// fired anything needs to be told the shelf fills itself, not left looking at nothing.
void drawStoryArchive(Framebuffer& fb, const StoryArchiveRow* rows, int rowCount,
                      int cursor, int beat);

}  // namespace mal
