// shiboleet_screen.h: THE SHIBOLEET's three pages (Nav::ShiboleetHail, Nav::Shiboleet,
// Nav::ShiboleetVerdict), as one screen type.
//
// A guardian meeting is a hail, then a riddle when the fluency roll called for one, then
// a verdict. The pages are never open together and are one meeting, so they are one
// screen. It holds the riddle's cursor, owns the three pages' input and draw, and
// reaches the meeting through ShiboleetHost. The meeting itself (the welcome band, the
// cipher, the reply and what it paid) stays the game's (game_shiboleet.cpp), because
// its consequences land on the pet and the walk.
#pragma once

#include <cstdint>

#include "core/content/areas/area_defs.h"   // GuardianOutcome
#include "core/device/platform.h"
#include "core/render/swarm.h"   // SwarmView

namespace mal {

class Framebuffer;

// What the pages need from the game. Game implements it; a test can too.
class ShiboleetHost {
public:
    virtual const char* guardianName() const = 0;
    virtual const char* guardianDemeanour() const = 0;   // what the pet can see it doing
    virtual uint32_t cantSigils() const = 0;
    virtual int shakesUnspent() const = 0;
    virtual SwarmView guardianSwarm() const = 0;
    // The meeting's words, already drawn in the Cant.
    virtual void shiboleetGreeting(char* out, int cap) const = 0;
    virtual void shiboleetRiddleText(char* out, int cap) const = 0;
    virtual void shiboleetReplyText(int row, char* out, int cap) const = 0;
    virtual void shiboleetOutcomeSpeech(char* out, int cap) const = 0;
    // How it came out, and what that paid or cost.
    virtual GuardianOutcome shiboleetOutcome() const = 0;
    virtual const char* shiboleetOutcomeSeen() const = 0;
    virtual const char* shiboleetVerdictLine() const = 0;
    virtual const char* shiboleetFlavor() const = 0;
    virtual bool shiboleetVerdictFights() const = 0;
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
