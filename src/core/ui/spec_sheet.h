// spec_sheet.h — the one layout every "what does this thing do" panel uses.
//
// A move / mod / item / rig upgrade is described by the same three things, so they
// get one renderer instead of four private layouts that each rediscover the maths:
//
//   1. a NAME line with a right-aligned tag (ATK/DEF, +DEF, a rarity word),
//   2. the derived readout as an aligned grid (widgets.h's drawSpecGrid), and
//   3. the row's own prose.
//
// The layout rule that makes it fit, in both senses: **the readout is bounded and is
// never cut; the prose is elastic and scrolls.** The readout comes from a fixed
// generated vocabulary (effect_text.h's specRows), so its height is knowable and can
// simply be reserved — a magnitude never silently vanishes off the end of a line.
// Prose is authored, so its length isn't knowable: it takes whatever is left and
// reports the overflow, and the caller offers a scroll rather than truncating.
//
// Consumers: mods_screen (picker + detail), items_screen (detail), train_screen
// (picker + detail).
#pragma once

#include "core/content/effect_text.h"
#include "core/render/color.h"

namespace mal {

class Framebuffer;

// What to describe. `name`/`tag` are optional (a picker already names the focused row
// in its list, so it passes nullptr and starts at the readout).
struct SpecSheet {
    const char* name = nullptr;
    const char* tag = nullptr;          // right-aligned beside the name
    Rgb565 tagColor = 0;
    const SpecRow* rows = nullptr;      // the derived readout
    int rowCount = 0;
    const char* prose = nullptr;
    int proseScroll = 0;                // first prose line to draw (the caller's page offset)
};

// What was drawn, so a caller can flow a footer under it and decide whether to offer
// a scroll.
struct SpecSheetLayout {
    int endY = 0;                       // y just past the last line drawn
    bool proseScrollable = false;       // the prose needs more room than it has
};

// Draw into the band [y, maxY) across `w` pixels from `x`. Never draws past maxY.
// The gap drawSpecSheet leaves between the readout block and the prose under it.
// Public because a screen sizing its own prose band has to subtract it — a panel
// budget that forgets it reads one line high, and the line it loses is the tail of a
// description, cut with nothing on screen to say so.
constexpr int kSpecBlockGap = 4;

SpecSheetLayout drawSpecSheet(Framebuffer& fb, int x, int y, int w, int maxY,
                              const SpecSheet& s);

// Total prose lines at this width — for a caller sizing its own scroll bounds.
int proseLineCount(int w, const char* prose);

}  // namespace mal
