#include "core/app/settings.h"

#include <cstdio>

#include "core/render/palette.h"   // palThemeByName, kPalThemeNames

namespace mal {

namespace {

int clampLevel(int level, int levels) {
    if (level < 0) return 0;
    return level >= levels ? levels - 1 : level;
}

template <typename T>
bool change(T& field, T value) {
    if (field == value) return false;
    field = value;
    return true;
}

}  // namespace

bool Settings::setBrightness(int level) {
    return change(brightness, clampLevel(level, kBrightnessLevels));
}

bool Settings::setUiMode(UiMode m) { return change(uiMode, m); }

bool Settings::setCareAlerts(bool on) { return change(careAlerts, on); }

bool Settings::setSoundMode(SoundMode m) {
    if (static_cast<int>(m) < 0 || static_cast<int>(m) >= kSoundModeCount) m = SoundMode::Off;
    return change(soundMode, m);
}

bool Settings::setVolume(int level) { return change(volume, clampLevel(level, kVolumeLevels)); }

bool Settings::setExploreSound(bool on) { return change(exploreSound, on); }

SaveSettings Settings::saved() const {
    SaveSettings s;
    s.brightness = brightness;
    s.uiMode = static_cast<uint8_t>(uiMode);
    s.careAlerts = careAlerts ? 1 : 0;
    s.soundMode = static_cast<uint8_t>(soundMode);
    s.volume = static_cast<uint8_t>(volume);
    s.exploreSound = exploreSound ? 1 : 0;
    std::snprintf(s.theme, sizeof(s.theme), "%s", kPalThemeNames[theme]);
    return s;
}

Settings Settings::loaded(const SaveSettings& s) {
    Settings v;
    v.brightness = clampLevel(s.brightness, kBrightnessLevels);
    v.uiMode = s.uiMode <= static_cast<uint8_t>(UiMode::TextOnly) ? static_cast<UiMode>(s.uiMode)
                                                                  : UiMode::IconsLabel;
    v.careAlerts = s.careAlerts != 0;
    v.soundMode = s.soundMode < kSoundModeCount ? static_cast<SoundMode>(s.soundMode)
                                                : SoundMode::Off;
    v.volume = s.volume < kVolumeLevels ? s.volume : kVolumeLevels - 1;
    v.exploreSound = s.exploreSound != 0;
    // An empty name (every pre-v64 blob) and a name this build ships no set for both
    // resolve to base.
    v.theme = palThemeByName(s.theme);
    return v;
}

}  // namespace mal
