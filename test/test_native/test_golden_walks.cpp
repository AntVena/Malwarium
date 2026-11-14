// test_golden_walks.cpp (native gates): scripted walks through the game, held to a
// recording.
//
// A refactor that means to change nothing is checked by playing the same presses and
// ticks before and after and comparing every frame. These gates keep that check in the
// suite: each walk drives Game through its public input (onButton, tick) and records,
// for every step, the screen and face it lands on and an FNV-1a hash of the frame.
// golden_<walk>.h holds the recording. A walk that drifts from it fails, naming the
// first step that differs, so a change nobody meant shows up in the next run rather
// than on a device.
//
// A change that MEANS to alter what a walk shows is recorded again with
// tools/golden_walks.sh, which rewrites the headers; the diff then shows which steps
// moved. Each game starts its own clock at zero, since a clock carried across games
// jumps on the first tick and hides how long a screen really held.
#include "test_gates.h"

#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

// One recorded step: what the walk did, where it landed, and the frame it drew.
struct GoldenStep {
    const char* tag;   // null ends a recording
    int nav;
    int face;
    uint64_t frame;
};

}  // namespace

#include "golden_cfg.h"
#include "golden_chord.h"
#include "golden_idle.h"

namespace {

uint64_t fnv1a64(const uint8_t* p, size_t n) {
    uint64_t h = 1469598103934665603ull;
    for (size_t i = 0; i < n; ++i) {
        h ^= p[i];
        h *= 1099511628211ull;
    }
    return h;
}

// A walk in progress: the steps it has taken, and the clock of the game it is driving.
class Walk {
public:
    explicit Walk(const char* name) : name_(name) {}

    // Every game starts its clock at zero, as a booted device does.
    void newGame() { t_ = 1000; }
    uint32_t& clock() { return t_; }

    void snap(Game& g, const char* tag) {
        Framebuffer fb(kActiveW, kActiveH);
        g.render(fb);
        const auto* bytes = reinterpret_cast<const uint8_t*>(fb.data());
        steps_.push_back({tag, static_cast<int>(g.nav()), static_cast<int>(g.face()),
                          fnv1a64(bytes, sizeof(Rgb565) * kActiveW * kActiveH)});
    }
    void press(Game& g, Button b, const char* tag = "press") {
        g.onButton(::press(b));
        g.onButton(lift(b));
        snap(g, tag);
    }
    void chord(Game& g) {
        g.onButton(chordAC());
        snap(g, "chord");
        g.onButton(lift(Button::A));
        g.onButton(lift(Button::C));
    }
    void beats(Game& g, int n, const char* tag = "beats") {
        for (int i = 0; i < n; ++i) g.tick(t_ += kHeartbeatMs);
        snap(g, tag);
    }
    // Untouched for `ms`, in one step: the clocks that read elapsed time see it all.
    void jump(Game& g, uint32_t ms, const char* tag) {
        g.tick(t_ += ms);
        snap(g, tag);
    }
    void home(Game& g) {
        for (int k = 0; k < 8 && g.nav() != Game::Nav::Idle; ++k) tapC(g);
    }
    // A seeded run of presses, chords and ticks, the walk's fuzz.
    void script(Game& g, int n, uint32_t seed) {
        for (int i = 0; i < n; ++i) {
            seed ^= seed << 13;
            seed ^= seed >> 17;
            seed ^= seed << 5;
            switch (seed % 9) {
                case 0: press(g, Button::A, "rnd"); break;
                case 1: press(g, Button::B, "rnd"); break;
                case 2: press(g, Button::C, "rnd"); break;
                case 3:
                case 4: chord(g); break;
                case 5: g.tick(t_ += 7); snap(g, "rnd"); break;
                case 6: g.tick(t_ += 63); snap(g, "rnd"); break;
                default: beats(g, 1, "rnd"); break;
            }
        }
    }

    // Writes the recording when MAL_GOLDEN_WRITE names a directory, else compares with
    // `golden` and reports the first step that differs.
    void settle(const GoldenStep* golden, const char* header) const {
        if (const char* dir = std::getenv("MAL_GOLDEN_WRITE")) {
            write(std::string(dir) + "/" + header);
            return;
        }
        size_t n = 0;
        while (golden[n].tag) ++n;
        size_t differing = 0, first = steps_.size();
        for (size_t i = 0; i < steps_.size() && i < n; ++i) {
            const GoldenStep& want = golden[i];
            const GoldenStep& got = steps_[i];
            if (want.nav != got.nav || want.face != got.face || want.frame != got.frame ||
                std::strcmp(want.tag, got.tag) != 0) {
                if (first == steps_.size()) first = i;
                ++differing;
            }
        }
        if (differing == 0 && n == steps_.size()) return;
        std::printf("  golden walk '%s': %zu of %zu steps differ (recorded %zu)\n", name_,
                    differing, steps_.size(), n);
        if (first < steps_.size() && first < n)
            std::printf("    first at step %zu '%s': nav %d face %d frame %016llx, recorded"
                        " '%s' nav %d face %d frame %016llx\n",
                        first, steps_[first].tag, steps_[first].nav, steps_[first].face,
                        static_cast<unsigned long long>(steps_[first].frame), golden[first].tag,
                        golden[first].nav, golden[first].face,
                        static_cast<unsigned long long>(golden[first].frame));
        std::printf("    a change meant to alter this walk is recorded with "
                    "tools/golden_walks.sh\n");
        CHECK(differing == 0 && n == steps_.size());
    }

private:
    void write(const std::string& path) const {
        FILE* f = std::fopen(path.c_str(), "w");
        CHECK(f != nullptr);
        if (!f) return;
        std::fprintf(f,
                     "// Generated by tools/golden_walks.sh from test_golden_walks.cpp; do not\n"
                     "// edit by hand. One row per step of the '%s' walk: what the step did,\n"
                     "// the screen and face it landed on, and an FNV-1a hash of the frame.\n"
                     "#pragma once\n\n"
                     "static const GoldenStep kGolden_%s[] = {\n",
                     name_, name_);
        for (const GoldenStep& s : steps_)
            std::fprintf(f, "    {\"%s\", %d, %d, 0x%016llxull},\n", s.tag, s.nav, s.face,
                         static_cast<unsigned long long>(s.frame));
        std::fprintf(f, "    {nullptr, 0, 0, 0},\n};\n");
        std::fclose(f);
    }

    const char* name_;
    uint32_t t_ = 1000;
    std::vector<GoldenStep> steps_;
};

void playedBoard(Walk& w, MaintKind k, ArcadeGameKind engine, uint32_t seed) {
    w.newGame();
    Game g{StartMode::Hatched};
    startPlayedMaint(g, k, engine);
    g.onButton(lift(Button::A));
    w.snap(g, "board");
    w.chord(g);
    w.beats(g, 2);
    w.chord(g);
    w.beats(g, 3);
    w.script(g, 60, seed);
}

}  // namespace

// The A+C chord on every screen that gives it a meaning, and on the ones that do not:
// the home screen's four readings, the fight's picker, the arena's briefing, the
// DECRYPTOGRAM's drop, the boards' RULES page and the egg's crack, with a fuzz of
// presses and ticks after each.
void test_golden_chord_walk() {
    Walk w("chord");
    {
        w.newGame();
        Game g{StartMode::Hatched};
        w.snap(g, "home");
        w.chord(g);
        w.chord(g);
        w.press(g, Button::A);
        w.chord(g);
        w.press(g, Button::A);
        w.chord(g);
        w.chord(g);
        w.beats(g, 3);
        w.script(g, 120, 12345);
    }
    {
        w.newGame();
        Game g{StartMode::Hatched};
        enterWalk(g);
        g.onButton(lift(Button::A));
        w.snap(g, "walk");
        w.chord(g);
        w.press(g, Button::C);
        w.chord(g);
        w.beats(g, 30);
        w.script(g, 120, 99);
    }
    {
        w.newGame();
        Game g{StartMode::Hatched};
        enterSimBattle(g);
        g.onButton(lift(Button::A));
        w.snap(g, "sim");
        w.chord(g);
        w.beats(g, 4);
        w.press(g, Button::B);
        w.chord(g);
        w.script(g, 80, 7);
    }
    {
        w.newGame();
        Game g{StartMode::Hatched};
        walkToEncounter(g);
        w.snap(g, "encounter");
        w.chord(g);
        w.beats(g, 2);
        w.script(g, 60, 8);
    }
    {
        w.newGame();
        Game g{StartMode::Hatched};
        walkToWifiEvent(g);
        w.snap(g, "wifi");
        w.chord(g);
        w.beats(g, 20);
        w.script(g, 60, 9);
    }
    {
        w.newGame();
        Game g{StartMode::Hatched};
        walkToAnyCombat(g);
        w.snap(g, "fight");
        w.chord(g);
        for (int i = 0; i < 30; ++i) {
            tapB(g);
            w.beats(g, 2);
        }
        w.chord(g);
        w.script(g, 60, 10);
    }
    {
        w.newGame();
        Game g{StartMode::Hatched};
        g.inventory().add("decryptogram", 2);
        enterHackerSlot(g, HackerSlotId::Vault);
        g.onButton(press(Button::B));
        w.snap(g, "cryptogram");
        w.chord(g);
        w.chord(g);
        w.press(g, Button::A);
        w.press(g, Button::B);
        w.chord(g);
        w.script(g, 80, 11);
    }
    {
        w.newGame();
        Game g{StartMode::Hatched, "bruinforce"};
        g.addCombatXp(600000);
        g.liveOperator().world.sectorCleared[0] = true;
        enterSubmenuId(g, SubmenuId::Expl);
        explPickCategory(g, ExplCat::Arena);
        g.onButton(press(Button::B));
        g.onButton(lift(Button::A));
        w.snap(g, "arena");
        w.chord(g);
        w.press(g, Button::B);
        w.chord(g);
        w.script(g, 80, 12);
    }
    playedBoard(w, MaintKind::Av, ArcadeGameKind::Stacker, 13);
    playedBoard(w, MaintKind::Defrag, ArcadeGameKind::Fragger, 14);
    playedBoard(w, MaintKind::Defrag, ArcadeGameKind::Decryption, 15);
    {
        // The egg: a hatch board, the incubation, a ready egg from a menu and from home,
        // and the reveal mashed.
        w.newGame();
        Game g;
        g.onButton(press(Button::B));
        g.onButton(lift(Button::B));
        w.snap(g, "egg-board");
        w.chord(g);
        w.beats(g, 3);
        w.script(g, 60, 16);
    }
    {
        w.newGame();
        Game g;
        pickFirstEggLine(g);
        w.snap(g, "egg");
        w.chord(g);
        w.beats(g, 40);
        w.chord(g);
        g.accelerateEggHatch(g.bootHatchRemainMs() - kHatchRevealMs / 2);
        w.snap(g, "ready");
        enterSlot(g, SubmenuId::Stat);
        g.onButton(lift(Button::A));
        w.snap(g, "menu");
        w.chord(g);
        w.home(g);
        w.chord(g);
        w.beats(g, 2);
        w.chord(g);
        w.beats(g, 80);
        w.script(g, 80, 17);
    }
    w.settle(kGolden_chord, "golden_chord.h");
}

// Every CFG page: UPDATES through each install state and its flash QR, the radio pages
// with the AP's QR, FORMAT SD and RADIO with an update job live and without, then the
// hardware, looks, settings, TIPS and the Factory Reset in both scopes.
void test_golden_cfg_walk() {
    Walk w("cfg");
    auto found = [&w](Game& g) {
        g.setNetProvisioned(true);
        g.setUpdateSourceKnown(true);
        UpdateStatus f;
        f.state = UpdateState::Available;
        f.firmwareNewer = true;
        std::strcpy(f.firmwareVersion, "0.4.2");
        f.webNewer = true;
        std::strcpy(f.webVersion, "0.4.0");
        g.requestUpdateCheck();
        w.snap(g, "checking");
        g.setUpdateStatus(f);
        w.snap(g, "found");
    };
    auto install = [&w](Game& g, InstallState s, uint32_t received) {
        InstallStatus st;
        st.state = s;
        st.target = g.updateJobTarget();
        st.received = received;
        st.total = 4096;
        if (s == InstallState::Failed) st.fail = InstallFail::NoNetwork;
        g.setInstallStatus(st);
        w.snap(g, "install");
    };
    {
        w.newGame();
        Game g{StartMode::Hatched};
        enterCfgTarget(g, CfgScreen::Update);
        w.snap(g, "updates");
        for (Button b : {Button::A, Button::B, Button::A, Button::B, Button::C}) w.press(g, b);
    }
    {
        w.newGame();
        Game g{StartMode::Hatched};
        found(g);
        enterCfgTarget(g, CfgScreen::Update);
        w.snap(g, "updates");
        for (Button b : {Button::A, Button::B, Button::A, Button::A, Button::C, Button::B,
                         Button::A, Button::B})
            w.press(g, b);
        install(g, InstallState::Downloading, 1000);
        w.press(g, Button::A);
        install(g, InstallState::Verifying, 4096);
        install(g, InstallState::Installing, 4096);
        w.press(g, Button::C);
        w.beats(g, 3);
        enterCfgTarget(g, CfgScreen::Update);
        w.snap(g, "back");
        install(g, InstallState::Done, 4096);
        w.press(g, Button::C);
        enterCfgTarget(g, CfgScreen::Update);
        for (int i = 0; i < 3; ++i) w.press(g, Button::A);
        w.press(g, Button::B, "flash-qr");
        w.press(g, Button::C);
        w.press(g, Button::C);
    }
    {
        w.newGame();
        Game g{StartMode::Hatched};
        found(g);
        enterCfgTarget(g, CfgScreen::Update);
        for (Button b : {Button::A, Button::A, Button::B, Button::A, Button::B}) w.press(g, b);
        install(g, InstallState::Failed, 10);
        w.press(g, Button::C);
    }
    for (int live = 0; live < 2; ++live) {
        w.newGame();
        Game g{StartMode::Hatched};
        SdStatus sd;
        sd.present = true;
        sd.sizeMB = 3800;
        g.setSdStatus(sd);
        if (live) {
            found(g);
            g.requestUpdateInstall(UpdateTarget::Firmware);
        }
        enterCfgTarget(g, CfgScreen::FormatSd);
        w.snap(g, "format-sd");
        w.press(g, Button::A);
        w.press(g, Button::B);
        if (g.sdFormatState() == SdFormatState::Working) {
            g.finishSdFormat(true);
            w.snap(g, "formatted");
        }
        w.press(g, Button::C);
        w.home(g);
        enterCfgTarget(g, CfgScreen::Radio);
        w.snap(g, "radio");
        for (Button b : {Button::A, Button::B, Button::A, Button::B, Button::C}) w.press(g, b);
    }
    for (int stored = 0; stored < 2; ++stored) {
        w.newGame();
        Game g{StartMode::Hatched};
        g.setNetProvisioned(stored != 0);
        enterCfgTarget(g, CfgScreen::Audit);
        w.snap(g, "audit");
        for (int i = 0; i < 4; ++i) w.press(g, Button::A);
        w.press(g, Button::B);
        w.home(g);
        enterCfgTarget(g, CfgScreen::PediaAp);
        w.snap(g, "ap");
        w.press(g, Button::A);
        w.press(g, Button::B, "pedia-qr");
        for (int i = 0; i < 3; ++i) w.press(g, Button::A);
        w.press(g, Button::C);
        w.home(g);
        enterCfgTarget(g, CfgScreen::Link);
        w.snap(g, "link");
        w.press(g, Button::A);
        w.press(g, Button::B);
    }
    for (int pass = 0; pass < 2; ++pass) {
        w.newGame();
        Game g{StartMode::Hatched};
        SdStatus sd;
        sd.present = pass != 0;
        sd.sizeMB = 3800;
        g.setSdStatus(sd);
        if (pass) {
            g.unlockTitle(0);
            g.unlockTitle(2);
        }
        uint32_t& t = w.clock();
        enterCfgTarget(g, CfgScreen::SysInfo);
        w.snap(g, "sysinfo");
        w.press(g, Button::A);
        g.onButton(press(Button::B));
        w.beats(g, 4, "holding");
        g.onButton(lift(Button::B));
        g.onButton(press(Button::B));
        g.tick(t += kFactoryRevealMs + kHeartbeatMs);
        w.snap(g, "reveal");
        g.onButton(lift(Button::B));
        w.press(g, Button::A);
        w.press(g, Button::C);
        w.home(g);
        enterCfgTarget(g, CfgScreen::HackerTag);
        w.snap(g, "tag");
        for (Button b : {Button::A, Button::A, Button::B, Button::A, Button::C, Button::C,
                         Button::B, Button::B})
            w.press(g, b);
        for (int i = 0; i < kHackerTagMax + 2; ++i) w.press(g, Button::B);
        const CfgScreen pickers[] = {CfgScreen::Titles,     CfgScreen::Background,
                                     CfgScreen::Theme,      CfgScreen::UiMode,
                                     CfgScreen::Brightness, CfgScreen::Sound,
                                     CfgScreen::Volume};
        for (CfgScreen c : pickers) {
            w.home(g);
            enterCfgTarget(g, c);
            w.snap(g, "picker");
            for (int i = 0; i < 2 + pass; ++i) w.press(g, Button::A);
            w.press(g, pass ? Button::B : Button::C);
        }
        w.home(g);
        enterCfgTarget(g, CfgScreen::Device);
        w.snap(g, "device");
        for (int i = 0; i < 12; ++i) {
            w.press(g, Button::A);
            if (g.cfgScreen() == CfgScreen::Device) w.press(g, Button::B);
            if (g.cfgScreen() != CfgScreen::Device) w.press(g, Button::C);
        }
        w.home(g);
        enterCfgTarget(g, CfgScreen::Tips);
        w.snap(g, "tips");
        w.press(g, Button::A);
        w.press(g, Button::B);
        w.beats(g, 2);
        w.press(g, Button::C);
        w.home(g);
        enterCfgTarget(g, CfgScreen::Travel);
        w.snap(g, "travel");
        for (Button b : {Button::A, Button::A, Button::B, Button::A, Button::B}) w.press(g, b);
        w.home(g);
        enterCfgTarget(g, CfgScreen::SysInfo);
        g.onButton(press(Button::B));
        g.tick(t += kFactoryRevealMs + kHeartbeatMs);
        g.onButton(lift(Button::B));
        if (pass) w.press(g, Button::A);
        g.onButton(press(Button::B));
        g.tick(t += kFactoryCommitMs + kHeartbeatMs);
        w.snap(g, "committed");
        g.onButton(lift(Button::B));
        w.beats(g, 2);
    }
    w.settle(kGolden_cfg, "golden_cfg.h");
}

// How long each screen holds the menu tree untouched, and whether a Lockout crisis
// fires over it: every slot's list and page, every Hacker slot, the CFG pages, a live
// update job, the walk screens and the boards, each left for 30 seconds, then for 9
// minutes, then 11 (the long budget is 10).
void test_golden_idle_walk() {
    Walk w("idle");
    auto wait = [&w](Game& g, const char* tag) {
        w.snap(g, tag);
        for (int s = 0; s < 6; ++s) w.beats(g, 20, "5s");
        w.jump(g, 9u * 60u * 1000u, "9m");
        w.jump(g, 2u * 60u * 1000u, "11m");
    };
    auto starve = [&w](Game& g, const char* tag) {
        g.model().setHunger(0);
        w.beats(g, 8, tag);
    };
    {
        w.newGame();
        Game g{StartMode::Hatched};
        wait(g, "idle");
        g.onButton(press(Button::A));
        g.onButton(lift(Button::A));
        wait(g, "cursor");
    }
    const SubmenuId slots[] = {SubmenuId::Stat, SubmenuId::Items, SubmenuId::Games,
                               SubmenuId::Expl, SubmenuId::Maint, SubmenuId::Mods,
                               SubmenuId::Arch, SubmenuId::Cfg};
    for (SubmenuId id : slots)
        for (int page = 0; page < 2; ++page) {
            w.newGame();
            Game g{StartMode::Hatched};
            enterSubmenuId(g, id);
            g.onButton(lift(Button::A));
            if (page) {
                g.onButton(press(Button::B));
                g.onButton(lift(Button::B));
            }
            wait(g, page ? "slot-page" : "slot-list");
        }
    const HackerSlotId hacker[] = {HackerSlotId::Crew, HackerSlotId::Profile,
                                   HackerSlotId::Shop, HackerSlotId::Vault,
                                   HackerSlotId::Merge, HackerSlotId::Scan,
                                   HackerSlotId::Link, HackerSlotId::Peers};
    for (HackerSlotId h : hacker)
        for (int page = 0; page < 2; ++page) {
            w.newGame();
            Game g{StartMode::Hatched};
            enterHackerSlot(g, h);
            g.onButton(lift(Button::A));
            g.onButton(lift(Button::B));
            if (page) {
                g.onButton(press(Button::B));
                g.onButton(lift(Button::B));
            }
            wait(g, page ? "hacker-page" : "hacker-list");
        }
    const CfgScreen cfgs[] = {CfgScreen::SysInfo, CfgScreen::HackerTag, CfgScreen::Titles,
                              CfgScreen::Device,  CfgScreen::Theme,     CfgScreen::Tips,
                              CfgScreen::Radio,   CfgScreen::Audit,     CfgScreen::Update,
                              CfgScreen::Travel};
    for (CfgScreen c : cfgs) {
        w.newGame();
        Game g{StartMode::Hatched};
        enterCfgTarget(g, c);
        g.onButton(lift(Button::A));
        wait(g, "cfg-page");
    }
    {
        w.newGame();
        Game g{StartMode::Hatched};
        enterCfgTarget(g, CfgScreen::PediaAp);
        g.onButton(press(Button::A));
        g.onButton(press(Button::B));
        wait(g, "pedia-qr");
    }
    {
        w.newGame();
        Game g{StartMode::Hatched};
        g.setNetProvisioned(true);
        g.setUpdateSourceKnown(true);
        g.requestUpdateCheck();
        enterSubmenuId(g, SubmenuId::Stat);
        g.onButton(lift(Button::A));
        wait(g, "update-job");
    }
    {
        // A page CFG showed last, then left: another slot's page holds or lingers by its
        // own row, not by the page CFG remembers.
        const CfgScreen stale[] = {CfgScreen::HackerTag, CfgScreen::Update};
        for (CfgScreen c : stale) {
            w.newGame();
            Game g{StartMode::Hatched};
            enterCfgTarget(g, c);
            g.onButton(press(Button::C));
            w.home(g);
            enterSubmenuId(g, SubmenuId::Items);
            g.onButton(lift(Button::A));
            g.onButton(press(Button::B));
            g.onButton(lift(Button::B));
            wait(g, "after-cfg");
        }
    }
    {
        w.newGame();
        Game g{StartMode::Hatched};
        enterWalk(g);
        g.onButton(lift(Button::A));
        wait(g, "walk");
    }
    {
        w.newGame();
        Game g{StartMode::Hatched};
        walkToEncounter(g);
        wait(g, "encounter");
    }
    {
        w.newGame();
        Game g{StartMode::Hatched};
        enterSimBattle(g);
        g.onButton(lift(Button::A));
        wait(g, "sim");
    }
    {
        w.newGame();
        Game g{StartMode::Hatched};
        startPlayedMaint(g, MaintKind::Av, ArcadeGameKind::Stacker);
        g.onButton(lift(Button::A));
        wait(g, "board");
    }
    {
        w.newGame();
        Game g;
        g.onButton(press(Button::B));
        g.onButton(lift(Button::B));
        wait(g, "egg-board");
    }
    {
        w.newGame();
        Game g{StartMode::Hatched};
        starve(g, "crisis-home");
    }
    {
        w.newGame();
        Game g{StartMode::Hatched};
        enterSubmenuId(g, SubmenuId::Items);
        g.onButton(lift(Button::A));
        starve(g, "crisis-items");
    }
    {
        w.newGame();
        Game g{StartMode::Hatched};
        g.inventory().add("decryptogram", 1);
        enterHackerSlot(g, HackerSlotId::Vault);
        g.onButton(press(Button::B));
        starve(g, "crisis-cryptogram");
    }
    {
        w.newGame();
        Game g{StartMode::Hatched};
        startPlayedMaint(g, MaintKind::Defrag, ArcadeGameKind::Decryption);
        g.onButton(lift(Button::A));
        starve(g, "crisis-decryption");
    }
    {
        w.newGame();
        Game g{StartMode::Hatched};
        enterSimBattle(g);
        g.onButton(lift(Button::A));
        starve(g, "crisis-fight");
    }
    w.settle(kGolden_idle, "golden_idle.h");
}
