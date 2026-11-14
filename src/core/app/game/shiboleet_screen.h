// shiboleet_screen.h: THE SHIBOLEET's three pages (Nav::ShiboleetHail, Nav::Shiboleet,
// Nav::ShiboleetVerdict), as one screen type.
//
// A guardian meeting is a hail, then a riddle when the fluency roll called for one, then
// a verdict. The pages are never open together and are one meeting, so they are one
// screen. It holds the riddle's cursor, owns the three pages' input and draw, and reads
// the meeting (core/model/shiboleet_meeting.h: the welcome band, the cipher, the reply
// and what it paid) through ShiboleetHost. Moving the meeting on is the game's
// (game_shiboleet.cpp), because its consequences land on the pet and the walk.
#pragma once

#include <cstdint>

#include "core/device/platform.h"
#include "core/render/swarm.h"   // SwarmView

namespace mal {

class Framebuffer;
struct ShiboleetMeeting;

// What the pages need from the game. Game implements it; a test can too.
class ShiboleetHost {
public:
    virtual const ShiboleetMeeting& shiboleetMeeting() const = 0;
    virtual SwarmView guardianSwarm() const = 0;   // the meeting's flock, as the draw sees it
    // The pet's standing in the Cant, which the hail shows beside the guardian.
    virtual uint32_t cantSigils() const = 0;
    virtual int shakesUnspent() const = 0;
    virtual int eventHoldBeat() const = 0;   // how far the page's hands-off hold has run

    // Each page moves on by itself after a hold; a press starts that hold again so a
    // player still reading is not hurried past it.
    virtual void restartEventHold() = 0;
    virtual void openShiboleetWelcome() = 0;     // past the hail
    virtual void answerShiboleet(int row) = 0;
    virtual void leaveShiboleetUnanswered() = 0;
    virtual void finishShiboleet() = 0;          // past the verdict

protected:
    ~ShiboleetHost() = default;
};

class ShiboleetScreen {
public:
    // A riddle always opens on its first reply.
    void open() { row_ = 0; }
    int row() const { return row_; }

    void onHail(ShiboleetHost& host, const ButtonEvent& ev);
    void onRiddle(ShiboleetHost& host, const ButtonEvent& ev);
    void onVerdict(ShiboleetHost& host, const ButtonEvent& ev);
    void drawHail(Framebuffer& fb, const ShiboleetHost& host) const;
    void drawRiddle(Framebuffer& fb, const ShiboleetHost& host) const;
    void drawVerdict(Framebuffer& fb, const ShiboleetHost& host) const;

private:
    int row_ = 0;   // the reply under the cursor
};

}  // namespace mal
