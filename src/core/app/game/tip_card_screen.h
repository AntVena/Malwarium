// tip_card_screen.h: the one-time TIP CARD (Nav::TipCard), as a feature that owns its own
// state.
//
// A short page of prose shown once, at a moment the player is certainly looking. The
// screen holds which tip it is showing, the window it is scrolled to, and whether it
// leads with the "!" explainer. Which tips are due, marking them seen, and where
// dismissing one returns are the game's (game_onboard.cpp), reached through TipCardHost.
//
// This header names no core/ui type beyond the prose row it returns, so game.h can hold
// a TipCardScreen by value.
#pragma once

#include <vector>

#include "core/app/operator.h"   // Tip
#include "core/device/platform.h"

namespace mal {

class Framebuffer;
struct Pet;
struct ProseRow;

// What a tip card needs from the game. Game implements it; a test can too.
class TipCardHost {
public:
    virtual int screenBeat() const = 0;
    // The NEAR THE LINE card says whether the pet's once-per-life shield is armed.
    virtual const Pet& petRecord() const = 0;
    virtual void closeTipCard() = 0;   // back to wherever the card was opened over

protected:
    ~TipCardHost() = default;
};

class TipCardScreen {
public:
    // `markRow`: lead with the "!" explainer, which only the first CARE card shows.
    void open(Tip t, bool markRow) {
        tip_ = t;
        scroll_ = 0;
        markRow_ = markRow;
    }
    void onButton(TipCardHost& host, const ButtonEvent& ev);
    void draw(Framebuffer& fb, const TipCardHost& host) const;
    std::vector<ProseRow> rows(const TipCardHost& host) const;

    Tip tip() const { return tip_; }
    // The short name CFG > TIPS lists a card under.
    static const char* title(Tip t);

private:
    Tip tip_ = Tip::MenuOpened;
    int scroll_ = 0;
    bool markRow_ = false;
};

}  // namespace mal
