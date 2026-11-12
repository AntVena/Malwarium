// game_titles.cpp: the zone-completion Titles the operator earns and equips in CFG.

#include "core/app/game.h"

#include "core/ui/expl_screen.h"

namespace mal {

void Game::unlockTitle(int sector) {
    // Grant the sector's Title (player-level). Idempotent: re-clearing a sector
    // (or a migrated save that already had it) never double-logs. Auto-equip the
    // first Title earned so the reward is visible without a trip to CFG.
    if (sector < 0 || sector >= kAreaCount) return;
    if (op_.world.titlesUnlocked & (1u << sector)) return;      // already earned
    op_.world.titlesUnlocked |= (1u << sector);
    if (op_.world.equippedTitle < 0) op_.world.equippedTitle = sector;   // first Title auto-equips
    log_.push(LogEventType::CombatWon, "TITLE UNLOCKED");
    markSaveDirty();
}

int Game::titlesUnlockedCount() const {
    int n = 0;
    for (int i = 0; i < kAreaCount; ++i)
        if (op_.world.titlesUnlocked & (1u << i)) ++n;
    return n;
}

const char* Game::equippedTitleName() const {
    return titleUnlocked(op_.world.equippedTitle) ? sectorTitle(op_.world.equippedTitle) : "NONE";
}

int Game::nextSelectableTitle(int cur) const {
    // Selectable ring: NONE (-1), then each unlocked sector in ascending order.
    // Walk forward from `cur`, wrapping through -1, to the next selectable value.
    for (int step = 0; step < kAreaCount + 1; ++step) {
        cur = (cur + 1 > kAreaCount - 1) ? -1 : cur + 1;   // ... , n-1, -1, 0, ...
        if (cur < 0 || titleUnlocked(cur)) return cur;
    }
    return -1;   // nothing unlocked → only NONE is selectable
}

}  // namespace mal
