// test_combat.cpp — native gates for the combat engine and MODS.
//
// One slice of the native suite; see test_gates.h for the shared fixtures and
// test_main.cpp for the roster that runs these. A save-vNN gate sits with the
// feature whose field it migrates, not in a migrations pile of its own.
#include "test_gates.h"

// ARCH: the rack list shows the active pet; the record opens, cycles its
// actions (Store/Sell), and backs out. Both actions are inert shells.
void test_arch_list_and_record() {
    Game g{StartMode::Hatched};
    enterSubmenuId(g, SubmenuId::Arch);
    CHECK(g.nav() == Game::Nav::Submenu);
    Framebuffer fb(kActiveW, kActiveH);
    g.render(fb);
    CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));   // list reads in grayscale

    g.onButton(press(Button::B));                       // open the pet record (L3)
    CHECK(g.nav() == Game::Nav::Detail);
    g.render(fb);
    CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));    // record reads in grayscale
    g.onButton(press(Button::A));                       // cycle Store -> Sell (inert)
    CHECK(g.nav() == Game::Nav::Detail);                // still on the record
    tapC(g);                       // back to the rack
    CHECK(g.nav() == Game::Nav::Submenu);
    tapC(g);                       // back to the carousel
    CHECK(g.nav() == Game::Nav::Cursor);
}

// MODS data model (D3): mods are PERMANENT — consumed from the spare pool
// when equipped, never unequipped, only overwritten (the old one discarded, not
// returned). The seed installs two mods and holds two spares.
void test_loadout_permanent_mods() {
    Loadout l = Loadout::starting();
    CHECK(l.slotOf("firewall_patch") == 0);        // installed, not a spare
    CHECK(l.slotOf("clock_speed_boost") == 1);
    CHECK(!l.owns("firewall_patch"));              // equipped mods aren't in the pool
    CHECK(l.owns("packet_sniffer") && l.owns("raid_mirror"));  // two spares held
    CHECK(l.equipped(2) == nullptr);

    // Equip a spare into an empty slot: it's CONSUMED out of the pool.
    l.equip(2, "packet_sniffer");
    CHECK(l.slotOf("packet_sniffer") == 2);
    CHECK(!l.owns("packet_sniffer"));              // consumed on equip
    CHECK(l.equipped(0) != nullptr);               // did NOT move firewall out of slot 0

    // Overwrite a slot: the displaced mod is DISCARDED (not returned to the pool),
    // the new one consumed.
    l.equip(0, "raid_mirror");
    CHECK(l.slotOf("raid_mirror") == 0);
    CHECK(l.slotOf("firewall_patch") == -1);       // firewall gone for good
    CHECK(!l.owns("firewall_patch") && !l.owns("raid_mirror"));

    // Inert with no spare of that id available.
    l.equip(3, "firewall_patch");
    CHECK(l.equipped(3) == nullptr);
}

// MoveLoadout: a move lives in one slot — equipping it elsewhere
// MOVES it; the innate default sits outside the slots; slot COUNT grows by Stage.
void test_move_loadout() {
    MoveLoadout l = MoveLoadout::starting();
    CHECK(std::strcmp(l.defaultMove(), "quick_jab") == 0);
    CHECK(l.slotOf("packet_storm") == 0);
    CHECK(l.equipped(1) == nullptr);                // slot 1 = Quick Jab per-slot fallback (#11)
    // fork_bomb + checksum_guard start OWNED but unequipped (the Attack/Defend spares).
    CHECK(l.owns("fork_bomb") && l.slotOf("fork_bomb") < 0);
    CHECK(l.owns("checksum_guard") && l.slotOf("checksum_guard") < 0);
    l.equip(2, "packet_storm");                       // move 0 -> 2
    CHECK(l.slotOf("packet_storm") == 2 && l.equipped(0) == nullptr);
    l.unequip(2);
    CHECK(l.equipped(2) == nullptr && l.owns("packet_storm"));   // stays owned

    // Slot counts grow Boot(1) -> Process(2) -> Script(3) -> Daemon(4).
    CHECK(MoveLoadout::slotsForStage(Stage::BootSector) == 1);
    CHECK(MoveLoadout::slotsForStage(Stage::Process) == 2);
    CHECK(MoveLoadout::slotsForStage(Stage::Script) == 3);
    CHECK(MoveLoadout::slotsForStage(Stage::Daemon) == kMaxMoveSlots);
    // Slot 1 first becomes available at Process (the "unlocks at" affordance).
    CHECK(MoveLoadout::stageUnlockingSlot(0) == Stage::BootSector);
    CHECK(MoveLoadout::stageUnlockingSlot(1) == Stage::Process);
    CHECK(MoveLoadout::stageUnlockingSlot(3) == Stage::Daemon);

    // The roster + default resolve through the registry.
    ContentRegistry r = ContentRegistry::embedded();
    CHECK(r.move("quick_jab") && r.move("quick_jab")->kind == MoveDef::Kind::Attack);
    CHECK(r.move("checksum_guard")->kind == MoveDef::Kind::Defend);
    CHECK(r.move("runaway_fork")->channelTurns == 3);   // the roster's one wind-up
    CHECK(r.move("fork_bomb")->chainNextId != nullptr);  // ...the rest chain instead
    CHECK(static_cast<int>(r.allMoves().size()) >= 4);
}

// Every combatant the game can put on screen must name a sprite the atlas actually
// carries. Sprites resolve by STRING at draw time, so a name with no PNG behind it
// is not a compile error and not a crash — it silently draws nothing, which is how
// the wild roster spent its life wearing the pet dog's frame. Distinctness is half
// the point: six wilds sharing one sprite is the bug this locks out.
void test_every_combatant_sprite_resolves() {
    ContentRegistry r = ContentRegistry::embedded();
    std::vector<const char*> wildSprites;

    for (int tier = 1; tier <= 3; ++tier) {
        for (uint32_t variant = 0; variant < 2; ++variant) {
            const CombatEnemy e = wildMalbeast(tier, variant);
            CHECK(e.spriteName != nullptr);
            CHECK(r.sprite(e.spriteName) != nullptr);
            wildSprites.push_back(e.spriteName);
            // The name is also what the 'Pedia's seen/defeated masks key on.
            CHECK(wildMalbeastIndex(e.name) >= 0);
        }
    }
    CHECK(static_cast<int>(wildSprites.size()) == kWildMalbeastCount);
    for (size_t i = 0; i < wildSprites.size(); ++i)
        for (size_t j = i + 1; j < wildSprites.size(); ++j)
            CHECK(std::strcmp(wildSprites[i], wildSprites[j]) != 0);

    for (int tier = 0; tier <= 1; ++tier) {
        const CombatEnemy d = simDummy(tier);
        CHECK(d.spriteName != nullptr);
        CHECK(r.sprite(d.spriteName) != nullptr);
    }

    // Same contract for the raisable roster: every creature row's art exists.
    for (const CreatureDef* c : r.allCreatures()) {
        CHECK(c->spriteName != nullptr);
        CHECK(r.creatureSprite(*c) != nullptr);
    }
}

// ===========================================================================
// The shared combat engine
// ===========================================================================

static Combat::Outcome runToEnd(Combat& cb) {
    int guard = 0;
    while (cb.outcome() == Combat::Outcome::Ongoing && guard++ < 1000) cb.step();
    return cb.outcome();
}

// Deterministic resolution: the same seed replays an identical fight; a strong
// player beats a weak dummy.
void test_combat_deterministic() {
    ContentRegistry r = ContentRegistry::embedded();
    Combatant p = mkCombatant(r, "P", 60, 10, {"quick_jab", "packet_storm"});
    Combatant e = mkCombatant(r, "E", 20, 8, {"quick_jab"});

    Combat a; a.begin(p, e, Combat::Stakes::Safe, 12345);
    CHECK(runToEnd(a) == Combat::Outcome::Win);
    Combat b; b.begin(p, e, Combat::Stakes::Safe, 12345);
    CHECK(runToEnd(b) == Combat::Outcome::Win);
    CHECK(a.player().health == b.player().health);   // identical replay
}

// The autonomous roll never repeats a move twice in a row.
void test_combat_no_consecutive() {
    ContentRegistry r = ContentRegistry::embedded();
    Combatant p = mkCombatant(r, "P", 100, 10, {"quick_jab", "packet_storm"});
    Combatant e = mkCombatant(r, "E", 100, 5, {"quick_jab"});
    Combat cb; cb.begin(p, e, Combat::Stakes::Safe, 777);
    const char* prev = nullptr; int checks = 0;
    for (int i = 0; i < 80 && cb.outcome() == Combat::Outcome::Ongoing; ++i) {
        const bool pturn = cb.playerTurnNext();
        cb.step();
        if (pturn) {
            if (prev) CHECK(std::strcmp(prev, cb.lastMoveName()) != 0);
            prev = cb.lastMoveName(); ++checks;
        }
    }
    CHECK(checks > 3);
}

// A one-shot `guard` is spent whole on the next hit, so a pure-brace Defend rolled while
// the brace is still up buys nothing but overkill. braceOnlyDefend classifies the rows it
// applies to, and the roll re-rolls off them.
void test_brace_only_defend_is_not_recast() {
    ContentRegistry r = ContentRegistry::embedded();
    // Classification first: a bare brace qualifies; a Defend that ALSO pools a shield,
    // arms a trap or stacks the Cipher cut is worth its turn whatever the brace is doing.
    CHECK(braceOnlyDefend(*r.move("checksum_guard")));
    CHECK(braceOnlyDefend(*r.move("null_route")));
    CHECK(!braceOnlyDefend(*r.move("spoof_bubble")));    // pools instead of bracing
    CHECK(!braceOnlyDefend(*r.move("killswitch")));      // arms a trap
    CHECK(!braceOnlyDefend(*r.move("aes_lockbox")));     // stacks the Cipher cut
    CHECK(!braceOnlyDefend(*r.move("packet_storm")));    // not a Defend at all

    // A brace-heavy kit that still owns one attack, against a slow enemy so the player
    // takes runs of turns — the shape that used to stack brace onto live brace.
    Combatant p = mkCombatant(r, "P", 200, 30,
                              {"quick_jab", "checksum_guard", "null_route"});
    Combatant e = mkCombatant(r, "E", 200, 4, {"quick_jab"});
    Combat cb;
    cb.begin(p, e, Combat::Stakes::Safe, 4242);
    int braces = 0;
    for (int i = 0; i < 200 && cb.outcome() == Combat::Outcome::Ongoing; ++i) {
        const bool pturn = cb.playerTurnNext();
        const int before = cb.player().guard;
        cb.step();
        if (!pturn) continue;
        if (cb.player().guard > before) {   // the player braced this turn
            CHECK(before == 0);             // ...never onto one already standing
            ++braces;
        }
    }
    CHECK(braces > 2);                      // the kit really did brace, repeatedly
}

// stealMaxHpPct MOVES a Health pool: the caster gains the ceiling and the Health inside
// it, the target loses both. A ceiling alone would be unreachable — combat has no heal to
// climb into one — so the transfer is what makes the rider a reward and not just a debuff.
void test_steal_max_health_moves_the_pool() {
    ContentRegistry r = ContentRegistry::embedded();
    const MoveDef* toll = r.move("toll_charge");
    CHECK(toll->stealMaxHpPct > 0);

    // Slow, fat target so the steal lands before anything dies, and the caster opens.
    Combatant p = mkCombatant(r, "P", 100, 30, {"toll_charge"});
    Combatant e = mkCombatant(r, "E", 200, 1, {"quick_jab"});
    Combat cb;
    cb.begin(p, e, Combat::Stakes::Safe, 31337);
    const int pMaxBefore = cb.player().maxHealth;
    const int eMaxBefore = cb.enemy().maxHealth;
    for (int i = 0; i < 8 && cb.outcome() == Combat::Outcome::Ongoing; ++i) {
        const bool pturn = cb.playerTurnNext();
        cb.step();
        if (pturn) break;
    }
    const int gained = cb.player().maxHealth - pMaxBefore;
    CHECK(gained > 0);                                   // a ceiling crossed
    CHECK(eMaxBefore - cb.enemy().maxHealth == gained);   // ...off the target, exactly
    // The Health came with it: the caster is not sitting under a ceiling it cannot reach.
    CHECK(cb.player().health == cb.player().maxHealth);
}

// A chained move hands its slot to a follow-up step: the entry resolves normally and
// COMMITS the caster's next turn to the step, so both turns are real casts. That is the
// difference from the wind-up it replaces, where the first turn resolved nothing at all.
void test_chained_move_plays_both_halves() {
    ContentRegistry r = ContentRegistry::embedded();
    const MoveDef* lure = r.move("smish_hook");
    CHECK(lure->chainNextId != nullptr);
    CHECK(lure->channelTurns == 1);                      // a chain is not a channel
    // The step is reachable as a step and NOT as a move: nothing can own, equip, be
    // taught or be rolled one, which is why it lives outside the move roster.
    CHECK(r.chainStep(lure->chainNextId) != nullptr);
    CHECK(r.move(lure->chainNextId) == nullptr);

    // A pet holding only the lure: every one of its turns is either the lure or its step,
    // so the alternation is observable without the roll getting a say.
    Combatant p = mkCombatant(r, "P", 400, 50, {"smish_hook"});
    Combatant e = mkCombatant(r, "E", 400, 50, {"quick_jab"});
    Combat cb;
    cb.begin(p, e, Combat::Stakes::Safe, 1);
    int lures = 0, strikes = 0;
    const char* prev = nullptr;
    for (int i = 0; i < 12 && cb.outcome() == Combat::Outcome::Ongoing; ++i) {
        const bool pturn = cb.playerTurnNext();
        cb.step();
        if (!pturn) continue;
        const bool isLure = std::strcmp(cb.lastMoveName(), "Smish-Hook") == 0;
        if (isLure) ++lures; else ++strikes;
        // Neither half ever resolves as a charge: a chain spends no turn winding up.
        CHECK(!cb.lastWasCharge());
        CHECK(cb.lastDamage() > 0);                      // ...and both halves connect
        // Strictly alternating, and the step always follows its entry.
        if (prev) CHECK(std::strcmp(prev, cb.lastMoveName()) != 0);
        prev = cb.lastMoveName();
    }
    CHECK(lures > 1 && strikes > 1);
}

// The level stats' three curve SHAPES, asserted directly (each is a total function).
// Power and max-Health ACCELERATE past their specialisation point, Defence's cut bends the
// other way, and Defence's two tiers are thresholds rather than curves at all.
void test_level_stat_curves() {
    // Power: base rate to the spec point, the higher rate past it, then the cap.
    CHECK(levelPowerPct(0) == 0);
    CHECK(levelPowerPct(kLevelPowerSpecPoints) ==
          kLevelPowerSpecPoints * kLevelPowerPctPerPoint);
    CHECK(levelPowerPct(kLevelPowerSpecPoints + 2) ==
          kLevelPowerSpecPoints * kLevelPowerPctPerPoint + 2 * kLevelPowerPctPerSpecPoint);
    CHECK(levelPowerPct(1000) == kLevelPowerSpecCapPct);
    // The point of the bend: the point AFTER it is worth more than the one before.
    const int beforeBend = levelPowerPct(kLevelPowerSpecPoints) -
                           levelPowerPct(kLevelPowerSpecPoints - 1);
    const int afterBend = levelPowerPct(kLevelPowerSpecPoints + 2) -
                          levelPowerPct(kLevelPowerSpecPoints + 1);
    CHECK(afterBend > beforeBend);

    CHECK(levelHealthBonus(0) == 0);
    CHECK(levelHealthBonus(kLevelHealthSpecPoints) ==
          kLevelHealthSpecPoints * kLevelHealthPerPoint);
    CHECK(levelHealthBonus(1000) == kLevelHealthSpecCap);

    // Defence bends the OTHER way — the two curves are deliberately mirror images.
    const int defBefore = levelDefenseCutPct(kLevelDefenseSoftPoints) -
                          levelDefenseCutPct(kLevelDefenseSoftPoints - 1);
    const int defAfter = levelDefenseCutPct(kLevelDefenseSoftPoints + 2) -
                         levelDefenseCutPct(kLevelDefenseSoftPoints + 1);
    CHECK(defAfter < defBefore);

    // Defence's tiers: nothing at all until the threshold, then the whole bonus.
    CHECK(levelDefensePierceResistPct(kLevelDefensePierceResistPoints - 1) == 0);
    CHECK(levelDefensePierceResistPct(kLevelDefensePierceResistPoints) ==
          kLevelDefensePierceResistPct);
    CHECK(levelDefenseBraceRetainPct(kLevelDefenseBraceRetainPoints - 1) == 0);
    CHECK(levelDefenseBraceRetainPct(kLevelDefenseBraceRetainPoints) ==
          kLevelDefenseBraceRetainPct);
}

// Defence tier 2 in a fight: an over-sized one-shot brace used to bin whatever the hit it
// ate did not need. A committed wall CARRIES that remainder to the next hit instead —
// efficiency, which is the thing the % cut's own ceiling stops it being paid in.
void test_defence_tier_retains_an_unspent_brace() {
    ContentRegistry r = ContentRegistry::embedded();
    // Equal speed so the order is exactly P(brace), E(hit) — one brace meeting one hit is
    // the whole question, and a speed edge either way would blur it into several.
    auto run = [&](int defPoints, Combat& out) {
        // A big brace against a small hit, so there is a real remainder to argue about.
        Combatant p = mkCombatant(r, "P", 200, 50, {"onion_layer"});
        const int pts[4] = {0, defPoints, 0, 0};
        applyLevelStatPoints(p, pts);
        Combatant e = mkCombatant(r, "E", 200, 50, {"quick_jab"});
        out.begin(p, e, Combat::Stakes::Safe, 3, /*forceEnemyFirst=*/false);
    };
    // Uncommitted: the brace is spent whole on one hit whatever it had left over.
    Combat plain; run(0, plain);
    CHECK(plain.player().braceRetainPct == 0);
    // Committed: the same brace against the same hit keeps a share of the remainder.
    Combat spec; run(kLevelDefenseBraceRetainPoints, spec);
    CHECK(spec.player().braceRetainPct == kLevelDefenseBraceRetainPct);
    // One brace, then one hit into it, and compare what is left standing.
    for (int i = 0; i < 2; ++i) { plain.step(); spec.step(); }
    CHECK(plain.player().guard == 0);                    // spent whole, remainder binned
    CHECK(spec.player().guard > 0);                      // ...carried instead
    CHECK(spec.player().guard > plain.player().guard);
}

// The Exploit override commands the next move AND breaks the no-consecutive rule:
// it can repeat the move just played. Opening doesn't spend; committing does.
void test_combat_override_breaks_rule() {
    ContentRegistry r = ContentRegistry::embedded();
    Combatant p = mkCombatant(r, "P", 100, 10, {"quick_jab", "packet_storm"});
    Combatant e = mkCombatant(r, "E", 300, 10, {"quick_jab"});   // equal speed → strict alternation
    Combat cb; cb.begin(p, e, Combat::Stakes::Safe, 42);
    // Resolve the first player turn, note the move it played.
    cb.step();                                   // player acts (player-first on tie)
    const char* m1 = cb.lastMoveName();
    int idx = -1;
    for (int i = 0; i < static_cast<int>(cb.player().moves.size()); ++i)
        if (std::strcmp(cb.player().moves[i]->displayName, m1) == 0) idx = i;
    CHECK(idx >= 0);

    CHECK(cb.overrideReady());
    cb.openOverride();
    CHECK(cb.overrideReady());                   // opening does NOT spend
    while (cb.overridePick() != idx) cb.cycleOverride();
    cb.commitOverride();                         // commit the SAME move → spends
    CHECK(!cb.overrideReady());

    cb.step();                                   // enemy turn
    cb.step();                                   // player turn → forced repeat
    CHECK(std::strcmp(cb.lastMoveName(), m1) == 0 && cb.lastByPlayer());
}

// The Exploit picker's USE-ITEM branch: committing an item patches transient
// Health (clamped to max), spends one use, and reports the id back for the Game to
// consume exactly once. The item sits past the moves in the flat picker list.
void test_combat_override_item_use() {
    ContentRegistry r = ContentRegistry::embedded();
    Combatant p = mkCombatant(r, "P", 100, 10, {"quick_jab", "packet_storm"});
    Combatant e = mkCombatant(r, "E", 300, 5, {"quick_jab"});
    Combat cb;
    cb.begin(p, e, Combat::Stakes::Safe, 42, /*forceEnemyFirst=*/false,
             /*carryPlayerHealth=*/50, /*exploitUses=*/1);
    CHECK(cb.player().health == 50);
    cb.openOverride({{"dyno_nuggets", "Dyno Nuggets", 30}});
    CHECK(cb.overrideOpen() && cb.overrideMoveCount() == 2);
    while (cb.overridePick() != cb.overrideMoveCount()) cb.cycleOverride();
    cb.commitOverride();
    CHECK(cb.player().health == 80);             // +30 patch
    CHECK(!cb.overrideReady());                  // the single use is spent
    const char* used = cb.takeCommittedItem();
    CHECK(used && std::strcmp(used, "dyno_nuggets") == 0);
    CHECK(cb.takeCommittedItem() == nullptr);    // reported exactly once

    // The patch clamps to max Health (no overheal).
    cb.begin(p, e, Combat::Stakes::Safe, 42, false, /*carryPlayerHealth=*/90, 1);
    cb.openOverride({{"dyno_nuggets", "Dyno Nuggets", 30}});
    while (cb.overridePick() != cb.overrideMoveCount()) cb.cycleOverride();
    cb.commitOverride();
    CHECK(cb.player().health == 100);
}

// exploitUsesPerBattle: >1 lets the override fire more than once; each commit
// spends one, and begin() resets the allowance for the next fight / gauntlet round.
void test_exploit_uses_per_battle() {
    ContentRegistry r = ContentRegistry::embedded();
    Combatant p = mkCombatant(r, "P", 100, 10, {"quick_jab", "packet_storm"});
    Combatant e = mkCombatant(r, "E", 300, 5, {"quick_jab"});
    Combat cb;
    cb.begin(p, e, Combat::Stakes::Safe, 42, false, -1, /*exploitUses=*/2);
    CHECK(cb.overrideUsesTotal() == 2 && cb.overrideUsesLeft() == 2);
    cb.openOverride(); cb.commitOverride();       // force a move
    CHECK(cb.overrideUsesLeft() == 1 && cb.overrideReady());
    cb.openOverride(); cb.commitOverride();
    CHECK(cb.overrideUsesLeft() == 0 && !cb.overrideReady());
    cb.openOverride();
    CHECK(!cb.overrideOpen());                    // exhausted → won't reopen
    cb.begin(p, e, Combat::Stakes::Safe, 7, false, -1, 2);
    CHECK(cb.overrideUsesLeft() == 2);            // a fresh fight restores it
}

// End-to-end: A+C in a live fight opens the picker with the combat item, and
// committing it consumes the inventory stack + spends the single use.
void test_combat_item_in_game() {
    Game g{StartMode::Hatched, "paypup"};
    g.debugStartCombat(/*live=*/true);
    CHECK(g.nav() == Game::Nav::Combat);
    const int snacks0 = g.inventory().count("dyno_nuggets");
    CHECK(snacks0 > 0);
    g.onButton({Button::A, true, true});           // A+C -> open picker
    CHECK(g.combat().overrideOpen());
    const int moveN = g.combat().overrideMoveCount();
    while (g.combat().overridePick() != moveN)      // cursor onto the item row
        g.onButton(press(Button::A));
    g.onButton(press(Button::B));                  // commit -> use item
    CHECK(g.inventory().count("dyno_nuggets") == snacks0 - 1);
    CHECK(!g.combat().overrideReady());            // single use spent
}

// A multi-turn channel move winds up (no damage) then detonates for full power.
void test_combat_channel() {
    ContentRegistry r = ContentRegistry::embedded();
    // Runaway Fork is the roster's one remaining wind-up, and the only row where winding
    // up IS the move — so it is what holds this path. Every other two-beat move CHAINS
    // (MoveDef::chainNextId), which spends both of its turns on something.
    Combatant p = mkCombatant(r, "P", 100, 10, {"runaway_fork"});  // channel 3, power 52
    Combatant e = mkCombatant(r, "E", 100, 10, {"quick_jab"});     // equal speed → P,E,P…
    Combat cb; cb.begin(p, e, Combat::Stakes::Safe, 1);
    cb.step();                                   // player: wind-up begins
    CHECK(cb.lastByPlayer() && cb.lastWasCharge() && cb.lastDamage() == 0);
    CHECK(cb.enemy().health == 100);             // no payoff yet
    cb.step();                                   // enemy turn
    cb.step();                                   // player: still charging
    CHECK(cb.lastByPlayer() && cb.lastWasCharge() && cb.lastDamage() == 0);
    CHECK(cb.enemy().health == 100);             // ...and still no payoff
    cb.step();                                   // enemy turn
    cb.step();                                   // player: detonate
    CHECK(cb.lastByPlayer() && !cb.lastWasCharge() && cb.lastDamage() == 52);
    CHECK(cb.enemy().health == 48);
}

// MODS passives measurably change the fight.
void test_combat_mod_passives() {
    ContentRegistry r = ContentRegistry::embedded();
    Combatant enemy = mkCombatant(r, "E", 100, 10, {"packet_storm"});  // 12 dmg, fast

    // Firewall Patch cuts incoming damage by its own magnitude (read off the mod row).
    const int fwCut = r.mod("firewall_patch")->magnitude;
    Combatant plain = mkCombatant(r, "P", 100, 5, {"quick_jab"});      // slower → enemy first
    Combat a; a.begin(plain, enemy, Combat::Stakes::Safe, 5);
    a.step();
    CHECK(a.player().health == 88);                                    // 12 full
    Combatant fire = plain; fire.dmgReducePct = fwCut;
    Combat b; b.begin(fire, enemy, Combat::Stakes::Safe, 5);
    b.step();
    CHECK(b.player().health == 100 - 12 * (100 - fwCut) / 100);        // reduced

    // RAID Mirror negates exactly the first incoming hit, then is consumed.
    Combatant mir = plain; mir.mods.arm(ModEffect::RaidMirror);
    Combat c; c.begin(mir, enemy, Combat::Stakes::Safe, 5);
    c.step();
    CHECK(c.player().health == 100);                                   // first hit negated
    c.step(); c.step();                                               // player, then enemy again
    CHECK(c.player().health == 88);                                    // mirror gone → full hit

    // Clock-Speed Boost flips initiative: enemy(12) outspeeds base(10), the boost wins.
    Combatant base = mkCombatant(r, "P", 100, kCombatBaseSpeed, {"quick_jab"});
    Combatant fast = mkCombatant(r, "E", 100, 12, {"quick_jab"});
    Combat d; d.begin(base, fast, Combat::Stakes::Safe, 9);
    CHECK(!d.playerActsFirst());
    Combatant boosted = base; boosted.speed += r.mod("clock_speed_boost")->magnitude;
    Combat eC; eC.begin(boosted, fast, Combat::Stakes::Safe, 9);
    CHECK(eC.playerActsFirst());
}

// Stakes: safe flee quits immediately (Sim-Battle C). makePlayer/Enemy builders
// wire stage→maxHealth, the default move, and mod passives.
void test_combat_builders_and_flee() {
    ContentRegistry r = ContentRegistry::embedded();
    const CreatureDef* pet = r.creature("paypup");        // Process → 2 slots, 60 HP
    MoveLoadout ml = MoveLoadout::starting();
    Loadout mods = Loadout::starting();                   // Firewall + Clock-Speed equipped
    Combatant pc = makePlayerCombatant(r, *pet, ml, mods);
    CHECK(pc.maxHealth == kMaxHealthByStage[stageIndex(Stage::Process)]);
    // #11: one pool entry per unlocked slot — slot 0's equipped packet_storm, and
    // slot 1 falls back to the Quick Jab default (the starter leaves it unequipped).
    CHECK(pc.moves.size() == 2);
    CHECK(std::strcmp(pc.moves[0]->id, "packet_storm") == 0);
    CHECK(std::strcmp(pc.moves[1]->id, "quick_jab") == 0);
    CHECK(pc.speed == kCombatBaseSpeed + r.mod("clock_speed_boost")->magnitude);  // Clock-Speed read
    CHECK(pc.dmgReducePct == r.mod("firewall_patch")->magnitude);                 // Firewall read

    CombatEnemy spec{"Dummy", "SPR_PET_CACHEMUTT", 1, 30, 8, {"quick_jab"}};
    Combatant ec = makeEnemyCombatant(r, spec);
    CHECK(ec.maxHealth == 30 && ec.diffPips == 1 && ec.moves.size() == 1);

    Combat cb; cb.begin(pc, ec, Combat::Stakes::Safe, 3);
    cb.flee();
    CHECK(cb.outcome() == Combat::Outcome::Fled);          // safe stakes → instant quit
}

// MODS equip flow: open an empty slot's picker, choose a spare, inspect its detail
// then equip it (no confirm for an empty slot). The mod is CONSUMED out of
// the spare pool (D3) — equipping is permanent.
void test_mods_equip_flow() {
    Game g{StartMode::Hatched};
    enterLoadoutTab(g, 0);
    CHECK(g.nav() == Game::Nav::Submenu);
    Framebuffer fb(kActiveW, kActiveH);
    g.render(fb);
    CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));     // list grayscale

    g.onButton(press(Button::A));                  // slot 1 -> slot 2
    g.onButton(press(Button::A));                  // slot 2 -> slot 3 (empty)
    CHECK(g.loadout().equipped(2) == nullptr);
    g.onButton(press(Button::B));                  // open slot 3 picker
    CHECK(g.nav() == Game::Nav::Detail);
    g.render(fb);
    CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));     // picker grayscale
    g.onButton(press(Button::B));                  // open the first spare's detail
    Framebuffer det = fb;
    g.render(det);
    CHECK(hasDarkInk(det, 0, 0, kActiveW, kActiveH));    // detail grayscale
    CHECK(g.loadout().equipped(2) == nullptr);          // inspecting equips nothing yet
    // C backs out of the detail to the picker without equipping.
    tapC(g);
    CHECK(g.nav() == Game::Nav::Detail && g.loadout().equipped(2) == nullptr);
    g.onButton(press(Button::B));                  // re-open detail
    g.onButton(press(Button::B));                  // EQUIP from the detail
    CHECK(g.nav() == Game::Nav::Submenu);
    const char* installed = g.loadout().equipped(2);
    CHECK(installed != nullptr);                    // a spare is now installed there
    CHECK(!g.loadout().owns(installed));            // consumed out of the pool (permanent)
}

// MODS overwrite confirm (D3): equipping over a slot that already holds a
// different mod opens the inline Cancel/Confirm "discards {current} — permanent"
// warning. Confirm installs the new mod and DISCARDS the old one; Cancel leaves the
// slot unchanged and the spare un-consumed.
void test_mods_overwrite_confirm() {
    // Confirm path. Slot 0 holds Firewall Patch; selecting a spare opens its detail
    // and EQUIP raises the overwrite confirm back on the picker.
    { Game g{StartMode::Hatched};
      enterLoadoutTab(g, 0);          // slot 1 holds Firewall Patch
      CHECK(std::strcmp(g.loadout().equipped(0), "firewall_patch") == 0);
      g.onButton(press(Button::B));                // open slot 1 picker (rows = spares)
      const char* spare = g.loadout().owned().front().id;  // first available spare
      g.onButton(press(Button::B));                // open the spare's detail
      g.onButton(press(Button::B));                // EQUIP -> inline confirm on picker
      CHECK(g.nav() == Game::Nav::Detail);         // still in the picker (confirm up)
      Framebuffer fb(kActiveW, kActiveH); g.render(fb);
      CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));
      g.onButton(press(Button::A));                // Cancel -> Confirm
      g.onButton(press(Button::B));                // commit
      CHECK(g.nav() == Game::Nav::Submenu);
      CHECK(std::strcmp(g.loadout().equipped(0), spare) == 0);   // new mod installed
      CHECK(g.loadout().slotOf("firewall_patch") == -1);         // old mod discarded
      CHECK(!g.loadout().owns("firewall_patch")); }              // not returned to pool

    // Cancel path leaves the slot unchanged and the spare un-consumed.
    { Game g{StartMode::Hatched};
      enterLoadoutTab(g, 0);
      const char* spare = g.loadout().owned().front().id;
      g.onButton(press(Button::B));                // open slot 1 picker
      g.onButton(press(Button::B));                // open the spare's detail
      g.onButton(press(Button::B));                // EQUIP -> confirm (default Cancel)
      g.onButton(press(Button::B));                // B on Cancel -> abort
      CHECK(std::strcmp(g.loadout().equipped(0), "firewall_patch") == 0);
      CHECK(g.loadout().owns(spare)); }            // spare still held (not consumed)
}

// the mod detail spells out the effect + flags a ONE-SHOT. The one-shot
// caveat line (y~130) is drawn for a consumed-on-trigger mod and absent otherwise —
// the grayscale-safe channel that distinguishes the two mod kinds.
void test_mod_detail_oneshot() {
    ContentRegistry r = ContentRegistry::embedded();
    const ModDef* oneShot = r.mod("raid_mirror");        // oneShot == true
    const ModDef* reuse = r.mod("firewall_patch");       // oneShot == false
    CHECK(oneShot && reuse);
    CHECK(oneShot->oneShot && !reuse->oneShot);

    Framebuffer os(kActiveW, kActiveH), ru(kActiveW, kActiveH);
    Loadout load;
    drawModDetail(os, r, load, *oneShot, /*equippedHere=*/false, /*slot=*/2,
                  /*reqLevel=*/0, /*petLevel=*/0, /*petLine=*/nullptr,
                  /*storageCap=*/kModCopyCapBase, /*beat=*/0);
    drawModDetail(ru, r, load, *reuse, /*equippedHere=*/false, /*slot=*/2,
                  /*reqLevel=*/0, /*petLevel=*/0, /*petLine=*/nullptr,
                  /*storageCap=*/kModCopyCapBase, /*beat=*/0);
    CHECK(anyNonPaper(os, 0, 0, kActiveW, kActiveH));     // renders
    // The ONE-SHOT line sits alone at y~130; present (non-paper ink) for the one-shot,
    // blank (all paper) for the reusable mod (whose effect text ends well above it).
    CHECK(anyNonPaper(os, 0, 128, kActiveW, 140));
    CHECK(!anyNonPaper(ru, 0, 128, kActiveW, 140));
}

// A mod holds at most one slot per pet: equip() refuses to install an id already
// occupying a different slot, and doesn't consume a spare on refusal.
void test_loadout_one_slot_per_mod() {
    Loadout l;
    CHECK(l.grant("firewall_patch", kModCopyCapBase));
    CHECK(l.grant("firewall_patch", kModCopyCapBase));
    CHECK(l.countOf("firewall_patch") == 2);
    l.equip(0, "firewall_patch");
    CHECK(std::strcmp(l.equipped(0), "firewall_patch") == 0);
    CHECK(l.countOf("firewall_patch") == 1);

    l.equip(1, "firewall_patch");
    CHECK(l.equipped(1) == nullptr);
    CHECK(l.countOf("firewall_patch") == 1);
}

// mod effects are DATA-DRIVEN (effectKind + magnitude) — makePlayerCombatant
// reads each equipped mod and pokes the matching Combatant field, replacing the old
// hardcoded per-mod `if`. A line-affinity mod adds its bonus only for a matching line.
void test_mod_effects_data_driven() {
    ContentRegistry r = ContentRegistry::embedded();
    const CreatureDef* pet = r.creature("paypup");        // Process, ransomware line
    CHECK(pet && pet->line && std::strcmp(pet->line, "ransomware") == 0);
    MoveLoadout ml = MoveLoadout::starting();
    Loadout empty;                                        // no slots filled
    Combatant base = makePlayerCombatant(r, *pet, ml, empty);
    auto build = [&](const char* mod) {
        Loadout l; l.grant(mod, kModCopyCapBase); l.equip(0, mod);
        return makePlayerCombatant(r, *pet, ml, l);
    };
    CHECK(build("crypto_coprocessor").powerMultPct == base.powerMultPct + 10);
    CHECK(build("tpm_chip").dmgReducePct == base.dmgReducePct + 15);
    CHECK(build("solid_state_cache").maxHealth == base.maxHealth + 12);
    Combatant oc = build("overclock_chip");
    CHECK(oc.speed == base.speed + 5);                                  // +speed
    CHECK(oc.powerMultPct == base.powerMultPct * (100 - 8) / 100);      // ...at a power cost
    CHECK(build("honeytoken").mods.mag(ModEffect::Thorns) == 4);
    CHECK(build("deadman_switch").mods.mag(ModEffect::DeathBlast) == 12);
    // Line affinity: Cipher ASIC is +10% cut generic, +10 more for a Ransomware pet (=20).
    CHECK(build("cipher_asic").dmgReducePct == base.dmgReducePct + 20);
    // The plain-magnitude originals apply their row value straight through.
    CHECK(build("firewall_patch").dmgReducePct == base.dmgReducePct + r.mod("firewall_patch")->magnitude);
    CHECK(build("clock_speed_boost").speed == base.speed + r.mod("clock_speed_boost")->magnitude);
    CHECK(build("raid_mirror").mods.armed(ModEffect::RaidMirror));
}

// Backup Drive's death-save (ItemEffect::ArmCombatShieldBuff, save v30) is NOT the RAID
// Mirror's negate-the-first-hit: a survivable hit lands in full and leaves the drive
// untouched, so it's still there for the blow that actually puts the pet down.
void test_backup_drive_death_save_ignores_survivable_hits() {
    ContentRegistry r = ContentRegistry::embedded();
    Combatant pc = mkCombatant(r, "P", 100, 5, {"quick_jab"});
    pc.itemShield = true;
    Combatant e = mkCombatant(r, "E", 100, 12, {"packet_storm"});  // fast → hits first
    Combat c; c.begin(pc, e, Combat::Stakes::Safe, 5);             // full Health: survivable
    c.step();
    CHECK(c.player().health < 100);              // the hit landed in full...
    CHECK(c.player().health > 0);
    CHECK(c.player().itemShield);                // ...and the drive is still held
    CHECK(c.player().backupUse == Combatant::BackupUse::None);
}

// A pet that goes down is restored with half its MAX Health, measured from where it
// actually landed — so the same drive gives back the same amount whatever knocked it
// over. Records BackupUse::Restored (unlike mirrorFired, not reset per-turn) so Game can
// clear the buff's timed save-side deadline once the fight ends.
void test_backup_drive_death_save_restores_half_max_health() {
    ContentRegistry r = ContentRegistry::embedded();
    Combatant pc = mkCombatant(r, "P", 100, 5, {"quick_jab"});
    pc.itemShield = true;
    Combatant e = mkCombatant(r, "E", 100, 12, {"quick_jab"});
    // begin() always resets Health to maxHealth unless told to carry a wounded value in
    // (the gauntlet no-heal-between-rounds path) — use that to start on death's door.
    Combat c; c.begin(pc, e, Combat::Stakes::Safe, 5, /*forceEnemyFirst=*/false,
                       /*carryPlayerHealth=*/1);
    const int jab = r.move("quick_jab")->power;
    c.step();
    // 1 Health, minus the jab that took it under, plus the drive's half of max (50).
    CHECK(c.player().health == 1 - jab + 50);
    CHECK(c.player().health > 0);
    CHECK(!c.player().itemShield);               // spent
    CHECK(c.player().backupUse == Combatant::BackupUse::Restored);  // Game reads this post-fight
    CHECK(c.outcome() != Combat::Outcome::Lose); // the fight did NOT end here
}

// The save asks one question — is this pet down? — so it needs to know nothing about
// what put it there: a fatal DoT tick at turn-start restores exactly like a fatal hit.
void test_backup_drive_death_save_covers_a_fatal_dot() {
    ContentRegistry r = ContentRegistry::embedded();
    Combatant pc = mkCombatant(r, "P", 100, 12, {"quick_jab"});   // fast → its turn first
    pc.itemShield = true;
    pc.dotPerTurn = 8;
    pc.dotTurnsLeft = 3;
    Combatant e = mkCombatant(r, "E", 100, 5, {"quick_jab"});
    Combat c; c.begin(pc, e, Combat::Stakes::Safe, 5, /*forceEnemyFirst=*/false,
                       /*carryPlayerHealth=*/5);
    c.step();                                    // the pet's turn opens with the rot tick
    CHECK(c.player().health == 5 - 8 + 50);      // rotted under, then restored
    CHECK(c.player().backupUse == Combatant::BackupUse::Restored);
    CHECK(c.outcome() != Combat::Outcome::Lose);
}

// ...and it is a restore, not immortality: half of max added to a deep enough hole still
// leaves the pet under, and it dies. This is the case that keeps the save honest — it
// falls straight out of adding a fixed amount rather than setting a floor.
void test_backup_drive_death_save_loses_to_overkill() {
    ContentRegistry r = ContentRegistry::embedded();
    Combatant pc = mkCombatant(r, "P", 100, 12, {"quick_jab"});
    pc.itemShield = true;
    pc.dotPerTurn = 90;                          // 5 - 90 = -85, past the drive's 50
    pc.dotTurnsLeft = 3;
    Combatant e = mkCombatant(r, "E", 100, 5, {"quick_jab"});
    Combat c; c.begin(pc, e, Combat::Stakes::Safe, 5, /*forceEnemyFirst=*/false,
                       /*carryPlayerHealth=*/5);
    c.step();
    CHECK(c.player().health == 0);               // clamped at the floor, and gone
    CHECK(c.player().backupUse == Combatant::BackupUse::Overwhelmed);  // spent trying
    CHECK(c.outcome() == Combat::Outcome::Lose);
}

// Thorns (Honeytoken) reflects onto any attacker; Deadman Switch blasts the
// enemy when the pet is KO'd (a mutual KO resolves as a Win via enemy-death priority).
void test_mod_thorns_and_deathblast() {
    ContentRegistry r = ContentRegistry::embedded();
    // Thorns: a fast enemy hits the player and takes the reflect back.
    Combatant pc = mkCombatant(r, "P", 100, 5, {"quick_jab"});
    pc.mods.arm(ModEffect::Thorns, 4);
    Combatant enemy = mkCombatant(r, "E", 100, 12, {"quick_jab"});     // fast → acts first
    Combat c; c.begin(pc, enemy, Combat::Stakes::Safe, 5);
    c.step();                                                          // enemy hits player
    CHECK(c.player().health < 100);                                    // player took the hit
    CHECK(c.enemy().health == 96);                                     // ...reflected 4 back
    // Deadman Switch: a hit that KOs the pet blasts the enemy for deathBlast. A blast
    // that also drops the enemy is a mutual KO → resolves as a Win (enemy-death priority).
    Combatant weak = mkCombatant(r, "P", 5, 5, {"quick_jab"});
    weak.mods.arm(ModEffect::DeathBlast, 12);
    Combatant strong = mkCombatant(r, "E", 12, 12, {"packet_storm"});  // 12 dmg → KOs the 5hp pet
    Combat d; d.begin(weak, strong, Combat::Stakes::Safe, 5);
    d.step();                                                          // enemy KO → deathblast
    CHECK(d.player().health == 0);
    CHECK(d.enemy().health == 0);                                      // 12 - 12 parting blast
    CHECK(d.outcome() == Combat::Outcome::Win);                        // mutual KO = a Win
    // A blast that does NOT finish the enemy is still a Loss (no free win).
    Combatant weak2 = mkCombatant(r, "P", 5, 5, {"quick_jab"});
    weak2.mods.arm(ModEffect::DeathBlast, 12);
    Combatant tank = mkCombatant(r, "E", 40, 12, {"packet_storm"});    // survives the 12 blast
    Combat d2; d2.begin(weak2, tank, Combat::Stakes::Safe, 5);
    d2.step();
    CHECK(d2.enemy().health == 28);                                    // 40 - 12
    CHECK(d2.outcome() == Combat::Outcome::Lose);
}

// Deferred-mod pass — ECC Memory: a max single-hit cap. The primitive clamps any ONE
// incoming hit to the cap (after all mitigation); makePlayerCombatant resolves the
// mod's magnitude% of max Health into that flat cap.
void test_mod_ecc_memory_hitcap() {
    ContentRegistry r = ContentRegistry::embedded();
    // Primitive: a 20-power enemy hit is clamped to the cap; without the cap it lands full.
    auto oneHit = [&](int cap) {
        Combatant pc = mkCombatant(r, "P", 100, 5, {"quick_jab"});
        pc.mods.arm(ModEffect::MaxHitCapPct, cap);
        Combatant e = mkCombatant(r, "E", 100, 12, {"buffer_overflow"});  // 20 dmg, acts first
        Combat c; c.begin(pc, e, Combat::Stakes::Safe, 5);
        c.step();                                                          // enemy hits player
        return c.player().health;
    };
    CHECK(oneHit(0) == 80);      // uncapped: 100 - 20
    CHECK(oneHit(15) == 85);     // capped at 15: 100 - 15
    CHECK(oneHit(30) == 80);     // cap above the hit is inert (still 100 - 20)
    // Data-driven wiring: equipping ecc_memory resolves cap = 35% of the pet's max Health.
    const CreatureDef* pet = r.creature("paypup");
    MoveLoadout ml = MoveLoadout::starting();
    Loadout empty;
    Combatant base = makePlayerCombatant(r, *pet, ml, empty);
    CHECK(base.mods.mag(ModEffect::MaxHitCapPct) == 0);                   // no mod → uncapped
    Loadout l; l.grant("ecc_memory", kModCopyCapBase); l.equip(0, "ecc_memory");
    Combatant ecc = makePlayerCombatant(r, *pet, ml, l);
    CHECK(ecc.mods.mag(ModEffect::MaxHitCapPct) == base.maxHealth * 35 / 100);
    CHECK(ecc.mods.mag(ModEffect::MaxHitCapPct) > 0);
}

// Deferred-mod pass — Load Balancer: a big hit is SPLIT (immediate portion now, the rest
// deferred to the victim's next turn-start). Unlike ECC it doesn't reduce total damage.
void test_mod_load_balancer_split() {
    ContentRegistry r = ContentRegistry::embedded();
    // Split + timing: a 20 hit (>= threshold 10) with split 50% lands 10 now, 10 next turn.
    Combatant pc = mkCombatant(r, "P", 100, 5, {"quick_jab"});
    pc.mods.arm(ModEffect::LoadBalance, 10, 50);
    Combatant e = mkCombatant(r, "E", 100, 5, {"buffer_overflow"});        // 20 dmg
    // Equal speed + forceEnemyFirst → strict E,P,E,P (enemy leads, no double-actions).
    Combat c; c.begin(pc, e, Combat::Stakes::Safe, 5, /*forceEnemyFirst=*/true);
    c.step();                                                              // enemy hits player
    CHECK(c.player().health == 90);                                        // only the immediate half
    c.step();                                                              // player's turn-start tick
    CHECK(c.player().health == 80);                                        // deferred half now due
    CHECK(c.enemy().health == 94);                                         // ...and the pet still acted (jab 6)
    // Below the threshold: the hit is NOT split — it lands full immediately, no debt.
    Combatant pc2 = mkCombatant(r, "P", 100, 5, {"quick_jab"});
    pc2.mods.arm(ModEffect::LoadBalance, 25, 50);                          // 20 < 25 → untouched
    Combat c2; c2.begin(pc2, e, Combat::Stakes::Safe, 5, /*forceEnemyFirst=*/true);
    c2.step();  CHECK(c2.player().health == 80);                           // full 20 now
    c2.step();  CHECK(c2.player().health == 80);                           // no deferred tick
    // The deferred debt can KO ON ITS OWN at turn-start — and the actor then doesn't act.
    Combatant weak = mkCombatant(r, "P", 12, 5, {"quick_jab"});
    weak.mods.arm(ModEffect::LoadBalance, 10, 50);
    Combat d; d.begin(weak, e, Combat::Stakes::Safe, 5, /*forceEnemyFirst=*/true);
    d.step();                                                              // 20 → 10 now (12→2), 10 deferred
    CHECK(d.player().health == 2 && d.outcome() == Combat::Outcome::Ongoing);
    d.step();                                                              // turn-start debt: 2 - 10 → KO
    CHECK(d.player().health == 0);
    CHECK(d.outcome() == Combat::Outcome::Lose);
    CHECK(d.enemy().health == 100);                                        // pet never got to swing
    CHECK(std::strcmp(d.lastMoveName(), "OVERLOAD") == 0);
    // Data-driven wiring: equipping load_balancer resolves threshold = 30% of max Health,
    // split = 50%; an unmodded pet has no threshold (feature off).
    const CreatureDef* pet = r.creature("paypup");
    MoveLoadout ml = MoveLoadout::starting();
    Loadout empty;
    Combatant base = makePlayerCombatant(r, *pet, ml, empty);
    CHECK(base.mods.mag(ModEffect::LoadBalance) == 0);
    Loadout l; l.grant("load_balancer", kModCopyCapBase); l.equip(0, "load_balancer");
    Combatant lb = makePlayerCombatant(r, *pet, ml, l);
    CHECK(lb.mods.mag(ModEffect::LoadBalance) == base.maxHealth * 30 / 100);
    CHECK(lb.mods.mag(ModEffect::LoadBalance) > 0);
    CHECK(lb.mods.mag2(ModEffect::LoadBalance) == 50);
}

// Deferred-mod pass — Watchdog Timer: the THREAT is a STUN move (system_hang, lockTurns)
// that freezes the player's turns; the MOD clamps how many turns a stun may last (a hung
// process reboots after N turns).
void test_mod_watchdog_timer() {
    ContentRegistry r = ContentRegistry::embedded();
    // Burn: a pre-set 2-turn stun costs exactly 2 turns and deals NO damage in either
    // direction — the enemy is untouched until the pet is free.
    Combatant pc = mkCombatant(r, "P", 100, 12, {"quick_jab"});            // acts first
    pc.lockedTurnsLeft = 2;
    Combatant e = mkCombatant(r, "E", 100, 12, {"quick_jab"});            // equal → P,E,P,E,P
    Combat c; c.begin(pc, e, Combat::Stakes::Safe, 5);
    c.step();  CHECK(c.enemy().health == 100);                            // turn 1 skipped
    CHECK(std::strcmp(c.lastMoveName(), "STUN LOCK") == 0);
    c.step();                                                             // enemy hits pet
    c.step();  CHECK(c.enemy().health == 100);                            // turn 2 still skipped
    CHECK(std::strcmp(c.lastMoveName(), "STUN LOCK") == 0);
    c.step();                                                             // enemy hits pet
    c.step();  CHECK(c.enemy().health == 94);                             // freed → jab lands (6)
    // Clamp at application: the STUN move sets the lock to lockTurns (2), but the pet's
    // Watchdog clamps it to its own magnitude. Read the live lock right after the hit lands.
    auto lockAfterHit = [&](int watchdog) {
        Combatant p = mkCombatant(r, "P", 100, 5, {"quick_jab"});         // slow → stunned
        p.mods.arm(ModEffect::WatchdogClamp, watchdog);
        Combatant s = mkCombatant(r, "E", 100, 12, {"system_hang"});      // lockTurns 2, first
        Combat cb; cb.begin(p, s, Combat::Stakes::Safe, 5);
        cb.step();                                                        // enemy stuns the pet
        return cb.player().lockedTurnsLeft;
    };
    CHECK(lockAfterHit(0) == 2);                                          // no watchdog: full 2
    CHECK(lockAfterHit(1) == 1);                                          // watchdog clamps to 1
    // Data-driven wiring: equipping watchdog_timer sets the clamp to its magnitude (1).
    const CreatureDef* pet = r.creature("paypup");
    MoveLoadout ml = MoveLoadout::starting();
    Loadout empty;
    Combatant base = makePlayerCombatant(r, *pet, ml, empty);
    CHECK(base.mods.mag(ModEffect::WatchdogClamp) == 0);
    Loadout l; l.grant("watchdog_timer", kModCopyCapBase); l.equip(0, "watchdog_timer");
    Combatant w = makePlayerCombatant(r, *pet, ml, l);
    CHECK(w.mods.mag(ModEffect::WatchdogClamp) == 1);
}

// Deferred-mod pass — Faraday Cage: the THREAT is a DoT move (data_rot, dot*) that plants
// corruption ticking at each of the victim's turn-starts; the MOD cuts (or negates) it.
void test_mod_faraday_cage() {
    ContentRegistry r = ContentRegistry::embedded();
    // DoT tick: a pre-set 5/turn × 3 DoT bites at the START of each of the pet's next 3 turns,
    // independent of acting. Enemy only DEFENDS, so the pet's Health drops purely from the DoT.
    Combatant pc = mkCombatant(r, "P", 100, 12, {"quick_jab"});           // acts first
    pc.dotPerTurn = 5; pc.dotTurnsLeft = 3;
    Combatant e = mkCombatant(r, "E", 200, 5, {"checksum_guard"});        // never hits back
    Combat c; c.begin(pc, e, Combat::Stakes::Safe, 5);
    c.step();  CHECK(c.player().health == 95);                            // tick 1 (-5)
    c.step();                                                             // enemy defends
    c.step();  CHECK(c.player().health == 90);                            // tick 2
    c.step();
    c.step();  CHECK(c.player().health == 85);                            // tick 3
    c.step();
    c.step();  CHECK(c.player().health == 85);                            // DoT spent — no tick 4
    // Faraday cut at application: the DoT magnitude is reduced (or zeroed) when it's PLANTED.
    auto dotAfterHit = [&](int cut) {
        Combatant p = mkCombatant(r, "P", 100, 5, {"quick_jab"});
        p.mods.arm(ModEffect::FaradayCut, cut);
        Combatant s = mkCombatant(r, "E", 100, 12, {"data_rot"});         // 5/turn, acts first
        Combat cb; cb.begin(p, s, Combat::Stakes::Safe, 5);
        cb.step();                                                        // enemy plants the DoT
        return cb.player().dotPerTurn;
    };
    CHECK(dotAfterHit(0) == 5);                                           // no cage: full 5/turn
    CHECK(dotAfterHit(50) == 2);                                          // 50% cut: 5*50/100
    CHECK(dotAfterHit(100) == 0);                                         // immune: nothing planted
    // The DoT can KO on its own at a turn-start (and the pet then doesn't act).
    Combatant weak = mkCombatant(r, "P", 8, 12, {"quick_jab"});
    weak.dotPerTurn = 5; weak.dotTurnsLeft = 3;
    Combatant tank = mkCombatant(r, "E", 200, 5, {"checksum_guard"});
    Combat d; d.begin(weak, tank, Combat::Stakes::Safe, 5);
    d.step();  CHECK(d.player().health == 3 && d.outcome() == Combat::Outcome::Ongoing);
    d.step();                                                             // enemy defends
    d.step();  CHECK(d.player().health == 0);                             // 3 - 5 → KO
    CHECK(d.outcome() == Combat::Outcome::Lose);
    CHECK(std::strcmp(d.lastMoveName(), "CORRUPTED") == 0);
    // Data-driven wiring: equipping faraday_cage sets the cut to its magnitude (100 = immune).
    const CreatureDef* pet = r.creature("paypup");
    MoveLoadout ml = MoveLoadout::starting();
    Loadout empty;
    Combatant base = makePlayerCombatant(r, *pet, ml, empty);
    CHECK(base.mods.mag(ModEffect::FaradayCut) == 0);
    Loadout l; l.grant("faraday_cage", kModCopyCapBase); l.equip(0, "faraday_cage");
    Combatant f = makePlayerCombatant(r, *pet, ml, l);
    CHECK(f.mods.mag(ModEffect::FaradayCut) == 100);
}

// content calibration: each mod carries the right rarity, its power tier agrees with
// where it actually DROPS, and the signature Cipher ASIC is the ransomware-affinity mod.
void test_mod_content_rarity_tier() {
    ContentRegistry r = ContentRegistry::embedded();
    struct Exp { const char* id; ItemDef::Rarity rar; };
    const Exp exp[] = {
        {"clock_speed_boost", ItemDef::Rarity::Common},
        {"packet_sniffer", ItemDef::Rarity::Common},
        {"crypto_coprocessor", ItemDef::Rarity::Uncommon},
        {"tpm_chip", ItemDef::Rarity::Uncommon},
        {"solid_state_cache", ItemDef::Rarity::Uncommon},
        {"firewall_patch", ItemDef::Rarity::Rare},
        {"watchdog_timer", ItemDef::Rarity::Epic},  // threat-adjacent (Pirate Bayou)
        {"overclock_chip", ItemDef::Rarity::Uncommon},
        {"heat_sink", ItemDef::Rarity::Rare},
        {"honeytoken", ItemDef::Rarity::Rare},
        {"cipher_asic", ItemDef::Rarity::Rare},
        {"faraday_cage", ItemDef::Rarity::Epic},    // threat-adjacent (Napstorrent)
        {"deadman_switch", ItemDef::Rarity::Epic},
        {"raid_mirror", ItemDef::Rarity::Epic},
        {"ecc_memory", ItemDef::Rarity::Epic},
        {"load_balancer", ItemDef::Rarity::Epic},
        // --- Niche-flavour mods ---
        {"canary_trap", ItemDef::Rarity::Rare},
        {"scratch_disk_buffer", ItemDef::Rarity::Common},
        {"botnet_swarm", ItemDef::Rarity::Uncommon},
        {"airgap_ward", ItemDef::Rarity::Uncommon},
        {"tripwire", ItemDef::Rarity::Rare},
        {"cold_storage", ItemDef::Rarity::Uncommon},
        {"prowlware", ItemDef::Rarity::Rare},
        {"meltdown_core", ItemDef::Rarity::Rare},
        {"zero_day_exploit", ItemDef::Rarity::Rare},
        {"phishing_rod", ItemDef::Rarity::Epic},
        {"extortion_ledger", ItemDef::Rarity::Epic},
        {"backup_uplink", ItemDef::Rarity::Rare},
    };
    for (const Exp& e : exp) {
        const ModDef* m = r.mod(e.id);
        CHECK(m);
        CHECK(m->rarity == e.rar);
    }
    // powerTier is NOT restated above, because it is not independent data: a rank IS a
    // ladder depth, so a mod's rank must equal the tier of the SHALLOWEST area whose pool
    // drops it. (Shallowest, not only: a deeper area may re-stock an earlier counter — the
    // keep does it for Watchdog and Faraday, the Net-Sea for Watchdog — and that restock
    // must not restate the mod as harder than where it first appears.) Checked by walking
    // the pools, so inserting or reordering an area is caught here as a contradiction
    // rather than passing on a table nobody re-derived.
    for (const ModDef* mp : r.allMods()) {
        const ModDef& m = *mp;
        int home = -1;
        for (int a = 0; a < kExplSectors && home < 0; ++a)
            for (int k = 0; k < area(a).modPoolCount; ++k)
                if (std::strcmp(area(a).modPoolIds[k], m.id) == 0) { home = areaTier(a); break; }
        if (home < 0)                                  // DeepWeb-only: one past the ladder,
            for (int k = 0; k < kAreaModsDeepWebCount; ++k)   // sharing the last rung's depth
                if (std::strcmp(kAreaModsDeepWeb[k], m.id) == 0) { home = areaTier(kAreaCount - 1); break; }
        CHECK(home > 0);                               // every mod drops SOMEWHERE
        CHECK(m.powerTier == home);
    }
    const ModDef* ca = r.mod("cipher_asic");
    CHECK(ca && ca->line && std::strcmp(ca->line, "ransomware") == 0);
    CHECK(ca->affinityBonus == 10);
    // Niche-flavour pass: the two hard-gated signatures carry ModDef::requiresLine (a
    // real EQUIP block, distinct from the soft `line`/`affinityBonus` every other mod
    // uses — Cipher ASIC above stays fully line-agnostic).
    // One hard-gated build-around PER LINE, each naming its own line. Walked off
    // kCreatureLines rather than a list of ids, so a line added without a mod of its own
    // fails here rather than shipping with nothing that speaks to what it is.
    for (const CreatureLine& cl : kCreatureLines) {
        int gated = 0;
        for (const ModDef* m : r.allMods())
            if (m->requiresLine && std::strcmp(m->requiresLine, cl.id) == 0) ++gated;
        if (gated != 1) std::printf("  LINE %s has %d hard-gated mods\n", cl.id, gated);
        CHECK(gated == 1);
    }
    // A hard gate only ever names a REAL line, and only ever sits on an effect kind that
    // would be inert off-line — the rule that keeps `requiresLine` from being reached for
    // where a soft `line`/`affinityBonus` would do (content_mods.cpp).
    for (const ModDef* m : r.allMods()) {
        if (!m->requiresLine) continue;
        CHECK(creatureLine(m->requiresLine) != nullptr);
        CHECK(m->effectKind == ModEffect::StealAmplifyPct ||
              m->effectKind == ModEffect::ExecOverridePct ||
              m->effectKind == ModEffect::ReplicaSpawnPct ||
              m->effectKind == ModEffect::PowerPct);   // Extortion Ledger, the one stat row
        CHECK(m->line == nullptr);                     // never both shapes at once
    }
    // Every line also has a SOFT-affinity mod, and it gates far shallower than the hard
    // one — "your line starts paying early" is the property, not "a line has some mod".
    for (const CreatureLine& cl : kCreatureLines) {
        int soft = 0, shallowest = kModEquipLevelMax, hard = 0;
        for (const ModDef* m : r.allMods()) {
            if (m->line && std::strcmp(m->line, cl.id) == 0) {
                ++soft;
                if (m->equipLevel < shallowest) shallowest = m->equipLevel;
            }
            if (m->requiresLine && std::strcmp(m->requiresLine, cl.id) == 0)
                hard = m->equipLevel;
        }
        if (soft < 1) std::printf("  LINE %s has no soft-affinity mod\n", cl.id);
        CHECK(soft >= 1);
        CHECK(shallowest < hard);
    }
}

// The equip ladder: gates are authored per row (ModDef::equipLevel) rather than derived
// from the tier, which buys the density this checks. Ordering is checked in the same
// place because authoring is exactly what makes it possible to break.
void test_mod_equip_ladder_is_ordered_and_dense() {
    ContentRegistry r = ContentRegistry::embedded();
    // Every gate is inside the ceiling, and a deeper TIER never gates shallower than a
    // shallower tier's deepest row. Tier still means ladder depth even though it no
    // longer computes the level, so the two must not contradict each other.
    int lo[kModPowerTiers + 1], hi[kModPowerTiers + 1];
    for (int t = 0; t <= kModPowerTiers; ++t) { lo[t] = kModEquipLevelMax + 1; hi[t] = -1; }
    for (const ModDef* m : r.allMods()) {
        CHECK(m->equipLevel >= 0 && m->equipLevel <= kModEquipLevelMax);
        CHECK(m->powerTier >= 1 && m->powerTier <= kModPowerTiers);
        if (m->equipLevel < lo[m->powerTier]) lo[m->powerTier] = m->equipLevel;
        if (m->equipLevel > hi[m->powerTier]) hi[m->powerTier] = m->equipLevel;
    }
    for (int t = 2; t <= kModPowerTiers; ++t) {
        if (hi[t - 1] >= lo[t])
            std::printf("  TIER %d tops at %d but tier %d starts at %d\n",
                        t - 1, hi[t - 1], t, lo[t]);
        CHECK(hi[t - 1] < lo[t]);
    }
    // DENSITY: walking the sorted gates, no step larger than kModLadderMaxGap. This is the
    // whole point of authoring the level per row — a raising pet should never cross more
    // than a couple of levels with nothing new to slot. Checked as a max STEP rather than
    // a per-bucket count so it stays true however the roster is grouped.
    constexpr int kModLadderMaxGap = 2;
    std::vector<int> gates;
    for (const ModDef* m : r.allMods()) gates.push_back(m->equipLevel);
    std::sort(gates.begin(), gates.end());
    CHECK(gates.front() == 0);                       // something is always equippable
    for (size_t i = 1; i < gates.size(); ++i) {
        if (gates[i] - gates[i - 1] > kModLadderMaxGap)
            std::printf("  LADDER gap %d -> %d\n", gates[i - 1], gates[i]);
        CHECK(gates[i] - gates[i - 1] <= kModLadderMaxGap);
    }
    // ...and it reaches deep enough that a SIXTH area extends the ladder rather than
    // forcing a re-band of every row already on it (content_mods.cpp).
    CHECK(gates.back() >= 60);
}

// earn path: an area's mod loot table draws ONLY that area's power tier (an
// early area never hands out a late mod), and grantMod rolls the per-instance
// equip-level gate inside the tier's band (the "lucky roll" mechanic).
void test_mod_earn_tables_and_reqlevel() {
    ContentRegistry r = ContentRegistry::embedded();
    Game g{StartMode::Hatched};
    // An area's table never rolls a mod from DEEPER than that area: a rung may re-stock
    // an earlier counter (the keep does, for both), but nothing hands out a mod from
    // further down the ladder than the player has reached. Walked off the ladder, so an
    // insert re-derives the bound instead of failing on a stale index.
    auto sampled = std::set<std::string>{};
    for (int i = 0; i < 120; ++i) {
        for (int a = 0; a < kExplSectors; ++a) {
            const char* id = g.debugRollAreaModId(a);
            CHECK(id);
            CHECK(r.mod(id)->powerTier <= areaTier(a));
            sampled.insert(id);
        }
        const char* dw = g.debugRollAreaModId(kDeepWebSector);
        CHECK(dw);
        CHECK(r.mod(dw)->powerTier == areaTier(kAreaCount - 1));  // DeepWeb: deepest only
        sampled.insert(dw);
    }
    // Every pooled mod is actually REACHABLE — the tier check above would pass just as
    // happily on a pool whose ids never came up, so assert the sample covers the union of
    // every table. That subsumes the per-id spot-checks this used to spell out (ECC, Load
    // Balancer, Watchdog, Faraday, Ghost Process, the niche-flavour ids), and it keeps
    // covering a new area's pool the day it lands without a line added here.
    for (int a = 0; a < kExplSectors; ++a)
        for (int k = 0; k < area(a).modPoolCount; ++k)
            CHECK(sampled.count(area(a).modPoolIds[k]) == 1);
    for (int k = 0; k < kAreaModsDeepWebCount; ++k)
        CHECK(sampled.count(kAreaModsDeepWeb[k]) == 1);
    // A mod's equip gate is its OWN, authored on the row, and the same for every copy —
    // there is no roll to land inside a band. Sampled off two real mods at opposite ends
    // of the ladder rather than named tiers, and asserted to be deterministic, which is
    // the property that replaced the roll.
    const ModDef* deep = r.mod("ghost_process");
    const ModDef* shallow = r.mod("packet_sniffer");
    CHECK(modEquipLevel(*deep) == deep->equipLevel);
    CHECK(modEquipLevel(*shallow) == 0);            // the shallowest rung
    CHECK(modEquipLevel(*deep) > modEquipLevel(*shallow));
    for (int i = 0; i < 8; ++i) {                   // every grant agrees with the row
        Game gd{StartMode::Hatched};
        gd.debugGrantMod("ghost_process");
        CHECK(gd.loadout().countOf("ghost_process") == 1);
        CHECK(modEquipLevel(*r.mod("ghost_process")) == modEquipLevel(*deep));
    }
}

// --- Niche-flavour mods -----------------------------------------------------------

// Data-driven wiring: makePlayerCombatant reads each new ModEffect kind and pokes the
// matching Combatant field, same idiom as test_mod_effects_data_driven above.
void test_mod_niche_flavour_data_driven() {
    ContentRegistry r = ContentRegistry::embedded();
    const CreatureDef* pet = r.creature("paypup");         // Process, ransomware line
    MoveLoadout ml = MoveLoadout::starting();
    Loadout empty;
    Combatant base = makePlayerCombatant(r, *pet, ml, empty);
    auto build = [&](const char* mod) {
        Loadout l; l.grant(mod, kModCopyCapBase); l.equip(0, mod);
        return makePlayerCombatant(r, *pet, ml, l);
    };
    CHECK(build("prowlware").mods.armed(ModEffect::FirstStrikeRankMult));
    CHECK(build("canary_trap").mods.mag(ModEffect::FirstHitCutPct) == 50);
    Combatant mc = build("meltdown_core");
    CHECK(mc.mods.mag(ModEffect::LowHealthPowerPct) == 30 &&
          mc.mods.mag2(ModEffect::LowHealthPowerPct) == 40);
    Combatant zd = build("zero_day_exploit");
    CHECK(zd.mods.mag(ModEffect::GambleBattlePowerPct) == 25 &&
          zd.mods.mag2(ModEffect::GambleBattlePowerPct) == 60);
    Combatant tw = build("tripwire");
    CHECK(tw.mods.mag(ModEffect::ConditionalThorns) == 10 &&
          tw.mods.mag2(ModEffect::ConditionalThorns) == 40);
    Combatant cs = build("cold_storage");
    CHECK(cs.maxHealth == base.maxHealth + 20);
    CHECK(cs.speed == base.speed - 2);
    CHECK(build("scratch_disk_buffer").dmgReducePct == base.dmgReducePct + 8);
    CHECK(build("phishing_rod").mods.mag(ModEffect::StealAmplifyPct) == 75);
    // Extortion Ledger's requiresLine gate is an EQUIP-time UI check (game_care.cpp),
    // not a combat-engine one — makePlayerCombatant applies whatever is already
    // installed, so it correctly still wires up here even off-line (see
    // test_mod_hard_line_gate for the actual gate enforcement).
    CHECK(build("extortion_ledger").powerMultPct == base.powerMultPct + 30);
    // Backup Uplink is read post-battle by the Game (like Packet Sniffer), not stored
    // on Combatant at all — assert its content data directly instead.
    const ModDef* bu = r.mod("backup_uplink");
    CHECK(bu && bu->effectKind == ModEffect::PostBattleBits && bu->magnitude == 20);
}

// Botnet Swarm / Air-Gap Ward: bonus scales with the COUNT of equipped Attack/Defend
// moves — built with an explicit loadout so the composition (2 Attack, 1 Defend) is
// self-evident rather than relying on MoveLoadout::starting()'s defaults.
void test_mod_botnet_swarm_and_airgap_ward() {
    ContentRegistry r = ContentRegistry::embedded();
    const CreatureDef* pet = r.creature("malbear");        // Script, 3 unlocked slots
    CHECK(pet);
    MoveLoadout ml = MoveLoadout::starting();
    ml.equip(0, "packet_storm");    // Attack
    ml.equip(1, "fork_bomb");       // Attack
    ml.equip(2, "checksum_guard");  // Defend
    Loadout empty;
    Combatant base = makePlayerCombatant(r, *pet, ml, empty);
    CHECK(base.moves.size() == 3);

    Loadout withSwarm; withSwarm.grant("botnet_swarm", kModCopyCapBase); withSwarm.equip(0, "botnet_swarm");
    Combatant sw = makePlayerCombatant(r, *pet, ml, withSwarm);
    CHECK(sw.powerMultPct == base.powerMultPct + 6 * 2);    // +6% per Attack move (2)

    Loadout withWard; withWard.grant("airgap_ward", kModCopyCapBase); withWard.equip(0, "airgap_ward");
    Combatant wd = makePlayerCombatant(r, *pet, ml, withWard);
    CHECK(wd.dmgReducePct == base.dmgReducePct + 6 * 1);    // +6% per Defend move (1)
}

// The per-kind combine rules (mod_state.cpp) — what happens when two equipped mods
// declare the same ModEffect. Asserted straight on ModStateSet::apply: the loadout can
// only reach a handful of these pairings with the shipped roster, but every rule has to
// hold for the next mod that lands on an existing kind.
void test_mod_state_combine_rules() {
    // Sum: magnitudes accumulate.
    ModStateSet sum;
    sum.apply(ModEffect::Thorns, 4, 0);
    sum.apply(ModEffect::Thorns, 3, 0);
    CHECK(sum.mag(ModEffect::Thorns) == 7);

    // HighestMag: the stronger cut wins outright and brings its OWN mag2 with it, so a
    // weaker copy can neither dilute it nor swap in a mismatched payload.
    ModStateSet high;
    high.apply(ModEffect::LowHealthPowerPct, 30, 40);
    high.apply(ModEffect::LowHealthPowerPct, 20, 90);
    CHECK(high.mag(ModEffect::LowHealthPowerPct) == 30 &&
          high.mag2(ModEffect::LowHealthPowerPct) == 40);
    high.apply(ModEffect::LowHealthPowerPct, 45, 10);
    CHECK(high.mag(ModEffect::LowHealthPowerPct) == 45 &&
          high.mag2(ModEffect::LowHealthPowerPct) == 10);

    // ...and a capped kind never records past its ceiling.
    ModStateSet capped;
    capped.apply(ModEffect::FaradayCut, 150, 0);
    CHECK(capped.mag(ModEffect::FaradayCut) == 100);

    // LowestMag: the tightest guarantee wins, so stacking a second copy can only ever
    // narrow the ceiling. A 0 magnitude reads as "no ceiling yet", not as the tightest.
    ModStateSet low;
    CHECK(low.mag(ModEffect::MaxHitCapPct) == 0);          // absent reads as 0
    low.apply(ModEffect::MaxHitCapPct, 0, 0);              // declares nothing
    CHECK(low.mag(ModEffect::MaxHitCapPct) == 0);
    low.apply(ModEffect::MaxHitCapPct, 35, 0);
    low.apply(ModEffect::MaxHitCapPct, 50, 0);             // looser — loses
    CHECK(low.mag(ModEffect::MaxHitCapPct) == 35);
    low.apply(ModEffect::MaxHitCapPct, 20, 0);             // tighter — wins
    CHECK(low.mag(ModEffect::MaxHitCapPct) == 20);
    // The winner's mag2 rides along (Load Balancer's split share follows its threshold).
    ModStateSet lb;
    lb.apply(ModEffect::LoadBalance, 30, 50);
    lb.apply(ModEffect::LoadBalance, 20, 75);
    CHECK(lb.mag(ModEffect::LoadBalance) == 20 && lb.mag2(ModEffect::LoadBalance) == 75);

    // HighestMag2: Tripwire's threshold is its SECOND magnitude, so the widest window
    // wins and carries its own reflect amount.
    ModStateSet tw;
    tw.apply(ModEffect::ConditionalThorns, 10, 40);
    tw.apply(ModEffect::ConditionalThorns, 99, 25);
    CHECK(tw.mag(ModEffect::ConditionalThorns) == 10 &&
          tw.mag2(ModEffect::ConditionalThorns) == 40);

    // Replace: a gamble has no "better" pair to prefer, so the last one equipped defines it.
    ModStateSet gamble;
    gamble.apply(ModEffect::GambleBattlePowerPct, 25, 60);
    gamble.apply(ModEffect::GambleBattlePowerPct, 10, 90);
    CHECK(gamble.mag(ModEffect::GambleBattlePowerPct) == 10 &&
          gamble.mag2(ModEffect::GambleBattlePowerPct) == 90);

    // Arm + one-shot: a second copy adds no extra use, and spend() burns exactly one.
    ModStateSet arm;
    CHECK(!arm.armed(ModEffect::RaidMirror) && !arm.spend(ModEffect::RaidMirror));
    arm.apply(ModEffect::RaidMirror, 0, 0);
    arm.apply(ModEffect::RaidMirror, 0, 0);
    CHECK(arm.armed(ModEffect::RaidMirror));
    CHECK(arm.spend(ModEffect::RaidMirror));
    CHECK(!arm.armed(ModEffect::RaidMirror) && !arm.spend(ModEffect::RaidMirror));

    // A kind that folds into a base stat (or is read post-battle) keeps no entry at all.
    ModStateSet none;
    none.apply(ModEffect::PowerPct, 10, 0);
    none.apply(ModEffect::PostBattleBits, 10, 0);
    CHECK(none.size() == 0);

    // arm() stages a passive at resolved values, one-shots included, and re-arming the
    // same kind overwrites rather than stacking a second entry.
    ModStateSet direct;
    direct.arm(ModEffect::FirstHitCutPct, 50);
    CHECK(direct.mag(ModEffect::FirstHitCutPct) == 50 &&
          direct.armed(ModEffect::FirstHitCutPct));
    direct.arm(ModEffect::FirstHitCutPct, 20);
    CHECK(direct.mag(ModEffect::FirstHitCutPct) == 20 && direct.size() == 1);
}

// Prowlware's rank: distinct Attack-move power tiers, ascending (weakest = rank 1),
// resolved per slot — including the empty-slot Quick Jab fallback.
void test_mod_prowlware_rank_computation() {
    ContentRegistry r = ContentRegistry::embedded();
    const CreatureDef* pet = r.creature("malbear");        // Script; slotKinds A,A,D,A
    CHECK(pet);
    MoveLoadout ml = MoveLoadout::starting();
    ml.equip(0, "quick_jab");       // power 6  -> weakest tier
    ml.equip(1, "packet_storm");    // power 12 -> strongest tier
    // slot 2 (Defend-typed) left unequipped -> falls back to quick_jab (Attack, power 6).
    Loadout l; l.grant("prowlware", kModCopyCapBase); l.equip(0, "prowlware");
    Combatant c = makePlayerCombatant(r, *pet, ml, l);
    CHECK(c.mods.armed(ModEffect::FirstStrikeRankMult));
    CHECK(c.moves.size() == 3);
    CHECK(attackPowerRank(c.moves, 0) == 1);   // quick_jab (6) -> weakest of 2 tiers
    CHECK(attackPowerRank(c.moves, 1) == 2);   // packet_storm (12) -> strongest
    CHECK(attackPowerRank(c.moves, 2) == 1);   // fallback quick_jab -> weakest
    CHECK(attackPowerRank(c.moves, 3) == 0);   // past the kit -> no rank
    CHECK(attackPowerRank(c.moves, -1) == 0);  // a hijacked cast indexes no slot
    // A Defend move never ranks, and a kit with one attack tier ranks 1 throughout (no
    // bonus) — the mod only pays off on a genuine power spread.
    Combatant flat = mkCombatant(r, "F", 100, 5, {"quick_jab", "checksum_guard"});
    CHECK(attackPowerRank(flat.moves, 0) == 1);
    CHECK(attackPowerRank(flat.moves, 1) == 0);
}

// Prowlware's runtime effect: the first landed damaging hit multiplies by rank, fires
// only once, and a fully-mirrored (0-damage) hit doesn't consume it.
void test_mod_prowlware_combat_effect() {
    ContentRegistry r = ContentRegistry::embedded();
    // Three distinct Attack tiers, strongest in slot 0 (rank 3). The Exploit override
    // commands that slot both times, so the two hits are the same move and the only
    // difference between them is whether Prowlware is still armed.
    Combatant pc = mkCombatant(r, "P", 100, 12,
                               {"buffer_overflow", "quick_jab", "packet_storm"});
    pc.mods.arm(ModEffect::FirstStrikeRankMult);
    Combatant e = mkCombatant(r, "E", 100, 5, {"quick_jab"});
    Combat c; c.begin(pc, e, Combat::Stakes::Safe, 5, /*forceEnemyFirst=*/false,
                      /*carryPlayerHealth=*/-1, /*exploitUses=*/2);
    c.openOverride(); c.commitOverride(); c.step();                // forced buffer_overflow
    CHECK(c.enemy().health == 100 - 20 * 3);                       // rank-3 multiplied
    CHECK(!c.player().mods.armed(ModEffect::FirstStrikeRankMult)); // consumed
    const int afterFirst = c.enemy().health;
    while (!c.playerTurnNext()) c.step();                          // let the enemy swing
    c.openOverride(); c.commitOverride(); c.step();                // the same move again
    CHECK(c.enemy().health == afterFirst - 20);                    // plain damage, no repeat

    Combatant pc2 = mkCombatant(r, "P", 100, 12, {"quick_jab"});
    pc2.mods.arm(ModEffect::FirstStrikeRankMult);
    Combatant e2 = mkCombatant(r, "E", 100, 5, {"quick_jab"});
    e2.mods.arm(ModEffect::RaidMirror);                           // negates the first hit
    Combat c2; c2.begin(pc2, e2, Combat::Stakes::Safe, 5);
    c2.step();
    CHECK(c2.enemy().health == 100);                              // fully mirrored
    CHECK(c2.player().mods.armed(ModEffect::FirstStrikeRankMult));  // not consumed
}

// Canary Trap: an extra cut on the FIRST hit taken this fight only.
void test_mod_canary_trap() {
    ContentRegistry r = ContentRegistry::embedded();
    Combatant pc = mkCombatant(r, "P", 100, 5, {"quick_jab"});    // slow, acts second
    pc.mods.arm(ModEffect::FirstHitCutPct, 50);
    Combatant e = mkCombatant(r, "E", 100, 12, {"packet_storm"}); // fast, power 12
    Combat c; c.begin(pc, e, Combat::Stakes::Safe, 5);
    c.step();                                                     // enemy's first hit
    CHECK(c.player().health == 100 - 6);                          // 12 cut 50% -> 6
    CHECK(!c.player().mods.armed(ModEffect::FirstHitCutPct));   // absorbed
    c.step();                                                     // player's turn
    c.step();                                                     // enemy's second hit — no cut
    CHECK(c.player().health == 100 - 6 - 12);
}

// Meltdown Core: attack power gains the comeback bonus only while at/under the
// health threshold — checked live every attack, not a one-shot.
void test_mod_meltdown_core() {
    ContentRegistry r = ContentRegistry::embedded();
    Combatant low = mkCombatant(r, "P", 100, 12, {"quick_jab"});
    low.health = 25;                                              // 25% <= the 30% threshold
    low.mods.arm(ModEffect::LowHealthPowerPct, 30, 40);
    Combatant e1 = mkCombatant(r, "E", 100, 5, {"quick_jab"});
    // begin() otherwise starts the player at full Health — carryPlayerHealth is what
    // actually seats the pre-set low.health (25) for the fight.
    Combat c1; c1.begin(low, e1, Combat::Stakes::Safe, 5, false, 25);
    c1.step();
    CHECK(c1.enemy().health == 100 - 6 * 140 / 100);              // 6 * 1.4 -> 8

    Combatant healthy = mkCombatant(r, "P", 100, 12, {"quick_jab"});
    healthy.health = 50;                                          // above the threshold
    healthy.mods.arm(ModEffect::LowHealthPowerPct, 30, 40);
    Combatant e2 = mkCombatant(r, "E", 100, 5, {"quick_jab"});
    Combat c2; c2.begin(healthy, e2, Combat::Stakes::Safe, 5, false, 50);
    c2.step();
    CHECK(c2.enemy().health == 100 - 6);                          // no bonus
}

// Zero-Day Exploit: a one-time gamble rolled in Combat::begin(). Tested at its two
// deterministic boundaries (0% and 100% chance) rather than reverse-engineering a seed.
void test_mod_zero_day_exploit() {
    ContentRegistry r = ContentRegistry::embedded();
    Combatant always = mkCombatant(r, "P", 100, 12, {"quick_jab"});
    always.mods.arm(ModEffect::GambleBattlePowerPct, 100, 60);
    Combatant e1 = mkCombatant(r, "E", 100, 5, {"quick_jab"});
    Combat c1; c1.begin(always, e1, Combat::Stakes::Safe, 5);
    CHECK(c1.player().powerMultPct == 160);

    Combatant never = mkCombatant(r, "P", 100, 12, {"quick_jab"});
    never.mods.arm(ModEffect::GambleBattlePowerPct, 0, 60);
    Combatant e2 = mkCombatant(r, "E", 100, 5, {"quick_jab"});
    Combat c2; c2.begin(never, e2, Combat::Stakes::Safe, 5);
    CHECK(c2.player().powerMultPct == 100);
}

// Tripwire: reflects only while own Health is at/under the threshold, and stays a
// SEPARATE accumulation from an always-on Thorns mod (Honeytoken) equipped alongside it.
void test_mod_tripwire() {
    ContentRegistry r = ContentRegistry::embedded();
    Combatant low = mkCombatant(r, "P", 100, 5, {"quick_jab"});
    low.health = 30;                                              // 30% <= the 40% threshold
    low.mods.arm(ModEffect::ConditionalThorns, 10, 40);
    Combatant e1 = mkCombatant(r, "E", 100, 12, {"quick_jab"});    // fast, hits first
    // carryPlayerHealth seats the pre-set low.health (30) — begin() otherwise resets
    // the player to full.
    Combat c1; c1.begin(low, e1, Combat::Stakes::Safe, 5, false, 30);
    c1.step();
    CHECK(c1.enemy().health == 100 - 10);                         // reflected

    Combatant healthy = mkCombatant(r, "P", 100, 5, {"quick_jab"});
    healthy.health = 100;                                         // above the threshold
    healthy.mods.arm(ModEffect::ConditionalThorns, 10, 40);
    Combatant e2 = mkCombatant(r, "E", 100, 12, {"quick_jab"});
    Combat c2; c2.begin(healthy, e2, Combat::Stakes::Safe, 5, false, 100);
    c2.step();
    CHECK(c2.enemy().health == 100);                              // dormant

    Combatant both = mkCombatant(r, "P", 100, 5, {"quick_jab"});
    both.health = 100;                                            // above Tripwire's threshold
    both.mods.arm(ModEffect::Thorns, 4);                           // Honeytoken: always-on
    both.mods.arm(ModEffect::ConditionalThorns, 10, 40);           // Tripwire: dormant here
    Combatant e3 = mkCombatant(r, "E", 100, 12, {"quick_jab"});
    Combat c3; c3.begin(both, e3, Combat::Stakes::Safe, 5, false, 100);
    c3.step();
    CHECK(c3.enemy().health == 100 - 4);                          // only Honeytoken fires
}

// Bubble-gated half of the steal track: stealSpeedPct/stealCurrentHpPct only fire while
// the caster's Obfuscation shield is up (shieldHp > 0) — the interplay the line is built
// around (the volatile bonus costs the defensive pool's uptime). Both combatants sit at
// speed 50 (not the usual 10) so 6% of it — smish_hook's stealSpeedPct — lands on an
// exact float (3.0) instead of a repeating binary fraction. Stage stays at its BootSector
// default so the Perfect-Bite chance is a guaranteed 0% (kPhishingBiteChancePctByStage[0]),
// isolating the BASE bubble-gated amounts from the chance-based bonus (see
// test_phishing_perfect_bite). Feed-frenzy also lives here, since a landed power siphon
// while the bubble's up triggers it.
//
// The track is CHAINED, not channelled: order is P(lure), E(quick_jab), P(strike), and
// every one of those turns resolves something. The lure carries the siphons; the strike
// (smish_strike, content_chain_steps.cpp) is the damage half and steals nothing.
void test_phishing_bubble_steal() {
    ContentRegistry r = ContentRegistry::embedded();
    auto run = [&](int shield, Combat& out) {
        Combatant pc = mkCombatant(r, "P", 100, 50, {"smish_hook"});
        pc.shieldHp = shield;
        Combatant e = mkCombatant(r, "E", 100, 50, {"quick_jab"});
        out.begin(pc, e, Combat::Stakes::Safe, 1);
        out.step(); out.step(); out.step();
    };
    // No bubble: the unconditional siphons fire (power, and the max-Health take), the
    // bubble-gated pair does not.
    Combat c0; run(0, c0);
    CHECK(c0.player().speed == 50 && c0.enemy().speed == 50);      // no speed siphon
    CHECK(c0.player().powerMultPct == 108 && c0.enemy().powerMultPct == 92);
    // The max-Health take is NOT bubble-gated, and the pool MOVES: 6% of the enemy's 100
    // crosses whole, ceiling and contents, so the pet is topped up at its new ceiling.
    CHECK(c0.enemy().maxHealth == 94 && c0.player().maxHealth == 106);
    // The power siphon then feeds back into BOTH sides' later damage, which is the whole
    // reason the line steals it: quick_jab lands 5 rather than 6 off the enemy's siphoned
    // 92%, and the strike lands 17 rather than 16 off the pet's 108%.
    // player: 100 + 6 (the pool that crossed) - 5 (quick_jab) = 101.
    CHECK(c0.player().health == 101);
    // enemy: 100 - 6 (lure) - 17 (strike) = 77. Two real casts, which is the point of the
    // chain — the wind-up this replaced spent the fight's first turn doing nothing.
    CHECK(c0.enemy().health == 77);

    // Bubble up: stealSpeedPct/stealCurrentHpPct now fire at their base (6%) amounts on
    // top of the power siphon, and the landed power siphon feeds Feed-Frenzy. The
    // player's OWN shieldHp(50) also absorbs quick_jab's 6 damage outright (the
    // pre-existing Obfuscation absorb, ahead of any steal effect) instead of it
    // touching Health, so Health only ever moves via this hit's own heals.
    Combat c1; run(50, c1);
    CHECK(c1.player().speed == 53.0f && c1.enemy().speed == 47.0f);   // 6% of 50 = 3
    // enemy: 100 - 6 (lure) - 5 (6% of 94, lifesteal) - 17 (siphon-boosted strike) = 72.
    CHECK(c1.enemy().health == 72);
    // player: the lifesteal lands while the ceiling is still 100 (so it caps there), the
    // crossed pool then lifts both to 106, and the frenzy's +1 caps again. quick_jab is
    // absorbed by the shield and never reaches Health.
    CHECK(c1.player().maxHealth == 106 && c1.player().health == 106);
}

// Perfect Bite: while the bubble's up, a stage-scaled chance (kPhishingBiteChancePctByStage)
// additionally doubles WHICHEVER of stealSpeedPct/stealCurrentHpPct lands the hit (picked at
// random when a move sets both, as every Phishing steal-attack does); the Phishing Rod's
// the Phishing Rod scales that bonus specifically, not the base amount. Daemon (45%) is the
// easiest stage to observe both a biting and a non-biting roll in a short scan. Stage is
// set directly on the Combatant purely to select the chance-table row — Combat itself never
// checks a move's minStage (that gate lives at the picker/equip layer).
void test_phishing_perfect_bite() {
    ContentRegistry r = ContentRegistry::embedded();
    auto run = [&](uint32_t seed, int amplify, Combat& out) {
        Combatant pc = mkCombatant(r, "P", 100, 50, {"smish_hook"});
        pc.stage = Stage::Daemon;
        pc.shieldHp = 50;
        pc.mods.arm(ModEffect::StealAmplifyPct, amplify);
        Combatant e = mkCombatant(r, "E", 100, 50, {"quick_jab"});
        out.begin(pc, e, Combat::Stakes::Safe, seed, /*forceEnemyFirst=*/false,
                  /*carryPlayerHealth=*/50);
        out.step(); out.step(); out.step();
    };
    // Baselines (no bite): speed 50 -> 53/47 (6% of 50). Health: quick_jab is absorbed by
    // the player's OWN shieldHp(50), not Health (the pre-existing Obfuscation absorb,
    // ahead of any steal effect), so carryPlayerHealth(50) only ever moves via the lure's
    // own take — +5 lifesteal (6% of 94), +6 for the max-Health pool crossing whole, +1
    // frenzy (floored) = 62. A bite doubles ONE of the two volatile steals: speed goes to
    // 56, or Health to 68, never both.
    constexpr float kBaseSpeed = 53.0f;
    constexpr int kBaseHealth = 62;

    uint32_t biteSpeedSeed = 0, biteHpSeed = 0, noBiteSeed = 0;
    for (uint32_t s = 1; s <= 80 && !(biteSpeedSeed && biteHpSeed && noBiteSeed); ++s) {
        Combat c; run(s, 0, c);
        const bool bitSpeed = c.player().speed > kBaseSpeed;
        const bool bitHp = c.player().health > kBaseHealth;
        CHECK(!(bitSpeed && bitHp));                 // exactly one stat bites per hit
        if (bitSpeed && !biteSpeedSeed) biteSpeedSeed = s;
        else if (bitHp && !biteHpSeed) biteHpSeed = s;
        else if (!bitSpeed && !bitHp && !noBiteSeed) noBiteSeed = s;
    }
    CHECK(biteSpeedSeed && biteHpSeed && noBiteSeed);   // all three outcomes reachable

    // Rod: re-running a biting seed with the Rod equipped scales that hit's bonus up
    // further (still only the bitten stat — the other stays at its base value).
    Combat plainSpeed; run(biteSpeedSeed, 0, plainSpeed);
    Combat rodSpeed; run(biteSpeedSeed, 75, rodSpeed);
    CHECK(rodSpeed.player().speed > plainSpeed.player().speed);
    CHECK(rodSpeed.player().health == plainSpeed.player().health);   // hp untouched

    Combat plainHp; run(biteHpSeed, 0, plainHp);
    Combat rodHp; run(biteHpSeed, 75, rodHp);
    CHECK(rodHp.player().health > plainHp.player().health);
    CHECK(rodHp.player().speed == plainHp.player().speed);   // speed untouched
}

// Feed-frenzy combo gate: the run is counted by the BUBBLE, not by move adjacency. A
// two-slot kit alternates strictly (no-consecutive-repeat over two moves), so the
// Spoof-Bubble cast lands between every pair of bites — and the run survives it, because
// raising the bubble is the thing the combo is gated ON. maxHealth is far above anything
// the pool reaches so the frenzy LEAN stays off and this measures the gate alone.
void test_phishing_frenzy_survives_the_bubble() {
    ContentRegistry r = ContentRegistry::embedded();
    Combatant pc = mkCombatant(r, "P", 4000, 50, {"smish_hook", "spoof_bubble"});
    pc.shieldHp = 200;                       // up from the first bite onward
    Combatant e = mkCombatant(r, "E", 4000, 50, {"quick_jab"});
    Combat cb;
    cb.begin(pc, e, Combat::Stakes::Safe, 1);
    for (int i = 0; i < 40; ++i) cb.step();

    // Interleaving the brace no longer restarts the run: several bites have banked, so
    // the streak is past its first cast and the flat bonus is non-zero. Under the old
    // adjacency rule this kit could never bank anything at all.
    CHECK(cb.player().phishStreak > 1);
    CHECK(cb.player().phishComboBonus > 0);
}

// ...and the run DOES break on a bite taken with the bubble down — the fail state that
// replaces the old "any other move breaks it". Same kit, same seed, no starting pool and
// no way to raise one (attack-only kit), so every cast is an exposed one.
void test_phishing_frenzy_breaks_when_exposed() {
    ContentRegistry r = ContentRegistry::embedded();
    Combatant pc = mkCombatant(r, "P", 4000, 50, {"smish_hook"});
    Combatant e = mkCombatant(r, "E", 4000, 50, {"quick_jab"});
    Combat cb;
    cb.begin(pc, e, Combat::Stakes::Safe, 1);
    for (int i = 0; i < 40; ++i) cb.step();
    CHECK(cb.player().phishStreak == 0);        // never got off the ground
    CHECK(cb.player().phishComboBonus == 0);    // and banked nothing

    // The break is about EXPOSURE, not about which move: a generic swing taken with the
    // bubble down ends a banked run too, so a mixed kit can't swing off-line moves
    // through the exposed stretch and re-bubble with its run intact.
    auto swing = [&](const char* moveId, int shield, int streakIn, Combat& out) {
        Combatant p2 = mkCombatant(r, "P", 4000, 50, {moveId});
        p2.shieldHp = shield;
        p2.phishStreak = streakIn;
        p2.phishComboBonus = 10;                // already banked; never decays either way
        Combatant e2 = mkCombatant(r, "E", 4000, 1, {"quick_jab"});
        out.begin(p2, e2, Combat::Stakes::Safe, 1);
        for (int i = 0; i < 6; ++i) out.step();
    };
    Combat exposed; swing("packet_storm", 0, 5, exposed);
    CHECK(exposed.player().phishStreak == 0);        // generic swing, bubble down: broken
    CHECK(exposed.player().phishComboBonus == 10);   // ...but the bank is never given back

    // ...and with the bubble UP a generic swing is frenzy-neutral: it neither advances
    // the run (it steals nothing) nor breaks it, which is what keeps a heavy off-line
    // hitter a real choice in a Phishing kit rather than a strictly wrong one.
    Combat covered; swing("packet_storm", 400, 5, covered);
    CHECK(covered.player().phishStreak == 5);
}

// The frenzy LEAN: a pool stacked past the pet's own max Health re-rolls Defend picks
// into Attack ones, and the ratchet reads the pool's HIGH-WATER mark, so it holds while
// the bubble is chewed down and releases only when the pool is actually overrun.
void test_phishing_frenzy_lean_ratchets_until_the_bubble_pops() {
    ContentRegistry r = ContentRegistry::embedded();
    // The ramp itself, at its most granular point — the combat screen draws its frenzy
    // ribs off this same function, so the tell and the behaviour can't drift apart.
    Combatant probe = mkCombatant(r, "P", 100, 50, {"smish_hook"});
    CHECK(phishFrenzyLeanPct(probe) == 0);            // no pool at all
    probe.phishShieldPeak = 100;
    CHECK(phishFrenzyLeanPct(probe) == 0);            // at max Health, not past it
    probe.phishShieldPeak = 150;
    CHECK(phishFrenzyLeanPct(probe) == 50);           // halfway to the full-lean point
    probe.phishShieldPeak = 200;
    CHECK(phishFrenzyLeanPct(probe) == kPhishFrenzyLeanMaxPct);        // 2x -> saturated
    probe.phishShieldPeak = 10000;
    CHECK(phishFrenzyLeanPct(probe) == kPhishFrenzyLeanMaxPct);        // ...and clamped

    // The attack half is quick_jab, not one of the line's own lures: a lure takes max
    // Health off the target and adds it to the caster, and this pet's maxHealth is the
    // DENOMINATOR the frenzy lean is measured against (phishFrenzyLeanPct). Fighting with
    // one would move the very quantity under test, and the ratchet would look unstable
    // when what actually changed was the pet's size.
    auto run = [&](int maxHp, int shield, int enemyPower, Combat& out) {
        Combatant pc = mkCombatant(r, "P", maxHp, 50, {"quick_jab", "spoof_bubble"});
        pc.shieldHp = shield;
        Combatant e = mkCombatant(r, "E", 4000, 50, {enemyPower ? "rootkit_strike"
                                                                : "quick_jab"});
        e.powerMultPct = enemyPower ? enemyPower : 100;
        out.begin(pc, e, Combat::Stakes::Safe, 1);
    };
    // maxHealth 20 against a 200 pool: peak is 10x max Health, so the lean saturates.
    // begin() seeds the ratchet off a pool the pet walked in with.
    Combat armed; run(20, 200, 0, armed);
    CHECK(armed.player().phishShieldPeak == 200);
    for (int i = 0; i < 40; ++i) armed.step();
    CHECK(armed.player().phishShieldPeak == 200);   // ratchet holds as the pool drains

    // The SAME kit and seed with max Health above the pool leaves the lean off, so the
    // pet keeps alternating and tops the bubble up instead of only spending it.
    Combat unarmed; run(4000, 200, 0, unarmed);
    for (int i = 0; i < 40; ++i) unarmed.step();
    CHECK(unarmed.player().shieldHp > armed.player().shieldHp);

    // Popping it releases the ratchet: a heavy hitter breaks through the pool, and the
    // peak clears so the pet returns to mixed play.
    Combat popped; run(20, 40, 900, popped);
    CHECK(popped.player().phishShieldPeak == 40);
    int guard = 0;
    while (popped.player().shieldHp > 0 && guard++ < 200) popped.step();
    CHECK(popped.player().shieldHp == 0);           // overrun
    CHECK(popped.player().phishShieldPeak == 0);    // ...and the lean is spent
}

// Obfuscation shield pool (Phishing defensive track): a shieldPool Defend move POOLS
// into shieldHp (a second health bar) that absorbs before real Health, overflows the
// remainder to Health when it pops, and STACKS additively on recast — distinct from the
// one-shot `guard` brace every other Defend move sets.
void test_phishing_shield_pool() {
    ContentRegistry r = ContentRegistry::embedded();
    const MoveDef* sb = r.move("spoof_bubble");
    CHECK(sb && sb->kind == MoveDef::Kind::Defend && sb->shieldPool > 0 && sb->power == 8);

    // Absorb + overflow: cast the bubble (shieldHp 8), then eat a 20-power hit — the
    // shield soaks 8 and pops, the remaining 12 carries through to Health.
    Combatant p = mkCombatant(r, "P", 100, 10, {"spoof_bubble"});
    Combatant e = mkCombatant(r, "E", 100, 10, {"buffer_overflow"});  // 20 power, no rider
    Combat c; c.begin(p, e, Combat::Stakes::Safe, 1);
    c.step();                                            // P casts Spoof-Bubble
    CHECK(c.player().shieldHp == 8);
    CHECK(c.player().health == 100);                     // nothing spent yet
    c.step();                                            // E hits for 20
    CHECK(c.player().shieldHp == 0);                     // popped
    CHECK(c.player().health == 88);                      // overflow 12 through to Health

    // A non-shield Defend move still sets the one-shot guard, NOT the pool.
    Combatant p2 = mkCombatant(r, "P", 100, 10, {"checksum_guard"});
    Combatant e2 = mkCombatant(r, "E", 100, 10, {"checksum_guard"});
    Combat c2; c2.begin(p2, e2, Combat::Stakes::Safe, 1);
    c2.step();
    CHECK(c2.player().shieldHp == 0);
    CHECK(c2.player().guard == 14);                      // ordinary brace, not pooled

    // Additive stacking: with an enemy that only defends (no damage), two player casts
    // pool to 16 rather than refreshing to 8.
    Combatant p3 = mkCombatant(r, "P", 100, 10, {"spoof_bubble"});
    Combatant e3 = mkCombatant(r, "E", 100, 10, {"checksum_guard"});
    Combat c3; c3.begin(p3, e3, Combat::Stakes::Safe, 1);
    c3.step();                                           // P bubble -> 8
    c3.step();                                           // E defends (no damage to P)
    c3.step();                                           // P bubble again -> 16 (stacks)
    CHECK(c3.player().shieldHp == 16);
}

// Speed action economy: relative speed drives how many actions each pet gets. Equal
// speed alternates strictly (matches the pre-scheduler engine); a faster pet acts more
// often, in proportion to the speed ratio.
void test_speed_action_economy() {
    ContentRegistry r = ContentRegistry::embedded();
    // Equal speed -> strict alternation. Both pets only defend, so nobody dies and the
    // scheduler can be observed over many turns.
    {
        Combatant p = mkCombatant(r, "P", 100, 10, {"checksum_guard"});
        Combatant e = mkCombatant(r, "E", 100, 10, {"checksum_guard"});
        Combat c; c.begin(p, e, Combat::Stakes::Safe, 1);
        bool want = c.playerActsFirst();
        for (int i = 0; i < 8; ++i) {
            CHECK(c.playerTurnNext() == want);
            c.step();
            want = !want;
        }
    }
    // A 3x-faster pet takes comfortably more than twice as many actions over a long run.
    {
        Combatant p = mkCombatant(r, "P", 100, 30, {"checksum_guard"});
        Combatant e = mkCombatant(r, "E", 100, 10, {"checksum_guard"});
        Combat c; c.begin(p, e, Combat::Stakes::Safe, 1);
        int pl = 0, en = 0;
        for (int i = 0; i < 40; ++i) {
            if (c.playerTurnNext()) ++pl; else ++en;
            c.step();
        }
        CHECK(pl > en * 2);
    }
}

// Minimum penetration: a real attack always lands >=1 through pure defensive mitigation
// (% cut + guard), so no pet is an invincible wall — but RAID Mirror's deliberate full
// negation is still exempt.
void test_min_damage_penetration() {
    ContentRegistry r = ContentRegistry::embedded();
    // Quick Jab (6) into a maxed damage-cut would round to 0; the floor lands 1.
    Combatant p = mkCombatant(r, "P", 100, 20, {"quick_jab"});
    Combatant e = mkCombatant(r, "E", 100, 5, {"checksum_guard"});
    e.dmgReducePct = 95;                                 // clamps high; 6 * ~15% -> 0
    Combat c; c.begin(p, e, Combat::Stakes::Safe, 1);
    c.step();                                            // P jabs the wall
    CHECK(c.enemy().health == 99);                       // 1 penetrated, not 0

    // RAID Mirror still negates the whole hit (its own branch, exempt from the floor).
    Combatant p2 = mkCombatant(r, "P", 100, 20, {"quick_jab"});
    Combatant e2 = mkCombatant(r, "E", 100, 5, {"checksum_guard"});
    e2.mods.arm(ModEffect::RaidMirror);
    Combat c2; c2.begin(p2, e2, Combat::Stakes::Safe, 1);
    c2.step();
    CHECK(c2.enemy().health == 100);                     // fully mirrored, 0 damage
}

// The HARD line gate (ModDef::requiresLine, niche-flavour pass): distinct from every
// other mod's soft `line`/`affinityBonus` bonus. Driven through the real MODS UI,
// mirroring test_mod_equip_level_gate's pattern — Paypup's starting owned spares are
// exactly {packet_sniffer, raid_mirror} (Loadout::starting()), so granting ONE more
// mod always lands it at picker row 2 (registry order, filtered to owned).
// A mod's equip gate — level a test pet to this and that mod is equippable. Derived from
// the mod's own rank rather than written as a literal, so shifting the ladder moves these
// tests' targets with it instead of stranding them under a level that used to be enough.
static int modGateCeiling(const char* id) {
    ContentRegistry r = ContentRegistry::embedded();
    return modEquipLevel(*r.mod(id));
}

void test_mod_hard_line_gate() {
    Game g{StartMode::Hatched};                             // Paypup, ransomware line
    const int deepGate = modGateCeiling("extortion_ledger");  // both spares share a rank
    while (g.combatLevel() < deepGate) g.debugAddCombatXp(g.xpToNextLevel());
    g.debugGrantMod("phishing_rod");                  // requiresLine = phishing (mismatch)
    enterLoadoutTab(g, 0);
    g.onButton(press(Button::A)); g.onButton(press(Button::A));  // listRow 0->2 (empty slot)
    g.onButton(press(Button::B));                           // open slot-2 picker
    g.onButton(press(Button::A)); g.onButton(press(Button::A));  // pick row 2 = phishing_rod
    g.onButton(press(Button::B));                           // open its detail
    g.onButton(press(Button::B));                           // EQUIP -> blocked (wrong line)
    CHECK(g.loadout().equipped(2) == nullptr);
    CHECK(g.loadout().owns("phishing_rod"));                // spare not consumed

    Game g2{StartMode::Hatched};
    while (g2.combatLevel() < deepGate) g2.debugAddCombatXp(g2.xpToNextLevel());
    g2.debugGrantMod("extortion_ledger");             // requiresLine = ransomware (match)
    enterLoadoutTab(g2, 0);
    g2.onButton(press(Button::A)); g2.onButton(press(Button::A));
    g2.onButton(press(Button::B));
    g2.onButton(press(Button::A)); g2.onButton(press(Button::A));
    g2.onButton(press(Button::B));
    g2.onButton(press(Button::B));                          // EQUIP -> allowed (matching line)
    CHECK(g2.loadout().slotOf("extortion_ledger") == 2);
    CHECK(!g2.loadout().owns("extortion_ledger"));
}

// the rolled equip-LEVEL gate — a spare above the pet's level can't be
// equipped; leveling past it unlocks the equip. Driven through the real MODS UI.
void test_mod_equip_level_gate() {
    Game g{StartMode::Hatched};                            // pet at level 0
    g.debugGrantMod("overclock_chip");               // a mid-ladder rank: req > 0
    const int ocGate = modGateCeiling("overclock_chip");
    CHECK(ocGate > 0);                                     // a mid-ladder rank does gate
    // Navigate: MODS → slot 3 (empty) picker → select the overclock spare → EQUIP.
    enterLoadoutTab(g, 0);
    g.onButton(press(Button::A)); g.onButton(press(Button::A));  // listRow 0→2 (empty slot)
    g.onButton(press(Button::B));                          // open slot-2 picker
    // Owned spares in registry order: packet_sniffer, overclock_chip, raid_mirror.
    g.onButton(press(Button::A));                          // pick row 1 = overclock
    g.onButton(press(Button::B));                          // open its detail
    g.onButton(press(Button::B));                          // EQUIP → blocked (under-level)
    CHECK(g.loadout().equipped(2) == nullptr);             // not installed
    CHECK(g.loadout().owns("overclock_chip"));             // spare not consumed
    // Level the pet above the gate, then the same equip succeeds.
    while (g.combatLevel() < ocGate) g.debugAddCombatXp(g.xpToNextLevel());
    enterLoadoutTab(g, 0);
    g.onButton(press(Button::A)); g.onButton(press(Button::A));
    g.onButton(press(Button::B));
    g.onButton(press(Button::A));
    g.onButton(press(Button::B));
    g.onButton(press(Button::B));                          // EQUIP → now allowed
    CHECK(g.loadout().slotOf("overclock_chip") == 2);
    CHECK(!g.loadout().owns("overclock_chip"));            // consumed on equip
}

// drawModPicker windowing (twin of train_screen's drawMovePicker fix): with more
// than kModPickerVisibleRows owned mods, the row list must CAP at the window and
// SCROLL to follow the cursor, never overrun into the description zone at y=150.
// Grants every embedded mod (16, well past the 6-row window) as an owned spare.
void test_mod_picker_windows_large_pool() {
    ContentRegistry r = ContentRegistry::embedded();
    Loadout load;
    const auto allMods = r.allMods();
    CHECK(allMods.size() > 6);                  // the pool this test needs to overflow
    for (const ModDef* m : allMods) load.grant(m->id, kModCopyCapBase);  // one spare each

    const auto owned = ownedModList(r, load, /*petLevel=*/99, /*petLine=*/nullptr,
                                    /*slot=*/0);
    const int lastPick = static_cast<int>(owned.size()) - 1;
    CHECK(lastPick >= 6);

    // (a) The focused row-cursor triangle for the LAST mod must still land inside
    // the windowed list band (y in [22,150)) — the pre-fix math (`kRowTop +
    // pick*rowH`) would place it at 26 + 15*18 = 296, off the 224-tall panel and
    // nowhere near the visible list.
    Framebuffer last(kActiveW, kActiveH);
    drawModPicker(last, r, load, /*slot=*/0, lastPick, /*confirmActive=*/false,
                  0, nullptr, /*petLevel=*/99, /*petLine=*/nullptr, /*beat=*/0);
    bool cursorInWindow = false;
    for (int y = 22; y < 150 && !cursorInWindow; ++y)
        for (int x = 6; x < 20; ++x)
            if (last.get(x, y) == palColor(Pal::ACCENT)) { cursorInWindow = true; break; }
    CHECK(cursorInWindow);

    // (b) The window actually FOLLOWS the cursor rather than just clipping: the
    // picker focused on the first mod renders differently from the one focused
    // on the last mod (different rows are on screen).
    Framebuffer first(kActiveW, kActiveH);
    drawModPicker(first, r, load, 0, 0, false, 0, nullptr, 99, nullptr, 0);
    CHECK(!fbEqual(first, last));

    // (c) The invariant holds across the whole pool, not just at the last index:
    // for every pick value the row-cursor triangle stays inside the window.
    for (int pick = 0; pick <= lastPick; pick += 3) {
        Framebuffer fb(kActiveW, kActiveH);
        drawModPicker(fb, r, load, 0, pick, false, 0, nullptr, 99, nullptr, 0);
        bool inWindow = false;
        for (int y = 22; y < 150 && !inWindow; ++y)
            for (int x = 6; x < 20; ++x)
                if (fb.get(x, y) == palColor(Pal::ACCENT)) { inWindow = true; break; }
        CHECK(inWindow);
    }
}

// Shared Resources (Worm), the SPEED half: a worm's speed is not its own — it is the
// opponent's, continuously. It matches at the opening bell (so the baseline the stat
// panel diffs against is already the matched value), it re-matches the moment the
// opponent's speed MOVES mid-fight, and the equal gauges it produces mean neither side
// ever takes two actions in a row.
void test_worm_shared_resources_speed() {
    ContentRegistry r = ContentRegistry::embedded();

    // Matched at the bell, from far behind: a speed-10 worm against a speed-40 enemy
    // opens the fight at 40, not at 10.
    Combatant slow = mkCombatant(r, "W", 100, 10, {"quick_jab"});
    slow.setLine(r, "worm");
    Combatant fast = mkCombatant(r, "E", 100, 40, {"quick_jab"});
    Combat c; c.begin(slow, fast, Combat::Stakes::Safe, 1);
    CHECK(c.player().speed == 40.0f);
    CHECK(c.player().baseSpeed == 40.0f);          // baselined AFTER the match
    // ...and lockstep speed means lockstep turns: nobody ever gets a second action
    // before the other has had one.
    for (int i = 0; i < 8; ++i) { c.step(); CHECK(c.streakCount() == 1); }

    // It TRACKS: a Phishing speed siphon drags the enemy's speed up and would normally
    // leave its victim behind — the worm comes with it instead, and the pair stay equal.
    Combatant worm = mkCombatant(r, "W", 100, 50, {"quick_jab"});
    worm.setLine(r, "worm");
    Combatant thief = mkCombatant(r, "E", 100, 50, {"smish_hook"});
    thief.shieldHp = 50;                            // the bubble the siphon is gated on
    Combat c2; c2.begin(worm, thief, Combat::Stakes::Safe, 1);
    for (int i = 0; i < 4; ++i) c2.step();
    CHECK(c2.enemy().speed > 50.0f);                // the siphon moved it
    CHECK(c2.player().speed == c2.enemy().speed);   // and the worm moved with it

    // No worm in the fight, nothing matched — an ordinary speed gap stays a speed gap.
    Combatant a = mkCombatant(r, "A", 100, 10, {"quick_jab"});
    Combatant b = mkCombatant(r, "B", 100, 40, {"quick_jab"});
    Combat c3; c3.begin(a, b, Combat::Stakes::Safe, 1);
    CHECK(c3.player().speed == 10.0f && c3.enemy().speed == 40.0f);
}

// Shared Resources (Worm), the ARITHMETIC half — the pure functions the balance is
// actually written in (combat.h). Each kind's magnitude comes from the OTHER kind's count
// AT SPAWN, floored at kWormReplicaMultFloor so the first copy of either sort is worth its
// base rather than nothing; incoming attacks draw a victim by weight, defenders hardest.
void test_worm_replica_arithmetic() {
    Combatant w;
    w.maxHealth = 100;
    w.powerMultPct = 100;
    CHECK(wormReplicaDamage(w) == 0);                     // an empty board adds nothing

    // One attacker, no defenders: the floor pays it its base and no more.
    w.wormReplicas[w.wormReplicaCount++] = {/*defender=*/false, 1, 1, /*attack=*/5};
    CHECK(wormReplicaCount(w, /*defenders=*/false) == 1);
    CHECK(wormReplicaDamage(w) == 5);
    // Cover arriving AFTERWARDS does not reach it. A copy banks what it is worth when it
    // spawns and keeps it: it has its own Health, its own damage and its own chance of
    // being hit, and a later arrival is a different thing, not a buff to this one.
    w.wormReplicas[w.wormReplicaCount++] = {/*defender=*/true, 20, 20, 0};
    w.wormReplicas[w.wormReplicaCount++] = {/*defender=*/true, 20, 20, 0};
    CHECK(wormReplicaCount(w, /*defenders=*/true) == 2);
    CHECK(wormReplicaDamage(w) == 5);

    // What the cover DOES buy is worth more teeth from here on, which is the same dial
    // read at the moment it matters — so spawn ORDER is the decision, not spawn count.
    Combatant uncovered;
    uncovered.maxHealth = 100;
    uncovered.powerMultPct = 100;
    const int alone = wormAttackerDamage(uncovered, /*movePower=*/10, /*pct=*/60);
    const int behindCover = wormAttackerDamage(w, /*movePower=*/10, /*pct=*/60);
    CHECK(alone == 6);                                    // 10 * 60% * 100% * floor(1)
    CHECK(behindCover == 12);                             // ...times the two defenders
    // ...and a copy is never worth nothing, however the shares round down.
    CHECK(wormAttackerDamage(uncovered, /*movePower=*/1, /*pct=*/1) >= 1);

    // A defender's Health is the mirror image: a share of the PARENT's max, times the
    // attackers already standing (and the same floor when there are none).
    Combatant q;
    q.maxHealth = 100;
    CHECK(wormDefenderHealth(q, 20) == 20);
    q.wormReplicas[q.wormReplicaCount++] = {false, 1, 1, 5};
    q.wormReplicas[q.wormReplicaCount++] = {false, 1, 1, 5};
    CHECK(wormDefenderHealth(q, 20) == 40);

    // Targeting: index 0 is the parent, 1+ the replicas, weighted by kind.
    const std::vector<int> wt = wormTargetWeights(w);
    CHECK(wt.size() == 4);
    CHECK(wt[0] == kWormTargetWeightParent);
    CHECK(wt[1] == kWormTargetWeightAttacker);
    CHECK(wt[2] == kWormTargetWeightDefender && wt[3] == kWormTargetWeightDefender);
    CHECK(kWormTargetWeightDefender > kWormTargetWeightAttacker);
    CHECK(kWormTargetWeightAttacker > kWormTargetWeightParent);

    // ...resolved by a pure roll, so a duel's two devices name the same victim. Total
    // weight is 1+2+4+4 = 11, laid out parent / attacker / defender / defender.
    CHECK(wormTargetPick(w, 0) == -1);                    // the parent itself
    CHECK(wormTargetPick(w, 1) == 0 && wormTargetPick(w, 2) == 0);
    CHECK(wormTargetPick(w, 3) == 1 && wormTargetPick(w, 6) == 1);
    CHECK(wormTargetPick(w, 7) == 2 && wormTargetPick(w, 10) == 2);
    CHECK(wormTargetPick(w, 11) == -1);                   // wraps

    // A worm with no copies out is always the target — nothing to hide behind.
    Combatant bare;
    CHECK(wormTargetPick(bare, 0) == -1 && wormTargetPick(bare, 999) == -1);
}

// Shared Resources (Worm) in a resolved fight: a Defend move puts a body on the board
// with certainty and stops at the slot cap; attacking copies pile onto the parent's own
// swings; and an attack aimed at the worm is often eaten by a copy instead, which dies
// and frees its slot.
void test_worm_replication_in_combat() {
    ContentRegistry r = ContentRegistry::embedded();
    const MoveDef* squat = r.move("host_squat");
    CHECK(squat && squat->kind == MoveDef::Kind::Defend && squat->replicaSpawnPct == 100);

    // A Defend cast is certain, and its body is sized off the parent (20% of 100, times
    // the floor with no attackers out). The enemy only braces, so nothing kills a copy.
    Combatant p = mkCombatant(r, "W", 100, 10, {"host_squat"});
    p.setLine(r, "worm");
    Combatant e = mkCombatant(r, "E", 100, 10, {"checksum_guard"});
    Combat c; c.begin(p, e, Combat::Stakes::Safe, 1);
    c.step();                                        // equal speed, the tie goes to P
    CHECK(c.player().wormReplicaCount == 1);
    CHECK(c.player().wormReplicas[0].defender);
    CHECK(c.player().wormReplicas[0].maxHealth == 20);
    // ...and the slots are a hard ceiling: four more of the worm's own turns add three.
    for (int i = 0; i < 8; ++i) c.step();
    CHECK(c.player().wormReplicaCount == kWormReplicaSlots);

    // Attacking copies pile onto the parent's swing, each adding exactly what it banked
    // when it spawned. Built directly on the Combatant so the arithmetic is read off one
    // swing rather than through a run of spawn rolls — which is also what makes the
    // banking visible here: these copies are handed their `attack` outright, so cover
    // standing beside them at swing time is cover that arrived too late to matter.
    auto swing = [&](int attackers, int defenders) {
        Combatant w = mkCombatant(r, "W", 100, 10, {"quick_jab"});   // 6 power
        w.setLine(r, "worm");
        for (int i = 0; i < attackers; ++i)
            w.wormReplicas[w.wormReplicaCount++] = {false, 1, 1, /*attack=*/4};
        for (int i = 0; i < defenders; ++i)
            w.wormReplicas[w.wormReplicaCount++] = {true, 20, 20, 0};
        Combatant t = mkCombatant(r, "T", 100, 10, {"checksum_guard"});
        Combat f; f.begin(w, t, Combat::Stakes::Safe, 1);
        f.step();                                    // the worm swings first
        return 100 - f.enemy().health;
    };
    CHECK(swing(0, 0) == 6);                         // the parent alone is feeble
    CHECK(swing(1, 0) == 10);                        // + one attacker's banked 4
    CHECK(swing(1, 2) == 10);                        // cover that arrived later adds none
    // What cover DOES buy is the next copy: banked behind two defenders, one attacker is
    // worth double, and it keeps that for the rest of the fight however the board moves.
    Combatant covered = mkCombatant(r, "W", 100, 10, {"quick_jab"});
    covered.powerMultPct = 100;
    for (int i = 0; i < 2; ++i)
        covered.wormReplicas[covered.wormReplicaCount++] = {true, 20, 20, 0};
    CHECK(wormAttackerDamage(covered, /*movePower=*/10, /*pct=*/20) == 4);   // 2 * floor2

    // An attack into a worm picks its victim by weight. Both outcomes are reachable, and
    // when a copy catches it the parent takes NOTHING — a copy is not armour, it is
    // somebody else standing there. A 1-Health attacker dies to it and frees its slot,
    // leaving the trace the combat screen plays the dissolve from.
    bool sawParentHit = false, sawCopyKilled = false;
    for (uint32_t seed = 1; seed <= 40; ++seed) {
        Combatant w = mkCombatant(r, "W", 100, 10, {"checksum_guard"});
        w.setLine(r, "worm");
        w.wormReplicas[w.wormReplicaCount++] = {false, 1, 1, 4};
        Combatant hitter = mkCombatant(r, "H", 100, 10, {"buffer_overflow"});  // 20 power
        Combat f; f.begin(w, hitter, Combat::Stakes::Safe, seed,
                          /*forceEnemyFirst=*/true);
        f.step();                                    // the enemy's opening swing
        if (f.player().wormReplicaCount == 0) {      // the copy ate it and popped
            CHECK(f.player().health == 100);         // ...and the worm is untouched
            CHECK(f.lastWormKill().happened && !f.lastWormKill().defender);
            CHECK(f.lastWormKill().onPlayer);
            sawCopyKilled = true;
        } else {                                     // the roll found the parent
            CHECK(f.player().health < 100);
            CHECK(!f.lastWormKill().happened);
            sawParentHit = true;
        }
    }
    CHECK(sawCopyKilled && sawParentHit);
}

// THE PANEL USED TO LIE BY OMISSION. Its absorbs-and-afflictions readout was packed
// into one string and drawn into a 24-character box, so a fighter carrying a shield, a
// brace, a drive, a ransom bill, traps, a rot and a stun at once had most of that cut —
// and a fight with all of them live is exactly the fight worth reading. The set is now
// a token list the draw WRAPS, so this asserts every live state is reported.
void test_combat_panel_reports_every_live_state() {
    ContentRegistry r = ContentRegistry::embedded();
    Combatant c = makeEnemyCombatant(r, simDummy(0));
    c.shieldHp = 40;
    c.itemShield = true;
    c.guard = 12;
    c.ransomPool = 30;
    c.ransomTurnsLeft = 2;
    c.trojanTrapCount = 2;
    c.wormReplicaCount = 3;
    c.dotPerTurn = 5;
    c.dotTurnsLeft = 3;
    c.lockedTurnsLeft = 1;
    c.stackPowerBonus = 8;
    c.stackDefenseBonus = 6;
    c.dmgReducePct = 20;
    c.speed = 14;
    c.baseSpeed = 17;                       // a siphon took three ticks
    c.powerMultPct = 88;
    c.basePowerMultPct = 100;
    c.crewExploit.kind = CrewExploitKind::NegateNextHits;
    c.crewExploit.charges = 2;

    // The panel reports a fighter across TWO surfaces, and "the operator cannot see it
    // mid-fight" is the defect either of them can carry — so both are checked here rather
    // than only the one that happens to be a list.
    //
    // The four VITALS are the VS page's column of digits, effective: after the siphon,
    // after the stack, under the never-immune clamp.
    const CombatVitals v = combatVitals(c);
    CHECK(v.power == 88 + 8);                // powerMultPct + stackPowerBonus, the SUM
    CHECK(v.defense == 20 + 6);              // dmgReducePct + the Cipher stack
    CHECK(v.defense <= kLevelDmgReduceMaxPct);
    CHECK(v.speed == 14);
    CHECK(v.maxHealth == c.maxHealth);

    // ...and everything else reaches the operator as a ROW of the VS grid: a tag with
    // each fighter's value under its own column. This is the assertion that matters —
    // "the operator cannot see it mid-fight" is the defect, and the grid is now the only
    // surface that can carry it.
    const Combatant clean = makeEnemyCombatant(r, simDummy(0));
    const CombatVsGrid g = combatVsGrid(c, clean, /*localGuard=*/true);
    for (const char* tag : {"HP", "PWR", "DEF", "SPD", "STUN", "DOT", "SHLD", "GRD",
                            "RNSM"})
        CHECK(g.has(tag));
    CHECK(g.n <= CombatVsGrid::kCap);        // the set fits its own store, unclipped

    // ...and the rest reaches the operator on the FIGHTER, as its status strip — which is
    // why they are not grid rows. Both surfaces are swept because "the operator cannot
    // see it mid-fight" is the defect either of them can carry, and moving a fact from
    // one to the other must not be a way to lose it.
    const CombatStatusStrip st = combatStatusStrip(c, /*withGuard=*/true);
    auto onStrip = [&](CombatVsKind k) {
        for (int i = 0; i < st.n; ++i) if (st.k[i] == k) return true;
        return false;
    };
    CHECK(onStrip(CombatVsKind::Backup));
    CHECK(onStrip(CombatVsKind::Trap));
    // A countable one repeats its glyph rather than printing a digit, so the count IS the
    // representation and a strip that showed one trap for a pile of two would be wrong.
    int traps = 0;
    for (int i = 0; i < st.n; ++i) if (st.k[i] == CombatVsKind::Trap) ++traps;
    CHECK(traps == c.trojanTrapCount);
    // Worm copies are on NEITHER surface on purpose: they are drawn as bodies on the
    // shelf (drawReplicaRow, ui/worm_replicas.h), which is the same fact said better, so
    // a glyph for them would be the third copy of one thing.
    CHECK(!g.has("COPY"));
    CHECK(!onStrip(CombatVsKind::Copy));
    CHECK(c.wormReplicaCount > 0);           // ...and the fixture really does have some
    CHECK(st.n <= CombatStatusStrip::kCap);
    // The deltas ride ON the vital they moved rather than as rows of their own — that
    // consolidation is what stopped a loaded fight from overrunning the box.
    for (int i = 0; i < g.n; ++i) {
        if (std::strcmp(g.r[i].tag, "PWR") == 0)
            CHECK(std::strcmp(g.r[i].local, "96-4") == 0);   // 88+8 effective, -12+8 moved
        if (std::strcmp(g.r[i].tag, "SPD") == 0)
            CHECK(std::strcmp(g.r[i].local, "14-3") == 0);   // a siphon took three ticks
    }
    // A fighter with none of it leaves the cell EMPTY rather than zero, which is what
    // lets the draw show a dash: "not in play" and "reported as nothing" must differ.
    for (int i = 0; i < g.n; ++i)
        if (std::strcmp(g.r[i].tag, "STUN") == 0) CHECK(g.r[i].rival[0] == '\0');

    // Two clean fighters produce the four vitals and nothing else — an ordinary wild
    // encounter does not open with a list of conditions that are not there.
    Combatant plain = makeEnemyCombatant(r, simDummy(0));
    plain.basePowerMultPct = plain.powerMultPct;
    plain.baseSpeed = plain.speed;
    const CombatVsGrid pg = combatVsGrid(plain, clean, /*localGuard=*/true);
    CHECK(pg.n == 4);
    CHECK(pg.has("HP") && pg.has("PWR") && pg.has("DEF") && pg.has("SPD"));
    // ...and its vitals still report, which is the half that must never go quiet.
    const CombatVitals pv = combatVitals(plain);
    CHECK(pv.power == plain.powerMultPct);
    CHECK(pv.maxHealth > 0);
}

// B CYCLES the panel rather than toggling it — closed, VS, KIT, closed — and paging
// it never pauses the fight, which is what makes reading it a real decision.
void test_combat_panel_pages_cycle() {
    Game g{StartMode::Hatched, "paypup"};
    g.debugStartCombat(/*live=*/false);
    CHECK(g.nav() == Game::Nav::Combat);
    CHECK(g.combatStatsPage() == 0);
    for (int expect = 1; expect <= kCombatStatPages; ++expect) {
        g.onButton(press(Button::B));
        CHECK(g.combatStatsPage() == expect);
    }
    g.onButton(press(Button::B));
    CHECK(g.combatStatsPage() == 0);          // wraps closed rather than sticking open
    // The next fight opens closed, whatever the last one was left on.
    g.onButton(press(Button::B));
    CHECK(g.combatStatsPage() == 1);
    while (g.combat().outcome() == Combat::Outcome::Ongoing) g.onButton(press(Button::A));
    g.onButton(press(Button::B));
    g.debugStartCombat(/*live=*/false);
    CHECK(g.combatStatsPage() == 0);
}
