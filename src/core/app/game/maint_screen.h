// maint_screen.h: the MAINT action page (Nav::Detail under MAINT), as a feature that owns
// its own state.
//
// One process's page: which process is open, the focused payment variant (DEFRAG:
// Quick / Tool / Played; AV: Quick / Played), and the PLAYED row's difficulty dial. A
// run started from it takes a copy of all three (MaintRun); the price, the wallet and
// the roll are the game's, reached through MaintHost.
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
    virtual ArcadeGameKind maintPlayedGame(MaintKind k) const = 0;

    virtual void startMaint(MaintKind kind, int variant, ArcadeDifficulty dial) = 0;
    virtual void closeDetail() = 0;

protected:
    ~MaintHost() = default;
};

class MaintScreen {
public:
    // Every opening starts on QUICK with the dial at EASY.
    void open(MaintKind kind) {
        kind_ = kind;
        variant_ = 0;
        difficulty_ = ArcadeDifficulty::Easy;
    }
    void onButton(MaintHost& host, const ButtonEvent& ev);
    void draw(Framebuffer& fb, const MaintHost& host) const;

    MaintKind kind() const { return kind_; }
    ArcadeDifficulty difficulty() const { return difficulty_; }
    // Test seams (Game's debug hooks): a played run started without the page, at a dial.
    void setKind(MaintKind k) { kind_ = k; }
    void setDifficulty(ArcadeDifficulty d) { difficulty_ = d; }

private:
    MaintKind kind_ = MaintKind::Defrag;
    // The played variant is the last one, and the only one that can come up short
    // without failing.
    int variant_ = 0;
    ArcadeDifficulty difficulty_ = ArcadeDifficulty::Easy;
};

}  // namespace mal
