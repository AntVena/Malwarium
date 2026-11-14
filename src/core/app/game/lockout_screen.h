// lockout_screen.h: the Lockout modal (Nav::ModalLockout), the crisis a starving pet
// raises.
//
// The screen holds which of the two paths has the focus, and nothing else: the deadline,
// the price and what resolves the crisis are the game's (game_lifecycle.cpp), reached
// through LockoutHost. A goes between the paths, B takes the focused one, and C does
// nothing because a crisis can't be cancelled.
#pragma once

#include <cstdint>

#include "core/device/platform.h"

namespace mal {

class Framebuffer;
struct SpriteData;

// What the modal needs from the game. Game implements it; a test can too.
class LockoutHost {
public:
    virtual const SpriteData* petSprite() const = 0;
    virtual uint32_t lockoutMsLeft() const = 0;   // 0 once the deadline has passed
    virtual bool canPayLockout() const = 0;       // the wallet covers the Bits
    virtual bool lockoutFoodHeld() const = 0;     // the bag holds something that resolves it
    virtual int32_t failingLeftMs() const = 0;    // -1 unless the pet is already failing
    virtual bool yubiReady() const = 0;
    virtual int screenBeat() const = 0;
    virtual void payLockout() = 0;    // spend the Bits and resolve it, when they cover it
    virtual void feedLockout() = 0;   // the Yubi-Cookie when it saves the pet, else ITEMS

protected:
    ~LockoutHost() = default;
};

class LockoutScreen {
public:
    // Open on whichever path can actually resolve it: feeding, unless the bag has
    // nothing to feed and the wallet can cover the Bits instead. Coming back from the
    // ITEMS list does not reopen it, so the focus is where the player left it.
    void open(const LockoutHost& host) { pay_ = !host.lockoutFoodHeld() && host.canPayLockout(); }
    void onButton(LockoutHost& host, const ButtonEvent& ev);
    void draw(Framebuffer& fb, const LockoutHost& host) const;

private:
    bool pay_ = false;   // the Pay Bits path has the focus, rather than Feed
};

}  // namespace mal
