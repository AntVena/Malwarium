#include "core/audio/sound.h"

namespace mal {

namespace {

constexpr uint16_t A3 = 220;
constexpr uint16_t C4 = 262, D4 = 294, E4 = 330, F4 = 349, G4 = 392, A4 = 440;
constexpr uint16_t C5 = 523, D5 = 587, E5 = 659, F5 = 698, G5 = 784, A5 = 880;
constexpr uint16_t C6 = 1047, D6 = 1175, E6 = 1319, G6 = 1568;
constexpr uint16_t A6 = 1760, C7 = 2093, E7 = 2637;
constexpr uint16_t REST = 0;

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

constexpr SoundNote kLockout[] = {{E7, 90}, {REST, 60}, {E7, 90}, {REST, 60},
                                  {E7, 90}};
constexpr SoundNote kFailing[] = {{A6, 150}, {E6, 150}, {A6, 150}, {E6, 150}};
constexpr SoundNote kPetLost[] = {{C5, 200}, {A4, 200}, {F4, 200}, {D4, 500}};

constexpr SoundNote kPreview[] = {{C6, 90}, {G6, 120}};

// A combat turn lands every kHeartbeatMs at the fastest frenzy pace, so the per-turn
// cues stay well under that or the next turn's cue cuts them off mid-note.
constexpr SoundNote kCombatStart[] = {{E5, 40}, {REST, 20}, {E5, 40}, {A5, 110}};
constexpr SoundNote kBossStart[]   = {{A3, 90}, {REST, 30}, {A3, 90}, {REST, 30},
                                      {E4, 90}, {A4, 200}};
constexpr SoundNote kHitDealt[]    = {{G6, 25}, {C6, 40}};
constexpr SoundNote kHitTaken[]    = {{D4, 30}, {A3, 60}};
constexpr SoundNote kBlocked[]     = {{E7, 15}, {REST, 15}, {E7, 15}};
constexpr SoundNote kStunned[]     = {{A5, 30}, {F5, 30}, {A5, 30}, {F5, 30}};
constexpr SoundNote kExploitFire[] = {{C6, 25}, {E6, 25}, {G6, 25}, {C7, 70}};
constexpr SoundNote kKnockout[]    = {{G5, 40}, {D5, 40}, {A4, 40}, {D4, 120}};
constexpr SoundNote kFled[]        = {{G5, 35}, {E5, 35}, {C5, 35}, {G4, 60}};

constexpr SoundNote kArcadeStart[] = {{G5, 40}, {REST, 20}, {G5, 40}, {D6, 100}};
constexpr SoundNote kPoint[]       = {{C6, 20}, {G6, 35}};
constexpr SoundNote kMiss[]        = {{E5, 30}, {C5, 50}};
constexpr SoundNote kProbe[]       = {{D6, 30}, {REST, 20}, {D6, 30}};
constexpr SoundNote kCrash[]       = {{C5, 40}, {G4, 40}, {D4, 40}, {A3, 140}};
constexpr SoundNote kClear[]       = {{C6, 50}, {E6, 50}, {G6, 50}, {C7, 50},
                                      {G6, 50}, {C7, 160}};
constexpr SoundNote kNewBest[]     = {{G5, 60}, {C6, 60}, {E6, 60}, {G6, 60},
                                      {REST, 40}, {E6, 60}, {G6, 220}};

#define MAL_SOUND_ROW(id, tier, notes) \
    {Sound::id, SoundTier::tier, #id, notes, static_cast<int>(sizeof(notes) / sizeof(notes[0]))}

// Must follow Sound's order: soundDef indexes by value - 1.
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
    MAL_SOUND_ROW(CombatStart, Event, kCombatStart),
    MAL_SOUND_ROW(BossStart, Event, kBossStart),
    MAL_SOUND_ROW(HitDealt, Event, kHitDealt),
    MAL_SOUND_ROW(HitTaken, Event, kHitTaken),
    MAL_SOUND_ROW(Blocked, Event, kBlocked),
    MAL_SOUND_ROW(Stunned, Event, kStunned),
    MAL_SOUND_ROW(ExploitFire, Event, kExploitFire),
    MAL_SOUND_ROW(Knockout, Event, kKnockout),
    MAL_SOUND_ROW(Fled, Event, kFled),
    MAL_SOUND_ROW(ArcadeStart, Event, kArcadeStart),
    MAL_SOUND_ROW(Point, Event, kPoint),
    MAL_SOUND_ROW(Miss, Event, kMiss),
    MAL_SOUND_ROW(Probe, Event, kProbe),
    MAL_SOUND_ROW(Crash, Event, kCrash),
    MAL_SOUND_ROW(Clear, Event, kClear),
    MAL_SOUND_ROW(NewBest, Event, kNewBest),
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
