// line_select_screen.h: NEW SPECIMEN, the line-select modal (Nav::ModalLineSelect) an
// empty save opens on when more than one egg line is unlocked.
//
// A cursor over the unlocked egg lines: A cycles, B lays the highlighted line's egg, and
// C backs out to ARCH, but only with something on the rack to go back to. The screen
// holds its cursor. Which lines are unlocked, laying the egg and the way back are the
// game's, through LineSelectHost.
#pragma once

#include <vector>

#include "core/device/platform.h"

namespace mal {

class Framebuffer;
struct EggLineDef;

// What the modal needs from the game. Game implements it; a test can too.
class LineSelectHost {
public:
    virtual std::vector<const EggLineDef*> availableEggLines() const = 0;
    virtual bool archRackEmpty() const = 0;
    virtual void layEgg(const EggLineDef* line) = 0;
    virtual void archReturnFromLineSelect() = 0;

protected:
    ~LineSelectHost() = default;
};

class LineSelectScreen {
public:
    void open() { row_ = 0; }
    int row() const { return row_; }
    void onButton(LineSelectHost& host, const ButtonEvent& ev);
    void draw(Framebuffer& fb, const LineSelectHost& host) const;

private:
    int row_ = 0;
};

}  // namespace mal
