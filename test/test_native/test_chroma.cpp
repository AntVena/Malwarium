// test_chroma.cpp — native gates for the CHROMATOPHORE, the Metamorphic egg's hatch
// minigame, and the egg-locomotion rule its jellyfish egg is the reason for.
//
// One slice of the native suite; see test_gates.h for the shared fixtures and
// test_main.cpp for the roster that runs these.
#include "test_gates.h"

#include "core/model/chromatophore.h"
#include "core/render/camo.h"

// --- The rules model (core/model/chromatophore.h) --------------------------

// Wear `skin` and hold it until the repaint is finished. Returns the ms spent, so a
// gate can tell how much of its window it has left.
static uint32_t chromaSettle(Chromatophore& c, int skin) {
    c.wear(skin);
    uint32_t spent = 0;
    while (c.wearPct() < 100 && c.running()) {
        c.tick(50);
        spent += 50;
    }
    return spent;
}

// Run the clock out on the round the board is on.
static void chromaRunOutWindow(Chromatophore& c) {
    const int round = c.round();
    for (int guard = 0; guard < 400 && c.running() && c.round() == round; ++guard)
        c.tick(50);
}

// The opening plate is never what the pet is already wearing: every round is a real
// change, including the first, so no round plays itself.
void test_chroma_opens_on_a_change() {
    for (uint32_t seed = 1; seed <= 40; ++seed) {
        Chromatophore c;
        c.reset(seed, 6, 4000, 400, /*switching=*/false);
        CHECK(c.running());
        CHECK(c.plate() != c.worn());
        CHECK(c.wearPct() == 100);     // settled in whatever it opened as
        CHECK(!c.hidden());            // ...which is not the water
        CHECK(c.passes() == 0 && c.round() == 0);
    }
}

// The pass the board is for: wear the water, let the repaint finish, and the sweep
// finds nothing. The run moves on to the next round with one pass banked.
void test_chroma_settled_match_survives_the_sweep() {
    Chromatophore c;
    c.reset(3u, 6, 4000, 400, /*switching=*/false);
    chromaSettle(c, c.plate());
    CHECK(c.hidden());
    chromaRunOutWindow(c);
    CHECK(c.running());
    CHECK(c.passes() == 1 && c.round() == 1);
    CHECK(c.plate() != c.worn());      // and the next round asks for another change
}

// The whole point of the settle: pressing the right button is not the same as wearing
// it. A change started too late is caught halfway and scores as caught.
void test_chroma_caught_midchange_is_spotted() {
    Chromatophore c;
    c.reset(3u, 6, 4000, 400, /*switching=*/false);
    // Burn the window down to less than a repaint, THEN press the right button.
    while (c.running() && c.sweepPct() < 90) c.tick(50);
    c.wear(c.plate());
    CHECK(c.wearPct() < 100);          // started, not finished
    CHECK(!c.hidden());
    chromaRunOutWindow(c);
    CHECK(!c.running());
    CHECK(c.state() == Chromatophore::State::Spotted);
    CHECK(c.passes() == 0);            // nothing banked from a round it lost
}

// Wearing the wrong skin, however settled, is just being visible in a different colour.
void test_chroma_wrong_skin_is_spotted() {
    Chromatophore c;
    c.reset(9u, 6, 4000, 400, /*switching=*/false);
    const int wrong = (c.plate() + 1) % kChromaSkins;
    chromaSettle(c, wrong);
    CHECK(c.wearPct() == 100 && !c.hidden());
    chromaRunOutWindow(c);
    CHECK(c.state() == Chromatophore::State::Spotted);
}

// A nervous second press on the skin already worn is inert — it must not restart a
// repaint that had already finished, which would turn a safe pass into a caught one.
void test_chroma_pressing_the_worn_skin_is_inert() {
    Chromatophore c;
    c.reset(5u, 6, 4000, 400, /*switching=*/false);
    chromaSettle(c, c.plate());
    CHECK(c.hidden());
    c.wear(c.worn());
    c.wear(c.worn());
    CHECK(c.wearPct() == 100 && c.hidden());
    chromaRunOutWindow(c);
    CHECK(c.passes() == 1);
}

// The ramp: each round gives less time than the last, and the shrink stops at the floor
// rather than closing to nothing however many rounds a run asks for.
void test_chroma_window_shrinks_to_a_floor() {
    Chromatophore c;
    c.reset(11u, 20, 4000, 400, /*switching=*/false);
    uint32_t prev = 0;
    for (int r = 0; r < 20 && c.running(); ++r) {
        // Time the round by playing it perfectly: settle on the plate, then count.
        const uint32_t settle = chromaSettle(c, c.plate());
        uint32_t window = settle;
        const int round = c.round();
        while (c.running() && c.round() == round) { c.tick(50); window += 50; }
        if (r > 0) CHECK(window <= prev);            // never longer than the one before
        CHECK(window + 50 >= kChromaWindowFloorMs);  // ...and never below the floor
        prev = window;
    }
    CHECK(c.clean());
}

// Switching is the hard cabinet's rule and nothing else's: off, the water the round
// opened with is the water the sweep judges.
void test_chroma_water_holds_still_unless_switching() {
    Chromatophore c;
    c.reset(21u, 6, 4000, 400, /*switching=*/false);
    const int plate = c.plate();
    chromaSettle(c, plate);
    while (c.running() && c.round() == 0) {
        CHECK(c.plate() == plate);
        c.tick(50);
    }
    CHECK(c.passes() == 1);
}

// ...and on, a committed disguise can be made wrong mid-window, which is the whole
// difference: the pass now needs a SECOND repaint inside what is left of the clock.
void test_chroma_switching_moves_the_water_mid_window() {
    Chromatophore c;
    c.reset(21u, 6, 4000, 400, /*switching=*/true);
    const int opened = c.plate();
    chromaSettle(c, opened);
    CHECK(c.hidden());
    while (c.running() && c.round() == 0 && c.plate() == opened) c.tick(50);
    CHECK(c.plate() != opened);        // the water changed under it
    CHECK(!c.hidden());                // ...and the disguise is now the wrong one
    // There is still time to answer it, which is what keeps the setting fair.
    chromaSettle(c, c.plate());
    chromaRunOutWindow(c);
    CHECK(c.passes() == 1);
}

// Every round survived is the clean run, and it ends the board rather than rolling on.
void test_chroma_clean_run_takes_every_round() {
    Chromatophore c;
    c.reset(4u, 6, 4000, 400, /*switching=*/false);
    for (int r = 0; r < 6 && c.running(); ++r) {
        chromaSettle(c, c.plate());
        chromaRunOutWindow(c);
    }
    CHECK(!c.running());
    CHECK(c.clean() && c.passes() == 6 && c.round() == 6);
    // A finished board is inert: nothing a late press does can add to a banked run.
    c.wear((c.worn() + 1) % kChromaSkins);
    c.tick(5000);
    CHECK(c.passes() == 6 && c.clean());
}

// --- The screen (game_chroma.cpp) ------------------------------------------

// The button that wears skin `s`, in the order the chips are drawn.
static Button chromaButton(int skin) {
    return skin == 0 ? Button::A : skin == 1 ? Button::B : Button::C;
}

// Lay a Metamorphic egg, which drops straight into the CHROMATOPHORE.
static Game metaEgg() {
    Game g;
    g.unlockAchievement(ach::kHashCollision);   // unlocks the Metamorphic line
    g.resetToHatch();
    const auto lines = g.availableEggLines();
    for (int i = 0; i < static_cast<int>(lines.size()); ++i) {
        if (std::strcmp(lines[g.lineSelectRow()]->id, "metamorphic") == 0) break;
        g.onButton(press(Button::A));           // cycle the highlighted line
    }
    g.onButton(press(Button::B));               // lay it
    return g;
}

// The egg is laid and the board opens at once, on the shipped shape: every round, the
// window shrinking, and the water holding still.
void test_chroma_opens_on_a_laid_metamorphic_egg() {
    Game g = metaEgg();
    CHECK(g.pet() && std::strcmp(g.pet()->id, "polystaria") == 0);
    CHECK(g.inEggPhase());
    CHECK(g.nav() == Game::Nav::Chroma);
    CHECK(g.chroma().running());
    CHECK(g.chroma().goal() == kChromaRounds);
    CHECK(g.bootHatchRemainMs() == kBootHatchMs);
}

// The board runs on real time and the incubation clock is frozen underneath it, so the
// window a round quoted is the window it is judged by.
void test_chroma_runs_on_real_time_with_the_clock_frozen() {
    Game g = metaEgg();
    uint32_t t = 0;
    const int sweep0 = g.chroma().sweepPct();
    g.tick(t += 500);
    CHECK(g.chroma().sweepPct() > sweep0);
    CHECK(g.bootHatchRemainMs() == kBootHatchMs);
}

// A pass is worth kChromaPassMs off the incubation clock, banked on the way out — and
// banked ONCE, however many times the verdict screen is pressed.
void test_chroma_passes_pay_the_incubation_clock() {
    Game g = metaEgg();
    uint32_t t = 0;
    // Play it perfectly until the run ends, wearing whatever the water is.
    while (g.chroma().running()) {
        g.onButton(press(chromaButton(g.chroma().plate())));
        g.tick(t += kFxAnimMs);
    }
    const int passes = g.chroma().passes();
    CHECK(passes > 0);
    CHECK(g.bootHatchRemainMs() == kBootHatchMs);   // nothing spent until it is banked
    tapC(g);                                        // C is disabled on the verdict
    CHECK(g.nav() == Game::Nav::Chroma);
    g.onButton(press(Button::B));
    const uint32_t owed = static_cast<uint32_t>(passes) * kChromaPassMs;
    CHECK(g.bootHatchRemainMs() == kBootHatchMs - owed);
    // ...and the same passes cannot be sold twice.
    g.onButton(press(Button::B));
    CHECK(g.bootHatchRemainMs() == kBootHatchMs - owed);
}

// A clean run buys the WHOLE incubation clock, which is the Isolation Protocol's
// identity and now this board's: play it perfectly and the egg hatches out of the run
// rather than out of a wait. A run that falls over partway still keeps what it bought,
// which is the difference between this and the all-or-nothing game the line used to
// borrow.
void test_chroma_clean_run_buys_the_whole_clock() {
    CHECK(static_cast<uint32_t>(kChromaRounds) * kChromaPassMs == kBootHatchMs);
}

// ...and that is what it does on the device: the last pass takes the clock to zero, so
// finishing the board hatches the pet on the spot and fires NEVER_SEEN.
void test_chroma_clean_hatch_pops_the_egg_and_unlocks() {
    Game g = metaEgg();
    uint32_t t = 0;
    while (g.chroma().running()) {
        g.onButton(press(chromaButton(g.chroma().plate())));
        g.tick(t += kFxAnimMs);
    }
    CHECK(g.chroma().clean() && g.chroma().passes() == kChromaRounds);
    CHECK(!g.hasAchievement(ach::kNeverSeen));   // nothing banked until B is taken
    g.onButton(press(Button::B));
    CHECK(g.hasAchievement(ach::kNeverSeen));
    CHECK(g.bootHatchRemainMs() == 0);
    CHECK(!g.inEggPhase());                      // hatched out of the run itself
    CHECK(g.nav() != Game::Nav::Chroma);
}

// All three buttons wear a skin, in the order the chips are drawn. That is the
// deviation from the standard A/B/C contract, so it is the one thing the screen has to
// get exactly right.
void test_chroma_three_buttons_are_three_skins() {
    Game g = metaEgg();
    g.onButton(press(Button::A));
    CHECK(g.chroma().worn() == 0);
    g.onButton(press(Button::B));
    CHECK(g.chroma().worn() == 1);
    g.onButton(press(Button::C));
    CHECK(g.chroma().worn() == 2);
    CHECK(g.nav() == Game::Nav::Chroma);   // and none of them leaves the board
}

// The subject is the pet when the pet's family wears borrowed colours, which is every
// route in from a hatch, and the Metamorphic egg otherwise — so an arcade cabinet
// played by anybody's pet still has something on the board.
void test_chroma_subject_falls_back_to_the_egg() {
    Game g = metaEgg();
    CHECK(g.chromaSubject() && std::strcmp(g.chromaSubject()->id, "polystaria") == 0);

    Game r;                                  // a fresh save is a Ransomware egg
    if (r.inLineSelect()) r.onButton(press(Button::B));
    CHECK(r.pet() && std::strcmp(r.pet()->line, "ransomware") == 0);
    CHECK(r.chromaSubject() && std::strcmp(r.chromaSubject()->id, "polystaria") == 0);
}

// GRAYSCALE. The three skins are a luminance ladder, so which skin the water is — and
// which one the creature is wearing — survives the colour being taken away. The board
// backs that up with per-skin texture and named chips, but the ladder is the part a
// retune can silently break.
void test_chroma_grayscale() {
    const Pal skins[kChromaSkins] = {Pal::CAMO_KELP, Pal::CAMO_SILT, Pal::CAMO_BLOOM};
    float prev = 0.0f;
    for (int i = 0; i < kChromaSkins; ++i) {
        const float lum = luminance(palColor(skins[i]));
        if (i > 0) CHECK(lum - prev > 0.15f);   // strictly ascending, and by enough
        prev = lum;
    }
    CHECK(luminance(palColor(skins[0])) >
          luminance(palColor(Pal::PAPER)) + 0.15f);      // the bottom rung clears PAPER
    CHECK(prev < luminance(palColor(Pal::INK)) - 0.15f); // ...and the top clears INK

    // The water is drawn, the creature is drawn on it, and the chips are drawn under
    // it — all three in a frame with no colour decision of their own to make.
    Framebuffer fb(kActiveW, kActiveH);
    Game g = metaEgg();
    g.render(fb);
    CHECK(anyNonPaper(fb, 20, 50, 200, 138));    // the water
    CHECK(anyNonPaper(fb, 20, 152, 200, 168));   // the chips
}

// --- Egg locomotion (the reason the jellyfish egg exists) ------------------

// An egg moves the way its ROW says, not the way its stage does. Almost every egg is
// Static and sits exactly where it was laid; the Metamorphic one is a bell hanging in
// water and drifts, which is the whole reason the rule is content rather than code.
void test_chroma_egg_drifts_because_its_row_says_so() {
    Game g = metaEgg();
    CHECK(g.pet()->locomotion == Locomotion::Swim);
    uint32_t t = 0;
    bool moved = false;
    for (int i = 0; i < 200 && !moved; ++i) {
        g.tick(t += kHeartbeatMs);
        moved = g.petWander().offsetX() != 0 || g.petWander().offsetY() != 0;
    }
    CHECK(moved);

    Game r;                                  // the Ransomware egg, and every other one
    if (r.inLineSelect()) r.onButton(press(Button::B));
    CHECK(r.pet()->locomotion == Locomotion::Static);
    t = 0;
    for (int i = 0; i < 200; ++i) {
        r.tick(t += kHeartbeatMs);
        CHECK(r.petWander().offsetX() == 0 && r.petWander().offsetY() == 0);
    }
}

// --- The colour source (core/render/camo.h) --------------------------------

// camoRampFromTone is the seam a caller with no sprite to sample uses. It has to hand
// back the same KIND of thing camoRampFrom does — a darkest-first value scale — or the
// draw path would be able to tell where a ramp came from.
void test_camo_ramp_from_a_named_tone_is_a_value_scale() {
    for (Pal p : {Pal::CAMO_KELP, Pal::CAMO_SILT, Pal::CAMO_BLOOM}) {
        const CamoRamp r = camoRampFromTone(palColor(p), 5);
        CHECK(r.count == 5 && !r.empty());
        for (int i = 1; i < r.count; ++i)
            CHECK(luminance(r.tone[i]) > luminance(r.tone[i - 1]));   // darkest first
        // The colour asked for is IN the ramp, not merely near it: the chip beside the
        // water and the creature standing in it have to be the same skin.
        bool found = false;
        for (int i = 0; i < r.count; ++i) found |= (r.tone[i] == palColor(p));
        CHECK(found);
    }
    // Clamped at both ends rather than trusted: a caller asking for one tone or for
    // twenty gets a usable ramp, never an out-of-bounds one.
    CHECK(camoRampFromTone(palColor(Pal::CAMO_SILT), 1).count == 2);
    CHECK(camoRampFromTone(palColor(Pal::CAMO_SILT), 99).count == kCamoRampMax);
}

// --- Endless: the cabinet shape --------------------------------------------

// A cabinet run has no round it finishes on: the board keeps dealing water until the
// sweep finds the player, which is what makes the score worth chasing rather than a box
// to tick. The window ramp still runs — it just settles at the floor and stays there.
void test_chroma_endless_run_has_no_finish() {
    Chromatophore c;
    c.reset(31u, /*rounds=*/0, 4000, 250, /*switching=*/false);
    CHECK(c.endless() && c.goal() == 0);
    for (int r = 0; r < kChromaRounds * 3; ++r) {
        CHECK(c.running() && !c.clean());
        chromaSettle(c, c.plate());
        chromaRunOutWindow(c);
    }
    CHECK(c.running());                       // still going, well past a hatch's length
    CHECK(c.passes() == kChromaRounds * 3);
    // ...and it ends the one way it can.
    const int wrong = (c.plate() + 1) % kChromaSkins;
    chromaSettle(c, wrong);
    chromaRunOutWindow(c);
    CHECK(!c.running() && !c.clean());
}
