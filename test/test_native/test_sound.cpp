// test_sound.cpp — device sound: the cue table, the SOUND mode's tiers, the VOLUME
// level, the moments that ask for a cue, and the two preferences surviving a reboot.
#include "test_gates.h"

namespace {

// The gates' speaker: every cue the engine hands the platform, in order, with the
// volume it was asked at.
struct RecordingSound : ISoundOut {
    struct Cue {
        Sound sound;
        int volumePercent;
    };
    std::vector<Cue> cues;
    void play(Sound s, int volumePercent) override { cues.push_back({s, volumePercent}); }
    int count(Sound s) const {
        int n = 0;
        for (const Cue& c : cues) n += c.sound == s;
        return n;
    }
};

} // namespace

// Every Sound has its row, at its own index, with notes to play and a name to log.
void test_sound_table_is_indexed() {
    CHECK(soundDef(Sound::None) == nullptr);
    CHECK(soundDef(Sound::Count) == nullptr);
    for (int i = 1; i < static_cast<int>(Sound::Count); ++i) {
        const Sound s = static_cast<Sound>(i);
        const SoundDef* d = soundDef(s);
        CHECK(d != nullptr);
        if (!d) continue;
        CHECK(d->id == s);
        CHECK(d->name && d->name[0]);
        CHECK(d->noteCount > 0);
        CHECK(soundDurationMs(s) > 0);
    }
    // The three care alerts are the Alert tier, and nothing else is.
    int alerts = 0;
    for (int i = 1; i < static_cast<int>(Sound::Count); ++i)
        alerts += soundTier(static_cast<Sound>(i)) == SoundTier::Alert;
    CHECK(alerts == 3);
    CHECK(soundTier(Sound::Lockout) == SoundTier::Alert);
    CHECK(soundTier(Sound::Failing) == SoundTier::Alert);
    CHECK(soundTier(Sound::PetLost) == SoundTier::Alert);
}

// ALL plays every tier, ALERTS ONLY keeps the care alerts alone, OFF plays nothing;
// whatever plays, plays at the VOLUME level's percent.
void test_sound_mode_filters_tiers() {
    CHECK(soundModeAllows(SoundMode::All, SoundTier::Ui));
    CHECK(soundModeAllows(SoundMode::All, SoundTier::Alert));
    CHECK(!soundModeAllows(SoundMode::AlertsOnly, SoundTier::Ui));
    CHECK(!soundModeAllows(SoundMode::AlertsOnly, SoundTier::Event));
    CHECK(soundModeAllows(SoundMode::AlertsOnly, SoundTier::Alert));
    CHECK(!soundModeAllows(SoundMode::Off, SoundTier::Alert));

    RecordingSound out;
    Game g{StartMode::Hatched};
    g.setSoundOut(&out);
    CHECK(g.soundMode() == SoundMode::Off);                  // silent out of the box
    CHECK(g.volume() == kVolumeDefault);
    g.onButton(press(Button::A));
    g.playSound(Sound::Lockout);
    CHECK(out.cues.empty());
    g.onButton(lift(Button::A));

    g.setSoundMode(SoundMode::All);
    g.onButton(press(Button::A));                            // a key click
    CHECK(out.cues.size() == 1 && out.cues[0].sound == Sound::KeyNext);
    CHECK(out.cues[0].volumePercent == volumePercent(kVolumeDefault));
    g.onButton(lift(Button::A));                             // a release is silent
    CHECK(out.cues.size() == 1);

    g.setVolume(kVolumeLevels - 1);
    g.playSound(Sound::Achievement);
    CHECK(out.cues.back().sound == Sound::Achievement && out.cues.back().volumePercent == 100);
    g.setVolume(kVolumeLevels + 3);                          // clamps to the top step
    CHECK(g.volume() == kVolumeLevels - 1);

    g.setSoundMode(SoundMode::AlertsOnly);
    out.cues.clear();
    g.playSound(Sound::KeyNext);
    g.playSound(Sound::BattleWin);
    g.playSound(Sound::Lockout);
    CHECK(out.cues.size() == 1 && out.cues[0].sound == Sound::Lockout);

    g.setSoundMode(SoundMode::Off);
    out.cues.clear();
    g.playSound(Sound::Lockout);
    g.onButton(press(Button::B));
    CHECK(out.cues.empty());
    // ...except the VOLUME sample, which is asked for by name.
    g.previewVolume(0);
    CHECK(out.cues.size() == 1 && out.cues[0].sound == Sound::Preview &&
          out.cues[0].volumePercent == volumePercent(0));
}

// The Lockout sounds when it opens and once more kLockoutReminderMs before it runs
// out — and in ALERTS ONLY those are the only two cues the whole crisis makes.
void test_sound_lockout_alert_and_reminder() {
    RecordingSound out;
    Game g{StartMode::Hatched};
    g.setSoundOut(&out);
    g.setSoundMode(SoundMode::AlertsOnly);
    g.model().setHunger(0);
    uint32_t t = kHeartbeatMs;
    g.tick(t);
    CHECK(g.lockoutActive());
    CHECK(out.count(Sound::Lockout) == 1);
    // Nothing more until the reminder is due...
    while (t + kHeartbeatMs < kHeartbeatMs + kLockoutDurationMs - kLockoutReminderMs)
        g.tick(t += kHeartbeatMs);
    CHECK(out.count(Sound::Lockout) == 1);
    // ...then exactly one, however long the crisis keeps running.
    while (g.lockoutActive()) g.tick(t += kHeartbeatMs);
    CHECK(out.count(Sound::Lockout) == 2);
    CHECK(out.cues.size() == 2);
}

// The FAILING window sounds as the pet reaches 5/5, again every kFailingAlertEveryMs of
// the window, and the loss at its end is the third alert.
void test_sound_failing_alert_repeats_then_pet_lost() {
    RecordingSound out;
    Game g{StartMode::Hatched};
    g.setSoundOut(&out);
    g.setSoundMode(SoundMode::AlertsOnly);
    g.model().setCareMistakes(kCareDying);
    uint32_t t = kHeartbeatMs;
    g.tick(t);
    CHECK(out.count(Sound::Failing) == 1);
    // Step a second at a time with Hunger topped up, so the window is the only clock
    // running and no Lockout joins in.
    const uint32_t step = 1000;
    while (t < kHeartbeatMs + kFailingAlertEveryMs - step) {
        g.model().setHunger(100);
        g.tick(t += step);
    }
    CHECK(out.count(Sound::Failing) == 1);
    g.tick(t += 2 * step);
    CHECK(out.count(Sound::Failing) == 2);
    while (g.nav() != Game::Nav::ModalCSF && t < 2 * kCsfDyingGraceMs) {
        g.model().setHunger(100);
        g.tick(t += step);
    }
    CHECK(g.nav() == Game::Nav::ModalCSF);
    CHECK(out.count(Sound::Failing) ==
          static_cast<int>((kCsfDyingGraceMs - 1) / kFailingAlertEveryMs) + 1);
    CHECK(out.count(Sound::PetLost) == 1);
    CHECK(out.cues.back().sound == Sound::PetLost);
}

// An evolution boundary firing off the clock calls the owner over with a jingle — an
// Event, so ALERTS ONLY drops it.
void test_sound_evolution_jingle() {
    RecordingSound out;
    Game g{StartMode::Hatched, "cryptoshell"};
    g.setSoundOut(&out);
    g.setSoundMode(SoundMode::All);
    g.tick(1000 + kEvolveProcessToScriptMs);
    CHECK(g.nav() == Game::Nav::ModalEvolve);
    CHECK(out.count(Sound::Evolve) == 1);

    RecordingSound quiet;
    Game h{StartMode::Hatched, "cryptoshell"};
    h.setSoundOut(&quiet);
    h.setSoundMode(SoundMode::AlertsOnly);
    h.tick(1000 + kEvolveProcessToScriptMs);
    CHECK(h.nav() == Game::Nav::ModalEvolve);
    CHECK(quiet.cues.empty());
}

// CFG > DEVICE > SOUND and VOLUME. Sound is OFF until chosen; each picker opens on the
// applied value, A walks it (VOLUME playing each level it lands on), B applies, C leaves
// it as it was. Both are
// device preferences: they survive a reboot, and the codec carries them both ways.
void test_cfg_sound_and_volume_persist() {
    MemSaveStore store;
    {
        RecordingSound out;
        Game g{StartMode::Hatched, "paypup", &store};
        g.setSoundOut(&out);

        enterCfgTarget(g, CfgScreen::Sound);
        CHECK(g.cfgScreen() == CfgScreen::Sound);
        g.onButton(press(Button::A));                        // OFF -> ALL
        g.onButton(press(Button::C));                        // ...not applied
        CHECK(g.soundMode() == SoundMode::Off);
        CHECK(g.cfgScreen() == CfgScreen::Device);           // the group, on SOUND's row
        g.onButton(press(Button::B));                        // reopen it
        CHECK(g.cfgScreen() == CfgScreen::Sound);
        g.onButton(press(Button::A));
        g.onButton(press(Button::A));                        // -> ALERTS ONLY
        g.onButton(press(Button::B));
        CHECK(g.soundMode() == SoundMode::AlertsOnly);
        CHECK(g.cfgScreen() == CfgScreen::Device);

        out.cues.clear();
        g.onButton(press(Button::A));                        // VOLUME is the next row
        g.onButton(press(Button::B));
        CHECK(g.cfgScreen() == CfgScreen::Volume);
        CHECK(out.cues.empty());                             // ALERTS ONLY: no key clicks
        g.onButton(press(Button::A));                        // the level after the default
        CHECK(out.cues.size() == 1 && out.cues[0].sound == Sound::Preview);   // heard anyway
        CHECK(out.cues[0].volumePercent == volumePercent(kVolumeDefault + 1));
        g.onButton(press(Button::B));
        CHECK(g.volume() == kVolumeDefault + 1);
        CHECK(g.cfgScreen() == CfgScreen::Device);

        Framebuffer fb(kActiveW, kActiveH);
        g.render(fb);                                        // the rows preview both
        CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));
        g.tick(kAutoDefocusMs + 1 + kSaveAutosaveMs + kHeartbeatMs);   // autosave
    }
    Game again{StartMode::Hatched, "paypup", &store};        // a reboot onto the save
    CHECK(again.soundMode() == SoundMode::AlertsOnly);
    CHECK(again.volume() == kVolumeDefault + 1);

    SaveData d;                                              // the codec, both ways
    d.soundMode = static_cast<uint8_t>(SoundMode::AlertsOnly);
    d.volume = 4;
    SaveData back;
    CHECK(deserializeSave(serializeSave(d), back));
    CHECK(back.soundMode == static_cast<uint8_t>(SoundMode::AlertsOnly) && back.volume == 4);
    // The pre-v68 reading, and a blob naming a mode or level this build has none for.
    CHECK(SaveData{}.soundMode == static_cast<uint8_t>(SoundMode::Off) &&
          SaveData{}.volume == kVolumeDefault);
    MemSaveStore odd;
    {
        SaveData bad;
        bad.soundMode = 9;
        bad.volume = 200;
        odd.save(serializeSave(bad));
    }
    Game clamped{StartMode::Hatched, "paypup", &odd};
    CHECK(clamped.soundMode() == SoundMode::Off);
    CHECK(clamped.volume() == kVolumeLevels - 1);
}
