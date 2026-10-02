#include "core/audio/sound.h"

namespace mal {

namespace {

// Pitches, named so a cue reads as a tune rather than a column of numbers.
constexpr uint16_t C4 = 262, D4 = 294, E4 = 330, F4 = 349, G4 = 392, A4 = 440;
constexpr uint16_t C5 = 523, E5 = 659, G5 = 784;
constexpr uint16_t C6 = 1047, E6 = 1319, G6 = 1568;
constexpr uint16_t A6 = 1760, C7 = 2093, E7 = 2637;
constexpr uint16_t REST = 0;

// The key clicks are a single blip each, short enough that holding A to repeat a list
// step never stacks them into a tone. Each button has its own pitch, so a press the
// eye missed still says which one landed.
constexpr SoundNote kKeyNext[]   = {{A6, 12}};
constexpr SoundNote kKeyAccept[] = {{C7, 18}};
constexpr SoundNote kKeyBack[]   = {{E6, 18}};
constexpr SoundNote kKeyChord[]  = {{G6, 14}, {C7, 14}};

constexpr SoundNote kHatch[]       = {{C5, 80}, {E5, 80}, {G5, 80}, {C6, 160}};
constexpr SoundNote kEvolve[]      = {{G4, 60}, {C5, 60}, {E5, 60}, {G5, 60},
                                      {C6, 60}, {E6, 220}};
constexpr SoundNote kLevelUp[]     = {{E5, 70}, {G5, 70}, {E6, 140}};
constexpr SoundNote kAchievement[] = {{G5, 60}, {C6, 60}, {E6, 60}, {G6, 180}};
constexpr SoundNote kBattleWin[]   = {{C5, 90}, {G5, 90}, {C6, 200}};
constexpr SoundNote kBattleLose[]  = {{G4, 120}, {E4, 120}, {C4, 260}};
constexpr SoundNote kGameWin[]     = {{E5, 70}, {G5, 70}, {C6, 180}};
constexpr SoundNote kGameLose[]    = {{E4, 140}, {C4, 240}};

// The alerts sit high, where a small speaker is loudest and a room is quietest, and
// they are rhythmic rather than melodic so they cannot be mistaken for a jingle.
constexpr SoundNote kLockout[] = {{E7, 90}, {REST, 60}, {E7, 90}, {REST, 60},
                                  {E7, 90}};
constexpr SoundNote kFailing[] = {{A6, 150}, {E6, 150}, {A6, 150}, {E6, 150}};
constexpr SoundNote kPetLost[] = {{C5, 200}, {A4, 200}, {F4, 200}, {D4, 500}};

constexpr SoundNote kPreview[] = {{C6, 90}, {G6, 120}};

#define MAL_SOUND_ROW(id, tier, notes) \
    {Sound::id, SoundTier::tier, #id, notes, static_cast<int>(sizeof(notes) / sizeof(notes[0]))}

// Indexed by Sound's value less one (None has no row); test_sound_table_is_indexed
// holds the order to the enum.
constexpr SoundDef kSounds[] = {
    MAL_SOUND_ROW(KeyNext, Ui, kKeyNext),
    MAL_SOUND_ROW(KeyAccept, Ui, kKeyAccept),
    MAL_SOUND_ROW(KeyBack, Ui, kKeyBack),
    MAL_SOUND_ROW(KeyChord, Ui, kKeyChord),
    MAL_SOUND_ROW(Hatch, Event, kHatch),
    MAL_SOUND_ROW(Evolve, Event, kEvolve),
    MAL_SOUND_ROW(LevelUp, Event, kLevelUp),
    MAL_SOUND_ROW(Achievement, Event, kAchievement),
    MAL_SOUND_ROW(BattleWin, Event, kBattleWin),
    MAL_SOUND_ROW(BattleLose, Event, kBattleLose),
    MAL_SOUND_ROW(GameWin, Event, kGameWin),
    MAL_SOUND_ROW(GameLose, Event, kGameLose),
    MAL_SOUND_ROW(Lockout, Alert, kLockout),
    MAL_SOUND_ROW(Failing, Alert, kFailing),
    MAL_SOUND_ROW(PetLost, Alert, kPetLost),
    MAL_SOUND_ROW(Preview, Ui, kPreview),
};

#undef MAL_SOUND_ROW

constexpr int kSoundRows = static_cast<int>(sizeof(kSounds) / sizeof(kSounds[0]));
static_assert(kSoundRows == static_cast<int>(Sound::Count) - 1,
              "every Sound but None needs a row in kSounds");

} // namespace

const SoundDef* soundDef(Sound s) {
    const int i = static_cast<int>(s) - 1;
    if (i < 0 || i >= kSoundRows) return nullptr;
    return &kSounds[i];
}

int soundDurationMs(Sound s) {
    const SoundDef* d = soundDef(s);
    if (!d) return 0;
    int ms = 0;
    for (int i = 0; i < d->noteCount; ++i) ms += d->notes[i].ms;
    return ms;
}

const char* soundModeName(SoundMode m) {
    switch (m) {
        case SoundMode::All: return "ALL";
        case SoundMode::AlertsOnly: return "ALERTS ONLY";
        case SoundMode::Off: return "OFF";
    }
    return "?";
}

} // namespace mal
