// test_main.cpp — native logic + render gates.
//
// Runs with plain CMake/ctest (no PlatformIO/Unity dependency yet). The CHECK
// macro mirrors Unity's TEST_ASSERT semantics so these can be ported into a
// `pio test -e native` Unity suite when PlatformIO is wired up.
#include <cmath>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <vector>

#include "core/app/game.h"
#include "core/app/game_internal.h"   // kMergeRecipes — item earn-path coverage guard
#include "core/app/pedia_state.h"
#include "core/content/areas/deepweb_dive/area.h"
#include "core/content/content_passives.h"
#include "core/content/content_tables.h"
#include "core/content/effect_text.h"
#include "core/content/registry.h"
#include "core/model/event_log.h"
#include "core/model/hacker_rank.h"
#include "core/model/inventory.h"
#include "core/model/combat.h"
#include "core/model/loadout.h"
#include "core/model/move_loadout.h"
#include "core/model/pet_model.h"
#include "core/model/save.h"
#include "core/model/stacker.h"
#include "core/net/audit_capture.h"
#include "core/net/eapol.h"
#include "core/net/network_ledger.h"
#include "core/net/pcap.h"
#include "core/net/pcap_naming.h"
#include "core/net/peer_ledger.h"
#include "core/net/peer_link.h"
#include "core/net/pvp_link.h"
#include "core/net/tar_reader.h"
#include "core/net/update_manifest.h"
#include "core/net/version_marker.h"
#include "core/model/pvp_battle.h"
#include "core/render/canvas.h"
#include "core/render/color.h"
#include "core/render/font5x7.h"      // textWidth — mirroring a screen's own layout maths
#include "core/render/framebuffer.h"
#include "core/render/palette.h"
#include "core/render/sprite.h"
#include "core/render/upscaler.h"
#include "core/ui/carousel.h"
#include "core/ui/combat_screen.h"
#include "core/ui/expl_screen.h"
#include "core/app/game_rig_shop.h"  // rigSpec — the Rig Shop shares the readout rails
#include "core/app/game_internal.h"  // kRigRowPitch — the SHOP row geometry
#include "core/ui/widgets.h"      // gridLines — the readout grid's own packing rule
#include "core/ui/items_screen.h"
#include "core/ui/maint_screen.h"
#include "core/ui/mods_screen.h"
#include "core/ui/stat_screen.h"
#include "generated/assets.h"
#include "tunables.h"
#include "version.h"

// Button press edge (the common case in these gates).
static mal::ButtonEvent press(mal::Button b) { return {b, true, false}; }

using namespace mal;

// In-memory ISaveStore for the persistence gates (the test tier of the seam).
class MemSaveStore : public ISaveStore {
public:
    std::vector<uint8_t> load() override { return blob_; }
    bool save(const std::vector<uint8_t>& d) override { blob_ = d; return true; }
    void clear() override { blob_.clear(); }
    const std::vector<uint8_t>& bytes() const { return blob_; }
private:
    std::vector<uint8_t> blob_;
};

// A store that can be told to refuse a write. The heap guard's refusal is already
// drivable (setHeapProbe); this is the other half — the save was built and handed over,
// and the MEDIUM said no, which on device is a short NVS write.
class RefusingSaveStore : public ISaveStore {
public:
    std::vector<uint8_t> load() override { return blob_; }
    bool save(const std::vector<uint8_t>& d) override {
        ++attempts;
        if (refuse) return false;          // whatever was on "flash" stays there
        blob_ = d;
        return true;
    }
    void clear() override { blob_.clear(); }
    const std::vector<uint8_t>& bytes() const { return blob_; }
    bool refuse = false;
    int attempts = 0;
private:
    std::vector<uint8_t> blob_;
};

// The Bits an achievement pays on unlock, read off its own row. Gates that check a
// wallet after an action that also EARNS something use this instead of restating the
// number, so retuning a reward doesn't need a test edit to go with it.
static int achBitsReward(const char* id) {
    const AchievementDef* d = achievementById(id);
    if (!d) return 0;
    int n = 0;
    for (const AchievementReward& r : d->rewards)
        if (r.kind == AchievementReward::Kind::Bits) n += r.magnitude;
    return n;
}

// Dual-mode gate runner. The gate bodies below are the single source of truth;
// they run under two harnesses depending on how the file is compiled:
//   * ctest (CMake `mal_tests`)      — the counting CHECK + hand-rolled main().
//   * `pio test -e native` (Unity)   — CHECK maps to a Unity assertion and each
//                                       gate is a RUN_TEST (PlatformIO defines
//                                       PIO_UNIT_TESTING for test builds).
// The gate roster lives once in MAL_RUN_ALL_GATES (near main) so the two
// harnesses can never drift out of sync.
#ifdef PIO_UNIT_TESTING
#include <unity.h>
#define CHECK(cond) TEST_ASSERT_TRUE(cond)
#else
static int g_failures = 0;
#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);    \
            ++g_failures;                                                    \
        }                                                                    \
    } while (0)
#endif

// --- Gate: PNG round-trips to RGB565 and blits pixel-correct ---------------
static void test_sprite_roundtrip() {
    // Paypup decoded to the expected sheet geometry (448x48, 8 frames of 56).
    const SpriteData& pet = ASSET_SPR_PET_PAYPUP;
    CHECK(pet.sheetW == 448);
    CHECK(pet.h == 48);
    CHECK(pet.frameW == 56);
    CHECK(pet.frames == 8);

    // Blitting frame 0 reproduces every opaque source pixel exactly.
    Framebuffer fb(kLogicalW, kLogicalH);
    fb.clear(palColor(Pal::PAPER));
    drawSprite(fb, pet, 0, 0, 0);
    int opaqueChecked = 0;
    for (int y = 0; y < pet.h; ++y) {
        for (int x = 0; x < pet.frameW; ++x) {
            if (spriteAlphaAt(pet, x, y) == 255) {
                CHECK(fb.get(x, y) == spriteColorAt(pet, x, y));
                ++opaqueChecked;
            }
        }
    }
    CHECK(opaqueChecked > 0);  // sanity: the sprite isn't fully transparent
}

// --- Gate: x1.75 upscale has clean logical-pixel boundaries ----------------
static void test_upscale_boundaries() {
    Framebuffer fb(kLogicalW, kLogicalH);
    // Distinct value per source column so we can verify the mapping exactly.
    for (int y = 0; y < kLogicalH; ++y)
        for (int x = 0; x < kLogicalW; ++x) fb.set(x, y, static_cast<Rgb565>(x));

    std::vector<Rgb565> out;
    upscale(fb, out, kScaleNum, kScaleDen);
    CHECK(static_cast<int>(out.size()) == kActiveW * kActiveH);

    // Every output pixel equals its nearest source column (no blending).
    for (int ox = 0; ox < kActiveW; ++ox) {
        int expected = ox * kScaleDen / kScaleNum;
        CHECK(out[ox] == static_cast<Rgb565>(expected));
    }
    // 7/4 expands each run of 4 source columns into 7 output columns.
    CHECK(kActiveW == 224 && kActiveH == 224);
    CHECK((out[6] == 3) && (out[7] == 4));  // the 2,2,2,1 boundary
}

// --- Gate: PAL_CORE zones are luminance-ordered (grayscale survives) -------
static void test_palette_luminance_ordered() {
    // calm < warn < hot must be monotonic so status reads without colour.
    CHECK(palLum(Pal::HOT) < palLum(Pal::WARN));
    CHECK(palLum(Pal::WARN) < palLum(Pal::CALM));
    // ink-on-paper is the highest-contrast pair.
    CHECK(std::fabs(palLum(Pal::INK) - palLum(Pal::PAPER)) > 0.5f);
}

// --- T1 idle canvas helpers ------------------------------------------------
static void renderIdleAtBeat(Framebuffer& fb, int beat, bool hungry) {
    Game game{StartMode::Hatched};
    // The raising loop owns Hunger now; drive the model directly to the Critical
    // band (not 0 — that would fire the Lockout modal instead of the idle alert).
    if (hungry) game.model().setHunger(10);
    for (int i = 0; i <= beat; ++i)
        game.tick(static_cast<uint32_t>(i) * kHeartbeatMs);
    game.render(fb);
}

static bool anyNonPaper(const Framebuffer& fb, int x0, int y0, int x1, int y1) {
    const Rgb565 paper = palColor(Pal::PAPER);
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x)
            if (fb.get(x, y) != paper) return true;
    return false;
}

// --- Resting habitat — pet in the living area, carousel always framing it.
static void test_idle_habitat() {
    Framebuffer fb(kActiveW, kActiveH);
    renderIdleAtBeat(fb, 0, /*hungry=*/false);

    // Pet occupies the living band; the carousel tracks are always drawn now.
    CHECK(anyNonPaper(fb, 0, kLivingTop, kActiveW, kLivingBottom));   // pet
    CHECK(anyNonPaper(fb, 0, 0, kActiveW, kTrackH));                  // top track icons
    CHECK(anyNonPaper(fb, 0, kLivingBottom, kActiveW, kActiveH));     // bottom track icons
    // Resting = no cursor: no focused-slot caption in the top edge band, and no
    // focus box (slot-1 box pixel reads as bare track, not accent — cf.
    // test_carousel_focus_grayscale which summons the cursor there).
    CHECK(!anyNonPaper(fb, 0, kLivingTop, kActiveW, kLivingTop + 12));
    CHECK(luminance(fb.get(12, 3)) < 0.4f);
}

// --- T1: hunger alert is gated on the hungry state -------------------------
// Now sits in the living area's top-right (the top track is the icon shelf).
static void test_hunger_alert_gated() {
    const int x0 = kActiveW - 24, y0 = kLivingTop + 2, y1 = kLivingTop + 26;
    Framebuffer calm(kActiveW, kActiveH), hungry(kActiveW, kActiveH);
    renderIdleAtBeat(calm, 0, false);
    renderIdleAtBeat(hungry, 0, true);   // beat 0 is an "on" blink phase
    CHECK(!anyNonPaper(calm, x0, y0, kActiveW, y1));
    CHECK(anyNonPaper(hungry, x0, y0, kActiveW, y1));
}

// --- T1: the idle loop animates (frames differ across the heartbeat) -------
static void test_idle_breathe_animates() {
    Framebuffer a(kActiveW, kActiveH), b(kActiveW, kActiveH);
    renderIdleAtBeat(a, 0, false);   // idle_a
    renderIdleAtBeat(b, 1, false);   // idle_b
    int diff = 0;
    for (int y = 0; y < kActiveH; ++y)
        for (int x = 0; x < kActiveW; ++x)
            if (a.get(x, y) != b.get(x, y)) ++diff;
    CHECK(diff > 0);
}

// --- Idle status icons (SD-present reveal + hot/broadcasting) ---------------
// A region has a grayscale-bright pixel (the white glyph reads against the dark
// #14171c paper without colour) — the non-colour channel these icons live on.
static bool anyLitGray(const Framebuffer& fb, int x0, int y0, int x1, int y1) {
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x)
            if (luminance(fb.get(x, y)) > 0.12f) return true;
    return false;
}

// SD-present icon: reveals when the card becomes present, then hides after the
// window; a re-read of an already-present card doesn't re-flash it, but a
// re-insert (absent -> present) does.
static void test_sd_icon_reveal_window() {
    Game g{StartMode::Hatched};
    CHECK(!g.sdIconVisible());                       // no card -> no icon

    g.setSdStatus({true, 8000});                     // card present -> reveal arms (t=0)
    CHECK(g.sdIconVisible());
    g.tick(kSdIconRevealMs / 2);
    CHECK(g.sdIconVisible());                         // still within the window
    g.tick(kSdIconRevealMs + kHeartbeatMs);
    CHECK(!g.sdIconVisible());                        // window elapsed -> hidden
    CHECK(g.sdStatus().present);                      // ...card still present (SD line stays)

    g.setSdStatus({true, 8000});                     // same card, fresh reading
    CHECK(!g.sdIconVisible());                        // no re-flash on a non-transition

    g.setSdStatus({false, 0});                       // pulled
    g.setSdStatus({true, 8000});                     // re-inserted -> re-flash
    CHECK(g.sdIconVisible());
}

// The capture-phase badge tracks the Audit SM through every phase (the string
// that renders top-right on the idle screen). Disarmed shows nothing.
static void test_capture_badge_phases() {
    Game g{StartMode::Hatched};
    char buf[16];
    CHECK(!g.captureBadge(buf, sizeof buf));         // Disarmed -> no badge
    CHECK(buf[0] == '\0');

    g.setAuditCaptureEnabled(true);                  // -> Armed
    CHECK(g.captureBadge(buf, sizeof buf));
    CHECK(std::strncmp(buf, "ARM", 3) == 0);

    const uint8_t bssid[6] = {0xde, 0xad, 0xbe, 0xef, 0x00, 0x01};
    g.registerHandshake(bssid);                      // Armed -> Hot
    CHECK(g.auditCapture().broadcasting());
    CHECK(g.captureBadge(buf, sizeof buf));
    CHECK(std::strncmp(buf, "HOT", 3) == 0);

    g.tick(kAuditHotBroadcastMs + kHeartbeatMs);     // Hot expires -> Cooldown
    CHECK(!g.auditCapture().broadcasting());
    CHECK(g.captureBadge(buf, sizeof buf));
    CHECK(std::strncmp(buf, "COOL", 4) == 0);        // the ~30-min radio-off window

    // Cooldown elapses with the toggle still on -> auto re-arm -> ARM again.
    g.tick(kAuditHotBroadcastMs + kHeartbeatMs + kAuditRearmCooldownMs);
    CHECK(g.captureBadge(buf, sizeof buf));
    CHECK(std::strncmp(buf, "ARM", 3) == 0);
}

// Render the idle screen with any subset of the status conditions active at beat
// 0 (the hunger blink is "on" there). `hot` drives the capture SM to Hot so the
// top-right phase badge renders.
static void renderIdleStatus(Framebuffer& fb, bool hungry, bool sd, bool hot) {
    Game g{StartMode::Hatched};
    if (hungry) g.model().setHunger(10);
    if (hot) {
        g.setAuditCaptureEnabled(true);
        const uint8_t bssid[6] = {0xde, 0xad, 0xbe, 0xef, 0x00, 0x01};
        g.registerHandshake(bssid);                  // -> Hot (badge "HOT ...")
    }
    if (sd) g.setSdStatus({true, 8000});             // arms the reveal at t=0
    g.tick(0);                                       // beat 0
    g.render(fb);
}

// THE GATE: the idle status indicators sit in distinct, non-colliding slots so a
// grayscale screenshot separates them by shape + position — hunger top-right
// (upper), the capture badge top-right (lower), the SD flash top-left. Each reads
// in grayscale against the dark paper, and the SD flash is independently gated.
static void test_idle_status_icons_grayscale() {
    const int hx0 = kActiveW - 24, hy0 = kLivingTop + 2, hy1 = kLivingTop + 26;
    const int sx0 = 2, sx1 = 22, sy0 = kLivingTop + 2, sy1 = kLivingTop + 22;
    const int bx0 = kActiveW - 64, bx1 = kActiveW - 2,
              by0 = kLivingTop + 28, by1 = kLivingTop + 40;

    Framebuffer all(kActiveW, kActiveH);
    renderIdleStatus(all, /*hungry=*/true, /*sd=*/true, /*hot=*/true);
    CHECK(anyNonPaper(all, hx0, hy0, kActiveW, hy1));   // hunger present (top-right upper)
    CHECK(anyLitGray(all, sx0, sy0, sx1, sy1));         // SD flash reads in grayscale
    CHECK(anyLitGray(all, bx0, by0, bx1, by1));         // capture badge reads in grayscale

    // The SD flash is independently gated (its slot clears when no card) while the
    // capture badge stays — positions don't collide.
    Framebuffer noSd(kActiveW, kActiveH);
    renderIdleStatus(noSd, true, /*sd=*/false, /*hot=*/true);
    CHECK(!anyNonPaper(noSd, sx0, sy0, sx1, sy1));      // SD slot empty
    CHECK(anyLitGray(noSd, bx0, by0, bx1, by1));        // badge still shown
}

// Runtime SD re-check engine seam (mirrors the netScan one-shot). CFG sets it;
// the device tier reads + clears it after re-mounting.
static void test_sd_recheck_request_seam() {
    Game g{StartMode::Hatched};
    CHECK(!g.sdRecheckRequested());
    g.requestSdRecheck();
    CHECK(g.sdRecheckRequested());
    g.clearSdRecheck();
    CHECK(!g.sdRecheckRequested());
}

// --- T2: stat model logic --------------------------------------------------
static void test_pet_model_zones() {
    PetModel m;
    CHECK(m.hunger() == kStartHunger && m.fragmentation() == kStartFragmentation &&
          m.happiness() == kStartHappiness && m.careMistakes() == 0);

    m.setHunger(31); CHECK(m.hungerZone() == Zone::Ok);
    m.setHunger(16); CHECK(m.hungerZone() == Zone::Caution);
    m.setHunger(15); CHECK(m.hungerZone() == Zone::Critical && m.isHungry());

    m.setFragmentation(39); CHECK(m.fragZone() == Zone::Ok);
    m.setFragmentation(40); CHECK(m.fragZone() == Zone::Caution);
    m.setFragmentation(75); CHECK(m.fragZone() == Zone::Critical);

    m.setHappiness(30); CHECK(m.happyZone() == Zone::Ok);
    m.setHappiness(29); CHECK(m.happyZone() == Zone::Caution);
    m.setHappiness(9);  CHECK(m.happyZone() == Zone::Critical);
}

static void test_care_branch_and_clamp() {
    PetModel m;
    m.setCareMistakes(2); CHECK(m.careBranch() == CareBranch::Good);
    m.setCareMistakes(3); CHECK(m.careBranch() == CareBranch::Bad);
    m.setCareMistakes(4); CHECK(m.careBranch() == CareBranch::Bad);
    m.setCareMistakes(5); CHECK(m.careBranch() == CareBranch::Dying);
    m.addCareMistake(3);  CHECK(m.careMistakes() == kCareDying);  // clamps at 5
}

static void test_hunger_decay() {
    PetModel m;  // starts 80, -1 / 15 game-minutes
    m.tick(15u * 60u * 1000u);
    CHECK(m.hunger() == kStartHunger - 1);
    m.tick(15u * 60u * 1000u);
    CHECK(m.hunger() == kStartHunger - 2);
    // Sub-threshold time accumulates rather than dropping a point early.
    PetModel m2;
    m2.tick(14u * 60u * 1000u);
    CHECK(m2.hunger() == kStartHunger);
}

// --- Content registry seam (data-driven, SD-ready) -------------------------
static void test_content_registry() {
    ContentRegistry r = ContentRegistry::embedded();
    const CreatureDef* p = r.creature("paypup");
    CHECK(p != nullptr);
    CHECK(std::strcmp(p->displayName, "Paypup") == 0);
    CHECK(p->stage == Stage::Process);
    CHECK(r.creatureSprite(*p) == &ASSET_SPR_PET_PAYPUP);  // resolved by name
    CHECK(r.creature("does_not_exist") == nullptr);
    CHECK(r.eggLine("ransomware") != nullptr);
    CHECK(r.item("airgap_snack") != nullptr);
}

// --- THE GATE: STAT stays readable in grayscale (dual-coding) --------------
// Sample each gauge cell centre, desaturate, and confirm the lit-cell COUNT
// (the non-colour fill-level channel) matches the value at every zone colour.
static int litCellsGray(const Framebuffer& fb, int gx, int gw, int rowY) {
    const int cellW = gw / 10;
    int lit = 0;
    for (int i = 0; i < 10; ++i) {
        int cx = gx + i * cellW + cellW / 2;
        int cy = rowY + 5;  // gauge is 10px tall
        if (luminance(fb.get(cx, cy)) > 0.12f) ++lit;  // grayscale threshold
    }
    return lit;
}

static void test_grayscale_gate() {
    // Mirror stat_screen.cpp layout (kGaugeX=70, kGaugeW=110, rows 60/82/104).
    const int GX = 70, GW = 110;
    Game game{StartMode::Hatched};
    game.model().setHunger(12);          // Critical (hot 0.19)
    game.model().setFragmentation(82);   // ramp purple->pink
    game.model().setHappiness(50);       // Ok (calm)
    game.onButton(press(Button::A));     // idle A -> carousel @ STAT
    game.onButton(press(Button::B));     // B -> open STAT submenu (lands on VITALS)
    Framebuffer fb(kActiveW, kActiveH);
    game.render(fb);                     // beat 0 = pulse "on"

    CHECK(litCellsGray(fb, GX, GW, 60) == 1);   // hunger 12 -> 1
    CHECK(litCellsGray(fb, GX, GW, 82) == 8);   // frag 82 -> 8
    CHECK(litCellsGray(fb, GX, GW, 104) == 5);  // happy 50 -> 5
}

// Carousel summon + book-wrap -----------------------
static void test_carousel_summon() {
    Game a{StartMode::Hatched};
    a.onButton(press(Button::A));   // idle A -> carousel @ slot 1
    CHECK(a.nav() == Game::Nav::Cursor && a.cursor() == 0);
    Framebuffer fb(kActiveW, kActiveH);
    a.render(fb);
    CHECK(anyNonPaper(fb, 0, 0, kActiveW, kTrackH));               // top track chrome
    CHECK(anyNonPaper(fb, 0, kLivingBottom, kActiveW, kActiveH));  // bottom track chrome

    Game c{StartMode::Hatched};
    c.onButton(press(Button::C));   // idle C -> carousel @ slot 8
    CHECK(c.nav() == Game::Nav::Cursor && c.cursor() == kCarouselSlots - 1);

    Game b{StartMode::Hatched};                         // idle B is a no-op (no target)
    b.onButton(press(Button::B));
    CHECK(b.nav() == Game::Nav::Idle);
}

static void test_carousel_bookwrap() {
    Game g{StartMode::Hatched};
    g.onButton(press(Button::A));   // cursor 0
    const int fwd[] = {1, 2, 3, 4, 5, 6, 7, 0};   // A wraps 4->5 and 8->1
    for (int e : fwd) { g.onButton(press(Button::A)); CHECK(g.cursor() == e); }
    const int rev[] = {7, 6, 5, 4, 3, 2, 1, 0};   // C is the exact mirror
    for (int e : rev) { g.onButton(press(Button::C)); CHECK(g.cursor() == e); }
}

// Focus is dual-coded: the UI_CURSOR_BOX (shape) reads in grayscale, and (in
// IconsLabel mode) the focused slot's own icon swaps for its text label —
// in place, not a caption floated over the living area.
static void test_carousel_focus_grayscale() {
    Game g{StartMode::Hatched};
    g.onButton(press(Button::A));   // carousel @ slot 0 (top-left)
    Framebuffer fb(kActiveW, kActiveH);
    g.render(fb);
    // Box top-edge pixel of the focused slot (accent) vs the same point at the
    // unfocused neighbour (bare track band) — separable without colour.
    float lit = luminance(fb.get(12, 3));   // focused box border
    float dim = luminance(fb.get(68, 3));   // slot-1 track margin, no box
    CHECK(lit - dim > 0.3f);

    // The focused slot (slot 0, x in [0, kSlotW)) should render identically to
    // TextOnly there (both show text) and differently from IconsOnly (which
    // keeps the icon).
    g.setUiMode(UiMode::TextOnly);
    Framebuffer txt(kActiveW, kActiveH);
    g.render(txt);
    g.setUiMode(UiMode::IconsOnly);
    Framebuffer ico(kActiveW, kActiveH);
    g.render(ico);

    bool matchesText = true, matchesIcon = true;
    for (int y = 0; y < kTrackH; ++y)
        for (int x = 0; x < kSlotW; ++x) {
            if (fb.get(x, y) != txt.get(x, y)) matchesText = false;
            if (fb.get(x, y) != ico.get(x, y)) matchesIcon = false;
        }
    CHECK(matchesText);
    CHECK(!matchesIcon);
}

// Enter routes through the slot table; back restores the entered slot.
static void test_carousel_enter_back() {
    Game g{StartMode::Hatched};
    g.onButton(press(Button::A));   // carousel @ STAT
    g.onButton(press(Button::B));   // enter STAT submenu (lands on VITALS)
    CHECK(g.nav() == Game::Nav::Submenu);
    // The vitals page == the standalone STAT render (same viewer, now reached via nav).
    Framebuffer got(kActiveW, kActiveH), ref(kActiveW, kActiveH);
    g.render(got);
    drawStatScreen(ref, g.model(), "Paypup", Stage::Process, g.generation(),
                   g.combatLevel(), g.combatXp(), g.xpToNextLevel(), 0,
                   g.hasNextEvolution(), g.evolveRemainMs());
    bool same = true;
    for (int y = 0; y < kActiveH && same; ++y)
        for (int x = 0; x < kActiveW; ++x)
            if (got.get(x, y) != ref.get(x, y)) { same = false; break; }
    CHECK(same);
    g.onButton(press(Button::C));   // back -> carousel, restores slot 1
    CHECK(g.nav() == Game::Nav::Cursor && g.cursor() == 0);

    // Enter another slot from a different cursor; back restores THAT slot.
    g.onButton(press(Button::A));   // -> slot 2 (ITEMS)
    g.onButton(press(Button::A));   // -> slot 3 (TRAIN — a real list shell now)
    g.onButton(press(Button::B));
    CHECK(g.nav() == Game::Nav::Submenu);
    Framebuffer ph(kActiveW, kActiveH);
    g.render(ph);
    CHECK(anyNonPaper(ph, 0, 0, kActiveW, kActiveH));   // submenu renders
    g.onButton(press(Button::C));
    CHECK(g.nav() == Game::Nav::Cursor && g.cursor() == 2);
}

// The IconsLabel text swap renders in the focused slot's own track row, so a
// bottom-row cursor never drops label text into the living area onto the pet.
static void test_caption_pinned_top() {
    Game g{StartMode::Hatched};
    g.onButton(press(Button::C));   // summon @ slot 8 (bottom row)
    CHECK(g.cursor() == kCarouselSlots - 1);
    g.setUiMode(UiMode::IconsLabel);
    Framebuffer fb(kActiveW, kActiveH);
    g.render(fb);
    CHECK(!anyNonPaper(fb, 0, kLivingTop, kActiveW, kLivingTop + 12));  // no floating text
    CHECK(anyNonPaper(fb, 0, kLivingBottom, kActiveW, kActiveH));       // swap in bottom track
}

// One global 5s timer collapses the whole tree; any press resets it.
static void test_carousel_autodefocus() {
    Game g{StartMode::Hatched};
    g.onButton(press(Button::A));        // summon at t=0 (no tick yet)
    g.tick(kAutoDefocusMs - 250);        // just under 5s -> still summoned
    CHECK(g.nav() == Game::Nav::Cursor);
    g.tick(kAutoDefocusMs);              // 5s of silence -> idle
    CHECK(g.nav() == Game::Nav::Idle);

    Game r{StartMode::Hatched};
    r.onButton(press(Button::A));
    r.tick(4000);                        // advances time, no defocus
    r.onButton(press(Button::A));        // press resets the timer (stamped @4000)
    r.tick(8000);                        // 4s since reset -> still summoned
    CHECK(r.nav() == Game::Nav::Cursor);
    r.tick(9001);                        // >5s since reset -> idle
    CHECK(r.nav() == Game::Nav::Idle);
}

// Each UI Mode renders; none float text into the living area, and IconsLabel
// swaps only the focused slot's icon for text (verified pixel-exact against
// TextOnly/IconsOnly in test_carousel_focus_grayscale).
static void test_carousel_ui_modes() {
    Game g{StartMode::Hatched};
    g.onButton(press(Button::A));
    const int capTop = kLivingTop, capBot = kLivingTop + 12;

    g.setUiMode(UiMode::IconsLabel);
    Framebuffer lbl(kActiveW, kActiveH); g.render(lbl);
    CHECK(!anyNonPaper(lbl, 0, capTop, kActiveW, capBot));  // no floating caption
    CHECK(anyNonPaper(lbl, 0, 0, kActiveW, kTrackH));       // slot 0 text + other icons render

    g.setUiMode(UiMode::IconsOnly);
    Framebuffer ico(kActiveW, kActiveH); g.render(ico);
    CHECK(!anyNonPaper(ico, 0, capTop, kActiveW, capBot));
    CHECK(anyNonPaper(ico, 0, 0, kActiveW, kTrackH));       // icons still render

    g.setUiMode(UiMode::TextOnly);
    Framebuffer txt(kActiveW, kActiveH); g.render(txt);
    CHECK(!anyNonPaper(txt, 0, capTop, kActiveW, capBot));
    CHECK(anyNonPaper(txt, 0, 0, kActiveW, kTrackH));       // text slots render
}

// ===========================================================================
// The raising loop (ITEMS · MAINT · decay · Lockout)
// ===========================================================================

// Mirrors items_screen.cpp's content-area top (kRowTop) for pixel assertions.
static constexpr int kItemsRowTop = 26;

// Walk the carousel cursor from idle to the slot routing to `id`, then enter it.
static void enterSlot(Game& g, SubmenuId id) {
    g.onButton(press(Button::A));                     // idle A -> cursor @ slot 0
    while (carouselSlots()[g.cursor()].id != id)
        g.onButton(press(Button::A));
    g.onButton(press(Button::B));                     // B -> enter the submenu
}

static void test_inventory() {
    Inventory inv;
    CHECK(inv.count("airgap_snack") == 0 && !inv.has("airgap_snack"));
    inv.add("airgap_snack", 2);
    inv.add("airgap_snack");                          // default +1
    CHECK(inv.count("airgap_snack") == 3 && inv.has("airgap_snack"));
    CHECK(inv.remove("airgap_snack", 1));
    CHECK(inv.count("airgap_snack") == 2);
    CHECK(!inv.remove("airgap_snack", 5));            // insufficient -> no-op
    CHECK(inv.count("airgap_snack") == 2);

    Inventory s = Inventory::starting();
    CHECK(s.count("airgap_snack") == 3 && s.count("backup_drive") == 1);
    CHECK(s.count("decryptogram") == 1);             // egg accelerator
}

static void test_event_log() {
    EventLog log;
    CHECK(log.size() == 0);
    log.push(LogEventType::ItemUsed, "FED A");
    log.push(LogEventType::CareMistake, "FAILED DEFRAG");
    CHECK(log.size() == 2);
    CHECK(log.at(0).type == LogEventType::CareMistake);   // newest first
    CHECK(std::strcmp(log.at(1).text, "FED A") == 0);
    // Ring eviction: capacity is kept, oldest falls off.
    for (int i = 0; i < EventLog::kCapacity + 3; ++i)
        log.push(LogEventType::ItemGained, "X");
    CHECK(log.size() == EventLog::kCapacity);
}

// Grouped inventory list: section headers per group, cursor skips them, wraps.
static void test_inventory_rows_grouped() {
    ContentRegistry r = ContentRegistry::embedded();
    Inventory inv = Inventory::starting();           // 7 items across 3 groups
    auto rows = buildInventoryRows(r, inv, false);

    int headers = 0, items = 0;
    for (const auto& row : rows) (row.header ? headers : items)++;
    CHECK(items == 7 && headers == 3);               // FOOD/BUFFS/QUEST
    CHECK(rows.front().header && std::strcmp(rows.front().label, "FOOD") == 0);

    const int f = firstSelectableRow(rows);
    CHECK(f >= 0 && !rows[f].header);
    // Stepping never lands on a header; one lap over the selectable items wraps
    // back to the start.
    int cur = f;
    for (int i = 0; i < items; ++i) {
        cur = stepSelectableRow(rows, cur, +1);
        CHECK(!rows[cur].header);
    }
    CHECK(cur == f);                                 // full wrap returns to start
}

// Within a group the list reads richest-first, so the top of a block is always the
// best thing the bag holds of that type.
static void test_inventory_rows_rarity_desc() {
    // Four Foods whose alphabetical order is the REVERSE of their rarity order, so
    // "sorted by rarity" and "sorted by name" can't both pass.
    ContentRegistry r = ContentRegistry::embedded();
    Inventory inv;
    inv.add("breadcrumbs");            // Common
    inv.add("java");                   // Uncommon
    inv.add("applets");                // Uncommon
    inv.add("salted_hashed_browns");   // Rare
    auto rows = buildInventoryRows(r, inv, false);

    const char* const kExpected[] = {"Salted&Hashed Browns", "Applets", "Java",
                                     "Breadcrumbs"};
    int seen = 0;
    for (const auto& row : rows) {
        if (row.header) continue;
        CHECK(seen < 4 && std::strcmp(row.def->displayName, kExpected[seen]) == 0);
        ++seen;
    }
    CHECK(seen == 4);   // rarest first, then alphabetical between the two Uncommons
}

// >6 rows scrolls: the scrollbar column carries position without colour.
static void test_inventory_scrollbar() {
    ContentRegistry r = ContentRegistry::embedded();
    Inventory inv = Inventory::starting();
    auto rows = buildInventoryRows(r, inv, false);
    CHECK(static_cast<int>(rows.size()) > 6);        // 6 items + 3 headers = 9
    Framebuffer fb(kActiveW, kActiveH);
    drawItemsList(fb, rows, firstSelectableRow(rows), false, 0);
    CHECK(anyNonPaper(fb, kActiveW - 3, kItemsRowTop, kActiveW, kActiveH));
}

static void test_feed_and_maint_model() {
    PetModel m;
    m.setHunger(40);
    m.feed(40, 0);
    CHECK(m.hunger() == 80);                          // food restores Hunger
    m.feed(80, 5);
    CHECK(m.hunger() == 100 && m.happiness() == kStartHappiness + 5);  // clamps

    // NOTE: the +1 care mistake on a maint FAILURE now lands Game-side (via the shielded
    // path — see test_restore_shield_covers_maint_fail); PetModel only reports the failure
    // (returns true) and applies the frag penalty. So these model-level checks assert frag
    // + the returned failure flag, NOT the mistake counter.
    PetModel d;
    d.setFragmentation(50);
    CHECK(!d.applyDefrag(true));
    CHECK(d.fragmentation() == 50 - kDefragReduction && d.careMistakes() == 0);
    CHECK(d.applyDefrag(false));
    CHECK(d.fragmentation() == 30 + kMaintFailPenalty && d.careMistakes() == 0);

    PetModel a;
    a.setFragmentation(50);
    a.setGhost(true);
    a.setDebuffs(1);
    CHECK(!a.applyAntivirus(true));
    CHECK(a.fragmentation() == 40 && !a.hasGhost() && a.debuffs() == 0);
    a.setFragmentation(50);
    CHECK(a.applyAntivirus(false));
    CHECK(a.fragmentation() == 65 && a.careMistakes() == 0);
}

// Feeding: ITEMS -> detail -> Use launches the modal, Hunger rises, count drops,
// and an ITEM_USED row is logged.
static void test_feeding_flow() {
    Game g{StartMode::Hatched};
    g.model().setHunger(40);
    enterSlot(g, SubmenuId::Items);
    CHECK(g.nav() == Game::Nav::Submenu);
    g.onButton(press(Button::B));                    // open first food's detail
    CHECK(g.nav() == Game::Nav::Detail);

    const int q0 = g.inventory().count("airgap_snack");
    g.onButton(press(Button::B));                    // Use -> feeding modal
    CHECK(g.nav() == Game::Nav::ModalFeeding);
    CHECK(g.model().hunger() == 80);                 // 40 + 40 restore
    CHECK(g.inventory().count("airgap_snack") == q0 - 1);
    CHECK(g.log().size() >= 1 && g.log().at(0).type == LogEventType::ItemUsed);

    g.onButton(press(Button::B));                    // dismiss -> back to detail
    CHECK(g.nav() == Game::Nav::Detail);
}

// Gated item: Use is inert outside its context (Decryption Key is Lockout-only).
static void test_item_context_gate() {
    Game g{StartMode::Hatched};
    enterSlot(g, SubmenuId::Items);
    // Walk to the Decryption Key row, then open + try to Use it.
    auto rows = buildInventoryRows(ContentRegistry::embedded(),
                                   g.inventory(), false);
    // The detail subject is driven by the engine's cursor; instead exercise the
    // pure gate predicate the engine uses.
    const ItemDef* key = ContentRegistry::embedded().item("decrypt_key");
    CHECK(key && key->context == ItemDef::Context::LockoutOnly);
    (void)rows;
}

// MAINT: list -> action -> non-interruptible process -> outcome changes Frag.
static void test_maint_flow() {
    Game g{StartMode::Hatched};
    g.model().setFragmentation(50);
    enterSlot(g, SubmenuId::Maint);
    CHECK(g.nav() == Game::Nav::Submenu);
    g.onButton(press(Button::B));                    // enter Defrag action
    CHECK(g.nav() == Game::Nav::Detail);
    g.onButton(press(Button::B));                    // Run -> process
    CHECK(g.nav() == Game::Nav::Process);

    // C is ignored while the process runs (non-interruptible).
    g.onButton(press(Button::C));
    CHECK(g.nav() == Game::Nav::Process);

    for (int i = 1; i <= kProcessBeats + 1; ++i)
        g.tick(static_cast<uint32_t>(i) * kHeartbeatMs);
    const int frag = g.model().fragmentation();
    CHECK(frag == 50 - kDefragReduction || frag == 50 + kMaintFailPenalty);
    g.onButton(press(Button::B));                    // dismiss outcome -> action
    CHECK(g.nav() == Game::Nav::Detail);
}

// a Defrag costs stage-scaled Bits (free to spam before). A Process pet
// (default Paypup) pays kDefragCostByStage[Process]; the wallet is debited on RUN.
static void test_defrag_costs_stage_bits() {
    Game g{StartMode::Hatched};                       // Paypup = Process stage
    const int cost = kDefragCostByStage[static_cast<int>(Stage::Process)];
    CHECK(g.defragCost() == cost);
    CHECK(cost > 0);                                   // defrag costs Bits
    g.model().setFragmentation(50);
    g.debugSetBits(cost + 3);
    enterSlot(g, SubmenuId::Maint);
    g.onButton(press(Button::B));                      // enter Defrag action
    g.onButton(press(Button::B));                      // RUN
    CHECK(g.nav() == Game::Nav::Process);              // launched
    CHECK(g.bits() == 3);                              // charged exactly `cost`
}

// an empty (or too-thin) wallet makes Defrag inert — RUN doesn't launch
// and no Bits move, even with Fragmentation to clear.
static void test_defrag_gated_when_broke() {
    Game g{StartMode::Hatched};
    g.model().setFragmentation(50);
    g.debugSetBits(g.defragCost() - 1);               // one short
    enterSlot(g, SubmenuId::Maint);
    g.onButton(press(Button::B));                      // enter Defrag action
    const int bits0 = g.bits();
    g.onButton(press(Button::B));                      // RUN -> inert
    CHECK(g.nav() == Game::Nav::Detail);              // stayed on the action screen
    CHECK(g.bits() == bits0);                          // no charge
    CHECK(g.model().fragmentation() == 50);            // nothing defragged
}

// two defrag variants. QUICK is Bits-only with the normal fail roll (covered
// by test_defrag_costs_stage_bits); the TOOL variant spends one Defrag Tool for a
// GUARANTEED clean, is inert with no tool held, and every successful defrag advances the
// per-pet tally.
static void test_defrag_variants() {
    Game g{StartMode::Hatched};                       // Paypup = Process stage
    g.model().setFragmentation(60);
    g.debugSetBits(999);
    enterSlot(g, SubmenuId::Maint);
    g.onButton(press(Button::B));                     // enter Defrag action
    CHECK(g.nav() == Game::Nav::Detail);
    g.onButton(press(Button::A));                     // switch to the TOOL variant
    const int bits0 = g.bits();
    g.onButton(press(Button::B));                     // RUN with no tool -> inert
    CHECK(g.nav() == Game::Nav::Detail);              // gated, stayed on the screen
    CHECK(g.bits() == bits0);                          // no charge when inert
    CHECK(g.defragCount() == 0);
    // Hold two tools, run the TOOL defrag -> guaranteed clean, one tool spent, tally++.
    g.inventory().add("disk_scrubber", 2);
    g.onButton(press(Button::B));                     // RUN (TOOL variant) -> process
    CHECK(g.nav() == Game::Nav::Process);
    uint32_t t = 0;
    for (int i = 1; i <= kProcessBeats + 1; ++i) g.tick(t += kHeartbeatMs);
    CHECK(g.model().fragmentation() == 60 - kDefragReduction);  // guaranteed success
    CHECK(g.inventory().count("disk_scrubber") == 1);           // exactly one consumed
    CHECK(g.bits() == bits0 - g.defragCost());                  // Bits still charged
    CHECK(g.defragCount() == 1);                                // the tally advanced
}

// the Defrag Tool can't be Used from the ITEMS path (a stray Use would burn
// it) — it's gated with a "use in MAINT" message.
static void test_defrag_tool_gated_in_items() {
    Game g{StartMode::Hatched};
    g.inventory().add("disk_scrubber", 1);
    g.debugUseItem("disk_scrubber");                  // real Use path (gate → no-op)
    CHECK(g.inventory().count("disk_scrubber") == 1); // not consumed (inert)
}

// the per-pet defrag tally stays consistent through ARCH freeze/thaw — a
// stored pet keeps its own count, and the original's count returns when redeployed.
static void test_defrag_count_freeze_thaw() {
    Game g{StartMode::Hatched};                       // active Paypup
    g.model().setFragmentation(60);
    g.debugSetBits(999);
    g.inventory().add("disk_scrubber", 1);
    enterSlot(g, SubmenuId::Maint);
    g.onButton(press(Button::B));                     // Defrag action
    g.onButton(press(Button::A));                     // TOOL variant
    g.onButton(press(Button::B));                     // RUN -> process (guaranteed)
    uint32_t t = 0;
    for (int i = 1; i <= kProcessBeats + 1; ++i) g.tick(t += kHeartbeatMs);
    CHECK(g.defragCount() == 1);                       // Paypup: one defrag
    g.onButton(press(Button::B));                      // dismiss outcome -> action
    g.onButton(press(Button::C));                      // action -> MAINT list
    g.onButton(press(Button::C));                      // list -> carousel
    g.onButton(press(Button::C));                      // carousel -> idle habitat

    g.debugSeedRack("cryptoshell");                    // a stored pet at rack row 1
    auto deployRow1 = [](Game& gg) {                   // Deploy the rack-row-1 pet
        enterSlot(gg, SubmenuId::Arch);
        gg.onButton(press(Button::A));                 // row 0 (active) -> row 1 (stored)
        gg.onButton(press(Button::B));                 // open record (Deploy default)
        gg.onButton(press(Button::B));                 // Deploy -> confirm
        gg.onButton(press(Button::A));                 // Cancel -> Confirm
        gg.onButton(press(Button::B));                 // commit
    };
    deployRow1(g);                                     // cryptoshell in, Paypup frozen
    CHECK(g.defragCount() == 0);                       // cryptoshell's own (fresh) tally
    deployRow1(g);                                     // Paypup back out of the rack
    CHECK(g.defragCount() == 1);                       // thawed Paypup's tally intact
}

// ARCH Release valve: a stored pet can be RELEASED — no reward, frees a rack
// slot. Cycle a stored record's actions Deploy -> Sell -> Release, then confirm.
static void test_arch_release_stored_frees_slot() {
    Game g{StartMode::Hatched};
    g.debugSeedRack("cryptoshell");                    // a stored pet at rack row 1
    CHECK(g.rackCount() == 1);
    enterSlot(g, SubmenuId::Arch);
    g.onButton(press(Button::A));                      // row 0 (active) -> row 1 (stored)
    g.onButton(press(Button::B));                      // open record (Deploy default)
    g.onButton(press(Button::A));                      // Deploy -> Sell
    g.onButton(press(Button::A));                      // Sell   -> Release
    g.onButton(press(Button::B));                      // Release -> confirm
    g.onButton(press(Button::A));                      // Cancel -> Confirm
    g.onButton(press(Button::B));                      // commit the release
    CHECK(g.rackCount() == 0);                          // slot freed, no record left
    CHECK(g.nav() == Game::Nav::Submenu);              // back to the (empty) rack list
}

// Lockout fires at Hunger 0; an unresolved expiry costs 2 care mistakes.
static void test_lockout_expire_mistakes() {
    Game g{StartMode::Hatched};
    g.model().setHunger(0);
    g.tick(kHeartbeatMs);
    CHECK(g.lockoutActive() && g.nav() == Game::Nav::ModalLockout);

    g.tick(kLockoutDurationMs + 2 * kHeartbeatMs);   // past the deadline
    CHECK(!g.lockoutActive() && g.nav() == Game::Nav::Idle);
    // A fully-failed Lockout still nets 2 total: +1 "went hungry" when Hunger hit 0
    // (fireLockout) + kLockoutExpiryMistakes on expiry (the split budget invariant).
    CHECK(g.model().careMistakes() == kWentHungryMistakes + kLockoutExpiryMistakes);
    CHECK(g.model().careMistakes() == 2);            // the invariant holds numerically
    CHECK(g.model().hunger() == kLockoutRecoveryHunger);
    CHECK(g.log().size() >= 1 && g.log().at(0).type == LogEventType::CareMistake);
}

// Restore Point shield: a shielded pet ignores the NEXT positive care mistake, then
// takes the following one. Covers the went-hungry increment path (generic interception).
static void test_restore_point_shield_blocks_next_mistake() {
    Game g{StartMode::Hatched};
    CHECK(g.inventory().count("restore_point") == 1);
    g.debugUseItem("restore_point");                 // arm the shield (once-per-lifetime)
    CHECK(g.inventory().count("restore_point") == 0);
    CHECK(g.model().careMistakes() == 0);

    // First failed Lockout: the "went hungry" +1 is BLOCKED by the shield; expiry +1
    // lands (shield already spent). Net = kLockoutExpiryMistakes, not 2.
    g.model().setHunger(0);
    g.tick(kHeartbeatMs);
    g.tick(kLockoutDurationMs + 2 * kHeartbeatMs);
    CHECK(g.model().careMistakes() == kLockoutExpiryMistakes);   // one mistake shielded

    // Second failed Lockout: shield is gone, so both halves land (2 more).
    const int before = g.model().careMistakes();
    g.model().setHunger(0);
    g.tick(kHeartbeatMs);
    g.tick(kLockoutDurationMs + 2 * kHeartbeatMs);
    CHECK(g.model().careMistakes() ==
          before + kWentHungryMistakes + kLockoutExpiryMistakes);
}

// Restore Point is once-per-lifetime: after its shield is spent, a second Use never
// re-arms (the gate flag is consumed for the pet's whole life).
static void test_restore_point_once_per_lifetime() {
    Game g{StartMode::Hatched};
    g.debugUseItem("restore_point");                 // arm + spend the once-per-life gate
    // Spend the shield on a went-hungry mistake (blocked; net 0 from this half).
    g.model().setHunger(0);
    g.tick(kHeartbeatMs);
    CHECK(g.model().careMistakes() == 0);            // shielded — the went-hungry blocked
    g.tick(kLockoutDurationMs + 2 * kHeartbeatMs);   // let the Lockout expire cleanly
    const int after = g.model().careMistakes();      // expiry mistake stands (shield gone)

    // Grant + Use another copy: the gate is spent, so no shield re-arms.
    g.inventory().add("restore_point", 1);
    g.debugUseItem("restore_point");                 // consumed as a buff, but NO re-arm
    g.model().setHunger(0);
    g.tick(kHeartbeatMs);
    CHECK(g.model().careMistakes() == after + kWentHungryMistakes);  // not re-shielded
}

// The Restore Point shield covers the +1 care mistake from a FAILED Defrag/AV
// (the maint-fail mistake routes through resolveMaint -> addCareMistakeShielded).
// An armed shield eats the first failed defrag's mistake; a second failure then
// lands normally.
static void test_restore_shield_covers_maint_fail() {
    Game g{StartMode::Hatched};
    g.debugUseItem("restore_point");                 // arm the shield
    CHECK(g.model().careMistakes() == 0);

    const int frag0 = g.model().fragmentation();
    g.debugResolveDefrag(false);                     // first failed defrag → mistake SHIELDED
    CHECK(g.model().careMistakes() == 0);            // no mistake (shield consumed)
    CHECK(g.model().fragmentation() == frag0 + kMaintFailPenalty);  // frag penalty still lands

    g.debugResolveDefrag(false);                     // second failure → mistake lands
    CHECK(g.model().careMistakes() == 1);
}

// Yubi-Cookie removes one existing mistake, ONCE PER LIFETIME: a second Use is inert.
static void test_yubi_cookie_once_per_lifetime() {
    Game g{StartMode::Hatched};
    g.model().addCareMistake(2);                     // two mistakes to shave
    CHECK(g.inventory().count("yubi_cookie") == 1);
    g.debugUseItem("yubi_cookie");                   // -1 mistake (first use)
    CHECK(g.model().careMistakes() == 1);
    g.inventory().add("yubi_cookie", 1);             // grant another copy
    g.debugUseItem("yubi_cookie");                   // already consumed -> no -1
    CHECK(g.model().careMistakes() == 1);            // unchanged
}

// Backup Drive is a combat BUFF, not a Lockout item: Use arms a timed shield
// (Game::backupShieldArmed) instead of touching the Lockout at all.
static void test_backup_drive_arms_and_lapses() {
    Game g{StartMode::Hatched};
    CHECK(!g.backupShieldArmed());
    g.debugUseItem("backup_drive");
    CHECK(g.inventory().count("backup_drive") == 0);       // consumed on Use
    CHECK(g.backupShieldArmed());
    // Ticking less than the 1h window leaves it armed; crossing it lapses it unused
    // (the item row's magnitude — content_items.cpp's ArmCombatShieldBuff — is 60 min).
    // Game::tick(nowMs) takes an ABSOLUTE clock reading, not a delta.
    const uint32_t oneHourMs = 60u * 60u * 1000u;
    g.tick(oneHourMs / 2);
    CHECK(g.backupShieldArmed());
    g.tick(oneHourMs + kHeartbeatMs);
    CHECK(!g.backupShieldArmed());
}


// A use that can achieve literally nothing is refused and KEEPS the item, rather
// than spending it on a state the pet already holds. itemUsable gates on
// Game::itemUseIsInert, which reads the ItemEffect vocabulary — so every arming
// effect gets the same protection without a per-id branch.
static void test_inert_use_keeps_the_item() {
    Game g{StartMode::Hatched};
    g.inventory().add("backup_drive", 2);
    g.debugUseItem("backup_drive");
    CHECK(g.backupShieldArmed());
    const int armed = g.inventory().count("backup_drive");
    // A second over a live shield would only replace a deadline it already has.
    g.debugUseItem("backup_drive");
    CHECK(g.inventory().count("backup_drive") == armed);
    // Once the window lapses there is something to arm again, so it spends.
    g.tick(60u * 60u * 1000u + kHeartbeatMs);
    CHECK(!g.backupShieldArmed());
    g.debugUseItem("backup_drive");
    CHECK(g.backupShieldArmed());
    CHECK(g.inventory().count("backup_drive") == armed - 1);

    // Same rule off a different state: the Yubi-Cookie is once per lifetime, so
    // the second one is refused instead of vanishing for nothing.
    g.model().setCareMistakes(2);
    g.inventory().add("yubi_cookie", 2);
    g.debugUseItem("yubi_cookie");
    const int spent = g.inventory().count("yubi_cookie");
    g.debugUseItem("yubi_cookie");
    CHECK(g.inventory().count("yubi_cookie") == spent);
}

// A Rig Shop row with a prerequisite (RigUpgradeDef::requiresRow) isn't on sale
// until that row is owned: the recipes need the MERGE HUB they'd be cooked in.
// The SHOP list and the buy path share one predicate (shopRowOffered), so
// refusing the purchase and hiding the row are the same fact asserted once.
static void test_recipe_rows_wait_on_the_merge_hub() {
    Game g{StartMode::Hatched};
    g.debugSetBits(99999);
    const int start = g.bits();
    g.debugBuyRecipe(0);
    CHECK(g.bits() == start);           // gated: nothing bought, nothing charged
    CHECK(!g.mergeHubUnlocked());

    g.debugBuyMergeHub();
    CHECK(g.mergeHubUnlocked());
    CHECK(g.bits() == start - kRigMergeHubCost);
    g.debugBuyRecipe(0);                // now offered, so it sells
    CHECK(g.bits() == start - kRigMergeHubCost - kRigRecipeUnlockCost);
}

// The second gate axis (RigUpgradeDef::requiresItems): the Browns recipes want the
// operator to have HELD both dishes, not just to own the hub. It's an ever-held
// record (Game::itemCollected), so meeting a dish and then eating it still counts —
// which is the whole reason the gate reads that and not the current bag.
static void test_browns_recipes_wait_on_meeting_both_dishes() {
    Game g{StartMode::Hatched};
    g.debugSetBits(99999);
    g.debugBuyMergeHub();

    int brownsRecipe = -1;
    for (int i = 0; i < kMergeRecipeCount; ++i)
        if (std::strcmp(kMergeRecipes[i].outputId, "hashed_browns") == 0) brownsRecipe = i;
    CHECK(brownsRecipe >= 0);

    // Whether the row actually sold, measured across the call — an absolute Bits
    // figure would drift the moment a tick pays out an achievement.
    auto sold = [&] {
        const int before = g.bits();
        g.debugBuyRecipe(brownsRecipe);
        return g.bits() != before;
    };
    // The collected-items record is folded in by the achievement sweep, which is
    // throttled — walk the clock past its interval rather than by a millisecond.
    // Game::tick takes an ABSOLUTE timestamp, so the cursor has to advance.
    uint32_t now = 0;
    auto settle = [&] { now += kAchSweepIntervalMs + 1; g.tick(now); };

    CHECK(!sold());                            // never met either dish -> not on sale

    // One of the two isn't enough: the row names both.
    g.inventory().add("hashed_browns", 1);
    settle();
    CHECK(g.itemCollected("hashed_browns"));
    CHECK(!sold());

    g.inventory().add("salted_hashed_browns", 1);
    settle();
    CHECK(g.itemCollected("salted_hashed_browns"));
    // Eat the evidence — a met dish stays met.
    g.inventory().remove("hashed_browns", 1);
    g.inventory().remove("salted_hashed_browns", 1);
    CHECK(sold());
}

// A recipe consumes every ingredient it names, not just the first two: Hashed Browns
// takes four. The craft is gated on ALL of them, so being short on the last one is
// refused exactly like being short on the first.
static void test_four_ingredient_recipe_consumes_all_inputs() {
    Game g{StartMode::Hatched};
    int idx = -1;
    for (int i = 0; i < kMergeRecipeCount; ++i)
        if (std::strcmp(kMergeRecipes[i].outputId, "hashed_browns") == 0) idx = i;
    CHECK(idx >= 0);
    const MergeRecipe& r = kMergeRecipes[idx];
    CHECK(recipeInputCount(r) == 4);

    g.debugSetBits(99999);
    g.debugBuyMergeHub();
    g.inventory().add("hashed_browns", 1);
    g.inventory().add("salted_hashed_browns", 1);
    g.tick(kAchSweepIntervalMs + 1);   // the sweep records both as met
    g.debugBuyRecipe(idx);

    // Three of the four in the bag: still refused, nothing consumed.
    for (int i = 0; i < 3; ++i) g.inventory().add(r.inputs[i].id, r.inputs[i].qty);
    CHECK(!g.debugRecipeCraftable(idx));
    g.debugCraftRecipe(idx);
    CHECK(g.inventory().count(r.inputs[0].id) == r.inputs[0].qty);

    const int before = g.inventory().count("hashed_browns");
    g.inventory().add(r.inputs[3].id, r.inputs[3].qty);
    CHECK(g.debugRecipeCraftable(idx));
    g.debugCraftRecipe(idx);
    CHECK(g.inventory().count("hashed_browns") == before + r.outputQty);
    for (const RecipeInput& in : r.inputs)
        if (in.id) CHECK(g.inventory().count(in.id) == 0);   // all four spent
}

// Polltatoes (ItemEffect::Kind::HungerStacking): a run of stacking foods pays
// magnitude x how many have been eaten since the last Hunger point lost to decay,
// counting the current bite. Eaten alone it is worth exactly its magnitude.
static void test_stacking_food_run_climbs_then_resets_on_decay() {
    Game g{StartMode::Hatched};
    g.inventory().add("polltatoes", 8);
    // Well clear of the 100 ceiling so every gain is observable, and clear of the
    // Lockout floor so the feed path isn't resolving a crisis instead.
    g.model().setHunger(40);

    int last = g.model().hunger();
    for (int expect = 1; expect <= 3; ++expect) {
        g.debugUseItem("polltatoes");
        CHECK(g.model().hunger() - last == expect);   // 1, then 2, then 3
        last = g.model().hunger();
    }

    // A Hunger point lost to passive decay ends the run — the next one is worth 1
    // again, not 5. Long enough for at least one decay step, and we re-level Hunger
    // afterwards so the assertion measures the bite alone.
    g.tick(kHungerMinutesPerPoint * 60u * 1000u);
    g.model().setHunger(40);
    g.debugUseItem("polltatoes");
    CHECK(g.model().hunger() - 40 == 1);
}

// Every id named in a loot/reward pool has to resolve to a real item with a positive
// draw weight. A typo'd id would otherwise cost that entry silently — the pool would
// still draw, just never that row — which is exactly the kind of failure a weighted
// table makes invisible.
static void test_loot_pools_resolve_and_carry_weight() {
    ContentRegistry r = ContentRegistry::embedded();
    auto checkPool = [&r](const LootEntry* pool, int n) {
        for (int i = 0; i < n; ++i) {
            const ItemDef* d = r.item(pool[i].id);
            if (!d) std::printf("  UNRESOLVED POOL ENTRY: %s\n", pool[i].id);
            CHECK(d != nullptr);
            CHECK((pool[i].weight > 0 ? pool[i].weight : itemDropWeight(*d)) > 0);
        }
    };
    checkPool(kLootPool, kLootPoolCount);
    for (const ItemDef* c : r.allItems()) checkPool(c->cache.pool, c->cache.poolSize);

    // The resolver's two arms: a row that names its own weight reports it; one that
    // doesn't falls back to its rarity's default.
    CHECK(itemDropWeight(*r.item("spam")) == 90);          // names its own
    CHECK(itemDropWeight(*r.item("boolean_cubes")) ==
          kItemRarityDropWeight[static_cast<int>(ItemDef::Rarity::Common)]);
}

// Resolving by paying Bits clears the crisis before the timer expires.
static void test_lockout_resolve_pay() {
    Game g{StartMode::Hatched};
    g.model().setHunger(0);
    g.tick(kHeartbeatMs);
    CHECK(g.bits() == kStartBits);
    g.onButton(press(Button::A));                    // toggle to Pay Bits
    g.onButton(press(Button::B));                    // pay
    CHECK(!g.lockoutActive() && g.nav() == Game::Nav::Idle);
    // Resolving also EARNS SURVIVED_LOCKOUT, which pays its own Bits — read the amount
    // off the achievement row rather than restating it, so retuning the reward doesn't
    // break this gate.
    CHECK(g.bits() == kStartBits - kLockoutBitsCost + achBitsReward(ach::kSurvivedLockout));
    CHECK(g.model().hunger() == kLockoutRecoveryHunger);
}

// Resolving via the Lockout ITEMS escape hatch (feed a food).
static void test_lockout_resolve_feed() {
    Game g{StartMode::Hatched};
    g.model().setHunger(0);
    g.tick(kHeartbeatMs);
    CHECK(g.nav() == Game::Nav::ModalLockout);
    g.onButton(press(Button::B));                    // "Open Items" (lockout ctx)
    CHECK(g.nav() == Game::Nav::Submenu);
    g.onButton(press(Button::B));                    // open the top food detail
    CHECK(g.nav() == Game::Nav::Detail);
    g.onButton(press(Button::B));                    // Use food -> feeding modal
    CHECK(g.nav() == Game::Nav::ModalFeeding);
    CHECK(!g.lockoutActive());                        // the crisis is already off
    // The deadline is disarmed: ticking well past it adds NO further mistake. The one
    // "went hungry" +1 (charged when Hunger hit 0) stands — the intended new cost.
    g.tick(kLockoutDurationMs + 10000);
    CHECK(g.model().careMistakes() == kWentHungryMistakes);
    g.onButton(press(Button::B));                    // dismiss -> resolves lockout
    CHECK(!g.lockoutActive() && g.nav() == Game::Nav::Idle);
    CHECK(g.model().hunger() > 0);
}

// Grayscale gate: the focused ITEMS row's cursor marker reads without colour.
static void test_items_grayscale() {
    ContentRegistry r = ContentRegistry::embedded();
    Inventory inv = Inventory::starting();
    auto rows = buildInventoryRows(r, inv, false);
    const int cur = firstSelectableRow(rows);        // first FOOD row (v=1)
    Framebuffer fb(kActiveW, kActiveH);
    drawItemsList(fb, rows, cur, false, 0);
    // The cursor triangle (accent) sits at x~9 of the focused row; an unfocused
    // item row has only paper there. Separable by luminance alone.
    const int focusY = kItemsRowTop + 1 * 28 + 13;   // row v=1, mid-triangle
    const int plainY = kItemsRowTop + 3 * 28 + 13;   // row v=3, no marker
    CHECK(luminance(fb.get(9, focusY)) - luminance(fb.get(9, plainY)) > 0.3f);
}

// Forward decls: the egg-at-idle gates below reuse these carousel/render helpers
// that are defined further down (with the other submenu tests).
static void enterSubmenuId(Game& g, SubmenuId id);
static bool hasDarkInk(const Framebuffer& fb, int x0, int y0, int x1, int y1);

// Decryption Hatch (egg-at-idle) ------------------------------

// Fresh boot now opens the line-select modal when >1 egg line is unlocked
// (Ransomware + Phishing). These gates predate line-select, so pick the first line
// (Ransomware) to lay its egg and match the historical straight-to-egg behaviour. A
// no-op when the modal isn't up (e.g. a save-store boot that loaded a live pet).
static void pickFirstEggLine(Game& g) {
    if (g.inLineSelect()) g.onButton(press(Button::B));
}

// Fresh save LAYS THE EGG at idle (not a blocking modal): the Boot-Sector creature
// is on-screen and interactable, incubating over kBootHatchMs. Line-select
// auto-skips with one line unlocked. The egg counts as generation 1 at once.
static void test_hatch_lays_egg_at_idle() {
    Game g;                                   // default = FreshHatch
    pickFirstEggLine(g);                      // >1 line unlocked -> pick Ransomware
    CHECK(g.nav() == Game::Nav::Idle);        // egg sits at idle, no modal
    CHECK(g.pet() != nullptr);
    CHECK(std::strcmp(g.pet()->id, "cryptoshell") == 0);
    CHECK(g.pet()->stage == Stage::BootSector);
    CHECK(g.inEggPhase());
    CHECK(g.bootHatchRemainMs() == kBootHatchMs);
    CHECK(!g.hatchMinigameReady());           // first half: not decryptable yet
    CHECK(g.generation() == 1);               // the egg is laid = generation 1
}

// The decrypt minigame is gated to the SECOND HALF of incubation: not available
// while >half the clock remains, available once <= half. B on idle opens it then.
static void test_hatch_minigame_second_half_gate() {
    Game g;
    pickFirstEggLine(g);
    uint32_t t = 0;
    g.tick(t += 1000);
    CHECK(!g.hatchMinigameReady());
    g.onButton(press(Button::B));             // idle B is inert in the first half
    CHECK(g.nav() == Game::Nav::Idle);
    g.tick(t += kBootHatchMs / 2);            // cross halfway
    CHECK(g.hatchMinigameReady());
    CHECK(g.inEggPhase());                    // still an egg until it's decrypted
    g.onButton(press(Button::B));             // idle B now opens the decrypt modal
    CHECK(g.nav() == Game::Nav::ModalHatch);
    CHECK(g.hatchDeadlineMs() == t + kHatchDurationMs);
}

// Inside the decrypt modal, any A/B/C press cuts kHatchPressReductionMs off
// the deadline (C is a press here, not "back").
static void test_hatch_press_subtracts_minute() {
    Game g;
    pickFirstEggLine(g);
    uint32_t t = 0;
    g.tick(t += 1000);
    g.tick(t += kBootHatchMs / 2);
    g.onButton(press(Button::B));             // open the decrypt modal
    uint32_t d0 = g.hatchDeadlineMs();
    g.onButton(press(Button::A));
    CHECK(g.hatchDeadlineMs() == d0 - kHatchPressReductionMs);
    uint32_t d1 = g.hatchDeadlineMs();
    g.onButton(press(Button::C));             // C counts as a press here too
    CHECK(g.hatchDeadlineMs() == d1 - kHatchPressReductionMs);
}

// Completing the decrypt (enough presses to exhaust the deadline) hatches
// STRAIGHT TO PROCESS (Paypup) — the egg/Boot-Sector shell is skipped
// visually, the whole point of the redesign.
static void test_hatch_decrypt_to_process() {
    Game g;
    pickFirstEggLine(g);
    uint32_t t = 0;
    g.tick(t += 1000);
    g.tick(t += kBootHatchMs / 2);            // reach the second half
    g.onButton(press(Button::B));             // open the decrypt modal
    CHECK(g.nav() == Game::Nav::ModalHatch);
    const int presses = kHatchDurationMs / kHatchPressReductionMs;
    for (int i = 0; i < presses; ++i) g.onButton(press(Button::B));   // mash -> hatch
    CHECK(g.nav() == Game::Nav::Idle);
    CHECK(g.pet() != nullptr);
    CHECK(g.pet()->stage == Stage::Process);  // hatched to a (random) Process, not CryptoShell
    CHECK(std::strcmp(g.pet()->line, "ransomware") == 0);  // from the Ransomware egg pool
    CHECK(!g.inEggPhase());
    CHECK(g.model().hunger() == kStartHunger);          // fresh Process vitals
}

// Crack frame maps to elapsed fraction across the egg's frames, inside the modal.
static void test_hatch_crack_frame_mapping() {
    Game g;
    pickFirstEggLine(g);
    uint32_t t = 0;
    g.tick(t += 1000);
    g.tick(t += kBootHatchMs / 2);
    g.onButton(press(Button::B));             // open modal; deadline = t + dur
    CHECK(g.hatchCrackFrame() == 0);          // progress 0 -> first frame
    g.tick(t += kHatchDurationMs / 2);        // ~50% -> mid frame
    CHECK(g.inHatch());
    CHECK(g.hatchCrackFrame() == 2);          // (int)(0.5 * 4)
    g.tick(t += kHatchDurationMs / 2 - 1000); // ~99.7% -> last frame, not yet hatched
    CHECK(g.inHatch());
    CHECK(g.hatchCrackFrame() == 3);
}

// Waiting out the full incubation (never opening the minigame) auto-hatches on its
// own — straight to Process, no soft-lock.
static void test_hatch_waits_out() {
    Game g;
    pickFirstEggLine(g);
    uint32_t t = 0;
    g.tick(t += 1000);
    g.tick(t += kBootHatchMs + kHeartbeatMs);
    CHECK(g.nav() == Game::Nav::Idle);
    CHECK(g.pet() != nullptr && g.pet()->stage == Stage::Process);
    CHECK(!g.inEggPhase());
}

// A newly-seen NETWORK shaves kBootHatchNetworkAccelMs off the incubation clock.
// An egg can't explore, so the network seam is the only hatch accelerator.
static void test_hatch_network_accelerates() {
    Game g;
    pickFirstEggLine(g);
    uint32_t t = 0;
    g.tick(t += 1000);
    CHECK(g.inEggPhase());
    const uint32_t before = g.bootHatchRemainMs();
    const uint8_t bssid[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01};
    CHECK(g.registerNetwork(bssid, "TestNet"));   // freshly queued -> accelerates
    CHECK(g.bootHatchRemainMs() == before - kBootHatchNetworkAccelMs);
}

// Vitals are frozen while the pet is still an egg (an egg doesn't get hungry): a
// long idle stretch doesn't move Hunger, but resumes decaying once hatched.
static void test_hatch_egg_vitals_frozen() {
    Game g;
    pickFirstEggLine(g);
    uint32_t t = 0;
    g.tick(t += 1000);
    const int h0 = g.model().hunger();
    g.tick(t += kBootHatchMs / 4);            // well within the first half
    CHECK(g.inEggPhase());
    CHECK(g.model().hunger() == h0);          // frozen — no decay while an egg
}

// Grayscale gate: the egg-at-idle reads without colour (egg silhouette + the
// incubation prompt ink).
static void test_hatch_grayscale() {
    Game g;
    pickFirstEggLine(g);
    g.tick(1000);
    Framebuffer fb(kActiveW, kActiveH);
    g.render(fb);
    CHECK(anyNonPaper(fb, 60, 40, kActiveW - 60, 184));  // egg silhouette present
    CHECK(hasDarkInk(fb, 0, kLivingTop, kActiveW, kLivingTop + 16));  // prompt ink
}

// The Hatched seam (used by the tests) skips the egg entirely.
static void test_hatch_seam_skips() {
    Game g{StartMode::Hatched};
    CHECK(g.nav() == Game::Nav::Idle);
    CHECK(g.pet() != nullptr);
    CHECK(!g.inHatch());
    CHECK(!g.inEggPhase());                   // a seam pet is already hatched
}

// A chord press edge (both A+C held) — the pet-side Exploit gesture.
static mal::ButtonEvent chordAC() { return {Button::C, true, true}; }

// Move the carousel cursor onto the slot routing to `id` WITHOUT entering it
// (unlike enterSubmenuId, which presses B). Used to probe locked slots.
static void cursorToSlot(Game& g, SubmenuId id) {
    g.onButton(press(Button::A));                 // idle A -> carousel
    while (carouselSlots()[g.cursor()].id != id)
        g.onButton(press(Button::A));
}

// Egg-phase menu lock: TRAIN/MAINT/MODS/EXPL are inert while the
// pet is an egg (B on them does not open the submenu) — an egg can't explore. Only
// STAT/ITEMS stay reachable. Once hatched, every slot enters normally.
static void test_egg_menu_locks() {
    Game g;                                       // FreshHatch -> egg at idle
    pickFirstEggLine(g);
    CHECK(g.inEggPhase());
    for (SubmenuId locked : {SubmenuId::Train, SubmenuId::Maint, SubmenuId::Mods,
                             SubmenuId::Expl}) {
        cursorToSlot(g, locked);
        CHECK(g.nav() == Game::Nav::Cursor);
        g.onButton(press(Button::B));             // inert on a locked slot
        CHECK(g.nav() == Game::Nav::Cursor);      // stayed on the carousel
        g.onButton(press(Button::C));             // back to idle for the next probe
    }
    // STAT / ITEMS still enter.
    for (SubmenuId open : {SubmenuId::Stat, SubmenuId::Items}) {
        cursorToSlot(g, open);
        g.onButton(press(Button::B));
        CHECK(g.nav() == Game::Nav::Submenu);
        g.onButton(press(Button::C));
    }
    // Hatched: the egg-locked slots are open.
    Game h{StartMode::Hatched};
    CHECK(!h.inEggPhase());
    cursorToSlot(h, SubmenuId::Train);
    h.onButton(press(Button::B));
    CHECK(h.nav() == Game::Nav::Submenu);
}

// Egg-phase items: only quest items are usable — a Food item's Use is inert (no
// feeding, count unchanged) while an egg, but works once hatched.
static void test_egg_items_quest_only() {
    { Game g;                                     // egg
      pickFirstEggLine(g);
      enterSubmenuId(g, SubmenuId::Items);        // ITEMS is not locked
      g.onButton(press(Button::B));               // first selectable row = airgap_snack (FOOD)
      CHECK(g.nav() == Game::Nav::Detail);
      const int n0 = g.inventory().count("airgap_snack");
      g.onButton(press(Button::B));               // Use -> gated on an egg
      CHECK(g.nav() == Game::Nav::Detail);        // no feeding modal
      CHECK(g.inventory().count("airgap_snack") == n0); }
    { Game h{StartMode::Hatched};                 // hatched -> the same food feeds
      enterSubmenuId(h, SubmenuId::Items);
      h.onButton(press(Button::B));
      const int n0 = h.inventory().count("airgap_snack");
      h.onButton(press(Button::B));               // Use -> feeds
      CHECK(h.nav() == Game::Nav::ModalFeeding);
      CHECK(h.inventory().count("airgap_snack") == n0 - 1); }
}

// The A+C Exploit chord launches the Boot-Sector decrypt minigame — but only in
// the second half of incubation (the exploit symbol's window). Inert before that.
static void test_egg_exploit_chord_hatches() {
    Game g;
    pickFirstEggLine(g);
    uint32_t t = 0;
    g.tick(t += 1000);
    CHECK(!g.hatchMinigameReady());
    g.onButton(chordAC());                        // first half: chord is inert
    CHECK(g.nav() == Game::Nav::Idle);
    g.tick(t += kBootHatchMs / 2);                // cross halfway
    CHECK(g.hatchMinigameReady());
    // Mirror hardware: A summons the cursor, then C completes the chord.
    g.onButton(press(Button::A));
    g.onButton(chordAC());
    CHECK(g.nav() == Game::Nav::ModalHatch);
}

// An egg can't arm explore-mode: EXPL is greyed/locked, so B on the EXPL
// slot is inert and never starts a run.
static void test_egg_cannot_explore() {
    Game g;
    pickFirstEggLine(g);
    uint32_t t = 0;
    g.tick(t += 1000);
    CHECK(g.inEggPhase());
    cursorToSlot(g, SubmenuId::Expl);
    CHECK(g.nav() == Game::Nav::Cursor);
    g.onButton(press(Button::B));                 // inert on a locked slot
    CHECK(g.nav() == Game::Nav::Cursor);
    CHECK(!g.exploreActive());
}

// STAT time-to-next-evolution readout (all stages): a mid-chain pet counts down
// from its stage's dwell (kEvolveProcessToScriptMs for a Process pet); an egg
// reports its incubation clock; a Daemon terminus has no successor
// (hasNextEvolution() false, remaining 0 -> STAT shows MAX).
static void test_evolve_remaining_readout() {
    Game g{StartMode::Hatched, "paypup"};             // Process -> Script successor
    CHECK(g.hasNextEvolution());
    g.tick(1000);
    CHECK(g.evolveRemainMs() == kEvolveProcessToScriptMs - 1000);
    g.tick(61000);
    CHECK(g.evolveRemainMs() == kEvolveProcessToScriptMs - 61000);

    Game e;                                           // egg: readout = incubation clock
    pickFirstEggLine(e);
    e.tick(1000);
    CHECK(e.hasNextEvolution());
    CHECK(e.evolveRemainMs() == e.bootHatchRemainMs());

    Game d{StartMode::Hatched, "bruinforce"};   // Daemon terminus: no successor
    CHECK(!d.hasNextEvolution());
    CHECK(d.evolveRemainMs() == 0);
}

// --- Task 1: dev "reset to egg" shortcut -----------------------------------
// resetToHatch() wipes a raised pet back to a fresh empty save + a laid egg at
// idle, and a real first boot can be replayed from there (incubate -> Process).
static void test_reset_to_hatch() {
    Game g{StartMode::Hatched};                 // raised Paypup
    CHECK(g.nav() == Game::Nav::Idle && g.pet() != nullptr);
    g.model().setHunger(5);
    g.inventory().remove("airgap_snack", 1);    // perturb state to prove the wipe
    g.resetToHatch();
    pickFirstEggLine(g);                         // reset re-enters line-select; pick Ransomware
    CHECK(g.nav() == Game::Nav::Idle);          // egg laid at idle
    CHECK(g.pet() != nullptr && std::strcmp(g.pet()->id, "cryptoshell") == 0);
    CHECK(g.inEggPhase());
    CHECK(!g.lockoutActive());
    CHECK(g.bits() == kStartBits);
    CHECK(g.log().size() == 0);
    // Replays a real first boot from the wipe: wait out incubation -> Process idle.
    uint32_t t = 0;
    g.tick(t += 1000);
    g.tick(t += kBootHatchMs + kHeartbeatMs);
    CHECK(g.nav() == Game::Nav::Idle);
    CHECK(g.pet() != nullptr && g.pet()->stage == Stage::Process);   // random Process
    CHECK(g.model().hunger() == kStartHunger);
}

// --- Evolution boundary stub ------------------------------------
// Helper: advance the cinematic past hold + flash into the reveal.
static void advanceToReveal(Game& g, uint32_t& t) {
    for (int i = 0; i < kEvoHoldBeats + kEvoFlashBeats + 1; ++i)
        g.tick(t += kHeartbeatMs);
}

// Boot -> Process: debug trigger fires the modal; B (only once revealed)
// commits the swap to the Process successor and advances the stage indicator.
static void test_evolution_boot_to_process() {
    Game g{StartMode::Hatched, "cryptoshell"};  // Boot-Sector start
    CHECK(g.pet()->stage == Stage::BootSector);
    g.debugTriggerEvolution();
    CHECK(g.nav() == Game::Nav::ModalEvolve);
    CHECK(g.pet()->stage == Stage::BootSector);  // old sprite held through cinematic
    uint32_t t = 0;
    advanceToReveal(g, t);
    g.onButton(press(Button::B));
    CHECK(g.nav() == Game::Nav::Idle);
    CHECK(g.pet() != nullptr && g.pet()->stage == Stage::Process);
    CHECK(std::strcmp(g.pet()->id, "paypup") == 0);  // documented stub target
    CHECK(g.log().size() == 0);                       // not logged in v1
}

// C is disabled (not "back"); B before the reveal is inert (the cinematic gate).
static void test_evolution_c_disabled_b_gated() {
    Game g{StartMode::Hatched, "cryptoshell"};
    g.debugTriggerEvolution();
    g.onButton(press(Button::C));                // disabled -> still in the modal
    CHECK(g.nav() == Game::Nav::ModalEvolve);
    g.onButton(press(Button::B));                // pre-reveal -> ignored
    CHECK(g.nav() == Game::Nav::ModalEvolve);
    uint32_t t = 0;
    advanceToReveal(g, t);
    g.onButton(press(Button::B));                // revealed -> continues
    CHECK(g.nav() == Game::Nav::Idle);
}

// Natural trigger: time-in-stage gate fires from rest; a Process pet (no
// successor) and a Dying care budget (5/5 -> Critical, not evolution) do not.
static void test_evolution_triggers_and_gates() {
    Game g{StartMode::Hatched, "cryptoshell"};
    g.tick(1000);
    CHECK(g.nav() == Game::Nav::Idle);                       // not yet (in-stage)
    g.tick(1000 + kEvolveProcessToScriptMs);
    CHECK(g.nav() == Game::Nav::ModalEvolve);                // gate met -> fires

    Game term{StartMode::Hatched, "bruinforce"};       // Daemon terminus, no successor
    term.tick(1000 + kEvolveProcessToScriptMs);
    CHECK(term.nav() == Game::Nav::Idle);

    Game dying{StartMode::Hatched, "cryptoshell"};
    dying.model().setCareMistakes(kCareDying);               // 5/5 -> Critical path
    dying.tick(1000 + kEvolveProcessToScriptMs);
    CHECK(dying.nav() == Game::Nav::Idle);
}

// Script's dwell is longer than Process's: a Script-stage pet must NOT evolve
// at the shorter Process dwell, only at its own (longer) one.
static void test_evolution_script_dwell_longer_than_process() {
    Game g{StartMode::Hatched, "malbear"};                   // Script -> Daemon successor
    CHECK(g.hasNextEvolution());
    g.tick(1000 + kEvolveProcessToScriptMs);
    CHECK(g.nav() == Game::Nav::Idle);                       // too soon for Script's dwell
    g.tick(1000 + kEvolveScriptToDaemonMs);
    CHECK(g.nav() == Game::Nav::ModalEvolve);                // Script's own gate met -> fires
}

// The full chain evolves through all four stages: CryptoShell (Boot) -> Paypup
// (Process) -> Barkmail (Script) -> Wire Heir (Daemon). With default (Good) care
// the Script->Daemon branch resolves to the Good successor. Each hop fires the
// modal and commits on B; the Daemon terminus has no further evolution.
static void test_evolution_full_chain() {
    Game g{StartMode::Hatched, "cryptoshell"};
    const char* chain[] = {"paypup", "barkmail", "wire_heir"};
    const Stage stages[] = {Stage::Process, Stage::Script, Stage::Daemon};
    uint32_t t = 0;
    for (int hop = 0; hop < 3; ++hop) {
        g.debugTriggerEvolution();
        CHECK(g.nav() == Game::Nav::ModalEvolve);
        advanceToReveal(g, t);
        g.onButton(press(Button::B));
        CHECK(g.nav() == Game::Nav::Idle);
        CHECK(g.pet() && std::strcmp(g.pet()->id, chain[hop]) == 0);
        CHECK(g.pet()->stage == stages[hop]);
    }
    // Bruinforce is the terminus: no successor, so the trigger never fires.
    g.debugTriggerEvolution();
    CHECK(g.nav() == Game::Nav::Idle);
    g.tick(t += 1000 + kEvolveProcessToScriptMs);
    CHECK(g.nav() == Game::Nav::Idle);
}

// Grayscale gate: the reveal reads without colour (name ink + new sprite
// silhouette + the advanced stage-indicator node).
static void test_evolution_grayscale() {
    Game g{StartMode::Hatched, "cryptoshell"};
    g.debugTriggerEvolution();
    uint32_t t = 0;
    advanceToReveal(g, t);
    Framebuffer fb(kActiveW, kActiveH);
    g.render(fb);
    CHECK(anyNonPaper(fb, 0, 60, kActiveW, 120));      // new sprite silhouette
    bool nameInk = false;
    for (int y = 124; y < 136 && !nameInk; ++y)
        for (int x = 0; x < kActiveW; ++x)
            if (luminance(fb.get(x, y)) < 0.4f) { nameInk = true; break; }
    CHECK(nameInk);                                     // name text survives desaturation
    CHECK(anyNonPaper(fb, 0, 144, kActiveW, 158));      // stage indicator present
}

// --- Hardening --------------------------------------------------------------
// Walk the cursor from idle to a given submenu and enter it (mirrors the
// dump_frame nav helper) so a screen can be driven for a render assertion.
static void enterSubmenuId(Game& g, SubmenuId id) {
    g.onButton(press(Button::A));                 // idle A -> carousel @ slot 1
    while (carouselSlots()[g.cursor()].id != id)
        g.onButton(press(Button::A));
    g.onButton(press(Button::B));                 // enter the submenu
}

// Walk from the carousel to a CFG L3 screen by TARGET, descending through a group
// screen (DEVICE / RADIO) when the target lives in one. Scanning the tables rather
// than counting presses keeps every CFG gate independent of the row order.
static void enterCfgTarget(Game& g, CfgScreen target) {
    const CfgScreen group = cfgParentGroup(target);
    const CfgRow* rows = nullptr;
    int n = cfgRows(rows);
    int row = -1;
    for (int i = 0; i < n; ++i) if (rows[i].target == group) row = i;
    CHECK(row >= 0);
    enterSubmenuId(g, SubmenuId::Cfg);
    for (int i = 0; i < row; ++i) g.onButton(press(Button::A));
    g.onButton(press(Button::B));                 // open the row (a group, or the target)
    if (group == target) return;
    n = cfgGroupRows(group, rows);
    int sub = -1;
    for (int i = 0; i < n; ++i) if (rows[i].target == target) sub = i;
    CHECK(sub >= 0);
    for (int i = 0; i < sub; ++i) g.onButton(press(Button::A));
    g.onButton(press(Button::B));                 // open the setting inside the group
}

// True if any pixel in the region survives desaturation as ink (dark on paper)
// — the non-colour channel every screen must carry (release gate 1).
static bool hasDarkInk(const Framebuffer& fb, int x0, int y0, int x1, int y1) {
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x)
            if (luminance(fb.get(x, y)) < 0.4f) return true;
    return false;
}

// True if any pixel in [x0,x1)×[y0,y1) differs between two framebuffers.
static bool regionDiffers(const Framebuffer& a, const Framebuffer& b,
                          int x0, int y0, int x1, int y1) {
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x)
            if (a.get(x, y) != b.get(x, y)) return true;
    return false;
}

static bool fbEqual(const Framebuffer& a, const Framebuffer& b) {
    return !regionDiffers(a, b, 0, 0, kActiveW, kActiveH);
}

// STAT is 5 paged screens — 0 pet VITALS (the landing, + an XP bar), 1 LOADOUT
// (the equipped moves + mods, WITH their effect text), 2 BUFFS (armed item
// buffs), 3 SPECIES (the pet's own lore), 4 AUDIT LOG — cycled by A, and C
// backs out. (Device/account stats live on the Hacker PROFILE slot.)
static void test_stat_paging_loadout_xp() {
    Game g{StartMode::Hatched};
    g.model().setHunger(100);                        // full gauge -> 10 lit cells
    enterSubmenuId(g, SubmenuId::Stat);
    Framebuffer p0(kActiveW, kActiveH);
    g.render(p0);                                    // page 0 = VITALS (landing)
    CHECK(litCellsGray(p0, 70, 110, 60) == 10);       // full hunger gauge (fresh pet)

    // the XP bar at y=50 fills with banked XP — a half-level of XP paints
    // the fill region that an empty (0 XP) fresh pet leaves blank on the landing page.
    Game g3{StartMode::Hatched};
    g3.debugAddCombatXp(g3.xpToNextLevel() / 2);
    enterSubmenuId(g3, SubmenuId::Stat);
    Framebuffer p0xp(kActiveW, kActiveH);
    g3.render(p0xp);
    CHECK(regionDiffers(p0, p0xp, 31, 51, 120, 57));  // XP bar fill differs

    // A -> page 1 = LOADOUT (a distinct page).
    g.onButton(press(Button::A));
    Framebuffer p1(kActiveW, kActiveH);
    g.render(p1);
    CHECK(!fbEqual(p0, p1));

    // A -> page 2 = BUFFS (distinct), then page 3 = SPECIES, page 4 = audit
    // log, A -> wraps back to page 0 = VITALS.
    g.onButton(press(Button::A));
    Framebuffer p2(kActiveW, kActiveH);
    g.render(p2);
    CHECK(!fbEqual(p1, p2));
    g.onButton(press(Button::A));   // -> page 3 SPECIES
    g.onButton(press(Button::A));   // -> page 4 AUDIT LOG
    g.onButton(press(Button::A));   // -> wraps to page 0 VITALS
    Framebuffer wrap(kActiveW, kActiveH);
    g.render(wrap);
    CHECK(fbEqual(p0, wrap));                          // cycle wraps to VITALS
}

// buildLoadoutRows(): the STAT LOADOUT page's row model. Every UNLOCKED slot yields exactly one row carrying BOTH the name
// AND the effect text: the equipped move, or — when the slot is EMPTY — the innate
// Quick Jab fallback tagged isDefault. There is NO standalone default row (Quick Jab is
// not a dedicated fixture; it surfaces only inside an empty slot). An empty MODS loadout
// still gets its section (a "- NONE -" row); an egg collapses to one "- no loadout -" line.
static void test_loadout_rows_model() {
    ContentRegistry r = ContentRegistry::embedded();
    MoveLoadout ml = MoveLoadout::starting();   // owns 3, equips packet_storm (slot 0 only)
    Loadout mods = Loadout::starting();         // equips firewall_patch + clock_speed_boost

    auto rows = buildLoadoutRows(r, ml, mods, Stage::Process, /*isEgg=*/false);

    bool sawMovesHeader = false, sawModsHeader = false;
    for (const auto& row : rows) {
        if (row.header && std::strcmp(row.label, "MOVES") == 0) sawMovesHeader = true;
        if (row.header && std::strcmp(row.label, "MODS") == 0) sawModsHeader = true;
    }
    CHECK(sawMovesHeader && sawModsHeader);

    // At Process the pet has 2 slots: slot 0 = packet_storm (equipped), slot 1 = empty →
    // the Quick Jab fallback. So there's a default (isDefault) row, it names Quick Jab and
    // carries its effect, and it appears AFTER the equipped move (in slot order) — NOT as a
    // leading fixture. The first non-header row is the equipped move, not the default.
    const LoadoutRow* firstMove = nullptr;
    const LoadoutRow* defaultRow = nullptr;
    for (const auto& row : rows) {
        if (row.header) continue;
        if (!firstMove) firstMove = &row;
        if (row.isDefault && !defaultRow) defaultRow = &row;
    }
    CHECK(firstMove && !firstMove->isDefault);            // equipped move leads, not the default
    const MoveDef* def = r.move(ml.defaultMove());
    CHECK(defaultRow && def && std::strcmp(defaultRow->label, def->displayName) == 0);
    CHECK(defaultRow && !defaultRow->effect.empty() &&
          std::strcmp(defaultRow->effect.c_str(), effectText(*def).c_str()) == 0);

    // Every UNLOCKED slot yields one row: equipped moves by name+effect, empty slots as
    // the default. Exact row count = 2 headers + (one row per unlocked slot) + mod rows.
    auto containsNameAndEffect = [&](const char* name, const EffectText& effect) {
        for (const auto& row : rows)
            if (!row.header && std::strcmp(row.label, name) == 0 &&
                !row.effect.empty() &&
                std::strcmp(row.effect.c_str(), effect.c_str()) == 0)
                return true;
        return false;
    };
    const int unlocked = MoveLoadout::slotsForStage(Stage::Process);
    for (int i = 0; i < unlocked; ++i) {
        if (const char* id = ml.equipped(i)) {
            const MoveDef* m = r.move(id);
            CHECK(m && containsNameAndEffect(m->displayName, effectText(*m)));
        }
    }
    int equippedModCount = 0;
    for (int i = 0; i < kModSlots; ++i) {
        if (const char* id = mods.equipped(i)) {
            const ModDef* m = r.mod(id);
            CHECK(m && containsNameAndEffect(m->displayName, effectText(*m)));
            ++equippedModCount;
        }
    }
    CHECK(equippedModCount > 0);   // Loadout::starting does equip mods (sample)
    CHECK(static_cast<int>(rows.size()) == 2 + unlocked + equippedModCount);

    // No mods equipped -> a single "- NONE -" row (the section still appears).
    Loadout noMods;   // default-constructed: equipped(i) == nullptr for every slot
    auto rowsNoMods = buildLoadoutRows(r, ml, noMods, Stage::Process, false);
    bool sawNone = false;
    for (const auto& row : rowsNoMods)
        if (!row.header && std::strcmp(row.label, "- NONE -") == 0) sawNone = true;
    CHECK(sawNone);

    // An egg collapses the whole page to one line, no headers.
    auto eggRows = buildLoadoutRows(r, ml, mods, Stage::BootSector, /*isEgg=*/true);
    CHECK(eggRows.size() == 1 && !eggRows[0].header && eggRows[0].effect.empty());
    CHECK(std::strcmp(eggRows[0].label, "- NO LOADOUT -") == 0);
}

// Descriptions are TEMPLATES over their own row (core/content/effect_text.h): a
// description writes `{mag}` and the value is substituted from the same row, so a
// balance retune can never leave the prose quoting the old number. That only holds
// while every token a row names actually resolves — an unknown or misspelt token is
// passed through braces and all, which is exactly what this asserts against, across
// every item / mod / move in the shipped tables. It also catches the reverse mistake:
// a bare digit typed into prose that a {token} should have supplied is invisible to
// the expander, so the stat line is checked to be carrying the row's magnitudes
// independently of whatever the prose says.
static void test_effect_text_templates_resolve() {
    auto clean = [](const EffectText& t) {
        return std::strchr(t.c_str(), '{') == nullptr &&
               std::strchr(t.c_str(), '}') == nullptr;
    };
    for (int i = 0; i < kItemsCount; ++i) CHECK(clean(effectText(kItems[i])));
    for (int i = 0; i < kModsCount; ++i) CHECK(clean(effectText(kMods[i])));
    for (int i = 0; i < kMovesCount; ++i) CHECK(clean(effectText(kMoves[i])));

    // A worked case end to end: Tripwire's two magnitudes land in the right holes
    // (magnitude = the reflected damage, magnitude2 = the Health threshold — the
    // pair a hand-written sentence is most likely to transpose).
    const ModDef* tw = nullptr;
    for (int i = 0; i < kModsCount; ++i)
        if (std::strcmp(kMods[i].id, "tripwire") == 0) tw = &kMods[i];
    CHECK(tw && tw->magnitude == 10 && tw->magnitude2 == 40);
    CHECK(std::strcmp(effectText(*tw).c_str(),
                      "Below 40% Health, reflects 10 damage to any attacker.") == 0);

    // Every structured magnitude a row carries reaches the player through the stat
    // line whether or not the prose mentions it: the Tor-Tilla Chip's description is
    // pure flavour, so its Happiness 10 has to come from here.
    const ItemDef* chip = nullptr;
    for (int i = 0; i < kItemsCount; ++i)
        if (std::strcmp(kItems[i].id, "tortilla_chip") == 0) chip = &kItems[i];
    CHECK(chip && std::strstr(effectText(*chip).c_str(), "10") == nullptr);
    CHECK(chip && std::strcmp(statLine(*chip).c_str(), "HAPPY +10") == 0);

    // Descriptions are drawn with the 5x7 font (core/render/font5x7.cpp), which has
    // no glyph above ASCII — a typographic dash or ellipsis renders as a run of
    // blanks AND measures 3 characters wide, throwing off every wrap that sizes
    // itself with textWidth(). So the tables stay ASCII-only.
    auto ascii = [](const char* s) {
        for (const char* p = s; *p; ++p)
            if (static_cast<unsigned char>(*p) > 127) return false;
        return true;
    };
    for (int i = 0; i < kItemsCount; ++i) CHECK(ascii(kItems[i].effect));
    for (int i = 0; i < kModsCount; ++i) CHECK(ascii(kMods[i].effect));
    for (int i = 0; i < kMovesCount; ++i) CHECK(ascii(kMoves[i].effect));

    // {|token|} renders the magnitude unsigned, for prose that carries the sign.
    ItemDef probe = kItems[0];
    probe.effect = "{hunger} then {|hunger|}";
    probe.effects[0] = {ItemEffect::Kind::Hunger, -15};
    probe.effects[1] = {};
    CHECK(std::strcmp(effectText(probe).c_str(), "-15 then 15") == 0);

    // An unknown token survives as literal braces — which is what makes the sweep
    // above a real gate rather than a formality.
    probe.effect = "{nosuchfield}";
    CHECK(std::strcmp(effectText(probe).c_str(), "{nosuchfield}") == 0);
}

// A content row's panel is the readout GRID plus the prose (ui/spec_sheet.h), and the
// two share one band. The grid is reserved first and never cut, so the prose is what
// overruns — and on the pages with no scroll it loses its tail with no ellipsis and no
// warning. This gates the three bands where that can happen, measured through the
// renderers' own wrap + grid packing so it can't drift from what ships:
//   * ITEMS / MODS detail — grid + prose must fit the band above their footer rows.
//   * an area STOREFRONT — kShopDescLines, much tighter, and only binding on the rows
//     an area actually stocks (a full shop's worst case; a short list gets more).
//   * every move's grid alone must fit the reserve the MOVE detail page sets aside for
//     it (train_screen's kDetailGridLines) — its prose is allowed to overrun, because
//     that page pages it on A rather than truncating.
//   * every RIG SHOP row's readout, swept across its whole tier ladder, must fit the
//     space under its name in a kRigRowPitch row (game_rig_shop.h's rigSpec).
// Prose is also checked against EffectText's own buffer, which caps it before any
// screen is involved.
// It also catches a readout LABEL that outgrew SpecRow's buffer, which truncates
// silently (a 20-char flag losing its last letter is invisible in review).
static void test_effect_text_fits_its_screen_budget() {
    constexpr int kMaxW = kActiveW - 2 * 8;   // both margins, every panel screen
    constexpr int kLineH = 12;
    constexpr int kDetailPanelLines = 7;      // items/mods: y=56 to the footer rows
    constexpr int kMoveGridReserve = 3;       // train_screen's kDetailGridLines
    auto proseLines = [&](const EffectText& t) {
        return textWrapLines(t.c_str(), kMaxW);
    };
    auto labelsIntact = [](const SpecRows& s) {
        for (int i = 0; i < s.count; ++i) {
            const SpecRow& r = s.rows[i];
            if (std::strlen(r.label) >= sizeof(r.label) - 1) return false;  // truncated
            if (std::strlen(r.value) >= sizeof(r.value) - 1) return false;
            if (textWidth(r.label) + kFontAdvance + textWidth(r.value) > kMaxW)
                return false;                                              // unpackable
        }
        return true;
    };
    auto panelFits = [&](const SpecRows& s, const EffectText& prose) {
        return labelsIntact(s) &&
               gridLines(kMaxW, s.rows, s.count) + proseLines(prose) <= kDetailPanelLines;
    };
    for (int i = 0; i < kItemsCount; ++i)
        CHECK(panelFits(specRows(kItems[i]), effectText(kItems[i])));
    for (int i = 0; i < kModsCount; ++i)
        CHECK(panelFits(specRows(kMods[i]), effectText(kMods[i])));
    for (int i = 0; i < kMovesCount; ++i) {
        const SpecRows s = specRows(kMoves[i]);
        CHECK(labelsIntact(s));
        CHECK(gridLines(kMaxW, s.rows, s.count) <= kMoveGridReserve);
    }

    // Prose is capped by EffectText's own buffer before any screen sees it, so a row
    // that reaches the cap has already lost its tail — catch that at the source rather
    // than wondering why a sentence stops mid-word on the device.
    for (int i = 0; i < kItemsCount; ++i) CHECK(!effectText(kItems[i]).atCap());
    for (int i = 0; i < kModsCount; ++i) CHECK(!effectText(kMods[i]).atCap());
    for (int i = 0; i < kMovesCount; ++i) CHECK(!effectText(kMoves[i]).atCap());

    // The Rig Shop builds its readout the same way (rigSpec), and its values are the
    // longest anyone builds — a "NOW -> NEXT" pair at a deep tier. Sweep every row
    // across its ladder: no truncation, and the grid has to fit the space a SHOP row
    // leaves under its name (kRigRowPitch minus the name line).
    constexpr int kRigRowGridLines = (kRigRowPitch - (kFontH + 5)) / (kFontH + 3);
    for (int i = 0; i < kRigUpgradeCount; ++i) {
        const RigUpgradeDef& d = kRigUpgrades[i];
        // Bandwidth is deliberately uncapped (kRigLevelUnlimited), so bound the sweep
        // at a level far past any real tier ladder rather than iterating a million.
        const int top = d.maxLevel < 300 ? d.maxLevel : 300;
        for (int lvl = 0; lvl <= top; ++lvl) {
            const SpecRows s = rigSpec(d, lvl);
            CHECK(labelsIntact(s));
            CHECK(gridLines(kMaxW, s.rows, s.count) <= kRigRowGridLines);
        }
    }

    ContentRegistry reg;
    reg.addSource(embeddedContent());
    for (int a = 0; a < kAreaCount; ++a) {
        const AreaDef& ar = area(a);
        for (int i = 0; i < ar.shop.listingCount; ++i) {
            const ItemDef* d = reg.item(ar.shop.listings[i].id);
            CHECK(d && proseLines(effectText(*d)) <= kShopDescLines);
        }
        for (int i = 0; i < ar.modShop.listingCount; ++i) {
            const ModDef* m = reg.mod(ar.modShop.listings[i].id);
            CHECK(m && proseLines(effectText(*m)) <= kShopDescLines);
        }
    }
}

// The STAT LOADOUT page's B-scroll: B is a no-op on pages 0/2
// (only page 1 gives it meaning), it advances the row window on page 1, and
// wraps back to the top once it runs past the end. The fresh starting loadout
// (2 moves + 2 mods + 2 section headers = 7 rows) already overflows
// kLoadoutVisibleRows (5), so no extra setup is needed to force a scroll.
static void test_stat_loadout_b_scroll() {
    Game g{StartMode::Hatched};
    enterSubmenuId(g, SubmenuId::Stat);

    // Page 0 (VITALS): B is a no-op.
    Framebuffer v0(kActiveW, kActiveH); g.render(v0);
    g.onButton(press(Button::B));
    Framebuffer v0b(kActiveW, kActiveH); g.render(v0b);
    CHECK(fbEqual(v0, v0b));

    // A -> page 1 (LOADOUT). Confirm the starting loadout really does overflow
    // the visible window (the precondition this test needs).
    g.onButton(press(Button::A));
    auto rows = buildLoadoutRows(g.content(), g.moveLoadout(), g.loadout(),
                                 g.pet()->stage, g.inEggPhase());
    CHECK(static_cast<int>(rows.size()) > kLoadoutVisibleRows);

    Framebuffer l0(kActiveW, kActiveH); g.render(l0);
    g.onButton(press(Button::B));               // scroll forward one window
    Framebuffer l1(kActiveW, kActiveH); g.render(l1);
    CHECK(!fbEqual(l0, l1));                     // the row window actually moved

    g.onButton(press(Button::B));                // past the end -> wraps to the top
    Framebuffer l2(kActiveW, kActiveH); g.render(l2);
    CHECK(fbEqual(l0, l2));

    // C -> back to the carousel (resets statPage_/loadoutScroll_); B re-enters
    // the submenu (cursor is still parked on STAT's slot); A -> page 1 confirms
    // the scroll reset (renders identically to the very first page-1 view, l0).
    g.onButton(press(Button::C));
    g.onButton(press(Button::B));
    g.onButton(press(Button::A));
    Framebuffer l3(kActiveW, kActiveH); g.render(l3);
    CHECK(fbEqual(l0, l3));

    // Page 2 (AUDIT LOG): B is a no-op there too.
    g.onButton(press(Button::A));                // page 1 -> page 2
    Framebuffer a0(kActiveW, kActiveH); g.render(a0);
    g.onButton(press(Button::B));
    Framebuffer a0b(kActiveW, kActiveH); g.render(a0b);
    CHECK(fbEqual(a0, a0b));
}

// Release gate 1 (grayscale readability) on every screen that lacked a dedicated
// grayscale test — each must read with colour stripped: ink text + structure.
static void test_remaining_screens_grayscale() {
    Framebuffer fb(kActiveW, kActiveH);
    const int W = kActiveW, H = kActiveH;

    // MAINT list.
    { Game g{StartMode::Hatched}; g.model().setFragmentation(40);
      enterSubmenuId(g, SubmenuId::Maint); g.render(fb);
      CHECK(hasDarkInk(fb, 0, 0, W, H)); }

    // MAINT action detail.
    { Game g{StartMode::Hatched}; g.model().setFragmentation(40);
      enterSubmenuId(g, SubmenuId::Maint); g.onButton(press(Button::B));
      CHECK(g.nav() == Game::Nav::Detail);
      g.render(fb); CHECK(hasDarkInk(fb, 0, 0, W, H)); }

    // MAINT process (running) then outcome (resolved) — both render via Process.
    { Game g{StartMode::Hatched}; g.model().setFragmentation(40);
      enterSubmenuId(g, SubmenuId::Maint); g.onButton(press(Button::B));
      g.onButton(press(Button::B));                 // start the non-interruptible run
      CHECK(g.nav() == Game::Nav::Process);
      g.render(fb); CHECK(hasDarkInk(fb, 0, 0, W, H));   // progress bar + label
      uint32_t t = 0;
      for (int i = 0; i < kProcessBeats + 1; ++i) g.tick(t += kHeartbeatMs);
      CHECK(g.nav() == Game::Nav::Process);            // resolved, awaiting dismiss
      g.render(fb); CHECK(hasDarkInk(fb, 0, 0, W, H)); }   // outcome text

    // Item detail.
    { Game g{StartMode::Hatched}; enterSubmenuId(g, SubmenuId::Items);
      g.onButton(press(Button::B)); CHECK(g.nav() == Game::Nav::Detail);
      g.render(fb); CHECK(hasDarkInk(fb, 0, 0, W, H)); }

    // Feeding modal (Use the first food item).
    { Game g{StartMode::Hatched}; enterSubmenuId(g, SubmenuId::Items);
      g.onButton(press(Button::B)); g.onButton(press(Button::B));   // detail -> Use
      CHECK(g.nav() == Game::Nav::ModalFeeding);
      g.render(fb); CHECK(hasDarkInk(fb, 0, 0, W, H)); }

    // Lockout modal.
    { Game g{StartMode::Hatched}; g.model().setHunger(0); g.tick(1);
      CHECK(g.lockoutActive());
      g.render(fb); CHECK(hasDarkInk(fb, 0, 0, W, H)); }

    // Hacker-Log (STAT page 2).
    { Game g{StartMode::Hatched}; enterSubmenuId(g, SubmenuId::Stat);
      g.onButton(press(Button::A));                 // page Vitals -> Log
      g.render(fb); CHECK(hasDarkInk(fb, 0, 0, W, H)); }

    // TRAIN list shell (a submenu with a deferred ·soon· action).
    { Game g{StartMode::Hatched}; enterSubmenuId(g, SubmenuId::Train);
      g.render(fb); CHECK(hasDarkInk(fb, 0, 0, W, H)); }
}

// Event-driven repaint: no free-running loop. Between
// heartbeats a static screen issues no redraw; a heartbeat repaints (idle
// animation); a state change (button) repaints exactly once, then quiesces.
static void test_event_driven_repaint() {
    Game g{StartMode::Hatched};
    g.tick(0);                              // consume the initial dirty paint
    CHECK(!g.tick(10));                     // static, mid-heartbeat -> no repaint
    CHECK(!g.tick(20));
    CHECK(g.tick(kHeartbeatMs));            // heartbeat boundary -> animation repaint
    g.onButton(press(Button::A));           // state change (summon cursor)
    CHECK(g.tick(kHeartbeatMs + 10));      // exactly one repaint for the change...
    CHECK(!g.tick(kHeartbeatMs + 20));     // ...then it quiesces
}

// Release gate 2 (legible before the x1.75 upscale) + the creature gates,
// sampled at the logical sprite resolution: each pet's frame 0 must read as a
// clear silhouette (a solid object, neither empty nor a full block) with distinct
// grayscale value steps (shading, not one flat blob).
static void test_sprite_grayscale_legibility() {
    ContentRegistry r = ContentRegistry::embedded();
    // The full Ransomware chain (Boot->Process->Script->Daemon) + Pingcub: each
    // sprite — incl. the single-frame Malbear / Daemon-branch frames — must
    // pass the shading/silhouette law.
    const char* ids[] = {"cryptoshell", "paypup", "malbear", "bruinforce",
                         "berserkernel", "pingcub"};
    for (const char* id : ids) {
        const CreatureDef* c = r.creature(id);
        CHECK(c != nullptr);
        const SpriteData* s = c ? r.creatureSprite(*c) : nullptr;
        CHECK(s != nullptr);
        if (!s) continue;
        int opaque = 0;
        bool band[5] = {false, false, false, false, false};   // 5 luminance steps
        for (int y = 0; y < s->h; ++y)
            for (int x = 0; x < s->frameW; ++x) {
                if (spriteAlphaAt(*s, x, y) < 128) continue;   // frame 0
                ++opaque;
                int b = static_cast<int>(luminance(spriteColorAt(*s, x, y)) * 5);
                if (b > 4) b = 4;
                if (b < 0) b = 0;
                band[b] = true;
            }
        const int cells = s->frameW * s->h;
        CHECK(opaque > cells / 10 && opaque < cells * 95 / 100);   // clear silhouette
        int bands = 0;
        for (bool v : band) if (v) ++bands;
        CHECK(bands >= 3);                                          // distinct value steps
    }
}

// ===========================================================================
// Carousel submenus (CFG · ARCH · MODS · TRAIN · EXPL)
// ===========================================================================

// CFG UI Mode toggle is wired to the live Game::setUiMode: cycle the
// option, B applies and changes the carousel presentation in-menu.
static void test_cfg_uimode_toggle() {
    Game g{StartMode::Hatched};
    CHECK(g.uiMode() == UiMode::IconsLabel);
    enterCfgTarget(g, CfgScreen::UiMode);         // CFG -> DEVICE -> UI MODE
    CHECK(g.nav() == Game::Nav::Detail);
    CHECK(g.cfgScreen() == CfgScreen::UiMode);
    g.onButton(press(Button::A));                 // cycle IconsLabel -> IconsOnly
    g.onButton(press(Button::B));                 // apply -> back to DEVICE
    CHECK(g.nav() == Game::Nav::Detail);
    CHECK(g.cfgScreen() == CfgScreen::Device);
    CHECK(g.uiMode() == UiMode::IconsOnly);       // the live mode changed
    g.onButton(press(Button::C));                 // and DEVICE backs out to the list
    CHECK(g.nav() == Game::Nav::Submenu);
}

// CFG -> DEVICE -> TRAVEL MODE opens on NO and needs a second, deliberate yes, like
// every other screen that commits something the row offering it cannot take back.
static void test_cfg_travel_confirm_asks_twice() {
    {   // B on the NO it opened on asks for nothing, and backs out to the group.
        Game g{StartMode::Hatched};
        enterCfgTarget(g, CfgScreen::Travel);
        CHECK(g.cfgScreen() == CfgScreen::Travel);
        CHECK(!g.travelSleepRequested());
        g.onButton(press(Button::B));
        CHECK(!g.travelSleepRequested());
        CHECK(g.cfgScreen() == CfgScreen::Device);
    }
    {   // C is the other way out, and equally silent.
        Game g{StartMode::Hatched};
        enterCfgTarget(g, CfgScreen::Travel);
        g.onButton(press(Button::C));
        CHECK(!g.travelSleepRequested());
        CHECK(g.cfgScreen() == CfgScreen::Device);
    }
    {   // A moves onto YES; only then does B latch the request.
        Game g{StartMode::Hatched};
        enterCfgTarget(g, CfgScreen::Travel);
        g.onButton(press(Button::A));
        g.onButton(press(Button::B));
        CHECK(g.travelSleepRequested());
        // Latched, not a pulse: the device tier is already landing a save and
        // powering the panel down, so nothing here half-cancels it.
        g.onButton(press(Button::C));
        g.onButton(press(Button::A));
        CHECK(g.travelSleepRequested());
        CHECK(g.cfgScreen() == CfgScreen::Travel);
    }
}

// The travel-sleep contract, and the reason the mode needs no clock-freeze machinery
// of its own: the device tier lands a save and then DEEP-sleeps, which on this board
// is a reset, so a wake re-enters through that save. Whatever the gap was, the state
// it comes back to must be the state it left.
//
// Manufactured at the seam — the save the sleep writes, reloaded the way a wake
// reloads it — rather than by trying to sleep, which the host tier cannot do.
static void test_travel_sleep_credits_nothing_to_the_gap() {
    MemSaveStore store;
    Game before(StartMode::Hatched, "paypup", &store);
    for (uint32_t t = 1000; t <= 120000; t += 1000) before.tick(t);
    const int hunger = before.model().hunger();
    const int happy = before.model().happiness();
    const uint32_t evolveRemain = before.evolveRemainMs();
    const uint32_t uptime = before.lifetimeUptimeMs();
    CHECK(evolveRemain > 0);                    // there IS a growth clock to strand
    CHECK(before.saveNow());                    // what the device tier does last

    // The wake. A fresh Game over the same store is exactly what the reset produces.
    Game after(StartMode::Hatched, "paypup", &store);
    CHECK(after.model().hunger() == hunger);    // no decay credited to the gap
    CHECK(after.model().happiness() == happy);
    CHECK(after.evolveRemainMs() == evolveRemain);   // nor any growth
    // Lifetime uptime counts time AWAKE, so it resumes rather than jumping: the
    // Backup Drive shield and the CSF grace window both anchor on it.
    CHECK(after.lifetimeUptimeMs() == uptime);

    // And the one outcome the device tier must not sleep over. persistSave defers
    // when the heap is too tight to serialize, and a deferral before a reset is a
    // lost session, so saveNow reports it instead of returning a bare void.
    Game tight(StartMode::Hatched, "paypup", &store);
    tight.setHeapProbe([]() -> uint32_t { return 1024; });
    CHECK(!tight.saveNow());
}

// A setting inside a group returns to that group, not to the top of CFG — and the
// group's cursor lands on the row just left, so a second visit resumes there.
static void test_cfg_group_back_resumes_row() {
    Game g{StartMode::Hatched};
    enterCfgTarget(g, CfgScreen::PediaAp);        // CFG -> RADIO -> PEDIA AP
    CHECK(g.cfgScreen() == CfgScreen::PediaAp);
    g.onButton(press(Button::C));                 // no change -> back to RADIO
    CHECK(g.nav() == Game::Nav::Detail);
    CHECK(g.cfgScreen() == CfgScreen::Radio);
    g.onButton(press(Button::B));                 // B re-opens the SAME row
    CHECK(g.cfgScreen() == CfgScreen::PediaAp);
    g.onButton(press(Button::C));
    g.onButton(press(Button::C));                 // RADIO -> the CFG list
    CHECK(g.nav() == Game::Nav::Submenu);
}

// The RADIO group reports the arbiter's resolved owner, not each toggle's intent:
// two consents can read ON while exactly one holds the radio.
static void test_cfg_radio_reports_owner() {
    Game g{StartMode::Hatched};
    CHECK(g.radioOwner() == RadioOwner::None);    // host: nothing owns the radio
    g.setNetScanEnabled(true);
    g.setApEnabled(true);                          // both consents on...
    g.setRadioOwner(RadioOwner::Ap);               // ...the arbiter granted ONE
    CHECK(std::string(radioOwnerName(g.radioOwner())) == "PEDIA AP");
    enterCfgTarget(g, CfgScreen::Radio);
    Framebuffer fb(kActiveW, kActiveH);
    g.render(fb);
    CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));
}

// The RADIO group tells the operator "nearest the top wins", so the row order has
// to BE the arbiter's priority order — otherwise the screen's one explanation of
// why a switched-on row isn't running is a lie. RadioOwner is declared lowest
// priority first (radio_status.h), so the table read top-down must be strictly
// descending in that enum. AUDIT maps to two owners; it is compared at the higher
// of them, which is the most the row can ever resolve to.
static void test_cfg_radio_rows_follow_arbiter_priority() {
    const CfgRow* rows = nullptr;
    const int n = cfgGroupRows(CfgScreen::Radio, rows);
    CHECK(n == 3);

    auto rank = [](CfgScreen s) {
        switch (s) {
            case CfgScreen::PediaAp: return RadioOwner::Ap;
            case CfgScreen::Link: return RadioOwner::Link;
            case CfgScreen::Audit: return RadioOwner::Capture;
            default: return RadioOwner::None;
        }
    };
    for (int i = 0; i < n; ++i) CHECK(rank(rows[i].target) != RadioOwner::None);
    for (int i = 1; i < n; ++i)
        CHECK(static_cast<int>(rank(rows[i].target)) <
              static_cast<int>(rank(rows[i - 1].target)));
}

// the CFG list scrolls once it overflows the viewport. The pure offset
// helper keeps the cursor on-screen and clamps to a valid window.
static void test_cfg_list_scroll_offset() {
    // Fits entirely: never scrolls.
    CHECK(cfgScrollTop(0, 6, 6) == 0);
    CHECK(cfgScrollTop(5, 6, 6) == 0);
    // Overflows (10 rows, 6 visible): top rows show window 0; the cursor stays
    // pinned to the bottom visible slot as it descends; clamps at n-visible.
    CHECK(cfgScrollTop(0, 10, 6) == 0);
    CHECK(cfgScrollTop(5, 10, 6) == 0);      // last fully-visible row at window 0
    CHECK(cfgScrollTop(6, 10, 6) == 1);      // one past -> scroll by 1
    CHECK(cfgScrollTop(9, 10, 6) == 4);      // last row -> max window (10-6)
    // The live table, whatever its current length, always keeps the last row visible.
    const CfgRow* rows = nullptr;
    const int n = cfgRows(rows);
    const int top = cfgScrollTop(n - 1, n, 6);
    CHECK(top >= 0);
    CHECK(n - 1 >= top);                       // cursor at or after the window start
    CHECK(n - 1 < top + 6);                    // ...and within the visible window
}

// The DEV "Reset to Hatch" row (dev_config.h) wipes to the Hatch in one B press.
static void test_cfg_dev_reset_row() {
    const CfgRow* rows = nullptr;
    const int n = cfgRows(rows);
    int resetRow = -1;
    for (int i = 0; i < n; ++i) if (rows[i].target == CfgScreen::ResetHatch) resetRow = i;
    if (resetRow < 0) return;                     // row compiled out (release build)
    Game g{StartMode::Hatched};
    enterSubmenuId(g, SubmenuId::Cfg);
    for (int i = 0; i < resetRow; ++i) g.onButton(press(Button::A));
    g.onButton(press(Button::B));                 // one press -> resetToHatch (line-select)
    pickFirstEggLine(g);                           // pick Ransomware -> egg at idle
    CHECK(g.nav() == Game::Nav::Idle);            // egg laid at idle
    CHECK(g.pet() != nullptr && g.inEggPhase());
}

// The hidden Factory Reset: hold-B on System Info reveals it, hold-B on
// the reset screen commits the wipe. Releasing B aborts the hold.
static void test_cfg_factory_reset_hold() {
    Game g{StartMode::Hatched};
    enterSubmenuId(g, SubmenuId::Cfg);            // listRow 0 == System Info
    g.onButton(press(Button::B));                 // open System Info (L3)
    CHECK(g.nav() == Game::Nav::Detail);
    uint32_t t = 0;

    // A short hold, then release, must NOT reveal (abort).
    g.onButton(press(Button::B));                 // arm hold
    g.tick(t += kFactoryRevealMs / 2);
    g.onButton({Button::B, false, false});        // release -> abort
    g.tick(t += kHeartbeatMs);                     // (a longer wait would auto-defocus)
    CHECK(g.nav() == Game::Nav::Detail);          // still System Info, not revealed

    // A full hold reveals the Factory Reset screen.
    g.onButton(press(Button::B));                 // arm hold
    g.tick(t += kFactoryRevealMs + kHeartbeatMs); // elapses -> reveal
    CHECK(g.nav() == Game::Nav::Detail);          // FactoryReset is an L3 (Detail)
    Framebuffer fb(kActiveW, kActiveH);
    g.render(fb);
    CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));

    // Hold-B again commits the wipe -> line-select -> a freshly laid egg at idle.
    g.onButton(press(Button::B));                 // arm hold-to-commit
    g.tick(t += kFactoryCommitMs + kHeartbeatMs);
    pickFirstEggLine(g);                           // commit re-enters line-select; pick Ransomware
    CHECK(g.nav() == Game::Nav::Idle);
    CHECK(g.pet() != nullptr && g.inEggPhase());
}

// The Factory Reset's two scopes differ, and each does exactly what its screen
// says: RESET PET lays a fresh egg and leaves the operator's history standing,
// WIPE EVERYTHING clears that history too (and returns the radio consents to their
// out-of-box OFF). A picker whose options behaved alike would be a lie.
static void test_cfg_factory_reset_scopes_differ() {
    // Three different storage shapes, so the wipe is shown to reach all of them:
    // a bitmask (Titles), a vector of registry pointers ('Pedia reveals), and the
    // persisted consent bools.
    auto seedProgress = [](Game& g) {
        g.debugUnlockTitle(0);                     // a Title (auto-equips)
        g.markCreatureSeen("berserkernel");     // a 'Pedia reveal
        g.setNetScanEnabled(true);
        g.setApEnabled(true);                      // two radio consents
    };
    // Commit the reveal + the hold that follows it, exactly as the screen does.
    auto commitWipe = [](Game& g, int scope) {
        enterCfgTarget(g, CfgScreen::SysInfo);
        uint32_t t = 0;
        g.onButton(press(Button::B));
        g.tick(t += kFactoryRevealMs + kHeartbeatMs);   // hold-B -> the reset screen
        CHECK(g.cfgScreen() == CfgScreen::FactoryReset);
        for (int i = 0; i < scope; ++i) g.onButton(press(Button::A));   // cycle scope
        g.onButton(press(Button::B));
        g.tick(t += kFactoryCommitMs + kHeartbeatMs);   // hold-B -> commit
        pickFirstEggLine(g);
    };

    // RESET PET keeps every one of them.
    { Game g{StartMode::Hatched};
      seedProgress(g);
      CHECK(g.equippedTitle() == 0 && g.creatureSeen("berserkernel"));
      commitWipe(g, /*scope=*/0);
      CHECK(g.pet() != nullptr && g.inEggPhase()); // ...a fresh egg all the same
      CHECK(g.equippedTitle() == 0);
      CHECK(g.creatureSeen("berserkernel"));
      CHECK(g.netScanEnabled() && g.apEnabled()); }

    // WIPE EVERYTHING clears them.
    { Game g{StartMode::Hatched};
      seedProgress(g);
      commitWipe(g, /*scope=*/1);
      CHECK(g.pet() != nullptr && g.inEggPhase());
      CHECK(g.equippedTitle() == -1);
      CHECK(!g.creatureSeen("berserkernel"));
      CHECK(!g.netScanEnabled());                  // consents back to out-of-box OFF
      CHECK(!g.apEnabled() && !g.linkEnabled());
      // ...but the pet it just laid is still tallied as this device's own.
      CHECK(g.speciesRaised() >= 1); }
}

// Grayscale gate: every CFG screen reads with colour stripped (ink + structure) —
// the list, both group screens, and every setting inside them.
static void test_cfg_screens_grayscale() {
    Framebuffer fb(kActiveW, kActiveH);
    const int W = kActiveW, H = kActiveH;

    // List.
    { Game g{StartMode::Hatched}; enterSubmenuId(g, SubmenuId::Cfg);
      g.render(fb); CHECK(hasDarkInk(fb, 0, 0, W, H)); }

    const CfgRow* rows = nullptr;
    const int n = cfgRows(rows);
    for (int r = 0; r < n; ++r) {
        // The DEV reset row has no L3 viewer (it acts + stays on the list).
        if (rows[r].target == CfgScreen::ResetHatch) continue;
        Game g{StartMode::Hatched};
        enterCfgTarget(g, rows[r].target);
        CHECK(g.nav() == Game::Nav::Detail);
        g.render(fb);
        CHECK(hasDarkInk(fb, 0, 0, W, H));

        // ...and, for a group row, each setting it holds.
        const CfgRow* sub = nullptr;
        const int m = cfgGroupRows(rows[r].target, sub);
        for (int s = 0; s < m; ++s) {
            Game gs{StartMode::Hatched};
            enterCfgTarget(gs, sub[s].target);
            CHECK(gs.nav() == Game::Nav::Detail);
            CHECK(gs.cfgScreen() == sub[s].target);
            gs.render(fb);
            CHECK(hasDarkInk(fb, 0, 0, W, H));
        }
    }
}

// SD RECHECK is the A press on System Info, not a list row — it acts on the SD
// line that screen already reports through.
static void test_cfg_sysinfo_sd_recheck() {
    Game g{StartMode::Hatched};
    enterCfgTarget(g, CfgScreen::SysInfo);
    CHECK(g.cfgScreen() == CfgScreen::SysInfo);
    CHECK(!g.sdRecheckRequested());
    g.onButton(press(Button::A));                 // ask the device tier to re-mount
    CHECK(g.sdRecheckRequested());
    CHECK(g.nav() == Game::Nav::Detail);          // ...and stay to watch the SD line
    g.clearSdRecheck();                            // (the device tier's half)
    g.onButton(press(Button::C));
    CHECK(g.nav() == Game::Nav::Submenu);
}

// ARCH: the rack list shows the active pet; the record opens, cycles its
// actions (Store/Sell), and backs out. Both actions are inert shells.
static void test_arch_list_and_record() {
    Game g{StartMode::Hatched};
    enterSubmenuId(g, SubmenuId::Arch);
    CHECK(g.nav() == Game::Nav::Submenu);
    Framebuffer fb(kActiveW, kActiveH);
    g.render(fb);
    CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));   // list reads in grayscale

    g.onButton(press(Button::B));                       // open the pet record (L3)
    CHECK(g.nav() == Game::Nav::Detail);
    g.render(fb);
    CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));    // record reads in grayscale
    g.onButton(press(Button::A));                       // cycle Store -> Sell (inert)
    CHECK(g.nav() == Game::Nav::Detail);                // still on the record
    g.onButton(press(Button::C));                       // back to the rack
    CHECK(g.nav() == Game::Nav::Submenu);
    g.onButton(press(Button::C));                       // back to the carousel
    CHECK(g.nav() == Game::Nav::Cursor);
}

// MODS data model (D3): mods are PERMANENT — consumed from the spare pool
// when equipped, never unequipped, only overwritten (the old one discarded, not
// returned). The seed installs two mods and holds two spares.
static void test_loadout_permanent_mods() {
    Loadout l = Loadout::starting();
    CHECK(l.slotOf("firewall_patch") == 0);        // installed, not a spare
    CHECK(l.slotOf("clock_speed_boost") == 1);
    CHECK(!l.owns("firewall_patch"));              // equipped mods aren't in the pool
    CHECK(l.owns("packet_sniffer") && l.owns("raid_mirror"));  // two spares held
    CHECK(l.equipped(2) == nullptr);

    // Equip a spare into an empty slot: it's CONSUMED out of the pool.
    l.equip(2, "packet_sniffer");
    CHECK(l.slotOf("packet_sniffer") == 2);
    CHECK(!l.owns("packet_sniffer"));              // consumed on equip
    CHECK(l.equipped(0) != nullptr);               // did NOT move firewall out of slot 0

    // Overwrite a slot: the displaced mod is DISCARDED (not returned to the pool),
    // the new one consumed.
    l.equip(0, "raid_mirror");
    CHECK(l.slotOf("raid_mirror") == 0);
    CHECK(l.slotOf("firewall_patch") == -1);       // firewall gone for good
    CHECK(!l.owns("firewall_patch") && !l.owns("raid_mirror"));

    // Inert with no spare of that id available.
    l.equip(3, "firewall_patch");
    CHECK(l.equipped(3) == nullptr);
}

// MoveLoadout: a move lives in one slot — equipping it elsewhere
// MOVES it; the innate default sits outside the slots; slot COUNT grows by Stage.
static void test_move_loadout() {
    MoveLoadout l = MoveLoadout::starting();
    CHECK(std::strcmp(l.defaultMove(), "quick_jab") == 0);
    CHECK(l.slotOf("packet_storm") == 0);
    CHECK(l.equipped(1) == nullptr);                // slot 1 = Quick Jab per-slot fallback (#11)
    // fork_bomb + checksum_guard start OWNED but unequipped (the Attack/Defend spares).
    CHECK(l.owns("fork_bomb") && l.slotOf("fork_bomb") < 0);
    CHECK(l.owns("checksum_guard") && l.slotOf("checksum_guard") < 0);
    l.equip(2, "packet_storm");                       // move 0 -> 2
    CHECK(l.slotOf("packet_storm") == 2 && l.equipped(0) == nullptr);
    l.unequip(2);
    CHECK(l.equipped(2) == nullptr && l.owns("packet_storm"));   // stays owned

    // Slot counts grow Boot(1) -> Process(2) -> Script(3) -> Daemon(4).
    CHECK(MoveLoadout::slotsForStage(Stage::BootSector) == 1);
    CHECK(MoveLoadout::slotsForStage(Stage::Process) == 2);
    CHECK(MoveLoadout::slotsForStage(Stage::Script) == 3);
    CHECK(MoveLoadout::slotsForStage(Stage::Daemon) == kMaxMoveSlots);
    // Slot 1 first becomes available at Process (the "unlocks at" affordance).
    CHECK(MoveLoadout::stageUnlockingSlot(0) == Stage::BootSector);
    CHECK(MoveLoadout::stageUnlockingSlot(1) == Stage::Process);
    CHECK(MoveLoadout::stageUnlockingSlot(3) == Stage::Daemon);

    // The roster + default resolve through the registry.
    ContentRegistry r = ContentRegistry::embedded();
    CHECK(r.move("quick_jab") && r.move("quick_jab")->kind == MoveDef::Kind::Attack);
    CHECK(r.move("checksum_guard")->kind == MoveDef::Kind::Defend);
    CHECK(r.move("fork_bomb")->channelTurns == 2);
    CHECK(static_cast<int>(r.allMoves().size()) >= 4);
}

// Every combatant the game can put on screen must name a sprite the atlas actually
// carries. Sprites resolve by STRING at draw time, so a name with no PNG behind it
// is not a compile error and not a crash — it silently draws nothing, which is how
// the wild roster spent its life wearing the pet dog's frame. Distinctness is half
// the point: six wilds sharing one sprite is the bug this locks out.
static void test_every_combatant_sprite_resolves() {
    ContentRegistry r = ContentRegistry::embedded();
    std::vector<const char*> wildSprites;

    for (int tier = 1; tier <= 3; ++tier) {
        for (uint32_t variant = 0; variant < 2; ++variant) {
            const CombatEnemy e = wildMalbeast(tier, variant);
            CHECK(e.spriteName != nullptr);
            CHECK(r.sprite(e.spriteName) != nullptr);
            wildSprites.push_back(e.spriteName);
            // The name is also what the 'Pedia's seen/defeated masks key on.
            CHECK(wildMalbeastIndex(e.name) >= 0);
        }
    }
    CHECK(static_cast<int>(wildSprites.size()) == kWildMalbeastCount);
    for (size_t i = 0; i < wildSprites.size(); ++i)
        for (size_t j = i + 1; j < wildSprites.size(); ++j)
            CHECK(std::strcmp(wildSprites[i], wildSprites[j]) != 0);

    for (int tier = 0; tier <= 1; ++tier) {
        const CombatEnemy d = simDummy(tier);
        CHECK(d.spriteName != nullptr);
        CHECK(r.sprite(d.spriteName) != nullptr);
    }

    // Same contract for the raisable roster: every creature row's art exists.
    for (const CreatureDef* c : r.allCreatures()) {
        CHECK(c->spriteName != nullptr);
        CHECK(r.creatureSprite(*c) != nullptr);
    }
}

// ===========================================================================
// The shared combat engine
// ===========================================================================

static Combatant mkCombatant(const ContentRegistry& r, const char* name, int hp,
                             int spd, std::vector<const char*> ids) {
    Combatant c;
    c.name = name; c.maxHealth = hp; c.health = hp; c.speed = spd;
    for (const char* id : ids)
        if (const MoveDef* m = r.move(id)) c.moves.push_back(m);
    return c;
}

static Combat::Outcome runToEnd(Combat& cb) {
    int guard = 0;
    while (cb.outcome() == Combat::Outcome::Ongoing && guard++ < 1000) cb.step();
    return cb.outcome();
}

// Deterministic resolution: the same seed replays an identical fight; a strong
// player beats a weak dummy.
static void test_combat_deterministic() {
    ContentRegistry r = ContentRegistry::embedded();
    Combatant p = mkCombatant(r, "P", 60, 10, {"quick_jab", "packet_storm"});
    Combatant e = mkCombatant(r, "E", 20, 8, {"quick_jab"});

    Combat a; a.begin(p, e, Combat::Stakes::Safe, 12345);
    CHECK(runToEnd(a) == Combat::Outcome::Win);
    Combat b; b.begin(p, e, Combat::Stakes::Safe, 12345);
    CHECK(runToEnd(b) == Combat::Outcome::Win);
    CHECK(a.player().health == b.player().health);   // identical replay
}

// The autonomous roll never repeats a move twice in a row.
static void test_combat_no_consecutive() {
    ContentRegistry r = ContentRegistry::embedded();
    Combatant p = mkCombatant(r, "P", 100, 10, {"quick_jab", "packet_storm"});
    Combatant e = mkCombatant(r, "E", 100, 5, {"quick_jab"});
    Combat cb; cb.begin(p, e, Combat::Stakes::Safe, 777);
    const char* prev = nullptr; int checks = 0;
    for (int i = 0; i < 80 && cb.outcome() == Combat::Outcome::Ongoing; ++i) {
        const bool pturn = cb.playerTurnNext();
        cb.step();
        if (pturn) {
            if (prev) CHECK(std::strcmp(prev, cb.lastMoveName()) != 0);
            prev = cb.lastMoveName(); ++checks;
        }
    }
    CHECK(checks > 3);
}

// The Exploit override commands the next move AND breaks the no-consecutive rule:
// it can repeat the move just played. Opening doesn't spend; committing does.
static void test_combat_override_breaks_rule() {
    ContentRegistry r = ContentRegistry::embedded();
    Combatant p = mkCombatant(r, "P", 100, 10, {"quick_jab", "packet_storm"});
    Combatant e = mkCombatant(r, "E", 300, 10, {"quick_jab"});   // equal speed → strict alternation
    Combat cb; cb.begin(p, e, Combat::Stakes::Safe, 42);
    // Resolve the first player turn, note the move it played.
    cb.step();                                   // player acts (player-first on tie)
    const char* m1 = cb.lastMoveName();
    int idx = -1;
    for (int i = 0; i < static_cast<int>(cb.player().moves.size()); ++i)
        if (std::strcmp(cb.player().moves[i]->displayName, m1) == 0) idx = i;
    CHECK(idx >= 0);

    CHECK(cb.overrideReady());
    cb.openOverride();
    CHECK(cb.overrideReady());                   // opening does NOT spend
    while (cb.overridePick() != idx) cb.cycleOverride();
    cb.commitOverride();                         // commit the SAME move → spends
    CHECK(!cb.overrideReady());

    cb.step();                                   // enemy turn
    cb.step();                                   // player turn → forced repeat
    CHECK(std::strcmp(cb.lastMoveName(), m1) == 0 && cb.lastByPlayer());
}

// The Exploit picker's USE-ITEM branch: committing an item patches transient
// Health (clamped to max), spends one use, and reports the id back for the Game to
// consume exactly once. The item sits past the moves in the flat picker list.
static void test_combat_override_item_use() {
    ContentRegistry r = ContentRegistry::embedded();
    Combatant p = mkCombatant(r, "P", 100, 10, {"quick_jab", "packet_storm"});
    Combatant e = mkCombatant(r, "E", 300, 5, {"quick_jab"});
    Combat cb;
    cb.begin(p, e, Combat::Stakes::Safe, 42, /*forceEnemyFirst=*/false,
             /*carryPlayerHealth=*/50, /*exploitUses=*/1);
    CHECK(cb.player().health == 50);
    cb.openOverride({{"airgap_snack", "Air-Gapped Snack", 30}});
    CHECK(cb.overrideOpen() && cb.overrideMoveCount() == 2);
    while (cb.overridePick() != cb.overrideMoveCount()) cb.cycleOverride();
    cb.commitOverride();
    CHECK(cb.player().health == 80);             // +30 patch
    CHECK(!cb.overrideReady());                  // the single use is spent
    const char* used = cb.takeCommittedItem();
    CHECK(used && std::strcmp(used, "airgap_snack") == 0);
    CHECK(cb.takeCommittedItem() == nullptr);    // reported exactly once

    // The patch clamps to max Health (no overheal).
    cb.begin(p, e, Combat::Stakes::Safe, 42, false, /*carryPlayerHealth=*/90, 1);
    cb.openOverride({{"airgap_snack", "Air-Gapped Snack", 30}});
    while (cb.overridePick() != cb.overrideMoveCount()) cb.cycleOverride();
    cb.commitOverride();
    CHECK(cb.player().health == 100);
}

// exploitUsesPerBattle: >1 lets the override fire more than once; each commit
// spends one, and begin() resets the allowance for the next fight / gauntlet round.
static void test_exploit_uses_per_battle() {
    ContentRegistry r = ContentRegistry::embedded();
    Combatant p = mkCombatant(r, "P", 100, 10, {"quick_jab", "packet_storm"});
    Combatant e = mkCombatant(r, "E", 300, 5, {"quick_jab"});
    Combat cb;
    cb.begin(p, e, Combat::Stakes::Safe, 42, false, -1, /*exploitUses=*/2);
    CHECK(cb.overrideUsesTotal() == 2 && cb.overrideUsesLeft() == 2);
    cb.openOverride(); cb.commitOverride();       // force a move
    CHECK(cb.overrideUsesLeft() == 1 && cb.overrideReady());
    cb.openOverride(); cb.commitOverride();
    CHECK(cb.overrideUsesLeft() == 0 && !cb.overrideReady());
    cb.openOverride();
    CHECK(!cb.overrideOpen());                    // exhausted → won't reopen
    cb.begin(p, e, Combat::Stakes::Safe, 7, false, -1, 2);
    CHECK(cb.overrideUsesLeft() == 2);            // a fresh fight restores it
}

// End-to-end: A+C in a live fight opens the picker with the combat item, and
// committing it consumes the inventory stack + spends the single use.
static void test_combat_item_in_game() {
    Game g{StartMode::Hatched, "paypup"};
    g.debugStartCombat(/*live=*/true);
    CHECK(g.nav() == Game::Nav::Combat);
    const int snacks0 = g.inventory().count("airgap_snack");
    CHECK(snacks0 > 0);
    g.onButton({Button::A, true, true});           // A+C -> open picker
    CHECK(g.combat().overrideOpen());
    const int moveN = g.combat().overrideMoveCount();
    while (g.combat().overridePick() != moveN)      // cursor onto the item row
        g.onButton(press(Button::A));
    g.onButton(press(Button::B));                  // commit -> use item
    CHECK(g.inventory().count("airgap_snack") == snacks0 - 1);
    CHECK(!g.combat().overrideReady());            // single use spent
}

// A multi-turn channel move winds up (no damage) then detonates for full power.
static void test_combat_channel() {
    ContentRegistry r = ContentRegistry::embedded();
    Combatant p = mkCombatant(r, "P", 100, 10, {"fork_bomb"});   // channel 2, power 26
    Combatant e = mkCombatant(r, "E", 100, 10, {"quick_jab"});   // equal speed → P,E,P
    Combat cb; cb.begin(p, e, Combat::Stakes::Safe, 1);
    cb.step();                                   // player: wind-up
    CHECK(cb.lastByPlayer() && cb.lastWasCharge() && cb.lastDamage() == 0);
    CHECK(cb.enemy().health == 100);             // no payoff yet
    cb.step();                                   // enemy turn
    cb.step();                                   // player: detonate
    CHECK(cb.lastByPlayer() && !cb.lastWasCharge() && cb.lastDamage() == 26);
    CHECK(cb.enemy().health == 74);
}

// MODS passives measurably change the fight.
static void test_combat_mod_passives() {
    ContentRegistry r = ContentRegistry::embedded();
    Combatant enemy = mkCombatant(r, "E", 100, 10, {"packet_storm"});  // 12 dmg, fast

    // Firewall Patch cuts incoming damage by its own magnitude (read off the mod row).
    const int fwCut = r.mod("firewall_patch")->magnitude;
    Combatant plain = mkCombatant(r, "P", 100, 5, {"quick_jab"});      // slower → enemy first
    Combat a; a.begin(plain, enemy, Combat::Stakes::Safe, 5);
    a.step();
    CHECK(a.player().health == 88);                                    // 12 full
    Combatant fire = plain; fire.dmgReducePct = fwCut;
    Combat b; b.begin(fire, enemy, Combat::Stakes::Safe, 5);
    b.step();
    CHECK(b.player().health == 100 - 12 * (100 - fwCut) / 100);        // reduced

    // RAID Mirror negates exactly the first incoming hit, then is consumed.
    Combatant mir = plain; mir.mods.arm(ModEffect::RaidMirror);
    Combat c; c.begin(mir, enemy, Combat::Stakes::Safe, 5);
    c.step();
    CHECK(c.player().health == 100);                                   // first hit negated
    c.step(); c.step();                                               // player, then enemy again
    CHECK(c.player().health == 88);                                    // mirror gone → full hit

    // Clock-Speed Boost flips initiative: enemy(12) outspeeds base(10), the boost wins.
    Combatant base = mkCombatant(r, "P", 100, kCombatBaseSpeed, {"quick_jab"});
    Combatant fast = mkCombatant(r, "E", 100, 12, {"quick_jab"});
    Combat d; d.begin(base, fast, Combat::Stakes::Safe, 9);
    CHECK(!d.playerActsFirst());
    Combatant boosted = base; boosted.speed += r.mod("clock_speed_boost")->magnitude;
    Combat eC; eC.begin(boosted, fast, Combat::Stakes::Safe, 9);
    CHECK(eC.playerActsFirst());
}

// Stakes: safe flee quits immediately (Sim-Battle C). makePlayer/Enemy builders
// wire stage→maxHealth, the default move, and mod passives.
static void test_combat_builders_and_flee() {
    ContentRegistry r = ContentRegistry::embedded();
    const CreatureDef* pet = r.creature("paypup");        // Process → 2 slots, 60 HP
    MoveLoadout ml = MoveLoadout::starting();
    Loadout mods = Loadout::starting();                   // Firewall + Clock-Speed equipped
    Combatant pc = makePlayerCombatant(r, *pet, ml, mods);
    CHECK(pc.maxHealth == kMaxHealthByStage[stageIndex(Stage::Process)]);
    // #11: one pool entry per unlocked slot — slot 0's equipped packet_storm, and
    // slot 1 falls back to the Quick Jab default (the starter leaves it unequipped).
    CHECK(pc.moves.size() == 2);
    CHECK(std::strcmp(pc.moves[0]->id, "packet_storm") == 0);
    CHECK(std::strcmp(pc.moves[1]->id, "quick_jab") == 0);
    CHECK(pc.speed == kCombatBaseSpeed + r.mod("clock_speed_boost")->magnitude);  // Clock-Speed read
    CHECK(pc.dmgReducePct == r.mod("firewall_patch")->magnitude);                 // Firewall read

    CombatEnemy spec{"Dummy", "SPR_PET_CACHEMUTT", 1, 30, 8, {"quick_jab"}};
    Combatant ec = makeEnemyCombatant(r, spec);
    CHECK(ec.maxHealth == 30 && ec.diffPips == 1 && ec.moves.size() == 1);

    Combat cb; cb.begin(pc, ec, Combat::Stakes::Safe, 3);
    cb.flee();
    CHECK(cb.outcome() == Combat::Outcome::Fled);          // safe stakes → instant quit
}

// MODS equip flow: open an empty slot's picker, choose a spare, inspect its detail
// then equip it (no confirm for an empty slot). The mod is CONSUMED out of
// the spare pool (D3) — equipping is permanent.
static void test_mods_equip_flow() {
    Game g{StartMode::Hatched};
    enterSubmenuId(g, SubmenuId::Mods);
    CHECK(g.nav() == Game::Nav::Submenu);
    Framebuffer fb(kActiveW, kActiveH);
    g.render(fb);
    CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));     // list grayscale

    g.onButton(press(Button::A));                  // slot 1 -> slot 2
    g.onButton(press(Button::A));                  // slot 2 -> slot 3 (empty)
    CHECK(g.loadout().equipped(2) == nullptr);
    g.onButton(press(Button::B));                  // open slot 3 picker
    CHECK(g.nav() == Game::Nav::Detail);
    g.render(fb);
    CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));     // picker grayscale
    g.onButton(press(Button::B));                  // open the first spare's detail
    Framebuffer det = fb;
    g.render(det);
    CHECK(hasDarkInk(det, 0, 0, kActiveW, kActiveH));    // detail grayscale
    CHECK(g.loadout().equipped(2) == nullptr);          // inspecting equips nothing yet
    // C backs out of the detail to the picker without equipping.
    g.onButton(press(Button::C));
    CHECK(g.nav() == Game::Nav::Detail && g.loadout().equipped(2) == nullptr);
    g.onButton(press(Button::B));                  // re-open detail
    g.onButton(press(Button::B));                  // EQUIP from the detail
    CHECK(g.nav() == Game::Nav::Submenu);
    const char* installed = g.loadout().equipped(2);
    CHECK(installed != nullptr);                    // a spare is now installed there
    CHECK(!g.loadout().owns(installed));            // consumed out of the pool (permanent)
}

// MODS overwrite confirm (D3): equipping over a slot that already holds a
// different mod opens the inline Cancel/Confirm "discards {current} — permanent"
// warning. Confirm installs the new mod and DISCARDS the old one; Cancel leaves the
// slot unchanged and the spare un-consumed.
static void test_mods_overwrite_confirm() {
    // Confirm path. Slot 0 holds Firewall Patch; selecting a spare opens its detail
    // and EQUIP raises the overwrite confirm back on the picker.
    { Game g{StartMode::Hatched};
      enterSubmenuId(g, SubmenuId::Mods);          // slot 1 holds Firewall Patch
      CHECK(std::strcmp(g.loadout().equipped(0), "firewall_patch") == 0);
      g.onButton(press(Button::B));                // open slot 1 picker (rows = spares)
      const char* spare = g.loadout().owned().front().id;  // first available spare
      g.onButton(press(Button::B));                // open the spare's detail
      g.onButton(press(Button::B));                // EQUIP -> inline confirm on picker
      CHECK(g.nav() == Game::Nav::Detail);         // still in the picker (confirm up)
      Framebuffer fb(kActiveW, kActiveH); g.render(fb);
      CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));
      g.onButton(press(Button::A));                // Cancel -> Confirm
      g.onButton(press(Button::B));                // commit
      CHECK(g.nav() == Game::Nav::Submenu);
      CHECK(std::strcmp(g.loadout().equipped(0), spare) == 0);   // new mod installed
      CHECK(g.loadout().slotOf("firewall_patch") == -1);         // old mod discarded
      CHECK(!g.loadout().owns("firewall_patch")); }              // not returned to pool

    // Cancel path leaves the slot unchanged and the spare un-consumed.
    { Game g{StartMode::Hatched};
      enterSubmenuId(g, SubmenuId::Mods);
      const char* spare = g.loadout().owned().front().id;
      g.onButton(press(Button::B));                // open slot 1 picker
      g.onButton(press(Button::B));                // open the spare's detail
      g.onButton(press(Button::B));                // EQUIP -> confirm (default Cancel)
      g.onButton(press(Button::B));                // B on Cancel -> abort
      CHECK(std::strcmp(g.loadout().equipped(0), "firewall_patch") == 0);
      CHECK(g.loadout().owns(spare)); }            // spare still held (not consumed)
}

// the mod detail spells out the effect + flags a ONE-SHOT. The one-shot
// caveat line (y~130) is drawn for a consumed-on-trigger mod and absent otherwise —
// the grayscale-safe channel that distinguishes the two mod kinds.
static void test_mod_detail_oneshot() {
    ContentRegistry r = ContentRegistry::embedded();
    const ModDef* oneShot = r.mod("raid_mirror");        // oneShot == true
    const ModDef* reuse = r.mod("firewall_patch");       // oneShot == false
    CHECK(oneShot && reuse);
    CHECK(oneShot->oneShot && !reuse->oneShot);

    Framebuffer os(kActiveW, kActiveH), ru(kActiveW, kActiveH);
    Loadout load;
    drawModDetail(os, r, load, *oneShot, /*equippedHere=*/false, /*slot=*/2,
                  /*reqLevel=*/0, /*petLevel=*/0, /*petLine=*/nullptr,
                  /*storageCap=*/kModCopyCapBase);
    drawModDetail(ru, r, load, *reuse, /*equippedHere=*/false, /*slot=*/2,
                  /*reqLevel=*/0, /*petLevel=*/0, /*petLine=*/nullptr,
                  /*storageCap=*/kModCopyCapBase);
    CHECK(anyNonPaper(os, 0, 0, kActiveW, kActiveH));     // renders
    // The ONE-SHOT line sits alone at y~130; present (non-paper ink) for the one-shot,
    // blank (all paper) for the reusable mod (whose effect text ends well above it).
    CHECK(anyNonPaper(os, 0, 128, kActiveW, 140));
    CHECK(!anyNonPaper(ru, 0, 128, kActiveW, 140));
}

// A mod holds at most one slot per pet: equip() refuses to install an id already
// occupying a different slot, and doesn't consume a spare on refusal.
static void test_loadout_one_slot_per_mod() {
    Loadout l;
    CHECK(l.grant("firewall_patch", kModCopyCapBase));
    CHECK(l.grant("firewall_patch", kModCopyCapBase));
    CHECK(l.countOf("firewall_patch") == 2);
    l.equip(0, "firewall_patch");
    CHECK(std::strcmp(l.equipped(0), "firewall_patch") == 0);
    CHECK(l.countOf("firewall_patch") == 1);

    l.equip(1, "firewall_patch");
    CHECK(l.equipped(1) == nullptr);
    CHECK(l.countOf("firewall_patch") == 1);
}

// mod effects are DATA-DRIVEN (effectKind + magnitude) — makePlayerCombatant
// reads each equipped mod and pokes the matching Combatant field, replacing the old
// hardcoded per-mod `if`. A line-affinity mod adds its bonus only for a matching line.
static void test_mod_effects_data_driven() {
    ContentRegistry r = ContentRegistry::embedded();
    const CreatureDef* pet = r.creature("paypup");        // Process, ransomware line
    CHECK(pet && pet->line && std::strcmp(pet->line, "ransomware") == 0);
    MoveLoadout ml = MoveLoadout::starting();
    Loadout empty;                                        // no slots filled
    Combatant base = makePlayerCombatant(r, *pet, ml, empty);
    auto build = [&](const char* mod) {
        Loadout l; l.grant(mod, kModCopyCapBase); l.equip(0, mod);
        return makePlayerCombatant(r, *pet, ml, l);
    };
    CHECK(build("crypto_coprocessor").powerMultPct == base.powerMultPct + 10);
    CHECK(build("tpm_chip").dmgReducePct == base.dmgReducePct + 15);
    CHECK(build("solid_state_cache").maxHealth == base.maxHealth + 12);
    Combatant oc = build("overclock_chip");
    CHECK(oc.speed == base.speed + 5);                                  // +speed
    CHECK(oc.powerMultPct == base.powerMultPct * (100 - 8) / 100);      // ...at a power cost
    CHECK(build("honeytoken").mods.mag(ModEffect::Thorns) == 4);
    CHECK(build("deadman_switch").mods.mag(ModEffect::DeathBlast) == 12);
    // Line affinity: Cipher ASIC is +10% cut generic, +10 more for a Ransomware pet (=20).
    CHECK(build("cipher_asic").dmgReducePct == base.dmgReducePct + 20);
    // The plain-magnitude originals apply their row value straight through.
    CHECK(build("firewall_patch").dmgReducePct == base.dmgReducePct + r.mod("firewall_patch")->magnitude);
    CHECK(build("clock_speed_boost").speed == base.speed + r.mod("clock_speed_boost")->magnitude);
    CHECK(build("raid_mirror").mods.armed(ModEffect::RaidMirror));
}

// Backup Drive's death-save (ItemEffect::ArmCombatShieldBuff, save v30) is NOT the RAID
// Mirror's negate-the-first-hit: a survivable hit lands in full and leaves the drive
// untouched, so it's still there for the blow that actually puts the pet down.
static void test_backup_drive_death_save_ignores_survivable_hits() {
    ContentRegistry r = ContentRegistry::embedded();
    Combatant pc = mkCombatant(r, "P", 100, 5, {"quick_jab"});
    pc.itemShield = true;
    Combatant e = mkCombatant(r, "E", 100, 12, {"packet_storm"});  // fast → hits first
    Combat c; c.begin(pc, e, Combat::Stakes::Safe, 5);             // full Health: survivable
    c.step();
    CHECK(c.player().health < 100);              // the hit landed in full...
    CHECK(c.player().health > 0);
    CHECK(c.player().itemShield);                // ...and the drive is still held
    CHECK(c.player().backupUse == Combatant::BackupUse::None);
}

// A pet that goes down is restored with half its MAX Health, measured from where it
// actually landed — so the same drive gives back the same amount whatever knocked it
// over. Records BackupUse::Restored (unlike mirrorFired, not reset per-turn) so Game can
// clear the buff's timed save-side deadline once the fight ends.
static void test_backup_drive_death_save_restores_half_max_health() {
    ContentRegistry r = ContentRegistry::embedded();
    Combatant pc = mkCombatant(r, "P", 100, 5, {"quick_jab"});
    pc.itemShield = true;
    Combatant e = mkCombatant(r, "E", 100, 12, {"quick_jab"});
    // begin() always resets Health to maxHealth unless told to carry a wounded value in
    // (the gauntlet no-heal-between-rounds path) — use that to start on death's door.
    Combat c; c.begin(pc, e, Combat::Stakes::Safe, 5, /*forceEnemyFirst=*/false,
                       /*carryPlayerHealth=*/1);
    const int jab = r.move("quick_jab")->power;
    c.step();
    // 1 Health, minus the jab that took it under, plus the drive's half of max (50).
    CHECK(c.player().health == 1 - jab + 50);
    CHECK(c.player().health > 0);
    CHECK(!c.player().itemShield);               // spent
    CHECK(c.player().backupUse == Combatant::BackupUse::Restored);  // Game reads this post-fight
    CHECK(c.outcome() != Combat::Outcome::Lose); // the fight did NOT end here
}

// The save asks one question — is this pet down? — so it needs to know nothing about
// what put it there: a fatal DoT tick at turn-start restores exactly like a fatal hit.
static void test_backup_drive_death_save_covers_a_fatal_dot() {
    ContentRegistry r = ContentRegistry::embedded();
    Combatant pc = mkCombatant(r, "P", 100, 12, {"quick_jab"});   // fast → its turn first
    pc.itemShield = true;
    pc.dotPerTurn = 8;
    pc.dotTurnsLeft = 3;
    Combatant e = mkCombatant(r, "E", 100, 5, {"quick_jab"});
    Combat c; c.begin(pc, e, Combat::Stakes::Safe, 5, /*forceEnemyFirst=*/false,
                       /*carryPlayerHealth=*/5);
    c.step();                                    // the pet's turn opens with the rot tick
    CHECK(c.player().health == 5 - 8 + 50);      // rotted under, then restored
    CHECK(c.player().backupUse == Combatant::BackupUse::Restored);
    CHECK(c.outcome() != Combat::Outcome::Lose);
}

// ...and it is a restore, not immortality: half of max added to a deep enough hole still
// leaves the pet under, and it dies. This is the case that keeps the save honest — it
// falls straight out of adding a fixed amount rather than setting a floor.
static void test_backup_drive_death_save_loses_to_overkill() {
    ContentRegistry r = ContentRegistry::embedded();
    Combatant pc = mkCombatant(r, "P", 100, 12, {"quick_jab"});
    pc.itemShield = true;
    pc.dotPerTurn = 90;                          // 5 - 90 = -85, past the drive's 50
    pc.dotTurnsLeft = 3;
    Combatant e = mkCombatant(r, "E", 100, 5, {"quick_jab"});
    Combat c; c.begin(pc, e, Combat::Stakes::Safe, 5, /*forceEnemyFirst=*/false,
                       /*carryPlayerHealth=*/5);
    c.step();
    CHECK(c.player().health == 0);               // clamped at the floor, and gone
    CHECK(c.player().backupUse == Combatant::BackupUse::Overwhelmed);  // spent trying
    CHECK(c.outcome() == Combat::Outcome::Lose);
}

// Thorns (Honeytoken) reflects onto any attacker; Deadman Switch blasts the
// enemy when the pet is KO'd (a mutual KO resolves as a Win via enemy-death priority).
static void test_mod_thorns_and_deathblast() {
    ContentRegistry r = ContentRegistry::embedded();
    // Thorns: a fast enemy hits the player and takes the reflect back.
    Combatant pc = mkCombatant(r, "P", 100, 5, {"quick_jab"});
    pc.mods.arm(ModEffect::Thorns, 4);
    Combatant enemy = mkCombatant(r, "E", 100, 12, {"quick_jab"});     // fast → acts first
    Combat c; c.begin(pc, enemy, Combat::Stakes::Safe, 5);
    c.step();                                                          // enemy hits player
    CHECK(c.player().health < 100);                                    // player took the hit
    CHECK(c.enemy().health == 96);                                     // ...reflected 4 back
    // Deadman Switch: a hit that KOs the pet blasts the enemy for deathBlast. A blast
    // that also drops the enemy is a mutual KO → resolves as a Win (enemy-death priority).
    Combatant weak = mkCombatant(r, "P", 5, 5, {"quick_jab"});
    weak.mods.arm(ModEffect::DeathBlast, 12);
    Combatant strong = mkCombatant(r, "E", 12, 12, {"packet_storm"});  // 12 dmg → KOs the 5hp pet
    Combat d; d.begin(weak, strong, Combat::Stakes::Safe, 5);
    d.step();                                                          // enemy KO → deathblast
    CHECK(d.player().health == 0);
    CHECK(d.enemy().health == 0);                                      // 12 - 12 parting blast
    CHECK(d.outcome() == Combat::Outcome::Win);                        // mutual KO = a Win
    // A blast that does NOT finish the enemy is still a Loss (no free win).
    Combatant weak2 = mkCombatant(r, "P", 5, 5, {"quick_jab"});
    weak2.mods.arm(ModEffect::DeathBlast, 12);
    Combatant tank = mkCombatant(r, "E", 40, 12, {"packet_storm"});    // survives the 12 blast
    Combat d2; d2.begin(weak2, tank, Combat::Stakes::Safe, 5);
    d2.step();
    CHECK(d2.enemy().health == 28);                                    // 40 - 12
    CHECK(d2.outcome() == Combat::Outcome::Lose);
}

// Deferred-mod pass — ECC Memory: a max single-hit cap. The primitive clamps any ONE
// incoming hit to the cap (after all mitigation); makePlayerCombatant resolves the
// mod's magnitude% of max Health into that flat cap.
static void test_mod_ecc_memory_hitcap() {
    ContentRegistry r = ContentRegistry::embedded();
    // Primitive: a 20-power enemy hit is clamped to the cap; without the cap it lands full.
    auto oneHit = [&](int cap) {
        Combatant pc = mkCombatant(r, "P", 100, 5, {"quick_jab"});
        pc.mods.arm(ModEffect::MaxHitCapPct, cap);
        Combatant e = mkCombatant(r, "E", 100, 12, {"buffer_overflow"});  // 20 dmg, acts first
        Combat c; c.begin(pc, e, Combat::Stakes::Safe, 5);
        c.step();                                                          // enemy hits player
        return c.player().health;
    };
    CHECK(oneHit(0) == 80);      // uncapped: 100 - 20
    CHECK(oneHit(15) == 85);     // capped at 15: 100 - 15
    CHECK(oneHit(30) == 80);     // cap above the hit is inert (still 100 - 20)
    // Data-driven wiring: equipping ecc_memory resolves cap = 35% of the pet's max Health.
    const CreatureDef* pet = r.creature("paypup");
    MoveLoadout ml = MoveLoadout::starting();
    Loadout empty;
    Combatant base = makePlayerCombatant(r, *pet, ml, empty);
    CHECK(base.mods.mag(ModEffect::MaxHitCapPct) == 0);                   // no mod → uncapped
    Loadout l; l.grant("ecc_memory", kModCopyCapBase); l.equip(0, "ecc_memory");
    Combatant ecc = makePlayerCombatant(r, *pet, ml, l);
    CHECK(ecc.mods.mag(ModEffect::MaxHitCapPct) == base.maxHealth * 35 / 100);
    CHECK(ecc.mods.mag(ModEffect::MaxHitCapPct) > 0);
}

// Deferred-mod pass — Load Balancer: a big hit is SPLIT (immediate portion now, the rest
// deferred to the victim's next turn-start). Unlike ECC it doesn't reduce total damage.
static void test_mod_load_balancer_split() {
    ContentRegistry r = ContentRegistry::embedded();
    // Split + timing: a 20 hit (>= threshold 10) with split 50% lands 10 now, 10 next turn.
    Combatant pc = mkCombatant(r, "P", 100, 5, {"quick_jab"});
    pc.mods.arm(ModEffect::LoadBalance, 10, 50);
    Combatant e = mkCombatant(r, "E", 100, 5, {"buffer_overflow"});        // 20 dmg
    // Equal speed + forceEnemyFirst → strict E,P,E,P (enemy leads, no double-actions).
    Combat c; c.begin(pc, e, Combat::Stakes::Safe, 5, /*forceEnemyFirst=*/true);
    c.step();                                                              // enemy hits player
    CHECK(c.player().health == 90);                                        // only the immediate half
    c.step();                                                              // player's turn-start tick
    CHECK(c.player().health == 80);                                        // deferred half now due
    CHECK(c.enemy().health == 94);                                         // ...and the pet still acted (jab 6)
    // Below the threshold: the hit is NOT split — it lands full immediately, no debt.
    Combatant pc2 = mkCombatant(r, "P", 100, 5, {"quick_jab"});
    pc2.mods.arm(ModEffect::LoadBalance, 25, 50);                          // 20 < 25 → untouched
    Combat c2; c2.begin(pc2, e, Combat::Stakes::Safe, 5, /*forceEnemyFirst=*/true);
    c2.step();  CHECK(c2.player().health == 80);                           // full 20 now
    c2.step();  CHECK(c2.player().health == 80);                           // no deferred tick
    // The deferred debt can KO ON ITS OWN at turn-start — and the actor then doesn't act.
    Combatant weak = mkCombatant(r, "P", 12, 5, {"quick_jab"});
    weak.mods.arm(ModEffect::LoadBalance, 10, 50);
    Combat d; d.begin(weak, e, Combat::Stakes::Safe, 5, /*forceEnemyFirst=*/true);
    d.step();                                                              // 20 → 10 now (12→2), 10 deferred
    CHECK(d.player().health == 2 && d.outcome() == Combat::Outcome::Ongoing);
    d.step();                                                              // turn-start debt: 2 - 10 → KO
    CHECK(d.player().health == 0);
    CHECK(d.outcome() == Combat::Outcome::Lose);
    CHECK(d.enemy().health == 100);                                        // pet never got to swing
    CHECK(std::strcmp(d.lastMoveName(), "OVERLOAD") == 0);
    // Data-driven wiring: equipping load_balancer resolves threshold = 30% of max Health,
    // split = 50%; an unmodded pet has no threshold (feature off).
    const CreatureDef* pet = r.creature("paypup");
    MoveLoadout ml = MoveLoadout::starting();
    Loadout empty;
    Combatant base = makePlayerCombatant(r, *pet, ml, empty);
    CHECK(base.mods.mag(ModEffect::LoadBalance) == 0);
    Loadout l; l.grant("load_balancer", kModCopyCapBase); l.equip(0, "load_balancer");
    Combatant lb = makePlayerCombatant(r, *pet, ml, l);
    CHECK(lb.mods.mag(ModEffect::LoadBalance) == base.maxHealth * 30 / 100);
    CHECK(lb.mods.mag(ModEffect::LoadBalance) > 0);
    CHECK(lb.mods.mag2(ModEffect::LoadBalance) == 50);
}

// Deferred-mod pass — Watchdog Timer: the THREAT is a STUN move (system_hang, lockTurns)
// that freezes the player's turns; the MOD clamps how many turns a stun may last (a hung
// process reboots after N turns).
static void test_mod_watchdog_timer() {
    ContentRegistry r = ContentRegistry::embedded();
    // Burn: a pre-set 2-turn stun costs exactly 2 turns and deals NO damage in either
    // direction — the enemy is untouched until the pet is free.
    Combatant pc = mkCombatant(r, "P", 100, 12, {"quick_jab"});            // acts first
    pc.lockedTurnsLeft = 2;
    Combatant e = mkCombatant(r, "E", 100, 12, {"quick_jab"});            // equal → P,E,P,E,P
    Combat c; c.begin(pc, e, Combat::Stakes::Safe, 5);
    c.step();  CHECK(c.enemy().health == 100);                            // turn 1 skipped
    CHECK(std::strcmp(c.lastMoveName(), "STUN LOCK") == 0);
    c.step();                                                             // enemy hits pet
    c.step();  CHECK(c.enemy().health == 100);                            // turn 2 still skipped
    CHECK(std::strcmp(c.lastMoveName(), "STUN LOCK") == 0);
    c.step();                                                             // enemy hits pet
    c.step();  CHECK(c.enemy().health == 94);                             // freed → jab lands (6)
    // Clamp at application: the STUN move sets the lock to lockTurns (2), but the pet's
    // Watchdog clamps it to its own magnitude. Read the live lock right after the hit lands.
    auto lockAfterHit = [&](int watchdog) {
        Combatant p = mkCombatant(r, "P", 100, 5, {"quick_jab"});         // slow → stunned
        p.mods.arm(ModEffect::WatchdogClamp, watchdog);
        Combatant s = mkCombatant(r, "E", 100, 12, {"system_hang"});      // lockTurns 2, first
        Combat cb; cb.begin(p, s, Combat::Stakes::Safe, 5);
        cb.step();                                                        // enemy stuns the pet
        return cb.player().lockedTurnsLeft;
    };
    CHECK(lockAfterHit(0) == 2);                                          // no watchdog: full 2
    CHECK(lockAfterHit(1) == 1);                                          // watchdog clamps to 1
    // Data-driven wiring: equipping watchdog_timer sets the clamp to its magnitude (1).
    const CreatureDef* pet = r.creature("paypup");
    MoveLoadout ml = MoveLoadout::starting();
    Loadout empty;
    Combatant base = makePlayerCombatant(r, *pet, ml, empty);
    CHECK(base.mods.mag(ModEffect::WatchdogClamp) == 0);
    Loadout l; l.grant("watchdog_timer", kModCopyCapBase); l.equip(0, "watchdog_timer");
    Combatant w = makePlayerCombatant(r, *pet, ml, l);
    CHECK(w.mods.mag(ModEffect::WatchdogClamp) == 1);
}

// Deferred-mod pass — Faraday Cage: the THREAT is a DoT move (data_rot, dot*) that plants
// corruption ticking at each of the victim's turn-starts; the MOD cuts (or negates) it.
static void test_mod_faraday_cage() {
    ContentRegistry r = ContentRegistry::embedded();
    // DoT tick: a pre-set 5/turn × 3 DoT bites at the START of each of the pet's next 3 turns,
    // independent of acting. Enemy only DEFENDS, so the pet's Health drops purely from the DoT.
    Combatant pc = mkCombatant(r, "P", 100, 12, {"quick_jab"});           // acts first
    pc.dotPerTurn = 5; pc.dotTurnsLeft = 3;
    Combatant e = mkCombatant(r, "E", 200, 5, {"checksum_guard"});        // never hits back
    Combat c; c.begin(pc, e, Combat::Stakes::Safe, 5);
    c.step();  CHECK(c.player().health == 95);                            // tick 1 (-5)
    c.step();                                                             // enemy defends
    c.step();  CHECK(c.player().health == 90);                            // tick 2
    c.step();
    c.step();  CHECK(c.player().health == 85);                            // tick 3
    c.step();
    c.step();  CHECK(c.player().health == 85);                            // DoT spent — no tick 4
    // Faraday cut at application: the DoT magnitude is reduced (or zeroed) when it's PLANTED.
    auto dotAfterHit = [&](int cut) {
        Combatant p = mkCombatant(r, "P", 100, 5, {"quick_jab"});
        p.mods.arm(ModEffect::FaradayCut, cut);
        Combatant s = mkCombatant(r, "E", 100, 12, {"data_rot"});         // 5/turn, acts first
        Combat cb; cb.begin(p, s, Combat::Stakes::Safe, 5);
        cb.step();                                                        // enemy plants the DoT
        return cb.player().dotPerTurn;
    };
    CHECK(dotAfterHit(0) == 5);                                           // no cage: full 5/turn
    CHECK(dotAfterHit(50) == 2);                                          // 50% cut: 5*50/100
    CHECK(dotAfterHit(100) == 0);                                         // immune: nothing planted
    // The DoT can KO on its own at a turn-start (and the pet then doesn't act).
    Combatant weak = mkCombatant(r, "P", 8, 12, {"quick_jab"});
    weak.dotPerTurn = 5; weak.dotTurnsLeft = 3;
    Combatant tank = mkCombatant(r, "E", 200, 5, {"checksum_guard"});
    Combat d; d.begin(weak, tank, Combat::Stakes::Safe, 5);
    d.step();  CHECK(d.player().health == 3 && d.outcome() == Combat::Outcome::Ongoing);
    d.step();                                                             // enemy defends
    d.step();  CHECK(d.player().health == 0);                             // 3 - 5 → KO
    CHECK(d.outcome() == Combat::Outcome::Lose);
    CHECK(std::strcmp(d.lastMoveName(), "CORRUPTED") == 0);
    // Data-driven wiring: equipping faraday_cage sets the cut to its magnitude (100 = immune).
    const CreatureDef* pet = r.creature("paypup");
    MoveLoadout ml = MoveLoadout::starting();
    Loadout empty;
    Combatant base = makePlayerCombatant(r, *pet, ml, empty);
    CHECK(base.mods.mag(ModEffect::FaradayCut) == 0);
    Loadout l; l.grant("faraday_cage", kModCopyCapBase); l.equip(0, "faraday_cage");
    Combatant f = makePlayerCombatant(r, *pet, ml, l);
    CHECK(f.mods.mag(ModEffect::FaradayCut) == 100);
}

// content calibration: each mod carries the right rarity, its power tier agrees with
// where it actually DROPS, and the signature Cipher ASIC is the ransomware-affinity mod.
static void test_mod_content_rarity_tier() {
    ContentRegistry r = ContentRegistry::embedded();
    struct Exp { const char* id; ItemDef::Rarity rar; };
    const Exp exp[] = {
        {"clock_speed_boost", ItemDef::Rarity::Common},
        {"packet_sniffer", ItemDef::Rarity::Common},
        {"crypto_coprocessor", ItemDef::Rarity::Uncommon},
        {"tpm_chip", ItemDef::Rarity::Uncommon},
        {"solid_state_cache", ItemDef::Rarity::Uncommon},
        {"firewall_patch", ItemDef::Rarity::Rare},
        {"watchdog_timer", ItemDef::Rarity::Epic},  // threat-adjacent (Pirate Bayou)
        {"overclock_chip", ItemDef::Rarity::Uncommon},
        {"heat_sink", ItemDef::Rarity::Rare},
        {"honeytoken", ItemDef::Rarity::Rare},
        {"cipher_asic", ItemDef::Rarity::Rare},
        {"faraday_cage", ItemDef::Rarity::Epic},    // threat-adjacent (Napstorrent)
        {"deadman_switch", ItemDef::Rarity::Epic},
        {"raid_mirror", ItemDef::Rarity::Epic},
        {"ecc_memory", ItemDef::Rarity::Epic},
        {"load_balancer", ItemDef::Rarity::Epic},
        // --- Niche-flavour mods ---
        {"canary_trap", ItemDef::Rarity::Rare},
        {"scratch_disk_buffer", ItemDef::Rarity::Common},
        {"botnet_swarm", ItemDef::Rarity::Uncommon},
        {"airgap_ward", ItemDef::Rarity::Uncommon},
        {"tripwire", ItemDef::Rarity::Rare},
        {"cold_storage", ItemDef::Rarity::Uncommon},
        {"prowlware", ItemDef::Rarity::Rare},
        {"meltdown_core", ItemDef::Rarity::Rare},
        {"zero_day_exploit", ItemDef::Rarity::Rare},
        {"phishing_rod", ItemDef::Rarity::Epic},
        {"extortion_ledger", ItemDef::Rarity::Epic},
        {"backup_uplink", ItemDef::Rarity::Rare},
    };
    for (const Exp& e : exp) {
        const ModDef* m = r.mod(e.id);
        CHECK(m);
        CHECK(m->rarity == e.rar);
    }
    // powerTier is NOT restated above, because it is not independent data: a rank IS a
    // ladder depth, so a mod's rank must equal the tier of the SHALLOWEST area whose pool
    // drops it. (Shallowest, not only: a deeper area may re-stock an earlier counter — the
    // keep does it for Watchdog and Faraday, the Net-Sea for Watchdog — and that restock
    // must not restate the mod as harder than where it first appears.) Checked by walking
    // the pools, so inserting or reordering an area is caught here as a contradiction
    // rather than passing on a table nobody re-derived.
    for (const ModDef* mp : r.allMods()) {
        const ModDef& m = *mp;
        int home = -1;
        for (int a = 0; a < kExplSectors && home < 0; ++a)
            for (int k = 0; k < area(a).modPoolCount; ++k)
                if (std::strcmp(area(a).modPoolIds[k], m.id) == 0) { home = areaTier(a); break; }
        if (home < 0)                                  // DeepWeb-only: one past the ladder,
            for (int k = 0; k < kAreaModsDeepWebCount; ++k)   // sharing the last rung's depth
                if (std::strcmp(kAreaModsDeepWeb[k], m.id) == 0) { home = areaTier(kAreaCount - 1); break; }
        CHECK(home > 0);                               // every mod drops SOMEWHERE
        CHECK(m.powerTier == home);
    }
    const ModDef* ca = r.mod("cipher_asic");
    CHECK(ca && ca->line && std::strcmp(ca->line, "ransomware") == 0);
    CHECK(ca->affinityBonus == 10);
    // Niche-flavour pass: the two hard-gated signatures carry ModDef::requiresLine (a
    // real EQUIP block, distinct from the soft `line`/`affinityBonus` every other mod
    // uses — Cipher ASIC above stays fully line-agnostic).
    const ModDef* pr = r.mod("phishing_rod");
    CHECK(pr && pr->requiresLine && std::strcmp(pr->requiresLine, "phishing") == 0);
    const ModDef* el = r.mod("extortion_ledger");
    CHECK(el && el->requiresLine && std::strcmp(el->requiresLine, "ransomware") == 0);
    // Every OTHER mod carries no hard gate.
    for (const ModDef* m : r.allMods()) {
        if (std::strcmp(m->id, "phishing_rod") == 0 || std::strcmp(m->id, "extortion_ledger") == 0)
            continue;
        CHECK(m->requiresLine == nullptr);
    }
}

// earn path: an area's mod loot table draws ONLY that area's power tier (an
// early area never hands out a late mod), and grantMod rolls the per-instance
// equip-level gate inside the tier's band (the "lucky roll" mechanic).
static void test_mod_earn_tables_and_reqlevel() {
    ContentRegistry r = ContentRegistry::embedded();
    Game g{StartMode::Hatched};
    // An area's table never rolls a mod from DEEPER than that area: a rung may re-stock
    // an earlier counter (the keep does, for both), but nothing hands out a mod from
    // further down the ladder than the player has reached. Walked off the ladder, so an
    // insert re-derives the bound instead of failing on a stale index.
    auto sampled = std::set<std::string>{};
    for (int i = 0; i < 120; ++i) {
        for (int a = 0; a < kExplSectors; ++a) {
            const char* id = g.debugRollAreaModId(a);
            CHECK(id);
            CHECK(r.mod(id)->powerTier <= areaTier(a));
            sampled.insert(id);
        }
        const char* dw = g.debugRollAreaModId(kDeepWebSector);
        CHECK(dw);
        CHECK(r.mod(dw)->powerTier == areaTier(kAreaCount - 1));  // DeepWeb: deepest only
        sampled.insert(dw);
    }
    // Every pooled mod is actually REACHABLE — the tier check above would pass just as
    // happily on a pool whose ids never came up, so assert the sample covers the union of
    // every table. That subsumes the per-id spot-checks this used to spell out (ECC, Load
    // Balancer, Watchdog, Faraday, Ghost Process, the niche-flavour ids), and it keeps
    // covering a new area's pool the day it lands without a line added here.
    for (int a = 0; a < kExplSectors; ++a)
        for (int k = 0; k < area(a).modPoolCount; ++k)
            CHECK(sampled.count(area(a).modPoolIds[k]) == 1);
    for (int k = 0; k < kAreaModsDeepWebCount; ++k)
        CHECK(sampled.count(kAreaModsDeepWeb[k]) == 1);
    // A mod's equip gate is its OWN, off its power tier, and the same for every copy —
    // there is no roll to land inside a band. Sampled off two real mods at opposite ends
    // of the ladder rather than named tiers, and asserted to be deterministic, which is
    // the property that replaced the roll.
    const ModDef* deep = r.mod("ghost_process");
    const ModDef* shallow = r.mod("crypto_coprocessor");
    CHECK(modEquipLevel(*deep) == modEquipLevelFloor(deep->powerTier));
    CHECK(modEquipLevel(*shallow) == 0);            // the shallowest rung
    CHECK(modEquipLevel(*deep) > modEquipLevel(*shallow));
    for (int i = 0; i < 8; ++i) {                   // every grant agrees with the row
        Game gd{StartMode::Hatched};
        gd.debugGrantMod("ghost_process");
        CHECK(gd.loadout().countOf("ghost_process") == 1);
        CHECK(modEquipLevel(*r.mod("ghost_process")) == modEquipLevel(*deep));
    }
}

// --- Niche-flavour mods -----------------------------------------------------------

// Data-driven wiring: makePlayerCombatant reads each new ModEffect kind and pokes the
// matching Combatant field, same idiom as test_mod_effects_data_driven above.
static void test_mod_niche_flavour_data_driven() {
    ContentRegistry r = ContentRegistry::embedded();
    const CreatureDef* pet = r.creature("paypup");         // Process, ransomware line
    MoveLoadout ml = MoveLoadout::starting();
    Loadout empty;
    Combatant base = makePlayerCombatant(r, *pet, ml, empty);
    auto build = [&](const char* mod) {
        Loadout l; l.grant(mod, kModCopyCapBase); l.equip(0, mod);
        return makePlayerCombatant(r, *pet, ml, l);
    };
    CHECK(build("prowlware").mods.armed(ModEffect::FirstStrikeRankMult));
    CHECK(build("canary_trap").mods.mag(ModEffect::FirstHitCutPct) == 50);
    Combatant mc = build("meltdown_core");
    CHECK(mc.mods.mag(ModEffect::LowHealthPowerPct) == 30 &&
          mc.mods.mag2(ModEffect::LowHealthPowerPct) == 40);
    Combatant zd = build("zero_day_exploit");
    CHECK(zd.mods.mag(ModEffect::GambleBattlePowerPct) == 25 &&
          zd.mods.mag2(ModEffect::GambleBattlePowerPct) == 60);
    Combatant tw = build("tripwire");
    CHECK(tw.mods.mag(ModEffect::ConditionalThorns) == 10 &&
          tw.mods.mag2(ModEffect::ConditionalThorns) == 40);
    Combatant cs = build("cold_storage");
    CHECK(cs.maxHealth == base.maxHealth + 20);
    CHECK(cs.speed == base.speed - 2);
    CHECK(build("scratch_disk_buffer").dmgReducePct == base.dmgReducePct + 8);
    CHECK(build("phishing_rod").mods.mag(ModEffect::StealAmplifyPct) == 75);
    // Extortion Ledger's requiresLine gate is an EQUIP-time UI check (game_care.cpp),
    // not a combat-engine one — makePlayerCombatant applies whatever is already
    // installed, so it correctly still wires up here even off-line (see
    // test_mod_hard_line_gate for the actual gate enforcement).
    CHECK(build("extortion_ledger").powerMultPct == base.powerMultPct + 30);
    // Backup Uplink is read post-battle by the Game (like Packet Sniffer), not stored
    // on Combatant at all — assert its content data directly instead.
    const ModDef* bu = r.mod("backup_uplink");
    CHECK(bu && bu->effectKind == ModEffect::PostBattleBits && bu->magnitude == 20);
}

// Botnet Swarm / Air-Gap Ward: bonus scales with the COUNT of equipped Attack/Defend
// moves — built with an explicit loadout so the composition (2 Attack, 1 Defend) is
// self-evident rather than relying on MoveLoadout::starting()'s defaults.
static void test_mod_botnet_swarm_and_airgap_ward() {
    ContentRegistry r = ContentRegistry::embedded();
    const CreatureDef* pet = r.creature("malbear");        // Script, 3 unlocked slots
    CHECK(pet);
    MoveLoadout ml = MoveLoadout::starting();
    ml.equip(0, "packet_storm");    // Attack
    ml.equip(1, "fork_bomb");       // Attack
    ml.equip(2, "checksum_guard");  // Defend
    Loadout empty;
    Combatant base = makePlayerCombatant(r, *pet, ml, empty);
    CHECK(base.moves.size() == 3);

    Loadout withSwarm; withSwarm.grant("botnet_swarm", kModCopyCapBase); withSwarm.equip(0, "botnet_swarm");
    Combatant sw = makePlayerCombatant(r, *pet, ml, withSwarm);
    CHECK(sw.powerMultPct == base.powerMultPct + 6 * 2);    // +6% per Attack move (2)

    Loadout withWard; withWard.grant("airgap_ward", kModCopyCapBase); withWard.equip(0, "airgap_ward");
    Combatant wd = makePlayerCombatant(r, *pet, ml, withWard);
    CHECK(wd.dmgReducePct == base.dmgReducePct + 6 * 1);    // +6% per Defend move (1)
}

// The per-kind combine rules (mod_state.cpp) — what happens when two equipped mods
// declare the same ModEffect. Asserted straight on ModStateSet::apply: the loadout can
// only reach a handful of these pairings with the shipped roster, but every rule has to
// hold for the next mod that lands on an existing kind.
static void test_mod_state_combine_rules() {
    // Sum: magnitudes accumulate.
    ModStateSet sum;
    sum.apply(ModEffect::Thorns, 4, 0);
    sum.apply(ModEffect::Thorns, 3, 0);
    CHECK(sum.mag(ModEffect::Thorns) == 7);

    // HighestMag: the stronger cut wins outright and brings its OWN mag2 with it, so a
    // weaker copy can neither dilute it nor swap in a mismatched payload.
    ModStateSet high;
    high.apply(ModEffect::LowHealthPowerPct, 30, 40);
    high.apply(ModEffect::LowHealthPowerPct, 20, 90);
    CHECK(high.mag(ModEffect::LowHealthPowerPct) == 30 &&
          high.mag2(ModEffect::LowHealthPowerPct) == 40);
    high.apply(ModEffect::LowHealthPowerPct, 45, 10);
    CHECK(high.mag(ModEffect::LowHealthPowerPct) == 45 &&
          high.mag2(ModEffect::LowHealthPowerPct) == 10);

    // ...and a capped kind never records past its ceiling.
    ModStateSet capped;
    capped.apply(ModEffect::FaradayCut, 150, 0);
    CHECK(capped.mag(ModEffect::FaradayCut) == 100);

    // LowestMag: the tightest guarantee wins, so stacking a second copy can only ever
    // narrow the ceiling. A 0 magnitude reads as "no ceiling yet", not as the tightest.
    ModStateSet low;
    CHECK(low.mag(ModEffect::MaxHitCapPct) == 0);          // absent reads as 0
    low.apply(ModEffect::MaxHitCapPct, 0, 0);              // declares nothing
    CHECK(low.mag(ModEffect::MaxHitCapPct) == 0);
    low.apply(ModEffect::MaxHitCapPct, 35, 0);
    low.apply(ModEffect::MaxHitCapPct, 50, 0);             // looser — loses
    CHECK(low.mag(ModEffect::MaxHitCapPct) == 35);
    low.apply(ModEffect::MaxHitCapPct, 20, 0);             // tighter — wins
    CHECK(low.mag(ModEffect::MaxHitCapPct) == 20);
    // The winner's mag2 rides along (Load Balancer's split share follows its threshold).
    ModStateSet lb;
    lb.apply(ModEffect::LoadBalance, 30, 50);
    lb.apply(ModEffect::LoadBalance, 20, 75);
    CHECK(lb.mag(ModEffect::LoadBalance) == 20 && lb.mag2(ModEffect::LoadBalance) == 75);

    // HighestMag2: Tripwire's threshold is its SECOND magnitude, so the widest window
    // wins and carries its own reflect amount.
    ModStateSet tw;
    tw.apply(ModEffect::ConditionalThorns, 10, 40);
    tw.apply(ModEffect::ConditionalThorns, 99, 25);
    CHECK(tw.mag(ModEffect::ConditionalThorns) == 10 &&
          tw.mag2(ModEffect::ConditionalThorns) == 40);

    // Replace: a gamble has no "better" pair to prefer, so the last one equipped defines it.
    ModStateSet gamble;
    gamble.apply(ModEffect::GambleBattlePowerPct, 25, 60);
    gamble.apply(ModEffect::GambleBattlePowerPct, 10, 90);
    CHECK(gamble.mag(ModEffect::GambleBattlePowerPct) == 10 &&
          gamble.mag2(ModEffect::GambleBattlePowerPct) == 90);

    // Arm + one-shot: a second copy adds no extra use, and spend() burns exactly one.
    ModStateSet arm;
    CHECK(!arm.armed(ModEffect::RaidMirror) && !arm.spend(ModEffect::RaidMirror));
    arm.apply(ModEffect::RaidMirror, 0, 0);
    arm.apply(ModEffect::RaidMirror, 0, 0);
    CHECK(arm.armed(ModEffect::RaidMirror));
    CHECK(arm.spend(ModEffect::RaidMirror));
    CHECK(!arm.armed(ModEffect::RaidMirror) && !arm.spend(ModEffect::RaidMirror));

    // A kind that folds into a base stat (or is read post-battle) keeps no entry at all.
    ModStateSet none;
    none.apply(ModEffect::PowerPct, 10, 0);
    none.apply(ModEffect::PostBattleBits, 10, 0);
    CHECK(none.size() == 0);

    // arm() stages a passive at resolved values, one-shots included, and re-arming the
    // same kind overwrites rather than stacking a second entry.
    ModStateSet direct;
    direct.arm(ModEffect::FirstHitCutPct, 50);
    CHECK(direct.mag(ModEffect::FirstHitCutPct) == 50 &&
          direct.armed(ModEffect::FirstHitCutPct));
    direct.arm(ModEffect::FirstHitCutPct, 20);
    CHECK(direct.mag(ModEffect::FirstHitCutPct) == 20 && direct.size() == 1);
}

// Prowlware's rank: distinct Attack-move power tiers, ascending (weakest = rank 1),
// resolved per slot — including the empty-slot Quick Jab fallback.
static void test_mod_prowlware_rank_computation() {
    ContentRegistry r = ContentRegistry::embedded();
    const CreatureDef* pet = r.creature("malbear");        // Script; slotKinds A,A,D,A
    CHECK(pet);
    MoveLoadout ml = MoveLoadout::starting();
    ml.equip(0, "quick_jab");       // power 6  -> weakest tier
    ml.equip(1, "packet_storm");    // power 12 -> strongest tier
    // slot 2 (Defend-typed) left unequipped -> falls back to quick_jab (Attack, power 6).
    Loadout l; l.grant("prowlware", kModCopyCapBase); l.equip(0, "prowlware");
    Combatant c = makePlayerCombatant(r, *pet, ml, l);
    CHECK(c.mods.armed(ModEffect::FirstStrikeRankMult));
    CHECK(c.moves.size() == 3);
    CHECK(attackPowerRank(c.moves, 0) == 1);   // quick_jab (6) -> weakest of 2 tiers
    CHECK(attackPowerRank(c.moves, 1) == 2);   // packet_storm (12) -> strongest
    CHECK(attackPowerRank(c.moves, 2) == 1);   // fallback quick_jab -> weakest
    CHECK(attackPowerRank(c.moves, 3) == 0);   // past the kit -> no rank
    CHECK(attackPowerRank(c.moves, -1) == 0);  // a hijacked cast indexes no slot
    // A Defend move never ranks, and a kit with one attack tier ranks 1 throughout (no
    // bonus) — the mod only pays off on a genuine power spread.
    Combatant flat = mkCombatant(r, "F", 100, 5, {"quick_jab", "checksum_guard"});
    CHECK(attackPowerRank(flat.moves, 0) == 1);
    CHECK(attackPowerRank(flat.moves, 1) == 0);
}

// Prowlware's runtime effect: the first landed damaging hit multiplies by rank, fires
// only once, and a fully-mirrored (0-damage) hit doesn't consume it.
static void test_mod_prowlware_combat_effect() {
    ContentRegistry r = ContentRegistry::embedded();
    // Three distinct Attack tiers, strongest in slot 0 (rank 3). The Exploit override
    // commands that slot both times, so the two hits are the same move and the only
    // difference between them is whether Prowlware is still armed.
    Combatant pc = mkCombatant(r, "P", 100, 12,
                               {"buffer_overflow", "quick_jab", "packet_storm"});
    pc.mods.arm(ModEffect::FirstStrikeRankMult);
    Combatant e = mkCombatant(r, "E", 100, 5, {"quick_jab"});
    Combat c; c.begin(pc, e, Combat::Stakes::Safe, 5, /*forceEnemyFirst=*/false,
                      /*carryPlayerHealth=*/-1, /*exploitUses=*/2);
    c.openOverride(); c.commitOverride(); c.step();                // forced buffer_overflow
    CHECK(c.enemy().health == 100 - 20 * 3);                       // rank-3 multiplied
    CHECK(!c.player().mods.armed(ModEffect::FirstStrikeRankMult)); // consumed
    const int afterFirst = c.enemy().health;
    while (!c.playerTurnNext()) c.step();                          // let the enemy swing
    c.openOverride(); c.commitOverride(); c.step();                // the same move again
    CHECK(c.enemy().health == afterFirst - 20);                    // plain damage, no repeat

    Combatant pc2 = mkCombatant(r, "P", 100, 12, {"quick_jab"});
    pc2.mods.arm(ModEffect::FirstStrikeRankMult);
    Combatant e2 = mkCombatant(r, "E", 100, 5, {"quick_jab"});
    e2.mods.arm(ModEffect::RaidMirror);                           // negates the first hit
    Combat c2; c2.begin(pc2, e2, Combat::Stakes::Safe, 5);
    c2.step();
    CHECK(c2.enemy().health == 100);                              // fully mirrored
    CHECK(c2.player().mods.armed(ModEffect::FirstStrikeRankMult));  // not consumed
}

// Canary Trap: an extra cut on the FIRST hit taken this fight only.
static void test_mod_canary_trap() {
    ContentRegistry r = ContentRegistry::embedded();
    Combatant pc = mkCombatant(r, "P", 100, 5, {"quick_jab"});    // slow, acts second
    pc.mods.arm(ModEffect::FirstHitCutPct, 50);
    Combatant e = mkCombatant(r, "E", 100, 12, {"packet_storm"}); // fast, power 12
    Combat c; c.begin(pc, e, Combat::Stakes::Safe, 5);
    c.step();                                                     // enemy's first hit
    CHECK(c.player().health == 100 - 6);                          // 12 cut 50% -> 6
    CHECK(!c.player().mods.armed(ModEffect::FirstHitCutPct));   // absorbed
    c.step();                                                     // player's turn
    c.step();                                                     // enemy's second hit — no cut
    CHECK(c.player().health == 100 - 6 - 12);
}

// Meltdown Core: attack power gains the comeback bonus only while at/under the
// health threshold — checked live every attack, not a one-shot.
static void test_mod_meltdown_core() {
    ContentRegistry r = ContentRegistry::embedded();
    Combatant low = mkCombatant(r, "P", 100, 12, {"quick_jab"});
    low.health = 25;                                              // 25% <= the 30% threshold
    low.mods.arm(ModEffect::LowHealthPowerPct, 30, 40);
    Combatant e1 = mkCombatant(r, "E", 100, 5, {"quick_jab"});
    // begin() otherwise starts the player at full Health — carryPlayerHealth is what
    // actually seats the pre-set low.health (25) for the fight.
    Combat c1; c1.begin(low, e1, Combat::Stakes::Safe, 5, false, 25);
    c1.step();
    CHECK(c1.enemy().health == 100 - 6 * 140 / 100);              // 6 * 1.4 -> 8

    Combatant healthy = mkCombatant(r, "P", 100, 12, {"quick_jab"});
    healthy.health = 50;                                          // above the threshold
    healthy.mods.arm(ModEffect::LowHealthPowerPct, 30, 40);
    Combatant e2 = mkCombatant(r, "E", 100, 5, {"quick_jab"});
    Combat c2; c2.begin(healthy, e2, Combat::Stakes::Safe, 5, false, 50);
    c2.step();
    CHECK(c2.enemy().health == 100 - 6);                          // no bonus
}

// Zero-Day Exploit: a one-time gamble rolled in Combat::begin(). Tested at its two
// deterministic boundaries (0% and 100% chance) rather than reverse-engineering a seed.
static void test_mod_zero_day_exploit() {
    ContentRegistry r = ContentRegistry::embedded();
    Combatant always = mkCombatant(r, "P", 100, 12, {"quick_jab"});
    always.mods.arm(ModEffect::GambleBattlePowerPct, 100, 60);
    Combatant e1 = mkCombatant(r, "E", 100, 5, {"quick_jab"});
    Combat c1; c1.begin(always, e1, Combat::Stakes::Safe, 5);
    CHECK(c1.player().powerMultPct == 160);

    Combatant never = mkCombatant(r, "P", 100, 12, {"quick_jab"});
    never.mods.arm(ModEffect::GambleBattlePowerPct, 0, 60);
    Combatant e2 = mkCombatant(r, "E", 100, 5, {"quick_jab"});
    Combat c2; c2.begin(never, e2, Combat::Stakes::Safe, 5);
    CHECK(c2.player().powerMultPct == 100);
}

// Tripwire: reflects only while own Health is at/under the threshold, and stays a
// SEPARATE accumulation from an always-on Thorns mod (Honeytoken) equipped alongside it.
static void test_mod_tripwire() {
    ContentRegistry r = ContentRegistry::embedded();
    Combatant low = mkCombatant(r, "P", 100, 5, {"quick_jab"});
    low.health = 30;                                              // 30% <= the 40% threshold
    low.mods.arm(ModEffect::ConditionalThorns, 10, 40);
    Combatant e1 = mkCombatant(r, "E", 100, 12, {"quick_jab"});    // fast, hits first
    // carryPlayerHealth seats the pre-set low.health (30) — begin() otherwise resets
    // the player to full.
    Combat c1; c1.begin(low, e1, Combat::Stakes::Safe, 5, false, 30);
    c1.step();
    CHECK(c1.enemy().health == 100 - 10);                         // reflected

    Combatant healthy = mkCombatant(r, "P", 100, 5, {"quick_jab"});
    healthy.health = 100;                                         // above the threshold
    healthy.mods.arm(ModEffect::ConditionalThorns, 10, 40);
    Combatant e2 = mkCombatant(r, "E", 100, 12, {"quick_jab"});
    Combat c2; c2.begin(healthy, e2, Combat::Stakes::Safe, 5, false, 100);
    c2.step();
    CHECK(c2.enemy().health == 100);                              // dormant

    Combatant both = mkCombatant(r, "P", 100, 5, {"quick_jab"});
    both.health = 100;                                            // above Tripwire's threshold
    both.mods.arm(ModEffect::Thorns, 4);                           // Honeytoken: always-on
    both.mods.arm(ModEffect::ConditionalThorns, 10, 40);           // Tripwire: dormant here
    Combatant e3 = mkCombatant(r, "E", 100, 12, {"quick_jab"});
    Combat c3; c3.begin(both, e3, Combat::Stakes::Safe, 5, false, 100);
    c3.step();
    CHECK(c3.enemy().health == 100 - 4);                          // only Honeytoken fires
}

// Bubble-gated half of the steal track: stealSpeedPct/stealCurrentHpPct only fire while
// the caster's Obfuscation shield is up (shieldHp > 0) — the interplay the line is built
// around (the volatile bonus costs the defensive pool's uptime). Both combatants sit at
// speed 50 (not the usual 10) so 6% of it — smish_hook's stealSpeedPct — lands on an
// exact float (3.0) instead of a repeating binary fraction. Stage stays at its BootSector
// default so the Perfect-Bite chance is a guaranteed 0% (kPhishingBiteChancePctByStage[0]),
// isolating the BASE bubble-gated amounts from the chance-based bonus (see
// test_phishing_perfect_bite). Feed-frenzy also lives here, since a landed power siphon
// while the bubble's up triggers it: the enemy never attacks, so Health moves via the
// lifesteal + frenzy heal alone. Order is P(wind-up), E(guard), P(detonate).
static void test_phishing_bubble_steal() {
    ContentRegistry r = ContentRegistry::embedded();
    auto run = [&](int shield, Combat& out) {
        Combatant pc = mkCombatant(r, "P", 100, 50, {"smish_hook"});
        pc.shieldHp = shield;
        Combatant e = mkCombatant(r, "E", 100, 50, {"quick_jab"});
        out.begin(pc, e, Combat::Stakes::Safe, 1);
        out.step(); out.step(); out.step();
    };
    // No bubble: only the unconditional power siphon fires. quick_jab (6 dmg) is the
    // only thing touching player Health, and enemy Health only takes the attack's own
    // damage — no lifesteal, no frenzy.
    Combat c0; run(0, c0);
    CHECK(c0.player().speed == 50 && c0.enemy().speed == 50);
    CHECK(c0.player().health == 94);              // 100 - 6 (quick_jab), no heals
    CHECK(c0.enemy().health == 92);                // 100 - 8 (smish_hook), no lifesteal
    CHECK(c0.player().powerMultPct == 108 && c0.enemy().powerMultPct == 92);

    // Bubble up: stealSpeedPct/stealCurrentHpPct now fire at their base (6%) amounts on
    // top of the power siphon, and the landed power siphon feeds Feed-Frenzy. The
    // player's OWN shieldHp(50) also absorbs quick_jab's 6 damage outright (the
    // pre-existing Obfuscation absorb, ahead of any steal effect) instead of it
    // touching Health, so Health only ever moves via this hit's own heals.
    Combat c1; run(50, c1);
    CHECK(c1.player().speed == 53.0f && c1.enemy().speed == 47.0f);   // 6% of 50 = 3
    // enemy.health: 100 - 8 (attack) - 5 (6% of 92, lifesteal) = 87.
    CHECK(c1.enemy().health == 87);
    // player.health: 100 (quick_jab absorbed by shieldHp, not Health) + 5 (lifesteal)
    // + 1 (frenzy: floor(44*7.5/1000) floored to a minimum of 1) = 100, capped.
    CHECK(c1.player().health == 100);
}

// Perfect Bite: while the bubble's up, a stage-scaled chance (kPhishingBiteChancePctByStage)
// additionally doubles WHICHEVER of stealSpeedPct/stealCurrentHpPct lands the hit (picked at
// random when a move sets both, as every Phishing steal-attack does); the Phishing Rod's
// the Phishing Rod scales that bonus specifically, not the base amount. Daemon (45%) is the
// easiest stage to observe both a biting and a non-biting roll in a short scan. Stage is
// set directly on the Combatant purely to select the chance-table row — Combat itself never
// checks a move's minStage (that gate lives at the picker/equip layer).
static void test_phishing_perfect_bite() {
    ContentRegistry r = ContentRegistry::embedded();
    auto run = [&](uint32_t seed, int amplify, Combat& out) {
        Combatant pc = mkCombatant(r, "P", 100, 50, {"smish_hook"});
        pc.stage = Stage::Daemon;
        pc.shieldHp = 50;
        pc.mods.arm(ModEffect::StealAmplifyPct, amplify);
        Combatant e = mkCombatant(r, "E", 100, 50, {"quick_jab"});
        out.begin(pc, e, Combat::Stakes::Safe, seed, /*forceEnemyFirst=*/false,
                  /*carryPlayerHealth=*/50);
        out.step(); out.step(); out.step();
    };
    // Baselines (no bite): speed 50 -> 53/47 (6% of 50). Health: quick_jab's 6 damage
    // is absorbed by the player's OWN shieldHp(50), not Health (the pre-existing
    // Obfuscation absorb, ahead of any steal effect) — so carryPlayerHealth(50) stays
    // 50 through step 2, then +5 lifesteal (6% of 92) +1 frenzy (floored) = 56.
    constexpr float kBaseSpeed = 53.0f;
    constexpr int kBaseHealth = 56;

    uint32_t biteSpeedSeed = 0, biteHpSeed = 0, noBiteSeed = 0;
    for (uint32_t s = 1; s <= 80 && !(biteSpeedSeed && biteHpSeed && noBiteSeed); ++s) {
        Combat c; run(s, 0, c);
        const bool bitSpeed = c.player().speed > kBaseSpeed;
        const bool bitHp = c.player().health > kBaseHealth;
        CHECK(!(bitSpeed && bitHp));                 // exactly one stat bites per hit
        if (bitSpeed && !biteSpeedSeed) biteSpeedSeed = s;
        else if (bitHp && !biteHpSeed) biteHpSeed = s;
        else if (!bitSpeed && !bitHp && !noBiteSeed) noBiteSeed = s;
    }
    CHECK(biteSpeedSeed && biteHpSeed && noBiteSeed);   // all three outcomes reachable

    // Rod: re-running a biting seed with the Rod equipped scales that hit's bonus up
    // further (still only the bitten stat — the other stays at its base value).
    Combat plainSpeed; run(biteSpeedSeed, 0, plainSpeed);
    Combat rodSpeed; run(biteSpeedSeed, 75, rodSpeed);
    CHECK(rodSpeed.player().speed > plainSpeed.player().speed);
    CHECK(rodSpeed.player().health == plainSpeed.player().health);   // hp untouched

    Combat plainHp; run(biteHpSeed, 0, plainHp);
    Combat rodHp; run(biteHpSeed, 75, rodHp);
    CHECK(rodHp.player().health > plainHp.player().health);
    CHECK(rodHp.player().speed == plainHp.player().speed);   // speed untouched
}

// Obfuscation shield pool (Phishing defensive track): a shieldPool Defend move POOLS
// into shieldHp (a second health bar) that absorbs before real Health, overflows the
// remainder to Health when it pops, and STACKS additively on recast — distinct from the
// one-shot `guard` brace every other Defend move sets.
static void test_phishing_shield_pool() {
    ContentRegistry r = ContentRegistry::embedded();
    const MoveDef* sb = r.move("spoof_bubble");
    CHECK(sb && sb->kind == MoveDef::Kind::Defend && sb->shieldPool > 0 && sb->power == 8);

    // Absorb + overflow: cast the bubble (shieldHp 8), then eat a 20-power hit — the
    // shield soaks 8 and pops, the remaining 12 carries through to Health.
    Combatant p = mkCombatant(r, "P", 100, 10, {"spoof_bubble"});
    Combatant e = mkCombatant(r, "E", 100, 10, {"buffer_overflow"});  // 20 power, no rider
    Combat c; c.begin(p, e, Combat::Stakes::Safe, 1);
    c.step();                                            // P casts Spoof-Bubble
    CHECK(c.player().shieldHp == 8);
    CHECK(c.player().health == 100);                     // nothing spent yet
    c.step();                                            // E hits for 20
    CHECK(c.player().shieldHp == 0);                     // popped
    CHECK(c.player().health == 88);                      // overflow 12 through to Health

    // A non-shield Defend move still sets the one-shot guard, NOT the pool.
    Combatant p2 = mkCombatant(r, "P", 100, 10, {"checksum_guard"});
    Combatant e2 = mkCombatant(r, "E", 100, 10, {"checksum_guard"});
    Combat c2; c2.begin(p2, e2, Combat::Stakes::Safe, 1);
    c2.step();
    CHECK(c2.player().shieldHp == 0);
    CHECK(c2.player().guard == 14);                      // ordinary brace, not pooled

    // Additive stacking: with an enemy that only defends (no damage), two player casts
    // pool to 16 rather than refreshing to 8.
    Combatant p3 = mkCombatant(r, "P", 100, 10, {"spoof_bubble"});
    Combatant e3 = mkCombatant(r, "E", 100, 10, {"checksum_guard"});
    Combat c3; c3.begin(p3, e3, Combat::Stakes::Safe, 1);
    c3.step();                                           // P bubble -> 8
    c3.step();                                           // E defends (no damage to P)
    c3.step();                                           // P bubble again -> 16 (stacks)
    CHECK(c3.player().shieldHp == 16);
}

// Speed action economy: relative speed drives how many actions each pet gets. Equal
// speed alternates strictly (matches the pre-scheduler engine); a faster pet acts more
// often, in proportion to the speed ratio.
static void test_speed_action_economy() {
    ContentRegistry r = ContentRegistry::embedded();
    // Equal speed -> strict alternation. Both pets only defend, so nobody dies and the
    // scheduler can be observed over many turns.
    {
        Combatant p = mkCombatant(r, "P", 100, 10, {"checksum_guard"});
        Combatant e = mkCombatant(r, "E", 100, 10, {"checksum_guard"});
        Combat c; c.begin(p, e, Combat::Stakes::Safe, 1);
        bool want = c.playerActsFirst();
        for (int i = 0; i < 8; ++i) {
            CHECK(c.playerTurnNext() == want);
            c.step();
            want = !want;
        }
    }
    // A 3x-faster pet takes comfortably more than twice as many actions over a long run.
    {
        Combatant p = mkCombatant(r, "P", 100, 30, {"checksum_guard"});
        Combatant e = mkCombatant(r, "E", 100, 10, {"checksum_guard"});
        Combat c; c.begin(p, e, Combat::Stakes::Safe, 1);
        int pl = 0, en = 0;
        for (int i = 0; i < 40; ++i) {
            if (c.playerTurnNext()) ++pl; else ++en;
            c.step();
        }
        CHECK(pl > en * 2);
    }
}

// Minimum penetration: a real attack always lands >=1 through pure defensive mitigation
// (% cut + guard), so no pet is an invincible wall — but RAID Mirror's deliberate full
// negation is still exempt.
static void test_min_damage_penetration() {
    ContentRegistry r = ContentRegistry::embedded();
    // Quick Jab (6) into a maxed damage-cut would round to 0; the floor lands 1.
    Combatant p = mkCombatant(r, "P", 100, 20, {"quick_jab"});
    Combatant e = mkCombatant(r, "E", 100, 5, {"checksum_guard"});
    e.dmgReducePct = 95;                                 // clamps high; 6 * ~15% -> 0
    Combat c; c.begin(p, e, Combat::Stakes::Safe, 1);
    c.step();                                            // P jabs the wall
    CHECK(c.enemy().health == 99);                       // 1 penetrated, not 0

    // RAID Mirror still negates the whole hit (its own branch, exempt from the floor).
    Combatant p2 = mkCombatant(r, "P", 100, 20, {"quick_jab"});
    Combatant e2 = mkCombatant(r, "E", 100, 5, {"checksum_guard"});
    e2.mods.arm(ModEffect::RaidMirror);
    Combat c2; c2.begin(p2, e2, Combat::Stakes::Safe, 1);
    c2.step();
    CHECK(c2.enemy().health == 100);                     // fully mirrored, 0 damage
}

// The HARD line gate (ModDef::requiresLine, niche-flavour pass): distinct from every
// other mod's soft `line`/`affinityBonus` bonus. Driven through the real MODS UI,
// mirroring test_mod_equip_level_gate's pattern — Paypup's starting owned spares are
// exactly {packet_sniffer, raid_mirror} (Loadout::starting()), so granting ONE more
// mod always lands it at picker row 2 (registry order, filtered to owned).
// A mod's equip gate — level a test pet to this and that mod is equippable. Derived from
// the mod's own rank rather than written as a literal, so shifting the ladder moves these
// tests' targets with it instead of stranding them under a level that used to be enough.
static int modGateCeiling(const char* id) {
    ContentRegistry r = ContentRegistry::embedded();
    return modEquipLevel(*r.mod(id));
}

static void test_mod_hard_line_gate() {
    Game g{StartMode::Hatched};                             // Paypup, ransomware line
    const int deepGate = modGateCeiling("extortion_ledger");  // both spares share a rank
    while (g.combatLevel() < deepGate) g.debugAddCombatXp(g.xpToNextLevel());
    g.debugGrantMod("phishing_rod");                  // requiresLine = phishing (mismatch)
    enterSubmenuId(g, SubmenuId::Mods);
    g.onButton(press(Button::A)); g.onButton(press(Button::A));  // listRow 0->2 (empty slot)
    g.onButton(press(Button::B));                           // open slot-2 picker
    g.onButton(press(Button::A)); g.onButton(press(Button::A));  // pick row 2 = phishing_rod
    g.onButton(press(Button::B));                           // open its detail
    g.onButton(press(Button::B));                           // EQUIP -> blocked (wrong line)
    CHECK(g.loadout().equipped(2) == nullptr);
    CHECK(g.loadout().owns("phishing_rod"));                // spare not consumed

    Game g2{StartMode::Hatched};
    while (g2.combatLevel() < deepGate) g2.debugAddCombatXp(g2.xpToNextLevel());
    g2.debugGrantMod("extortion_ledger");             // requiresLine = ransomware (match)
    enterSubmenuId(g2, SubmenuId::Mods);
    g2.onButton(press(Button::A)); g2.onButton(press(Button::A));
    g2.onButton(press(Button::B));
    g2.onButton(press(Button::A)); g2.onButton(press(Button::A));
    g2.onButton(press(Button::B));
    g2.onButton(press(Button::B));                          // EQUIP -> allowed (matching line)
    CHECK(g2.loadout().slotOf("extortion_ledger") == 2);
    CHECK(!g2.loadout().owns("extortion_ledger"));
}

// the rolled equip-LEVEL gate — a spare above the pet's level can't be
// equipped; leveling past it unlocks the equip. Driven through the real MODS UI.
static void test_mod_equip_level_gate() {
    Game g{StartMode::Hatched};                            // pet at level 0
    g.debugGrantMod("overclock_chip");               // a mid-ladder rank: req > 0
    const int ocGate = modGateCeiling("overclock_chip");
    CHECK(ocGate > 0);                                     // a mid-ladder rank does gate
    // Navigate: MODS → slot 3 (empty) picker → select the overclock spare → EQUIP.
    enterSubmenuId(g, SubmenuId::Mods);
    g.onButton(press(Button::A)); g.onButton(press(Button::A));  // listRow 0→2 (empty slot)
    g.onButton(press(Button::B));                          // open slot-2 picker
    // Owned spares in registry order: packet_sniffer, overclock_chip, raid_mirror.
    g.onButton(press(Button::A));                          // pick row 1 = overclock
    g.onButton(press(Button::B));                          // open its detail
    g.onButton(press(Button::B));                          // EQUIP → blocked (under-level)
    CHECK(g.loadout().equipped(2) == nullptr);             // not installed
    CHECK(g.loadout().owns("overclock_chip"));             // spare not consumed
    // Level the pet above the gate, then the same equip succeeds.
    while (g.combatLevel() < ocGate) g.debugAddCombatXp(g.xpToNextLevel());
    enterSubmenuId(g, SubmenuId::Mods);
    g.onButton(press(Button::A)); g.onButton(press(Button::A));
    g.onButton(press(Button::B));
    g.onButton(press(Button::A));
    g.onButton(press(Button::B));
    g.onButton(press(Button::B));                          // EQUIP → now allowed
    CHECK(g.loadout().slotOf("overclock_chip") == 2);
    CHECK(!g.loadout().owns("overclock_chip"));            // consumed on equip
}

// drawModPicker windowing (twin of train_screen's drawMovePicker fix): with more
// than kModPickerVisibleRows owned mods, the row list must CAP at the window and
// SCROLL to follow the cursor, never overrun into the description zone at y=150.
// Grants every embedded mod (16, well past the 6-row window) as an owned spare.
static void test_mod_picker_windows_large_pool() {
    ContentRegistry r = ContentRegistry::embedded();
    Loadout load;
    const auto allMods = r.allMods();
    CHECK(allMods.size() > 6);                  // the pool this test needs to overflow
    for (const ModDef* m : allMods) load.grant(m->id, kModCopyCapBase);  // one spare each

    const auto owned = ownedModList(r, load);
    const int lastPick = static_cast<int>(owned.size()) - 1;
    CHECK(lastPick >= 6);

    // (a) The focused row-cursor triangle for the LAST mod must still land inside
    // the windowed list band (y in [22,150)) — the pre-fix math (`kRowTop +
    // pick*rowH`) would place it at 26 + 15*18 = 296, off the 224-tall panel and
    // nowhere near the visible list.
    Framebuffer last(kActiveW, kActiveH);
    drawModPicker(last, r, load, /*slot=*/0, lastPick, /*confirmActive=*/false,
                  0, nullptr, /*petLevel=*/99, /*petLine=*/nullptr);
    bool cursorInWindow = false;
    for (int y = 22; y < 150 && !cursorInWindow; ++y)
        for (int x = 6; x < 20; ++x)
            if (last.get(x, y) == palColor(Pal::ACCENT)) { cursorInWindow = true; break; }
    CHECK(cursorInWindow);

    // (b) The window actually FOLLOWS the cursor rather than just clipping: the
    // picker focused on the first mod renders differently from the one focused
    // on the last mod (different rows are on screen).
    Framebuffer first(kActiveW, kActiveH);
    drawModPicker(first, r, load, 0, 0, false, 0, nullptr, 99, nullptr);
    CHECK(!fbEqual(first, last));

    // (c) The invariant holds across the whole pool, not just at the last index:
    // for every pick value the row-cursor triangle stays inside the window.
    for (int pick = 0; pick <= lastPick; pick += 3) {
        Framebuffer fb(kActiveW, kActiveH);
        drawModPicker(fb, r, load, 0, pick, false, 0, nullptr, 99, nullptr);
        bool inWindow = false;
        for (int y = 22; y < 150 && !inWindow; ++y)
            for (int x = 6; x < 20; ++x)
                if (fb.get(x, y) == palColor(Pal::ACCENT)) { inWindow = true; break; }
        CHECK(inWindow);
    }
}

// Forges a blob byte-shaped like a pre-v33 writer would have produced for a
// SaveData otherwise identical to `proto`. v33 REMOVES three wire fields
// (the v4 dedup-mask byte, the v7 seenBssids vector, the v21 full-mask u64)
// instead of only appending — the first removal in this format's history — so
// the usual "serialize CURRENT, drop trailing bytes, restamp the version word"
// trick every migration test below relies on is invalid for a restamped version
// in [4,32]: those bytes sit mid-stream, not at the tail, and a v33+ writer no
// longer emits them at all, so restamping alone would silently misalign every
// field after the removal point. This reconstructs them: builds a byte-
// identical "shape twin" of `proto` with distinctive sentinel values stamped
// onto the handful of FIXED-WIDTH scalar fields immediately preceding each
// splice point (overwriting a fixed-width scalar can't shift any other field's
// position, so the twin's byte offsets exactly match the real blob's), locates
// those sentinels, and inserts the removed bytes at the same offsets into the
// real, caller-supplied blob — independent of whatever actual values `proto`
// holds (so a test's own meaningful field values never collide with the search).
static std::vector<uint8_t> forgeLegacyNetworkBytes(const SaveData& proto, int targetVersion) {
    SaveData shape = proto;
    shape.networksSeen = 0x1A2B3C4D;
    shape.handshakesSeen = 0x5E6F7081;
    shape.mistakeShieldActive = 0xAA;
    shape.shieldItemConsumed = 0xBB;
    shape.yubiConsumed = 0xCC;
    const auto shapeBlob = serializeSave(shape);
    auto blob = serializeSave(proto);

    auto le32 = [](int32_t v) {
        std::vector<uint8_t> b(4);
        for (int i = 0; i < 4; ++i) b[i] = static_cast<uint8_t>(static_cast<uint32_t>(v) >> (8 * i));
        return b;
    };
    auto findOnce = [](const std::vector<uint8_t>& hay, const std::vector<uint8_t>& pat) -> long {
        long found = -1;
        for (size_t i = 0; i + pat.size() <= hay.size(); ++i) {
            if (std::equal(pat.begin(), pat.end(), hay.begin() + i)) {
                CHECK(found < 0);   // must be a unique match, else the offset is ambiguous
                found = static_cast<long>(i);
            }
        }
        CHECK(found >= 0);
        return found;
    };

    // v4: 1 zero byte right after networksSeen(4B)+hackerRank(4B). No prior
    // insertion yet, so shapeBlob's offset applies to `blob` unshifted.
    long at = findOnce(shapeBlob, le32(0x1A2B3C4D)) + 8;
    blob.insert(blob.begin() + at, uint8_t{0});

    if (targetVersion >= 7) {
        // v7: an empty seenBssids vector (u16 count = 0) right after
        // handshakesSeen(4B). +1 corrects for the v4 byte already inserted above
        // (shapeBlob's own measurement is unaffected — it's never mutated).
        long hsAt = findOnce(shapeBlob, le32(0x5E6F7081)) + 4 + 1;
        blob.insert(blob.begin() + hsAt, {0, 0});
    }
    if (targetVersion >= 21) {
        // v21: the full 64-bit mask (8 bytes) right after yubiConsumed, before
        // fragAmountTier. +1 (v4) +2 (v7, always already inserted — v21 implies
        // v7) corrects for both prior insertions.
        const std::vector<uint8_t> shieldPat = {0xAA, 0xBB, 0xCC};
        long shAt = findOnce(shapeBlob, shieldPat) + 3 + 1 + 2;
        const std::vector<uint8_t> zeros8(8, 0);
        blob.insert(blob.begin() + shAt, zeros8.begin(), zeros8.end());
    }
    return blob;
}

// save v18: the per-spare equip gate round-trips; a pre-v18 blob (version
// stripped to 17) has no tail → ownedModReqLevels empty + hasModEquipLevelData false
// (the game_persist apply path then grants every spare at req 0 — no retroactive gate).
static void test_save_v18_mod_reqlevel() {
    SaveData a;
    std::strcpy(a.activeId, "paypup");
    a.ownedMods.resize(2);
    std::strcpy(a.ownedMods[0].id, "crypto_coprocessor");
    std::strcpy(a.ownedMods[1].id, "overclock_chip");
    a.ownedModReqLevels = {3, 22};
    auto full = serializeSave(a);

    SaveData out;
    CHECK(deserializeSave(full, out));
    CHECK(out.hasModEquipLevelData);
    CHECK(out.ownedModReqLevels == a.ownedModReqLevels);

    // Force the version word down to 17 → deserialize stops before the v18 tail.
    auto blob = forgeLegacyNetworkBytes(a, 17);
    blob[4] = 17; blob[5] = 0;
    SaveData mig;
    CHECK(deserializeSave(blob, mig));
    CHECK(!mig.hasModEquipLevelData);
    CHECK(mig.ownedModReqLevels.empty());        // no tail → apply defaults every req to 0
    CHECK(mig.ownedMods.size() == 2);            // the spares themselves still load
}

// The pool has a ceiling now, and the Rig Shop row is what moves it. Both halves matter:
// a cap with no way to raise it is a nerf, and a shop row that doesn't actually change
// what a grant does is a Bits sink that sells nothing.
static void test_mod_storage_cap_bounds_the_pool_and_the_shop_raises_it() {
    // The tier table has to stay inside what a nibble can hold, or a bought tier would
    // sell room the save cannot describe.
    CHECK(kModCopyCapBase >= 1 && kModCopyCapBase <= kModCopyCapMax);
    for (int t = 0; t <= kModStorageMaxTier; ++t) {
        CHECK(modCopyCap(t) >= kModCopyCapBase);
        CHECK(modCopyCap(t) <= kModCopyCapMax);
    }
    CHECK(modCopyCap(0) == kModCopyCapBase);
    CHECK(modCopyCap(kModStorageMaxTier) > kModCopyCapBase);       // the ladder climbs
    CHECK(modCopyCap(kModStorageMaxTier + 9) == modCopyCap(kModStorageMaxTier));  // clamps

    Loadout l;
    CHECK(l.grant("heat_sink", kModCopyCapBase));
    CHECK(l.grant("heat_sink", kModCopyCapBase));
    CHECK(!l.grant("heat_sink", kModCopyCapBase));   // full: reported, not silently dropped
    CHECK(l.countOf("heat_sink") == kModCopyCapBase);
    // Equipping frees a copy, so the cap bounds what is HELD, not what may ever be earned.
    l.equip(0, "heat_sink");
    CHECK(l.countOf("heat_sink") == kModCopyCapBase - 1);
    CHECK(l.grant("heat_sink", kModCopyCapBase));

    // A drop into a full pool leaves the pool alone, and the count doesn't creep.
    Game g{StartMode::Hatched};
    const int cap = g.modStorageCap();
    CHECK(cap == kModCopyCapBase);                  // nothing bought yet
    for (int i = 0; i < cap + 5; ++i) g.debugGrantMod("heat_sink");
    CHECK(g.loadout().countOf("heat_sink") == cap);

    // Buying MOD STORAGE raises what the same call will accept.
    g.debugSetBits(kModStorageStart);
    g.debugBuyModStorage();                         // through the real till
    CHECK(g.modStorageCap() == modCopyCap(1));
    CHECK(g.modStorageCap() > cap);
    g.debugGrantMod("heat_sink");
    CHECK(g.loadout().countOf("heat_sink") == cap + 1);
}

// The v45 pool is a nibble of COUNT per mod wire, two mods to a byte. The packing is
// where this could go wrong invisibly — a count bleeding into the neighbouring nibble
// hands the player copies of a mod they never earned — so the odd/even split, the 0..15
// range and the clamp are asserted directly rather than through a save.
static void test_save_mod_count_nibbles_pack_two_mods_per_byte() {
    std::vector<uint8_t> packed;
    CHECK(saveModCount(packed, 0) == 0);            // nothing written reads as none held
    CHECK(saveModCount(packed, 999) == 0);          // ...and so does out of range
    CHECK(saveModCount(packed, -1) == 0);

    saveSetModCount(packed, 4, 3);                  // even wire -> low nibble
    saveSetModCount(packed, 5, 12);                 // odd wire  -> high nibble, same byte
    CHECK(packed.size() == 3);                      // wires 0..5 -> 3 bytes
    CHECK(saveModCount(packed, 4) == 3);
    CHECK(saveModCount(packed, 5) == 12);
    CHECK(saveModCount(packed, 3) == 0);            // neighbours untouched
    CHECK(saveModCount(packed, 6) == 0);

    // Rewriting one nibble must leave its partner alone — the bug that would silently
    // move copies between two unrelated mods.
    saveSetModCount(packed, 4, 1);
    CHECK(saveModCount(packed, 4) == 1 && saveModCount(packed, 5) == 12);

    // 15 is what 4 bits can say, and kModCopyCapMax is set to exactly that. A count past
    // it clamps rather than wrapping into the next mod.
    CHECK(kModCopyCapMax == 15);
    saveSetModCount(packed, 5, 99);
    CHECK(saveModCount(packed, 5) == kModCopyCapMax);
    CHECK(saveModCount(packed, 4) == 1);
}

// A v44 blob's flat per-copy pool migrates to counts, clamped to the base cap. This is
// the one that ran against a real device save: 424 copies of 24 mods, which is what the
// whole change is for.
static void test_save_v44_to_v45_mod_pool_migration() {
    SaveData a; std::strcpy(a.activeId, "paypup"); a.generation = 1;
    // Nine copies of one mod and one of another, with rolled levels that no longer mean
    // anything — the shape the measured save was made of.
    for (int i = 0; i < 9; ++i) {
        a.ownedMods.push_back(SaveId{"packet_sniffer"});
        a.ownedModReqLevels.push_back(i);
    }
    a.ownedMods.push_back(SaveId{"raid_mirror"});
    a.ownedModReqLevels.push_back(4);
    // A plain serialize + version stamp, NOT forgeLegacyNetworkBytes: that helper
    // re-inserts the pre-v21 network-field bytes, which a v44 blob doesn't carry. v45
    // only APPENDS to v44, so trimming its 2-byte tail leaves a byte-exact v44 stream —
    // which is the property that made the old flat list survivable to migrate at all.
    auto blob = serializeSave(a);
    blob.resize(blob.size() - 2);                 // drop the v45 tail's length word
    blob[4] = 44; blob[5] = 0;

    SaveData out;
    CHECK(deserializeSave(blob, out));
    CHECK(!out.hasModCountData);                 // no v45 tail -> the migration path
    CHECK(out.ownedMods.size() == 10);           // the old flat list still loads

    MemSaveStore store; store.save(blob);
    Game g(StartMode::Hatched, "paypup", &store);
    // Nine copies come back as the cap, not as nine. The surplus is dropped, which no
    // screen has ever been able to show.
    CHECK(g.loadout().countOf("packet_sniffer") == kModCopyCapBase);
    CHECK(g.loadout().countOf("raid_mirror") == 1);

    // And the save it writes back is the counted one, which is the whole point: the same
    // pool costs a nibble a mod instead of 28 bytes a copy.
    CHECK(g.saveNow());
    SaveData back;
    CHECK(deserializeSave(store.bytes(), back));
    CHECK(back.hasModCountData);
    CHECK(back.ownedMods.empty());               // the flat list is written empty from v45
    const ContentRegistry reg = ContentRegistry::embedded();
    CHECK(saveModCount(back.ownedModCounts, reg.mod("packet_sniffer")->wire)
          == kModCopyCapBase);
    CHECK(saveModCount(back.ownedModCounts, reg.mod("raid_mirror")->wire) == 1);
    // And the encoding is the point. The pool the v44 blob spent 10 id cells on now costs
    // a nibble per MOD held — measured against the list it replaced, not against a whole
    // save, since the two blobs describe different games.
    const int wasBytes = static_cast<int>(out.ownedMods.size()) * kSaveIdCap;
    CHECK(static_cast<int>(back.ownedModCounts.size()) < wasBytes);
    CHECK(back.ownedModCounts.size() * 2 <= static_cast<size_t>(kModWireCap));
}

// A count stored against a wire the running content no longer defines is dropped rather
// than resurrected under some other mod's id — the failure a positional array would have
// and a wire-keyed one must not.
static void test_save_mod_count_for_a_retired_wire_is_dropped() {
    MemSaveStore store;
    {
        Game g(StartMode::Hatched, "paypup", &store);
        CHECK(g.saveNow());
    }
    SaveData d;
    CHECK(deserializeSave(store.bytes(), d));
    // kModWireCap - 1 is addressable by the format and belongs to no shipped row.
    CHECK(ContentRegistry::embedded().modByWire(kModWireCap - 1) == nullptr);
    saveSetModCount(d.ownedModCounts, kModWireCap - 1, 5);
    store.save(serializeSave(d));

    Game g2(StartMode::Hatched, "paypup", &store);
    int copies = 0;
    for (const OwnedMod& m : g2.loadout().owned()) copies += m.count;
    CHECK(copies == 2);                          // just the two the seed grants
}

// a pre-v19 blob (no Hacker SHOP tail) still loads, defaulting bwUpgradeCount
// to 0 (a migrated save has bought no upgrades). Forge a v18 blob by dropping the v19
// i32 tail and stamping the version word down to 18.
static void test_save_v18_to_v19_bwupgrade_default() {
    SaveData a; std::strcpy(a.activeId, "paypup"); a.generation = 1;
    a.bwUpgradeCount = 4;                       // would round-trip IF the tail were read
    auto blob = forgeLegacyNetworkBytes(a, 18);
    blob.resize(blob.size() - 4);               // drop the v19 bwUpgradeCount i32
    blob[4] = 18; blob[5] = 0;                  // stamp the version word down to 18
    SaveData out;
    CHECK(deserializeSave(blob, out));          // a v18 blob still deserializes
    CHECK(out.bwUpgradeCount == 0);             // no tail → migrated to "bought none"
}

// v19 → v20: a pre-AP blob has no apEnabled byte; it must migrate to OFF (never
// silently stand up an AP on a save that predates the feature).
static void test_save_v19_to_v20_ap_default() {
    SaveData a; std::strcpy(a.activeId, "paypup"); a.generation = 1;
    a.apEnabled = 1;                            // would round-trip IF the tail were read
    auto blob = forgeLegacyNetworkBytes(a, 19);
    blob.resize(blob.size() - 1);               // drop the v20 apEnabled u8
    blob[4] = 19; blob[5] = 0;                  // stamp the version word down to 19
    SaveData out;
    CHECK(deserializeSave(blob, out));          // a v19 blob still deserializes
    CHECK(out.apEnabled == 0);                  // no tail → migrated to OFF
}

// v37 round-trip: the pet-to-pet LINK opt-in. Only the consent bit is in the blob —
// the met operators themselves ride the SD-backed PeerLedger.
static void test_save_v37_link_roundtrip() {
    SaveData a; std::strcpy(a.activeId, "paypup"); a.generation = 1;
    a.linkEnabled = true;
    auto blob = serializeSave(a);
    SaveData out;
    CHECK(deserializeSave(blob, out));
    CHECK(out.linkEnabled);
}

// v36 → v37: a pre-LINK blob has no linkEnabled byte and must migrate to OFF. This
// is the one that actually matters — a device upgrading its firmware must never
// start broadcasting its operator's identity as a side effect of the upgrade.
static void test_save_v36_to_v37_link_default() {
    SaveData a; std::strcpy(a.activeId, "paypup"); a.generation = 1;
    a.linkEnabled = true;                       // would round-trip IF the tail were read
    auto blob = serializeSave(a);
    blob.resize(blob.size() - 1);               // drop the v37 linkEnabled u8
    blob[4] = 36; blob[5] = 0;                  // stamp the version word down to 36
    SaveData out;
    CHECK(deserializeSave(blob, out));          // a v36 blob still deserializes
    CHECK(!out.linkEnabled);                    // no tail → migrated to OFF
}

// The v38 byte is RESERVED: it held a standing internet opt-in and no longer means
// anything, so a blob written with it set must come back with this device off the
// 'net. The byte itself has to stay — v39..v42 sit behind it and the tail is read
// positionally — which is what the second half checks.
static void test_save_v38_net_slot_is_reserved() {
    SaveData a; std::strcpy(a.activeId, "paypup"); a.generation = 1;
    a.netReserved = true;                       // whatever a caller puts here...
    a.linkEnabled = true;
    auto blob = serializeSave(a);
    SaveData out;
    CHECK(deserializeSave(blob, out));
    CHECK(!out.netReserved);                    // ...never survives the round trip
    CHECK(out.linkEnabled);                     // the v37 field ahead of it still does

    // The slot still occupies its byte, so everything appended behind it lands where
    // the reader expects. A v39 field is the nearest one to prove that with.
    SaveId sp; std::strcpy(sp.id, "malbear");
    a.raisedCreatures.push_back(sp);
    blob = serializeSave(a);
    SaveData out2;
    CHECK(deserializeSave(blob, out2));
    CHECK(out2.raisedCreatures.size() == 1);
    CHECK(std::strcmp(out2.raisedCreatures[0].id, "malbear") == 0);
}

// v37 → v38: a pre-INTERNET blob has no byte in that slot at all, and must still
// parse. The consequential half is the same either way — no firmware update, and no
// save predating the field, may leave this device able to reach the network.
static void test_save_v37_to_v38_net_default() {
    SaveData a; std::strcpy(a.activeId, "paypup"); a.generation = 1;
    a.linkEnabled = true;
    auto blob = serializeSave(a);
    blob.resize(blob.size() - 1);               // drop the reserved v38 u8
    blob[4] = 37; blob[5] = 0;                  // stamp the version word down to 37
    SaveData out;
    CHECK(deserializeSave(blob, out));          // a v37 blob still deserializes
    CHECK(!out.netReserved);
    CHECK(out.linkEnabled == a.linkEnabled);    // the v37 tail still reads correctly
}

// A save is the engine's biggest allocation and it fires off a timer, so it can land
// while something else holds the heap. On the device that is not a failed save — the
// Arduino build has exceptions off, so a bad_alloc resets the board and loses the very
// state being written. The guard has to refuse the attempt, and it has to still say
// yes where nothing reports heap at all, or the host build would never save.
static void test_save_defers_when_the_heap_is_too_low() {
    Game g{StartMode::Hatched};
    CHECK(g.heapHeadroom() == 0);          // no probe wired
    CHECK(g.saveHasHeadroom());            // ...so nothing is ever held back

    // A Game that has never written still owes the blob buffer its one allocation, so
    // until that is paid every save is measured against the higher of the two bars.
    CHECK(g.saveHeapFloor() == kSaveGrowHeapFloorBytes);
    g.setHeapProbe([]() -> uint32_t { return kSaveGrowHeapFloorBytes - 1; });
    CHECK(!g.saveHasHeadroom());           // one byte under is under

    g.setHeapProbe([]() -> uint32_t { return kSaveGrowHeapFloorBytes; });
    CHECK(g.saveHasHeadroom());            // the floor itself is enough

    // That bar has to clear the reservation with room to spare, because building the
    // SaveData allocates BEFORE the buffer is sized — a floor that only covered the
    // reservation leaves nothing for the SaveData standing next to it.
    CHECK(kSaveGrowHeapFloorBytes > kSaveReserveBytes);
    // The everyday bar sits below it, which is the whole point of there being two: a
    // save writing into a buffer the engine already owns has no large allocation left
    // to clear, so it must not be turned away on a heap that could not spare a fresh one.
    CHECK(kSaveHeapFloorBytes < kSaveGrowHeapFloorBytes);

    // A refusal is COUNTED, because it is otherwise unobservable: the allocation that
    // triggered it is freed on the way out, so anything sampling the heap afterwards
    // sees a healthy number. Without the count there is no way to tell a guard that
    // worked from a device that happened to have room.
    MemSaveStore store;
    Game low(StartMode::Hatched, "paypup", &store);
    low.setHeapProbe([]() -> uint32_t { return 1024; });
    CHECK(low.savesDeferred() == 0);
    low.setApEnabled(true);                            // a change worth persisting
    for (int i = 1; i <= 40; ++i) low.tick(i * 1000);  // past debounce and autosave
    CHECK(low.savesDeferred() > 0);                    // turned away, not attempted
    CHECK(low.savesDeferredLowMark() == 1024);         // and it recorded how bad it got

    // ...and it is a DEFERRAL, not a drop: the moment there is room, the write that
    // was held back lands, carrying the change that was waiting on it.
    low.setHeapProbe([]() -> uint32_t { return kSaveGrowHeapFloorBytes; });
    low.tick(50000);
    CHECK(low.savesDeferred() == 0);
    SaveData back;
    CHECK(deserializeSave(store.bytes(), back));
    CHECK(back.apEnabled);

    // That write bought the blob buffer, so every save after it writes into memory the
    // engine already holds and is judged on the everyday floor instead. This is what
    // lets a save land while the radio owns the heap: the SAME reading that turned the
    // first one away is now enough — and the write still has to actually LAND, because
    // a guard that quietly stopped saving would pass every did-it-crash check there is.
    CHECK(low.saveHeapFloor() == kSaveHeapFloorBytes);
    low.setHeapProbe([]() -> uint32_t { return kSaveHeapFloorBytes; });
    CHECK(low.saveHasHeadroom());
    low.setLinkEnabled(true);
    for (int i = 51; i <= 90; ++i) low.tick(i * 1000);
    CHECK(low.savesDeferred() == 0);
    SaveData after;
    CHECK(deserializeSave(store.bytes(), after));
    CHECK(after.linkEnabled);
}

// A write the STORE refused is not a write. Counting it as one is what makes the
// failure silent: the flag clears, nothing retries, and a reboot later the blob on
// flash is older than the state that produced it — which is the one symptom that has no
// other explanation, a pet replaced by the one before it with nothing in between having
// looked wrong. So the refusal has to survive as an obligation.
static void test_a_refused_write_is_not_a_save() {
    RefusingSaveStore store;
    Game g(StartMode::Hatched, "paypup", &store);
    uint32_t t = 0;
    g.setApEnabled(true);
    for (int i = 1; i <= 5; ++i) g.tick(t += 1000);
    SaveData landed;
    CHECK(deserializeSave(store.bytes(), landed));       // a baseline reached "flash"
    CHECK(landed.apEnabled);
    CHECK(g.saveWritesFailed() == 0);

    // Now the medium starts refusing, and the state moves on without it.
    store.refuse = true;
    g.setLinkEnabled(true);
    CHECK(!g.saveNow());                                 // reported, not swallowed
    CHECK(g.saveWritesFailed() > 0);
    SaveData stale;
    CHECK(deserializeSave(store.bytes(), stale));
    CHECK(!stale.linkEnabled);                           // flash is behind RAM: the revert

    // It keeps trying rather than giving up — and NOT once per beat. A store that just
    // said no is not one to hammer, so the retries are paced by the same debounce every
    // other write is.
    const int before = store.attempts;
    g.tick(t += 10);
    CHECK(store.attempts == before);                     // inside the debounce window
    for (int i = 1; i <= 5; ++i) g.tick(t += kSaveDebounceMs);
    CHECK(store.attempts > before);

    // ...and the moment it accepts, the state stranded in RAM lands, which is the whole
    // point of having kept the obligation.
    store.refuse = false;
    for (int i = 1; i <= 3; ++i) g.tick(t += kSaveDebounceMs);
    CHECK(g.saveWritesFailed() == 0);
    CHECK(g.saveNow());
    SaveData caught;
    CHECK(deserializeSave(store.bytes(), caught));
    CHECK(caught.linkEnabled);
}

// The reported shape of it, end to end. A hatch persists IMMEDIATELY, and that one call
// is all that stands between a new pet and a reboot: nothing in completeHatch marks the
// save dirty on its own — installPet doesn't, and the first-hatch achievement has
// already fired by the second egg — so a write turned away there used to leave the pet
// in RAM only. It is persistSave itself that now owes the retry, which is what makes
// this true of every "persist immediately" call site rather than the ones that
// remembered.
static void test_a_hatch_the_store_refused_still_reaches_flash() {
    RefusingSaveStore store;
    Game g(StartMode::FreshHatch, "paypup", &store);
    pickFirstEggLine(g);
    uint32_t t = 0;
    g.tick(t += 1000);
    // The egg's own persist has landed by now, so "flash" holds the pet the device
    // would come back as — the previous one, in the report.
    SaveData onFlash;
    CHECK(deserializeSave(store.bytes(), onFlash));
    const std::string previous = onFlash.activeId;
    CHECK(!previous.empty());

    store.refuse = true;                                 // the medium starts saying no
    g.tick(t += kBootHatchMs / 2);
    g.onButton(press(Button::B));                        // open the decrypt
    const int presses = kHatchDurationMs / kHatchPressReductionMs;
    for (int i = 0; i < presses; ++i) g.onButton(press(Button::B));
    CHECK(g.pet() != nullptr && g.pet()->stage == Stage::Process);
    const std::string hatched = g.pet()->id;
    CHECK(hatched != previous);                          // RAM has moved on
    CHECK(g.saveWritesFailed() > 0);                     // the hatch's own write bounced
    SaveData still;
    CHECK(deserializeSave(store.bytes(), still));
    CHECK(previous == still.activeId);                   // a reboot here = the old pet

    // The pet is owed a write, so the next store that says yes gets it — no further
    // player action, and no waiting out the 30s autosave.
    store.refuse = false;
    g.tick(t += kSaveDebounceMs);
    SaveData back;
    CHECK(deserializeSave(store.bytes(), back));
    CHECK(hatched == back.activeId);
    CHECK(g.saveWritesFailed() == 0);
}

// The three radio consents are independent: none of them may imply another, so
// setting one must never move the others. Reaching the 'net is deliberately NOT
// among them — it has no standing consent to be independent of.
static void test_radio_consents_are_independent() {
    Game g{StartMode::Hatched};
    CHECK(!g.linkEnabled() && !g.apEnabled() && !g.netScanEnabled());  // default OFF

    g.setLinkEnabled(true);
    CHECK(g.linkEnabled());
    CHECK(!g.apEnabled());        // announcing never implies hosting
    CHECK(!g.netScanEnabled());   // nor listening

    g.setApEnabled(true);
    CHECK(g.linkEnabled());       // and hosting never revokes announcing
}

// Nothing persisted may leave this device able to reach the 'net. The association
// is raised by a running job and by nothing else, so it cannot outlive the job —
// and there is no saved bit left that could smuggle one across a reboot.
static void test_net_access_needs_a_live_job() {
    Game g{StartMode::Hatched};
    g.setNetProvisioned(true);            // somewhere to go...
    g.setUpdateSourceKnown(true);
    CHECK(!g.netConnectWanted());         // ...and still nothing wants the radio

    g.requestUpdateCheck();
    CHECK(g.netConnectWanted());          // only the job raises it

    UpdateStatus done; done.state = UpdateState::UpToDate;
    g.setUpdateStatus(done);
    CHECK(!g.netConnectWanted());         // and it goes back the moment that ends

    // A fresh device carrying a save written when the standing opt-in still existed
    // reads that slot as reserved, so it lands off the 'net like any other.
    SaveData d; std::strcpy(d.activeId, "paypup"); d.generation = 1;
    d.netReserved = true;
    SaveData out;
    CHECK(deserializeSave(serializeSave(d), out));
    CHECK(!out.netReserved);
}

// Walk the CFG list to the UPDATES row and open it.
static void openUpdatesScreen(Game& g) {
    enterCfgTarget(g, CfgScreen::Update);
    CHECK(g.nav() == Game::Nav::Detail && g.cfgScreen() == CfgScreen::Update);
}

// A check needs a network to reach through and somewhere to look — setup, not
// permission. Missing either, the ask is refused outright rather than starting a
// job that would sit holding the radio with nothing to fetch.
static void test_update_check_requires_network_and_source() {
    Game g{StartMode::Hatched};
    openUpdatesScreen(g);

    CHECK(!g.updateCheckReady());
    g.onButton(press(Button::B));            // CHECK NOW, with nothing set up
    CHECK(!g.updateJobLive());
    CHECK(g.updateStatus().state == UpdateState::Idle);

    g.setNetProvisioned(true);               // somewhere to go, nowhere to look
    CHECK(!g.updateCheckReady());
    g.requestUpdateCheck();
    CHECK(!g.updateJobLive());

    g.setUpdateSourceKnown(true);
    CHECK(g.updateCheckReady());
    g.requestUpdateCheck();
    CHECK(g.updateJobLive());
    CHECK(g.updateStatus().state == UpdateState::Checking);
}

// THE seam. A check is held by the JOB, not by a screen — a transfer that died the
// moment the menu collapsed could never finish — so it keeps the radio until it
// reaches a terminal outcome, and then hands it straight back.
static void test_update_job_holds_the_radio_past_the_screen() {
    Game g{StartMode::Hatched};
    g.setNetProvisioned(true);
    g.setUpdateSourceKnown(true);

    openUpdatesScreen(g);
    g.onButton(press(Button::B));            // CHECK NOW
    CHECK(g.updateJobLive());
    CHECK(g.netConnectWanted());             // the job is what raises the radio

    // Leave the screen. Nothing nav-derived is holding the radio, so if the job
    // weren't what raises it, the fetch would be cut off mid-transfer.
    g.onButton(press(Button::C));
    CHECK(g.cfgScreen() != CfgScreen::Update || g.nav() != Game::Nav::Detail);
    CHECK(g.updateJobLive());
    CHECK(g.netConnectWanted());
    CHECK(g.radioScreenOpen());              // ...so nothing may sleep the panel either

    // A terminal outcome ends the job and releases the radio — the only thing that
    // does, since no screen is watching it.
    UpdateStatus done;
    done.state = UpdateState::Available;
    done.firmwareNewer = true;
    std::strcpy(done.firmwareVersion, "0.4.2");
    g.setUpdateStatus(done);
    CHECK(!g.updateJobLive());
    CHECK(!g.netConnectWanted());

    // The verdict survives being walked away from: it is waiting on the way back.
    CHECK(g.updateStatus().state == UpdateState::Available);
    CHECK(g.updateStatus().firmwareNewer && !g.updateStatus().webNewer);

    // "Still working" is not terminal — that one keeps the radio.
    UpdateStatus busy;
    busy.state = UpdateState::Checking;
    g.requestUpdateCheck();
    g.setUpdateStatus(busy);
    CHECK(g.updateJobLive() && g.netConnectWanted());
}

// UPDATES is a screen you WAIT in front of, so the global 5s menu collapse must not
// run there. Both halves of the wait are longer than that on their own: a check is an
// association plus a fetch, and the verdict it leaves is then read and acted on. The
// job holds the radio by itself (above) — this is about the screen still being there
// when the answer arrives, and staying long enough to press INSTALL on it.
static void test_updates_screen_outlives_the_menu_idle_timer() {
    Game g{StartMode::Hatched};
    g.setNetProvisioned(true);
    g.setUpdateSourceKnown(true);
    uint32_t t = 0;

    openUpdatesScreen(g);
    g.onButton(press(Button::B));                 // CHECK NOW
    CHECK(g.updateJobLive());

    // The device tier is still associating/fetching, and nobody is touching buttons.
    for (int i = 0; i < 4; ++i) g.tick(t += kAutoDefocusMs);
    CHECK(g.updateScreenOpen());                  // the screen is still there to report to

    UpdateStatus done;
    done.state = UpdateState::Available;
    done.firmwareNewer = true;
    std::strcpy(done.firmwareVersion, "0.4.2");
    g.setUpdateStatus(done);
    CHECK(!g.updateJobLive());

    // ...and the verdict survives being read: nothing is holding the radio now, so
    // this is the window the standard budget used to close in five seconds.
    for (int i = 0; i < 4; ++i) g.tick(t += kAutoDefocusMs);
    CHECK(g.updateScreenOpen());
    CHECK(g.updateStatus().state == UpdateState::Available);

    // Not forever, though — an abandoned screen still collapses on the long budget.
    g.tick(t += kRadioScreenDefocusMs);
    CHECK(!g.updateScreenOpen());
    CHECK(g.nav() == Game::Nav::Idle);
}

// A latched job must always reach a terminal outcome, because nothing else will
// ever take the radio off it. The association never coming up is the way that most
// plausibly happens — a passphrase changed, the router out of range — so a failed
// join has to end the job and say so, not leave it claiming to check forever.
static void test_update_job_dies_when_the_join_fails() {
    Game g{StartMode::Hatched};
    g.setNetProvisioned(true);
    g.setUpdateSourceKnown(true);
    g.requestUpdateCheck();
    CHECK(g.updateJobLive());
    CHECK(g.netStatus().state == NetState::Connecting);  // joining is step one

    NetStatus failed; failed.state = NetState::Failed;
    g.setNetStatus(failed);
    CHECK(!g.updateJobLive());
    CHECK(!g.netConnectWanted());
    CHECK(g.updateStatus().state == UpdateState::Failed);
    CHECK(g.updateStatus().fail == UpdateFail::NoNetwork);  // named, not blank

    // A second ask while one is already in flight doesn't restart it or wipe the
    // Checking state out from under the device tier.
    g.requestUpdateCheck();
    UpdateStatus busy;
    busy.state = UpdateState::Checking;
    g.setUpdateStatus(busy);
    g.requestUpdateCheck();
    CHECK(g.updateJobLive());
    CHECK(g.updateStatus().state == UpdateState::Checking);
}

// Set the game up as if a check had just reported both artifacts newer.
static void seedAvailableUpdate(Game& g) {
    g.setNetProvisioned(true);
    g.setUpdateSourceKnown(true);
    UpdateStatus found;
    found.state = UpdateState::Available;
    found.firmwareNewer = true;
    std::strcpy(found.firmwareVersion, "0.4.2");
    found.webNewer = true;
    std::strcpy(found.webVersion, "0.4.0");
    g.requestUpdateCheck();
    g.setUpdateStatus(found);
}

// An install writes something, so it may only ever run against what a check
// actually confirmed. A target the last check didn't report as newer is refused
// outright — otherwise a stale screen, or a row that moved under the cursor, could
// flash an artifact the device never verified exists.
static void test_update_install_needs_a_confirmed_finding() {
    Game g{StartMode::Hatched};
    g.setNetProvisioned(true);
    g.setUpdateSourceKnown(true);

    // Nothing has been checked: every target is refused.
    g.requestUpdateInstall(UpdateTarget::Firmware);
    CHECK(!g.updateJobLive());
    g.requestUpdateInstall(UpdateTarget::Web);
    CHECK(!g.updateJobLive());
    g.requestUpdateInstall(UpdateTarget::None);   // "install the check" is not a thing
    CHECK(!g.updateJobLive());

    // A check that found only the firmware doesn't authorise the web bundle.
    UpdateStatus fwOnly;
    fwOnly.state = UpdateState::Available;
    fwOnly.firmwareNewer = true;
    std::strcpy(fwOnly.firmwareVersion, "0.4.2");
    g.requestUpdateCheck();
    g.setUpdateStatus(fwOnly);
    // CHECK NOW + the one it found + FLASH OVER USB, which is always last.
    CHECK(updateCheckRows(fwOnly) == 3);
    CHECK(updateCheckRowTarget(fwOnly, 1) == UpdateTarget::Firmware);
    CHECK(updateCheckRowTarget(fwOnly, 2) == UpdateTarget::None);
    CHECK(updateCheckRowKind(fwOnly, 2) == UpdateRowKind::FlashQr);

    g.requestUpdateInstall(UpdateTarget::Web);
    CHECK(!g.updateJobLive());

    g.requestUpdateInstall(UpdateTarget::Firmware);
    CHECK(g.updateJobLive());
    CHECK(g.updateJobTarget() == UpdateTarget::Firmware);
    CHECK(g.installStatus().state == InstallState::Downloading);

    // One job at a time: a second ask can't start a competing download over the
    // one already writing to the flash slot.
    g.requestUpdateInstall(UpdateTarget::Firmware);
    CHECK(g.updateJobTarget() == UpdateTarget::Firmware);
    g.requestUpdateCheck();
    CHECK(g.updateJobTarget() == UpdateTarget::Firmware);
}

// Walking the actual buttons: an install is always two deliberate yeses, and the
// confirm starts on NO. Pressing B twice — the reflex — must install nothing.
static void test_update_install_takes_two_yeses() {
    Game g{StartMode::Hatched};
    seedAvailableUpdate(g);
    openUpdatesScreen(g);

    UpdateStatus found = g.updateStatus();
    CHECK(updateCheckRows(found) == 4);           // check + firmware + web + flasher

    g.onButton(press(Button::A));                 // row 0 -> the firmware row
    g.onButton(press(Button::B));                 // opens the confirm, installs nothing
    CHECK(!g.updateJobLive());

    g.onButton(press(Button::B));                 // B again lands on NO — still nothing
    CHECK(!g.updateJobLive());

    // And C out of the confirm is equally inert.
    g.onButton(press(Button::B));
    g.onButton(press(Button::C));
    CHECK(!g.updateJobLive());
    CHECK(g.nav() == Game::Nav::Detail);          // C left the confirm, not the screen

    // A to YES, then B: only now does anything start.
    g.onButton(press(Button::B));
    g.onButton(press(Button::A));
    g.onButton(press(Button::B));
    CHECK(g.updateJobLive());
    CHECK(g.updateJobTarget() == UpdateTarget::Firmware);

    // ...and like a check, it holds the radio past the screen it was started from.
    g.onButton(press(Button::C));
    CHECK(g.updateJobLive() && g.netConnectWanted());

    InstallStatus done;
    done.state = InstallState::Done;
    done.target = UpdateTarget::Firmware;
    done.restartNeeded = true;
    g.setInstallStatus(done);
    CHECK(!g.updateJobLive());
    CHECK(!g.netConnectWanted());
    CHECK(g.installStatus().state == InstallState::Done);
}

// The USB flasher's address is DERIVED from the manifest one, so a device pointed
// at a fork or a laptop offers that host's flasher without being told twice.
static void test_flasher_url_follows_the_publish_host() {
    char out[192];

    CHECK(updateFlasherUrl("https://antvena.github.io/Malwarium/manifest.json",
                           out, sizeof(out)));
    CHECK(std::strcmp(out, "https://antvena.github.io/Malwarium/flash/") == 0);

    // A laptop publish: same rule, different host, no scheme assumption.
    CHECK(updateFlasherUrl("http://192.168.1.50:8000/manifest.json", out, sizeof(out)));
    CHECK(std::strcmp(out, "http://192.168.1.50:8000/flash/") == 0);

    // Only the FILENAME is replaced — a nested publish keeps its whole path.
    CHECK(updateFlasherUrl("https://box.local/a/b/manifest.json", out, sizeof(out)));
    CHECK(std::strcmp(out, "https://box.local/a/b/flash/") == 0);

    // A bare host has no filename to replace, and earns the slash it lacks. The
    // "//" of the scheme is not a path separator, which is what this pins.
    CHECK(updateFlasherUrl("https://box.local", out, sizeof(out)));
    CHECK(std::strcmp(out, "https://box.local/flash/") == 0);

    // Nowhere to look, or nowhere to put it: no address, and `out` says so rather
    // than holding a half-built one.
    CHECK(!updateFlasherUrl("", out, sizeof(out)));
    CHECK(out[0] == '\0');
    CHECK(!updateFlasherUrl(nullptr, out, sizeof(out)));
    char tiny[8];
    CHECK(!updateFlasherUrl("https://box.local/manifest.json", tiny, sizeof(tiny)));
    CHECK(tiny[0] == '\0');
}

// The flasher row rides at the BOTTOM of a list whose middle changes, and it is
// the one row that works without a network — so B opens the code even when a
// check can't run, which is exactly when someone needs it.
static void test_flasher_row_is_last_and_needs_no_network() {
    Game g{StartMode::Hatched};
    // A source but no stored network: a check is impossible, the flasher is not.
    g.setUpdateManifestUrl("https://antvena.github.io/Malwarium/manifest.json");
    CHECK(!g.updateCheckReady());
    openUpdatesScreen(g);

    UpdateStatus idle;
    CHECK(updateCheckRows(idle) == 2);                        // check + flasher
    CHECK(updateCheckRowKind(idle, 0) == UpdateRowKind::Check);
    CHECK(updateCheckRowKind(idle, 1) == UpdateRowKind::FlashQr);

    g.onButton(press(Button::A));                             // onto the flasher row
    g.onButton(press(Button::B));
    CHECK(g.cfgScreen() == CfgScreen::UpdateQr);
    CHECK(g.qrScreenActive());                                // holds the panel awake
    CHECK(!g.updateJobLive());                                // and asks for no radio

    g.onButton(press(Button::C));                             // back to UPDATES, not out
    CHECK(g.cfgScreen() == CfgScreen::Update);
    CHECK(g.nav() == Game::Nav::Detail);
    CHECK(!g.qrScreenActive());

    // With nowhere to look there is nothing to encode, so the row is inert rather
    // than opening a screen that would have to apologise.
    Game blank{StartMode::Hatched};
    openUpdatesScreen(blank);
    blank.onButton(press(Button::A));
    blank.onButton(press(Button::B));
    CHECK(blank.cfgScreen() == CfgScreen::Update);
}

// A download in progress is not terminal; only Done and Failed hand the radio back.
static void test_update_install_progress_is_not_terminal() {
    Game g{StartMode::Hatched};
    seedAvailableUpdate(g);
    g.requestUpdateInstall(UpdateTarget::Web);
    CHECK(g.updateJobLive());

    for (InstallState s : {InstallState::Downloading, InstallState::Verifying,
                           InstallState::Installing}) {
        InstallStatus st;
        st.state = s;
        st.target = UpdateTarget::Web;
        st.received = 1024;
        st.total = 4096;
        g.setInstallStatus(st);
        CHECK(g.updateJobLive());
        CHECK(g.netConnectWanted());
    }

    InstallStatus bad;
    bad.state = InstallState::Failed;
    bad.fail = InstallFail::Corrupt;
    g.setInstallStatus(bad);
    CHECK(!g.updateJobLive());
    CHECK(!g.netConnectWanted());
    CHECK(g.installStatus().fail == InstallFail::Corrupt);

    // An install raises its own association, so it dies the same way a check does
    // when that join never comes up — nothing else would take the radio off it.
    g.requestUpdateInstall(UpdateTarget::Web);
    CHECK(g.updateJobLive());
    CHECK(g.netStatus().state == NetState::Connecting);
    NetStatus noJoin; noJoin.state = NetState::Failed;
    g.setNetStatus(noJoin);
    CHECK(!g.updateJobLive());
    CHECK(g.installStatus().state == InstallState::Failed);
    CHECK(g.installStatus().fail == InstallFail::NoNetwork);
}

// ---- Tar reader -----------------------------------------------------------

namespace {
// Build one tar header block. `size` is written as tar's NUL-terminated octal.
void tarHeader(uint8_t* block, const char* name, uint32_t size, char type) {
    std::memset(block, 0, kTarBlock);
    std::strncpy(reinterpret_cast<char*>(block), name, 99);
    std::snprintf(reinterpret_cast<char*>(block) + 124, 12, "%011o",
                  static_cast<unsigned>(size));
    block[156] = static_cast<uint8_t>(type);
}
}  // namespace

// An archive names its own destinations, so the name check is the whole security
// story of unpacking one. Anything that could climb out of the extraction root is
// rejected — never rewritten, because a name we had to fix is not one we understood.
static void test_tar_rejects_escaping_names() {
    for (const char* bad : {"../evil", "web/../../evil", "/etc/passwd", "a/../../b",
                            "..", "web\\..\\evil", ""}) {
        CHECK(!tarPathSafe(bad));
    }
    for (const char* good : {"index.html", "assets/ICON_STAT.png", "data/pedia_data.js",
                             "a..b/c", "...", "deep/nested/path/file.txt"}) {
        CHECK(tarPathSafe(good));
    }

    // ...and the header parser enforces it, so a caller can't reach a file handle
    // with a name it never validated.
    uint8_t block[kTarBlock];
    TarEntry e;
    tarHeader(block, "../../boot/app.bin", 64, '0');
    CHECK(parseTarHeader(block, sizeof(block), &e));
    CHECK(e.kind == TarEntryKind::Invalid);
}

// The rest of the header: types, octal sizes, the terminating zero block, and the
// padding maths that keeps the stream cursor aligned.
static void test_tar_reads_entries() {
    uint8_t block[kTarBlock];
    TarEntry e;

    tarHeader(block, "index.html", 1234, '0');
    CHECK(parseTarHeader(block, sizeof(block), &e));
    CHECK(e.kind == TarEntryKind::File);
    CHECK(std::strcmp(e.name, "index.html") == 0);
    CHECK(e.size == 1234);

    tarHeader(block, "assets/", 0, '5');          // a directory carries no data
    CHECK(parseTarHeader(block, sizeof(block), &e));
    CHECK(e.kind == TarEntryKind::Directory && e.size == 0);

    tarHeader(block, "link", 0, '2');             // a symlink: nothing the bundle uses
    CHECK(parseTarHeader(block, sizeof(block), &e));
    CHECK(e.kind == TarEntryKind::Skip);

    // A type byte of 0 is an old-style regular file, not a broken header.
    tarHeader(block, "style.css", 512, '\0');
    CHECK(parseTarHeader(block, sizeof(block), &e));
    CHECK(e.kind == TarEntryKind::File && e.size == 512);

    // A size field that isn't octal is a broken header — reporting it as a
    // zero-length file would leave the stream cursor pointing at nothing real.
    tarHeader(block, "bad.bin", 0, '0');
    std::memcpy(block + 124, "99999999999", 11);  // 8 and 9 are not octal digits
    CHECK(parseTarHeader(block, sizeof(block), &e));
    CHECK(e.kind == TarEntryKind::Invalid);

    // Real tar output is not always the tidy form `make pedia-tar` produces — a
    // bundle built with `tar -C dir .` prefixes every name with "./", and that has
    // to extract to the same place rather than being rejected as suspicious.
    tarHeader(block, "./assets/ICON_STAT.png", 300, '0');
    CHECK(parseTarHeader(block, sizeof(block), &e));
    CHECK(e.kind == TarEntryKind::File);

    std::memset(block, 0, sizeof(block));
    CHECK(parseTarHeader(block, sizeof(block), &e));
    CHECK(e.kind == TarEntryKind::End);

    // Data is padded up to a whole block; an exact multiple gains nothing.
    CHECK(tarDataSpan(0) == 0);
    CHECK(tarDataSpan(1) == kTarBlock);
    CHECK(tarDataSpan(kTarBlock) == kTarBlock);
    CHECK(tarDataSpan(kTarBlock + 1) == 2 * kTarBlock);

    // A short read is a truncated archive, not a short entry.
    CHECK(!parseTarHeader(block, kTarBlock - 1, &e));
}

// ---- Update manifest ------------------------------------------------------

namespace {
// A well-formed two-artifact manifest, in the published shape. `parse` takes a
// length, so these helpers pass strlen rather than assuming NUL-termination —
// the real input arrives as an HTTP body.
const char* kGoodManifest =
    "{\"artifacts\":["
    "{\"id\":\"firmware\",\"version\":\"0.4.2\",\"code\":402,"
    "\"url\":\"https://example.invalid/mal-0.4.2.bin\",\"size\":1758361,"
    "\"sha256\":\"" "0123456789abcdef0123456789abcdef"
                    "0123456789abcdef0123456789abcdef" "\"},"
    "{\"id\":\"web\",\"version\":\"0.4.0\",\"code\":400,"
    "\"url\":\"https://example.invalid/web-0.4.0.tar\",\"size\":291840,"
    "\"sha256\":\"" "fedcba9876543210fedcba9876543210"
                    "fedcba9876543210fedcba9876543210" "\"}"
    "]}";

ManifestParse parseOf(UpdateManifest& m, const char* s) {
    return m.parse(s, std::strlen(s));
}
}  // namespace

// The published shape round-trips, and rows are reachable by what they ARE rather
// than where they sit — a later publish reordering the list must not change which
// artifact the firmware check reads.
static void test_manifest_parses_published_shape() {
    UpdateManifest m;
    CHECK(parseOf(m, kGoodManifest) == ManifestParse::Ok);
    CHECK(m.count() == 2);

    const UpdateArtifact* fw = m.byId("firmware");
    CHECK(fw != nullptr);
    CHECK(std::strcmp(fw->version, "0.4.2") == 0);
    CHECK(fw->code == 402);
    CHECK(fw->size == 1758361);
    CHECK(std::strcmp(fw->url, "https://example.invalid/mal-0.4.2.bin") == 0);
    CHECK(fw->sha256[0] == 0x01 && fw->sha256[31] == 0xef);

    const UpdateArtifact* web = m.byId("web");
    CHECK(web != nullptr && web->code == 400);
    CHECK(web->sha256[0] == 0xfe && web->sha256[31] == 0x10);

    CHECK(m.byId("bootloader") == nullptr);   // absent, not row 0 by accident

    // Strictly newer, both directions, and never equal — an update prompt that
    // re-offers the build you're already on is one the operator learns to dismiss.
    CHECK(artifactIsNewer(*fw, 401));
    CHECK(!artifactIsNewer(*fw, 402));
    CHECK(!artifactIsNewer(*fw, 500));
}

// What actually arrives when the fetch goes wrong. A captive portal answering
// with a login page is the ordinary case, not the exotic one, so every shape here
// must be rejected outright rather than half-read into a plausible artifact.
static void test_manifest_rejects_junk_and_unusable_rows() {
    UpdateManifest m;

    // Not JSON at all — the portal login page.
    CHECK(parseOf(m, "<!DOCTYPE html><html><body>Sign in</body></html>") ==
          ManifestParse::Malformed);
    CHECK(m.count() == 0);

    // Valid JSON, wrong document.
    CHECK(parseOf(m, "{\"error\":\"not found\"}") == ManifestParse::Malformed);

    // Truncated mid-download: cutting the good manifest anywhere must never yield
    // a usable row, whatever the cut lands in the middle of.
    const size_t full = std::strlen(kGoodManifest);
    for (size_t cut = 1; cut < full; ++cut) {
        UpdateManifest t;
        if (t.parse(kGoodManifest, cut) == ManifestParse::Ok) {
            CHECK(false);   // a prefix of the manifest parsed as complete
            break;
        }
    }

    // An empty publish is its own answer, distinct from a broken one.
    CHECK(parseOf(m, "{\"artifacts\":[]}") == ManifestParse::NoArtifacts);

    // Rows that cannot be VERIFIED are not installable: no hash, a short hash, a
    // non-hex hash, and a zero size each sink the parse rather than yielding a row
    // the downloader would happily flash without checking.
    CHECK(parseOf(m, "{\"artifacts\":[{\"id\":\"firmware\",\"version\":\"1\","
                     "\"code\":1,\"url\":\"https://x/y\",\"size\":10}]}") ==
          ManifestParse::Malformed);
    CHECK(parseOf(m, "{\"artifacts\":[{\"id\":\"firmware\",\"version\":\"1\","
                     "\"code\":1,\"url\":\"https://x/y\",\"size\":10,"
                     "\"sha256\":\"abcd\"}]}") == ManifestParse::Malformed);
    CHECK(parseOf(m, "{\"artifacts\":[{\"id\":\"firmware\",\"version\":\"1\","
                     "\"code\":1,\"url\":\"https://x/y\",\"size\":0,"
                     "\"sha256\":\"0123456789abcdef0123456789abcdef"
                     "0123456789abcdef0123456789abcdef\"}]}") ==
          ManifestParse::Malformed);

    // A URL too long for the row's buffer is REJECTED, never truncated — a URL cut
    // at the buffer edge still looks like a good row and would fetch the wrong thing.
    std::string longUrl = "{\"artifacts\":[{\"id\":\"firmware\",\"version\":\"1\","
                          "\"code\":1,\"size\":10,\"sha256\":\"0123456789abcdef"
                          "0123456789abcdef0123456789abcdef0123456789abcdef\","
                          "\"url\":\"https://example.invalid/";
    longUrl.append(200, 'a');
    longUrl += "\"}]}";
    CHECK(parseOf(m, longUrl.c_str()) == ManifestParse::Malformed);
}

// A device must survive meeting a manifest published by a LATER firmware: unknown
// fields, unknown keys of any length, and more rows than it can hold. Failing any
// of these would strand every device already in the field the first time the
// manifest grows.
static void test_manifest_tolerates_a_newer_publish() {
    UpdateManifest m;
    CHECK(parseOf(m,
        "{\"schema\":3,\"generated\":\"2026-07-26T00:00:00Z\","
        "\"artifacts\":[{"
        "\"id\":\"firmware\",\"version\":\"0.5.0\",\"code\":500,"
        "\"url\":\"https://x/y.bin\",\"size\":42,"
        "\"sha256\":\"0123456789abcdef0123456789abcdef"
        "0123456789abcdef0123456789abcdef\","
        "\"notes\":\"fixes the thing\",\"channels\":[\"beta\",\"stable\"],"
        "\"a_field_name_far_longer_than_any_key_this_build_knows\":{\"x\":1}"
        "}]}") == ManifestParse::Ok);
    CHECK(m.count() == 1);
    const UpdateArtifact* fw = m.byId("firmware");
    CHECK(fw != nullptr && fw->code == 500 && fw->size == 42);

    // More rows than fit: the ones that do are usable, and the caller is told.
    std::string many = "{\"artifacts\":[";
    for (int i = 0; i < UpdateManifest::kMaxArtifacts + 2; ++i) {
        if (i) many += ',';
        many += "{\"id\":\"a";
        many += static_cast<char>('0' + i);
        many += "\",\"version\":\"1\",\"code\":1,\"url\":\"https://x/y\",\"size\":1,"
                "\"sha256\":\"0123456789abcdef0123456789abcdef"
                "0123456789abcdef0123456789abcdef\"}";
    }
    many += "]}";
    CHECK(parseOf(m, many.c_str()) == ManifestParse::TooMany);
    CHECK(m.count() == UpdateManifest::kMaxArtifacts);
    CHECK(m.byId("a0") != nullptr);
}

// ---- Web-bundle version marker --------------------------------------------

namespace {
bool markerOf(VersionMarker& v, const char* s) {
    return parseVersionMarker(s, std::strlen(s), &v);
}
}  // namespace

// The stamp an installer writes must be the stamp the next check reads — these are
// the two halves of the same fact, and a device that writes one shape and reads
// another would re-offer a bundle it just installed, forever.
static void test_version_marker_round_trips() {
    VersionMarker w;
    w.code = 402;
    std::strcpy(w.version, "0.4.2");

    char buf[kVersionMarkerCap];
    const size_t n = formatVersionMarker(w, buf, sizeof(buf));
    CHECK(n > 0 && n < sizeof(buf));

    VersionMarker r;
    CHECK(parseVersionMarker(buf, n, &r));
    CHECK(r.code == 402);
    CHECK(std::strcmp(r.version, "0.4.2") == 0);

    // The code is what gates, so it has to feed artifactIsNewer directly.
    UpdateArtifact a{};
    a.code = 500;
    CHECK(artifactIsNewer(a, r.code));
    a.code = 402;
    CHECK(!artifactIsNewer(a, r.code));

    // A buffer too small yields nothing rather than a truncated stamp: half of
    // "code=402" is still a parseable code, and a plausible wrong one.
    char tiny[6];
    CHECK(formatVersionMarker(w, tiny, sizeof(tiny)) == 0);
    CHECK(tiny[0] == '\0');
}

// A marker is read off a card that may have been written by a later publish, a
// different OS's line endings, or a person with a text editor. Everything that
// isn't a key this build knows is decoration.
static void test_version_marker_tolerates_real_files() {
    VersionMarker v;

    // The shape web/VERSION ships: comments, CRLF, blank lines, loose spacing.
    CHECK(markerOf(v,
        "# Which web 'Pedia bundle this is.\r\n"
        "\r\n"
        "  code = 100  \r\n"
        "version=0.1.0\r\n"));
    CHECK(v.code == 100 && std::strcmp(v.version, "0.1.0") == 0);

    // A key from a publish this build predates is skipped, not fatal — the whole
    // point of a key=value file over a bare number.
    CHECK(markerOf(v, "code=7\nchannel=beta\nnotes=whatever=this=is\nversion=0.0.7\n"));
    CHECK(v.code == 7 && std::strcmp(v.version, "0.0.7") == 0);

    // A final line with no newline is still a line.
    CHECK(markerOf(v, "version=9.9.9\ncode=90909"));
    CHECK(v.code == 90909 && std::strcmp(v.version, "9.9.9") == 0);

    // A version too long to hold is dropped rather than clipped, so the prompt
    // says nothing instead of naming a version nobody published. The code — the
    // only field that decides anything — survives.
    CHECK(markerOf(v, "code=3\nversion=0.1.0-rc1-build-20260101\n"));
    CHECK(v.code == 3 && v.version[0] == '\0');
}

// Unlike the manifest, a marker that won't parse is not a failure to report — it
// reads as "unknown", which makes any published bundle newer and offers a
// re-install. Redundant, never wrong; the install then replaces the unknown.
static void test_version_marker_absent_reads_as_unknown() {
    VersionMarker v;

    CHECK(!parseVersionMarker(nullptr, 0, &v));
    CHECK(v.code == 0);
    CHECK(!parseVersionMarker("", 0, &v));
    CHECK(v.code == 0);

    // An HTML page, a half-written file, a version with no code: all the same
    // answer, because none of them says what is installed.
    CHECK(!markerOf(v, "<!doctype html><title>404</title>"));
    CHECK(v.code == 0);
    CHECK(!markerOf(v, "version=0.4.2\n"));
    CHECK(v.code == 0 && v.version[0] == '\0');   // cleared, not half-filled
    CHECK(!markerOf(v, "code=\ncode=twelve\ncode=99999999999999\n"));
    CHECK(v.code == 0);

    // A garbled line must not clobber a good value read from another one.
    CHECK(markerOf(v, "code=12\ncode=NaN\n"));
    CHECK(v.code == 12);

    UpdateArtifact a{};
    a.code = 1;
    CHECK(artifactIsNewer(a, v.code) == false);   // 1 is not newer than 12
    VersionMarker unknown;
    CHECK(artifactIsNewer(a, unknown.code));      // ...but it is newer than nothing
}

// The QR carousel carries the setup step only while there is nothing stored, and A
// always walks past it — it must never stand between the operator and the 'Pedia.
static void test_qr_setup_step_appears_only_until_provisioned() {
    Game g{StartMode::Hatched};
    CHECK(pediaQrPages(/*provisioned=*/false) == 3);   // join, set up, 'Pedia
    CHECK(pediaQrPages(/*provisioned=*/true) == 2);    // join, 'Pedia

    enterCfgTarget(g, CfgScreen::PediaAp);             // CFG -> RADIO -> PEDIA AP (OFF)
    g.onButton(press(Button::A));                      // OFF -> ON
    g.onButton(press(Button::B));                      // apply -> the QR screen
    CHECK(g.cfgScreen() == CfgScreen::PediaQr && g.pediaQrPage() == 0);

    // Unprovisioned: three steps, wrapping back to the join code.
    g.onButton(press(Button::A)); CHECK(g.pediaQrPage() == 1);   // set up
    g.onButton(press(Button::A)); CHECK(g.pediaQrPage() == 2);   // 'Pedia
    g.onButton(press(Button::A)); CHECK(g.pediaQrPage() == 0);

    // Once a network is stored the setup step drops out, and the cursor can't be
    // left pointing past the end of the shorter carousel.
    g.setNetProvisioned(true);
    g.onButton(press(Button::A)); CHECK(g.pediaQrPage() == 1);   // 'Pedia
    g.onButton(press(Button::A)); CHECK(g.pediaQrPage() == 0);
}

// How an attempt ends decides whether the radio goes back. There is no screen
// holding it — the JOB is, so the outcomes are read through the job's lifetime.
static void test_net_attempt_outcomes() {
    Game g{StartMode::Hatched};
    g.setNetProvisioned(true);
    g.setUpdateSourceKnown(true);
    g.requestUpdateCheck();
    CHECK(g.netConnectWanted());

    NetStatus failed; failed.state = NetState::Failed;
    g.setNetStatus(failed);
    CHECK(!g.netConnectWanted());                     // radio handed back
    CHECK(g.netStatus().state == NetState::Failed);   // ...but the screen says why

    // Online is NOT terminal: the job holds the link it just raised, and only its
    // own outcome gives the radio back.
    g.requestUpdateCheck();
    NetStatus online; online.state = NetState::Online;
    std::strcpy(online.ip, "192.168.1.9");
    g.setNetStatus(online);
    CHECK(g.netConnectWanted());                      // an online link is held
    CHECK(std::strcmp(g.netStatus().ip, "192.168.1.9") == 0);

    UpdateStatus done; done.state = UpdateState::UpToDate;
    g.setUpdateStatus(done);
    CHECK(!g.netConnectWanted());                     // the job ended, so the link does
}

// v21 round-trip: the per-pet care-mistake shield + once-per-lifetime item gates,
// and fragAmountTier. (The v21 tail's dedup-mask field was removed as of v33 —
// see test_save_v33_drops_network_dedup below for that boundary.)
static void test_save_v21_shield_roundtrip() {
    SaveData a; std::strcpy(a.activeId, "paypup"); a.generation = 1;
    a.mistakeShieldActive = 1;
    a.shieldItemConsumed = 1;
    a.yubiConsumed = 1;
    a.fragAmountTier = 3;
    auto blob = serializeSave(a);
    SaveData out;
    CHECK(deserializeSave(blob, out));
    CHECK(out.mistakeShieldActive == 1);
    CHECK(out.shieldItemConsumed == 1);
    CHECK(out.yubiConsumed == 1);
    CHECK(out.fragAmountTier == 3);
}

// v20 → v21: a pre-shield blob has no v21 tail (3 u8 + u8 = 4 bytes, current/v33
// width). It must migrate to no shield / neither item consumed / fragAmountTier 0.
static void test_save_v20_to_v21_shield_default() {
    SaveData a; std::strcpy(a.activeId, "paypup"); a.generation = 1;
    a.mistakeShieldActive = 1;                   // would round-trip IF the tail were read
    a.fragAmountTier = 5;
    auto blob = forgeLegacyNetworkBytes(a, 20);
    blob.resize(blob.size() - 4);               // drop the v21 tail (3+1 bytes)
    blob[4] = 20; blob[5] = 0;                  // stamp the version word down to 20
    SaveData out;
    CHECK(deserializeSave(blob, out));          // a v20 blob still deserializes
    CHECK(out.mistakeShieldActive == 0);        // no tail → no shield
    CHECK(out.shieldItemConsumed == 0);
    CHECK(out.yubiConsumed == 0);
    CHECK(out.fragAmountTier == 0);             // no tail → default tier
}

// v33 removes the v4 dedup-pool byte, the v7 seenBssids vector, and the v21 full
// mask (real-network dedup/history moved to the SD-backed NetworkLedger) — a
// GENUINE v32 blob (forged via forgeLegacyNetworkBytes, carrying all three
// legacy fields a real v32 writer would have emitted) must still deserialize
// every LATER field correctly. This is the critical byte-alignment gate: if the
// version-gated skip logic in save.cpp's reader is even one byte off, every
// field after the removal points comes out wrong, not just these three.
static void test_save_v33_drops_network_dedup() {
    SaveData a; std::strcpy(a.activeId, "paypup"); a.generation = 1;
    a.networksSeen = 3;
    a.hackerRank = 1;
    a.mistakeShieldActive = 1;
    a.fragAmountTier = 2;                       // a v21 field AFTER all three removals
    a.fragTriggerTier = 6;                      // a v22 field — proves alignment holds
    // past the v21 splice too.
    auto blob = forgeLegacyNetworkBytes(a, 32);  // a genuine v32-shaped blob
    blob[4] = 32; blob[5] = 0;                  // stamp the version word down to 32

    SaveData out;
    CHECK(deserializeSave(blob, out));
    CHECK(out.networksSeen == 3);
    CHECK(out.hackerRank == 1);
    CHECK(out.mistakeShieldActive == 1);
    CHECK(out.fragAmountTier == 2);
    CHECK(out.fragTriggerTier == 6);
    CHECK(std::strcmp(out.activeId, "paypup") == 0);

    // ...and the current (v33) writer never emits the legacy bytes at all.
    SaveData rt;
    CHECK(deserializeSave(serializeSave(a), rt));
    CHECK(rt.networksSeen == 3);
    CHECK(rt.fragTriggerTier == 6);
}

// Save v21 → v22 (c): a pre-v22 blob reads fragTriggerTier as 0, and every v21
// field still round-trips (the v22 tail is a single appended byte).
static void test_save_v21_to_v22_frag_trigger_default() {
    SaveData a; std::strcpy(a.activeId, "paypup"); a.generation = 1;
    a.mistakeShieldActive = 1;                  // a v21 field that must still round-trip
    a.fragAmountTier = 3;
    a.fragTriggerTier = 4;                      // would round-trip IF the tail were read
    auto blob = forgeLegacyNetworkBytes(a, 21);
    blob.resize(blob.size() - 1);              // drop the v22 tail (1 byte)
    blob[4] = 21; blob[5] = 0;                  // stamp the version word down to 21
    SaveData out;
    CHECK(deserializeSave(blob, out));          // a v21 blob still deserializes
    CHECK(out.fragTriggerTier == 0);            // no tail → default trigger tier
    CHECK(out.mistakeShieldActive == 1);        // v21 fields survive
    CHECK(out.fragAmountTier == 3);

    // ...and a full v22 round-trip preserves the new field.
    SaveData rt;
    CHECK(deserializeSave(serializeSave(a), rt));
    CHECK(rt.fragTriggerTier == 4);
}

// Save v22 → v23 (d/e): a pre-v23 blob reads both one-time unlocks as false,
// and every v22 field still round-trips (the v23 tail is two appended bytes).
static void test_save_v22_to_v23_shop_unlocks_default() {
    SaveData a; std::strcpy(a.activeId, "paypup"); a.generation = 1;
    a.fragTriggerTier = 5;                       // a v22 field that must still round-trip
    a.itemTabsUnlocked = 1;                      // would round-trip IF the tail were read
    a.bulkOpenUnlocked = 1;
    auto blob = forgeLegacyNetworkBytes(a, 22);
    blob.resize(blob.size() - 2);               // drop the v23 tail (2 bytes)
    blob[4] = 22; blob[5] = 0;                   // stamp the version word down to 22
    SaveData out;
    CHECK(deserializeSave(blob, out));           // a v22 blob still deserializes
    CHECK(out.itemTabsUnlocked == 0);            // no tail → default false
    CHECK(out.bulkOpenUnlocked == 0);
    CHECK(out.fragTriggerTier == 5);             // v22 fields survive

    // ...and a full v23 round-trip preserves both new fields.
    SaveData rt;
    CHECK(deserializeSave(serializeSave(a), rt));
    CHECK(rt.itemTabsUnlocked == 1);
    CHECK(rt.bulkOpenUnlocked == 1);
}

// The 'Pedia AP runtime toggle + QR integration (default OFF): turning it ON
// through the RADIO group's "PEDIA AP" row leads to the QR screen (connect prompt),
// turning it OFF backs out to RADIO, and the opt-in survives a reboot (save v20).
static void test_ap_toggle_via_cfg_and_persist() {
    MemSaveStore store;
    {
        Game g(StartMode::Hatched, "paypup", &store);
        CHECK(!g.apEnabled());                    // default OFF (radio silent at boot)

        // Open PEDIA AP once; every exit below lands back on RADIO with the cursor
        // still on this row, so B re-opens it without re-navigating.
        enterCfgTarget(g, CfgScreen::PediaAp);    // CFG -> RADIO -> PEDIA AP (OFF)
        CHECK(g.nav() == Game::Nav::Detail);
        g.onButton(press(Button::A));             // OFF -> ON
        g.onButton(press(Button::B));             // apply ON -> QR
        CHECK(g.apEnabled());
        CHECK(g.nav() == Game::Nav::Detail && g.cfgScreen() == CfgScreen::PediaQr);
        g.onButton(press(Button::C));             // QR -> back to RADIO
        CHECK(g.cfgScreen() == CfgScreen::Radio);

        // "See the QR" while already ON: re-open the row, apply ON (no change) -> QR.
        g.onButton(press(Button::B));             // re-open (pick starts on ON)
        g.onButton(press(Button::B));             // apply ON -> QR again
        CHECK(g.cfgScreen() == CfgScreen::PediaQr);
        g.onButton(press(Button::C));             // back to RADIO

        // Turning it OFF has no QR — it backs straight to RADIO.
        g.onButton(press(Button::B));             // re-open (pick starts on ON)
        g.onButton(press(Button::A));             // ON -> OFF
        g.onButton(press(Button::B));             // apply OFF -> RADIO (no QR)
        CHECK(!g.apEnabled());
        CHECK(g.cfgScreen() == CfgScreen::Radio);
        g.onButton(press(Button::C));             // RADIO -> the CFG list
        CHECK(g.nav() == Game::Nav::Submenu);

        g.setApEnabled(true);                     // leave ON for the reboot check
        g.tick(kSaveAutosaveMs + kHeartbeatMs);
    }
    {
        // Reboot over the same store: the AP opt-in is restored.
        Game g(StartMode::FreshHatch, "paypup", &store);
        pickFirstEggLine(g);
        CHECK(g.apEnabled());
    }
}

// PEDIA QR: two pages (join-network / 'Pedia-URL) cycled with A, exited with C.
// The engine 5s auto-defocus is SUSPENDED on the QR page (scanning + joining takes
// longer than the idle window) and resumes the moment the player exits with C.
static void test_pedia_qr_two_pages_no_timeout() {
    Game g{StartMode::Hatched};
    // A settled device: with a home network already stored the carousel is its
    // two-step form (join, 'Pedia), which keeps this test about the TIMEOUT rather
    // than about how many setup steps happen to be in the list. The step list
    // itself is test_qr_setup_step_appears_only_until_provisioned's subject.
    g.setNetProvisioned(true);

    // Turn the AP on -> the QR page (starts on page 0, the join-network QR).
    enterCfgTarget(g, CfgScreen::PediaAp);        // CFG -> RADIO -> PEDIA AP (OFF)
    g.onButton(press(Button::A));                 // -> ON
    g.onButton(press(Button::B));                 // apply ON -> QR
    CHECK(g.qrScreenActive() && g.pediaQrPage() == 0);

    // A cycles the two pages without leaving the QR screen.
    g.onButton(press(Button::A));
    CHECK(g.qrScreenActive() && g.pediaQrPage() == 1);
    g.onButton(press(Button::A));
    CHECK(g.qrScreenActive() && g.pediaQrPage() == 0);

    // No timeout on the QR page: ticking well past the auto-defocus window keeps
    // the page up (the menu does NOT collapse to idle). tick() takes an absolute
    // clock, so run a monotonic `t`.
    uint32_t t = kAutoDefocusMs * 3 + kHeartbeatMs;
    g.tick(t);
    CHECK(g.qrScreenActive());                    // still on the QR page
    CHECK(g.nav() != Game::Nav::Idle);

    // Exit with C -> back to the RADIO group; the QR-page hold releases (this press
    // stamps lastInput at the current clock).
    g.onButton(press(Button::C));
    CHECK(!g.qrScreenActive());
    CHECK(g.cfgScreen() == CfgScreen::Radio);

    // Timeout resumes: now a long idle tick collapses the menu to idle as usual.
    t += kAutoDefocusMs * 2 + kHeartbeatMs;
    g.tick(t);
    CHECK(g.nav() == Game::Nav::Idle);
}

// TRAIN is now the real combat-prep loadout; EXPL stays a deferred shell.
static void test_train_expl_shells() {
    Framebuffer fb(kActiveW, kActiveH);
    // TRAIN: opens straight to the loadout (L2); B on a slot opens the move picker
    // (L3); C backs out to the carousel.
    { Game g{StartMode::Hatched}; enterSubmenuId(g, SubmenuId::Train);
      CHECK(g.nav() == Game::Nav::Submenu);
      g.render(fb); CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));
      g.onButton(press(Button::B));                  // slot 1 -> move picker
      CHECK(g.nav() == Game::Nav::Detail);
      g.render(fb); CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));
      g.onButton(press(Button::C));                  // back to the loadout
      CHECK(g.nav() == Game::Nav::Submenu);
      g.onButton(press(Button::C));
      CHECK(g.nav() == Game::Nav::Cursor); }

    // EXPL: the NESTED area/sub-area list. Areas are LINEAR complete-to-
    // advance; every sub-area of an OPEN area is reachable (explore any, one at a time).
    // Two-level nav: entering parks on the area-0 header (TOP level); B
    // DRILLS into the area, then B arms its first open sub-area 1 → the IDLE habitat (09
    // ). Re-open and A cycles to sub-area 2. Area 1's subs stay LOCKED until area 0
    // clears (its header still isn't enterable-to-arm — a locked area is inert).
    { Game g{StartMode::Hatched}; enterSubmenuId(g, SubmenuId::Expl);
      CHECK(g.nav() == Game::Nav::Submenu);
      g.render(fb); CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));
      g.onButton(press(Button::B));                  // drill into area 0
      g.onButton(press(Button::B));                  // arm sub-area 1 (index 0)
      CHECK(g.nav() == Game::Nav::Idle);
      CHECK(g.exploreActive() && g.exploreSector() == 0 && g.exploreSub() == 0);
      // Re-open with explore-mode RUNNING: EXPL resumes inside area 0 with the cursor
      // already on the armed sub-area (no drill press), so A advances straight to
      // sub-area 2 and B arms it.
      enterSubmenuId(g, SubmenuId::Expl);
      g.onButton(press(Button::A));                  // sub-area 1 -> sub-area 2
      g.onButton(press(Button::B));                  // arm explore on sub-area 2
      CHECK(g.nav() == Game::Nav::Idle && g.exploreSub() == 1);
      bool fl[kExplSectors] = {g.sectorCleared(0), g.sectorCleared(1)};
      CHECK(!explSectorOpen(1, fl)); }               // area 1 locked until area 0 clears
}

// ===========================================================================
// Persistence
// ===========================================================================

// The save blob round-trips: every field survives serialize -> deserialize.
static void test_save_roundtrip() {
    SaveData a;
    std::strcpy(a.activeId, "paypup");
    a.hunger = 42; a.frag = 13; a.happy = 88; a.mistakes = 3;
    a.debuffs = 2; a.ghost = 1; a.timeInStageMs = 123456; a.generation = 4;
    a.bits = 777; std::strcpy(a.hackerTag, "L33T_HAXX0R");
    a.lifetimeUptimeMs = 99999; a.lifetimeSteps = 12; a.petsRaised = 4;
    a.items.push_back(SaveStack{"airgap_snack", 3});
    a.items.push_back(SaveStack{"yubi_cookie", 1});
    a.ownedMods.push_back(SaveId{"firewall_patch"});
    a.equipped.push_back(SaveId{"firewall_patch"});
    a.equipped.push_back(SaveId{""});
    SaveLogEntry le; le.type = static_cast<uint8_t>(LogEventType::ItemUsed);
    std::strcpy(le.text, "USED YUBI-COOKIE"); a.log.push_back(le);
    SaveStoredPet sp; std::strcpy(sp.id, "cryptoshell");
    sp.hunger = 60; sp.generation = 2; sp.defragCount = 9;  // v16: rack pet's tally
    a.rack.push_back(sp);
    a.ownedMoves.push_back(SaveId{"packet_storm"});
    a.ownedMoves.push_back(SaveId{"fork_bomb"});
    a.equippedMoves.push_back(SaveId{"packet_storm"});
    a.equippedMoves.push_back(SaveId{""});
    a.combatXp = 35; a.combatLevel = 2;
    a.sectorCleared = {1, 0};                 // v8: sector 0 cleared, sector 1 not
    a.bootHatchRemainMs = 424242;             // v9: an egg mid-incubation
    a.titlesUnlocked = 0x3; a.equippedTitle = 1;  // v10: both Titles, sector 1 equipped
    a.bossUnlocked = {1, 1};                   // v12: both sectors' bosses unlocked
    a.subCleared = {0x07, 0x00};               // v13: area 0 subs 1-3 cleared, area 1 none
    a.subBossUnlocked = {0x0F, 0x01};          // v13: area 0 subs 1-4 unlocked, area 1 sub 1
    a.brightness = 2;                          // v14: a non-default backlight level
    a.subRefarm.assign(kExplSectors * kExplSubAreas, 0);  // v15: per-sub re-farm counts
    a.subRefarm[2] = 7; a.subRefarm[9] = 3;
    a.defragCount = 12;                        // v16: the active pet's defrag tally
    a.bwUpgradeCount = 5;                       // v19: Hacker SHOP bandwidth upgrades bought
    a.apEnabled = 1;                            // v20: 'Pedia local-AP toggle

    SaveData b;
    CHECK(deserializeSave(serializeSave(a), b));
    CHECK(std::strcmp(b.activeId, "paypup") == 0);
    CHECK(b.hunger == 42 && b.frag == 13 && b.happy == 88 && b.mistakes == 3);
    CHECK(b.debuffs == 2 && b.ghost == 1);
    CHECK(b.timeInStageMs == 123456u && b.generation == 4);
    CHECK(b.bits == 777 && std::strcmp(b.hackerTag, "L33T_HAXX0R") == 0);
    CHECK(b.lifetimeUptimeMs == 99999u && b.lifetimeSteps == 12u && b.petsRaised == 4);
    CHECK(b.items.size() == 2 && std::strcmp(b.items[0].id, "airgap_snack") == 0 &&
          b.items[0].qty == 3);
    CHECK(b.ownedMods.size() == 1 && b.equipped.size() == 2);
    CHECK(std::strcmp(b.equipped[0].id, "firewall_patch") == 0 && b.equipped[1].id[0] == '\0');
    CHECK(b.hasPermanentModData);   // v17: the permanent-mod semantics marker (D3)
    CHECK(b.log.size() == 1 && std::strcmp(b.log[0].text, "USED YUBI-COOKIE") == 0);
    CHECK(b.rack.size() == 1 && std::strcmp(b.rack[0].id, "cryptoshell") == 0 &&
          b.rack[0].generation == 2);
    CHECK(b.hasMoveData);
    CHECK(b.ownedMoves.size() == 2 && std::strcmp(b.ownedMoves[0].id, "packet_storm") == 0);
    CHECK(b.equippedMoves.size() == 2 &&
          std::strcmp(b.equippedMoves[0].id, "packet_storm") == 0 &&
          b.equippedMoves[1].id[0] == '\0');
    CHECK(b.combatXp == 35 && b.combatLevel == 2);
    CHECK(b.sectorCleared.size() == 2 && b.sectorCleared[0] == 1 &&
          b.sectorCleared[1] == 0);
    CHECK(b.bootHatchRemainMs == 424242u);
    CHECK(b.titlesUnlocked == 0x3u && b.equippedTitle == 1);
    CHECK(b.bossUnlocked.size() == 2 && b.bossUnlocked[0] == 1 &&
          b.bossUnlocked[1] == 1);
    CHECK(b.subCleared.size() == 2 && b.subCleared[0] == 0x07 && b.subCleared[1] == 0x00);
    CHECK(b.subBossUnlocked.size() == 2 && b.subBossUnlocked[0] == 0x0F &&
          b.subBossUnlocked[1] == 0x01);
    CHECK(b.brightness == 2);                  // v14: the backlight level round-trips
    CHECK(b.subRefarm.size() == static_cast<size_t>(kExplSectors * kExplSubAreas) &&
          b.subRefarm[2] == 7 && b.subRefarm[9] == 3);   // v15: re-farm counts round-trip
    CHECK(b.defragCount == 12);                          // v16: active defrag tally
    CHECK(b.rack.size() == 1 && b.rack[0].defragCount == 9);  // v16: rack pet tally
    CHECK(b.bwUpgradeCount == 5);                         // v19: SHOP bandwidth upgrades
    CHECK(b.apEnabled == 1);                              // v20: 'Pedia AP toggle round-trips
}

// a pre-v14 blob (no brightness tail) still loads, defaulting brightness to
// kBrightnessDefault (brightest) rather than a surprise dim. Forge a v13 blob by
// serializing then patching the version word down to 13 and dropping the i32 tail.
static void test_save_v13_to_v14_brightness_default() {
    SaveData a; std::strcpy(a.activeId, "paypup"); a.generation = 1;
    a.brightness = 1;                          // would be dim IF the tail were read
    auto blob = forgeLegacyNetworkBytes(a, 13);
    // Drop the trailing tails down to v13: the v15 subRefarm count (empty → u16 = 2
    // bytes) then the v14 brightness i32 (4 bytes), and stamp the version word to 13.
    blob.resize(blob.size() - 6);
    blob[4] = 13; blob[5] = 0;                  // little-endian u16 version at offset 4
    SaveData out;
    CHECK(deserializeSave(blob, out));          // a v13 blob still deserializes
    CHECK(out.brightness == kBrightnessDefault); // migrated to brightest, not dim
}

// a pre-v15 blob (no re-farm tail) still loads, defaulting every per-sub
// count to 0 (a migrated save's cleared areas start un-depleted). Forge a v14 blob by
// dropping the v15 subRefarm tail and stamping the version word down to 14.
static void test_save_v14_to_v15_refarm_default() {
    SaveData a; std::strcpy(a.activeId, "paypup"); a.generation = 1;
    auto blob = forgeLegacyNetworkBytes(a, 14);
    // Drop the trailing v15 subRefarm count (empty → u16 = 2 bytes); stamp version 14.
    blob.resize(blob.size() - 2);
    blob[4] = 14; blob[5] = 0;
    SaveData out;
    CHECK(deserializeSave(blob, out));          // a v14 blob still deserializes
    CHECK(out.subRefarm.empty());               // no tail → loader defaults counts to 0
}

// a pre-v16 blob (no defrag-count tail) still loads, defaulting the active
// and every rack pet's tally to 0. Forge a v15 blob by dropping the v16 tail (active
// i32 + a u16 rack-count = 0) and stamping the version word down to 15.
static void test_save_v15_to_v16_defrag_default() {
    SaveData a; std::strcpy(a.activeId, "paypup"); a.generation = 1;
    a.defragCount = 5;                          // would round-trip IF the tail were read
    auto blob = forgeLegacyNetworkBytes(a, 15);
    // Drop the v16 tail: the rack-count u16 (2 bytes, rack empty) + the active i32 (4).
    blob.resize(blob.size() - 6);
    blob[4] = 15; blob[5] = 0;
    SaveData out;
    CHECK(deserializeSave(blob, out));          // a v15 blob still deserializes
    CHECK(out.defragCount == 0);                // no tail → active tally defaults to 0
}

// the CFG Brightness row cycles a discrete level and B applies it to the
// live, persisted setting; setBrightness clamps out-of-range input.
static void test_cfg_brightness_apply() {
    Game g{StartMode::Hatched};
    CHECK(g.brightness() == kBrightnessDefault);  // starts brightest
    // Clamp guards.
    g.setBrightness(-5);  CHECK(g.brightness() == 0);
    g.setBrightness(999); CHECK(g.brightness() == kBrightnessLevels - 1);

    // Drive it through the CFG UI: CFG -> DEVICE -> BRIGHTNESS, cycle once, apply.
    g.setBrightness(0);                            // known start
    enterCfgTarget(g, CfgScreen::Brightness);      // the picker starts on the applied 0
    CHECK(g.nav() == Game::Nav::Detail);
    g.onButton(press(Button::A));                  // cycle 0 -> 1
    g.onButton(press(Button::B));                  // apply -> back to DEVICE
    CHECK(g.cfgScreen() == CfgScreen::Device);
    CHECK(g.brightness() == 1);                    // the live level changed + persists
}

// A v12 blob (no per-sub-area tail) still loads under v13: subCleared/subBossUnlocked
// stay empty, so the Game loader MIGRATES a cleared AREA → all 5 sub-areas cleared +
// their bosses unlocked (a beaten area had every sub-area beaten). An uncleared
// area's sub-areas all default false. Verified through the Game loader below.
static void test_save_v12_to_v13_migration() {
    // Forge a v12 blob: serialize (v13) a save with the two per-sub tails, drop them
    // (each a u16 count + N bytes), and patch the version word back to 12.
    SaveData a; std::strcpy(a.activeId, "paypup"); a.generation = 1;
    a.sectorCleared = {1, 0};                  // area 0 cleared, area 1 not
    a.bossUnlocked = {1, 0};                    // v12 field still present
    a.subCleared = {0, 0}; a.subBossUnlocked = {0, 0};  // would be lost in a v12 blob
    auto blob = forgeLegacyNetworkBytes(a, 12);
    CHECK(blob.size() > 4);
    // Drop both v13 tails (subBossUnlocked then subCleared), each u16 count + N bytes.
    blob.resize(blob.size() - (2 + a.subBossUnlocked.size()));
    blob.resize(blob.size() - (2 + a.subCleared.size()));
    blob[4] = 12; blob[5] = 0;                  // version word -> 12

    SaveData out;
    CHECK(deserializeSave(blob, out));          // a v12 blob still deserializes
    CHECK(out.subCleared.empty() && out.subBossUnlocked.empty());  // no tail (loader migrates)

    // Through the Game loader: the cleared area 0 migrates to all 5 sub-areas cleared +
    // unlocked; the uncleared area 1 has nothing.
    MemSaveStore store; store.save(blob);
    Game g(StartMode::Hatched, "paypup", &store);
    CHECK(g.sectorCleared(0) && !g.sectorCleared(1));
    for (int s = 0; s < kExplSubAreas; ++s) {
        CHECK(g.subCleared(0, s) && g.subBossUnlocked(0, s));    // cleared area → all subs
        CHECK(!g.subCleared(1, s) && !g.subBossUnlocked(1, s));  // uncleared area → none
    }
}

// A v9 blob (no Titles tail) still loads under v10: titlesUnlocked defaults to 0
// and equippedTitle to -1 (nothing earned, none equipped) — the honest default for
// a save that predates zone Titles.
static void test_save_v9_to_v10_migration() {
    // Forge a v9 blob: serialize (v10) a save, drop the 8-byte v10 tail (u32 mask +
    // i32 index), and patch the version word back to 9.
    SaveData a; std::strcpy(a.activeId, "paypup"); a.generation = 1;
    a.titlesUnlocked = 0x3; a.equippedTitle = 1;   // would be lost in a v9 blob
    auto blob = forgeLegacyNetworkBytes(a, 9);
    CHECK(blob.size() > 8);
    blob.resize(blob.size() - 8);             // drop the v10 tail (u32 + i32)
    blob[4] = 9; blob[5] = 0;                 // version word -> 9

    SaveData out;
    CHECK(deserializeSave(blob, out));        // a v9 blob still deserializes
    CHECK(out.titlesUnlocked == 0u && out.equippedTitle == -1);  // defaulted -> none
    CHECK(std::strcmp(out.activeId, "paypup") == 0);
}

// A pre-v17 (swappable-era) blob migrates to the permanent-mod model (D3): its
// ownedMods listed EVERY owned mod, equipped ones included. The loader must NOT gift
// a duplicate spare of an installed mod — an equipped id that also appears in
// ownedMods is dropped from the spare pool. v17 adds no wire bytes, so flipping the
// version word to 16 forges a byte-valid v16 blob.
static void test_save_v16_to_v17_mod_migration() {
    SaveData a; std::strcpy(a.activeId, "paypup"); a.generation = 1;
    // Old semantics: firewall + clock are equipped AND listed among owned; packet is a
    // genuine spare. equipped holds the two installed mods.
    a.ownedMods.push_back(SaveId{"firewall_patch"});
    a.ownedMods.push_back(SaveId{"clock_speed_boost"});
    a.ownedMods.push_back(SaveId{"packet_sniffer"});
    a.equipped.push_back(SaveId{"firewall_patch"});
    a.equipped.push_back(SaveId{"clock_speed_boost"});
    auto blob = forgeLegacyNetworkBytes(a, 16);
    blob[4] = 16; blob[5] = 0;                 // version word -> 16

    SaveData out;
    CHECK(deserializeSave(blob, out));
    CHECK(!out.hasPermanentModData);           // pre-v17 → migration path

    MemSaveStore store; store.save(blob);
    Game g(StartMode::Hatched, "paypup", &store);
    // Equipped mods stay installed; the ONLY spare is the genuine un-equipped one —
    // firewall/clock are not duplicated back into the pool.
    CHECK(std::strcmp(g.loadout().equipped(0), "firewall_patch") == 0);
    CHECK(std::strcmp(g.loadout().equipped(1), "clock_speed_boost") == 0);
    CHECK(g.loadout().owned().size() == 1 && g.loadout().owns("packet_sniffer"));
    CHECK(!g.loadout().owns("firewall_patch") && !g.loadout().owns("clock_speed_boost"));
}

// equip + persist: a Title equipped via the CFG picker survives a reboot, and
// Titles are player-level (a pet reset keeps them, like sector-clear flags).
static void test_zone_titles_equip_via_cfg_and_persist() {
    MemSaveStore store;
    {
        Game g(StartMode::Hatched, "paypup", &store);
        // No Titles yet -> nothing equipped, "NONE" shown.
        CHECK(g.equippedTitle() == -1);
        CHECK(std::strcmp(g.equippedTitleName(), "NONE") == 0);
        // Grant both (the real path is a sector clear; debug mirrors the grant).
        g.debugUnlockTitle(0);
        CHECK(g.equippedTitle() == 0);            // first Title auto-equips
        g.debugUnlockTitle(1);
        CHECK(g.equippedTitle() == 0);            // a later grant doesn't re-equip

        // Equip sector 1's Title through the CFG Titles picker.
        enterCfgTarget(g, CfgScreen::Titles);     // the picker starts on equipped=0
        CHECK(g.nav() == Game::Nav::Detail);
        g.onButton(press(Button::A));             // cycle 0 -> 1 (next unlocked)
        g.onButton(press(Button::B));             // apply -> back to the list
        CHECK(g.nav() == Game::Nav::Submenu);
        CHECK(g.equippedTitle() == 1);
        CHECK(std::strcmp(g.equippedTitleName(), sectorTitle(1)) == 0);
        g.tick(kSaveAutosaveMs + kHeartbeatMs);   // persist the equip
    }
    {
        // Reboot over the same store: unlocks + the equip are restored.
        Game g(StartMode::FreshHatch, "paypup", &store);
        pickFirstEggLine(g);
        CHECK(g.titleUnlocked(0) && g.titleUnlocked(1));
        CHECK(g.equippedTitle() == 1);
        CHECK(std::strcmp(g.equippedTitleName(), sectorTitle(1)) == 0);
        // Player-level: a pet reset keeps the earned Titles (like sectorCleared).
        g.resetToHatch();
        CHECK(g.titleUnlocked(0) && g.titleUnlocked(1));
        CHECK(g.equippedTitle() == 1);
    }
}

// picker ring: A cycles NONE + unlocked Titles only, skipping locked ones.
static void test_zone_titles_picker_skips_locked() {
    Game g{StartMode::Hatched};
    g.debugUnlockTitle(1);                        // unlock ONLY sector 1 (0 stays locked)
    CHECK(g.equippedTitle() == 1);                // auto-equipped the first earned
    enterCfgTarget(g, CfgScreen::Titles);         // the picker focus starts on 1
    g.onButton(press(Button::A));                 // 1 -> wraps past locked 0 to NONE(-1)
    g.onButton(press(Button::B));                 // equip NONE
    CHECK(g.equippedTitle() == -1);
    CHECK(std::strcmp(g.equippedTitleName(), "NONE") == 0);
}

// A v8 blob (no incubation-timer tail) still loads under v9: bootHatchRemainMs
// defaults to 0, so a migrated pet is treated as already hatched (no in-flight egg
// to resume) — the honest default for a save that predates the egg-at-idle redesign.
static void test_save_v8_to_v9_migration() {
    // Forge a v8 blob: serialize (v9) a save, drop the 4-byte v9 u32 tail, and patch
    // the version word back to 8.
    SaveData a; std::strcpy(a.activeId, "paypup"); a.generation = 1;
    a.bootHatchRemainMs = 999;                // would be lost in a v8 blob
    auto blob = forgeLegacyNetworkBytes(a, 8);
    CHECK(blob.size() > 4);
    blob.resize(blob.size() - 4);             // drop the v9 u32 tail
    blob[4] = 8; blob[5] = 0;                 // version word -> 8

    SaveData out;
    CHECK(deserializeSave(blob, out));        // a v8 blob still deserializes
    CHECK(out.bootHatchRemainMs == 0u);       // defaulted -> treated as hatched
    CHECK(std::strcmp(out.activeId, "paypup") == 0);
}

// A v7 blob (no sector-clear tail) still loads under v8: the sectorCleared vector
// stays empty, which the engine reads as "nothing cleared" (only sector 0 open) —
// the honest default for a save that predates linear gating.
static void test_save_v7_to_v8_migration() {
    // Forge a v7 blob: serialize (v8) an empty-sectorCleared save, drop the 2-byte
    // v8 tail (the u16 count = 0), and patch the version word back to 7.
    SaveData a; std::strcpy(a.activeId, "paypup"); a.generation = 1;
    CHECK(a.sectorCleared.empty());
    auto blob = forgeLegacyNetworkBytes(a, 7);
    CHECK(blob.size() > 2);
    blob.resize(blob.size() - 2);             // drop the v8 count word (was 0)
    blob[4] = 7; blob[5] = 0;                 // version word -> 7

    SaveData out;
    CHECK(deserializeSave(blob, out));        // a v7 blob still deserializes
    CHECK(out.sectorCleared.empty());         // nothing cleared -> only sector 0 open
    CHECK(std::strcmp(out.activeId, "paypup") == 0);
}

// A v1 blob (no move tail) still loads: deserialize accepts it with the v2 fields
// defaulted, and a Game booted over it seeds the starting move loadout (graceful
// forward-compat migration — the contract).
static void test_save_v1_migration() {
    // Forge a v1 blob: serialize (v2) a pet save with empty move vectors, strip the
    // 12-byte v2 tail (two count words + xp + level), and patch the version word.
    SaveData a; std::strcpy(a.activeId, "paypup"); a.generation = 1; a.hunger = 50;
    auto blob = serializeSave(a);
    CHECK(blob.size() > 12);
    blob.resize(blob.size() - 12);          // drop the v2 tail
    blob[4] = 1; blob[5] = 0;               // version word -> 1

    SaveData out;
    CHECK(deserializeSave(blob, out));      // a v1 blob still deserializes
    CHECK(!out.hasMoveData);                // no move data present
    CHECK(!out.hasLevelData);               // no v11 level tail present
    CHECK(out.combatLevel == 0 && out.combatXp == 0);   // 0-based model default
    CHECK(std::strcmp(out.activeId, "paypup") == 0);

    // Booting a Game over the v1 store seeds the starting move loadout (not stripped).
    MemSaveStore store;
    store.save(blob);
    Game g(StartMode::FreshHatch, "paypup", &store);
    pickFirstEggLine(g);
    CHECK(g.nav() == Game::Nav::Idle && g.pet());
    CHECK(g.moveLoadout().owns("payload_drop"));            // line-native starting kit seeded
    CHECK(g.moveLoadout().equipped(0) != nullptr);
    CHECK(g.combatLevel() == 0);   // pre-v11 blob migrates to a fresh level 0
}

// A missing / bad-magic / wrong-version / truncated blob deserializes as empty.
static void test_save_version_and_empty() {
    SaveData out;
    CHECK(!deserializeSave({}, out));                       // empty -> false
    CHECK(out.activeId[0] == '\0');                         // left defaulted
    std::vector<uint8_t> junk = {'X', 'X', 'X', 'X', 1, 0};
    CHECK(!deserializeSave(junk, out));                     // bad magic
    SaveData a; std::strcpy(a.activeId, "paypup");
    auto blob = serializeSave(a);
    blob[4] = 0xFF; blob[5] = 0xFF;                         // corrupt the version word
    CHECK(!deserializeSave(blob, out));
    auto trunc = serializeSave(a); trunc.resize(10);        // cut mid-stream
    CHECK(!deserializeSave(trunc, out));
    CHECK(out.activeId[0] == '\0');
}

// Boot-from-save vs Hatch: an empty store hatches; once a pet is raised + saved,
// a fresh Game over the same store boots straight onto the raised pet, restored.
static void test_boot_from_save_vs_hatch() {
    MemSaveStore store;
    CHECK(store.bytes().empty());
    {
        Game g(StartMode::FreshHatch, "paypup", &store);   // empty save -> line-select
        pickFirstEggLine(g);                               // pick Ransomware -> egg at idle
        CHECK(g.nav() == Game::Nav::Idle);
        CHECK(g.pet() && std::strcmp(g.pet()->id, "cryptoshell") == 0 && g.inEggPhase());
        CHECK(!store.bytes().empty());                     // the laid egg persisted
        uint32_t t = 1000; g.tick(t);
        g.tick(t += kBootHatchMs + kHeartbeatMs);          // wait out incubation -> Process
        // Random hatch: the CryptoShell egg -> a random ransomware Process
        // (Paypup or Conkittenate), so assert the STAGE, not a specific species.
        CHECK(g.pet() && g.pet()->stage == Stage::Process);
        CHECK(g.generation() == 1);
        g.model().setHunger(33);
        g.inventory().remove("airgap_snack", 1);           // 3 -> 2
        g.tick(t += kSaveAutosaveMs + kHeartbeatMs);        // periodic autosave
    }
    {
        Game g(StartMode::FreshHatch, "paypup", &store);   // would lay an egg if empty
        pickFirstEggLine(g);                               // no-op: booted from the save
        CHECK(g.nav() == Game::Nav::Idle);                 // booted from save instead
        CHECK(g.pet() && g.pet()->stage == Stage::Process);  // the hatched pet round-tripped
        CHECK(!g.inEggPhase());                            // restored as a hatched Process pet
        CHECK(g.generation() == 1);
        CHECK(g.model().hunger() == 33);                   // vitals restored
        CHECK(g.inventory().count("airgap_snack") == 2);   // inventory restored
    }
}

// A plain reboot (a fresh Game over the same store — the ARCH rack never enters
// it) round-trips the ACTIVE pet's creature level, not just its care stats.
// test_arch_store_deploy_preserves_creature_level covers the rack path; this
// covers the simpler power-cycle/reflash path, since NVS survives both.
static void test_reboot_preserves_active_creature_level() {
    MemSaveStore store;
    int lvl = 0, xp = 0;
    {
        Game g{StartMode::Hatched, "paypup", &store};
        g.debugAddCombatXp(1200);
        lvl = g.combatLevel();
        xp = g.combatXp();
        CHECK(lvl >= 1);
        g.tick(kSaveAutosaveMs + kHeartbeatMs);   // force the periodic autosave to fire
    }
    {
        Game g{StartMode::Hatched, "paypup", &store};
        CHECK(g.combatLevel() == lvl);
        CHECK(g.combatXp() == xp);
    }
}

// The dev / Factory reset wipes the raised pet and lays a fresh egg in its place:
// the store holds no raised pet, and the next boot resumes the fresh egg.
static void test_persistence_reset_clears_store() {
    MemSaveStore store;
    { Game g(StartMode::Hatched, "paypup", &store);
      g.tick(kSaveAutosaveMs + kHeartbeatMs); }            // autosave a raised pet
    CHECK(!store.bytes().empty());
    { Game g(StartMode::Hatched, "paypup", &store);
      CHECK(g.nav() == Game::Nav::Idle);                   // loaded from save
      g.resetToHatch();
      pickFirstEggLine(g);                       // reset re-enters line-select; pick Ransomware
      // The wipe replaced the raised pet with a freshly-laid egg (persisted, v9) —
      // the store is rewritten, not emptied (a laid egg is durable).
      CHECK(g.pet() && std::strcmp(g.pet()->id, "cryptoshell") == 0 && g.inEggPhase()); }
    { Game g(StartMode::FreshHatch, "paypup", &store);
      CHECK(g.nav() == Game::Nav::Idle);                   // boots the persisted egg
      CHECK(g.pet() && std::strcmp(g.pet()->id, "cryptoshell") == 0 && g.inEggPhase());
      CHECK(g.generation() == 1); }                        // the reset pet is generation 1
}

// HackerTag on-device editor: A cycles a cell, B advances, the OK cell
// saves; the edited tag persists.
static void test_hackertag_editor() {
    MemSaveStore store;
    Game g(StartMode::Hatched, "paypup", &store);
    enterCfgTarget(g, CfgScreen::HackerTag);               // open the editor
    CHECK(g.nav() == Game::Nav::Detail);
    g.onButton(press(Button::A));                          // cell 0: 'N' -> 'O'
    for (int i = 0; i < kHackerTagMax; ++i) g.onButton(press(Button::B));  // caret -> OK
    g.onButton(press(Button::B));                          // OK -> save
    CHECK(g.nav() == Game::Nav::Submenu);
    CHECK(std::strcmp(g.hackerTag(), "OETRUNNER_99") == 0);
    // Persists across a reboot.
    g.tick(kSaveAutosaveMs + kHeartbeatMs);
    Game g2(StartMode::Hatched, "paypup", &store);
    CHECK(std::strcmp(g2.hackerTag(), "OETRUNNER_99") == 0);
}

// ARCH Store + Deploy: Store sets the active pet aside into the rack and
// fires the new-egg Hatch; Deploy swaps a stored pet back in (slot-neutral).
static void test_arch_store_and_deploy() {
    Game g{StartMode::Hatched, "paypup"};                  // active = Paypup, gen 1
    CHECK(g.rackCount() == 0 && g.generation() == 1);

    enterSubmenuId(g, SubmenuId::Arch);
    g.onButton(press(Button::B));                          // open active record (Store)
    g.onButton(press(Button::B));                          // Store -> confirm (default Cancel)
    g.onButton(press(Button::A));                          // Cancel -> Confirm
    g.onButton(press(Button::B));                          // commit Store
    pickFirstEggLine(g);                                   // vacated -> line-select; pick Ransomware
    CHECK(g.nav() == Game::Nav::Idle);                     // active vacated -> new egg at idle
    CHECK(g.rackCount() == 1 && g.pet() && g.inEggPhase());
    CHECK(std::strcmp(g.pet()->id, "cryptoshell") == 0);
    CHECK(g.generation() == 2);                            // lifetime ordinal advanced

    enterSubmenuId(g, SubmenuId::Arch);
    g.onButton(press(Button::A));                          // focus the stored row (Paypup)
    g.onButton(press(Button::B));                          // open stored record (Deploy)
    g.onButton(press(Button::B));                          // Deploy -> confirm
    g.onButton(press(Button::A));                          // -> Confirm
    g.onButton(press(Button::B));                          // commit Deploy
    CHECK(g.nav() == Game::Nav::Idle);
    CHECK(g.pet() && std::strcmp(g.pet()->id, "paypup") == 0);   // Paypup is active again
    CHECK(g.generation() == 1);                            // its own generation rode along
    CHECK(g.rackCount() == 1);                             // slot-neutral: CryptoShell stored
}

// STAT surfaces the persisted generation + the lifetime footer.
static void test_stat_footer_and_generation() {
    Game g{StartMode::Hatched, "cryptoshell"};
    CHECK(g.generation() == 1);
    enterSubmenuId(g, SubmenuId::Stat);
    Framebuffer fb(kActiveW, kActiveH);
    g.render(fb);                                           // vitals page (name + gen)
    CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));
    g.onButton(press(Button::A));                          // page to the Hacker-Log
    g.render(fb);
    CHECK(hasDarkInk(fb, 0, 150, kActiveW, kActiveH));      // lifetime footer band renders
}

// Grayscale gate: the ARCH record with a populated rack (Deploy variant) reads
// without colour, and the inline confirm overlay does too.
static void test_arch_rack_grayscale() {
    Game g{StartMode::Hatched, "paypup"};
    g.debugSeedRack("cryptoshell");
    enterSubmenuId(g, SubmenuId::Arch);
    Framebuffer fb(kActiveW, kActiveH);
    g.render(fb); CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));   // list w/ stored row
    g.onButton(press(Button::A));                          // focus the stored pet
    g.onButton(press(Button::B));                          // open its record (Deploy)
    g.render(fb); CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));
    g.onButton(press(Button::B));                          // Deploy -> confirm overlay
    g.render(fb); CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));
}

// Single-frame creatures render statically without flicker: idleFrame() never
// indexes a missing frame, so a 1-frame placeholder sprite is usable as-is and a
// richer sheet animates automatically as data (add-now / animate-later).
static void test_idle_frame_single_frame_safe() {
    SpriteData one{}; one.frames = 1;            // a single-frame placeholder
    for (int b = 0; b < 24; ++b) CHECK(idleFrame(one, b) == 0);   // always valid + static

    SpriteData two{}; two.frames = 2;            // breathe, but no blink frame
    bool saw0 = false, saw1 = false;
    for (int b = 0; b < 24; ++b) {
        const int f = idleFrame(two, b);
        CHECK(f >= 0 && f < 2);                  // never the absent blink frame
        saw0 |= (f == 0); saw1 |= (f == 1);
    }
    CHECK(saw0 && saw1);                          // both breathe frames used

    const SpriteData& pay = ASSET_SPR_PET_PAYPUP;  // rich sheet: blink reachable
    bool sawBlink = false;
    for (int b = 0; b < 24; ++b) {
        const int f = idleFrame(pay, b);
        CHECK(f >= 0 && f < pay.frames);
        sawBlink |= (f == 2);
    }
    CHECK(sawBlink);
}

// --- Resting motion (core/model/idle_wander.h) -----------------------------

// Run a mover for a while and describe where it went. `beats` are heartbeats, so
// these figures are what the habitat would actually have drawn over that stretch.
struct WanderTrace {
    int minX = 0, maxX = 0, minY = 0, maxY = 0;
    int onShelfBeats = 0;      // heartbeats spent with the feet on the floor
    int lowBeats = 0;          // ...spent in the bottom third of the box
    int highBeats = 0;         // ...and in the top third
    int movingBeats = 0;       // ...spent going somewhere on either axis
    int movedYBeats = 0;       // ...spent changing height
};
static WanderTrace traceWander(Locomotion loco, int beats) {
    IdleWander w;
    WanderTrace t;
    int lastX = 0, lastY = 0;
    for (int i = 0; i < beats; ++i) {
        w.step(loco);
        const int x = w.offsetX(), y = w.offsetY();
        t.minX = std::min(t.minX, x); t.maxX = std::max(t.maxX, x);
        t.minY = std::min(t.minY, y); t.maxY = std::max(t.maxY, y);
        if (y == 0) ++t.onShelfBeats;
        if (y <= kWanderRiseMax / 3) ++t.lowBeats;
        if (y >= kWanderRiseMax * 2 / 3) ++t.highBeats;
        if (x != lastX || y != lastY) ++t.movingBeats;
        if (y != lastY) ++t.movedYBeats;
        lastX = x; lastY = y;
    }
    return t;
}

// The box is the whole safety contract: the sprite is drawn from this offset, so a
// mover that walks out of it walks off the canvas. Every locomotion is bounded by
// the same box no matter how long it runs, and none of them ever go BELOW the shelf.
static void test_idle_wander_stays_inside_the_living_box() {
    for (Locomotion loco : {Locomotion::Walk, Locomotion::Fly, Locomotion::Swim}) {
        const WanderTrace t = traceWander(loco, 4000);
        CHECK(t.minX >= -kWanderHalfSpanX && t.maxX <= kWanderHalfSpanX);
        CHECK(t.minY >= 0 && t.maxY <= kWanderRiseMax);
        CHECK(t.maxX > 0 && t.minX < 0);      // and it uses both sides, not one
    }
}

// The three read as three different creatures, which is the point of the field:
// a walker is a floor animal that ambles and then stands still, a flier is almost
// always in the air, and a swimmer is neither pulled down nor holding a height —
// it just keeps drifting, on both axes at once.
static void test_idle_wander_reads_differently_per_locomotion() {
    const int beats = 4000;

    const WanderTrace walk = traceWander(Locomotion::Walk, beats);
    CHECK(walk.onShelfBeats == beats);        // never once off the floor
    CHECK(walk.movedYBeats == 0);
    CHECK(walk.movingBeats > 0);              // but it does get about
    CHECK(walk.movingBeats < beats / 2);      // ...spending most of its time parked

    const WanderTrace fly = traceWander(Locomotion::Fly, beats);
    CHECK(fly.onShelfBeats * 100 < beats);    // touches down on under 1% of beats
    CHECK(fly.lowBeats * 10 < beats);         // and is rarely even near the floor
    CHECK(fly.maxY == kWanderRiseMax);        // it is the top of the box it lives in
    CHECK(fly.movingBeats > beats / 2);       // hardly ever still

    const WanderTrace swim = traceWander(Locomotion::Swim, beats);
    CHECK(swim.onShelfBeats * 100 < beats);   // nothing pulls it down...
    CHECK(swim.highBeats * 10 > beats);       // ...and nothing holds it up either:
    CHECK(swim.lowBeats * 10 > beats);        // it uses the whole depth of the box
    CHECK(swim.movingBeats > beats / 2 && swim.movedYBeats > beats / 4);
}

// The habitat has ONE call site and no reset hook, so the component has to notice a
// new occupant itself — otherwise a walker that evolved into a swimmer would keep
// its feet glued to the shelf until a reboot.
static void test_idle_wander_rehomes_when_the_mover_changes() {
    IdleWander w;
    for (int i = 0; i < 200; ++i) w.step(Locomotion::Walk);
    CHECK(w.offsetY() == 0);

    w.step(Locomotion::Swim);                 // a different creature entirely
    CHECK(w.offsetX() == 0 && w.offsetY() == 0);   // back on the anchor to start from
    bool leftTheFloor = false;
    for (int i = 0; i < 200; ++i) { w.step(Locomotion::Swim); leftTheFloor |= w.offsetY() > 0; }
    CHECK(leftTheFloor);

    w.park();
    CHECK(w.offsetX() == 0 && w.offsetY() == 0);
}

// The seam, at the engine: a raised pet drifts around its shelf as the heartbeat
// runs, and an egg does not — an egg sits where it was laid, with its incubation
// countdown drawn directly above it.
static void test_habitat_moves_a_pet_and_parks_an_egg() {
    Game g{StartMode::Hatched};
    uint32_t t = 0;
    CHECK(g.nav() == Game::Nav::Idle);
    bool moved = false;
    for (int i = 0; i < 200 && !moved; ++i) {
        g.tick(t += kHeartbeatMs);
        moved = g.petWander().offsetX() != 0;
    }
    CHECK(moved);

    Game egg{StartMode::FreshHatch};
    for (int i = 0; i < 200; ++i) {
        egg.tick(t += kHeartbeatMs);
        CHECK(egg.petWander().offsetX() == 0 && egg.petWander().offsetY() == 0);
    }
}

// ===========================================================================
// Sim-Battle + combat integration
// ===========================================================================

// Walk TRAIN's loadout to the Sim-Battle row and launch the practice fight.
static void enterSimBattle(Game& g) {
    enterSubmenuId(g, SubmenuId::Train);
    const int unlocked = MoveLoadout::slotsForStage(g.pet()->stage);
    for (int i = 0; i < unlocked; ++i) g.onButton(press(Button::A));  // -> Sim-Battle row
    g.onButton(press(Button::B));                 // -> tier pick (Detail)
    g.onButton(press(Button::B));                 // Start -> combat
}

// A Sim-Battle plays end-to-end: launch from TRAIN, auto-resolve at the heartbeat,
// win returns to the loadout with a flat reward, safe stakes (no Frag), not logged.
static void test_sim_battle_end_to_end() {
    Game g{StartMode::Hatched, "paypup"};
    const int bits0 = g.bits();
    enterSimBattle(g);
    CHECK(g.nav() == Game::Nav::Combat);
    uint32_t t = 0;
    for (int i = 0; i < 400 && g.combat().outcome() == Combat::Outcome::Ongoing; ++i)
        g.tick(t += kHeartbeatMs);
    CHECK(g.combat().outcome() == Combat::Outcome::Win);     // a raised pet beats Basic
    g.onButton(press(Button::B));                            // dismiss -> TRAIN loadout
    CHECK(g.nav() == Game::Nav::Submenu);
    const int snifferBonus = ContentRegistry::embedded().mod("packet_sniffer")->magnitude;
    CHECK(g.bits() == bits0 + kSimBitsReward + snifferBonus ||
          g.bits() == bits0 + kSimBitsReward);               // flat Bits (sniffer optional)
    CHECK(g.bits() > bits0);
    CHECK(g.combatXp() == kSimXpReward);                     // combat XP grew
    CHECK(g.model().fragmentation() == kStartFragmentation); // safe stakes -> no Frag
    CHECK(g.log().size() == 0);                              // not logged
}

// A Sim Battle is the one fight an armed Backup Drive save sits out.
static void test_backup_drive_save_not_spent_in_sim_battle() {
    Game g{StartMode::Hatched, "paypup"};
    g.debugUseItem("backup_drive");
    CHECK(g.backupShieldArmed());
    // Consuming the last Backup Drive drops useItem() into Nav::Submenu (the ITEMS
    // "item left the list" case) — back out to the carousel before enterSimBattle's
    // TRAIN walk, which assumes it's starting from the carousel/idle layer.
    g.onButton(press(Button::C));
    enterSimBattle(g);
    // A Sim Battle deliberately fights WITHOUT the save: no stakes means no death to be
    // saved from, so a training bout must never burn a Rare item (Game::startSimBattle).
    CHECK(!g.combat().player().itemShield);
    CHECK(g.backupShieldArmed());   // untouched — still there for a fight that counts
}

// Creature levels: XP banks into levels on a geometric curve, each level
// grants +1 to a random combat stat, and level always equals the sum of earned
// points (the invariant Rollback preserves). A big lump crosses several boundaries.
static void test_creature_level_curve_and_invariant() {
    Game g{StartMode::Hatched, "paypup"};
    CHECK(g.combatLevel() == 0 && g.combatXp() == 0);
    for (int i = 0; i < kLevelStatCount; ++i) CHECK(g.levelStatPoint(i) == 0);

    // First level costs kLevelXpBase; one under it doesn't level, exactly it does.
    g.debugAddCombatXp(kLevelXpBase - 1);
    CHECK(g.combatLevel() == 0 && g.combatXp() == kLevelXpBase - 1);
    g.debugAddCombatXp(1);
    CHECK(g.combatLevel() == 1);
    CHECK(g.combatXp() == 0);                 // bucket spent exactly
    CHECK(g.levelStatPoint(0) + g.levelStatPoint(1) + g.levelStatPoint(2) +
          g.levelStatPoint(3) == 1);          // one point granted
    CHECK(g.lastLevelUpStat() >= 0 && g.lastLevelUpStat() < kLevelStatCount);

    // Second level costs more than the first (geometric, ~1.1x): base+1 XP is short.
    g.debugAddCombatXp(kLevelXpBase);
    CHECK(g.combatLevel() == 1);              // kLevelXpBase < cost of level 2
    // A big lump crosses multiple boundaries; the invariant + a bounded leftover hold.
    g.debugAddCombatXp(5000);
    int sum = 0;
    for (int i = 0; i < kLevelStatCount; ++i) sum += g.levelStatPoint(i);
    CHECK(sum == g.combatLevel());            // level == total earned points
    CHECK(g.combatXp() >= 0);                 // leftover bucket is a valid partial
    CHECK(g.combatLevel() >= 5);              // 5000 XP is many levels
}

// Level stat points feed the combat maths: a leveled pet's built Combatant
// carries exactly its earned points, additive over the un-levelled baseline (small
// point counts stay under the defense caps, so the arithmetic is exact).
static void test_creature_level_feeds_combat() {
    // Baseline: an un-levelled Sim-Battle player.
    Game g0{StartMode::Hatched, "paypup"};
    enterSimBattle(g0);
    const Combatant base = g0.combat().player();

    // Leveled: a modest level so no stat hits the defense cap (≤~8 points each).
    Game g1{StartMode::Hatched, "paypup"};
    g1.debugAddCombatXp(1200);
    const int pP = g1.levelStatPoint(0), pD = g1.levelStatPoint(1),
              pS = g1.levelStatPoint(2), pH = g1.levelStatPoint(3);
    CHECK(pP + pD + pS + pH == g1.combatLevel());
    enterSimBattle(g1);
    const Combatant lv = g1.combat().player();

    CHECK(lv.maxHealth == base.maxHealth + pH * kLevelHealthPerPoint);
    CHECK(lv.speed == base.speed + pS * kLevelSpeedPerPoint);
    CHECK(lv.powerMultPct == base.powerMultPct + pP * kLevelPowerPctPerPoint);
    CHECK(lv.dmgReducePct == base.dmgReducePct + pD * kLevelDefensePctPerPoint);
    CHECK(lv.health == lv.maxHealth);         // starts full at the leveled max
}

// Rollback: opens a stat picker; a confirm sheds one earned point (−1 that
// stat, −1 level), zeroes the XP bucket (re-grind to re-roll), and consumes the item.
// Inert at level 0 (nothing to shed).
static void test_rollback_item() {
    // Level-0 gate: Rollback is inert with no earned points — no picker, item kept.
    { Game g{StartMode::Hatched, "paypup"};
      g.inventory().add("rollback", 1);
      g.debugUseItem("rollback");
      CHECK(g.nav() != Game::Nav::RollbackPicker);
      CHECK(g.inventory().count("rollback") == 1); }   // not consumed

    // Exactly one level → exactly one eligible stat; the picker parks on it, B sheds.
    { Game g{StartMode::Hatched, "paypup"};
      g.debugAddCombatXp(kLevelXpBase);                 // → level 1, one point somewhere
      CHECK(g.combatLevel() == 1);
      const int shed = g.lastLevelUpStat();
      CHECK(g.levelStatPoint(shed) == 1);
      g.inventory().add("rollback", 1);
      g.debugUseItem("rollback");
      CHECK(g.nav() == Game::Nav::RollbackPicker);
      g.onButton(press(Button::B));                     // shed the (only) eligible stat
      CHECK(g.combatLevel() == 0);                      // −1 level
      CHECK(g.levelStatPoint(shed) == 0);               // −1 that stat
      CHECK(g.combatXp() == 0);                         // XP re-zeroed (re-grind)
      CHECK(g.inventory().count("rollback") == 0);      // item consumed
      CHECK(g.nav() == Game::Nav::Submenu); }

    // C cancels without consuming the item or touching the level.
    { Game g{StartMode::Hatched, "paypup"};
      g.debugAddCombatXp(kLevelXpBase);
      g.inventory().add("rollback", 1);
      g.debugUseItem("rollback");
      CHECK(g.nav() == Game::Nav::RollbackPicker);
      g.onButton(press(Button::C));
      CHECK(g.combatLevel() == 1);
      CHECK(g.inventory().count("rollback") == 1); }
}

// Levels are PER-PET: they persist through an evolution (same creature) but
// a new egg resets them to 0. Also exercises the save v11 round-trip of the points.
static void test_creature_level_persist_evolution_reset_egg() {
    Game g{StartMode::Hatched, "paypup"};
    g.debugAddCombatXp(1200);
    const int lvl = g.combatLevel();
    CHECK(lvl >= 1);
    int pts[kLevelStatCount];
    for (int i = 0; i < kLevelStatCount; ++i) pts[i] = g.levelStatPoint(i);

    // Evolve Process → Script: the level + points carry (it's the same creature).
    uint32_t t = 0;
    g.debugTriggerEvolution();
    advanceToReveal(g, t);
    g.onButton(press(Button::B));
    CHECK(g.pet()->stage == Stage::Script);
    CHECK(g.combatLevel() == lvl);
    for (int i = 0; i < kLevelStatCount; ++i) CHECK(g.levelStatPoint(i) == pts[i]);

    // A new egg (resetToHatch) starts the level system fresh at 0.
    g.resetToHatch();
    pickFirstEggLine(g);                            // reset re-enters line-select
    CHECK(g.combatLevel() == 0 && g.combatXp() == 0);
    for (int i = 0; i < kLevelStatCount; ++i) CHECK(g.levelStatPoint(i) == 0);

    // Save v11 round-trips the stat points (sum == level).
    SaveData a; a.combatLevel = 3; a.combatXp = 42;
    a.statPoints = {1, 0, 2, 0}; a.hasLevelData = true;
    SaveData b;
    CHECK(deserializeSave(serializeSave(a), b));
    CHECK(b.hasLevelData && b.combatLevel == 3 && b.combatXp == 42);
    CHECK(b.statPoints.size() == 4 && b.statPoints[0] == 1 && b.statPoints[2] == 2);
}

// Regression: ARCH Store/Deploy round-trips a pet's creature level (combatLevel_/
// combatXp_/statPoints_/slotKinds_), not just its care stats. Before the fix,
// SaveStoredPet never carried these fields, so a leveled pet silently reset to
// level 0 on Store -> Deploy. Levels a pet, stores it, deploys a placeholder in
// its place, then deploys the leveled pet back out and confirms level/XP/points
// survived the round-trip through the rack.
static void test_arch_store_deploy_preserves_creature_level() {
    Game g{StartMode::Hatched, "paypup"};
    g.debugAddCombatXp(1200);                              // level the active pet up
    const int lvl = g.combatLevel();
    const int xp = g.combatXp();
    CHECK(lvl >= 1);
    int pts[kLevelStatCount];
    for (int i = 0; i < kLevelStatCount; ++i) pts[i] = g.levelStatPoint(i);

    // Store the leveled Paypup into the rack; a fresh egg becomes active.
    enterSubmenuId(g, SubmenuId::Arch);
    g.onButton(press(Button::B));                          // open active record (Store)
    g.onButton(press(Button::B));                          // Store -> confirm (default Cancel)
    g.onButton(press(Button::A));                          // Cancel -> Confirm
    g.onButton(press(Button::B));                          // commit Store
    pickFirstEggLine(g);                                   // vacated -> line-select; pick Ransomware
    CHECK(g.rackCount() == 1 && g.inEggPhase());
    // The new egg's own (fresh) level state must not bleed into the stored pet.
    CHECK(g.combatLevel() == 0 && g.combatXp() == 0);

    // Deploy the leveled Paypup back out of the rack.
    enterSubmenuId(g, SubmenuId::Arch);
    g.onButton(press(Button::A));                          // focus the stored row (Paypup)
    g.onButton(press(Button::B));                          // open stored record (Deploy)
    g.onButton(press(Button::B));                          // Deploy -> confirm
    g.onButton(press(Button::A));                          // -> Confirm
    g.onButton(press(Button::B));                          // commit Deploy
    CHECK(g.nav() == Game::Nav::Idle);
    CHECK(g.pet() && std::strcmp(g.pet()->id, "paypup") == 0);

    // The level, XP bucket, and every earned stat point survived the Store/Deploy
    // round-trip through the rack unchanged.
    CHECK(g.combatLevel() == lvl);
    CHECK(g.combatXp() == xp);
    for (int i = 0; i < kLevelStatCount; ++i) CHECK(g.levelStatPoint(i) == pts[i]);
}

// The move + mod loadout is per-pet: deploying a different rack pet must not
// carry the outgoing pet's TRAIN/MODS state onto it, and deploying the original
// pet back out must restore its own.
static void test_arch_deploy_loadout_is_per_pet() {
    Game g{StartMode::Hatched, "paypup"};   // ransomware line
    const char* paypupMove = g.moveLoadout().equipped(0);
    CHECK(paypupMove != nullptr);
    CHECK(std::strcmp(g.loadout().equipped(0), "firewall_patch") == 0);

    g.debugSeedRack("tadpoll");             // phishing line
    CHECK(g.rackCount() == 1);

    enterSubmenuId(g, SubmenuId::Arch);
    g.onButton(press(Button::A));           // focus the stored row (Tadpoll)
    g.onButton(press(Button::B));           // open stored record (Deploy)
    g.onButton(press(Button::B));           // Deploy -> confirm
    g.onButton(press(Button::A));           // -> Confirm
    g.onButton(press(Button::B));           // commit Deploy
    CHECK(g.pet() && std::strcmp(g.pet()->id, "tadpoll") == 0);

    for (int i = 0; i < kMaxMoveSlots; ++i) {
        const char* eq = g.moveLoadout().equipped(i);
        CHECK(!eq || std::strcmp(eq, paypupMove) != 0);
    }
    for (int i = 0; i < kModSlots; ++i) CHECK(g.loadout().equipped(i) == nullptr);

    enterSubmenuId(g, SubmenuId::Arch);
    g.onButton(press(Button::A));           // focus the stored row (Paypup)
    g.onButton(press(Button::B));           // open stored record (Deploy)
    g.onButton(press(Button::B));           // Deploy -> confirm
    g.onButton(press(Button::A));           // -> Confirm
    g.onButton(press(Button::B));           // commit Deploy
    CHECK(g.pet() && std::strcmp(g.pet()->id, "paypup") == 0);
    CHECK(std::strcmp(g.moveLoadout().equipped(0), paypupMove) == 0);
    CHECK(std::strcmp(g.loadout().equipped(0), "firewall_patch") == 0);
}

// Auto-battle paces itself: an autonomous turn resolves only every
// kCombatBeatsPerTurn heartbeats (readable exchanges), while the A "SKIP"
// fast-forward steps immediately regardless of the pace.
static void test_combat_auto_pacing() {
    { Game g{StartMode::Hatched, "paypup"};
      g.debugStartCombat(/*live=*/false);
      CHECK(g.nav() == Game::Nav::Combat);
      CHECK(g.combat().lastMoveName()[0] == '\0');   // no turn yet
      uint32_t t = 0;
      for (int i = 0; i < kCombatBeatsPerTurn - 1; ++i) g.tick(t += kHeartbeatMs);
      CHECK(g.combat().lastMoveName()[0] == '\0');    // still waiting out the pace
      g.tick(t += kHeartbeatMs);                       // the kCombatBeatsPerTurn-th beat
      CHECK(g.combat().lastMoveName()[0] != '\0'); }   // one turn resolved

    // A fast-forwards without waiting for the pace.
    { Game g{StartMode::Hatched, "paypup"};
      g.debugStartCombat(/*live=*/false);
      g.onButton(press(Button::A));
      CHECK(g.combat().lastMoveName()[0] != '\0'); }
}

// Combat-length balance: a typical fight should resolve in ~4–8
// exchanges (an exchange = both actors act ≈ 2 resolved turns). The per-stage
// offensive scale (kStagePowerScalePct) keeps player output pacing tier-scaled
// enemy Health so higher stages don't drag. We sample many seeded fights per
// representative matchup and assert the MEDIAN exchange count lands in-band —
// individual seeds spread wider (that variance is intended), so the median is the
// contract, not any single fight.
static int medianExchanges(const Combatant& player, const CombatEnemy& enemySpec,
                           const ContentRegistry& reg) {
    std::vector<int> ex;
    for (uint32_t s = 1; s <= 200; ++s) {
        Combatant e = makeEnemyCombatant(reg, enemySpec);
        Combat cb;
        cb.begin(player, e, Combat::Stakes::Live, s * 2654435761u + 1);
        int turns = 0, guard = 0;
        while (cb.outcome() == Combat::Outcome::Ongoing && guard++ < 5000)
            if (cb.step()) ++turns;
        ex.push_back((turns + 1) / 2);            // turns -> exchanges (ceil)
    }
    std::sort(ex.begin(), ex.end());
    return ex[ex.size() / 2];
}
static void test_combat_length_in_band() {
    ContentRegistry r = ContentRegistry::embedded();
    MoveLoadout ml = MoveLoadout::starting();
    Loadout mods = Loadout::starting();
    // Process / Script / Daemon each vs their sector-tier wild malbeast (both
    // variants), and the two sector bosses. Wilds carry the challenge buff
    // (kWildEnemy*Pct — +Health lengthens the fight), so their median sits in the
    // wider [4, 10] band; bosses are unbuffed and hold the tighter [4, 8].
    struct Case { const char* pet; int tier; };
    const Case wilds[] = {{"paypup", 1}, {"malbear", 2}, {"bruinforce", 3}};
    for (const Case& c : wilds) {
        Combatant p = makePlayerCombatant(r, *r.creature(c.pet), ml, mods);
        for (int v = 0; v < 2; ++v) {
            int m = medianExchanges(p, wildMalbeast(c.tier, v), r);
            // Speed-weighted turns let a faster pet land more actions per exchange, so an
            // early fast pet can close a fight a touch quicker — floor drops to 3.
            CHECK(m >= 3 && m <= 10);
        }
    }
    // The signature (apex) sub-area boss of each area vs a stage-appropriate pet: the
    // apex is "the wall," so it holds a slightly wider [4, 12] band than a wild.
    Combatant script = makePlayerCombatant(r, *r.creature("malbear"), ml, mods);
    Combatant daemon = makePlayerCombatant(r, *r.creature("bruinforce"), ml, mods);
    int mb0 = medianExchanges(script, subAreaBoss(0, kExplSubAreas - 1).rounds[0], r);
    int mb1 = medianExchanges(daemon, subAreaBoss(1, kExplSubAreas - 1).rounds[0], r);
    CHECK(mb0 >= 4 && mb0 <= 12);
    CHECK(mb1 >= 4 && mb1 <= 12);
}

// Wild-encounter challenge buff (kWildEnemy*Pct). Two contracts: (1) the buff is
// wild-only — a wild spec builds with boosted Health + a >100 damage mult, an
// otherwise-identical non-wild spec (bosses/Sim reuse the shape) stays neutral;
// (2) it bites — a Process pet still wins the sector-0 wild, but ends visibly
// wounded (well under the near-untouched ~86% the flat base stats produced).
static void test_wild_encounter_challenge_buff() {
    ContentRegistry r = ContentRegistry::embedded();
    // (1) Build the same base spec wild vs non-wild and compare.
    CombatEnemy base{"T", "SPR_PET_CACHEMUTT", 1, 40, 9, {"quick_jab"}, false};
    CombatEnemy wild = base; wild.isWild = true;
    Combatant nb = makeEnemyCombatant(r, base);
    Combatant wb = makeEnemyCombatant(r, wild);
    CHECK(nb.enemyDamageMultPct == 100 && nb.maxHealth == 40);        // neutral
    CHECK(wb.enemyDamageMultPct == kWildEnemyDamagePct);              // hits harder
    CHECK(wb.maxHealth == 40 * kWildEnemyHealthPct / 100);           // soaks more
    // (2) Win still lands, but at a real Health cost. Sample many seeds; the pet
    // must win the vast majority AND the median ending Health must be a clear dent.
    MoveLoadout ml = MoveLoadout::starting();
    Loadout mods = Loadout::starting();
    Combatant p = makePlayerCombatant(r, *r.creature("paypup"), ml, mods);
    int wins = 0; std::vector<int> endPct;
    for (uint32_t s = 1; s <= 200; ++s) {
        Combatant pc = p;
        Combatant e = makeEnemyCombatant(r, wildMalbeast(1, s % 2));
        Combat cb; cb.begin(pc, e, Combat::Stakes::Live, s * 2654435761u + 1);
        int g = 0;
        while (cb.outcome() == Combat::Outcome::Ongoing && g++ < 5000) cb.step();
        if (cb.outcome() == Combat::Outcome::Win) {
            ++wins;
            endPct.push_back(100 * cb.player().health / cb.player().maxHealth);
        }
    }
    CHECK(wins >= 190);                          // reliable win (≥95%)
    std::sort(endPct.begin(), endPct.end());
    const int medEnd = endPct[endPct.size() / 2];
    // A real dent, not a scratch. A fully-equipped Process pet (both slots filled)
    // doesn't dilute its pool with the weak Quick Jab default, so its attack/defend
    // split is an even 1/1 (50% atk) — enough defend weight that it ends measurably
    // tankier (=80 at the median).
    CHECK(medEnd <= 82);
}

// Wild win-rate for a given player vs a fully-built wild spec, sampled over many
// seeds. Mirrors the auto-explore path (Live stakes, hands-off resolve).
static int wildWinPct(const Combatant& player, const CombatEnemy& spec,
                      const ContentRegistry& reg) {
    int wins = 0; const int N = 300;
    for (uint32_t s = 1; s <= (uint32_t)N; ++s) {
        Combatant p = player;
        Combatant e = makeEnemyCombatant(reg, spec);
        Combat cb; cb.begin(p, e, Combat::Stakes::Live, s * 2654435761u + 1);
        int g = 0;
        while (cb.outcome() == Combat::Outcome::Ongoing && g++ < 5000) cb.step();
        if (cb.outcome() == Combat::Outcome::Win) ++wins;
    }
    return 100 * wins / N;
}

// The (area, sub) difficulty ramp: auto-explore must get
// HARDER through a sector's sub-areas, "steep/gated" so a fresh Process pet clears
// only the first sub-area or two before losses start cancelling runs. Three contracts:
//   (1) the ramp mutates the moveset for sub>0 and is a no-op at sub 0 (baseline);
//   (2) a fresh Paypup's win-rate is monotonic non-increasing across sub 0..4;
//   (3) it clears the early sub-areas (sub 0 near-certain) yet walls out by the apex
//       (sub 4 a hard gate) — a real, steep spread, not a flat line.
static void test_explore_subarea_ramp() {
    ContentRegistry r = ContentRegistry::embedded();
    // (1) Shape: sub 0 keeps the roster moves; each later sub swaps in a distinct,
    // heavier kit. Compare against the untouched baseline spec.
    CombatEnemy base = wildMalbeast(1, 0);
    CombatEnemy s0 = base; applyWildSubAreaRamp(s0, 0, 0);
    CHECK(s0.moveIds == base.moveIds);                 // sub 0 == baseline (no-op)
    CombatEnemy s4 = base; applyWildSubAreaRamp(s4, 0, 4);
    CHECK(s4.moveIds != base.moveIds);                 // apex is re-kitted
    // (2)+(3) Win-rate curve for a fresh Process pet across area 0's sub-areas. Average
    // the two roster variants per sub (their spread is intended), then require a
    // non-increasing curve with a near-certain floor and a hard-gated apex.
    MoveLoadout ml = MoveLoadout::starting();
    Loadout mods = Loadout::starting();
    Combatant paypup = makePlayerCombatant(r, *r.creature("paypup"), ml, mods);
    int win[kExplSubAreas];
    for (int sub = 0; sub < kExplSubAreas; ++sub) {
        int acc = 0;
        for (int v = 0; v < 2; ++v) {
            CombatEnemy e = wildMalbeast(1, v);
            applyWildSubAreaRamp(e, 0, sub);
            acc += wildWinPct(paypup, e, r);
        }
        win[sub] = acc / 2;
    }
    for (int sub = 1; sub < kExplSubAreas; ++sub)
        CHECK(win[sub] <= win[sub - 1] + 2);           // monotonic (small sampling slack)
    CHECK(win[0] >= 95);                               // sub 0 is a near-certain clear
    CHECK(win[kExplSubAreas - 1] <= 20);              // apex is a hard gate for a Process pet
    CHECK(win[0] - win[kExplSubAreas - 1] >= 60);     // a real, steep spread (not flat)
}

// Line-identity combat -------------------------

// Line-gating: a move is learnable/equippable iff generic (no line) or same line.
// This is what makes a Ransomware move exclusive to Ransomware pets — the whole point
// of adding Phishing moves no current pet can touch.
static void test_line_move_gating() {
    ContentRegistry r = ContentRegistry::embedded();
    // Generic moves stay open to everyone (incl. a lineless creature).
    CHECK(moveAllowedForLine(*r.move("quick_jab"), "ransomware"));
    CHECK(moveAllowedForLine(*r.move("buffer_overflow"), "phishing"));
    CHECK(moveAllowedForLine(*r.move("quick_jab"), nullptr));
    // Ransomware line moves: only Ransomware pets.
    CHECK(moveAllowedForLine(*r.move("payload_drop"), "ransomware"));
    CHECK(!moveAllowedForLine(*r.move("payload_drop"), "phishing"));
    CHECK(!moveAllowedForLine(*r.move("aes_lockbox"), nullptr));
    // Phishing line moves: never a Ransomware pet (the exclusivity proof).
    CHECK(moveAllowedForLine(*r.move("smish_hook"), "phishing"));
    CHECK(!moveAllowedForLine(*r.move("smish_hook"), "ransomware"));
    CHECK(!moveAllowedForLine(*r.move("bathyspoof"), "ransomware"));
    // The creature actually carries the line tag (Paypup = Ransomware).
    CHECK(r.creature("paypup")->line && std::strcmp(r.creature("paypup")->line,
                                                    "ransomware") == 0);
}

// Ransomware Lockout track stacks the caster's Power on each landed hit (capped), and
// Cipher track stacks Defense on each cast; the defend brace also scales with the
// caster's Defense-derived defenseMultPct. All transient (wiped every Combat::begin).
static void test_ransomware_stacking() {
    ContentRegistry r = ContentRegistry::embedded();
    // Lockout: Payload Drop only (single move → cast every player turn) vs a fat dummy.
    Combatant atk; atk.name = "R"; atk.maxHealth = 300; atk.health = 300; atk.speed = 10;
    atk.line = "ransomware"; atk.stage = Stage::Process;
    atk.moves.push_back(r.move("payload_drop"));
    Combatant dummy = mkCombatant(r, "D", 500, 3, {"quick_jab"});
    Combat cb; cb.begin(atk, dummy, Combat::Stakes::Safe, 4242);
    int firstHit = -1, lastHit = 0;
    for (int i = 0; i < 40 && cb.outcome() == Combat::Outcome::Ongoing; ++i) {
        const bool pturn = cb.playerTurnNext();
        cb.step();
        if (pturn && cb.lastByPlayer() && cb.lastDamage() > 0) {
            if (firstHit < 0) firstHit = cb.lastDamage();
            lastHit = cb.lastDamage();
        }
    }
    CHECK(cb.player().stackPowerBonus == 24);          // +8 ×3, capped at +24%
    CHECK(lastHit > firstHit);                          // stacked Power hits harder

    // Cipher: AES Lockbox only (defend every turn) — Defense stacks to its +20% cap,
    // and the brace magnitude scales with defenseMultPct (200 → double the 14 base).
    Combatant def; def.name = "W"; def.maxHealth = 300; def.health = 300; def.speed = 20;
    def.line = "ransomware"; def.stage = Stage::Process; def.defenseMultPct = 200;
    def.moves.push_back(r.move("aes_lockbox"));
    Combatant poke = mkCombatant(r, "P", 300, 1, {"quick_jab"});
    Combat cd; cd.begin(def, poke, Combat::Stakes::Safe, 99);
    cd.step();                                          // faster defender braces first
    CHECK(cd.player().guard == 28);                     // 14 base × 200% defenseMult
    for (int i = 0; i < 12 && cd.outcome() == Combat::Outcome::Ongoing; ++i) cd.step();
    CHECK(cd.player().stackDefenseBonus == 20);         // +10 ×2, capped at +20%
}

// Ransom Note: the Ransomware passive HOLDS the damage of hits taken inside an armed
// window, banks it in a pool, and drops the whole pool at once when the pool's countdown
// of the ransomer's own turns runs out. It defers damage, it never erases it.
static void test_ransom_note() {
    ContentRegistry r = ContentRegistry::embedded();
    auto makeRansomer = [&](const char* line, Stage stage, float speed) {
        Combatant p; p.name = "R"; p.maxHealth = 5000; p.health = 5000; p.speed = speed;
        p.line = line; p.stage = stage;
        p.moves.push_back(r.move("quick_jab"));
        return p;
    };

    // (1) The window itself, scripted. Boot's 0% arm chance means the roll never fires on
    //     its own (and draws no rng), so an armed window handed in is the only one — and
    //     begin() must preserve rather than re-roll it. A speed-1 pet against a speed-20
    //     enemy takes a run of hits before it ever acts: the FIRST is caught and closes
    //     the window, the rest land normally.
    Combatant p = makeRansomer("ransomware", Stage::BootSector, 1);
    p.ransomArmed = true;
    Combatant e = mkCombatant(r, "E", 5000, 20, {"quick_jab"});
    Combat cb; cb.begin(p, e, Combat::Stakes::Safe, 4242);
    CHECK(cb.player().ransomArmed);                     // begin() kept the handed-in window
    cb.step();
    const int held = cb.lastDamage();
    CHECK(cb.lastRansomed());                           // the hit it caught is held...
    CHECK(held > 0);
    CHECK(cb.player().ransomPool == held);              // ...into the pool, in full
    CHECK(cb.player().health == 5000);                  // ...and Health never moved
    CHECK(cb.player().ransomTurnsLeft == kRansomHoldTurns);
    CHECK(!cb.player().ransomArmed);                    // the window closed on that hit
    while (cb.outcome() == Combat::Outcome::Ongoing && !cb.playerTurnNext()) {
        const int before = cb.player().health;
        cb.step();
        CHECK(!cb.lastRansomed());                      // window shut: later hits land
        CHECK(cb.player().health == before - cb.lastDamage());
        CHECK(cb.player().ransomPool == held);          // ...and don't join the pool
    }
    // The countdown burns the ransomer's OWN turns: two tick by, then the third drops the
    // whole pool at once and costs that turn. The window is closed the whole time (Boot
    // re-rolls to false), so the enemy's hits in between land normally.
    auto upToRansomerTurn = [&] {   // stop with the ransomer's own turn pending
        while (cb.outcome() == Combat::Outcome::Ongoing && !cb.playerTurnNext()) cb.step();
    };
    upToRansomerTurn();  cb.step();  CHECK(cb.player().ransomTurnsLeft == 2);
    CHECK(cb.player().ransomPool == held);              // still owed, not yet paid
    upToRansomerTurn();  cb.step();  CHECK(cb.player().ransomTurnsLeft == 1);
    upToRansomerTurn();
    const int beforeBill = cb.player().health;          // the enemy's in-between hits landed
    cb.step();
    CHECK(std::strcmp(cb.lastMoveName(), "RANSOM DUE") == 0);
    CHECK(cb.lastDamage() == held);                     // the lump lands whole...
    CHECK(cb.player().health == beforeBill - held);     // ...and it is real damage
    CHECK(cb.player().ransomPool == 0);
    CHECK(cb.player().ransomTurnsLeft == 0);

    // (2) Riders are NOT ransomed — only the damage number is. A stun move landing inside
    //     an armed window still freezes the ransomer even though its damage was held.
    Combatant s = makeRansomer("ransomware", Stage::BootSector, 1);
    s.ransomArmed = true;
    Combatant st = mkCombatant(r, "E", 5000, 20, {"system_hang"});   // lockTurns rider
    Combat cs; cs.begin(s, st, Combat::Stakes::Safe, 11);
    cs.step();
    CHECK(cs.player().ransomPool > 0);                  // damage held...
    CHECK(cs.player().lockedTurnsLeft > 0);             // ...stun applied anyway

    // (3) Deferred, never erased — the invariant, across a real fight at a real arm rate.
    //     At every point: Health lost == damage dealt to the ransomer minus what the pool
    //     is still holding. Also asserts the passive actually fires at Daemon's rate.
    int fightsWithHold = 0, fightsWithBill = 0;
    for (uint32_t seed = 1; seed <= 60; ++seed) {
        Combatant d = makeRansomer("ransomware", Stage::Daemon, 10);
        Combatant o = mkCombatant(r, "E", 5000, 10, {"quick_jab"});
        Combat cd; cd.begin(d, o, Combat::Stakes::Safe, seed * 2654435761u + 1);
        int dealt = 0; bool sawHold = false, sawBill = false;
        for (int i = 0; i < 80 && cd.outcome() == Combat::Outcome::Ongoing; ++i) {
            cd.step();
            if (!cd.lastByPlayer()) dealt += cd.lastDamage();   // enemy hit the ransomer
            if (cd.lastRansomed()) sawHold = true;
            if (std::strcmp(cd.lastMoveName(), "RANSOM DUE") == 0) sawBill = true;
            CHECK(5000 - cd.player().health == dealt - cd.player().ransomPool);
        }
        if (sawHold) ++fightsWithHold;
        if (sawBill) ++fightsWithBill;
    }
    CHECK(fightsWithHold >= 50);        // fires reliably at 55%/turn over many turns
    CHECK(fightsWithBill >= 50);        // and the bill actually comes due

    // (4) SIDE-AGNOSTIC — the property a linked duel needs. The same ransomer seated in
    //     Combat's ENEMY slot (where a duel guest's pet sits, core/model/pvp_battle.h)
    //     gets the identical passive: its pool fills and its Health holds.
    bool enemySideHeld = false;
    for (uint32_t seed = 1; seed <= 20 && !enemySideHeld; ++seed) {
        Combatant plain = mkCombatant(r, "P", 5000, 10, {"quick_jab"});
        Combatant far = makeRansomer("ransomware", Stage::Daemon, 10);
        Combat cx; cx.begin(plain, far, Combat::Stakes::Safe, seed * 40503u + 7);
        int dealt = 0;
        for (int i = 0; i < 60 && cx.outcome() == Combat::Outcome::Ongoing; ++i) {
            cx.step();
            if (cx.lastByPlayer()) dealt += cx.lastDamage();    // player hit the ransomer
            CHECK(5000 - cx.enemy().health == dealt - cx.enemy().ransomPool);
            if (cx.enemy().ransomPool > 0) enemySideHeld = true;
        }
    }
    CHECK(enemySideHeld);

    // (5) A non-Ransomware pet never holds anything, and the line check short-circuits
    //     before any rng draw — so a fight without the line replays identically.
    Combatant q = makeRansomer(nullptr, Stage::Daemon, 10);
    Combatant e1 = mkCombatant(r, "E", 200, 8, {"buffer_overflow"});
    Combat a; a.begin(q, e1, Combat::Stakes::Safe, 777);
    while (a.outcome() == Combat::Outcome::Ongoing) {
        a.step();
        CHECK(!a.lastRansomed());
        CHECK(a.player().ransomPool == 0);
    }
    Combat c2; c2.begin(makeRansomer(nullptr, Stage::Daemon, 10), e1,
                        Combat::Stakes::Safe, 777);
    while (c2.outcome() == Combat::Outcome::Ongoing) c2.step();
    CHECK(a.player().health == c2.player().health);    // identical replay
}

// A passive nobody ever sees is a passive that doesn't exist. The mechanic working when
// armed says nothing about how often a REAL fight arms it: a healthy Process pet against
// the weakest practice dummy out-speeds it and takes only a couple of hits all fight, so
// the pool has very few chances to fill. This pins the reachable rate at that worst case,
// through the same builders the Game uses, so a future tuning pass can't quietly starve
// the passive back out of sight.
static void test_ransom_note_shows_up_in_pve() {
    ContentRegistry r = ContentRegistry::embedded();
    const CreatureDef* pet = r.creature("paypup");           // Process, ransomware
    MoveLoadout ml = MoveLoadout::starting();
    Loadout mods;
    const int kFights = 200;
    int fightsWithPool = 0;
    for (int s = 0; s < kFights; ++s) {
        Combat c;
        c.begin(makePlayerCombatant(r, *pet, ml, mods),
                makeEnemyCombatant(r, simDummy(0)),        // the softest fight in the game
                Combat::Stakes::Safe, s * 2654435761u + 17);
        for (int i = 0; i < 300 && c.outcome() == Combat::Outcome::Ongoing; ++i) {
            c.step();
            if (c.player().ransomPool > 0) { ++fightsWithPool; break; }
        }
    }
    CHECK(fightsWithPool > kFights * 2 / 3);   // seen in most fights, not the odd one
}

// the per-sub-area enemy LEVEL + the level-difference XP scaling.
//   (1) applyWildSubAreaRamp stamps a global depth level (+1 per sub, +5 per area)
//       and thickens Health at the deeper subs (the "rolled level-up stats");
//   (2) wildWinXp scales the flat base by (enemy − pet): parity = base, punching up
//       pays more, farming under pays a floored trickle, both ends clamped, never <1.
static void test_wild_subarea_level_and_xp_scaling() {
    // (1) Level is the global rung; deeper subs carry more Health than sub 0.
    CombatEnemy a0s0 = wildMalbeast(1, 0); applyWildSubAreaRamp(a0s0, 0, 0);
    CombatEnemy a0s4 = wildMalbeast(1, 0); applyWildSubAreaRamp(a0s4, 0, 4);
    CombatEnemy a1s0 = wildMalbeast(2, 0); applyWildSubAreaRamp(a1s0, 1, 0);
    CHECK(a0s0.level == 0);
    CHECK(a0s4.level == 4);                             // +1 per sub within a sector
    CHECK(a1s0.level == kSubAreasPerArea);             // +kSubAreasPerArea across areas
    CHECK(a0s4.maxHealth > a0s0.maxHealth);            // deeper sub soaks longer
    // (2) XP scaling. Parity → the flat base; the diff moves it symmetrically.
    CHECK(wildWinXp(kWildWinXpReward, 3, 3) == kWildWinXpReward);       // diff 0
    CHECK(wildWinXp(kWildWinXpReward, 5, 3) > kWildWinXpReward);        // punching up
    CHECK(wildWinXp(kWildWinXpReward, 1, 5) < kWildWinXpReward);        // farming under
    // Clamps: an absurd over-level caps at the ceiling; an absurd under-level floors
    // at the min; XP is never below 1.
    CHECK(wildWinXp(kWildWinXpReward, 99, 0) ==
          kWildWinXpReward * kWildXpDiffMaxPct / 100);
    CHECK(wildWinXp(kWildWinXpReward, 0, 99) ==
          kWildWinXpReward * kWildXpDiffMinPct / 100);
    CHECK(wildWinXp(1, 0, 99) >= 1);                    // floored trickle, never zero
}

// evolution-gating on MOVES (not just slots). A move opens at its
// minStage; owning it earlier isn't enough. Three contracts:
//   (1) the pure gate moveUnlockedAtStage();
//   (2) makePlayerCombatant fields an equipped-but-locked move ONLY once evolved
//       (always keeps the innate default);
//   (3) the equip picker refuses a locked move (a Boot cryptoshell owns its whole
//       ransomware kit from hatch, all Process+ gated — it can't slot any of it).
static void test_move_evolution_gating() {
    ContentRegistry r = ContentRegistry::embedded();
    // (1) The pure stage gate.
    const MoveDef* bo = r.move("buffer_overflow");   // Script
    const MoveDef* rk = r.move("rootkit_strike");    // Daemon
    const MoveDef* ps = r.move("packet_storm");      // Boot
    CHECK(!moveUnlockedAtStage(*bo, Stage::Process));
    CHECK(moveUnlockedAtStage(*bo, Stage::Script));
    CHECK(!moveUnlockedAtStage(*rk, Stage::Script));
    CHECK(moveUnlockedAtStage(*rk, Stage::Daemon));
    CHECK(moveUnlockedAtStage(*ps, Stage::BootSector));
    // (2) The combat gate. Equip a Script-gated move into an unlocked slot, then build
    // the combatant at Process (locked → inert) vs Script (unlocked → fielded).
    MoveLoadout ml;
    ml.grant("buffer_overflow");
    ml.equip(0, "buffer_overflow");
    Loadout mods;
    auto hasMove = [](const Combatant& c, const char* id) {
        for (const MoveDef* m : c.moves)
            if (std::strcmp(m->id, id) == 0) return true;
        return false;
    };
    Combatant proc = makePlayerCombatant(r, *r.creature("paypup"), ml, mods);   // Process
    Combatant scr  = makePlayerCombatant(r, *r.creature("malbear"), ml, mods);  // Script
    CHECK(!hasMove(proc, "buffer_overflow"));    // locked at Process → not fielded
    CHECK(hasMove(scr, "buffer_overflow"));       // unlocked at Script → fielded
    CHECK(hasMove(proc, "quick_jab") && hasMove(scr, "quick_jab"));  // default always
    // (3) The picker refuses a locked move. cryptoshell (Boot) owns its whole
    // ransomware-line kit from hatch (its nature) — every row gates at Process+,
    // so even the auto-equipped payload_drop is locked at Boot: the default
    // (filtered) view has nothing to offer for slot 0, and showAll reveals
    // payload_drop as a locked, unselectable row.
    Game g{StartMode::Hatched, "cryptoshell"};
    CHECK(std::strcmp(g.moveLoadout().equipped(0), "payload_drop") == 0);
    CHECK(ownedMoveList(r, g.moveLoadout(), g.slotRequiredKind(0), g.pet()->stage,
                        g.pet()->line, 0, /*showAll=*/false)
              .empty());
    enterSubmenuId(g, SubmenuId::Train);
    g.onButton(press(Button::B));                 // open slot 0 picker
    g.onButton(press(Button::A));                 // filtered list is just [unequip]; A stays put
    g.onButton(press(Button::B));                 // B on [unequip] clears the slot -> Submenu
    CHECK(g.moveLoadout().equipped(0) == nullptr);

    g.onButton(press(Button::B));                 // re-open slot 0 picker (resets moveShowAll_)
    g.debugSetMoveShowAll(true);                  // reveal the locked row for this check
    g.onButton(press(Button::A));                 // pick row 1 = payload_drop (locked at Boot)
    g.onButton(press(Button::B));                 // drill in — the page states the gate
    g.onButton(press(Button::B));                 // attempt equip → blocked (no-op)
    CHECK(g.moveLoadout().equipped(0) == nullptr);
}

// --- Move-slot rework (#11/#12) ---------------------------------------------

// #11 — the combat pool is exactly one entry per UNLOCKED slot: that slot's
// equipped move if present, else the innate default FILLING that slot — not an
// always-additive extra. Unequipped -> default x N; one equipped -> [move] +
// default x (N-1); fully equipped -> zero default entries in the pool.
static void test_move_pool_per_slot_fallback() {
    ContentRegistry r = ContentRegistry::embedded();
    Loadout mods;                                  // no mod passives in play
    const CreatureDef* proc = r.creature("paypup");    // Process: 2 slots
    const int slots = MoveLoadout::slotsForStage(proc->stage);
    CHECK(slots == 2);

    { // Unequipped: every slot falls back to the default.
        MoveLoadout ml;                             // owns/equips nothing
        Combatant c = makePlayerCombatant(r, *proc, ml, mods);
        CHECK(static_cast<int>(c.moves.size()) == slots);
        for (const MoveDef* m : c.moves)
            CHECK(std::strcmp(m->id, ml.defaultMove()) == 0);
    }
    { // One slot equipped: [move] + default filling the rest.
        MoveLoadout ml;
        ml.grant("packet_storm");
        ml.equip(0, "packet_storm");
        Combatant c = makePlayerCombatant(r, *proc, ml, mods);
        CHECK(static_cast<int>(c.moves.size()) == slots);
        CHECK(std::strcmp(c.moves[0]->id, "packet_storm") == 0);
        for (int i = 1; i < slots; ++i)
            CHECK(std::strcmp(c.moves[i]->id, ml.defaultMove()) == 0);
    }
    { // Fully equipped: zero default entries — a well-kitted pet never rolls
      // Quick Jab.
        MoveLoadout ml;
        ml.grant("packet_storm");
        ml.grant("checksum_guard");
        ml.equip(0, "packet_storm");
        ml.equip(1, "checksum_guard");
        Combatant c = makePlayerCombatant(r, *proc, ml, mods);
        CHECK(static_cast<int>(c.moves.size()) == slots);
        for (const MoveDef* m : c.moves)
            CHECK(std::strcmp(m->id, ml.defaultMove()) != 0);
    }
}

// #12 — type-lock: each move slot is permanently typed Attack/Defend (stamped by
// Game::stampSlotKinds from CreatureDef::slotKinds); ownedMoveList — exactly what
// onMovePicker/drawMovePicker read — is filtered to that kind, so a mismatched
// move is structurally UNREACHABLE through the UI (not merely dimmed), while a
// matching one equips normally. Malbear's seed stamps slot0/1 Attack and slot2
// Defend — asserted as literals here because the scenario NEEDS one slot of each
// kind on the same pet; if that stops holding, this test has nothing left to
// exercise and should fail rather than quietly test one kind twice. (Contrast
// test_move_slot_stamping_locks_at_unlock, which reads seeds from the row so a
// balance tweak can't break it.)
static void test_move_slot_type_lock() {
    Game g{StartMode::Hatched, "malbear"};
    CHECK(g.slotKind(0) == Game::SlotKind::Attack);
    CHECK(g.slotKind(1) == Game::SlotKind::Attack);
    CHECK(g.slotKind(2) == Game::SlotKind::Defend);
    CHECK(g.slotRequiredKind(0) == MoveDef::Kind::Attack);
    CHECK(g.slotRequiredKind(2) == MoveDef::Kind::Defend);

    ContentRegistry r = ContentRegistry::embedded();
    // Malbear owns its whole ransomware-line kit from hatch (line-native starter,
    // startingForLine): 3 Attack rows (payload_drop/double_extortion/mbr_wipe) and
    // 3 Defend rows (aes_lockbox/rsa_vault/full_disk_encryption). The Attack slot's
    // candidate list excludes every owned Defend move and vice-versa.
    auto atkList = ownedMoveList(r, g.moveLoadout(), g.slotRequiredKind(0),
                                 g.pet()->stage, g.pet()->line, 0, /*showAll=*/true);
    for (const MoveDef* m : atkList) CHECK(m->kind == MoveDef::Kind::Attack);
    bool atkListHasDefend = false;
    for (const MoveDef* m : atkList)
        if (std::strcmp(m->id, "aes_lockbox") == 0) atkListHasDefend = true;
    CHECK(!atkListHasDefend);                        // rejected — never even listed
    auto defList = ownedMoveList(r, g.moveLoadout(), g.slotRequiredKind(2),
                                 g.pet()->stage, g.pet()->line, 2, /*showAll=*/true);
    CHECK(defList.size() == 3);
    for (const MoveDef* m : defList) CHECK(m->kind == MoveDef::Kind::Defend);

    // Drive the real picker for slot 2 (Defend): B on its first candidate row
    // (aes_lockbox — Process-gated, already unlocked at Script) equips it — a
    // matching kind succeeds through the actual UI, not just the data layer.
    enterSubmenuId(g, SubmenuId::Train);
    g.onButton(press(Button::A));                    // slot0 -> slot1
    g.onButton(press(Button::A));                    // slot1 -> slot2
    g.onButton(press(Button::B));                     // open slot 2's picker (row0 = unequip)
    CHECK(g.nav() == Game::Nav::Detail);
    g.onButton(press(Button::A));                     // row0 -> row1 (aes_lockbox)
    g.onButton(press(Button::B));                     // drill into its detail page
    g.onButton(press(Button::B));                      // equip — no confirm (slot was empty)
    CHECK(std::strcmp(g.moveLoadout().equipped(2), "aes_lockbox") == 0);
}

// A creature's declared seed for one slot, as Game::SlotKind — what
// Game::stampSlotKinds is expected to copy onto the pet when that slot unlocks.
static Game::SlotKind seedKind(const ContentRegistry& r, const char* id, int slot) {
    const CreatureDef* c = r.creature(id);
    CHECK(c != nullptr);
    return c->slotKinds[slot] == MoveDef::Kind::Attack ? Game::SlotKind::Attack
                                                        : Game::SlotKind::Defend;
}

// #12 — stamping: a slot's kind is fixed the instant it first unlocks, from
// whichever creature is installed at that moment, and never changes again —
// evolving further never rewrites an already-stamped slot.
//
// Expectations are read from each creature's OWN slotKinds seed rather than
// written out as Attack/Defend literals. The mechanism under test is "slot N
// takes the kind of whoever is installed when N unlocks, and earlier slots are
// immutable" — WHICH kind any shipped creature seeds is a balance number
// (defs.h calls the current layout a placeholder), and a test that pins it turns
// every balance tweak into a false failure.
static void test_move_slot_stamping_locks_at_unlock() {
    ContentRegistry r = ContentRegistry::embedded();

    // Good branch: raise through the linear chain, capturing slot kinds at each
    // stage, and confirm slots 0-2 never change once stamped.
    { Game g{StartMode::Hatched, "cryptoshell"};       // Boot: only slot 0 unlocked
      const Game::SlotKind boot0 = g.slotKind(0);
      CHECK(boot0 == seedKind(r, "cryptoshell", 0));   // cryptoshell's own seed
      CHECK(g.slotKind(1) == Game::SlotKind::Unset);   // not yet unlocked

      g.debugTriggerEvolution();                       // -> paypup (Process, 2 slots)
      uint32_t t = 0; advanceToReveal(g, t);
      g.onButton(press(Button::B));
      CHECK(g.pet() && std::strcmp(g.pet()->id, "paypup") == 0);
      CHECK(g.slotKind(0) == boot0);                   // untouched by the evolve
      const Game::SlotKind slot1 = g.slotKind(1);
      CHECK(slot1 == seedKind(r, "paypup", 1));         // paypup's seed, freshly stamped

      g.debugTriggerEvolution();                       // -> barkmail (Script, 3 slots)
      t = 0; advanceToReveal(g, t);
      g.onButton(press(Button::B));
      CHECK(g.pet() && std::strcmp(g.pet()->id, "barkmail") == 0);
      CHECK(g.slotKind(0) == boot0);                   // still untouched
      const Game::SlotKind slot2 = g.slotKind(2);
      CHECK(slot2 == seedKind(r, "barkmail", 2));       // barkmail's seed, freshly stamped

      g.model().setCareMistakes(0);                     // 0-2 -> Good branch
      CHECK(g.model().careBranch() == CareBranch::Good);
      g.debugTriggerEvolution();                        // -> wire_heir (Daemon, 4 slots)
      t = 0; advanceToReveal(g, t);
      g.onButton(press(Button::B));
      CHECK(g.pet() && std::strcmp(g.pet()->id, "wire_heir") == 0);
      CHECK(g.slotKind(0) == boot0 && g.slotKind(1) == slot1 &&
            g.slotKind(2) == slot2);                    // slots 0-2 STILL unchanged
      CHECK(g.slotKind(3) == seedKind(r, "wire_heir", 3));
    }
    // Bad branch: same chain, but slot 3 stamps from the OTHER Daemon's seed —
    // proving the branch taken (not just the terminal stage) decides the newly-
    // unlocked slot. The two branch seeds must differ for that to be observable,
    // so assert it: if a balance pass ever flattens them this scenario has gone
    // vacuous and should say so rather than pass silently.
    CHECK(seedKind(r, "bruinforce", 3) != seedKind(r, "berserkernel", 3));
    { Game g{StartMode::Hatched, "malbear"};
      g.model().setCareMistakes(kCareGoodMax + 1);      // 3-4 -> Bad branch
      CHECK(g.model().careBranch() == CareBranch::Bad);
      const Game::SlotKind slot2 = g.slotKind(2);
      CHECK(slot2 == seedKind(r, "malbear", 2));        // stamped on install
      g.debugTriggerEvolution();
      uint32_t t = 0; advanceToReveal(g, t);
      g.onButton(press(Button::B));
      CHECK(g.pet() && std::strcmp(g.pet()->id, "berserkernel") == 0);
      CHECK(g.slotKind(2) == slot2);                    // slot 2 unchanged by the branch pick
      CHECK(g.slotKind(3) == seedKind(r, "berserkernel", 3));
    }
}

// Save v24: slotKinds round-trips verbatim; a pre-v24 blob (no tail) loads with
// every slot Unset, and Game::applySave deterministically re-stamps them from the
// loaded pet AND unequips any move that no longer matches its slot's kind.
static void test_save_v24_slot_kinds_roundtrip_and_migration() {
    // Raw SaveData round-trip (mirrors the v22/v23 default tests' shape).
    SaveData a; std::strcpy(a.activeId, "malbear"); a.generation = 1;
    a.slotKinds = {1, 1, 2, 0};   // Attack, Attack, Defend, Unset (SlotKind values)
    auto blobFull = serializeSave(a);
    SaveData full;
    CHECK(deserializeSave(blobFull, full));
    CHECK(full.hasSlotKindData);
    CHECK(full.slotKinds.size() == 4);
    CHECK(full.slotKinds[0] == 1 && full.slotKinds[1] == 1 &&
          full.slotKinds[2] == 2 && full.slotKinds[3] == 0);

    // Drop the v24 tail (a u16 count of 4 + 4 bytes = 6 bytes) and stamp the
    // version word down to 23 — a pre-v24 blob must still deserialize cleanly.
    auto blob = forgeLegacyNetworkBytes(a, 23);
    blob.resize(blob.size() - 6);
    blob[4] = 23; blob[5] = 0;
    SaveData out;
    CHECK(deserializeSave(blob, out));
    CHECK(!out.hasSlotKindData);
    CHECK(out.slotKinds.empty());

    // Game-level migration: load that pre-v24 blob into a Game. malbear's slots
    // deterministically re-stamp from its own CreatureDef::slotKinds seed
    // (Attack, Attack, Defend) via stampSlotKinds — not a guess, not left Unset.
    MemSaveStore store; store.save(blob);
    Game g(StartMode::Hatched, "paypup", &store);   // hatchedCreature ignored: store wins
    CHECK(g.pet() && std::strcmp(g.pet()->id, "malbear") == 0);
    CHECK(g.slotKind(0) == Game::SlotKind::Attack);
    CHECK(g.slotKind(1) == Game::SlotKind::Attack);
    CHECK(g.slotKind(2) == Game::SlotKind::Defend);

    // Invalid-equip-on-load cleanup: craft a pre-v24 blob whose v2 equippedMoves
    // puts a Defend move (checksum_guard) into slot 1, which malbear stamps
    // Attack — applySave must unequip it rather than grandfather the mismatch.
    SaveData bad; std::strcpy(bad.activeId, "malbear"); bad.generation = 1;
    bad.ownedMoves.push_back(SaveId{"checksum_guard"});
    bad.equippedMoves.push_back(SaveId{});                  // slot 0 — left empty
    bad.equippedMoves.push_back(SaveId{"checksum_guard"});  // slot 1 — the invalid one
    auto badBlob = forgeLegacyNetworkBytes(bad, 23);
    // `bad.slotKinds` was never set, so the v24 tail here is just the empty
    // length prefix (2 bytes) — unlike `a` above, which carried 4 entries.
    badBlob.resize(badBlob.size() - 2);   // drop the v24 tail
    badBlob[4] = 23; badBlob[5] = 0;      // pre-v24
    MemSaveStore badStore; badStore.save(badBlob);
    Game bg(StartMode::Hatched, "paypup", &badStore);
    CHECK(bg.pet() && std::strcmp(bg.pet()->id, "malbear") == 0);
    CHECK(bg.slotKind(1) == Game::SlotKind::Attack);
    // The mismatched equip was dropped, not grandfathered.
    CHECK(bg.moveLoadout().equipped(1) == nullptr);

    // Live round-trip: a Game raised to malbear, stamped kinds intact after an
    // ordinary autosave + reload (not a migration this time).
    MemSaveStore rtStore;
    {
        Game rt(StartMode::Hatched, "malbear", &rtStore);
        CHECK(rt.slotKind(2) == Game::SlotKind::Defend);
        rt.tick(kSaveAutosaveMs + kHeartbeatMs);            // autosave
    }
    Game rt2(StartMode::Hatched, "paypup", &rtStore);       // hatchedCreature ignored: store wins
    CHECK(rt2.pet() && std::strcmp(rt2.pet()->id, "malbear") == 0);
    CHECK(rt2.slotKind(0) == Game::SlotKind::Attack);
    CHECK(rt2.slotKind(1) == Game::SlotKind::Attack);
    CHECK(rt2.slotKind(2) == Game::SlotKind::Defend);
}

// The A+C override is live in combat (the one pet-side place): the chord opens the
// picker (without spending); committing spends the once-per-battle pip.
static void test_combat_override_in_game() {
    Game g{StartMode::Hatched, "paypup"};
    g.debugStartCombat(/*live=*/false);
    CHECK(g.nav() == Game::Nav::Combat && g.combat().overrideReady());
    g.onButton({Button::A, true, true});           // A+C chord -> open picker
    CHECK(g.combat().overrideOpen() && g.combat().overrideReady());  // open != spend
    g.onButton(press(Button::B));                  // commit -> spends
    CHECK(!g.combat().overrideOpen() && !g.combat().overrideReady());
    // Outside combat the chord stays an inert no-op (early-out not bypassed).
    Game idle{StartMode::Hatched, "paypup"};
    idle.onButton({Button::A, true, true});
    CHECK(idle.nav() == Game::Nav::Idle);
}

// Live loss applies +Fragmentation + COMBAT_LOST; a safe (Sim) loss does neither
// the live-vs-safe stakes contract on the shared engine.
static void test_combat_stakes_live_vs_safe() {
    { Game g{StartMode::Hatched, "paypup"};
      const int f0 = g.model().fragmentation();
      g.debugStartCombat(/*live=*/true, /*lethal=*/true);
      uint32_t t = 0;
      for (int i = 0; i < 50 && g.combat().outcome() == Combat::Outcome::Ongoing; ++i)
          g.tick(t += kHeartbeatMs);
      CHECK(g.combat().outcome() == Combat::Outcome::Lose);
      g.onButton(press(Button::B));
      CHECK(g.model().fragmentation() > f0);                 // live -> +Frag
      CHECK(g.log().size() >= 1 && g.log().at(0).type == LogEventType::CombatLost); }

    { Game g{StartMode::Hatched, "paypup"};
      const int f0 = g.model().fragmentation();
      g.debugStartCombat(/*live=*/false, /*lethal=*/true);
      uint32_t t = 0;
      for (int i = 0; i < 50 && g.combat().outcome() == Combat::Outcome::Ongoing; ++i)
          g.tick(t += kHeartbeatMs);
      CHECK(g.combat().outcome() == Combat::Outcome::Lose);
      g.onButton(press(Button::B));
      CHECK(g.model().fragmentation() == f0);                // safe -> no Frag
      CHECK(g.log().size() == 0); }                          // not logged
}

// The combat screen + the override overlay read in grayscale (dual-coding gate).
static void test_combat_screen_grayscale() {
    Game g{StartMode::Hatched, "paypup"};
    g.debugStartCombat(/*live=*/false);
    Framebuffer fb(kActiveW, kActiveH);
    g.render(fb);
    CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));
    g.onButton({Button::A, true, true});           // open override picker
    g.render(fb);
    CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));
}

// The move loadout survives a reboot (save v2): equip a different move via the
// TRAIN picker, autosave, and confirm a fresh Game over the store restores it.
// Uses malbear (Script) rather than Paypup (Process): a pet now owns its whole
// line's kit from hatch, but Paypup's Process stage only has ONE unlocked Attack
// row (payload_drop) to switch between — Script unlocks a second (double_extortion).
static void test_move_loadout_persist() {
    MemSaveStore store;
    {
        Game g(StartMode::Hatched, "malbear", &store);
        CHECK(std::strcmp(g.moveLoadout().equipped(0), "payload_drop") == 0);
        enterSubmenuId(g, SubmenuId::Train);
        g.onButton(press(Button::B));              // open slot 0 picker
        g.onButton(press(Button::A));              // unequip -> payload_drop
        g.onButton(press(Button::A));              // -> double_extortion (a different move)
        g.onButton(press(Button::B));              // drill into its detail page
        g.onButton(press(Button::B));              // equip -> hands back the overwrite confirm
        g.onButton(press(Button::A));              // Cancel -> Confirm
        g.onButton(press(Button::B));              // commit
        CHECK(std::strcmp(g.moveLoadout().equipped(0), "double_extortion") == 0);
        g.tick(kSaveAutosaveMs + kHeartbeatMs);    // autosave
    }
    Game g2(StartMode::Hatched, "malbear", &store);
    CHECK(g2.nav() == Game::Nav::Idle);
    CHECK(std::strcmp(g2.moveLoadout().equipped(0), "double_extortion") == 0);   // restored
}

// --- Evolution branching (Good/Bad) + Critical System Failure ------------------

// The Script->Daemon hop BRANCHES on the care budget: 0-2
// mistakes reach the Good successor, 3-4 reach the Bad (glass-cannon) successor.
// Both branches are reachable and the selection is deterministic from care.
static void test_evolution_branch_selection() {
    { Game g{StartMode::Hatched, "malbear"};                 // default care = Good
      CHECK(g.model().careBranch() == CareBranch::Good);
      g.debugTriggerEvolution();
      uint32_t t = 0; advanceToReveal(g, t);
      g.onButton(press(Button::B));
      CHECK(g.pet() && std::strcmp(g.pet()->id, "bruinforce") == 0); }

    { Game g{StartMode::Hatched, "malbear"};
      g.model().setCareMistakes(kCareGoodMax + 1);           // 3 -> Bad branch
      CHECK(g.model().careBranch() == CareBranch::Bad);
      g.debugTriggerEvolution();
      uint32_t t = 0; advanceToReveal(g, t);
      g.onButton(press(Button::B));
      CHECK(g.pet() && std::strcmp(g.pet()->id, "berserkernel") == 0); }
}

// The branch changes mechanics: the Bad-branch leans aggressive (higher
// attack power) and takes MORE Frag on a loss; the Good-branch is the durable
// inverse. Both are measurable in an actual fight, not just the multiplier fields.
static void test_evolution_branch_mechanics() {
    ContentRegistry r = ContentRegistry::embedded();
    MoveLoadout mv = MoveLoadout::starting();
    Loadout md = Loadout::starting();
    const CreatureDef* good = r.creature("bruinforce");
    const CreatureDef* bad = r.creature("berserkernel");
    CHECK(good && bad);
    Combatant cg = makePlayerCombatant(r, *good, mv, md);
    Combatant cb = makePlayerCombatant(r, *bad, mv, md);
    // Branch lean at the source: Good leans below neutral, Bad above.
    CHECK(good->powerMultPct < 100 && bad->powerMultPct > 100);
    // The per-stage offensive scale multiplies both equally, so it lifts the
    // absolute values (Daemon ×230) but PRESERVES the branch ordering in the built
    // combatants — Bad still out-hits Good.
    CHECK(cb.powerMultPct > cg.powerMultPct);
    CHECK(cb.fragMultPct > cg.fragMultPct);

    // Same enemy + seed: the Bad-branch deals strictly more damage over the run.
    CombatEnemy tank{"Tank", "SPR_PET_CACHEMUTT", 3, 500, 1, {"quick_jab"}};
    Combat combGood, combBad;
    combGood.begin(cg, makeEnemyCombatant(r, tank), Combat::Stakes::Safe, 4242);
    combBad.begin(cb, makeEnemyCombatant(r, tank), Combat::Stakes::Safe, 4242);
    for (int i = 0; i < 8; ++i) { combGood.step(); combBad.step(); }
    CHECK(combBad.enemy().health < combGood.enemy().health);  // glass cannon hits harder

    // And a live loss on the Bad branch costs MORE Fragmentation.
    int fragGood = 0, fragBad = 0;
    { Game g{StartMode::Hatched, "bruinforce"};
      g.debugStartCombat(/*live=*/true, /*lethal=*/true);
      uint32_t t = 0;
      for (int i = 0; i < 50 && g.combat().outcome() == Combat::Outcome::Ongoing; ++i)
          g.tick(t += kHeartbeatMs);
      g.onButton(press(Button::B)); fragGood = g.model().fragmentation(); }
    { Game g{StartMode::Hatched, "berserkernel"};
      g.debugStartCombat(/*live=*/true, /*lethal=*/true);
      uint32_t t = 0;
      for (int i = 0; i < 50 && g.combat().outcome() == Combat::Outcome::Ongoing; ++i)
          g.tick(t += kHeartbeatMs);
      g.onButton(press(Button::B)); fragBad = g.model().fragmentation(); }
    CHECK(fragBad > fragGood);
}

// Combat loss NEVER kills (the invariant): the lethal live loss above leaves the
// pet alive (Frag rose, but it's still the active pet, not archived).
static void test_combat_loss_never_kills() {
    Game g{StartMode::Hatched, "paypup"};
    g.debugStartCombat(/*live=*/true, /*lethal=*/true);
    uint32_t t = 0;
    for (int i = 0; i < 50 && g.combat().outcome() == Combat::Outcome::Ongoing; ++i)
        g.tick(t += kHeartbeatMs);
    CHECK(g.combat().outcome() == Combat::Outcome::Lose);
    const char* petId = g.pet() ? g.pet()->id : "";
    g.onButton(press(Button::B));                              // dismiss the result
    CHECK(g.pet() != nullptr && std::strcmp(g.pet()->id, petId) == 0);  // same pet, alive
    CHECK(g.nav() != Game::Nav::ModalCSF);                    // combat never triggers CSF
    CHECK(g.recordCount() == 0);                              // not archived
}

// Critical System Failure — the ONLY death path. The 5/5 dying state, once
// its ageing window expires, fires the terminal modal; B (gated behind the crash
// hold; C disabled) archives the pet as a [CORRUPTED] record + a new-egg Hatch.
static void test_csf_fires_and_archives() {
    Game g{StartMode::Hatched, "bruinforce"};   // terminus (no evolution interference)
    g.model().setCareMistakes(kCareDying);            // 5/5 dying
    uint32_t t = 0;
    g.tick(t += 1000);                                // arms the ageing window
    CHECK(g.nav() == Game::Nav::Idle);                // grace not elapsed yet
    g.tick(t += kCsfDyingGraceMs);                    // window expires -> CSF
    CHECK(g.nav() == Game::Nav::ModalCSF);
    CHECK(g.recordCount() == 0);                      // not archived until acknowledged
    g.onButton(press(Button::C));                     // C disabled
    CHECK(g.nav() == Game::Nav::ModalCSF);
    g.onButton(press(Button::B));                     // B gated until the crash holds
    CHECK(g.nav() == Game::Nav::ModalCSF);
    for (int i = 0; i < kCsfHoldBeats; ++i) g.tick(t += kHeartbeatMs);
    g.onButton(press(Button::B));                     // acknowledge -> line-select
    pickFirstEggLine(g);                              // pick Ransomware -> new egg
    CHECK(g.nav() == Game::Nav::Idle);                // -> a new egg laid at idle
    CHECK(g.pet() && g.inEggPhase());                 // dead pet replaced by a fresh egg
    CHECK(g.recordCount() == 1);
    CHECK(static_cast<RecordStatus>(g.records()[0].status) == RecordStatus::Corrupted);
}

// Recoverable up to the deadline: dropping below 5/5 (Backup Drive / Yubi-Cookie)
// before the window expires disarms the ageing timer — no death.
static void test_csf_recovery_disarms() {
    Game g{StartMode::Hatched, "bruinforce"};
    g.model().setCareMistakes(kCareDying);
    uint32_t t = 0;
    g.tick(t += 1000);                                // arm
    g.model().setCareMistakes(kCareDying - 1);        // pulled back below 5
    g.tick(t += kCsfDyingGraceMs);                    // window would have expired
    CHECK(g.nav() == Game::Nav::Idle);                // survived
    CHECK(g.recordCount() == 0);
}

// The [CORRUPTED] record + vacated active survive a reboot (save v3): the reboot
// resumes at the Hatch with the record intact.
static void test_csf_record_persists() {
    MemSaveStore store;
    {
        Game g(StartMode::Hatched, "bruinforce", &store);
        g.model().setCareMistakes(kCareDying);
        uint32_t t = 0;
        g.tick(t += 1000);
        g.tick(t += kCsfDyingGraceMs);
        for (int i = 0; i < kCsfHoldBeats; ++i) g.tick(t += kHeartbeatMs);
        g.onButton(press(Button::B));                 // acknowledge -> line-select
        pickFirstEggLine(g);                          // pick Ransomware -> lays + persists the egg
        CHECK(g.recordCount() == 1);                  // persisted immediately
    }
    Game g2(StartMode::Hatched, "paypup", &store);    // reboot (store wins over the seam)
    CHECK(g2.nav() == Game::Nav::Idle);               // the post-CSF egg resumes at idle
    CHECK(g2.pet() && g2.inEggPhase());               // mid-incubation egg survived the reboot
    CHECK(g2.recordCount() == 1);
    CHECK(static_cast<RecordStatus>(g2.records()[0].status) == RecordStatus::Corrupted);
}

// The 5/5 window is the ONE timed window that survives a reboot (save v42). Every
// other one resets on boot by the rule in core/net/audit_capture.h, but those are
// penalties — refunding THIS one makes the game's only death path opt-out for anyone
// who notices the pulse and power-cycles. What persists is time AWAKE at 5/5, so a
// device that is off neither kills the pet nor buys it a reprieve.
static void test_csf_window_survives_reboot() {
    MemSaveStore store;
    {
        Game g(StartMode::Hatched, "bruinforce", &store);
        g.model().setCareMistakes(kCareDying);
        uint32_t t = 0;
        g.tick(t += 1000);                            // arm
        // Burn the window down to a few seconds, then tick once more so the debounced
        // autosave writes the figure rather than leaving it to the slow periodic one.
        g.tick(t += kCsfDyingGraceMs - 4 * kSaveDebounceMs);
        g.tick(t += kSaveDebounceMs);
        CHECK(g.nav() == Game::Nav::Idle);            // alive, with the window nearly gone
    }
    Game g2(StartMode::Hatched, "bruinforce", &store);   // power cycle
    CHECK(g2.model().careMistakes() == kCareDying);      // still 5/5 — the state persisted
    uint32_t t = 0;
    g2.tick(t += kHeartbeatMs);                          // re-anchor against the fresh clock
    CHECK(g2.nav() == Game::Nav::Idle);                  // a beat is not the whole remainder
    // Only the few seconds that were actually left. Had the reboot refunded the window,
    // this would be ~6s into a fresh 120s and the pet would still be idle.
    g2.tick(t += 3 * kSaveDebounceMs);
    CHECK(g2.nav() == Game::Nav::ModalCSF);              // the burned time was NOT refunded
}

// The window is per-brush-with-death, not per-lifetime: a pet pulled back below 5/5
// gets the whole grace period again, and that reset survives a reboot too — otherwise
// a rescued pet would carry a shortened window for the rest of its life.
static void test_csf_recovery_clears_burned_window() {
    MemSaveStore store;
    {
        Game g(StartMode::Hatched, "bruinforce", &store);
        g.model().setCareMistakes(kCareDying);
        uint32_t t = 0;
        g.tick(t += 1000);
        g.tick(t += kCsfDyingGraceMs - 2 * kHeartbeatMs);   // nearly spent
        g.model().setCareMistakes(kCareDying - 1);          // rescued
        g.tick(t += kSaveDebounceMs);                       // disarm + zero, then persist
        g.model().setCareMistakes(kCareDying);              // straight back to 5/5
        g.tick(t += kSaveDebounceMs);
        CHECK(g.nav() == Game::Nav::Idle);
    }
    Game g2(StartMode::Hatched, "bruinforce", &store);
    uint32_t t = 0;
    g2.tick(t += kHeartbeatMs);                             // arm
    g2.tick(t += kCsfDyingGraceMs - 2 * kHeartbeatMs);      // a full window, less a beat
    CHECK(g2.nav() == Game::Nav::Idle);                     // survived: the clock restarted
    g2.tick(t += 2 * kHeartbeatMs);
    CHECK(g2.nav() == Game::Nav::ModalCSF);                 // and it is a REAL window
}

// Save v3 round-trips the ARCH records (and a v1/v2 blob migrates with none).
static void test_save_records_roundtrip() {
    SaveData d;
    std::strncpy(d.activeId, "paypup", kSaveIdCap - 1);
    SaveRecord rec;
    std::strncpy(rec.id, "berserkernel", kSaveIdCap - 1);
    rec.status = static_cast<uint8_t>(RecordStatus::Corrupted);
    rec.generation = 3;
    d.records.push_back(rec);
    SaveData out;
    CHECK(deserializeSave(serializeSave(d), out));
    CHECK(out.records.size() == 1);
    CHECK(std::strcmp(out.records[0].id, "berserkernel") == 0);
    CHECK(out.records[0].generation == 3);
    CHECK(static_cast<RecordStatus>(out.records[0].status) == RecordStatus::Corrupted);
}

// Save v4 round-trips Hacker Rank: the counter, the derived
// rank, and the dedup-pool bitmask (so a reboot doesn't re-count an
// already-seen network as new).
static void test_save_hacker_rank_roundtrip() {
    SaveData d;
    std::strncpy(d.activeId, "paypup", kSaveIdCap - 1);
    d.networksSeen = 5;
    d.hackerRank = 2;
    SaveData out;
    CHECK(deserializeSave(serializeSave(d), out));
    CHECK(out.networksSeen == 5);
    CHECK(out.hackerRank == 2);
}

// A v3 blob (no Hacker Rank tail) still loads: deserialize accepts it with
// networksSeen/hackerRank defaulted to 0 (a save that predates the system
// honestly starts at Script Kiddie / 0 networks, not a retroactively-invented
// rank). Target version 3 is below the v4 removal point, so no forge needed —
// see forgeLegacyNetworkBytes's comment for why versions >= 4 do need it.
static void test_save_v3_migration_defaults_hacker_rank() {
    SaveData a; std::strcpy(a.activeId, "paypup"); a.networksSeen = 5; a.hackerRank = 2;
    auto blob = serializeSave(a);
    CHECK(blob.size() > 8);
    blob.resize(blob.size() - 8);   // drop the v4 tail (i32 + i32 = 8 bytes)
    blob[4] = 3; blob[5] = 0;       // version word -> 3

    SaveData out;
    CHECK(deserializeSave(blob, out));
    CHECK(out.networksSeen == 0);
    CHECK(out.hackerRank == 0);
    CHECK(std::strcmp(out.activeId, "paypup") == 0);

    // Booting a Game over the v3 store starts fresh at rank 0 (graceful
    // forward-compat migration, same contract as the v1 move-loadout case).
    MemSaveStore store;
    store.save(blob);
    Game g(StartMode::FreshHatch, "paypup", &store);
    pickFirstEggLine(g);
    CHECK(g.nav() == Game::Nav::Idle && g.pet());
    CHECK(g.hackerRank() == 0);
    CHECK(g.networksSeen() == 0);
}

// The CSF modal reads in grayscale (dual-coding gate): title/message ink survives.
static void test_csf_grayscale() {
    Game g{StartMode::Hatched, "bruinforce"};
    g.model().setCareMistakes(kCareDying);
    uint32_t t = 0; g.tick(t += 1000); g.tick(t += kCsfDyingGraceMs);
    CHECK(g.nav() == Game::Nav::ModalCSF);
    Framebuffer fb(kActiveW, kActiveH);
    g.render(fb);
    CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));
}

// The corrupted record surfaces in ARCH as a greyed, read-only entry:
// it opens to a detail with no actions; A/B do nothing, C backs.
static void test_arch_record_readonly() {
    Game g{StartMode::Hatched, "bruinforce"};
    g.model().setCareMistakes(kCareDying);
    uint32_t t = 0; g.tick(t += 1000); g.tick(t += kCsfDyingGraceMs);
    for (int i = 0; i < kCsfHoldBeats; ++i) g.tick(t += kHeartbeatMs);
    g.onButton(press(Button::B));                     // -> line-select, 1 record
    pickFirstEggLine(g);                              // pick Ransomware -> a fresh egg (active pet)
    CHECK(g.pet() != nullptr && g.nav() == Game::Nav::Idle);
    CHECK(g.recordCount() == 1);

    enterSubmenuId(g, SubmenuId::Arch);
    CHECK(g.nav() == Game::Nav::Submenu);
    g.onButton(press(Button::A));                     // active row 0 -> record row
    g.onButton(press(Button::B));                     // open the record (read-only)
    CHECK(g.nav() == Game::Nav::Detail);
    Framebuffer fb(kActiveW, kActiveH);
    g.render(fb);
    CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));
    g.onButton(press(Button::A));                     // no actions on a record
    CHECK(g.nav() == Game::Nav::Detail);
    g.onButton(press(Button::C));                     // C backs to the list
    CHECK(g.nav() == Game::Nav::Submenu);
}

// ===========================================================================
// EXPL wild-encounter wiring — the Walk activity, the
// periodic step-roll, the Fight/Flee/Sinkhole encounter intro, and the shared
// combat core wired with LIVE stakes (replacing debugStartCombat as the real
// entry point; the dev hook stays for TRAIN/combat-core-only tests).
// ===========================================================================

// Arm explore-mode on the open starter sector: EXPL -> B drops back to the
// IDLE habitat with explore-mode running (there is no walk screen).
static void enterWalk(Game& g) {
    // Two-level EXPL nav: entering parks on the area-0 header at the TOP
    // level; B drills into that area, a second B arms its first open sub-area.
    enterSubmenuId(g, SubmenuId::Expl);
    g.onButton(press(Button::B));                    // drill into the focused area
    g.onButton(press(Button::B));                    // arm its first open sub-area
}

// Fire the next GUARANTEED explore step NOW via the A+C control chord's Network Ping
// the chord opens the control overlay, A fires doExploreStep. Only valid
// from the idle habitat while exploring.
static void pingExplore(Game& g) {
    g.onButton(chordAC());                        // A+C -> control overlay
    g.onButton(press(Button::A));                 // A -> Network Ping (next event)
}

// Steps explore-mode until a guaranteed step types a WILD encounter. In the
// hands-off auto mode a wild encounter AUTO-STARTS the fight (no Fight/Flee intro,
// ) — so this lands the game IN live wild combat, bounded-searching past any other
// typed event (Wi-Fi / shop) along the way. (An awakened-guardian Wi-Fi outcome also
// enters live wild combat, — either is a valid stop for callers.)
static void walkToEncounter(Game& g) {
    enterWalk(g);
    for (int i = 0; i < 400 && g.nav() != Game::Nav::Combat; ++i) {
        switch (g.nav()) {
            case Game::Nav::Idle: pingExplore(g); break;
            case Game::Nav::Wifi: g.onButton(press(Button::B)); break;  // may enter combat
            case Game::Nav::Shop: g.onButton(press(Button::C)); break;
            case Game::Nav::ModShop: g.onButton(press(Button::C)); break;
            default: g.onButton(press(Button::B)); break;
        }
    }
}

// An armed Backup Drive save rides into a REAL fight (Game::buildPlayerCombatant, not
// the raw makePlayerCombatant the combat-model tests use) — the wiring that matters,
// since the Sim Battle it's stripped from is the only fight that doesn't carry it.
static void test_backup_drive_save_armed_into_wild_combat() {
    Game g{StartMode::Hatched, "paypup"};
    g.debugUseItem("backup_drive");
    CHECK(g.backupShieldArmed());
    // Consuming the last Backup Drive drops useItem() into Nav::Submenu (the ITEMS
    // "item left the list" case) — back out to the carousel before the EXPL walk,
    // which assumes it's starting from the carousel/idle layer.
    g.onButton(press(Button::C));
    walkToEncounter(g);
    CHECK(g.nav() == Game::Nav::Combat);
    CHECK(g.combat().player().itemShield);
}

// Auto Backup (Rig Shop g): the purchased upgrade arms the death-save for free the
// moment explore-mode starts — and never reaches into the Vault for an actual drive.
static void test_rig_auto_backup_arms_save_on_explore() {
    Game g{StartMode::Hatched, "paypup"};
    const int drives = g.inventory().count("backup_drive");
    enterWalk(g);
    CHECK(!g.backupShieldArmed());            // nothing free without the upgrade
    g.debugSetBits(kRigAutoBackupCost);
    g.debugBuyAutoBackup();
    CHECK(!g.backupShieldArmed());            // buying alone arms nothing...
    enterWalk(g);                             // ...arming a sub-area does
    CHECK(g.backupShieldArmed());
    CHECK(g.inventory().count("backup_drive") == drives);   // and it cost no item
}

// Continuous Auto-Backup (Rig Shop h) re-arms mid-run: every resolved explore event
// puts a fresh save up, so one purchase covers a whole walk rather than its first fight.
static void test_rig_continuous_backup_rearms_mid_run() {
    Game g{StartMode::Hatched, "paypup"};
    const int drives = g.inventory().count("backup_drive");
    enterWalk(g);
    g.debugReturnToExplore();                 // a resolved event hands back to the habitat
    CHECK(!g.backupShieldArmed());            // ...which arms nothing on its own
    g.debugSetBits(kRigContinuousBackupCost);
    g.debugBuyContinuousBackup();
    g.debugReturnToExplore();
    CHECK(g.backupShieldArmed());             // now the next event re-arms it, free
    CHECK(g.inventory().count("backup_drive") == drives);
}

// Arming explore-mode drops back to the IDLE habitat with the mode running:
// exploreActive is true, the sector is armed, the streak starts at 0, and the badge
// renders. Summoning the carousel pauses stepping; C in EXPL doesn't cancel the mode.
static void test_explore_arm_returns_to_idle() {
    Game g{StartMode::Hatched};
    enterWalk(g);
    CHECK(g.nav() == Game::Nav::Idle);
    CHECK(g.exploreActive());
    CHECK(g.exploreSector() == 0);
    CHECK(g.exploreStreak() == 0);
    Framebuffer fb(kActiveW, kActiveH);
    g.render(fb);
    CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));   // the badge draws over the habitat
    // The A+C control chord opens the explore-control overlay.
    g.onButton(chordAC());
    CHECK(g.nav() == Game::Nav::ExploreControl);
    g.onButton(press(Button::C));                       // C -> Stop explore (cancel)
    CHECK(g.nav() == Game::Nav::Idle);
    CHECK(!g.exploreActive());
    CHECK(g.exploreStreak() == 0);
}

// A guaranteed explore step types a wild encounter among its outcomes. In
// the hands-off auto mode there's no human at a Fight/Flee intro, so the encounter
// AUTO-STARTS the fight — live wild combat, no decision screen.
static void test_walk_event_roll_reaches_encounter() {
    Game g{StartMode::Hatched};
    walkToEncounter(g);
    CHECK(g.nav() == Game::Nav::Combat);
    CHECK(g.combat().stakes() == Combat::Stakes::Live);
    Framebuffer fb(kActiveW, kActiveH);
    g.render(fb);
    CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));
}

// re-farming an already-CLEARED sub-area gives diminishing non-Bits
// rewards. Three contracts:
//   (1) the pure decay refarmDropScalePct (100 → floor, monotonic, guarded);
//   (2) a wild win in a CLEARED sub-area advances that sub's re-farm count while Bits
//       still flow;
//   (3) a win in an UNCLEARED sub-area leaves the count at 0 (full drops).
static void winAutoExploreFight(Game& g) {   // resolve + auto-dismiss the wild fight
    uint32_t t = 0;
    for (int i = 0; i < 400 && g.combat().outcome() == Combat::Outcome::Ongoing; ++i)
        g.tick(t += kHeartbeatMs);
    CHECK(g.combat().outcome() == Combat::Outcome::Win);
    for (int i = 0; i < 40 && g.nav() == Game::Nav::Combat; ++i)  // auto-dismiss hold
        g.tick(t += kHeartbeatMs);
}
static void test_refarm_diminishing_rewards() {
    // (1) The pure decay.
    CHECK(Game::refarmDropScalePct(0) == 100);
    CHECK(Game::refarmDropScalePct(1) == 100 - kRefarmDropDecayStep);
    CHECK(Game::refarmDropScalePct(2) < Game::refarmDropScalePct(1));   // monotonic down
    CHECK(Game::refarmDropScalePct(10000) == kRefarmDropFloorPct);      // floored
    CHECK(Game::refarmDropScalePct(-3) == 100);                         // guarded
    // (2) A cleared sub-area accrues a re-farm win ONLY once Bandwidth is spent:
    //     with Bandwidth drained to 0 the decay curve advances the count; Bits still flow.
    {
        Game g{StartMode::Hatched};
        walkToEncounter(g);
        const int S = g.exploreSector(), U = g.exploreSub();
        g.debugSetSubCleared(S, U, true);          // this sub is a re-farm ground now
        g.debugSetBandwidth(0);                    // ...and Bandwidth is spent (decay path)
        CHECK(g.debugSubRefarmCount(S, U) == 0);
        const int bits0 = g.bits();
        winAutoExploreFight(g);
        CHECK(g.debugSubRefarmCount(S, U) == 1);   // the re-farm win advanced the count
        CHECK(g.bits() > bits0);                    // Bits never decay
    }
    // (3) An uncleared sub-area never accrues (it's a first clear, not a re-farm).
    {
        Game g{StartMode::Hatched};
        walkToEncounter(g);
        const int S = g.exploreSector(), U = g.exploreSub();
        winAutoExploreFight(g);
        CHECK(g.debugSubRefarmCount(S, U) == 0);
    }
    // (4) — the curve is PER-PET, not per-device: a farmed-out count resets
    //     when a new egg is laid, so a fresh pet finds the area undepleted again.
    {
        Game g{StartMode::Hatched};
        walkToEncounter(g);
        const int S = g.exploreSector(), U = g.exploreSub();
        g.debugSetSubCleared(S, U, true);
        g.debugSetBandwidth(0);                        // decay path
        winAutoExploreFight(g);
        CHECK(g.debugSubRefarmCount(S, U) == 1);       // this pet depleted it a notch
        g.resetToHatch();                              // a new egg = a new pet
        pickFirstEggLine(g);                           // reset re-enters line-select
        CHECK(g.debugSubRefarmCount(S, U) == 0);       // ...starts the area fresh
    }
}

// (D2) — Bandwidth is a per-fight FRAGMENTATION SHIELD:
// ANY resolved exploration fight (first-clear, re-farm, or DeepWeb) spends 1 charge to
// skip the corruption tax; when the pool is dry the tax bites. Contracts:
//   (1) a cleared-sub farm win SPENDS 1 charge, keeps loot full, and FREEZES the
// decay count (while a charge covered the fight);
// (2) with Bandwidth at 0, the decay curve applies + the count advances;
//   (3) Bandwidth REGENERATES over real elapsed time on the heartbeat;
//   (4) a FIRST-CLEAR (uncleared-sub) win now ALSO spends a charge (the shield covers
//       every fight, not just farming);
//   (5) a DeepWeb Dive win ALSO spends a charge (it's prime farming — protect the pet).
static void test_bandwidth_farming_resource() {
    // (1) A charge is spent, loot stays full, the decay count is frozen.
    {
        Game g{StartMode::Hatched};
        walkToEncounter(g);
        const int S = g.exploreSector(), U = g.exploreSub();
        g.debugSetSubCleared(S, U, true);
        const int bw0 = g.bandwidth();
        CHECK(bw0 == kBandwidthMax);                   // fresh pool
        winAutoExploreFight(g);
        CHECK(g.bandwidth() == bw0 - 1);               // one farm win = one charge spent
        CHECK(g.debugSubRefarmCount(S, U) == 0);       // ...and the decay count is frozen
    }
    // (2) Once depleted, the decay resumes (count advances, Bandwidth stays 0).
    {
        Game g{StartMode::Hatched};
        walkToEncounter(g);
        const int S = g.exploreSector(), U = g.exploreSub();
        g.debugSetSubCleared(S, U, true);
        g.debugSetBandwidth(0);
        winAutoExploreFight(g);
        CHECK(g.bandwidth() == 0);                     // stays empty
        CHECK(g.debugSubRefarmCount(S, U) == 1);       // decay path advances the count
    }
    // (3) Regeneration over real time: a depleted pool trickles back on the heartbeat.
    {
        Game g{StartMode::Hatched};
        g.debugSetBandwidth(0);
        // Advance well past kBandwidthRegenMinutesPerPoint so at least one point returns.
        const uint32_t ms = (kBandwidthRegenMinutesPerPoint + 1) * 60u * 1000u;
        g.tick(ms);
        CHECK(g.bandwidth() >= 1);
        CHECK(g.bandwidth() <= kBandwidthMax);         // never over the cap
    }
    // (4) A first-clear (UNCLEARED sub) win now ALSO spends a charge — the shield covers
    //     every fight — while leaving the re-farm count untouched (it's not a re-farm).
    {
        Game g{StartMode::Hatched};
        walkToEncounter(g);
        const int S = g.exploreSector(), U = g.exploreSub();
        const int bw0 = g.bandwidth();
        winAutoExploreFight(g);                        // sub is NOT cleared → shield spend
        CHECK(g.bandwidth() == bw0 - 1);
        CHECK(g.debugSubRefarmCount(S, U) == 0);       // ...not a re-farm, count stays 0
    }
    // (5) A DeepWeb Dive win ALSO spends a charge (prime farming — protect the pet).
    {
        Game g{StartMode::Hatched, "bruinforce"};
        for (int a = 0; a < kExplSectors; ++a) g.debugSetSectorCleared(a, true);
        g.debugStartDeepWebDive();
        CHECK(g.inDeepWebDive());
        const int bw0 = g.bandwidth();
        uint32_t t = 0; int guard = 0;
        while (g.nav() != Game::Nav::Combat && guard++ < 400) {
            switch (g.nav()) {
                case Game::Nav::Idle: if (g.inDeepWebDive()) pingExplore(g); else guard = 400; break;
                case Game::Nav::Wifi: g.onButton(press(Button::B)); break;
                case Game::Nav::Shop: g.onButton(press(Button::C)); break;
                case Game::Nav::ModShop: g.onButton(press(Button::C)); break;
                default: g.onButton(press(Button::C)); break;
            }
            (void)t;
        }
        CHECK(g.nav() == Game::Nav::Combat);
        winAutoExploreFight(g);
        CHECK(g.bandwidth() == bw0 - 1);               // DeepWeb spends the shield too
    }
    // (6) Corruption shield: ANY won exploration fight with Bandwidth left never rolls the
    // fragmentation (corruption) tax — the spent charge keeps the pet clean.
    //     So a bigger pool makes exploring safer, not just farming. (First-clear here.)
    {
        Game g{StartMode::Hatched};
        walkToEncounter(g);
        CHECK(g.bandwidth() > 0);
        const int frag0 = g.model().fragmentation();
        winAutoExploreFight(g);                        // first-clear win, Bandwidth shielded
        CHECK(g.model().fragmentation() == frag0);     // ...no corruption accrued
    }
    // (7) With Bandwidth exhausted, the shield is gone — the tax can now bite
    //     (a first-clear win at 0 charges leaves the pet exposed; fragmentation may rise).
    {
        Game g{StartMode::Hatched};
        walkToEncounter(g);
        g.debugSetBandwidth(0);
        winAutoExploreFight(g);
        CHECK(g.bandwidth() == 0);                      // nothing to spend, stays empty
    }
}

// A+C flips PET ↔ HACKER at the top level, reusing the carousel/submenu
// contracts. The flip only fires from Idle/Cursor; on the HACKER face B enters a live
// slot (PROFILE/SHOP) but is inert on an inaccessible one; A+C always returns.
static void test_hacker_face_toggle() {
    Game g{StartMode::Hatched};
    CHECK(g.face() == Game::Face::Pet && g.nav() == Game::Nav::Idle);

    // A+C from idle → the Hacker face home (still Nav::Idle, but the hacker face now).
    g.onButton({Button::A, true, true});
    CHECK(g.face() == Game::Face::Hacker && g.nav() == Game::Nav::Idle);

    // toggleFace() already lands the cursor on the first ACCESSIBLE slot (PROFILE);
    // walk (by id, not index — order is a display detail) to PROFILE and B enters it.
    g.onButton(press(Button::A));
    while (hackerCarouselSlots()[g.cursor()].id != HackerSlotId::Profile)
        g.onButton(press(Button::A));
    g.onButton(press(Button::B));
    CHECK(g.nav() == Game::Nav::Submenu);               // PROFILE opened
    g.onButton(press(Button::C));
    CHECK(g.nav() == Game::Nav::Cursor);                // C backs to the hacker carousel

    // B on an inaccessible slot (SCAN) is inert — stays on the carousel.
    while (hackerCarouselSlots()[g.cursor()].id != HackerSlotId::Scan)
        g.onButton(press(Button::A));
    CHECK(!hackerCarouselSlots()[g.cursor()].accessible);
    g.onButton(press(Button::B));
    CHECK(g.nav() == Game::Nav::Cursor);                // no entry

    // A+C from the hacker carousel returns to the pet face (idle habitat).
    g.onButton({Button::A, true, true});
    CHECK(g.face() == Game::Face::Pet && g.nav() == Game::Nav::Idle);
}

// a — the Hacker SHOP "Increase Bandwidth" upgrade: a buy spends Bits, raises
// the farming-pool CAP (bandwidthMax) by one step, persists (bwUpgradeCount), and the
// price climbs per purchase. A buy is inert when the wallet can't afford it.
static void test_hacker_shop_bandwidth_upgrade() {
    Game g{StartMode::Hatched};
    const int cap0 = g.bandwidthMax();
    const int cost0 = g.bandwidthUpgradeCost();
    CHECK(cost0 == kBandwidthUpgradeBaseCost);          // first purchase = base price
    g.debugSetBits(cost0 + 10);

    g.debugBuyBandwidthUpgrade();
    CHECK(g.bwUpgradeCount() == 1);
    CHECK(g.bandwidthMax() == cap0 + kBandwidthUpgradeStep);
    CHECK(g.bits() == 10);                              // Bits deducted
    CHECK(g.bandwidthUpgradeCost() == cost0 + kBandwidthUpgradeCostStep);  // price climbs

    // Now broke → a second buy is inert (no cap change, no Bits change).
    const int cap1 = g.bandwidthMax();
    g.debugBuyBandwidthUpgrade();
    CHECK(g.bwUpgradeCount() == 1 && g.bandwidthMax() == cap1 && g.bits() == 10);
}

// the shared rigUpgradeCost curve engine (tunables.h). Pure-function
// check of all six named curves at their defining boundary points, so the balance
// knobs (start price) can be retuned without re-deriving the shape by hand.
static void test_rig_cost_curve_formulas() {
    // Fixed: Cost(n) = start, regardless of n (one-time-unlock rows).
    CHECK(rigUpgradeCost(1024, 0, RigCostCurve::kFixed) == 1024);
    CHECK(rigUpgradeCost(1024, 5, RigCostCurve::kFixed) == 1024);

    // Linear: Cost(n) = start + step*n.
    CHECK(rigUpgradeCost(128, 0, RigCostCurve::kLinear, 64) == 128);
    CHECK(rigUpgradeCost(128, 1, RigCostCurve::kLinear, 64) == 192);
    CHECK(rigUpgradeCost(128, 3, RigCostCurve::kLinear, 64) == 320);

    // Doubling: Cost(n) = start * 2^n.
    CHECK(rigUpgradeCost(512, 0, RigCostCurve::kDoubling) == 512);
    CHECK(rigUpgradeCost(512, 1, RigCostCurve::kDoubling) == 1024);
    CHECK(rigUpgradeCost(512, 4, RigCostCurve::kDoubling) == 8192);

    // Halving: Cost(n) = start / 2^n, floored at 1.
    CHECK(rigUpgradeCost(2048, 0, RigCostCurve::kHalving) == 2048);
    CHECK(rigUpgradeCost(2048, 1, RigCostCurve::kHalving) == 1024);
    CHECK(rigUpgradeCost(2048, 2, RigCostCurve::kHalving) == 512);

    // LogStep: Cost(n) = start * 2^floor(log2(n)), n clamped to >=1 (plateaus at n=0,1).
    CHECK(rigUpgradeCost(128, 0, RigCostCurve::kLogStep) == 128);
    CHECK(rigUpgradeCost(128, 1, RigCostCurve::kLogStep) == 128);
    CHECK(rigUpgradeCost(128, 2, RigCostCurve::kLogStep) == 256);
    CHECK(rigUpgradeCost(128, 3, RigCostCurve::kLogStep) == 256);
    CHECK(rigUpgradeCost(128, 4, RigCostCurve::kLogStep) == 512);

    // LogStepHalf: Cost(n) = start * 2^(floor(log2(n+1))-1) — same shape, one tier down.
    CHECK(rigUpgradeCost(128, 0, RigCostCurve::kLogStepHalf) == 64);
    CHECK(rigUpgradeCost(128, 1, RigCostCurve::kLogStepHalf) == 128);
    CHECK(rigUpgradeCost(128, 2, RigCostCurve::kLogStepHalf) == 128);
    CHECK(rigUpgradeCost(128, 3, RigCostCurve::kLogStepHalf) == 256);
}

// Containment Rack Slot: broke → inert, an affordable buy raises rackSlots
// by 1 and gates ARCH Store at the new capacity, cost doubles per purchase, buy-to-cap
// MAXES out (inert past kRackSlotUpgradeMax).
static void test_hacker_shop_rack_slot_upgrade() {
    Game g{StartMode::Hatched};
    const int base = g.rackSlots();
    CHECK(g.rackSlotUpgradeCost() == kRackSlotUpgradeStart);
    g.debugSetBits(0);
    g.debugBuyRackSlotUpgrade();
    CHECK(g.rackSlots() == base && g.bits() == 0);          // broke → inert

    g.debugSetBits(kRackSlotUpgradeStart + 5);
    g.debugBuyRackSlotUpgrade();
    CHECK(g.rackSlots() == base + 1 && g.bits() == 5);      // +1 slot, Bits deducted
    CHECK(g.rackSlotUpgradeCost() == kRackSlotUpgradeStart * 2);  // doubles

    g.debugSetBits(100000000);
    for (int i = 0; i < kRackSlotUpgradeMax + 2; ++i) g.debugBuyRackSlotUpgrade();
    CHECK(g.rackSlotUpgradeCount() == kRackSlotUpgradeMax);       // capped (MAXED)
    CHECK(g.rackSlots() == base + kRackSlotUpgradeMax);
}

// Scraping Cluster Expansion (+combat Bits %) and Enhanced DataMining
// (+cache Bits %): each level lifts applyCombatBitsBonus()/applyCacheBitsBonus() by its
// per-level %, and the two bonuses are independent of each other.
static void test_hacker_shop_scraping_and_datamining_bonus() {
    Game g{StartMode::Hatched};
    CHECK(g.combatBitsBonusPct() == 0 && g.cacheBitsBonusPct() == 0);
    CHECK(g.applyCombatBitsBonus(100) == 100 && g.applyCacheBitsBonus(100) == 100);

    g.debugSetBits(1000000);
    for (int i = 0; i < 3; ++i) g.debugBuyScrapingCluster();
    CHECK(g.scrapingClusterLevel() == 3);
    CHECK(g.combatBitsBonusPct() == 3 * kScrapingClusterPctPerLevel);
    CHECK(g.applyCombatBitsBonus(100) == 100 + 100 * g.combatBitsBonusPct() / 100);
    CHECK(g.cacheBitsBonusPct() == 0);                      // untouched by the other track
    // Regression: on-device, buying 1-2 DataMining levels looked inert on a Common
    // cache (10 Bits) — `bits * pct / 100` truncates to 0 below the 100-Bits-worth-of-
    // percent threshold. Any ACTIVE bonus must add at least 1 Bit, even on a tiny grant.
    CHECK(g.applyCombatBitsBonus(10) == 10 + 1);            // 10 * 6% = 0.6 -> would floor to 0

    for (int i = 0; i < 2; ++i) g.debugBuyDataMining();
    CHECK(g.dataMiningLevel() == 2);
    CHECK(g.cacheBitsBonusPct() == 2 * kDataMiningPctPerLevel);
    CHECK(g.applyCacheBitsBonus(100) == 100 + 100 * g.cacheBitsBonusPct() / 100);
    const int commonBits = ContentRegistry::embedded().item("sealed_cache_common")->cache.bits;
    CHECK(g.applyCacheBitsBonus(commonBits) == commonBits + 1);  // same bug, real tier
    CHECK(g.combatBitsBonusPct() == 3 * kScrapingClusterPctPerLevel);  // still independent

    // A single level is the shallowest on-device case: still must not truncate to 0.
    Game g2{StartMode::Hatched};
    g2.debugSetBits(1000000);
    g2.debugBuyDataMining();
    CHECK(g2.dataMiningLevel() == 1 && g2.cacheBitsBonusPct() == kDataMiningPctPerLevel);
    CHECK(g2.applyCacheBitsBonus(commonBits) == commonBits + 1);
}

// b: the Reduce-Explore-Frag hacker-shop upgrade. Buying a tier follows the
// descending price ladder, is gated by Bits, caps at kFragReducerMaxTier, and lowers the
// effective battle-fatigue frag AMOUNT cap (kBattleFatigueFragMax − fragAmountTier).
static void test_hacker_shop_frag_reducer() {
    Game g{StartMode::Hatched};
    CHECK(g.fragAmountTier() == 0);
    g.debugSetBits(0);
    g.debugBuyFragReducer();
    CHECK(g.fragAmountTier() == 0 && g.bits() == 0);        // broke → inert
    g.debugSetBits(kFragReducerStart + 5);
    g.debugBuyFragReducer();
    CHECK(g.fragAmountTier() == 1 && g.bits() == 5);        // tier up, Bits deducted
    g.debugSetBits(100000);
    for (int i = 0; i < 10; ++i) g.debugBuyFragReducer();
    CHECK(g.fragAmountTier() == kFragReducerMaxTier);       // capped (MAXED)
    const int spent = 100000 - g.bits();
    g.debugBuyFragReducer();                                // MAXED → inert, no spend
    CHECK(100000 - g.bits() == spent);
    // The wired combat effect: the effective frag cap is reduced by the tier.
    CHECK(kBattleFatigueFragMax - static_cast<int>(g.fragAmountTier()) < kBattleFatigueFragMax);
}

// j/k — Passive XP Farming + its XP Farming Window: buying the rate row pays
// hungerXpRateLevel() XP per hunger point lost while Hunger stays above
// hungerXpThreshold(), and buying the window row lowers that threshold so more
// of a decay step counts. Every case below stays well under kLevelXpBase (100)
// so combatXp() is a direct, un-rolled-over read. The two tracks are
// independent Rig Shop rows.
static void test_hacker_shop_hunger_xp_farming() {
    Game g{StartMode::Hatched};
    CHECK(g.hungerXpRateLevel() == 0 && g.hungerXpThreshold() == kHungerXpBaseThreshold);

    // No rate bought yet: hunger decay pays nothing, even a lot of it.
    g.model().setHunger(100);
    g.debugTickHunger(60u * 60u * 1000u * 20u);           // plenty of decay steps
    CHECK(g.combatXp() == 0 && g.combatLevel() == 0);     // rate 0 → no XP

    // Buy 2 levels of the rate row: hunger 100 -> 95 (5 points) all sit above the
    // default 90%% threshold, so all 5 count at 2 XP/point = 10 XP.
    Game g2{StartMode::Hatched};
    g2.debugSetBits(1000000);
    for (int i = 0; i < 2; ++i) g2.debugBuyHungerXpRate();
    CHECK(g2.hungerXpRateLevel() == 2);
    g2.model().setHunger(100);
    g2.debugTickHunger(kHungerMinutesPerPoint * 60u * 1000u * 5u);  // 5 hunger points lost
    CHECK(g2.combatXp() == 5 * 2 && g2.combatLevel() == 0);

    // Drop hunger to sit right at the threshold band: only points ABOVE 90 count.
    Game g3{StartMode::Hatched};
    g3.debugSetBits(1000000);
    g3.debugBuyHungerXpRate();                              // rate level 1
    g3.model().setHunger(93);
    g3.debugTickHunger(kHungerMinutesPerPoint * 60u * 1000u * 10u);  // 93 -> 83
    // Only 93..91 (3 points) sit above the 90 threshold.
    CHECK(g3.combatXp() == 3 * 1 && g3.model().hunger() == 83);

    // Buying the window row lowers the threshold — more of the same decay counts.
    Game g4{StartMode::Hatched};
    g4.debugSetBits(1000000);
    g4.debugBuyHungerXpRate();                               // rate level 1
    g4.debugBuyHungerXpWindow();                              // threshold 90 -> 85
    CHECK(g4.hungerXpThreshold() == kHungerXpBaseThreshold - kHungerXpWindowStepPct);
    g4.model().setHunger(93);
    g4.debugTickHunger(kHungerMinutesPerPoint * 60u * 1000u * 10u);  // 93 -> 83
    // max(hungerAfter=83, threshold=85) = 85, so 93-85 = 8 points count — more
    // than g3's 3 (whose threshold, 90, sat above hungerAfter).
    CHECK(g4.combatXp() == 8 * 1);
}

// l/m — Well-Fed XP Boost + its XP Boost Window: the %% bonus only applies while
// CURRENT Hunger is at or above combatXpWindowThreshold(); buying the window row
// lowers that bar independently of the hunger-XP-farming window (k) above.
static void test_hacker_shop_combat_xp_boost() {
    Game g{StartMode::Hatched};
    CHECK(g.combatXpBonusPct() == 0 && g.applyCombatXpBonus(100) == 100);

    g.debugSetBits(1000000);
    for (int i = 0; i < 5; ++i) g.debugBuyCombatXpPct();
    CHECK(g.combatXpWindowThreshold() == kCombatXpBaseThreshold);  // window not bought yet

    // Hunger below the 90%% base threshold: the bonus is inert even though the
    // rate is bought.
    g.model().setHunger(kCombatXpBaseThreshold - 1);
    CHECK(g.combatXpBonusPct() == 0 && g.applyCombatXpBonus(100) == 100);

    // Hunger at/above the threshold: the bonus is live.
    g.model().setHunger(kCombatXpBaseThreshold);
    CHECK(g.combatXpBonusPct() == 5 * kCombatXpPctPerLevel);
    CHECK(g.applyCombatXpBonus(100) == 100 + 100 * g.combatXpBonusPct() / 100);
    // Small-value floor, same rounding rule as the Bits bonuses.
    CHECK(g.applyCombatXpBonus(10) == 10 + 1);

    // Buying the window row widens the qualifying range without touching the
    // hunger-XP-farming window (k) — the two thresholds are independent.
    g.debugBuyCombatXpWindow();
    CHECK(g.combatXpWindowThreshold() == kCombatXpBaseThreshold - kCombatXpWindowStepPct);
    CHECK(g.hungerXpThreshold() == kHungerXpBaseThreshold);   // untouched by sibling upgrade
    g.model().setHunger(kCombatXpBaseThreshold - kCombatXpWindowStepPct);
    CHECK(g.combatXpBonusPct() == 5 * kCombatXpPctPerLevel);  // now qualifies

    // Wired at a real payout site: Sim-Battle XP scales with the active bonus.
    // The heartbeat loop below covers well under kHungerMinutesPerPoint of game
    // time, so Hunger doesn't decay out of the window mid-fight.
    Game g2{StartMode::Hatched, "paypup"};
    g2.debugSetBits(1000000);
    for (int i = 0; i < 3; ++i) g2.debugBuyCombatXpPct();
    g2.model().setHunger(100);
    enterSimBattle(g2);
    uint32_t t = 0;
    for (int i = 0; i < 400 && g2.combat().outcome() == Combat::Outcome::Ongoing; ++i)
        g2.tick(t += kHeartbeatMs);
    CHECK(g2.combat().outcome() == Combat::Outcome::Win);
    g2.onButton(press(Button::B));                            // dismiss -> reward granted
    const int expectedXp = kSimXpReward + std::max(1, kSimXpReward * (3 * kCombatXpPctPerLevel) / 100);
    CHECK(g2.combatXp() == expectedXp);
}

// c — the Hacker-SHOP "Reduce Explore Frag TRIGGER %" upgrade: broke → inert,
// affording a buy → tier up + Bits deducted, buy to cap → MAXED/inert, and the effective
// battle-fatigue TRIGGER chance drops below the baseline. Sibling to the AMOUNT reducer.
static void test_hacker_shop_frag_trigger() {
    Game g{StartMode::Hatched};
    CHECK(g.fragTriggerTier() == 0);
    g.debugSetBits(0);
    g.debugBuyFragTrigger();
    CHECK(g.fragTriggerTier() == 0 && g.bits() == 0);      // broke → inert
    g.debugSetBits(kFragTriggerStart + 5);
    g.debugBuyFragTrigger();
    CHECK(g.fragTriggerTier() == 1 && g.bits() == 5);      // tier up, Bits deducted
    // The wired combat effect: the effective trigger chance now drops below baseline.
    CHECK(kFragTriggerReducedPct[g.fragTriggerTier() - 1] < kBattleFatigueChancePct);
    g.debugSetBits(1000000);
    for (int i = 0; i < 20; ++i) g.debugBuyFragTrigger();
    CHECK(g.fragTriggerTier() == kFragTriggerMaxTier);     // capped (MAXED)
    const int spent = 1000000 - g.bits();
    g.debugBuyFragTrigger();                               // MAXED → inert, no spend
    CHECK(1000000 - g.bits() == spent);
}

// d — the Hacker SHOP "ITEMS Type-Tabs" one-time unlock: a buy spends Bits
// and sets itemTabsUnlocked() (no tiers — a second buy is inert/OWNED, and an
// unaffordable buy leaves both Bits and the flag untouched).
static void test_hacker_shop_item_tabs_buy() {
    Game g{StartMode::Hatched};
    CHECK(!g.itemTabsUnlocked());
    g.debugSetBits(kShopItemTabsCost - 1);
    g.debugBuyItemTabs();
    CHECK(!g.itemTabsUnlocked() && g.bits() == kShopItemTabsCost - 1);   // broke -> inert
    g.debugSetBits(kShopItemTabsCost + 7);
    g.debugBuyItemTabs();
    CHECK(g.itemTabsUnlocked() && g.bits() == 7);            // bought, Bits deducted
    g.debugSetBits(9999);
    g.debugBuyItemTabs();                                    // already OWNED -> inert
    CHECK(g.bits() == 9999);
}

// e — the Hacker SHOP "VAULT Bulk-Open" one-time unlock: same shape as
// d (buy / broke-inert / OWNED-inert), on its own flag + cost.
static void test_hacker_shop_bulk_open_buy() {
    Game g{StartMode::Hatched};
    CHECK(!g.bulkOpenUnlocked());
    g.debugSetBits(kShopBulkOpenCost - 1);
    g.debugBuyBulkOpen();
    CHECK(!g.bulkOpenUnlocked() && g.bits() == kShopBulkOpenCost - 1);
    g.debugSetBits(kShopBulkOpenCost + 3);
    g.debugBuyBulkOpen();
    CHECK(g.bulkOpenUnlocked() && g.bits() == 3);
    g.debugSetBits(9999);
    g.debugBuyBulkOpen();                                    // already OWNED -> inert
    CHECK(g.bits() == 9999);
}

// d — buildInventoryRows(filter) actually narrows the list: Food/Buffs/Quest
// each return only that type's rows (+ its one header); All matches today's full
// grouped list. Inventory::starting() carries 2 Food + 3 Buffs + 2 Quest (7 items,
// 3 headers, per test_inventory_rows_grouped) — Tor-Tilla Chip is Food (no lasting
// buff; a happiness bump + a one-shot Deep Web surface effect, not a stat modifier).
// Backup Drive is a Buff (its combat shield, content_items.cpp), not a Quest item.
static void test_item_filter_narrows_rows() {
    ContentRegistry r = ContentRegistry::embedded();
    Inventory inv = Inventory::starting();
    auto all   = buildInventoryRows(r, inv, false, ItemFilter::All);
    auto food  = buildInventoryRows(r, inv, false, ItemFilter::Food);
    auto buffs = buildInventoryRows(r, inv, false, ItemFilter::Buffs);
    auto quest = buildInventoryRows(r, inv, false, ItemFilter::Quest);

    auto headerCount = [](const std::vector<InvRow>& rows) {
        int n = 0; for (const auto& row : rows) if (row.header) ++n; return n;
    };
    auto itemCount = [](const std::vector<InvRow>& rows) {
        int n = 0; for (const auto& row : rows) if (!row.header) ++n; return n;
    };
    CHECK(headerCount(all) == 3 && itemCount(all) == 7);
    CHECK(headerCount(food) == 1 && itemCount(food) == 2);
    CHECK(headerCount(buffs) == 1 && itemCount(buffs) == 3);
    CHECK(headerCount(quest) == 1 && itemCount(quest) == 2);
    for (const auto& row : food)  if (!row.header) CHECK(row.def->type == ItemDef::Type::Food);
    for (const auto& row : buffs) if (!row.header) CHECK(row.def->type == ItemDef::Type::Buff);
    for (const auto& row : quest) if (!row.header) CHECK(row.def->type == ItemDef::Type::Quest);
}

// d — the ITEMS hold-A gesture. Unowned: A steps the cursor immediately on
// PRESS (zero regression — no hold, no release wait). Owned: a short tap (release
// before kItemFilterHoldMs) still steps on release; holding past the threshold
// cycles the filter instead (ALL -> FOOD), narrowing the list, and the eventual
// release is a no-op (aHeld_ already consumed by the hold-fire).
static void test_item_hold_a_cycles_filter_tap_steps() {
    // (a) Unowned: A fires the step on PRESS alone (no release needed).
    {
        Game g{StartMode::Hatched};
        CHECK(!g.itemTabsUnlocked());
        enterSubmenuId(g, SubmenuId::Items);          // row 0 = Air-Gapped Snack (FOOD)
        g.onButton(press(Button::A));                 // PRESS only — steps immediately
        const int tc0 = g.inventory().count("tortilla_chip");
        g.onButton(press(Button::B));                 // open the now-focused row's detail
        g.onButton(press(Button::B));                 // Use
        CHECK(g.inventory().count("tortilla_chip") == tc0 - 1);  // row 1 (Tor-Tilla Chip, FOOD)
    }
    // (b) Owned, short tap: release before the hold threshold still steps (on release).
    {
        Game g{StartMode::Hatched};
        g.debugSetBits(kShopItemTabsCost);
        g.debugBuyItemTabs();
        CHECK(g.itemTabsUnlocked());
        enterSubmenuId(g, SubmenuId::Items);
        CHECK(g.itemFilter() == ItemFilter::All);
        uint32_t t = 0;
        g.tick(t);
        g.onButton(press(Button::A));                 // arm the hold — no step yet
        g.tick(t += kItemFilterHoldMs / 2);            // well under the threshold
        g.onButton({Button::A, false, false});         // release -> short tap -> step
        CHECK(g.itemFilter() == ItemFilter::All);      // a tap never touches the filter
        const int tc0 = g.inventory().count("tortilla_chip");
        g.onButton(press(Button::B));
        g.onButton(press(Button::B));                  // Use
        CHECK(g.inventory().count("tortilla_chip") == tc0 - 1);  // row 1, same as unowned
    }
    // (c) Owned, a hold past the threshold cycles the filter instead of stepping.
    {
        Game g{StartMode::Hatched};
        g.debugSetBits(kShopItemTabsCost);
        g.debugBuyItemTabs();
        enterSubmenuId(g, SubmenuId::Items);
        uint32_t t = 0;
        g.tick(t);
        g.onButton(press(Button::A));                  // arm the hold
        g.tick(t += kItemFilterHoldMs + kHeartbeatMs);  // cross the threshold -> cycles
        CHECK(g.itemFilter() == ItemFilter::Food);      // ALL -> FOOD
        g.onButton({Button::A, false, false});          // release AFTER the fire -> no-op
        CHECK(g.itemFilter() == ItemFilter::Food);       // unchanged by the release
        // The list is now narrowed to FOOD — B opens the first (only) food row.
        const int as0 = g.inventory().count("airgap_snack");
        g.onButton(press(Button::B));
        g.onButton(press(Button::B));                    // Use
        CHECK(g.inventory().count("airgap_snack") == as0 - 1);
    }
}

// The Rig Shop "ITEMS Type-Picker" one-time unlock: same buy shape as the two
// gesture unlocks above (broke-inert / bought / OWNED-inert), on its own flag + cost.
static void test_hacker_shop_item_picker_buy() {
    Game g{StartMode::Hatched};
    CHECK(!g.itemPickerUnlocked());
    g.debugSetBits(kShopItemPickerCost - 1);
    g.debugBuyItemPicker();
    CHECK(!g.itemPickerUnlocked() && g.bits() == kShopItemPickerCost - 1);
    g.debugSetBits(kShopItemPickerCost + 5);
    g.debugBuyItemPicker();
    CHECK(g.itemPickerUnlocked() && g.bits() == 5);
    g.debugSetBits(9999);
    g.debugBuyItemPicker();                                  // already OWNED -> inert
    CHECK(g.bits() == 9999);
}

// The category axis splits QUEST into KEYS + TOOLS without touching the type axis:
// the starting shelf's 2 quest items are both Keys (Decryption Key, Decryptogram),
// so a fresh bag has an EMPTY Tools category until a Defrag Tool lands in it. The
// filtered list's one header names the CATEGORY, not the type it sits under.
static void test_item_category_filters_split_quest() {
    ContentRegistry r = ContentRegistry::embedded();
    Inventory inv = Inventory::starting();
    auto itemCount = [](const std::vector<InvRow>& rows) {
        int n = 0; for (const auto& row : rows) if (!row.header) ++n; return n;
    };

    auto quest = buildInventoryRows(r, inv, false, ItemFilter::Quest);
    auto keys  = buildInventoryRows(r, inv, false, ItemFilter::Keys);
    auto tools = buildInventoryRows(r, inv, false, ItemFilter::Tools);
    CHECK(itemCount(quest) == 2 && itemCount(keys) == 2);   // every quest item is a Key
    CHECK(tools.empty());                                   // nothing carried yet
    CHECK(std::strcmp(keys[0].label, "KEYS") == 0);         // header names the category
    for (const auto& row : keys)
        if (!row.header) CHECK(itemCategory(*row.def) == ItemDef::Category::Keys);

    inv.add("disk_scrubber", 2);                            // a Defrag Tool is a TOOL
    tools = buildInventoryRows(r, inv, false, ItemFilter::Tools);
    keys  = buildInventoryRows(r, inv, false, ItemFilter::Keys);
    CHECK(itemCount(tools) == 1 && std::strcmp(tools[0].label, "TOOLS") == 0);
    CHECK(itemCount(keys) == 2);                            // Keys untouched by it
}

// The type-picker's tiles: fixed ALL/FOOD/BUFFS/KEYS/TOOLS order, each carrying the
// UNIT count of what's behind it. Caches are absent by construction (they decrypt in
// the VAULT, so buildInventoryRows never lists one and no tile can hold one).
static void test_item_picker_tiles_count_units() {
    ContentRegistry r = ContentRegistry::embedded();
    Inventory inv = Inventory::starting();      // 5 food + 3 buffs + 2 keys = 10 units
    auto tiles = buildItemPickerRows(r, inv);
    CHECK(tiles.size() == 5);
    CHECK(tiles[0].filter == ItemFilter::All   && tiles[0].units == 10);
    CHECK(tiles[1].filter == ItemFilter::Food  && tiles[1].units == 5);
    CHECK(tiles[2].filter == ItemFilter::Buffs && tiles[2].units == 3);
    CHECK(tiles[3].filter == ItemFilter::Keys  && tiles[3].units == 2);
    CHECK(tiles[4].filter == ItemFilter::Tools && tiles[4].units == 0);

    inv.add("sealed_cache_epic", 4);            // a cache moves no tile at all
    auto withCache = buildItemPickerRows(r, inv);
    for (size_t i = 0; i < tiles.size(); ++i) CHECK(withCache[i].units == tiles[i].units);
}

// Grayscale gate: on the type-picker, BOTH the focused tile's cursor marker and an
// empty category's dimmed row read without colour — selection and emptiness each
// carry a luminance channel of their own.
static void test_item_picker_grayscale() {
    ContentRegistry r = ContentRegistry::embedded();
    auto tiles = buildItemPickerRows(r, Inventory::starting());   // TOOLS tile is x0
    Framebuffer fb(kActiveW, kActiveH);
    drawItemTypePicker(fb, tiles, 0);

    constexpr int kPickRowH = 32;
    const int focusY = kItemsRowTop + 0 * kPickRowH + kPickRowH / 2;  // ALL (selected)
    const int plainY = kItemsRowTop + 1 * kPickRowH + kPickRowH / 2;  // FOOD (not)
    CHECK(luminance(fb.get(9, focusY)) - luminance(fb.get(9, plainY)) > 0.3f);

    // An empty tile's label is dimmer than a populated one's, at the same x.
    const int filledY = kItemsRowTop + 1 * kPickRowH + (kPickRowH - 7) / 2 + 3;
    const int emptyY  = kItemsRowTop + 4 * kPickRowH + (kPickRowH - 7) / 2 + 3;
    float filledMax = 0.f, emptyMax = 0.f;
    for (int x = 24; x < 24 + textWidth("BUFFS"); ++x) {
        for (int dy = -4; dy <= 4; ++dy) {
            filledMax = std::max(filledMax, luminance(fb.get(x, filledY + dy)));
            emptyMax  = std::max(emptyMax,  luminance(fb.get(x, emptyY + dy)));
        }
    }
    CHECK(filledMax - emptyMax > 0.15f);
}

// The picker's nav contract. Unowned: ITEMS opens straight on the list, exactly as
// before. Owned: ITEMS opens on the picker, A steps the tile, B commits that tile's
// filter and drills into its list, C on the list walks BACK to the picker, and C on
// the picker leaves ITEMS entirely.
static void test_item_picker_nav_drills_in_and_back() {
    {   // (a) Unowned — no picker layer anywhere.
        Game g{StartMode::Hatched};
        enterSubmenuId(g, SubmenuId::Items);
        CHECK(g.itemsScreen() == Game::ItemsScreen::List);
        g.onButton(press(Button::C));                       // straight back out
        CHECK(g.nav() == Game::Nav::Cursor);
    }
    // (b) Owned — the picker fronts the list.
    Game g{StartMode::Hatched};
    g.debugSetBits(kShopItemPickerCost);
    g.debugBuyItemPicker();
    enterSubmenuId(g, SubmenuId::Items);
    CHECK(g.itemsScreen() == Game::ItemsScreen::Picker && g.itemPickRow() == 0);

    g.onButton(press(Button::A));                           // ALL -> FOOD
    g.onButton(press(Button::A));                           // FOOD -> BUFFS
    g.onButton(press(Button::A));                           // BUFFS -> KEYS
    CHECK(g.itemPickRow() == 3);
    g.onButton(press(Button::B));                           // drill into KEYS
    CHECK(g.itemsScreen() == Game::ItemsScreen::List);
    CHECK(g.itemFilter() == ItemFilter::Keys);
    CHECK(g.nav() == Game::Nav::Submenu);                   // still L2, new screen

    g.onButton(press(Button::C));                           // back to the tiles
    CHECK(g.itemsScreen() == Game::ItemsScreen::Picker && g.nav() == Game::Nav::Submenu);
    CHECK(g.itemPickRow() == 3);                            // the tile is where we left it
    g.onButton(press(Button::C));                           // out of ITEMS
    CHECK(g.nav() == Game::Nav::Cursor);

    // Re-entering resets both the tile cursor and the filter — a visit never
    // inherits the last one's state.
    enterSubmenuId(g, SubmenuId::Items);
    CHECK(g.itemPickRow() == 0 && g.itemFilter() == ItemFilter::All);

    // An EMPTY category still opens; the list is simply empty and C walks back.
    g.onButton(press(Button::A)); g.onButton(press(Button::A));
    g.onButton(press(Button::A)); g.onButton(press(Button::A));   // -> TOOLS (x0)
    g.onButton(press(Button::B));
    CHECK(g.itemsScreen() == Game::ItemsScreen::List && g.itemFilter() == ItemFilter::Tools);
    CHECK(buildInventoryRows(ContentRegistry::embedded(), g.inventory(), false,
                             g.itemFilter()).empty());
    g.onButton(press(Button::B));                           // B on an empty list is inert
    CHECK(g.nav() == Game::Nav::Submenu);
    g.onButton(press(Button::C));
    CHECK(g.itemsScreen() == Game::ItemsScreen::Picker);
}

// The Lockout Open-Items escape hatch never routes through the picker, even when
// it's owned — a crisis doesn't get an extra screen between the player and a meal,
// and C from that list still returns to the Lockout modal rather than to tiles.
static void test_item_picker_skipped_in_lockout() {
    Game g{StartMode::Hatched};
    g.debugSetBits(kShopItemPickerCost);
    g.debugBuyItemPicker();
    g.model().setHunger(0);
    g.tick(kHeartbeatMs);
    CHECK(g.nav() == Game::Nav::ModalLockout);
    g.onButton(press(Button::B));                    // "Open Items" (lockout ctx)
    CHECK(g.nav() == Game::Nav::Submenu);
    CHECK(g.itemsScreen() == Game::ItemsScreen::List);   // straight onto the list
    g.onButton(press(Button::C));                    // C goes back to the crisis
    CHECK(g.nav() == Game::Nav::ModalLockout);
}

// Owning the picker upgrades the hold-A tab cycle to the finer CATEGORY axis, so
// the gesture and the tiles can never disagree about what a tab means. Type-Tabs
// alone still walks the type axis (its QUEST tab), and holding A on the PICKER
// screen does nothing at all — only the list arms the gesture.
static void test_item_hold_a_follows_picker_axis() {
    auto holdA = [](Game& g, uint32_t& t) {
        g.onButton(press(Button::A));
        g.tick(t += kItemFilterHoldMs + kHeartbeatMs);
    };
    {   // (a) Type-Tabs only: ALL -> FOOD -> BUFFS -> QUEST.
        Game g{StartMode::Hatched};
        g.debugSetBits(kShopItemTabsCost);
        g.debugBuyItemTabs();
        enterSubmenuId(g, SubmenuId::Items);
        uint32_t t = 0;
        g.tick(t);
        holdA(g, t); holdA(g, t); holdA(g, t);
        CHECK(g.itemFilter() == ItemFilter::Quest);
    }
    {   // (b) Both owned: ALL -> FOOD -> BUFFS -> KEYS -> TOOLS -> ALL.
        Game g{StartMode::Hatched};
        g.debugSetBits(kShopItemTabsCost + kShopItemPickerCost);
        g.debugBuyItemTabs();
        g.debugBuyItemPicker();
        enterSubmenuId(g, SubmenuId::Items);
        uint32_t t = 0;
        g.tick(t);
        // On the PICKER, A is a plain tile step — the hold gesture is never armed.
        holdA(g, t);
        CHECK(g.itemFilter() == ItemFilter::All && g.itemPickRow() == 1);
        g.onButton({Button::A, false, false});
        g.onButton(press(Button::C));                // out and back in -> tile 0 (ALL)
        enterSubmenuId(g, SubmenuId::Items);
        g.onButton(press(Button::B));                // drill into ALL
        CHECK(g.itemsScreen() == Game::ItemsScreen::List);
        holdA(g, t); holdA(g, t); holdA(g, t);
        CHECK(g.itemFilter() == ItemFilter::Keys);
        holdA(g, t);
        CHECK(g.itemFilter() == ItemFilter::Tools);
        holdA(g, t);
        CHECK(g.itemFilter() == ItemFilter::All);    // wraps, never lands on QUEST
    }
}

// e — openAllCachesOfRarity's aggregation: grant N caches of one rarity plus
// caches of a DIFFERENT rarity, invoke the bulk path directly (debugBulkOpenCaches),
// and assert ALL of the target rarity are consumed in one action, the aggregate
// tally + Bits sum across every open, and the other rarity is left untouched.
static void test_vault_bulk_open_consumes_all_of_rarity() {
    Game g{StartMode::Hatched};
    g.inventory().add("sealed_cache_common", 4);
    g.inventory().add("sealed_cache_uncommon", 2);    // different rarity — must survive
    const int bits0 = g.bits();

    g.debugBulkOpenCaches("sealed_cache_common");

    CHECK(g.inventory().count("sealed_cache_common") == 0);     // ALL of that rarity gone
    CHECK(g.inventory().count("sealed_cache_uncommon") == 2);   // other rarity untouched
    CHECK(g.bulkYieldCachesOpened() == 4);
    CHECK(g.bulkYieldBits() == 4 * ContentRegistry::embedded().item("sealed_cache_common")->cache.bits);  // aggregate Bits sum
    CHECK(g.bits() == bits0 + g.bulkYieldBits());
    CHECK(g.nav() == Game::Nav::BulkYield);

    // Each common cache draws exactly 1 item (its row's cache.draws == 1) — the tally
    // sums to 4 across however many distinct item ids were drawn.
    int tallied = 0;
    for (int i = 0; i < g.bulkYieldTallyCount(); ++i) tallied += g.bulkYieldTallyCountAt(i);
    CHECK(tallied == 4);
}

// e — the VAULT hold-B gesture, end to end through the real Hacker-face
// navigation: a short tap still opens just the focused cache (unchanged, single-open
// reveal); holding past kBulkOpenHoldMs bulk-opens everything sharing its rarity in
// one action (the aggregate reveal), and the eventual release after the fire is a
// no-op (bHeld_ already consumed).
static void test_vault_hold_b_bulk_opens_tap_opens_one() {
    Game g{StartMode::Hatched};
    g.inventory().add("sealed_cache_common", 3);
    g.debugSetBits(kShopBulkOpenCost);
    g.debugBuyBulkOpen();
    CHECK(g.bulkOpenUnlocked());

    g.onButton({Button::A, true, true});              // PET -> HACKER
    g.onButton(press(Button::A));                      // summon the hacker cursor
    while (hackerCarouselSlots()[g.cursor()].id != HackerSlotId::Vault)
        g.onButton(press(Button::A));
    g.onButton(press(Button::B));                       // enter VAULT
    CHECK(g.nav() == Game::Nav::Submenu);

    // A short tap opens just the one (unchanged).
    uint32_t t = 0;
    g.tick(t);
    g.onButton(press(Button::B));                        // arm the hold
    g.tick(t += kBulkOpenHoldMs / 2);                     // well under the threshold
    g.onButton({Button::B, false, false});                // release -> short tap -> open one
    CHECK(g.inventory().count("sealed_cache_common") == 2);   // exactly one consumed
    CHECK(g.nav() == Game::Nav::CacheYield);                  // single-open reveal
    g.onButton(press(Button::C));                              // dismiss -> back to VAULT
    CHECK(g.nav() == Game::Nav::Submenu);

    // Holding past the threshold bulk-opens the rest in one action.
    g.onButton(press(Button::B));                         // arm the hold again
    g.tick(t += kBulkOpenHoldMs + kHeartbeatMs);           // cross the threshold -> bulk
    CHECK(g.inventory().count("sealed_cache_common") == 0);    // the remaining 2, together
    CHECK(g.bulkYieldCachesOpened() == 2);
    CHECK(g.nav() == Game::Nav::BulkYield);
    g.onButton({Button::B, false, false});                 // release AFTER the fire -> no-op
    CHECK(g.nav() == Game::Nav::BulkYield);                 // unchanged
}

// re-home: sealed caches decrypt from the Hacker VAULT, not pet-side ITEMS. Using
// an openable from ITEMS is inert (gated "DECRYPT IN VAULT") and does NOT consume it or
// pay out; the VAULT path (debugOpenCache) is the one that decrypts.
static void test_cache_not_openable_from_items() {
    Game g{StartMode::Hatched};
    g.inventory().add("sealed_cache_epic", 1);
    const int bits0 = g.bits();
    g.debugUseItem("sealed_cache_epic");                   // pet-side ITEMS — now inert
    CHECK(g.inventory().count("sealed_cache_epic") == 1);  // not consumed
    CHECK(g.bits() == bits0);                              // no reward paid
    CHECK(g.nav() != Game::Nav::CacheYield);               // no reveal
    g.debugOpenCache("sealed_cache_epic");                 // VAULT path decrypts it
    CHECK(g.inventory().count("sealed_cache_epic") == 0);  // consumed
    CHECK(g.nav() == Game::Nav::CacheYield);
}

// battle fatigue is a per-fight FRAGMENTATION tax (no separate meter).
//   (1) hands-off auto-explore PAUSES once frag hits the danger gate, and resumes once
//       a defrag drops it back below (the mode stays armed throughout);
//   (2) resolved wild non-boss fights accrue Fragmentation (the pet degrades from
//       fighting), so endless farming forces a defrag.
static void test_battle_fatigue() {
    // (1) The auto-explore pause gate.
    {
        Game g{StartMode::Hatched};
        enterWalk(g);                                    // arm explore, resting at idle
        CHECK(!g.exploreAutoPausedByFatigue());
        g.model().setFragmentation(kBattleFatigueAutoPauseFrag);  // too fragmented
        CHECK(g.exploreAutoPausedByFatigue());
        const int steps0 = g.exploreSteps();
        uint32_t t = 0;
        for (int i = 0; i < 300; ++i) g.tick(t += kHeartbeatMs);
        CHECK(g.exploreSteps() == steps0);               // paused: no auto-steps fired
        CHECK(g.exploreActive());                        // ...but the mode stays armed
        g.model().setFragmentation(0);                   // a defrag clears the fatigue
        CHECK(!g.exploreAutoPausedByFatigue());
        for (int i = 0; i < 300 && g.exploreSteps() == steps0; ++i)
            g.tick(t += kHeartbeatMs);
        CHECK(g.exploreSteps() > steps0);                // stepping resumes
    }
    // (2) Wild fights fatigue the pet: with the Bandwidth shield EXHAUSTED,
    //     frag climbs over several wild wins. (A stocked pool would absorb the
    //     tax instead — that's covered by test_bandwidth_farming_resource (6).) Keep the
    //     pool pinned at 0 each iteration so the slow regen can't sneak a charge back in.
    {
        Game g{StartMode::Hatched};
        const int frag0 = g.model().fragmentation();
        walkToEncounter(g);
        g.debugSetBandwidth(0);
        int wilds = 0, guard = 0;
        uint32_t t = 0;
        while (wilds < 4 && guard++ < 400) {
            g.debugSetBandwidth(0);                          // no shield → fatigue applies
            switch (g.nav()) {
                case Game::Nav::Combat: {
                    for (int i = 0; i < 400 &&
                         g.combat().outcome() == Combat::Outcome::Ongoing; ++i)
                        g.tick(t += kHeartbeatMs);
                    if (g.combat().outcome() == Combat::Outcome::Win) ++wilds;
                    for (int i = 0; i < 40 && g.nav() == Game::Nav::Combat; ++i)
                        g.tick(t += kHeartbeatMs);       // auto-dismiss the result hold
                    break;
                }
                case Game::Nav::Wifi: g.onButton(press(Button::B)); break;
                case Game::Nav::Shop: g.onButton(press(Button::C)); break;
                case Game::Nav::ModShop: g.onButton(press(Button::C)); break;
                case Game::Nav::Idle:
                    if (g.exploreActive()) pingExplore(g);   // fire the next event
                    else guard = 400;                        // explore ended (a loss) → stop
                    break;
                default: g.onButton(press(Button::C)); break;  // dismiss in-place events
            }
        }
        CHECK(wilds >= 3);                               // actually fought several wilds
        CHECK(g.model().fragmentation() > frag0);        // battle fatigue accrued frag
    }
}

// hands-free auto-step: the mode steps ITSELF on a timer from the idle
// habitat — no button mashing. Ticking ~kWalkAutoStepBeats heartbeats fires a
// guaranteed event hands-free (here the deterministic first roll leaves Idle).
static void test_explore_autosteps_hands_free() {
    Game g{StartMode::Hatched};
    enterWalk(g);
    CHECK(g.nav() == Game::Nav::Idle);
    CHECK(g.exploreActive());
    uint32_t t = 0;
    // Tick hands-free until the first guaranteed event fires (leaves Idle).
    for (int i = 0; i < 2000 && g.nav() == Game::Nav::Idle; ++i)
        g.tick(t += kHeartbeatMs);
    CHECK(g.nav() != Game::Nav::Idle);                  // reached an event, zero presses
    CHECK(g.exploreSteps() >= 1);
}

// every step is a guaranteed event: pinging from the idle habitat NEVER
// leaves you resting on a "quiet" beat — each ping resolves a real event (an
// in-place one keeps you at Idle, a full-screen one moves nav). Over many pings the
// step counter climbs one-for-one and at least one full-screen event is reached.
static void test_explore_every_step_is_an_event() {
    Game g{StartMode::Hatched};
    enterWalk(g);
    int before = g.exploreSteps();
    bool sawScreenEvent = false;
    uint32_t t = 0;
    for (int i = 0; i < 60; ++i) {
        if (g.nav() == Game::Nav::Idle) {
            pingExplore(g);
            CHECK(g.exploreSteps() == before + 1);      // the ping DID step
            before = g.exploreSteps();
            if (g.nav() != Game::Nav::Idle) sawScreenEvent = true;
        } else {
            // Dismiss whatever full-screen event we landed on, back to Idle.
            switch (g.nav()) {
                case Game::Nav::Encounter: g.onButton(press(Button::C)); break;  // flee
                case Game::Nav::Wifi:      g.onButton(press(Button::B)); break;
                case Game::Nav::Shop:      g.onButton(press(Button::C)); break;
                case Game::Nav::ModShop:      g.onButton(press(Button::C)); break;
                case Game::Nav::Combat:
                    for (int j = 0; j < 400 &&
                            g.combat().outcome() == Combat::Outcome::Ongoing; ++j)
                        g.tick(t += kHeartbeatMs);
                    g.onButton(press(Button::B));
                    break;
                default: g.onButton(press(Button::B)); break;
            }
        }
    }
    CHECK(sawScreenEvent);                               // wild dominates the table
}

// A wild encounter auto-starts the shared combat core with LIVE stakes —
// this replaces debugStartCombat as the real entry point. A raised pet beats the
// sector[0] malbeast; on dismiss it returns to the IDLE habitat with explore-mode
// STILL running (never the EXPL list or the carousel), advances the win-streak, and
// logs a COMBAT_WON with wild-win rewards.
static void test_encounter_fight_live_combat_win() {
    Game g{StartMode::Hatched};
    walkToEncounter(g);                                 // lands IN the auto-started fight
    const int bits0 = g.bits();
    const int xp0 = g.combatXp();   // measure THIS fight's delta — the walk here may
                                    // pass through an awakened-guardian combat
    const int streak0 = g.exploreStreak();
    CHECK(g.nav() == Game::Nav::Combat);
    CHECK(g.combat().stakes() == Combat::Stakes::Live);
    uint32_t t = 0;
    for (int i = 0; i < 400 && g.combat().outcome() == Combat::Outcome::Ongoing; ++i)
        g.tick(t += kHeartbeatMs);
    CHECK(g.combat().outcome() == Combat::Outcome::Win);
    g.onButton(press(Button::B));                        // dismiss
    // A won WILD fight parks on the post-encounter status readout first
    // before the habitat — any button dismisses it too.
    CHECK(g.nav() == Game::Nav::PostEncounter);
    g.onButton(press(Button::B));                        // dismiss the readout
    CHECK(g.nav() == Game::Nav::Idle);                    // back to the habitat,
    CHECK(g.exploreActive());                             // mode keeps running
    CHECK(g.exploreStreak() == streak0 + 1);              // a win advances the streak
    // payout scales with the opponent: the sector-0 malbeast is a 1-pip
    // (Process-tier, R=2) wild, so a win pays randInt(2, 4). No mods
    // equipped → no bonus.
    const int gained = g.bits() - bits0;
    CHECK(gained >= 2 && gained <= 4);
    CHECK(g.combatXp() - xp0 == kWildWinXpReward);       // this fight = the flat wild-win XP
    CHECK(g.log().size() >= 1 && g.log().at(0).type == LogEventType::CombatWon);
}

// Hands-off end-to-end: a wild fight auto-starts, auto-resolves, then
// auto-DISMISSES after the result hold — with ZERO button presses — so explore-mode
// keeps stepping on its own. A raised pet wins, so the streak advances and the mode
// stays armed. (Without the auto-dismiss the fight would park on the result beat
// forever, stalling the background loop.)
static void test_explore_auto_continues_after_fight() {
    Game g{StartMode::Hatched};
    walkToEncounter(g);                                 // lands IN the auto-started fight
    CHECK(g.nav() == Game::Nav::Combat);
    const int streak0 = g.exploreStreak();
    // Tick hands-free: resolve -> hold -> auto-dismiss back to the idle habitat.
    uint32_t t = 0;
    bool returned = false;
    for (int i = 0; i < 2000 && !returned; ++i) {
        g.tick(t += kHeartbeatMs);
        if (g.nav() == Game::Nav::Idle) returned = true;
    }
    CHECK(returned);                                     // auto-dismissed, no presses
    CHECK(g.exploreActive());                            // mode still running
    CHECK(g.exploreStreak() == streak0 + 1);             // the win advanced the streak
}

// ---------------------------------------------------------------------------
// Post-encounter status readout: a brief BANDWIDTH/FRAG delta
// overlay after an EXPLORE wild fight resolves, so the player knows whether to
// keep exploring or go defrag. Resolve-and-stop-at-the-overlay helper (mirrors
// winAutoExploreFight, but doesn't assume Win — a Lose also reaches it).
// ---------------------------------------------------------------------------
static void resolveWildFightToPostEncounter(Game& g, uint32_t& t) {
    for (int i = 0; i < 400 && g.combat().outcome() == Combat::Outcome::Ongoing; ++i)
        g.tick(t += kHeartbeatMs);
    for (int i = 0; i < 40 && g.nav() == Game::Nav::Combat; ++i)
        g.tick(t += kHeartbeatMs);           // the explore hands-off reveal hold
}

// A WON farm fight (a re-armed CLEARED sub-area) with Bandwidth still in the pool
// hits the shield: the overlay reports the exact 1-charge spend AND
// SHIELDED (never a frag rise alongside it — the two are mutually exclusive).
static void test_post_encounter_reports_bandwidth_shield() {
    Game g{StartMode::Hatched};
    walkToEncounter(g);
    const int S = g.exploreSector(), U = g.exploreSub();
    g.debugSetSubCleared(S, U, true);            // a re-farm ground now
    CHECK(g.bandwidth() == kBandwidthMax);       // fresh pool
    const int frag0 = g.model().fragmentation();
    uint32_t t = 0;
    resolveWildFightToPostEncounter(g, t);
    CHECK(g.combat().outcome() == Combat::Outcome::Win);
    CHECK(g.nav() == Game::Nav::PostEncounter);  // parked on the readout, not idle yet
    // Bandwidth line: exactly 1 charge spent this encounter.
    CHECK(g.debugPostEncBwBefore() == kBandwidthMax);
    CHECK(g.debugPostEncBwAfter() == kBandwidthMax - 1);
    CHECK(g.bandwidth() == kBandwidthMax - 1);
    // Frag line: SHIELDED, and indeed no rise happened.
    CHECK(g.debugPostEncShielded());
    CHECK(g.debugPostEncFragAfter() == g.debugPostEncFragBefore());
    CHECK(g.model().fragmentation() == frag0);
    // The rendered overlay reads (grayscale-safe, per the release gate).
    Framebuffer fb(kActiveW, kActiveH);
    g.render(fb);
    CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));
    // Any button dismisses early, straight back to the habitat (explore-mode live).
    g.onButton(press(Button::C));
    CHECK(g.nav() == Game::Nav::Idle);
    CHECK(g.exploreActive());
}

// With Bandwidth drained to 0, a farm win is UNSHIELDED — the fatigue tax
// can land (it's a chance roll, so this samples several fights). When it lands the
// overlay reports the exact +n rise; the shield flag stays off throughout, and
// bandwidth (already 0) never reads as spent again ("nothing to spend" case).
static void test_post_encounter_reports_frag_rise_when_unshielded() {
    Game g{StartMode::Hatched};
    walkToEncounter(g);
    const int S = g.exploreSector(), U = g.exploreSub();
    g.debugSetSubCleared(S, U, true);
    g.debugSetBandwidth(0);                      // nothing left to spend -> no shield
    bool sawRise = false;
    uint32_t t = 0;
    for (int fights = 0; fights < 20 && !sawRise; ++fights) {
        resolveWildFightToPostEncounter(g, t);
        CHECK(g.nav() == Game::Nav::PostEncounter);
        CHECK(!g.debugPostEncShielded());        // bandwidth stayed 0 -> never shielded
        CHECK(g.debugPostEncBwBefore() == 0);
        CHECK(g.debugPostEncBwAfter() == 0);      // "nothing to spend", not a fresh -1
        const int before = g.debugPostEncFragBefore(), after = g.debugPostEncFragAfter();
        CHECK(after >= before);                   // never SHIELDED and rose at once
        if (after > before) sawRise = true;
        g.onButton(press(Button::B));             // dismiss -> back to the habitat
        if (!g.exploreActive()) break;             // a loss ended this run
        pingExplore(g);
        if (g.nav() != Game::Nav::Combat) break;   // landed on a non-combat event; stop
    }
    CHECK(sawRise);                                // the tax landed at least once
}

// Sim-Battle (TRAIN's safe-stakes practice fight) never touches Bandwidth or the
// explore stakes, so it must never route to Nav::PostEncounter on dismiss —
// confirms the overlay is explore-Wild-only, not "any resolved fight".
static void test_post_encounter_never_for_sim_battle() {
    Game g{StartMode::Hatched, "paypup"};
    enterSimBattle(g);
    CHECK(g.nav() == Game::Nav::Combat);
    CHECK(g.combat().stakes() == Combat::Stakes::Safe);
    uint32_t t = 0;
    for (int i = 0; i < 400 && g.combat().outcome() == Combat::Outcome::Ongoing; ++i)
        g.tick(t += kHeartbeatMs);
    g.onButton(press(Button::B));                 // dismiss
    CHECK(g.nav() != Game::Nav::PostEncounter);    // straight to the TRAIN loadout
    CHECK(g.nav() == Game::Nav::Submenu);
}

// The post-encounter readout names the stat a level-up gained (surface the
// stat delta). Asserted directly on the render function — a non-null level line paints
// its own band (y~100), a null one leaves it blank — so it's checked without grinding a
// fight to an organic level-up. The frag state is None so nothing else occupies that band.
static void test_post_encounter_level_line_renders() {
    Framebuffer withLine(kActiveW, kActiveH), noLine(kActiveW, kActiveH);
    drawPostEncounter(withLine, kBandwidthMax, kBandwidthMax - 1, kBandwidthMax,
                      PostEncounterFragState::None, 0, "LVL 3  POWER +1");
    drawPostEncounter(noLine, kBandwidthMax, kBandwidthMax - 1, kBandwidthMax,
                      PostEncounterFragState::None, 0, nullptr);
    // The two frames are identical except for the level band (y~100): the line paints
    // there only when present, and nothing above it changes. (regionDiffers is theme-
    // agnostic — hasDarkInk can't be used since PAPER itself is a dark fill.)
    CHECK(regionDiffers(withLine, noLine, 0, 96, kActiveW, 112));  // the line renders
    CHECK(!regionDiffers(withLine, noLine, 0, 0, kActiveW, 90));   // ...only there
}

// ---------------------------------------------------------------------------
// Sector clearing: linear complete-to-advance gating + boss/gauntlet
// ---------------------------------------------------------------------------

// The linear gate: sector 0 is always open; sector N>0 opens
// only once sector N-1 is cleared; a null flags array = only sector 0. Rank plays
// no part (reward-only).
static void test_expl_sector_linear_gating() {
    CHECK(explSectorOpen(0, nullptr));            // sector 0 always open
    CHECK(!explSectorOpen(1, nullptr));           // null flags -> nothing else open
    bool none[kExplSectors] = {false, false};
    CHECK(explSectorOpen(0, none));
    CHECK(!explSectorOpen(1, none));              // sector 0 not cleared -> 1 locked
    bool one[kExplSectors] = {true, false, false};
    CHECK(explSectorOpen(0, one));
    CHECK(explSectorOpen(1, one));                // sector 0 cleared -> 1 open
    CHECK(!explSectorOpen(2, one));               // sector 1 NOT cleared -> 2 locked
    CHECK(!explSectorOpen(-1, one));
    bool two[kExplSectors] = {true, true, false};
    CHECK(explSectorOpen(2, two));               // sectors 0+1 cleared -> 2 opens
    CHECK(!explSectorOpen(kExplSectors, two));   // no area past the last -> closed
    // Sub-area names are table-indexed and non-empty for every sector.
    for (int s = 0; s < kExplSectors; ++s)
        for (int i = 0; i < kExplSubAreas; ++i)
            CHECK(explSubAreaName(s, i)[0] != '\0');
    CHECK(explSubAreaName(0, kExplSubAreas)[0] == '\0');   // out of range -> ""
    // Content spot-checks, addressed by area ID rather than ladder index: what an area
    // IS must survive being moved, so a reorder should never land here. The one genuinely
    // positional fact — its tier — is asserted against the position, not a literal.
    auto rung = [](const char* id) {
        for (int i = 0; i < kExplSectors; ++i)
            if (std::strcmp(area(i).id, id) == 0) return i;
        return -1;
    };
    const int sea = rung("net_sea_crossing");     // sailed, between the bayou and the moors
    CHECK(sea >= 0);
    CHECK(std::strcmp(explSectorName(sea), "NET-SEA CROSSING") == 0);
    CHECK(explSectorTier(sea) == sea + 1);
    CHECK(std::strcmp(sectorTitle(sea), "BUNDLE BREAKER") == 0);
    CHECK(std::strcmp(explSubAreaName(sea, 0), "UNINSTALL UNDERTOW") == 0);
    CHECK(std::strcmp(explSubAreaName(sea, kExplSubAreas - 1), "SANDBOX BEACH") == 0);
    CHECK(std::strcmp(shopName(sea), "FLOATING POINT") == 0);
    CHECK(std::strcmp(modShopName(sea), "THE HARDENED SHELL") == 0);
    const int moors = rung("napstorrent_moors");  // Napster/P2P, marshy journey → the castle
    CHECK(moors >= 0);
    CHECK(std::strcmp(explSectorName(moors), "NAPSTORRENT MOORS") == 0);
    CHECK(explSectorTier(moors) == moors + 1);
    CHECK(std::strcmp(sectorTitle(moors), "MOOR MARAUDER") == 0);
    CHECK(std::strcmp(explSubAreaName(moors, 0), "SEEDER SHALLOWS") == 0);
    CHECK(std::strcmp(explSubAreaName(moors, kExplSubAreas - 1), "CASTLE CAUSEWAY") == 0);
    CHECK(std::strcmp(shopName(moors), "MOOR-TO-MOOR") == 0);
    const int keep = rung("castle_rapidscare");   // where the Moors' Castle Causeway arrives
    CHECK(keep >= 0);
    CHECK(std::strcmp(explSectorName(keep), "CASTLE RAPIDSCARE") == 0);
    CHECK(explSectorTier(keep) == keep + 1);
    CHECK(std::strcmp(sectorTitle(keep), "KING OF THE KEEP") == 0);
    CHECK(std::strcmp(explSubAreaName(keep, 0), "404 DRAWBRIDGE") == 0);
    CHECK(std::strcmp(explSubAreaName(keep, kExplSubAreas - 1), "COMMENT CATACOMBS") == 0);
    CHECK(std::strcmp(shopName(keep), "SPAM & SCRAM") == 0);
    CHECK(std::strcmp(modShopName(keep), "THE GHOST IN THE MACHINE") == 0);
    // The causeway walks up to the keep, so the fiction needs those two ADJACENT — the
    // one ordering constraint on the ladder, and the thing a future insert must not break.
    CHECK(keep == moors + 1);
}

// Every EXPL sub-area row has to fit BESIDE its right-aligned state tag, and every
// storefront header beside the wallet — a name that outgrows either just draws over
// the other with no warning, on a screen no test renders name-by-name. Measured
// through the renderers' own metrics so it can't drift from what ships.
static void test_expl_names_fit_their_rows() {
    // Every EXPL row is a TITLE line — name at x=34, state tag right-aligned against the
    // 8px margin — over a DETAIL line running the full width from the same x. Each name
    // is budgeted against the widest tag ITS OWN row can pair with.
    const int titleX = 34, margin = 8;
    const int subNameW  = kActiveW - margin - textWidth("> FIGHT BOSS") - titleX;
    const int areaNameW = kActiveW - margin - textWidth("CLEARED") - titleX;
    const int bossNameW = kActiveW - margin - textWidth("> AREA BOSS") - titleX;
    const int detailW   = kActiveW - margin - titleX;
    // The breadcrumb header inside an area: "EXPL", the cursor triangle, then the area
    // name at a fixed offset past both.
    const int crumbX = margin + textWidth("EXPL") + 16;
    for (int s = 0; s < kExplSectors; ++s) {
        CHECK(textWidth(explSectorName(s)) <= areaNameW);         // top-level zone row
        CHECK(textWidth(explSectorName(s)) <= kActiveW - margin - crumbX);
        CHECK(textWidth(area(s).areaBossName) <= bossNameW);      // the area-gauntlet row
        // The Title a cleared area grants rides that same row's DETAIL line, as do the
        // sub-boss names on a boss-ready sub-area row.
        CHECK(textWidth("TITLE: ") + textWidth(sectorTitle(s)) <= detailW);
        for (int i = 0; i < kExplSubAreas; ++i) {
            CHECK(textWidth(explSubAreaName(s, i)) <= subNameW);
            CHECK(textWidth("BOSS: ") + textWidth(area(s).subBossNames[i]) <= detailW);
        }
        // Storefront headers: drawn at the left margin, with the Bits wallet right-
        // aligned opposite. Budget the wallet at a 6-figure purse plus its unit.
        const int storeNameW = kActiveW - 2 * margin - textWidth("999999 B");
        CHECK(textWidth(shopName(s)) <= storeNameW);
        CHECK(textWidth(modShopName(s)) <= storeNameW);
    }
}

// THE LEVEL IS THE LIST (explRowInLevel): the TOP level draws the DeepWeb row plus one
// row per AREA and none of the sub-areas; inside an area it draws that area's own block
// and nothing else. That is what keeps the drawn list ~6 rows however long the ladder
// grows, instead of a 13-row window over the whole thing.
static void test_expl_level_scoped_rows() {
    int top = 0;
    for (int r = 0; r < explRowCount(); ++r) {
        if (!explRowInLevel(r, -1)) continue;
        ++top;
        CHECK(explRowIsDeepWeb(r) || explRowSub(r) < 0);   // zones only, no sub-areas
    }
    CHECK(top == 1 + kExplSectors);                        // DeepWeb + every area
    for (int a = 0; a < kExplSectors; ++a) {
        int inside = 0;
        for (int r = 0; r < explRowCount(); ++r) {
            if (!explRowInLevel(r, a)) continue;
            ++inside;
            // Its own block only — not a neighbour's rows, and not the DeepWeb row.
            CHECK(!explRowIsDeepWeb(r) && explRowArea(r) == a);
        }
        CHECK(inside == 1 + kExplSubAreas);                 // boss row + its sub-areas
    }
}

// Combat::begin's carry-health (the gauntlet no-heal contract): a round that
// carries a wounded Health in starts there, not at full; a fresh round (-1) starts
// full; the carry is clamped to [1, maxHealth].
static void test_combat_carry_health() {
    ContentRegistry r = ContentRegistry::embedded();
    Combatant p = mkCombatant(r, "P", 60, 10, {"quick_jab"});
    Combatant e = mkCombatant(r, "E", 40, 8, {"quick_jab"});
    Combat c;
    c.begin(p, e, Combat::Stakes::Live, 111, false, /*carry=*/-1);
    CHECK(c.player().health == 60);               // fresh round: full
    c.begin(p, e, Combat::Stakes::Live, 111, false, /*carry=*/25);
    CHECK(c.player().health == 25);               // carried wounded Health
    c.begin(p, e, Combat::Stakes::Live, 111, false, /*carry=*/999);
    CHECK(c.player().health == 60);               // clamped to max
    c.begin(p, e, Combat::Stakes::Live, 111, false, /*carry=*/0);
    CHECK(c.player().health == 1);                // never a dead start
}

// The Bits payout bounds: a NORMAL opponent pays [R, R²];
// a BOSS sums R such rolls -> [R², R³]. Sampled over many rolls, every draw stays
// in range and the extremes are reachable.
static void test_bits_reward_bounds() {
    uint32_t rng = 0xC0FFEEu;
    for (int R = 1; R <= 4; ++R) {
        int nLo = 0, nHi = 0;
        for (int i = 0; i < 4000; ++i) {
            const int n = normalBitsReward(R, rng);
            CHECK(n >= R && n <= R * R);           // normal draw: [R, R²]
            if (n == R) ++nLo;
            if (n == R * R) ++nHi;
            // Boss sums R normal draws -> [R², R³] (Process 4..8, Script 9..27,
            // Daemon 16..64). The exact extremes need all R draws to agree, which
            // is astronomically rare for R≥3, so only the range is asserted here.
            const int b = bossBitsReward(R, rng);
            CHECK(b >= R * R && b <= R * R * R);
        }
        CHECK(nLo > 0 && nHi > 0);                 // both normal extremes reachable
    }
}

// Grind wild WINS on the currently-armed frontier sub-area until its boss unlocks,
// then trigger the boss MANUALLY from EXPL and beat it so the sub-area is CLEARED
// (Re)arms the frontier first, so calling it in order clears sub-areas
// 0..4 one by one. Assumes a pet strong enough to win reliably (a leveled Daemon).
static void clearSubArea(Game& g, int area, int sub) {
    uint32_t t = 0;
    g.debugArmExplore(area, sub);                  // (re)arm THIS sub-area (cleared subs
                                                   // are now selectable → "first row" is
                                                   // ambiguous; arm the target directly)
    CHECK(g.exploreSector() == area && g.exploreSub() == sub);
    // Grind to the boss unlock. The boss never rolls as an event (manual trigger), so
    // the loop reaches it only via a 10-win streak; a stray loss re-arms defensively.
    for (int i = 0; i < 60000 && !g.subBossUnlocked(area, sub); ++i) {
        switch (g.nav()) {
            case Game::Nav::Idle:
                if (g.exploreActive()) pingExplore(g); else g.debugArmExplore(area, sub);
                break;
            case Game::Nav::Encounter: g.onButton(press(Button::B)); break;  // Fight
            case Game::Nav::Wifi: g.onButton(press(Button::B)); break;
            case Game::Nav::Shop: g.onButton(press(Button::C)); break;
            case Game::Nav::ModShop: g.onButton(press(Button::C)); break;
            case Game::Nav::Combat:
                for (int j = 0; j < 800 &&
                        g.combat().outcome() == Combat::Outcome::Ongoing; ++j)
                    g.tick(t += kHeartbeatMs);
                g.onButton(press(Button::B));
                break;
            default: g.onButton(press(Button::B)); break;
        }
    }
    CHECK(g.subBossUnlocked(area, sub));           // 10-win streak unlocked its boss
    // Trigger THIS sub's boss directly: cleared subs are re-farmable (selectable),
    // so the EXPL "first-selectable" row isn't uniquely the boss-ready frontier —
    // the UI FIGHT-BOSS path is covered by test_expl_nested_list_nav.
    g.debugFightSubBoss(area, sub);                // "> FIGHT BOSS" on the frontier row
    CHECK(g.nav() == Game::Nav::Combat);
    for (int i = 0; i < 40000 && !g.subCleared(area, sub); ++i) {
        if (g.nav() != Game::Nav::Combat) break;
        for (int j = 0; j < 800 &&
                g.combat().outcome() == Combat::Outcome::Ongoing; ++j)
            g.tick(t += kHeartbeatMs);
        g.onButton(press(Button::B));              // advance the round / dismiss
    }
    CHECK(g.subCleared(area, sub));                // its boss beaten → sub-area cleared
}

// Run the armed walk — auto-fighting whatever it rolls, declining storefronts — until
// `done`, or a bound. The walk stopping (a loss cancels explore-mode) ends it too, so a
// caller's CHECK reports the real outcome instead of the loop spinning to its bound.
template <typename F>
static void runWalkUntil(Game& g, F done) {
    uint32_t t = 0;
    for (int i = 0; i < 60000 && !done(); ++i) {
        switch (g.nav()) {
            case Game::Nav::Idle:
                if (!g.exploreActive()) return;    // the walk ended — stop, don't re-arm
                pingExplore(g);
                break;
            case Game::Nav::Combat:
                for (int j = 0; j < 800 &&
                        g.combat().outcome() == Combat::Outcome::Ongoing; ++j)
                    g.tick(t += kHeartbeatMs);
                g.onButton(press(Button::B));
                break;
            case Game::Nav::Shop:
            case Game::Nav::ModShop: g.onButton(press(Button::C)); break;
            default: g.onButton(press(Button::B)); break;
        }
    }
}

// AUTO-PROGRESS steps the ladder POSITIONALLY — the next rung by index, never "the next
// unbeaten one". That is what lets a finished ladder keep rotating, and it means an area
// spliced into the middle of kAreaList joins the rotation with no rule to update.
static void test_auto_progress_steps_positionally() {
    // Mid-area, every sub-area already cleared (so no sub-boss is ever due again and
    // only the positional step can move the walk): the rung after 1 is 2.
    { Game g{StartMode::Hatched, "bruinforce"};
      g.debugAddCombatXp(600000);
      for (int s = 0; s < kExplSubAreas; ++s) g.debugSetSubCleared(0, s, true);
      g.debugSetSectorCleared(0, true);
      g.debugSetAutoProgress(true);
      g.debugArmExplore(0, 1);
      g.debugSetExploreStreak(kExploreStreakToBoss);   // the step condition, met
      runWalkUntil(g, [&] { return g.exploreSub() != 1; });
      CHECK(g.exploreActive() && g.exploreSector() == 0 && g.exploreSub() == 2);
      CHECK(g.exploreStreak() < kExploreStreakToBoss); }  // re-armed → streak reset

    // On the LAST sub-area with the gauntlet not standing (an earlier sub-area is still
    // unbeaten — what arming auto-progress midway through an area leaves behind), the
    // rotation wraps to the area's first rung rather than stalling on a fight it can't
    // start. Coming round again is what eventually makes the gauntlet reachable.
    { Game g{StartMode::Hatched, "bruinforce"};
      g.debugAddCombatXp(600000);
      const int last = kExplSubAreas - 1;
      g.debugSetSubCleared(0, last, true);         // ...but sub-area 0 is not
      g.debugSetAutoProgress(true);
      g.debugArmExplore(0, last);
      g.debugSetExploreStreak(kExploreStreakToBoss);
      runWalkUntil(g, [&] { return g.exploreSub() != last; });
      CHECK(g.exploreActive() && g.exploreSector() == 0 && g.exploreSub() == 0); }
}

// A won gauntlet rolls the rotation on to the next OPEN area instead of relaunching
// itself — the one step the shared hand-back hook can't read off the armed sub-area,
// since "the gauntlet is next" and "the gauntlet just happened" leave the walk standing
// on the same rung. Also proves a CLEARED area's gauntlet is re-runnable at all.
static void test_auto_progress_gauntlet_rolls_to_next_area() {
    Game g{StartMode::Hatched, "bruinforce"};
    g.debugAddCombatXp(600000);                    // level hard: the gauntlet is winnable
    for (int s = 0; s < kExplSubAreas; ++s) g.debugSetSubCleared(0, s, true);
    g.debugSetSectorCleared(0, true);              // area 0 DONE — and so re-runnable
    g.debugSetAutoProgress(true);
    g.debugArmExplore(0, kExplSubAreas - 1);       // the last rung before the gauntlet
    g.debugSetExploreStreak(kExploreStreakToBoss);
    const int bits0 = g.bits();
    runWalkUntil(g, [&] { return g.exploreSector() != 0; });
    CHECK(g.exploreActive());
    CHECK(g.exploreSector() == 1 && g.exploreSub() == 0);   // rolled on, didn't repeat
    CHECK(g.bits() > bits0);                       // the re-run paid its Bits lump
}

// End-to-end: clear all 5 sub-areas of an area (each = a 10-win streak →
// FIGHT BOSS), which unlocks the AREA boss = a 5-stage gauntlet of those sub-area
// bosses; beating that sets sectorCleared[0], unlocks area 1, pays a Bits lump, and
// grants the Title. Uses a heavily-leveled Daemon so the ladder + no-heal gauntlet
// are winnable deterministically (a fresh Process pet is not meant to clear a finale).
static void test_explore_streak_unlocks_boss_then_clears() {
    Game g{StartMode::Hatched, "bruinforce"};
    g.debugAddCombatXp(600000);                    // level hard: enough stat points to clear
    CHECK(!g.sectorCleared(0));
    for (int s = 0; s < kExplSubAreas; ++s) CHECK(!g.subBossUnlocked(0, s));
    { bool fl[kExplSectors] = {g.sectorCleared(0), g.sectorCleared(1)};
      CHECK(!explSectorOpen(1, fl)); }            // area 1 locked at the start

    // Clear every sub-area in order (linear within the area).
    for (int s = 0; s < kExplSubAreas; ++s) {
        clearSubArea(g, 0, s);
        // A sub-area clear does NOT clear the area (that needs the area boss).
        CHECK(!g.sectorCleared(0));
    }
    CHECK(g.areaBossReady(0));                      // all 5 sub-areas cleared → area boss
    const int bits0 = g.bits();

    // Trigger the AREA boss: the 5-stage gauntlet, carried Health, no heal. clearSubArea
    // leaves explore-mode armed on the last sub, so EXPL resumes INSIDE area 0 on that
    // row; A wraps within the area to the boss-ready header and B launches the AREA BOSS.
    uint32_t t = 0;
    enterSubmenuId(g, SubmenuId::Expl);
    g.onButton(press(Button::A));                  // armed sub -> boss-ready header
    g.onButton(press(Button::B));                  // AREA BOSS (boss-ready header)
    CHECK(g.nav() == Game::Nav::Combat);
    for (int i = 0; i < 80000 && !g.sectorCleared(0); ++i) {
        if (g.nav() != Game::Nav::Combat) break;
        for (int j = 0; j < 800 &&
                        g.combat().outcome() == Combat::Outcome::Ongoing; ++j)
            g.tick(t += kHeartbeatMs);
        g.onButton(press(Button::B));              // advance the round / dismiss
    }
    CHECK(g.sectorCleared(0));                     // the 5-stage gauntlet cleared the area
    CHECK(g.bits() > bits0);                       // a Bits lump was paid
    { bool fl[kExplSectors] = {g.sectorCleared(0), g.sectorCleared(1)};
      CHECK(explSectorOpen(1, fl)); }             // area 1 is now unlocked
    // clearing the area grants its Title, and the first one auto-equips.
    CHECK(g.titleUnlocked(0));                      // Citrus Circuit's Title earned
    CHECK(!g.titleUnlocked(1));                     // area 1 not cleared -> not earned
    CHECK(g.equippedTitle() == 0);                 // auto-equipped (was none before)
    CHECK(std::strcmp(g.equippedTitleName(), sectorTitle(0)) == 0);
}

// The nested-list row model as pure functions: row decode, per-state tags,
// and selectability (A skips headers/locked/cleared; the frontier + boss + area-boss
// rows are selectable). Boss-ready takes priority over exploring so FIGHT BOSS stays
// reachable; a cleared area's subs go inert and its header becomes AREA-BOSS ready.
static void test_expl_nested_row_helpers() {
    CHECK(explRowCount() == 1 + kExplSectors * (1 + kExplSubAreas));  // DeepWeb + ladder
    // DeepWeb leads at row 0; the area ladder is offset by +1.
    constexpr int kDwRow = 0;                                    // the DeepWeb Dive row
    CHECK(explRowIsDeepWeb(kDwRow) && !explRowIsDeepWeb(1));
    CHECK(explRowArea(1) == 0 && explRowSub(1) == -1);           // area 0 header
    CHECK(explRowArea(2) == 0 && explRowSub(2) == 0);            // area 0, sub-area 1
    CHECK(explRowArea(7) == 1 && explRowSub(7) == -1);           // area 1 header
    CHECK(explRowArea(12) == 1 && explRowSub(12) == kExplSubAreas - 1);

    constexpr int N = kExplSectors * kExplSubAreas;
    bool cleared[N] = {}, boss[N] = {};
    bool area[kExplSectors] = {};
    // Fresh: area 0 open (all 5 subs OPEN/selectable); area 1 locked (subs LOCKED).
    CHECK(explRowState(1, area, cleared, boss, -1, -1) == ExplRowState::AreaProgress);
    for (int r = 2; r <= 1 + kExplSubAreas; ++r)
        CHECK(explRowState(r, area, cleared, boss, -1, -1) == ExplRowState::SubOpen);
    CHECK(explRowState(7, area, cleared, boss, -1, -1) == ExplRowState::AreaLocked);
    CHECK(explRowState(8, area, cleared, boss, -1, -1) == ExplRowState::SubLocked);
    CHECK(!explRowSelectable(ExplRowState::AreaLocked));
    CHECK(!explRowSelectable(ExplRowState::AreaProgress));
    CHECK(!explRowSelectable(ExplRowState::SubLocked));
    CHECK(explRowSelectable(ExplRowState::SubOpen));

    // Arm sub-area 1 → EXPLORING; unlock its boss → FIGHT BOSS (priority); clear it →
    // CLEARED but RE-FARMABLE. (row 2 = area 0, sub 0; flat index 0.)
    CHECK(explRowState(2, area, cleared, boss, 0, 0) == ExplRowState::SubExploring);
    boss[0] = true;
    CHECK(explRowState(2, area, cleared, boss, 0, 0) == ExplRowState::SubBossReady);
    boss[0] = false; cleared[0] = true;
    CHECK(explRowState(2, area, cleared, boss, -1, -1) == ExplRowState::SubCleared);
    CHECK(explRowSelectable(ExplRowState::SubCleared));   // re-armable to FARM
    // A cleared sub that is the ARMED sub reads EXPLORING (which one you're farming),
    // and cleared wins over a still-set boss-unlock flag (no FIGHT BOSS on a done sub).
    CHECK(explRowState(2, area, cleared, boss, 0, 0) == ExplRowState::SubExploring);
    boss[0] = true;
    CHECK(explRowState(2, area, cleared, boss, -1, -1) == ExplRowState::SubCleared);
    boss[0] = false;

    // All 5 sub-areas cleared → area 0 header is AREA-BOSS ready (selectable).
    for (int s = 0; s < kExplSubAreas; ++s) cleared[s] = true;
    CHECK(explRowState(1, area, cleared, boss, -1, -1) == ExplRowState::AreaBossReady);
    CHECK(explRowSelectable(ExplRowState::AreaBossReady));
    // Clear the area → header CLEARED (inert); area 1 opens (its subs become OPEN).
    // The area-0 sub rows stay CLEARED but SELECTABLE — a fully-cleared area is still a
    // farmable training ground (the whole point of re-farming).
    area[0] = true;
    CHECK(explRowState(1, area, cleared, boss, -1, -1) == ExplRowState::AreaCleared);
    CHECK(explRowState(2, area, cleared, boss, -1, -1) == ExplRowState::SubCleared);
    CHECK(explRowSelectable(explRowState(2, area, cleared, boss, -1, -1)));
    CHECK(explRowState(7, area, cleared, boss, -1, -1) == ExplRowState::AreaProgress);
    CHECK(explRowState(8, area, cleared, boss, -1, -1) == ExplRowState::SubOpen);

    // DeepWeb Dive (row 0): LOCKED until EVERY area is cleared, then OPEN
    // (> DIVE, selectable); DIVING when armed (exploringSector == kDeepWebSector).
    CHECK(explRowState(kDwRow, area, cleared, boss, -1, -1) == ExplRowState::DeepWebLocked);
    CHECK(!explRowSelectable(ExplRowState::DeepWebLocked));
    for (int a = 0; a < kExplSectors; ++a) area[a] = true;       // beat the whole game
    CHECK(explRowState(kDwRow, area, cleared, boss, -1, -1) == ExplRowState::DeepWebOpen);
    CHECK(explRowSelectable(ExplRowState::DeepWebOpen));
    CHECK(explRowState(kDwRow, area, cleared, boss, kDeepWebSector, 0) ==
          ExplRowState::DeepWebDiving);
    CHECK(explRowSelectable(ExplRowState::DeepWebDiving));
}

// The AREA boss is a 5-stage gauntlet of the area's sub-area bosses, in
// order, with Health climbing to the signature apex (round 5).
static void test_area_boss_gauntlet_composition() {
    for (int a = 0; a < kExplSectors; ++a) {
        BossGauntlet gg = areaBoss(a);
        CHECK(static_cast<int>(gg.rounds.size()) == kExplSubAreas);   // 5 stages
        CHECK(gg.stageRank >= 2);
        for (int s = 0; s < kExplSubAreas; ++s)
            CHECK(std::strcmp(gg.rounds[s].name, subAreaBoss(a, s).rounds[0].name) == 0);
        CHECK(gg.rounds[kExplSubAreas - 1].maxHealth > gg.rounds[0].maxHealth);  // apex
    }
}

// A THREAT rider is declared on the area's OWN row (AreaDef::apexThreatMoveId) and
// reaches a player on exactly one enemy: that area's SIGNATURE sub-boss (sub 4), where it
// is telegraphed and matched by the counter-mod that area's loot table pays out. Walked
// off the ladder rather than named per index, so splicing an area in carries every rider
// with it instead of failing here.
static void test_boss_threat_moves_area_adjacent() {
    auto hasMove = [](const BossGauntlet& g, const char* id) {
        for (const char* m : g.rounds[0].moveIds)
            if (std::strcmp(m, id) == 0) return true;
        return false;
    };
    const int sig = kExplSubAreas - 1;
    int riders = 0, unarmed = 0;
    for (int a = 0; a < kExplSectors; ++a) {
        const char* rider = area(a).apexThreatMoveId;
        if (!rider) { ++unarmed; continue; }         // an area may debut no threat
        ++riders;
        CHECK(hasMove(subAreaBoss(a, sig), rider));  // on its own signature...
        CHECK(!hasMove(subAreaBoss(a, 0), rider));   // ...and no earlier sub of it
        for (int b = 0; b < kExplSectors; ++b) {     // nor any other area's signature:
            if (b == a) continue;                    // areas never cross threats
            CHECK(!hasMove(subAreaBoss(b, sig), rider));
            CHECK(area(b).apexThreatMoveId == nullptr ||
                  std::strcmp(area(b).apexThreatMoveId, rider) != 0);  // one rider, one area
        }
    }
    // Both shapes are actually represented, or the loop above proves nothing: at least one
    // rung declares no rider (the entry area, where a telegraph would have no counter to
    // buy yet) and most declare one.
    CHECK(unarmed >= 1);
    CHECK(riders >= 3);
}

// Two-level nested-list nav: the TOP level lands on the DeepWeb
// row + area HEADERS; B on an area DRILLS in; INSIDE, A cycles that area's subs (+ the
// boss-ready header) and B acts (arm / FIGHT BOSS / AREA BOSS); C pops back to the area
// list. The rendered page is unchanged — only the traversal is two-level.
static void test_expl_nested_list_nav() {
    // Drill into area 0, then A-cycle its subs: sub 0 cleared (FARMABLE), sub 1 boss-ready,
    // subs 2-4 open. Inside, firstLandable = sub 0; two A's reach sub 2; B arms it.
    { Game g{StartMode::Hatched, "bruinforce"};
      g.debugSetSubCleared(0, 0, true);
      g.debugSetSubBossUnlocked(0, 1, true);
      enterSubmenuId(g, SubmenuId::Expl);
      g.onButton(press(Button::B));                 // drill into area 0 (cursor -> sub 0)
      g.onButton(press(Button::A));                 // farm sub 0 → boss-ready sub 1
      g.onButton(press(Button::A));                 // sub 1 → OPEN sub 2
      g.onButton(press(Button::B));                 // arm explore on sub 2 (OPEN)
      CHECK(g.nav() == Game::Nav::Idle);
      CHECK(g.exploreActive() && g.exploreSector() == 0 && g.exploreSub() == 2); }

    // A cleared sub-area is re-armable to FARM. All 5 cleared → drilling in
    // lands on the boss-ready header; A steps to cleared sub 0, B arms it to FARM.
    { Game g{StartMode::Hatched, "bruinforce"};
      for (int s = 0; s < kExplSubAreas; ++s) g.debugSetSubCleared(0, s, true);
      enterSubmenuId(g, SubmenuId::Expl);
      g.onButton(press(Button::B));                 // drill in (cursor -> boss-ready header)
      g.onButton(press(Button::A));                 // AREA BOSS header → cleared sub 0
      g.onButton(press(Button::B));                 // arm explore to FARM sub 0
      CHECK(g.nav() == Game::Nav::Idle);
      CHECK(g.exploreActive() && g.exploreSector() == 0 && g.exploreSub() == 0); }

    // B on a boss-ready frontier launches the SUB-AREA boss (Combat, not arm-explore).
    { Game g{StartMode::Hatched, "bruinforce"};
      g.debugSetSubBossUnlocked(0, 0, true);
      enterSubmenuId(g, SubmenuId::Expl);
      g.onButton(press(Button::B));                 // drill into area 0 (cursor -> sub 0)
      g.onButton(press(Button::B));                 // FIGHT BOSS (sub 0)
      CHECK(g.nav() == Game::Nav::Combat); }

    // All 5 sub-areas cleared → drilling in lands on the boss-ready header; B launches it.
    { Game g{StartMode::Hatched, "bruinforce"};
      for (int s = 0; s < kExplSubAreas; ++s) g.debugSetSubCleared(0, s, true);
      CHECK(g.areaBossReady(0));
      enterSubmenuId(g, SubmenuId::Expl);
      g.onButton(press(Button::B));                 // drill into area 0
      g.onButton(press(Button::B));                 // AREA BOSS
      CHECK(g.nav() == Game::Nav::Combat); }

    // TOP level cycles whole AREAS (not subs): clear area 0 so area 1 opens; A steps from
    // the area-0 header to the area-1 header (a whole area), and drilling+arming lands in
    // area 1 — proving A skipped area 0's five sub-rows.
    { Game g{StartMode::Hatched, "bruinforce"};
      g.debugSetSectorCleared(0, true);             // area 0 cleared → area 1 now open
      enterSubmenuId(g, SubmenuId::Expl);
      g.onButton(press(Button::A));                 // TOP: area-0 header → area-1 header
      g.onButton(press(Button::B));                 // drill into area 1
      g.onButton(press(Button::B));                 // arm area 1's first sub
      CHECK(g.exploreActive() && g.exploreSector() == 1 && g.exploreSub() == 0); }

    // C is two-level: inside an area it pops back to the area list (still in EXPL); a
    // second C at the top level leaves EXPL for the carousel.
    { Game g{StartMode::Hatched, "bruinforce"};
      enterSubmenuId(g, SubmenuId::Expl);
      g.onButton(press(Button::B));                 // drill into area 0
      g.onButton(press(Button::C));                 // C → back to the TOP area list
      CHECK(g.nav() == Game::Nav::Submenu);         // still in EXPL, not the carousel
      g.onButton(press(Button::C));                 // C at TOP → leave EXPL
      CHECK(g.nav() == Game::Nav::Cursor); }

    // Opening EXPL while explore-mode is RUNNING RESUMES where the pet is — already
    // drilled into the armed area, cursor already on the armed sub-area. Checking or
    // changing the current walk is why the list gets opened mid-run, so it costs no
    // presses: a single B acts on that sub (re-arms it) rather than drilling anywhere.
    { Game g{StartMode::Hatched, "bruinforce"};
      g.debugArmExplore(0, 3);
      enterSubmenuId(g, SubmenuId::Expl);
      g.onButton(press(Button::B));
      CHECK(g.nav() == Game::Nav::Idle);            // acted, not drilled
      CHECK(g.exploreActive() && g.exploreSector() == 0 && g.exploreSub() == 3); }

    // A CLEARED area's gauntlet stays re-runnable, the way a cleared sub-area stays
    // re-farmable: inside the area, B on its boss row starts the 5-round gauntlet again.
    { Game g{StartMode::Hatched, "bruinforce"};
      for (int s = 0; s < kExplSubAreas; ++s) g.debugSetSubCleared(0, s, true);
      g.debugSetSectorCleared(0, true);
      enterSubmenuId(g, SubmenuId::Expl);
      g.onButton(press(Button::B));                 // drill into the cleared area 0
      g.onButton(press(Button::B));                 // B on its boss row → RERUN
      CHECK(g.nav() == Game::Nav::Combat); }

    // AUTO-PROGRESS is armed by the SAME chord that opens the explore-control overlay:
    // A+C from the habitat opens it, A+C again toggles the mode, and the overlay's three
    // single-press actions are untouched. It is off until asked for.
    { Game g{StartMode::Hatched, "bruinforce"};
      g.debugArmExplore(0, 0);
      CHECK(!g.autoProgress());
      g.onButton(chordAC());                        // habitat → the control overlay
      CHECK(g.nav() == Game::Nav::ExploreControl);
      g.onButton(chordAC());                        // ...again → arm auto-progress
      CHECK(g.autoProgress());
      CHECK(g.nav() == Game::Nav::ExploreControl);  // still on the overlay
      g.onButton(chordAC());
      CHECK(!g.autoProgress());                     // and it toggles back off
      g.onButton(press(Button::C));                 // C still stops the walk
      CHECK(!g.exploreActive()); }

    // The DeepWeb dive resumes at the TOP level instead — its row lives there, not
    // inside any area — so one B re-arms the dive.
    { Game g{StartMode::Hatched, "bruinforce"};
      for (int a = 0; a < kExplSectors; ++a) g.debugSetSectorCleared(a, true);
      g.debugStartDeepWebDive();
      enterSubmenuId(g, SubmenuId::Expl);
      g.onButton(press(Button::B));
      CHECK(g.nav() == Game::Nav::Idle);
      CHECK(g.exploreActive() && g.exploreSector() == kDeepWebSector); }
}

// the DEEPWEB DIVE endless zone: unlocked only by clearing every area,
// arms an endless explore mode on kDeepWebSector, and scales the wild enemy to the
// PET's level (parity → full base XP forever; no boss ladder).
static void test_deepweb_dive() {
    // (1) The pure scaler: enemy level = pet level (parity), Health climbs per level,
    //     and parity level makes wildWinXp pay the FULL base (endless steady XP).
    {
        CombatEnemy e = wildMalbeast(3, 0);
        const int baseHp = e.maxHealth, baseSpd = e.speed;
        applyDeepWebScale(e, 10);
        CHECK(e.level == 10 + kDeepWebEnemyLevelOffset);
        CHECK(e.maxHealth == baseHp + 10 * kDeepWebHealthPerLevel);
        CHECK(e.speed == baseSpd + 10 / kDeepWebSpeedPerNLevels);
        CHECK(wildWinXp(kWildWinXpReward, e.level, 10) == kWildWinXpReward);   // parity
        CombatEnemy e2 = wildMalbeast(3, 0);
        applyDeepWebScale(e2, -5);                    // petLevel clamps at 0
        CHECK(e2.level == 0 + kDeepWebEnemyLevelOffset);
    }
    // (1b) Depth ramp: a deep win-streak folds in as a logarithmic bonus effective
    //      level, so the enemy outlevels the pet and wildWinXp pays a bonus.
    {
        CombatEnemy e = wildMalbeast(3, 0);
        applyDeepWebScale(e, 10, 0);
        CHECK(e.level == 10);                          // depth=0 -> unchanged parity
        CombatEnemy e3 = wildMalbeast(3, 0);
        applyDeepWebScale(e3, 10, 7);                   // floorLog2(8) = 3
        CHECK(e3.level == 10 + 3 * kDeepWebDepthLevelPerLog2);
        CHECK(wildWinXp(kWildWinXpReward, e3.level, 10) > kWildWinXpReward);  // punches up
        CombatEnemy e4 = wildMalbeast(3, 0);
        applyDeepWebScale(e4, 10, -3);                  // depth clamps at 0
        CHECK(e4.level == 10);
    }
    // (1c) Depth ramp, Bits half: mirrors the same logarithmic curve onto the Bits
    //      payout pct, since diffPips-keyed Bits don't move with the level bonus above.
    {
        CHECK(deepWebDepthBitsPct(0) == 100);              // fresh dive: unchanged
        CHECK(deepWebDepthBitsPct(-9) == 100);              // clamps at 0
        CHECK(deepWebDepthBitsPct(7) == 100 + 3 * kDeepWebDepthBitsPctPerLog2);  // log2(8)=3
        CHECK(deepWebDepthBitsPct(1 << 30) == kDeepWebDepthBitsMaxPct);  // ceiling holds
    }
    // (2) Unlock gating: locked until EVERY area is cleared; startDeepWebDive is inert.
    {
        Game g{StartMode::Hatched, "bruinforce"};
        CHECK(!g.allSectorsCleared());
        g.debugStartDeepWebDive();
        CHECK(!g.inDeepWebDive());                    // inert while the game isn't beaten
        for (int a = 0; a < kExplSectors; ++a) g.debugSetSectorCleared(a, true);
        CHECK(g.allSectorsCleared());
        g.debugStartDeepWebDive();
        CHECK(g.inDeepWebDive());
        CHECK(g.exploreActive() && g.exploreSector() == kDeepWebSector);
    }
    // (3) In-game scaling: a dive encounter stamps the enemy at the pet's level (parity),
    //     so a high-level pet still meets a level-matched foe (full-XP endless grind).
    {
        Game g{StartMode::Hatched, "bruinforce"};
        for (int a = 0; a < kExplSectors; ++a) g.debugSetSectorCleared(a, true);
        g.debugAddCombatXp(600000);                  // level the pet up high
        const int L = g.combatLevel();
        CHECK(L > 3);
        g.debugStartDeepWebDive();
        uint32_t t = 0; int guard = 0;
        while (g.nav() != Game::Nav::Combat && guard++ < 400) {
            switch (g.nav()) {
                case Game::Nav::Idle:
                    if (g.inDeepWebDive()) pingExplore(g); else guard = 400; break;
                case Game::Nav::Wifi: g.onButton(press(Button::B)); break;
                case Game::Nav::Shop: g.onButton(press(Button::C)); break;
                case Game::Nav::ModShop: g.onButton(press(Button::C)); break;
                default: g.onButton(press(Button::C)); break;
            }
            (void)t;
        }
        CHECK(g.nav() == Game::Nav::Combat);          // a wild dive fight started
        CHECK(g.debugEncounterEnemyLevel() == L + kDeepWebEnemyLevelOffset);  // scaled to pet
    }
}

// Combat::begin's forceEnemyFirst override (retained combat-engine mechanic):
// even a much faster player is forced to act second, independent of speed.
static void test_combat_force_enemy_first() {
    ContentRegistry r = ContentRegistry::embedded();
    MoveLoadout mv = MoveLoadout::starting();
    Loadout md = Loadout::starting();
    const CreatureDef* pet = r.creature("paypup");
    CHECK(pet != nullptr);
    Combatant p = makePlayerCombatant(r, *pet, mv, md);
    p.speed = 999;                                        // would normally act first
    Combatant e = makeEnemyCombatant(r, wildMalbeast(1));
    Combat c;
    c.begin(p, e, Combat::Stakes::Live, 4242, /*forceEnemyFirst=*/true);
    CHECK(!c.playerActsFirst());
    CHECK(!c.playerTurnNext());
}

// A held Sinkhole Trap auto-bypasses a wild encounter in the hands-off auto mode
// the wild is converted into an XP lump and the trap consumed — no fight,
// no Fight/Flee intro, straight back to the idle habitat. A bounded search steps
// explore until a wild is typed (observed as the trap being spent), dismissing any
// other event along the way. (An awakened-guardian Wi-Fi fight still fights — the
// sinkhole only bypasses the explore-table wild.)
static void test_encounter_sinkhole_bypass() {
    Game g{StartMode::Hatched};
    g.inventory().add("sinkhole_trap", 1);
    const int xp0 = g.combatXp();
    enterWalk(g);
    uint32_t t = 0;
    bool bypassed = false;
    for (int i = 0; i < 400 && !bypassed; ++i) {
        switch (g.nav()) {
            case Game::Nav::Idle:
                if (g.exploreActive()) pingExplore(g); else enterWalk(g);
                break;
            case Game::Nav::Wifi: g.onButton(press(Button::B)); break;
            case Game::Nav::Shop: g.onButton(press(Button::C)); break;
            case Game::Nav::ModShop: g.onButton(press(Button::C)); break;
            case Game::Nav::Combat:
                for (int j = 0; j < 400 &&
                        g.combat().outcome() == Combat::Outcome::Ongoing; ++j)
                    g.tick(t += kHeartbeatMs);
                g.onButton(press(Button::B));
                break;
            default: g.onButton(press(Button::B)); break;
        }
        if (g.inventory().count("sinkhole_trap") == 0) bypassed = true;
    }
    CHECK(bypassed);                                      // the wild auto-bypassed
    CHECK(g.nav() == Game::Nav::Idle);                    // no fight, back to the habitat
    CHECK(g.combatXp() ==                                 // converted to an XP lump
          xp0 + ContentRegistry::embedded().item("sinkhole_trap")->preEncounterXp);
    CHECK(g.inventory().count("sinkhole_trap") == 0);     // trap consumed
}

// Regression: resolveSinkhole() persists immediately, like every other XP grant
// (finishCombat/finishBossRound) — it's the one XP source that bypasses combat
// entirely, so nothing else forces a flush. Before the fix it only markSaveDirty()'d,
// so an XP/level gain (and the trap's consumption) could vanish on a reboot that lands
// before the next debounced autosave.
static void test_sinkhole_xp_persists_immediately() {
    MemSaveStore store;
    int xp = 0, lvl = 0;
    {
        Game g{StartMode::Hatched, "paypup", &store};
        g.inventory().add("sinkhole_trap", 1);
        enterWalk(g);
        uint32_t t = 0;
        bool bypassed = false;
        for (int i = 0; i < 400 && !bypassed; ++i) {
            switch (g.nav()) {
                case Game::Nav::Idle:
                    if (g.exploreActive()) pingExplore(g); else enterWalk(g);
                    break;
                case Game::Nav::Wifi: g.onButton(press(Button::B)); break;
                case Game::Nav::Shop: g.onButton(press(Button::C)); break;
                case Game::Nav::ModShop: g.onButton(press(Button::C)); break;
                case Game::Nav::Combat:
                    for (int j = 0; j < 400 &&
                            g.combat().outcome() == Combat::Outcome::Ongoing; ++j)
                        g.tick(t += kHeartbeatMs);
                    g.onButton(press(Button::B));
                    break;
                default: g.onButton(press(Button::B)); break;
            }
            if (g.inventory().count("sinkhole_trap") == 0) bypassed = true;
        }
        CHECK(bypassed);
        xp = g.combatXp();
        lvl = g.combatLevel();
        // No tick() here — a real reboot moments after the item resolves would land
        // well inside the kSaveAutosaveMs debounce window, so nothing else must be
        // relied on to flush this XP grant to the store.
    }
    Game g{StartMode::Hatched, "paypup", &store};
    CHECK(g.combatXp() == xp);
    CHECK(g.combatLevel() == lvl);
}

// ===========================================================================
// The Wi-Fi network explore event: a self-contained
// typed event alongside Quiet/Loot/Wild that rolls one of 4 sub-outcomes
// (sleeping guardian -> loot, awakened guardian -> wild combat, open cache ->
// loot, friendly visit -> a v1 generic ally-buff substitute for the doc-07
// Met-Pets Roster) and a lifetime networks-seen counter (dedup'd, —
// the seed for Hacker Rank, not yet consumed this milestone).
// ===========================================================================

// Walks with a stack of Sinkhole Traps in hand (so any Wild encounter rolled
// along the way bypasses for free, no rng/steps cost) until the periodic roll
// types a Wi-Fi network event. A bounded search rather than hand-tracing the
// fixed-seed roll sequence — robust to any later rebalance of the event-type
// weights — mirroring dump_frame's own "wifi" flag.
static void walkToWifiEvent(Game& g) {
    g.inventory().add("sinkhole_trap", 20);
    enterWalk(g);
    uint32_t t = 0;
    for (int i = 0; i < 400 && g.nav() != Game::Nav::Wifi; ++i) {
        if (g.nav() == Game::Nav::Encounter) {
            g.onButton(press(Button::A));   // Fight -> Flee
            g.onButton(press(Button::A));   // Flee -> Sinkhole
            g.onButton(press(Button::B));   // confirm -> back to idle
        } else if (g.nav() == Game::Nav::Shop || g.nav() == Game::Nav::ModShop) {
            g.onButton(press(Button::C));   // leave the shop -> back to idle
        } else if (g.nav() == Game::Nav::Combat) {
            // An awakened-guardian fight — ride it out + dismiss so the Wi-Fi
            // search keeps going.
            for (int j = 0; j < 800 &&
                            g.combat().outcome() == Combat::Outcome::Ongoing; ++j)
                g.tick(t += kHeartbeatMs);
            g.onButton(press(Button::B));
        } else if (g.nav() == Game::Nav::PostEncounter) {
            g.onButton(press(Button::B));   // that fight's status readout -> dismiss
        } else if (g.nav() == Game::Nav::Idle) {
            if (g.exploreActive()) pingExplore(g); else enterWalk(g);
        }
    }
}

// Same search, but keeps re-rolling (resolving each non-matching outcome via
// B, or riding out+dismissing an awakened-guardian fight) until the specific
// sub-outcome `want` comes up, leaving the game AT Nav::Wifi with it unresolved
// — so a test can then press B itself and assert the resolution.
static void walkToWifiOutcome(Game& g, Game::WifiOutcome want) {
    for (int tries = 0; tries < 50 && g.nav() != Game::Nav::Wifi; ++tries) {
        walkToWifiEvent(g);
        if (g.nav() != Game::Nav::Wifi) return;   // search exhausted (shouldn't happen)
        if (g.wifiOutcome() == want) return;
        g.onButton(press(Button::B));             // wrong outcome — resolve, keep looking
        if (g.nav() == Game::Nav::Combat) {        // an awakened-guardian miss
            uint32_t t = 0;
            for (int i = 0; i < 400 && g.combat().outcome() == Combat::Outcome::Ongoing; ++i)
                g.tick(t += kHeartbeatMs);
            g.onButton(press(Button::B));          // dismiss -> back to the Walk
        }
    }
}

// Queues `bssid` and walks to the next Wi-Fi EXPL event (asserting it's
// reached), leaving the game AT Nav::Wifi with resolveNetworkDiscovery's own
// effects (credit / XP / Happiness / reward) ALREADY applied but the
// UNRELATED guardian/cache/friendly roll (startWifiEvent's other half, see
// game.h's comment on it) still unresolved. A test that cares about isolating
// resolveNetworkDiscovery's effects from that roll's own possible Bits/combat-
// XP side effects (an awakened-guardian win grants combat XP; a loot outcome
// can grant Bits) should snapshot state here, BEFORE calling
// resolveWifiEventToIdle below.
static void queueAndReachWifiEvent(Game& g, const uint8_t* bssid, const char* ssid) {
    CHECK(g.registerNetwork(bssid, ssid));
    walkToWifiEvent(g);
    CHECK(g.nav() == Game::Nav::Wifi);
}

// Resolves whatever guardian/cache/friendly outcome the current Wi-Fi event
// rolled (riding out combat if awakened) back to Idle.
static void resolveWifiEventToIdle(Game& g) {
    g.onButton(press(Button::B));
    if (g.nav() == Game::Nav::Combat) {
        uint32_t t = 0;
        for (int j = 0; j < 400 && g.combat().outcome() == Combat::Outcome::Ongoing; ++j)
            g.tick(t += kHeartbeatMs);
        g.onButton(press(Button::B));
    }
}

// Queues + walks + resolves in one step — for tests that don't need to isolate
// the moment between resolveNetworkDiscovery's own effects and the guardian/
// cache/friendly roll's (see queueAndReachWifiEvent's comment for why that
// moment sometimes matters).
static void walkAndCreditNetwork(Game& g, const uint8_t* bssid, const char* ssid) {
    queueAndReachWifiEvent(g, bssid, ssid);
    resolveWifiEventToIdle(g);
}

// The periodic roll can type a Wi-Fi network event (not just Quiet/Loot/Wild)
// a decision-free, self-contained screen rendering the sector header
// unchanged + the rolled sub-outcome's flavor line + a B-continue hint band.
static void test_wifi_event_reached() {
    Game g{StartMode::Hatched};
    walkToWifiEvent(g);
    CHECK(g.nav() == Game::Nav::Wifi);
    Framebuffer fb(kActiveW, kActiveH);
    g.render(fb);
    CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));

    // Like Walk/Encounter/Combat, it's an activity screen beyond the 3-deep
    // menu budget — the global 5s auto-defocus must stay suspended while it's
    // up (analogy), not silently drop the player out mid-event.
    g.tick(kAutoDefocusMs + kHeartbeatMs);
    CHECK(g.nav() == Game::Nav::Wifi);
}

// A real network only credits the lifetime networks-seen counter once the EXPL
// Wi-Fi event actually resolves it (never at registerNetwork/scan time), and
// only for a GENUINELY new BSSID — never decreasing, never re-crediting a
// repeat, and never crediting anything when nothing was queued.
static void test_wifi_networks_seen_counter() {
    Game g{StartMode::Hatched};
    CHECK(g.networksSeen() == 0);

    // Nothing queued yet: the event resolves to the empty-queue outcome, no credit.
    walkToWifiEvent(g);
    CHECK(g.nav() == Game::Nav::Wifi);
    CHECK(g.networksSeen() == 0);
    g.onButton(press(Button::B));
    if (g.nav() == Game::Nav::Combat) {
        uint32_t t = 0;
        for (int j = 0; j < 400 && g.combat().outcome() == Combat::Outcome::Ongoing; ++j)
            g.tick(t += kHeartbeatMs);
        g.onButton(press(Button::B));
    }

    // Queue a real sighting: the NEXT Wi-Fi event resolves it as genuinely new.
    const uint8_t bssid[6] = {0x30, 0x40, 0x50, 0x60, 0x70, 0x80};
    walkAndCreditNetwork(g, bssid, "Neighbour");
    CHECK(g.networksSeen() == 1);

    // Re-queue the SAME BSSID: the ledger already knows it, so the next event
    // resolves it as a repeat — never a second credit.
    walkAndCreditNetwork(g, bssid, "Neighbour");
    CHECK(g.networksSeen() == 1);
}

// Loops the Walk, auto-resolving whatever the periodic roll types (Fight on
// any Wild encounter, continue through any Wi-Fi event), until it lands in
// real combat — either path (a Fight or an awakened Wi-Fi guardian) goes
// through buildPlayerCombatant, so this is the shared way to reach a REAL
// battle entry (as opposed to the debugStartCombat dev/test hook, which
// deliberately stays raw/unbuffed).
static void walkToAnyCombat(Game& g) {
    uint32_t t = 0;
    for (int i = 0; i < 2000 && g.nav() != Game::Nav::Combat; ++i) {
        if (g.nav() == Game::Nav::Idle) {
            if (g.exploreActive()) pingExplore(g); else enterWalk(g);
        } else if (g.nav() == Game::Nav::Encounter) {
            g.onButton(press(Button::B));   // default choice 0 = Fight
        } else if (g.nav() == Game::Nav::Wifi) {
            g.onButton(press(Button::B));   // resolve whatever it is
        } else if (g.nav() == Game::Nav::Shop || g.nav() == Game::Nav::ModShop) {
            g.onButton(press(Button::C));   // leave the shop -> back to idle
        } else if (g.nav() == Game::Nav::PostEncounter) {
            g.onButton(press(Button::B));   // a prior fight's status readout -> dismiss
        }
    }
}

// Sleeping guardian -> sneak the cache uncontested: the reward-pool draw
// (Bits, same path as a loot cache) with no fight and no Fragmentation risk.
static void test_wifi_sleeping_guardian_grants_loot() {
    Game g{StartMode::Hatched};
    walkToWifiOutcome(g, Game::WifiOutcome::SleepingGuardian);
    CHECK(g.nav() == Game::Nav::Wifi);
    CHECK(g.wifiOutcome() == Game::WifiOutcome::SleepingGuardian);
    const int bits0 = g.bits();
    g.onButton(press(Button::B));
    CHECK(g.nav() == Game::Nav::Idle);
    CHECK(g.bits() == bits0 + kLootBitsReward);
}

// Open cache -> the same straight reward-pool draw, no guardian at all.
static void test_wifi_open_cache_grants_loot() {
    Game g{StartMode::Hatched};
    walkToWifiOutcome(g, Game::WifiOutcome::OpenCache);
    const int bits0 = g.bits();
    g.onButton(press(Button::B));
    CHECK(g.nav() == Game::Nav::Idle);
    CHECK(g.bits() == bits0 + kLootBitsReward);
}

// Drive the audit-capture SM to Hot ("broadcasting") — the only signal that
// flags the player as a target. Toggle on (-> Armed) then a captured
// handshake (-> Hot).
static void makeHot(Game& g) {
    g.setAuditCaptureEnabled(true);                  // -> Armed
    const uint8_t bssid[6] = {0xde, 0xad, 0xbe, 0xef, 0x00, 0x01};
    g.registerHandshake(bssid);                      // Armed -> Hot
    CHECK(g.auditCapture().broadcasting());
}

// Awakened guardian routes straight into the shared wild-combat path with LIVE
// stakes ("reuse startWildCombat/Nav::Combat") — no Fight/Flee/Sinkhole
// intro, the guardian is already hostile. A guardian only wakes when the player
// is HOT: drive the SM hot up front, then search. The walk helpers keep
// the game clock low (each resets a local tick base at 0), so nowMs stays well
// under the ~2-min hot window and broadcasting() persists across the whole search.
static void test_wifi_awakened_guardian_enters_combat() {
    Game g{StartMode::Hatched};
    makeHot(g);
    walkToWifiOutcome(g, Game::WifiOutcome::AwakenedGuardian);
    CHECK(g.auditCapture().broadcasting());          // still hot at the decision point
    CHECK(g.nav() == Game::Nav::Wifi);
    CHECK(g.wifiOutcome() == Game::WifiOutcome::AwakenedGuardian);
    g.onButton(press(Button::B));
    CHECK(g.nav() == Game::Nav::Combat);
    CHECK(g.combat().stakes() == Combat::Stakes::Live);
}

// The gate: merely DISCOVERING networks never flags the player hot, so a
// non-hot player is never auto-attacked by a Wi-Fi event — over a large sample of
// Wi-Fi events, none awaken a guardian and none route into combat. (Wild-step
// combats reached separately are the expected majority path — not our concern here.)
static void test_wifi_never_awakens_when_not_hot() {
    Game g{StartMode::Hatched};
    CHECK(!g.auditCapture().broadcasting());         // not hot: no capture done
    int wifiEvents = 0;
    for (int k = 0; k < 80 && wifiEvents < 40; ++k) {
        walkToWifiEvent(g);                          // robust: reaches a Wi-Fi screen
        if (g.nav() != Game::Nav::Wifi) break;       // search exhausted (shouldn't happen)
        CHECK(!g.auditCapture().broadcasting());     // still not hot
        CHECK(g.wifiOutcome() != Game::WifiOutcome::AwakenedGuardian);
        ++wifiEvents;
        g.onButton(press(Button::B));                // a peaceful outcome -> back to idle
        CHECK(g.nav() != Game::Nav::Combat);         // never a fight from a Wi-Fi event
    }
    CHECK(wifiEvents >= 20);                          // sampled enough to trust the gate
}

// Friendly visit grants a transient ally buff for the next 3 battles (
// ) — the v1 substitute for the doc-07 Met-Pets Roster: it boosts
// the player's combat power (buildPlayerCombatant) rather than naming a
// remembered pet, consumed one battle at a time regardless of outcome.
static void test_wifi_friendly_visit_grants_ally_buff() {
    Game g{StartMode::Hatched};
    walkToWifiOutcome(g, Game::WifiOutcome::FriendlyVisit);
    CHECK(g.allyBuffBattlesLeft() == 0);
    g.onButton(press(Button::B));
    CHECK(g.nav() == Game::Nav::Idle);
    CHECK(g.allyBuffBattlesLeft() == kAllyBuffBattles);

    walkToAnyCombat(g);                                  // a REAL battle entry
    CHECK(g.nav() == Game::Nav::Combat);
    CHECK(g.allyBuffBattlesLeft() == kAllyBuffBattles - 1);
}

// Hands-off Wi-Fi reveal: with no button press the rolled outcome auto-
// plays out after the ~3s reveal hold — here a sleeping guardian's free loot — and
// hands back to the idle habitat so exploration keeps running. Zero presses.
static void test_wifi_auto_plays_out_after_hold() {
    Game g{StartMode::Hatched};
    walkToWifiOutcome(g, Game::WifiOutcome::SleepingGuardian);
    CHECK(g.nav() == Game::Nav::Wifi);
    const int bits0 = g.bits();
    uint32_t t = g.lifetimeUptimeMs();                   // continue from the game's clock
    // Well within the reveal hold: still parked on the reveal.
    for (int i = 0; i < kExploreRevealHoldBeats / 3; ++i) g.tick(t += kHeartbeatMs);
    CHECK(g.nav() == Game::Nav::Wifi);
    // Past the hold: auto-plays out (stop the instant it leaves the reveal, before the
    // idle auto-stepper can type the next event).
    bool played = false;
    for (int i = 0; i < kExploreRevealHoldBeats + 4 && !played; ++i) {
        g.tick(t += kHeartbeatMs);
        if (g.nav() != Game::Nav::Wifi) played = true;
    }
    CHECK(played);
    CHECK(g.nav() == Game::Nav::Idle);                   // auto-resolved, no presses
    CHECK(g.bits() == bits0 + kLootBitsReward);          // the free loot landed
}

// ===========================================================================
// Backlog / — shops as explore events + the two shop consumables.
// A storefront is a self-contained walk event (like the Wi-Fi event): one item,
// restocked + priced per that area's own shop def (src/core/content/areas/). Byte
// to Eat (Citrus Circuit -> Null Noodles) and Pier-to-Peer (The Pirate Bayou ->
// R007_B33R). The two items break "+Hunger = fills" in opposite ways.
// ===========================================================================

// Bounded search: walk until the periodic roll types a storefront shop event,
// leaving the game AT Nav::Shop. Dismisses any other typed event (Wild via a
// Sinkhole Trap, Wi-Fi via B, boss via ride-out) — robust to the weight mix.
// `stopAt` is the shop nav (Shop or ModShop) this walk is trying to reach; landing
// in the OTHER shop kind just leaves it (C) so the walk keeps stepping instead of
// getting stuck there.
static void walkToShopNav(Game& g, Game::Nav stopAt) {
    g.inventory().add("sinkhole_trap", 60);   // bypass Wild encounters, free
    enterWalk(g);
    uint32_t t = 0;
    for (int i = 0; i < 800 && g.nav() != stopAt; ++i) {
        switch (g.nav()) {
            case Game::Nav::Idle:
                if (g.exploreActive()) pingExplore(g); else enterWalk(g);
                break;
            case Game::Nav::Encounter:
                g.onButton(press(Button::A));   // Fight -> Flee
                g.onButton(press(Button::A));   // Flee -> Sinkhole
                g.onButton(press(Button::B));   // confirm -> back to idle
                break;
            case Game::Nav::Wifi: g.onButton(press(Button::B)); break;
            case Game::Nav::Shop:
            case Game::Nav::ModShop:
                g.onButton(press(Button::C));   // the OTHER shop kind -> leave, keep stepping
                break;
            case Game::Nav::Combat:
                for (int j = 0; j < 800 &&
                        g.combat().outcome() == Combat::Outcome::Ongoing; ++j)
                    g.tick(t += kHeartbeatMs);
                g.onButton(press(Button::B));
                break;
            case Game::Nav::PostEncounter:
                g.onButton(press(Button::B));   // that fight's status readout -> dismiss
                break;
            default: break;
        }
    }
}
static void walkToShop(Game& g) { walkToShopNav(g, Game::Nav::Shop); }
static void walkToModShop(Game& g) { walkToShopNav(g, Game::Nav::ModShop); }

// Buying spends the shop's OWN price in Bits (AreaShopDef::price, distinct from
// the item's own bitsPrice), adds one to the inventory, and draws down the
// per-visit stock; the storefront stays open to buy more, and C leaves back to
// the Walk.
static void test_shop_event_buy_decrements() {
    Game g{StartMode::Hatched};
    walkToShop(g);
    CHECK(g.nav() == Game::Nav::Shop);
    const int row = g.shopCursor();
    const char* id = g.shopListingId(row);
    CHECK(id != nullptr && id[0] != '\0');
    const int price = g.shopListingBitsPrice(row);
    CHECK(price > 0);
    const int stock0 = g.shopListingStock(row);       // refilled for this visit
    CHECK(stock0 > 0);
    const int bits0 = g.bits();
    const int inv0 = g.inventory().count(id);
    g.onButton(press(Button::B));                     // buy one
    CHECK(g.bits() == bits0 - price);
    CHECK(g.inventory().count(id) == inv0 + 1);
    CHECK(g.shopListingStock(row) == stock0 - 1);
    CHECK(g.nav() == Game::Nav::Shop);               // still open
    CHECK(g.log().size() >= 1 &&
          g.log().at(0).type == LogEventType::ItemGained);
    g.onButton(press(Button::C));                    // leave -> back to the habitat
    CHECK(g.nav() == Game::Nav::Idle);
}

// Broke gate: with fewer Bits than the price, a buy is inert — no spend, no
// item, no stock draw — and the status line spells out the reason (grayscale).
static void test_shop_buy_gated_when_broke() {
    Game g{StartMode::Hatched};
    walkToShop(g);
    CHECK(g.nav() == Game::Nav::Shop);
    const int row = g.shopCursor();
    const char* id = g.shopListingId(row);
    const int price = g.shopListingBitsPrice(row);
    CHECK(id != nullptr && price > 0);
    g.debugSetBits(price - 1);                       // one Bit short
    const int inv0 = g.inventory().count(id);
    const int stock0 = g.shopListingStock(row);
    g.onButton(press(Button::B));                    // try to buy -> inert
    CHECK(g.bits() == price - 1);                    // unchanged
    CHECK(g.inventory().count(id) == inv0);
    CHECK(g.shopListingStock(row) == stock0);
    CHECK(g.nav() == Game::Nav::Shop);
}

// Selling out: buying the whole stock leaves it at 0, after which further
// buys are inert (the stock gate). Wallet is topped up so only stock limits it.
static void test_shop_sold_out_after_stock() {
    Game g{StartMode::Hatched};
    walkToShop(g);
    CHECK(g.nav() == Game::Nav::Shop);
    const int row = g.shopCursor();
    const char* id = g.shopListingId(row);
    CHECK(id != nullptr);
    const int stock0 = g.shopListingStock(row);
    g.debugSetBits(g.shopListingBitsPrice(row) * (stock0 + 5));  // plenty to clear the shelf
    for (int i = 0; i < stock0; ++i) g.onButton(press(Button::B));
    CHECK(g.shopListingStock(row) == 0);
    const int bits0 = g.bits();
    const int inv0 = g.inventory().count(id);
    g.onButton(press(Button::B));                       // sold out -> inert
    CHECK(g.bits() == bits0);
    CHECK(g.inventory().count(id) == inv0);
    CHECK(g.shopListingStock(row) == 0);
}

// Arbitrary-length listing cycling: A steps the cursor through EVERY listing (not
// just a 0/1 toggle) and wraps back to 0 — generic over whatever count the current
// area's storefront happens to carry, so a future area with more rows doesn't need
// a new test.
static void test_shop_cursor_cycles_all_listings() {
    Game g{StartMode::Hatched};
    walkToShop(g);
    CHECK(g.nav() == Game::Nav::Shop);
    const int n = g.shopListingCount();
    CHECK(n >= 1);
    for (int i = 0; i < n; ++i) {
        CHECK(g.shopCursor() == i);
        g.onButton(press(Button::A));
    }
    CHECK(g.shopCursor() == 0);   // wrapped back to the first listing
}

// Mod shop: buying spends Bits AND every item cost the listing carries (not Bits
// alone), then grants the mod into the loadout's spare pool — a guaranteed buy, not
// a lucky drop.
static void test_mod_shop_buy_grants_mod() {
    Game g{StartMode::Hatched};
    walkToModShop(g);
    CHECK(g.nav() == Game::Nav::ModShop);
    CHECK(g.shopIsModShop());
    const int row = g.shopCursor();
    const char* id = g.shopListingId(row);
    CHECK(id != nullptr && id[0] != '\0');
    const int price = g.shopListingBitsPrice(row);
    const int costCount = g.shopListingCostCount(row);
    CHECK(costCount >= 1);   // the mod shop's whole point: an item cost beyond Bits
    for (int k = 0; k < costCount; ++k)
        g.inventory().add(g.shopListingCostId(row, k), g.shopListingCostQty(row, k));
    g.debugSetBits(price);
    g.onButton(press(Button::B));
    CHECK(g.loadout().owns(id));
    CHECK(g.bits() == 0);
    for (int k = 0; k < costCount; ++k)
        CHECK(g.shopListingCostHave(row, k) == 0);   // the cost items were consumed
}

// Item-cost gate: an ample wallet alone isn't enough — a mod-shop listing's item
// cost(s) must also be held, or the buy stays inert (mirrors the Bits-only gate
// test_shop_buy_gated_when_broke, but for the NEW cost axis this feature adds).
static void test_mod_shop_buy_gated_by_item_cost() {
    Game g{StartMode::Hatched};
    walkToModShop(g);
    CHECK(g.nav() == Game::Nav::ModShop);
    const int row = g.shopCursor();
    const char* id = g.shopListingId(row);
    CHECK(g.shopListingCostCount(row) >= 1);
    g.debugSetBits(g.shopListingBitsPrice(row) * 10);   // plenty of Bits
    const int bits0 = g.bits();
    const bool ownedBefore = g.loadout().owns(id);
    g.onButton(press(Button::B));                       // no cost items held -> inert
    CHECK(g.bits() == bits0);
    CHECK(g.loadout().owns(id) == ownedBefore);
}

// Dual-coding gate: the shop screen reads in grayscale (the release rule).
static void test_shop_grayscale() {
    Game g{StartMode::Hatched};
    walkToShop(g);
    CHECK(g.nav() == Game::Nav::Shop);
    Framebuffer fb(kActiveW, kActiveH);
    g.render(fb);
    CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));
}

// Hands-off shop DECISION: a shop is a real buy/leave choice, so the auto-
// mode holds it ~10s (longer than a reveal) to let a watching player act. A button
// press restarts the hold so an engaged shopper isn't rushed; left untouched past the
// full hold it auto-leaves back to the habitat and exploration continues.
static void test_shop_auto_leaves_after_hold() {
    Game g{StartMode::Hatched};
    walkToShop(g);
    CHECK(g.nav() == Game::Nav::Shop);
    uint32_t t = g.lifetimeUptimeMs();                   // continue from the game's clock
    // A reveal-length wait is NOT enough — the decision hold is longer.
    for (int i = 0; i < kExploreRevealHoldBeats + 2; ++i) g.tick(t += kHeartbeatMs);
    CHECK(g.nav() == Game::Nav::Shop);
    // A press restarts the hold — the engaged shopper keeps the shop open.
    g.onButton(press(Button::A));                        // one-item shop: A is a no-op cycle
    for (int i = 0; i < kExploreDecisionHoldBeats - 2; ++i) g.tick(t += kHeartbeatMs);
    CHECK(g.nav() == Game::Nav::Shop);                   // the press extended the window
    // Left alone past the full hold, it auto-leaves (stop the instant it does).
    bool left = false;
    for (int i = 0; i < kExploreDecisionHoldBeats + 4 && !left; ++i) {
        g.tick(t += kHeartbeatMs);
        if (g.nav() != Game::Nav::Shop) left = true;
    }
    CHECK(left);
    CHECK(g.nav() == Game::Nav::Idle);                   // auto-left, no purchase forced
    CHECK(g.exploreActive());                            // explore keeps running
}

// Sealed Cache ----------------------------------------
// A container collected NON-INTERRUPTING on the walk (the walk keeps going, no
// sub-screen), then ALWAYS openable from ITEMS for a rarity-tiered reward draw.
// a find rolls one of four rarity tiers (sealed_cache_<rarity>), so the
// test counts the whole family, not a single base id.
static const char* const kCacheIds[] = {"sealed_cache_common",
                                        "sealed_cache_uncommon",
                                        "sealed_cache_rare", "sealed_cache_epic"};
static int cacheCount(const Game& g) {
    int n = 0;
    for (const char* id : kCacheIds) n += g.inventory().count(id);
    return n;
}
// walkToCache bounded-searches to a find, bypassing Wilds with Sinkhole Traps (like
// walkToShop) and fighting the sector boss out if it forces (combat returns to Walk).
static void walkToCache(Game& g) {
    g.inventory().add("sinkhole_trap", 200);   // bypass Wild encounters, free
    enterWalk(g);
    uint32_t t = 0;
    for (int i = 0; i < 2000 && cacheCount(g) == 0; ++i) {
        switch (g.nav()) {
            case Game::Nav::Idle:
                if (g.exploreActive()) pingExplore(g); else enterWalk(g);
                break;
            case Game::Nav::Encounter:
                g.onButton(press(Button::A));   // Fight -> Flee
                g.onButton(press(Button::A));   // Flee -> Sinkhole
                g.onButton(press(Button::B));   // confirm -> back to idle
                break;
            case Game::Nav::Wifi: g.onButton(press(Button::B)); break;
            case Game::Nav::Shop: g.onButton(press(Button::C)); break;
            case Game::Nav::ModShop: g.onButton(press(Button::C)); break;
            case Game::Nav::Combat:
                for (int j = 0; j < 800 &&
                        g.combat().outcome() == Combat::Outcome::Ongoing; ++j)
                    g.tick(t += kHeartbeatMs);
                g.onButton(press(Button::B));
                break;
            case Game::Nav::PostEncounter:
                g.onButton(press(Button::B));   // that fight's status readout -> dismiss
                break;
            default: break;
        }
    }
}

// The cache find is non-interrupting: it lands a (rarity-tiered) cache in the
// inventory while explore-mode keeps stepping (no sub-screen), and logs the find.
static void test_sealed_cache_walk_find() {
    Game g{StartMode::Hatched};
    walkToCache(g);
    CHECK(cacheCount(g) >= 1);
    CHECK(g.nav() == Game::Nav::Idle);             // explore was NOT interrupted
}

// Decrypting a Sealed Cache now happens in the Hacker VAULT, NOT pet-side
// ITEMS. (a) pet-side Use is inert (gated "DECRYPT IN VAULT" — nothing consumed);
// (b) VAULT B decrypts: consumes one container, draws the reward pool (>= flat Bits),
// logs it, reveals the yield (Nav::CacheYield).
static void test_sealed_cache_open_grants_reward() {
    // (a) pet-side ITEMS no longer opens caches — Use is inert.
    {
        Game g{StartMode::Hatched};
        g.inventory().add("sealed_cache", 2);
        const int bits0 = g.bits();
        g.debugUseItem("sealed_cache");                  // pet-side Use path
        CHECK(g.inventory().count("sealed_cache") == 2); // NOTHING consumed
        CHECK(g.bits() == bits0);                        // no reward drawn
        CHECK(g.nav() != Game::Nav::CacheYield);         // no yield reveal
    }
    // (b) VAULT decrypt consumes + grants the reward.
    {
        Game g{StartMode::Hatched};
        g.inventory().add("sealed_cache", 2);
        const int bits0 = g.bits();
        // Into the Hacker face, focus VAULT, enter it, press B to decrypt.
        g.onButton({Button::A, true, true});             // PET -> HACKER
        g.onButton(press(Button::A));                    // summon the hacker cursor
        while (hackerCarouselSlots()[g.cursor()].id != HackerSlotId::Vault)
            g.onButton(press(Button::A));
        CHECK(hackerCarouselSlots()[g.cursor()].accessible);  // VAULT is live
        g.onButton(press(Button::B));                    // enter VAULT
        CHECK(g.nav() == Game::Nav::Submenu);
        g.onButton(press(Button::B));                    // decrypt the selected cache
        CHECK(g.inventory().count("sealed_cache") == 1); // one container consumed
        CHECK(g.bits() >= bits0 + kLootBitsReward);      // reward-pool draw paid Bits
        CHECK(g.log().size() >= 1);                      // the open/contents were logged
        CHECK(g.nav() == Game::Nav::CacheYield);         // yield reveal shown
    }
}

// Every rarity-tiered cache is an openable Quest container carrying no vitals/price
// — found, not bought, opened for its draw. Distinct rarities across the family.
static void test_sealed_cache_item_shape() {
    ContentRegistry r = ContentRegistry::embedded();
    const ItemDef* d = r.item("sealed_cache");
    CHECK(d != nullptr);
    CHECK(d->use == ItemDef::Use::OpenContainer);
    CHECK(d->type == ItemDef::Type::Quest);          // always openable (egg gate skips Quest)
    CHECK(d->bitsPrice == 0);                         // not sold at a shop
    // The four tiered variants exist and carry ascending rarities.
    const ItemDef* c = r.item("sealed_cache_common");
    const ItemDef* e = r.item("sealed_cache_epic");
    CHECK(c && c->use == ItemDef::Use::OpenContainer && c->rarity == ItemDef::Rarity::Common);
    CHECK(e && e->use == ItemDef::Use::OpenContainer && e->rarity == ItemDef::Rarity::Epic);
    for (const char* id : {"sealed_cache_common", "sealed_cache_uncommon",
                           "sealed_cache_rare", "sealed_cache_epic"}) {
        const ItemDef* t = r.item(id);
        CHECK(t != nullptr && t->use == ItemDef::Use::OpenContainer &&
              t->type == ItemDef::Type::Quest && t->bitsPrice == 0);
    }
}

// opening a tiered cache pays that tier's Bits and drops item(s) from the
// tier's rarity-specific pool. The epic tier is the multi-item drop (2 items), and
// its pool is scarce-utility only (rollback / yubi_cookie / backup_drive) — none of
// the common consumables. Grayscale-independent: assertions are on ids + counts.
static void test_cache_epic_pool_and_multidrop() {
    Game g{StartMode::Hatched};
    g.inventory().add("sealed_cache_epic", 1);
    const int bits0 = g.bits();
    // Count the WHOLE epic pool (rollback / yubi_cookie / backup_drive / restore_point /
    // deep_learning_core / zeroday_bell) so the delta is robust to which two members the
    // seeded draw picks.
    auto epicPool = [&] {
        return g.inventory().count("rollback") + g.inventory().count("yubi_cookie") +
               g.inventory().count("backup_drive") + g.inventory().count("restore_point") +
               g.inventory().count("deep_learning_core") + g.inventory().count("zeroday_bell");
    };
    const int items0 = epicPool();
    g.debugOpenCache("sealed_cache_epic");           // decrypt (VAULT path)
    CHECK(g.bits() == bits0 + ContentRegistry::embedded().item("sealed_cache_epic")->cache.bits);  // epic Bits, not the flat loot
    CHECK(epicPool() - items0 == 2);                 // epic drops TWO items
    // Nothing from a lower pool leaked in (epic pool has no common consumables).
    CHECK(g.inventory().count("airgap_snack") == Inventory::starting().count("airgap_snack"));
    CHECK(g.inventory().count("null_noodles") == 0);
}

// The common tier pays the common Bits and drops exactly one consumable from its
// pool — a cheaper draw than epic. Counted over the cache's OWN pool rather than a
// hand-listed set of ids, so adding a row to the pantry can't quietly turn this into
// a test of five specific items that the draw is now allowed to miss.
static void test_cache_common_pool_single_drop() {
    Game g{StartMode::Hatched};
    g.inventory().add("sealed_cache_common", 1);
    const int bits0 = g.bits();
    const CacheDef& common = ContentRegistry::embedded().item("sealed_cache_common")->cache;
    auto commonPool = [&] {
        int n = 0;
        for (int i = 0; i < common.poolSize; ++i) n += g.inventory().count(common.pool[i].id);
        return n;
    };
    const int pool0 = commonPool();
    g.debugOpenCache("sealed_cache_common");         // decrypt (VAULT path)
    CHECK(g.bits() == bits0 + ContentRegistry::embedded().item("sealed_cache_common")->cache.bits);
    CHECK(commonPool() - pool0 == 1);                // exactly one common item
}

// Earn-path coverage: EVERY understood item must be obtainable in play, not just
// listed in the registry. An item is earnable if it is on the one-time starting
// shelf, in some container's reward pool (ItemDef::cache.pool), in the walk loot-cache
// pool (kLootPool), shop-buyable (bitsPrice), a container the walk can FIND
// (cache.findWeight > 0) or a warp key, the OUTPUT of a Hacker MERGE HUB recipe
// (game_internal.h kMergeRecipes — its two INPUT ingredients still need their own
// earn path, same as any other item), or handed over by an ACHIEVEMENT reward.
//
// Note the container rule is findWeight, not "is a container": a cache that is only ever
// granted (the Commendation Cache) has to justify itself through the achievement table
// like anything else, so adding an unreachable container fails here.
static void test_item_earn_coverage() {
    ContentRegistry r = ContentRegistry::embedded();
    Inventory starting = Inventory::starting();
    auto inCachePool = [&r](const char* id) {
        for (const ItemDef* c : r.allItems())
            for (int i = 0; i < c->cache.poolSize; ++i)
                if (std::strcmp(c->cache.pool[i].id, id) == 0) return true;
        return false;
    };
    auto isMergeOutput = [](const char* id) {
        for (const MergeRecipe& rec : kMergeRecipes)
            if (std::strcmp(rec.outputId, id) == 0) return true;
        return false;
    };
    auto inLootPool = [](const char* id) {
        for (int i = 0; i < kLootPoolCount; ++i)
            if (std::strcmp(kLootPool[i].id, id) == 0) return true;
        return false;
    };
    auto isAchievementReward = [](const char* id) {
        for (int i = 0; i < kAchievementCount; ++i)
            for (const AchievementReward& rw : kAchievements[i].rewards)
                if (rw.kind == AchievementReward::Kind::Item && rw.id &&
                    std::strcmp(rw.id, id) == 0)
                    return true;
        return false;
    };
    for (const ItemDef* it : r.allItems()) {
        // The one deliberate orphan: the untiered original cache. It predates the rarity
        // tiers and is kept ONLY so a save written before them still has an openable
        // container — nothing grants it any more, and nothing should.
        if (std::strcmp(it->id, "sealed_cache") == 0) continue;
        const bool earnable =
            starting.count(it->id) > 0 ||                        // starting shelf
            inCachePool(it->id) ||                               // a container's pool
            inLootPool(it->id) ||                                // walk loot-cache pool
            it->bitsPrice > 0 ||                                 // shop-buyable
            it->cache.findWeight > 0 ||                          // a container the walk drops
            it->walkWarp != ItemDef::WalkWarp::None ||           // warp key found on the walk
            isMergeOutput(it->id) ||                             // Merge Hub recipe output
            isAchievementReward(it->id);                         // an achievement pays it
        if (!earnable) std::printf("  ORPHAN ITEM (no earn path): %s\n", it->id);
        CHECK(earnable);
    }
    // Pin the specifically-placed items to their homes (documents intent).
    CHECK(inCachePool("decrypt_key"));
    CHECK(inCachePool("restore_point"));
    CHECK(isAchievementReward("commend_cache"));
    // The untiered legacy cache is deliberately unfindable now, and so is the earned-only
    // Commendation Cache — the walk must never roll either.
    CHECK(r.item("sealed_cache")->cache.findWeight == 0);
    CHECK(r.item("commend_cache")->cache.findWeight == 0);
}

// opening a cache surfaces a yield reveal (Nav::CacheYield) naming the
// container + what came out; B dismisses it back to the ITEMS list. The reveal's
// item count matches the tier's multi-drop (2 for epic).
static void test_cache_yield_reveal_and_dismiss() {
    Game g{StartMode::Hatched};
    g.inventory().add("sealed_cache_epic", 1);
    g.debugOpenCache("sealed_cache_epic");            // decrypt (VAULT path)
    CHECK(g.nav() == Game::Nav::CacheYield);          // reveal is up
    CHECK(g.cacheYieldItemCount() == 2);              // epic yielded two items
    CHECK(g.cacheYieldBits() == ContentRegistry::embedded().item("sealed_cache_epic")->cache.bits);  // and the Bits it paid
    g.onButton(press(Button::B));                     // OK dismisses...
    CHECK(g.nav() == Game::Nav::Submenu);             // ...back to the submenu (VAULT list)
}

// Key warp items -------------------------------------------------
// Two explore-use keys: Access Token warps to the nearest shop, Safe-Mode Key to a
// safe rest. Both are Quest-typed (egg gate skips them), priceless (found, not
// bought), carry no vitals, and are spent ON the walk (walkWarp != None), never
// from the ITEMS use path.
static void test_warp_item_shape() {
    ContentRegistry r = ContentRegistry::embedded();
    const ItemDef* tok = r.item("access_token");
    const ItemDef* key = r.item("safe_mode_key");
    CHECK(tok != nullptr);
    CHECK(key != nullptr);
    CHECK(tok->walkWarp == ItemDef::WalkWarp::Shop);
    CHECK(key->walkWarp == ItemDef::WalkWarp::SafeRest);
    CHECK(tok->type == ItemDef::Type::Quest && key->type == ItemDef::Type::Quest);
    CHECK(tok->bitsPrice == 0 && key->bitsPrice == 0);            // found, not sold
    CHECK(tok->use != ItemDef::Use::OpenContainer &&             // not containers
          key->use != ItemDef::Use::OpenContainer);
}

// A warp key held in inventory is INERT from the ITEMS use path ("USE ON THE WALK"),
// so a stray Use can't burn it for nothing — the count is untouched.
static void test_warp_key_inert_from_items() {
    Game g{StartMode::Hatched};
    g.inventory().add("access_token", 1);
    g.debugUseItem("access_token");                  // real path: gate -> inert
    CHECK(g.inventory().count("access_token") == 1); // NOT consumed
}

// Using the Access Token WHILE EXPLORING (A+C control chord → B opens the picker,
// B spends it) warps straight to a shop event — consumes the key and lands on
// Nav::Shop (re-homed onto the control chord).
static void test_access_token_warps_to_shop() {
    Game g{StartMode::Hatched};
    enterWalk(g);
    g.inventory().add("access_token", 1);
    g.onButton(chordAC());                           // A+C -> control overlay
    CHECK(g.nav() == Game::Nav::ExploreControl);
    g.onButton(press(Button::B));                    // B -> warp picker
    CHECK(g.nav() == Game::Nav::WarpPicker);
    g.onButton(press(Button::B));                    // spend the focused key
    CHECK(g.nav() == Game::Nav::Shop);               // warped to the storefront
    CHECK(g.inventory().count("access_token") == 0); // key consumed
}

// Using the Safe-Mode Key while exploring warps to a safe rest: it de-frags the pet
// by kSafeRestDefrag, consumes the key, and returns to the idle habitat (no combat).
static void test_safe_mode_key_warps_to_rest() {
    Game g{StartMode::Hatched};
    enterWalk(g);
    g.model().setFragmentation(60);
    g.inventory().add("safe_mode_key", 1);
    g.onButton(chordAC());                           // A+C -> control overlay
    g.onButton(press(Button::B));                    // B -> warp picker
    CHECK(g.nav() == Game::Nav::WarpPicker);
    g.onButton(press(Button::B));                    // spend the focused key
    CHECK(g.nav() == Game::Nav::Idle);               // safe rest resolves in place
    CHECK(g.model().fragmentation() == 60 - kSafeRestDefrag);  // rest de-frags
    CHECK(g.inventory().count("safe_mode_key") == 0);          // key consumed
}

// With no warp keys held, the control overlay's B (Warp) is a harmless no-op — it
// opens no picker and stays in the overlay ("else inert").
static void test_explore_warp_no_keys_is_noop() {
    Game g{StartMode::Hatched};
    enterWalk(g);
    g.onButton(chordAC());                           // A+C -> control overlay
    CHECK(g.nav() == Game::Nav::ExploreControl);
    g.onButton(press(Button::B));                    // Warp with no key held -> inert
    CHECK(g.nav() == Game::Nav::ExploreControl);     // no picker opened
}

// The Key-item find walk event is NON-INTERRUPTING: it lands a warp
// key in the inventory while the walk stays live. A bounded walk to the first key
// find (bypassing wilds with Sinkhole Traps, fighting a boss out if it forces).
static void test_warp_key_walk_find() {
    Game g{StartMode::Hatched};
    g.inventory().add("sinkhole_trap", 200);         // bypass Wild encounters, free
    enterWalk(g);
    uint32_t t = 0;
    auto keysHeld = [&] {
        return g.inventory().count("access_token") +
               g.inventory().count("safe_mode_key");
    };
    for (int i = 0; i < 4000 && keysHeld() == 0; ++i) {
        switch (g.nav()) {
            case Game::Nav::Idle:
                if (g.exploreActive()) pingExplore(g); else enterWalk(g);
                break;
            case Game::Nav::Encounter:
                g.onButton(press(Button::A));        // Fight -> Flee
                g.onButton(press(Button::A));        // Flee -> Sinkhole
                g.onButton(press(Button::B));        // confirm -> back to idle
                break;
            case Game::Nav::Wifi: g.onButton(press(Button::B)); break;
            case Game::Nav::Shop: g.onButton(press(Button::C)); break;
            case Game::Nav::ModShop: g.onButton(press(Button::C)); break;
            case Game::Nav::Combat:
                for (int j = 0; j < 800 &&
                        g.combat().outcome() == Combat::Outcome::Ongoing; ++j)
                    g.tick(t += kHeartbeatMs);
                g.onButton(press(Button::B));
                break;
            case Game::Nav::PostEncounter:
                g.onButton(press(Button::B));   // that fight's status readout -> dismiss
                break;
            default: break;
        }
    }
    CHECK(keysHeld() >= 1);                           // a key was found
    CHECK(g.nav() == Game::Nav::Idle);               // explore was NOT interrupted
}

// Null Noodles: the "empty feeling" food — de-frags, makes the pet HUNGRIER
// (negative eating, the explicit exception to "+Hunger = fills"), and pulls
// Happiness TOWARD 50% from ABOVE (a too-happy pet is numbed down).
static void test_null_noodles_effects() {
    Game g{StartMode::Hatched};
    g.inventory().add("null_noodles", 1);
    g.model().setHunger(60);
    g.model().setFragmentation(40);
    g.model().setHappiness(90);                      // above the midpoint
    g.debugUseItem("null_noodles");
    CHECK(g.model().hunger() == 60 - 15);            // negative eating -> hungrier
    CHECK(g.model().fragmentation() == 40 - 15);     // de-frags
    CHECK(g.model().happiness() == 90 - 20);         // pulled toward 50 (item's pull=20)
    CHECK(g.model().happiness() >= 50);              // never overshoots past 50
    CHECK(g.inventory().count("null_noodles") == 0); // consumed
}

// The Null Noodles Happiness pull works from BELOW 50 too — it lifts a miserable
// pet toward the midpoint (the "numb" middle), never past it.
static void test_null_noodles_happy_pull_from_below() {
    Game g{StartMode::Hatched};
    g.inventory().add("null_noodles", 2);
    g.model().setHappiness(20);                      // below the midpoint
    g.debugUseItem("null_noodles");
    CHECK(g.model().happiness() == 20 + 20);         // pulled up toward 50 (item's pull=20)
    CHECK(g.model().happiness() <= 50);
    // From just under 50 it clamps AT 50, not past it.
    g.model().setHappiness(45);
    g.debugUseItem("null_noodles");
    CHECK(g.model().happiness() == 50);              // clamped to the midpoint
}

// R007_B33R: fills Hunger, lifts Happiness, but RAISES Fragmentation — the
// straightforwardly indulgent counterpart to Null Noodles.
static void test_r007_b33r_effects() {
    Game g{StartMode::Hatched};
    g.inventory().add("r007_b33r", 1);
    g.model().setHunger(50);
    g.model().setHappiness(40);
    g.model().setFragmentation(20);
    g.debugUseItem("r007_b33r");
    CHECK(g.model().hunger() == 50 + 15);            // fills
    CHECK(g.model().happiness() == 40 + 25);         // cheers
    CHECK(g.model().fragmentation() == 20 + 5);      // but fragments
    CHECK(g.inventory().count("r007_b33r") == 0);    // consumed
}

// Hacker Rank XP model: every network = kHackerRankXpPerNetwork
// XP; kHackerRankXpPerRank XP per rank. Rank is UNBOUNDED (no tier cap) and
// monotonic; XP-into-rank wraps cleanly; titles clamp to the named tiers.
static void test_hacker_rank_xp_model() {
    const int perRank = kHackerRankXpPerRank / kHackerRankXpPerNetwork;  // networks/rank

    CHECK(hackerRankForNetworksSeen(0) == 0);
    CHECK(hackerRankXp(0) == 0);
    CHECK(hackerRankXpIntoRank(0) == 0);

    // The XP math holds over many networks — walk a big range and check the
    // rank/xp identities exactly (the "XP bounds test").
    for (int n = 0; n <= 5000; ++n) {
        CHECK(hackerRankXp(n) == n * kHackerRankXpPerNetwork);
        CHECK(hackerRankForNetworksSeen(n) == n / perRank);
        CHECK(hackerRankXpIntoRank(n) ==
              (n * kHackerRankXpPerNetwork) % kHackerRankXpPerRank);
    }
    // Just-below / just-at a rank boundary tips exactly one rank.
    CHECK(hackerRankForNetworksSeen(perRank - 1) == 0);
    CHECK(hackerRankForNetworksSeen(perRank) == 1);
    CHECK(hackerRankForNetworksSeen(2 * perRank) == 2);
    // Unbounded: far past the named-tier count still climbs.
    CHECK(hackerRankForNetworksSeen(1000 * perRank) == 1000);

    // Titles: a growable, threshold-keyed ladder — hackerRankTitle(rank) is the
    // nearest tier at or beneath the numeric rank. The count is DERIVED (no fixed
    // constant), so appending a title just extends the ladder.
    const int tiers = hackerRankTierCount();
    CHECK(tiers >= 1);
    CHECK(std::strcmp(hackerRankTitle(0), "PACKET RAT") == 0);
    CHECK(std::strcmp(hackerRankTitle(-1), "PACKET RAT") == 0);   // below first → first
    // Every named tier resolves to itself at its unlock rank, and each tier's title
    // holds until the next tier's threshold (the "nearest at/beneath" contract).
    for (int i = 0; i < tiers; ++i) {
        const int at = hackerRankTierUnlock(i);
        CHECK(at >= 0);
        CHECK(std::strcmp(hackerRankTitle(at), hackerRankTitle(at)) == 0);   // stable
        if (i + 1 < tiers)
            CHECK(std::strcmp(hackerRankTitle(hackerRankTierUnlock(i + 1) - 1),
                              hackerRankTitle(at)) == 0);
    }
    // A rank far past the top tier keeps the top (highest) title — the numeric rank
    // keeps climbing while the title caps at the end of the ladder.
    const char* top = hackerRankTitle(hackerRankTierUnlock(tiers - 1));
    CHECK(std::strcmp(hackerRankTitle(9999), top) == 0);
}

// registerNetwork is the scan-time seam: dedup a sighting against the pending
// queue ONLY (never the full ledger, never a credit) and hold it for the EXPL
// Wi-Fi event to resolve — networksSeen stays 0 through all of this; crediting
// is resolveNetworkDiscovery's job, exercised via walkAndCreditNetwork in the
// tests below.
static void test_register_network_queue_dedup() {
    Game g{StartMode::Hatched};
    CHECK(g.networksSeen() == 0);

    uint8_t a[6]  = {0xAA, 0xBB, 0xCC, 0x00, 0x00, 0x01};
    uint8_t a2[6] = {0xAA, 0xBB, 0xCC, 0x00, 0x00, 0x02};
    CHECK(g.registerNetwork(a, "A"));        // new -> queued
    CHECK(g.networksSeen() == 0);            // queuing never credits
    CHECK(!g.registerNetwork(a, "A"));       // already pending -> not re-queued
    CHECK(g.registerNetwork(a2, "A2"));      // differs in one byte -> queued
    CHECK(g.networksSeen() == 0);

    // The pending queue is capped, and a FULL queue makes room by evicting its
    // OLDEST unresolved sighting rather than refusing the new one — a backlog the
    // player hasn't walked off must never stop fresh sightings being recorded.
    for (int i = 0; i < kPendingNetworkQueueCap - 2; ++i) {   // 2 already queued above
        uint8_t m[6] = {0x02, 0x03, 0x04, 0x05,
                        static_cast<uint8_t>(i >> 8), static_cast<uint8_t>(i)};
        CHECK(g.registerNetwork(m, "M"));
    }
    CHECK(g.pendingNetworks() == kPendingNetworkQueueCap);
    uint8_t overflow[6] = {0x99, 0x99, 0x99, 0x99, 0x99, 0x99};
    CHECK(g.registerNetwork(overflow, "Overflow"));    // queue full -> admitted anyway
    CHECK(g.pendingNetworks() == kPendingNetworkQueueCap);   // ...by eviction, not growth
    // `a` was the oldest, so it's what got evicted: it re-queues as a new sighting.
    CHECK(g.registerNetwork(a, "A"));
    CHECK(g.networksSeen() == 0);                      // none of this ever credits
}

// Crossing a rank via a real credited network (XP model): a flat Bits lump PER
// rank gained, no Hacker-Log entry for the celebration itself (rank-up excluded
// from the closed log vocabulary). Reward-ONLY: rank gates nothing anymore (all
// EXPL sectors open regardless of rank — sector gating is sectorCleared[]).
static void test_hacker_rank_up_grants_reward() {
    const int perRank = kHackerRankXpPerRank / kHackerRankXpPerNetwork;
    Game g{StartMode::Hatched};
    CHECK(g.hackerRank() == 0);

    // Credit exactly one rank's worth of distinct networks -> one rank up.
    // Each walkToWifiEvent search can pass through OTHER incidental bits-
    // granting explore events on the way to landing on Wifi (loot/cache steps,
    // sinkholed wild encounters) — noise unrelated to the rank-up reward, and it
    // accumulates across iterations, so comparing a running total against a
    // pre-loop baseline would be unreliable. Instead isolate the reward at its
    // source: track the Bits delta across ONLY the specific credit call where
    // hackerRank() actually changes (before vs. right when that event is
    // reached — resolveNetworkDiscovery's own effects only, still before the
    // unrelated guardian/cache/friendly roll that always follows resolves).
    int rankUpBitsGrant = 0;
    int prevRank = g.hackerRank();
    for (int i = 0; i < perRank; ++i) {
        uint8_t m[6] = {0x10, 0, 0, 0, 0, static_cast<uint8_t>(i)};
        const int bitsBefore = g.bits();
        queueAndReachWifiEvent(g, m, "R");
        if (g.hackerRank() != prevRank) {
            rankUpBitsGrant += g.bits() - bitsBefore;
            prevRank = g.hackerRank();
        }
        resolveWifiEventToIdle(g);
    }
    CHECK(g.networksSeen() == perRank);
    CHECK(g.hackerRank() == 1);
    // Reward-only: climbing rank does NOT unlock sectors — sector[1] stays LOCKED
    // (gating is sectorCleared[], not rank). Only sector 0 is open.
    { bool fl[kExplSectors] = {g.sectorCleared(0), g.sectorCleared(1)};
      CHECK(explSectorOpen(0, fl));
      CHECK(!explSectorOpen(1, fl)); }
    // >= not == : the isolated delta above still spans this ONE credit call's own
    // walkToWifiEvent search, which (rarely) may ALSO pass through an unrelated
    // bits-granting step (a loot cache) before landing on Wifi — noise can only
    // ADD, so a lower bound still proves the reward itself fires.
    CHECK(rankUpBitsGrant >= kHackerRankUpBitsReward);
    // The rank-up celebration ITSELF still isn't logged — but each of the
    // `perRank` new-network credits now also grants + logs a discovery Sealed
    // Cache (the pcap-blowup follow-on reward), so log size DOES grow (capped
    // at EventLog::kCapacity). Assert the property directly: no entry mentions
    // the rank-up celebration text, rather than asserting "size didn't move"
    // (the reward logging writes an entry, so that would be wrong).
    for (int i = 0; i < g.log().size(); ++i)
        CHECK(std::strstr(g.log().at(i).text, "RANK") == nullptr);

    // A batch that crosses several ranks at once pays per rank gained — same
    // per-credit isolation, summed across every iteration that actually crosses.
    const int rank1 = g.hackerRank();
    int totalRankUpBits = 0;
    int prevRank2 = rank1;
    for (int i = 0; i < 2 * perRank; ++i) {
        uint8_t m[6] = {0x20, 0, 0, 0, static_cast<uint8_t>(i >> 8),
                        static_cast<uint8_t>(i)};
        const int bitsBefore = g.bits();
        queueAndReachWifiEvent(g, m, "R2");
        if (g.hackerRank() != prevRank2) {
            totalRankUpBits += g.bits() - bitsBefore;
            prevRank2 = g.hackerRank();
        }
        resolveWifiEventToIdle(g);
    }
    const int gained = g.hackerRank() - rank1;
    CHECK(gained == 2);
    CHECK(totalRankUpBits >= kHackerRankUpBitsReward * gained);   // see the >= note above
}

// NetworkLedger (core/net/network_ledger.h) — pure logic, no file I/O exercised
// here (native tests don't touch real SD/file access, matching net_capture.h's
// own documented host/device testing boundary). recordNew seeds count 1;
// recordSeenAgain bumps it and can upgrade a name from empty (e.g. a hidden
// network scanned before its SSID was ever known) once a real one arrives.
static void test_network_ledger_new_and_repeat() {
    NetworkLedger ledger;
    NetworkLedger::Entry e;
    CHECK(!ledger.lookup(1, &e));

    ledger.recordNew(1, "Alpha");
    CHECK(ledger.lookup(1, &e));
    CHECK(e.count == 1);
    CHECK(std::strcmp(e.name, "Alpha") == 0);

    ledger.recordSeenAgain(1, "Alpha");
    CHECK(ledger.lookup(1, &e));
    CHECK(e.count == 2);

    ledger.recordNew(2, "");           // e.g. a hidden network's BSSID fallback
    CHECK(ledger.lookup(2, &e));
    CHECK(e.name[0] == '\0');
    ledger.recordSeenAgain(2, "NowKnown");   // its SSID resolves on a later scan
    CHECK(ledger.lookup(2, &e));
    CHECK(std::strcmp(e.name, "NowKnown") == 0);
}

// PeerHello codec (core/net/peer_link.h) — the pet-to-pet beacon's wire format.
// A round-trip must preserve every field, and the encoded frame must be exactly
// one fixed size so the device tier can send it as a single ESP-NOW payload.
static void test_peer_hello_round_trip() {
    PeerHello out;
    std::strncpy(out.tag, "GHOSTBYTE", sizeof(out.tag) - 1);
    std::strncpy(out.petName, "Paypup", sizeof(out.petName) - 1);
    std::strncpy(out.crewName, "Deniers of Service", sizeof(out.crewName) - 1);
    out.stage = static_cast<uint8_t>(Stage::Script);
    out.rank = 7;
    out.crewRed = false;

    uint8_t frame[kPeerHelloSize];
    CHECK(encodePeerHello(out, frame, sizeof(frame)) == kPeerHelloSize);

    PeerHello in;
    CHECK(decodePeerHello(frame, kPeerHelloSize, &in));
    CHECK(std::strcmp(in.tag, "GHOSTBYTE") == 0);
    CHECK(std::strcmp(in.petName, "Paypup") == 0);
    CHECK(std::strcmp(in.crewName, "Deniers of Service") == 0);
    CHECK(in.stage == static_cast<uint8_t>(Stage::Script));
    CHECK(in.rank == 7);
    CHECK(!in.crewRed);

    // The team bit is carried independently of the crew name.
    out.crewRed = true;
    CHECK(encodePeerHello(out, frame, sizeof(frame)) == kPeerHelloSize);
    CHECK(decodePeerHello(frame, kPeerHelloSize, &in));
    CHECK(in.crewRed);

    // An unenlisted operator broadcasts an empty crew — that's the "no crew"
    // signal the PEERS screen reads, not a separate flag.
    PeerHello solo;
    std::strncpy(solo.tag, "LONEWOLF", sizeof(solo.tag) - 1);
    CHECK(encodePeerHello(solo, frame, sizeof(frame)) == kPeerHelloSize);
    CHECK(decodePeerHello(frame, kPeerHelloSize, &in));
    CHECK(in.crewName[0] == '\0');
}

// Everything a decode accepts gets rendered and written to SD, so the frame is a
// trust boundary: a wrong length, foreign magic, or a protocol version this build
// can't lay out must be refused outright rather than parsed on a guess.
static void test_peer_hello_rejects_malformed() {
    PeerHello src;
    std::strncpy(src.tag, "VALID", sizeof(src.tag) - 1);
    uint8_t frame[kPeerHelloSize];
    CHECK(encodePeerHello(src, frame, sizeof(frame)) == kPeerHelloSize);

    PeerHello out;
    CHECK(decodePeerHello(frame, kPeerHelloSize, &out));      // baseline: accepted

    CHECK(!decodePeerHello(frame, kPeerHelloSize - 1, &out)); // short frame
    CHECK(!decodePeerHello(frame, kPeerHelloSize + 1, &out)); // long frame
    CHECK(!decodePeerHello(nullptr, kPeerHelloSize, &out));
    CHECK(!decodePeerHello(frame, kPeerHelloSize, nullptr));

    uint8_t bad[kPeerHelloSize];
    std::memcpy(bad, frame, sizeof(bad));
    bad[0] = 'X';                                             // not our magic
    CHECK(!decodePeerHello(bad, kPeerHelloSize, &out));

    std::memcpy(bad, frame, sizeof(bad));
    bad[4] = kPeerProtoVersion + 1;                           // a newer peer
    CHECK(!decodePeerHello(bad, kPeerHelloSize, &out));

    // Too small a buffer must fail the encode rather than write past it.
    uint8_t tiny[4];
    CHECK(encodePeerHello(src, tiny, sizeof(tiny)) == 0);
    CHECK(encodePeerHello(src, nullptr, kPeerHelloSize) == 0);
}

// A HELLO's strings are attacker-controlled and land in the framebuffer and the
// roster file, so decode keeps printable ASCII only. Offending bytes are dropped
// (not frame-rejected), so a mangled name still yields a usable shorter row — and
// an unterminated wire field can never escape as an unterminated C string.
static void test_peer_hello_sanitizes_hostile_fields() {
    uint8_t frame[kPeerHelloSize];
    PeerHello src;
    std::strncpy(src.tag, "OK", sizeof(src.tag) - 1);
    CHECK(encodePeerHello(src, frame, sizeof(frame)) == kPeerHelloSize);

    // Overwrite the pet-name slot with control bytes, a high-bit byte, and no
    // terminator at all — the worst a sender could put on the wire.
    const size_t petOff = 9 + kPeerTagCap;
    for (size_t i = 0; i < kPeerPetCap; ++i) frame[petOff + i] = 0xFF;
    frame[petOff + 0] = 'A';
    frame[petOff + 1] = 0x07;   // BEL
    frame[petOff + 2] = 'B';
    frame[petOff + 3] = 0x1B;   // ESC

    PeerHello out;
    CHECK(decodePeerHello(frame, kPeerHelloSize, &out));
    CHECK(std::strcmp(out.petName, "AB") == 0);   // control + high-bit bytes dropped
    CHECK(out.petName[kPeerPetCap - 1] == '\0');  // terminated regardless of the wire
}

// PeerLedger (core/net/peer_ledger.h) — pure logic, no file I/O exercised here
// (native tests don't touch real SD, matching the NetworkLedger tests above). The
// distinction from that ledger is the point: an operator's pet/crew/rank CHANGE
// between meetings, so a repeat sighting refreshes them while the tally accrues.
static void test_peer_ledger_new_and_refresh() {
    PeerLedger ledger;
    PeerLedger::Entry e;
    CHECK(!ledger.lookup(0xAABBCCDDEEFFull, &e));

    PeerHello first;
    std::strncpy(first.tag, "GHOSTBYTE", sizeof(first.tag) - 1);
    std::strncpy(first.petName, "Paypup", sizeof(first.petName) - 1);
    first.stage = static_cast<uint8_t>(Stage::Process);
    first.rank = 2;

    CHECK(ledger.record(0xAABBCCDDEEFFull, first));            // true == new operator
    CHECK(ledger.size() == 1);
    CHECK(ledger.lookup(0xAABBCCDDEEFFull, &e));
    CHECK(e.timesMet == 1);
    CHECK(std::strcmp(e.petName, "Paypup") == 0);
    CHECK(e.crewName[0] == '\0');

    // Met again, later: same operator, but they've evolved their pet, enlisted,
    // and ranked up. The row must show who they are NOW.
    PeerHello later;
    std::strncpy(later.tag, "GHOSTBYTE", sizeof(later.tag) - 1);
    std::strncpy(later.petName, "Malbear", sizeof(later.petName) - 1);
    std::strncpy(later.crewName, "Deniers of Service", sizeof(later.crewName) - 1);
    later.stage = static_cast<uint8_t>(Stage::Script);
    later.rank = 5;
    later.crewRed = false;

    CHECK(!ledger.record(0xAABBCCDDEEFFull, later));           // false == already known
    CHECK(ledger.size() == 1);                                 // refreshed, not appended
    CHECK(ledger.lookup(0xAABBCCDDEEFFull, &e));
    CHECK(e.timesMet == 2);
    CHECK(std::strcmp(e.petName, "Malbear") == 0);
    CHECK(std::strcmp(e.crewName, "Deniers of Service") == 0);
    CHECK(e.stage == static_cast<uint8_t>(Stage::Script));
    CHECK(e.rank == 5);

    // A different radio MAC is a different operator even with an identical tag —
    // the key is the identity, the tag is just what they call themselves.
    CHECK(ledger.record(0x112233445566ull, later));
    CHECK(ledger.size() == 2);
}

// inTopN backs the "home turf vs. occasional" split resolveNetworkDiscovery
// uses: fewer than N OTHER entries with a strictly higher count qualifies
// (ties all qualify — with few entries total, everything reads as "top N"
// until enough variety exists to actually rank below others).
static void test_network_ledger_in_top_n() {
    NetworkLedger ledger;
    for (uint64_t k = 1; k <= 8; ++k) {
        ledger.recordNew(k, "N");
        for (int i = 0; i < 4; ++i) ledger.recordSeenAgain(k, "N");   // count 5
    }
    ledger.recordNew(9, "Rare");                                      // count 1
    for (uint64_t k = 1; k <= 8; ++k) CHECK(ledger.inTopN(k, 8));
    CHECK(!ledger.inTopN(9, 8));      // the 9th, lower-count network isn't top-8
    CHECK(!ledger.inTopN(999, 8));    // unknown key -> false
}

// A repeat sighting outside the top-8 by count (an "occasional" network — your
// town, not your daily-driver routers) still feeds the pet: pet combat XP + the
// same small Happiness bump a new network gets, no Hacker Rank XP. A repeat
// INSIDE the top-8 ("home turf") stays flat — no XP, no Happiness change.
static void test_network_discovery_repeat_familiar_vs_home_turf() {
    Game g{StartMode::Hatched};
    uint8_t common[8][6];
    for (int i = 0; i < 8; ++i) {
        common[i][0] = 0x70; common[i][1] = 0; common[i][2] = 0; common[i][3] = 0;
        common[i][4] = 0; common[i][5] = static_cast<uint8_t>(i);
        walkAndCreditNetwork(g, common[i], "Common");       // count 1 each
    }
    uint8_t rare[6] = {0x71, 0, 0, 0, 0, 0x01};
    walkAndCreditNetwork(g, rare, "Rare");                  // count 1, new -> credited
    CHECK(g.networksSeen() == 9);

    // Bump each of the 8 "common" networks to count 2 — a repeat while every
    // count is still tied at 1 reads as home-turf (see test_network_ledger_in_top_n's
    // comment), so this loop itself grants nothing; it's just setup.
    for (int i = 0; i < 8; ++i) walkAndCreditNetwork(g, common[i], "Common");
    CHECK(g.networksSeen() == 9);   // still just the 9 originals — no new credit

    // Now `rare` (count 1) sits strictly below all 8 "common" networks (count 2) —
    // a repeat resolves it as familiar-but-occasional: pet XP + Happiness, no
    // Hacker Rank XP. Verified via netDiscoveryFlavor_ (set exclusively by
    // resolveNetworkDiscovery) rather than combatXp/combatLevel deltas — a
    // walkToWifiEvent search can incidentally pass through an UNRELATED sinkholed
    // wild encounter (walkToWifiEvent stocks Sinkhole Traps so any Wild roll
    // along the way auto-resolves via resolveSinkhole, which grants its OWN
    // combat XP), so a raw XP delta can't reliably isolate MY grant across a
    // real walk. Same reasoning for hackerRank/networksSeen — those two ARE
    // exclusively mine (nothing else touches them), so they stay precise checks.
    const int rank0 = g.hackerRank();
    queueAndReachWifiEvent(g, rare, "Rare");
    CHECK(g.networksSeen() == 9);                 // no Hacker Rank credit
    CHECK(g.hackerRank() == rank0);
    CHECK(std::strstr(g.netDiscoveryFlavor(), "FONDLY REMEMBERS") != nullptr);
    resolveWifiEventToIdle(g);

    // A repeat of one of the "common" (home-turf) networks stays flat: the
    // flavor reads "TIRED OF", not "FONDLY REMEMBERS" — no pet-XP branch taken,
    // no further Hacker Rank movement.
    queueAndReachWifiEvent(g, common[0], "Common");
    CHECK(std::strstr(g.netDiscoveryFlavor(), "TIRED OF") != nullptr);
    CHECK(g.hackerRank() == rank0);
    resolveWifiEventToIdle(g);
}

// The empty-queue Happiness penalty only actually applies on the 1st miss, then
// every kNetDiscoveryEmptyCooldownStrikes-th miss after (streak 1, 4, 7…) — a
// long unmonitored walk through a dead zone (nothing ever queued) tapers off
// instead of grinding Happiness to 0.
static void test_network_discovery_empty_queue_penalty_throttles() {
    Game g{StartMode::Hatched};
    g.model().setHappiness(80);   // headroom so repeated -kNetDiscoveryNoneHappyPenalty
                                   // hits never clamp at 0 and mask a missed/extra strike
    g.inventory().add("sinkhole_trap", 20);   // bypass any Wild roll along the way, free
    enterWalk(g);   // ONCE — walkToWifiEvent re-enters via enterWalk on every call,
                     // which re-arms the sub-area and resets emptyQueueStreak_
                     // (game_explore.cpp's startExplore), defeating the whole point of
                     // this test (observing the streak persist/throttle ACROSS misses).
                     // Step with pingExplore instead, staying in the same session.
    int strikes = 0;
    for (int i = 0; i < 6; ++i) {
        const int before = g.model().happiness();
        for (int tries = 0; tries < 400 && g.nav() != Game::Nav::Wifi; ++tries) {
            if (g.nav() == Game::Nav::Encounter) {
                g.onButton(press(Button::A));   // Fight -> Flee
                g.onButton(press(Button::A));   // Flee -> Sinkhole
                g.onButton(press(Button::B));   // confirm -> back to idle
            } else if (g.nav() == Game::Nav::Shop || g.nav() == Game::Nav::ModShop) {
                g.onButton(press(Button::C));   // leave the shop -> back to idle
            } else if (g.nav() == Game::Nav::Combat) {
                uint32_t t = 0;
                for (int j = 0; j < 800 &&
                                g.combat().outcome() == Combat::Outcome::Ongoing; ++j)
                    g.tick(t += kHeartbeatMs);
                g.onButton(press(Button::B));
            } else if (g.nav() == Game::Nav::PostEncounter) {
                g.onButton(press(Button::B));
            } else if (g.nav() == Game::Nav::Idle) {
                pingExplore(g);
            }
        }
        if (g.nav() != Game::Nav::Wifi) break;   // search exhausted — stop, don't fail
        const int after = g.model().happiness();
        if (after < before) ++strikes;
        else CHECK(after == before);
        g.onButton(press(Button::B));
        if (g.nav() == Game::Nav::Combat) {
            uint32_t t = 0;
            for (int j = 0; j < 400 && g.combat().outcome() == Combat::Outcome::Ongoing; ++j)
                g.tick(t += kHeartbeatMs);
            g.onButton(press(Button::B));
        }
    }
    // Trigger 1 always strikes; trigger 4 strikes if reached within 6 tries;
    // 2/3/5/6 never do — so at most 2 of the 6 tries actually cost Happiness.
    CHECK(strikes >= 1 && strikes <= 2);
}

// The runtime Audit-scan toggle persists across a reboot (save
// v5) — an authorized-use choice must survive, and default OFF must be honored.
static void test_audit_scan_toggle_persists() {
    MemSaveStore store;
    {
        Game g(StartMode::Hatched, "paypup", &store);
        CHECK(!g.netScanEnabled());          // default OFF (authorized-use opt-in)
        g.setNetScanEnabled(true);
        g.tick(kSaveAutosaveMs + kHeartbeatMs);   // flush the debounced write
    }
    {
        Game g(StartMode::Hatched, "paypup", &store);
        CHECK(g.netScanEnabled());           // survived the reboot
    }
}

// A v4 blob (no v5 audit tail) still loads: netScanEnabled defaults OFF (a
// migrated save never silently arms the radio). v4 fields survive intact.
static void test_save_v4_migration_defaults_audit_off() {
    SaveData a;
    std::strcpy(a.activeId, "paypup");
    a.networksSeen = 7;
    a.hackerRank = 0;
    a.netScanEnabled = 1;                    // set it, then strip the v5 tail below
    auto blob = forgeLegacyNetworkBytes(a, 4);
    CHECK(blob.size() > 1);
    blob.resize(blob.size() - 1);            // drop the v5 tail (u8 = 1 byte)
    blob[4] = 4; blob[5] = 0;                // version word -> 4

    SaveData out;
    CHECK(deserializeSave(blob, out));
    CHECK(out.netScanEnabled == 0);          // absent tail -> OFF
    CHECK(out.networksSeen == 7);            // v4 fields intact
    CHECK(std::strcmp(out.activeId, "paypup") == 0);
}

// Audit handshake capture —.pcap writer + capture policy SM -------

// A memory PcapSink: the native seam standing in for the device's SD FILE. Every
// byte the writer emits accumulates here so a gate can assert the exact stream.
class MemPcapSink : public PcapSink {
public:
    bool write(const uint8_t* data, size_t n) override {
        bytes_.insert(bytes_.end(), data, data + n);
        return true;
    }
    const std::vector<uint8_t>& bytes() const { return bytes_; }
private:
    std::vector<uint8_t> bytes_;
};

// Little-endian u32 read helper for asserting header fields.
static uint32_t leU32(const std::vector<uint8_t>& b, size_t off) {
    return static_cast<uint32_t>(b[off]) | (static_cast<uint32_t>(b[off + 1]) << 8) |
           (static_cast<uint32_t>(b[off + 2]) << 16) |
           (static_cast<uint32_t>(b[off + 3]) << 24);
}

// The 24-byte global header is byte-exact classic libpcap (magic a1b2c3d4,
// version 2.4, snaplen, DLT_IEEE802_11=105). A reader keys off these exactly.
static void test_pcap_global_header() {
    uint8_t h[kPcapGlobalHeaderLen];
    writePcapGlobalHeader(h);
    // Magic bytes are little-endian a1b2c3d4 -> d4 c3 b2 a1 on disk.
    CHECK(h[0] == 0xd4 && h[1] == 0xc3 && h[2] == 0xb2 && h[3] == 0xa1);
    CHECK(h[4] == 2 && h[5] == 0);           // version_major = 2
    CHECK(h[6] == 4 && h[7] == 0);           // version_minor = 4
    std::vector<uint8_t> v(h, h + sizeof(h));
    CHECK(leU32(v, 8) == 0);                 // thiszone
    CHECK(leU32(v, 12) == 0);                // sigfigs
    CHECK(leU32(v, 16) == kPcapSnaplen);     // snaplen
    CHECK(leU32(v, 20) == kPcapLinkTypeIEEE80211);  // 105
}

// The writer streams a valid file: one global header, then a record header +
// frame per writeFrame, with the byte/frame counters and snaplen truncation
// (incl_len capped, orig_len = true length) all correct.
static void test_pcap_writer_stream() {
    MemPcapSink sink;
    PcapWriter w;
    CHECK(!w.begun());
    CHECK(w.begin(sink));
    CHECK(w.begun());
    CHECK(w.begin(sink));                    // idempotent: no second header
    CHECK(sink.bytes().size() == static_cast<size_t>(kPcapGlobalHeaderLen));

    // A small EAPOL-sized frame writes whole (incl == orig).
    const uint8_t frame[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    CHECK(w.writeFrame(sink, frame, sizeof(frame), 0x11223344u, 500u));
    CHECK(w.frames() == 1);
    const size_t recOff = kPcapGlobalHeaderLen;
    const auto& b = sink.bytes();
    CHECK(b.size() == static_cast<size_t>(kPcapGlobalHeaderLen + kPcapRecordHeaderLen + 8));
    CHECK(leU32(b, recOff + 0) == 0x11223344u);   // ts_sec
    CHECK(leU32(b, recOff + 4) == 500u);          // ts_usec
    CHECK(leU32(b, recOff + 8) == 8u);            // incl_len
    CHECK(leU32(b, recOff + 12) == 8u);           // orig_len
    CHECK(b[recOff + 16] == 1 && b[recOff + 16 + 7] == 8);  // frame payload

    // An oversized frame is truncated to snaplen in the file but records its true
    // length in orig_len (standard libpcap semantics).
    std::vector<uint8_t> big(kPcapSnaplen + 100, 0xAB);
    const size_t before = sink.bytes().size();
    CHECK(w.writeFrame(sink, big.data(), static_cast<uint32_t>(big.size()), 1u, 2u));
    CHECK(w.frames() == 2);
    const size_t rec2 = before;
    CHECK(leU32(sink.bytes(), rec2 + 8) == kPcapSnaplen);           // incl capped
    CHECK(leU32(sink.bytes(), rec2 + 12) == kPcapSnaplen + 100u);   // orig true
    CHECK(sink.bytes().size() == before + kPcapRecordHeaderLen + kPcapSnaplen);

    CHECK(w.bytesWritten() == sink.bytes().size());
}

// writeFrame before begin() must fail without emitting or counting (the global
// header is a hard precondition for a valid file).
static void test_pcap_writer_requires_begin() {
    MemPcapSink sink;
    PcapWriter w;
    const uint8_t f[4] = {9, 9, 9, 9};
    CHECK(!w.writeFrame(sink, f, sizeof(f), 0, 0));
    CHECK(w.frames() == 0);
    CHECK(sink.bytes().empty());
}

// The capture policy SM: arm -> capture -> hot broadcast ->
// seal -> re-arm cooldown -> auto re-arm. Pure, driven by a fake game-ms clock.
static void test_audit_capture_state_machine() {
    AuditCapture ac;
    CHECK(ac.phase() == AuditPhase::Disarmed);
    CHECK(!ac.enabled());
    CHECK(!ac.capturing());

    // Turning the toggle on arms immediately (never sealed -> canArm true).
    ac.setEnabled(true, 0);
    CHECK(ac.enabled());
    CHECK(ac.phase() == AuditPhase::Armed);
    CHECK(ac.capturing());
    CHECK(!ac.broadcasting());

    // A handshake capture enters the hot-broadcast window.
    ac.noteHandshake(1000);
    CHECK(ac.phase() == AuditPhase::Hot);
    CHECK(ac.broadcasting());
    CHECK(ac.hotRemainingMs(1000) == kAuditHotBroadcastMs);

    // Mid-window: still hot, still broadcasting.
    ac.tick(1000 + kAuditHotBroadcastMs / 2);
    CHECK(ac.phase() == AuditPhase::Hot);

    // The window expires -> seal -> cooldown (broadcast stops).
    const uint32_t sealAt = 1000 + kAuditHotBroadcastMs;
    ac.tick(sealAt);
    CHECK(ac.phase() == AuditPhase::Cooldown);
    CHECK(!ac.broadcasting());
    CHECK(!ac.canArm(sealAt));
    CHECK(ac.cooldownRemainingMs(sealAt) == kAuditRearmCooldownMs);

    // Cooling: still can't arm; a handshake is ignored.
    ac.noteHandshake(sealAt + 5000);
    CHECK(ac.phase() == AuditPhase::Cooldown);

    // Cooldown elapses with the toggle still on -> auto re-arm.
    const uint32_t rearmAt = sealAt + kAuditRearmCooldownMs;
    CHECK(ac.canArm(rearmAt));
    ac.tick(rearmAt);
    CHECK(ac.phase() == AuditPhase::Armed);
    CHECK(ac.cooldownRemainingMs(rearmAt) == 0);
}

// Turning the toggle OFF seals a live broadcast immediately and starts cooldown;
// after cooldown, with the toggle off, it lands Disarmed (not re-armed). And
// leaving the event (softSeal) stops a hot broadcast without touching the toggle.
static void test_audit_capture_seal_paths() {
    AuditCapture ac;
    ac.setEnabled(true, 0);
    ac.noteHandshake(100);
    CHECK(ac.broadcasting());
    // Toggle off mid-broadcast: immediate seal.
    ac.setEnabled(false, 200);
    CHECK(!ac.enabled());
    CHECK(ac.phase() == AuditPhase::Cooldown);
    CHECK(!ac.broadcasting());
    // After cooldown, toggle still off -> Disarmed (no silent re-arm).
    ac.tick(200 + kAuditRearmCooldownMs);
    CHECK(ac.phase() == AuditPhase::Disarmed);

    // softSeal path: leaving the audit event stops a hot broadcast (-> cooldown)
    // but the toggle intent stays on, so it re-arms after the cooldown.
    AuditCapture ac2;
    ac2.setEnabled(true, 0);
    ac2.noteHandshake(10);
    ac2.softSeal(20);
    CHECK(ac2.phase() == AuditPhase::Cooldown);
    CHECK(ac2.enabled());
    ac2.tick(20 + kAuditRearmCooldownMs);
    CHECK(ac2.phase() == AuditPhase::Armed);
}

// The Audit-CAPTURE toggle (save v6) persists across a reboot, default OFF.
static void test_audit_capture_toggle_persists() {
    MemSaveStore store;
    {
        Game g(StartMode::Hatched, "paypup", &store);
        CHECK(!g.auditCaptureEnabled());     // default OFF (authorized-use opt-in)
        g.setAuditCaptureEnabled(true);
        g.tick(kSaveAutosaveMs + kHeartbeatMs);   // flush the debounced write
    }
    {
        Game g(StartMode::Hatched, "paypup", &store);
        CHECK(g.auditCaptureEnabled());      // survived the reboot
    }
}

// The escalating AuditMode ties scan + capture into one ordered dial: capture can
// never run without the discovery scan, whichever setter you go through.
static void test_audit_mode_enforces_scan_dependency() {
    MemSaveStore store;
    Game g(StartMode::Hatched, "paypup", &store);
    CHECK(g.auditMode() == Game::AuditMode::Off);
    CHECK(!g.netScanEnabled());
    CHECK(!g.auditCaptureEnabled());

    // ScanCapture arms both; Scan keeps scan but disarms capture.
    g.setAuditMode(Game::AuditMode::ScanCapture);
    CHECK(g.auditMode() == Game::AuditMode::ScanCapture);
    CHECK(g.netScanEnabled() && g.auditCaptureEnabled());
    g.setAuditMode(Game::AuditMode::Scan);
    CHECK(g.netScanEnabled() && !g.auditCaptureEnabled());

    // Turning scan OFF via the low-level setter also disarms capture (invariant).
    g.setAuditMode(Game::AuditMode::ScanCapture);
    g.setNetScanEnabled(false);
    CHECK(!g.netScanEnabled() && !g.auditCaptureEnabled());
    CHECK(g.auditMode() == Game::AuditMode::Off);

    // Enabling capture via the low-level setter pulls scan up to match.
    g.setAuditCaptureEnabled(true);
    CHECK(g.netScanEnabled() && g.auditCaptureEnabled());
    CHECK(g.auditMode() == Game::AuditMode::ScanCapture);
}

// A legacy save from before the single control could hold capture-ON with
// scan-OFF (the two toggles were independent). On load, scan is pulled up so the
// invariant holds — capture never runs without its discovery feed.
static void test_audit_legacy_capture_without_scan_normalizes_on_load() {
    SaveData a;
    std::strcpy(a.activeId, "paypup");
    a.netScanEnabled = 0;
    a.auditCaptureEnabled = 1;               // the inconsistent legacy state
    MemSaveStore store;
    store.save(serializeSave(a));
    Game g(StartMode::Hatched, "paypup", &store);
    CHECK(g.netScanEnabled());               // pulled up to match capture
    CHECK(g.auditCaptureEnabled());
    CHECK(g.auditMode() == Game::AuditMode::ScanCapture);
}

// A v5 blob (no v6 capture tail) still loads: auditCaptureEnabled defaults OFF,
// v5 fields (netScanEnabled) survive intact.
static void test_save_v5_migration_defaults_capture_off() {
    SaveData a;
    std::strcpy(a.activeId, "paypup");
    a.netScanEnabled = 1;
    a.auditCaptureEnabled = 1;               // set it, then strip the v6 tail below
    auto blob = forgeLegacyNetworkBytes(a, 5);
    blob.resize(blob.size() - 1);            // drop the v6 tail (u8 = 1 byte)
    blob[4] = 5; blob[5] = 0;                // version word -> 5

    SaveData out;
    CHECK(deserializeSave(blob, out));
    CHECK(out.auditCaptureEnabled == 0);     // absent tail -> OFF
    CHECK(out.netScanEnabled == 1);          // v5 field intact
    CHECK(std::strcmp(out.activeId, "paypup") == 0);
}

// RF half — pure EAPOL classifier + handshake tracker -------------

// Build a raw 802.11 EAPOL-Key MPDU (no radiotap — the DLT_IEEE802_11 bytes the
// promiscuous callback hands us). fromDs=true models an AP->STA message (M1/M3,
// BSSID in addr2); fromDs=false a STA->AP message (M2/M4, ToDS, BSSID in addr1).
// Returns the frame length. Layout: 24B MAC hdr + 8B LLC/SNAP + 4B 802.1X hdr +
// EAPOL-Key body (descriptor type + 2B Key Information + a little tail).
static uint32_t buildEapolKey(uint8_t out[64], bool fromDs, const uint8_t bssid[6],
                              uint16_t keyInfo) {
    std::memset(out, 0, 64);
    out[0] = 0x08;                       // data frame, subtype 0
    out[1] = fromDs ? 0x02 : 0x01;       // FromDS (AP->STA) or ToDS (STA->AP)
    // Addresses: put the BSSID in the field the DS bits select.
    std::memcpy(out + (fromDs ? 10 : 4), bssid, 6);
    uint8_t* llc = out + 24;
    const uint8_t snap[8] = {0xAA, 0xAA, 0x03, 0x00, 0x00, 0x00, 0x88, 0x8E};
    std::memcpy(llc, snap, 8);
    uint8_t* eapol = llc + 8;
    eapol[0] = 0x02;                     // 802.1X version
    eapol[1] = 0x03;                     // packet type = EAPOL-Key
    eapol[2] = 0x00; eapol[3] = 0x5f;    // body length (illustrative)
    uint8_t* body = eapol + 4;
    body[0] = 0x02;                      // descriptor type = RSN
    body[1] = static_cast<uint8_t>(keyInfo >> 8);   // Key Information (big-endian)
    body[2] = static_cast<uint8_t>(keyInfo & 0xFF);
    return 24 + 8 + 4 + 3 + 8;           // + a small key-data tail
}

// Key Information flag combos for each 4-way message (pairwise 0x08 + version 2).
static constexpr uint16_t kKiM1 = 0x0088;  // pairwise + Ack
static constexpr uint16_t kKiM2 = 0x0108;  // pairwise + MIC
static constexpr uint16_t kKiM3 = 0x03c8;  // pairwise + Install + Ack + MIC + Secure
static constexpr uint16_t kKiM4 = 0x0308;  // pairwise + MIC + Secure

// parseEapolKey classifies each 4-way message and pulls the right BSSID out of
// the address field the DS bits select; non-EAPOL / malformed frames are inert.
static void test_eapol_parse_classifies_messages() {
    uint8_t buf[64];
    const uint8_t ap[6] = {0x02, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE};

    auto m1 = parseEapolKey(buf, buildEapolKey(buf, true, ap, kKiM1));
    CHECK(m1.isEapolKey && m1.msg == 1);
    CHECK(std::memcmp(m1.bssid, ap, 6) == 0);          // FromDS -> addr2

    auto m2 = parseEapolKey(buf, buildEapolKey(buf, false, ap, kKiM2));
    CHECK(m2.isEapolKey && m2.msg == 2);
    CHECK(std::memcmp(m2.bssid, ap, 6) == 0);          // ToDS -> addr1

    auto m3 = parseEapolKey(buf, buildEapolKey(buf, true, ap, kKiM3));
    CHECK(m3.isEapolKey && m3.msg == 3);

    auto m4 = parseEapolKey(buf, buildEapolKey(buf, false, ap, kKiM4));
    CHECK(m4.isEapolKey && m4.msg == 4);

    // A group-rekey EAPOL-Key (no pairwise bit) parses but isn't a 4-way msg.
    auto grp = parseEapolKey(buf, buildEapolKey(buf, true, ap, 0x0380));  // no 0x08
    CHECK(grp.isEapolKey && grp.msg == 0);
}

// Frames that must NOT classify as EAPOL: a non-EAPOL ethertype, a management
// frame, and a buffer truncated before the EAPOL header. None may overrun `len`.
static void test_eapol_parse_rejects_non_eapol() {
    uint8_t buf[64];
    const uint8_t ap[6] = {0x02, 0, 0, 0, 0, 1};
    // Valid EAPOL frame, then stomp the ethertype to IPv4 (0x0800).
    uint32_t n = buildEapolKey(buf, true, ap, kKiM1);
    buf[24 + 6] = 0x08; buf[24 + 7] = 0x00;
    CHECK(!parseEapolKey(buf, n).isEapolKey);

    // Management frame (type 0) is never EAPOL.
    n = buildEapolKey(buf, true, ap, kKiM1);
    buf[0] = 0x00;                        // type/subtype -> mgmt beacon-ish
    CHECK(!parseEapolKey(buf, n).isEapolKey);

    // Truncated to the MAC header only: no LLC/EAPOL to read.
    CHECK(!parseEapolKey(buf, 24).isEapolKey);
    // Empty / null are safe.
    CHECK(!parseEapolKey(buf, 0).isEapolKey);
    CHECK(!parseEapolKey(nullptr, 64).isEapolKey);
}

// HandshakeTracker fires exactly once per BSSID, on the transition where the
// capture first becomes crackable (M2 + a neighbour), and never for a lone
// message or a repeat. Distinct APs are tracked independently.
static void test_handshake_tracker_first_crackable() {
    HandshakeTracker t;
    const uint8_t a[6] = {0x02, 0, 0, 0, 0, 0xA1};
    const uint8_t b[6] = {0x02, 0, 0, 0, 0, 0xB2};

    CHECK(!t.observe(a, 1));              // M1 alone: not yet crackable
    CHECK(t.observe(a, 2));               // M1+M2: first crackable -> true
    CHECK(!t.observe(a, 3));              // already credited -> never again
    CHECK(!t.observe(a, 2));

    // A different AP needs its own pair; M2 alone isn't enough.
    CHECK(!t.observe(b, 2));
    CHECK(t.observe(b, 3));               // M2+M3 also crackable
    CHECK(t.trackedAps() == 2);

    // A msg of 0 (non-4-way) is inert.
    CHECK(!t.observe(a, 0));

    t.reset();
    CHECK(t.trackedAps() == 0);
    CHECK(!t.observe(a, 2));              // fresh state: M2 alone not crackable
    CHECK(t.observe(a, 1));               // now crackable again
}

// Bounded arming (RF power budget): an Armed session that never captures
// self-seals after kAuditArmWindowMs down the normal seal->cooldown path, and
// re-arms afterwards because the toggle intent is untouched (a listen/sleep duty
// cycle instead of an open-ended radio drain).
static void test_audit_capture_arm_window_self_seals() {
    AuditCapture ac;
    ac.setEnabled(true, 0);
    CHECK(ac.phase() == AuditPhase::Armed);
    ac.tick(kAuditArmWindowMs / 2);           // mid-window: still listening
    CHECK(ac.phase() == AuditPhase::Armed);
    ac.tick(kAuditArmWindowMs);                // window elapsed -> seal
    CHECK(ac.phase() == AuditPhase::Cooldown);
    CHECK(ac.enabled());                       // toggle intent preserved
    // Re-arms once the cooldown clears (toggle still on) -> a NEW listen window.
    const uint32_t rearm = kAuditArmWindowMs + kAuditRearmCooldownMs;
    ac.tick(rearm);
    CHECK(ac.phase() == AuditPhase::Armed);
    ac.tick(rearm + kAuditArmWindowMs - 1);    // window measured from the re-arm
    CHECK(ac.phase() == AuditPhase::Armed);
    ac.tick(rearm + kAuditArmWindowMs);
    CHECK(ac.phase() == AuditPhase::Cooldown);
}

// The low-battery guard seam: sealActive() seals from EITHER Armed or Hot while
// keeping the toggle intent (unlike toggle-off), so a pack that recovers re-arms.
static void test_audit_capture_seal_active_from_armed() {
    AuditCapture ac;
    ac.setEnabled(true, 0);
    CHECK(ac.phase() == AuditPhase::Armed);
    ac.sealActive(500);                        // battery dipped while merely armed
    CHECK(ac.phase() == AuditPhase::Cooldown);
    CHECK(ac.enabled());                       // intent kept -> re-arms after cooldown
    ac.tick(500 + kAuditRearmCooldownMs);
    CHECK(ac.phase() == AuditPhase::Armed);

    // Also seals a Hot broadcast; a no-op when already disarmed/cooling.
    AuditCapture ac2;
    ac2.sealActive(0);                         // Disarmed -> nothing to seal
    CHECK(ac2.phase() == AuditPhase::Disarmed);
    ac2.setEnabled(true, 0);
    ac2.noteHandshake(100);
    CHECK(ac2.broadcasting());
    ac2.sealActive(200);
    CHECK(ac2.phase() == AuditPhase::Cooldown && !ac2.broadcasting());
}

// registerHandshake is the device-capture seam: SHAKES counts DISTINCT
// handshakes, so one bump per new BSSID and none for a repeat (walking past a
// known AP again). Mirrors registerNetwork's dedup, on a separate ledger.
static void test_register_handshake_dedup() {
    Game g{StartMode::Hatched};
    CHECK(g.handshakesSeen() == 0);

    uint8_t a[6]  = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01};
    uint8_t a2[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x02};
    CHECK(g.registerHandshake(a));       // new AP -> credited
    CHECK(g.handshakesSeen() == 1);
    CHECK(!g.registerHandshake(a));      // same AP -> not re-credited
    CHECK(g.handshakesSeen() == 1);
    CHECK(g.registerHandshake(a2));      // different AP -> credited
    CHECK(g.handshakesSeen() == 2);

    // The handshake ledger is independent of the NETS ledger.
    CHECK(g.networksSeen() == 0);
}

// The core "leave the house and come home" requirement, for SHAKES: the
// handshake dedup ledger PERSISTS (save v7), so a reboot never re-credits a
// handshake already captured. (NETS/registerNetwork no longer has an equivalent
// save-blob guarantee — real-network dedup/history moved to the SD-backed
// NetworkLedger, which is file-backed, not save-blob-backed; registerNetwork's
// pending queue is deliberately ephemeral, see game.h's comment on it. Native
// tests don't exercise real SD file I/O — see forgeLegacyNetworkBytes's sibling
// note and net_capture.h's own documented host/device testing boundary.)
static void test_audit_ledgers_persist_no_recredit() {
    MemSaveStore store;
    uint8_t shake[6] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x22};
    {
        Game g(StartMode::Hatched, "paypup", &store);
        g.setAuditCaptureEnabled(true);
        CHECK(g.registerHandshake(shake));
        CHECK(g.handshakesSeen() == 1);
        g.tick(kSaveAutosaveMs + kHeartbeatMs);   // flush the debounced write
    }
    {
        Game g(StartMode::Hatched, "paypup", &store);
        // The counter survived the reboot...
        CHECK(g.handshakesSeen() == 1);
        // ...and the dedup ledger came back with it: coming home past the same
        // AP grants NO new credit.
        CHECK(!g.registerHandshake(shake));
        CHECK(g.handshakesSeen() == 1);
    }
}

// A v6 blob (no v7 SHAKES-ledger tail) still loads: the ledger comes back empty +
// handshakesSeen 0 (the pre-v7 behaviour), v6 fields survive intact.
static void test_save_v6_migration_defaults_ledgers_empty() {
    SaveData a;
    std::strcpy(a.activeId, "paypup");
    a.auditCaptureEnabled = 1;
    a.handshakesSeen = 9;                     // set the v7 field, then strip the tail
    a.seenHandshakeBssids.push_back(0x5678);
    // Rebuild a v6-only blob: re-serialize with the tail intent removed is fiddly,
    // so instead force the version word to 6 — deserialize then stops after the v6
    // tail and never reads the v7 ledger (leftover bytes are ignored). Target
    // version 6 is still >= 4, so the v4 splice is needed (just not v7's).
    auto blob = forgeLegacyNetworkBytes(a, 6);
    blob[4] = 6; blob[5] = 0;                 // version word -> 6

    SaveData out;
    CHECK(deserializeSave(blob, out));
    CHECK(out.handshakesSeen == 0);          // absent tail -> 0
    CHECK(out.seenHandshakeBssids.empty());
    CHECK(out.auditCaptureEnabled == 1);     // v6 field intact
}

// v7 round-trip: the SHAKES ledger + handshake count serialize and restore
// byte-exact (the persistence contract behind test_audit_ledgers_persist).
static void test_save_v7_ledger_roundtrip() {
    SaveData a;
    std::strcpy(a.activeId, "paypup");
    a.handshakesSeen = 3;
    a.seenHandshakeBssids = {0xABCDEFULL, 0x010203040506ULL};
    auto blob = serializeSave(a);

    SaveData out;
    CHECK(deserializeSave(blob, out));
    CHECK(out.handshakesSeen == 3);
    CHECK(out.seenHandshakeBssids == a.seenHandshakeBssids);
}

// pcap-blowup fix: filename derivation + discovery rewards --------

// buildPcapFilename (pcap_naming.h) is the pure piece net_capture.h uses to name
// a session after the network it captured, instead of a sequential audit_N
// counter (the counter scheme is what minted a fresh file every arm/seal radio
// cycle regardless of whether anything new was heard). A sanitized SSID is
// preferred; the promiscuous EAPOL-only capture path never actually sees one
// (DATA frames only, no beacons), so v1 always falls back to the BSSID hex —
// exercise both paths since the helper supports both.
static void test_pcap_naming_sanitizes_and_falls_back_to_bssid() {
    char name[24];
    // No SSID -> BSSID hex fallback, deterministic + collision-free per network.
    CHECK(sanitizeForFilename(nullptr, name, sizeof(name), 16) == 0);
    CHECK(sanitizeForFilename("", name, sizeof(name), 16) == 0);

    uint8_t bssid[6] = {0xAA, 0xBB, 0xCC, 0x00, 0x11, 0x22};
    char path[48];
    buildPcapFilename(path, sizeof(path), "/sdcard/audit_", bssid, nullptr);
    CHECK(std::strcmp(path, "/sdcard/audit_aabbcc001122.pcap") == 0);

    // A different BSSID -> a different filename (the whole point: named by
    // network, not a counter, so distinct networks never collide/overwrite).
    uint8_t bssid2[6] = {0xAA, 0xBB, 0xCC, 0x00, 0x11, 0x23};
    char path2[48];
    buildPcapFilename(path2, sizeof(path2), "/sdcard/audit_", bssid2, nullptr);
    CHECK(std::strcmp(path2, "/sdcard/audit_aabbcc001123.pcap") == 0);
    CHECK(std::strcmp(path, path2) != 0);

    // Sanitized SSID path: named after the network, suffixed with the last 3
    // BSSID octets so two APs sharing an SSID stay on distinct files.
    char ssidName[24];
    CHECK(sanitizeForFilename("My Home AP!", ssidName, sizeof(ssidName), 16) > 0);
    CHECK(std::strcmp(ssidName, "My_Home_AP_") == 0);
    char ssidPath[48];
    buildPcapFilename(ssidPath, sizeof(ssidPath), "/sdcard/audit_", bssid, "My Home AP!");
    CHECK(std::strcmp(ssidPath, "/sdcard/audit_My_Home_AP__001122.pcap") == 0);

    // Same SSID, different BSSID -> different file (the suffix disambiguates).
    char ssidPath2[48];
    buildPcapFilename(ssidPath2, sizeof(ssidPath2), "/sdcard/audit_", bssid2, "My Home AP!");
    CHECK(std::strcmp(ssidPath2, "/sdcard/audit_My_Home_AP__001123.pcap") == 0);
    CHECK(std::strcmp(ssidPath, ssidPath2) != 0);

    // A too-long SSID truncates rather than overflowing the output buffer.
    char longName[8];
    const size_t n = sanitizeForFilename("ThisIsWayTooLongForTheBuffer", longName,
                                         sizeof(longName), 16);
    CHECK(n == sizeof(longName) - 1);      // clamped by outCap, not maxLen
    CHECK(std::strlen(longName) == n);
}

// Game::handshakeAlreadyCaptured is the read-only query net_capture.h checks
// BEFORE opening a file for a BSSID — it must mirror registerHandshake's dedup
// ledger exactly (same networks report captured/not-captured) and never mutate
// state (repeated queries don't grant credit or rewards on their own).
static void test_handshake_already_captured_query() {
    Game g{StartMode::Hatched};
    uint8_t a[6] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    uint8_t b[6] = {0x11, 0x12, 0x13, 0x14, 0x15, 0x16};

    CHECK(!g.handshakeAlreadyCaptured(a));
    CHECK(!g.handshakeAlreadyCaptured(b));
    CHECK(g.handshakesSeen() == 0);

    CHECK(g.registerHandshake(a));             // credits `a`
    CHECK(g.handshakeAlreadyCaptured(a));       // now reports captured
    CHECK(!g.handshakeAlreadyCaptured(b));      // `b` still not captured

    // Querying is side-effect-free: repeating it doesn't change SHAKES or the
    // inventory (no reward re-fires from a query alone).
    const int shakes = g.handshakesSeen();
    const int cacheBefore = g.inventory().count("sealed_cache_uncommon") +
                            g.inventory().count("sealed_cache_rare") +
                            g.inventory().count("sealed_cache_epic");
    for (int i = 0; i < 5; ++i) CHECK(g.handshakeAlreadyCaptured(a));
    CHECK(g.handshakesSeen() == shakes);
    CHECK(g.inventory().count("sealed_cache_uncommon") +
          g.inventory().count("sealed_cache_rare") +
          g.inventory().count("sealed_cache_epic") == cacheBefore);
}

// A genuinely new network reward fires EXACTLY ONCE per BSSID (the ledger's
// new-vs-repeat split gates it, resolveNetworkDiscovery), never on a repeat
// sighting. Verified via netDiscoveryFlavor_ (set ONLY by resolveNetworkDiscovery,
// so it's immune to a confound every other numeric signal here has: a
// walkToWifiEvent search can incidentally pass through OTHER explore steps
// that grant the SAME item ids — a "Sealed Cache find" EXPL event
// (resolveCacheEvent, unrelated to network discovery) also drops
// sealed_cache_common/uncommon via its own weighted roll, so a raw inventory
// count can't reliably isolate MY reward across a real walk. The cache-count
// check below is intentionally a lower bound (>=), not exact, for the same
// reason — it still catches a reward that never fires at all.
static void test_network_discovery_reward_fires_once_per_network() {
    Game g{StartMode::Hatched};
    auto cacheTotal = [&]() {
        return g.inventory().count("sealed_cache_common") +
               g.inventory().count("sealed_cache_uncommon");
    };
    CHECK(cacheTotal() == 0);

    uint8_t a[6] = {0x40, 0, 0, 0, 0, 0x01};
    queueAndReachWifiEvent(g, a, "A");
    CHECK(std::strstr(g.netDiscoveryFlavor(), "ADMIRING") ||
          std::strstr(g.netDiscoveryFlavor(), "ENVIES"));   // genuinely-new flavor
    CHECK(cacheTotal() >= 1);                                // reward granted
    resolveWifiEventToIdle(g);

    // Repeat sightings of the same BSSID resolve as familiar/home-turf (only
    // one entry exists so far, so it trivially reads as "top favorites" —
    // see test_network_ledger_in_top_n's comment) — never a second NEW-network
    // reward, confirmed by the flavor never reading as genuinely-new again.
    for (int i = 0; i < 3; ++i) {
        queueAndReachWifiEvent(g, a, "A");
        CHECK(std::strstr(g.netDiscoveryFlavor(), "TIRED OF") != nullptr);
        resolveWifiEventToIdle(g);
    }

    uint8_t b[6] = {0x40, 0, 0, 0, 0, 0x02};
    const int cacheBeforeB = cacheTotal();
    queueAndReachWifiEvent(g, b, "B");
    CHECK(std::strstr(g.netDiscoveryFlavor(), "ADMIRING") ||
          std::strstr(g.netDiscoveryFlavor(), "ENVIES"));   // a second, DIFFERENT network
    CHECK(cacheTotal() >= cacheBeforeB + 1);
    resolveWifiEventToIdle(g);
}

// The network-discovery reward rolls 70% common / 30% uncommon
// (kNetDiscoveryCacheCommonPct/…UncommonPct) — both tiers must actually be
// reachable (catches a wiring bug that always picks one tier). A precise
// percentage check isn't reliable here: a walkToWifiEvent search can
// incidentally pass through the unrelated "Sealed Cache find" EXPL event
// (resolveCacheEvent), which grants the SAME sealed_cache_common/uncommon ids
// via its own weighted roll, skewing any raw inventory-count ratio measured
// across many real walks (see test_network_discovery_reward_fires_once_per_network's
// comment for the same confound). Classify each sample by its flavor line
// instead — set exclusively by resolveNetworkDiscovery, immune to that noise.
static void test_network_discovery_reward_rarity_ratio() {
    Game g{StartMode::Hatched};
    const int kSamples = 60;
    int common = 0, uncommon = 0;
    for (int i = 0; i < kSamples; ++i) {
        uint8_t bssid[6] = {0x50, static_cast<uint8_t>(i >> 8), static_cast<uint8_t>(i),
                            0, 0, 0};
        const int commonBefore = g.inventory().count("sealed_cache_common");
        const int uncommonBefore = g.inventory().count("sealed_cache_uncommon");
        queueAndReachWifiEvent(g, bssid, "S");
        CHECK(std::strstr(g.netDiscoveryFlavor(), "ADMIRING") ||
              std::strstr(g.netDiscoveryFlavor(), "ENVIES"));
        if (g.inventory().count("sealed_cache_common") > commonBefore) ++common;
        else if (g.inventory().count("sealed_cache_uncommon") > uncommonBefore) ++uncommon;
        resolveWifiEventToIdle(g);
    }
    CHECK(common > 0 && uncommon > 0);   // both tiers reachable
    CHECK(common + uncommon == kSamples);
}

// A genuinely new CAPTURED HANDSHAKE reward fires exactly once per BSSID, mirrors
// the network-discovery test above but on registerHandshake()'s ledger (SHAKES).
static void test_handshake_capture_reward_fires_once_per_handshake() {
    Game g{StartMode::Hatched};
    auto cacheTotal = [&]() {
        return g.inventory().count("sealed_cache_uncommon") +
               g.inventory().count("sealed_cache_rare") +
               g.inventory().count("sealed_cache_epic");
    };
    CHECK(cacheTotal() == 0);

    uint8_t a[6] = {0x60, 0, 0, 0, 0, 0x01};
    CHECK(g.registerHandshake(a));
    CHECK(cacheTotal() == 1);
    for (int i = 0; i < 10; ++i) CHECK(!g.registerHandshake(a));   // repeats: no-op
    CHECK(cacheTotal() == 1);

    uint8_t b[6] = {0x60, 0, 0, 0, 0, 0x02};
    CHECK(g.registerHandshake(b));
    CHECK(cacheTotal() == 2);

    // Never grants the plain Common tier — a captured handshake is the rarer,
    // harder-earned event than a bare network sighting.
    CHECK(g.inventory().count("sealed_cache_common") == 0);
}

// The handshake-capture reward rolls 50% uncommon / 30% rare / 20% epic
// (kHandshakeCaptureCacheUncommonPct/…RarePct/…EpicPct).
static void test_handshake_capture_reward_rarity_ratio() {
    Game g{StartMode::Hatched};
    // Stays comfortably under kBssidDedupCap (256), same reasoning as the
    // network-discovery ratio test above.
    const int kSamples = 240;
    for (int i = 0; i < kSamples; ++i) {
        uint8_t bssid[6] = {0x70, static_cast<uint8_t>(i >> 16),
                            static_cast<uint8_t>(i >> 8), static_cast<uint8_t>(i),
                            0, 0};
        CHECK(g.registerHandshake(bssid));
    }
    const int uncommon = g.inventory().count("sealed_cache_uncommon");
    const int rare = g.inventory().count("sealed_cache_rare");
    const int epic = g.inventory().count("sealed_cache_epic");
    CHECK(uncommon + rare + epic == kSamples);
    const double uncommonPct = 100.0 * uncommon / kSamples;
    const double rarePct = 100.0 * rare / kSamples;
    const double epicPct = 100.0 * epic / kSamples;
    CHECK(uncommonPct > 40.0 && uncommonPct < 60.0);
    CHECK(rarePct > 20.0 && rarePct < 40.0);
    CHECK(epicPct > 10.0 && epicPct < 30.0);
}

// Move drop tables + malbeast roster depth -------

// MoveLoadout::grant is the guard the wild-win move-drop roll relies on
// (game.cpp): granting an already-owned move must be a no-op, never a
// duplicate entry in owned(). Unit-level, not a game loop — the property
// belongs to MoveLoadout itself, independent of any RNG.
static void test_move_loadout_grant_no_duplicate() {
    MoveLoadout mv = MoveLoadout::starting();
    CHECK(!mv.owns("buffer_overflow"));
    mv.grant("buffer_overflow");
    CHECK(mv.owns("buffer_overflow"));
    const int n = static_cast<int>(mv.owned().size());
    mv.grant("buffer_overflow");                   // already owned -> no-op
    CHECK(static_cast<int>(mv.owned().size()) == n);
}

// The wild-win reward path (game.cpp applyCombatResult) wires an independent
// move-drop roll (kWildMoveDropPct) right next to the existing item-drop roll.
// Reuses walkToAnyCombat (defined above) in a bounded loop — it's driven
// entirely by live nav state (each explore step is a guaranteed event), so
// it composes across repeated calls on the same Game. Searches for a "LEARNED ..."
// log entry, confirming the roll is actually wired, not just present in the pool.
static void test_wild_win_can_drop_a_move() {
    Game g{StartMode::Hatched};
    enterWalk(g);                                        // arm explore-mode first
    bool learned = false;
    for (int tries = 0; tries < 20 && !learned; ++tries) {
        walkToAnyCombat(g);
        if (g.nav() != Game::Nav::Combat) break;        // search exhausted
        uint32_t t = 0;
        for (int j = 0; j < 400 && g.combat().outcome() == Combat::Outcome::Ongoing; ++j)
            g.tick(t += kHeartbeatMs);
        g.onButton(press(Button::B));                    // dismiss -> apply reward
        for (int k = 0; k < g.log().size(); ++k) {
            const LogEntry& e = g.log().at(k);
            if (e.type == LogEventType::ItemGained &&
                std::strncmp(e.text, "LEARNED ", 8) == 0) {
                learned = true;
                break;
            }
        }
    }
    CHECK(learned);
}

// wildMalbeast now rolls among 2 variants per sector tier instead of
// returning one fixed enemy — a pure-function check, no Game needed. Default
// variantRoll=0 (used by test_combat_force_enemy_first above) keeps returning
// the original tier-1 enemy, so that gate is unaffected.
static void test_wild_malbeast_roster_variants() {
    CHECK(std::strcmp(wildMalbeast(1, 0).name, "GlitchHog") == 0);
    CHECK(std::strcmp(wildMalbeast(1, 0).name, wildMalbeast(1, 1).name) != 0);
    CHECK(std::strcmp(wildMalbeast(2, 0).name, wildMalbeast(2, 1).name) != 0);
    CHECK(std::strcmp(wildMalbeast(3, 0).name, wildMalbeast(3, 1).name) != 0);
}

// Namespace-disjoint guard: the wild-malbeast name pool must not
// intersect the raised roster's display names. A wild enemy that shares a name
// with a petware/malbeast in the / roster reads to a player as the same
// creature — this gate fails the instant a name collides, whichever side a future
// name is added on.
static void test_wild_and_roster_names_disjoint() {
    ContentRegistry r = ContentRegistry::embedded();
    const auto roster = r.allCreatures();
    CHECK(!roster.empty());                       // guard against a vacuous pass
    for (int tier = 1; tier <= 3; ++tier)         // whole wild pool: 2 variants x 3 tiers
        for (uint32_t v = 0; v < 2; ++v) {
            const char* wild = wildMalbeast(tier, v).name;
            for (const CreatureDef* c : roster)
                CHECK(std::strcmp(wild, c->displayName) != 0);
        }
    // Boss enemy names are a third pool — hold the same invariant so a sub-area
    // boss (and the area gauntlet built from them) never reads as a roster creature the
    // player might raise. Cover every sub-area boss across both areas.
    for (int a = 0; a < kExplSectors; ++a)
        for (int sub = 0; sub < kExplSubAreas; ++sub)
            for (const CombatEnemy& round : subAreaBoss(a, sub).rounds)
                for (const CreatureDef* c : roster)
                    CHECK(std::strcmp(round.name, c->displayName) != 0);
}

// Evolution routing: a Script->Daemon weighted pool is the one hop that is NOT on
// the creature row, so it is the one the registry has to answer for. A Script with a
// pool draws from it per care-branch; everything else (linear hops, care branches
// without a pool, termini) reads its own row, which is what the chain tests walk.
static void test_evolution_routing_tables() {
    ContentRegistry r = ContentRegistry::embedded();
    // Script->Daemon: the care-branch selects the pool; single entry today.
    const DaemonPoolDef* good = r.daemonPool("malbear", /*bad=*/false);
    const DaemonPoolDef* bad = r.daemonPool("malbear", /*bad=*/true);
    CHECK(good && good->count == 1 &&
          std::strcmp(good->entries[0].daemonId, "bruinforce") == 0);
    CHECK(bad && bad->count == 1 &&
          std::strcmp(bad->entries[0].daemonId, "berserkernel") == 0);
    // A pooled Script's row must still agree with its pool — the row is what the
    // registry falls back to, so a disagreement would be a silent routing change.
    const CreatureDef* malbear = r.creature("malbear");
    CHECK(malbear && std::strcmp(malbear->evolvesToGoodId, "bruinforce") == 0);
    CHECK(malbear && std::strcmp(malbear->evolvesToBadId, "berserkernel") == 0);
    // No pool for anything else: the row is the whole answer.
    CHECK(r.daemonPool("paypup", false) == nullptr);
    CHECK(r.daemonPool("barkmail", false) == nullptr);
}

// The dominant signal is computed from live care interactions: a
// fresh pet is Balanced (no interactions); feeding it tips the balance to
// Feeding. The tally that answers this is the input a signal-dependent evolution
// route would key off; nothing routes on it today, so this is what keeps it honest.
static void test_dominant_signal_from_care() {
    Game g{StartMode::Hatched};                      // Paypup, no interactions yet
    CHECK(g.dominantSignal() == DominantSignal::Balanced);
    g.model().setHunger(40);
    enterSlot(g, SubmenuId::Items);
    g.onButton(press(Button::B));                    // open first food's detail
    g.onButton(press(Button::B));                    // Use -> feed (tally Feeding)
    CHECK(g.nav() == Game::Nav::ModalFeeding);
    CHECK(g.dominantSignal() == DominantSignal::Feeding);
}

// Battery SoC mapping (CFG "BATT" line): the portable helper the device reader
// leans on. Endpoints, clamping (a charging pack can read >4.2V), and monotonicity.
static void test_battery_percent_from_mv() {
    CHECK(batteryPercentFromMilliVolts(4200) == 100);   // full
    CHECK(batteryPercentFromMilliVolts(3300) == 0);     // empty
    CHECK(batteryPercentFromMilliVolts(5000) == 100);   // clamp high (on charger)
    CHECK(batteryPercentFromMilliVolts(3000) == 0);     // clamp low
    const int mid = batteryPercentFromMilliVolts(3750); // ~halfway
    CHECK(mid >= 45 && mid <= 55);
    int prev = -1;                                       // non-decreasing across range
    for (int mv = 3000; mv <= 4300; mv += 50) {
        const int p = batteryPercentFromMilliVolts(mv);
        CHECK(p >= prev);
        prev = p;
    }
}

// Web 'Pedia slice -----------------------------------

// Game::setHackerTag: the one safe write the web 'Pedia is allowed. Validates
// length (1..kHackerTagMax) and charset (A-Z0-9_) before mutating, and a reject
// must leave the live tag untouched.
static void test_set_hacker_tag_validates() {
    Game g{StartMode::Hatched};
    const std::string original = g.hackerTag();

    CHECK(g.setHackerTag("NEWTAG_1"));
    CHECK(std::strcmp(g.hackerTag(), "NEWTAG_1") == 0);

    // Reject: empty.
    CHECK(!g.setHackerTag(""));
    CHECK(std::strcmp(g.hackerTag(), "NEWTAG_1") == 0);   // unchanged

    // Reject: too long (> kHackerTagMax).
    std::string tooLong(kHackerTagMax + 1, 'A');
    CHECK(!g.setHackerTag(tooLong.c_str()));
    CHECK(std::strcmp(g.hackerTag(), "NEWTAG_1") == 0);

    // Reject: bad charset (lowercase, punctuation, space).
    CHECK(!g.setHackerTag("lower"));
    CHECK(!g.setHackerTag("BAD-TAG"));
    CHECK(!g.setHackerTag("HAS SPACE"));
    CHECK(std::strcmp(g.hackerTag(), "NEWTAG_1") == 0);

    // Exactly kHackerTagMax chars is the boundary — must be accepted.
    std::string maxLen(kHackerTagMax, 'Z');
    CHECK(g.setHackerTag(maxLen.c_str()));
    CHECK(std::strcmp(g.hackerTag(), maxLen.c_str()) == 0);

    CHECK(g.setHackerTag(original.c_str()));  // restore, tidy
}

// buildPediaStateJson: on a known (Hatched) state, the payload is well-formed
// enough to eyeball (balanced braces) and contains the fields a live device
// reload most needs — the tag, the bits, and the active pet's own "hatched"
// entry in `pets`.
static void test_pedia_state_json_shape() {
    Game g{StartMode::Hatched, "paypup"};
    CHECK(g.setHackerTag("NETRUNNER_9"));

    const std::string json = buildPediaStateJson(g);
    CHECK(!json.empty());
    CHECK(json.front() == '{' && json.back() == '}');

    // Braces/brackets balance (a cheap parseable-ness smoke check without
    // pulling in a JSON library).
    int braces = 0, brackets = 0;
    for (char c : json) {
        if (c == '{') ++braces;
        else if (c == '}') --braces;
        else if (c == '[') ++brackets;
        else if (c == ']') --brackets;
    }
    CHECK(braces == 0 && brackets == 0);

    CHECK(json.find("\"hacker_tag\":\"NETRUNNER_9\"") != std::string::npos);
    CHECK(json.find("\"currency_bits\":") != std::string::npos);
    CHECK(json.find("\"active_pet\":{") != std::string::npos);
    CHECK(json.find("\"species\":\"paypup\"") != std::string::npos);
    // The active pet must reveal as "hatched" in the pets{} map.
    CHECK(json.find("\"paypup\":\"hatched\"") != std::string::npos);
    // A never-touched creature in the same line stays locked.
    CHECK(json.find("\"bruinforce\":\"locked\"") != std::string::npos);
    // A never-encountered malbeast stays locked; every achievement bit starts
    // incomplete on a fresh Hatched-seam Game.
    CHECK(json.find("\"glitchhog\":\"locked\"") != std::string::npos);
    CHECK(json.find("\"DEVTOOLS_INTRUDER\":\"incomplete\"") != std::string::npos);
}

// A fresh boot (FreshHatch — the real "empty save" first-boot path,
// redesign) lays the Boot-Sector egg immediately rather than leaving pet()
// null, so this exercises the "unhatched egg" shape rather than JSON null.
// buildPediaStateJson defensively emits `null` if pet() ever IS nullptr (no
// public path constructs that today, per Game::startHatch's every branch
// assigning pet_), so this gate covers the realistic case instead.
static void test_pedia_state_json_fresh_hatch_egg() {
    Game g{StartMode::FreshHatch};
    pickFirstEggLine(g);
    CHECK(g.pet() != nullptr);          // the egg IS a creature (CryptoShell)
    CHECK(g.inEggPhase());
    const std::string json = buildPediaStateJson(g);
    CHECK(json.find("\"active_pet\":{") != std::string::npos);
    CHECK(json.find("\"species\":\"cryptoshell\"") != std::string::npos);
    CHECK(json.find("\"stage\":\"BOOT SECTOR\"") != std::string::npos);
    CHECK(json.find("\"cryptoshell\":\"hatched\"") != std::string::npos);
}

// Save v25: web-'Pedia reveal-state bookkeeping -----

// wildMalbeastIndex: name -> roster index via slugging; anything outside the
// fixed 6-entry roster (a boss/Sim-dummy/unknown name) misses.
static void test_wild_malbeast_index_mapping() {
    CHECK(wildMalbeastIndex("GlitchHog") == 0);
    CHECK(wildMalbeastIndex("Segfault Pup") == 1);
    CHECK(wildMalbeastIndex("Packet Wraith") == 2);
    CHECK(wildMalbeastIndex("Cache Ghoul") == 3);
    CHECK(wildMalbeastIndex("Buffer Wyrm") == 4);
    CHECK(wildMalbeastIndex("Kernel Leviathan") == 5);
    CHECK(wildMalbeastIndex("PHISH PHRY") == -1);     // a sub-area boss name
    CHECK(wildMalbeastIndex("Basic Dummy") == -1);    // a Sim dummy
    CHECK(wildMalbeastIndex("Lethal") == -1);         // the debug unbeatable enemy
    CHECK(wildMalbeastIndex(nullptr) == -1);
}

// Save v24 -> v25: a pre-v25 blob (no tail) loads with seenCreatures empty and both
// malbeast masks + the achievements mask at 0 (mirrors the v20->v21 shield-default
// pattern) — the honest default for a save that predates this system.
static void test_save_v24_to_v25_pedia_defaults() {
    SaveData a; std::strcpy(a.activeId, "paypup"); a.generation = 1;
    a.seenCreatures.push_back(SaveId{"malbear"});   // would round-trip IF the tail were read
    a.malbeastSeen = 0x3;
    a.malbeastDefeated = 0x1;
    a.achievementsMask = 0x5;
    auto blob = forgeLegacyNetworkBytes(a, 24);
    // Drop the v25 tail: seenCreatures (u16 count + 1*kSaveIdCap) + 2 malbeast u16s
    // + the achievements u32.
    const size_t v25TailBytes = 2 + kSaveIdCap + 2 + 2 + 4;
    blob.resize(blob.size() - v25TailBytes);
    blob[4] = 24; blob[5] = 0;                  // stamp the version word down to 24
    SaveData out;
    CHECK(deserializeSave(blob, out));          // a v24 blob still deserializes
    CHECK(out.seenCreatures.empty());
    CHECK(out.malbeastSeen == 0);
    CHECK(out.malbeastDefeated == 0);
    CHECK(out.achievementsMask == 0);
}

// Save v25 round-trip: seenCreatures + both malbeast masks survive a raw
// serialize/deserialize cycle, and a real Game::captureSave -> persistSave -> reload
// round-trips the same state through the public API. (The v25 achievements u32 that used
// to ride here is no longer written — v40 replaced it with a wire-indexed bitset, whose
// own round-trip and migration are gated separately.)
static void test_save_v25_roundtrip() {
    SaveData a; std::strcpy(a.activeId, "paypup"); a.generation = 1;
    a.seenCreatures.push_back(SaveId{"malbear"});
    a.seenCreatures.push_back(SaveId{"berserkernel"});
    a.malbeastSeen = 0x2A;       // bits 1, 3, 5
    a.malbeastDefeated = 0x01;   // bit 0
    SaveData out;
    CHECK(deserializeSave(serializeSave(a), out));
    CHECK(out.seenCreatures.size() == 2);
    CHECK(std::strcmp(out.seenCreatures[0].id, "malbear") == 0);
    CHECK(std::strcmp(out.seenCreatures[1].id, "berserkernel") == 0);
    CHECK(out.malbeastSeen == 0x2A);
    CHECK(out.malbeastDefeated == 0x01);

    // Game-level round trip through captureSave/persistSave/applySave (mirrors the
    // v24 gate's "Live round-trip" section).
    MemSaveStore store;
    {
        Game g{StartMode::Hatched, "malbear", &store};
        g.markCreatureSeen("berserkernel");
        g.unlockAchievement("BIT_BARON");
        g.tick(kSaveAutosaveMs + kHeartbeatMs);   // autosave
    }
    Game g2(StartMode::Hatched, "paypup", &store);   // hatchedCreature ignored: store wins
    CHECK(g2.creatureSeen("berserkernel"));
    CHECK(g2.hasAchievement("BIT_BARON"));
}

// v27 — the rig-upgrade levels round-trip, and a pre-v27 blob (no tail)
// migrates all three to 0 (a migrated save has bought none).
static void test_save_v27_roundtrip() {
    SaveData a; std::strcpy(a.activeId, "paypup"); a.generation = 1;
    a.rackSlotUpgradeCount = 3;
    a.scrapingClusterLevel = 7;
    a.dataMiningLevel = 12;
    SaveData out;
    CHECK(deserializeSave(serializeSave(a), out));
    CHECK(out.rackSlotUpgradeCount == 3);
    CHECK(out.scrapingClusterLevel == 7);
    CHECK(out.dataMiningLevel == 12);

    std::vector<uint8_t> blob = forgeLegacyNetworkBytes(a, 26);
    blob.resize(blob.size() - 3 * 4);   // drop the v27 tail (3 i32s)
    blob[4] = 26; blob[5] = 0;          // stamp the version word down to 26
    SaveData migrated;
    CHECK(deserializeSave(blob, migrated));   // a v26 blob still deserializes
    CHECK(migrated.rackSlotUpgradeCount == 0);
    CHECK(migrated.scrapingClusterLevel == 0);
    CHECK(migrated.dataMiningLevel == 0);

    // Game-level round trip through captureSave/persistSave/applySave.
    MemSaveStore store;
    {
        Game g{StartMode::Hatched, "malbear", &store};
        g.debugSetBits(100000000);
        g.debugBuyRackSlotUpgrade();
        g.debugBuyScrapingCluster();
        g.debugBuyDataMining();
        g.tick(kSaveAutosaveMs + kHeartbeatMs);   // autosave
    }
    Game g2(StartMode::Hatched, "paypup", &store);
    CHECK(g2.rackSlotUpgradeCount() == 1);
    CHECK(g2.scrapingClusterLevel() == 1);
    CHECK(g2.dataMiningLevel() == 1);
}

// v32 — rigLevelsExt (Rig Shop rows beyond the legacy 11) round-trips, and a
// pre-v32 blob (no tail) migrates it to empty (every such row unbought).
static void test_save_v32_roundtrip() {
    SaveData a; std::strcpy(a.activeId, "paypup"); a.generation = 1;
    a.rigLevelsExt = {5, 9};
    SaveData out;
    CHECK(deserializeSave(serializeSave(a), out));
    CHECK(out.rigLevelsExt.size() == 2 && out.rigLevelsExt[0] == 5 && out.rigLevelsExt[1] == 9);

    std::vector<uint8_t> blob = forgeLegacyNetworkBytes(a, 31);
    blob.resize(blob.size() - 2 * 2 - 2);   // drop the v32 tail (u16 size + 2 u16s)
    blob[4] = 31; blob[5] = 0;              // stamp the version word down to 31
    SaveData migrated;
    CHECK(deserializeSave(blob, migrated));  // a v31 blob still deserializes
    CHECK(migrated.rigLevelsExt.empty());
}

// --- Hacker CREW: membership, the home-network gate, and the crew Exploit -------

// The crew Exploit's negation charges absorb whole attacks, exactly `magnitude` of
// them, and are spent BEFORE a passive one-shot (RAID Mirror) so the passive stays
// held for afterwards.
static void test_crew_exploit_negates_next_hits() {
    ContentRegistry r = ContentRegistry::embedded();
    Combatant p = mkCombatant(r, "P", 100, 5, {"quick_jab"});
    p.mods.arm(ModEffect::RaidMirror);          // a passive one-shot armed alongside
    Combatant e = mkCombatant(r, "E", 5000, 10, {"quick_jab"});   // enemy acts first/often
    Combat cb;
    cb.begin(p, e, Combat::Stakes::Safe, 42, /*forceEnemyFirst=*/true,
             /*carryPlayerHealth=*/-1, /*exploitUses=*/1);

    cb.openOverride({}, CrewExploit{"DENIAL OF SERVICE",
                                    CrewExploitKind::NegateNextHits, 3});
    CHECK(cb.overrideCrewRows() == 1);
    const int crewRow = cb.overrideMoveCount();          // no items in this picker
    while (cb.overridePick() != crewRow) cb.cycleOverride();
    cb.commitOverride();
    CHECK(cb.player().crewExploit.charges == 3);
    CHECK(!cb.overrideReady());                          // firing it spends a use

    const int hp0 = cb.player().health;
    int guard = 0;
    while (cb.player().crewExploit.charges > 0 && guard++ < 400) cb.step();
    CHECK(cb.player().health == hp0);                    // all three bounced
    CHECK(cb.player().mods.armed(ModEffect::RaidMirror));  // ...without burning the passive

    // With the charges gone the mirror takes the NEXT hit, and only after that does
    // damage finally reach Health.
    while (cb.player().mods.armed(ModEffect::RaidMirror) && guard++ < 400) cb.step();
    CHECK(cb.player().health == hp0);
    while (cb.player().health == hp0 && guard++ < 400) cb.step();
    CHECK(cb.player().health < hp0);
}

// Enlisting is gated on holding a home network, and membership cannot outlive it.
static void test_crew_requires_home_network() {
    Game g{StartMode::Hatched};
    CHECK(g.crewIndex() == -1 && !g.hasHomeNetwork() && g.activeCrew() == nullptr);

    CHECK(!g.joinCrew(0));                        // no home network -> refused
    CHECK(g.crewIndex() == -1);

    g.setHomeNetwork(0x001122334455ull, "HOME_AP");
    CHECK(g.hasHomeNetwork() && std::strcmp(g.homeNetworkName(), "HOME_AP") == 0);
    CHECK(g.joinCrew(0));
    CHECK(g.crewIndex() == 0);
    CHECK(std::strcmp(g.activeCrew()->id, "deniers_of_service") == 0);
    CHECK(!g.joinCrew(kCrewCount));               // out of range -> refused, no change
    CHECK(g.crewIndex() == 0);

    // Clearing the home network drops the crew with it — a member is always somebody's
    // defender.
    g.setHomeNetwork(0, "");
    CHECK(!g.hasHomeNetwork() && g.crewIndex() == -1);
}

// The CREW screen end-to-end on buttons: designate a home network from the ledger
// the pet built by walking, then enlist. The screen renders in both sub-modes.
static void test_crew_screen_pick_home_then_enlist() {
    Game g{StartMode::Hatched};
    const uint8_t bssid[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    walkAndCreditNetwork(g, bssid, "Neighbour");   // one known network in the ledger

    // Explore-mode claims A+C for its control overlay, so stop exploring first —
    // the Hacker face is only reachable from a quiet habitat.
    while (g.nav() != Game::Nav::Idle) g.onButton(press(Button::C));
    if (g.exploreActive()) {
        g.onButton({Button::A, true, true});        // A+C -> explore control overlay
        g.onButton(press(Button::C));               // C -> stop exploring
    }
    CHECK(!g.exploreActive() && g.nav() == Game::Nav::Idle);
    g.onButton({Button::A, true, true});           // A+C -> hacker face
    CHECK(g.face() == Game::Face::Hacker);
    g.onButton(press(Button::A));
    while (hackerCarouselSlots()[g.cursor()].id != HackerSlotId::Crew)
        g.onButton(press(Button::A));
    CHECK(hackerCarouselSlots()[g.cursor()].accessible);
    g.onButton(press(Button::B));
    CHECK(g.nav() == Game::Nav::Submenu);

    Framebuffer fb(kActiveW, kActiveH);
    g.render(fb);
    CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));

    // Row 0 is HOME NET: B opens the picker, B again takes the focused network.
    g.onButton(press(Button::B));
    g.render(fb);
    CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));
    g.onButton(press(Button::B));
    CHECK(g.hasHomeNetwork());
    CHECK(std::strcmp(g.homeNetworkName(), "Neighbour") == 0);

    // A steps onto the crew row; B enlists, and B again resigns.
    g.onButton(press(Button::A));
    g.onButton(press(Button::B));
    CHECK(g.crewIndex() == 0);
    g.onButton(press(Button::B));
    CHECK(g.crewIndex() == -1);

    g.onButton(press(Button::C));                  // C backs out to the hacker carousel
    CHECK(g.nav() == Game::Nav::Cursor);
}

// The picker answers one question — "which of the networks I can hear RIGHT NOW am I
// claiming?" — so every row is a live in-range sighting. History alone is never an
// offer; the ledger only supplies the walked-in tally that tells the familiar network
// apart from its neighbours' APs.
static void test_crew_picker_offers_networks_in_range() {
    Game g{StartMode::Hatched};
    g.debugSeedNetworkLedger(0x00AAAAAAAAAAull, "WALKED_PAST", 7);   // history only

    Game::CrewNetRow rows[kNetVisibleCap];
    CHECK(g.crewNetworkRows(rows, kNetVisibleCap) == 0);   // walked past != in range now

    const uint8_t fresh[6] = {0x00, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB};   // in range, unwalked
    CHECK(g.registerNetwork(fresh, "IN_RANGE"));
    const uint8_t both[6] = {0x00, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA};    // in range AND walked
    CHECK(g.registerNetwork(both, "WALKED_PAST"));

    const int n = g.crewNetworkRows(rows, kNetVisibleCap);
    CHECK(n == 2);                                   // one row per in-range network
    // Most-walked first — the tally is evidence for the choice, not a source of rows.
    CHECK(std::strcmp(rows[0].name, "WALKED_PAST") == 0 && rows[0].count == 7);
    CHECK(std::strcmp(rows[1].name, "IN_RANGE") == 0 && rows[1].count == 0);

    // A network in range is a real, claimable offer — no walk required first.
    g.setHomeNetwork(rows[1].key, rows[1].name);
    CHECK(g.hasHomeNetwork() && std::strcmp(g.homeNetworkName(), "IN_RANGE") == 0);
}

// The regression the reward-queue/snapshot split exists to prevent: a pending queue
// saturated by a backlog the player never walked off must not hide a network they are
// standing in front of.
static void test_crew_picker_unaffected_by_saturated_reward_queue() {
    Game g{StartMode::Hatched};
    for (int i = 0; i < kPendingNetworkQueueCap; ++i) {
        const uint8_t m[6] = {0x02, 0x03, 0x04, 0x05,
                              static_cast<uint8_t>(i >> 8), static_cast<uint8_t>(i)};
        g.registerNetwork(m, "BACKLOG");
    }
    CHECK(g.pendingNetworks() == kPendingNetworkQueueCap);   // full, nothing resolved

    const uint8_t home[6] = {0x10, 0x20, 0x30, 0x40, 0x50, 0x60};
    g.registerNetwork(home, "HOME_WIFI");

    Game::CrewNetRow rows[kNetVisibleCap];
    const int n = g.crewNetworkRows(rows, kNetVisibleCap);
    bool offered = false;
    for (int i = 0; i < n; ++i)
        if (std::strcmp(rows[i].name, "HOME_WIFI") == 0) offered = true;
    CHECK(offered);
}

// The snapshot is a freshness window, not a lifetime list: a network the radio stops
// hearing ages out of the picker on its own, and is offered again when it comes back.
static void test_crew_picker_drops_networks_out_of_range() {
    Game g{StartMode::Hatched};
    uint32_t t = 1000;
    g.tick(t);
    const uint8_t ap[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    g.registerNetwork(ap, "NEIGHBOUR");
    Game::CrewNetRow rows[kNetVisibleCap];
    CHECK(g.crewNetworkRows(rows, kNetVisibleCap) == 1);
    CHECK(g.visibleNetworkCount() == 1);

    g.tick(t += kNetVisibleFreshMs + 1);                    // stopped hearing it
    CHECK(g.crewNetworkRows(rows, kNetVisibleCap) == 0);
    CHECK(g.visibleNetworkCount() == 0);

    g.registerNetwork(ap, "NEIGHBOUR");                     // back in range
    CHECK(g.crewNetworkRows(rows, kNetVisibleCap) == 1);
}

// Build a wire frame for `hello`, the way another device's beacon would arrive.
static size_t makePeerFrame(const PeerHello& hello, uint8_t* frame) {
    return encodePeerHello(hello, frame, kPeerHelloSize);
}

// A beacon repeats about once a second for as long as two devices sit next to each
// other, so the roster's tally would be meaningless if every frame counted. Only
// the ARRIVAL edge is a meeting: a peer that keeps beaconing stays one encounter,
// and only aging out of the snapshot and coming back counts as meeting them again.
static void test_register_peer_counts_encounters_not_frames() {
    Game g{StartMode::Hatched};
    uint32_t t = 0;
    const uint8_t mac[6] = {0x02, 0x11, 0x22, 0x33, 0x44, 0x55};

    PeerHello hello;
    std::strncpy(hello.tag, "GHOSTBYTE", sizeof(hello.tag) - 1);
    std::strncpy(hello.petName, "Paypup", sizeof(hello.petName) - 1);
    hello.stage = static_cast<uint8_t>(Stage::Process);
    uint8_t frame[kPeerHelloSize];
    CHECK(makePeerFrame(hello, frame) == kPeerHelloSize);

    CHECK(g.registerPeer(mac, frame, kPeerHelloSize));      // true == a new operator
    CHECK(g.livePeerCount() == 1);
    PeerLedger::Entry e;
    CHECK(g.peerLedger().lookup(packPeerKey(mac), &e));
    CHECK(e.timesMet == 1);

    // Ten more beacons across the encounter: still one meeting, still one row.
    for (int i = 0; i < 10; ++i) {
        g.tick(t += 1000);
        CHECK(!g.registerPeer(mac, frame, kPeerHelloSize));  // false == already known
    }
    CHECK(g.peerLedger().lookup(packPeerKey(mac), &e));
    CHECK(e.timesMet == 1);                                  // ...one encounter
    CHECK(g.peerLedger().size() == 1);

    // They walk away — the snapshot ages them out, so the room is empty again.
    g.tick(t += kPeerVisibleFreshMs + 1);
    CHECK(g.livePeerCount() == 0);

    // ...and come back later. THAT is meeting them a second time.
    CHECK(!g.registerPeer(mac, frame, kPeerHelloSize));      // known, but a new meeting
    CHECK(g.peerLedger().lookup(packPeerKey(mac), &e));
    CHECK(e.timesMet == 2);
    CHECK(g.livePeerCount() == 1);
}

// Anything in range can transmit, so a frame that doesn't decode is background
// noise rather than an error: dropped without touching the snapshot or the roster.
static void test_register_peer_ignores_foreign_traffic() {
    Game g{StartMode::Hatched};
    const uint8_t mac[6] = {0x02, 0x11, 0x22, 0x33, 0x44, 0x55};

    uint8_t junk[kPeerHelloSize] = {0};
    CHECK(!g.registerPeer(mac, junk, kPeerHelloSize));      // right length, wrong magic
    CHECK(g.livePeerCount() == 0);
    CHECK(g.peerLedger().size() == 0);

    PeerHello hello;
    std::strncpy(hello.tag, "REAL", sizeof(hello.tag) - 1);
    uint8_t frame[kPeerHelloSize];
    makePeerFrame(hello, frame);
    CHECK(!g.registerPeer(mac, frame, kPeerHelloSize - 2)); // truncated
    CHECK(!g.registerPeer(nullptr, frame, kPeerHelloSize));
    CHECK(g.peerLedger().size() == 0);

    CHECK(g.registerPeer(mac, frame, kPeerHelloSize));      // a real one still lands
    CHECK(g.peerLedger().size() == 1);
}

// The screen merges two sources, and the merge is the interesting part: a live
// operator must appear ONCE, from the fresh snapshot (showing what they're raising
// right now) rather than twice with the roster's older copy underneath.
static void test_peer_rows_live_first_and_deduped() {
    Game g{StartMode::Hatched};
    uint32_t t = 0;

    // Someone met before, not here now.
    PeerHello old;
    std::strncpy(old.tag, "ABSENT", sizeof(old.tag) - 1);
    std::strncpy(old.petName, "Pingcub", sizeof(old.petName) - 1);
    g.debugSeedPeer(0xAAAAAAAAAAAAull, old);

    // Someone standing here, whose pet has evolved since we last met them.
    const uint8_t mac[6] = {0x02, 0, 0, 0, 0, 0x01};
    PeerHello then;
    std::strncpy(then.tag, "PRESENT", sizeof(then.tag) - 1);
    std::strncpy(then.petName, "Paypup", sizeof(then.petName) - 1);
    then.stage = static_cast<uint8_t>(Stage::Process);
    g.debugSeedPeer(packPeerKey(mac), then);

    PeerHello now;
    std::strncpy(now.tag, "PRESENT", sizeof(now.tag) - 1);
    std::strncpy(now.petName, "Malbear", sizeof(now.petName) - 1);
    now.stage = static_cast<uint8_t>(Stage::Script);
    uint8_t frame[kPeerHelloSize];
    makePeerFrame(now, frame);
    g.tick(t += 1000);
    g.registerPeer(mac, frame, kPeerHelloSize);

    Game::PeerRow rows[8];
    const int n = g.peerRows(rows, 8);
    CHECK(n == 2);                                       // two operators, not three
    CHECK(rows[0].live);                                 // the live one leads
    CHECK(std::strcmp(rows[0].tag, "PRESENT") == 0);
    // ...and reads from the fresh beacon, not the roster's last-meeting copy.
    CHECK(std::strcmp(rows[0].petName, "Malbear") == 0);
    CHECK(rows[0].stage == static_cast<uint8_t>(Stage::Script));
    CHECK(!rows[1].live);
    CHECK(std::strcmp(rows[1].tag, "ABSENT") == 0);
}

// Release gate: a grayscale screenshot of PEERS must stay fully readable. The one
// status meaning on this screen is "is this operator here right now", and it is
// carried by a WORD (LIVE) against a count (x2) — never by colour — so a live and a
// remembered row must differ in lit pixels with the hue thrown away. Rendering both
// states and diffing the state column is what actually proves that.
static void test_peers_screen_grayscale_live_vs_remembered() {
    Game g{StartMode::Hatched};
    uint32_t t = 0;
    const uint8_t mac[6] = {0x02, 0, 0, 0, 0, 0x01};

    PeerHello hello;
    std::strncpy(hello.tag, "GHOSTBYTE", sizeof(hello.tag) - 1);
    std::strncpy(hello.petName, "Malbear", sizeof(hello.petName) - 1);
    std::strncpy(hello.crewName, "Deniers of Service", sizeof(hello.crewName) - 1);
    hello.stage = static_cast<uint8_t>(Stage::Script);
    hello.rank = 4;
    uint8_t frame[kPeerHelloSize];
    encodePeerHello(hello, frame, sizeof(frame));

    while (g.nav() != Game::Nav::Idle) g.onButton(press(Button::C));
    g.onButton({Button::A, true, true});                  // A+C -> hacker face
    g.onButton(press(Button::A));
    while (hackerCarouselSlots()[g.cursor()].id != HackerSlotId::Peers)
        g.onButton(press(Button::A));
    g.onButton(press(Button::B));                         // enter PEERS

    g.tick(t += 1000);
    g.registerPeer(mac, frame, kPeerHelloSize);
    Framebuffer live(kActiveW, kActiveH);
    g.render(live);
    // The row itself reads without colour at all: tag, pet and crew are lit glyphs.
    CHECK(anyLitGray(live, 0, 46, kActiveW, 110));

    // Same operator, no longer in range — the roster row survives, the LIVE tag
    // must not. Compare the right-hand state column between the two renders.
    g.tick(t += kPeerVisibleFreshMs + 1);
    CHECK(g.livePeerCount() == 0);
    Framebuffer remembered(kActiveW, kActiveH);
    g.render(remembered);
    CHECK(anyLitGray(remembered, 0, 46, kActiveW, 110));   // still listed

    int diff = 0;
    for (int y = 46; y < 70; ++y)
        for (int x = kActiveW - 60; x < kActiveW; ++x)
            if (luminance(live.get(x, y)) != luminance(remembered.get(x, y))) ++diff;
    CHECK(diff > 0);   // LIVE vs. the meeting count differ in LUMINANCE, not just hue
}

// Opening PEERS arms the LINK radio on its own — two people holding devices
// together shouldn't wait on a duty cycle — and closing it hands the radio back.
// LINK stays independent of the AUDIT ladder in both directions: announcing is a
// separate consent from listening, so neither toggle may imply the other.
static void test_peers_screen_raises_link_without_touching_config() {
    Game g{StartMode::Hatched};
    CHECK(!g.linkEnabled() && !g.linkWanted());           // CFG default: LINK off

    while (g.nav() != Game::Nav::Idle) g.onButton(press(Button::C));
    g.onButton({Button::A, true, true});                  // A+C -> hacker face
    g.onButton(press(Button::A));
    while (hackerCarouselSlots()[g.cursor()].id != HackerSlotId::Peers)
        g.onButton(press(Button::A));
    CHECK(!g.linkWanted());                               // still just the carousel
    g.onButton(press(Button::B));                         // enter PEERS

    CHECK(g.peersScreenOpen());
    CHECK(g.linkWanted());                                // the screen wants the radio
    CHECK(!g.linkEnabled());                              // ...but the opt-in is untouched
    CHECK(!g.radioScanWanted());                          // ...and the audit scan is NOT
    CHECK(g.auditMode() == Game::AuditMode::Off);         //     dragged along with it

    g.onButton(press(Button::C));                         // leave PEERS
    CHECK(!g.linkWanted());                               // config-dictated mode restored

    // The reverse, too: arming the audit scan must never start announcing.
    g.setNetScanEnabled(true);
    CHECK(g.radioScanWanted());
    CHECK(!g.linkWanted());
}

// Opening the CREW screen arms the passive scan on its own so the picker has live
// results, and closing it hands the radio straight back to the config-dictated mode —
// without ever touching the persisted CFG opt-in, and without arming capture.
static void test_crew_screen_raises_scan_without_touching_config() {
    Game g{StartMode::Hatched};
    CHECK(!g.netScanEnabled() && !g.radioScanWanted());   // CFG default: AUDIT off

    while (g.nav() != Game::Nav::Idle) g.onButton(press(Button::C));
    g.onButton({Button::A, true, true});                  // A+C -> hacker face
    g.onButton(press(Button::A));
    while (hackerCarouselSlots()[g.cursor()].id != HackerSlotId::Crew)
        g.onButton(press(Button::A));
    CHECK(!g.radioScanWanted());                          // still just the carousel
    g.onButton(press(Button::B));                         // enter CREW

    CHECK(g.radioScanWanted());                           // the screen wants the radio
    CHECK(!g.netScanEnabled());                           // ...but the opt-in is untouched
    CHECK(g.auditMode() == Game::AuditMode::Off);         // ...and capture stays off

    g.onButton(press(Button::C));                         // leave CREW
    CHECK(!g.radioScanWanted());                          // config-dictated mode restored

    // With AUDIT already on, the screen changes nothing — it only ever raises.
    g.setNetScanEnabled(true);
    CHECK(g.radioScanWanted());
    g.onButton(press(Button::B));
    CHECK(g.radioScanWanted() && g.netScanEnabled());
    g.onButton(press(Button::C));
    CHECK(g.radioScanWanted() && g.netScanEnabled());
}

// The CREW screen outlives the global 5s menu-idle collapse. It has to: holding the
// screen open is what arms the scan, so defocusing on the standard budget powered the
// radio down before a sweep could finish — the picker starved itself and stayed empty
// forever. It still collapses eventually, on the kRadioScreenDefocusMs budget.
static void test_crew_screen_outlives_the_menu_idle_timer() {
    Game g{StartMode::Hatched};
    uint32_t t = 0;
    while (g.nav() != Game::Nav::Idle) g.onButton(press(Button::C));
    g.onButton({Button::A, true, true});                  // A+C -> hacker face
    g.onButton(press(Button::A));
    while (hackerCarouselSlots()[g.cursor()].id != HackerSlotId::Crew)
        g.onButton(press(Button::A));
    g.onButton(press(Button::B));                         // enter CREW
    CHECK(g.crewScreenOpen() && g.radioScanWanted());

    // Well past the point any other submenu would have collapsed, with no input.
    for (int i = 0; i < 8; ++i) g.tick(t += kAutoDefocusMs);
    CHECK(g.crewScreenOpen());                            // still open
    CHECK(g.radioScanWanted());                           // ...so the radio stays armed

    // But not forever — the longer budget still expires.
    g.tick(t += kRadioScreenDefocusMs);
    CHECK(!g.crewScreenOpen());
    CHECK(!g.radioScanWanted());                          // radio handed back on collapse
}

// PEERS needs the same reprieve as CREW, and for a sharper reason: two devices only
// discover each other while BOTH have their radios up, so a screen that collapses on
// the standard 5s budget can leave an operator reading a list on a device that has
// quietly stopped looking. The device tier holds the panel awake off the same
// predicate (radioScreenOpen), so the screen doesn't go dark underneath them either.
static void test_peers_screen_outlives_the_menu_idle_timer() {
    Game g{StartMode::Hatched};
    uint32_t t = 0;
    while (g.nav() != Game::Nav::Idle) g.onButton(press(Button::C));
    g.onButton({Button::A, true, true});                  // A+C -> hacker face
    g.onButton(press(Button::A));
    while (hackerCarouselSlots()[g.cursor()].id != HackerSlotId::Peers)
        g.onButton(press(Button::A));
    g.onButton(press(Button::B));                         // enter PEERS
    CHECK(g.peersScreenOpen() && g.linkWanted());
    CHECK(g.radioScreenOpen());                           // ...and holds the panel awake

    // Well past the point any other submenu would have collapsed, with no input —
    // this is the case that bit on hardware: reading a peer's row takes longer than
    // five seconds.
    for (int i = 0; i < 8; ++i) g.tick(t += kAutoDefocusMs);
    CHECK(g.peersScreenOpen());                           // still open
    CHECK(g.linkWanted());                                // ...so the radio stays armed

    // But not forever — the longer budget still expires and hands the radio back.
    g.tick(t += kRadioScreenDefocusMs);
    CHECK(!g.peersScreenOpen());
    CHECK(!g.linkWanted());
    CHECK(!g.radioScreenOpen());                          // panel sleep resumes too
}

// --- 1v1 duels over LINK (core/net/pvp_link.h, core/model/pvp_battle.h) -------

// Every frame type survives the wire, and a malformed one is rejected outright rather
// than half-parsed. The fighter spec is the interesting payload: it is what both
// devices rebuild their combatants from, so a field that doesn't survive encoding
// would desync a duel rather than merely look wrong.
static void test_pvp_frame_round_trip() {
    PvpFighter f;
    std::strncpy(f.tag, "NETRUNNER_99", sizeof(f.tag) - 1);
    std::strncpy(f.creatureId, "paypup", sizeof(f.creatureId) - 1);
    std::strncpy(f.moveIds[0], "packet_storm", kPvpIdCap - 1);
    std::strncpy(f.moveIds[1], "checksum_guard", kPvpIdCap - 1);
    std::strncpy(f.modIds[0], "firewall_patch", kPvpIdCap - 1);
    f.level = 300;                                    // past a byte, to catch a narrow field
    f.statPoints[0] = 4; f.statPoints[3] = 9;

    uint8_t buf[kPvpFrameCap];
    PvpFrame out;

    CHECK(encodePvpInvite(0xDEADBEEF, f, buf, sizeof(buf)) == kPvpInviteSize);
    CHECK(decodePvpFrame(buf, kPvpInviteSize, &out));
    CHECK(out.type == PvpFrameType::Invite);
    CHECK(out.session == 0xDEADBEEF);
    CHECK(out.rules == kPvpRulesVersion);
    CHECK(std::strcmp(out.fighter.tag, "NETRUNNER_99") == 0);
    CHECK(std::strcmp(out.fighter.creatureId, "paypup") == 0);
    CHECK(std::strcmp(out.fighter.moveIds[1], "checksum_guard") == 0);
    CHECK(std::strcmp(out.fighter.modIds[0], "firewall_patch") == 0);
    CHECK(out.fighter.moveIds[2][0] == '\0');         // an empty slot stays empty
    CHECK(out.fighter.level == 300);
    CHECK(out.fighter.statPoints[0] == 4 && out.fighter.statPoints[3] == 9);

    CHECK(encodePvpAccept(7, f, buf, sizeof(buf)) == kPvpAcceptSize);
    CHECK(decodePvpFrame(buf, kPvpAcceptSize, &out) && out.type == PvpFrameType::Accept);

    CHECK(encodePvpDecline(7, PvpDeclineReason::Busy, buf, sizeof(buf)) == kPvpDeclineSize);
    CHECK(decodePvpFrame(buf, kPvpDeclineSize, &out));
    CHECK(out.type == PvpFrameType::Decline && out.reason == PvpDeclineReason::Busy);

    CHECK(encodePvpStart(7, 12345, 999, buf, sizeof(buf)) == kPvpStartSize);
    CHECK(decodePvpFrame(buf, kPvpStartSize, &out));
    CHECK(out.type == PvpFrameType::Start && out.seed == 12345 && out.commit == 999);

    CHECK(encodePvpBye(7, buf, sizeof(buf)) == kPvpByeSize);
    CHECK(decodePvpFrame(buf, kPvpByeSize, &out) && out.type == PvpFrameType::Bye);

    // Malformed: wrong length for the declared type, a corrupted magic, and a
    // protocol version this build can't lay out.
    CHECK(encodePvpStart(7, 1, 2, buf, sizeof(buf)) == kPvpStartSize);
    CHECK(!decodePvpFrame(buf, kPvpStartSize - 1, &out));
    CHECK(!decodePvpFrame(buf, kPvpStartSize + 1, &out));
    CHECK(!decodePvpFrame(nullptr, kPvpStartSize, &out));
    uint8_t bad[kPvpFrameCap];
    std::memcpy(bad, buf, kPvpStartSize);
    bad[0] = 'X';
    CHECK(!decodePvpFrame(bad, kPvpStartSize, &out));
    std::memcpy(bad, buf, kPvpStartSize);
    bad[4] = kPvpProtoVersion + 1;
    CHECK(!decodePvpFrame(bad, kPvpStartSize, &out));
    std::memcpy(bad, buf, kPvpStartSize);
    bad[5] = 99;                                      // unknown frame type
    CHECK(!decodePvpFrame(bad, kPvpStartSize, &out));

    // A HELLO beacon must never be mistaken for a duel frame, or the reverse — they
    // share one receive path on the device (platform/esp32/net_link.h).
    PeerHello hello;
    std::strncpy(hello.tag, "SOMEONE", sizeof(hello.tag) - 1);
    uint8_t beacon[kPeerHelloSize];
    CHECK(encodePeerHello(hello, beacon, sizeof(beacon)) == kPeerHelloSize);
    CHECK(!decodePvpFrame(beacon, kPeerHelloSize, &out));
    PeerHello back;
    CHECK(encodePvpInvite(1, f, buf, sizeof(buf)) == kPvpInviteSize);
    CHECK(!decodePeerHello(buf, kPvpInviteSize, &back));
}

// Both devices must independently reach the SAME answer about who hosts, from
// information they both already have. Lower MAC wins, and it is a total order — no
// tie is possible because two devices cannot share a MAC.
static void test_pvp_host_election_agrees_on_both_sides() {
    const uint8_t lowMac[6] = {0x02, 0, 0, 0, 0, 0x01};
    const uint8_t highMac[6] = {0x02, 0, 0, 0, 0, 0x02};
    const uint64_t low = packPeerKey(lowMac);
    const uint64_t high = packPeerKey(highMac);
    CHECK(low < high);
    CHECK(pvpHostIsLocal(low, high));           // the low device says "I host"
    CHECK(!pvpHostIsLocal(high, low));          // ...and the high device agrees

    // A packed key round-trips back to the same six bytes, which is what lets a screen
    // turn a selected roster row into an address to send to.
    uint8_t back[6] = {0};
    unpackPeerKey(high, back);
    CHECK(std::memcmp(back, highMac, 6) == 0);
}

// START's commit hash is the divergence guard: it must change if ANY of the three
// things it covers changes, so a guest that would have played a different fight
// refuses instead of playing it.
static void test_pvp_commit_hash_catches_divergence() {
    PvpFighter a, b;
    std::strncpy(a.creatureId, "paypup", sizeof(a.creatureId) - 1);
    std::strncpy(b.creatureId, "pingcub", sizeof(b.creatureId) - 1);

    const uint32_t base = pvpCommitHash(42, a, b);
    CHECK(pvpCommitHash(42, a, b) == base);          // stable
    CHECK(pvpCommitHash(43, a, b) != base);          // a different seed
    CHECK(pvpCommitHash(42, b, a) != base);          // ...or the fighters seated swapped
    PvpFighter a2 = a;
    a2.statPoints[0] = 1;
    CHECK(pvpCommitHash(42, a2, b) != base);         // ...or one stat point of difference
}

// The heart of the feature: two devices, no turn-by-turn traffic, one fight. Feeding
// the same two specs and the same seed to two independent Combats must produce the
// same fight hit for hit — that is the entire reason a duel can play out on both
// screens at once without broadcasting anything.
static void test_pvp_same_seed_plays_the_same_fight() {
    ContentRegistry reg;
    reg.addSource(embeddedContent());
    reg.setAssets(embeddedAssets());

    PvpFighter host, guest;
    std::strncpy(host.creatureId, "paypup", sizeof(host.creatureId) - 1);
    std::strncpy(host.moveIds[0], "packet_storm", kPvpIdCap - 1);
    host.statPoints[0] = 3;
    std::strncpy(guest.creatureId, "pingcub", sizeof(guest.creatureId) - 1);
    std::strncpy(guest.moveIds[0], "checksum_guard", kPvpIdCap - 1);
    guest.statPoints[3] = 5;
    CHECK(canBuildPvpCombatant(reg, host) && canBuildPvpCombatant(reg, guest));

    Combat left, right;
    beginPvpBattle(left, reg, host, guest, 0xC0FFEE);
    beginPvpBattle(right, reg, host, guest, 0xC0FFEE);

    // Different fighters, so a seating mistake would show up as a mismatch below
    // rather than hiding behind a mirror match.
    CHECK(std::strcmp(left.player().name, right.player().name) == 0);
    CHECK(std::strcmp(left.player().name, left.enemy().name) != 0);

    int steps = 0;
    while (left.outcome() == Combat::Outcome::Ongoing && steps < 500) {
        CHECK(left.step() == right.step());
        CHECK(left.player().health == right.player().health);
        CHECK(left.enemy().health == right.enemy().health);
        CHECK(left.playerTurnNext() == right.playerTurnNext());
        ++steps;
    }
    CHECK(steps > 0 && steps < 500);                 // it actually resolved
    CHECK(left.outcome() == right.outcome());
    CHECK(left.outcome() != Combat::Outcome::Ongoing);

    // A different seed is a different fight — otherwise the agreement above would be
    // proving nothing about the seed at all.
    Combat other;
    beginPvpBattle(other, reg, host, guest, 0xBADBAD);
    int otherSteps = 0;
    while (other.outcome() == Combat::Outcome::Ongoing && otherSteps < 500) {
        other.step();
        ++otherSteps;
    }
    CHECK(otherSteps != steps || other.outcome() != left.outcome());
}

// Seating is fixed to HOST-as-player_ precisely because Combat is not symmetric:
// swapping the two arguments produces a different fight, so both devices agreeing on
// the order is load-bearing, not cosmetic.
static void test_pvp_seating_order_is_load_bearing() {
    ContentRegistry reg;
    reg.addSource(embeddedContent());
    reg.setAssets(embeddedAssets());

    PvpFighter a, b;
    std::strncpy(a.creatureId, "paypup", sizeof(a.creatureId) - 1);
    std::strncpy(b.creatureId, "pingcub", sizeof(b.creatureId) - 1);

    Combat forward, swapped;
    beginPvpBattle(forward, reg, a, b, 99);
    beginPvpBattle(swapped, reg, b, a, 99);
    CHECK(std::strcmp(forward.player().name, swapped.enemy().name) == 0);
    CHECK(std::strcmp(forward.enemy().name, swapped.player().name) == 0);

    // No Exploit is offered on either side: the A+C picker pauses the fight for a
    // human, and there is no way to pause the other device's copy of it.
    CHECK(forward.overrideUsesTotal() == 0 && !forward.overrideReady());
    // And no stakes — a loss must not reach Fragmentation.
    CHECK(forward.stakes() == Combat::Stakes::Safe);
}

// Walk the hacker carousel to `slot` and enter it.
static void enterHackerSlot(Game& g, HackerSlotId slot) {
    while (g.nav() != Game::Nav::Idle) g.onButton(press(Button::C));
    g.onButton({Button::A, true, true});                  // A+C -> hacker face
    g.onButton(press(Button::A));
    while (hackerCarouselSlots()[g.cursor()].id != slot) g.onButton(press(Button::A));
    g.onButton(press(Button::B));
}

// Move every frame `from` has queued FOR `to` into `to`, as a unicast radio would.
// Honouring the destination MAC matters: a device mid-duel is queueing frames for its
// opponent, and delivering those to an unrelated third device would be a fiction the
// radio never performs. Frames addressed elsewhere are dropped, as they would be here.
static int relayPvp(Game& from, const uint8_t* fromMac, Game& to, const uint8_t* toMac) {
    uint8_t mac[6];
    uint8_t buf[kPvpFrameCap];
    size_t len = 0;
    int n = 0;
    while (from.takePvpOut(mac, buf, sizeof(buf), &len)) {
        if (std::memcmp(mac, toMac, 6) != 0) continue;
        to.onPvpFrame(fromMac, buf, len);
        ++n;
    }
    return n;
}

// Let `a` hear `b`'s beacon so `b` shows up as a live, challengeable peer.
static void hearBeacon(Game& listener, const uint8_t* senderMac, const char* tag) {
    PeerHello hello;
    std::strncpy(hello.tag, tag, sizeof(hello.tag) - 1);
    std::strncpy(hello.petName, "Paypup", sizeof(hello.petName) - 1);
    hello.stage = static_cast<uint8_t>(Stage::Process);
    uint8_t frame[kPeerHelloSize];
    encodePeerHello(hello, frame, sizeof(frame));
    listener.registerPeer(senderMac, frame, kPeerHelloSize);
}

// The whole handshake, end to end, between two engines: challenge, human consent,
// seed agreement, and then both devices playing the same fight to the same verdict —
// stated from each operator's own point of view.
static void test_pvp_two_devices_duel_end_to_end() {
    const uint8_t macA[6] = {0x02, 0, 0, 0, 0, 0x01};   // lower -> hosts
    const uint8_t macB[6] = {0x02, 0, 0, 0, 0, 0x02};

    Game a{StartMode::Hatched, "paypup"};
    Game b{StartMode::Hatched, "pingcub"};
    a.setLocalRadioMac(macA);
    b.setLocalRadioMac(macB);
    uint32_t t = 0;
    a.tick(t += 1000);
    b.tick(t);

    // Both operators are on the LINK screen — that IS the consent to be challenged.
    enterHackerSlot(a, HackerSlotId::Link);
    enterHackerSlot(b, HackerSlotId::Link);
    CHECK(a.linkScreenOpen() && b.linkScreenOpen());
    CHECK(a.linkWanted() && !a.linkEnabled());          // the screen arms the radio,
    CHECK(a.radioScreenOpen());                          // holds the panel awake,
    CHECK(!a.netScanEnabled());                          // and never arms the audit scan

    hearBeacon(a, macB, "OPERATOR_B");
    hearBeacon(b, macA, "OPERATOR_A");

    // A challenges B.
    uint64_t keys[8];
    CHECK(a.pvpChallengeableKeys(keys, 8) == 1);
    CHECK(a.pvpChallenge(keys[0]));
    CHECK(a.pvpPhase() == Game::PvpPhase::Inviting);
    CHECK(a.pvpLocalIsHost() && !b.pvpLocalIsHost());   // ...before B has even heard

    CHECK(relayPvp(a, macA, b, macB) == 1);                   // the INVITE lands
    CHECK(b.pvpPhase() == Game::PvpPhase::Invited);
    CHECK(std::strcmp(b.pvpOpponentTag(), "NETRUNNER_99") == 0);

    // B's human accepts. B is the guest, so it sends its fighter and waits.
    b.pvpAcceptChallenge();
    CHECK(b.pvpPhase() == Game::PvpPhase::Arming);
    CHECK(relayPvp(b, macB, a, macA) >= 1);                   // the ACCEPT lands
    CHECK(a.pvpFighting());                             // the host starts at once
    CHECK(a.nav() == Game::Nav::Combat);

    CHECK(relayPvp(a, macA, b, macB) >= 1);                   // the START lands
    CHECK(b.pvpFighting());
    CHECK(b.nav() == Game::Nav::Combat);

    // Same two fighters, seated the same way round on both screens.
    CHECK(std::strcmp(a.combat().player().name, b.combat().player().name) == 0);
    CHECK(std::strcmp(a.combat().enemy().name, b.combat().enemy().name) == 0);
    CHECK(std::strcmp(a.combat().player().name, a.combat().enemy().name) != 0);

    // Neither screen can pause or bail: A+C is inert and C does not flee.
    a.onButton({Button::A, true, true});
    CHECK(!a.combat().overrideOpen());
    a.onButton(press(Button::C));
    CHECK(a.combat().outcome() == Combat::Outcome::Ongoing);

    // Play both out. Each device is stepping its OWN copy; they must stay identical.
    int steps = 0;
    while (a.combat().outcome() == Combat::Outcome::Ongoing && steps < 500) {
        a.onButton(press(Button::A));
        b.onButton(press(Button::A));
        CHECK(a.combat().player().health == b.combat().player().health);
        CHECK(a.combat().enemy().health == b.combat().enemy().health);
        ++steps;
    }
    CHECK(steps < 500);
    CHECK(a.combat().outcome() == b.combat().outcome());

    const int bitsBefore = a.bits();
    const int fragBefore = a.model().fragmentation();
    const int levelBefore = a.combatLevel();

    a.onButton(press(Button::B));                       // dismiss the result
    b.onButton(press(Button::B));
    CHECK(a.pvpPhase() == Game::PvpPhase::Result);
    CHECK(b.pvpPhase() == Game::PvpPhase::Result);
    // Exactly one of them won, and each was told so from its own side.
    CHECK(a.pvpLocalWon() != b.pvpLocalWon());

    // No stakes: nothing was earned, lost, or corrupted by fighting.
    CHECK(a.bits() == bitsBefore);
    CHECK(a.model().fragmentation() == fragBefore);
    CHECK(a.combatLevel() == levelBefore);

    // A verdict left on screen must not pin the radio awake — only a session that
    // still needs the air does that.
    CHECK(a.pvpActive() && !a.pvpSessionLive());

    // Any key clears the verdict back to the challengeable list.
    a.onButton(press(Button::A));
    CHECK(a.pvpPhase() == Game::PvpPhase::Idle);
    CHECK(a.linkScreenOpen());
}

// A duel is the only fight whose opponent is a real species rather than a name-only
// malbeast/boss/dummy spec, so it is the only thing that can move a creature from
// "locked" to "seen". Both devices record the other's species, and neither records
// its own — you raised that one.
static void test_pvp_duel_marks_the_opponent_species_seen() {
    const uint8_t macA[6] = {0x02, 0, 0, 0, 0, 0x01};
    const uint8_t macB[6] = {0x02, 0, 0, 0, 0, 0x02};

    Game a{StartMode::Hatched, "paypup"};
    Game b{StartMode::Hatched, "pingcub"};
    a.setLocalRadioMac(macA);
    b.setLocalRadioMac(macB);
    uint32_t t = 0;
    a.tick(t += 1000);
    b.tick(t);
    enterHackerSlot(a, HackerSlotId::Link);
    enterHackerSlot(b, HackerSlotId::Link);
    hearBeacon(a, macB, "OPERATOR_B");
    hearBeacon(b, macA, "OPERATOR_A");

    CHECK(!a.creatureSeen("pingcub") && !b.creatureSeen("paypup"));

    uint64_t keys[8];
    CHECK(a.pvpChallengeableKeys(keys, 8) == 1);
    CHECK(a.pvpChallenge(keys[0]));
    CHECK(relayPvp(a, macA, b, macB) == 1);        // INVITE
    b.pvpAcceptChallenge();
    CHECK(relayPvp(b, macB, a, macA) >= 1);        // ACCEPT -> the host starts
    CHECK(a.pvpFighting());
    CHECK(relayPvp(a, macA, b, macB) >= 1);        // START -> the guest starts
    CHECK(b.pvpFighting());

    // Recorded at the START, before either fight resolves — facing the species is
    // the glimpse, not beating it.
    CHECK(a.creatureSeen("pingcub"));
    CHECK(b.creatureSeen("paypup"));
    CHECK(!a.creatureSeen("paypup"));              // its own pet: raised, never "seen"
    CHECK(!b.creatureSeen("pingcub"));
    CHECK(a.creatureRaised("paypup") && !a.creatureRaised("pingcub"));

    // ...and that is exactly the tier the 'Pedia reports for it.
    const std::string json = buildPediaStateJson(a);
    CHECK(json.find("\"pingcub\":\"seen\"") != std::string::npos);
    CHECK(json.find("\"paypup\":\"hatched\"") != std::string::npos);
}

// A human takes SECONDS to press accept, and the challenger repeats its INVITE every
// kPvpRetryMs the whole time. So the target sees the same invite many times over while
// its own prompt is on screen, and must ignore those repeats: answering one with BUSY
// declines the very challenge it is displaying, and the challenger gives up before its
// opponent can possibly say yes.
//
// This is the case the end-to-end test missed by relaying each frame exactly once —
// on hardware it failed 100% of the time.
static void test_pvp_invite_retries_do_not_decline_the_pending_challenge() {
    const uint8_t macA[6] = {0x02, 0, 0, 0, 0, 0x01};
    const uint8_t macB[6] = {0x02, 0, 0, 0, 0, 0x02};

    Game a{StartMode::Hatched, "paypup"};
    Game b{StartMode::Hatched, "pingcub"};
    a.setLocalRadioMac(macA);
    b.setLocalRadioMac(macB);
    uint32_t t = 0;
    a.tick(t += 1000);
    b.tick(t);
    enterHackerSlot(a, HackerSlotId::Link);
    enterHackerSlot(b, HackerSlotId::Link);
    hearBeacon(a, macB, "OPERATOR_B");
    hearBeacon(b, macA, "OPERATOR_A");

    uint64_t keys[8];
    CHECK(a.pvpChallengeableKeys(keys, 8) == 1);
    CHECK(a.pvpChallenge(keys[0]));
    CHECK(relayPvp(a, macA, b, macB) == 1);
    CHECK(b.pvpPhase() == Game::PvpPhase::Invited);

    // B's human dithers for several seconds while A keeps retrying. Every retry must
    // land on a still-Invited B and produce NOTHING back.
    for (int i = 0; i < kPvpMaxRetries + 2; ++i) {
        a.tick(t += kPvpRetryMs);
        b.tick(t);
        relayPvp(a, macA, b, macB);
        CHECK(b.pvpPhase() == Game::PvpPhase::Invited);   // prompt still up
        CHECK(relayPvp(b, macB, a, macA) == 0);                 // ...and B said nothing back
        CHECK(a.pvpPhase() == Game::PvpPhase::Inviting);  // so A is still waiting
    }

    // Now the human finally accepts, and the duel proceeds normally.
    b.pvpAcceptChallenge();
    relayPvp(b, macB, a, macA);
    relayPvp(a, macA, b, macB);
    CHECK(a.pvpFighting() && b.pvpFighting());
    CHECK(std::strcmp(a.combat().player().name, b.combat().player().name) == 0);

    // A genuinely DIFFERENT session from a device already in a duel still gets BUSY —
    // ignoring retries must not have made the busy answer unreachable.
    const uint8_t macC[6] = {0x02, 0, 0, 0, 0, 0x03};
    Game c{StartMode::Hatched, "paypup"};
    c.setLocalRadioMac(macC);
    c.tick(t += 1000);
    enterHackerSlot(c, HackerSlotId::Link);
    hearBeacon(c, macB, "OPERATOR_B");
    uint64_t ckeys[8];
    CHECK(c.pvpChallengeableKeys(ckeys, 8) == 1);
    CHECK(c.pvpChallenge(ckeys[0]));
    relayPvp(c, macC, b, macB);
    CHECK(relayPvp(b, macB, c, macC) >= 1);                     // B answered the newcomer
    CHECK(c.pvpPhase() == Game::PvpPhase::Idle);
    CHECK(std::strcmp(c.pvpStatusText(), "OPERATOR BUSY") == 0);
    CHECK(a.pvpFighting() && b.pvpFighting());            // ...without disturbing the duel
}

// A challenge is only ever considered by a human looking at the LINK screen. Anywhere
// else the device answers BUSY rather than interrupting someone who isn't looking for
// a fight — and the challenger is told which, so a refusal never reads as a hang.
static void test_pvp_challenge_needs_a_human_on_the_link_screen() {
    const uint8_t macA[6] = {0x02, 0, 0, 0, 0, 0x01};
    const uint8_t macB[6] = {0x02, 0, 0, 0, 0, 0x02};

    Game a{StartMode::Hatched, "paypup"};
    Game b{StartMode::Hatched, "paypup"};
    a.setLocalRadioMac(macA);
    b.setLocalRadioMac(macB);
    uint32_t t = 0;
    a.tick(t += 1000);
    b.tick(t);

    enterHackerSlot(a, HackerSlotId::Link);
    hearBeacon(a, macB, "OPERATOR_B");
    uint64_t keys[8];
    CHECK(a.pvpChallengeableKeys(keys, 8) == 1);
    CHECK(a.pvpChallenge(keys[0]));

    // B is sitting on its idle habitat, not the LINK screen.
    CHECK(!b.linkScreenOpen());
    relayPvp(a, macA, b, macB);
    CHECK(b.pvpPhase() == Game::PvpPhase::Idle);        // never even prompted
    relayPvp(b, macB, a, macA);                                // ...but it did answer
    CHECK(a.pvpPhase() == Game::PvpPhase::Idle);
    CHECK(std::strcmp(a.pvpStatusText(), "OPERATOR BUSY") == 0);

    // And a refusal by the human reads differently from being busy.
    enterHackerSlot(b, HackerSlotId::Link);
    CHECK(a.pvpChallenge(keys[0]));
    relayPvp(a, macA, b, macB);
    CHECK(b.pvpPhase() == Game::PvpPhase::Invited);
    b.pvpDeclineChallenge();
    CHECK(b.pvpPhase() == Game::PvpPhase::Idle);
    relayPvp(b, macB, a, macA);
    CHECK(a.pvpPhase() == Game::PvpPhase::Idle);
    CHECK(std::strcmp(a.pvpStatusText(), "DECLINED") == 0);
}

// An unanswered challenge must not strand the screen: it times out, says so, and hands
// the radio back.
static void test_pvp_unanswered_challenge_times_out() {
    const uint8_t macA[6] = {0x02, 0, 0, 0, 0, 0x01};
    const uint8_t macB[6] = {0x02, 0, 0, 0, 0, 0x02};
    Game a{StartMode::Hatched, "paypup"};
    a.setLocalRadioMac(macA);
    uint32_t t = 0;
    a.tick(t += 1000);
    enterHackerSlot(a, HackerSlotId::Link);
    hearBeacon(a, macB, "OPERATOR_B");

    uint64_t keys[8];
    CHECK(a.pvpChallengeableKeys(keys, 8) == 1);
    CHECK(a.pvpChallenge(keys[0]));
    CHECK(a.pvpActive() && a.linkWanted());

    a.tick(t += kPvpInviteTimeoutMs + 1);
    CHECK(a.pvpPhase() == Game::PvpPhase::Idle);
    CHECK(std::strcmp(a.pvpStatusText(), "NO ANSWER") == 0);
}

// The bottom row owns the zoned Health gauge with its Critical pulse — the "my pet is
// in trouble" read — so each duellist must see their OWN pet there. The guest holds
// Combat's enemy_ slot, so its screen swaps the rows for display. Rendering one shared
// fight from both sides and diffing proves the swap is actually wired: without it the
// two screens would be pixel-identical and the guest would be watching its rival's
// health pulse at it.
static void test_pvp_guest_sees_its_own_pet_on_the_bottom_gauge() {
    ContentRegistry reg;
    reg.addSource(embeddedContent());
    reg.setAssets(embeddedAssets());

    PvpFighter host, guest;
    std::strncpy(host.creatureId, "paypup", sizeof(host.creatureId) - 1);
    std::strncpy(guest.creatureId, "pingcub", sizeof(guest.creatureId) - 1);
    guest.statPoints[3] = 9;                 // a different max Health, so the gauges differ

    Combat c;
    beginPvpBattle(c, reg, host, guest, 0xC0FFEE);
    for (int i = 0; i < 3 && c.outcome() == Combat::Outcome::Ongoing; ++i) c.step();

    const SpriteData* ps = reg.sprite(c.player().spriteName);
    const SpriteData* es = reg.sprite(c.enemy().spriteName);

    CombatSides hostSides;                   // host: the ordinary reading
    hostSides.rivalLabel = "RIVAL";
    hostSides.localLabel = "YOU";
    CombatSides guestSides = hostSides;
    guestSides.localIsEnemySide = true;      // guest: roles swapped for display

    Framebuffer hostView(kActiveW, kActiveH), guestView(kActiveW, kActiveH);
    drawCombat(hostView, c, ps, es, 0, 0, 0, false, hostSides);
    drawCombat(guestView, c, ps, es, 0, 0, 0, false, guestSides);

    int diff = 0;
    for (int y = 0; y < kActiveH; ++y)
        for (int x = 0; x < kActiveW; ++x)
            if (hostView.get(x, y) != guestView.get(x, y)) ++diff;
    CHECK(diff > 0);                         // the two screens are NOT the same picture

    // ...and the difference survives grayscale, since it is geometry and digits rather
    // than hue: the release gate is that a colourless screenshot still tells you whose
    // pet is whose.
    int grayDiff = 0;
    for (int y = 0; y < kActiveH; ++y)
        for (int x = 0; x < kActiveW; ++x)
            if (luminance(hostView.get(x, y)) != luminance(guestView.get(x, y))) ++grayDiff;
    CHECK(grayDiff > 0);
}

// The local pet fights from the LEFT stage seat, its rival from the RIGHT — in every
// fight, PVE and duel alike. The seat follows the local/rival ROLE, not Combat's
// player_/enemy_ slot, so a duel guest (which holds enemy_) still finds its own pet on
// the left. Rendering with ONE sprite supplied at a time pins each seat to a role:
// whichever combatant is the local one lights the left box and leaves the right dark.
static void test_combat_seats_local_pet_on_the_left() {
    // The two stage boxes and the band they're bottom-anchored in (combat_screen.cpp).
    // Nothing but the sprites draws here while a fight is Ongoing.
    constexpr int kLeftX0 = 4, kLeftX1 = 108, kRightX0 = 116, kRightX1 = 220;
    constexpr int kBandY0 = 81, kBandY1 = 165;

    ContentRegistry reg;
    reg.addSource(embeddedContent());
    reg.setAssets(embeddedAssets());

    PvpFighter host, guest;
    std::strncpy(host.creatureId, "paypup", sizeof(host.creatureId) - 1);
    std::strncpy(guest.creatureId, "pingcub", sizeof(guest.creatureId) - 1);

    Combat c;
    beginPvpBattle(c, reg, host, guest, 0xC0FFEE);   // left Ongoing: no result banner
    const SpriteData* ps = reg.sprite(c.player().spriteName);
    const SpriteData* es = reg.sprite(c.enemy().spriteName);

    auto seatOf = [&](const SpriteData* p, const SpriteData* e, bool localIsEnemySide) {
        CombatSides sides;
        sides.localIsEnemySide = localIsEnemySide;
        Framebuffer fb(kActiveW, kActiveH);
        drawCombat(fb, c, p, e, 0, 0, 0, false, sides);
        const bool left = anyLitGray(fb, kLeftX0, kBandY0, kLeftX1, kBandY1);
        const bool right = anyLitGray(fb, kRightX0, kBandY0, kRightX1, kBandY1);
        CHECK(left != right);                        // exactly one seat is occupied
        return left;
    };

    CHECK(seatOf(ps, nullptr, false));               // PVE: the pet is Combat's player_
    CHECK(!seatOf(nullptr, es, false));              // PVE: the wild malbeast sits right
    CHECK(seatOf(nullptr, es, true));                // duel guest: enemy_ IS its own pet
    CHECK(!seatOf(ps, nullptr, true));               // ...so the host's pet is its rival
}

// Release gate: the ransom pool (combat.h ransomPool) must stay readable in grayscale.
// Both of its meanings ride on a non-colour channel — HOW MUCH is the green gauge's fill
// width, HOW LONG is the count of lit blips beside it — so throwing the hue away has to
// leave a bigger pool wider and a longer countdown brighter.
static void test_combat_ransom_pool_grayscale() {
    ContentRegistry r = ContentRegistry::embedded();

    // The pool row sits directly above the local Health gauge (combat_screen.cpp): the
    // gauge spans phX..phX+phW at phY-7, the kRansomHoldTurns blips the strip to its right.
    const int phX = 8 + textWidth("YOU") + 6, phW = 110;
    const int rowY = 175 - 7, rowH = 5;
    const int blipX = phX + phW + 6, blipW = kRansomHoldTurns * 7;
    // Summed LUMINANCE, not a lit-pixel count: the blips code their state as calm-green
    // (0.67) against ink-dim (0.47), both of which read as "lit" — brightness is the
    // channel that separates them once the hue is gone.
    auto grayIn = [&](const Framebuffer& fb, int x0, int w) {
        float sum = 0;
        for (int y = rowY; y < rowY + rowH; ++y)
            for (int x = x0; x < x0 + w; ++x) sum += luminance(fb.get(x, y));
        return sum;
    };
    auto renderPool = [&](int pool, int turnsLeft, Framebuffer& fb) {
        Combatant p = mkCombatant(r, "P", 100, 10, {"quick_jab"});
        p.ransomPool = pool;                             // begin() carries both through
        p.ransomTurnsLeft = turnsLeft;
        Combatant e = mkCombatant(r, "E", 100, 10, {"quick_jab"});
        Combat c; c.begin(p, e, Combat::Stakes::Safe, 1);   // Ongoing: no result banner
        drawCombat(fb, c, nullptr, nullptr, 0, 0, 0, false, CombatSides{});
    };

    Framebuffer none(kActiveW, kActiveH), small(kActiveW, kActiveH), big(kActiveW, kActiveH);
    renderPool(0, 0, none);
    renderPool(4, 1, small);
    renderPool(400, kRansomHoldTurns, big);

    // Against bare paper (nothing drawn while there's no pool), each step up in pool size
    // and in turns owed has to be brighter than the last.
    CHECK(grayIn(small, phX, phW) > grayIn(none, phX, phW));
    CHECK(grayIn(big, phX, phW) > grayIn(small, phX, phW));         // fill width = how much
    CHECK(grayIn(small, blipX, blipW) > grayIn(none, blipX, blipW));
    CHECK(grayIn(big, blipX, blipW) > grayIn(small, blipX, blipW)); // lit blips = how long
}

// Release gate: a grayscale screenshot of LINK must stay fully readable. The verdict
// is the one status meaning on this screen, and it is carried by a WORD — so a win and
// a loss must differ in lit pixels with the hue thrown away.
static void test_pvp_link_screen_verdict_grayscale() {
    const uint8_t macA[6] = {0x02, 0, 0, 0, 0, 0x01};
    const uint8_t macB[6] = {0x02, 0, 0, 0, 0, 0x02};

    auto renderVerdict = [&](bool asWinner, Framebuffer& fb) {
        Game a{StartMode::Hatched, "paypup"};
        Game b{StartMode::Hatched, "pingcub"};
        a.setLocalRadioMac(macA);
        b.setLocalRadioMac(macB);
        uint32_t t = 0;
        a.tick(t += 1000);
        b.tick(t);
        enterHackerSlot(a, HackerSlotId::Link);
        enterHackerSlot(b, HackerSlotId::Link);
        hearBeacon(a, macB, "OPERATOR_B");
        hearBeacon(b, macA, "OPERATOR_A");
        uint64_t keys[8];
        a.pvpChallengeableKeys(keys, 8);
        a.pvpChallenge(keys[0]);
        relayPvp(a, macA, b, macB);
        b.pvpAcceptChallenge();
        relayPvp(b, macB, a, macA);
        relayPvp(a, macA, b, macB);
        int steps = 0;
        while (a.combat().outcome() == Combat::Outcome::Ongoing && steps++ < 500) {
            a.onButton(press(Button::A));
            b.onButton(press(Button::A));
        }
        a.onButton(press(Button::B));
        b.onButton(press(Button::B));
        // Whichever engine holds the outcome we were asked for renders the panel.
        Game& shown = a.pvpLocalWon() == asWinner ? a : b;
        CHECK(shown.pvpLocalWon() == asWinner);
        shown.render(fb);
    };

    Framebuffer won(kActiveW, kActiveH), lost(kActiveW, kActiveH);
    renderVerdict(true, won);
    renderVerdict(false, lost);

    // Desaturated, the two panels must still be distinguishable — if the only
    // difference were hue, these would match.
    int diff = 0;
    for (int y = 0; y < kActiveH; ++y)
        for (int x = 0; x < kActiveW; ++x)
            if (luminance(won.get(x, y)) != luminance(lost.get(x, y))) ++diff;
    CHECK(diff > 0);
}

// In a live fight, the A+C picker only offers the crew row while the player belongs
// to a crew, and committing it arms that crew's charges.
static void test_crew_exploit_in_combat_picker() {
    Game g{StartMode::Hatched, "paypup"};
    g.debugStartCombat(/*live=*/true);
    CHECK(g.nav() == Game::Nav::Combat);

    g.onButton({Button::A, true, true});           // A+C -> picker
    CHECK(g.combat().overrideOpen() && g.combat().overrideCrewRows() == 0);
    g.onButton(press(Button::C));                  // cancel -> no spend
    CHECK(g.combat().overrideReady());

    g.setHomeNetwork(0xAABBCCDDEEFFull, "HOME");
    CHECK(g.joinCrew(0));
    g.onButton({Button::A, true, true});
    CHECK(g.combat().overrideCrewRows() == 1);
    const int crewRow = g.combat().overrideMoveCount() +
                        static_cast<int>(g.combat().overrideItems().size());
    while (g.combat().overridePick() != crewRow) g.onButton(press(Button::A));
    g.onButton(press(Button::B));
    CHECK(g.combat().player().crewExploit.charges == kCrews[0].exploit.magnitude);
}

// v36: the crew id + home network round-trip, an unknown crew id loads as
// unaffiliated, and a pre-v36 blob migrates to neither.
static void test_save_v36_crew_roundtrip() {
    SaveData a; std::strcpy(a.activeId, "paypup"); a.generation = 1;
    std::strcpy(a.crewId, "deniers_of_service");
    a.homeNetworkKey = 0x0123456789ABull;
    std::strcpy(a.homeNetworkName, "HOME_AP");
    SaveData out;
    CHECK(deserializeSave(serializeSave(a), out));
    CHECK(std::strcmp(out.crewId, "deniers_of_service") == 0);
    CHECK(out.homeNetworkKey == 0x0123456789ABull);
    CHECK(std::strcmp(out.homeNetworkName, "HOME_AP") == 0);

    MemSaveStore store; store.save(serializeSave(a));
    Game g(StartMode::Hatched, "paypup", &store);
    CHECK(g.crewIndex() == 0 && g.hasHomeNetwork());
    CHECK(std::strcmp(g.homeNetworkName(), "HOME_AP") == 0);

    // A crew id no longer in the table loads as unaffiliated, never as whatever now
    // occupies that row.
    SaveData b = a; std::strcpy(b.crewId, "no_such_crew");
    MemSaveStore store2; store2.save(serializeSave(b));
    Game g2(StartMode::Hatched, "paypup", &store2);
    CHECK(g2.crewIndex() == -1 && g2.hasHomeNetwork());

    std::vector<uint8_t> blob = serializeSave(a);
    blob[4] = 35; blob[5] = 0;      // version-gated: the v36+ tails go unread
    SaveData migrated;
    CHECK(deserializeSave(blob, migrated));
    CHECK(migrated.crewId[0] == '\0' && migrated.homeNetworkKey == 0);
    CHECK(migrated.homeNetworkName[0] == '\0');
}

// buildPediaStateJson: pets{} distinguishes "seen" from "hatched"/"locked";
// malbeasts{} reflects defeated > seen > locked; achievements{} reflects the
// unlocked bits. Seeded via a v25 save load (applySave) rather than the runtime
// triggers, so this isolates the JSON-shape half from the trigger-wiring half
// (covered separately below).
static void test_pedia_state_json_reveal_states() {
    SaveData a; std::strcpy(a.activeId, "pingcub"); a.generation = 1;
    a.seenCreatures.push_back(SaveId{"malbear"});   // glimpsed, not hatched
    a.malbeastSeen = 1 << 1;                        // Segfault Pup — seen only
    a.malbeastDefeated = 1 << 0;                    // GlitchHog — defeated
    MemSaveStore store; store.save(serializeSave(a));
    Game g(StartMode::Hatched, "paypup", &store);   // hatchedCreature ignored: store wins

    const std::string json = buildPediaStateJson(g);
    CHECK(json.find("\"pingcub\":\"hatched\"") != std::string::npos);
    CHECK(json.find("\"malbear\":\"seen\"") != std::string::npos);
    CHECK(json.find("\"glitchhog\":\"defeated\"") != std::string::npos);
    CHECK(json.find("\"segfault_pup\":\"seen\"") != std::string::npos);
    CHECK(json.find("\"cache_ghoul\":\"locked\"") != std::string::npos);
    CHECK(json.find("\"AIR_GAPPED\":\"incomplete\"") != std::string::npos);

    g.unlockAchievement(ach::kAirGapped);
    const std::string json2 = buildPediaStateJson(g);
    CHECK(json2.find("\"AIR_GAPPED\":\"complete\"") != std::string::npos);
    CHECK(json2.find("\"WORM_WHISPERER\":\"incomplete\"") != std::string::npos);
}

// The raised tally is what the 'Pedia reveals from, so a species stays revealed
// after the pet stops being it. Raise a line by real evolutions and every rung
// behind the live pet must still read "hatched" — the possession-only test this
// replaced re-encrypted each one the moment its successor stamped over it.
static void test_pedia_raised_tally_survives_evolution() {
    Game g{StartMode::Hatched, "pingcub"};
    CHECK(g.creatureRaised("pingcub"));
    CHECK(g.speciesRaised() == 1);

    g.debugTriggerEvolution();                        // pingcub -> malbear
    uint32_t t = 0; advanceToReveal(g, t);
    g.onButton(press(Button::B));
    CHECK(g.pet() && std::strcmp(g.pet()->id, "malbear") == 0);
    CHECK(g.creatureRaised("pingcub"));               // the species it USED to be
    CHECK(g.creatureRaised("malbear"));
    CHECK(g.speciesRaised() == 2);

    g.model().setCareMistakes(0);
    g.debugTriggerEvolution();                        // malbear -> bruinforce
    t = 0; advanceToReveal(g, t);
    g.onButton(press(Button::B));
    CHECK(g.speciesRaised() == 3);

    const std::string json = buildPediaStateJson(g);
    CHECK(json.find("\"pingcub\":\"hatched\"") != std::string::npos);
    CHECK(json.find("\"malbear\":\"hatched\"") != std::string::npos);
    CHECK(json.find("\"bruinforce\":\"hatched\"") != std::string::npos);
    CHECK(json.find("\"species_raised\":3") != std::string::npos);
    // The not-taken branch sibling was neither raised nor fought, so it stays locked —
    // walking a line reveals the rungs you walked, not the one you didn't.
    CHECK(!g.creatureRaised("berserkernel"));
    CHECK(json.find("\"berserkernel\":\"locked\"") != std::string::npos);
}

// Save v39: an evolved-past species reaches the blob and comes back — the one thing
// no earlier version could express. A pre-v39 blob has no tally, so the loader
// rebuilds what it can from the active pet + rack + records; anything evolved past
// is simply gone from that format, and the test says so.
// v41: a creature that was renamed in content is renamed on the WIRE too, so a blob
// written before the rename still resolves. Every creature-id-bearing field has to be
// covered — missing one doesn't fail loudly, it silently drops that pet/record/tally,
// which is exactly the failure this guards.
static void test_save_v41_renames_retired_creature_ids() {
    SaveData a;
    std::strcpy(a.activeId, "grizzgabyte_good");
    SaveStoredPet stored; std::strcpy(stored.id, "grizzgabyte_bad");
    a.rack.push_back(stored);
    SaveRecord rec; std::strcpy(rec.id, "grizzgabyte_good");
    a.records.push_back(rec);
    a.seenCreatures.push_back(SaveId{"grizzgabyte_bad"});
    a.raisedCreatures.push_back(SaveId{"grizzgabyte_good"});
    a.speciesDiveIds.push_back(SaveId{"grizzgabyte_bad"});
    a.speciesDiveDepths.push_back(12);

    std::vector<uint8_t> blob = serializeSave(a);
    blob[4] = 40; blob[5] = 0;                      // stamp back to v40
    SaveData out;
    CHECK(deserializeSave(blob, out));
    CHECK(std::strcmp(out.activeId, "bruinforce") == 0);
    CHECK(std::strcmp(out.rack[0].id, "berserkernel") == 0);
    CHECK(std::strcmp(out.records[0].id, "bruinforce") == 0);
    CHECK(std::strcmp(out.seenCreatures[0].id, "berserkernel") == 0);
    CHECK(std::strcmp(out.raisedCreatures[0].id, "bruinforce") == 0);
    CHECK(std::strcmp(out.speciesDiveIds[0].id, "berserkernel") == 0);

    // The renamed pet is a LIVE pet again, not a dropped id — which is the point of
    // doing this in the codec rather than leaving an alias in the content tables.
    MemSaveStore store; store.save(blob);
    Game g(StartMode::Hatched, "paypup", &store);
    CHECK(g.pet() && std::strcmp(g.pet()->id, "bruinforce") == 0);
    CHECK(g.creatureSeen("berserkernel"));
    CHECK(g.creatureRaised("bruinforce"));

    // A CURRENT blob carries current ids and is left alone — the pass is gated on the
    // version, so it can't keep rewriting ids forever.
    SaveData b;
    std::strcpy(b.activeId, "bruinforce");
    SaveData back;
    CHECK(deserializeSave(serializeSave(b), back));
    CHECK(std::strcmp(back.activeId, "bruinforce") == 0);
}

// v43: NET-SEA CROSSING spliced into the MIDDLE of the ladder. Every persisted EXPL field
// is positional, so a v42 blob's flags from rung 2 on describe the area now one rung to
// their left. The failure this guards is silent and generous in the worst direction — the
// player is handed the new area pre-cleared and loses the deepest one they actually beat —
// so it is asserted field by field on a save whose rungs are deliberately all different.
static void test_save_v43_ladder_insert_shifts_expl_progress() {
    SaveData a;
    // A v42 ladder: 4 rungs, cleared through rung 2 (the old Napstorrent) and mid-run on 3.
    a.sectorCleared    = {1, 1, 1, 0};
    a.bossUnlocked     = {1, 1, 1, 0};
    a.subCleared       = {0x1F, 0x1F, 0x1F, 0x03};   // bitmask per area, bit s = sub s
    a.subBossUnlocked  = {0x1F, 0x1F, 0x1F, 0x07};
    a.subRefarm.assign(4 * kSubAreasPerArea, 0);
    for (int i = 0; i < 4 * kSubAreasPerArea; ++i)
        a.subRefarm[i] = static_cast<uint16_t>(100 + i);   // every cell distinguishable
    a.titlesUnlocked = 0b0111;                        // rungs 0,1,2 earned
    a.equippedTitle  = 2;                             // showing the old rung 2's Title

    std::vector<uint8_t> blob = serializeSave(a);
    blob[4] = 42; blob[5] = 0;                        // stamp back to v42
    SaveData out;
    CHECK(deserializeSave(blob, out));

    // Rungs 0 and 1 are BELOW the splice and must not move at all.
    CHECK(out.sectorCleared.size() == 5);
    CHECK(out.sectorCleared[0] == 1 && out.sectorCleared[1] == 1);
    CHECK(out.bossUnlocked[0] == 1 && out.bossUnlocked[1] == 1);
    CHECK(out.subCleared[0] == 0x1F && out.subCleared[1] == 0x1F);
    CHECK(out.subBossUnlocked[0] == 0x1F && out.subBossUnlocked[1] == 0x1F);
    // Rung 2 is the new area: blank, so it has to be played rather than arriving beaten.
    CHECK(out.sectorCleared[2] == 0);
    CHECK(out.bossUnlocked[2] == 0);
    CHECK(out.subCleared[2] == 0);
    CHECK(out.subBossUnlocked[2] == 0);
    // ...and everything the player DID beat is still beaten, one rung deeper.
    CHECK(out.sectorCleared[3] == 1 && out.sectorCleared[4] == 0);
    CHECK(out.subCleared[3] == 0x1F && out.subCleared[4] == 0x03);
    CHECK(out.subBossUnlocked[3] == 0x1F && out.subBossUnlocked[4] == 0x07);

    // subRefarm moves a whole ROW, not a cell: the new rung's five counts read 0 and every
    // old count keeps the sub-area it was earned in.
    CHECK(out.subRefarm.size() == 5 * kSubAreasPerArea);
    for (int i = 0; i < 2 * kSubAreasPerArea; ++i)
        CHECK(out.subRefarm[i] == 100 + i);                       // below the splice
    for (int s = 0; s < kSubAreasPerArea; ++s)
        CHECK(out.subRefarm[2 * kSubAreasPerArea + s] == 0);      // the opened row
    for (int i = 2 * kSubAreasPerArea; i < 4 * kSubAreasPerArea; ++i)
        CHECK(out.subRefarm[i + kSubAreasPerArea] == 100 + i);    // shifted, order intact

    // The Titles are a bitmask over the same rungs, and the equipped one is an index into
    // it — both move, or the player's shown Title silently becomes a different area's.
    CHECK(out.titlesUnlocked == 0b1011);              // rungs 0,1 stay; old 2 -> 3
    CHECK(out.equippedTitle == 3);

    // A CURRENT blob is left alone — the pass is version-gated, so it cannot keep
    // shifting the same save every time it loads.
    std::vector<uint8_t> now = serializeSave(a);
    SaveData back;
    CHECK(deserializeSave(now, back));
    CHECK(back.sectorCleared.size() == 4);
    CHECK(back.sectorCleared[2] == 1);
    CHECK(back.titlesUnlocked == 0b0111);
    CHECK(back.equippedTitle == 2);
}

// The splice table's own invariants, so save.h's rules are enforced rather than merely
// written down. A stale row here re-shifts a save that was already correct, which no
// other test would notice.
static void test_ladder_inserts_table_invariants() {
    int n = 0;
    const LadderInsert* rows = ladderInserts(n);
    for (int i = 0; i < n; ++i) {
        // Retirement: a row whose blobs the codec no longer opens is dead weight, so
        // raising kOldestAcceptedVersion is what tells you to delete it.
        CHECK(rows[i].sinceVersion > kOldestAcceptedVersion);
        CHECK(rows[i].sinceVersion <= kSaveVersion);
        // A splice lands ON the ladder. Past its end it would be an append, which needs
        // no row at all — so such a row is a mistake, not a no-op.
        CHECK(rows[i].atIndex >= 0);
        CHECK(rows[i].atIndex < kExplSectors);
        // Oldest-first, which is what lets each atIndex read against the ladder its
        // predecessors built instead of being restated whenever a later splice lands.
        if (i > 0) CHECK(rows[i - 1].sinceVersion <= rows[i].sinceVersion);
    }
}

// An area's sector glyph is keyed by its own id, never by its rung. That is the whole
// reason the name lives on the row, and it is exactly the sort of thing a copy-pasted
// area.cpp breaks silently: two areas sharing a glyph still resolves, still draws, and
// shows the wrong picture. Derived here from the id rather than compared against a list,
// so a new area is covered the moment it joins the ladder.
static void test_area_icons_are_keyed_by_area_id() {
    for (int i = 0; i < kAreaCount; ++i) {
        const AreaDef& a = area(i);
        CHECK(a.icon && a.icon[0]);
        char want[64];
        std::snprintf(want, sizeof(want), "ICON_SECTOR_%s", a.id);
        for (char* p = want; *p; ++p)
            *p = static_cast<char>(std::toupper(static_cast<unsigned char>(*p)));
        if (std::strcmp(a.icon, want) != 0)
            std::printf("  AREA ICON OFF ITS ID: %s names %s, wanted %s\n", a.id, a.icon,
                        want);
        CHECK(std::strcmp(a.icon, want) == 0);
        // ...and no two areas can end up pointing at one picture.
        for (int j = i + 1; j < kAreaCount; ++j)
            CHECK(std::strcmp(area(j).icon, a.icon) != 0);
    }
}

// The DEFRAG minigame's rules (core/model/stacker.h), driven a press at a time. The model
// is deterministic on purpose — no RNG anywhere — which is what lets a run be replayed
// exactly here, and what makes the variant it backs a test of skill rather than luck.
namespace {
// Slide the run until its left edge reaches `col`, then lock it. Returns false if the
// column is unreachable, which would make the test a no-op rather than a failure.
bool stackerDropAt(Stacker& s, int col) {
    for (int guard = 0; guard < 4 * kStackerCols; ++guard) {
        if (s.left() == col) { s.drop(); return true; }
        s.step();
    }
    return false;
}
// How many blocks are locked in a row.
int stackerRowCount(const Stacker& s, int r) {
    int n = 0;
    for (int c = 0; c < kStackerCols; ++c) if (s.locked(r, c)) ++n;
    return n;
}
}  // namespace

static void test_stacker_shaves_the_overhang() {
    Stacker s;
    CHECK(s.running());
    CHECK(s.width() == kStackerStartWidth);
    CHECK(s.row() == 0);

    // The BASE row rests on the floor, so it keeps everything wherever it lands.
    CHECK(stackerDropAt(s, 1));
    CHECK(stackerRowCount(s, 0) == kStackerStartWidth);
    CHECK(s.locked(0, 1) && s.locked(0, 2) && s.locked(0, 3));
    CHECK(!s.locked(0, 0) && !s.locked(0, 4));
    CHECK(s.row() == 1);
    CHECK(s.width() == kStackerStartWidth);   // nothing shaved yet

    // Row 1 offset by one: only the two columns over the row below survive, and the run
    // in hand narrows by exactly the overhang rather than by a fixed step.
    CHECK(stackerDropAt(s, 2));
    CHECK(stackerRowCount(s, 1) == 2);
    CHECK(s.locked(1, 2) && s.locked(1, 3));
    CHECK(!s.locked(1, 4));                   // the overhanging block is gone, not moved
    CHECK(s.width() == 2);
    CHECK(s.row() == 2);
    CHECK(s.running());
}

static void test_stacker_missing_entirely_loses() {
    Stacker s;
    CHECK(stackerDropAt(s, 0));               // base row at columns 0..2
    CHECK(s.running());
    // A drop with no column in common with the row below keeps nothing, and a run with
    // no blocks left is the end of it — the only losing condition there is.
    CHECK(stackerDropAt(s, 4));
    CHECK(!s.running());
    CHECK(!s.won());
    CHECK(s.state() == Stacker::State::Lost);
    CHECK(stackerRowCount(s, 1) == 0);
    // A finished run ignores further input rather than resuming somewhere odd.
    const int endRow = s.row();
    s.step();
    s.drop();
    CHECK(s.row() == endRow);
    CHECK(s.state() == Stacker::State::Lost);
}

static void test_stacker_clearing_every_row_wins() {
    Stacker s;
    // Drop every row at the same column: nothing ever overhangs, so the run keeps its
    // full width to the top. That is the ceiling on how well a run can go — reaching the
    // last row is the win, and it is reachable without ever narrowing.
    for (int r = 0; r < kStackerRows; ++r) {
        CHECK(s.running());
        CHECK(s.row() == r);
        CHECK(stackerDropAt(s, 0));
    }
    CHECK(!s.running());
    CHECK(s.won());
    CHECK(s.width() == kStackerStartWidth);
    for (int r = 0; r < kStackerRows; ++r) CHECK(stackerRowCount(s, r) == kStackerStartWidth);

    s.reset();                                 // a fresh run starts over cleanly
    CHECK(s.running() && s.row() == 0 && s.width() == kStackerStartWidth);
    for (int r = 0; r < kStackerRows; ++r) CHECK(stackerRowCount(s, r) == 0);
}

// The run stays ON the board and stays CONTIGUOUS, which the drawing and the shave both
// assume — a run that walked off an edge or split in two would corrupt the board rather
// than fail visibly. Walked over a long slide and a deliberately narrowing game.
static void test_stacker_run_stays_on_board_and_contiguous() {
    Stacker s;
    for (int i = 0; i < 200; ++i) {            // several full bounces
        s.step();
        CHECK(s.left() >= 0);
        CHECK(s.left() + s.width() <= kStackerCols);
    }
    // Narrow it by shifting one column each row, and confirm every locked row is one
    // unbroken run — the property that lets a row be described by a left edge and a width.
    Stacker n;
    int col = 0;
    while (n.running() && col + 1 < kStackerCols) {
        if (!stackerDropAt(n, col)) break;
        ++col;
    }
    for (int r = 0; r < kStackerRows; ++r) {
        int first = -1, last = -1, count = 0;
        for (int c = 0; c < kStackerCols; ++c)
            if (n.locked(r, c)) { if (first < 0) first = c; last = c; ++count; }
        if (count == 0) continue;
        CHECK(last - first + 1 == count);      // no gaps
    }
}

// `rowWidth` is what the achievement rows read, and it has to answer for the TOP row of a
// won board — the one case `width()` can't cover, since a win stops the run before the
// survivors become the next hand.
static void test_stacker_row_width_reports_the_survivors() {
    Stacker s;
    CHECK(s.rowWidth(0) == 0);                 // nothing locked yet
    CHECK(stackerDropAt(s, 1));
    CHECK(s.rowWidth(0) == kStackerStartWidth);
    CHECK(stackerDropAt(s, 2));                // one column of overhang, shaved
    CHECK(s.rowWidth(1) == 2);
    CHECK(s.rowWidth(-1) == 0 && s.rowWidth(kStackerRows) == 0);   // out of range
}

// score() is the whole reward curve, so the thing worth pinning is that HEIGHT is paid
// for twice — a block on a high row is worth more than the same block low down, which is
// what makes stalling two rows short beat stalling halfway.
static void test_stacker_score_pays_for_height_twice() {
    Stacker s;
    CHECK(s.score() == 0);                        // nothing locked, nothing earned

    // Base row (level 1) at full width.
    CHECK(stackerDropAt(s, 0));
    CHECK(s.score() == kStackerStartWidth);

    // Row 1 (level 2) at full width again: three more blocks, each worth two.
    CHECK(stackerDropAt(s, 0));
    CHECK(s.score() == kStackerStartWidth * 3);

    // A clean board all the way up is the ceiling, and the ceiling is the constant the
    // payout rate is calibrated against.
    Stacker best;
    for (int r = 0; r < kStackerRows; ++r) CHECK(stackerDropAt(best, 0));
    CHECK(best.won());
    CHECK(best.score() == kStackerMaxScore);

    // Two boards that reached the same height with different survivors: the wider one is
    // worth more, and a board is scored on what LOCKED, never on the run still in hand.
    Stacker narrow;
    CHECK(stackerDropAt(narrow, 0));
    CHECK(stackerDropAt(narrow, 2));              // shaved to one block
    CHECK(narrow.rowWidth(1) == 1);
    CHECK(narrow.score() == kStackerStartWidth + 2);
    CHECK(narrow.running() && narrow.row() == 2);  // the hand on level 3 counts for nothing
}

namespace {
// Play a whole board through the REAL button path, locking each row at `col(row)`. Leaves
// the run parked on its result; the caller presses once more to finish it. False if a
// column turned out unreachable, which would make the test a no-op rather than a failure.
template <typename ColFn>
bool playStackerBoard(Game& g, ColFn col) {
    for (int r = 0; r < kStackerRows && g.stacker().running(); ++r) {
        const int want = col(r);
        bool aligned = false;
        for (int guard = 0; guard < 4 * kStackerCols; ++guard) {
            if (g.stacker().left() == want) { aligned = true; break; }
            g.debugStepStacker();
        }
        if (!aligned) return false;
        g.onButton(press(Button::B));
    }
    return true;
}
}  // namespace

// The two rows a board's SHAPE earns, and the tally underneath them. A perfect board and
// a one-block board are the two ends of the same measurement, so they are asserted
// against each other rather than one at a time — reading the wrong end of it is the
// mistake worth catching.
static void test_stacker_win_credits_the_tally_and_the_shape_rows() {
    // Column 0 every row: nothing ever overhangs, so the run reaches the top at full
    // width. That is the perfect board, and NOT the narrow one.
    Game g{StartMode::Hatched};
    g.model().setFragmentation(60);
    CHECK(g.stackerWins() == 0);
    g.debugStartStackerDefrag();
    CHECK(playStackerBoard(g, [](int) { return 0; }));
    CHECK(g.stacker().won());
    CHECK(g.stacker().rowWidth(kStackerRows - 1) == kStackerStartWidth);
    g.onButton(press(Button::B));                   // park -> finish
    CHECK(g.stackerWins() == 1);
    CHECK(g.hasAchievement(ach::kPerfectDefrag));
    CHECK(!g.hasAchievement(ach::kHangingByABit));

    // Step one column right per row until the run is down to a single block, then hold
    // that column to the top: a win, but the narrowest one there is.
    Game n{StartMode::Hatched};
    n.model().setFragmentation(60);
    n.debugStartStackerDefrag();
    CHECK(playStackerBoard(n, [](int r) { return r < 2 ? r : 2; }));
    CHECK(n.stacker().won());
    CHECK(n.stacker().rowWidth(kStackerRows - 1) == 1);
    n.onButton(press(Button::B));
    CHECK(n.stackerWins() == 1);
    CHECK(n.hasAchievement(ach::kHangingByABit));
    CHECK(!n.hasAchievement(ach::kPerfectDefrag));
}

// A board that ends in mid-air is NOT a failed defrag: it pays for the blocks it landed
// and costs nothing extra. The two halves of that are asserted together because taking
// only one of them is the mistake — a partial payout that still charged the care mistake
// would be worse than the old all-or-nothing rule.
static void test_stacker_short_board_pays_what_it_stacked_and_costs_nothing() {
    Game g{StartMode::Hatched};
    g.model().setFragmentation(60);
    const int mistakes = g.model().careMistakes();
    g.debugStartStackerDefrag();
    // Base row at 0..2, then a drop with no column in common with it — the run keeps
    // nothing and the board ends there, well short of the top.
    CHECK(playStackerBoard(g, [](int r) { return r == 0 ? 0 : 4; }));
    CHECK(!g.stacker().running() && !g.stacker().won());
    CHECK(g.stacker().row() < kStackerRows - 1);
    const int worth = g.stacker().score() / kStackerScorePerFrag;
    g.onButton(press(Button::B));
    CHECK(g.model().fragmentation() == 60 - worth);     // cleaned, just not much
    CHECK(g.model().careMistakes() == mistakes);        // no failure, so no mistake
    CHECK(!g.model().hasGhost());                       // and no fork, however bad the disk
    CHECK(g.stackerWins() == 0);                        // the tally counts CLEARED boards
    CHECK(!g.hasAchievement(ach::kPerfectDefrag));
    CHECK(!g.hasAchievement(ach::kHangingByABit));

    // The rate is what makes height worth climbing for: the same run stopped one row
    // higher has to be worth strictly more.
    Game low{StartMode::Hatched};
    low.model().setFragmentation(90);
    low.debugStartStackerDefrag();
    CHECK(playStackerBoard(low, [](int r) { return r < 2 ? 0 : 4; }));
    Game high{StartMode::Hatched};
    high.model().setFragmentation(90);
    high.debugStartStackerDefrag();
    CHECK(playStackerBoard(high, [](int r) { return r < 6 ? 0 : 4; }));
    low.onButton(press(Button::B));
    high.onButton(press(Button::B));
    CHECK(high.model().fragmentation() < low.model().fragmentation());
    CHECK(low.model().fragmentation() < 90);            // both still cleaned something

    // A board with a critical disk under it stays a partial clean, not a ghost fork:
    // the played path never reaches the failed-defrag branch that raises one.
    Game crit{StartMode::Hatched};
    crit.model().setFragmentation(kFragCriticalMin + 10);
    crit.debugStartStackerDefrag();
    CHECK(playStackerBoard(crit, [](int r) { return r == 0 ? 0 : 4; }));
    crit.onButton(press(Button::B));
    CHECK(!crit.model().hasGhost());
}

// A cleared board is a FULL defrag — the only thing in the game that takes a disk to
// zero, and the reason the variant is worth its difficulty.
static void test_stacker_cleared_board_wipes_the_disk() {
    Game g{StartMode::Hatched};
    g.model().setFragmentation(95);                     // far past what one defrag fixes
    g.debugStartStackerDefrag();
    CHECK(playStackerBoard(g, [](int) { return 0; }));
    CHECK(g.stacker().won());
    g.onButton(press(Button::B));
    CHECK(g.model().fragmentation() == 0);
    CHECK(g.model().careMistakes() == 0);

    // The narrowest possible win is still a win, so it wipes the disk exactly the same:
    // the top row's width buys achievement rows, never a bigger or smaller clean.
    Game n{StartMode::Hatched};
    n.model().setFragmentation(95);
    n.debugStartStackerDefrag();
    CHECK(playStackerBoard(n, [](int r) { return r < 2 ? r : 2; }));
    CHECK(n.stacker().won() && n.stacker().rowWidth(kStackerRows - 1) == 1);
    n.onButton(press(Button::B));
    CHECK(n.model().fragmentation() == 0);
}

// C stops the run early, and banking is the point: since a board that runs out of blocks
// keeps what it locked, quitting has to keep it too, or C would be a button that throws
// away Fragmentation the player already earned.
static void test_stacker_stopping_early_banks_the_board() {
    Game g{StartMode::Hatched};
    g.model().setFragmentation(70);
    g.debugStartStackerDefrag();
    for (int r = 0; r < 4; ++r) {
        for (int guard = 0; guard < 4 * kStackerCols; ++guard) {
            if (g.stacker().left() == 0) break;
            g.debugStepStacker();
        }
        g.onButton(press(Button::B));
    }
    CHECK(g.stacker().running());
    const int worth = g.stacker().score() / kStackerScorePerFrag;
    CHECK(worth > 0);
    g.onButton(press(Button::C));
    CHECK(g.model().fragmentation() == 70 - worth);
    CHECK(g.model().careMistakes() == 0);
    CHECK(g.stackerWins() == 0);
}

// The counting ladder rides the ordinary sweep, like every other counted series, and the
// tally is PLAYER-level: it has to survive the pet that set it.
static void test_stacker_wins_ladder_sweeps_and_persists() {
    Game g{StartMode::Hatched};
    uint32_t t = 0;
    g.debugAddStackerWins(10);
    g.tick(t += kAchSweepIntervalMs);
    CHECK(g.hasAchievement("DEFRAG_BY_HAND"));
    CHECK(g.hasAchievement("STACK_10"));
    CHECK(!g.hasAchievement("STACK_50"));

    // v44 on the wire, and back into a Game: the tally is the operator's, so it has to
    // come back on a device that has been power-cycled since.
    SaveData a; std::strcpy(a.activeId, "paypup"); a.generation = 1;
    a.stackerWins = 10;
    SaveData out;
    CHECK(deserializeSave(serializeSave(a), out));
    CHECK(out.stackerWins == 10);
    MemSaveStore store; store.save(serializeSave(a));
    Game loaded(StartMode::Hatched, "paypup", &store);
    CHECK(loaded.stackerWins() == 10);

    // A pre-v44 blob starts the ladder at zero rather than inheriting the pet's own
    // defrag tally, which counts bought and rolled cleans too.
    SaveData old; std::strcpy(old.activeId, "paypup"); old.generation = 1;
    old.defragCount = 40;
    std::vector<uint8_t> blob = serializeSave(old);
    blob[4] = 43; blob[5] = 0;                     // stamp back to v43
    MemSaveStore oldStore; oldStore.save(blob);
    Game migrated(StartMode::Hatched, "paypup", &oldStore);
    CHECK(migrated.stackerWins() == 0);
    CHECK(!migrated.hasAchievement("DEFRAG_BY_HAND"));
}

// The ROLLED/BOUGHT defrag seam, which the played one deliberately does not share: a
// Quick or Tool run takes a fixed bite or pays a fixed penalty, and the penalty is where
// the care mistake lives. Asserted here so a change to the played variant's own payout
// can't quietly move what the other two are worth.
static void test_rolled_defrag_takes_its_fixed_bite() {
    Game g{StartMode::Hatched};
    g.model().setFragmentation(60);
    const int before = g.model().fragmentation();
    g.debugResolveDefrag(true);
    CHECK(g.model().fragmentation() == before - kDefragReduction);

    // ...and a lost board costs what a failed Quick defrag costs: the frag penalty plus
    // the care mistake, not merely "no clean".
    Game g2{StartMode::Hatched};
    g2.model().setFragmentation(60);
    const int mistakes = g2.model().careMistakes();
    const int frag = g2.model().fragmentation();
    g2.debugResolveDefrag(false);
    CHECK(g2.model().fragmentation() == frag + kMaintFailPenalty);
    CHECK(g2.model().careMistakes() == mistakes + 1);
}

// The Replication Ghost's whole lifecycle, which until now had no way to begin: the
// model carried a flag, a setter and a save field that nothing ever set true, so the AV
// screen's GHOST state and the AIR_GAPPED row were both unreachable by play.
static void test_replication_ghost_is_raised_by_a_failed_defrag_on_a_critical_disk() {
    // A failed defrag on a disk that is NOT yet critical is just a failed defrag.
    Game ok{StartMode::Hatched};
    ok.model().setFragmentation(kFragCriticalMin - kMaintFailPenalty - 1);
    ok.debugResolveDefrag(false);
    CHECK(ok.model().fragmentation() < kFragCriticalMin);
    CHECK(!ok.model().hasGhost());

    // Fail one on an already-critical disk and the write forks a phantom copy.
    Game g{StartMode::Hatched};
    g.model().setFragmentation(kFragCriticalMin);
    CHECK(!g.model().hasGhost());
    g.debugResolveDefrag(false);
    CHECK(g.model().hasGhost());

    // A SUCCESSFUL defrag never raises one, however bad the disk was.
    Game s{StartMode::Hatched};
    s.model().setFragmentation(90);
    s.debugResolveDefrag(true);
    CHECK(!s.model().hasGhost());
}

// The cure, and the achievement that marks it. The snack stays an ordinary food when
// there is nothing to cure — which is the common case, and must not unlock anything.
static void test_air_gapped_snack_cures_the_ghost_and_unlocks() {
    Game g{StartMode::Hatched};
    g.inventory().add("airgap_snack", 2);

    // Eaten with no ghost: it feeds, and that is all. No unlock.
    g.model().setHunger(20);
    g.debugUseItem("airgap_snack");
    CHECK(g.model().hunger() > 20);                 // still an ordinary snack
    CHECK(!g.hasAchievement(ach::kAirGapped));

    // Raise a ghost the only way there is, then cure it.
    g.model().setFragmentation(kFragCriticalMin);
    g.debugResolveDefrag(false);
    CHECK(g.model().hasGhost());
    g.debugUseItem("airgap_snack");
    CHECK(!g.model().hasGhost());
    CHECK(g.hasAchievement(ach::kAirGapped));
}

// The AV scan is the other cure, and the two are deliberately the same shape as the
// defrag variants: the scan is free but can fail, the snack is an item that cannot. So a
// ghost must be reachable AND clearable through both, or one of the pair is decoration.
static void test_ghost_also_clears_through_a_successful_av_scan() {
    Game g{StartMode::Hatched};
    g.model().setFragmentation(kFragCriticalMin);
    g.debugResolveDefrag(false);
    CHECK(g.model().hasGhost());
    CHECK(!avGated(g.model()));                     // a ghost is work for the AV screen
    CHECK(!g.model().applyAntivirus(true));          // succeeded
    CHECK(!g.model().hasGhost());
    // Curing it this way is NOT the Air-Gapped achievement — that row names the snack.
    CHECK(!g.hasAchievement(ach::kAirGapped));
}

// Spoilage: a perishable held through a feeding turns into its `spoilsInto` row, and the
// conversion is one-for-one — a stack that rots must not lose or gain count, which is the
// failure a percentage roll would otherwise hide behind "unlucky".
static void test_perishable_food_spoils_on_a_feeding() {
    Game g(StartMode::Hatched, "paypup");
    g.inventory().add("fresh_macrol", 40);
    int spoilages = 0, prevFresh = 40;
    for (int i = 0; i < 200; ++i) {
        g.inventory().add("airgap_snack", 1);
        g.debugUseItem("airgap_snack");   // eat something ELSE — the beat, not the item

        const int fresh = g.inventory().count("fresh_macrol");
        CHECK(fresh + g.inventory().count("spoiled_macrol") == 40);  // conserved either way
        CHECK(fresh >= prevFresh - 1);        // at most one unit per disturbance
        if (fresh < prevFresh) ++spoilages;
        prevFresh = fresh;
    }
    // A 5% roll that never fires across 200 feedings isn't riding the beat at all.
    CHECK(spoilages > 0);

    // An imperishable row never converts, however much the bag is disturbed — and the
    // chain stops after one stage rather than rotting away to nothing.
    Game h(StartMode::Hatched, "paypup");
    h.inventory().add("spoiled_macrol", 3);
    for (int i = 0; i < 200; ++i) {
        h.inventory().add("airgap_snack", 1);
        h.debugUseItem("airgap_snack");
    }
    CHECK(h.inventory().count("spoiled_macrol") == 3);
}

// Tinting: an icon master is a flat fill, so drawing it in another colour must keep
// its shape exactly and change nothing else. And the theme index has to be the only
// thing a colour lookup depends on — that indirection is what a theme swap rides on.
static void test_icon_tint_and_theme_indirection() {
    const SpriteData* icon = embeddedAssets().sprite("ICON_ITEM_SEALED_CACHE");
    CHECK(icon != nullptr);

    Framebuffer plain(kActiveW, kActiveH), tinted(kActiveW, kActiveH);
    const Rgb565 bg = palColor(Pal::PAPER), ink = palColor(Pal::RARITY_EPIC);
    plain.clear(bg);
    tinted.clear(bg);
    drawSprite(plain, *icon, 0, 4, 4);
    drawSpriteTinted(tinted, *icon, 0, 4, 4, ink);

    int shape = 0;
    for (int y = 0; y < kActiveH; ++y) {
        for (int x = 0; x < kActiveW; ++x) {
            const bool onA = plain.get(x, y) != bg;
            const bool onB = tinted.get(x, y) != bg;
            CHECK(onA == onB);                       // identical silhouette
            if (onB) { ++shape; CHECK(tinted.get(x, y) == ink); }
        }
    }
    CHECK(shape > 0);                                // it actually drew something

    // The tier is countable BEFORE it is coloured: the four cache masters differ in
    // how many chevrons they carry, which is why tinting them stays decoration and
    // the ramp survives grayscale. Equal pixel counts would mean the tier had
    // quietly become colour-only.
    const char* tiers[] = {"ICON_ITEM_SEALED_CACHE_COMMON", "ICON_ITEM_SEALED_CACHE_UNCOMMON",
                           "ICON_ITEM_SEALED_CACHE_RARE", "ICON_ITEM_SEALED_CACHE_EPIC"};
    int on[4] = {0, 0, 0, 0};
    for (int i = 0; i < 4; ++i) {
        const SpriteData* s = embeddedAssets().sprite(tiers[i]);
        CHECK(s != nullptr);
        for (int y = 0; y < s->h; ++y)
            for (int x = 0; x < s->sheetW; ++x)
                if (spriteAlphaAt(*s, x, y) > 0) ++on[i];
    }
    for (int i = 0; i < 4; ++i)
        for (int j = i + 1; j < 4; ++j) CHECK(on[i] != on[j]);

    // Every lookup goes through the active theme, so an out-of-range index falls
    // back rather than reading past the table.
    setPalTheme(kPalThemeCount + 5);
    CHECK(palThemeIndex() == 0);
    setPalTheme(-1);
    CHECK(palThemeIndex() == 0);
    CHECK(palThemeName() != nullptr);
}

// The rename table's own invariants, so the rules in save.h are enforced rather than
// merely written down. Each row is a promise about ids that only ever LEAVE the content
// tables, so nothing else can notice when one goes stale.
static void test_renamed_ids_table_invariants() {
    int n = 0;
    const RenamedId* rows = renamedIds(n);
    ContentRegistry r = ContentRegistry::embedded();

    for (int i = 0; i < n; ++i) {
        // Retirement: a row whose blobs the codec no longer opens is dead weight, so
        // raising kOldestAcceptedVersion is what tells you to delete it.
        CHECK(rows[i].sinceVersion >= kOldestAcceptedVersion);
        CHECK(rows[i].sinceVersion <= kSaveVersion);

        // The point of the table: `from` is gone from content, `to` is real. A `from`
        // that still resolves means the id was never actually retired.
        CHECK(r.creature(rows[i].from) == nullptr);
        CHECK(r.creature(rows[i].to) != nullptr);

        // Flattening: distinct `from`s, and no `to` that is itself renamed — the
        // rewrite takes exactly one hop and stops.
        for (int j = i + 1; j < n; ++j) CHECK(std::strcmp(rows[i].from, rows[j].from) != 0);
        for (int j = 0; j < n; ++j) CHECK(std::strcmp(rows[i].to, rows[j].from) != 0);
    }
}

static void test_save_v39_raised_tally_roundtrip_and_migration() {
    { // A real evolution, written through the live autosave, read back cold.
        MemSaveStore store;
        { Game g(StartMode::Hatched, "pingcub", &store);
          g.debugTriggerEvolution();
          uint32_t t = 0; advanceToReveal(g, t);
          g.onButton(press(Button::B));
          CHECK(g.pet() && std::strcmp(g.pet()->id, "malbear") == 0);
          g.tick(t += kSaveDebounceMs + kHeartbeatMs); }   // flush the debounced save
        SaveData out;
        CHECK(deserializeSave(store.bytes(), out));
        CHECK(out.raisedCreatures.size() == 2);
        Game g2(StartMode::Hatched, "paypup", &store);   // hatchedCreature ignored
        CHECK(g2.pet() && std::strcmp(g2.pet()->id, "malbear") == 0);
        CHECK(g2.creatureRaised("pingcub"));               // nothing HOLDS pingcub
        CHECK(g2.speciesRaised() == 2);
    }
    { // Pre-v39 shape: drop the v39 tail and stamp the version back.
        SaveData a;
        std::strcpy(a.activeId, "malbear");
        SaveStoredPet stored; std::strcpy(stored.id, "pingcub");
        a.rack.push_back(stored);
        SaveRecord rec; std::strcpy(rec.id, "paypup");
        a.records.push_back(rec);
        a.raisedCreatures.push_back(SaveId{"bruinforce"});  // evolved past
        // Stamping the version down is the whole migration: deserialize is
        // version-gated, so it stops before the v39 tail and never reads the bytes
        // still sitting there. No truncation — a byte count would aim at the wrong
        // tail as soon as a later version appends its own.
        std::vector<uint8_t> blob = serializeSave(a);
        blob[4] = 38; blob[5] = 0;                     // stamp back to v38
        SaveData out;
        CHECK(deserializeSave(blob, out));
        CHECK(out.raisedCreatures.empty());            // the codec invents nothing

        MemSaveStore store; store.save(blob);
        Game g(StartMode::Hatched, "paypup", &store);
        CHECK(g.creatureRaised("malbear") && g.creatureRaised("pingcub") &&
              g.creatureRaised("paypup"));             // rebuilt from what's held
        CHECK(!g.creatureRaised("bruinforce"));  // unrecoverable, and not faked
        CHECK(g.speciesRaised() == 3);
    }
    { // The shape of a real in-play save that predates the tally: a mid-line pet, a
      // couple of generations behind it, and pets parked in the rack. The bar is that
      // it comes back no emptier than it went in — every species the blob still points
      // at is revealed, and the rest of the save is untouched by the migration.
        SaveData a;
        std::strcpy(a.activeId, "tadpoll");
        std::strcpy(a.hackerTag, "ALG0M3AN");
        a.generation = 2;
        a.combatLevel = 36;
        for (const char* id : {"pingcub", "paypup"}) {
            SaveStoredPet stored; std::strcpy(stored.id, id);
            a.rack.push_back(stored);
        }
        SaveRecord rec; std::strcpy(rec.id, "malbear");
        rec.generation = 1;
        a.records.push_back(rec);
        std::vector<uint8_t> blob = serializeSave(a);
        blob[4] = 38; blob[5] = 0;      // version-gated: the v39+ tails go unread

        MemSaveStore store; store.save(blob);
        Game g(StartMode::Hatched, "paypup", &store);
        CHECK(g.creatureRaised("tadpoll"));            // active
        CHECK(g.creatureRaised("pingcub") && g.creatureRaised("paypup"));  // racked
        CHECK(g.creatureRaised("malbear"));            // recorded
        CHECK(g.speciesRaised() == 4);
        // Nothing else shifted: the migration adds a tally, it does not rewrite a save.
        CHECK(std::strcmp(g.hackerTag(), "ALG0M3AN") == 0);
        CHECK(g.generation() == 2 && g.combatLevel() == 36);
        CHECK(g.rack().size() == 2 && g.records().size() == 1);
    }
}

// FULL_PEDIA_L1 wants a whole line RAISED, which is only ever true across time — a
// line is walked one stage at a time, so no player holds all of it at once and the
// possession test this replaced could never fire. Seeded through a save so the
// achievement's own re-check (fired by the evolution below) is what unlocks it.
static void test_full_pedia_achievement_reads_the_raised_tally() {
    ContentRegistry r = ContentRegistry::embedded();
    SaveData a; std::strcpy(a.activeId, "pingcub"); a.generation = 1;
    for (const CreatureDef* c : r.allCreatures())
        if (c->line && std::strcmp(c->line, "ransomware") == 0 &&
            std::strcmp(c->id, "malbear") != 0) {      // the one still to be raised
            SaveId s{};
            std::strncpy(s.id, c->id, kSaveIdCap - 1);
            a.raisedCreatures.push_back(s);
        }
    MemSaveStore store; store.save(serializeSave(a));
    Game g(StartMode::Hatched, "pingcub", &store);
    CHECK(!g.hasAchievement("FULL_PEDIA_L1"));
    CHECK(!g.creatureRaised("malbear"));

    g.debugTriggerEvolution();                         // pingcub -> malbear: the last one
    uint32_t t = 0; advanceToReveal(g, t);
    g.onButton(press(Button::B));
    CHECK(g.creatureRaised("malbear"));
    g.tick(t += kAchSweepIntervalMs);      // the sweep is what unlocks it
    CHECK(g.hasAchievement("FULL_PEDIA_L1"));
}

// --- Achievement catalogue + machinery ------------------------------------------

// The mod table carries save wire numbers for the same reason the achievement table does
// — the owned-mod pool is stored as a count per wire — so it needs the same guard. A
// duplicated number would make two mods share a slot in that array, which reads back as
// the wrong mod in the player's pool rather than as any kind of error.
static void test_mod_table_wires_are_unique_and_in_range() {
    CHECK(kModsCount > 0);
    for (int i = 0; i < kModsCount; ++i) {
        const ModDef& m = kMods[i];
        CHECK(m.id && m.id[0]);
        CHECK(m.displayName && m.displayName[0]);
        // The pool array is sized by this cap; a row past it would never persist.
        CHECK(m.wire >= 0 && m.wire < kModWireCap);
        for (int j = i + 1; j < kModsCount; ++j) {
            if (kMods[j].wire == m.wire)
                std::printf("  DUPLICATE MOD WIRE %d: %s / %s\n", m.wire, m.id, kMods[j].id);
            CHECK(kMods[j].wire != m.wire);
            CHECK(std::strcmp(kMods[j].id, m.id) != 0);
        }
    }
    // And the registry has to agree, since that is the lookup the save actually uses.
    ContentRegistry reg = ContentRegistry::embedded();
    for (int i = 0; i < kModsCount; ++i) {
        const ModDef* byWire = reg.modByWire(kMods[i].wire);
        CHECK(byWire != nullptr);
        CHECK(std::strcmp(byWire->id, kMods[i].id) == 0);
    }
    CHECK(reg.modByWire(kModWireCap) == nullptr);      // out of range resolves to nothing
    CHECK(reg.modByWire(-1) == nullptr);
}

// Table integrity. These are the invariants the whole system leans on, and every one of
// them is the sort of thing a hand-edited table breaks silently: a duplicated wire number
// would make two rows share a save bit, a renamed id would strand a call site, and a
// kGoalAll row on an unbounded series would ship an achievement nobody can ever earn.
static void test_achievement_table_is_well_formed() {
    CHECK(kAchievementCount > 0);
    for (int i = 0; i < kAchievementCount; ++i) {
        const AchievementDef& d = kAchievements[i];
        CHECK(d.id && d.id[0]);
        CHECK(d.displayName && d.displayName[0]);
        CHECK(d.trigger && d.trigger[0]);
        CHECK(d.icon && d.icon[0]);
        // The bitset is sized by this cap; a row past it would silently never persist.
        CHECK(d.wire >= 0 && d.wire < kAchievementWireCap);
        // Unique wire, unique id.
        for (int j = i + 1; j < kAchievementCount; ++j) {
            if (kAchievements[j].wire == d.wire)
                std::printf("  DUPLICATE WIRE %d: %s / %s\n", d.wire, d.id,
                            kAchievements[j].id);
            CHECK(kAchievements[j].wire != d.wire);
            CHECK(std::strcmp(kAchievements[j].id, d.id) != 0);
        }
        // Every row must be reachable: an Event row by a call site, anything else by a
        // goal that resolves to a positive number.
        if (d.series != AchSeries::Event) {
            const int goal = achievementGoal(d);
            if (goal <= 0)
                std::printf("  UNEARNABLE ROW (goal resolves to %d): %s\n", goal, d.id);
            CHECK(goal > 0);
        }
        // A series that takes a subject must have been given one.
        if (d.series == AchSeries::LineRaised || d.series == AchSeries::ItemCollected ||
            d.series == AchSeries::DeepWebDepthLine)
            CHECK(d.key && d.key[0]);
        // Reward ids must name real items — a typo here is a reward that never arrives.
        ContentRegistry r = ContentRegistry::embedded();
        for (const AchievementReward& rw : d.rewards)
            if (rw.kind == AchievementReward::Kind::Item)
                CHECK(rw.id && r.item(rw.id) != nullptr);
        // The trigger template must fully resolve — no stray braces reaching a reader.
        const EffectText txt = achievementTrigger(d);
        if (std::strchr(txt.c_str(), '{'))
            std::printf("  UNRESOLVED TRIGGER TOKEN: %s -> %s\n", d.id, txt.c_str());
        CHECK(std::strchr(txt.c_str(), '{') == nullptr);
    }
    // The ids the engine fires by hand all exist. This is the check that makes the
    // `ach::` constants worth having.
    for (const char* id : {ach::kFirstBruteForce, ach::kSurvivedLockout, ach::kFlawlessRun,
                           ach::kGoneRogue, ach::kWormWhisperer, ach::kAirGapped,
                           ach::kDevtoolsIntruder, ach::kTrojanUnleashed, ach::kFirstDuel,
                           ach::kBackUpAndDriven, ach::kNeededMoreBackup,
                           ach::kShatteredPlatter, ach::kPerfectDefrag,
                           ach::kHangingByABit,
                           ach::kDeepWebDepth8, ach::kDeepWebDepth64}) {
        if (!achievementById(id)) std::printf("  MISSING ACHIEVEMENT ID: %s\n", id);
        CHECK(achievementById(id) != nullptr);
    }
    // The legacy fourteen keep wire numbers 0-13 in their original enum order — that is
    // what makes the pre-v40 u32 mask a straight byte copy rather than a mapping table.
    const char* kLegacyWireOrder[] = {
        "FIRST_BRUTE_FORCE", "SURVIVED_LOCKOUT", "FLAWLESS_RUN", "GONE_ROGUE",
        "WORM_WHISPERER", "FULL_PEDIA_L1", "BIT_BARON", "AIR_GAPPED", "GENERATION_X",
        "DEVTOOLS_INTRUDER", "DEEPWEB_DEPTH_8", "DEEPWEB_DEPTH_64", "DEEPWEB_DEPTH_256",
        "TROJAN_UNLEASHED"};
    for (int i = 0; i < 14; ++i) {
        const AchievementDef* d = achievementById(kLegacyWireOrder[i]);
        CHECK(d && d->wire == i);
    }
}

// The three Backup Drive achievements are cut from one mapping (backupDriveAchievement):
// what the drive did, crossed with how the fight ended. Asserted directly rather than by
// staging three fights, because the mapping IS the thing that can be wrong.
//
// Compared by std::strcmp, not `==`: the two sides are `inline constexpr const char*`
// string-literal pointers read from DIFFERENT translation units (this file and
// game_combat.cpp). The variable itself is one address (C++17 guarantees that), but
// nothing guarantees the LITERAL each TU's copy of that address points at is the same
// object across TUs — only linkers that happen to fold identical string constants make
// raw `==` pass. That held on macOS/ld64 (folds by default) and failed on Linux/GCC's
// default linker, which doesn't — the same cross-TU hazard achievementById() already
// avoids by looking up ids with strcmp instead of pointer identity.
static bool sameAchId(const char* got, const char* want) {
    return got && want && std::strcmp(got, want) == 0;
}
static void test_backup_drive_achievement_mapping() {
    using BU = Combatant::BackupUse;
    using O = Combat::Outcome;
    // No drive spent earns nothing, however the fight went.
    for (O o : {O::Win, O::Lose, O::Fled, O::Ongoing})
        CHECK(backupDriveAchievement(BU::None, o) == nullptr);
    // Saved and went on to win / to lose anyway.
    CHECK(sameAchId(backupDriveAchievement(BU::Restored, O::Win), ach::kBackUpAndDriven));
    CHECK(sameAchId(backupDriveAchievement(BU::Restored, O::Lose), ach::kNeededMoreBackup));
    // Fleeing settles nothing: the pet is alive and the story isn't over.
    CHECK(backupDriveAchievement(BU::Restored, O::Fled) == nullptr);
    // The restore that wasn't enough — including on a mutual KO, which resolves as a Win
    // even though the pet never got back up.
    CHECK(sameAchId(backupDriveAchievement(BU::Overwhelmed, O::Lose), ach::kShatteredPlatter));
    CHECK(sameAchId(backupDriveAchievement(BU::Overwhelmed, O::Win), ach::kShatteredPlatter));
}

// A ladder unlocks off its series' progress with no per-row code, pays the row's own
// rewards, and does it exactly once.
static void test_achievement_ladder_unlocks_and_pays() {
    Game g{StartMode::Hatched};
    uint32_t t = 0;
    g.tick(t += kHeartbeatMs);
    CHECK(!g.hasAchievement("BOSS_FIRST"));
    const int bits0 = g.bits();
    g.debugAddBossWins(1);
    g.tick(t += kAchSweepIntervalMs);
    CHECK(g.hasAchievement("BOSS_FIRST"));
    CHECK(g.bits() == bits0 + achBitsReward("BOSS_FIRST"));
    CHECK(!g.hasAchievement("BOSS_10"));            // the next rung is still open

    // Idempotent: sweeping again neither re-pays nor re-announces.
    const int bits1 = g.bits();
    g.tick(t += kAchSweepIntervalMs);
    g.tick(t += kAchSweepIntervalMs);
    CHECK(g.bits() == bits1);

    // A row carrying an item reward puts it in the bag. BOSS_10's rung pays a cache.
    const int caches0 = g.inventory().count("sealed_cache_common");
    g.debugAddBossWins(9);
    g.tick(t += kAchSweepIntervalMs);
    CHECK(g.hasAchievement("BOSS_10"));
    CHECK(g.inventory().count("sealed_cache_common") == caches0 + 1);
}

// kGoalAll is the sentinel that makes "all of them" an ordinary row: it resolves against
// the size of the set the series counts over, so the row keeps meaning "all" when the set
// grows instead of freezing at a hand-typed number.
static void test_achievement_goal_all_tracks_the_set_size() {
    const AchievementDef* subsAll = achievementById("SUBS_ALL");
    CHECK(subsAll && subsAll->goal == kGoalAll);
    CHECK(achievementGoal(*subsAll) == kAreaCount * kSubAreasPerArea);
    const AchievementDef* beasts = achievementById("MALBEAST_ALL");
    CHECK(beasts && achievementGoal(*beasts) == kWildMalbeastCount);
    // ...and the prose quotes the resolved number, so it can't drift from the check.
    char want[64];
    std::snprintf(want, sizeof(want), "%d", achievementGoal(*subsAll));
    CHECK(std::strstr(achievementTrigger(*subsAll).c_str(), want) != nullptr);
}

// The banner is the only way an unlock reaches the player, so the rules around it matter:
// it only appears on the idle home screen, it retires on its own, and a row is marked
// announced ONLY once its banner has actually been shown.
static void test_achievement_banner_announces_on_the_home_screen() {
    Game g{StartMode::Hatched};
    uint32_t t = 0;
    g.tick(t += kHeartbeatMs);
    while (g.achBanner()) {                       // drain anything the start state earned
        g.tick(t += kAchBannerMs);
        g.tick(t += kHeartbeatMs);
    }
    g.unlockAchievement(ach::kSurvivedLockout);
    CHECK(g.achPendingNotify() == 1);
    CHECK(g.achBanner() == nullptr);              // not until a tick raises it

    g.tick(t += kHeartbeatMs);
    const AchievementDef* shown = g.achBanner();
    CHECK(shown && std::strcmp(shown->id, ach::kSurvivedLockout) == 0);
    CHECK(g.achBannerCount() == 1);
    CHECK(g.achPendingNotify() == 1);             // still pending until it retires

    g.tick(t += kAchBannerMs);
    CHECK(g.achBanner() == nullptr);
    CHECK(g.achPendingNotify() == 0);             // shown, so now it counts as announced
}

// An unlock earned while the player is elsewhere waits for them: nothing is announced to
// an empty room, which is what makes "shown" a safe thing to persist.
static void test_achievement_banner_waits_for_the_home_screen() {
    Game g{StartMode::Hatched};
    uint32_t t = 0;
    g.tick(t += kHeartbeatMs);
    while (g.achBanner()) { g.tick(t += kAchBannerMs); g.tick(t += kHeartbeatMs); }
    g.onButton(press(Button::A));                 // summon the carousel — no longer idle
    CHECK(g.nav() != Game::Nav::Idle);
    g.unlockAchievement(ach::kFlawlessRun);
    g.tick(t += kHeartbeatMs);
    CHECK(g.achBanner() == nullptr);              // held back
    CHECK(g.achPendingNotify() == 1);
    g.onButton(press(Button::C));                 // back out to the habitat
    while (g.nav() != Game::Nav::Idle) g.tick(t += kHeartbeatMs);
    g.tick(t += kHeartbeatMs);
    CHECK(g.achBanner() != nullptr);              // ...and delivered on arrival
}

// A backlog too long to parade past one at a time collapses into a single summary, so a
// firmware update that retro-awards a whole catalogue doesn't lock up the home screen.
static void test_achievement_banner_collapses_a_burst() {
    Game g{StartMode::Hatched};
    uint32_t t = 0;
    g.tick(t += kHeartbeatMs);
    while (g.achBanner()) { g.tick(t += kAchBannerMs); g.tick(t += kHeartbeatMs); }
    int unlocked = 0;
    for (int i = 0; i < kAchievementCount && unlocked <= kAchBannerBurstMax; ++i) {
        const AchievementDef& d = kAchievements[i];
        if (g.hasAchievement(d)) continue;
        g.unlockAchievement(d.id);
        ++unlocked;
    }
    CHECK(g.achPendingNotify() > kAchBannerBurstMax);
    g.tick(t += kHeartbeatMs);
    CHECK(g.achBannerCount() > 1);                // one banner speaking for the lot
    g.tick(t += kAchBannerMs);
    CHECK(g.achPendingNotify() == 0);             // and it clears the whole backlog
}

// v40 round-trip + the pre-v40 migration, which is the part an upgraded device actually
// walks through: the legacy u32 mask becomes the first bytes of the bitset, nothing is
// marked announced (so the whole history parades), and the counters are seeded from what
// the rest of the save can honestly account for.
static void test_save_v40_achievements_roundtrip_and_migration() {
    {   // Raw round-trip of all four v40 fields.
        SaveData a; std::strcpy(a.activeId, "paypup"); a.generation = 1;
        a.achievementEarned.assign(8, 0);
        a.achievementEarned[0] = 0x05;            // wires 0 and 2
        a.achievementNotified.assign(8, 0);
        a.achievementNotified[0] = 0x01;          // wire 0 announced, wire 2 not
        a.bossWins = 37;
        a.collectedItems.push_back(SaveId{"osi_dip"});
        a.speciesDiveIds.push_back(SaveId{"malbear"});
        a.speciesDiveDepths.push_back(312);
        SaveData out;
        CHECK(deserializeSave(serializeSave(a), out));
        CHECK(out.achievementEarned.size() == 8 && out.achievementEarned[0] == 0x05);
        CHECK(out.achievementNotified.size() == 8 && out.achievementNotified[0] == 0x01);
        CHECK(out.bossWins == 37);
        CHECK(out.collectedItems.size() == 1);
        CHECK(std::strcmp(out.collectedItems[0].id, "osi_dip") == 0);
        CHECK(out.speciesDiveIds.size() == 1 && out.speciesDiveDepths.size() == 1);
        CHECK(out.speciesDiveDepths[0] == 312);
    }
    {   // A pre-v40 blob: the legacy mask migrates bit-for-bit, and the achievement it
        // encodes is re-announced because nothing was ever marked as shown.
        SaveData a; std::strcpy(a.activeId, "paypup"); a.generation = 1;
        a.achievementsMask = (1u << 1) | (1u << 6);   // SURVIVED_LOCKOUT + BIT_BARON
        a.subCleared.assign(kAreaCount, 0);
        a.subCleared[0] = 0x03;                        // two sub-areas cleared
        SaveStack held; std::strcpy(held.id, "airgap_snack"); held.qty = 2;
        a.items.push_back(held);                       // something already in the bag
        std::vector<uint8_t> blob = serializeSave(a);
        blob[4] = 39; blob[5] = 0;                     // stamp back to v39
        MemSaveStore store; store.save(blob);
        Game g(StartMode::Hatched, "paypup", &store);
        CHECK(g.hasAchievement("SURVIVED_LOCKOUT"));
        CHECK(g.hasAchievement("BIT_BARON"));
        CHECK(!g.hasAchievement("FLAWLESS_RUN"));
        CHECK(g.achPendingNotify() >= 2);              // the upgrade announces its history
        // Boss wins seed from the clears: two sub-areas beaten is two bosses beaten.
        CHECK(g.bossWins() == 2);
        // And what the bag already holds counts as collected, so the collection ladders
        // don't start an established device back at zero.
        CHECK(g.itemCollected("airgap_snack"));
    }
}

// The species dive record outlives the pet that set it, and is what the per-line and
// "different species" depth rows count off.
static void test_species_dive_records_feed_the_depth_rows() {
    Game g{StartMode::Hatched, "malbear"};
    CHECK(g.deepestDiveEver() == 0);
    g.debugRecordSpeciesDive("malbear", 300);
    g.debugRecordSpeciesDive("pingcub", 70);
    CHECK(g.speciesDeepestDive("malbear") == 300);
    CHECK(g.deepestDiveEver() == 300);
    g.debugRecordSpeciesDive("malbear", 120);          // a shallower run never demotes it
    CHECK(g.speciesDeepestDive("malbear") == 300);

    uint32_t t = 0;
    g.tick(t += kAchSweepIntervalMs);
    CHECK(g.hasAchievement("DEEPWEB_DEPTH_256"));
    CHECK(!g.hasAchievement("DEEPWEB_DEPTH_512"));
    // Both are ransomware-line species, so the line record follows the deeper of them.
    CHECK(g.hasAchievement("DEEP_LINE_RANSOMWARE"));
    // Two species past depth 64 is not yet three.
    CHECK(!g.hasAchievement("DEEP_BENCH_3"));
}

// The collection ladders count what has EVER been held, not what is in the bag now —
// spending an item must never un-earn an achievement.
static void test_collected_items_survive_being_spent() {
    Game g{StartMode::Hatched};
    uint32_t t = 0;
    g.tick(t += kHeartbeatMs);
    CHECK(g.itemCollected("airgap_snack"));            // on the starting shelf
    const int before = g.itemsCollected();
    while (g.inventory().count("airgap_snack") > 0)
        g.inventory().remove("airgap_snack", 1);
    g.tick(t += kAchSweepIntervalMs);
    CHECK(g.itemCollected("airgap_snack"));            // still collected
    CHECK(g.itemsCollected() == before);
}

// pets{} counts a creature the player still HOLDS in any of the three places a
// pet can sit — active, frozen in the ARCH rack, or an ARCH record. Those are all
// paths that install or record a pet, so all three land in the raised tally too.
static void test_pedia_state_json_rack_and_record_hatched() {
    SaveData a;
    std::strcpy(a.activeId, "paypup");
    a.generation = 3;
    SaveStoredPet stored; std::strcpy(stored.id, "pingcub");
    a.rack.push_back(stored);
    SaveRecord rec; std::strcpy(rec.id, "malbear");
    rec.status = static_cast<uint8_t>(RecordStatus::Corrupted);
    rec.generation = 2;
    a.records.push_back(rec);
    MemSaveStore store; store.save(serializeSave(a));
    Game g(StartMode::Hatched, "paypup", &store);
    CHECK(g.rack().size() == 1 && g.records().size() == 1);

    const std::string json = buildPediaStateJson(g);
    CHECK(json.find("\"paypup\":\"hatched\"") != std::string::npos);     // active
    CHECK(json.find("\"pingcub\":\"hatched\"") != std::string::npos);  // racked
    CHECK(json.find("\"malbear\":\"hatched\"") != std::string::npos);    // record
    CHECK(json.find("\"archive\":[{") != std::string::npos);
}

// End-to-end trigger #1: a real FreshHatch, waited out to completion, flips
// FIRST_BRUTE_FORCE (Game::completeHatch).
static void test_pedia_first_brute_force_achievement() {
    Game g(StartMode::FreshHatch);
    pickFirstEggLine(g);
    CHECK(!g.hasAchievement(ach::kFirstBruteForce));
    uint32_t t = 1000;
    g.tick(t);
    g.tick(t += kBootHatchMs + kHeartbeatMs);   // wait out incubation -> hatch to Process
    CHECK(g.pet() && g.pet()->stage != Stage::BootSector);
    CHECK(g.hasAchievement(ach::kFirstBruteForce));
}

// End-to-end trigger #2: a real Good-branch and a real Bad-branch Daemon
// evolution (Game::completeEvolution) — FLAWLESS_RUN/GONE_ROGUE fire on the correct
// branch only, and the sibling NOT taken stays LOCKED. Seeing is a fight; an
// evolution cinematic is not one, so the branch it reveals is not a reveal.
static void test_pedia_evolution_achievements_leave_the_sibling_locked() {
    { // Good branch (0 mistakes): FLAWLESS_RUN fires, GONE_ROGUE does not, and the
      // Bad sibling (berserkernel) is never marked.
        Game g{StartMode::Hatched, "malbear"};
        g.model().setCareMistakes(0);
        CHECK(g.model().careBranch() == CareBranch::Good);
        g.debugTriggerEvolution();
        uint32_t t = 0;
        advanceToReveal(g, t);
        g.onButton(press(Button::B));
        CHECK(g.pet() && std::strcmp(g.pet()->id, "bruinforce") == 0);
        CHECK(g.hasAchievement(ach::kFlawlessRun));
        CHECK(!g.hasAchievement(ach::kGoneRogue));
        CHECK(!g.creatureSeen("berserkernel"));
    }
    { // Bad branch (3+ mistakes): GONE_ROGUE fires, FLAWLESS_RUN does not, and the
      // Good sibling (bruinforce) is never marked.
        Game g{StartMode::Hatched, "malbear"};
        g.model().setCareMistakes(kCareGoodMax + 1);
        CHECK(g.model().careBranch() == CareBranch::Bad);
        g.debugTriggerEvolution();
        uint32_t t = 0;
        advanceToReveal(g, t);
        g.onButton(press(Button::B));
        CHECK(g.pet() && std::strcmp(g.pet()->id, "berserkernel") == 0);
        CHECK(g.hasAchievement(ach::kGoneRogue));
        CHECK(!g.hasAchievement(ach::kFlawlessRun));
        CHECK(!g.creatureSeen("bruinforce"));
    }
}

// --- New lines: cat (Ransomware) + frog (Phishing) -------------------------

// The CryptoShell (Ransomware) egg's random hatch pool holds Paypup, Conkittenate
// AND Pingcub — every Process creature on the ransomware line is a
// candidate, so the same egg can hatch any of the three species. None of them is
// deep-dive gated the way Phishlet is, so all three are in the pool from a fresh save.
static void test_hatch_pool_ransomware() {
    ContentRegistry r = ContentRegistry::embedded();
    Game g;                                     // fresh save, no achievements
    bool paypup = false, conk = false, pingcub = false;
    for (const CreatureDef* c : r.allCreatures())
        if (c->stage == Stage::Process && c->line &&
            std::strcmp(c->line, "ransomware") == 0) {
            CHECK(g.hatchProcessUnlocked(c));   // ungated: in the pool from the start
            if (std::strcmp(c->id, "paypup") == 0) paypup = true;
            else if (std::strcmp(c->id, "conkittenate") == 0) conk = true;
            else if (std::strcmp(c->id, "pingcub") == 0) pingcub = true;
        }
    CHECK(paypup && conk && pingcub);
}

// Canine chain (Ransomware): Paypup (Process) -> Barkmail (Script) -> a Good/Bad
// Daemon branch (Wire Heir | Extorgi), care-gated like Malbear's pair. Paypup routes
// Paypup has no Daemon pool, so every hop here is read straight off the creature row.
static void test_canine_line_evolution_branch() {
    { // Good care (0-2): Paypup -> Barkmail -> Wire Heir.
        Game g{StartMode::Hatched, "paypup"};
        uint32_t t = 0;
        g.debugTriggerEvolution(); advanceToReveal(g, t); g.onButton(press(Button::B));
        CHECK(g.pet() && std::strcmp(g.pet()->id, "barkmail") == 0);
        CHECK(g.pet()->stage == Stage::Script);
        CHECK(g.model().careBranch() == CareBranch::Good);
        g.debugTriggerEvolution(); advanceToReveal(g, t); g.onButton(press(Button::B));
        CHECK(g.pet() && std::strcmp(g.pet()->id, "wire_heir") == 0);
        CHECK(g.pet()->stage == Stage::Daemon);
    }
    { // Bad care (3-4): Barkmail -> Extorgi (the glass-cannon branch).
        Game g{StartMode::Hatched, "barkmail"};
        g.model().setCareMistakes(3);
        CHECK(g.model().careBranch() == CareBranch::Bad);
        uint32_t t = 0;
        g.debugTriggerEvolution(); advanceToReveal(g, t); g.onButton(press(Button::B));
        CHECK(g.pet() && std::strcmp(g.pet()->id, "extorgi") == 0);
        CHECK(g.pet()->stage == Stage::Daemon);
    }
}

// Pingcub is the ursine chain's own head: Malbear and its Daemon pair are reached
// only through it, now that Paypup heads the canine chain instead.
static void test_pingcub_rejoins_the_bear_line() {
    ContentRegistry r = ContentRegistry::embedded();
    const CreatureDef* pingcub = r.creature("pingcub");
    CHECK(pingcub);
    CHECK(pingcub->stage == Stage::Process);
    CHECK(r.creatureSprite(*pingcub) == &ASSET_SPR_PET_PINGCUB);   // its own cub art
    Game g{StartMode::Hatched, "pingcub"};
    uint32_t t = 0;
    g.debugTriggerEvolution(); advanceToReveal(g, t); g.onButton(press(Button::B));
    CHECK(g.pet() && std::strcmp(g.pet()->id, "malbear") == 0);
    CHECK(g.pet()->stage == Stage::Script);
}

// Cat line (Ransomware): Conkittenate (Process) -> Kalico (Script) -> a Good/Bad
// Daemon branch (Pwnther | Breecheetah), care-gated like Malbear's pair.
static void test_cat_line_evolution_branch() {
    { // Good care (0-2): Conkittenate -> Kalico -> Pwnther.
        Game g{StartMode::Hatched, "conkittenate"};
        uint32_t t = 0;
        g.debugTriggerEvolution(); advanceToReveal(g, t); g.onButton(press(Button::B));
        CHECK(g.pet() && std::strcmp(g.pet()->id, "kalico") == 0);
        CHECK(g.pet()->stage == Stage::Script);
        CHECK(g.model().careBranch() == CareBranch::Good);
        g.debugTriggerEvolution(); advanceToReveal(g, t); g.onButton(press(Button::B));
        CHECK(g.pet() && std::strcmp(g.pet()->id, "pwnther") == 0);
        CHECK(g.pet()->stage == Stage::Daemon);
    }
    { // Bad care (3-4): Kalico -> Breecheetah (the glass-cannon branch).
        Game g{StartMode::Hatched, "kalico"};
        g.model().setCareMistakes(3);
        CHECK(g.model().careBranch() == CareBranch::Bad);
        uint32_t t = 0;
        g.debugTriggerEvolution(); advanceToReveal(g, t); g.onButton(press(Button::B));
        CHECK(g.pet() && std::strcmp(g.pet()->id, "breecheetah") == 0);
        CHECK(g.pet()->stage == Stage::Daemon);
    }
}

// Frog line (Phishing) is LINEAR: Tadpoll -> Croaken -> Goliauth, and BOTH care
// branches land on Goliauth (no Good/Bad split).
static void test_frog_line_linear() {
    for (int mistakes : {0, 4}) {   // Good and Bad care both -> Goliauth
        Game g{StartMode::Hatched, "tadpoll"};
        g.model().setCareMistakes(mistakes);
        uint32_t t = 0;
        g.debugTriggerEvolution(); advanceToReveal(g, t); g.onButton(press(Button::B));
        CHECK(g.pet() && std::strcmp(g.pet()->id, "croaken") == 0);
        CHECK(g.pet()->stage == Stage::Script);
        g.debugTriggerEvolution(); advanceToReveal(g, t); g.onButton(press(Button::B));
        CHECK(g.pet() && std::strcmp(g.pet()->id, "goliauth") == 0);
        CHECK(g.pet()->stage == Stage::Daemon);
    }
}

// Anglerfish line (Phishing) is the DEEP-DIVE catch: Phishlet only joins the Phrogspawn
// egg's hatch pool once DEEPWEB_DEPTH_64 (the 2nd DeepWeb-depth milestone) is earned;
// Tadpoll is always in the pool. hatchProcessUnlocked is the granular gate the pool loop
// consults, so assert it directly (an rng-driven full hatch would be flaky on the 50/50).
static void test_anglerfish_deepdive_hatch_gate() {
    ContentRegistry r = ContentRegistry::embedded();
    const CreatureDef* phishlet = r.creature("phishlet");
    const CreatureDef* tadpoll  = r.creature("tadpoll");
    CHECK(phishlet && tadpoll);
    CHECK(phishlet->stage == Stage::Process);
    CHECK(std::strcmp(phishlet->line, "phishing") == 0);
    CHECK(r.creatureSprite(*phishlet) == &ASSET_SPR_PET_PHISHLET);  // the anglerfish art
    Game g;                                                 // no achievements yet
    CHECK(!g.hasAchievement(ach::kDeepWebDepth64));
    CHECK(!g.hatchProcessUnlocked(phishlet));               // gated out of the pool
    CHECK(g.hatchProcessUnlocked(tadpoll));                 // always in the pool
    g.unlockAchievement(ach::kDeepWebDepth64);
    CHECK(g.hatchProcessUnlocked(phishlet));                // now a 50/50 pool member
}

// Anglerfish line (Phishing): Phishlet (Process) -> ClickBait (Script) -> a Good/Bad
// Daemon branch (Spamwhale | Baitracuda), care-gated like Kalico's pair.
static void test_anglerfish_line_evolution_branch() {
    { // Good care (0-2): Phishlet -> ClickBait -> Spamwhale.
        Game g{StartMode::Hatched, "phishlet"};
        g.debugSeedRng(1);   // no-divert seed: Phishlet keeps the Phishing chain
        uint32_t t = 0;
        g.debugTriggerEvolution(); advanceToReveal(g, t); g.onButton(press(Button::B));
        CHECK(g.pet() && std::strcmp(g.pet()->id, "clickbait") == 0);
        CHECK(g.pet()->stage == Stage::Script);
        CHECK(g.model().careBranch() == CareBranch::Good);
        g.debugTriggerEvolution(); advanceToReveal(g, t); g.onButton(press(Button::B));
        CHECK(g.pet() && std::strcmp(g.pet()->id, "spamwhale") == 0);
        CHECK(g.pet()->stage == Stage::Daemon);
    }
    { // Bad care (3-4): ClickBait -> Baitracuda (the glass-cannon branch).
        Game g{StartMode::Hatched, "clickbait"};
        g.model().setCareMistakes(3);
        CHECK(g.model().careBranch() == CareBranch::Bad);
        uint32_t t = 0;
        g.debugTriggerEvolution(); advanceToReveal(g, t); g.onButton(press(Button::B));
        CHECK(g.pet() && std::strcmp(g.pet()->id, "baitracuda") == 0);
        CHECK(g.pet()->stage == Stage::Daemon);
    }
}

// Trojan family — content wiring: pierce attacks (armorPiercePct 100), trap defends,
// the two creatures on line "trojan", and Phishlet's cross-line divert hook.
static void test_trojan_content() {
    ContentRegistry r = ContentRegistry::embedded();
    for (const char* id : {"backdoor_breach", "payload_puncture", "rootkit_rupture"}) {
        const MoveDef* m = r.move(id);
        CHECK(m && m->kind == MoveDef::Kind::Attack &&
              std::strcmp(m->line, "trojan") == 0 && m->armorPiercePct == 100);
    }
    for (const char* id : {"logic_bomb", "sandbox_snare", "killswitch"}) {
        const MoveDef* m = r.move(id);
        CHECK(m && m->kind == MoveDef::Kind::Defend && m->trapArm > 0 &&
              m->trapEvasionPct > 0 && m->trapReboundPct > 0 && m->trapArmorRot > 0 &&
              m->trapPassiveBonusPct > 0);
    }
    const CreatureDef* kl = r.creature("keyloggerhead");
    const CreatureDef* dae = r.creature("placeholder_daemon");
    CHECK(kl && kl->stage == Stage::Script && std::strcmp(kl->line, "trojan") == 0);
    CHECK(dae && dae->stage == Stage::Daemon && std::strcmp(dae->line, "trojan") == 0);
    const CreatureDef* ph = r.creature("phishlet");
    CHECK(ph && ph->evolvesToTrojanId &&
          std::strcmp(ph->evolvesToTrojanId, "keyloggerhead") == 0);
}

// The cross-line infiltration divert (the headline mechanic): a Phishlet that rolls the
// divert becomes Keyloggerhead (a Trojan) instead of ClickBait — dropping its Phishing
// moves for the Trojan kit and unlocking the family. Seeded so the roll is deterministic.
static void test_trojan_cross_line_divert() {
    {   // divert seed: Phishlet -> Keyloggerhead, loadout re-seeded, family unlocked
        Game g{StartMode::Hatched, "phishlet"};
        CHECK(g.moveLoadout().owns("smish_hook"));            // starts with Phishing moves
        CHECK(!g.hasAchievement(ach::kTrojanUnleashed));
        g.debugSeedRng(13);                                  // rolls < kTrojanDivertPct
        uint32_t t = 0;
        g.debugTriggerEvolution(); advanceToReveal(g, t); g.onButton(press(Button::B));
        CHECK(g.pet() && std::strcmp(g.pet()->id, "keyloggerhead") == 0);
        CHECK(std::strcmp(g.pet()->line, "trojan") == 0);
        CHECK(g.moveLoadout().owns("backdoor_breach"));      // gained the Trojan kit
        CHECK(!g.moveLoadout().owns("smish_hook"));          // dropped the Phishing kit
        CHECK(g.hasAchievement(ach::kTrojanUnleashed));
    }
    {   // no-divert seed: the normal Phishing chain is unaffected
        Game g{StartMode::Hatched, "phishlet"};
        g.debugSeedRng(1);                                   // rolls >= kTrojanDivertPct
        uint32_t t = 0;
        g.debugTriggerEvolution(); advanceToReveal(g, t); g.onButton(press(Button::B));
        CHECK(g.pet() && std::strcmp(g.pet()->id, "clickbait") == 0);
        CHECK(!g.hasAchievement(ach::kTrojanUnleashed));
    }
}

// Trojan combat: (1) a pierce attack ignores a defended enemy's damage cut; (2) an armed
// trap springs on an incoming hit — evasion cuts the damage taken, rebound chips the
// attacker, and the trap is consumed; (3) Execution-Override chance gates off-line and
// scales with armed traps.
static void test_trojan_combat() {
    ContentRegistry r = ContentRegistry::embedded();

    // (1) Pierce: backdoor_breach (power 12, pierce 100) vs a 50%-cut enemy lands full 12;
    //     packet_storm (power 12, no pierce) lands 6. Player faster -> acts first.
    {
        Combatant e = mkCombatant(r, "E", 100, 1, {"quick_jab"});
        e.dmgReducePct = 50;
        Combatant pPierce = mkCombatant(r, "P", 100, 20, {"backdoor_breach"});
        Combatant pPlain  = mkCombatant(r, "P", 100, 20, {"packet_storm"});
        Combat a; a.begin(pPierce, e, Combat::Stakes::Safe, 42); a.step();
        Combat b; b.begin(pPlain,  e, Combat::Stakes::Safe, 42); b.step();
        CHECK(a.enemy().health == 100 - 12);      // pierce ignored the 50% cut
        CHECK(b.enemy().health == 100 - 6);        // plain hit was halved
    }

    // (2) Trap trigger. Player NOT on line "trojan" (nullptr) so the Execution-Override
    //     passive can't interfere — the trap mechanic itself is not line-gated. Equal
    //     speed -> strict alternation, player first: step1 arms logic_bomb, step2 the
    //     enemy's packet_storm springs it.
    {
        Combatant p = mkCombatant(r, "P", 100, 10, {"logic_bomb"});
        Combatant e = mkCombatant(r, "E", 100, 10, {"packet_storm"});  // power 12
        Combat cb; cb.begin(p, e, Combat::Stakes::Safe, 42);
        cb.step();                                 // player arms the trap
        CHECK(cb.player().trojanTrapCount == 1);
        cb.step();                                 // enemy hits -> trap springs
        CHECK(cb.player().trojanTrapCount == 0);   // consumed
        CHECK(cb.player().health == 100 - 9);      // 12 - 20% evasion = 9 taken
        CHECK(cb.enemy().health < 100);            // rebound chipped the attacker
    }

    // (3) Execution-Override chance: 0 off-line; base on-line with no traps; base + the
    //     armed trap bonuses with traps held.
    {
        Combatant nonTrojan = mkCombatant(r, "P", 100, 10, {"quick_jab"});
        Combatant trojan = mkCombatant(r, "P", 100, 10, {"quick_jab"});
        trojan.line = "trojan";
        Combat cb; cb.begin(nonTrojan, nonTrojan, Combat::Stakes::Safe, 1);
        CHECK(cb.execOverrideChance(nonTrojan) == 0);
        CHECK(cb.execOverrideChance(trojan) == kExecOverrideBasePct);
        trojan.trojanTraps[trojan.trojanTrapCount++] = r.move("logic_bomb");   // +10
        trojan.trojanTraps[trojan.trojanTrapCount++] = r.move("killswitch");   // +20
        CHECK(cb.execOverrideChance(trojan) ==
              kExecOverrideBasePct + r.move("logic_bomb")->trapPassiveBonusPct +
                  r.move("killswitch")->trapPassiveBonusPct);
    }
}

// --- Clutch Pick (the Phishing hatch minigame, game_eggpick.cpp) -----------

// Lay a Phishing egg, which drops straight into the Clutch Pick.
static Game phishingEgg() {
    Game g;
    g.unlockAchievement(ach::kDeepWebDepth8);   // unlocks the Phishing line
    g.resetToHatch();
    g.onButton(press(Button::A));               // cycle to Phishing
    g.onButton(press(Button::B));               // lay it
    return g;
}

// Aim at whichever half actually holds the live egg, every round. Mirrors the cut
// game_eggpick.cpp documents — alternating columns/rows over the 8x4 clutch — so it
// also pins that order: a change to the split axis or count fails here.
static void eggPickPlayPerfect(Game& g) {
    const int col = g.eggPickTargetSlot() % Game::kEggPickCols;
    const int row = g.eggPickTargetSlot() / Game::kEggPickCols;
    int c0 = 0, cw = Game::kEggPickCols, r0 = 0, rh = Game::kEggPickRows;
    for (int i = 0; i < Game::kEggPickRounds; ++i) {
        bool second;
        if (i % 2 == 0) { cw /= 2; second = col >= c0 + cw; if (second) c0 += cw; }
        else            { rh /= 2; second = row >= r0 + rh; if (second) r0 += rh; }
        g.onButton(press(second ? Button::C : Button::A));   // aim
        g.onButton(press(Button::B));                        // commit the half
    }
}

// A clean run halves what's left of the incubation clock; the reveal waits for B.
static void test_eggpick_win_halves_incubation() {
    Game g = phishingEgg();
    CHECK(g.inEggPick());
    CHECK(g.eggPickRound() == 0 && !g.eggPickResolved());
    CHECK(g.bootHatchRemainMs() == kBootHatchMs);
    eggPickPlayPerfect(g);
    CHECK(g.eggPickRound() == Game::kEggPickRounds);
    CHECK(g.eggPickResolved() && g.eggPickWon());
    CHECK(g.bootHatchRemainMs() == kBootHatchMs);   // banked on the reveal's B, not before
    g.onButton(press(Button::B));
    CHECK(g.nav() == Game::Nav::Idle);
    CHECK(g.bootHatchRemainMs() == kBootHatchMs / 2);
    CHECK(g.inEggPhase());                          // still an egg, just a faster one
}

// Picking the wrong half loses the egg for good — the remaining rounds still play out
// (the player isn't told mid-run), and the incubation clock is left at full length.
static void test_eggpick_miss_keeps_full_incubation() {
    Game g = phishingEgg();
    const int col = g.eggPickTargetSlot() % Game::kEggPickCols;
    const bool correct = col >= Game::kEggPickCols / 2;
    g.onButton(press(correct ? Button::A : Button::C));   // aim at the half it ISN'T in
    g.onButton(press(Button::B));
    CHECK(!g.eggPickTargetInSpan());                      // already lost
    CHECK(!g.eggPickResolved());                          // but the run keeps going
    for (int i = 1; i < Game::kEggPickRounds; ++i) {
        g.onButton(press(Button::A));
        g.onButton(press(Button::B));
    }
    CHECK(g.eggPickResolved() && !g.eggPickWon());
    g.onButton(press(Button::B));
    CHECK(g.nav() == Game::Nav::Idle);
    CHECK(g.bootHatchRemainMs() == kBootHatchMs);         // no bonus, no penalty
}

// A and C AIM here rather than step/cancel: C can't back out of the modal, and
// re-aiming before B is free.
static void test_eggpick_aim_is_not_cancel() {
    Game g = phishingEgg();
    g.onButton(press(Button::C));
    CHECK(g.inEggPick() && g.eggPickRound() == 0);   // C aimed, it didn't exit or commit
    g.onButton(press(Button::A));
    CHECK(g.inEggPick() && g.eggPickRound() == 0);
    g.onButton(press(Button::B));
    CHECK(g.eggPickRound() == 1);                    // only B advances a round
}

// The live egg never hides in a wrapped cell — the odd-row shift puts the last cell of
// those rows half off each edge of the clutch, which has no honest left/right answer.
static void test_eggpick_target_never_wraps() {
    for (uint32_t seed = 1; seed <= 64; ++seed) {
        Game g;
        g.unlockAchievement(ach::kDeepWebDepth8);
        g.debugSeedRng(seed);
        g.resetToHatch();
        g.onButton(press(Button::A));
        g.onButton(press(Button::B));
        const int slot = g.eggPickTargetSlot();
        const int col = slot % Game::kEggPickCols, row = slot / Game::kEggPickCols;
        CHECK(!(row % 2 == 1 && col == Game::kEggPickCols - 1));
    }
}

// Take a laid Phishing egg through its Clutch Pick and back out to idle.
static void settleEggPick(Game& g) {
    for (int i = 0; i < Game::kEggPickRounds; ++i) {
        g.onButton(press(Button::A));
        g.onButton(press(Button::B));
    }
    g.onButton(press(Button::B));                 // dismiss the reveal -> idle
}

// A Phishing egg never opens the Decrypt line's minigame — its hatch game was the
// Clutch Pick, already played at lay-time. hatchMinigameReady() carries the line half of
// that question alongside the clock half, so every caller (idle's B and A+C, the
// Decryptogram, and the idle prompt that advertises it) agrees.
static void test_eggpick_line_never_opens_decrypt() {
    Game g = phishingEgg();
    settleEggPick(g);
    uint32_t t = 0;
    g.tick(t += 1000);
    g.tick(t += kBootHatchMs / 2);                // the Decrypt line's arming window
    CHECK(!g.hatchMinigameReady());               // clock says yes, but this line has no decrypt
    CHECK(!g.eggCrackable());                     // ...so nothing is advertised yet either
    g.onButton(press(Button::B));
    CHECK(g.nav() == Game::Nav::Idle);
    CHECK(g.inEggPhase());                        // still incubating
}

// The home stretch of ANY incubation is crackable on demand: in the last kHatchRevealMs
// the Exploit chord plays the shell's full hatch one-shot and hatches off the end, so a
// line with no decrypt still gets to SHOW its animation. Non-interactive throughout.
static void test_hatch_reveal_plays_the_animation() {
    Game g = phishingEgg();
    settleEggPick(g);
    uint32_t t = 0;
    g.tick(t += 1000);
    g.tick(t += kBootHatchMs / 2);
    CHECK(!g.eggCrackable());                     // still short of the reveal window
    g.onButton(chordAC());
    CHECK(g.nav() == Game::Nav::Idle);            // the chord is inert out here
    // Run the clock into the last kHatchRevealMs, stopping short of the auto-hatch.
    g.tick(t += g.bootHatchRemainMs() - kHatchRevealMs / 2);
    CHECK(g.hatchRevealReady() && g.eggCrackable());
    g.onButton(chordAC());
    CHECK(g.nav() == Game::Nav::ModalHatchReveal);
    CHECK(g.hatchRevealFrame() == 0);             // opens on the shell's first frame

    // A frame per heartbeat, and every button inert while it runs.
    g.tick(t += kHeartbeatMs);
    CHECK(g.hatchRevealFrame() == 1);
    g.onButton(press(Button::B));
    g.onButton(press(Button::C));
    CHECK(g.nav() == Game::Nav::ModalHatchReveal);   // nothing skips it
    const int frames = 8;                            // SPR_PET_EGG_PHISH_HATCH
    for (int i = 0; i < frames + kHatchRevealHoldBeats; ++i) g.tick(t += kHeartbeatMs);
    CHECK(g.hatchRevealFrame() == frames - 1);       // held the last frame, didn't run off
    CHECK(g.nav() == Game::Nav::Idle);               // ...then hatched itself
    CHECK(g.pet() && g.pet()->stage == Stage::Process);
    CHECK(!g.inEggPhase());
}

// A Ransomware egg is untouched by the reveal: its own decrypt minigame opens earlier
// (and already plays its crack), so the two never compete for the chord.
static void test_hatch_reveal_leaves_decrypt_line_alone() {
    Game g;
    pickFirstEggLine(g);
    uint32_t t = 0;
    g.tick(t += 1000);
    g.tick(t += kBootHatchMs - kHatchRevealMs / 2);   // deep in the reveal window
    CHECK(!g.hatchRevealReady());                     // ...but this line doesn't use it
    CHECK(g.hatchMinigameReady() && g.eggCrackable());
    g.onButton(chordAC());
    CHECK(g.nav() == Game::Nav::ModalHatch);          // still the decrypt
}

// Grayscale gate. The aim rides on SHAPE first — a bar down the aimed half's outer
// edge, which moves to the other edge when the aim flips — backed by a luminance step
// between the aimed, still-in-play, and eliminated cells. None of it is carried by hue.
static void test_eggpick_grayscale() {
    Framebuffer fb(kActiveW, kActiveH);
    auto meanLum = [&](int x0, int x1, int y0, int y1) {
        float sum = 0.0f;
        for (int y = y0; y < y1; ++y)
            for (int x = x0; x < x1; ++x) sum += luminance(fb.get(x, y));
        return sum / static_cast<float>((x1 - x0) * (y1 - y0));
    };
    // The panel is centred and drawn at x2: 224x112 spanning y 56..168.
    const int top = 62, bot = 160;

    {   // Round 1 cuts columns and opens aimed LEFT.
        Game g = phishingEgg();
        g.render(fb);
        CHECK(hasDarkInk(fb, 0, 0, kActiveW, 40));          // title + round counter
        CHECK(meanLum(0, 4, top, bot) > meanLum(kActiveW - 4, kActiveW, top, bot));
        CHECK(meanLum(8, 104, top, bot) > meanLum(120, 216, top, bot));
    }
    {   // C flips the aim: the bar and the bright half both move to the right.
        Game g = phishingEgg();
        g.onButton(press(Button::C));
        g.render(fb);
        CHECK(meanLum(kActiveW - 4, kActiveW, top, bot) > meanLum(0, 4, top, bot));
        CHECK(meanLum(120, 216, top, bot) > meanLum(8, 104, top, bot));
    }
    {   // After committing left, round 2 cuts rows: aimed top > in-play bottom-left >
        // the eliminated right, three ordered steps with no colour involved.
        Game g = phishingEgg();
        g.onButton(press(Button::B));
        g.render(fb);
        const float aimed = meanLum(8, 104, top, 110);
        const float inPlay = meanLum(8, 104, 118, bot);
        const float dead = meanLum(120, 216, top, bot);
        CHECK(aimed > inPlay && inPlay > dead);
    }
}

// The hatch line-select: the Phishing line is now gated behind the first
// DeepWeb-depth milestone (DEEPWEB_DEPTH_8), so a fresh save offers only Ransomware
// and line-select auto-skips. Once depth 8 is earned, a fresh hatch presents
// the choice; A cycles, B lays the chosen line's egg. Choosing Phishing lays
// Phrogspawn -> hatches to Tadpoll (Phishing).
static void test_line_select_phishing_egg() {
    Game g;                                     // FreshHatch: only Ransomware unlocked
    CHECK(!g.inLineSelect());                   // one line -> auto-skip, egg laid
    CHECK(g.pet() && std::strcmp(g.pet()->line, "ransomware") == 0);
    g.unlockAchievement(ach::kDeepWebDepth8);   // unlocks Phishing
    g.resetToHatch();                           // re-hatch: now two lines -> the modal
    CHECK(g.inLineSelect());
    CHECK(g.pet() == nullptr);                  // empty save while choosing
    CHECK(g.lineSelectRow() == 0);              // Ransomware first
    g.onButton(press(Button::C));               // C disabled -> still choosing
    CHECK(g.inLineSelect());
    g.onButton(press(Button::A));               // cycle to Phishing (row 1)
    CHECK(g.lineSelectRow() == 1);
    g.onButton(press(Button::B));               // select -> lay the Phrogspawn egg
    CHECK(g.pet() && std::strcmp(g.pet()->id, "phrogspawn") == 0);
    CHECK(g.inEggPhase() && std::strcmp(g.pet()->line, "phishing") == 0);
    CHECK(g.inEggPick());                       // the Phishing line opens its Clutch Pick
    eggPickPlayPerfect(g);
    g.onButton(press(Button::B));               // dismiss the reveal -> the egg at idle
    CHECK(g.nav() == Game::Nav::Idle);
    uint32_t t = 0; g.tick(t += 1000); g.tick(t += kBootHatchMs + kHeartbeatMs);
    CHECK(g.pet() && g.pet()->stage == Stage::Process);       // hatched
    CHECK(std::strcmp(g.pet()->id, "tadpoll") == 0);          // only unlocked phishing Process (Phishlet gates on depth-64)
}

// Line grayscale gate: the line-select modal reads without colour (title ink +
// the ">" cursor glyph marking the highlighted line).
static void test_line_select_grayscale() {
    Game g;
    g.unlockAchievement(ach::kDeepWebDepth8);   // unlock Phishing -> a real choice
    g.resetToHatch();                                          // re-hatch into the modal
    CHECK(g.inLineSelect());
    Framebuffer fb(kActiveW, kActiveH);
    g.render(fb);
    CHECK(hasDarkInk(fb, 0, 40, kActiveW, 120));   // title / rows carry ink
}

// Frog line-move access (moveAllowedForLine): a Phishing pet can learn a phishing
// move (spoof_bubble); a Ransomware pet cannot; a generic move is open to both.
static void test_frog_line_move_access() {
    ContentRegistry r = ContentRegistry::embedded();
    const CreatureDef* frog = r.creature("croaken");      // phishing Script
    const CreatureDef* cat = r.creature("kalico");        // ransomware Script
    const MoveDef* phish = r.move("spoof_bubble");        // phishing line move
    const MoveDef* generic = r.move("packet_storm");      // generic move
    CHECK(frog && cat && phish && generic);
    CHECK(moveAllowedForLine(*phish, frog->line));        // frog: yes
    CHECK(!moveAllowedForLine(*phish, cat->line));        // ransomware: no
    CHECK(moveAllowedForLine(*generic, frog->line));      // generic: anyone
    CHECK(moveAllowedForLine(*generic, cat->line));
}

// An update is offered on a strict > against the packed version code, so the
// packing has to keep MAJOR.MINOR.PATCH ordered across every rollover, and a
// code that failed to parse must never look newer than what's running.
static void test_firmware_version_ordering() {
    CHECK(!firmwareUpdateAvailable(kFirmwareVersionCode));      // the same build
    CHECK(!firmwareUpdateAvailable(kFirmwareVersionCode - 1));  // an older one
    CHECK(firmwareUpdateAvailable(kFirmwareVersionCode + 1));   // a newer one

    // A missing/unparseable manifest version lands here as 0. Offering an
    // update on that would flash an arbitrary image over a working device.
    CHECK(!firmwareUpdateAvailable(0));

    // Fields don't overlap: a maxed patch never outranks a minor bump, and a
    // maxed minor never outranks a major one.
    constexpr uint32_t kV0_1_0  = 0 * 10000u + 1 * 100u + 0;
    constexpr uint32_t kV0_1_99 = 0 * 10000u + 1 * 100u + 99;
    constexpr uint32_t kV0_99_0 = 0 * 10000u + 99 * 100u + 0;
    constexpr uint32_t kV1_0_0  = 1 * 10000u + 0 * 100u + 0;
    CHECK(kV0_1_0 < kV0_1_99 && kV0_1_99 < kV0_99_0 && kV0_99_0 < kV1_0_0);
}

// The gate roster — listed once, expanded by whichever harness compiles the
// file. RUN(fn) is supplied per harness (a plain call for ctest, RUN_TEST for
// Unity). Keep this in milestone order; add new gates here, not in main().
#define MAL_RUN_ALL_GATES(RUN)              \
    /* T0–T2 — pipeline + idle + STAT */    \
    RUN(test_sprite_roundtrip)              \
    RUN(test_upscale_boundaries)            \
    RUN(test_palette_luminance_ordered)     \
    RUN(test_idle_habitat)                  \
    RUN(test_hunger_alert_gated)            \
    RUN(test_idle_breathe_animates)         \
    RUN(test_sd_icon_reveal_window)         \
    RUN(test_capture_badge_phases)          \
    RUN(test_idle_status_icons_grayscale)   \
    RUN(test_sd_recheck_request_seam)       \
    RUN(test_pet_model_zones)               \
    RUN(test_care_branch_and_clamp)         \
    RUN(test_hunger_decay)                  \
    RUN(test_content_registry)              \
    RUN(test_grayscale_gate)                \
    RUN(test_stat_paging_loadout_xp)        \
    RUN(test_loadout_rows_model)            \
    RUN(test_effect_text_templates_resolve) \
    RUN(test_effect_text_fits_its_screen_budget) \
    RUN(test_stat_loadout_b_scroll)         \
    /* Carousel */                          \
    RUN(test_carousel_summon)               \
    RUN(test_carousel_bookwrap)             \
    RUN(test_carousel_focus_grayscale)      \
    RUN(test_carousel_enter_back)           \
    RUN(test_caption_pinned_top)            \
    RUN(test_carousel_autodefocus)          \
    RUN(test_carousel_ui_modes)             \
    /* The raising loop */                  \
    RUN(test_inventory)                     \
    RUN(test_event_log)                     \
    RUN(test_inventory_rows_grouped)        \
    RUN(test_inventory_rows_rarity_desc)    \
    RUN(test_inventory_scrollbar)           \
    RUN(test_feed_and_maint_model)          \
    RUN(test_feeding_flow)                  \
    RUN(test_item_context_gate)             \
    RUN(test_maint_flow)                    \
    RUN(test_defrag_costs_stage_bits)       \
    RUN(test_defrag_gated_when_broke)       \
    RUN(test_defrag_variants)               \
    RUN(test_defrag_tool_gated_in_items)    \
    RUN(test_defrag_count_freeze_thaw)      \
    RUN(test_arch_release_stored_frees_slot) \
    RUN(test_restore_point_shield_blocks_next_mistake) \
    RUN(test_restore_point_once_per_lifetime) \
    RUN(test_restore_shield_covers_maint_fail) \
    RUN(test_yubi_cookie_once_per_lifetime) \
    RUN(test_lockout_expire_mistakes)       \
    RUN(test_backup_drive_arms_and_lapses)  \
    RUN(test_inert_use_keeps_the_item)      \
    RUN(test_recipe_rows_wait_on_the_merge_hub) \
    RUN(test_browns_recipes_wait_on_meeting_both_dishes) \
    RUN(test_four_ingredient_recipe_consumes_all_inputs) \
    RUN(test_stacking_food_run_climbs_then_resets_on_decay) \
    RUN(test_loot_pools_resolve_and_carry_weight) \
    RUN(test_lockout_resolve_pay)           \
    RUN(test_lockout_resolve_feed)          \
    RUN(test_items_grayscale)               \
    /* Decryption Hatch */                  \
    RUN(test_hatch_lays_egg_at_idle)        \
    RUN(test_hatch_minigame_second_half_gate) \
    RUN(test_hatch_press_subtracts_minute)  \
    RUN(test_hatch_decrypt_to_process)      \
    RUN(test_hatch_crack_frame_mapping)     \
    RUN(test_hatch_waits_out)               \
    RUN(test_hatch_network_accelerates)     \
    RUN(test_hatch_egg_vitals_frozen)       \
    RUN(test_hatch_grayscale)               \
    RUN(test_hatch_seam_skips)              \
    RUN(test_egg_menu_locks)                \
    RUN(test_egg_items_quest_only)          \
    RUN(test_egg_exploit_chord_hatches)     \
    RUN(test_egg_cannot_explore)            \
    RUN(test_evolve_remaining_readout)      \
    /* Task 1 — dev reset shortcut */       \
    RUN(test_reset_to_hatch)                \
    /* Evolution boundary stub */           \
    RUN(test_evolution_boot_to_process)     \
    RUN(test_evolution_c_disabled_b_gated)  \
    RUN(test_evolution_triggers_and_gates)  \
    RUN(test_evolution_script_dwell_longer_than_process)  \
    RUN(test_evolution_full_chain)          \
    RUN(test_evolution_grayscale)           \
    /* Hardening */                         \
    RUN(test_remaining_screens_grayscale)   \
    RUN(test_event_driven_repaint)          \
    RUN(test_sprite_grayscale_legibility)   \
    /* CFG submenu */                       \
    RUN(test_cfg_uimode_toggle)             \
    RUN(test_cfg_travel_confirm_asks_twice) \
    RUN(test_travel_sleep_credits_nothing_to_the_gap) \
    RUN(test_cfg_group_back_resumes_row)    \
    RUN(test_cfg_radio_reports_owner)       \
    RUN(test_cfg_radio_rows_follow_arbiter_priority) \
    RUN(test_cfg_sysinfo_sd_recheck)        \
    RUN(test_cfg_list_scroll_offset)        \
    RUN(test_cfg_brightness_apply)          \
    RUN(test_cfg_dev_reset_row)             \
    RUN(test_cfg_factory_reset_hold)        \
    RUN(test_cfg_factory_reset_scopes_differ) \
    RUN(test_cfg_screens_grayscale)         \
    /* ARCH submenu */                      \
    RUN(test_arch_list_and_record)          \
    /* MODS submenu */                      \
    RUN(test_loadout_permanent_mods)        \
    RUN(test_move_loadout)                  \
    /* Combat engine */                     \
    RUN(test_combat_deterministic)          \
    RUN(test_combat_no_consecutive)         \
    RUN(test_combat_override_breaks_rule)   \
    RUN(test_combat_override_item_use)      \
    RUN(test_exploit_uses_per_battle)       \
    RUN(test_combat_item_in_game)           \
    RUN(test_combat_channel)                \
    RUN(test_combat_mod_passives)           \
    RUN(test_combat_builders_and_flee)      \
    RUN(test_mods_equip_flow)               \
    RUN(test_mods_overwrite_confirm)        \
    RUN(test_mod_detail_oneshot)            \
    RUN(test_loadout_one_slot_per_mod)      \
    /* TRAIN / EXPL shells */               \
    RUN(test_train_expl_shells)             \
    /* Persistence */                       \
    RUN(test_save_roundtrip)                \
    RUN(test_save_version_and_empty)        \
    RUN(test_save_v1_migration)             \
    RUN(test_save_v7_to_v8_migration)       \
    RUN(test_save_v8_to_v9_migration)       \
    RUN(test_save_v9_to_v10_migration)      \
    RUN(test_save_v16_to_v17_mod_migration) \
    RUN(test_save_v12_to_v13_migration)     \
    RUN(test_save_v13_to_v14_brightness_default) \
    RUN(test_save_v14_to_v15_refarm_default) \
    RUN(test_save_v15_to_v16_defrag_default) \
    RUN(test_boot_from_save_vs_hatch)       \
    RUN(test_reboot_preserves_active_creature_level) \
    RUN(test_persistence_reset_clears_store)\
    RUN(test_hackertag_editor)              \
    RUN(test_arch_store_and_deploy)         \
    RUN(test_stat_footer_and_generation)    \
    RUN(test_arch_rack_grayscale)           \
    /* Single-frame creatures */            \
    RUN(test_idle_frame_single_frame_safe)  \
    RUN(test_idle_wander_stays_inside_the_living_box) \
    RUN(test_idle_wander_reads_differently_per_locomotion) \
    RUN(test_idle_wander_rehomes_when_the_mover_changes) \
    RUN(test_habitat_moves_a_pet_and_parks_an_egg) \
    /* Sim-Battle + combat integration */   \
    RUN(test_sim_battle_end_to_end)         \
    RUN(test_backup_drive_save_not_spent_in_sim_battle) \
    RUN(test_backup_drive_save_armed_into_wild_combat) \
    RUN(test_rig_auto_backup_arms_save_on_explore) \
    RUN(test_rig_continuous_backup_rearms_mid_run) \
    RUN(test_creature_level_curve_and_invariant) \
    RUN(test_creature_level_feeds_combat)   \
    RUN(test_rollback_item)                 \
    RUN(test_creature_level_persist_evolution_reset_egg) \
    RUN(test_arch_store_deploy_preserves_creature_level) \
    RUN(test_arch_deploy_loadout_is_per_pet) \
    RUN(test_combat_auto_pacing)            \
    RUN(test_combat_length_in_band)         \
    RUN(test_wild_encounter_challenge_buff) \
    RUN(test_explore_subarea_ramp)          \
    RUN(test_line_move_gating)              \
    RUN(test_ransomware_stacking)           \
    RUN(test_ransom_note)                   \
    RUN(test_ransom_note_shows_up_in_pve)   \
    RUN(test_wild_subarea_level_and_xp_scaling) \
    RUN(test_move_evolution_gating)         \
    /* Move-slot rework #11/#12 */          \
    RUN(test_move_pool_per_slot_fallback)   \
    RUN(test_move_slot_type_lock)           \
    RUN(test_move_slot_stamping_locks_at_unlock) \
    RUN(test_save_v24_slot_kinds_roundtrip_and_migration) \
    RUN(test_combat_override_in_game)       \
    RUN(test_combat_stakes_live_vs_safe)    \
    RUN(test_combat_screen_grayscale)       \
    RUN(test_move_loadout_persist)          \
    /* Evolution branching + Critical System Failure */ \
    RUN(test_evolution_branch_selection)    \
    RUN(test_evolution_branch_mechanics)    \
    RUN(test_combat_loss_never_kills)       \
    RUN(test_csf_fires_and_archives)        \
    RUN(test_csf_recovery_disarms)          \
    RUN(test_csf_record_persists)           \
    RUN(test_csf_window_survives_reboot)    \
    RUN(test_csf_recovery_clears_burned_window) \
    RUN(test_save_records_roundtrip)        \
    RUN(test_save_hacker_rank_roundtrip)    \
    RUN(test_save_v3_migration_defaults_hacker_rank) \
    RUN(test_save_v4_migration_defaults_audit_off)   \
    RUN(test_audit_scan_toggle_persists)    \
    RUN(test_csf_grayscale)                 \
    RUN(test_arch_record_readonly)          \
    /* EXPL explore-mode wiring */          \
    RUN(test_explore_arm_returns_to_idle)         \
    RUN(test_walk_event_roll_reaches_encounter)   \
    RUN(test_explore_autosteps_hands_free)        \
    RUN(test_explore_every_step_is_an_event)      \
    RUN(test_encounter_fight_live_combat_win)     \
    RUN(test_refarm_diminishing_rewards)          \
    RUN(test_bandwidth_farming_resource)          \
    RUN(test_hacker_face_toggle)                  \
    RUN(test_hacker_shop_bandwidth_upgrade)       \
    RUN(test_rig_cost_curve_formulas)              \
    RUN(test_hacker_shop_rack_slot_upgrade)       \
    RUN(test_hacker_shop_scraping_and_datamining_bonus) \
    RUN(test_hacker_shop_frag_reducer)            \
    RUN(test_hacker_shop_frag_trigger)            \
    RUN(test_hacker_shop_hunger_xp_farming)       \
    RUN(test_hacker_shop_combat_xp_boost)         \
    RUN(test_cache_not_openable_from_items)       \
    RUN(test_battle_fatigue)                      \
    RUN(test_explore_auto_continues_after_fight)  \
    RUN(test_post_encounter_reports_bandwidth_shield) \
    RUN(test_post_encounter_reports_frag_rise_when_unshielded) \
    RUN(test_post_encounter_level_line_renders) \
    RUN(test_post_encounter_never_for_sim_battle) \
    RUN(test_expl_sector_linear_gating)           \
    RUN(test_expl_names_fit_their_rows)           \
    RUN(test_expl_level_scoped_rows)              \
    RUN(test_combat_carry_health)                 \
    RUN(test_bits_reward_bounds)                   \
    RUN(test_explore_streak_unlocks_boss_then_clears) \
    RUN(test_auto_progress_steps_positionally)    \
    RUN(test_auto_progress_gauntlet_rolls_to_next_area) \
    RUN(test_expl_nested_row_helpers)             \
    RUN(test_area_boss_gauntlet_composition)      \
    RUN(test_boss_threat_moves_area_adjacent)     \
    RUN(test_expl_nested_list_nav)                \
    RUN(test_deepweb_dive)                        \
    RUN(test_zone_titles_equip_via_cfg_and_persist) \
    RUN(test_zone_titles_picker_skips_locked)     \
    RUN(test_combat_force_enemy_first)            \
    RUN(test_encounter_sinkhole_bypass)           \
    RUN(test_sinkhole_xp_persists_immediately)    \
    /* The Wi-Fi network explore event */   \
    RUN(test_wifi_event_reached)                  \
    RUN(test_wifi_networks_seen_counter)          \
    RUN(test_wifi_sleeping_guardian_grants_loot)  \
    RUN(test_wifi_open_cache_grants_loot)         \
    RUN(test_wifi_awakened_guardian_enters_combat) \
    RUN(test_wifi_never_awakens_when_not_hot)     \
    RUN(test_wifi_friendly_visit_grants_ally_buff) \
    RUN(test_wifi_auto_plays_out_after_hold)      \
    /*Backlog / — shop explore events + the two shop consumables */ \
    RUN(test_shop_event_buy_decrements)           \
    RUN(test_shop_buy_gated_when_broke)           \
    RUN(test_shop_sold_out_after_stock)           \
    RUN(test_shop_cursor_cycles_all_listings)     \
    RUN(test_mod_shop_buy_grants_mod)             \
    RUN(test_mod_shop_buy_gated_by_item_cost)     \
    RUN(test_shop_grayscale)                      \
    RUN(test_shop_auto_leaves_after_hold)         \
    RUN(test_sealed_cache_walk_find)              \
    RUN(test_sealed_cache_open_grants_reward)     \
    RUN(test_sealed_cache_item_shape)             \
    RUN(test_cache_epic_pool_and_multidrop)       \
    RUN(test_cache_common_pool_single_drop)       \
    RUN(test_item_earn_coverage)                  \
    RUN(test_cache_yield_reveal_and_dismiss)      \
    RUN(test_warp_item_shape)                     \
    RUN(test_warp_key_inert_from_items)           \
    RUN(test_access_token_warps_to_shop)          \
    RUN(test_safe_mode_key_warps_to_rest)         \
    RUN(test_explore_warp_no_keys_is_noop)        \
    RUN(test_warp_key_walk_find)                  \
    RUN(test_null_noodles_effects)                \
    RUN(test_null_noodles_happy_pull_from_below)  \
    RUN(test_r007_b33r_effects)                   \
    /* Hacker Rank — XP model (S60) + device-scan dedup (J.21b) */ \
    RUN(test_hacker_rank_xp_model)                \
    RUN(test_register_network_queue_dedup)         \
    RUN(test_peer_hello_round_trip)               \
    RUN(test_register_peer_counts_encounters_not_frames) \
    RUN(test_register_peer_ignores_foreign_traffic) \
    RUN(test_peer_rows_live_first_and_deduped)    \
    RUN(test_peers_screen_grayscale_live_vs_remembered) \
    RUN(test_peers_screen_raises_link_without_touching_config) \
    RUN(test_peers_screen_outlives_the_menu_idle_timer) \
    RUN(test_pvp_frame_round_trip)          \
    RUN(test_pvp_host_election_agrees_on_both_sides) \
    RUN(test_pvp_commit_hash_catches_divergence) \
    RUN(test_pvp_same_seed_plays_the_same_fight) \
    RUN(test_pvp_seating_order_is_load_bearing) \
    RUN(test_pvp_two_devices_duel_end_to_end) \
    RUN(test_pvp_duel_marks_the_opponent_species_seen) \
    RUN(test_pvp_invite_retries_do_not_decline_the_pending_challenge) \
    RUN(test_pvp_challenge_needs_a_human_on_the_link_screen) \
    RUN(test_pvp_unanswered_challenge_times_out) \
    RUN(test_pvp_link_screen_verdict_grayscale) \
    RUN(test_pvp_guest_sees_its_own_pet_on_the_bottom_gauge) \
    RUN(test_combat_seats_local_pet_on_the_left) \
    RUN(test_combat_ransom_pool_grayscale)  \
    RUN(test_save_v37_link_roundtrip)             \
    RUN(test_save_v38_net_slot_is_reserved)              \
    RUN(test_save_v37_to_v38_net_default)         \
    RUN(test_radio_consents_are_independent)      \
    RUN(test_save_defers_when_the_heap_is_too_low) \
    RUN(test_a_refused_write_is_not_a_save) \
    RUN(test_a_hatch_the_store_refused_still_reaches_flash) \
    RUN(test_net_access_needs_a_live_job)     \
    RUN(test_net_attempt_outcomes)                \
    RUN(test_qr_setup_step_appears_only_until_provisioned) \
    RUN(test_update_check_requires_network_and_source) \
    RUN(test_update_job_holds_the_radio_past_the_screen) \
    RUN(test_updates_screen_outlives_the_menu_idle_timer) \
    RUN(test_update_job_dies_when_the_join_fails)        \
    RUN(test_update_install_needs_a_confirmed_finding) \
    RUN(test_update_install_takes_two_yeses)      \
    RUN(test_flasher_url_follows_the_publish_host) \
    RUN(test_flasher_row_is_last_and_needs_no_network) \
    RUN(test_update_install_progress_is_not_terminal) \
    RUN(test_tar_rejects_escaping_names)          \
    RUN(test_tar_reads_entries)                   \
    RUN(test_manifest_parses_published_shape)     \
    RUN(test_manifest_rejects_junk_and_unusable_rows) \
    RUN(test_manifest_tolerates_a_newer_publish)  \
    RUN(test_version_marker_round_trips)          \
    RUN(test_version_marker_tolerates_real_files) \
    RUN(test_version_marker_absent_reads_as_unknown) \
    RUN(test_save_v36_to_v37_link_default)        \
    RUN(test_peer_hello_rejects_malformed)        \
    RUN(test_peer_hello_sanitizes_hostile_fields) \
    RUN(test_peer_ledger_new_and_refresh)         \
    RUN(test_network_ledger_new_and_repeat)       \
    RUN(test_network_ledger_in_top_n)             \
    RUN(test_network_discovery_repeat_familiar_vs_home_turf) \
    RUN(test_network_discovery_empty_queue_penalty_throttles) \
    RUN(test_hacker_rank_up_grants_reward)        \
    /* Audit handshake capture (pcap + policy SM, save v6) */ \
    RUN(test_pcap_global_header)                  \
    RUN(test_pcap_writer_stream)                  \
    RUN(test_pcap_writer_requires_begin)          \
    RUN(test_audit_capture_state_machine)         \
    RUN(test_audit_capture_seal_paths)            \
    RUN(test_audit_capture_toggle_persists)       \
    RUN(test_audit_mode_enforces_scan_dependency) \
    RUN(test_audit_legacy_capture_without_scan_normalizes_on_load) \
    RUN(test_save_v5_migration_defaults_capture_off) \
    RUN(test_eapol_parse_classifies_messages)     \
    RUN(test_eapol_parse_rejects_non_eapol)       \
    RUN(test_handshake_tracker_first_crackable)   \
    RUN(test_audit_capture_arm_window_self_seals) \
    RUN(test_audit_capture_seal_active_from_armed) \
    RUN(test_register_handshake_dedup)            \
    RUN(test_audit_ledgers_persist_no_recredit)   \
    RUN(test_save_v6_migration_defaults_ledgers_empty) \
    RUN(test_save_v7_ledger_roundtrip)            \
    /*pcap-blowup fix — filename-by-network + discovery rewards */ \
    RUN(test_pcap_naming_sanitizes_and_falls_back_to_bssid) \
    RUN(test_handshake_already_captured_query)    \
    RUN(test_network_discovery_reward_fires_once_per_network) \
    RUN(test_network_discovery_reward_rarity_ratio) \
    RUN(test_handshake_capture_reward_fires_once_per_handshake) \
    RUN(test_handshake_capture_reward_rarity_ratio) \
    /* Move drop tables + malbeast roster depth */ \
    RUN(test_move_loadout_grant_no_duplicate)     \
    RUN(test_wild_win_can_drop_a_move)            \
    /* Mods into combat (data-driven effects, earn path, equip-level gate) */ \
    RUN(test_mod_effects_data_driven)             \
    RUN(test_backup_drive_death_save_ignores_survivable_hits) \
    RUN(test_backup_drive_death_save_restores_half_max_health) \
    RUN(test_backup_drive_death_save_covers_a_fatal_dot) \
    RUN(test_backup_drive_death_save_loses_to_overkill) \
    RUN(test_mod_thorns_and_deathblast)           \
    RUN(test_mod_ecc_memory_hitcap)               \
    RUN(test_mod_load_balancer_split)             \
    RUN(test_mod_watchdog_timer)                  \
    RUN(test_mod_faraday_cage)                    \
    RUN(test_mod_content_rarity_tier)             \
    RUN(test_mod_earn_tables_and_reqlevel)        \
    RUN(test_mod_niche_flavour_data_driven)       \
    RUN(test_mod_botnet_swarm_and_airgap_ward)    \
    RUN(test_mod_state_combine_rules)             \
    RUN(test_mod_prowlware_rank_computation)      \
    RUN(test_mod_prowlware_combat_effect)         \
    RUN(test_mod_canary_trap)                     \
    RUN(test_mod_meltdown_core)                   \
    RUN(test_mod_zero_day_exploit)                \
    RUN(test_mod_tripwire)                        \
    RUN(test_phishing_bubble_steal)               \
    RUN(test_phishing_perfect_bite)               \
    RUN(test_phishing_shield_pool)                \
    RUN(test_speed_action_economy)                \
    RUN(test_min_damage_penetration)              \
    RUN(test_mod_hard_line_gate)                  \
    RUN(test_mod_equip_level_gate)                \
    RUN(test_mod_picker_windows_large_pool)       \
    RUN(test_save_v18_mod_reqlevel)               \
    RUN(test_mod_storage_cap_bounds_the_pool_and_the_shop_raises_it) \
    RUN(test_save_mod_count_nibbles_pack_two_mods_per_byte) \
    RUN(test_save_v44_to_v45_mod_pool_migration) \
    RUN(test_save_mod_count_for_a_retired_wire_is_dropped) \
    RUN(test_save_v18_to_v19_bwupgrade_default)   \
    RUN(test_save_v19_to_v20_ap_default)          \
    RUN(test_save_v21_shield_roundtrip)           \
    RUN(test_save_v20_to_v21_shield_default)      \
    RUN(test_save_v33_drops_network_dedup)        \
    RUN(test_save_v21_to_v22_frag_trigger_default) \
    RUN(test_save_v22_to_v23_shop_unlocks_default) \
    /* ITEMS type-tabs + VAULT bulk-open (Hacker SHOP, save v23) */ \
    RUN(test_hacker_shop_item_tabs_buy)           \
    RUN(test_hacker_shop_bulk_open_buy)           \
    RUN(test_item_filter_narrows_rows)            \
    RUN(test_item_hold_a_cycles_filter_tap_steps) \
    RUN(test_hacker_shop_item_picker_buy)         \
    RUN(test_item_category_filters_split_quest)   \
    RUN(test_item_picker_tiles_count_units)       \
    RUN(test_item_picker_grayscale)               \
    RUN(test_item_picker_nav_drills_in_and_back)  \
    RUN(test_item_picker_skipped_in_lockout)      \
    RUN(test_item_hold_a_follows_picker_axis)     \
    RUN(test_vault_bulk_open_consumes_all_of_rarity) \
    RUN(test_vault_hold_b_bulk_opens_tap_opens_one) \
    RUN(test_ap_toggle_via_cfg_and_persist)       \
    RUN(test_pedia_qr_two_pages_no_timeout)       \
    RUN(test_wild_malbeast_roster_variants)       \
    RUN(test_wild_and_roster_names_disjoint)      \
    /* Branching creature system scaffold */ \
    RUN(test_evolution_routing_tables)            \
    RUN(test_hatch_pool_ransomware)               \
    RUN(test_canine_line_evolution_branch)        \
    RUN(test_pingcub_rejoins_the_bear_line)       \
    RUN(test_cat_line_evolution_branch)           \
    RUN(test_frog_line_linear)                    \
    RUN(test_anglerfish_deepdive_hatch_gate)      \
    RUN(test_anglerfish_line_evolution_branch)    \
    RUN(test_trojan_content)                      \
    RUN(test_trojan_cross_line_divert)            \
    RUN(test_trojan_combat)                       \
    RUN(test_line_select_phishing_egg)            \
    RUN(test_line_select_grayscale)               \
    RUN(test_eggpick_win_halves_incubation)       \
    RUN(test_eggpick_miss_keeps_full_incubation)  \
    RUN(test_eggpick_aim_is_not_cancel)           \
    RUN(test_eggpick_target_never_wraps)          \
    RUN(test_eggpick_line_never_opens_decrypt)    \
    RUN(test_hatch_reveal_plays_the_animation)    \
    RUN(test_hatch_reveal_leaves_decrypt_line_alone) \
    RUN(test_eggpick_grayscale)                   \
    RUN(test_frog_line_move_access)               \
    RUN(test_dominant_signal_from_care)           \
    RUN(test_battery_percent_from_mv)           \
    RUN(test_firmware_version_ordering)         \
    /*Web 'Pedia slice — HackerTag rename + the state JSON builder */ \
    RUN(test_set_hacker_tag_validates)            \
    RUN(test_pedia_state_json_shape)              \
    RUN(test_pedia_state_json_fresh_hatch_egg)    \
    RUN(test_wild_malbeast_index_mapping)              \
    RUN(test_save_v24_to_v25_pedia_defaults)           \
    RUN(test_save_v25_roundtrip)                       \
    RUN(test_save_v27_roundtrip)                        \
    RUN(test_save_v32_roundtrip)                        \
    RUN(test_crew_exploit_negates_next_hits)            \
    RUN(test_crew_requires_home_network)                \
    RUN(test_crew_screen_pick_home_then_enlist)         \
    RUN(test_crew_picker_offers_networks_in_range)   \
    RUN(test_crew_picker_unaffected_by_saturated_reward_queue)   \
    RUN(test_crew_picker_drops_networks_out_of_range)   \
    RUN(test_crew_screen_raises_scan_without_touching_config) \
    RUN(test_crew_screen_outlives_the_menu_idle_timer)   \
    RUN(test_crew_exploit_in_combat_picker)             \
    RUN(test_save_v36_crew_roundtrip)                   \
    RUN(test_pedia_state_json_reveal_states)           \
    RUN(test_pedia_raised_tally_survives_evolution)    \
    RUN(test_save_v39_raised_tally_roundtrip_and_migration) \
    RUN(test_save_v41_renames_retired_creature_ids) \
    RUN(test_renamed_ids_table_invariants)  \
    RUN(test_save_v43_ladder_insert_shifts_expl_progress) \
    RUN(test_ladder_inserts_table_invariants) \
    RUN(test_area_icons_are_keyed_by_area_id) \
    RUN(test_stacker_shaves_the_overhang)     \
    RUN(test_stacker_missing_entirely_loses)  \
    RUN(test_stacker_clearing_every_row_wins) \
    RUN(test_stacker_run_stays_on_board_and_contiguous) \
    RUN(test_stacker_row_width_reports_the_survivors) \
    RUN(test_stacker_score_pays_for_height_twice) \
    RUN(test_stacker_win_credits_the_tally_and_the_shape_rows) \
    RUN(test_stacker_short_board_pays_what_it_stacked_and_costs_nothing) \
    RUN(test_stacker_cleared_board_wipes_the_disk) \
    RUN(test_stacker_stopping_early_banks_the_board) \
    RUN(test_stacker_wins_ladder_sweeps_and_persists) \
    RUN(test_rolled_defrag_takes_its_fixed_bite) \
    RUN(test_replication_ghost_is_raised_by_a_failed_defrag_on_a_critical_disk) \
    RUN(test_air_gapped_snack_cures_the_ghost_and_unlocks) \
    RUN(test_ghost_also_clears_through_a_successful_av_scan) \
    RUN(test_perishable_food_spoils_on_a_feeding) \
    RUN(test_icon_tint_and_theme_indirection) \
    RUN(test_full_pedia_achievement_reads_the_raised_tally) \
    /* Achievements: the catalogue's own invariants, the data-driven ladder sweep, \
       the kGoalAll sentinel, the home-screen banner queue, and v40 save migration */ \
    RUN(test_mod_table_wires_are_unique_and_in_range) \
    RUN(test_achievement_table_is_well_formed) \
    RUN(test_achievement_ladder_unlocks_and_pays) \
    RUN(test_backup_drive_achievement_mapping) \
    RUN(test_achievement_goal_all_tracks_the_set_size) \
    RUN(test_achievement_banner_announces_on_the_home_screen) \
    RUN(test_achievement_banner_waits_for_the_home_screen) \
    RUN(test_achievement_banner_collapses_a_burst) \
    RUN(test_save_v40_achievements_roundtrip_and_migration) \
    RUN(test_species_dive_records_feed_the_depth_rows) \
    RUN(test_collected_items_survive_being_spent) \
    RUN(test_pedia_state_json_rack_and_record_hatched) \
    RUN(test_pedia_first_brute_force_achievement)      \
    RUN(test_pedia_evolution_achievements_leave_the_sibling_locked) \
    RUN(test_every_combatant_sprite_resolves)

#ifdef PIO_UNIT_TESTING
void setUp() {}
void tearDown() {}
int main() {
    UNITY_BEGIN();
#define MAL_RUN_GATE(fn) RUN_TEST(fn);
    MAL_RUN_ALL_GATES(MAL_RUN_GATE)
#undef MAL_RUN_GATE
    return UNITY_END();
}
#else
int main() {
    std::puts("running native gates...");
#define MAL_RUN_GATE(fn) fn();
    MAL_RUN_ALL_GATES(MAL_RUN_GATE)
#undef MAL_RUN_GATE
    if (g_failures == 0) {
        std::puts("OK — all gates passed");
        return 0;
    }
    std::printf("FAILED — %d check(s)\n", g_failures);
    return 1;
}
#endif
