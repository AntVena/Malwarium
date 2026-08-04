#pragma once

// Private, engine-internal helpers shared by more than one Game unit
// (src/core/app/game/*.cpp). NOT part of the public contract — keep this out of
// game.h. Anything used by only a single unit belongs in that unit's own
// anonymous namespace, not here.

#include <algorithm>
#include <cstring>

#include "core/app/game_rig_shop.h"   // RigRow — kMergeRecipes names its unlock row
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
// game_merge.cpp (craft/render) and the native item earn-path coverage test: a
// recipe's OUTPUT is earnable by being crafted, which the generic starting-shelf/
// cache-pool/shop/container/warp checks can't see on their own. Whether a recipe is
// UNLOCKED lives on its own Rig Shop row (game_rig_shop.h), named here by `rigRow` —
// this table only owns the craft mechanic: the input/output ids + quantities
// craftRecipe consumes/grants, and the Hacker-Log line the craft writes.
//
// Adding a recipe is one row here plus one row in kRigUpgrades, wired to each other
// by `rigRow`. Nothing derives one from the other's POSITION, so the two tables can
// grow independently (rig rows are append-only for save compatibility; this one is
// free to be reordered).
constexpr int kMaxRecipeInputs = 4;

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
    int rigRow;                // the Rig Shop row that unlocks this recipe
    const char* logText;       // Hacker-Log line written on a successful craft
};
inline const MergeRecipe kMergeRecipes[] = {
    {"Pwnzu-Patched Noodles", {{"null_noodles", 1}, {"pwnzu_sauce", 1}},
     "pwnzu_patched_noodles", 1, kRigRowRecipeNoodles, "MERGED PATCHED NOODLES"},
    {"Fully-Stacked Nachos", {{"tortilla_chip", 1}, {"osi_dip", 1}},
     "fully_stacked_nachos", 1, kRigRowRecipeNachos, "MERGED STACKED NACHOS"},
    // The pantry's own two: a four-ingredient base dish, then a one-ingredient pass
    // back through the pan that turns it into the better version of itself.
    {"Hashed Browns",
     {{"c_salt", 1}, {"grepsed_oil", 1}, {"cronstarch", 1}, {"polltatoes", 1}},
     "hashed_browns", 1, kRigRowRecipeHashedBrowns, "MERGED HASHED BROWNS"},
    {"Salted&Hashed Browns", {{"c_salt", 1}, {"hashed_browns", 1}},
     "salted_hashed_browns", 1, kRigRowRecipeSaltedBrowns, "MERGED SALTED BROWNS"},
};
inline constexpr int kMergeRecipeCount =
    static_cast<int>(sizeof(kMergeRecipes) / sizeof(kMergeRecipes[0]));

// How many ingredient slots `r` actually uses.
inline int recipeInputCount(const MergeRecipe& r) {
    int n = 0;
    for (const RecipeInput& in : r.inputs)
        if (in.id) ++n;
    return n;
}

}  // namespace mal
