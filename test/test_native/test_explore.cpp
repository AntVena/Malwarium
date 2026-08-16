// test_explore.cpp — native gates for EXPL: the nested ladder, walking, encounters and boss rounds.
//
// One slice of the native suite; see test_gates.h for the shared fixtures and
// test_main.cpp for the roster that runs these. A save-vNN gate sits with the
// feature whose field it migrates, not in a migrations pile of its own.
#include "test_gates.h"

// battle fatigue is a per-fight FRAGMENTATION tax (no separate meter).
//   (1) hands-off auto-explore PAUSES once frag hits the danger gate, and resumes once
//       a defrag drops it back below (the mode stays armed throughout);
//   (2) resolved wild non-boss fights accrue Fragmentation (the pet degrades from
//       fighting), so endless farming forces a defrag.
void test_battle_fatigue() {
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
                case Game::Nav::Shop: tapC(g); break;
                case Game::Nav::ModShop: tapC(g); break;
                case Game::Nav::Idle:
                    if (g.exploreActive()) pingExplore(g);   // fire the next event
                    else guard = 400;                        // explore ended (a loss) → stop
                    break;
                default: tapC(g); break;  // dismiss in-place events
            }
        }
        CHECK(wilds >= 3);                               // actually fought several wilds
        CHECK(g.model().fragmentation() > frag0);        // battle fatigue accrued frag
    }
}

// hands-free auto-step: the mode steps ITSELF on a timer from the idle
// habitat — no button mashing. Ticking ~kWalkAutoStepBeats heartbeats fires a
// guaranteed event hands-free (here the deterministic first roll leaves Idle).
void test_explore_autosteps_hands_free() {
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
void test_explore_every_step_is_an_event() {
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
                case Game::Nav::Encounter: tapC(g); break;  // flee
                case Game::Nav::Wifi:      g.onButton(press(Button::B)); break;
                case Game::Nav::Shop:      tapC(g); break;
                case Game::Nav::ModShop:      tapC(g); break;
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
void test_encounter_fight_live_combat_win() {
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
void test_explore_auto_continues_after_fight() {
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
void test_post_encounter_reports_bandwidth_shield() {
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
    tapC(g);
    CHECK(g.nav() == Game::Nav::Idle);
    CHECK(g.exploreActive());
}

// With Bandwidth drained to 0, a farm win is UNSHIELDED — the fatigue tax
// can land (it's a chance roll, so this samples several fights). When it lands the
// overlay reports the exact +n rise; the shield flag stays off throughout, and
// bandwidth (already 0) never reads as spent again ("nothing to spend" case).
void test_post_encounter_reports_frag_rise_when_unshielded() {
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
void test_post_encounter_never_for_sim_battle() {
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
void test_post_encounter_level_line_renders() {
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
void test_expl_sector_linear_gating() {
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

// Every EXPL name and storefront banner is drawn through drawTextMarquee (widgets.h),
// so one that outgrows the column beside its right-aligned tag SCROLLS rather than
// drawing over it. That is what lets the world keep names like UNINSTALL UNDERTOW at
// FONT_UI's 8px advance instead of renaming the ladder to fit a font.
//
// What still has to hold is that the overflow stays scrollable. A line more than twice
// its column takes a cycle longer than anyone will watch it for, and a name that long
// is a naming problem the marquee would only be hiding. Budgets are measured through
// the renderers' own metrics so they can't drift from what ships.
// A hint band is the one line on a screen that CANNOT yield: it is drawn centred in a
// full-width strip, nothing scrolls it, and a band wider than the canvas loses whichever
// control sits at the ends — silently, and only in the state that draws that band. So
// every band's copy is measured here rather than discovered on the panel.
void test_hint_bands_fit_the_canvas() {
    static const char* const kBands[] = {
        "B CONTINUE", "A CYCLE B COMMIT C CANCEL", "A+C CMD B STAT C RUN A SKIP",
        "A ZONE  B ENTER  C BACK", "B - OPEN  C - BACK", "B SELECT   C DISABLED",
        "A NEXT", "B SET  C BACK", "B BUY   C LEAVE", "B APPLIES",
    };
    for (const char* b : kBands) CHECK(textWidth(b) <= kActiveW);
}

void test_expl_names_stay_scrollable() {
    // Every EXPL row is a TITLE line — name at kTextX, state tag right-aligned against
    // the 8px margin with a margin's gap — over a DETAIL line running the full width
    // from the same x. Each name is budgeted against the widest tag ITS OWN row pairs
    // with, then allowed twice that before it counts as unscrollable.
    const int titleX = 8 + 20 + 2, margin = 8;      // expl_screen's kTextX
    const int subNameW  = kActiveW - margin - (textWidth("EXPLORING") + margin) - titleX;
    const int areaNameW = kActiveW - margin - (textWidth("LOCKED") + margin) - titleX;
    const int bossNameW = kActiveW - margin - (textWidth("> BOSS") + margin) - titleX;
    const int detailW   = kActiveW - margin - titleX;
    // The breadcrumb header inside an area: "EXPL", the cursor triangle, then the area
    // name at a fixed offset past both.
    const int crumbX = margin + textWidth("EXPL") + 16;
    for (int s = 0; s < kExplSectors; ++s) {
        CHECK(textWidth(explSectorName(s)) <= 2 * areaNameW);         // top-level zone row
        CHECK(textWidth(explSectorName(s)) <= 2 * (kActiveW - margin - crumbX));
        CHECK(textWidth(area(s).areaBossName) <= 2 * bossNameW);      // the area-gauntlet row
        // The Title a cleared area grants rides that same row's DETAIL line, as do the
        // sub-boss names on a boss-ready sub-area row.
        CHECK(textWidth("TITLE: ") + textWidth(sectorTitle(s)) <= 2 * detailW);
        for (int i = 0; i < kExplSubAreas; ++i) {
            CHECK(textWidth(explSubAreaName(s, i)) <= 2 * subNameW);
            // The BANNER is what the row advertises; the ROUND names are what the fight
            // announces once it starts. Both are boss names a player reads, so both are
            // budgeted here — an escort authored wider than its banner would otherwise
            // ship unchecked.
            const SubBossDef& b = area(s).subBosses[i];
            CHECK(textWidth("BOSS: ") + textWidth(b.name) <= 2 * detailW);
            for (int r = 0; r < b.roundCount(); ++r)
                CHECK(textWidth("BOSS: ") + textWidth(b.round(r).name) <= 2 * detailW);
        }
        // Storefront headers: drawn at the left margin, with the Bits wallet right-
        // aligned opposite. Budget the wallet at a 6-figure purse plus its unit.
        const int storeNameW = kActiveW - 2 * margin - textWidth("999999 B");
        CHECK(textWidth(shopName(s)) <= 2 * storeNameW);
        CHECK(textWidth(modShopName(s)) <= 2 * storeNameW);
    }
}

// THE LEVEL IS THE LIST (explRowInLevel): the TOP level draws the two special rows
// (the DeepWeb Dive above the ladder, ROCK THE DOCK below it) plus one row per AREA and
// none of the sub-areas; inside an area it draws that area's own block and nothing
// else. That is what keeps the drawn list ~7 rows however long the ladder grows,
// instead of a 13-row window over the whole thing.
void test_expl_level_scoped_rows() {
    int top = 0;
    for (int r = 0; r < explRowCount(); ++r) {
        if (!explRowInLevel(r, -1)) continue;
        ++top;
        CHECK(explRowIsSpecial(r) || explRowSub(r) < 0);   // zones only, no sub-areas
    }
    CHECK(top == kExplLeadRows + kExplSectors + kExplTailRows);
    for (int a = 0; a < kExplSectors; ++a) {
        int inside = 0;
        for (int r = 0; r < explRowCount(); ++r) {
            if (!explRowInLevel(r, a)) continue;
            ++inside;
            // Its own block only — not a neighbour's rows, and neither special row.
            CHECK(!explRowIsSpecial(r) && explRowArea(r) == a);
        }
        CHECK(inside == 1 + kExplSubAreas);                 // boss row + its sub-areas
    }
}

// Combat::begin's carry-health (the gauntlet no-heal contract): a round that
// carries a wounded Health in starts there, not at full; a fresh round (-1) starts
// full; the carry is clamped to [1, maxHealth].
void test_combat_carry_health() {
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
void test_bits_reward_bounds() {
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
    // This helper drives the sub-boss (debugFightSubBoss) and the area boss (the
    // caller's own loop) by hand, one fight at a time — auto-progress defaulting ON
    // would otherwise auto-launch each boss itself the instant a streak/clear crosses
    // its threshold, out from under the manual trigger this helper is mid-sequencing.
    g.debugSetAutoProgress(false);
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
            case Game::Nav::Shop: tapC(g); break;
            case Game::Nav::ModShop: tapC(g); break;
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
            case Game::Nav::ModShop: tapC(g); break;
            default: g.onButton(press(Button::B)); break;
        }
    }
}

// AUTO-PROGRESS steps the ladder POSITIONALLY — the next rung by index, never "the next
// unbeaten one". That is what lets a finished ladder keep rotating, and it means an area
// spliced into the middle of kAreaList joins the rotation with no rule to update.
void test_auto_progress_steps_positionally() {
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
void test_auto_progress_gauntlet_rolls_to_next_area() {
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
void test_explore_streak_unlocks_boss_then_clears() {
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
void test_expl_nested_row_helpers() {
    // The DeepWeb Dive leads, the ladder follows offset by +1, and the arena trails.
    CHECK(explRowCount() == 1 + kExplSectors * (1 + kExplSubAreas) + 1);
    constexpr int kDwRow = 0;                                    // the DeepWeb Dive row
    const int kArenaRow = explRowCount() - 1;                    // ROCK THE DOCK's row
    CHECK(explRowIsDeepWeb(kDwRow) && !explRowIsDeepWeb(1));
    CHECK(explRowIsTourney(kArenaRow) && !explRowIsTourney(kDwRow));
    CHECK(explRowIsSpecial(kDwRow) && explRowIsSpecial(kArenaRow));
    CHECK(!explRowIsSpecial(1));                                 // area 0's header
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
// order, with Health climbing to the signature apex (round 5). Five stages whatever
// escorts a sub-area has grown: the finale fights each BOSS, under its banner, not that
// boss's own supporting cast.
void test_area_boss_gauntlet_composition() {
    for (int a = 0; a < kExplSectors; ++a) {
        BossGauntlet gg = areaBoss(a);
        CHECK(static_cast<int>(gg.rounds.size()) == kExplSubAreas);   // 5 stages
        CHECK(gg.stageRank >= 2);
        for (int s = 0; s < kExplSubAreas; ++s)
            CHECK(std::strcmp(gg.rounds[s].name, area(a).subBosses[s].name) == 0);
        CHECK(gg.rounds[kExplSubAreas - 1].maxHealth > gg.rounds[0].maxHealth);  // apex
    }
}

// A SUB-AREA boss is fought as the rounds its own row spells out. The default is one
// round carrying the banner — and that round must stay identical to what a boss was
// before rounds existed, since every unauthored rung is balanced around it. A row that
// DOES author rounds gets them in order, with SubBossRound::rung read as a depth delta:
// an escort is the same fight a rung shallower, so it is strictly the weaker enemy.
void test_sub_boss_rounds_and_escorts() {
    int plain = 0, gauntlets = 0;
    for (int a = 0; a < kExplSectors; ++a)
        for (int s = 0; s < kExplSubAreas; ++s) {
            const SubBossDef& b = area(a).subBosses[s];
            const BossGauntlet g = subAreaBoss(a, s);
            CHECK(static_cast<int>(g.rounds.size()) == b.roundCount());
            CHECK(std::strcmp(g.name, b.name) == 0);          // the banner is the gauntlet's
            if (b.roundCount() == 1) {
                ++plain;
                CHECK(std::strcmp(g.rounds[0].name, b.name) == 0);
                // The one round IS the area-boss round for this rung — the shared shape
                // both paths build from, so a drift between them can't hide.
                CHECK(g.rounds[0].maxHealth == areaBoss(a).rounds[s].maxHealth);
                continue;
            }
            ++gauntlets;
            for (int r = 0; r < b.roundCount(); ++r) {
                CHECK(std::strcmp(g.rounds[r].name, b.round(r).name) == 0);
                // A negative rung is strictly the softer fight AT EVERY RUNG, including
                // sub 0 — a doorway boss's escort has to reach below the area's own first
                // sub-area to be an escort at all, and flooring there would make the
                // authored `-1` a silent no-op.
                if (b.round(r).rung < 0)
                    CHECK(g.rounds[r].maxHealth < areaBoss(a).rounds[s].maxHealth);
                CHECK(g.rounds[r].maxHealth > 0);
            }
        }
    // Both shapes are represented, or the branches above prove nothing.
    CHECK(plain > 0);
    CHECK(gauntlets > 0);
}

// EVERY GENERIC MOVE IS CARRIED BY SOMEBODY. A drop is drawn from the defeated enemy's
// KIT (Game::rollEnemyMoveDrop), so a generic move no enemy carries is not "rare" — it is
// unreachable, and nothing about the row says so. Two shipped braces were exactly that
// until the boss pool placed them. This walks the whole enemy population and fails on the
// next one, which is the only thing that makes authoring a move safe.
//
// LINE moves are exempt and not accidentally so: a hatch owns its whole line kit
// (MoveLoadout::startingForLine), so they need no enemy to teach them.
void test_every_generic_move_is_carried() {
    ContentRegistry reg = ContentRegistry::embedded();
    std::vector<const char*> kits;
    auto swallow = [&](const BossGauntlet& g) {
        for (const CombatEnemy& e : g.rounds)
            for (const char* m : e.moveIds) kits.push_back(m);
    };
    for (int a = 0; a < kExplSectors; ++a) {
        for (int s = 0; s < kExplSubAreas; ++s) swallow(subAreaBoss(a, s));
        swallow(areaBoss(a));
    }
    // The wild pool, both variants — and at every RUNG, because applyWildSubAreaRamp
    // REPLACES a wild's kit with the depth ladder rather than adding to it. Two moves
    // (buffer_overflow, rootkit_strike) live only on that ladder's deep rungs, so walking
    // the unramped base kit alone would call them orphans.
    for (int tier = 1; tier <= 3; ++tier)
        for (uint32_t v = 0; v < 2; ++v)
            for (int a = 0; a < kExplSectors; ++a)
                for (int s = 0; s < kExplSubAreas; ++s) {
                    CombatEnemy w = wildMalbeast(tier, v);
                    applyWildSubAreaRamp(w, a, s);
                    for (const char* m : w.moveIds) kits.push_back(m);
                }
    auto carried = [&](const char* id) {
        for (const char* m : kits)
            if (std::strcmp(m, id) == 0) return true;
        return false;
    };
    int checked = 0;
    for (int i = 0; i < kMovesCount; ++i) {
        const MoveDef& m = kMoves[i];
        if (m.line) continue;                        // a line kit is granted at hatch
        // The innate jab sits outside the owned pool entirely — rollEnemyMoveDrop skips
        // it by name, so carrying it teaches nobody anything.
        if (std::strcmp(m.id, "quick_jab") == 0) continue;
        ++checked;
        CHECK(carried(m.id));
    }
    CHECK(checked > 20);                             // guard against a vacuous pass

    // Every id a boss NAMES must resolve — a typo'd `teaches` entry would otherwise be a
    // move that exists in the table and can never be found, the same bug from the other
    // end. And no kit may hold two Defend moves: chooseMove is uniform, so a second brace
    // is a boss spending half its turns bracing rather than fighting.
    for (int a = 0; a < kExplSectors; ++a) {
        if (const char* am = area(a).areaBossMoveId) {
            CHECK(reg.move(am) != nullptr);
            // The banner's own move costs the WHOLE gauntlet: it rides the final round and
            // nowhere else, so it can't be farmed off the sub-area that happens to sit last.
            const BossGauntlet g = areaBoss(a);
            auto holds = [&](const CombatEnemy& e) {
                for (const char* id : e.moveIds)
                    if (std::strcmp(id, am) == 0) return true;
                return false;
            };
            CHECK(holds(g.rounds[kExplSubAreas - 1]));
            for (int r = 0; r < kExplSubAreas - 1; ++r) CHECK(!holds(g.rounds[r]));
            CHECK(!holds(subAreaBoss(a, kExplSubAreas - 1).rounds[0]));
        }
        for (int s = 0; s < kExplSubAreas; ++s) {
            for (const char* id : area(a).subBosses[s].teaches)
                if (id) CHECK(reg.move(id) != nullptr);
            for (const CombatEnemy& e : subAreaBoss(a, s).rounds) {
                int braces = 0;
                for (const char* id : e.moveIds)
                    if (const MoveDef* m = reg.move(id))
                        if (m->kind == MoveDef::Kind::Defend) ++braces;
                CHECK(braces <= 1);
            }
        }
    }
}

// A THREAT rider is declared on the area's OWN row (AreaDef::apexThreatMoveId) and
// reaches a player on exactly one enemy: that area's SIGNATURE sub-boss (sub 4), where it
// is telegraphed and matched by the counter-mod that area's loot table pays out. Walked
// off the ladder rather than named per index, so splicing an area in carries every rider
// with it instead of failing here.
void test_boss_threat_moves_area_adjacent() {
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

// The apex riders are the only thing a boss knows that nothing else does, and until the
// move drop reached the boss path they were unobtainable — a boss round returns via
// finishBossRound() before applyCombatResult's drop block ever runs. Beating Pirate
// Bayou's signature sub-boss (the one carrying `system_hang`) enough times must
// eventually teach it, which is the re-run incentive the mode was missing.
//
// Re-fought rather than cleared once: the roll is kBossMoveDropPct and its kit holds
// three moves a Ransomware pet lacks, so any single win is a minority chance. It
// converges fast because learning one narrows the pool for the next.
void test_boss_teaches_its_own_apex_move() {
    const int sig = kExplSubAreas - 1;
    const char* rider = area(1).apexThreatMoveId;      // Pirate Bayou -> system_hang
    CHECK(rider && std::strcmp(rider, "system_hang") == 0);

    Game g{StartMode::Hatched, "bruinforce"};
    g.debugAddCombatXp(600000);                        // level hard: the boss is winnable
    g.debugSetAutoProgress(false);
    CHECK(!g.moveLoadout().owns(rider));               // unobtainable at the start

    uint32_t t = 0;
    for (int fight = 0; fight < 40 && !g.moveLoadout().owns(rider); ++fight) {
        g.debugFightSubBoss(1, sig);
        if (g.nav() != Game::Nav::Combat) break;
        for (int i = 0; i < 800 && g.nav() == Game::Nav::Combat; ++i) {
            for (int j = 0; j < 800 &&
                    g.combat().outcome() == Combat::Outcome::Ongoing; ++j)
                g.tick(t += kHeartbeatMs);
            g.onButton(press(Button::B));              // advance the round / dismiss
        }
    }
    CHECK(g.moveLoadout().owns(rider));                // ...and now it is a reward
}

// Two-level nested-list nav: the TOP level lands on the DeepWeb
// row + area HEADERS; B on an area DRILLS in; INSIDE, A cycles that area's subs (+ the
// boss-ready header) and B acts (arm / FIGHT BOSS / AREA BOSS); C pops back to the area
// list. The rendered page is unchanged — only the traversal is two-level.
void test_expl_nested_list_nav() {
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
      tapC(g);                 // C → back to the TOP area list
      CHECK(g.nav() == Game::Nav::Submenu);         // still in EXPL, not the carousel
      tapC(g);                 // C at TOP → leave EXPL
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

    // The explore-control overlay is a CURSOR LIST: the A+C chord opens it, and from
    // there plain A/B/C drive it — the chord is the way in and never a navigation key
    // inside the screen it opened. It opens on the first action, A walks the rows, B
    // does the focused one, C backs out with the walk still running.
    { Game g{StartMode::Hatched, "bruinforce"};
      g.debugArmExplore(0, 0);
      CHECK(g.autoProgress());                      // defaults ON
      g.onButton(chordAC());                        // habitat → the overlay, on PING
      CHECK(g.nav() == Game::Nav::ExploreControl);
      g.onButton(press(Button::A));                 // → WARP
      g.onButton(press(Button::A));                 // → AUTO-PROGRESS
      g.onButton(press(Button::B));                 // opt out
      CHECK(!g.autoProgress());
      CHECK(g.nav() == Game::Nav::ExploreControl);  // a MODE leaves the list open
      g.onButton(press(Button::B));
      CHECK(g.autoProgress());                      // and toggles back on
      tapC(g);                 // C backs out, walk untouched
      CHECK(g.nav() == Game::Nav::Idle && g.exploreActive()); }

    // STOP EXPLORE is now a row rather than the C key, so backing out of the overlay
    // can no longer cancel the walk by accident.
    { Game g{StartMode::Hatched, "bruinforce"};
      g.debugArmExplore(0, 0);
      stopExplore(g);
      CHECK(!g.exploreActive() && g.nav() == Game::Nav::Idle); }

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
void test_deepweb_dive() {
    // (1) The pure scaler: enemy level = pet level (parity, so wildWinXp pays the FULL
    //     base forever), and a BUDGET of points spent at random across the same four
    //     stats a pet levels — not the old fixed Health+speed split. The budget is what
    //     is pinned here; where any one roll puts it is the feature.
    {
        CombatEnemy e = wildMalbeast(3, 0);
        applyDeepWebScale(e, 10);
        CHECK(e.level == 10 + kDeepWebEnemyLevelOffset);
        CHECK(wildWinXp(kWildWinXpReward, e.level, 10) == kWildWinXpReward);   // parity
        CombatEnemy e2 = wildMalbeast(3, 0);
        applyDeepWebScale(e2, -5);                    // petLevel clamps at 0
        CHECK(e2.level == 0 + kDeepWebEnemyLevelOffset);
    }
    // (1a) The spread spends the whole budget and no more, and POWER is in the hat —
    //      the flat-offence hole that let a stacked wall chip a depth-500 enemy forever.
    {
        const CombatEnemy base = wildMalbeast(3, 0);
        int sawPower = 0, sawDef = 0, sawSpeed = 0, sawHealth = 0;
        for (uint32_t seed = 1; seed <= 40; ++seed) {
            CombatEnemy e = wildMalbeast(3, 0);
            applyDeepWebScale(e, 12, 0, seed);        // budget = 12 (depth 0, no linear)
            const int pPts = (e.powerMultPct - base.powerMultPct) / kLevelPowerPctPerPoint;
            const int sPts = (e.speed - base.speed) / kLevelSpeedPerPoint;
            const int hPts = (e.maxHealth - base.maxHealth) / kDeepWebHealthPerLevel;
            CHECK(pPts >= 0 && sPts >= 0 && hPts >= 0);
            CHECK(pPts + sPts + hPts <= 12);          // the rest went to Defence
            CHECK(e.dmgReducePct <= kLevelDefenseCapPct);
            if (pPts > 0) ++sawPower;
            if (e.dmgReducePct > 0) ++sawDef;
            if (sPts > 0) ++sawSpeed;
            if (hPts > 0) ++sawHealth;
        }
        // Every stat is reachable — a spread that never rolls Power would be the old bug
        // wearing a new shape, and one that never rolls Defence is a claim the dive makes
        // to the player that it doesn't keep.
        CHECK(sawPower > 0 && sawDef > 0 && sawSpeed > 0 && sawHealth > 0);
    }
    // (1b) Same seed, same enemy — the dive rolls like every other roll in the engine.
    {
        CombatEnemy a = wildMalbeast(3, 0), b = wildMalbeast(3, 0);
        applyDeepWebScale(a, 9, 40, 12345u);
        applyDeepWebScale(b, 9, 40, 12345u);
        CHECK(a.powerMultPct == b.powerMultPct && a.maxHealth == b.maxHealth);
        CHECK(a.speed == b.speed && a.dmgReducePct == b.dmgReducePct);
        CHECK(a.moveIds.size() == b.moveIds.size());
    }
    // (1c) DEPTH keeps paying after the log ramp flattens. The linear term is the one
    //      that ends a run: a dive is meant to become unwinnable, not merely slow.
    {
        auto spend = [](int depth) {
            const CombatEnemy base = wildMalbeast(3, 0);
            long long total = 0;
            for (uint32_t seed = 1; seed <= 16; ++seed) {
                CombatEnemy e = wildMalbeast(3, 0);
                applyDeepWebScale(e, 10, depth, seed);
                total += (e.powerMultPct - base.powerMultPct) / kLevelPowerPctPerPoint;
                total += (e.speed - base.speed) / kLevelSpeedPerPoint;
                total += (e.maxHealth - base.maxHealth) / kDeepWebHealthPerLevel;
            }
            return total;
        };
        // 1024 and 2048 sit in the SAME log bracket pair but differ by 128 linear points,
        // so this fails the moment the budget goes back to being purely logarithmic.
        CHECK(spend(2048) > spend(1024));
        CHECK(spend(1024) > spend(64));
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
    // (1d) The KIT by depth, and the gate that keeps a boss the first place its move is
    //      ever seen. Below kDeepWebBossMoveDepth the dive may only hand back the shared
    //      pool; at and past it, the deep pool takes over.
    {
        ContentRegistry reg = ContentRegistry::embedded();
        auto isBossMove = [&](const char* id) {
            for (int a = 0; a < kExplSectors; ++a) {
                if (area(a).areaBossMoveId &&
                    std::strcmp(area(a).areaBossMoveId, id) == 0) return true;
                for (int s = 0; s < kExplSubAreas; ++s)
                    for (const char* t : area(a).subBosses[s].teaches)
                        if (t && std::strcmp(t, id) == 0) return true;
            }
            return false;
        };
        int deepBossHits = 0;
        for (uint32_t seed = 1; seed <= 30; ++seed) {
            for (int depth : {0, 1, 15, 63, 200, kDeepWebBossMoveDepth - 1}) {
                for (const char* id : deepWebMoveIds(depth, seed)) {
                    CHECK(reg.move(id) != nullptr);   // every rung id must resolve
                    CHECK(!isBossMove(id));           // ...and none may be a boss's
                }
            }
            for (const char* id : deepWebMoveIds(kDeepWebBossMoveDepth + 40, seed)) {
                CHECK(reg.move(id) != nullptr);
                if (isBossMove(id)) ++deepBossHits;
            }
        }
        CHECK(deepBossHits > 0);                      // the deep pool really is the bosses'
        // Two distinct moves, never the same one twice wearing two hats.
        for (uint32_t seed = 1; seed <= 20; ++seed) {
            const std::vector<const char*> k = deepWebMoveIds(500, seed);
            CHECK(k.size() == 2);
            CHECK(std::strcmp(k[0], k[1]) != 0);
        }
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
                case Game::Nav::Shop: tapC(g); break;
                case Game::Nav::ModShop: tapC(g); break;
                default: tapC(g); break;
            }
            (void)t;
        }
        CHECK(g.nav() == Game::Nav::Combat);          // a wild dive fight started
        CHECK(g.debugEncounterEnemyLevel() == L + kDeepWebEnemyLevelOffset);  // scaled to pet
    }
    // (4) The Deep-Learning buff SURVIVES the dive it was armed for. It is a
    //     Context::Anytime item, so the whole point is arming it before diving —
    //     resetting the multiplier in startDeepWebDive would consume it on the way in
    //     and it would never once apply. Only a dive ENDING spends it.
    {
        Game g{StartMode::Hatched, "bruinforce"};
        for (int a = 0; a < kExplSectors; ++a) g.debugSetSectorCleared(a, true);
        CHECK(g.debugDepthMultiplier() == 1);              // nothing armed

        g.inventory().add("deep_learning_core", 1);
        g.debugUseItem("deep_learning_core");
        CHECK(g.debugDepthMultiplier() == 4);              // armed, before the dive

        g.debugStartDeepWebDive();
        CHECK(g.inDeepWebDive());
        CHECK(g.debugDepthMultiplier() == 4);              // ...and it is STILL armed

        // Stopping the dive spends it.
        stopExplore(g);
        CHECK(!g.inDeepWebDive());
        CHECK(g.debugDepthMultiplier() == 1);
    }
    // (5) A SECTOR walk is not a dive: arming the buff and then taking an ordinary
    //     walk must not eat it, because the multiplier only ever applies on
    //     kDeepWebSector (the inDeepWebDive() guard on the streak advance).
    {
        Game g{StartMode::Hatched, "bruinforce"};
        g.inventory().add("deep_learning_module", 1);
        g.debugUseItem("deep_learning_module");
        CHECK(g.debugDepthMultiplier() == 2);

        g.debugArmExplore(0, 0);
        CHECK(g.exploreActive() && !g.inDeepWebDive());
        CHECK(g.debugDepthMultiplier() == 2);              // survives entering a walk
        stopExplore(g);
        CHECK(g.debugDepthMultiplier() == 2);              // ...and leaving one
    }
}

// Combat::begin's forceEnemyFirst override (retained combat-engine mechanic):
// even a much faster player is forced to act second, independent of speed.
void test_combat_force_enemy_first() {
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
void test_encounter_sinkhole_bypass() {
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
            case Game::Nav::Shop: tapC(g); break;
            case Game::Nav::ModShop: tapC(g); break;
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
void test_sinkhole_xp_persists_immediately() {
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
                case Game::Nav::Shop: tapC(g); break;
                case Game::Nav::ModShop: tapC(g); break;
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
