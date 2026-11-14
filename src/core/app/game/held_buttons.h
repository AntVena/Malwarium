// held_buttons.h: which of A, B and C is held and since when, and the repeat clocks the
// held gestures fire on. Game::onButton arms and disarms; the tick reads it to fire A's
// list repeat and C's backward walk (Game::tickListNav), B's second action
// (Game::tickHoldB) and the DECRYPTOGRAM's cursor repeat (Game::tickCryptogramRepeat).
//
// One flag per button because the three are independent physical buttons: a step can be
// repeating under one thumb while the other starts a screen gesture. Every screen-level
// B hold shares B's flag: the screens that hold B are never up together, and the release
// edge offers the press to the one that is.
#pragma once

#include <cstdint>

#include "tunables.h"
#include "core/device/platform.h"

namespace mal {

class HeldButtons {
public:
    void arm(Button b, uint32_t nowMs) {
        Hold& h = hold(b);
        h.held = true;
        h.downMs = nowMs;
    }
    void disarm(Button b) { hold(b).held = false; }
    bool held(Button b) const { return hold(b).held; }
    uint32_t heldMs(Button b, uint32_t nowMs) const { return nowMs - hold(b).downMs; }

    // C on a list: armed with nothing decided. Released before kListRepeatDelayMs it was
    // a tap; held past it, it walks the cursor backward and cancels nothing.
    void armListBack(uint32_t nowMs) {
        arm(Button::C, nowMs);
        backStepped_ = false;
    }
    // Whether the C just released walked the cursor, which makes it a hold and not a
    // cancel. Read once, on the release edge.
    bool takeBackStepped() {
        const bool stepped = backStepped_;
        backStepped_ = false;
        return stepped;
    }

    // Keeps each list-repeat clock on the current time while its button is up, so the
    // next hold can't burst-catch-up. A and C each have one: sharing a clock would let a
    // release-and-press of the opposite direction inherit the other's cadence.
    void primeListRepeat(uint32_t nowMs) {
        if (!a_.held) listRepeatMs_[0] = nowMs;
        if (!c_.held) listRepeatMs_[1] = nowMs;
    }
    // Whether a held A (forward) or C (backward) list step is due, stamping it when it
    // is. Crossing kListRepeatDelayMs is what turns C from a tap into a hold, so its
    // first step also latches takeBackStepped().
    bool listRepeatDue(Button b, uint32_t nowMs) {
        const Hold& h = hold(b);
        uint32_t& last = listRepeatMs_[b == Button::C ? 1 : 0];
        if (!h.held || nowMs - h.downMs < kListRepeatDelayMs || nowMs - last < kListRepeatMs)
            return false;
        last = nowMs;
        if (b == Button::C) backStepped_ = true;
        return true;
    }

    // The DECRYPTOGRAM's cursor repeat, on its own faster cadence: a board opens with
    // thirty-odd closed cells, and stepping those on the 4fps heartbeat would take ten
    // seconds to lap. Primed while the board can't repeat, so entering it can't
    // burst-catch-up.
    void primeBoardRepeat(uint32_t nowMs) { boardRepeatMs_ = nowMs; }
    // Whether the step under whichever of A or C is held is due, stamping it when it is.
    // The caller has already checked that exactly one of the two is held.
    bool boardRepeatDue(uint32_t nowMs) {
        const Hold& h = a_.held ? a_ : c_;
        if (nowMs - h.downMs < kCryptogramRepeatDelayMs ||
            nowMs - boardRepeatMs_ < kCryptogramRepeatMs)
            return false;
        boardRepeatMs_ = nowMs;
        return true;
    }

private:
    struct Hold {
        bool held = false;
        uint32_t downMs = 0;
    };
    Hold& hold(Button b) { return b == Button::A ? a_ : b == Button::B ? b_ : c_; }
    const Hold& hold(Button b) const { return b == Button::A ? a_ : b == Button::B ? b_ : c_; }

    Hold a_, b_, c_;
    uint32_t listRepeatMs_[2] = {0, 0};   // A forward, C backward
    bool backStepped_ = false;
    uint32_t boardRepeatMs_ = 0;
};

}  // namespace mal
