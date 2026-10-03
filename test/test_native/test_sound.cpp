#include "test_gates.h"

namespace {

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

}

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
    int alerts = 0;
    for (int i = 1; i < static_cast<int>(Sound::Count); ++i)
        alerts += soundTier(static_cast<Sound>(i)) == SoundTier::Alert;
    CHECK(alerts == 3);
    CHECK(soundTier(Sound::Lockout) == SoundTier::Alert);
    CHECK(soundTier(Sound::Failing) == SoundTier::Alert);
    CHECK(soundTier(Sound::PetLost) == SoundTier::Alert);
}

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
    CHECK(g.soundMode() == SoundMode::Off);
    CHECK(g.volume() == kVolumeDefault);
    g.onButton(press(Button::A));
    g.playSound(Sound::Lockout);
    CHECK(out.cues.empty());
    g.onButton(lift(Button::A));

    g.setSoundMode(SoundMode::All);
    g.onButton(press(Button::A));
    CHECK(out.cues.size() == 1 && out.cues[0].sound == Sound::KeyNext);
    CHECK(out.cues[0].volumePercent == volumePercent(kVolumeDefault));
    g.onButton(lift(Button::A));
    CHECK(out.cues.size() == 1);

    g.setVolume(kVolumeLevels - 1);
    g.playSound(Sound::Achievement);
    CHECK(out.cues.back().sound == Sound::Achievement && out.cues.back().volumePercent == 100);
    g.setVolume(kVolumeLevels + 3);
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
    g.previewVolume(0);
    CHECK(out.cues.size() == 1 && out.cues[0].sound == Sound::Preview &&
          out.cues[0].volumePercent == volumePercent(0));
}

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
    while (t + kHeartbeatMs < kHeartbeatMs + kLockoutDurationMs - kLockoutReminderMs)
        g.tick(t += kHeartbeatMs);
    CHECK(out.count(Sound::Lockout) == 1);
    while (g.lockoutActive()) g.tick(t += kHeartbeatMs);
    CHECK(out.count(Sound::Lockout) == 2);
    CHECK(out.cues.size() == 2);
}

void test_sound_failing_alert_repeats_then_pet_lost() {
    RecordingSound out;
    Game g{StartMode::Hatched};
    g.setSoundOut(&out);
    g.setSoundMode(SoundMode::AlertsOnly);
    g.model().setCareMistakes(kCareDying);
    uint32_t t = kHeartbeatMs;
    g.tick(t);
    CHECK(out.count(Sound::Failing) == 1);
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

void test_cfg_sound_and_volume_persist() {
    MemSaveStore store;
    {
        RecordingSound out;
        Game g{StartMode::Hatched, "paypup", &store};
        g.setSoundOut(&out);

        enterCfgTarget(g, CfgScreen::Sound);
        CHECK(g.cfgScreen() == CfgScreen::Sound);
        g.onButton(press(Button::A));
        g.onButton(press(Button::C));
        CHECK(g.soundMode() == SoundMode::Off);
        CHECK(g.cfgScreen() == CfgScreen::Device);
        g.onButton(press(Button::B));
        CHECK(g.cfgScreen() == CfgScreen::Sound);
        g.onButton(press(Button::A));
        g.onButton(press(Button::A));
        g.onButton(press(Button::B));
        CHECK(g.soundMode() == SoundMode::AlertsOnly);
        CHECK(g.cfgScreen() == CfgScreen::Device);

        out.cues.clear();
        g.onButton(press(Button::A));
        g.onButton(press(Button::B));
        CHECK(g.cfgScreen() == CfgScreen::Volume);
        CHECK(out.cues.empty());
        g.onButton(press(Button::A));
        CHECK(out.cues.size() == 1 && out.cues[0].sound == Sound::Preview);
        CHECK(out.cues[0].volumePercent == volumePercent(kVolumeDefault + 1));
        g.onButton(press(Button::B));
        CHECK(g.volume() == kVolumeDefault + 1);
        CHECK(g.cfgScreen() == CfgScreen::Device);

        Framebuffer fb(kActiveW, kActiveH);
        g.render(fb);
        CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));
        g.tick(kAutoDefocusMs + 1 + kSaveAutosaveMs + kHeartbeatMs);
    }
    Game again{StartMode::Hatched, "paypup", &store};
    CHECK(again.soundMode() == SoundMode::AlertsOnly);
    CHECK(again.volume() == kVolumeDefault + 1);

    SaveData d;
    d.soundMode = static_cast<uint8_t>(SoundMode::AlertsOnly);
    d.volume = 4;
    SaveData back;
    CHECK(deserializeSave(serializeSave(d), back));
    CHECK(back.soundMode == static_cast<uint8_t>(SoundMode::AlertsOnly) && back.volume == 4);
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

// Every fight opens on a cue, each swing sounds by who it hurt, and the turn that ends
// it is the knockout alone — the jingle waits for the result to be dismissed.
void test_sound_combat_cues() {
    RecordingSound out;
    Game g{StartMode::Hatched, "paypup"};
    g.setSoundOut(&out);
    g.setSoundMode(SoundMode::All);
    enterSimBattle(g);
    CHECK(g.nav() == Game::Nav::Combat);
    CHECK(out.count(Sound::CombatStart) == 1);

    int swings = 0;
    for (int i = 0; i < 400 && g.combat().outcome() == Combat::Outcome::Ongoing; ++i) {
        g.onButton(press(Button::A));
        g.onButton(lift(Button::A));
        if (g.combat().outcome() == Combat::Outcome::Ongoing && g.combat().lastWasStrike())
            ++swings;
    }
    CHECK(g.combat().outcome() != Combat::Outcome::Ongoing);
    CHECK(swings > 0);
    CHECK(out.count(Sound::HitDealt) + out.count(Sound::HitTaken) +
          out.count(Sound::Blocked) >= swings);
    CHECK(out.count(Sound::Knockout) == 1);
    CHECK(out.cues.back().sound == Sound::Knockout);
    CHECK(out.count(Sound::BattleWin) + out.count(Sound::BattleLose) == 0);
    g.onButton(press(Button::B));
    CHECK(out.count(Sound::BattleWin) + out.count(Sound::BattleLose) == 1);

    // A practice fight's C quits outright, and that sounds as a retreat, not a KO.
    out.cues.clear();
    g.debugStartCombat(/*live=*/false);
    g.onButton(press(Button::C));
    CHECK(g.combat().outcome() == Combat::Outcome::Fled);
    CHECK(out.count(Sound::Fled) == 1 && out.count(Sound::Knockout) == 0);

    // The per-hit cues are Event tier: ALERTS ONLY keeps a fight silent.
    RecordingSound quiet;
    Game h{StartMode::Hatched, "paypup"};
    h.setSoundOut(&quiet);
    h.setSoundMode(SoundMode::AlertsOnly);
    h.debugStartCombat(/*live=*/false);
    while (h.combat().outcome() == Combat::Outcome::Ongoing) h.onButton(press(Button::A));
    CHECK(quiet.cues.empty());
}

// The cabinet sounds the start, every clean lock, and the cleared board; the till plays
// the high-score fanfare in place of the win jingle when the run set a record.
void test_sound_arcade_stacker_cues() {
    RecordingSound out;
    Game g{StartMode::Hatched};
    g.setSoundOut(&out);
    g.setSoundMode(SoundMode::All);
    const int row = arcadeGameIndexById("stacker");
    enterArcadeCabinet(g, row, ArcadeDifficulty::Medium);
    out.cues.clear();
    g.onButton(press(Button::B));                 // START
    CHECK(out.count(Sound::ArcadeStart) == 1);
    CHECK(playStackerBoard(g, [](int) { return 0; }));
    CHECK(g.stacker().won());
    CHECK(out.count(Sound::Point) == kStackerRows - 1);
    CHECK(out.count(Sound::Miss) == 0);
    CHECK(out.count(Sound::Clear) == 1);
    g.onButton(press(Button::B));                 // park -> till
    CHECK(g.nav() == Game::Nav::ArcadeResult);
    CHECK(out.cues.back().sound == Sound::NewBest);
    CHECK(out.count(Sound::GameWin) == 0);

    // An overhang shaves the hand: such a lock is a miss, not a point.
    enterArcadeCabinet(g, row, ArcadeDifficulty::Medium);
    g.onButton(press(Button::B));
    out.cues.clear();
    CHECK(playStackerBoard(g, [](int r) { return r == 1 ? 1 : 0; }));
    CHECK(out.count(Sound::Miss) >= 1);
}

// The worm's run ends on exactly one crash cue, however long the board then sits parked.
void test_sound_isolation_crash_once() {
    RecordingSound out;
    Game g{StartMode::Hatched};
    g.setSoundOut(&out);
    g.setSoundMode(SoundMode::All);
    enterArcadeCabinet(g, arcadeGameIndexById("isolation"), ArcadeDifficulty::Medium);
    g.onButton(press(Button::B));
    CHECK(g.nav() == Game::Nav::Isolation);
    uint32_t t = 0;
    for (int i = 0; i < 400 && g.isolation().running(); ++i) g.tick(t += 400);
    CHECK(!g.isolation().running());
    for (int i = 0; i < 10; ++i) g.tick(t += 400);
    CHECK(out.count(Sound::Crash) == 1);
    CHECK(out.count(Sound::Clear) == 0);
}
