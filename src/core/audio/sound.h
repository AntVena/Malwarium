// sound.h — the device's sound vocabulary: every cue the engine can ask for, which
// TIER it belongs to, and the notes it is made of.
//
// The engine never touches a codec. It names a Sound (Game::playSound), the CFG >
// DEVICE > SOUND mode decides whether that tier is heard at all, and the platform's
// ISoundOut (platform/platform.h) turns the notes below into whatever its hardware
// makes: square waves over I2S on the board (platform/esp32/audio_esp32.h), WebAudio
// oscillators in the browser, a log line on the desktop preview, a recorded list in
// the native gates. The notes live here rather than in any one driver so every target
// plays the same tune.
#pragma once

#include <cstdint>

namespace mal {

// What a cue is FOR, which is what the SOUND mode filters on. Ordered by how much it
// matters that the cue is heard: ALERTS ONLY keeps the last tier and drops the rest.
enum class SoundTier : uint8_t {
    Ui,      // a button press acknowledged — the key click
    Event,   // something happened worth a jingle: a hatch, a win, an achievement
    Alert,   // the pet needs its owner now: the Lockout, the FAILING window, a loss
};

// The CFG > DEVICE > SOUND setting. The values are the save's wire numbers (v68). A
// device is silent until the operator picks ALL or ALERTS ONLY.
enum class SoundMode : uint8_t {
    All = 0,         // every tier
    AlertsOnly = 1,  // the care alerts and nothing else
    Off = 2,         // silent
};
constexpr int kSoundModeCount = 3;

// Every cue. Append only: the host log and the gates name cues by soundName, but a
// driver may index tables by value.
enum class Sound : uint8_t {
    None = 0,
    // Ui — one per button, plus the chord.
    KeyNext,
    KeyAccept,
    KeyBack,
    KeyChord,
    // Event
    Hatch,
    Evolve,
    LevelUp,
    Achievement,
    BattleWin,
    BattleLose,
    GameWin,
    GameLose,
    // Alert
    Lockout,          // hunger hit zero; the 30s crisis opened (and its reminder)
    Failing,          // the pet entered 5/5 errors; the FAILING window is counting
    PetLost,          // the window ran out — Critical System Failure
    // The VOLUME picker's sample. Ui-tier, but played by Game::previewVolume whatever
    // the SOUND mode, since asking to hear the level IS the request.
    Preview,
    Count,
};

// One step of a cue: a square-wave tone at `hz` for `ms`, or silence when hz is 0.
struct SoundNote {
    uint16_t hz;
    uint16_t ms;
};

struct SoundDef {
    Sound id;
    SoundTier tier;
    const char* name;          // stable, upper-case: the host log and the gates use it
    const SoundNote* notes;
    int noteCount;
};

// The row for `s`, or nullptr for None / an out-of-range value.
const SoundDef* soundDef(Sound s);

inline SoundTier soundTier(Sound s) {
    const SoundDef* d = soundDef(s);
    return d ? d->tier : SoundTier::Ui;
}

inline const char* soundName(Sound s) {
    const SoundDef* d = soundDef(s);
    return d ? d->name : "NONE";
}

// Total length of a cue in ms — what a driver waits out before powering its amp down.
int soundDurationMs(Sound s);

// Whether `mode` lets a cue of `tier` through.
constexpr bool soundModeAllows(SoundMode mode, SoundTier tier) {
    return mode == SoundMode::All ||
           (mode == SoundMode::AlertsOnly && tier == SoundTier::Alert);
}

const char* soundModeName(SoundMode m);

} // namespace mal
