// content_recipes.h — the MERGE HUB recipe table.
//
// One content table (see content_tables.h): the rows are in content_recipes.cpp and
// nothing outside this pair knows how they are stored. Read by game_merge.cpp
// (craft/render), game_cryptogram.cpp (a solved board's prize), game_achievements.h
// (the AchSeries::RecipesKnown total) and the native item earn-path test — a recipe's
// OUTPUT is earnable by being crafted, which the generic starting-shelf, cache-pool,
// shop, container and warp checks cannot see on their own.
//
// It is content rather than engine for the reason every other table here is: adding a
// dish is a row, and tools/dump_content.cpp has to be able to publish it without
// linking an engine.
#pragma once

#include <cstring>

#include "core/app/game_rig_shop.h"  // kMergeRecipeWireCap — the save bit a row claims

namespace mal {

inline constexpr int kMaxRecipeInputs = 4;

// Items a recipe must not be handed over before the operator has MET
// (MergeRecipe::requiresItems).
inline constexpr int kMaxRecipeRequiredItems = 2;

// One ingredient a recipe consumes. `id` nullptr = unused slot, so a two-ingredient
// recipe writes two and stops.
struct RecipeInput {
    const char* id = nullptr;
    int qty = 0;
};

struct MergeRecipe {
    const char* displayName;
    RecipeInput inputs[kMaxRecipeInputs];
    const char* outputId; int outputQty;
    // Stable save identity — the owned-recipe bitmask's bit index (SaveData::
    // recipesUnlocked). NEVER reused, never renumbered: rows may be reordered or
    // retired freely, but a retired recipe's number stays burned, exactly as a quote's
    // wire does. The gate asserts uniqueness and that every number is under
    // kMergeRecipeWireCap.
    int wire;
    const char* logText;       // Hacker-Log line written on a successful craft
    // Item ids the operator must have COLLECTED (ever held — Game::itemCollected, not
    // current possession) before the prize ladder will hand this recipe over. A
    // nullptr slot ends the list; an all-null array (what most rows carry) = no gate.
    // For a dish sold somewhere: meet it first, then win the method for it.
    const char* requiresItems[kMaxRecipeRequiredItems] = {};
};
extern const MergeRecipe kMergeRecipes[];

extern const int kMergeRecipeCount;

// The recipe that MAKES `outputId`, or -1 — how the Decryptogram's prize ladder names
// a recipe: by the dish you win the ability to cook, not by an index into this table.
// Outputs are unique across the table (the gate asserts it), so the dish IS the key.
inline int recipeIndexByOutput(const char* outputId) {
    if (!outputId) return -1;
    for (int i = 0; i < kMergeRecipeCount; ++i)
        if (std::strcmp(kMergeRecipes[i].outputId, outputId) == 0) return i;
    return -1;
}

// How many ingredient slots `r` actually uses.
inline int recipeInputCount(const MergeRecipe& r) {
    int n = 0;
    for (const RecipeInput& in : r.inputs)
        if (in.id) ++n;
    return n;
}

// Does any recipe consume `itemId`? Read off the rows rather than a second,
// hand-authored "is this an ingredient" flag on the item itself — a food row
// already says everything a recipe needs to know about it by appearing in one
// (or more) `inputs[]` list, and a dish that is ALSO an input further down the
// chain (Hashed Browns, folded into Salted&Hashed Browns) is correctly both.
inline bool itemIsRecipeIngredient(const char* itemId) {
    if (!itemId) return false;
    for (int i = 0; i < kMergeRecipeCount; ++i)
        for (const RecipeInput& in : kMergeRecipes[i].inputs)
            if (in.id && std::strcmp(in.id, itemId) == 0) return true;
    return false;
}

}  // namespace mal
