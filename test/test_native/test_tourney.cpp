// test_tourney.cpp — ROCK THE DOCK: the derivation, the headless bracket, and the run.
//
// The arena's whole correctness claim is that a bracket is a SEED: the same seed must
// build the same eight operators, the matches nobody watches must be really fought and
// really deterministic, and a run must reload identical after a reboot. Each of those
// is asserted directly here rather than through a played tournament, because a gate
// that can only see the end of a bracket cannot say which half of it went wrong.
#include "test_gates.h"

#include "core/content/content_crews.h"
#include "core/content/content_tournament.h"
#include "core/model/tournament.h"
#include "core/ui/layout.h"
#include "core/ui/prose_page.h"

using namespace mal;

namespace {

// The shipped content, built once — every entrant roll below walks the whole roster,
// so a per-gate registry would be the slowest thing in the suite.
const ContentRegistry& reg() {
    static ContentRegistry r = ContentRegistry::embedded();
    return r;
}

}  // namespace

// Every entrant handle has to survive being copied into a PvpFighter::tag, which is
// kHackerTagMax cells plus the terminator. A pool row that overflowed would be
// silently truncated into a different operator's name — so this is the one property of
// the table a reader cannot check by eye.
void test_tourney_handles_fit_an_operator_tag() {
    CHECK(kTourneyHandleCount >= kTourneySlots);   // eight names, drawn without replacement
    for (int i = 0; i < kTourneyHandleCount; ++i) {
        const char* h = tourneyHandleName(i);
        CHECK(h && h[0]);
        CHECK(static_cast<int>(std::strlen(h)) <= kHackerTagMax);
    }
    CHECK(tourneyHandleName(-1)[0] == '\0');
    CHECK(tourneyHandleName(kTourneyHandleCount)[0] == '\0');
}

// THE SEED IS THE RUN. The same seed rebuilds the identical field — that is what makes
// four persisted bytes enough — and eight slots of one bracket are eight DISTINCT
// operators rather than the same roll wearing different numbers.
void test_tourney_entrants_are_derived_from_the_seed() {
    const uint32_t seed = 0xC0FFEEu;
    for (int slot = 0; slot < kTourneySlots; ++slot) {
        const TourneyCard a = tourneyCard(seed, slot);
        const TourneyCard b = tourneyCard(seed, slot);
        CHECK(std::strcmp(a.handle, b.handle) == 0 && a.level == b.level);
        // ...and the full build agrees with the cheap card, which is the whole reason
        // the builder reads its level off the same stream instead of re-rolling it.
        const TourneyFighter f = tourneyEntrant(reg(), seed, slot);
        CHECK(static_cast<int>(f.spec.level) == a.level);
        CHECK(std::strcmp(f.spec.tag, a.handle) == 0);
    }
    // Handles are drawn WITHOUT replacement across a bracket.
    for (int i = 0; i < kTourneySlots; ++i)
        for (int j = i + 1; j < kTourneySlots; ++j)
            CHECK(std::strcmp(tourneyCard(seed, i).handle,
                              tourneyCard(seed, j).handle) != 0);
    // A different seed is a different draw (the whole field matching would mean the
    // seed reached nothing).
    bool anyDifferent = false;
    for (int slot = 0; slot < kTourneySlots; ++slot)
        if (std::strcmp(tourneyCard(seed, slot).handle,
                        tourneyCard(seed ^ 0x5A5A5A5Au, slot).handle) != 0)
            anyDifferent = true;
    CHECK(anyDifferent);
    // Out-of-range slots are inert rather than reading off the end of the pool.
    CHECK(tourneyCard(seed, -1).handle[0] == '\0');
    CHECK(tourneyCard(seed, kTourneySlots).handle[0] == '\0');
}

// An entrant is PETWARE, held to every rule a raised pet is: a level inside the
// arena's band, a stage that band actually fields, a kit that matches its own slot
// typing and never crosses a line, and mods it is high enough to equip. This is what
// stops the arena becoming a place where impossible pets exist.
void test_tourney_entrants_are_legal_petware() {
    for (uint32_t seed = 1; seed <= 40; ++seed) {
        for (int slot = 0; slot < kTourneySlots; ++slot) {
            const TourneyFighter f = tourneyEntrant(reg(), seed * 2654435761u, slot);
            const int level = static_cast<int>(f.spec.level);
            CHECK(level >= kTourneyMinLevel && level <= kTourneyMaxLevel);

            const CreatureDef* pet = reg().creature(f.spec.creatureId);
            CHECK(pet != nullptr);
            CHECK(pet->line != nullptr);            // real petware, never a bare frame
            CHECK(pet->stage != Stage::BootSector); // an egg is not an entrant
            // The stage bands, restated as the assertion rather than re-derived.
            if (level < kTourneyScriptLevel) CHECK(pet->stage == Stage::Process);
            else if (level < kTourneyDaemonLevel) CHECK(pet->stage == Stage::Script);
            else CHECK(pet->stage == Stage::Daemon);

            // Stat points are the level, spent one at a time — the same grant a raise
            // makes, so an entrant's stat line is one a real pet could have reached.
            int sum = 0;
            for (int i = 0; i < 4; ++i) sum += f.spec.statPoints[i];
            CHECK(sum == level);

            // Moves: only the slots this stage unlocked, each matching that slot's
            // stamped kind, unlocked at the stage, and inside the creature's own line.
            const int slots = MoveLoadout::slotsForStage(pet->stage);
            for (int i = 0; i < kMaxMoveSlots; ++i) {
                if (i >= slots) { CHECK(f.spec.moveIds[i][0] == '\0'); continue; }
                const MoveDef* m = reg().move(f.spec.moveIds[i]);
                CHECK(m != nullptr);
                CHECK(m->kind == pet->slotKinds[i]);
                CHECK(moveUnlockedAtStage(*m, pet->stage));
                CHECK(moveAllowedForLine(*m, pet->line));
                for (int j = 0; j < i; ++j)         // no slot repeats another's move
                    CHECK(std::strcmp(f.spec.moveIds[i], f.spec.moveIds[j]) != 0);
            }

            // Mods: distinct, and each one this level could actually equip.
            for (int i = 0; i < kModSlots; ++i) {
                if (!f.spec.modIds[i][0]) continue;
                const ModDef* d = reg().mod(f.spec.modIds[i]);
                CHECK(d != nullptr);
                CHECK(modEquipLevel(*d) <= level);
                for (int j = 0; j < i; ++j)
                    CHECK(std::strcmp(f.spec.modIds[i], f.spec.modIds[j]) != 0);
            }

            // And an Exploit off the crew roster, with a trigger off the arena's table.
            CHECK(f.exploit.kind != CrewExploitKind::None);
            bool known = false;
            for (int c = 0; c < kCrewCount; ++c)
                if (crew(c)->exploit.kind == f.exploit.kind &&
                    crew(c)->exploit.magnitude == f.exploit.magnitude)
                    known = true;
            CHECK(known);
            bool trigger = false;
            for (int t = 0; t < kTourneyTriggerCount; ++t)
                if (f.exploitAtHealthPct == kTourneyTriggerPcts[t]) trigger = true;
            CHECK(trigger);
            // The whole point of building it: it has to be a fightable combatant.
            const Combatant c = makeTourneyCombatant(reg(), f);
            CHECK(c.maxHealth > 0 && !c.moves.empty());
            CHECK(c.autoExploit.kind == f.exploit.kind);
        }
    }
}

// An opponent fires its OWN Exploit — nobody is at the picker for it. A 100% trigger
// means "opens with it", and firing SPENDS the turn, which is the cost that keeps it
// from being free. Asserted at the engine, on a fight with no tournament in it, so the
// mechanic stands on its own.
void test_a_fighter_can_fire_its_own_exploit() {
    const ContentRegistry& r = reg();
    CombatEnemy spec{"Sparring", "SPR_PET_CACHEMUTT", 1, 200, 6, {"quick_jab"}};
    Combatant a = makeEnemyCombatant(r, spec);
    Combatant b = makeEnemyCombatant(r, spec);
    // The enemy seat opens with three negation charges.
    b.autoExploit = {"DENIAL OF SERVICE", CrewExploitKind::NegateNextHits, 3};
    b.autoExploitAtHealthPct = 100;
    Combat c;
    c.begin(a, b, Combat::Stakes::Safe, 12345u, /*forceEnemyFirst=*/true,
            /*carryPlayerHealth=*/-1, /*exploitUses=*/0);
    CHECK(c.enemy().crewExploit.kind == CrewExploitKind::None);   // not yet armed
    c.step();                                                     // the enemy's opener
    CHECK(c.enemy().crewExploit.armed(CrewExploitKind::NegateNextHits));
    CHECK(c.enemy().crewExploit.charges == 3);
    CHECK(c.player().health == c.player().maxHealth);   // the turn was SPENT arming
    // ...and it only ever fires once, however long the fight runs.
    for (int i = 0; i < 200 && c.outcome() == Combat::Outcome::Ongoing; ++i) c.step();
    CHECK(c.enemy().crewExploit.charges <= 3);

    // A LOW trigger waits. A fighter at full Health with a 20% trigger must not arm on
    // its opening turn — the tell is the whole reason a trigger exists.
    Combatant d = makeEnemyCombatant(r, spec);
    d.autoExploit = {"DENIAL OF SERVICE", CrewExploitKind::NegateNextHits, 3};
    d.autoExploitAtHealthPct = 20;
    Combat c2;
    c2.begin(a, d, Combat::Stakes::Safe, 999u, /*forceEnemyFirst=*/true,
             /*carryPlayerHealth=*/-1, /*exploitUses=*/0);
    c2.step();
    CHECK(c2.enemy().crewExploit.kind == CrewExploitKind::None);
}

// The matches nobody watches. They have to TERMINATE (two brace-heavy kits can
// stalemate, and the bracket owes an opponent either way) and they have to be
// DETERMINISTIC, or a resumed run would climb a different tournament.
void test_tourney_headless_matches_terminate_and_repeat() {
    for (uint32_t seed = 1; seed <= 12; ++seed) {
        const uint32_t s = seed * 2246822519u;
        const TourneyFighter a = tourneyEntrant(reg(), s, 0);
        const TourneyFighter b = tourneyEntrant(reg(), s, 1);
        const uint32_t ms = tourneyMatchSeed(s, 0, 0);
        const bool first = tourneyResolveMatch(reg(), a, b, ms);
        CHECK(first == tourneyResolveMatch(reg(), a, b, ms));
    }
}

// The bracket tree: each round halves every block, the operator's own block is left
// for the screen, and the pairing a block names is always the other survivor in it.
void test_tourney_rounds_halve_the_field() {
    const uint32_t seed = 0xBADC0DEu;
    const int me = tourneyPlayerSlot(seed);
    CHECK(me >= 0 && me < kTourneySlots);
    uint8_t alive = static_cast<uint8_t>((1u << kTourneySlots) - 1);
    CHECK(tourneyAliveCount(alive) == kTourneySlots);

    for (int round = 0; round < kTourneyRounds; ++round) {
        const int before = tourneyAliveCount(alive);
        tourneyResolveRound(reg(), seed, round, me, alive);
        // Every block but the operator's has resolved: half the field is out, except
        // for the one pairing still waiting to be played on the screen.
        CHECK(tourneyAliveCount(alive) == before / 2 + 1);
        CHECK(alive & (1u << me));                    // never resolved out from under us
        // The operator's opponent is the other survivor of their own block.
        const int opp = tourneyOpponentSlot(alive, me, round);
        CHECK(opp >= 0 && opp != me);
        CHECK(tourneyBlockStart(opp, round) == tourneyBlockStart(me, round));
        CHECK(alive & (1u << opp));
        alive &= static_cast<uint8_t>(~(1u << opp));   // the operator wins their match
    }
    CHECK(tourneyAliveCount(alive) == 1 && (alive & (1u << me)));
}

// The run, end to end, through the real screens: the EXPL row is locked until the
// arena's water is reached, entering draws a bracket, and a won match advances it.
void test_tourney_run_from_the_expl_row() {
    Game g{StartMode::Hatched, "bruinforce"};
    g.debugAddCombatXp(600000);                  // a pet that can actually win a match
    const int arenaRow = explRowCount() - 1;
    bool cleared[kExplSectors * kExplSubAreas] = {};
    bool boss[kExplSectors * kExplSubAreas] = {};
    bool areas[kExplSectors] = {};
    CHECK(explRowState(arenaRow, areas, cleared, boss, -1, -1, false) ==
          ExplRowState::TourneyLocked);
    CHECK(!explRowSelectable(ExplRowState::TourneyLocked));

    // Reaching The Pirate Bayou — clearing the area before it — is the invitation.
    g.debugSetSectorCleared(0, true);
    areas[0] = true;
    CHECK(explRowState(arenaRow, areas, cleared, boss, -1, -1, false) ==
          ExplRowState::TourneyOpen);
    CHECK(explRowState(arenaRow, areas, cleared, boss, -1, -1, true) ==
          ExplRowState::TourneyRunning);

    // Walk to the row and enter. It is the LAST row, so C-holding backward from the
    // parked area header is the short way there; A round-trips just as well.
    enterSubmenuId(g, SubmenuId::Expl);
    for (int i = 0; i < explRowCount() && g.listRow() != arenaRow; ++i)
        g.onButton(press(Button::A));
    CHECK(g.listRow() == arenaRow);
    g.onButton(press(Button::B));
    CHECK(g.nav() == Game::Nav::Tourney);
    CHECK(g.tourneyRunning());
    CHECK(g.tourneyRound() == 0);
    // Round 1 resolved itself around the operator: eight entrants, five still standing
    // (the operator, their opponent, and the three winners of the other pairings).
    CHECK(tourneyAliveCount(g.tourneyAlive()) == kTourneySlots / 2 + 1);
    const int opp = g.tourneyOpponentSlot();
    CHECK(opp >= 0 && opp != g.tourneySlot());
    CHECK(g.tourneyOpponent().spec.creatureId[0]);    // the match card has a species

    // A TAPPED B starts the bout — B is a tap/hold pair here, so the press edge only
    // arms it and the tap resolves on the release. There is no running from a bracket,
    // so C is inert mid-bout.
    tapB(g);
    CHECK(g.nav() == Game::Nav::Combat);
    tapC(g);
    CHECK(g.combat().outcome() == Combat::Outcome::Ongoing);   // C did not forfeit
    uint32_t t = 0;
    for (int i = 0; i < 4000 && g.combat().outcome() == Combat::Outcome::Ongoing; ++i)
        g.tick(t += kHeartbeatMs);
    const bool won = g.combat().outcome() == Combat::Outcome::Win;
    tapB(g);                                                   // dismiss the verdict
    CHECK(g.nav() == Game::Nav::Tourney);
    if (won) {
        CHECK(g.tourneyRound() == 1);
        CHECK(!(g.tourneyAlive() & (1u << opp)));              // the beaten one is out
        CHECK(g.tourneyPhase() == Game::TourneyPhase::Ready);
    } else {
        CHECK(g.tourneyPhase() == Game::TourneyPhase::Eliminated);
        // A dismissed verdict forgets the run and hands the operator back to EXPL.
        tapB(g);
        CHECK(!g.tourneyRunning() && g.nav() == Game::Nav::Submenu);
    }
}

// The bracket screen's three ROW states — OUT, NEXT, YOU — and its two terminal
// banners have to survive the dual-coding gate: desaturate the panel and it must still
// say which entrants are gone and which fight is next. Colour is the emphasis here, and
// the word is the meaning.
void test_tourney_screen_grayscale() {
    auto renderRun = [](bool eliminate, Framebuffer& fb) {
        Game g{StartMode::Hatched, "bruinforce"};
        g.debugAddCombatXp(600000);
        g.debugSetSectorCleared(0, true);          // reach the Bayou → the arena opens
        enterSubmenuId(g, SubmenuId::Expl);
        for (int i = 0; i < explRowCount() && g.listRow() != explRowCount() - 1; ++i)
            g.onButton(press(Button::A));
        tapB(g);              // draw a bracket
        if (eliminate) g.debugEndTourneyRun(/*champion=*/false);
        g.render(fb);
    };
    Framebuffer live(kActiveW, kActiveH), out(kActiveW, kActiveH);
    renderRun(false, live);
    renderRun(true, out);
    int diff = 0;
    for (int y = 0; y < kActiveH; ++y)
        for (int x = 0; x < kActiveW; ++x)
            if (luminance(live.get(x, y)) != luminance(out.get(x, y))) ++diff;
    CHECK(diff > 0);
}

// The bracket screen's fixed copy has to fit the panel it is drawn on. Everything
// variable (a species name beside its Exploit tag) goes through drawLabelValue and
// yields; these lines do not, so a rewrite that overflows would simply be cut, and the
// cut is invisible to every other gate.
void test_tourney_screen_copy_fits_its_panel() {
    const int budget = kActiveW - kMargin * 2;
    char purse[40];
    std::snprintf(purse, sizeof(purse), "+%d BITS +%d XP +1 MOD", kTourneyWinBits,
                  kTourneyWinXp);
    const char* lines[] = {"THE FIELD IS YOURS", purse, "KNOCKED OUT OF THE DRAW",
                           "THE BRACKET GOES ON", "A READ   B FIGHT   C BACK"};
    for (const char* l : lines) CHECK(textWidth(l) <= budget);
    // A field row is a handle, then the fixed level + status columns on the right. The
    // longest handle in the pool must clear the widest thing those two ever hold.
    int widest = 0;
    for (int i = 0; i < kTourneyHandleCount; ++i)
        if (textWidth(tourneyHandleName(i)) > widest)
            widest = textWidth(tourneyHandleName(i));
    CHECK(kMargin + 10 + widest < kActiveW - kMargin - textWidth("NEXT") -
                                      textWidth("L60") - 6);
}

// The bracket is a screen you STUDY — the field, the levels, and what the next
// opponent is carrying. The 5s menu-idle collapse would take it away mid-decision, so
// it is held open the way the other wait-in-front-of-it screens are.
void test_tourney_screen_outlives_the_menu_idle_timer() {
    Game g{StartMode::Hatched, "bruinforce"};
    g.debugSetSectorCleared(0, true);
    enterSubmenuId(g, SubmenuId::Expl);
    for (int i = 0; i < explRowCount() && g.listRow() != explRowCount() - 1; ++i)
        g.onButton(press(Button::A));
    g.onButton(press(Button::B));
    CHECK(g.nav() == Game::Nav::Tourney);
    uint32_t t = 0;
    for (int i = 0; i < 40; ++i) g.tick(t += kAutoDefocusMs);   // far past the collapse
    CHECK(g.nav() == Game::Nav::Tourney);
}

// THE BRIEFING is the only thing that tells an operator what an arena bout is, so it
// has to actually fit the page it is drawn on: prose past EffectText's cap is silently
// lost, and a heading that overflows the panel is cut mid-word.
void test_tourney_brief_fits_its_page() {
    CHECK(kTourneyBriefCount > 0);
    for (int i = 0; i < kTourneyBriefCount; ++i) {
        const TourneyBriefDef& b = kTourneyBrief[i];
        CHECK(b.heading && b.heading[0] && b.text && b.text[0]);
        CHECK(textWidth(b.heading) <= kProseW);
        CHECK(static_cast<int>(std::strlen(b.text)) <= EffectText::kMaxProse);
    }
    // ...and the built rows are the table, in order, with the prose intact.
    Game g{StartMode::Hatched, "bruinforce"};
    const auto rows = g.tourneyBriefRows();
    CHECK(static_cast<int>(rows.size()) == kTourneyBriefCount);
    for (int i = 0; i < kTourneyBriefCount; ++i) {
        CHECK(std::strcmp(rows[i].label, kTourneyBrief[i].heading) == 0);
        CHECK(std::strcmp(rows[i].body.c_str(), kTourneyBrief[i].text) == 0);
        CHECK(!rows[i].body.atCap());        // nothing was clipped on the way in
    }
    // Every section fits a screen on its own — a section taller than the whole page
    // would draw clipped and never scroll past.
    for (int top = 0; top < kTourneyBriefCount; ++top)
        CHECK(proseRowsFitting(rows, top, 46) >= 1);
}

// SCOUTING a rival is the answer to "their loadout matters now": the sheet has to name
// every move and mod they are actually carrying, the stat spread their levels bought,
// and the Exploit they hold — and it has to say it in the move's OWN words, because it
// is the same builder STAT's LOADOUT page uses.
void test_tourney_scout_shows_the_rivals_whole_kit() {
    Game g{StartMode::Hatched, "bruinforce"};
    g.debugSetSectorCleared(0, true);
    enterSubmenuId(g, SubmenuId::Expl);
    for (int i = 0; i < explRowCount() && g.listRow() != explRowCount() - 1; ++i)
        g.onButton(press(Button::A));
    g.onButton(press(Button::B));
    CHECK(g.nav() == Game::Nav::Tourney);

    // Park on a rival, then HOLD B — the tap starts a bout, the hold opens the sheet.
    while (g.tourneyCursor() == g.tourneySlot()) g.onButton(press(Button::A));
    const int scouted = g.tourneyCursor();
    uint32_t t = 0;
    g.onButton(press(Button::B));
    g.tick(t += kTourneyScoutHoldMs + kHeartbeatMs);
    g.onButton(lift(Button::B));
    CHECK(g.tourneyView() == Game::TourneyView::Scout);
    CHECK(g.nav() == Game::Nav::Tourney);          // a reader, not a bout

    const TourneyFighter f = tourneyEntrant(g.content(), g.tourneySeed(), scouted);
    const auto rows = g.tourneyScoutRows();
    auto namesRow = [&](const char* label) {
        for (const auto& r : rows)
            if (std::strcmp(r.label, label) == 0) return true;
        return false;
    };
    // Every equipped move and mod, by its own displayName.
    for (int i = 0; i < kMaxMoveSlots; ++i)
        if (f.spec.moveIds[i][0])
            CHECK(namesRow(g.content().move(f.spec.moveIds[i])->displayName));
    for (int i = 0; i < kModSlots; ++i)
        if (f.spec.modIds[i][0])
            CHECK(namesRow(g.content().mod(f.spec.modIds[i])->displayName));
    // The stat spread — invisible in a move list, and half of what an opponent is.
    for (int i = 0; i < kLevelStatCount; ++i) CHECK(namesRow(levelStatName(i)));
    // The Exploit it holds, by name. Its TRIGGER is deliberately absent: which moment a
    // rival commits to is the read the arena exists to teach.
    CHECK(f.exploit.label && namesRow(f.exploit.label));
    for (const auto& r : rows) {
        char pct[8];
        std::snprintf(pct, sizeof(pct), "%d%%", f.exploitAtHealthPct);
        CHECK(std::strstr(r.body.c_str(), pct) == nullptr);
    }
    // C returns to the bracket, and the run is untouched by having been read.
    tapC(g);
    CHECK(g.tourneyView() == Game::TourneyView::Bracket && g.tourneyRunning());

    // The chord opens the BRIEFING from the same screen, and C closes that too.
    g.onButton(chordAC());
    CHECK(g.tourneyView() == Game::TourneyView::Brief);
    tapC(g);
    CHECK(g.tourneyView() == Game::TourneyView::Bracket);
}

// A bracket is four bytes plus a bitmask, so it has to survive a reboot exactly —
// otherwise a run resumed after a power cycle would be a different tournament.
void test_tourney_run_survives_a_reboot() {
    SaveData d;
    d.tourneySeed = 0xABCD1234u;
    d.tourneyAlive = 0x5A;
    d.tourneyRound = 2;
    d.tourneyPhase = static_cast<uint8_t>(Game::TourneyPhase::Champion);
    SaveData back;
    CHECK(deserializeSave(serializeSave(d), back));
    CHECK(back.tourneySeed == d.tourneySeed);
    CHECK(back.tourneyAlive == d.tourneyAlive);
    CHECK(back.tourneyRound == d.tourneyRound);
    CHECK(back.tourneyPhase == d.tourneyPhase);
}
