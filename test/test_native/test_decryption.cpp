// test_decryption.cpp — native gates for DISK DECRYPTION: the board's rules, the hatch it
// pays into, and the arcade dial that is the only thing allowed to move either.
//
// One slice of the native suite; see test_gates.h for the shared fixtures and
// test_main.cpp for the roster that runs these. A save-vNN gate sits with the
// feature whose field it migrates, not in a migrations pile of its own.
#include "test_gates.h"

#include "core/model/disk_decryption.h"

namespace {

// Drive one attempt straight onto `want`, off the model rather than through a Game.
void guessRow(DiskDecryption& d, const int* want) {
    for (int s = 0; s < kDecryptionSlots; ++s) {
        for (int i = 0; i < kDecryptionColours && d.guess(s) != want[s]; ++i)
            d.cycleColour();
        d.lockIn();
    }
}

}  // namespace

// Three locks play a row, and the third one is the submit — there is no fourth press.
// Until then the working row is editable, and C is what re-opens a settled slot.
void test_decryption_three_locks_play_a_row() {
    DiskDecryption d;
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
    CHECK(d.attemptsLeft() == kDecryptionAttempts - 1);
}

// The two counts, and the multiplicity rule that is the only subtle part of them: a
// colour the key holds once can be credited once, however many times it is guessed.
void test_decryption_counts_exact_and_elsewhere() {
    DiskDecryption d;
    // Find a seed whose key is the three distinct colours 0,1,2 in some order — the
    // shape that makes every count below unambiguous.
    int key[kDecryptionSlots] = {0};
    bool found = false;
    for (uint32_t seed = 1; seed < 4000 && !found; ++seed) {
        d.reset(seed, /*allowDuplicates=*/false);
        bool ok = true;
        for (int s = 0; s < kDecryptionSlots; ++s) {
            key[s] = d.codeAt(s);
            if (key[s] > 2) ok = false;
        }
        found = ok;
    }
    CHECK(found);

    // The key exactly: three exact, none elsewhere, and the run ends there.
    guessRow(d, key);
    CHECK(d.row(0).exact == kDecryptionSlots);
    CHECK(d.row(0).colour == 0);
    CHECK(d.cracked() && !d.running());

    // A rotation of the key: every colour is present and none is in place.
    d.reset(9001, /*allowDuplicates=*/false);
    int rot[kDecryptionSlots];
    for (int s = 0; s < kDecryptionSlots; ++s)
        rot[s] = d.codeAt((s + 1) % kDecryptionSlots);
    guessRow(d, rot);
    CHECK(d.row(0).exact == 0);
    CHECK(d.row(0).colour == kDecryptionSlots);

    // Three copies of one key colour: credited ONCE, in place, and never a second
    // time as "elsewhere" — the multiplicity rule.
    d.reset(9001, /*allowDuplicates=*/false);
    const int first = d.codeAt(0);
    int trip[kDecryptionSlots];
    for (int s = 0; s < kDecryptionSlots; ++s) trip[s] = first;
    guessRow(d, trip);
    CHECK(d.row(0).exact == 1);
    CHECK(d.row(0).colour == 0);
}

// A crack is worth the attempts it did NOT need — but the two opening PROBES are
// floored out of that, so cracking on guess one, two or three all pay the ceiling and
// only the rows past them cost anything. Five wrong attempts lock the board out, and a
// locked-out board is worth nothing.
void test_decryption_scores_the_attempts_it_saved() {
    DiskDecryption d;
    d.reset(4242, /*allowDuplicates=*/false);
    CHECK(d.score() == 0);
    int key[kDecryptionSlots];
    for (int s = 0; s < kDecryptionSlots; ++s) key[s] = d.codeAt(s);
    guessRow(d, key);
    CHECK(d.cracked());
    CHECK(d.score() == DiskDecryption::maxScore());

    // The probe floor: a crack on the third guess is worth exactly what a crack on the
    // first is, and the fourth is the first row that costs anything. Driven by wasting
    // n attempts on all-Green (never the key without duplicates) before taking it.
    const int green[kDecryptionSlots] = {0, 0, 0};
    for (int wasted = 0; wasted < kDecryptionAttempts; ++wasted) {
        d.reset(4242, /*allowDuplicates=*/false);
        for (int s = 0; s < kDecryptionSlots; ++s) key[s] = d.codeAt(s);
        for (int i = 0; i < wasted; ++i) guessRow(d, green);
        guessRow(d, key);
        CHECK(d.cracked());
        const int expect = wasted < kDecryptionFreeAttempts
                               ? DiskDecryption::maxScore()
                               : kDecryptionAttempts - wasted;
        CHECK(d.score() == expect);
    }
    CHECK(kDecryptionFreeAttempts == 2);

    // The other end: five rows of the same wrong guess. All-Green can never be the key
    // when duplicates are barred, so this is a guaranteed lock-out.
    d.reset(4242, /*allowDuplicates=*/false);
    for (int i = 0; i < kDecryptionAttempts; ++i) guessRow(d, green);
    CHECK(!d.running() && !d.cracked());
    CHECK(d.played() == kDecryptionAttempts);
    CHECK(d.score() == 0);
}

// The duplicate rule is a property of the KEY, and it is what the hard cabinet moves.
// Barred, every key is three distinct colours; allowed, some are not.
void test_decryption_duplicate_rule_is_the_key() {
    DiskDecryption d;
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
void test_decryption_hatch_pays_the_clock() {
    { Game g;
      if (g.inLineSelect()) g.onButton(press(Button::B));
      CHECK(g.inDecryption());
      CHECK(g.bootHatchRemainMs() == kBootHatchMs);
      crackDecryption(g);
      CHECK(g.nav() == Game::Nav::Idle);
      CHECK(g.bootHatchRemainMs() == kBootHatchMs / 2);
      CHECK(g.inEggPhase());                       // halved, not hatched
      CHECK(g.hasAchievement(ach::kFirstBruteForce)); }

    { Game g;
      if (g.inLineSelect()) g.onButton(press(Button::B));
      settleDecryption(g);
      CHECK(g.nav() == Game::Nav::Idle);
      CHECK(g.bootHatchRemainMs() == kBootHatchMs);   // a lost board costs nothing
      CHECK(!g.hasAchievement(ach::kFirstBruteForce)); }
}

// The hatch board always plays by the standard rules, whatever the arcade was last set
// to — a dial on a cabinet must never reach the egg.
void test_decryption_hatch_ignores_the_arcade_dial() {
    Game g{StartMode::Hatched};
    enterArcadeCabinet(g, arcadeGameIndexById("decryption"), ArcadeDifficulty::Hard);
    g.onButton(press(Button::B));                  // start it, so the dial is applied
    CHECK(g.inDecryption());
    CHECK(g.decryption().duplicatesAllowed());
    settleDecryption(g);
    g.onButton(press(Button::B));                  // dismiss the arcade payout

    g.resetToHatch();
    if (g.inLineSelect()) g.onButton(press(Button::B));
    CHECK(g.inDecryption());
    CHECK(!g.decryption().duplicatesAllowed());      // the egg gets the standard key
}

// The arcade cabinet pays on the SCORE, so a first-guess crack takes the whole bonus
// and a lost board takes none of it — and neither touches the incubation clock, which
// a raised pet does not have.
void test_decryption_arcade_pays_on_score() {
    const int row = arcadeGameIndexById("decryption");
    CHECK(row >= 0);
    { Game g{StartMode::Hatched};
      const int bits = g.bits();
      enterArcadeCabinet(g, row, ArcadeDifficulty::Medium);
      g.onButton(press(Button::B));
      crackDecryption(g);
      CHECK(g.nav() == Game::Nav::ArcadeResult);
      CHECK(g.bits() == bits + kArcadePlayBits + kArcadeScoreBits);
      CHECK(g.arcadeWins(row) == 1);
      // The arcade never fires the hatch's achievement — there was no egg in it.
      CHECK(!g.hasAchievement(ach::kFirstBruteForce)); }

    { Game g{StartMode::Hatched};
      const int bits = g.bits();
      enterArcadeCabinet(g, row, ArcadeDifficulty::Medium);
      g.onButton(press(Button::B));
      settleDecryption(g);
      CHECK(g.nav() == Game::Nav::ArcadeResult);
      CHECK(g.bits() == bits + kArcadePlayBits);   // the attempt, and no bonus
      CHECK(g.arcadeWins(row) == 0); }
}

// Grayscale gate. Nothing is written on a cell, so LUMINANCE is the whole non-colour
// channel and it has to be a real one: the five code colours sit on an even ladder,
// every rung clear of its neighbour by a margin no screenshot can close. This is the
// gate that stops a hue being retuned for looks and quietly collapsing the board.
void test_decryption_grayscale() {
    const Pal five[kDecryptionColours] = {Pal::DECRYPTION_PURPLE, Pal::DECRYPTION_BLUE,
                                          Pal::DECRYPTION_GREEN, Pal::DECRYPTION_ORANGE,
                                          Pal::DECRYPTION_WHITE};
    float prev = 0.0f;
    for (int i = 0; i < kDecryptionColours; ++i) {
        const float lum = luminance(palColor(five[i]));
        if (i > 0) CHECK(lum - prev > 0.12f);   // strictly ascending, and by enough
        prev = lum;
    }
    CHECK(prev > 0.8f);                          // the top rung clears the page
    CHECK(luminance(palColor(five[0])) >
          luminance(palColor(Pal::PAPER)) + 0.08f);   // ...and the bottom clears PAPER

    // The rest of the board's state rides on SHAPE, not on any of that: the row being
    // built carries a cursor box and a settled slot carries a bar, and a played row
    // carries its fragmentation damage as texture over the cell.
    Framebuffer fb(kActiveW, kActiveH);
    Game g;
    if (g.inLineSelect()) g.onButton(press(Button::B));
    CHECK(g.inDecryption());
    g.render(fb);
    CHECK(anyLitGray(fb, 6, 24, 6 + 40, 24 + 34));   // the cursor box round slot 0

    for (int r = 0; r < 4; ++r)
        for (int s = 0; s < kDecryptionSlots; ++s) {
            for (int c = 0; c < r; ++c) g.onButton(press(Button::A));
            g.onButton(press(Button::B));
        }
    g.render(fb);
    CHECK(anyNonPaper(fb, 12, 30, 12 + 26, 30 + 22));   // the opening row is drawn
}
