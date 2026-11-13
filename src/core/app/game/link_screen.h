// link_screen.h: the Hacker face's LINK slot, as a feature that owns its own state.
//
// The challengeable operators in range, or, while a duel session is open, that session's
// state: a challenge out, one in, the seed being agreed, the verdict. The screen holds
// its cursor. The session, the frames and why the last one ended are the game's, reached
// through LinkHost, because a session runs whatever screen is up.
#pragma once

#include <cstdint>

#include "core/device/platform.h"

namespace mal {

class Framebuffer;
struct Pet;
struct PeerRow;
struct PvpSession;

// What LINK needs from the game. Game implements it; a test can too.
class LinkHost {
public:
    // Every known operator, live first (PeersHost's list): LINK offers the live ones.
    virtual int peerRows(PeerRow* rows, int max) const = 0;
    virtual bool apEnabled() const = 0;
    virtual bool captureHoldsRadio() const = 0;
    virtual bool linkAmbientStarved() const = 0;
    virtual const Pet& petRecord() const = 0;
    // The open session, or null between sessions; and why the last one ended (empty when
    // it did not end early).
    virtual const PvpSession* pvpSession() const = 0;
    virtual const char* pvpStatusText() const = 0;

    virtual bool pvpChallenge(uint64_t key) = 0;
    virtual void pvpAcceptChallenge() = 0;
    virtual void pvpDeclineChallenge() = 0;
    virtual void pvpCancel() = 0;
    virtual void clearPvpVerdict() = 0;   // the Result read, back to the list
    virtual void leaveToCarousel() = 0;

protected:
    ~LinkHost() = default;
};

class LinkScreen {
public:
    void enter() { row_ = 0; }
    void onButton(LinkHost& host, const ButtonEvent& ev);
    void draw(Framebuffer& fb, const LinkHost& host) const;

private:
    void onList(LinkHost& host, const ButtonEvent& ev);
    void drawSession(Framebuffer& fb, const PvpSession& s) const;
    void drawList(Framebuffer& fb, const LinkHost& host) const;

    int row_ = 0;
};

}  // namespace mal
