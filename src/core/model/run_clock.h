// run_clock.h: when a played board last stepped, in game-ms. Each board's run holds one,
// so its cadence starts with the run and dies with it. Game's per-board cadence ticks
// (Game::tickStackerSlide and its siblings) restart it while the board is closed or
// paused and step the board each time a period has passed.
#pragma once

#include <cstdint>

namespace mal {

class RunClock {
public:
    void restart(uint32_t nowMs) { lastMs_ = nowMs; }
    // Unsigned subtraction, so a clock wrap behaves like every other elapsed-time check.
    uint32_t since(uint32_t nowMs) const { return nowMs - lastMs_; }

private:
    uint32_t lastMs_ = 0;
};

}  // namespace mal
