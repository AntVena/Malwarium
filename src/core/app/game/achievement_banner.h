// achievement_banner.h: the home-screen announcement of an unlocked achievement, from the
// beat it opens to the moment it retires. Which rows are due, marking them announced and
// composing what the plate says are the game's (Game::tickAchievementBanner,
// Game::achBannerCopy); this holds what is on screen and for how long.
#pragma once

#include <cstdint>

namespace mal {

class AchievementBanner {
public:
    // The row on screen, by wire number, or -1 for none.
    int wire() const { return wire_; }
    bool showing() const { return wire_ >= 0; }
    // How many unlocks it speaks for: 1 is the named row; more is a burst too long to
    // parade one at a time (a firmware update that retro-awards a back catalogue),
    // collapsed into one "N UNLOCKED" banner that still names `wire`.
    int count() const { return count_; }
    // The beat it arrived on: drives the wipe-open, and starts its marquee at the head
    // of the line rather than wherever the free-running beat happens to be.
    int openBeat() const { return openBeat_; }
    bool expired(uint32_t nowMs) const { return nowMs >= untilMs_; }

    // What it will say. The dwell is set after, by open(), because the composed copy
    // decides how long it needs.
    void compose(int wire, int count) {
        wire_ = wire;
        count_ = count;
    }
    void open(int beat, uint32_t untilMs) {
        openBeat_ = beat;
        untilMs_ = untilMs;
    }
    void clear() {
        wire_ = -1;
        count_ = 0;
    }

private:
    int wire_ = -1;
    int count_ = 0;
    uint32_t untilMs_ = 0;
    int openBeat_ = 0;
};

}  // namespace mal
