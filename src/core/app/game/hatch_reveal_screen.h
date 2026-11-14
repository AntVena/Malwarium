// hatch_reveal_screen.h: HATCHING (Nav::ModalHatchReveal), the on-demand crack that lets
// a line with no decrypt show its shell opening.
//
// Non-interactive: the shell's one-shot walks a frame per heartbeat, holds on the last
// one, then the egg hatches itself. The screen holds that clock and draws the frame; the
// hatch is the game's (HatchRevealHost).
#pragma once

#include "core/device/platform.h"

namespace mal {

class Framebuffer;
struct SpriteData;

// What the cinematic needs from the game. Game implements it; a test can too.
class HatchRevealHost {
public:
    virtual const SpriteData* hatchEggSprite() const = 0;   // the shell's one-shot
    virtual void completeHatch() = 0;

protected:
    ~HatchRevealHost() = default;
};

class HatchRevealScreen {
public:
    void open() { beat_ = 0; }
    // A heartbeat: the next frame, and the hatch once the last one has held.
    void tick(HatchRevealHost& host);
    // Every button is inert for the cinematic's few seconds, so a stray press cannot
    // skip it.
    void onButton(HatchRevealHost&, const ButtonEvent&) {}
    void draw(Framebuffer& fb, const HatchRevealHost& host) const;
    int frame(const HatchRevealHost& host) const;   // the one-shot's frame on screen

private:
    int beat_ = 0;   // heartbeats since the chord cracked the egg
};

}  // namespace mal
