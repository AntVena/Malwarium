#pragma once

// Private, engine-internal helpers shared by more than one Game unit
// (src/core/app/game/*.cpp). NOT part of the public contract — keep this out of
// game.h. Anything used by only a single unit belongs in that unit's own
// anonymous namespace, not here.

#include <algorithm>
#include <cstring>

#include "core/app/game_rig_shop.h"   // kRigVisibleRows/kRigRowPitch — the SHOP list's geometry
#include "core/model/combat.h"        // Combatant::BackupUse / Combat::Outcome
#include "core/render/canvas.h"       // kActiveW — the bar hangs off the right edge
#include "core/render/framebuffer.h"
#include "core/render/palette.h"
#include "tunables.h"

namespace mal {

// Which of the three Backup Drive achievements a finished fight earned, or nullptr for
// none — the drive unused, or a Fled fight that settled nothing. Pure over the only two
// facts the answer depends on (what the drive did, how the fight ended), so the whole
// mapping is assertable without staging three fights to reach it. Defined in
// game_combat.cpp, which is also its only caller (Game::settleBackupDrive).
const char* backupDriveAchievement(Combatant::BackupUse used, Combat::Outcome outcome);

// The slim right-edge scroll bar (UI_SCROLLBAR) --------------------
// Drawn only when a list is longer than its window, so a list that fits shows no
// chrome at all. `trackTop`/`trackH` are the window it runs beside; `total` and
// `visible` are row counts and `scrollTop` the first visible row. Every scrolling
// list on the Hacker face shares this one thumb calculation — two lists drifting
// apart on where the thumb sits is exactly the kind of difference nobody reports
// but everybody feels.
inline void drawScrollbar(Framebuffer& fb, int trackTop, int trackH, int total,
                          int scrollTop, int visible) {
    if (total <= visible) return;
    const int barX = kActiveW - 3;
    fb.fillRect(barX, trackTop, 2, trackH, palColor(Pal::TRACK));
    const int thumbH = std::max(8, trackH * visible / total);
    const int thumbY = trackTop + trackH * scrollTop / total;
    fb.fillRect(barX, thumbY, 2, thumbH, palColor(Pal::INK_DIM));
}

// Combat-stat labels ---------------------------------------------
// Indexed to statPoints_/lastLevelUpStat_: 0 power · 1 defense · 2 speed · 3
// max-Health. Shared by the Rollback picker + its log line (game_items), the
// post-encounter level-up readout (game_render), and any other unit that needs to
// name a levelled stat — so the labels live in exactly one place.
inline const char* levelStatName(int i) {
    static const char* const kNames[kLevelStatCount] = {
        "POWER", "DEFENSE", "SPEED", "MAX-HP"};
    return (i >= 0 && i < kLevelStatCount) ? kNames[i] : "?";
}

// Rig Shop list layout ---------------------------------------
// Row indices/data now live in game_rig_shop.h (RigRow/kRigUpgrades) — these two are
// pure render-layout constants for game_hacker.cpp's SHOP list (kept here, not there,
// since they're presentation, not rig data).
constexpr int kRigVisibleRows = 3;   // rows on screen at once
constexpr int kRigRowPitch = 54;

// Merge Hub recipes (Hacker MRG) ---------------------------------
// A recipe combines owned ingredient items into a rarer one. Shared by
// game_merge.cpp (craft/render), game_cryptogram.cpp (a solved board's prize) and the
// native item earn-path coverage test: a recipe's OUTPUT is earnable by being crafted,
// which the generic starting-shelf/cache-pool/shop/container/warp checks can't see on
// their own.
//
// A recipe is WON, NEVER BOUGHT. It has no price, no levels and no storefront row —
// the only thing that hands one over is a first solve on the DECRYPTOGRAM, in the order
// content_quotes.h's prize ladder names them. So this table owns the whole of a
// recipe: what it consumes, what it makes, what it must not be handed over before, and
// the save bit that says you have it.
//
// Adding a recipe is one row here plus its output dish on the prize ladder — a recipe
// missing from the ladder is one nothing can hand over, which the native gate fails on.
// The rows themselves are free to be reordered; `wire` is what the save holds onto.
constexpr int kMaxRecipeInputs = 4;

// Items a recipe must not be handed over before the operator has MET
// (MergeRecipe::requiresItems).
constexpr int kMaxRecipeRequiredItems = 2;

// Capacity for the save's owned-recipe bitmask, in wire numbers — one bit each, so the
// whole set is a single uint32 on the blob (SaveData::recipesUnlocked). Raise it (and
// the field's width, with a save-version note) as the table approaches it.
constexpr int kMergeRecipeWireCap = 32;

// Recipes on screen at once in the MERGE HUB (drawHackerMerge's window, the sibling of
// kRigVisibleRows above). Five is what the panel holds in the worst case: four folded
// titles plus the focused row unfolding kMaxRecipeInputs ingredient lines, all above
// the A NEXT / B MERGE footer rule.
constexpr int kMergeVisibleRows = 5;

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
inline const MergeRecipe kMergeRecipes[] = {
    {"Pwnzu-Patched Noodles", {{"null_noodles", 1}, {"pwnzu_sauce", 1}},
     "pwnzu_patched_noodles", 1, /*wire=*/0, "MERGED PATCHED NOODLES"},
    {"Fully-Stacked Nachos", {{"tortilla_chip", 1}, {"osi_dip", 1}},
     "fully_stacked_nachos", 1, /*wire=*/1, "MERGED STACKED NACHOS"},
    // The pantry's own two: a four-ingredient base dish, then a one-ingredient pass
    // back through the pan that turns it into the better version of itself. Both are
    // stocked at Moor-to-Moor, which is why both carry the met-the-dish gate.
    {"Hashed Browns",
     {{"c_salt", 1}, {"grepsed_oil", 1}, {"cronstarch", 1}, {"polltatoes", 1}},
     "hashed_browns", 1, /*wire=*/2, "MERGED HASHED BROWNS",
     {"hashed_browns", "salted_hashed_browns"}},
    {"Salted&Hashed Browns", {{"c_salt", 1}, {"hashed_browns", 1}},
     "salted_hashed_browns", 1, /*wire=*/3, "MERGED SALTED BROWNS",
     {"hashed_browns", "salted_hashed_browns"}},
    // The rest of the pantry, cooked. Between them these use every staple but Spoiled
    // Macrol (which is Pier-to-Peer's currency, not an ingredient), so no shelf of the
    // pantry is only ever eaten raw. Quantities are the point of each row as much as the
    // ids are: a dish that wants four Spam is a dish that clears the folder.
    {"Cracquettes",
     {{"spam", 4}, {"breadcrumbs", 2}, {"regeggs", 1}, {"grepsed_oil", 1}},
     "cracquettes", 1, /*wire=*/4, "MERGED CRACQUETTES"},
    {"Hackshuka",
     {{"regeggs", 2}, {"data_leek", 1}, {"boolean_cubes", 1}, {"c_salt", 1}},
     "hackshuka", 1, /*wire=*/5, "MERGED HACKSHUKA"},
    {"Applet Turnover",
     {{"applets", 2}, {"syntactic_sugar", 1}, {"cronstarch", 1}, {"vanilla_extract", 1}},
     "applet_turnover", 1, /*wire=*/6, "MERGED APPLET TURNOVER"},
    {"Serial Bar",
     {{"universal_cereal_box", 1}, {"breadcrumbs", 2}, {"syntactic_sugar", 1},
      {"boolean_cubes", 1}},
     "serial_bar", 1, /*wire=*/7, "MERGED SERIAL BAR"},
    {"Macrol Fry-Up",
     {{"fresh_macrol", 2}, {"grepsed_oil", 1}, {"breadcrumbs", 1}, {"c_salt", 1}},
     "macrol_fry_up", 1, /*wire=*/8, "MERGED MACROL FRY-UP"},
    {"Vanilla Java Roast",
     {{"java", 2}, {"syntactic_sugar", 1}, {"vanilla_extract", 1}, {"kernel_oil", 1}},
     "vanilla_java_roast", 1, /*wire=*/9, "MERGED JAVA ROAST"},
    {"GNUlash",
     {{"root_veg", 1}, {"kernel_oil", 1}, {"polltatoes", 2},
      {"desalinated_c_salt", 1}},
     "gnulash", 1, /*wire=*/10, "MERGED GNULASH"},
};
inline constexpr int kMergeRecipeCount =
    static_cast<int>(sizeof(kMergeRecipes) / sizeof(kMergeRecipes[0]));

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

}  // namespace mal
