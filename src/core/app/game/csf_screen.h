// csf_screen.h: CRITICAL SYSTEM FAILURE (Nav::ModalCSF), the terminal death modal.
//
// The dying pet corrupts out under a crash overlay, which holds a few beats before B can
// acknowledge it. The screen holds that hold's clock and owns the draw; acknowledging
// archives the pet and starts a new hatch, which is the game's (CsfHost). C is disabled:
// death is not cancellable.
#pragma once

#include "core/device/platform.h"

namespace mal {

class Framebuffer;
struct SpriteData;

// What the modal needs from the game. Game implements it; a test can too.
class CsfHost {
public:
    virtual const SpriteData* petSprite() const = 0;
    virtual int screenBeat() const = 0;
    virtual void acknowledgeCSF() = 0;

protected:
    ~CsfHost() = default;
};

class CsfScreen {
public:
    void open() { beat_ = 0; }
    void tick();   // a heartbeat of the crash hold
    bool revealed() const;
    void onButton(CsfHost& host, const ButtonEvent& ev) {
        if (ev.button == Button::B && revealed()) host.acknowledgeCSF();
    }
    void draw(Framebuffer& fb, const CsfHost& host) const;

private:
    int beat_ = 0;
};

}  // namespace mal
