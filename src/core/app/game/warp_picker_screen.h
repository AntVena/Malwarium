// warp_picker_screen.h: the walk's warp-key picker (Nav::WarpPicker), opened from the
// explore control overlay's WARP row.
//
// A flat list of the held warp keys: A cycles the row, B spends the focused key, C backs
// out to the habitat with the walk still running. The screen holds its cursor. The keys
// are the inventory's and spending one is the game's, through WarpPickerHost.
#pragma once

#include <vector>

#include "core/device/platform.h"

namespace mal {

class Framebuffer;
struct ItemDef;

// What the picker needs from the game. Game implements it; a test can too.
class WarpPickerHost {
public:
    virtual std::vector<const ItemDef*> heldWarpKeys() const = 0;   // inventory order
    virtual void useWarpKey(const ItemDef& key) = 0;                // jumps to its event
    virtual void returnToExplore() = 0;

protected:
    ~WarpPickerHost() = default;
};

class WarpPickerScreen {
public:
    void open() { row_ = 0; }
    void onButton(WarpPickerHost& host, const ButtonEvent& ev);
    void draw(Framebuffer& fb, const WarpPickerHost& host) const;

private:
    int row_ = 0;
};

}  // namespace mal
