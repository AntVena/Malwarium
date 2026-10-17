// recipe_wire.h: the save capacity of the owned-recipe bitset.
#pragma once

namespace mal {

// One bit per MergeRecipe::wire, carried length-prefixed as SaveData::recipeOwned so the
// table can outgrow any fixed width. Kept apart from content_recipes.h so game.h can size
// Game::recipesOwned_ without including the recipe table. Raise it in multiples of 8 (a
// whole byte) with a save-version note; the native gate asserts every wire is under it.
inline constexpr int kMergeRecipeWireCap = 128;
inline constexpr int kMergeRecipeWireBytes = kMergeRecipeWireCap / 8;

}  // namespace mal
