// game_sound.cpp: the SOUND/VOLUME preferences and Game::playSound.
#include "core/app/game.h"

#include "tunables.h"

namespace mal {

void Game::setSoundMode(SoundMode m) {
    if (settings_.setSoundMode(m)) settingChanged();
}

void Game::setVolume(int level) {
    if (settings_.setVolume(level)) settingChanged();
}

void Game::setExploreSound(bool on) {
    if (settings_.setExploreSound(on)) settingChanged();
}

void Game::playSound(Sound s) {
    if (!soundOut_ || s == Sound::None) return;
    if (!soundModeAllows(settings_.soundMode, soundTier(s))) return;
    // Alerts are about the pet, not the walk, so muting the walk never silences them.
    if (!settings_.exploreSound && soundTier(s) == SoundTier::Event && exploreFightLive()) return;
    soundOut_->play(s, volumePercent(settings_.volume));
}

void Game::previewVolume(int level) {
    if (soundOut_) soundOut_->play(Sound::Preview, volumePercent(level));
}

} // namespace mal
