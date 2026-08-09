// test_decypher.cpp — native gates for DISK DECYPHER: the board's rules, the hatch it
// pays into, and the arcade dial that is the only thing allowed to move either.
//
// One slice of the native suite; see test_gates.h for the shared fixtures and
// test_main.cpp for the roster that runs these. A save-vNN gate sits with the
// feature whose field it migrates, not in a migrations pile of its own.
#include "test_gates.h"

#include "core/model/disk_decypher.h"

namespace {

// Drive one attempt straight onto `want`, off the model rather than through a Game.
void guessRow(DiskDecypher& d, const int* want) {
    for (int s = 0; s < kDecypherSlots; ++s) {
        for (int i = 0; i < kDecypherColours && d.guess(s) != want[s]; ++i)
            d.cycleColour();
        d.lockIn();
    }
}

}  // namespace

// Three locks play a row, and the third one is the submit — there is no fourth press.
// Until then the working row is editable, and C is what re-opens a settled slot.
void test_decypher_three_locks_play_a_row() {
    DiskDecypher d;
    d.reset(12345, /*allowDuplicates=*/false);
    CHECK(d.played() == 0);
    CHECK(d.cursor() == 0);

    d.cycleColour();                      // slot 0 -> Blue
    CHECK(d.guess(0) == 1);
    d.lockIn();
    CHECK(d.locked(0) && d.cursor() == 1);
    d.cycleColour();                      // the cursor moved, so this is slot 1
    CHECK(d.guess(0) == 1 && d.guess(1) == 1);
    CHECK(d.played() == 0);               // still building — nothing played yet

    // C steps back and re-opens what it lands on, which is the whole undo.
    d.stepBack();
    CHECK(d.cursor() == 0 && !d.locked(0));
    d.cycleColour();
    CHECK(d.guess(0) == 2);

    d.lockIn();                           // slot 0 again -> cursor to the next unsettled
    d.lockIn();
    d.lockIn();
    CHECK(d.played() == 1);               // the third settle IS the submit
    CHECK(d.attemptsLeft() == kDecypherAttempts - 1);
}

// The two counts, and the multiplicity rule that is the only subtle part of them: a
// colour the key holds once can be credited once, however many times it is guessed.
void test_decypher_counts_exact_and_elsewhere() {
    DiskDecypher d;
    // Find a seed whose key is the three distinct colours 0,1,2 in some order — the
    // shape that makes every count below unambiguous.
    int key[kDecypherSlots] = {0};
    bool found = false;
    for (uint32_t seed = 1; seed < 4000 && !found; ++seed) {
        d.reset(seed, /*allowDuplicates=*/false);
        bool ok = true;
        for (int s = 0; s < kDecypherSlots; ++s) {
            key[s] = d.codeAt(s);
            if (key[s] > 2) ok = false;
        }
        found = ok;
    }
    CHECK(found);

    // The key exactly: three exact, none elsewhere, and the run ends there.
    guessRow(d, key);
    CHECK(d.row(0).exact == kDecypherSlots);
    CHECK(d.row(0).colour == 0);
    CHECK(d.cracked() && !d.running());

    // A rotation of the key: every colour is present and none is in place.
    d.reset(9001, /*allowDuplicates=*/false);
    int rot[kDecypherSlots];
    for (int s = 0; s < kDecypherSlots; ++s)
        rot[s] = d.codeAt((s + 1) % kDecypherSlots);
    guessRow(d, rot);
    CHECK(d.row(0).exact == 0);
    CHECK(d.row(0).colour == kDecypherSlots);

    // Three copies of one key colour: credited ONCE, in place, and never a second
    // time as "elsewhere" — the multiplicity rule.
    d.reset(9001, /*allowDuplicates=*/false);
    const int first = d.codeAt(0);
    int trip[kDecypherSlots];
    for (int s = 0; s < kDecypherSlots; ++s) trip[s] = first;
    guessRow(d, trip);
    CHECK(d.row(0).exact == 1);
    CHECK(d.row(0).colour == 0);
}

// Five wrong attempts lock the board out, and a locked-out board is worth nothing.
// A crack is worth the attempts it did NOT need, which is what the arcade prices on.
void test_decypher_scores_the_attempts_it_saved() {
    DiskDecypher d;
    d.reset(4242, /*allowDuplicates=*/false);
    CHECK(d.score() == 0);
    int key[kDecypherSlots];
    for (int s = 0; s < kDecypherSlots; ++s) key[s] = d.codeAt(s);
    guessRow(d, key);
    CHECK(d.cracked());
    CHECK(d.score() == kDecypherAttempts);          // first guess = the full ceiling
    CHECK(d.score() == DiskDecypher::maxScore());

    // The other end: five rows of the same wrong guess. All-Green can never be the key
    // when duplicates are barred, so this is a guaranteed lock-out.
    d.reset(4242, /*allowDuplicates=*/false);
    const int green[kDecypherSlots] = {0, 0, 0};
    for (int i = 0; i < kDecypherAttempts; ++i) guessRow(d, green);
    CHECK(!d.running() && !d.cracked());
    CHECK(d.played() == kDecypherAttempts);
    CHECK(d.score() == 0);
}

// The duplicate rule is a property of the KEY, and it is what the hard cabinet moves.
// Barred, every key is three distinct colours; allowed, some are not.
void test_decypher_duplicate_rule_is_the_key() {
    DiskDecypher d;
    for (uint32_t seed = 1; seed < 200; ++seed) {
        d.reset(seed, /*allowDuplicates=*/false);
        CHECK(d.codeAt(0) != d.codeAt(1));
        CHECK(d.codeAt(1) != d.codeAt(2));
        CHECK(d.codeAt(0) != d.codeAt(2));
    }
    bool sawADuplicate = false;
    for (uint32_t seed = 1; seed < 400 && !sawADuplicate; ++seed) {
        d.reset(seed, /*allowDuplicates=*/true);
        sawADuplicate = d.codeAt(0) == d.codeAt(1) || d.codeAt(1) == d.codeAt(2) ||
                        d.codeAt(0) == d.codeAt(2);
    }
    CHECK(sawADuplicate);
}

// The hatch payout: cracking the key halves what is left of the incubation clock and
// earns FIRST_BRUTE_FORCE; running out of attempts costs the egg nothing at all.
void test_decypher_hatch_pays_the_clock() {
    { Game g;
      if (g.inLineSelect()) g.onButton(press(Button::B));
      CHECK(g.inDecypher());
      CHECK(g.bootHatchRemainMs() == kBootHatchMs);
      crackDecypher(g);
      CHECK(g.nav() == Game::Nav::Idle);
      CHECK(g.bootHatchRemainMs() == kBootHatchMs / 2);
      CHECK(g.inEggPhase());                       // halved, not hatched
      CHECK(g.hasAchievement(ach::kFirstBruteForce)); }

    { Game g;
      if (g.inLineSelect()) g.onButton(press(Button::B));
      settleDecypher(g);
      CHECK(g.nav() == Game::Nav::Idle);
      CHECK(g.bootHatchRemainMs() == kBootHatchMs);   // a lost board costs nothing
      CHECK(!g.hasAchievement(ach::kFirstBruteForce)); }
}

// The hatch board always plays by the standard rules, whatever the arcade was last set
// to — a dial on a cabinet must never reach the egg.
void test_decypher_hatch_ignores_the_arcade_dial() {
    Game g{StartMode::Hatched};
    enterArcadeCabinet(g, arcadeGameIndexById("decypher"), ArcadeDifficulty::Hard);
    g.onButton(press(Button::B));                  // start it, so the dial is applied
    CHECK(g.inDecypher());
    CHECK(g.decypher().duplicatesAllowed());
    settleDecypher(g);
    g.onButton(press(Button::B));                  // dismiss the arcade payout

    g.resetToHatch();
    if (g.inLineSelect()) g.onButton(press(Button::B));
    CHECK(g.inDecypher());
    CHECK(!g.decypher().duplicatesAllowed());      // the egg gets the standard key
}

// The arcade cabinet pays on the SCORE, so a first-guess crack takes the whole bonus
// and a lost board takes none of it — and neither touches the incubation clock, which
// a raised pet does not have.
void test_decypher_arcade_pays_on_score() {
    const int row = arcadeGameIndexById("decypher");
    CHECK(row >= 0);
    { Game g{StartMode::Hatched};
      const int bits = g.bits();
      enterArcadeCabinet(g, row, ArcadeDifficulty::Medium);
      g.onButton(press(Button::B));
      crackDecypher(g);
      CHECK(g.nav() == Game::Nav::ArcadeResult);
      CHECK(g.bits() == bits + kArcadePlayBits + kArcadeScoreBits);
      CHECK(g.arcadeWins(row) == 1);
      // The arcade never fires the hatch's achievement — there was no egg in it.
      CHECK(!g.hasAchievement(ach::kFirstBruteForce)); }

    { Game g{StartMode::Hatched};
      const int bits = g.bits();
      enterArcadeCabinet(g, row, ArcadeDifficulty::Medium);
      g.onButton(press(Button::B));
      settleDecypher(g);
      CHECK(g.nav() == Game::Nav::ArcadeResult);
      CHECK(g.bits() == bits + kArcadePlayBits);   // the attempt, and no bonus
      CHECK(g.arcadeWins(row) == 0); }
}

// Grayscale gate. The five colours are a VOCABULARY, so the screen that cannot lean on
// them is this one: every cell carries its colour's initial, and the working row's
// focus is a box rather than a tint. Asserted as five distinct LUMINANCES across the
// palette plus ink inside every cell.
void test_decypher_grayscale() {
    Framebuffer fb(kActiveW, kActiveH);
    Game g;
    if (g.inLineSelect()) g.onButton(press(Button::B));
    CHECK(g.inDecypher());

    // Play four rows so the board carries history, each a different colour throughout.
    for (int r = 0; r < 4; ++r) {
        for (int s = 0; s < kDecypherSlots; ++s) {
            for (int c = 0; c < r; ++c) g.onButton(press(Button::A));
            g.onButton(press(Button::B));
        }
    }
    g.render(fb);
    // Every played cell has ink in it — the initial — so no cell is colour alone.
    CHECK(hasDarkInk(fb, 12, 30, 12 + 26, 30 + 22));

    // The five hues are luminance-separated, which is what keeps them five when the
    // colour is stripped. Checked off the palette, since that is where the promise is.
    const Pal five[kDecypherColours] = {Pal::DECYPHER_GREEN, Pal::DECYPHER_BLUE,
                                        Pal::DECYPHER_WHITE, Pal::DECYPHER_ORANGE,
                                        Pal::DECYPHER_PURPLE};
    for (int i = 0; i < kDecypherColours; ++i)
        for (int j = i + 1; j < kDecypherColours; ++j) {
            const float a = luminance(palColor(five[i]));
            const float b = luminance(palColor(five[j]));
            CHECK(std::fabs(a - b) > 0.04f);
        }
}
