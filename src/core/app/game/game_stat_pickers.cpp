#include "core/app/game.h"
#include "core/app/game_internal.h"

#include <cstdio>

namespace mal {

int Game::nextEligibleStat(int cur) const {
    // The next stat (wrapping) with ≥1 earned point: A cycles only eligible rows
    // Returns `cur` if it's the only eligible one, or -1 if none are.
    for (int step = 1; step <= kLevelStatCount; ++step) {
        const int i = (cur + step) % kLevelStatCount;
        if (rollbackEligible(i)) return i;
    }
    return rollbackEligible(cur) ? cur : -1;
}

void Game::openRollbackPicker() {
    // Park on the first eligible stat (itemUsable already guaranteed ≥1 point exists,
    // so nextEligibleStat can't return -1 here). detailItem_ (the Rollback item) stays
    // set so a confirmed shed consumes it.
    int first = -1;
    for (int i = 0; i < kLevelStatCount; ++i)
        if (rollbackEligible(i)) { first = i; break; }
    rollbackRow_ = first < 0 ? 0 : first;
    nav_ = Nav::RollbackPicker;
    dirty_ = true;
}

void Game::onRollbackPicker(const ButtonEvent& ev) {
    if (ev.button == Button::A) {
        const int nxt = nextEligibleStat(rollbackRow_);
        if (nxt >= 0) rollbackRow_ = nxt;
    } else if (ev.button == Button::B) {
        // Shed one point of the chosen stat: −1 that stat, −1 level (level == total
        // points). The bill is ONE level and nothing more, so the XP banked toward the
        // next one is carried down rather than binned: a pet 90% of the way up its rung
        // lands 90% of the way up the rung below, and re-grinds exactly the level it
        // shed. Carried as a fraction because the rung below is shorter. The raw count
        // would leave the pet over the lower rung's need and return the level on the
        // next award. The floor division also keeps it strictly under that need (it was
        // strictly under the taller one), so the shed can never undo itself. Consume
        // the Rollback item + log.
        if (rollbackEligible(rollbackRow_) && detailItem_) {
            --pet_.statPoints[rollbackRow_];
            const int wasNeed = xpToNextLevel();
            --pet_.level;
            if (wasNeed > 0)
                pet_.xp = static_cast<int>(static_cast<long long>(pet_.xp) *
                                             xpToNextLevel() / wasNeed);
            inventory_.remove(detailItem_->id, 1);
            char buf[28];
            // Surface the shed explicitly, which stat and by how much (
            // "show the -1 <stat> on a Rollback"). The stat WORD + signed number are
            // the non-colour channel, so it reads in a grayscale log.
            std::snprintf(buf, sizeof(buf), "ROLLBACK %s -1",
                          levelStatName(rollbackRow_));
            log_.push(LogEventType::ItemUsed, buf);
            markSaveDirty();
        }
        nav_ = Nav::Submenu;   // back to the ITEMS list (the item may have run out)
    } else if (ev.button == Button::C) {
        nav_ = Nav::Detail;    // cancel: item untouched, back to its detail
    }
    dirty_ = true;
}

int Game::nextRepartitionTarget(int cur) const {
    // The next stat (wrapping) that is not the source. Every other stat is a legal
    // destination, including one at zero earned points, so the only test is that the
    // point moves somewhere new.
    for (int step = 1; step <= kLevelStatCount; ++step) {
        const int i = (cur + step) % kLevelStatCount;
        if (i != repartitionFrom_) return i;
    }
    return cur;
}

void Game::openRepartitionPicker() {
    // Step one: park on the first stat with a point to give up. itemUsable already
    // guaranteed one exists (level > 0 == at least one earned point), so the scan
    // always lands. detailItem_ (the Repartition item) stays set for the commit.
    int first = -1;
    for (int i = 0; i < kLevelStatCount; ++i)
        if (rollbackEligible(i)) { first = i; break; }
    repartitionFrom_ = -1;
    repartitionRow_ = first < 0 ? 0 : first;
    nav_ = Nav::RepartitionPicker;
    dirty_ = true;
}

void Game::onRepartitionPicker(const ButtonEvent& ev) {
    const bool picking = repartitionFrom_ < 0;   // step one: which stat gives the point
    if (ev.button == Button::A) {
        const int nxt = picking ? nextEligibleStat(repartitionRow_)
                                : nextRepartitionTarget(repartitionRow_);
        if (nxt >= 0) repartitionRow_ = nxt;
    } else if (ev.button == Button::B) {
        if (picking) {
            // Lock the source in and hand the cursor to the destination step, parked on
            // a stat that is not the source (so B can never be a no-op move onto itself).
            if (rollbackEligible(repartitionRow_)) {
                repartitionFrom_ = repartitionRow_;
                repartitionRow_ = nextRepartitionTarget(repartitionRow_);
            }
        } else if (rollbackEligible(repartitionFrom_) &&
                   repartitionRow_ != repartitionFrom_ && detailItem_) {
            // The move: one point off the source, one onto the target. The level and the
            // XP bucket are untouched, because nothing was un-earned. The item changes
            // only the table's shape, and level == the sum of earned points still holds.
            // Consume the item + log.
            --pet_.statPoints[repartitionFrom_];
            ++pet_.statPoints[repartitionRow_];
            inventory_.remove(detailItem_->id, 1);
            char buf[28];
            // Both ends, in words: which stat paid and which one grew. The stat WORDS
            // are the non-colour channel, so the line reads in a grayscale log.
            std::snprintf(buf, sizeof(buf), "MOVED %s>%s",
                          levelStatName(repartitionFrom_), levelStatName(repartitionRow_));
            log_.push(LogEventType::ItemUsed, buf);
            markSaveDirty();
            repartitionFrom_ = -1;
            nav_ = Nav::Submenu;   // back to the ITEMS list (the item may have run out)
        }
    } else if (ev.button == Button::C) {
        // C walks back one step rather than straight out: from the destination step it
        // re-opens the source question (cursor back on the source, so the mis-pick is
        // visible), and from the source step it leaves. Nothing has been spent either way.
        if (picking) {
            nav_ = Nav::Detail;    // cancel: item untouched, back to its detail
        } else {
            repartitionRow_ = repartitionFrom_;
            repartitionFrom_ = -1;
        }
    }
    dirty_ = true;
}

}  // namespace mal
