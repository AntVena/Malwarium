// settings.h: the Settings record (CONTRIBUTING, "Where state lives"). The hardware's
// preferences: what the device looks and sounds like, which belongs to no pet and no
// operator. Kept through a pet's death and through both factory resets, which reset
// progress rather than preferences. Game holds it as Game::settings_ and reads it
// through Game::settingsRecord().
//
// Settings::saved and Settings::loaded are its only mapping to the save.
//
// It sits in app/ rather than model/ because UiMode is a ui-layer type (ui_state.h).
#pragma once

#include "tunables.h"   // kBrightnessDefault, kVolumeDefault
#include "core/audio/sound.h"
#include "core/model/save.h"
#include "core/ui/ui_state.h"

namespace mal {

struct Settings {
    int brightness = kBrightnessDefault;   // backlight index, 0..kBrightnessLevels-1
    UiMode uiMode = UiMode::IconsLabel;    // how the carousel labels its slots
    bool careAlerts = true;                // the carousel's "!" needs-attention marks
    SoundMode soundMode = SoundMode::Off;
    int volume = kVolumeDefault;           // 0..kVolumeLevels-1
    bool exploreSound = true;              // the walk's A+C SOUND row
    // The PAL_CORE theme index (palette.h). Saved by name, so a reordered palette
    // still loads the set the operator chose. Game::setThemePick is its only writer,
    // because a set must be unlocked to be applied.
    int theme = 0;

    // Each setter clamps out-of-range input and answers whether the value changed, so
    // the caller repaints and saves only on a real change.
    bool setBrightness(int level);
    bool setUiMode(UiMode m);
    bool setCareAlerts(bool on);
    bool setSoundMode(SoundMode m);
    bool setVolume(int level);
    bool setExploreSound(bool on);

    SaveSettings saved() const;
    // A value this build has no meaning for (a corrupt level, a mode or a theme name
    // from a newer build) reads as the nearest one it has, or the default.
    static Settings loaded(const SaveSettings& s);
};

}  // namespace mal
