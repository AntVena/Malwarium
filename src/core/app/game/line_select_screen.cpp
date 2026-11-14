#include "core/app/game/line_select_screen.h"

#include "core/content/defs.h"
#include "core/render/canvas.h"
#include "core/render/font.h"
#include "core/render/framebuffer.h"
#include "core/render/palette.h"
#include "core/ui/layout.h"

namespace mal {

void LineSelectScreen::onButton(LineSelectHost& host, const ButtonEvent& ev) {
    // C BACKS OUT TO ARCH, but only with something on the rack. The rule is not "the
    // hatch can't be cancelled", it is "an empty save has no pet to return to", and
    // reaching this modal from an ARCH Store makes that false: the pet is right there on
    // the shelf, frozen, one Deploy away. Cancelling into a habitat with nothing living
    // in it is not offered, so a device with an empty rack cannot back out at all.
    const auto lines = host.availableEggLines();   // only UNLOCKED lines (matches the draw)
    if (lines.empty()) return;
    const int n = static_cast<int>(lines.size());
    if (ev.button == Button::A) row_ = (row_ + 1) % n;
    else if (ev.button == Button::B) host.layEgg(lines[row_ % n]);
    else if (ev.button == Button::C && !host.archRackEmpty()) host.archReturnFromLineSelect();
}

void LineSelectScreen::draw(Framebuffer& fb, const LineSelectHost& host) const {
    // Full 224x224, no track/header chrome. Grayscale-safe: the highlighted line carries
    // BOTH a ">" cursor glyph (shape) and INK vs INK_DIM (luminance), so a desaturated
    // screenshot still reads the selection.
    fb.clear(palColor(Pal::PAPER));
    const char* title = "NEW SPECIMEN";
    drawText(fb, (kActiveW - textWidth(title)) / 2, 46, title, palColor(Pal::INK));
    const char* sub = "CHOOSE A LINE TO DECRYPT";
    drawText(fb, (kActiveW - textWidth(sub)) / 2, 46 + kFontH + 5, sub,
             palColor(Pal::INK_DIM));

    const auto lines = host.availableEggLines();
    const int n = static_cast<int>(lines.size());
    const int sel = n > 0 ? row_ % n : 0;
    const int rowH = 24;
    const int top = 100;
    for (int i = 0; i < n; ++i) {
        const bool on = (i == sel);
        const int y = top + i * rowH;
        const char* name = lines[i]->displayName;
        const int nameW = textWidth(name);
        const int x = (kActiveW - nameW) / 2;
        if (on) drawText(fb, x - 16, y, ">", palColor(Pal::ACCENT));
        drawText(fb, x, y, name, on ? palColor(Pal::INK) : palColor(Pal::INK_DIM));
    }

    const char* hint = "A NEXT   B SELECT";
    drawText(fb, (kActiveW - textWidth(hint)) / 2, kActiveH - 26, hint,
             palColor(Pal::INK_DIM));
}

}  // namespace mal
