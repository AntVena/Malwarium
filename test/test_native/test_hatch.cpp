// test_hatch.cpp — native gates for the egg's hatch, its menu gates and the evolution
// boundary. DISK DECYPHER's own rules live in test_decypher.cpp.
//
// One slice of the native suite; see test_gates.h for the shared fixtures and
// test_main.cpp for the roster that runs these. A save-vNN gate sits with the
// feature whose field it migrates, not in a migrations pile of its own.
#include "test_gates.h"

// Fresh save LAYS THE EGG at idle (not a blocking modal): the Boot-Sector creature
// is on-screen and interactable, incubating over kBootHatchMs. Line-select
// auto-skips with one line unlocked. The egg counts as generation 1 at once.
void test_hatch_lays_egg_at_idle() {
    Game g;                                   // default = FreshHatch
    pickFirstEggLine(g);                      // >1 line unlocked -> pick Ransomware
    CHECK(g.nav() == Game::Nav::Idle);        // egg sits at idle, no modal
    CHECK(g.pet() != nullptr);
    CHECK(std::strcmp(g.pet()->id, "cryptoshell") == 0);
    CHECK(g.pet()->stage == Stage::BootSector);
    CHECK(g.inEggPhase());
    CHECK(g.bootHatchRemainMs() == kBootHatchMs);
    CHECK(!g.eggCrackable());                 // nowhere near the reveal window yet
    CHECK(g.generation() == 1);               // the egg is laid = generation 1
}

// The Ransomware egg opens straight onto its DISK DECYPHER board — the same lay-time
// deal every other line already had — and a board that runs out of attempts costs the
// egg nothing but the bonus.
void test_hatch_opens_the_decypher_board() {
    Game g;
    if (g.inLineSelect()) g.onButton(press(Button::B));   // lay the Ransomware egg
    CHECK(g.nav() == Game::Nav::Decypher);
    CHECK(g.inEggPhase());
    CHECK(g.bootHatchRemainMs() == kBootHatchMs);

    settleDecypher(g);
    CHECK(g.nav() == Game::Nav::Idle);
    CHECK(g.inEggPhase());                    // still an egg, on its full clock
    CHECK(g.bootHatchRemainMs() == kBootHatchMs);
    CHECK(!g.hasAchievement(ach::kFirstBruteForce));   // a key that was never broken
}

// Waiting out the full incubation (never opening the minigame) auto-hatches on its
// own — straight to Process, no soft-lock.
void test_hatch_waits_out() {
    Game g;
    pickFirstEggLine(g);
    uint32_t t = 0;
    g.tick(t += 1000);
    g.tick(t += kBootHatchMs + kHeartbeatMs);
    CHECK(g.nav() == Game::Nav::Idle);
    CHECK(g.pet() != nullptr && g.pet()->stage == Stage::Process);
    CHECK(!g.inEggPhase());
}

// A newly-seen NETWORK shaves kBootHatchNetworkAccelMs off the incubation clock.
// An egg can't explore, so the network seam is the only hatch accelerator.
void test_hatch_network_accelerates() {
    Game g;
    pickFirstEggLine(g);
    uint32_t t = 0;
    g.tick(t += 1000);
    CHECK(g.inEggPhase());
    const uint32_t before = g.bootHatchRemainMs();
    const uint8_t bssid[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01};
    CHECK(g.registerNetwork(bssid, "TestNet"));   // freshly queued -> accelerates
    CHECK(g.bootHatchRemainMs() == before - kBootHatchNetworkAccelMs);
}

// Vitals are frozen while the pet is still an egg (an egg doesn't get hungry): a
// long idle stretch doesn't move Hunger, but resumes decaying once hatched.
void test_hatch_egg_vitals_frozen() {
    Game g;
    pickFirstEggLine(g);
    uint32_t t = 0;
    g.tick(t += 1000);
    const int h0 = g.model().hunger();
    g.tick(t += kBootHatchMs / 4);            // well within the first half
    CHECK(g.inEggPhase());
    CHECK(g.model().hunger() == h0);          // frozen — no decay while an egg
}

// Grayscale gate: the egg-at-idle reads without colour (egg silhouette + the
// incubation prompt ink).
void test_hatch_grayscale() {
    Game g;
    pickFirstEggLine(g);
    g.tick(1000);
    Framebuffer fb(kActiveW, kActiveH);
    g.render(fb);
    CHECK(anyNonPaper(fb, 60, 40, kActiveW - 60, 184));  // egg silhouette present
    CHECK(hasDarkInk(fb, 0, kLivingTop, kActiveW, kLivingTop + 16));  // prompt ink
}

// The Hatched seam (used by the tests) skips the egg entirely.
void test_hatch_seam_skips() {
    Game g{StartMode::Hatched};
    CHECK(g.nav() == Game::Nav::Idle);
    CHECK(g.pet() != nullptr);
    CHECK(!g.inDecypher());
    CHECK(!g.inEggPhase());                   // a seam pet is already hatched
}

// Move the carousel cursor onto the slot routing to `id` WITHOUT entering it
// (unlike enterSubmenuId, which presses B). Used to probe locked slots.
static void cursorToSlot(Game& g, SubmenuId id) {
    g.onButton(press(Button::A));                 // idle A -> carousel
    while (carouselSlots()[g.cursor()].id != id)
        g.onButton(press(Button::A));
}

// Egg-phase menu lock: GAMES/MAINT/MODS/EXPL are inert while the
// pet is an egg (B on them does not open the submenu) — an egg can't explore. Only
// STAT/ITEMS stay reachable. Once hatched, every slot enters normally.
void test_egg_menu_locks() {
    Game g;                                       // FreshHatch -> egg at idle
    pickFirstEggLine(g);
    CHECK(g.inEggPhase());
    for (SubmenuId locked : {SubmenuId::Games, SubmenuId::Maint, SubmenuId::Mods,
                             SubmenuId::Expl}) {
        cursorToSlot(g, locked);
        CHECK(g.nav() == Game::Nav::Cursor);
        g.onButton(press(Button::B));             // inert on a locked slot
        CHECK(g.nav() == Game::Nav::Cursor);      // stayed on the carousel
        g.onButton(press(Button::C));             // back to idle for the next probe
    }
    // STAT / ITEMS still enter.
    for (SubmenuId open : {SubmenuId::Stat, SubmenuId::Items}) {
        cursorToSlot(g, open);
        g.onButton(press(Button::B));
        CHECK(g.nav() == Game::Nav::Submenu);
        g.onButton(press(Button::C));
    }
    // Hatched: the egg-locked slots are open.
    Game h{StartMode::Hatched};
    CHECK(!h.inEggPhase());
    cursorToSlot(h, SubmenuId::Mods);
    h.onButton(press(Button::B));
    CHECK(h.nav() == Game::Nav::Submenu);
}

// Egg-phase items: only quest items are usable — a Food item's Use is inert (no
// feeding, count unchanged) while an egg, but works once hatched.
void test_egg_items_quest_only() {
    { Game g;                                     // egg
      pickFirstEggLine(g);
      enterSubmenuId(g, SubmenuId::Items);        // ITEMS is not locked
      g.onButton(press(Button::B));               // first selectable row = airgap_snack (FOOD)
      CHECK(g.nav() == Game::Nav::Detail);
      const int n0 = g.inventory().count("airgap_snack");
      g.onButton(press(Button::B));               // Use -> gated on an egg
      CHECK(g.nav() == Game::Nav::Detail);        // no feeding modal
      CHECK(g.inventory().count("airgap_snack") == n0); }
    { Game h{StartMode::Hatched};                 // hatched -> the same food feeds
      enterSubmenuId(h, SubmenuId::Items);
      h.onButton(press(Button::B));
      const int n0 = h.inventory().count("airgap_snack");
      h.onButton(press(Button::B));               // Use -> feeds
      CHECK(h.nav() == Game::Nav::ModalFeeding);
      CHECK(h.inventory().count("airgap_snack") == n0 - 1); }
}

// The A+C Exploit chord cracks the shell — but only in the home stretch of the clock
// (the exploit symbol's window). Inert before that, on every line.
void test_egg_exploit_chord_hatches() {
    Game g;
    pickFirstEggLine(g);
    uint32_t t = 0;
    g.tick(t += 1000);
    CHECK(!g.eggCrackable());
    g.onButton(chordAC());                        // too early: the chord is inert
    CHECK(g.nav() == Game::Nav::Idle);
    g.tick(t += g.bootHatchRemainMs() - kHatchRevealMs / 2);
    CHECK(g.eggCrackable());
    // Mirror hardware: A summons the cursor, then C completes the chord.
    g.onButton(press(Button::A));
    g.onButton(chordAC());
    CHECK(g.nav() == Game::Nav::ModalHatchReveal);
}

// An egg can't arm explore-mode: EXPL is greyed/locked, so B on the EXPL
// slot is inert and never starts a run.
void test_egg_cannot_explore() {
    Game g;
    pickFirstEggLine(g);
    uint32_t t = 0;
    g.tick(t += 1000);
    CHECK(g.inEggPhase());
    cursorToSlot(g, SubmenuId::Expl);
    CHECK(g.nav() == Game::Nav::Cursor);
    g.onButton(press(Button::B));                 // inert on a locked slot
    CHECK(g.nav() == Game::Nav::Cursor);
    CHECK(!g.exploreActive());
}

// STAT time-to-next-evolution readout (all stages): a mid-chain pet counts down
// from its stage's dwell (kEvolveProcessToScriptMs for a Process pet); an egg
// reports its incubation clock; a Daemon terminus has no successor
// (hasNextEvolution() false, remaining 0 -> STAT shows MAX).
void test_evolve_remaining_readout() {
    Game g{StartMode::Hatched, "paypup"};             // Process -> Script successor
    CHECK(g.hasNextEvolution());
    g.tick(1000);
    CHECK(g.evolveRemainMs() == kEvolveProcessToScriptMs - 1000);
    g.tick(61000);
    CHECK(g.evolveRemainMs() == kEvolveProcessToScriptMs - 61000);

    Game e;                                           // egg: readout = incubation clock
    pickFirstEggLine(e);
    e.tick(1000);
    CHECK(e.hasNextEvolution());
    CHECK(e.evolveRemainMs() == e.bootHatchRemainMs());

    Game d{StartMode::Hatched, "bruinforce"};   // Daemon terminus: no successor
    CHECK(!d.hasNextEvolution());
    CHECK(d.evolveRemainMs() == 0);
}

// --- Task 1: dev "reset to egg" shortcut -----------------------------------
// resetToHatch() wipes a raised pet back to a fresh empty save + a laid egg at
// idle, and a real first boot can be replayed from there (incubate -> Process).
void test_reset_to_hatch() {
    Game g{StartMode::Hatched};                 // raised Paypup
    CHECK(g.nav() == Game::Nav::Idle && g.pet() != nullptr);
    g.model().setHunger(5);
    g.inventory().remove("airgap_snack", 1);    // perturb state to prove the wipe
    g.resetToHatch();
    pickFirstEggLine(g);                         // reset re-enters line-select; pick Ransomware
    CHECK(g.nav() == Game::Nav::Idle);          // egg laid at idle
    CHECK(g.pet() != nullptr && std::strcmp(g.pet()->id, "cryptoshell") == 0);
    CHECK(g.inEggPhase());
    CHECK(!g.lockoutActive());
    CHECK(g.bits() == kStartBits);
    CHECK(g.log().size() == 0);
    // Replays a real first boot from the wipe: wait out incubation -> Process idle.
    uint32_t t = 0;
    g.tick(t += 1000);
    g.tick(t += kBootHatchMs + kHeartbeatMs);
    CHECK(g.nav() == Game::Nav::Idle);
    CHECK(g.pet() != nullptr && g.pet()->stage == Stage::Process);   // random Process
    CHECK(g.model().hunger() == kStartHunger);
}

// Boot -> Process: debug trigger fires the modal; B (only once revealed)
// commits the swap to the Process successor and advances the stage indicator.
void test_evolution_boot_to_process() {
    Game g{StartMode::Hatched, "cryptoshell"};  // Boot-Sector start
    CHECK(g.pet()->stage == Stage::BootSector);
    g.debugTriggerEvolution();
    CHECK(g.nav() == Game::Nav::ModalEvolve);
    CHECK(g.pet()->stage == Stage::BootSector);  // old sprite held through cinematic
    uint32_t t = 0;
    advanceToReveal(g, t);
    g.onButton(press(Button::B));
    CHECK(g.nav() == Game::Nav::Idle);
    CHECK(g.pet() != nullptr && g.pet()->stage == Stage::Process);
    CHECK(std::strcmp(g.pet()->id, "paypup") == 0);  // documented stub target
    CHECK(g.log().size() == 0);                       // not logged in v1
}

// C is disabled (not "back"); B before the reveal is inert (the cinematic gate).
void test_evolution_c_disabled_b_gated() {
    Game g{StartMode::Hatched, "cryptoshell"};
    g.debugTriggerEvolution();
    g.onButton(press(Button::C));                // disabled -> still in the modal
    CHECK(g.nav() == Game::Nav::ModalEvolve);
    g.onButton(press(Button::B));                // pre-reveal -> ignored
    CHECK(g.nav() == Game::Nav::ModalEvolve);
    uint32_t t = 0;
    advanceToReveal(g, t);
    g.onButton(press(Button::B));                // revealed -> continues
    CHECK(g.nav() == Game::Nav::Idle);
}

// Natural trigger: time-in-stage gate fires from rest; a Process pet (no
// successor) and a Dying care budget (5/5 -> Critical, not evolution) do not.
void test_evolution_triggers_and_gates() {
    Game g{StartMode::Hatched, "cryptoshell"};
    g.tick(1000);
    CHECK(g.nav() == Game::Nav::Idle);                       // not yet (in-stage)
    g.tick(1000 + kEvolveProcessToScriptMs);
    CHECK(g.nav() == Game::Nav::ModalEvolve);                // gate met -> fires

    Game term{StartMode::Hatched, "bruinforce"};       // Daemon terminus, no successor
    term.tick(1000 + kEvolveProcessToScriptMs);
    CHECK(term.nav() == Game::Nav::Idle);

    Game dying{StartMode::Hatched, "cryptoshell"};
    dying.model().setCareMistakes(kCareDying);               // 5/5 -> Critical path
    dying.tick(1000 + kEvolveProcessToScriptMs);
    CHECK(dying.nav() == Game::Nav::Idle);
}

// Script's dwell is longer than Process's: a Script-stage pet must NOT evolve
// at the shorter Process dwell, only at its own (longer) one.
void test_evolution_script_dwell_longer_than_process() {
    Game g{StartMode::Hatched, "malbear"};                   // Script -> Daemon successor
    CHECK(g.hasNextEvolution());
    g.tick(1000 + kEvolveProcessToScriptMs);
    CHECK(g.nav() == Game::Nav::Idle);                       // too soon for Script's dwell
    g.tick(1000 + kEvolveScriptToDaemonMs);
    CHECK(g.nav() == Game::Nav::ModalEvolve);                // Script's own gate met -> fires
}

// The full chain evolves through all four stages: CryptoShell (Boot) -> Paypup
// (Process) -> Barkmail (Script) -> Wire Heir (Daemon). With default (Good) care
// the Script->Daemon branch resolves to the Good successor. Each hop fires the
// modal and commits on B; the Daemon terminus has no further evolution.
void test_evolution_full_chain() {
    Game g{StartMode::Hatched, "cryptoshell"};
    const char* chain[] = {"paypup", "barkmail", "wire_heir"};
    const Stage stages[] = {Stage::Process, Stage::Script, Stage::Daemon};
    uint32_t t = 0;
    for (int hop = 0; hop < 3; ++hop) {
        g.debugTriggerEvolution();
        CHECK(g.nav() == Game::Nav::ModalEvolve);
        advanceToReveal(g, t);
        g.onButton(press(Button::B));
        CHECK(g.nav() == Game::Nav::Idle);
        CHECK(g.pet() && std::strcmp(g.pet()->id, chain[hop]) == 0);
        CHECK(g.pet()->stage == stages[hop]);
    }
    // Bruinforce is the terminus: no successor, so the trigger never fires.
    g.debugTriggerEvolution();
    CHECK(g.nav() == Game::Nav::Idle);
    g.tick(t += 1000 + kEvolveProcessToScriptMs);
    CHECK(g.nav() == Game::Nav::Idle);
}

// Grayscale gate: the reveal reads without colour (name ink + new sprite
// silhouette + the advanced stage-indicator node).
void test_evolution_grayscale() {
    Game g{StartMode::Hatched, "cryptoshell"};
    g.debugTriggerEvolution();
    uint32_t t = 0;
    advanceToReveal(g, t);
    Framebuffer fb(kActiveW, kActiveH);
    g.render(fb);
    CHECK(anyNonPaper(fb, 0, 60, kActiveW, 120));      // new sprite silhouette
    bool nameInk = false;
    for (int y = 124; y < 136 && !nameInk; ++y)
        for (int x = 0; x < kActiveW; ++x)
            if (luminance(fb.get(x, y)) < 0.4f) { nameInk = true; break; }
    CHECK(nameInk);                                     // name text survives desaturation
    CHECK(anyNonPaper(fb, 0, 144, kActiveW, 158));      // stage indicator present
}
