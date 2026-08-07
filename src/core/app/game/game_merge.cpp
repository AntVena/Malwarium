#include "core/app/game.h"
#include "core/app/game_internal.h"
#include "core/app/game_rig_shop.h"

#include <cstdio>

#include "core/render/canvas.h"
#include "core/render/font5x7.h"
#include "core/render/palette.h"
#include "core/ui/layout.h"
#include "core/ui/widgets.h"

// game_merge.cpp — the Hacker-face MERGE HUB (MRG) slot: combines two owned
// ingredient items into a rarer one. Unlocked itself as a one-time f Rig Shop
// purchase (kRigRowMergeHub, game_rig_shop.h) that flips the MRG carousel slot
// accessible (Game::hackerSlotAccessible / drawHackerCarousel's `mergeUnlocked`).
// Each RECIPE is a SEPARATE one-time Rig Shop row (kRigRowRecipeNoodles/Nachos) —
// owning a recipe is necessary but not sufficient to craft it here, the raw
// ingredients still have to be in the bag; buying + row rendering are generic
// (Game::buyRigUpgrade / drawHackerSubmenu's SHOP branch, game_hacker.cpp). This
// file only owns the craft mechanic: MergeRecipe (game_internal.h) + the screen body.

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

void Game::debugBuyRecipe(int i) {
    if (i < 0 || i >= kMergeRecipeCount) return;
    buyRigUpgrade(kMergeRecipes[i].rigRow);
}

bool Game::recipeCraftable(int recipeIndex) const {
    if (recipeIndex < 0 || recipeIndex >= kMergeRecipeCount) return false;
    const MergeRecipe& r = kMergeRecipes[recipeIndex];
    if (!rigUpgradeOwned(r.rigRow)) return false;   // recipe not bought
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
    log_.push(LogEventType::ItemUsed, r.logText);
    markSaveDirty();
    dirty_ = true;
}

void Game::drawHackerMerge(Framebuffer& fb) const {
    constexpr int kIndent = kMargin + 6;         // ingredient lines nest under the title
    constexpr int kRowGap = 6;                   // breathing room between recipes

    if (!mergeHubUnlocked()) {   // defensive; the carousel already gates entry
        drawText(fb, kMargin, 40, "LOCKED - BUY IN SHOP", palColor(Pal::INK_DIM));
        return;
    }

    // An accordion, not a flat list: every recipe shows its title, but only the
    // FOCUSED one unfolds its ingredients. Recipes carry between one and
    // kMaxRecipeInputs of them, so a flat layout would either overflow the panel or
    // have to be sized for the longest recipe and waste the space on every other one.
    // Unfolding one at a time keeps the whole roster on screen whatever it grows into.
    int rowY = 28;
    for (int i = 0; i < kMergeRecipeCount; ++i) {
        const MergeRecipe& r = kMergeRecipes[i];
        const bool sel = hackerMergeRow_ == i;
        if (sel) drawRowCursor(fb, 2, rowY + 1, palColor(Pal::ACCENT));

        const bool owned = rigUpgradeOwned(r.rigRow);

        // Title. LOCKED (recipe not bought yet) is a genuinely different state
        // from "missing ingredients" — it still gets a tag, but a short one, so
        // it never collides with the name.
        drawText(fb, kMargin, rowY, r.displayName,
                 sel ? palColor(Pal::INK) : palColor(Pal::INK_DIM));
        if (!owned) {
            const char* tag = "LOCKED";
            drawText(fb, kActiveW - kMargin - textWidth(tag), rowY, tag,
                     palColor(Pal::INK_DIM));
        }
        rowY += kLineH;

        if (sel) {
            // One line per ingredient: name + have/need. Craftability reads without
            // colour too — the fraction itself shows the shortfall (e.g. "0/1").
            for (const RecipeInput& in : r.inputs) {
                if (!in.id) continue;
                const ItemDef* def = registry_.item(in.id);
                const int have = inventory_.count(in.id);
                drawText(fb, kIndent, rowY, def ? def->displayName : in.id,
                         palColor(Pal::INK_DIM));
                char qty[12];
                std::snprintf(qty, sizeof(qty), "%d/%d", have, in.qty);
                drawText(fb, kActiveW - kMargin - textWidth(qty), rowY, qty,
                         have >= in.qty ? palColor(Pal::INK) : palColor(Pal::WARN));
                rowY += kLineH;
            }
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
