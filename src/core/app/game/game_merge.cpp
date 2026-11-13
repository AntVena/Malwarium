#include "core/app/game.h"
#include "core/app/game_internal.h"

#include <cstdio>


// game_merge.cpp. The Hacker-face MERGE HUB (MRG) slot: combines owned ingredient
// items into a rarer one. The HUB is a one-time Rig Shop purchase (kRigRowMergeHub,
// game_rig_shop.h) that flips the MRG carousel slot accessible
// (Game::hackerSlotAccessible, which drawHackerCarousel's accessible mask is built from).
//
// Its recipes cannot be bought. Each is won by a first solve on the DECRYPTOGRAM
// (game_cryptogram.cpp's prize ladder), and crafting also needs the ingredients. The
// raw ingredients still have to be in the bag. Ownership is a bit per recipe wire in
// RigAccount::recipesOwned (save v31's recipesUnlocked bitset, save v51), which this file
// is the only writer of.
//
// So this file owns cooking end to end: the ownership bit, the craft mechanic over
// MergeRecipe (core/content/content_recipes.h). The screen is MergeScreen
// (game/merge_screen.h).

namespace mal {

void Game::debugWinRecipe(int i) { grantRecipe(i); }

// --- Which recipes the operator has ----------------------------------------
//
// One bit per MergeRecipe::wire in RigAccount::recipesOwned. Every read and write of that
// packing
// is in this pair, so nothing else has to know the layout (the same discipline the
// quote tiers keep, game_cryptogram.cpp).

bool Game::recipeOwned(int recipeIndex) const {
    if (recipeIndex < 0 || recipeIndex >= kMergeRecipeCount) return false;
    const int wire = kMergeRecipes[recipeIndex].wire;
    if (wire < 0 || wire >= kMergeRecipeWireCap) return false;
    return (op_.rig.recipesOwned[wire / 8] & (1u << (wire % 8))) != 0;
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
    uint8_t& byte = op_.rig.recipesOwned[r.wire / 8];
    const uint8_t bit = static_cast<uint8_t>(1u << (r.wire % 8));
    if (byte & bit) return;                          // already known: nothing to write
    byte = static_cast<uint8_t>(byte | bit);
    markSaveDirty();
    dirty_ = true;
}

// Is this recipe one the operator is ALLOWED to be handed right now? Not owned yet,
// somewhere to cook it, and (for a dish a storefront stocks) proof they have met the
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
    ++op_.tallies.mergesCooked;         // the lifetime stove tally (save v56)
    log_.push(LogEventType::ItemUsed, r.logText);
    markSaveDirty();
    dirty_ = true;
}

}  // namespace mal
