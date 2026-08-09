// test_creature_lines.cpp — native gates for the creature lines, the Trojan divert and the Clutch Pick.
//
// One slice of the native suite; see test_gates.h for the shared fixtures and
// test_main.cpp for the roster that runs these. A save-vNN gate sits with the
// feature whose field it migrates, not in a migrations pile of its own.
#include "test_gates.h"

// --- New lines: cat (Ransomware) + frog (Phishing) -------------------------

// The CryptoShell (Ransomware) egg's random hatch pool holds Paypup, Conkittenate
// AND Pingcub — every Process creature on the ransomware line is a
// candidate, so the same egg can hatch any of the three species. None of them is
// deep-dive gated the way Phishlet is, so all three are in the pool from a fresh save.
void test_hatch_pool_ransomware() {
    ContentRegistry r = ContentRegistry::embedded();
    Game g;                                     // fresh save, no achievements
    bool paypup = false, conk = false, pingcub = false;
    for (const CreatureDef* c : r.allCreatures())
        if (c->stage == Stage::Process && c->line &&
            std::strcmp(c->line, "ransomware") == 0) {
            CHECK(g.hatchProcessUnlocked(c));   // ungated: in the pool from the start
            if (std::strcmp(c->id, "paypup") == 0) paypup = true;
            else if (std::strcmp(c->id, "conkittenate") == 0) conk = true;
            else if (std::strcmp(c->id, "pingcub") == 0) pingcub = true;
        }
    CHECK(paypup && conk && pingcub);
}

// Canine chain (Ransomware): Paypup (Process) -> Barkmail (Script) -> a Good/Bad
// Daemon branch (Wire Heir | Extorgi), care-gated like Malbear's pair. Paypup routes
// Paypup has no Daemon pool, so every hop here is read straight off the creature row.
void test_canine_line_evolution_branch() {
    { // Good care (0-2): Paypup -> Barkmail -> Wire Heir.
        Game g{StartMode::Hatched, "paypup"};
        uint32_t t = 0;
        g.debugTriggerEvolution(); advanceToReveal(g, t); g.onButton(press(Button::B));
        CHECK(g.pet() && std::strcmp(g.pet()->id, "barkmail") == 0);
        CHECK(g.pet()->stage == Stage::Script);
        CHECK(g.model().careBranch() == CareBranch::Good);
        g.debugTriggerEvolution(); advanceToReveal(g, t); g.onButton(press(Button::B));
        CHECK(g.pet() && std::strcmp(g.pet()->id, "wire_heir") == 0);
        CHECK(g.pet()->stage == Stage::Daemon);
    }
    { // Bad care (3-4): Barkmail -> Extorgi (the glass-cannon branch).
        Game g{StartMode::Hatched, "barkmail"};
        g.model().setCareMistakes(3);
        CHECK(g.model().careBranch() == CareBranch::Bad);
        uint32_t t = 0;
        g.debugTriggerEvolution(); advanceToReveal(g, t); g.onButton(press(Button::B));
        CHECK(g.pet() && std::strcmp(g.pet()->id, "extorgi") == 0);
        CHECK(g.pet()->stage == Stage::Daemon);
    }
}

// Pingcub is the ursine chain's own head: Malbear and its Daemon pair are reached
// only through it, now that Paypup heads the canine chain instead.
void test_pingcub_rejoins_the_bear_line() {
    ContentRegistry r = ContentRegistry::embedded();
    const CreatureDef* pingcub = r.creature("pingcub");
    CHECK(pingcub);
    CHECK(pingcub->stage == Stage::Process);
    CHECK(r.creatureSprite(*pingcub) == &ASSET_SPR_PET_PINGCUB);   // its own cub art
    Game g{StartMode::Hatched, "pingcub"};
    uint32_t t = 0;
    g.debugTriggerEvolution(); advanceToReveal(g, t); g.onButton(press(Button::B));
    CHECK(g.pet() && std::strcmp(g.pet()->id, "malbear") == 0);
    CHECK(g.pet()->stage == Stage::Script);
}

// Cat line (Ransomware): Conkittenate (Process) -> Kalico (Script) -> a Good/Bad
// Daemon branch (Pwnther | Breecheetah), care-gated like Malbear's pair.
void test_cat_line_evolution_branch() {
    { // Good care (0-2): Conkittenate -> Kalico -> Pwnther.
        Game g{StartMode::Hatched, "conkittenate"};
        uint32_t t = 0;
        g.debugTriggerEvolution(); advanceToReveal(g, t); g.onButton(press(Button::B));
        CHECK(g.pet() && std::strcmp(g.pet()->id, "kalico") == 0);
        CHECK(g.pet()->stage == Stage::Script);
        CHECK(g.model().careBranch() == CareBranch::Good);
        g.debugTriggerEvolution(); advanceToReveal(g, t); g.onButton(press(Button::B));
        CHECK(g.pet() && std::strcmp(g.pet()->id, "pwnther") == 0);
        CHECK(g.pet()->stage == Stage::Daemon);
    }
    { // Bad care (3-4): Kalico -> Breecheetah (the glass-cannon branch).
        Game g{StartMode::Hatched, "kalico"};
        g.model().setCareMistakes(3);
        CHECK(g.model().careBranch() == CareBranch::Bad);
        uint32_t t = 0;
        g.debugTriggerEvolution(); advanceToReveal(g, t); g.onButton(press(Button::B));
        CHECK(g.pet() && std::strcmp(g.pet()->id, "breecheetah") == 0);
        CHECK(g.pet()->stage == Stage::Daemon);
    }
}

// Frog line (Phishing) is LINEAR: Tadpoll -> Croaken -> Goliauth, and BOTH care
// branches land on Goliauth (no Good/Bad split).
void test_frog_line_linear() {
    for (int mistakes : {0, 4}) {   // Good and Bad care both -> Goliauth
        Game g{StartMode::Hatched, "tadpoll"};
        g.model().setCareMistakes(mistakes);
        uint32_t t = 0;
        g.debugTriggerEvolution(); advanceToReveal(g, t); g.onButton(press(Button::B));
        CHECK(g.pet() && std::strcmp(g.pet()->id, "croaken") == 0);
        CHECK(g.pet()->stage == Stage::Script);
        g.debugTriggerEvolution(); advanceToReveal(g, t); g.onButton(press(Button::B));
        CHECK(g.pet() && std::strcmp(g.pet()->id, "goliauth") == 0);
        CHECK(g.pet()->stage == Stage::Daemon);
    }
}

// Anglerfish line (Phishing) is the DEEP-DIVE catch: Phishlet only joins the Phrogspawn
// egg's hatch pool once DEEPWEB_DEPTH_64 (the 2nd DeepWeb-depth milestone) is earned;
// Tadpoll is always in the pool. hatchProcessUnlocked is the granular gate the pool loop
// consults, so assert it directly (an rng-driven full hatch would be flaky on the 50/50).
void test_anglerfish_deepdive_hatch_gate() {
    ContentRegistry r = ContentRegistry::embedded();
    const CreatureDef* phishlet = r.creature("phishlet");
    const CreatureDef* tadpoll  = r.creature("tadpoll");
    CHECK(phishlet && tadpoll);
    CHECK(phishlet->stage == Stage::Process);
    CHECK(std::strcmp(phishlet->line, "phishing") == 0);
    CHECK(r.creatureSprite(*phishlet) == &ASSET_SPR_PET_PHISHLET);  // the anglerfish art
    Game g;                                                 // no achievements yet
    CHECK(!g.hasAchievement(ach::kDeepWebDepth64));
    CHECK(!g.hatchProcessUnlocked(phishlet));               // gated out of the pool
    CHECK(g.hatchProcessUnlocked(tadpoll));                 // always in the pool
    g.unlockAchievement(ach::kDeepWebDepth64);
    CHECK(g.hatchProcessUnlocked(phishlet));                // now a 50/50 pool member
}

// Anglerfish line (Phishing): Phishlet (Process) -> ClickBait (Script) -> a Good/Bad
// Daemon branch (Spamwhale | Baitracuda), care-gated like Kalico's pair.
void test_anglerfish_line_evolution_branch() {
    { // Good care (0-2): Phishlet -> ClickBait -> Spamwhale.
        Game g{StartMode::Hatched, "phishlet"};
        g.debugSeedRng(1);   // no-divert seed: Phishlet keeps the Phishing chain
        uint32_t t = 0;
        g.debugTriggerEvolution(); advanceToReveal(g, t); g.onButton(press(Button::B));
        CHECK(g.pet() && std::strcmp(g.pet()->id, "clickbait") == 0);
        CHECK(g.pet()->stage == Stage::Script);
        CHECK(g.model().careBranch() == CareBranch::Good);
        g.debugTriggerEvolution(); advanceToReveal(g, t); g.onButton(press(Button::B));
        CHECK(g.pet() && std::strcmp(g.pet()->id, "spamwhale") == 0);
        CHECK(g.pet()->stage == Stage::Daemon);
    }
    { // Bad care (3-4): ClickBait -> Baitracuda (the glass-cannon branch).
        Game g{StartMode::Hatched, "clickbait"};
        g.model().setCareMistakes(3);
        CHECK(g.model().careBranch() == CareBranch::Bad);
        uint32_t t = 0;
        g.debugTriggerEvolution(); advanceToReveal(g, t); g.onButton(press(Button::B));
        CHECK(g.pet() && std::strcmp(g.pet()->id, "baitracuda") == 0);
        CHECK(g.pet()->stage == Stage::Daemon);
    }
}

// Trojan family — content wiring: pierce attacks (armorPiercePct 100), trap defends,
// the two creatures on line "trojan", and Phishlet's cross-line divert hook.
void test_trojan_content() {
    ContentRegistry r = ContentRegistry::embedded();
    for (const char* id : {"backdoor_breach", "payload_puncture", "rootkit_rupture"}) {
        const MoveDef* m = r.move(id);
        CHECK(m && m->kind == MoveDef::Kind::Attack &&
              std::strcmp(m->line, "trojan") == 0 && m->armorPiercePct == 100);
    }
    for (const char* id : {"logic_bomb", "sandbox_snare", "killswitch"}) {
        const MoveDef* m = r.move(id);
        CHECK(m && m->kind == MoveDef::Kind::Defend && m->trapArm > 0 &&
              m->trapEvasionPct > 0 && m->trapReboundPct > 0 && m->trapArmorRot > 0 &&
              m->trapPassiveBonusPct > 0);
    }
    const CreatureDef* kl = r.creature("keyloggerhead");
    const CreatureDef* dae = r.creature("placeholder_daemon");
    CHECK(kl && kl->stage == Stage::Script && std::strcmp(kl->line, "trojan") == 0);
    CHECK(dae && dae->stage == Stage::Daemon && std::strcmp(dae->line, "trojan") == 0);
    const CreatureDef* ph = r.creature("phishlet");
    CHECK(ph && ph->evolvesToTrojanId &&
          std::strcmp(ph->evolvesToTrojanId, "keyloggerhead") == 0);
}

// The cross-line infiltration divert (the headline mechanic): a Phishlet that rolls the
// divert becomes Keyloggerhead (a Trojan) instead of ClickBait — dropping its Phishing
// moves for the Trojan kit and unlocking the family. Seeded so the roll is deterministic.
void test_trojan_cross_line_divert() {
    {   // divert seed: Phishlet -> Keyloggerhead, loadout re-seeded, family unlocked
        Game g{StartMode::Hatched, "phishlet"};
        CHECK(g.moveLoadout().owns("smish_hook"));            // starts with Phishing moves
        CHECK(!g.hasAchievement(ach::kTrojanUnleashed));
        g.debugSeedRng(13);                                  // rolls < kTrojanDivertPct
        uint32_t t = 0;
        g.debugTriggerEvolution(); advanceToReveal(g, t); g.onButton(press(Button::B));
        CHECK(g.pet() && std::strcmp(g.pet()->id, "keyloggerhead") == 0);
        CHECK(std::strcmp(g.pet()->line, "trojan") == 0);
        CHECK(g.moveLoadout().owns("backdoor_breach"));      // gained the Trojan kit
        CHECK(!g.moveLoadout().owns("smish_hook"));          // dropped the Phishing kit
        CHECK(g.hasAchievement(ach::kTrojanUnleashed));
    }
    {   // no-divert seed: the normal Phishing chain is unaffected
        Game g{StartMode::Hatched, "phishlet"};
        g.debugSeedRng(1);                                   // rolls >= kTrojanDivertPct
        uint32_t t = 0;
        g.debugTriggerEvolution(); advanceToReveal(g, t); g.onButton(press(Button::B));
        CHECK(g.pet() && std::strcmp(g.pet()->id, "clickbait") == 0);
        CHECK(!g.hasAchievement(ach::kTrojanUnleashed));
    }
}

// Trojan combat: (1) a pierce attack ignores a defended enemy's damage cut; (2) an armed
// trap springs on an incoming hit — evasion cuts the damage taken, rebound chips the
// attacker, and the trap is consumed; (3) Execution-Override chance gates off-line and
// scales with armed traps.
void test_trojan_combat() {
    ContentRegistry r = ContentRegistry::embedded();

    // (1) Pierce: backdoor_breach (power 12, pierce 100) vs a 50%-cut enemy lands full 12;
    //     packet_storm (power 12, no pierce) lands 6. Player faster -> acts first.
    {
        Combatant e = mkCombatant(r, "E", 100, 1, {"quick_jab"});
        e.dmgReducePct = 50;
        Combatant pPierce = mkCombatant(r, "P", 100, 20, {"backdoor_breach"});
        Combatant pPlain  = mkCombatant(r, "P", 100, 20, {"packet_storm"});
        Combat a; a.begin(pPierce, e, Combat::Stakes::Safe, 42); a.step();
        Combat b; b.begin(pPlain,  e, Combat::Stakes::Safe, 42); b.step();
        CHECK(a.enemy().health == 100 - 12);      // pierce ignored the 50% cut
        CHECK(b.enemy().health == 100 - 6);        // plain hit was halved
    }

    // (2) Trap trigger. Player NOT on line "trojan" (nullptr) so the Execution-Override
    //     passive can't interfere — the trap mechanic itself is not line-gated. Equal
    //     speed -> strict alternation, player first: step1 arms logic_bomb, step2 the
    //     enemy's packet_storm springs it.
    {
        Combatant p = mkCombatant(r, "P", 100, 10, {"logic_bomb"});
        Combatant e = mkCombatant(r, "E", 100, 10, {"packet_storm"});  // power 12
        Combat cb; cb.begin(p, e, Combat::Stakes::Safe, 42);
        cb.step();                                 // player arms the trap
        CHECK(cb.player().trojanTrapCount == 1);
        cb.step();                                 // enemy hits -> trap springs
        CHECK(cb.player().trojanTrapCount == 0);   // consumed
        CHECK(cb.player().health == 100 - 9);      // 12 - 20% evasion = 9 taken
        CHECK(cb.enemy().health < 100);            // rebound chipped the attacker
    }

    // (3) Execution-Override chance: 0 off-line; base on-line with no traps; base + the
    //     armed trap bonuses with traps held.
    {
        Combatant nonTrojan = mkCombatant(r, "P", 100, 10, {"quick_jab"});
        Combatant trojan = mkCombatant(r, "P", 100, 10, {"quick_jab"});
        trojan.line = "trojan";
        Combat cb; cb.begin(nonTrojan, nonTrojan, Combat::Stakes::Safe, 1);
        CHECK(cb.execOverrideChance(nonTrojan) == 0);
        CHECK(cb.execOverrideChance(trojan) == kExecOverrideBasePct);
        trojan.trojanTraps[trojan.trojanTrapCount++] = r.move("logic_bomb");   // +10
        trojan.trojanTraps[trojan.trojanTrapCount++] = r.move("killswitch");   // +20
        CHECK(cb.execOverrideChance(trojan) ==
              kExecOverrideBasePct + r.move("logic_bomb")->trapPassiveBonusPct +
                  r.move("killswitch")->trapPassiveBonusPct);
    }
}

// --- Clutch Pick (the Phishing hatch minigame, game_eggpick.cpp) -----------

// Lay a Phishing egg, which drops straight into the Clutch Pick.
static Game phishingEgg() {
    Game g;
    g.unlockAchievement(ach::kDeepWebDepth8);   // unlocks the Phishing line
    g.resetToHatch();
    g.onButton(press(Button::A));               // cycle to Phishing
    g.onButton(press(Button::B));               // lay it
    return g;
}

// Aim at whichever half actually holds the live egg, every round. Mirrors the cut
// game_eggpick.cpp documents — alternating columns/rows over the 8x4 clutch — so it
// also pins that order: a change to the split axis or count fails here.
static void eggPickPlayPerfect(Game& g) {
    const int col = g.eggPickTargetSlot() % Game::kEggPickCols;
    const int row = g.eggPickTargetSlot() / Game::kEggPickCols;
    int c0 = 0, cw = Game::kEggPickCols, r0 = 0, rh = Game::kEggPickRows;
    for (int i = 0; i < Game::kEggPickRounds; ++i) {
        bool second;
        if (i % 2 == 0) { cw /= 2; second = col >= c0 + cw; if (second) c0 += cw; }
        else            { rh /= 2; second = row >= r0 + rh; if (second) r0 += rh; }
        g.onButton(press(second ? Button::C : Button::A));   // aim
        g.onButton(press(Button::B));                        // commit the half
    }
}

// A clean run halves what's left of the incubation clock; the reveal waits for B.
void test_eggpick_win_halves_incubation() {
    Game g = phishingEgg();
    CHECK(g.inEggPick());
    CHECK(g.eggPickRound() == 0 && !g.eggPickResolved());
    CHECK(g.bootHatchRemainMs() == kBootHatchMs);
    eggPickPlayPerfect(g);
    CHECK(g.eggPickRound() == Game::kEggPickRounds);
    CHECK(g.eggPickResolved() && g.eggPickWon());
    CHECK(g.bootHatchRemainMs() == kBootHatchMs);   // banked on the reveal's B, not before
    g.onButton(press(Button::B));
    CHECK(g.nav() == Game::Nav::Idle);
    CHECK(g.bootHatchRemainMs() == kBootHatchMs / 2);
    CHECK(g.inEggPhase());                          // still an egg, just a faster one
}

// Picking the wrong half loses the egg for good — the remaining rounds still play out
// (the player isn't told mid-run), and the incubation clock is left at full length.
void test_eggpick_miss_keeps_full_incubation() {
    Game g = phishingEgg();
    const int col = g.eggPickTargetSlot() % Game::kEggPickCols;
    const bool correct = col >= Game::kEggPickCols / 2;
    g.onButton(press(correct ? Button::A : Button::C));   // aim at the half it ISN'T in
    g.onButton(press(Button::B));
    CHECK(!g.eggPickTargetInSpan());                      // already lost
    CHECK(!g.eggPickResolved());                          // but the run keeps going
    for (int i = 1; i < Game::kEggPickRounds; ++i) {
        g.onButton(press(Button::A));
        g.onButton(press(Button::B));
    }
    CHECK(g.eggPickResolved() && !g.eggPickWon());
    g.onButton(press(Button::B));
    CHECK(g.nav() == Game::Nav::Idle);
    CHECK(g.bootHatchRemainMs() == kBootHatchMs);         // no bonus, no penalty
}

// A and C AIM here rather than step/cancel: C can't back out of the modal, and
// re-aiming before B is free.
void test_eggpick_aim_is_not_cancel() {
    Game g = phishingEgg();
    g.onButton(press(Button::C));
    CHECK(g.inEggPick() && g.eggPickRound() == 0);   // C aimed, it didn't exit or commit
    g.onButton(press(Button::A));
    CHECK(g.inEggPick() && g.eggPickRound() == 0);
    g.onButton(press(Button::B));
    CHECK(g.eggPickRound() == 1);                    // only B advances a round
}

// The live egg never hides in a wrapped cell — the odd-row shift puts the last cell of
// those rows half off each edge of the clutch, which has no honest left/right answer.
void test_eggpick_target_never_wraps() {
    for (uint32_t seed = 1; seed <= 64; ++seed) {
        Game g;
        g.unlockAchievement(ach::kDeepWebDepth8);
        g.debugSeedRng(seed);
        g.resetToHatch();
        g.onButton(press(Button::A));
        g.onButton(press(Button::B));
        const int slot = g.eggPickTargetSlot();
        const int col = slot % Game::kEggPickCols, row = slot / Game::kEggPickCols;
        CHECK(!(row % 2 == 1 && col == Game::kEggPickCols - 1));
    }
}

// Take a laid Phishing egg through its Clutch Pick and back out to idle.
static void settleEggPick(Game& g) {
    for (int i = 0; i < Game::kEggPickRounds; ++i) {
        g.onButton(press(Button::A));
        g.onButton(press(Button::B));
    }
    g.onButton(press(Button::B));                 // dismiss the reveal -> idle
}

// A settled Phishing egg has nothing left to play: its hatch game was the Clutch Pick,
// already spent at lay-time, and the shell is not crackable until the home stretch. So
// idle's B does nothing at all through the middle of the clock.
void test_eggpick_line_has_nothing_mid_clock() {
    Game g = phishingEgg();
    settleEggPick(g);
    uint32_t t = 0;
    g.tick(t += 1000);
    g.tick(t += kBootHatchMs / 2);                // halfway down the clock
    CHECK(!g.eggCrackable());                     // nothing is advertised out here
    g.onButton(press(Button::B));
    CHECK(g.nav() == Game::Nav::Idle);
    CHECK(g.inEggPhase());                        // still incubating
}

// The home stretch of ANY incubation is crackable on demand: in the last kHatchRevealMs
// the Exploit chord plays the shell's full hatch one-shot and hatches off the end, so a
// line with no decrypt still gets to SHOW its animation. Non-interactive throughout.
void test_hatch_reveal_plays_the_animation() {
    Game g = phishingEgg();
    settleEggPick(g);
    uint32_t t = 0;
    g.tick(t += 1000);
    g.tick(t += kBootHatchMs / 2);
    CHECK(!g.eggCrackable());                     // still short of the reveal window
    g.onButton(chordAC());
    CHECK(g.nav() == Game::Nav::Idle);            // the chord is inert out here
    // Run the clock into the last kHatchRevealMs, stopping short of the auto-hatch.
    g.tick(t += g.bootHatchRemainMs() - kHatchRevealMs / 2);
    CHECK(g.hatchRevealReady() && g.eggCrackable());
    g.onButton(chordAC());
    CHECK(g.nav() == Game::Nav::ModalHatchReveal);
    CHECK(g.hatchRevealFrame() == 0);             // opens on the shell's first frame

    // A frame per heartbeat, and every button inert while it runs.
    g.tick(t += kHeartbeatMs);
    CHECK(g.hatchRevealFrame() == 1);
    g.onButton(press(Button::B));
    g.onButton(press(Button::C));
    CHECK(g.nav() == Game::Nav::ModalHatchReveal);   // nothing skips it
    const int frames = 8;                            // SPR_PET_EGG_PHISH_HATCH
    for (int i = 0; i < frames + kHatchRevealHoldBeats; ++i) g.tick(t += kHeartbeatMs);
    CHECK(g.hatchRevealFrame() == frames - 1);       // held the last frame, didn't run off
    CHECK(g.nav() == Game::Nav::Idle);               // ...then hatched itself
    CHECK(g.pet() && g.pet()->stage == Stage::Process);
    CHECK(!g.inEggPhase());
}

// The reveal is every line's now, the Ransomware one included: its DISK DECYPHER board
// was spent at lay-time like everyone else's, so the home stretch belongs to the chord.
void test_hatch_reveal_covers_the_decypher_line() {
    Game g;
    pickFirstEggLine(g);
    uint32_t t = 0;
    g.tick(t += 1000);
    g.tick(t += kBootHatchMs - kHatchRevealMs / 2);   // deep in the reveal window
    CHECK(g.hatchRevealReady() && g.eggCrackable());
    g.onButton(chordAC());
    CHECK(g.nav() == Game::Nav::ModalHatchReveal);
}

// Grayscale gate. The aim rides on SHAPE first — a bar down the aimed half's outer
// edge, which moves to the other edge when the aim flips — backed by a luminance step
// between the aimed, still-in-play, and eliminated cells. None of it is carried by hue.
void test_eggpick_grayscale() {
    Framebuffer fb(kActiveW, kActiveH);
    auto meanLum = [&](int x0, int x1, int y0, int y1) {
        float sum = 0.0f;
        for (int y = y0; y < y1; ++y)
            for (int x = x0; x < x1; ++x) sum += luminance(fb.get(x, y));
        return sum / static_cast<float>((x1 - x0) * (y1 - y0));
    };
    // The panel is centred and drawn at x2: 224x112 spanning y 56..168.
    const int top = 62, bot = 160;

    {   // Round 1 cuts columns and opens aimed LEFT.
        Game g = phishingEgg();
        g.render(fb);
        CHECK(hasDarkInk(fb, 0, 0, kActiveW, 40));          // title + round counter
        CHECK(meanLum(0, 4, top, bot) > meanLum(kActiveW - 4, kActiveW, top, bot));
        CHECK(meanLum(8, 104, top, bot) > meanLum(120, 216, top, bot));
    }
    {   // C flips the aim: the bar and the bright half both move to the right.
        Game g = phishingEgg();
        g.onButton(press(Button::C));
        g.render(fb);
        CHECK(meanLum(kActiveW - 4, kActiveW, top, bot) > meanLum(0, 4, top, bot));
        CHECK(meanLum(120, 216, top, bot) > meanLum(8, 104, top, bot));
    }
    {   // After committing left, round 2 cuts rows: aimed top > in-play bottom-left >
        // the eliminated right, three ordered steps with no colour involved.
        Game g = phishingEgg();
        g.onButton(press(Button::B));
        g.render(fb);
        const float aimed = meanLum(8, 104, top, 110);
        const float inPlay = meanLum(8, 104, 118, bot);
        const float dead = meanLum(120, 216, top, bot);
        CHECK(aimed > inPlay && inPlay > dead);
    }
}

// The hatch line-select: the Phishing line is now gated behind the first
// DeepWeb-depth milestone (DEEPWEB_DEPTH_8), so a fresh save offers only Ransomware
// and line-select auto-skips. Once depth 8 is earned, a fresh hatch presents
// the choice; A cycles, B lays the chosen line's egg. Choosing Phishing lays
// Phrogspawn -> hatches to Tadpoll (Phishing).
void test_line_select_phishing_egg() {
    Game g;                                     // FreshHatch: only Ransomware unlocked
    CHECK(!g.inLineSelect());                   // one line -> auto-skip, egg laid
    CHECK(g.pet() && std::strcmp(g.pet()->line, "ransomware") == 0);
    g.unlockAchievement(ach::kDeepWebDepth8);   // unlocks Phishing
    g.resetToHatch();                           // re-hatch: now two lines -> the modal
    CHECK(g.inLineSelect());
    CHECK(g.pet() == nullptr);                  // empty save while choosing
    CHECK(g.lineSelectRow() == 0);              // Ransomware first
    g.onButton(press(Button::C));               // C disabled -> still choosing
    CHECK(g.inLineSelect());
    g.onButton(press(Button::A));               // cycle to Phishing (row 1)
    CHECK(g.lineSelectRow() == 1);
    g.onButton(press(Button::B));               // select -> lay the Phrogspawn egg
    CHECK(g.pet() && std::strcmp(g.pet()->id, "phrogspawn") == 0);
    CHECK(g.inEggPhase() && std::strcmp(g.pet()->line, "phishing") == 0);
    CHECK(g.inEggPick());                       // the Phishing line opens its Clutch Pick
    eggPickPlayPerfect(g);
    g.onButton(press(Button::B));               // dismiss the reveal -> the egg at idle
    CHECK(g.nav() == Game::Nav::Idle);
    uint32_t t = 0; g.tick(t += 1000); g.tick(t += kBootHatchMs + kHeartbeatMs);
    CHECK(g.pet() && g.pet()->stage == Stage::Process);       // hatched
    CHECK(std::strcmp(g.pet()->id, "tadpoll") == 0);          // only unlocked phishing Process (Phishlet gates on depth-64)
}

// Line grayscale gate: the line-select modal reads without colour (title ink +
// the ">" cursor glyph marking the highlighted line).
void test_line_select_grayscale() {
    Game g;
    g.unlockAchievement(ach::kDeepWebDepth8);   // unlock Phishing -> a real choice
    g.resetToHatch();                                          // re-hatch into the modal
    CHECK(g.inLineSelect());
    Framebuffer fb(kActiveW, kActiveH);
    g.render(fb);
    CHECK(hasDarkInk(fb, 0, 40, kActiveW, 120));   // title / rows carry ink
}

// Frog line-move access (moveAllowedForLine): a Phishing pet can learn a phishing
// move (spoof_bubble); a Ransomware pet cannot; a generic move is open to both.
void test_frog_line_move_access() {
    ContentRegistry r = ContentRegistry::embedded();
    const CreatureDef* frog = r.creature("croaken");      // phishing Script
    const CreatureDef* cat = r.creature("kalico");        // ransomware Script
    const MoveDef* phish = r.move("spoof_bubble");        // phishing line move
    const MoveDef* generic = r.move("packet_storm");      // generic move
    CHECK(frog && cat && phish && generic);
    CHECK(moveAllowedForLine(*phish, frog->line));        // frog: yes
    CHECK(!moveAllowedForLine(*phish, cat->line));        // ransomware: no
    CHECK(moveAllowedForLine(*generic, frog->line));      // generic: anyone
    CHECK(moveAllowedForLine(*generic, cat->line));
}

// An update is offered on a strict > against the packed version code, so the
// packing has to keep MAJOR.MINOR.PATCH ordered across every rollover, and a
// code that failed to parse must never look newer than what's running.
void test_firmware_version_ordering() {
    CHECK(!firmwareUpdateAvailable(kFirmwareVersionCode));      // the same build
    CHECK(!firmwareUpdateAvailable(kFirmwareVersionCode - 1));  // an older one
    CHECK(firmwareUpdateAvailable(kFirmwareVersionCode + 1));   // a newer one

    // A missing/unparseable manifest version lands here as 0. Offering an
    // update on that would flash an arbitrary image over a working device.
    CHECK(!firmwareUpdateAvailable(0));

    // Fields don't overlap: a maxed patch never outranks a minor bump, and a
    // maxed minor never outranks a major one.
    constexpr uint32_t kV0_1_0  = 0 * 10000u + 1 * 100u + 0;
    constexpr uint32_t kV0_1_99 = 0 * 10000u + 1 * 100u + 99;
    constexpr uint32_t kV0_99_0 = 0 * 10000u + 99 * 100u + 0;
    constexpr uint32_t kV1_0_0  = 1 * 10000u + 0 * 100u + 0;
    CHECK(kV0_1_0 < kV0_1_99 && kV0_1_99 < kV0_99_0 && kV0_99_0 < kV1_0_0);
}
