#include "core/app/game/warp_picker_screen.h"

#include "core/content/defs.h"   // ItemDef::displayName
#include "core/ui/expl_screen.h"

namespace mal {

void WarpPickerScreen::onButton(WarpPickerHost& host, const ButtonEvent& ev) {
    // The list is rebuilt each press so a key that ran out (spent) can't leave a stale
    // cursor.
    auto keys = host.heldWarpKeys();
    if (keys.empty()) { host.returnToExplore(); return; }
    if (row_ >= static_cast<int>(keys.size())) row_ = 0;
    if (ev.button == Button::A) {
        row_ = (row_ + 1) % static_cast<int>(keys.size());
    } else if (ev.button == Button::B) {
        host.useWarpKey(*keys[row_]);
    } else if (ev.button == Button::C) {
        host.returnToExplore();
    }
}

void WarpPickerScreen::draw(Framebuffer& fb, const WarpPickerHost& host) const {
    // Snapshot the held keys' display names into a small array the UI helper reads
    // (the picker is at most a couple of rows: one per warp-key type).
    auto keys = host.heldWarpKeys();
    const char* names[8];
    int n = 0;
    for (const ItemDef* d : keys) {
        if (n >= 8) break;
        names[n++] = d->displayName;
    }
    int cursor = row_;
    if (n > 0 && cursor >= n) cursor = 0;
    drawWarpPicker(fb, names, n, cursor);
}

}  // namespace mal
