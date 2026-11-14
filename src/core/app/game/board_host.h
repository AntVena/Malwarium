// board_host.h: what every played board's screen needs from the game, and what every
// played game reports when it ends.
//
// The boards (Stacker, Fragger, the hatch games) each own their input and draw in a
// screen type of their own, over a run that holds the rules. They share the RULES
// overlay the chord opens over any of them, the device's sounds, the footer that says
// what the run is worth in whichever context is paying for it, and the heartbeat.
#pragma once

#include "core/audio/sound.h"
#include "core/device/platform.h"

namespace mal {

class Framebuffer;
struct PlayHud;

// What every played game reports when it ends, whoever was paying for it. `units`
// and `bitsUnits` are what a MAINT process prices (ArcadeGameDef::unitName /
// bitsUnitName: cleaning, and Bits); `score` / `scoreMax` are what the arcade till
// prices. One shape for every engine is what lets any of them stand behind either
// MAINT process.
struct PlayOutcome {
    int units = 0;
    int bitsUnits = 0;
    bool won = false;
    int score = 0;
    int scoreMax = 0;
};

class BoardHost {
public:
    // The RULES overlay: while it is up, it takes the presses and the frame.
    virtual bool gameBriefOpen() const = 0;
    virtual bool onGameBriefInput(const ButtonEvent& ev) = 0;
    virtual void drawGameBrief(Framebuffer& fb) const = 0;
    virtual void playSound(Sound s) = 0;
    virtual bool inArcadeRun() const = 0;   // a cabinet's run, rather than a pet's
    virtual int screenBeat() const = 0;
    // The footer for a run worth `sofar`, in whichever context is armed.
    virtual PlayHud playHud(const PlayOutcome& sofar) const = 0;

protected:
    ~BoardHost() = default;
};

}  // namespace mal
