// game_sound.cpp — the engine's half of device sound: the SOUND/VOLUME preferences
// and the one door every cue goes through. The vocabulary is core/audio/sound.h; the
// speaker is whatever ISoundOut the platform wired (platform/platform.h). The CFG
// screens that set these live in game_config.cpp, and each cue is asked for at the
// moment it marks — a press in onButton, a hatch in completeHatch, the care alerts in
// tickLifecycle.
#include "core/app/game.h"

#include "tunables.h"

namespace mal {

void Game::setSoundMode(SoundMode m) {
    if (static_cast<int>(m) < 0 || static_cast<int>(m) >= kSoundModeCount) m = SoundMode::Off;
    if (m == soundMode_) return;
    soundMode_ = m;
    dirty_ = true;
    markSaveDirty();   // a persisted CFG pref (save v68), beside brightness
}

void Game::setVolume(int level) {
    if (level < 0) level = 0;
    if (level >= kVolumeLevels) level = kVolumeLevels - 1;
    if (level == volume_) return;
    volume_ = level;
    dirty_ = true;
    markSaveDirty();   // a persisted CFG pref (save v68)
}

void Game::playSound(Sound s) {
    if (!soundOut_ || s == Sound::None) return;
    if (!soundModeAllows(soundMode_, soundTier(s))) return;
    soundOut_->play(s, volumePercent(volume_));
}

void Game::previewVolume(int level) {
    if (soundOut_) soundOut_->play(Sound::Preview, volumePercent(level));
}

} // namespace mal
