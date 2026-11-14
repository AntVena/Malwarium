// fragger_screen.h: the FRAGGER board (Nav::Fragger), a played MAINT run and a GAMES
// cabinet, over the Fragger run (core/model/fragger.h).
//
// The run holds the rules. The screen holds the beam of the last shot, which lasts one
// row of fall, and owns the board's input and draw: A and C move the head, B fires, and
// a press on the parked result moves on. What a run buys is the game's
// (FraggerHost::finishFragger).
#pragma once

#include "core/app/game/board_host.h"

namespace mal {

class Fragger;

class FraggerHost : public BoardHost {
public:
    virtual Fragger& liveFragger() = 0;
    virtual const Fragger& fragger() const = 0;
    virtual void finishFragger() = 0;

protected:
    ~FraggerHost() = default;
};

class FraggerScreen {
public:
    // No beam: a fresh run, and every row of fall after a shot.
    void clearShot() { shotLane_ = -1; shotRow_ = -1; }
    void clearBeamLane() { shotLane_ = -1; }
    void onButton(FraggerHost& host, const ButtonEvent& ev);
    void draw(Framebuffer& fb, const FraggerHost& host) const;

private:
    int shotLane_ = -1;
    int shotRow_ = -1;
};

}  // namespace mal
