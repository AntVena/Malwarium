// test_listnav.cpp — the list-navigation contract every screen shares
// (src/core/app/game/game_listnav.cpp): A steps forward and repeats when held, C
// cancels on a tap and walks BACKWARD when held, and the row set each one walks is
// whatever the focused screen draws.
//
// The gates here drive the real input path — press edges, ticks, release edges — because
// the whole contract lives in the gap between them: what a press means is decided by how
// long it stays down, and a gate that only ever sends press edges cannot see that.
#include "test_gates.h"

#include "core/ui/layout.h"   // kMargin — the row budget these gates mirror

// A held A on a scrolling list steps once on the press and then REPEATS, so crossing a
// long bag costs a dwell rather than a tap per row. The repeat is bounded by the
// list — it wraps rather than running off the end — and stops dead on release.
void test_list_hold_a_repeats_the_step() {
    Game g{StartMode::Hatched};
    g.inventory().add("tortilla_chip", 1);
    g.inventory().add("airgap_snack", 1);
    g.inventory().add("polltatoes", 1);
    enterSubmenuId(g, SubmenuId::Items);
    uint32_t t = 0;
    g.tick(t);
    const int start = g.listRow();

    // A press alone steps exactly one row — the repeat has not begun.
    g.onButton(press(Button::A));
    const int afterPress = g.listRow();
    CHECK(afterPress != start);
    g.tick(t += kListRepeatDelayMs / 2);         // still inside the tap window
    CHECK(g.listRow() == afterPress);            // ...so nothing has repeated yet

    // Past the delay the same held press keeps stepping, one row per interval.
    g.tick(t += kListRepeatDelayMs);
    const int afterFirstRepeat = g.listRow();
    CHECK(afterFirstRepeat != afterPress);
    g.tick(t += kListRepeatMs);
    CHECK(g.listRow() != afterFirstRepeat);

    // Release stops it: further ticks move nothing.
    g.onButton(lift(Button::A));
    const int parked = g.listRow();
    g.tick(t += 10 * kListRepeatMs);
    CHECK(g.listRow() == parked);
}

// C keeps its tap — Cancel is the one button whose meaning never bends — and puts the
// backward step on the HOLD. A tap leaves the list; a hold walks the cursor back up it
// and leaves nothing on release.
void test_list_hold_c_steps_back_tap_cancels() {
    // (a) A tap of C still backs out of the submenu, exactly as it always has.
    {
        Game g{StartMode::Hatched};
        enterSubmenuId(g, SubmenuId::Items);
        CHECK(g.nav() == Game::Nav::Submenu);
        tapC(g);
        CHECK(g.nav() == Game::Nav::Cursor);
    }
    // (b) A held C walks the cursor BACKWARD instead, and does not cancel on release.
    {
        Game g{StartMode::Hatched};
        g.inventory().add("tortilla_chip", 1);
        g.inventory().add("airgap_snack", 1);
        g.inventory().add("polltatoes", 1);
        enterSubmenuId(g, SubmenuId::Items);
        uint32_t t = 0;
        g.tick(t);
        g.onButton(press(Button::A));               // forward two rows
        g.onButton(lift(Button::A));
        g.onButton(press(Button::A));
        g.onButton(lift(Button::A));
        const int forward = g.listRow();

        g.onButton(press(Button::C));               // arm — nothing happens yet
        CHECK(g.nav() == Game::Nav::Submenu);       // in particular, no cancel
        CHECK(g.listRow() == forward);
        g.tick(t += kListRepeatDelayMs + kHeartbeatMs);   // cross into the hold
        CHECK(g.listRow() != forward);              // ...and it stepped BACK
        const int back = g.listRow();
        g.onButton(lift(Button::C));                // release after a step: no cancel
        CHECK(g.nav() == Game::Nav::Submenu);
        CHECK(g.listRow() == back);
    }
    // (c) One press each way is a round trip — the overshoot this exists to undo.
    {
        Game g{StartMode::Hatched};
        g.inventory().add("tortilla_chip", 1);
        g.inventory().add("airgap_snack", 1);
        enterSubmenuId(g, SubmenuId::Items);
        uint32_t t = 0;
        g.tick(t);
        const int start = g.listRow();
        g.onButton(press(Button::A));
        g.onButton(lift(Button::A));
        CHECK(g.listRow() != start);
        g.onButton(press(Button::C));
        g.tick(t += kListRepeatDelayMs + kHeartbeatMs);
        g.onButton(lift(Button::C));
        CHECK(g.listRow() == start);                // back where it started
    }
}

// The contract is scoped to LISTS. On a screen with no rows to walk — an inline
// confirm, a detail page — C is the plain instant cancel it has always been, and a
// held A repeats nothing.
void test_list_contract_skips_non_lists() {
    Game g{StartMode::Hatched};
    g.inventory().add("tortilla_chip", 2);
    enterSubmenuId(g, SubmenuId::Items);
    g.onButton(press(Button::B));                   // into the item DETAIL page
    CHECK(g.nav() == Game::Nav::Detail);

    // C cancels on the PRESS edge here — no release needed, no dwell.
    g.onButton(press(Button::C));
    CHECK(g.nav() == Game::Nav::Submenu);

    // And a detail page's A never repeats: park on one and hold A through the window.
    g.onButton(press(Button::B));
    CHECK(g.nav() == Game::Nav::Detail);
    uint32_t t = 0;
    g.tick(t);
    g.onButton(press(Button::A));
    g.tick(t += kListRepeatDelayMs + 20 * kListRepeatMs);
    CHECK(g.nav() == Game::Nav::Detail);            // still the same page
}

// The MOD picker orders its rows fittable-first: everything the pet can put in this
// slot right now comes before everything it can't, so a grown pool never buries the
// usable half. Nothing is dropped — the locked rows are still all there, after.
void test_mod_picker_orders_fittable_first() {
    ContentRegistry r = ContentRegistry::embedded();
    Loadout load;
    for (const ModDef* m : r.allMods()) load.grant(m->id, 1);

    // A level-1 pet can field only the mods whose rolled gate it has reached.
    const auto rows = ownedModList(r, load, /*petLevel=*/1, /*petLine=*/nullptr,
                                   /*slot=*/0);
    CHECK(rows.size() == r.allMods().size());       // nothing hidden

    bool seenLocked = false;
    int fittable = 0, locked = 0;
    for (const ModDef* m : rows) {
        const bool isLocked = modLockedFor(*m, 1, nullptr, load, 0);
        if (isLocked) { seenLocked = true; ++locked; }
        else { CHECK(!seenLocked); ++fittable; }    // no fittable row after a locked one
    }
    CHECK(fittable > 0 && locked > 0);              // the gate is actually separating

    // Within each half the mod table's own order survives (std::stable_sort): a pool
    // that reshuffled as the pet levelled would move rows out from under a player for
    // no reason they could see. Checked as relative order against the table, since
    // which half a mod lands in is exactly what levelling is allowed to change.
    const auto& table = r.allMods();
    auto tableIndex = [&](const ModDef* m) {
        for (size_t i = 0; i < table.size(); ++i) if (table[i] == m) return (int)i;
        return -1;
    };
    int lastFittable = -1, lastLocked = -1;
    for (const ModDef* m : rows) {
        const int at = tableIndex(m);
        CHECK(at >= 0);
        int& last = modLockedFor(*m, 1, nullptr, load, 0) ? lastLocked : lastFittable;
        CHECK(at > last);
        last = at;
    }
}

// The cursor and the rows must agree on the order. Both callers pass the same five
// arguments, so equipping from the picker equips the mod the row under the cursor
// NAMES — the failure this guards is silent and picks a different mod.
void test_mod_picker_cursor_matches_drawn_order() {
    Game g{StartMode::Hatched};
    ContentRegistry r = ContentRegistry::embedded();
    for (const ModDef* m : r.allMods()) g.debugLoadout().grant(m->id, 1);

    const auto rows = ownedModList(r, g.loadout(), g.combatLevel(),
                                   g.pet() ? g.pet()->line : nullptr, /*slot=*/0);
    CHECK(!rows.empty());
    CHECK(!modLockedFor(*rows[0], g.combatLevel(),
                        g.pet() ? g.pet()->line : nullptr, g.loadout(), 0));

    enterLoadoutTab(g, 0);                          // MODS slot list
    g.onButton(press(Button::B));                   // open slot 0's picker
    CHECK(g.nav() == Game::Nav::Detail);
    g.onButton(press(Button::B));                   // drill into the focused row
    CHECK(g.modDetailId() != nullptr);
    CHECK(std::strcmp(g.modDetailId(), rows[0]->id) == 0);   // the row the cursor drew
}

// Row text can never print through the column beside it. Both pickers hand their name
// a measured budget (drawLabelValue) rather than the full row width, so the guard that
// matters is that the budget is never squeezed to nothing by an over-wide tag: every
// shipped mod and move must leave its name room to say something.
void test_list_rows_leave_room_for_the_name() {
    ContentRegistry r = ContentRegistry::embedded();
    // The picker rows start their name at x=22 and right-align the tag at
    // kActiveW - kMargin, with a margin's gap between the two (drawLabelValue).
    constexpr int kNameX = 22;
    constexpr int kMinNameChars = 4;

    for (const ModDef* m : r.allMods()) {
        // The widest thing that column ever holds for a mod: its effect tag, or the
        // "SLOT n" the one-slot-per-pet gate swaps in.
        const int tagW = std::max(textWidth(m->effectTag), textWidth("SLOT 4"));
        const int room = kActiveW - kMargin - tagW - kMargin - kNameX;
        CHECK(room >= kMinNameChars * kFontAdvance);
    }
    for (const MoveDef* m : r.allMoves()) {
        // Widest for a move: the EVO-{stage} gate tag, the ON + kind pair, or a SLOT n.
        int tagW = std::max(textWidth("SLOT 4"), textWidth(moveKindTag(m->kind)));
        char evo[20];
        std::snprintf(evo, sizeof(evo), "EVO %s", stageName(m->minStage));
        tagW = std::max(tagW, textWidth(evo));
        char on[20];
        std::snprintf(on, sizeof(on), "ON %s", moveKindTag(m->kind));
        tagW = std::max(tagW, textWidth(on));
        const int room = kActiveW - kMargin - tagW - kMargin - kNameX;
        CHECK(room >= kMinNameChars * kFontAdvance);
    }
}
