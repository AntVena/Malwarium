// crew_screen.h: the Hacker face's CREW slot, as a feature that owns its own state.
//
// Four views over one slot (CrewView): the Hub (who you are, and the two sides), a side's
// Team roster, one crew's Detail page, and the home-network Picker, a modal over the Hub.
// The screen holds which view is up and the cursor inside each. Membership and the home
// network are the Operator record's, which the screen reads by const reference and changes
// only through CrewHost.
//
// This header names no core/ui type, so game.h can hold a CrewScreen by value.
#pragma once

#include <cstdint>

#include "core/content/content_crews.h"   // CrewTeam
#include "core/device/platform.h"

namespace mal {

class Framebuffer;
struct Operator;

// The CREW screen's four views, deepest last. `Picker` is a modal over the Hub, because
// the rest of the screen needs a home network first.
enum class CrewView : uint8_t { Hub, Picker, Team, Detail };

// One offer in the home-network picker: a network the radio can hear right now. The
// SD-backed ledger contributes no rows; it only supplies `count`, the evidence that says
// which of the networks in front of you is the familiar one.
struct CrewNetRow {
    uint64_t key = 0;
    const char* name = "";
    uint32_t count = 0;   // ledger sighting tally; 0 = never resolved by a walk yet
};

// What CREW needs from the game. Game implements it; a test can too.
class CrewHost {
public:
    virtual const Operator& operatorRecord() const = 0;
    virtual int screenBeat() const = 0;
    // The in-range offers, most-sighted first, capped at `max`. Returns the count.
    virtual int crewNetworkRows(CrewNetRow* rows, int max) const = 0;
    // Who holds the radio when the picker's scan cannot run (RadioArbiter's priority).
    virtual bool apEnabled() const = 0;
    virtual bool captureHoldsRadio() const = 0;

    virtual void setHomeNetwork(uint64_t key, const char* name) = 0;
    virtual bool joinCrew(int index) = 0;     // refused without a home network
    virtual void leaveCrew() = 0;
    virtual void leaveToCarousel() = 0;       // C on the Hub

protected:
    ~CrewHost() = default;
};

class CrewScreen {
public:
    // Every entry opens at the top of the Hub.
    void enter() { *this = CrewScreen{}; }
    void onButton(CrewHost& host, const ButtonEvent& ev);
    // The Hub, Team and Picker are row lists; Detail pages its prose on A instead.
    void stepList(const CrewHost& host, int dir);
    void draw(Framebuffer& fb, const CrewHost& host) const;

    CrewView view() const { return view_; }

private:
    void drawHub(Framebuffer& fb, const CrewHost& host) const;
    void drawNetPicker(Framebuffer& fb, const CrewHost& host) const;
    void drawTeam(Framebuffer& fb, const CrewHost& host) const;
    void drawDetail(Framebuffer& fb, const CrewHost& host) const;

    // Rows on the Hub: HOME NET, then one per side. The sides are the fixed pair, not
    // a count over the roster: an empty side still has to be visitable, or a crew
    // added to it would have nowhere to appear.
    static constexpr int kHubRows = 3;

    CrewView view_ = CrewView::Hub;
    int hubRow_ = 0;                  // 0 = HOME NET, 1 = RED, 2 = BLUE
    CrewTeam team_ = CrewTeam::Red;   // the side Team and Detail are showing
    int teamRow_ = 0;                 // cursor within that side's roster
    int detail_ = -1;                 // the kCrews row Detail is showing
    int proseScroll_ = 0;             // Detail: first prose line drawn (A pages it)
    int netRow_ = 0;
};

}  // namespace mal
