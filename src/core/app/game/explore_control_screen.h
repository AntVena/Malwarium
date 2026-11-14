// explore_control_screen.h: the walk's A+C control overlay (Nav::ExploreControl), as a
// feature that owns its own state.
//
// A cursor list over the habitat, the shape of combat's Exploit picker: ping the next
// event now, spend a warp key, toggle auto-progress and the walk's sound, or stop the
// walk. The screen holds its cursor. The walk and its settings are the game's, reached
// through ExploreControlHost.
#pragma once

#include "core/audio/sound.h"   // SoundMode
#include "core/device/platform.h"

namespace mal {

class Framebuffer;

// What the overlay needs from the game. Game implements it; a test can too.
class ExploreControlHost {
public:
    virtual bool hasWarpKey() const = 0;
    virtual bool autoProgress() const = 0;
    virtual SoundMode soundMode() const = 0;
    virtual bool exploreSound() const = 0;
    virtual int exploreXpEfficiencyPct() const = 0;
    virtual void drawHabitat(Framebuffer& fb, int cursor) const = 0;   // the overlay's ground

    virtual void pingWalk() = 0;            // the next event now, skipping the step timer
    virtual void openWarpPicker() = 0;
    virtual void toggleAutoProgress() = 0;
    virtual void setExploreSound(bool on) = 0;
    virtual void stopWalk() = 0;
    virtual void leaveToIdle() = 0;         // back to the habitat, the walk still running

protected:
    ~ExploreControlHost() = default;
};

class ExploreControlScreen {
public:
    // Every opening starts on the first action, never on whatever was last pressed.
    void open() { row_ = 0; }
    void onButton(ExploreControlHost& host, const ButtonEvent& ev);
    void draw(Framebuffer& fb, const ExploreControlHost& host) const;

private:
    int row_ = 0;
};

}  // namespace mal
