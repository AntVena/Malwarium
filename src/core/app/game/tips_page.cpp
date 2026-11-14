#include "core/app/game/tips_page.h"

#include "core/app/game/tip_card_screen.h"   // TipCardScreen::title
#include "core/render/canvas.h"
#include "core/render/font.h"
#include "core/render/framebuffer.h"
#include "core/render/palette.h"
#include "core/ui/layout.h"
#include "core/ui/widgets.h"

namespace mal {

CfgMove TipsPage::onButton(TipsHost& host, const ButtonEvent& ev) {
    const std::vector<Tip> tips = host.replayableTips();
    const int n = static_cast<int>(tips.size());
    if (ev.button == Button::A && n > 0) {
        row_ = stepRow(row_, n, +1);
        host.markDirty();
    } else if (ev.button == Button::B && n > 0) {
        if (row_ >= n) row_ = 0;
        host.openTip(tips[row_]);
    } else if (ev.button == Button::C) {
        return CfgMove::leave();
    }
    return CfgMove::stay();
}

void TipsPage::draw(Framebuffer& fb, const TipsHost& host) const {
    fb.clear(palColor(Pal::PAPER));
    drawHeaderBand(fb, "TIPS");
    const std::vector<Tip> tips = host.replayableTips();
    const int n = static_cast<int>(tips.size());
    if (n == 0) {
        const char* none = "- NONE SHOWN YET -";
        drawText(fb, (kActiveW - textWidth(none)) / 2, kActiveH / 2, none,
                 palColor(Pal::INK_DIM));
        drawHintBand(fb, "C BACK");
        return;
    }
    const int cursor = row_ < n ? row_ : 0;
    const int scrollTop = listScrollTop(cursor, n, kVisibleRows);
    for (int v = 0; v < kVisibleRows && scrollTop + v < n; ++v) {
        const int i = scrollTop + v;
        const int y = kRowTop + v * kRowH;
        if (i == cursor) drawRowSelection(fb, y, kRowH);
        drawText(fb, 22, y + (kRowH - kFontH) / 2, TipCardScreen::title(tips[i]),
                 i == cursor ? palColor(Pal::ACCENT) : palColor(Pal::INK));
    }
    drawHintBand(fb, "A NEXT  B READ  C BACK");
}

}  // namespace mal
