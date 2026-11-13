#include "core/app/game/merge_screen.h"

#include <cstdio>

#include "core/app/game_internal.h"   // kMergeRecipes, kMergeVisibleRows
#include "core/content/registry.h"
#include "core/model/inventory.h"
#include "core/render/canvas.h"
#include "core/render/font.h"
#include "core/render/framebuffer.h"
#include "core/render/palette.h"
#include "core/ui/layout.h"
#include "core/ui/widgets.h"

namespace mal {

void MergeScreen::onButton(MergeHost& host, const ButtonEvent& ev) {
    if (ev.button == Button::C) { host.leaveToCarousel(); return; }
    if (!host.mergeHubUnlocked()) return;   // defensive; the carousel already gates entry
    if (ev.button == Button::A) row_ = stepRow(row_, kMergeRecipeCount, +1);
    else if (ev.button == Button::B) host.craftRecipe(row_);
}

void MergeScreen::draw(Framebuffer& fb, const MergeHost& host) const {
    // The roster outgrew the panel, so the body windows it and the band carries the n/N
    // the scroll would otherwise hide (the note drawItemsList's header prints, and only
    // when it scrolls).
    char pos[12];
    pos[0] = '\0';
    if (kMergeRecipeCount > kMergeVisibleRows)
        std::snprintf(pos, sizeof(pos), "%d/%d", row_ + 1, kMergeRecipeCount);
    drawHeaderBand(fb, "MERGE HUB", pos);

    constexpr int kIndent = kMargin + 6;         // ingredient lines nest under the title
    constexpr int kRowGap = 6;                   // breathing room between recipes

    if (!host.mergeHubUnlocked()) {   // defensive; the carousel already gates entry
        // The HUB is the part that's for sale: the recipes inside it never are.
        drawText(fb, kMargin, 40, "LOCKED - BUY IN SHOP", palColor(Pal::INK_DIM));
        return;
    }

    // An accordion, not a flat list: every recipe shows its title, but only the
    // FOCUSED one unfolds its ingredients. Recipes carry between one and
    // kMaxRecipeInputs of them, so a flat layout would either overflow the panel or
    // have to be sized for the longest recipe and waste the space on every other one.
    //
    // Folded rows are cheap but not free, so the roster is also WINDOWED to
    // kMergeVisibleRows (the same scroll the SHOP list runs, drawHackerSubmenu): a
    // fully-stocked kitchen is more titles than the panel has lines, and a list that
    // ran off the bottom would hide the recipe the cursor is on. The window follows the
    // cursor and clamps, so the first and last screens are full ones.
    int scrollTop = 0;
    if (kMergeRecipeCount > kMergeVisibleRows) {
        scrollTop = row_ - kMergeVisibleRows / 2;
        if (scrollTop < 0) scrollTop = 0;
        if (scrollTop > kMergeRecipeCount - kMergeVisibleRows)
            scrollTop = kMergeRecipeCount - kMergeVisibleRows;
    }
    const int rowsShown = kMergeRecipeCount < kMergeVisibleRows ? kMergeRecipeCount
                                                                : kMergeVisibleRows;

    int rowY = 28;
    for (int v = 0; v < rowsShown; ++v) {
        const int i = scrollTop + v;
        const MergeRecipe& r = kMergeRecipes[i];
        const bool sel = row_ == i;
        if (sel) drawRowCursor(fb, 2, rowY + 1, palColor(Pal::ACCENT));

        const bool owned = host.recipeOwned(i);

        // Title. LOCKED (the recipe hasn't been won off a Decryptogram yet) is a
        // separate state from missing ingredients and always shows: the dish name
        // yields to it and scrolls when focused.
        drawLabelValue(fb, kMargin, rowY, r.displayName,
                       sel ? palColor(Pal::INK) : palColor(Pal::INK_DIM),
                       owned ? "" : "LOCKED", palColor(Pal::INK_DIM), host.screenBeat(), scrollWhen(sel));
        rowY += kLineH;

        if (sel) {
            // One line per ingredient: name + have/need. Craftability reads without
            // colour too: the fraction itself shows the shortfall (e.g. "0/1").
            for (const RecipeInput& in : r.inputs) {
                if (!in.id) continue;
                const ItemDef* def = host.content().item(in.id);
                const int have = host.inventory().count(in.id);
                char qty[12];
                std::snprintf(qty, sizeof(qty), "%d/%d", have, in.qty);
                drawLabelValue(fb, kIndent, rowY, def ? def->displayName : in.id,
                               palColor(Pal::INK_DIM), qty,
                               have >= in.qty ? palColor(Pal::INK) : palColor(Pal::WARN),
                               host.screenBeat(), Overflow::Clip);
                rowY += kLineH;
            }
        } else if (owned) {
            // Folded rows still carry a second, dimmer line: a stocked count rather
            // than the full ingredient breakdown, so the row keeps the shop's
            // bold-title/dim-detail shape without overflowing.
            int total = 0, satisfied = 0;
            for (const RecipeInput& in : r.inputs) {
                if (!in.id) continue;
                ++total;
                if (host.inventory().count(in.id) >= in.qty) ++satisfied;
            }
            char stock[24];
            std::snprintf(stock, sizeof(stock), "%d/%d STOCKED", satisfied, total);
            drawText(fb, kIndent, rowY, stock,
                     satisfied == total ? palColor(Pal::INK_DIM) : palColor(Pal::WARN));
            rowY += kFontH + 3;   // tighter pitch than kLineH: a summary line, not a title
        }
        rowY += kRowGap;
    }

    // B MERGE dims when the selected recipe can't be crafted right now (locked,
    // or an ingredient line above is reading WARN).
    drawHackerHint(fb, "A NEXT", "B MERGE",
                   host.recipeCraftable(row_) ? VerbState::Ready
                                                    : VerbState::Unavailable);
}

}  // namespace mal
