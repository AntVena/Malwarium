#include "core/app/game/settings_pages.h"

#include "tunables.h"
#include "core/app/settings.h"
#include "core/ui/cfg_screen.h"
#include "core/ui/layout.h"

namespace mal {

void SettingsPages::open(const SettingsHost& host, CfgScreen page) {
    const Settings& s = host.settingsRecord();
    switch (page) {
        case CfgScreen::UiMode: pick_ = static_cast<int>(s.uiMode); break;
        case CfgScreen::Brightness: pick_ = s.brightness; break;
        case CfgScreen::Sound: pick_ = static_cast<int>(s.soundMode); break;
        case CfgScreen::Volume: pick_ = s.volume; break;
        default: break;
    }
}

CfgMove SettingsPages::onUiMode(SettingsHost& host, const ButtonEvent& ev) {
    return pickerPress(ev, [&] { pick_ = stepRow(pick_, 3, +1); }, [&] {
        host.setUiMode(static_cast<UiMode>(pick_));
        return CfgMove::leave();
    });
}

CfgMove SettingsPages::onBrightness(SettingsHost& host, const ButtonEvent& ev) {
    return pickerPress(ev, [&] { pick_ = stepRow(pick_, kBrightnessLevels, +1); }, [&] {
        host.setBrightness(pick_);
        return CfgMove::leave();
    });
}

CfgMove SettingsPages::onSound(SettingsHost& host, const ButtonEvent& ev) {
    return pickerPress(ev, [&] { pick_ = stepRow(pick_, kSoundModeCount, +1); }, [&] {
        host.setSoundMode(static_cast<SoundMode>(pick_));
        return CfgMove::leave();
    });
}

CfgMove SettingsPages::onVolume(SettingsHost& host, const ButtonEvent& ev) {
    return pickerPress(ev,
                       [&] {
                           pick_ = stepRow(pick_, kVolumeLevels, +1);
                           host.previewVolume(pick_);
                       },
                       [&] {
                           host.setVolume(pick_);
                           return CfgMove::leave();
                       });
}

void SettingsPages::flipCareAlerts(SettingsHost& host) {
    host.setCareAlerts(!host.settingsRecord().careAlerts);
}

void SettingsPages::drawUiMode(Framebuffer& fb, const SettingsHost& host) const {
    drawUiModeToggle(fb, pick_, host.settingsRecord().uiMode);
}

void SettingsPages::drawBrightness(Framebuffer& fb, const SettingsHost& host) const {
    mal::drawBrightness(fb, pick_, host.settingsRecord().brightness);
}

void SettingsPages::drawSound(Framebuffer& fb, const SettingsHost& host) const {
    drawSoundMode(fb, pick_, host.settingsRecord().soundMode);
}

void SettingsPages::drawVolume(Framebuffer& fb, const SettingsHost& host) const {
    mal::drawVolume(fb, pick_, host.settingsRecord().volume);
}

}  // namespace mal
