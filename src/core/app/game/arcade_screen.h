// arcade_screen.h: the GAMES slot's three screens, as a feature that owns its own state.
//
// The cabinet list (L2), one cabinet's page with its difficulty dial (L3), and the payout
// after a run. The screen holds the list cursor, the dial and the payout's frozen
// result. A run started from the page takes a copy of the cabinet and the dial
// (ArcadeRun), so the list stays where the player left it while the game plays. Which
// cabinets are offered and what they have been played to are the game's, read through
// ArcadeHost.
//
// This header names no core/ui type, so game.h can hold an ArcadeScreen by value.
#pragma once

#include "core/content/content_arcade.h"   // ArcadeDifficulty
#include "core/device/platform.h"
#include "core/model/arcade_run.h"         // ArcadeResult

namespace mal {

class ContentRegistry;
class Framebuffer;

// What GAMES needs from the game. Game implements it; a test can too.
class ArcadeHost {
public:
    virtual const ContentRegistry& content() const = 0;
    virtual int screenBeat() const = 0;
    // Is cabinet `i` in the list right now (ArcadeUnlock)? A locked cabinet is absent,
    // not greyed, so a menu never advertises a door the player has no key to.
    virtual bool arcadeCabinetOffered(int i) const = 0;
    // The roster indices of every offered cabinet, in table order. Returns the count.
    virtual int arcadeOfferedRows(int* out, int max) const = 0;
    virtual int arcadePlays(int i) const = 0;
    virtual int arcadeWins(int i) const = 0;
    virtual int arcadeBest(int i) const = 0;

    virtual void startArcadeRun(int cabinet, ArcadeDifficulty dial) = 0;
    virtual void openDetail() = 0;        // L2 -> L3
    virtual void closeDetail() = 0;       // L3 -> L2, and off the payout
    virtual void leaveToCarousel() = 0;

protected:
    ~ArcadeHost() = default;
};

class ArcadeScreen {
public:
    // Every entry opens the list on the first cabinet. The dial keeps its setting.
    void enter() { row_ = 0; }
    // A reset puts the dial back to its default too.
    void reset() { *this = ArcadeScreen{}; }
    // The payout screen's result, frozen when the run settles.
    void showResult(const ArcadeResult& r) { result_ = r; }

    void onList(ArcadeHost& host, const ButtonEvent& ev);
    void onCabinet(ArcadeHost& host, const ButtonEvent& ev);
    // Informational: the run is banked already, so any press leaves.
    void onResult(ArcadeHost& host, const ButtonEvent&) { host.closeDetail(); }
    // The list's cursor walks raw table order and stops only on offered cabinets.
    void stepList(const ArcadeHost& host, int dir);

    void drawList(Framebuffer& fb, const ArcadeHost& host) const;
    void drawCabinet(Framebuffer& fb, const ArcadeHost& host) const;
    void drawResult(Framebuffer& fb, const ArcadeHost& host) const;

    // A ROSTER index throughout, so the tallies and the start call index the same space
    // whether or not anything is hidden. It differs from the row position whenever a
    // cabinet is locked out of the list.
    int row() const { return row_; }
    ArcadeDifficulty difficulty() const { return difficulty_; }

private:
    int row_ = 0;
    ArcadeDifficulty difficulty_ = ArcadeDifficulty::Medium;
    ArcadeResult result_;
};

}  // namespace mal
