#pragma once

// Private, engine-internal helpers shared by more than one Game unit
// (src/core/app/game/*.cpp). NOT part of the public contract — keep this out of
// game.h. Anything used by only a single unit belongs in that unit's own
// anonymous namespace, not here.

#include <algorithm>
#include <cstring>

#include "core/app/game_rig_shop.h"   // kRigVisibleRows/kRigRowPitch — the SHOP list's geometry
#include "core/content/content_recipes.h"  // kMergeRecipes — the MRG list's rows
#include "core/model/combat.h"        // Combatant::BackupUse / Combat::Outcome
#include "core/render/canvas.h"       // kActiveW — the bar hangs off the right edge
#include "core/render/framebuffer.h"
#include "core/render/palette.h"
#include "tunables.h"

namespace mal {

// The two Hacker-face roster caps. Each sizes the stack buffer its screen fills, and
// each is a real ceiling rather than a window: the list is longer than what the screen
// shows, so a truncation here would silently lose peers or networks the radio heard.
// Shared because game_listnav.cpp walks the same rows the screens draw, and a cursor
// stepping past a buffer the drawing code sized differently is the one way these two
// numbers could ever disagree.
constexpr int kPeerMaxRows = 64;
constexpr int kNetMaxRows = kNetVisibleCap;

// Which of the three Backup Drive achievements a finished fight earned, or nullptr for
// none — the drive unused, or a Fled fight that settled nothing. Pure over the only two
// facts the answer depends on (what the drive did, how the fight ended), so the whole
// mapping is assertable without staging three fights to reach it. Defined in
// game_combat.cpp, which is also its only caller (Game::settleBackupDrive).
const char* backupDriveAchievement(Combatant::BackupUse used, Combat::Outcome outcome);


// Combat-stat labels ---------------------------------------------
// Indexed to statPoints_/lastLevelUpStat_: 0 power · 1 defense · 2 speed · 3
// max-Health. Shared by the Rollback picker + its log line (game_items), the
// post-encounter level-up readout (game_render), and any other unit that needs to
// name a levelled stat. The words themselves come from content/defs.h's levelStatWord,
// which the item readouts also draw on — an Epic dish grants a point on this same axis,
// so the two must agree on what to call it.
inline const char* levelStatName(int i) {
    return (i >= 0 && i < kLevelStatCount) ? levelStatWord(i) : "?";
}

// Rig Shop list layout ---------------------------------------
// Row indices/data live in game_rig_shop.h (RigRow/kRigUpgrades) — these are pure
// render-layout constants for game_rigshop.cpp's SHOP list (kept here, not there,
// since they're presentation, not rig data; test_stat.cpp sizes a row's grid by them).
constexpr int kRigRowTop = 34;       // first row, under the header band
constexpr int kRigVisibleRows = 3;   // rows on screen at once
constexpr int kRigRowPitch = 54;

// The Hacker face's one-line-per-row list, shared by the VAULT (game_hacker.cpp) and
// SHOP > SERVICES (game_rigshop.cpp) so the two read as the same kind of list.
constexpr int kHackerListTop = 34;
constexpr int kHackerListRowH = 22;
constexpr int kHackerListRows = 7;

// Merge Hub list layout (Hacker MRG) -----------------------------
// The recipe TABLE is content and lives with the other content tables
// (core/content/content_recipes.h, included above); what stays here is the geometry it
// is drawn through. Four rows is what the panel holds in the worst case: three folded
// rows (title + a dim "stocked" summary line each) plus the focused row unfolding
// kMaxRecipeInputs ingredient lines, all above the A NEXT / B MERGE footer rule.
constexpr int kMergeVisibleRows = 4;

}  // namespace mal
