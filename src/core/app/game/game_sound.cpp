// game_sound.cpp — the SOUND/VOLUME preferences and Game::playSound.
#include "core/app/game.h"

#include "tunables.h"

namespace mal {

void Game::setSoundMode(SoundMode m) {
    if (static_cast<int>(m) < 0 || static_cast<int>(m) >= kSoundModeCount) m = SoundMode::Off;
    if (m == soundMode_) return;
    soundMode_ = m;
    dirty_ = true;
    markSaveDirty();
}

void Game::setVolume(int level) {
    if (level < 0) level = 0;
    if (level >= kVolumeLevels) level = kVolumeLevels - 1;
    if (level == volume_) return;
    volume_ = level;
    dirty_ = true;
    markSaveDirty();
}

void Game::setExploreSound(bool on) {
    if (on == exploreSound_) return;
    exploreSound_ = on;
    dirty_ = true;
    markSaveDirty();
}

void Game::playSound(Sound s) {
    if (!soundOut_ || s == Sound::None) return;
    if (!soundModeAllows(soundMode_, soundTier(s))) return;
    // Alerts are about the pet, not the walk, so muting the walk never silences them.
    if (!exploreSound_ && soundTier(s) == SoundTier::Event && exploreFightLive()) return;
    soundOut_->play(s, volumePercent(volume_));
}

void Game::previewVolume(int level) {
    if (soundOut_) soundOut_->play(Sound::Preview, volumePercent(level));
}

} // namespace mal
