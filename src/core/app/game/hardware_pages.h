// hardware_pages.h: CFG's pages over the device itself: SYSTEM INFO, TRAVEL MODE and
// FORMAT SD.
//
// The pages hold the focused NO/YES of the two confirms. The card, the battery and the
// sleep the device tier lands are the game's, reached through HardwareHost. SYSTEM INFO
// is also the device's summary page, so its host carries the few reads it shows from the
// rest of the device (the rank, the title, what the radio is doing).
//
// This header names no core/ui type beyond ui_state.h, so game.h can hold the pages
// (inside CfgMenu) by value.
#pragma once

#include <cstdint>

#include "core/app/game/cfg_page.h"
#include "core/device/power_status.h"
#include "core/device/radio_status.h"
#include "core/device/sd_status.h"

namespace mal {

class Framebuffer;
struct Operator;
struct Settings;

class HardwareHost {
public:
    virtual const SdStatus& sdStatus() const = 0;
    virtual SdFormatState sdFormatState() const = 0;
    virtual SdCardUse sdCardUse() const = 0;
    virtual void requestSdFormat() = 0;
    virtual void clearSdFormatResult() = 0;
    virtual void requestSdRecheck() = 0;
    virtual const PowerStatus& powerStatus() const = 0;
    virtual uint32_t uptimeMs() const = 0;
    virtual bool captureEnabled() const = 0;
    virtual bool travelSleepRequested() const = 0;
    virtual void requestTravelSleep() = 0;
    // SYSTEM INFO's B arms the hold that reveals the Factory Reset; the page shows how
    // far along it is.
    virtual void armHoldB() = 0;
    virtual int32_t heldBMs() const = 0;
    virtual void markDirty() = 0;
    // What SYSTEM INFO summarises from the rest of the device.
    virtual const Operator& operatorRecord() const = 0;
    virtual const Settings& settingsRecord() const = 0;
    virtual RadioOwner radioOwner() const = 0;
    virtual const char* equippedTitleName() const = 0;

protected:
    ~HardwareHost() = default;
};

class HardwarePages {
public:
    // Both confirms open on NO, and FORMAT SD spends a result left on screen.
    void open(HardwareHost& host, CfgScreen page);

    CfgMove onSysInfo(HardwareHost& host, const ButtonEvent& ev);
    CfgMove onTravel(HardwareHost& host, const ButtonEvent& ev);
    CfgMove onFormatSd(HardwareHost& host, const ButtonEvent& ev);

    void drawSysInfo(Framebuffer& fb, const HardwareHost& host) const;
    void drawTravel(Framebuffer& fb, const HardwareHost& host) const;
    void drawFormatSd(Framebuffer& fb, const HardwareHost& host) const;

private:
    int pick_ = 0;   // the open confirm's NO/YES; never open together, so one field
};

}  // namespace mal
