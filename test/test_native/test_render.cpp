// test_render.cpp — native gates for the render pipeline and the resting habitat.
//
// One slice of the native suite; see test_gates.h for the shared fixtures and
// test_main.cpp for the roster that runs these. A save-vNN gate sits with the
// feature whose field it migrates, not in a migrations pile of its own.
#include "test_gates.h"

// --- Gate: PNG round-trips to RGB565 and blits pixel-correct ---------------
void test_sprite_roundtrip() {
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
void test_upscale_boundaries() {
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
void test_palette_luminance_ordered() {
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

// --- Resting habitat — pet in the living area, carousel always framing it.
void test_idle_habitat() {
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
void test_hunger_alert_gated() {
    const int x0 = kActiveW - 24, y0 = kLivingTop + 2, y1 = kLivingTop + 26;
    Framebuffer calm(kActiveW, kActiveH), hungry(kActiveW, kActiveH);
    renderIdleAtBeat(calm, 0, false);
    renderIdleAtBeat(hungry, 0, true);   // beat 0 is an "on" blink phase
    // The two frames differ only in the hunger state, and the backdrop behind them is
    // the same place at the same beat — so a difference in this slot IS the alert. An
    // alert drawn unconditionally would leave the slot identical and fail here.
    CHECK(regionDiffers(calm, hungry, x0, y0, kActiveW, y1));
    CHECK(anyLitGray(hungry, x0, y0, kActiveW, y1));
}

// --- T1: the idle loop animates (frames differ across the heartbeat) -------
void test_idle_breathe_animates() {
    Framebuffer a(kActiveW, kActiveH), b(kActiveW, kActiveH);
    renderIdleAtBeat(a, 0, false);   // idle_a
    renderIdleAtBeat(b, 1, false);   // idle_b
    int diff = 0;
    for (int y = 0; y < kActiveH; ++y)
        for (int x = 0; x < kActiveW; ++x)
            if (a.get(x, y) != b.get(x, y)) ++diff;
    CHECK(diff > 0);
}

// SD-present icon: reveals when the card becomes present, then hides after the
// window; a re-read of an already-present card doesn't re-flash it, but a
// re-insert (absent -> present) does.
void test_sd_icon_reveal_window() {
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
void test_capture_badge_phases() {
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
void test_idle_status_icons_grayscale() {
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
    CHECK(regionDiffers(all, noSd, sx0, sy0, sx1, sy1));   // the flash slot follows the card
    CHECK(!regionDiffers(all, noSd, bx0, by0, bx1, by1));  // ...and the badge slot does not
    CHECK(anyLitGray(noSd, bx0, by0, bx1, by1));           // badge still shown
}

// Runtime SD re-check engine seam (mirrors the netScan one-shot). CFG sets it;
// the device tier reads + clears it after re-mounting.
void test_sd_recheck_request_seam() {
    Game g{StartMode::Hatched};
    CHECK(!g.sdRecheckRequested());
    g.requestSdRecheck();
    CHECK(g.sdRecheckRequested());
    g.clearSdRecheck();
    CHECK(!g.sdRecheckRequested());
}
