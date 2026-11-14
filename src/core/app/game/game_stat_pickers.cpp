#include "core/app/game.h"
#include "core/app/game_internal.h"

#include <cstdio>

namespace mal {

void Game::openRollbackPicker(const ItemDef& item) {
    statPicker_.open(*this, item);
    nav_ = Nav::RollbackPicker;
    dirty_ = true;
}

void Game::openRepartitionPicker(const ItemDef& item) {
    statPicker_.open(*this, item);
    nav_ = Nav::RepartitionPicker;
    dirty_ = true;
}

void Game::rollBackStat(const ItemDef& item, int stat) {
    // Shed one point of the chosen stat: −1 that stat, −1 level (level == total
    // points). The bill is ONE level and nothing more, so the XP banked toward the
    // next one is carried down rather than binned: a pet 90% of the way up its rung
    // lands 90% of the way up the rung below, and re-grinds exactly the level it
    // shed. Carried as a fraction because the rung below is shorter. The raw count
    // would leave the pet over the lower rung's need and return the level on the
    // next award. The floor division also keeps it strictly under that need (it was
    // strictly under the taller one), so the shed can never undo itself. Consume
    // the Rollback item + log.
    if (!statCanGiveUp(stat)) return;
    --pet_.statPoints[stat];
    const int wasNeed = xpToNextLevel();
    --pet_.level;
    if (wasNeed > 0)
        pet_.xp = static_cast<int>(static_cast<long long>(pet_.xp) * xpToNextLevel() / wasNeed);
    inventory_.remove(item.id, 1);
    char buf[28];
    // Surface the shed explicitly, which stat and by how much (
    // "show the -1 <stat> on a Rollback"). The stat WORD + signed number are
    // the non-colour channel, so it reads in a grayscale log.
    std::snprintf(buf, sizeof(buf), "ROLLBACK %s -1", levelStatName(stat));
    log_.push(LogEventType::ItemUsed, buf);
    markSaveDirty();
}

void Game::moveStatPoint(const ItemDef& item, int from, int to) {
    // The move: one point off the source, one onto the target. The level and the
    // XP bucket are untouched, because nothing was un-earned. The item changes
    // only the table's shape, and level == the sum of earned points still holds.
    // Consume the item + log.
    if (!statCanGiveUp(from) || to == from || to < 0 || to >= kLevelStatCount) return;
    --pet_.statPoints[from];
    ++pet_.statPoints[to];
    inventory_.remove(item.id, 1);
    char buf[28];
    // Both ends, in words: which stat paid and which one grew. The stat WORDS
    // are the non-colour channel, so the line reads in a grayscale log.
    std::snprintf(buf, sizeof(buf), "MOVED %s>%s", levelStatName(from), levelStatName(to));
    log_.push(LogEventType::ItemUsed, buf);
    markSaveDirty();
}

}  // namespace mal
