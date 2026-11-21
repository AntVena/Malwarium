#include "core/app/game/warp_picker_screen.h"

#include <cstdio>

#include "core/content/defs.h"   // ItemDef::displayName, effects
#include "core/ui/expl_screen.h"
#include "core/ui/layout.h"

namespace mal {

namespace {

// The cursor's next destination `dir` way that a held key reaches. Rebuilt from the
// bag each call, so a key spent elsewhere can't leave the cursor on a row it no
// longer opens.
int stepHeld(const std::vector<WarpDestination>& d, int cur, int dir) {
    return stepRow(cur, static_cast<int>(d.size()), dir,
                   [&](int i) { return d[static_cast<size_t>(i)].held; });
}

}  // namespace

void WarpPickerScreen::open(const WarpPickerHost& host) {
    row_ = stepHeld(host.warpDestinations(), -1, +1);
    if (row_ < 0) row_ = 0;
}

void WarpPickerScreen::onButton(WarpPickerHost& host, const ButtonEvent& ev) {
    const auto dests = host.warpDestinations();
    const int n = static_cast<int>(dests.size());
    if (row_ >= n) row_ = 0;
    if (ev.button == Button::A) {
        stepList(host, +1);
    } else if (ev.button == Button::B) {
        if (n > 0 && dests[static_cast<size_t>(row_)].held) host.warpTo(dests[static_cast<size_t>(row_)]);
    } else if (ev.button == Button::C) {
        host.returnToExplore();
    }
}

void WarpPickerScreen::stepList(const WarpPickerHost& host, int dir) {
    row_ = stepHeld(host.warpDestinations(), row_, dir);
}

void WarpPickerScreen::draw(Framebuffer& fb, const WarpPickerHost& host) const {
    const auto dests = host.warpDestinations();
    WarpPickerRow rows[kWarpPickerMaxRows];
    char restTag[kWarpPickerMaxRows][16];
    int n = 0;
    for (const WarpDestination& d : dests) {
        if (n >= kWarpPickerMaxRows) break;
        WarpPickerRow& r = rows[n++];
        r.name = d.name;
        // The rest is tagged with what it gives, the key row's own Frag magnitude
        // (resolveSafeRestEvent applies the same effects).
        int frag = 0;
        if (d.key)
            for (const ItemEffect& e : d.key->effects)
                if (e.kind == ItemEffect::Kind::Frag) frag += e.magnitude;
        std::snprintf(restTag[n - 1], sizeof(restTag[n - 1]), "%d FRAG", frag);
        r.kind = d.target == WarpTarget::ItemShop  ? "ITEMS"
                 : d.target == WarpTarget::ModShop ? "MODS"
                                                   : restTag[n - 1];
        r.keyName = d.key ? d.key->displayName : "";
        r.held = d.held;
    }
    drawWarpPicker(fb, rows, n, row_ < n ? row_ : 0);
}

}  // namespace mal
