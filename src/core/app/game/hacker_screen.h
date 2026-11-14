// hacker_screen.h: the Hacker face's own screens, its home (Nav::Idle and Nav::Cursor
// on that face) and its PROFILE slot.
//
// The home is the terminal-reskin centre canvas over the hacker carousel; PROFILE is the
// read-only operator viewer behind one slot. Neither holds anything: what they show is
// the Operator record and what the game works out from it, reached through HackerHost.
// The carousel's cursor and the walk between slots are the face-agnostic carousel
// contract, which stays the game's, and each other hacker slot has a screen of its own.
//
// This header names no core/ui type, so game.h can hold a HackerScreen by value.
#pragma once

#include "core/device/platform.h"

namespace mal {

class Framebuffer;
struct CrewDef;
struct Operator;
struct PowerStatus;
struct Settings;

// What the Hacker face needs from the game. Game implements it; a test can too.
class HackerHost {
public:
    virtual const Operator& operatorRecord() const = 0;
    virtual const Settings& settingsRecord() const = 0;     // the carousel's label mode
    virtual const PowerStatus& powerStatus() const = 0;
    virtual int batteryLevel() const = 0;                   // glyph frame, or -1 with no reading
    virtual const CrewDef* activeCrew() const = 0;          // null with no crew
    virtual const char* equippedTitleName() const = 0;
    virtual bool hasHomeNetwork() const = 0;
    virtual int shakesUnspent() const = 0;
    virtual int sigilsKnown() const = 0;
    virtual int speciesRaised() const = 0;
    virtual int pendingNetworks() const = 0;                // sightings not yet on SD
    virtual int bandwidth() const = 0;
    virtual int bandwidthMax() const = 0;
    virtual bool hackerSlotAccessible(int slot) const = 0;  // the same answer B is given
    virtual int screenBeat() const = 0;
    virtual void leaveToCarousel() = 0;

protected:
    ~HackerHost() = default;
};

class HackerScreen {
public:
    // The home: the operator's identity and standing over the carousel, `cursor` the
    // focused slot or -1 at rest.
    void drawHome(Framebuffer& fb, const HackerHost& host, int cursor) const;
    void onProfile(HackerHost& host, const ButtonEvent& ev);
    void drawProfile(Framebuffer& fb, const HackerHost& host) const;
};

}  // namespace mal
