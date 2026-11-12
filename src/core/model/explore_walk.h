// explore_walk.h: the explore walk, a run (CONTRIBUTING, "Where state lives"). One walk on
// the idle habitat, from arming an area (or an endless zone) until a stop, a loss or a
// crisis ends it. Runtime-only: a reboot ends it, and the save keeps nothing of it.
//
// Game holds the last walk after it ends, inactive: EXPL opens with its cursor on the area
// last walked, and the screens that report on a walk that just ended (the post-encounter
// readout, a Wi-Fi event's banner) still name its area. The next start replaces it whole.
#pragma once

namespace mal {

struct ExploreWalk {
    bool active = false;
    int sector = 0;      // the armed area, or kDeepWebSector / kDarkWebSector
    int sub = 0;         // the armed sub-area; 0 on an endless zone
    // Wins on the armed sub-area toward its boss; on an endless zone, the depth.
    int streak = 0;
    int steps = 0;       // steps this walk; the lifetime count is the Operator's
    int stepBeat = 0;    // heartbeats since the last auto-step (the step pacer)
    // Heartbeats a hands-off event screen (Wi-Fi, a shop, a guardian) has been held.
    // Each event zeroes it as it opens.
    int eventBeat = 0;
    // The line under the walk's badge: what the last resolved event left behind. Wide
    // enough for the longest line a resolver composes ("<item> - DECRYPT IN VAULT" with
    // the widest cache name); wrapped to two lines on screen.
    char flavor[48] = "";

    static ExploreWalk started(int sector, int sub, int streak) {
        ExploreWalk w;
        w.active = true;
        w.sector = sector;
        w.sub = sub;
        w.streak = streak;
        return w;
    }
    // A stop, a loss or a crisis. The streak is per walk, so it goes with it.
    void end() {
        active = false;
        streak = 0;
    }
};

}  // namespace mal
