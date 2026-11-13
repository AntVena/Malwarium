// feeding.h: one feeding, a run (CONTRIBUTING, "Where state lives"). From the bite (the
// food is applied the moment it starts) until the modal dismisses itself on its own clock
// or under a press. It holds what the modal shows, the clock that ends it, and where it
// hands back to. Runtime-only and never saved: the food has already landed.
//
// Game holds it only while the feeding modal is up.
#pragma once

#include "core/content/defs.h"

namespace mal {

// The pet's vitals as they stood BEFORE the food was applied. The feeding modal
// reports the change that actually happened, which is the only way a pull TOWARD
// 50% Happiness, or a top-up onto an already-full stat, can state its own
// direction: the magnitude on the row doesn't know either. Game::startFeeding
// snapshots this the instant before Game::applyItemEffects.
struct FeedVitals {
    int hunger = 0;
    int frag = 0;
    int happy = 0;
};

struct Feeding {
    const ItemDef* item = nullptr;   // the food, in the static registry: a stable pointer
    FeedVitals before;
    int beat = 0;                    // heartbeats the modal has held (kFeedBeats ends it)
    // Fed from the Lockout crisis: the end resolves the Lockout instead of returning to
    // the ITEMS screen it would otherwise go back to.
    bool fromLockout = false;

    static Feeding started(const ItemDef& item, const FeedVitals& before, bool fromLockout) {
        Feeding f;
        f.item = &item;
        f.before = before;
        f.fromLockout = fromLockout;
        return f;
    }
};

}  // namespace mal
