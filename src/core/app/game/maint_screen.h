// maint_screen.h: the MAINT slot's three screens, as a feature that owns its own state.
//
// The process list (L2: DEFRAG, AV) and one process's action page (L3): which process is
// open, the focused payment variant (DEFRAG: Quick / Tool / Played; AV: Quick / Played),
// and the PLAYED row's difficulty dial. A run started from the page takes a copy of all
// three (MaintRun); the price, the wallet and the roll are the game's, reached through
// MaintHost. The process screen (Nav::Process) draws that run's bar and then its outcome,
// and holds nothing: the run is the game's until the outcome is dismissed.
//
// This header names no core/ui type, so game.h can hold a MaintScreen by value.
#pragma once

#include "core/content/content_arcade.h"   // ArcadeDifficulty, ArcadeGameKind
#include "core/device/platform.h"
#include "core/model/maint_run.h"          // MaintKind

namespace mal {

class Framebuffer;
class Inventory;
struct Operator;
struct Pet;

// What the MAINT action page needs from the game. Game implements it; a test can too.
class MaintHost {
public:
    virtual const Operator& operatorRecord() const = 0;   // the wallet the price is read against
    virtual const Pet& petRecord() const = 0;             // the disk, and its defrag tally
    virtual const Inventory& inventory() const = 0;       // Defrag Tools held
    virtual int defragCost() const = 0;
    virtual int maintFailPct() const = 0;                 // a QUICK run's chance to fail
    virtual int screenBeat() const = 0;
    virtual ArcadeGameKind maintPlayedGame(MaintKind k) const = 0;
    virtual const MaintRun* maintRun() const = 0;   // the process in play, or null

    virtual void startMaint(MaintKind kind, int variant, ArcadeDifficulty dial) = 0;
    virtual void openDetail() = 0;
    virtual void closeDetail() = 0;
    virtual void leaveToCarousel() = 0;
    virtual void dismissMaint() = 0;   // end the resolved run, back to its action page

protected:
    ~MaintHost() = default;
};

class MaintScreen {
public:
    // Every entry opens the list on DEFRAG.
    void enter() { row_ = 0; }

    // L2: the process list.
    void onButton(MaintHost& host, const ButtonEvent& ev);
    void stepList(const MaintHost&, int dir);
    void draw(Framebuffer& fb, const MaintHost& host) const;
    // L3: the open process's page.
    void onDetail(MaintHost& host, const ButtonEvent& ev);
    void drawDetail(Framebuffer& fb, const MaintHost& host) const;
    // The process screen: the run's bar, then its outcome.
    void onProcess(MaintHost& host, const ButtonEvent& ev);
    void drawProcess(Framebuffer& fb, const MaintHost& host) const;

    MaintKind kind() const { return kind_; }
    ArcadeDifficulty difficulty() const { return difficulty_; }
    // Test seams (Game's debug hooks): a played run started without the page, at a dial.
    void setKind(MaintKind k) { kind_ = k; }
    void setDifficulty(ArcadeDifficulty d) { difficulty_ = d; }

private:
    // Every opening starts on QUICK with the dial at EASY.
    void open(MaintKind kind) {
        kind_ = kind;
        variant_ = 0;
        difficulty_ = ArcadeDifficulty::Easy;
    }

    static constexpr int kRows = 2;   // DEFRAG, AV
    int row_ = 0;
    MaintKind kind_ = MaintKind::Defrag;
    // The played variant is the last one, and the only one that can come up short
    // without failing.
    int variant_ = 0;
    ArcadeDifficulty difficulty_ = ArcadeDifficulty::Easy;
};

}  // namespace mal
