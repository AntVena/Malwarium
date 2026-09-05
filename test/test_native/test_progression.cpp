// test_progression.cpp — native gates for creature level, move slots, evolution branching and Critical System Failure.
//
// One slice of the native suite; see test_gates.h for the shared fixtures and
// test_main.cpp for the roster that runs these. A save-vNN gate sits with the
// feature whose field it migrates, not in a migrations pile of its own.
#include <array>
#include <cstring>

#include "test_gates.h"

// A Sim-Battle plays end-to-end: launch from TRAIN, auto-resolve at the heartbeat,
// win returns to the loadout with a flat reward, safe stakes (no Frag), not logged.
void test_sim_battle_end_to_end() {
    Game g{StartMode::Hatched, "paypup"};
    const int bits0 = g.bits();
    enterSimBattle(g);
    CHECK(g.nav() == Game::Nav::Combat);
    uint32_t t = 0;
    for (int i = 0; i < 400 && g.combat().outcome() == Combat::Outcome::Ongoing; ++i)
        g.tick(t += kHeartbeatMs);
    CHECK(g.combat().outcome() == Combat::Outcome::Win);     // a raised pet beats Basic
    g.onButton(press(Button::B));                            // dismiss -> TRAIN loadout
    CHECK(g.nav() == Game::Nav::Submenu);
    const int snifferBonus = ContentRegistry::embedded().mod("packet_sniffer")->magnitude;
    CHECK(g.bits() == bits0 + kSimBitsReward + snifferBonus ||
          g.bits() == bits0 + kSimBitsReward);               // flat Bits (sniffer optional)
    CHECK(g.bits() > bits0);
    CHECK(g.combatXp() == kSimXpReward);                     // combat XP grew
    CHECK(g.model().fragmentation() == kStartFragmentation); // safe stakes -> no Frag
    CHECK(g.log().size() == 0);                              // not logged
}

// A Sim Battle is the one fight an armed Backup Drive save sits out.
void test_backup_drive_save_not_spent_in_sim_battle() {
    Game g{StartMode::Hatched, "paypup"};
    g.debugUseItem("backup_drive");
    CHECK(g.backupShieldArmed());
    // Consuming the last Backup Drive drops useItem() into Nav::Submenu (the ITEMS
    // "item left the list" case) — back out to the carousel before enterSimBattle's
    // TRAIN walk, which assumes it's starting from the carousel/idle layer.
    tapC(g);
    enterSimBattle(g);
    // A Sim Battle deliberately fights WITHOUT the save: no stakes means no death to be
    // saved from, so a training bout must never burn a Rare item (Game::startSimBattle).
    CHECK(!g.combat().player().itemShield);
    CHECK(g.backupShieldArmed());   // untouched — still there for a fight that counts
}

// Creature levels: XP banks into levels on a geometric curve, each level
// grants +1 to a random combat stat, and level always equals the sum of earned
// points (the invariant Rollback preserves). A big lump crosses several boundaries.
void test_creature_level_curve_and_invariant() {
    Game g{StartMode::Hatched, "paypup"};
    CHECK(g.combatLevel() == 0 && g.combatXp() == 0);
    for (int i = 0; i < kLevelStatCount; ++i) CHECK(g.levelStatPoint(i) == 0);

    // First level costs kLevelXpBase; one under it doesn't level, exactly it does.
    g.debugAddCombatXp(kLevelXpBase - 1);
    CHECK(g.combatLevel() == 0 && g.combatXp() == kLevelXpBase - 1);
    g.debugAddCombatXp(1);
    CHECK(g.combatLevel() == 1);
    CHECK(g.combatXp() == 0);                 // bucket spent exactly
    CHECK(g.levelStatPoint(0) + g.levelStatPoint(1) + g.levelStatPoint(2) +
          g.levelStatPoint(3) == 1);          // one point granted
    CHECK(g.lastLevelUpStat() >= 0 && g.lastLevelUpStat() < kLevelStatCount);

    // Second level costs more than the first (geometric, ~1.1x): base+1 XP is short.
    g.debugAddCombatXp(kLevelXpBase);
    CHECK(g.combatLevel() == 1);              // kLevelXpBase < cost of level 2
    // A big lump crosses multiple boundaries; the invariant + a bounded leftover hold.
    g.debugAddCombatXp(5000);
    int sum = 0;
    for (int i = 0; i < kLevelStatCount; ++i) sum += g.levelStatPoint(i);
    CHECK(sum == g.combatLevel());            // level == total earned points
    CHECK(g.combatXp() >= 0);                 // leftover bucket is a valid partial
    CHECK(g.combatLevel() >= 5);              // 5000 XP is many levels
}

// Level stat points feed the combat maths: a leveled pet's built Combatant
// carries exactly its earned points, additive over the un-levelled baseline (small
// point counts stay under the defense caps, so the arithmetic is exact).
void test_creature_level_feeds_combat() {
    // Baseline: an un-levelled Sim-Battle player.
    Game g0{StartMode::Hatched, "paypup"};
    enterSimBattle(g0);
    const Combatant base = g0.combat().player();

    // Leveled: a modest level so no stat hits the defense cap (≤~8 points each).
    Game g1{StartMode::Hatched, "paypup"};
    g1.debugAddCombatXp(1200);
    const int pP = g1.levelStatPoint(0), pD = g1.levelStatPoint(1),
              pS = g1.levelStatPoint(2), pH = g1.levelStatPoint(3);
    CHECK(pP + pD + pS + pH == g1.combatLevel());
    enterSimBattle(g1);
    const Combatant lv = g1.combat().player();

    CHECK(lv.maxHealth == base.maxHealth + pH * kLevelHealthPerPoint);
    CHECK(lv.speed == base.speed + pS * kLevelSpeedPerPoint);
    CHECK(lv.powerMultPct == base.powerMultPct + pP * kLevelPowerPctPerPoint);
    // Defence goes through its own curve rather than a flat per-point rate, so this
    // asserts the curve and not a coincidence of the point count staying under the bend.
    CHECK(lv.dmgReducePct == base.dmgReducePct + levelDefenseCutPct(pD));
    CHECK(lv.health == lv.maxHealth);         // starts full at the leveled max
}

// DEFENCE DIMINISHES, and then it stops. Defence is the only stat with a hard ceiling,
// which without a bend made the last points before that ceiling the best purchase in the
// game — buy enough and the wall was simply bought. Full rate to the soft point, half rate
// after, capped: pure and deterministic, so it is checked directly rather than through a
// fight that would only ever sample a few points of it.
void test_defense_diminishing_returns() {
    CHECK(levelDefenseCutPct(0) == 0);
    CHECK(levelDefenseCutPct(-4) == 0);                    // negatives are not a refund
    // Below the bend, a point is worth exactly its full rate — early Defence is untouched
    // by this curve, which is the point of putting the soft point above the mid game.
    for (int p = 1; p <= kLevelDefenseSoftPoints; ++p)
        CHECK(levelDefenseCutPct(p) == p * kLevelDefensePctPerPoint);
    // Past it, each point buys half as much...
    const int atSoft = levelDefenseCutPct(kLevelDefenseSoftPoints);
    CHECK(levelDefenseCutPct(kLevelDefenseSoftPoints + 2) ==
          atSoft + kLevelDefensePctPerPoint);              // 2 bent points = 1 full one
    // ...strictly monotonic while it climbs (a curve that stalls flat reads as a bug),
    // and never past the ceiling however many points are poured in.
    int prev = 0;
    for (int p = 1; p <= 400; ++p) {
        const int cut = levelDefenseCutPct(p);
        CHECK(cut >= prev);
        CHECK(cut <= kLevelDefenseCapPct);
        prev = cut;
    }
    CHECK(levelDefenseCutPct(400) == kLevelDefenseCapPct); // the ceiling is reachable
    // And the ceiling really is lower than the old flat rate would have given: the whole
    // change is that the stretch heading for immunity now costs double.
    CHECK(levelDefenseCutPct(40) < 40 * kLevelDefensePctPerPoint);
}

// --- Gate: a bonus the caps refuse is paid, not dropped ---
//
// A clamp is a promise about the ceiling. Before this, a pet at the never-immune cut got
// literally nothing from its next Defence point, mod or absorbed move and no screen said
// so — the row still read as if it paid. The discard now converts to max-Health at the
// level table's own exchange (capOverflowHealth), and the caps do not move.
void test_full_cap_overflows_into_health() {
    // The curve and its overflow PARTITION the uncapped curve — nothing is counted twice
    // and nothing goes missing, at every point count either side of the ceiling.
    for (int p = 0; p <= 400; ++p) {
        CHECK(levelDefenseCutPct(p) <= kLevelDefenseCapPct);
        CHECK(levelDefenseCutOverflowPct(p) >= 0);
        if (levelDefenseCutPct(p) < kLevelDefenseCapPct)
            CHECK(levelDefenseCutOverflowPct(p) == 0);     // under the cap, nothing spills
    }
    CHECK(levelDefenseCutOverflowPct(400) > 0);            // ...and over it, something does

    // The exchange: what the same investment would have bought spent on max-Health, and
    // never more. Overflowing must not be the better outcome.
    CHECK(capOverflowHealth(0, kLevelDefensePctPerPoint) == 0);
    CHECK(capOverflowHealth(-9, kLevelDefensePctPerPoint) == 0);   // not a refund either
    CHECK(capOverflowHealth(kLevelDefensePctPerPoint, kLevelDefensePctPerPoint) ==
          kLevelHealthPerPoint);                           // one point's worth, either way
    CHECK(capOverflowHealth(2 * kLevelDefensePctPerPoint, kLevelDefensePctPerPoint) ==
          2 * kLevelHealthPerPoint);

    // End to end, on a built fighter. A pet buried in Defence points sits at the same
    // wall as one merely at it — the caps do not move — and carries the difference as
    // body instead.
    auto build = [](int defensePoints) {
        Combatant c;
        c.maxHealth = 100;
        int points[kLevelStatCount] = {0, defensePoints, 0, 0};
        applyLevelStatPoints(c, points);
        return c;
    };
    const Combatant at = build(40);
    const Combatant past = build(400);
    CHECK(at.dmgReducePct == past.dmgReducePct);           // the ceiling is exactly where it was
    CHECK(at.defenseMultPct <= 100 + kLevelDefenseBraceCapPct);
    CHECK(past.defenseMultPct == 100 + kLevelDefenseBraceCapPct);
    CHECK(past.maxHealth > at.maxHealth);                  // the discard arrived as body
    CHECK(past.health == past.maxHealth);                  // ...and the pet may stand in it

    // A pet UNDER every ceiling is untouched: this pays a discard, and there is no
    // discard to pay until something is actually refused.
    Combatant plain;
    plain.maxHealth = 100;
    int few[kLevelStatCount] = {0, 1, 0, 0};
    applyLevelStatPoints(plain, few);
    CHECK(plain.maxHealth == 100);
}

// Rollback: opens a stat picker; a confirm sheds one earned point (−1 that
// stat, −1 level), zeroes the XP bucket (re-grind to re-roll), and consumes the item.
// Inert at level 0 (nothing to shed).
void test_rollback_item() {
    // Level-0 gate: Rollback is inert with no earned points — no picker, item kept.
    { Game g{StartMode::Hatched, "paypup"};
      g.inventory().add("rollback", 1);
      g.debugUseItem("rollback");
      CHECK(g.nav() != Game::Nav::RollbackPicker);
      CHECK(g.inventory().count("rollback") == 1); }   // not consumed

    // Exactly one level → exactly one eligible stat; the picker parks on it, B sheds.
    { Game g{StartMode::Hatched, "paypup"};
      g.debugAddCombatXp(kLevelXpBase);                 // → level 1, one point somewhere
      CHECK(g.combatLevel() == 1);
      const int shed = g.lastLevelUpStat();
      CHECK(g.levelStatPoint(shed) == 1);
      g.inventory().add("rollback", 1);
      g.debugUseItem("rollback");
      CHECK(g.nav() == Game::Nav::RollbackPicker);
      g.onButton(press(Button::B));                     // shed the (only) eligible stat
      CHECK(g.combatLevel() == 0);                      // −1 level
      CHECK(g.levelStatPoint(shed) == 0);               // −1 that stat
      CHECK(g.combatXp() == 0);                         // XP re-zeroed (re-grind)
      CHECK(g.inventory().count("rollback") == 0);      // item consumed
      CHECK(g.nav() == Game::Nav::Submenu); }

    // C cancels without consuming the item or touching the level.
    { Game g{StartMode::Hatched, "paypup"};
      g.debugAddCombatXp(kLevelXpBase);
      g.inventory().add("rollback", 1);
      g.debugUseItem("rollback");
      CHECK(g.nav() == Game::Nav::RollbackPicker);
      tapC(g);
      CHECK(g.combatLevel() == 1);
      CHECK(g.inventory().count("rollback") == 1); }
}

// Levels are PER-PET: they persist through an evolution (same creature) but
// a new egg resets them to 0. Also exercises the save v11 round-trip of the points.
void test_creature_level_persist_evolution_reset_egg() {
    Game g{StartMode::Hatched, "paypup"};
    g.debugAddCombatXp(1200);
    const int lvl = g.combatLevel();
    CHECK(lvl >= 1);
    int pts[kLevelStatCount];
    for (int i = 0; i < kLevelStatCount; ++i) pts[i] = g.levelStatPoint(i);

    // Evolve Process → Script: the level + points carry (it's the same creature).
    uint32_t t = 0;
    g.debugTriggerEvolution();
    advanceToReveal(g, t);
    g.onButton(press(Button::B));
    CHECK(g.pet()->stage == Stage::Script);
    CHECK(g.combatLevel() == lvl);
    for (int i = 0; i < kLevelStatCount; ++i) CHECK(g.levelStatPoint(i) == pts[i]);

    // A new egg (resetToHatch) starts the level system fresh at 0.
    g.resetToHatch();
    pickFirstEggLine(g);                            // reset re-enters line-select
    CHECK(g.combatLevel() == 0 && g.combatXp() == 0);
    for (int i = 0; i < kLevelStatCount; ++i) CHECK(g.levelStatPoint(i) == 0);

    // Save v11 round-trips the stat points (sum == level).
    SaveData a; a.combatLevel = 3; a.combatXp = 42;
    a.statPoints = {1, 0, 2, 0}; a.hasLevelData = true;
    SaveData b;
    CHECK(deserializeSave(serializeSave(a), b));
    CHECK(b.hasLevelData && b.combatLevel == 3 && b.combatXp == 42);
    CHECK(b.statPoints.size() == 4 && b.statPoints[0] == 1 && b.statPoints[2] == 2);
}

// Regression: ARCH Store/Deploy round-trips a pet's creature level (combatLevel_/
// combatXp_/statPoints_/slotKinds_), not just its care stats. Before the fix,
// SaveStoredPet never carried these fields, so a leveled pet silently reset to
// level 0 on Store -> Deploy. Levels a pet, stores it, deploys a placeholder in
// its place, then deploys the leveled pet back out and confirms level/XP/points
// survived the round-trip through the rack.
void test_arch_store_deploy_preserves_creature_level() {
    Game g{StartMode::Hatched, "paypup"};
    g.debugAddCombatXp(1200);                              // level the active pet up
    const int lvl = g.combatLevel();
    const int xp = g.combatXp();
    CHECK(lvl >= 1);
    int pts[kLevelStatCount];
    for (int i = 0; i < kLevelStatCount; ++i) pts[i] = g.levelStatPoint(i);

    // Store the leveled Paypup into the rack; a fresh egg becomes active. Driven off
    // ARCH's NEW EGG row, which is what a player reaches for and what the Store on the
    // active pet's record has always actually been.
    enterArchNewEgg(g);
    pickFirstEggLine(g);                                   // vacated -> line-select; pick Ransomware
    CHECK(g.rackCount() == 1 && g.inEggPhase());
    // The new egg's own (fresh) level state must not bleed into the stored pet.
    CHECK(g.combatLevel() == 0 && g.combatXp() == 0);

    // Deploy the leveled Paypup back out of the rack.
    enterArchStoredPet(g, "paypup");
    archConfirmAction(g);                                  // Deploy -> confirm -> commit
    CHECK(g.nav() == Game::Nav::Idle);
    CHECK(g.pet() && std::strcmp(g.pet()->id, "paypup") == 0);

    // The level, XP bucket, and every earned stat point survived the Store/Deploy
    // round-trip through the rack unchanged.
    CHECK(g.combatLevel() == lvl);
    CHECK(g.combatXp() == xp);
    for (int i = 0; i < kLevelStatCount; ++i) CHECK(g.levelStatPoint(i) == pts[i]);
}

// The move + mod loadout is per-pet: deploying a different rack pet must not
// carry the outgoing pet's TRAIN/MODS state onto it, and deploying the original
// pet back out must restore its own.
void test_arch_deploy_loadout_is_per_pet() {
    Game g{StartMode::Hatched, "paypup"};   // ransomware line
    const char* paypupMove = g.moveLoadout().equipped(0);
    CHECK(paypupMove != nullptr);
    CHECK(std::strcmp(g.loadout().equipped(0), "firewall_patch") == 0);

    g.debugSeedRack("tadpoll");             // phishing line
    CHECK(g.rackCount() == 1);

    enterArchStoredPet(g, "tadpoll");
    archConfirmAction(g);                   // Deploy -> confirm -> commit
    CHECK(g.pet() && std::strcmp(g.pet()->id, "tadpoll") == 0);

    for (int i = 0; i < kMaxMoveSlots; ++i) {
        const char* eq = g.moveLoadout().equipped(i);
        CHECK(!eq || std::strcmp(eq, paypupMove) != 0);
    }
    for (int i = 0; i < kModSlots; ++i) CHECK(g.loadout().equipped(i) == nullptr);

    enterArchStoredPet(g, "paypup");
    archConfirmAction(g);                   // Deploy -> confirm -> commit
    CHECK(g.pet() && std::strcmp(g.pet()->id, "paypup") == 0);
    CHECK(std::strcmp(g.moveLoadout().equipped(0), paypupMove) == 0);
    CHECK(std::strcmp(g.loadout().equipped(0), "firewall_patch") == 0);
}

// Auto-battle paces itself: an autonomous turn resolves only every
// kCombatBeatsPerTurn heartbeats (readable exchanges), while the A "SKIP"
// fast-forward steps immediately regardless of the pace.
void test_combat_auto_pacing() {
    { Game g{StartMode::Hatched, "paypup"};
      g.debugStartCombat(/*live=*/false);
      CHECK(g.nav() == Game::Nav::Combat);
      CHECK(g.combat().lastMoveName()[0] == '\0');   // no turn yet
      uint32_t t = 0;
      for (int i = 0; i < kCombatBeatsPerTurn - 1; ++i) g.tick(t += kHeartbeatMs);
      CHECK(g.combat().lastMoveName()[0] == '\0');    // still waiting out the pace
      g.tick(t += kHeartbeatMs);                       // the kCombatBeatsPerTurn-th beat
      CHECK(g.combat().lastMoveName()[0] != '\0'); }   // one turn resolved

    // A fast-forwards without waiting for the pace.
    { Game g{StartMode::Hatched, "paypup"};
      g.debugStartCombat(/*live=*/false);
      g.onButton(press(Button::A));
      CHECK(g.combat().lastMoveName()[0] != '\0'); }
}

// Combat-length balance: a typical fight should resolve in ~4–8
// exchanges (an exchange = both actors act ≈ 2 resolved turns). The per-stage
// offensive scale (kStagePowerScalePct) keeps player output pacing tier-scaled
// enemy Health so higher stages don't drag. We sample many seeded fights per
// representative matchup and assert the MEDIAN exchange count lands in-band —
// individual seeds spread wider (that variance is intended), so the median is the
// contract, not any single fight.
static int medianExchanges(const Combatant& player, const CombatEnemy& enemySpec,
                           const ContentRegistry& reg) {
    std::vector<int> ex;
    for (uint32_t s = 1; s <= 200; ++s) {
        Combatant e = makeEnemyCombatant(reg, enemySpec);
        Combat cb;
        cb.begin(player, e, Combat::Stakes::Live, s * 2654435761u + 1);
        int turns = 0, guard = 0;
        while (cb.outcome() == Combat::Outcome::Ongoing && guard++ < 5000)
            if (cb.step()) ++turns;
        ex.push_back((turns + 1) / 2);            // turns -> exchanges (ceil)
    }
    std::sort(ex.begin(), ex.end());
    return ex[ex.size() / 2];
}
void test_combat_length_in_band() {
    ContentRegistry r = ContentRegistry::embedded();
    MoveLoadout ml = MoveLoadout::starting();
    Loadout mods = Loadout::starting();
    // Process / Script / Daemon each vs their sector-tier wild malbeast (both
    // variants), and the two sector bosses. Wilds carry the challenge buff
    // (kWildEnemy*Pct — +Health lengthens the fight), so their median sits in the
    // wider [4, 10] band; bosses are unbuffed and hold the tighter [4, 8].
    struct Case { const char* pet; int tier; };
    const Case wilds[] = {{"paypup", 1}, {"malbear", 2}, {"bruinforce", 3}};
    for (const Case& c : wilds) {
        Combatant p = makePlayerCombatant(r, *r.creature(c.pet), ml, mods);
        for (int v = 0; v < 2; ++v) {
            int m = medianExchanges(p, wildMalbeast(c.tier, v), r);
            // Speed-weighted turns let a faster pet land more actions per exchange, so an
            // early fast pet can close a fight a touch quicker — floor drops to 3.
            CHECK(m >= 3 && m <= 10);
        }
    }
    // The signature (apex) sub-area boss of each area vs a stage-appropriate pet: the
    // apex is "the wall," so it holds a slightly wider [4, 12] band than a wild.
    Combatant script = makePlayerCombatant(r, *r.creature("malbear"), ml, mods);
    Combatant daemon = makePlayerCombatant(r, *r.creature("bruinforce"), ml, mods);
    int mb0 = medianExchanges(script, subAreaBoss(0, kExplSubAreas - 1).rounds[0], r);
    int mb1 = medianExchanges(daemon, subAreaBoss(1, kExplSubAreas - 1).rounds[0], r);
    CHECK(mb0 >= 4 && mb0 <= 12);
    CHECK(mb1 >= 4 && mb1 <= 12);
}

// Wild-encounter challenge buff (kWildEnemy*Pct). Two contracts: (1) the buff is
// wild-only — a wild spec builds with boosted Health + a >100 damage mult, an
// otherwise-identical non-wild spec (bosses/Sim reuse the shape) stays neutral;
// (2) it bites — a Process pet still wins the sector-0 wild, but ends visibly
// wounded (well under the near-untouched ~86% the flat base stats produced).
void test_wild_encounter_challenge_buff() {
    ContentRegistry r = ContentRegistry::embedded();
    // (1) Build the same base spec wild vs non-wild and compare.
    CombatEnemy base{"T", "SPR_PET_CACHEMUTT", 1, 40, 9, {"quick_jab"}, false};
    CombatEnemy wild = base; wild.isWild = true;
    Combatant nb = makeEnemyCombatant(r, base);
    Combatant wb = makeEnemyCombatant(r, wild);
    CHECK(nb.enemyDamageMultPct == 100 && nb.maxHealth == 40);        // neutral
    CHECK(wb.enemyDamageMultPct == kWildEnemyDamagePct);              // hits harder
    CHECK(wb.maxHealth == 40 * kWildEnemyHealthPct / 100);           // soaks more
    // (2) Win still lands, but at a real Health cost. Sample many seeds; the pet
    // must win the vast majority AND the median ending Health must be a clear dent.
    MoveLoadout ml = MoveLoadout::starting();
    Loadout mods = Loadout::starting();
    Combatant p = makePlayerCombatant(r, *r.creature("paypup"), ml, mods);
    int wins = 0; std::vector<int> endPct;
    for (uint32_t s = 1; s <= 200; ++s) {
        Combatant pc = p;
        Combatant e = makeEnemyCombatant(r, wildMalbeast(1, s % 2));
        Combat cb; cb.begin(pc, e, Combat::Stakes::Live, s * 2654435761u + 1);
        int g = 0;
        while (cb.outcome() == Combat::Outcome::Ongoing && g++ < 5000) cb.step();
        if (cb.outcome() == Combat::Outcome::Win) {
            ++wins;
            endPct.push_back(100 * cb.player().health / cb.player().maxHealth);
        }
    }
    CHECK(wins >= 190);                          // reliable win (≥95%)
    std::sort(endPct.begin(), endPct.end());
    const int medEnd = endPct[endPct.size() / 2];
    // A real dent, not a scratch. A fully-equipped Process pet (both slots filled)
    // doesn't dilute its pool with the weak Quick Jab default, so its attack/defend
    // split is an even 1/1 (50% atk) — enough defend weight that it ends measurably
    // tankier (=80 at the median).
    CHECK(medEnd <= 82);
}

// Wild win-rate for a given player vs a fully-built wild spec, sampled over many
// seeds. Mirrors the auto-explore path (Live stakes, hands-off resolve).
static int wildWinPct(const Combatant& player, const CombatEnemy& spec,
                      const ContentRegistry& reg) {
    int wins = 0; const int N = 300;
    for (uint32_t s = 1; s <= (uint32_t)N; ++s) {
        Combatant p = player;
        Combatant e = makeEnemyCombatant(reg, spec);
        Combat cb; cb.begin(p, e, Combat::Stakes::Live, s * 2654435761u + 1);
        int g = 0;
        while (cb.outcome() == Combat::Outcome::Ongoing && g++ < 5000) cb.step();
        if (cb.outcome() == Combat::Outcome::Win) ++wins;
    }
    return 100 * wins / N;
}

// The (area, sub) difficulty ramp: auto-explore must get
// HARDER through a sector's sub-areas, "steep/gated" so a fresh Process pet clears
// only the first sub-area or two before losses start cancelling runs. Three contracts:
//   (1) the ramp mutates the moveset for sub>0, and at sub 0 leaves the roster kit
//       alone apart from this area's own Attack (which every rung fields);
//   (2) a fresh Paypup's win-rate is monotonic non-increasing across sub 0..4;
//   (3) it clears the early sub-areas (sub 0 near-certain) yet walls out by the apex
//       (sub 4 a hard gate) — a real, steep spread, not a flat line.
// Does this spec's kit name `id`? Compares by VALUE, unlike the whole-kit checks below
// which compare the borrowed pointers a spec actually carries.
static bool holdsMove(const CombatEnemy& e, const char* id) {
    if (!id) return false;
    for (const char* m : e.moveIds)
        if (std::strcmp(m, id) == 0) return true;
    return false;
}

// --- Gate: a malbeast is its own creature at every depth ---
//
// Two bodies sharing a tier used to carry identical kits, so no two wilds were worth
// farming differently — a win teaches out of the beaten enemy's own kit
// (rollEnemyMoveDrop), and there was nothing on one that was not on the other. The
// signature is what a creature says about itself; this holds it to the three things that
// makes true.
void test_wild_signature_is_the_creatures_own() {
    ContentRegistry r = ContentRegistry::embedded();

    // ONE EACH, ALL DIFFERENT, ALL LEARNABLE. A line-exclusive signature would be a prize
    // half the roster could never field (moveIsTeachable), and a stun in an enemy kit
    // spends the PLAYER's turns doing nothing (fork_bomb's row says why).
    std::vector<const char*> sigs;
    for (int tier = 1; tier <= 3; ++tier)
        for (uint32_t v = 0; v < 2; ++v) {
            const CombatEnemy e = wildMalbeast(tier, v);
            CHECK(e.signatureMoveId != nullptr);
            const MoveDef* m = r.move(e.signatureMoveId);
            CHECK(m != nullptr);
            CHECK(m->line == nullptr);
            CHECK(m->lockTurns == 0);
            for (const char* prev : sigs)
                CHECK(std::strcmp(prev, e.signatureMoveId) != 0);
            sigs.push_back(e.signatureMoveId);
        }
    CHECK(static_cast<int>(sigs.size()) == kWildMalbeastCount);

    // IT SURVIVES THE DEPTH LADDER, which replaces moveIds outright — the whole reason it
    // is carried beside that list rather than in it. Every creature, every area, every
    // rung: a signature that died at sub 1 would leave the identity where it started.
    for (int tier = 1; tier <= 3; ++tier)
        for (uint32_t v = 0; v < 2; ++v)
            for (int a = 0; a < kExplSectors; ++a)
                for (int sub = 0; sub < kSubAreasPerArea; ++sub) {
                    CombatEnemy w = wildMalbeast(tier, v);
                    applyWildSubAreaRamp(w, a, sub);
                    CHECK(holdsMove(w, w.signatureMoveId));
                }

    // ...AND THE RUNG ORDER IS INTACT. The ladder is sorted by EFFECTIVE per-turn damage,
    // and a rider appended to every rung alike cannot reorder them — this is what says so
    // for every creature in every area rather than for the one pairing checked by hand.
    // Attacks only, and a channelled move at its per-turn rate: a brace mitigates rather
    // than swings, and a wind-up spreads its printed power over the turns it costs.
    auto swing = [&](const CombatEnemy& e) {
        int total = 0, n = 0;
        for (const char* id : e.moveIds)
            if (const MoveDef* m = r.move(id))
                if (m->kind == MoveDef::Kind::Attack) {
                    total += m->power / (m->channelTurns > 0 ? m->channelTurns : 1);
                    ++n;
                }
        return n > 0 ? total * 100 / n : 0;
    };
    for (int tier = 1; tier <= 3; ++tier)
        for (uint32_t v = 0; v < 2; ++v)
            for (int a = 0; a < kExplSectors; ++a) {
                int prev = -1;
                for (int sub = 1; sub < kSubAreasPerArea; ++sub) {
                    CombatEnemy w = wildMalbeast(tier, v);
                    applyWildSubAreaRamp(w, a, sub);
                    const int avg = swing(w);
                    CHECK(avg > prev);                  // each rung swings harder
                    prev = avg;
                }
            }
}

void test_explore_subarea_ramp() {
    ContentRegistry r = ContentRegistry::embedded();
    // (1) Shape: sub 0 keeps the roster moves; each later sub swaps in a distinct,
    // heavier kit. Compare against the untouched baseline spec. The area's own wild
    // Attack rides at EVERY rung including this one, because the pair states where the
    // player is rather than how deep — so the sub-0 kit is the baseline plus that one id,
    // and the Defend (kWildAreaDefendSub) is the half depth actually gates.
    CombatEnemy base = wildMalbeast(1, 0);
    CombatEnemy s0 = base; applyWildSubAreaRamp(s0, 0, 0);
    std::vector<const char*> baseline = base.moveIds;
    baseline.push_back(area(0).wildAttackMoveId);
    baseline.push_back(base.signatureMoveId);          // ...and the creature's own
    CHECK(s0.moveIds == baseline);                     // sub 0 == baseline + both riders
    CHECK(!holdsMove(s0, area(0).wildDefendMoveId));   // ...and not yet the Defend
    CombatEnemy s2 = base; applyWildSubAreaRamp(s2, 0, kWildAreaDefendSub);
    CHECK(holdsMove(s2, area(0).wildDefendMoveId));    // which the deeper rungs do field
    CombatEnemy s4 = base; applyWildSubAreaRamp(s4, 0, 4);
    CHECK(s4.moveIds != base.moveIds);                 // apex is re-kitted
    // (2)+(3) Win-rate curve for a fresh Process pet across area 0's sub-areas. Average
    // the two roster variants per sub (their spread is intended), then require a
    // non-increasing curve with a near-certain floor and a hard-gated apex.
    MoveLoadout ml = MoveLoadout::starting();
    Loadout mods = Loadout::starting();
    Combatant paypup = makePlayerCombatant(r, *r.creature("paypup"), ml, mods);
    int win[kExplSubAreas];
    for (int sub = 0; sub < kExplSubAreas; ++sub) {
        int acc = 0;
        for (int v = 0; v < 2; ++v) {
            CombatEnemy e = wildMalbeast(1, v);
            applyWildSubAreaRamp(e, 0, sub);
            acc += wildWinPct(paypup, e, r);
        }
        win[sub] = acc / 2;
    }
    for (int sub = 1; sub < kExplSubAreas; ++sub)
        CHECK(win[sub] <= win[sub - 1] + 2);           // monotonic (small sampling slack)
    CHECK(win[0] >= 95);                               // sub 0 is a near-certain clear
    CHECK(win[kExplSubAreas - 1] <= 20);              // apex is a hard gate for a Process pet
    CHECK(win[0] - win[kExplSubAreas - 1] >= 60);     // a real, steep spread (not flat)
}

// Line-identity combat -------------------------

// Line-gating: a move is learnable/equippable iff generic (no line) or same line.
// This is what makes a Ransomware move exclusive to Ransomware pets — the whole point
// of adding Phishing moves no current pet can touch.
void test_line_move_gating() {
    ContentRegistry r = ContentRegistry::embedded();
    // Generic moves stay open to everyone (incl. a lineless creature).
    CHECK(moveAllowedForLine(*r.move("quick_jab"), "ransomware"));
    CHECK(moveAllowedForLine(*r.move("buffer_overflow"), "phishing"));
    CHECK(moveAllowedForLine(*r.move("quick_jab"), nullptr));
    // Ransomware line moves: only Ransomware pets.
    CHECK(moveAllowedForLine(*r.move("payload_drop"), "ransomware"));
    CHECK(!moveAllowedForLine(*r.move("payload_drop"), "phishing"));
    CHECK(!moveAllowedForLine(*r.move("aes_lockbox"), nullptr));
    // Phishing line moves: never a Ransomware pet (the exclusivity proof).
    CHECK(moveAllowedForLine(*r.move("smish_hook"), "phishing"));
    CHECK(!moveAllowedForLine(*r.move("smish_hook"), "ransomware"));
    CHECK(!moveAllowedForLine(*r.move("bathyspoof"), "ransomware"));
    // The creature actually carries the line tag (Paypup = Ransomware).
    CHECK(r.creature("paypup")->line && std::strcmp(r.creature("paypup")->line,
                                                    "ransomware") == 0);
}

// Ransomware Lockout track stacks the caster's Power on each landed hit (capped), and
// Cipher track stacks Defense on each cast; the defend brace also scales with the
// caster's Defense-derived defenseMultPct. All transient (wiped every Combat::begin).
void test_ransomware_stacking() {
    ContentRegistry r = ContentRegistry::embedded();
    // Lockout: Payload Drop only (single move → cast every player turn) vs a fat dummy.
    Combatant atk; atk.name = "R"; atk.maxHealth = 300; atk.health = 300; atk.speed = 10;
    atk.setLine(r, "ransomware"); atk.stage = Stage::Process;
    atk.moves.push_back(r.move("payload_drop"));
    Combatant dummy = mkCombatant(r, "D", 500, 3, {"quick_jab"});
    Combat cb; cb.begin(atk, dummy, Combat::Stakes::Safe, 4242);
    int firstHit = -1, lastHit = 0;
    for (int i = 0; i < 40 && cb.outcome() == Combat::Outcome::Ongoing; ++i) {
        const bool pturn = cb.playerTurnNext();
        cb.step();
        if (pturn && cb.lastByPlayer() && cb.lastDamage() > 0) {
            if (firstHit < 0) firstHit = cb.lastDamage();
            lastHit = cb.lastDamage();
        }
    }
    CHECK(cb.player().stackPowerBonus == 24);          // +8 ×3, capped at +24%
    CHECK(lastHit > firstHit);                          // stacked Power hits harder

    // Cipher, and the INVERTED ladder that carries it. Where a row's cap sits decides how
    // soon it can arm a seizure (RansomSeizure), so the deep row caps LOW in one step and
    // the shallow row climbs in small ones to a much higher ceiling: the endgame row is the
    // fast one and the beginner's row is the long game.
    auto cipherRun = [&](const char* moveId, int steps) {
        Combatant def; def.name = "W"; def.maxHealth = 300; def.health = 300; def.speed = 20;
        def.setLine(r, "ransomware"); def.stage = Stage::Process; def.defenseMultPct = 200;
        def.moves.push_back(r.move(moveId));
        Combatant poke = mkCombatant(r, "P", 300, 1, {"quick_jab"});
        Combat cd; cd.begin(def, poke, Combat::Stakes::Safe, 99);
        cd.step();                                      // faster defender braces first
        const int afterOne = cd.player().stackDefenseBonus;
        const int guard = cd.player().guard;
        for (int i = 0; i < steps && cd.outcome() == Combat::Outcome::Ongoing; ++i) cd.step();
        return std::array<int, 3>{guard, afterOne, cd.player().stackDefenseBonus};
    };
    // Deep row: one cast IS the whole bar, which is what makes it the seizure's enabler.
    const auto deep = cipherRun("full_disk_encryption", 12);
    CHECK(deep[0] == 56);                               // 28 base × 200% defenseMult
    CHECK(deep[1] == r.move("full_disk_encryption")->stackDefenseCap);
    CHECK(deep[2] == deep[1]);                          // nothing left to climb
    // Shallow row: a small step, and a ceiling more than twice the deep row's.
    const auto shallow = cipherRun("aes_lockbox", 12);
    CHECK(shallow[0] == 28);                            // 14 base × 200% defenseMult
    CHECK(shallow[1] == r.move("aes_lockbox")->stackDefensePct);   // one small step
    CHECK(shallow[2] == r.move("aes_lockbox")->stackDefenseCap);    // ...eventually full
    CHECK(shallow[2] > deep[2]);                        // the long game builds the bigger wall
}

// Ransom Note: the Ransomware passive HOLDS the damage of hits taken inside an armed
// window, banks it in a pool, and drops the whole pool at once when the pool's countdown
// of the ransomer's own turns runs out. It defers damage, it never erases it.
void test_ransom_note() {
    ContentRegistry r = ContentRegistry::embedded();
    auto makeRansomer = [&](const char* line, Stage stage, float speed) {
        Combatant p; p.name = "R"; p.maxHealth = 5000; p.health = 5000; p.speed = speed;
        p.setLine(r, line); p.stage = stage;
        p.moves.push_back(r.move("quick_jab"));
        return p;
    };

    // (1) The window itself, scripted. Boot's 0% arm chance means the roll never fires on
    //     its own (and draws no rng), so an armed window handed in is the only one — and
    //     begin() must preserve rather than re-roll it. A speed-1 pet against a speed-20
    //     enemy takes a run of hits before it ever acts: the FIRST is caught and closes
    //     the window, the rest land normally.
    Combatant p = makeRansomer("ransomware", Stage::BootSector, 1);
    p.ransomArmed = true;
    Combatant e = mkCombatant(r, "E", 5000, 20, {"quick_jab"});
    Combat cb; cb.begin(p, e, Combat::Stakes::Safe, 4242);
    CHECK(cb.player().ransomArmed);                     // begin() kept the handed-in window
    cb.step();
    const int held = cb.lastDamage();
    CHECK(cb.lastRansomed());                           // the hit it caught is held...
    CHECK(held > 0);
    CHECK(cb.player().ransomPool == held);              // ...into the pool, in full
    CHECK(cb.player().health == 5000);                  // ...and Health never moved
    CHECK(cb.player().ransomTurnsLeft == kRansomHoldTurns);
    CHECK(!cb.player().ransomArmed);                    // the window closed on that hit
    while (cb.outcome() == Combat::Outcome::Ongoing && !cb.playerTurnNext()) {
        const int before = cb.player().health;
        cb.step();
        CHECK(!cb.lastRansomed());                      // window shut: later hits land
        CHECK(cb.player().health == before - cb.lastDamage());
        CHECK(cb.player().ransomPool == held);          // ...and don't join the pool
    }
    // The countdown burns the ransomer's OWN turns: two tick by, then the third drops the
    // whole pool at once and costs that turn. The window is closed the whole time (Boot
    // re-rolls to false), so the enemy's hits in between land normally.
    auto upToRansomerTurn = [&] {   // stop with the ransomer's own turn pending
        while (cb.outcome() == Combat::Outcome::Ongoing && !cb.playerTurnNext()) cb.step();
    };
    upToRansomerTurn();  cb.step();  CHECK(cb.player().ransomTurnsLeft == 2);
    CHECK(cb.player().ransomPool == held);              // still owed, not yet paid
    upToRansomerTurn();  cb.step();  CHECK(cb.player().ransomTurnsLeft == 1);
    upToRansomerTurn();
    const int beforeBill = cb.player().health;          // the enemy's in-between hits landed
    cb.step();
    CHECK(std::strcmp(cb.lastMoveName(), "RANSOM DUE") == 0);
    CHECK(cb.lastDamage() == held);                     // the lump lands whole...
    CHECK(cb.player().health == beforeBill - held);     // ...and it is real damage
    CHECK(cb.player().ransomPool == 0);
    CHECK(cb.player().ransomTurnsLeft == 0);

    // (2) Riders are NOT ransomed — only the damage number is. A stun move landing inside
    //     an armed window still freezes the ransomer even though its damage was held.
    Combatant s = makeRansomer("ransomware", Stage::BootSector, 1);
    s.ransomArmed = true;
    Combatant st = mkCombatant(r, "E", 5000, 20, {"system_hang"});   // lockTurns rider
    Combat cs; cs.begin(s, st, Combat::Stakes::Safe, 11);
    cs.step();
    CHECK(cs.player().ransomPool > 0);                  // damage held...
    CHECK(cs.player().lockedTurnsLeft > 0);             // ...stun applied anyway

    // (3) Deferred, never erased — the invariant, across a real fight at a real arm rate.
    //     At every point: Health lost == damage dealt to the ransomer minus what the pool
    //     is still holding. Also asserts the passive actually fires at Daemon's rate.
    int fightsWithHold = 0, fightsWithBill = 0;
    for (uint32_t seed = 1; seed <= 60; ++seed) {
        Combatant d = makeRansomer("ransomware", Stage::Daemon, 10);
        Combatant o = mkCombatant(r, "E", 5000, 10, {"quick_jab"});
        Combat cd; cd.begin(d, o, Combat::Stakes::Safe, seed * 2654435761u + 1);
        int dealt = 0; bool sawHold = false, sawBill = false;
        for (int i = 0; i < 80 && cd.outcome() == Combat::Outcome::Ongoing; ++i) {
            cd.step();
            if (!cd.lastByPlayer()) dealt += cd.lastDamage();   // enemy hit the ransomer
            if (cd.lastRansomed()) sawHold = true;
            if (std::strcmp(cd.lastMoveName(), "RANSOM DUE") == 0) sawBill = true;
            CHECK(5000 - cd.player().health == dealt - cd.player().ransomPool);
        }
        if (sawHold) ++fightsWithHold;
        if (sawBill) ++fightsWithBill;
    }
    CHECK(fightsWithHold >= 50);        // fires reliably at 55%/turn over many turns
    CHECK(fightsWithBill >= 50);        // and the bill actually comes due

    // (4) SIDE-AGNOSTIC — the property a linked duel needs. The same ransomer seated in
    //     Combat's ENEMY slot (where a duel guest's pet sits, core/model/pvp_battle.h)
    //     gets the identical passive: its pool fills and its Health holds.
    bool enemySideHeld = false;
    for (uint32_t seed = 1; seed <= 20 && !enemySideHeld; ++seed) {
        Combatant plain = mkCombatant(r, "P", 5000, 10, {"quick_jab"});
        Combatant far = makeRansomer("ransomware", Stage::Daemon, 10);
        Combat cx; cx.begin(plain, far, Combat::Stakes::Safe, seed * 40503u + 7);
        int dealt = 0;
        for (int i = 0; i < 60 && cx.outcome() == Combat::Outcome::Ongoing; ++i) {
            cx.step();
            if (cx.lastByPlayer()) dealt += cx.lastDamage();    // player hit the ransomer
            CHECK(5000 - cx.enemy().health == dealt - cx.enemy().ransomPool);
            if (cx.enemy().ransomPool > 0) enemySideHeld = true;
        }
    }
    CHECK(enemySideHeld);

    // (5) A non-Ransomware pet never holds anything, and the line check short-circuits
    //     before any rng draw — so a fight without the line replays identically.
    Combatant q = makeRansomer(nullptr, Stage::Daemon, 10);
    Combatant e1 = mkCombatant(r, "E", 200, 8, {"buffer_overflow"});
    Combat a; a.begin(q, e1, Combat::Stakes::Safe, 777);
    while (a.outcome() == Combat::Outcome::Ongoing) {
        a.step();
        CHECK(!a.lastRansomed());
        CHECK(a.player().ransomPool == 0);
    }
    Combat c2; c2.begin(makeRansomer(nullptr, Stage::Daemon, 10), e1,
                        Combat::Stakes::Safe, 777);
    while (c2.outcome() == Combat::Outcome::Ongoing) c2.step();
    CHECK(a.player().health == c2.player().health);    // identical replay
}

// A passive nobody ever sees is a passive that doesn't exist. The mechanic working when
// armed says nothing about how often a REAL fight arms it: a healthy Process pet against
// the weakest practice dummy out-speeds it and takes only a couple of hits all fight, so
// the pool has very few chances to fill. This pins the reachable rate at that worst case,
// through the same builders the Game uses, so a future tuning pass can't quietly starve
// the passive back out of sight.
void test_ransom_note_shows_up_in_pve() {
    ContentRegistry r = ContentRegistry::embedded();
    const CreatureDef* pet = r.creature("paypup");           // Process, ransomware
    MoveLoadout ml = MoveLoadout::starting();
    Loadout mods;
    const int kFights = 200;
    int fightsWithPool = 0;
    for (int s = 0; s < kFights; ++s) {
        Combat c;
        c.begin(makePlayerCombatant(r, *pet, ml, mods),
                makeEnemyCombatant(r, simDummy(0)),        // the softest fight in the game
                Combat::Stakes::Safe, s * 2654435761u + 17);
        for (int i = 0; i < 300 && c.outcome() == Combat::Outcome::Ongoing; ++i) {
            c.step();
            if (c.player().ransomPool > 0) { ++fightsWithPool; break; }
        }
    }
    CHECK(fightsWithPool > kFights * 2 / 3);   // seen in most fights, not the odd one
}

// the per-sub-area enemy LEVEL + the level-difference XP scaling.
//   (1) applyWildSubAreaRamp stamps a global depth level (+1 per sub, +5 per area)
//       and thickens Health at the deeper subs (the "rolled level-up stats");
//   (2) wildWinXp scales the flat base by (enemy − pet): parity = base, punching up
//       pays more per level than farming under costs, the under side floors early,
//       both ends clamped, never <1.
void test_wild_subarea_level_and_xp_scaling() {
    // (1) Level is the global rung; deeper subs carry more Health than sub 0.
    CombatEnemy a0s0 = wildMalbeast(1, 0); applyWildSubAreaRamp(a0s0, 0, 0);
    CombatEnemy a0s4 = wildMalbeast(1, 0); applyWildSubAreaRamp(a0s4, 0, 4);
    CombatEnemy a1s0 = wildMalbeast(2, 0); applyWildSubAreaRamp(a1s0, 1, 0);
    CHECK(a0s0.level == 0);
    CHECK(a0s4.level == 4);                             // +1 per sub within a sector
    CHECK(a1s0.level == kSubAreasPerArea);             // +kSubAreasPerArea across areas
    CHECK(a0s4.maxHealth > a0s0.maxHealth);            // deeper sub soaks longer
    // (2) XP scaling. Parity → the flat base; the diff moves it either way, at its own
    //     rate per direction.
    CHECK(wildWinXp(kWildWinXpReward, 3, 3) == kWildWinXpReward);       // diff 0
    CHECK(wildWinXp(kWildWinXpReward, 5, 3) > kWildWinXpReward);        // punching up
    CHECK(wildWinXp(kWildWinXpReward, 1, 5) < kWildWinXpReward);        // farming under
    // ...and ASYMMETRICALLY: the same distance up is worth more than it costs down, so a
    // pet that has outgrown a rung still banks a real share of the base while the ladder
    // pays for depth. Measured in percent (a 3-level swing either side of parity, big
    // enough that the base's integer division can't be what the gate is reading).
    CHECK(kWildXpOverLevelPct > kWildXpUnderLevelPct);
    CHECK(wildWinXp(100, 3, 0) - 100 > 100 - wildWinXp(100, 0, 3));
    // The under side floors EARLY — deep enough under the rung and every further level
    // is free, which is what keeps a cleared area a training ground rather than a
    // rounding error.
    CHECK(wildWinXp(100, 0, 20) == wildWinXp(100, 0, 40));
    CHECK(wildWinXp(100, 0, 20) == kWildXpDiffMinPct);
    // Clamps: an absurd over-level caps at the ceiling; an absurd under-level floors
    // at the min; XP is never below 1.
    CHECK(wildWinXp(kWildWinXpReward, 99, 0) ==
          kWildWinXpReward * kWildXpDiffMaxPct / 100);
    CHECK(wildWinXp(kWildWinXpReward, 0, 99) ==
          kWildWinXpReward * kWildXpDiffMinPct / 100);
    CHECK(wildWinXp(1, 0, 99) >= 1);                    // floored trickle, never zero
}

// evolution-gating on MOVES (not just slots). A move opens at its
// minStage; owning it earlier isn't enough. Three contracts:
//   (1) the pure gate moveUnlockedAtStage();
//   (2) makePlayerCombatant fields an equipped-but-locked move ONLY once evolved
//       (always keeps the innate default);
//   (3) the equip picker refuses a locked move (a Boot cryptoshell owns its whole
//       ransomware kit from hatch, all Process+ gated — it can't slot any of it).
void test_move_evolution_gating() {
    ContentRegistry r = ContentRegistry::embedded();
    // (1) The pure stage gate.
    const MoveDef* bo = r.move("buffer_overflow");   // Script
    const MoveDef* rk = r.move("rootkit_strike");    // Daemon
    const MoveDef* ps = r.move("packet_storm");      // Boot
    CHECK(!moveUnlockedAtStage(*bo, Stage::Process));
    CHECK(moveUnlockedAtStage(*bo, Stage::Script));
    CHECK(!moveUnlockedAtStage(*rk, Stage::Script));
    CHECK(moveUnlockedAtStage(*rk, Stage::Daemon));
    CHECK(moveUnlockedAtStage(*ps, Stage::BootSector));
    // (2) The combat gate. Equip a Script-gated move into an unlocked slot, then build
    // the combatant at Process (locked → inert) vs Script (unlocked → fielded).
    MoveLoadout ml;
    ml.grant("buffer_overflow");
    ml.equip(0, "buffer_overflow");
    Loadout mods;
    auto hasMove = [](const Combatant& c, const char* id) {
        for (const MoveDef* m : c.moves)
            if (std::strcmp(m->id, id) == 0) return true;
        return false;
    };
    Combatant proc = makePlayerCombatant(r, *r.creature("paypup"), ml, mods);   // Process
    Combatant scr  = makePlayerCombatant(r, *r.creature("malbear"), ml, mods);  // Script
    CHECK(!hasMove(proc, "buffer_overflow"));    // locked at Process → not fielded
    CHECK(hasMove(scr, "buffer_overflow"));       // unlocked at Script → fielded
    CHECK(hasMove(proc, "quick_jab") && hasMove(scr, "quick_jab"));  // default always
    // (3) The picker refuses a locked move. cryptoshell (Boot) owns its whole
    // ransomware-line kit from hatch (its nature) — every row gates at Process+,
    // so even the auto-equipped payload_drop is locked at Boot: the default
    // (filtered) view has nothing to offer for slot 0, and showAll reveals
    // payload_drop as a locked, unselectable row.
    Game g{StartMode::Hatched, "cryptoshell"};
    CHECK(std::strcmp(g.moveLoadout().equipped(0), "payload_drop") == 0);
    CHECK(ownedMoveList(r, g.moveLoadout(), g.slotRequiredKind(0), g.pet()->stage,
                        g.pet()->line, 0, /*showAll=*/false)
              .empty());
    enterLoadoutTab(g, 1);
    g.onButton(press(Button::B));                 // open slot 0 picker
    g.onButton(press(Button::A));                 // filtered list is just [unequip]; A stays put
    tapB(g);                                      // B on [unequip] clears the slot -> Submenu
    CHECK(g.moveLoadout().equipped(0) == nullptr);

    g.onButton(press(Button::B));                 // re-open slot 0 picker (resets moveShowAll_)
    g.debugSetMoveShowAll(true);                  // reveal the locked row for this check
    g.onButton(press(Button::A));                 // pick row 1 = payload_drop (locked at Boot)
    tapB(g);                                      // drill in — the page states the gate
    g.onButton(press(Button::B));                 // attempt equip → blocked (no-op)
    CHECK(g.moveLoadout().equipped(0) == nullptr);
}

// --- Move slots: the per-slot pool, and the Attack/Defend type-lock --------

// The combat pool is exactly one entry per UNLOCKED slot: that slot's
// equipped move if present, else the innate default FILLING that slot — not an
// always-additive extra. Unequipped -> default x N; one equipped -> [move] +
// default x (N-1); fully equipped -> zero default entries in the pool.
void test_move_pool_per_slot_fallback() {
    ContentRegistry r = ContentRegistry::embedded();
    Loadout mods;                                  // no mod passives in play
    const CreatureDef* proc = r.creature("paypup");    // Process: 2 slots
    const int slots = MoveLoadout::slotsForStage(proc->stage);
    CHECK(slots == 2);

    { // Unequipped: every slot falls back to the default.
        MoveLoadout ml;                             // owns/equips nothing
        Combatant c = makePlayerCombatant(r, *proc, ml, mods);
        CHECK(static_cast<int>(c.moves.size()) == slots);
        for (const MoveDef* m : c.moves)
            CHECK(std::strcmp(m->id, ml.defaultMove()) == 0);
    }
    { // One slot equipped: [move] + default filling the rest.
        MoveLoadout ml;
        ml.grant("packet_storm");
        ml.equip(0, "packet_storm");
        Combatant c = makePlayerCombatant(r, *proc, ml, mods);
        CHECK(static_cast<int>(c.moves.size()) == slots);
        CHECK(std::strcmp(c.moves[0]->id, "packet_storm") == 0);
        for (int i = 1; i < slots; ++i)
            CHECK(std::strcmp(c.moves[i]->id, ml.defaultMove()) == 0);
    }
    { // Fully equipped: zero default entries — a well-kitted pet never rolls
      // Quick Jab.
        MoveLoadout ml;
        ml.grant("packet_storm");
        ml.grant("checksum_guard");
        ml.equip(0, "packet_storm");
        ml.equip(1, "checksum_guard");
        Combatant c = makePlayerCombatant(r, *proc, ml, mods);
        CHECK(static_cast<int>(c.moves.size()) == slots);
        for (const MoveDef* m : c.moves)
            CHECK(std::strcmp(m->id, ml.defaultMove()) != 0);
    }
}

// Type-lock: each move slot is permanently typed Attack/Defend (stamped by
// Game::stampSlotKinds from CreatureDef::slotKinds); ownedMoveList — exactly what
// onMovePicker/drawMovePicker read — is filtered to that kind, so a mismatched
// move is structurally UNREACHABLE through the UI (not merely dimmed), while a
// matching one equips normally. Malbear's seed stamps slot0/1 Attack and slot2
// Defend — asserted as literals here because the scenario NEEDS one slot of each
// kind on the same pet; if that stops holding, this test has nothing left to
// exercise and should fail rather than quietly test one kind twice. (Contrast
// test_move_slot_stamping_locks_at_unlock, which reads seeds from the row so a
// balance tweak can't break it.)
void test_move_slot_type_lock() {
    Game g{StartMode::Hatched, "malbear"};
    CHECK(g.slotKind(0) == Game::SlotKind::Attack);
    CHECK(g.slotKind(1) == Game::SlotKind::Attack);
    CHECK(g.slotKind(2) == Game::SlotKind::Defend);
    CHECK(g.slotRequiredKind(0) == MoveDef::Kind::Attack);
    CHECK(g.slotRequiredKind(2) == MoveDef::Kind::Defend);

    ContentRegistry r = ContentRegistry::embedded();
    // Malbear owns its whole ransomware-line kit from hatch (line-native starter,
    // startingForLine): 3 Attack rows (payload_drop/double_extortion/mbr_wipe) and
    // 3 Defend rows (aes_lockbox/rsa_vault/full_disk_encryption). The Attack slot's
    // candidate list excludes every owned Defend move and vice-versa.
    auto atkList = ownedMoveList(r, g.moveLoadout(), g.slotRequiredKind(0),
                                 g.pet()->stage, g.pet()->line, 0, /*showAll=*/true);
    for (const MoveDef* m : atkList) CHECK(m->kind == MoveDef::Kind::Attack);
    bool atkListHasDefend = false;
    for (const MoveDef* m : atkList)
        if (std::strcmp(m->id, "aes_lockbox") == 0) atkListHasDefend = true;
    CHECK(!atkListHasDefend);                        // rejected — never even listed
    auto defList = ownedMoveList(r, g.moveLoadout(), g.slotRequiredKind(2),
                                 g.pet()->stage, g.pet()->line, 2, /*showAll=*/true);
    CHECK(defList.size() == 3);
    for (const MoveDef* m : defList) CHECK(m->kind == MoveDef::Kind::Defend);

    // Drive the real picker for slot 2 (Defend): B on its first candidate row
    // (aes_lockbox — Process-gated, already unlocked at Script) equips it — a
    // matching kind succeeds through the actual UI, not just the data layer.
    enterLoadoutTab(g, 1);
    g.onButton(press(Button::A));                    // slot0 -> slot1
    g.onButton(press(Button::A));                    // slot1 -> slot2
    g.onButton(press(Button::B));                     // open slot 2's picker (row0 = unequip)
    CHECK(g.nav() == Game::Nav::Detail);
    g.onButton(press(Button::A));                     // row0 -> row1 (aes_lockbox)
    tapB(g);                                          // drill into its detail page
    g.onButton(press(Button::B));                      // equip — no confirm (slot was empty)
    CHECK(std::strcmp(g.moveLoadout().equipped(2), "aes_lockbox") == 0);
}

// A creature's declared seed for one slot, as Game::SlotKind — what
// Game::stampSlotKinds is expected to copy onto the pet when that slot unlocks.
static Game::SlotKind seedKind(const ContentRegistry& r, const char* id, int slot) {
    const CreatureDef* c = r.creature(id);
    CHECK(c != nullptr);
    return c->slotKinds[slot] == MoveDef::Kind::Attack ? Game::SlotKind::Attack
                                                        : Game::SlotKind::Defend;
}

// Stamping: a slot's kind is fixed the instant it first unlocks, from
// whichever creature is installed at that moment, and never changes again —
// evolving further never rewrites an already-stamped slot.
//
// Expectations are read from each creature's OWN slotKinds seed rather than
// written out as Attack/Defend literals. The mechanism under test is "slot N
// takes the kind of whoever is installed when N unlocks, and earlier slots are
// immutable" — WHICH kind any shipped creature seeds is a balance number
// (defs.h calls the current layout a placeholder), and a test that pins it turns
// every balance tweak into a false failure.
void test_move_slot_stamping_locks_at_unlock() {
    ContentRegistry r = ContentRegistry::embedded();

    // Good branch: raise through the linear chain, capturing slot kinds at each
    // stage, and confirm slots 0-2 never change once stamped.
    { Game g{StartMode::Hatched, "cryptoshell"};       // Boot: only slot 0 unlocked
      const Game::SlotKind boot0 = g.slotKind(0);
      CHECK(boot0 == seedKind(r, "cryptoshell", 0));   // cryptoshell's own seed
      CHECK(g.slotKind(1) == Game::SlotKind::Unset);   // not yet unlocked

      g.debugTriggerEvolution();                       // -> paypup (Process, 2 slots)
      uint32_t t = 0; advanceToReveal(g, t);
      g.onButton(press(Button::B));
      CHECK(g.pet() && std::strcmp(g.pet()->id, "paypup") == 0);
      CHECK(g.slotKind(0) == boot0);                   // untouched by the evolve
      const Game::SlotKind slot1 = g.slotKind(1);
      CHECK(slot1 == seedKind(r, "paypup", 1));         // paypup's seed, freshly stamped

      g.debugTriggerEvolution();                       // -> barkmail (Script, 3 slots)
      t = 0; advanceToReveal(g, t);
      g.onButton(press(Button::B));
      CHECK(g.pet() && std::strcmp(g.pet()->id, "barkmail") == 0);
      CHECK(g.slotKind(0) == boot0);                   // still untouched
      const Game::SlotKind slot2 = g.slotKind(2);
      CHECK(slot2 == seedKind(r, "barkmail", 2));       // barkmail's seed, freshly stamped

      g.model().setCareMistakes(0);                     // 0-2 -> Good branch
      CHECK(g.model().careBranch() == CareBranch::Good);
      g.debugTriggerEvolution();                        // -> wire_heir (Daemon, 4 slots)
      t = 0; advanceToReveal(g, t);
      g.onButton(press(Button::B));
      CHECK(g.pet() && std::strcmp(g.pet()->id, "wire_heir") == 0);
      CHECK(g.slotKind(0) == boot0 && g.slotKind(1) == slot1 &&
            g.slotKind(2) == slot2);                    // slots 0-2 STILL unchanged
      CHECK(g.slotKind(3) == seedKind(r, "wire_heir", 3));
    }
    // Bad branch: same chain, but slot 3 stamps from the OTHER Daemon's seed —
    // proving the branch taken (not just the terminal stage) decides the newly-
    // unlocked slot. The two branch seeds must differ for that to be observable,
    // so assert it: if a balance pass ever flattens them this scenario has gone
    // vacuous and should say so rather than pass silently.
    CHECK(seedKind(r, "bruinforce", 3) != seedKind(r, "berserkernel", 3));
    { Game g{StartMode::Hatched, "malbear"};
      g.model().setCareMistakes(kCareGoodMax + 1);      // 3-4 -> Bad branch
      CHECK(g.model().careBranch() == CareBranch::Bad);
      const Game::SlotKind slot2 = g.slotKind(2);
      CHECK(slot2 == seedKind(r, "malbear", 2));        // stamped on install
      g.debugTriggerEvolution();
      uint32_t t = 0; advanceToReveal(g, t);
      g.onButton(press(Button::B));
      CHECK(g.pet() && std::strcmp(g.pet()->id, "berserkernel") == 0);
      CHECK(g.slotKind(2) == slot2);                    // slot 2 unchanged by the branch pick
      CHECK(g.slotKind(3) == seedKind(r, "berserkernel", 3));
    }
}

// Save v24: slotKinds round-trips verbatim, both as raw SaveData and through an
// ordinary autosave + reload.
void test_save_v24_slot_kinds_roundtrip() {
    // Raw SaveData round-trip.
    SaveData a; std::strcpy(a.activeId, "malbear"); a.generation = 1;
    a.slotKinds = {1, 1, 2, 0};   // Attack, Attack, Defend, Unset (SlotKind values)
    auto blobFull = serializeSave(a);
    SaveData full;
    CHECK(deserializeSave(blobFull, full));
    CHECK(full.hasSlotKindData);
    CHECK(full.slotKinds.size() == 4);
    CHECK(full.slotKinds[0] == 1 && full.slotKinds[1] == 1 &&
          full.slotKinds[2] == 2 && full.slotKinds[3] == 0);

    // Live round-trip: a Game raised to malbear, stamped kinds intact after an
    // ordinary autosave + reload.
    MemSaveStore rtStore;
    {
        Game rt(StartMode::Hatched, "malbear", &rtStore);
        CHECK(rt.slotKind(2) == Game::SlotKind::Defend);
        rt.tick(kSaveAutosaveMs + kHeartbeatMs);            // autosave
    }
    Game rt2(StartMode::Hatched, "paypup", &rtStore);       // hatchedCreature ignored: store wins
    CHECK(rt2.pet() && std::strcmp(rt2.pet()->id, "malbear") == 0);
    CHECK(rt2.slotKind(0) == Game::SlotKind::Attack);
    CHECK(rt2.slotKind(1) == Game::SlotKind::Attack);
    CHECK(rt2.slotKind(2) == Game::SlotKind::Defend);
}

// The A+C override is live in combat (the one pet-side place): the chord opens the
// picker (without spending); committing spends the once-per-battle pip. A starting bag
// holds a combat item, so the picker opens on its BAND list and B descends once before
// it commits — neither press spends until the second one lands on a row.
void test_combat_override_in_game() {
    Game g{StartMode::Hatched, "paypup"};
    g.debugStartCombat(/*live=*/false);
    CHECK(g.nav() == Game::Nav::Combat && g.combat().overrideReady());
    g.onButton({Button::A, true, true});           // A+C chord -> open picker
    CHECK(g.combat().overrideOpen() && g.combat().overrideReady());  // open != spend
    CHECK(g.combat().overrideAtBands());
    g.onButton(press(Button::B));                  // open MOVES
    CHECK(g.combat().overrideOpen() && g.combat().overrideReady());  // nor does opening
    CHECK(!g.combat().overrideAtBands());
    g.onButton(press(Button::B));                  // commit -> spends
    CHECK(!g.combat().overrideOpen() && !g.combat().overrideReady());
    // Outside combat the chord stays an inert no-op (early-out not bypassed).
    Game idle{StartMode::Hatched, "paypup"};
    idle.onButton({Button::A, true, true});
    CHECK(idle.nav() == Game::Nav::Idle);
}

// Live loss applies +Fragmentation + COMBAT_LOST; a safe (Sim) loss does neither
// the live-vs-safe stakes contract on the shared engine.
void test_combat_stakes_live_vs_safe() {
    { Game g{StartMode::Hatched, "paypup"};
      const int f0 = g.model().fragmentation();
      g.debugStartCombat(/*live=*/true, /*lethal=*/true);
      uint32_t t = 0;
      for (int i = 0; i < 50 && g.combat().outcome() == Combat::Outcome::Ongoing; ++i)
          g.tick(t += kHeartbeatMs);
      CHECK(g.combat().outcome() == Combat::Outcome::Lose);
      g.onButton(press(Button::B));
      CHECK(g.model().fragmentation() > f0);                 // live -> +Frag
      CHECK(g.log().size() >= 1 && g.log().at(0).type == LogEventType::CombatLost); }

    { Game g{StartMode::Hatched, "paypup"};
      const int f0 = g.model().fragmentation();
      g.debugStartCombat(/*live=*/false, /*lethal=*/true);
      uint32_t t = 0;
      for (int i = 0; i < 50 && g.combat().outcome() == Combat::Outcome::Ongoing; ++i)
          g.tick(t += kHeartbeatMs);
      CHECK(g.combat().outcome() == Combat::Outcome::Lose);
      g.onButton(press(Button::B));
      CHECK(g.model().fragmentation() == f0);                // safe -> no Frag
      CHECK(g.log().size() == 0); }                          // not logged
}

// The combat screen + the override overlay read in grayscale (dual-coding gate).
void test_combat_screen_grayscale() {
    Game g{StartMode::Hatched, "paypup"};
    g.debugStartCombat(/*live=*/false);
    Framebuffer fb(kActiveW, kActiveH);
    g.render(fb);
    CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));
    g.onButton({Button::A, true, true});           // open override picker
    g.render(fb);
    CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));
}

// The move loadout survives a reboot (save v2): equip a different move via the
// TRAIN picker, autosave, and confirm a fresh Game over the store restores it.
// Uses malbear (Script) rather than Paypup (Process): a pet now owns its whole
// line's kit from hatch AND auto-fills every unlocked slot with the strongest
// owned+unlocked move it can hold (the backfill) — both Attack slots are already
// spoken for (payload_drop/double_extortion) the instant the pet is installed, so
// slot 2 (Defend) is the one with a real second option to switch to: it opens on
// aes_lockbox (Process-tier, unlocked before the slot itself existed) and Script
// has since unlocked a second Defend (rsa_vault) to switch it to.
void test_move_loadout_persist() {
    MemSaveStore store;
    {
        Game g(StartMode::Hatched, "malbear", &store);
        CHECK(std::strcmp(g.moveLoadout().equipped(2), "aes_lockbox") == 0);
        enterLoadoutTab(g, 1);
        g.onButton(press(Button::A));              // slot0 -> slot1
        g.onButton(press(Button::A));              // slot1 -> slot2
        g.onButton(press(Button::B));              // open slot 2 picker
        g.onButton(press(Button::A));              // unequip -> aes_lockbox
        g.onButton(press(Button::A));              // -> rsa_vault (a different move)
        tapB(g);                                   // drill into its detail page
        g.onButton(press(Button::B));              // equip -> hands back the overwrite confirm
        g.onButton(press(Button::A));              // Cancel -> Confirm
        g.onButton(press(Button::B));              // commit
        CHECK(std::strcmp(g.moveLoadout().equipped(2), "rsa_vault") == 0);
        g.tick(kSaveAutosaveMs + kHeartbeatMs);    // autosave
    }
    Game g2(StartMode::Hatched, "malbear", &store);
    CHECK(g2.nav() == Game::Nav::Idle);
    CHECK(std::strcmp(g2.moveLoadout().equipped(2), "rsa_vault") == 0);   // restored
}

// --- Evolution branching (Good/Bad) + Critical System Failure ------------------

// The Script->Daemon hop BRANCHES on the care budget: 0-2
// mistakes reach the Good successor, 3-4 reach the Bad (glass-cannon) successor.
// Both branches are reachable and the selection is deterministic from care.
void test_evolution_branch_selection() {
    { Game g{StartMode::Hatched, "malbear"};                 // default care = Good
      CHECK(g.model().careBranch() == CareBranch::Good);
      g.debugTriggerEvolution();
      uint32_t t = 0; advanceToReveal(g, t);
      g.onButton(press(Button::B));
      CHECK(g.pet() && std::strcmp(g.pet()->id, "bruinforce") == 0); }

    { Game g{StartMode::Hatched, "malbear"};
      g.model().setCareMistakes(kCareGoodMax + 1);           // 3 -> Bad branch
      CHECK(g.model().careBranch() == CareBranch::Bad);
      g.debugTriggerEvolution();
      uint32_t t = 0; advanceToReveal(g, t);
      g.onButton(press(Button::B));
      CHECK(g.pet() && std::strcmp(g.pet()->id, "berserkernel") == 0); }
}

// The branch changes mechanics: the Bad-branch leans aggressive (higher
// attack power) and takes MORE Frag on a loss; the Good-branch is the durable
// inverse. Both are measurable in an actual fight, not just the multiplier fields.
void test_evolution_branch_mechanics() {
    ContentRegistry r = ContentRegistry::embedded();
    MoveLoadout mv = MoveLoadout::starting();
    Loadout md = Loadout::starting();
    const CreatureDef* good = r.creature("bruinforce");
    const CreatureDef* bad = r.creature("berserkernel");
    CHECK(good && bad);
    Combatant cg = makePlayerCombatant(r, *good, mv, md);
    Combatant cb = makePlayerCombatant(r, *bad, mv, md);
    // Branch lean at the source: Good leans below neutral, Bad above.
    CHECK(good->powerMultPct < 100 && bad->powerMultPct > 100);
    // The per-stage offensive scale multiplies both equally, so it lifts the
    // absolute values (Daemon ×230) but PRESERVES the branch ordering in the built
    // combatants — Bad still out-hits Good.
    CHECK(cb.powerMultPct > cg.powerMultPct);
    CHECK(cb.fragMultPct > cg.fragMultPct);

    // Same enemy + seed: the Bad-branch deals strictly more damage over the run.
    CombatEnemy tank{"Tank", "SPR_PET_CACHEMUTT", 3, 500, 1, {"quick_jab"}};
    Combat combGood, combBad;
    combGood.begin(cg, makeEnemyCombatant(r, tank), Combat::Stakes::Safe, 4242);
    combBad.begin(cb, makeEnemyCombatant(r, tank), Combat::Stakes::Safe, 4242);
    for (int i = 0; i < 8; ++i) { combGood.step(); combBad.step(); }
    CHECK(combBad.enemy().health < combGood.enemy().health);  // glass cannon hits harder

    // And a live loss on the Bad branch costs MORE Fragmentation.
    int fragGood = 0, fragBad = 0;
    { Game g{StartMode::Hatched, "bruinforce"};
      g.debugStartCombat(/*live=*/true, /*lethal=*/true);
      uint32_t t = 0;
      for (int i = 0; i < 50 && g.combat().outcome() == Combat::Outcome::Ongoing; ++i)
          g.tick(t += kHeartbeatMs);
      g.onButton(press(Button::B)); fragGood = g.model().fragmentation(); }
    { Game g{StartMode::Hatched, "berserkernel"};
      g.debugStartCombat(/*live=*/true, /*lethal=*/true);
      uint32_t t = 0;
      for (int i = 0; i < 50 && g.combat().outcome() == Combat::Outcome::Ongoing; ++i)
          g.tick(t += kHeartbeatMs);
      g.onButton(press(Button::B)); fragBad = g.model().fragmentation(); }
    CHECK(fragBad > fragGood);
}

// Combat loss NEVER kills (the invariant): the lethal live loss above leaves the
// pet alive (Frag rose, but it's still the active pet, not archived).
void test_combat_loss_never_kills() {
    Game g{StartMode::Hatched, "paypup"};
    g.debugStartCombat(/*live=*/true, /*lethal=*/true);
    uint32_t t = 0;
    for (int i = 0; i < 50 && g.combat().outcome() == Combat::Outcome::Ongoing; ++i)
        g.tick(t += kHeartbeatMs);
    CHECK(g.combat().outcome() == Combat::Outcome::Lose);
    const char* petId = g.pet() ? g.pet()->id : "";
    g.onButton(press(Button::B));                              // dismiss the result
    CHECK(g.pet() != nullptr && std::strcmp(g.pet()->id, petId) == 0);  // same pet, alive
    CHECK(g.nav() != Game::Nav::ModalCSF);                    // combat never triggers CSF
    CHECK(g.recordCount() == 0);                              // not archived
}

// Critical System Failure — the ONLY death path. The 5/5 dying state, once
// its ageing window expires, fires the terminal modal; B (gated behind the crash
// hold; C disabled) archives the pet as a [CORRUPTED] record + a new-egg Hatch.
void test_csf_fires_and_archives() {
    Game g{StartMode::Hatched, "bruinforce"};   // terminus (no evolution interference)
    g.model().setCareMistakes(kCareDying);            // 5/5 dying
    uint32_t t = 0;
    g.tick(t += 1000);                                // arms the ageing window
    CHECK(g.nav() == Game::Nav::Idle);                // grace not elapsed yet
    g.tick(t += kCsfDyingGraceMs);                    // window expires -> CSF
    CHECK(g.nav() == Game::Nav::ModalCSF);
    CHECK(g.recordCount() == 0);                      // not archived until acknowledged
    tapC(g);                     // C disabled
    CHECK(g.nav() == Game::Nav::ModalCSF);
    g.onButton(press(Button::B));                     // B gated until the crash holds
    CHECK(g.nav() == Game::Nav::ModalCSF);
    for (int i = 0; i < kCsfHoldBeats; ++i) g.tick(t += kHeartbeatMs);
    g.onButton(press(Button::B));                     // acknowledge -> line-select
    pickFirstEggLine(g);                              // pick Ransomware -> new egg
    CHECK(g.nav() == Game::Nav::Idle);                // -> a new egg laid at idle
    CHECK(g.pet() && g.inEggPhase());                 // dead pet replaced by a fresh egg
    CHECK(g.recordCount() == 1);
    CHECK(static_cast<RecordStatus>(g.records()[0].status) == RecordStatus::Corrupted);
}

// Recoverable up to the deadline: dropping below 5/5 (Backup Drive / Yubi-Cookie)
// before the window expires disarms the ageing timer — no death.
void test_csf_recovery_disarms() {
    Game g{StartMode::Hatched, "bruinforce"};
    g.model().setCareMistakes(kCareDying);
    uint32_t t = 0;
    g.tick(t += 1000);                                // arm
    g.model().setCareMistakes(kCareDying - 1);        // pulled back below 5
    g.tick(t += kCsfDyingGraceMs);                    // window would have expired
    CHECK(g.nav() == Game::Nav::Idle);                // survived
    CHECK(g.recordCount() == 0);
}

// The [CORRUPTED] record + vacated active survive a reboot (save v3): the reboot
// resumes at the Hatch with the record intact.
void test_csf_record_persists() {
    MemSaveStore store;
    {
        Game g(StartMode::Hatched, "bruinforce", &store);
        g.model().setCareMistakes(kCareDying);
        uint32_t t = 0;
        g.tick(t += 1000);
        g.tick(t += kCsfDyingGraceMs);
        for (int i = 0; i < kCsfHoldBeats; ++i) g.tick(t += kHeartbeatMs);
        g.onButton(press(Button::B));                 // acknowledge -> line-select
        pickFirstEggLine(g);                          // pick Ransomware -> lays + persists the egg
        CHECK(g.recordCount() == 1);                  // persisted immediately
    }
    Game g2(StartMode::Hatched, "paypup", &store);    // reboot (store wins over the seam)
    CHECK(g2.nav() == Game::Nav::Idle);               // the post-CSF egg resumes at idle
    CHECK(g2.pet() && g2.inEggPhase());               // mid-incubation egg survived the reboot
    CHECK(g2.recordCount() == 1);
    CHECK(static_cast<RecordStatus>(g2.records()[0].status) == RecordStatus::Corrupted);
}

// The 5/5 window is the ONE timed window that survives a reboot (save v42). Every
// other one resets on boot by the rule in core/net/audit_capture.h, but those are
// penalties — refunding THIS one makes the game's only death path opt-out for anyone
// who notices the pulse and power-cycles. What persists is time AWAKE at 5/5, so a
// device that is off neither kills the pet nor buys it a reprieve.
void test_csf_window_survives_reboot() {
    MemSaveStore store;
    {
        Game g(StartMode::Hatched, "bruinforce", &store);
        g.model().setCareMistakes(kCareDying);
        uint32_t t = 0;
        g.tick(t += 1000);                            // arm
        // Burn the window down to a few seconds, then tick once more so the debounced
        // autosave writes the figure rather than leaving it to the slow periodic one.
        g.tick(t += kCsfDyingGraceMs - 4 * kSaveDebounceMs);
        g.tick(t += kSaveDebounceMs);
        CHECK(g.nav() == Game::Nav::Idle);            // alive, with the window nearly gone
    }
    Game g2(StartMode::Hatched, "bruinforce", &store);   // power cycle
    CHECK(g2.model().careMistakes() == kCareDying);      // still 5/5 — the state persisted
    uint32_t t = 0;
    g2.tick(t += kHeartbeatMs);                          // re-anchor against the fresh clock
    CHECK(g2.nav() == Game::Nav::Idle);                  // a beat is not the whole remainder
    // Only the few seconds that were actually left. Had the reboot refunded the window,
    // this would be ~6s into a fresh 120s and the pet would still be idle.
    g2.tick(t += 3 * kSaveDebounceMs);
    CHECK(g2.nav() == Game::Nav::ModalCSF);              // the burned time was NOT refunded
}

// The window is per-brush-with-death, not per-lifetime: a pet pulled back below 5/5
// gets the whole grace period again, and that reset survives a reboot too — otherwise
// a rescued pet would carry a shortened window for the rest of its life.
void test_csf_recovery_clears_burned_window() {
    MemSaveStore store;
    {
        Game g(StartMode::Hatched, "bruinforce", &store);
        g.model().setCareMistakes(kCareDying);
        uint32_t t = 0;
        g.tick(t += 1000);
        g.tick(t += kCsfDyingGraceMs - 2 * kHeartbeatMs);   // nearly spent
        g.model().setCareMistakes(kCareDying - 1);          // rescued
        g.tick(t += kSaveDebounceMs);                       // disarm + zero, then persist
        g.model().setCareMistakes(kCareDying);              // straight back to 5/5
        g.tick(t += kSaveDebounceMs);
        CHECK(g.nav() == Game::Nav::Idle);
    }
    Game g2(StartMode::Hatched, "bruinforce", &store);
    uint32_t t = 0;
    g2.tick(t += kHeartbeatMs);                             // arm
    g2.tick(t += kCsfDyingGraceMs - 2 * kHeartbeatMs);      // a full window, less a beat
    CHECK(g2.nav() == Game::Nav::Idle);                     // survived: the clock restarted
    g2.tick(t += 2 * kHeartbeatMs);
    CHECK(g2.nav() == Game::Nav::ModalCSF);                 // and it is a REAL window
}

// Save v3 round-trips the ARCH records (and a v1/v2 blob migrates with none).
void test_save_records_roundtrip() {
    SaveData d;
    std::strncpy(d.activeId, "paypup", kSaveIdCap - 1);
    SaveRecord rec;
    std::strncpy(rec.id, "berserkernel", kSaveIdCap - 1);
    rec.status = static_cast<uint8_t>(RecordStatus::Corrupted);
    rec.generation = 3;
    d.records.push_back(rec);
    SaveData out;
    CHECK(deserializeSave(serializeSave(d), out));
    CHECK(out.records.size() == 1);
    CHECK(std::strcmp(out.records[0].id, "berserkernel") == 0);
    CHECK(out.records[0].generation == 3);
    CHECK(static_cast<RecordStatus>(out.records[0].status) == RecordStatus::Corrupted);
}

// Save v4 round-trips Hacker Rank: the counter, the derived
// rank, and the dedup-pool bitmask (so a reboot doesn't re-count an
// already-seen network as new).
void test_save_hacker_rank_roundtrip() {
    SaveData d;
    std::strncpy(d.activeId, "paypup", kSaveIdCap - 1);
    d.networksSeen = 5;
    d.hackerRank = 2;
    SaveData out;
    CHECK(deserializeSave(serializeSave(d), out));
    CHECK(out.networksSeen == 5);
    CHECK(out.hackerRank == 2);
}

// The CSF modal reads in grayscale (dual-coding gate): title/message ink survives.
void test_csf_grayscale() {
    Game g{StartMode::Hatched, "bruinforce"};
    g.model().setCareMistakes(kCareDying);
    uint32_t t = 0; g.tick(t += 1000); g.tick(t += kCsfDyingGraceMs);
    CHECK(g.nav() == Game::Nav::ModalCSF);
    Framebuffer fb(kActiveW, kActiveH);
    g.render(fb);
    CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));
}

// The corrupted record surfaces in ARCH as a greyed, read-only entry:
// it opens to a detail with no actions; A/B do nothing, C backs.
void test_arch_record_readonly() {
    Game g{StartMode::Hatched, "bruinforce"};
    g.model().setCareMistakes(kCareDying);
    uint32_t t = 0; g.tick(t += 1000); g.tick(t += kCsfDyingGraceMs);
    for (int i = 0; i < kCsfHoldBeats; ++i) g.tick(t += kHeartbeatMs);
    g.onButton(press(Button::B));                     // -> line-select, 1 record
    pickFirstEggLine(g);                              // pick Ransomware -> a fresh egg (active pet)
    CHECK(g.pet() != nullptr && g.nav() == Game::Nav::Idle);
    CHECK(g.recordCount() == 1);

    enterSubmenuId(g, SubmenuId::Arch);
    CHECK(g.nav() == Game::Nav::Submenu);
    // RECORDS is its own group on the picker now — the last row, after the families.
    for (int k = 0; k < g.archPickRowCount() &&
                    g.archPickRow() != g.archPickRowCount() - 1; ++k)
        g.onButton(press(Button::A));
    g.onButton(press(Button::B));                     // open the RECORDS group
    CHECK(g.archGroup().kind == ArchGroup::Kind::Records);
    g.onButton(press(Button::B));                     // open the record (read-only)
    CHECK(g.nav() == Game::Nav::Detail);
    Framebuffer fb(kActiveW, kActiveH);
    g.render(fb);
    CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));
    g.onButton(press(Button::A));                     // no actions on a record
    CHECK(g.nav() == Game::Nav::Detail);
    tapC(g);                     // C backs to the list
    CHECK(g.nav() == Game::Nav::Submenu);
}

// ===========================================================================
// THE INVESTMENT LADDER — the tiers every combat stat unlocks, and the rungs
// they sit on. The point of the feature is that the rungs are the SAME on all
// four stats, so most of what is gated here is that uniformity rather than any
// one magnitude: a table that drifted would still compile, still fight and
// still draw, and the only symptom would be a STAT page quietly lying.
// ===========================================================================

// One ladder, four stats. Every column carries the identical thresholds, in ascending
// order, and statTierPoints() reads them off column 0 on exactly that assumption — so
// this is the gate that keeps that shortcut honest rather than a restatement of it.
void test_stat_tier_ladder_is_uniform() {
    for (int t = 0; t < kStatTierCount; ++t) {
        const int rung = statTierPoints(t);
        for (int i = 0; i < kLevelStatCount; ++i)
            CHECK(statTier(static_cast<LevelStat>(i), t).points == rung);
        if (t > 0) CHECK(rung > statTierPoints(t - 1));   // strictly ascending
        // Every rung is named and explained. A blank row would draw as an empty line
        // on the STAT page and there is no other place the omission would show.
        for (int i = 0; i < kLevelStatCount; ++i) {
            const StatTierDef& d = statTier(static_cast<LevelStat>(i), t);
            CHECK(d.name != nullptr && d.name[0] != '\0');
            CHECK(d.effect != nullptr && d.effect[0] != '\0');
            // ...and its prose renders with the numbers filled in. A leftover brace is
            // a token this table does not carry, which reaches the player verbatim.
            const EffectText text = statTierText(static_cast<LevelStat>(i), t);
            CHECK(!text.empty());
            CHECK(!text.atCap());                    // authored past the buffer = a cut sentence
            CHECK(std::strchr(text.c_str(), '{') == nullptr);
        }
    }
    CHECK(statTierPoints(0) == kStatTier1Points);
    CHECK(statTierPoints(1) == kStatTier2Points);
    CHECK(statTierPoints(2) == kStatTier3Points);
}

// The two questions the STAT page asks of a point count, over the whole range including
// the edges nobody plays at. "Reached" and "to go" are one fact seen from two sides and
// must never disagree — a page reading "HELD" beside "3 TO GO" is the failure.
void test_stat_tier_progress_readout() {
    CHECK(statTiersReached(0) == 0);
    CHECK(statTiersReached(-5) == 0);                       // negatives are not a rung
    CHECK(statTierPointsToNext(0) == kStatTier1Points);
    // The boundary belongs to the rung: reaching the threshold IS holding it.
    CHECK(statTiersReached(kStatTier1Points - 1) == 0);
    CHECK(statTiersReached(kStatTier1Points) == 1);
    CHECK(statTiersReached(kStatTier2Points) == 2);
    CHECK(statTiersReached(kStatTier3Points) == 3);
    CHECK(statTiersReached(9999) == kStatTierCount);        // and it stops there
    CHECK(statTierPointsToNext(kStatTier3Points) == 0);     // topped out owes nothing
    CHECK(statTierPointsToNext(9999) == 0);
    int prev = 0;
    for (int p = 0; p <= 200; ++p) {
        const int reached = statTiersReached(p);
        CHECK(reached >= prev);                             // monotonic: a point never un-earns
        CHECK(reached <= kStatTierCount);
        prev = reached;
        const int owed = statTierPointsToNext(p);
        CHECK(owed >= 0);
        // The two agree: owing nothing means topped out, and owing something means the
        // next rung is exactly that many points away.
        if (reached < kStatTierCount) CHECK(p + owed == statTierPoints(reached));
        else CHECK(owed == 0);
    }
}

// Defence's cut ceiling lands EXACTLY on the top rung — 8 full-rate points plus 24 bent
// ones is 60%. tunables.h calls that coincidence load-bearing, because it is what makes
// "the stat stops buying % on the same rung it starts buying something else" true rather
// than approximately true, and moving any one of four numbers would break it silently.
void test_defense_cap_lands_on_the_top_rung() {
    CHECK(levelDefenseCutPct(kStatTier3Points) == kLevelDefenseCapPct);
    CHECK(levelDefenseCutPct(kStatTier3Points - 1) < kLevelDefenseCapPct);
    CHECK(levelDefenseCutOverflowPct(kStatTier3Points) == 0);   // reached, not overshot
    // The bend and the first rung are the same point, which is the other half of that
    // claim: one threshold the player can be told about, not two.
    CHECK(kLevelDefenseSoftPoints == kStatTier1Points);
}

// Every rung's applier: inert below the threshold, its magnitude at and above it. Pure
// functions of a point count, so they are checked directly rather than sampled through a
// fight that would only ever reach a few of them.
void test_stat_tier_appliers_gate_on_their_rung() {
    const int t1 = kStatTier1Points, t2 = kStatTier2Points, t3 = kStatTier3Points;

    // POWER: T1 is the accelerating band (levelPowerPct's own bend), T2 and T3 discrete.
    CHECK(levelPowerPct(t1) == t1 * kLevelPowerPctPerPoint);          // still flat AT the rung
    CHECK(levelPowerPct(t1 + 1) ==
          t1 * kLevelPowerPctPerPoint + kLevelPowerPctPerSpecPoint);  // accelerating past it
    CHECK(levelPowerPiercePct(t2 - 1) == 0);
    CHECK(levelPowerPiercePct(t2) == kLevelPowerPiercePct);
    CHECK(levelPowerGuardSmashPct(t3 - 1) == 0);
    CHECK(levelPowerGuardSmashPct(t3) == kLevelPowerGuardSmashPct);

    // DEFENSE: all three are discrete, on top of the curve gated above.
    CHECK(levelDefensePierceResistPct(t1 - 1) == 0);
    CHECK(levelDefensePierceResistPct(t1) == kLevelDefensePierceResistPct);
    CHECK(levelDefenseBraceRetainPct(t2 - 1) == 0);
    CHECK(levelDefenseBraceRetainPct(t2) == kLevelDefenseBraceRetainPct);
    CHECK(levelDefenseBackscatterPct(t3 - 1) == 0);
    CHECK(levelDefenseBackscatterPct(t3) == kLevelDefenseBackscatterPct);

    // SPEED. T1's inert value is 1, not 0 — it is a MULTIPLIER, and a fighter without
    // the rung has to leave the hit alone rather than delete it.
    CHECK(levelSpeedFirstStrikeMult(t1 - 1) == 1);
    CHECK(levelSpeedFirstStrikeMult(t1) == kLevelSpeedFirstStrikeMult);
    CHECK(levelSpeedAdrenalinePerStep(t3 - 1) == 0);
    CHECK(levelSpeedAdrenalinePerStep(t3) == kLevelSpeedAdrenalinePerStep);
    // T2 is the conditional one: earned at its rung, paying only while BEHIND, and
    // clamped so the catch-up reaches parity rather than overshooting it.
    CHECK(levelSpeedUnderdogBonus(t2 - 1, 100) == 0);                // not earned yet
    CHECK(levelSpeedUnderdogBonus(t2, t2) == 0);                     // matched is not behind
    CHECK(levelSpeedUnderdogBonus(100, t2) == 0);                    // ahead pays nothing
    // Far behind: the full doubled rate, and still short of the fighter in front.
    CHECK(levelSpeedUnderdogBonus(t2, 100) ==
          t2 * (kLevelSpeedUnderdogPerPoint - kLevelSpeedPerPoint));
    CHECK(t2 * kLevelSpeedPerPoint + levelSpeedUnderdogBonus(t2, 100) <
          100 * kLevelSpeedPerPoint);
    // Just behind: clamped to the gap, so the pet that invested MORE is never out-run by
    // the rung meant to console the one that invested less.
    for (int enemy = t2 + 1; enemy <= t2 + 40; ++enemy) {
        const int mine = t2 * kLevelSpeedPerPoint + levelSpeedUnderdogBonus(t2, enemy);
        CHECK(mine <= enemy * kLevelSpeedPerPoint);
    }

    // MAX-HEALTH: T1 is its accelerating band, T2 a rate, T3 a flag.
    CHECK(levelHealthBonus(t1) == t1 * kLevelHealthPerPoint);
    CHECK(levelHealthBonus(t1 + 1) == t1 * kLevelHealthPerPoint + kLevelHealthPerSpecPoint);
    CHECK(levelHealthScrubPct(t2 - 1) == 0);
    CHECK(levelHealthScrubPct(t2) == kLevelHealthScrubPct);
    CHECK(!levelHealthFailoverEarned(t3 - 1));
    CHECK(levelHealthFailoverEarned(t3));
}

// ...and that the applier actually stamps them onto a fighter. One pet with nothing
// invested and one holding every rung, both through the single seam the engine uses
// (applyLevelStatPoints), so a tier resolved but never wired to its field is caught here
// rather than as a mechanic that silently does nothing in a fight.
void test_stat_tiers_reach_the_combatant() {
    const int none[kLevelStatCount] = {0, 0, 0, 0};
    Combatant bare;
    bare.maxHealth = 100;
    applyLevelStatPoints(bare, none);
    CHECK(bare.piercePct == 0);
    CHECK(bare.guardSmashPct == 0);
    CHECK(bare.backscatterPct == 0);
    CHECK(bare.firstStrikeMult == 1);
    CHECK(bare.adrenalinePerStep == 0);
    CHECK(bare.scrubPct == 0);
    CHECK(!bare.failoverArmed);
    // The baseline every fighter carries, tier or no tier.
    CHECK(bare.braceRetainPct == kBraceRetainBasePct);

    const int maxed[kLevelStatCount] = {kStatTier3Points, kStatTier3Points,
                                        kStatTier3Points, kStatTier3Points};
    Combatant built;
    built.maxHealth = 100;
    applyLevelStatPoints(built, maxed);
    CHECK(built.piercePct == kLevelPowerPiercePct);
    CHECK(built.guardSmashPct == kLevelPowerGuardSmashPct);
    CHECK(built.pierceResistPct == kLevelDefensePierceResistPct);
    CHECK(built.braceRetainPct == kBraceRetainBasePct + kLevelDefenseBraceRetainPct);
    CHECK(built.backscatterPct == kLevelDefenseBackscatterPct);
    CHECK(built.firstStrikeMult == kLevelSpeedFirstStrikeMult);
    CHECK(built.adrenalinePerStep == kLevelSpeedAdrenalinePerStep);
    CHECK(built.scrubPct == kLevelHealthScrubPct);
    CHECK(built.failoverArmed);
    // Speed T2 is banked as its INPUT here — whether it pays needs an opponent, and
    // there isn't one until applySpeedRivalry.
    CHECK(bare.speedPoints == 0);
    CHECK(built.speedPoints == kStatTier3Points);
}

// Speed T2 (the catch-up rung) is the one tier that reads BOTH fighters, so it is the one
// that cannot be resolved while a Combatant is built alone. applySpeedRivalry is where it
// lands: it pays the fighter that is behind, pays nothing to a matched pair, and is a
// no-op — an exactly zero delta — for anyone who has not earned it.
void test_speed_underdog_rung_needs_both_fighters() {
    auto build = [](int speedPoints) {
        const int pts[kLevelStatCount] = {0, 0, speedPoints, 0};
        Combatant c;
        c.maxHealth = 100;
        c.health = 100;
        c.speed = 10;
        applyLevelStatPoints(c, pts);
        return c;
    };
    // The rung earned, and behind: the underdog is paid, the fighter in front is not.
    {
        Combatant slow = build(kStatTier2Points), fast = build(kStatTier3Points);
        const float slowBefore = slow.speed, fastBefore = fast.speed;
        applySpeedRivalry(slow, fast);
        CHECK(slow.speed > slowBefore);
        CHECK(fast.speed == fastBefore);          // the one in front is paid nothing
        CHECK(slow.speed <= fast.speed);          // ...and it is catch-up, not a leapfrog
    }
    // The leapfrog the clamp exists to prevent, at every gap it could open at: investing
    // FURTHER in Speed must never hand the rival the faster fighter.
    for (int enemy = kStatTier2Points + 1; enemy <= kStatTier2Points + 40; ++enemy) {
        Combatant slow = build(kStatTier2Points), fast = build(enemy);
        applySpeedRivalry(slow, fast);
        CHECK(slow.speed <= fast.speed);
    }
    // Matched: neither is behind, so neither is paid, however deep both invested.
    {
        Combatant a = build(kStatTier3Points), b = build(kStatTier3Points);
        const float aBefore = a.speed, bBefore = b.speed;
        applySpeedRivalry(a, b);
        CHECK(a.speed == aBefore);
        CHECK(b.speed == bBefore);
    }
    // Behind but short of the rung: being slower is not itself the qualification.
    {
        Combatant a = build(kStatTier2Points - 1), b = build(kStatTier3Points);
        const float aBefore = a.speed;
        applySpeedRivalry(a, b);
        CHECK(a.speed == aBefore);
    }
}

// Speed T3 is read LIVE rather than banked, because it is a function of Health the fight
// keeps changing. effectiveSpeed is that reading, and the scheduler goes through it.
void test_adrenaline_tracks_live_health() {
    const int pts[kLevelStatCount] = {0, 0, kStatTier3Points, 0};
    Combatant c;
    c.maxHealth = 100;
    c.health = 100;
    c.speed = 10;
    applyLevelStatPoints(c, pts);
    const float full = effectiveSpeed(c);
    CHECK(full == c.speed);                       // untouched pays nothing
    c.health = 50;                                // half gone = five 10% steps
    CHECK(effectiveSpeed(c) == c.speed + 5 * kLevelSpeedAdrenalinePerStep);
    c.health = 1;
    CHECK(effectiveSpeed(c) == c.speed + 9 * kLevelSpeedAdrenalinePerStep);
    // Health is left UNCLAMPED between a hit and checkOutcome's floor, so a fighter
    // buried past 0 must read as 100% missing rather than as more than all of it.
    c.health = -400;
    CHECK(effectiveSpeed(c) == c.speed + 10 * kLevelSpeedAdrenalinePerStep);
    // ...and a fighter without the rung reads its own speed at every Health.
    Combatant plain;
    plain.maxHealth = 100;
    plain.health = 1;
    plain.speed = 10;
    CHECK(effectiveSpeed(plain) == plain.speed);
}

// ===========================================================================
// EXPL wild-encounter wiring — the Walk activity, the
// periodic step-roll, the Fight/Flee/Sinkhole encounter intro, and the shared
// combat core wired with LIVE stakes (replacing debugStartCombat as the real
// entry point; the dev hook stays for TRAIN/combat-core-only tests).
// ===========================================================================
