// test_cryptogram.cpp — native gates for THE DECRYPTOGRAM: the board's rules, the
// quote pool's fit, the per-quote difficulty ratchet, and the two doors into it (the
// VAULT ticket and the arcade cabinet).
//
// One slice of the native suite; see test_gates.h for the shared fixtures and
// test_main.cpp for the roster that runs these. The save-v48 gate sits here, with the
// feature whose field it stores, not in a migrations pile of its own.
#include "test_gates.h"

#include "core/content/content_quotes.h"
#include "core/model/cryptogram.h"

namespace {

// How many distinct letters a text uses — the number every reveal count is measured
// against.
int distinctLetters(const char* s) {
    bool seen[26] = {false};
    int n = 0;
    for (const char* p = s; *p; ++p) {
        const char c = (*p >= 'a' && *p <= 'z') ? static_cast<char>(*p - 32) : *p;
        if (Cryptogram::isLetter(c) && !seen[c - 'A']) { seen[c - 'A'] = true; ++n; }
    }
    return n;
}

// Play the board correctly to the end: read the letter the cell cursor is already
// sitting on, find it in the pool, and place it there. Bounded so a rules bug ends the
// test rather than hanging it.
void solveBoard(Game& g) {
    for (int guard = 0; guard < 64 && g.cryptogram().running(); ++guard) {
        const Cryptogram& c = g.cryptogram();
        const char want = c.at(c.cellCursor());
        for (int i = 0; i < c.poolSize() && c.poolLetter(c.poolCursor()) != want; ++i)
            g.onButton(press(Button::A));
        CHECK(c.poolLetter(c.poolCursor()) == want);
        g.onButton(press(Button::B));    // take the letter
        g.onButton(press(Button::B));    // place it on the cell it came from
    }
}

// Place a letter that is NOT the one under the cursor, which is the whole of losing.
void missOnce(Game& g) {
    const Cryptogram& c = g.cryptogram();
    const char want = c.at(c.cellCursor());
    for (int i = 0; i < c.poolSize() && c.poolLetter(c.poolCursor()) == want; ++i)
        g.onButton(press(Button::A));
    CHECK(c.poolLetter(c.poolCursor()) != want);
    g.onButton(press(Button::B));
    g.onButton(press(Button::B));
}

// Bright pixels in one pool tile. A FILLED tile and an OUTLINED one differ by a lot of
// them, which is the shape channel the two pick stages are told apart by.
int litTile(const Framebuffer& fb, int x, int y) {
    int n = 0;
    for (int yy = y; yy < y + 14; ++yy)
        for (int xx = x; xx < x + 14; ++xx)
            if (luminance(fb.get(xx, yy)) > 0.12f) ++n;
    return n;
}

// Walk the hacker carousel to the VAULT with `n` Decryptograms in the bag, and cash one.
void cashADecryptogram(Game& g, int n = 1) {
    g.inventory().add("decryptogram", n);
    enterHackerSlot(g, HackerSlotId::Vault);
    // Tickets sort above caches, so the VAULT opens on one.
    g.onButton(press(Button::B));
}

}  // namespace

// EVERY QUOTE MUST FIT THE PANEL, and a character count is the wrong way to ask: the
// wrap wastes the tail of most lines, so the only honest check is to lay the text out
// and count the rows it actually took. This is the gate content_quotes.h points new
// rows at — add a quote, run this, and it either fits or it says so.
void test_cryptogram_quotes_fit_the_panel() {
    CHECK(quoteCount() > 0);
    Cryptogram c;
    std::set<int> wires;
    for (int i = 0; i < quoteCount(); ++i) {
        const QuoteDef& q = quotes()[i];
        // Wire numbers are the save's index: unique, in range, and never reused.
        CHECK(wires.insert(q.wire).second);
        CHECK(q.wire >= 0 && q.wire < kQuoteWireCap);
        CHECK(q.text && q.text[0]);
        CHECK(q.attribution && q.attribution[0]);

        // The face is an ASCII table with no curly punctuation in it, so anything
        // outside this set would render as a hole in the middle of the quote.
        for (const char* p = q.text; *p; ++p) {
            const char ch = *p;
            const bool ok = (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
                            (ch >= '0' && ch <= '9') ||
                            std::strchr(" .,;:!?'-()", ch) != nullptr;
            CHECK(ok);
        }
        // ...and the attribution rides the same face.
        for (const char* p = q.attribution; *p; ++p) CHECK(*p >= ' ' && *p < 127);

        // The board loads it, which is where the wrap and the buffer bound are both
        // enforced. Hardest opening (no extra reveals) — the one with the most closed
        // cells, so if any setting overflows the grid this one does.
        CHECK(c.reset(q.text, 0));
        CHECK(c.rowCount() <= kCryptogramRows);
        for (int k = 0; k < c.length(); ++k) {
            const uint8_t col = c.col(k);
            CHECK(col == kCryptogramHiddenCol || col < kCryptogramCols);
        }
        // A quote with fewer distinct letters than the opening reveal would open
        // itself. Three is the floor, so four distinct letters is the real minimum.
        CHECK(distinctLetters(q.text) > kCryptogramMinReveal);
    }
}

// A wall of empty rules is a guess, not a puzzle: three letters open before the first
// press, and never so many that the board hands itself over.
void test_cryptogram_opens_at_least_three_letters() {
    Cryptogram c;
    const char* text = "TALK IS CHEAP. SHOW ME THE CODE.";
    CHECK(c.reset(text, 0));
    const int distinct = distinctLetters(text);
    CHECK(c.poolSize() == distinct - kCryptogramMinReveal);
    CHECK(c.poolSize() >= 1);              // always something left to guess
    CHECK(c.state() == Cryptogram::State::Running);

    // The reveals are the COMMONEST letters, which is what makes three of them worth
    // having: E is in this quote five times and is not something the player is asked
    // to find. Frequency order, so the pool never holds the most common letter while
    // a rarer one is open.
    bool sawE = false;
    for (int i = 0; i < c.poolSize(); ++i) if (c.poolLetter(i) == 'E') sawE = true;
    CHECK(!sawE);

    // The pool is alphabetical, so a player hunting a letter knows roughly where it is.
    for (int i = 1; i < c.poolSize(); ++i) CHECK(c.poolLetter(i) > c.poolLetter(i - 1));
}

// The difficulty ratchet's whole promise: a rung down is a letter (or three) already
// open, off the same board rather than a different one.
void test_cryptogram_difficulty_opens_more() {
    const char* text = "COMPUTERS ARE USELESS. THEY CAN ONLY GIVE YOU ANSWERS.";
    Cryptogram hard, medium, easy;
    CHECK(hard.reset(text, cryptogramExtraReveals(CryptogramTier::Hard)));
    CHECK(medium.reset(text, cryptogramExtraReveals(CryptogramTier::Medium)));
    CHECK(easy.reset(text, cryptogramExtraReveals(CryptogramTier::Easy)));
    CHECK(medium.poolSize() == hard.poolSize() - 1);
    CHECK(easy.poolSize() == hard.poolSize() - 3);
    // A solved quote replayed for Bits is offered at the hard opening — the prize is
    // spent, so the puzzle is all that is left and an easy one is not that.
    CHECK(cryptogramExtraReveals(CryptogramTier::Solved) ==
          cryptogramExtraReveals(CryptogramTier::Hard));

    // Deterministic: the same quote at the same tier is the same board every time, which
    // is what lets the ratchet mean "one more letter" rather than "a different puzzle".
    Cryptogram again;
    CHECK(again.reset(text, cryptogramExtraReveals(CryptogramTier::Hard)));
    for (int i = 0; i < hard.poolSize(); ++i)
        CHECK(again.poolLetter(i) == hard.poolLetter(i));
}

// The rule that makes one good deduction worth a column: a right placement opens EVERY
// instance of that letter, not the cell it was placed in.
void test_cryptogram_right_letter_opens_every_instance() {
    Cryptogram c;
    const char* text = "TALK IS CHEAP. SHOW ME THE CODE.";
    CHECK(c.reset(text, 0));

    // Find a pool letter that appears more than once, and the first cell holding it.
    char target = '\0';
    int cell = -1, instances = 0;
    for (int p = 0; p < c.poolSize() && target == '\0'; ++p) {
        const char cand = c.poolLetter(p);
        int n = 0, first = -1;
        for (int i = 0; i < c.length(); ++i)
            if (c.at(i) == cand) { ++n; if (first < 0) first = i; }
        if (n > 1) { target = cand; cell = first; instances = n; }
    }
    CHECK(target != '\0' && instances > 1);

    while (c.poolLetter(c.poolCursor()) != target) c.cycle();
    c.accept();                                   // take the letter
    CHECK(c.stage() == Cryptogram::Stage::PickCell);
    while (c.cellCursor() != cell) c.cycle();
    const int before = c.cellsLeft();
    c.accept();                                   // place it

    CHECK(c.state() == Cryptogram::State::Running);
    CHECK(c.cellsLeft() == before - instances);   // all of them, not one
    for (int i = 0; i < c.length(); ++i)
        if (c.at(i) == target) CHECK(c.isOpen(i));
    // ...and the letter has left the pool, because the pool IS what is still owed.
    for (int i = 0; i < c.poolSize(); ++i) CHECK(c.poolLetter(i) != target);
    CHECK(c.stage() == Cryptogram::Stage::PickLetter);   // handed back for the next pick
    CHECK(c.score() == 1);
}

// One wrong placement ends the run. There is no second chance inside a board — the
// second chance is the next board, one rung easier.
void test_cryptogram_one_wrong_letter_ends_the_run() {
    Cryptogram c;
    CHECK(c.reset("TALK IS CHEAP. SHOW ME THE CODE.", 0));
    const char right = c.at(c.cellCursor());
    while (c.poolLetter(c.poolCursor()) == right) c.cycle();
    const char wrong = c.poolLetter(c.poolCursor());
    const int cell = c.cellCursor();
    c.accept();
    c.accept();

    CHECK(c.state() == Cryptogram::State::Failed);
    CHECK(!c.running() && !c.solved());
    CHECK(c.wrongLetter() == wrong && c.wrongCell() == cell);
    // The board is inert afterwards — no press can revive it or bank a late letter.
    const int left = c.cellsLeft();
    c.cycle(); c.cycleBack(); c.accept(); c.dropLetter();
    CHECK(c.state() == Cryptogram::State::Failed && c.cellsLeft() == left);
}

// BOTH cursors run both ways. This is the whole reason the chord is spent on "put the
// letter back": a board opens with thirty-odd closed cells, and a forward-only cursor
// makes overshooting by one cost a full lap of the quote.
void test_cryptogram_cursors_run_both_ways() {
    Game g{StartMode::Hatched};
    cashADecryptogram(g);
    const Cryptogram& c = g.cryptogram();
    CHECK(c.stage() == Cryptogram::Stage::PickLetter);

    // The pool: forward then back lands exactly where it started, and back from the
    // first letter wraps to the last rather than sticking.
    const int start = c.poolCursor();
    g.onButton(press(Button::A));
    CHECK(c.poolCursor() != start);
    g.onButton(press(Button::C));
    CHECK(c.poolCursor() == start);
    g.onButton(press(Button::C));
    CHECK(c.poolCursor() == c.poolSize() - 1);
    g.onButton(press(Button::A));
    CHECK(c.poolCursor() == start);

    // The cells: same contract, and every stop is a CLOSED cell in both directions —
    // a back-step that landed on an already-open one would be a stop the forward walk
    // never offers.
    g.onButton(press(Button::B));                 // take the letter
    CHECK(c.stage() == Cryptogram::Stage::PickCell);
    const int cell = c.cellCursor();
    for (int i = 0; i < 5; ++i) {
        g.onButton(press(Button::A));
        CHECK(!c.isOpen(c.cellCursor()));
    }
    for (int i = 0; i < 5; ++i) {
        g.onButton(press(Button::C));
        CHECK(!c.isOpen(c.cellCursor()));
    }
    CHECK(c.cellCursor() == cell);                // five out, five back, same gap
}

// A+C hands the letter back; nothing leaves the board. A ticket is spent the moment it
// is cashed, so an escape hatch here would be a free retry — and with A and C spent on
// the two cursor directions, the chord is the only button left to be one.
void test_cryptogram_chord_drops_the_letter_but_never_leaves() {
    Game g{StartMode::Hatched};
    cashADecryptogram(g);
    CHECK(g.nav() == Game::Nav::Cryptogram);
    CHECK(g.cryptogram().stage() == Cryptogram::Stage::PickLetter);
    g.onButton(chordAC());                        // inert at the pool, and never an exit
    CHECK(g.nav() == Game::Nav::Cryptogram);
    CHECK(g.cryptogram().stage() == Cryptogram::Stage::PickLetter);

    g.onButton(press(Button::B));                 // take a letter
    CHECK(g.cryptogram().stage() == Cryptogram::Stage::PickCell);
    g.onButton(chordAC());                        // ...and put it back
    CHECK(g.cryptogram().stage() == Cryptogram::Stage::PickLetter);
    CHECK(g.nav() == Game::Nav::Cryptogram);

    // C alone is a cursor step, never an exit — the deviation the hint band spells out.
    g.onButton(press(Button::C));
    CHECK(g.nav() == Game::Nav::Cryptogram);
}

// Holding a direction repeats it, which is what makes a thirty-cell walk bearable at
// all. Nothing fires before the delay (an ordinary tap must never trigger a run), and
// holding BOTH is the drop chord, not a repeat in some arbitrary direction.
void test_cryptogram_held_cursor_repeats() {
    Game g{StartMode::Hatched};
    cashADecryptogram(g);
    const Cryptogram& c = g.cryptogram();
    uint32_t t = 1000;
    g.tick(t);

    g.onButton(press(Button::A));                 // steps once, and arms the hold
    const int afterTap = c.poolCursor();
    g.tick(t += kCryptogramRepeatDelayMs - 50);   // still inside the delay
    CHECK(c.poolCursor() == afterTap);
    g.tick(t += 100);                             // ...past it: the first repeat lands
    CHECK(c.poolCursor() != afterTap);

    int steps = 0;
    for (int i = 0; i < 6; ++i) {
        const int was = c.poolCursor();
        g.tick(t += kCryptogramRepeatMs);
        if (c.poolCursor() != was) ++steps;
    }
    CHECK(steps == 6);                            // one step per interval, not per beat

    // Releasing stops it dead.
    g.onButton({Button::A, false, false});
    const int parked = c.poolCursor();
    for (int i = 0; i < 4; ++i) g.tick(t += kCryptogramRepeatMs);
    CHECK(c.poolCursor() == parked);
}

// The VAULT is the door: the ticket is spent there, a quote comes up unsolved, and
// solving it pays Bits plus the one account unlock.
void test_cryptogram_vault_ticket_pays_bits_and_an_upgrade() {
    Game g{StartMode::Hatched};
    const int bits = g.bits();
    const int pool = g.bandwidthMax();
    const int bought = g.bwUpgradeCount();
    CHECK(g.quotesSolved() == 0);

    cashADecryptogram(g, 2);
    CHECK(g.nav() == Game::Nav::Cryptogram);
    CHECK(g.inventory().count("decryptogram") == 1);   // spent up front
    const int quote = g.cryptogramQuote();
    CHECK(quote >= 0);
    CHECK(g.quoteTier(quote) == CryptogramTier::Hard); // every quote starts here

    solveBoard(g);
    CHECK(g.cryptogram().solved());
    CHECK(g.quoteTier(quote) == CryptogramTier::Solved);
    CHECK(g.quotesSolved() == 1);
    CHECK(g.bits() == bits + kQuoteWinBits);
    // The unlock is a free Rig Shop level, and a granted one lands exactly like a
    // bought one — the cap moves AND the live pool is topped up with it.
    CHECK(g.bwUpgradeCount() == bought + 1);
    CHECK(g.bandwidthMax() == pool + kBandwidthUpgradeStep);

    // B leaves the verdict for the VAULT it was cashed at, and cannot bank twice.
    g.onButton(press(Button::B));
    CHECK(g.nav() == Game::Nav::Submenu);
    CHECK(g.bits() == bits + kQuoteWinBits);
}

// Losing costs the ticket and pays nothing — and buys the one thing that makes that
// survivable: the same quote, one rung easier, twice, and then no further.
void test_cryptogram_losing_ratchets_the_quote_easier() {
    Game g{StartMode::Hatched};
    const int bits = g.bits();
    const int bought = g.bwUpgradeCount();

    cashADecryptogram(g, 4);
    const int quote = g.cryptogramQuote();
    const int hardPool = g.cryptogram().poolSize();
    missOnce(g);
    CHECK(!g.cryptogram().running() && !g.cryptogram().solved());
    CHECK(g.bits() == bits);                       // nothing at all
    CHECK(g.bwUpgradeCount() == bought);
    CHECK(g.quoteTier(quote) == CryptogramTier::Medium);
    g.onButton(press(Button::B));

    // Play that exact quote again, at the rung the loss bought.
    g.debugStartCryptogram(quote, cryptogramExtraReveals(g.quoteTier(quote)));
    CHECK(g.cryptogram().poolSize() == hardPool - 1);
    missOnce(g);
    CHECK(g.quoteTier(quote) == CryptogramTier::Easy);
    g.onButton(press(Button::B));

    g.debugStartCryptogram(quote, cryptogramExtraReveals(g.quoteTier(quote)));
    CHECK(g.cryptogram().poolSize() == hardPool - 3);
    missOnce(g);
    CHECK(g.quoteTier(quote) == CryptogramTier::Easy);   // Easy is the floor
}

// An achievement-gated quote is not in the pool until it is earned — the second
// eligibility axis, and the one that lets the table hold quotes worth waiting for.
void test_cryptogram_gated_quotes_wait_for_their_achievement() {
    Game g{StartMode::Hatched};
    int gated = -1;
    for (int i = 0; i < quoteCount(); ++i)
        if (quotes()[i].requiresAch) { gated = i; break; }
    CHECK(gated >= 0);                             // the table exercises the gate
    CHECK(!g.hasAchievement(quotes()[gated].requiresAch));
    CHECK(!g.quoteEligible(gated, Game::QuotePick::Unsolved));
    CHECK(!g.quoteEligible(gated, Game::QuotePick::Any));

    g.unlockAchievement(quotes()[gated].requiresAch);
    CHECK(g.quoteEligible(gated, Game::QuotePick::Unsolved));
    // Earned but unsolved is still not something the arcade may replay.
    CHECK(!g.quoteEligible(gated, Game::QuotePick::SolvedOnly));
}

// save v48: the tier ladder is the save's whole record of a quote, and it survives a
// reboot. Player-level, so it outlives the pet that was alive when it was earned.
void test_cryptogram_states_persist() {
    MemSaveStore store;
    const int solvedWire = quotes()[0].wire;
    const int lostWire = quotes()[1].wire;
    {
        Game g(StartMode::Hatched, "malbear", &store);
        g.debugSetQuoteTier(0, CryptogramTier::Solved);
        g.debugSetQuoteTier(1, CryptogramTier::Easy);
        CHECK(g.saveNow());
    }
    CHECK(!store.bytes().empty());
    {
        Game g(StartMode::Hatched, "malbear", &store);
        CHECK(g.quoteTier(quoteIndexByWire(solvedWire)) == CryptogramTier::Solved);
        CHECK(g.quoteTier(quoteIndexByWire(lostWire)) == CryptogramTier::Easy);
        CHECK(g.quotesSolved() == 1);
        // Everything else reads back as never played, which is the honest default.
        for (int i = 2; i < quoteCount(); ++i)
            CHECK(g.quoteTier(i) == CryptogramTier::Hard);
    }
    // A pre-v48 blob has no tail at all, and must load as a device that never had a
    // board rather than as a corrupt stream.
    SaveData d;
    CHECK(deserializeSave(store.bytes(), d));
    d.quoteStates.clear();
    MemSaveStore older;
    older.save(serializeSave(d));
    Game g(StartMode::Hatched, "malbear", &older);
    CHECK(g.quotesSolved() == 0);
}

// The cabinet replays a SOLVED quote, so it has nothing to show until there is a back
// catalogue: absent from the GAMES list, not greyed in it.
void test_cryptogram_arcade_cabinet_waits_for_eight_wins() {
    Game g{StartMode::Hatched};
    const int row = arcadeGameIndexById("cryptogram");
    CHECK(row >= 0);
    CHECK(!g.arcadeCabinetOffered(row));

    int rows[kArcadeMaxCabinets] = {0};
    int n = g.arcadeOfferedRows(rows, kArcadeMaxCabinets);
    CHECK(n == arcadeGameCount() - 1);
    for (int i = 0; i < n; ++i) CHECK(rows[i] != row);

    // The A-cycle skips it too, so the cursor can never rest on a row nobody drew —
    // walked past its position twice over, and B never opens it.
    enterSubmenuId(g, SubmenuId::Games);
    for (int i = 0; i < 2 * arcadeGameCount(); ++i) {
        CHECK(g.arcadeRow() != row);
        g.onButton(press(Button::A));
    }
    CHECK(g.arcadeRow() != row);

    for (int i = 0; i < kQuoteArcadeUnlockWins; ++i)
        g.debugSetQuoteTier(i, CryptogramTier::Solved);
    CHECK(g.quotesSolved() == kQuoteArcadeUnlockWins);
    CHECK(g.arcadeCabinetOffered(row));
    n = g.arcadeOfferedRows(rows, kArcadeMaxCabinets);
    CHECK(n == arcadeGameCount());
}

// Unlocked, the cabinet is a re-run: only quotes already won, no tier moved, no unlock
// paid — the arcade's flat Bits and nothing else.
void test_cryptogram_arcade_replays_a_solved_quote_for_bits() {
    Game g{StartMode::Hatched};
    const int row = arcadeGameIndexById("cryptogram");
    for (int i = 0; i < kQuoteArcadeUnlockWins; ++i)
        g.debugSetQuoteTier(i, CryptogramTier::Solved);
    const int bits = g.bits();
    const int bought = g.bwUpgradeCount();

    enterArcadeCabinet(g, row, ArcadeDifficulty::Medium);
    g.onButton(press(Button::B));                  // START
    CHECK(g.nav() == Game::Nav::Cryptogram);
    CHECK(g.inArcadeRun());
    const int quote = g.cryptogramQuote();
    CHECK(quote >= 0 && g.quoteTier(quote) == CryptogramTier::Solved);

    solveBoard(g);
    CHECK(g.cryptogram().solved());
    CHECK(g.bits() == bits);                       // the till pays on the way out
    g.onButton(press(Button::B));
    CHECK(g.nav() == Game::Nav::ArcadeResult);
    CHECK(g.bits() > bits);
    CHECK(g.bwUpgradeCount() == bought);           // never a second unlock
    CHECK(g.quotesSolved() == kQuoteArcadeUnlockWins);   // and never a new solve
    CHECK(g.arcadeWins(row) == 1);
}

// Nothing on this board is said in colour alone: the empty rule, the cell cursor, the
// focused pool tile and the misplacement are all SHAPE, so a grayscale screenshot of
// any of them still reads.
void test_cryptogram_grayscale() {
    Game g{StartMode::Hatched};
    cashADecryptogram(g);
    Framebuffer fb(kActiveW, kActiveH);
    g.render(fb);
    // The quote grid and the pool are both drawn, in their own bands.
    CHECK(anyLitGray(fb, 8, 30, 216, 30 + 5 * 15));
    CHECK(anyLitGray(fb, 8, 156, 216, 156 + 14));

    // Taking a letter changes the pool tile from FILLED to OUTLINED — a shape change,
    // which is what has to be true for the two stages to be distinguishable in gray.
    const int lit0 = litTile(fb, 8, 156);
    g.onButton(press(Button::B));
    g.render(fb);
    CHECK(g.cryptogram().stage() == Cryptogram::Stage::PickCell);
    CHECK(litTile(fb, 8, 156) != lit0);

    // ...and the cell cursor is a solid bar in the grid, over and above the dim rules.
    CHECK(anyLitGray(fb, 8, 30, 216, 30 + 5 * 15));
}
