#include "core/app/game.h"
#include "core/app/game_internal.h"
#include "core/app/game_rig_shop.h"

#include <cstdio>

#include "core/render/canvas.h"
#include "core/render/font.h"
#include "core/render/palette.h"
#include "core/ui/layout.h"
#include "core/ui/widgets.h"

// game_merge.cpp — the Hacker-face MERGE HUB (MRG) slot: combines owned ingredient
// items into a rarer one. The HUB is a one-time Rig Shop purchase (kRigRowMergeHub,
// game_rig_shop.h) that flips the MRG carousel slot accessible
// (Game::hackerSlotAccessible / drawHackerCarousel's `mergeUnlocked`).
//
// The RECIPES cooked in it are not for sale at any price: each is won off a first
// solve on the DECRYPTOGRAM (game_cryptogram.cpp's prize ladder), and owning one is
// necessary but not sufficient to craft — the raw ingredients still have to be in the
// bag. Ownership is a bit per recipe wire in recipesOwned_ (save v31's recipesUnlocked
// bitset, save v51), which this file is the only writer of.
//
// So this file owns cooking end to end: the ownership bit, the craft mechanic over
// MergeRecipe (core/content/content_recipes.h), and the screen body.

namespace mal {

void Game::onHackerMerge(const ButtonEvent& ev) {
    if (ev.button == Button::C) { nav_ = Nav::Cursor; return; }
    if (!mergeHubUnlocked()) return;   // defensive; the carousel already gates entry
    if (ev.button == Button::A) {
        hackerMergeRow_ = (hackerMergeRow_ + 1) % kMergeRecipeCount;
    } else if (ev.button == Button::B) {
        craftRecipe(hackerMergeRow_);
    }
}

void Game::debugWinRecipe(int i) { grantRecipe(i); }

// --- Which recipes the operator has ----------------------------------------
//
// One bit per MergeRecipe::wire in recipesOwned_. Every read and write of that packing
// is in this pair, so nothing else has to know the layout (the same discipline the
// quote tiers keep, game_cryptogram.cpp).

bool Game::recipeOwned(int recipeIndex) const {
    if (recipeIndex < 0 || recipeIndex >= kMergeRecipeCount) return false;
    const int wire = kMergeRecipes[recipeIndex].wire;
    if (wire < 0 || wire >= kMergeRecipeWireCap) return false;
    return (recipesOwned_[wire / 8] & (1u << (wire % 8))) != 0;
}

// The same bit, asked the way anything OUTSIDE the engine has to ask it: by the dish.
// A caller that can see kMergeRecipes has the index; one that can only see item ids
// (the 'Pedia payload) has the output, which is the table's unique key.
bool Game::recipeKnown(const char* outputId) const {
    return recipeOwned(recipeIndexByOutput(outputId));
}

void Game::grantRecipe(int recipeIndex) {
    if (recipeIndex < 0 || recipeIndex >= kMergeRecipeCount) return;
    const MergeRecipe& r = kMergeRecipes[recipeIndex];
    if (r.wire < 0 || r.wire >= kMergeRecipeWireCap) return;
    uint8_t& byte = recipesOwned_[r.wire / 8];
    const uint8_t bit = static_cast<uint8_t>(1u << (r.wire % 8));
    if (byte & bit) return;                          // already known — nothing to write
    byte = static_cast<uint8_t>(byte | bit);
    markSaveDirty();
    dirty_ = true;
}

// Is this recipe one the operator is ALLOWED to be handed right now? Not owned yet,
// somewhere to cook it, and — for a dish a storefront stocks — proof they have met the
// dish. The Decryptogram's prize ladder asks this and nothing else does, but it lives
// here beside the ownership bit rather than in the board's file, because what makes a
// recipe grantable is a fact about cooking.
bool Game::recipeGrantable(int recipeIndex) const {
    if (recipeIndex < 0 || recipeIndex >= kMergeRecipeCount) return false;
    if (recipeOwned(recipeIndex)) return false;
    if (!mergeHubUnlocked()) return false;
    for (const char* need : kMergeRecipes[recipeIndex].requiresItems)
        if (need && !itemCollected(need)) return false;
    return true;
}

bool Game::recipeCraftable(int recipeIndex) const {
    if (recipeIndex < 0 || recipeIndex >= kMergeRecipeCount) return false;
    const MergeRecipe& r = kMergeRecipes[recipeIndex];
    if (!recipeOwned(recipeIndex)) return false;   // recipe not won yet
    for (const RecipeInput& in : r.inputs)
        if (in.id && inventory_.count(in.id) < in.qty) return false;
    return true;
}

void Game::craftRecipe(int recipeIndex) {
    if (!recipeCraftable(recipeIndex)) return;
    const MergeRecipe& r = kMergeRecipes[recipeIndex];
    for (const RecipeInput& in : r.inputs)
        if (in.id) inventory_.remove(in.id, in.qty);
    inventory_.add(r.outputId, r.outputQty);
    sweepCollectedItems();   // a crafted dish counts as met right away, not next tick
    ++mergesCooked_;         // the lifetime stove tally (save v56)
    log_.push(LogEventType::ItemUsed, r.logText);
    markSaveDirty();
    dirty_ = true;
}

void Game::drawHackerMerge(Framebuffer& fb) const {
    constexpr int kIndent = kMargin + 6;         // ingredient lines nest under the title
    constexpr int kRowGap = 6;                   // breathing room between recipes

    if (!mergeHubUnlocked()) {   // defensive; the carousel already gates entry
        // The HUB is the part that's for sale — the recipes inside it never are.
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
        scrollTop = hackerMergeRow_ - kMergeVisibleRows / 2;
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
        const bool sel = hackerMergeRow_ == i;
        if (sel) drawRowCursor(fb, 2, rowY + 1, palColor(Pal::ACCENT));

        const bool owned = recipeOwned(i);

        // Title. LOCKED (the recipe hasn't been won off a Decryptogram yet) is a
        // genuinely different state from "missing ingredients", and it is the state the
        // row cannot lose — the dish name yields to it and scrolls when focused.
        drawLabelValue(fb, kMargin, rowY, r.displayName,
                       sel ? palColor(Pal::INK) : palColor(Pal::INK_DIM),
                       owned ? "" : "LOCKED", palColor(Pal::INK_DIM), beat_, sel);
        rowY += kLineH;

        if (sel) {
            // One line per ingredient: name + have/need. Craftability reads without
            // colour too — the fraction itself shows the shortfall (e.g. "0/1").
            for (const RecipeInput& in : r.inputs) {
                if (!in.id) continue;
                const ItemDef* def = registry_.item(in.id);
                const int have = inventory_.count(in.id);
                char qty[12];
                std::snprintf(qty, sizeof(qty), "%d/%d", have, in.qty);
                drawLabelValue(fb, kIndent, rowY, def ? def->displayName : in.id,
                               palColor(Pal::INK_DIM), qty,
                               have >= in.qty ? palColor(Pal::INK) : palColor(Pal::WARN),
                               beat_, false);
                rowY += kLineH;
            }
        } else if (owned) {
            // Folded rows still carry a second, dimmer line — a stocked count rather
            // than the full ingredient breakdown, so the row keeps the shop's
            // bold-title/dim-detail shape without reopening the overflow the
            // accordion exists to avoid.
            int total = 0, satisfied = 0;
            for (const RecipeInput& in : r.inputs) {
                if (!in.id) continue;
                ++total;
                if (inventory_.count(in.id) >= in.qty) ++satisfied;
            }
            char stock[24];
            std::snprintf(stock, sizeof(stock), "%d/%d STOCKED", satisfied, total);
            drawText(fb, kIndent, rowY, stock,
                     satisfied == total ? palColor(Pal::INK_DIM) : palColor(Pal::WARN));
            rowY += kFontH + 3;   // tighter pitch than kLineH: a summary line, not a title
        }
        rowY += kRowGap;
    }

    fb.fillRect(0, kActiveH - 26, kActiveW, 1, palColor(Pal::TRACK));
    drawText(fb, kMargin, kActiveH - 20, "A NEXT", palColor(Pal::INK_DIM));
    // B MERGE dims when the selected recipe can't be crafted right now (locked,
    // or an ingredient line above is reading WARN) — the disabled-button cue.
    const bool selCraftable = recipeCraftable(hackerMergeRow_);
    drawText(fb, kActiveW - kMargin - textWidth("B MERGE"), kActiveH - 20, "B MERGE",
             selCraftable ? palColor(Pal::ACCENT) : palColor(Pal::INK_DIM));
}

}  // namespace mal
