// spec_sheet.cpp — the shared name / readout / prose panel (see spec_sheet.h).
#include "core/ui/spec_sheet.h"

#include "core/render/canvas.h"
#include "core/render/font5x7.h"
#include "core/render/framebuffer.h"
#include "core/render/palette.h"
#include "core/ui/widgets.h"

namespace mal {

namespace {
constexpr int kLineH = 12;      // one text line + leading, shared by grid and prose
constexpr int kBlockGap = 4;    // between the name, the readout and the prose
}  // namespace

int proseLineCount(int w, const char* prose) {
    return (prose && prose[0]) ? textWrapLines(prose, w) : 0;
}

SpecSheetLayout drawSpecSheet(Framebuffer& fb, int x, int y, int w, int maxY,
                              const SpecSheet& s) {
    SpecSheetLayout out;
    int cy = y;

    if (s.name && s.name[0]) {
        drawText(fb, x, cy, s.name, palColor(Pal::INK));
        if (s.tag && s.tag[0])
            drawText(fb, x + w - textWidth(s.tag), cy, s.tag, s.tagColor);
        cy += kLineH;
    }

    // The readout takes exactly the height it needs — reserved, never clipped, so a
    // magnitude can't be the thing that falls off the bottom.
    if (s.rowCount > 0) {
        cy = drawSpecGrid(fb, x, cy, w, s.rows, s.rowCount, kLineH,
                          palColor(Pal::INK_DIM), palColor(Pal::ACCENT));
        cy += kBlockGap;
    }

    // Prose gets whatever is left. If that isn't all of it, say so via the layout
    // rather than quietly dropping the tail — the caller turns that into a scroll.
    const int total = proseLineCount(w, s.prose);
    if (total > 0) {
        const int room = (maxY - cy) / kLineH;
        if (room > 0) {
            const int first = s.proseScroll < 0 ? 0
                            : s.proseScroll > total - 1 ? total - 1
                                                        : s.proseScroll;
            cy = drawTextWrapped(fb, x, cy, w, s.prose, palColor(Pal::INK), kLineH,
                                 room, first);
            out.proseScrollable = total > room;
        } else {
            out.proseScrollable = true;
        }
    }
    out.endY = cy;
    return out;
}

}  // namespace mal
