// stacker_screen.h: the Stacker board (Nav::Stacker), a played MAINT run and a GAMES
// cabinet, over the Stacker run (core/model/stacker.h).
//
// The run holds the rules. The screen holds nothing of its own: B drops the block in
// hand, C stops and banks the board, and a press on the parked result moves on. What a
// run buys is the game's (StackerHost::finishStacker).
#pragma once

#include "core/app/game/board_host.h"

namespace mal {

class Stacker;

class StackerHost : public BoardHost {
public:
    virtual Stacker& liveStacker() = 0;
    virtual const Stacker& stacker() const = 0;
    virtual void finishStacker() = 0;

protected:
    ~StackerHost() = default;
};

class StackerScreen {
public:
    void onButton(StackerHost& host, const ButtonEvent& ev);
    void draw(Framebuffer& fb, const StackerHost& host) const;
};

}  // namespace mal
