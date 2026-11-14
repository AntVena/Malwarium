// cfg_menu.h: the CFG slot's menu: the CFG list (L2), its two group pages (DEVICE,
// RADIO), and which page behind them is up.
//
// Each page behind the list is a type of its own, holding its focus and owning its input
// and draw (UpdatesPage, RadioPages, HardwarePages, LooksPages, SettingsPages, TipsPage,
// FactoryResetPage), handed only its own host. The menu holds the list and group
// cursors and which page is open, dispatches to the page that is up, and makes the move
// a page asks of it (CfgMove). The settings, the radios and the jobs are the game's.
//
// This header names no core/ui type beyond ui_state.h, so game.h can hold a CfgMenu by
// value.
#pragma once

#include <cstdint>

#include "core/app/game/cfg_page.h"
#include "core/app/game/factory_reset_page.h"
#include "core/app/game/hardware_pages.h"
#include "core/app/game/looks_pages.h"
#include "core/app/game/radio_pages.h"
#include "core/app/game/settings_pages.h"
#include "core/app/game/tips_page.h"
#include "core/app/game/updates_page.h"
#include "core/device/platform.h"
#include "core/device/radio_status.h"
#include "core/ui/ui_state.h"           // CfgScreen

namespace mal {

class Framebuffer;
struct Operator;
struct Settings;

// What CFG needs from the game. Game implements it; a test can too. It extends every
// page's host, so the menu can hand each page its own. What the menu calls itself is
// declared here, even where a page's host declares it too, so a call through CfgHost
// names one function; Game's one override serves every host that declares it.
class CfgHost : public UpdatesHost, public RadioHost, public HardwareHost,
                public LooksHost, public SettingsHost, public TipsHost,
                public FactoryResetHost {
public:
    // What the CFG list and the DEVICE group page summarise.
    virtual const Settings& settingsRecord() const = 0;
    virtual const Operator& operatorRecord() const = 0;
    virtual RadioOwner radioOwner() const = 0;
    virtual const char* equippedTitleName() const = 0;
    virtual const char* themeName() const = 0;

    // The list's RESET TO HATCH row, and the moves between the menu's levels.
    virtual void resetToHatch() = 0;
    virtual void disarmHoldB() = 0;
    virtual void markDirty() = 0;
    virtual void openDetail() = 0;
    virtual void closeDetail() = 0;
    virtual void leaveToCarousel() = 0;

protected:
    ~CfgHost() = default;
};

class CfgMenu {
public:
    void enter() { row_ = 0; }
    void reset() { *this = CfgMenu{}; }

    // L2: the CFG list.
    void onButton(CfgHost& host, const ButtonEvent& ev);
    void stepList(const CfgHost& host, int dir);
    void draw(Framebuffer& fb, const CfgHost& host) const;
    // L3: the open page. A group page is a list too.
    void onDetail(CfgHost& host, const ButtonEvent& ev);
    bool detailIsList() const { return page_ == CfgScreen::Device || page_ == CfgScreen::Radio; }
    void stepGroup(int dir);
    void drawDetail(Framebuffer& fb, const CfgHost& host) const;
    // System Info's hidden hold reveals Factory Reset, whose own hold commits it.
    bool holdB(CfgHost& host, uint32_t heldMs);
    void releaseB(CfgHost&) {}

    CfgScreen page() const { return page_; }
    // The idle budget of the open page. Every CFG page holds the menu tree (a setting is
    // being chosen, a QR scanned, a tag composed) except UPDATES, which lingers: it is a
    // screen you wait in front of, and five seconds is shorter than an association plus a
    // fetch, but an abandoned check must still collapse, or a forgotten screen pins the
    // radio open forever.
    MenuIdle idle() const {
        return page_ == CfgScreen::Update ? MenuIdle::Lingers : MenuIdle::Holds;
    }
    const RadioPages& radioPages() const { return radio_; }

private:
    void openPage(CfgHost& host, CfgScreen target);
    void leavePage(CfgHost& host);
    void onGroup(CfgHost& host, const ButtonEvent& ev);
    void follow(CfgHost& host, CfgMove move);
    void drawDevicePage(Framebuffer& fb, const CfgHost& host) const;

    int row_ = 0;                          // the CFG list
    CfgScreen page_ = CfgScreen::SysInfo;  // the open L3
    // A group page's cursor, kept apart from row_ so backing out of a group lands on
    // the row that opened it.
    int groupRow_ = 0;
    HardwarePages hardware_;
    LooksPages looks_;
    SettingsPages settings_;
    TipsPage tips_;
    FactoryResetPage factoryReset_;
    UpdatesPage updates_;
    RadioPages radio_;
};

}  // namespace mal
