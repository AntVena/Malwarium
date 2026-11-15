// fx_clock.h: THE dissolve clock (FX_ABSORB / FX_SHRED), on the fast kFxAnimMs tick rather
// than the heartbeat: a sweep is redrawn every frame it is up, and stepped at 4fps it reads
// as jumping rather than moving. One clock serves every host because only one dissolve is
// ever on screen (the feeding modal, the Wi-Fi event, a beaten rival's combat outro, the
// guardian's swarm). Each host restarts it as its own sweep opens, so beat() always counts
// beats since THIS sweep.
//
// Kept apart from ExploreWalk::eventBeat and Fight::resultBeat: a press restarts those, and
// a dissolve animates something already resolved, so replaying it under a player leaning on
// A would show a second helping that never happened.
#pragma once

#include <cstdint>

#include "core/render/canvas.h"   // kFxAnimMs

namespace mal {

class FxClock {
public:
    int beat() const { return beat_; }
    void restart() { beat_ = 0; }
    // No sweep is up: hold the clock at `nowMs`, so the next sweep starts on a fresh
    // interval instead of catching up.
    void prime(uint32_t nowMs) { lastMs_ = nowMs; }
    // A sweep is up: one beat per kFxAnimMs. True when it stepped.
    bool advance(uint32_t nowMs) {
        if (nowMs - lastMs_ < static_cast<uint32_t>(kFxAnimMs)) return false;
        lastMs_ = nowMs;
        ++beat_;
        return true;
    }

private:
    int beat_ = 0;
    uint32_t lastMs_ = 0;
};

}  // namespace mal
