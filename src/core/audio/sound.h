// sound.h — the sound vocabulary: every cue, its tier, and its notes.
#pragma once

#include <cstdint>

namespace mal {

enum class SoundTier : uint8_t {
    Ui,
    Event,
    Alert,
};

// Values are save wire numbers (v68).
enum class SoundMode : uint8_t {
    All = 0,
    AlertsOnly = 1,
    Off = 2,
};
constexpr int kSoundModeCount = 3;

enum class Sound : uint8_t {
    None = 0,
    KeyNext,
    KeyAccept,
    KeyBack,
    KeyChord,
    Hatch,
    Evolve,
    LevelUp,
    Achievement,
    BattleWin,
    BattleLose,
    GameWin,
    GameLose,
    Lockout,
    Failing,
    PetLost,
    Preview,
    Count,
};

// hz == 0 is a rest.
struct SoundNote {
    uint16_t hz;
    uint16_t ms;
};

struct SoundDef {
    Sound id;
    SoundTier tier;
    const char* name;
    const SoundNote* notes;
    int noteCount;
};

const SoundDef* soundDef(Sound s);

inline SoundTier soundTier(Sound s) {
    const SoundDef* d = soundDef(s);
    return d ? d->tier : SoundTier::Ui;
}

inline const char* soundName(Sound s) {
    const SoundDef* d = soundDef(s);
    return d ? d->name : "NONE";
}

int soundDurationMs(Sound s);

constexpr bool soundModeAllows(SoundMode mode, SoundTier tier) {
    return mode == SoundMode::All ||
           (mode == SoundMode::AlertsOnly && tier == SoundTier::Alert);
}

const char* soundModeName(SoundMode m);

} // namespace mal
