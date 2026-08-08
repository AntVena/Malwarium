// test_model.cpp — native gates for the pet model, the content registry seam and the L1 carousel.
//
// One slice of the native suite; see test_gates.h for the shared fixtures and
// test_main.cpp for the roster that runs these. A save-vNN gate sits with the
// feature whose field it migrates, not in a migrations pile of its own.
#include "test_gates.h"

// --- T2: stat model logic --------------------------------------------------
void test_pet_model_zones() {
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

void test_care_branch_and_clamp() {
    PetModel m;
    m.setCareMistakes(2); CHECK(m.careBranch() == CareBranch::Good);
    m.setCareMistakes(3); CHECK(m.careBranch() == CareBranch::Bad);
    m.setCareMistakes(4); CHECK(m.careBranch() == CareBranch::Bad);
    m.setCareMistakes(5); CHECK(m.careBranch() == CareBranch::Dying);
    m.addCareMistake(3);  CHECK(m.careMistakes() == kCareDying);  // clamps at 5
}

void test_hunger_decay() {
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
void test_content_registry() {
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

void test_grayscale_gate() {
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

    CHECK(litCellsGray(fb, GX, GW, 74) == 1);   // hunger 12 -> 1
    CHECK(litCellsGray(fb, GX, GW, 96) == 8);   // frag 82 -> 8
    CHECK(litCellsGray(fb, GX, GW, 118) == 5);  // happy 50 -> 5
}

// Carousel summon + book-wrap -----------------------
void test_carousel_summon() {
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

void test_carousel_bookwrap() {
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
void test_carousel_focus_grayscale() {
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
void test_carousel_enter_back() {
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
void test_caption_pinned_top() {
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
void test_carousel_autodefocus() {
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
    r.tick(4000 + kAutoDefocusMs - 1000);  // just under the budget since reset -> still summoned
    CHECK(r.nav() == Game::Nav::Cursor);
    r.tick(4000 + kAutoDefocusMs + 1);     // past the budget since reset -> idle
    CHECK(r.nav() == Game::Nav::Idle);
}

// Each UI Mode renders; none float text into the living area, and IconsLabel
// swaps only the focused slot's icon for text (verified pixel-exact against
// TextOnly/IconsOnly in test_carousel_focus_grayscale).
void test_carousel_ui_modes() {
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
