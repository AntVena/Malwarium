// tips_page.h: CFG's TIPS page, the tips the operator has been shown, to read again.
//
// The page holds the focused row. Which tips have been seen is the Operator's, and
// reading one opens the tip card, both reached through TipsHost.
//
// This header names no core/ui type beyond ui_state.h, so game.h can hold the page
// (inside CfgMenu) by value.
#pragma once

#include <vector>

#include "core/app/operator.h"          // Tip
#include "core/app/game/cfg_page.h"

namespace mal {

class Framebuffer;

class TipsHost {
public:
    // The tips seen so far, in the order TIPS lists them.
    virtual std::vector<Tip> replayableTips() const = 0;
    virtual void openTip(Tip t) = 0;                  // read, then back to TIPS
    virtual void markDirty() = 0;

protected:
    ~TipsHost() = default;
};

class TipsPage {
public:
    void open() { row_ = 0; }
    CfgMove onButton(TipsHost& host, const ButtonEvent& ev);
    void draw(Framebuffer& fb, const TipsHost& host) const;

private:
    int row_ = 0;
};

}  // namespace mal
