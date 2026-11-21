// warp_picker_screen.h: the walk's warp picker (Nav::WarpPicker), opened from the
// explore control overlay's WARP row.
//
// A list of the places a warp key can jump to in the armed area: its item shop, its mod
// shop, a safe rest. Each row names the key it spends, and a destination whose key the
// bag lacks stays on the list, dimmed, so the player sees what a key would open. A steps
// to the next destination a held key reaches, B jumps there, C backs out to the habitat
// with the walk still running. The screen holds its cursor; the destinations and
// spending a key are the game's, through WarpPickerHost.
#pragma once

#include <cstdint>
#include <vector>

#include "core/device/platform.h"

namespace mal {

class Framebuffer;
struct ItemDef;

enum class WarpTarget : uint8_t { ItemShop, ModShop, SafeRest };

struct WarpDestination {
    const ItemDef* key = nullptr;   // the warp key this jump spends
    WarpTarget target = WarpTarget::ItemShop;
    const char* name = "";          // the storefront's own name, or the rest's
    bool held = false;              // the key is in the bag
};

// What the picker needs from the game. Game implements it; a test can too.
class WarpPickerHost {
public:
    // Every destination the shipped warp keys reach from the armed area, in key order.
    virtual std::vector<WarpDestination> warpDestinations() const = 0;
    virtual void warpTo(const WarpDestination& d) = 0;   // spends the key, then jumps
    virtual void returnToExplore() = 0;

protected:
    ~WarpPickerHost() = default;
};

class WarpPickerScreen {
public:
    // Opens on the first destination a held key reaches.
    void open(const WarpPickerHost& host);
    void onButton(WarpPickerHost& host, const ButtonEvent& ev);
    void stepList(const WarpPickerHost& host, int dir);
    void draw(Framebuffer& fb, const WarpPickerHost& host) const;

private:
    int row_ = 0;
};

}  // namespace mal
