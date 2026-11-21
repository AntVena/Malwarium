// dying_window.h: the 5/5 recovery window, a part of the Pet record (Pet::dying).
//
// It is the one timed window a reboot keeps: refunding it would make the only death path
// optional. The anchor is in game-ms and means nothing across a boot, so only the
// accumulated `elapsedMs` is saved (save v42), and a thawed window is unarmed until the
// next tick re-anchors it. What it counts is time AWAKE at 5/5: with no RTC the device
// cannot see how long it was off.
#pragma once

#include <cstdint>

#include "tunables.h"   // kCsfDyingGraceMs, kFailingAlertEveryMs

namespace mal {

class DyingWindow {
public:
    // What one tick asks of the game.
    struct Step {
        bool saveNow = false;   // the window opened or closed: worth writing at once
        bool alert = false;     // the FAILING beep is due
        bool expired = false;   // the window ran out: Critical System Failure
    };

    // Advance while the pet is (or is not) at 5/5. A pet pulled back below 5/5 forgets
    // what it burned and gets the full window again next time.
    Step tick(bool dying, uint32_t nowMs) {
        Step s;
        if (!dying) {
            if (armed_ || elapsedMs) { reset(); s.saveNow = true; }
            return s;
        }
        if (!armed_) {
            armed_ = true;
            enteredMs_ = nowMs;
            alertAtMs_ = elapsedMs;
            s.saveNow = true;
        }
        elapsedMs += nowMs - enteredMs_;
        enteredMs_ = nowMs;
        if (elapsedMs >= alertAtMs_ && elapsedMs < kCsfDyingGraceMs) {
            s.alert = true;
            alertAtMs_ = elapsedMs + kFailingAlertEveryMs;
        }
        s.expired = elapsedMs >= kCsfDyingGraceMs;
        return s;
    }

    void reset() { armed_ = false; elapsedMs = 0; }
    bool armed() const { return armed_; }
    uint32_t leftMs() const { return elapsedMs < kCsfDyingGraceMs ? kCsfDyingGraceMs - elapsedMs : 0; }

    uint32_t elapsedMs = 0;   // the saved half

private:
    bool armed_ = false;
    uint32_t enteredMs_ = 0;
    uint32_t alertAtMs_ = 0;   // in elapsedMs time
};

} // namespace mal
