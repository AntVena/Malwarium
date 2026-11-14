// egg_pick_screen.h: the Clutch Pick's modal (Nav::ModalEggPick), the screen over an
// EggPick run (core/model/egg_pick.h).
//
// The run holds the rules: where the live egg hides, the span still in play, which half
// is aimed at and the verdict. The screen holds nothing of its own. It owns the raft's
// geometry and its draw, and turns A and C into aiming at the first or second half and
// B into a commit. The game carries those out, because a pick's result halves the
// egg's clock or pays a cabinet.
#pragma once

#include "core/app/game/board_host.h"

namespace mal {

struct EggPick;

// What the modal needs from the game. Game implements it; a test can too. A cabinet's
// raft (inArcadeRun) has no egg behind it, and the heartbeat is the live tile's tell.
class EggPickHost : public BoardHost {
public:
    virtual const EggPick* eggPick() const = 0;   // null with no pick open
    virtual void eggPickAim(bool secondHalf) = 0;
    virtual void eggPickCommit() = 0;   // halve, or dismiss the reveal

protected:
    ~EggPickHost() = default;
};

class EggPickScreen {
public:
    void onButton(EggPickHost& host, const ButtonEvent& ev);
    void draw(Framebuffer& fb, const EggPickHost& host) const;

    // Whether the live egg may hide in `slot`. A cell that wraps the raft's edge
    // straddles both halves, so the egg is never put there.
    static bool slotCanHideEgg(int slot);
};

}  // namespace mal
