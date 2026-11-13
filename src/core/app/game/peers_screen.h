// peers_screen.h: the Hacker face's PEERS slot, as a feature that owns its own state.
//
// The operators this device has met: the live radio snapshot merged over the SD roster,
// live ones first. The screen holds its cursor and reports; the meetings themselves are
// the game's (game_peers.cpp), read through PeersHost. B is unbound here: what a
// selected peer would enable lives on the LINK slot.
#pragma once

#include <cstdint>

#include "core/device/platform.h"

namespace mal {

class Framebuffer;

// One row of the PEERS screen: an operator, live or remembered.
struct PeerRow {
    uint64_t key = 0;
    const char* tag = "";
    const char* petName = "";
    const char* crewName = "";
    uint8_t stage = 0;
    uint8_t rank = 0;
    bool crewRed = false;
    bool live = false;      // in range right now (vs. remembered from before)
    uint32_t timesMet = 0;
};

// What PEERS needs from the game. Game implements it; a test can too.
class PeersHost {
public:
    // Every known operator, live first, capped at `max`. Returns the count.
    virtual int peerRows(PeerRow* rows, int max) const = 0;
    // Who holds the radio when the link cannot run (RadioArbiter's priority).
    virtual bool apEnabled() const = 0;
    virtual bool captureHoldsRadio() const = 0;
    // LINK is on but has no ambient airtime, because the audit scan it borrows is off.
    virtual bool linkAmbientStarved() const = 0;
    virtual void leaveToCarousel() = 0;

protected:
    ~PeersHost() = default;
};

class PeersScreen {
public:
    void enter() { row_ = 0; }
    void onButton(PeersHost& host, const ButtonEvent& ev);
    void stepList(const PeersHost& host, int dir);
    void draw(Framebuffer& fb, const PeersHost& host) const;

private:
    int row_ = 0;
};

}  // namespace mal
