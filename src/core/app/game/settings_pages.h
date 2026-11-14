// settings_pages.h: CFG's pages over the device Settings record: UI MODE, BRIGHTNESS,
// SOUND and VOLUME, and the CARE ALERTS switch the DEVICE group flips in place.
//
// The pages hold the focused value, seeded from the record as each opens. The record is
// the game's, read by const reference and changed through SettingsHost, so a setting
// lands once (applied, persisted) whichever page changes it.
//
// This header names no core/ui type beyond ui_state.h, so game.h can hold the pages
// (inside CfgMenu) by value.
#pragma once

#include "core/app/game/cfg_page.h"
#include "core/audio/sound.h"           // SoundMode

namespace mal {

class Framebuffer;
struct Settings;

class SettingsHost {
public:
    virtual const Settings& settingsRecord() const = 0;
    virtual void setUiMode(UiMode m) = 0;
    virtual void setBrightness(int level) = 0;
    virtual void setSoundMode(SoundMode m) = 0;
    virtual void previewVolume(int level) = 0;   // heard while the focus moves
    virtual void setVolume(int level) = 0;
    virtual void setCareAlerts(bool on) = 0;

protected:
    ~SettingsHost() = default;
};

class SettingsPages {
public:
    // Seeds the focus at the applied value.
    void open(const SettingsHost& host, CfgScreen page);

    CfgMove onUiMode(SettingsHost& host, const ButtonEvent& ev);
    CfgMove onBrightness(SettingsHost& host, const ButtonEvent& ev);
    CfgMove onSound(SettingsHost& host, const ButtonEvent& ev);
    CfgMove onVolume(SettingsHost& host, const ButtonEvent& ev);
    // CARE ALERTS is a switch, not a page: the DEVICE group flips it where it stands.
    void flipCareAlerts(SettingsHost& host);

    void drawUiMode(Framebuffer& fb, const SettingsHost& host) const;
    void drawBrightness(Framebuffer& fb, const SettingsHost& host) const;
    void drawSound(Framebuffer& fb, const SettingsHost& host) const;
    void drawVolume(Framebuffer& fb, const SettingsHost& host) const;

private:
    int pick_ = 0;   // the open picker's value; never open together, so one field
};

}  // namespace mal
