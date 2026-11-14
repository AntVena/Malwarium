#include "core/app/game/item_offer_screen.h"

#include <cstdio>

#include "core/content/defs.h"
#include "core/content/effect_text.h"
#include "core/render/canvas.h"
#include "core/render/font.h"
#include "core/render/framebuffer.h"
#include "core/render/palette.h"
#include "core/ui/layout.h"
#include "core/ui/widgets.h"

namespace mal {

void ItemOfferScreen::onButton(ItemOfferHost& host, const ButtonEvent& ev) {
    // Rebuilt every press: using an item can take its row away (the last of a stack,
    // or a bell that makes every shallower bell pointless), so a cursor kept across
    // presses would point at the wrong row.
    const auto items = host.offerItems(for_);
    const int rows = static_cast<int>(items.size()) + 1;   // + the carry-on row
    if (row_ >= rows) row_ = rows - 1;
    if (ev.button == Button::A) {
        row_ = (row_ + 1) % rows;
    } else if (ev.button == Button::B) {
        if (row_ == rows - 1) { host.finishItemOffer(for_); return; }
        host.useOfferedItem(*items[row_]);
        // Nothing else left worth using: carry straight on rather than leave the
        // player on a list that only says "go".
        if (host.offerItems(for_).empty()) { host.finishItemOffer(for_); return; }
        row_ = 0;
    } else if (ev.button == Button::C) {
        host.leaveItemOffer(for_);
    }
}

void ItemOfferScreen::draw(Framebuffer& fb, const ItemOfferHost& host) const {
    const bool dive = for_ == OfferFor::Dive;
    drawHeaderBand(fb, dive ? "DIVE PREP" : "EGG");
    drawText(fb, kMargin, 28, dive ? "USE ONE BEFORE YOU DIVE?" : "SPEED UP THE EGG?",
             palColor(Pal::INK_DIM));

    const auto items = host.offerItems(for_);
    const int rows = static_cast<int>(items.size()) + 1;
    const int cursor = row_ < rows ? row_ : rows - 1;
    // Compact rows, windowed: there are six dive items, and the focused row's
    // description below is what tells the player which one they want.
    constexpr int kTop = 44;
    constexpr int kPitch = kFontH + 6;
    constexpr int kMaxRows = 5;
    int scrollTop = cursor >= kMaxRows ? cursor - kMaxRows + 1 : 0;
    for (int v = 0; v < kMaxRows && scrollTop + v < rows; ++v) {
        const int r = scrollTop + v;
        const int y = kTop + v * kPitch;
        if (r == cursor) {
            fb.fillRect(4, y - 3, kActiveW - 8, kPitch, palColor(Pal::TRACK));
            drawRowCursor(fb, kMargin, y, palColor(Pal::ACCENT));
        }
        char label[32];
        if (r < static_cast<int>(items.size()))
            std::snprintf(label, sizeof(label), "%s x%d", items[r]->displayName,
                          host.heldCount(items[r]->id));
        else
            std::snprintf(label, sizeof(label), "%s", dive ? "DIVE NOW" : "NOT NOW");
        drawText(fb, 24, y, label, palColor(r == cursor ? Pal::INK : Pal::INK_DIM));
    }

    // The focused item's own description (the row says what it is, this says what it
    // does) under a rule, down to the hint band.
    const int descTop = kTop + kMaxRows * kPitch + 6;
    if (cursor < static_cast<int>(items.size())) {
        const EffectText t = effectText(*items[cursor]);
        const int maxLines = (kActiveH - kHintBandH - descTop) / kLineH;
        drawTextWrapped(fb, kMargin, descTop, kActiveW - 2 * kMargin, t.c_str(),
                        palColor(Pal::INK_DIM), kLineH, maxLines);
    } else {
        drawTextWrapped(fb, kMargin, descTop, kActiveW - 2 * kMargin,
                        dive ? "Start the dive with nothing armed." :
                               "Leave the egg to hatch on its own clock.",
                        palColor(Pal::INK_DIM), kLineH, 3);
    }
    drawHintBand(fb, cursor == rows - 1 ? "A NEXT  B GO  C BACK"
                                        : "A NEXT  B USE  C BACK");
}

}  // namespace mal
