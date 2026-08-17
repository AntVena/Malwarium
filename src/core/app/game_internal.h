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

// Merge Hub list layout (Hacker MRG) -----------------------------
// The recipe TABLE is content and lives with the other content tables
// (core/content/content_recipes.h, included above); what stays here is the geometry it
// is drawn through. Four rows is what the panel holds in the worst case: three folded
// rows (title + a dim "stocked" summary line each) plus the focused row unfolding
// kMaxRecipeInputs ingredient lines, all above the A NEXT / B MERGE footer rule.
constexpr int kMergeVisibleRows = 4;

}  // namespace mal
