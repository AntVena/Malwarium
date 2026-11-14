// post_encounter_screen.h: the readout a fought wild battle parks on before the walk
// resumes (Nav::PostEncounter).
//
// It shows what the fight did to the pet's Bandwidth and fragmentation and any level it
// gained, all of it the last fight's FightResult (core/model/fight.h). The screen holds
// nothing: it draws that result, and any press dismisses it, since the readout has
// nothing to confirm or cancel. The game also dismisses it on its own clock.
#pragma once

#include "core/device/platform.h"

namespace mal {

class Framebuffer;
struct FightResult;

// What the readout needs from the game. Game implements it; a test can too.
class PostEncounterHost {
public:
    virtual const FightResult& lastFightResult() const = 0;
    virtual int bandwidthMax() const = 0;
    virtual void dismissPostEncounter() = 0;   // back to the walk

protected:
    ~PostEncounterHost() = default;
};

class PostEncounterScreen {
public:
    // Any button: unlike the standard A/B/C contract, every press is "seen it".
    void onButton(PostEncounterHost& host, const ButtonEvent&) { host.dismissPostEncounter(); }
    void draw(Framebuffer& fb, const PostEncounterHost& host) const;
};

}  // namespace mal
