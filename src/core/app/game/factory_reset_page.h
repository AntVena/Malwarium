// factory_reset_page.h: CFG's Factory Reset, reached by the hidden hold on SYSTEM INFO.
//
// The page holds the scope A cycles, and its own held B commits the reset in that scope.
// What each scope clears is the game's (Game::executeFactoryReset), reached through
// FactoryResetHost, with the hold's progress.
//
// This header names no core/ui type beyond ui_state.h, so game.h can hold the page
// (inside CfgMenu) by value.
#pragma once

#include <cstdint>

#include "core/app/game/cfg_page.h"

namespace mal {

class Framebuffer;

// What a Factory Reset clears, the choice A cycles on its page.
enum class FactoryScope : uint8_t { ResetPet, WipeEverything };

class FactoryResetHost {
public:
    virtual void executeFactoryReset(FactoryScope scope) = 0;
    virtual void armHoldB() = 0;
    // How long B has been held on this page, or -1 with it up.
    virtual int32_t heldBMs() const = 0;

protected:
    ~FactoryResetHost() = default;
};

class FactoryResetPage {
public:
    void open() { scope_ = FactoryScope::ResetPet; }   // the narrower scope first
    CfgMove onButton(FactoryResetHost& host, const ButtonEvent& ev);
    // The held B, once it has lasted kFactoryCommitMs: true when the reset landed.
    bool holdB(FactoryResetHost& host, uint32_t heldMs);
    void draw(Framebuffer& fb, const FactoryResetHost& host) const;

private:
    FactoryScope scope_ = FactoryScope::ResetPet;
};

}  // namespace mal
