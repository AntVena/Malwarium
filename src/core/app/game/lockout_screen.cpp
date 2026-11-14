#include "core/app/game/lockout_screen.h"

#include "tunables.h"
#include "core/ui/modals.h"

namespace mal {

void LockoutScreen::onButton(LockoutHost& host, const ButtonEvent& ev) {
    if (ev.button == Button::A) pay_ = !pay_;
    else if (ev.button == Button::B) {
        if (pay_) host.payLockout();
        else host.feedLockout();
    }
}

void LockoutScreen::draw(Framebuffer& fb, const LockoutHost& host) const {
    LockoutView v;
    v.pet = host.petSprite();
    if (const uint32_t left = host.lockoutMsLeft()) {
        v.secondsLeft = static_cast<int>((left + 999) / 1000);
        v.remainFrac = static_cast<float>(left) / kLockoutDurationMs;
    }
    v.focus = pay_ ? LockoutPath::Pay : LockoutPath::Feed;
    v.bitsCost = kLockoutBitsCost;
    v.canPay = host.canPayLockout();
    v.hasFood = host.lockoutFoodHeld();
    v.failingLeftMs = host.failingLeftMs();
    v.yubiReady = host.yubiReady();
    v.beat = host.screenBeat();
    drawLockoutModal(fb, v);
}

}  // namespace mal
