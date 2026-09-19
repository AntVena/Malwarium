// story_screen.h — the chapter READER, and the ARCHIVE read back through afterwards.
// A prose page (core/ui/prose_page.h) plus a deadline; the countdown rule is what stops
// a page turning itself from reading as a crash.
#pragma once

#include "core/content/story.h"
#include "core/render/canvas.h"
#include "core/ui/layout.h"
#include "core/ui/prose_page.h"
#include "core/ui/widgets.h"

namespace mal {

class Framebuffer;

// Public: the engine's scroll stepping and the draw must agree where the flow starts.
constexpr int kStoryTitleY = 28;
constexpr int kStoryBodyTop = 44;
constexpr int kStoryClockH = 2;
constexpr int kStoryClockY = kActiveH - kHintBandH - kStoryClockH - 2;
static_assert(kStoryClockY > kStoryBodyTop,
              "the countdown rule has to sit below the first line of prose");

// `windows` is passed, not re-derived, so page and press agree on the length.
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

struct StoryArchiveRow {
    const char* title = "";
    const char* zone = "";   // storyZoneBadge
    const char* beat = "";   // storyBeatWord
};

constexpr int kStoryArchiveRowH = 22;
constexpr int kStoryArchiveRows =
    (kActiveH - kHintBandH - kRowTop) / kStoryArchiveRowH;

// An EMPTY archive says so in words: blank and broken look identical.
void drawStoryArchive(Framebuffer& fb, const StoryArchiveRow* rows, int rowCount,
                      int cursor, int beat);

}  // namespace mal
