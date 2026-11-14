// isolation_screen.h: the Isolation Protocol board (Nav::Isolation), the Worm line's
// hatch game and a GAMES cabinet, over the Isolation run (core/model/isolation.h).
//
// The run holds the rules. The screen holds nothing of its own: it owns the buffer's
// geometry and draw, A and C steer, and B on the parked result banks and leaves. What a
// run buys is the game's (IsolationHost::finishIsolation).
#pragma once

#include "core/app/game/board_host.h"

namespace mal {

class Isolation;

class IsolationHost : public BoardHost {
public:
    virtual Isolation& liveIsolation() = 0;
    virtual const Isolation& isolation() const = 0;
    virtual void finishIsolation() = 0;

protected:
    ~IsolationHost() = default;
};

class IsolationScreen {
public:
    void onButton(IsolationHost& host, const ButtonEvent& ev);
    void draw(Framebuffer& fb, const IsolationHost& host) const;
};

}  // namespace mal
