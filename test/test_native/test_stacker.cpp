// test_stacker.cpp — native gates for the defrag Stacker minigame and the Replication Ghost.
//
// One slice of the native suite; see test_gates.h for the shared fixtures and
// test_main.cpp for the roster that runs these. A save-vNN gate sits with the
// feature whose field it migrates, not in a migrations pile of its own.
#include "test_gates.h"

// How many blocks are locked in a row.
int stackerRowCount(const Stacker& s, int r) {
    int n = 0;
    for (int c = 0; c < kStackerCols; ++c) if (s.locked(r, c)) ++n;
    return n;
}

// Slide the run until its left edge reaches `col`, then lock it. Returns false if the
// column is unreachable, which would make the test a no-op rather than a failure.
bool stackerDropAt(Stacker& s, int col) {
    for (int guard = 0; guard < 4 * kStackerCols; ++guard) {
        if (s.left() == col) { s.drop(); return true; }
        s.step();
    }
    return false;
}

void test_stacker_shaves_the_overhang() {
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

void test_stacker_missing_entirely_loses() {
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

void test_stacker_clearing_every_row_wins() {
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
void test_stacker_run_stays_on_board_and_contiguous() {
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
void test_stacker_row_width_reports_the_survivors() {
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
void test_stacker_score_pays_for_height_twice() {
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

// The two rows a board's SHAPE earns, and the tally underneath them. A perfect board and
// a one-block board are the two ends of the same measurement, so they are asserted
// against each other rather than one at a time — reading the wrong end of it is the
// mistake worth catching.
void test_stacker_win_credits_the_tally_and_the_shape_rows() {
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
void test_stacker_short_board_pays_what_it_stacked_and_costs_nothing() {
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
void test_stacker_cleared_board_wipes_the_disk() {
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
void test_stacker_stopping_early_banks_the_board() {
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
    tapC(g);
    CHECK(g.model().fragmentation() == 70 - worth);
    CHECK(g.model().careMistakes() == 0);
    CHECK(g.stackerWins() == 0);
}

// The counting ladder rides the ordinary sweep, like every other counted series, and the
// tally is PLAYER-level: it has to survive the pet that set it.
void test_stacker_wins_ladder_sweeps_and_persists() {
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
void test_rolled_defrag_takes_its_fixed_bite() {
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
void test_replication_ghost_is_raised_by_a_failed_defrag_on_a_critical_disk() {
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
void test_air_gapped_snack_cures_the_ghost_and_unlocks() {
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
void test_ghost_also_clears_through_a_successful_av_scan() {
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
void test_perishable_food_spoils_on_a_feeding() {
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
void test_icon_tint_and_theme_indirection() {
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
void test_renamed_ids_table_invariants() {
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

void test_save_v39_raised_tally_roundtrip_and_migration() {
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
void test_full_pedia_achievement_reads_the_raised_tally() {
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
