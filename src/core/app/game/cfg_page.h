// cfg_page.h: what a CFG page that is a type of its own shares with the menu it sits in.
//
// Where a page sits in the CFG menu is the menu's business (CfgMenu), so a page answers
// a press with the move it asks of the menu rather than making it, and the menu makes
// it (CfgMenu::follow). The value pickers all take a press the same way, so that shape is
// here too, once.
#pragma once

#include <cstdint>

#include "core/device/platform.h"
#include "core/ui/ui_state.h"           // CfgScreen

namespace mal {

// What a press asks of the menu: nothing, back out of the page (to its group, or to the
// CFG list), or put up another page in its place without leaving (a QR behind a row).
struct CfgMove {
    enum class Kind : uint8_t { Stay, Leave, Show };
    Kind kind = Kind::Stay;
    CfgScreen page = CfgScreen::SysInfo;   // the page Show puts up

    static CfgMove stay() { return {}; }
    static CfgMove leave() { return {Kind::Leave, CfgScreen::SysInfo}; }
    static CfgMove show(CfgScreen p) { return {Kind::Show, p}; }
};

// The shape every value picker shares: A steps the focus, B applies it, C backs out with
// nothing changed. `apply` answers the move, so a row that refuses (a locked set) stays
// put and one that moves on to a page of its own (the AP's QR) shows it.
template <class Step, class Apply>
CfgMove pickerPress(const ButtonEvent& ev, Step step, Apply apply) {
    if (ev.button == Button::A) step();
    else if (ev.button == Button::B) return apply();
    else if (ev.button == Button::C) return CfgMove::leave();
    return CfgMove::stay();
}

}  // namespace mal
