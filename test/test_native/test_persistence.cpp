// test_persistence.cpp — native gates for the save format, its migrations and ARCH.
//
// One slice of the native suite; see test_gates.h for the shared fixtures and
// test_main.cpp for the roster that runs these. A save-vNN gate sits with the
// feature whose field it migrates, not in a migrations pile of its own.
#include "test_gates.h"

// The LOADOUT hub's back-out chain, and EXPL's nested list.
void test_loadout_expl_nav() {
    Framebuffer fb(kActiveW, kActiveH);
    // MODS opens on the hub; MOVES is one row in, and its move picker one more. C
    // walks all three back out in order — picker -> slot list -> hub -> carousel —
    // so the hub costs exactly one press and never swallows the one that leaves.
    { Game g{StartMode::Hatched}; enterLoadoutTab(g, 1);
      CHECK(g.nav() == Game::Nav::Submenu);
      g.render(fb); CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));
      g.onButton(press(Button::B));                  // slot 1 -> move picker
      CHECK(g.nav() == Game::Nav::Detail);
      g.render(fb); CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));
      tapC(g);                  // back to the slot list
      CHECK(g.nav() == Game::Nav::Submenu);
      tapC(g);                  // back to the hub — still L2
      CHECK(g.nav() == Game::Nav::Submenu);
      g.render(fb); CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));
      tapC(g);
      CHECK(g.nav() == Game::Nav::Cursor); }

    // PRACTISE has no list of its own: the hub row opens straight into the tier pick
    // (L3), and C from there returns to the hub rather than to an empty level.
    { Game g{StartMode::Hatched}; enterLoadoutTab(g, 2);
      CHECK(g.nav() == Game::Nav::Detail);
      g.render(fb); CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));
      tapC(g);
      CHECK(g.nav() == Game::Nav::Submenu);
      tapC(g);
      CHECK(g.nav() == Game::Nav::Cursor); }

    // EXPL: the NESTED area/sub-area list. Areas are LINEAR complete-to-
    // advance; every sub-area of an OPEN area is reachable (explore any, one at a time).
    // Two-level nav: entering parks on the area-0 header (TOP level); B
    // DRILLS into the area, then B arms its first open sub-area 1 → the IDLE habitat (09
    // ). Re-open and A cycles to sub-area 2. Area 1's subs stay LOCKED until area 0
    // clears (its header still isn't enterable-to-arm — a locked area is inert).
    { Game g{StartMode::Hatched}; enterSubmenuId(g, SubmenuId::Expl);
      CHECK(g.nav() == Game::Nav::Submenu);
      g.render(fb); CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));
      g.onButton(press(Button::B));                  // drill into area 0
      g.onButton(press(Button::B));                  // arm sub-area 1 (index 0)
      CHECK(g.nav() == Game::Nav::Idle);
      CHECK(g.exploreActive() && g.exploreSector() == 0 && g.exploreSub() == 0);
      // Re-open with explore-mode RUNNING: EXPL resumes inside area 0 with the cursor
      // already on the armed sub-area (no drill press), so A advances straight to
      // sub-area 2 and B arms it.
      enterSubmenuId(g, SubmenuId::Expl);
      g.onButton(press(Button::A));                  // sub-area 1 -> sub-area 2
      g.onButton(press(Button::B));                  // arm explore on sub-area 2
      CHECK(g.nav() == Game::Nav::Idle && g.exploreSub() == 1);
      bool fl[kExplSectors] = {g.sectorCleared(0), g.sectorCleared(1)};
      CHECK(!explSectorOpen(1, fl)); }               // area 1 locked until area 0 clears
}

// ===========================================================================
// Persistence
// ===========================================================================

// The save blob round-trips: every field survives serialize -> deserialize.
void test_save_roundtrip() {
    SaveData a;
    std::strcpy(a.activeId, "paypup");
    a.hunger = 42; a.frag = 13; a.happy = 88; a.mistakes = 3;
    a.debuffs = 2; a.ghost = 1; a.timeInStageMs = 123456; a.generation = 4;
    a.bits = 777; std::strcpy(a.hackerTag, "L33T_HAXX0R");
    a.lifetimeUptimeMs = 99999; a.lifetimeSteps = 12; a.petsRaised = 4;
    a.items.push_back(SaveStack{"dyno_nuggets", 3});
    a.items.push_back(SaveStack{"yubi_cookie", 1});
    a.ownedMods.push_back(SaveId{"firewall_patch"});
    a.equipped.push_back(SaveId{"firewall_patch"});
    a.equipped.push_back(SaveId{""});
    SaveLogEntry le; le.type = static_cast<uint8_t>(LogEventType::ItemUsed);
    std::strcpy(le.text, "USED YUBI-COOKIE"); a.log.push_back(le);
    SaveStoredPet sp; std::strcpy(sp.id, "cryptoshell");
    sp.hunger = 60; sp.generation = 2; sp.defragCount = 9;  // v16: rack pet's tally
    a.rack.push_back(sp);
    a.ownedMoves.push_back(SaveId{"packet_storm"});
    a.ownedMoves.push_back(SaveId{"fork_bomb"});
    a.equippedMoves.push_back(SaveId{"packet_storm"});
    a.equippedMoves.push_back(SaveId{""});
    a.combatXp = 35; a.combatLevel = 2;
    a.sectorCleared = {1, 0};                 // v8: sector 0 cleared, sector 1 not
    a.bootHatchRemainMs = 424242;             // v9: an egg mid-incubation
    a.titlesUnlocked = 0x3; a.equippedTitle = 1;  // v10: both Titles, sector 1 equipped
    a.bossUnlocked = {1, 1};                   // v12: both sectors' bosses unlocked
    a.subCleared = {0x07, 0x00};               // v13: area 0 subs 1-3 cleared, area 1 none
    a.subBossUnlocked = {0x0F, 0x01};          // v13: area 0 subs 1-4 unlocked, area 1 sub 1
    a.brightness = 2;                          // v14: a non-default backlight level
    a.subRefarm.assign(kExplSectors * kExplSubAreas, 0);  // v15: per-sub re-farm counts
    a.subRefarm[2] = 7; a.subRefarm[9] = 3;
    a.defragCount = 12;                        // v16: the active pet's defrag tally
    a.bwUpgradeCount = 5;                       // v19: Hacker SHOP bandwidth upgrades bought
    a.apEnabled = 1;                            // v20: 'Pedia local-AP toggle

    SaveData b;
    CHECK(deserializeSave(serializeSave(a), b));
    CHECK(std::strcmp(b.activeId, "paypup") == 0);
    CHECK(b.hunger == 42 && b.frag == 13 && b.happy == 88 && b.mistakes == 3);
    CHECK(b.debuffs == 2 && b.ghost == 1);
    CHECK(b.timeInStageMs == 123456u && b.generation == 4);
    CHECK(b.bits == 777 && std::strcmp(b.hackerTag, "L33T_HAXX0R") == 0);
    CHECK(b.lifetimeUptimeMs == 99999u && b.lifetimeSteps == 12u && b.petsRaised == 4);
    CHECK(b.items.size() == 2 && std::strcmp(b.items[0].id, "dyno_nuggets") == 0 &&
          b.items[0].qty == 3);
    CHECK(b.ownedMods.size() == 1 && b.equipped.size() == 2);
    CHECK(std::strcmp(b.equipped[0].id, "firewall_patch") == 0 && b.equipped[1].id[0] == '\0');
    CHECK(b.hasPermanentModData);   // v17: the permanent-mod semantics marker (D3)
    CHECK(b.log.size() == 1 && std::strcmp(b.log[0].text, "USED YUBI-COOKIE") == 0);
    CHECK(b.rack.size() == 1 && std::strcmp(b.rack[0].id, "cryptoshell") == 0 &&
          b.rack[0].generation == 2);
    CHECK(b.hasMoveData);
    CHECK(b.ownedMoves.size() == 2 && std::strcmp(b.ownedMoves[0].id, "packet_storm") == 0);
    CHECK(b.equippedMoves.size() == 2 &&
          std::strcmp(b.equippedMoves[0].id, "packet_storm") == 0 &&
          b.equippedMoves[1].id[0] == '\0');
    CHECK(b.combatXp == 35 && b.combatLevel == 2);
    CHECK(b.sectorCleared.size() == 2 && b.sectorCleared[0] == 1 &&
          b.sectorCleared[1] == 0);
    CHECK(b.bootHatchRemainMs == 424242u);
    CHECK(b.titlesUnlocked == 0x3u && b.equippedTitle == 1);
    CHECK(b.bossUnlocked.size() == 2 && b.bossUnlocked[0] == 1 &&
          b.bossUnlocked[1] == 1);
    CHECK(b.subCleared.size() == 2 && b.subCleared[0] == 0x07 && b.subCleared[1] == 0x00);
    CHECK(b.subBossUnlocked.size() == 2 && b.subBossUnlocked[0] == 0x0F &&
          b.subBossUnlocked[1] == 0x01);
    CHECK(b.brightness == 2);                  // v14: the backlight level round-trips
    CHECK(b.subRefarm.size() == static_cast<size_t>(kExplSectors * kExplSubAreas) &&
          b.subRefarm[2] == 7 && b.subRefarm[9] == 3);   // v15: re-farm counts round-trip
    CHECK(b.defragCount == 12);                          // v16: active defrag tally
    CHECK(b.rack.size() == 1 && b.rack[0].defragCount == 9);  // v16: rack pet tally
    CHECK(b.bwUpgradeCount == 5);                         // v19: SHOP bandwidth upgrades
    CHECK(b.apEnabled == 1);                              // v20: 'Pedia AP toggle round-trips
}

// the CFG Brightness row cycles a discrete level and B applies it to the
// live, persisted setting; setBrightness clamps out-of-range input.
void test_cfg_brightness_apply() {
    Game g{StartMode::Hatched};
    CHECK(g.brightness() == kBrightnessDefault);  // starts brightest
    // Clamp guards.
    g.setBrightness(-5);  CHECK(g.brightness() == 0);
    g.setBrightness(999); CHECK(g.brightness() == kBrightnessLevels - 1);

    // Drive it through the CFG UI: CFG -> DEVICE -> BRIGHTNESS, cycle once, apply.
    g.setBrightness(0);                            // known start
    enterCfgTarget(g, CfgScreen::Brightness);      // the picker starts on the applied 0
    CHECK(g.nav() == Game::Nav::Detail);
    g.onButton(press(Button::A));                  // cycle 0 -> 1
    g.onButton(press(Button::B));                  // apply -> back to DEVICE
    CHECK(g.cfgScreen() == CfgScreen::Device);
    CHECK(g.brightness() == 1);                    // the live level changed + persists
}

// equip + persist: a Title equipped via the CFG picker survives a reboot, and
// Titles are player-level (a pet reset keeps them, like sector-clear flags).
void test_zone_titles_equip_via_cfg_and_persist() {
    MemSaveStore store;
    {
        Game g(StartMode::Hatched, "paypup", &store);
        // No Titles yet -> nothing equipped, "NONE" shown.
        CHECK(g.equippedTitle() == -1);
        CHECK(std::strcmp(g.equippedTitleName(), "NONE") == 0);
        // Grant both (the real path is a sector clear; debug mirrors the grant).
        g.debugUnlockTitle(0);
        CHECK(g.equippedTitle() == 0);            // first Title auto-equips
        g.debugUnlockTitle(1);
        CHECK(g.equippedTitle() == 0);            // a later grant doesn't re-equip

        // Equip sector 1's Title through the CFG Titles picker.
        enterCfgTarget(g, CfgScreen::Titles);     // the picker starts on equipped=0
        CHECK(g.nav() == Game::Nav::Detail);
        g.onButton(press(Button::A));             // cycle 0 -> 1 (next unlocked)
        g.onButton(press(Button::B));             // apply -> back to the list
        CHECK(g.nav() == Game::Nav::Submenu);
        CHECK(g.equippedTitle() == 1);
        CHECK(std::strcmp(g.equippedTitleName(), sectorTitle(1)) == 0);
        g.tick(kSaveAutosaveMs + kHeartbeatMs);   // persist the equip
    }
    {
        // Reboot over the same store: unlocks + the equip are restored.
        Game g(StartMode::FreshHatch, "paypup", &store);
        pickFirstEggLine(g);
        CHECK(g.titleUnlocked(0) && g.titleUnlocked(1));
        CHECK(g.equippedTitle() == 1);
        CHECK(std::strcmp(g.equippedTitleName(), sectorTitle(1)) == 0);
        // Player-level: a pet reset keeps the earned Titles (like sectorCleared).
        g.resetToHatch();
        CHECK(g.titleUnlocked(0) && g.titleUnlocked(1));
        CHECK(g.equippedTitle() == 1);
    }
}

// picker ring: A cycles NONE + unlocked Titles only, skipping locked ones.
void test_zone_titles_picker_skips_locked() {
    Game g{StartMode::Hatched};
    g.debugUnlockTitle(1);                        // unlock ONLY sector 1 (0 stays locked)
    CHECK(g.equippedTitle() == 1);                // auto-equipped the first earned
    enterCfgTarget(g, CfgScreen::Titles);         // the picker focus starts on 1
    g.onButton(press(Button::A));                 // 1 -> wraps past locked 0 to NONE(-1)
    g.onButton(press(Button::B));                 // equip NONE
    CHECK(g.equippedTitle() == -1);
    CHECK(std::strcmp(g.equippedTitleName(), "NONE") == 0);
}

// A missing / bad-magic / too-old / truncated blob deserializes as empty.
void test_save_version_and_empty() {
    SaveData out;
    CHECK(!deserializeSave({}, out));                       // empty -> false
    CHECK(out.activeId[0] == '\0');                         // left defaulted
    std::vector<uint8_t> junk = {'X', 'X', 'X', 'X', 1, 0};
    CHECK(!deserializeSave(junk, out));                     // bad magic
    SaveData a; std::strcpy(a.activeId, "paypup");
    auto blob = serializeSave(a);
    blob[4] = 0; blob[5] = 0;                               // below kOldestAcceptedVersion
    CHECK(!deserializeSave(blob, out));
    auto trunc = serializeSave(a); trunc.resize(10);        // cut mid-stream
    CHECK(!deserializeSave(trunc, out));
    CHECK(out.activeId[0] == '\0');
}

// A blob from a NEWER build still loads. This is the OTA rollback: an image boots on
// trial, rewrites the save at its own version, and the bootloader can then put the
// previous firmware back underneath it. Rejecting that blob would deserialize as empty
// — a fresh hatch with the pet, the rack and the records gone — so the reader takes the
// prefix it understands and drops only the tail it doesn't.
void test_save_from_a_newer_build_still_loads() {
    SaveData a;
    std::strcpy(a.activeId, "paypup");
    a.hunger = 61; a.happy = 43; a.bits = 777;
    auto blob = serializeSave(a);

    // Stamp it a version ahead and append a tail this build knows nothing about — the
    // exact shape every version bump here adds.
    const uint16_t future = kSaveVersion + 1;
    blob[4] = static_cast<uint8_t>(future);
    blob[5] = static_cast<uint8_t>(future >> 8);
    for (int i = 0; i < 32; ++i) blob.push_back(static_cast<uint8_t>(0xA5));

    SaveData out;
    CHECK(deserializeSave(blob, out));
    CHECK(std::strcmp(out.activeId, "paypup") == 0);        // the pet survives a rollback
    CHECK(out.hunger == 61);
    CHECK(out.happy == 43);
    CHECK(out.bits == 777);
}

// Boot-from-save vs Hatch: an empty store hatches; once a pet is raised + saved,
// a fresh Game over the same store boots straight onto the raised pet, restored.
void test_boot_from_save_vs_hatch() {
    MemSaveStore store;
    CHECK(store.bytes().empty());
    {
        Game g(StartMode::FreshHatch, "paypup", &store);   // empty save -> line-select
        pickFirstEggLine(g);                               // pick Ransomware -> egg at idle
        CHECK(g.nav() == Game::Nav::Idle);
        CHECK(g.pet() && std::strcmp(g.pet()->id, "cryptoshell") == 0 && g.inEggPhase());
        CHECK(!store.bytes().empty());                     // the laid egg persisted
        uint32_t t = 1000; g.tick(t);
        g.tick(t += kBootHatchMs + kHeartbeatMs);          // wait out incubation -> Process
        // Random hatch: the CryptoShell egg -> a random ransomware Process
        // (Paypup or Conkittenate), so assert the STAGE, not a specific species.
        CHECK(g.pet() && g.pet()->stage == Stage::Process);
        CHECK(g.generation() == 1);
        g.model().setHunger(33);
        g.inventory().remove("dyno_nuggets", 1);           // 3 -> 2
        g.tick(t += kSaveAutosaveMs + kHeartbeatMs);        // periodic autosave
    }
    {
        Game g(StartMode::FreshHatch, "paypup", &store);   // would lay an egg if empty
        pickFirstEggLine(g);                               // no-op: booted from the save
        CHECK(g.nav() == Game::Nav::Idle);                 // booted from save instead
        CHECK(g.pet() && g.pet()->stage == Stage::Process);  // the hatched pet round-tripped
        CHECK(!g.inEggPhase());                            // restored as a hatched Process pet
        CHECK(g.generation() == 1);
        CHECK(g.model().hunger() == 33);                   // vitals restored
        CHECK(g.inventory().count("dyno_nuggets") == 2);   // inventory restored
    }
}

// A plain reboot (a fresh Game over the same store — the ARCH rack never enters
// it) round-trips the ACTIVE pet's creature level, not just its care stats.
// test_arch_store_deploy_preserves_creature_level covers the rack path; this
// covers the simpler power-cycle/reflash path, since NVS survives both.
void test_reboot_preserves_active_creature_level() {
    MemSaveStore store;
    int lvl = 0, xp = 0;
    {
        Game g{StartMode::Hatched, "paypup", &store};
        g.debugAddCombatXp(1200);
        lvl = g.combatLevel();
        xp = g.combatXp();
        CHECK(lvl >= 1);
        g.tick(kSaveAutosaveMs + kHeartbeatMs);   // force the periodic autosave to fire
    }
    {
        Game g{StartMode::Hatched, "paypup", &store};
        CHECK(g.combatLevel() == lvl);
        CHECK(g.combatXp() == xp);
    }
}

// The dev / Factory reset wipes the raised pet and lays a fresh egg in its place:
// the store holds no raised pet, and the next boot resumes the fresh egg.
void test_persistence_reset_clears_store() {
    MemSaveStore store;
    { Game g(StartMode::Hatched, "paypup", &store);
      g.tick(kSaveAutosaveMs + kHeartbeatMs); }            // autosave a raised pet
    CHECK(!store.bytes().empty());
    { Game g(StartMode::Hatched, "paypup", &store);
      CHECK(g.nav() == Game::Nav::Idle);                   // loaded from save
      g.resetToHatch();
      pickFirstEggLine(g);                       // reset re-enters line-select; pick Ransomware
      // The wipe replaced the raised pet with a freshly-laid egg (persisted, v9) —
      // the store is rewritten, not emptied (a laid egg is durable).
      CHECK(g.pet() && std::strcmp(g.pet()->id, "cryptoshell") == 0 && g.inEggPhase()); }
    { Game g(StartMode::FreshHatch, "paypup", &store);
      CHECK(g.nav() == Game::Nav::Idle);                   // boots the persisted egg
      CHECK(g.pet() && std::strcmp(g.pet()->id, "cryptoshell") == 0 && g.inEggPhase());
      CHECK(g.generation() == 1); }                        // the reset pet is generation 1
}

// HackerTag on-device editor: A cycles a cell, B advances, the OK cell
// saves; the edited tag persists.
void test_hackertag_editor() {
    MemSaveStore store;
    Game g(StartMode::Hatched, "paypup", &store);
    enterCfgTarget(g, CfgScreen::HackerTag);               // open the editor
    CHECK(g.nav() == Game::Nav::Detail);
    g.onButton(press(Button::A));                          // cell 0: 'N' -> 'O'
    for (int i = 0; i < kHackerTagMax; ++i) g.onButton(press(Button::B));  // caret -> OK
    g.onButton(press(Button::B));                          // OK -> save
    CHECK(g.nav() == Game::Nav::Submenu);
    CHECK(std::strcmp(g.hackerTag(), "OETRUNNER_99") == 0);
    // Persists across a reboot.
    g.tick(kSaveAutosaveMs + kHeartbeatMs);
    Game g2(StartMode::Hatched, "paypup", &store);
    CHECK(std::strcmp(g2.hackerTag(), "OETRUNNER_99") == 0);
}

// ARCH Store + Deploy: Store sets the active pet aside into the rack and
// fires the new-egg Hatch; Deploy swaps a stored pet back in (slot-neutral).
void test_arch_store_and_deploy() {
    Game g{StartMode::Hatched, "paypup"};                  // active = Paypup, gen 1
    CHECK(g.rackCount() == 0 && g.generation() == 1);

    enterSubmenuId(g, SubmenuId::Arch);
    g.onButton(press(Button::B));                          // open active record (Store)
    g.onButton(press(Button::B));                          // Store -> confirm (default Cancel)
    g.onButton(press(Button::A));                          // Cancel -> Confirm
    g.onButton(press(Button::B));                          // commit Store
    pickFirstEggLine(g);                                   // vacated -> line-select; pick Ransomware
    CHECK(g.nav() == Game::Nav::Idle);                     // active vacated -> new egg at idle
    CHECK(g.rackCount() == 1 && g.pet() && g.inEggPhase());
    CHECK(std::strcmp(g.pet()->id, "cryptoshell") == 0);
    CHECK(g.generation() == 2);                            // lifetime ordinal advanced

    enterSubmenuId(g, SubmenuId::Arch);
    g.onButton(press(Button::A));                          // focus the stored row (Paypup)
    g.onButton(press(Button::B));                          // open stored record (Deploy)
    g.onButton(press(Button::B));                          // Deploy -> confirm
    g.onButton(press(Button::A));                          // -> Confirm
    g.onButton(press(Button::B));                          // commit Deploy
    CHECK(g.nav() == Game::Nav::Idle);
    CHECK(g.pet() && std::strcmp(g.pet()->id, "paypup") == 0);   // Paypup is active again
    CHECK(g.generation() == 1);                            // its own generation rode along
    CHECK(g.rackCount() == 1);                             // slot-neutral: CryptoShell stored
}

// STAT surfaces the persisted generation + the lifetime footer.
void test_stat_footer_and_generation() {
    Game g{StartMode::Hatched, "cryptoshell"};
    CHECK(g.generation() == 1);
    enterSubmenuId(g, SubmenuId::Stat);
    Framebuffer fb(kActiveW, kActiveH);
    g.render(fb);                                           // vitals page (name + gen)
    CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));
    g.onButton(press(Button::A));                          // page to the Hacker-Log
    g.render(fb);
    CHECK(hasDarkInk(fb, 0, 150, kActiveW, kActiveH));      // lifetime footer band renders
}

// Grayscale gate: the ARCH record with a populated rack (Deploy variant) reads
// without colour, and the inline confirm overlay does too.
void test_arch_rack_grayscale() {
    Game g{StartMode::Hatched, "paypup"};
    g.debugSeedRack("cryptoshell");
    enterSubmenuId(g, SubmenuId::Arch);
    Framebuffer fb(kActiveW, kActiveH);
    g.render(fb); CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));   // list w/ stored row
    g.onButton(press(Button::A));                          // focus the stored pet
    g.onButton(press(Button::B));                          // open its record (Deploy)
    g.render(fb); CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));
    g.onButton(press(Button::B));                          // Deploy -> confirm overlay
    g.render(fb); CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));
}

// Single-frame creatures render statically without flicker: idleFrame() never
// indexes a missing frame, so a 1-frame placeholder sprite is usable as-is and a
// richer sheet animates automatically as data (add-now / animate-later).
void test_idle_frame_single_frame_safe() {
    SpriteData one{}; one.frames = 1;            // a single-frame placeholder
    for (int b = 0; b < 24; ++b) CHECK(idleFrame(one, b) == 0);   // always valid + static

    SpriteData two{}; two.frames = 2;            // breathe, but no blink frame
    bool saw0 = false, saw1 = false;
    for (int b = 0; b < 24; ++b) {
        const int f = idleFrame(two, b);
        CHECK(f >= 0 && f < 2);                  // never the absent blink frame
        saw0 |= (f == 0); saw1 |= (f == 1);
    }
    CHECK(saw0 && saw1);                          // both breathe frames used

    const SpriteData& pay = ASSET_SPR_PET_PAYPUP;  // rich sheet: blink reachable
    bool sawBlink = false;
    for (int b = 0; b < 24; ++b) {
        const int f = idleFrame(pay, b);
        CHECK(f >= 0 && f < pay.frames);
        sawBlink |= (f == 2);
    }
    CHECK(sawBlink);
}

// --- Resting motion (core/model/idle_wander.h) -----------------------------

// Run a mover for a while and describe where it went. `beats` are heartbeats, so
// these figures are what the habitat would actually have drawn over that stretch.
struct WanderTrace {
    int minX = 0, maxX = 0, minY = 0, maxY = 0;
    int onShelfBeats = 0;      // heartbeats spent with the feet on the floor
    int lowBeats = 0;          // ...spent in the bottom third of the box
    int highBeats = 0;         // ...and in the top third
    int movingBeats = 0;       // ...spent going somewhere on either axis
    int movedYBeats = 0;       // ...spent changing height
    int longestStill = 0;      // the longest unbroken stretch parked between trips
};
static WanderTrace traceWander(Locomotion loco, int beats) {
    IdleWander w;
    WanderTrace t;
    int lastX = 0, lastY = 0, still = 0;
    for (int i = 0; i < beats; ++i) {
        w.step(loco);
        const int x = w.offsetX(), y = w.offsetY();
        t.minX = std::min(t.minX, x); t.maxX = std::max(t.maxX, x);
        t.minY = std::min(t.minY, y); t.maxY = std::max(t.maxY, y);
        if (y == 0) ++t.onShelfBeats;
        if (y <= kWanderRiseMax / 3) ++t.lowBeats;
        if (y >= kWanderRiseMax * 2 / 3) ++t.highBeats;
        if (x != lastX || y != lastY) { ++t.movingBeats; still = 0; }
        else t.longestStill = std::max(t.longestStill, ++still);
        if (y != lastY) ++t.movedYBeats;
        lastX = x; lastY = y;
    }
    return t;
}

// The box is the whole safety contract: the sprite is drawn from this offset, so a
// mover that walks out of it walks off the canvas. Every locomotion is bounded by
// the same box no matter how long it runs, and none of them ever go BELOW the shelf.
void test_idle_wander_stays_inside_the_living_box() {
    for (Locomotion loco : {Locomotion::Walk, Locomotion::Fly, Locomotion::Swim,
                            Locomotion::Ground, Locomotion::Static}) {
        const WanderTrace t = traceWander(loco, 4000);
        CHECK(t.minX >= -kWanderHalfSpanX && t.maxX <= kWanderHalfSpanX);
        CHECK(t.minY >= 0 && t.maxY <= kWanderRiseMax);
        // ...and every mover that moves at all uses both sides of the box rather than
        // one. Static is the exception by definition: it is the row an egg declares to
        // say it goes nowhere, so "nowhere" is what it has to keep doing.
        if (loco == Locomotion::Static) CHECK(t.movingBeats == 0);
        else CHECK(t.maxX > 0 && t.minX < 0);
    }
}

// The three read as three different creatures, which is the point of the field:
// a walker is a floor animal that ambles and then stands still, a flier is almost
// always in the air, and a swimmer is neither pulled down nor holding a height —
// it drifts on both axes at once, and then holds station.
//
// That last clause is load-bearing rather than descriptive. The habitat swaps a
// creature's "walk" clip in for as long as the wander is travelling, so a mover that
// retargets the beat it arrives never shows its idle at all — whatever is authored on
// row 0 is simply never reached. A swimmer's rest is therefore sized against the clip
// it has to make room for, which is why longestStill is asserted and not just the
// moving share: the question is not "does it pause" but "does it pause for long
// enough to play something through".
void test_idle_wander_reads_differently_per_locomotion() {
    const int beats = 4000;

    const WanderTrace walk = traceWander(Locomotion::Walk, beats);
    CHECK(walk.onShelfBeats == beats);        // never once off the floor
    CHECK(walk.movedYBeats == 0);
    CHECK(walk.movingBeats > 0);              // but it does get about
    CHECK(walk.movingBeats < beats / 2);      // ...spending most of its time parked

    const WanderTrace fly = traceWander(Locomotion::Fly, beats);
    CHECK(fly.onShelfBeats * 100 < beats);    // touches down on under 1% of beats
    CHECK(fly.lowBeats * 10 < beats);         // and is rarely even near the floor
    CHECK(fly.maxY == kWanderRiseMax);        // it is the top of the box it lives in
    CHECK(fly.movingBeats > beats / 2);       // hardly ever still
    CHECK(fly.longestStill < 8);              // and never long enough to pose

    const WanderTrace swim = traceWander(Locomotion::Swim, beats);
    CHECK(swim.onShelfBeats * 100 < beats);   // nothing pulls it down...
    CHECK(swim.highBeats * 10 > beats);       // ...and nothing holds it up either:
    CHECK(swim.lowBeats * 10 > beats);        // it uses the whole depth of the box
    CHECK(swim.movingBeats > beats / 3);      // it does spend its time drifting...
    CHECK(swim.movingBeats < beats * 2 / 3);  // ...but it is not the flier: it stops
    CHECK(swim.movedYBeats > beats / 5);      // and the drift is on both axes
    // Long enough parked for a four-frame idle at holdBeats=2 to run its whole loop.
    CHECK(swim.longestStill >= 8);
}

// A crawler is the floor-mover a walker only nearly is. It shares the shelf, but it
// also gives up the 2px shelf bob the habitat draws on top of the drift — which is
// the whole of the difference, and the reason the Worm line needed its own row.
void test_idle_wander_crawler_never_leaves_the_floor() {
    const int beats = 4000;
    const WanderTrace crawl = traceWander(Locomotion::Ground, beats);
    CHECK(crawl.onShelfBeats == beats);
    CHECK(crawl.movedYBeats == 0);
    CHECK(crawl.movingBeats > 0);            // it does still get about, slowly

    CHECK(!IdleWander::bobs(Locomotion::Ground));
    CHECK(IdleWander::bobs(Locomotion::Walk));   // ...where a walker still lifts
}

// The habitat has ONE call site and no reset hook, so the component has to notice a
// new occupant itself — otherwise a walker that evolved into a swimmer would keep
// its feet glued to the shelf until a reboot.
void test_idle_wander_rehomes_when_the_mover_changes() {
    IdleWander w;
    for (int i = 0; i < 200; ++i) w.step(Locomotion::Walk);
    CHECK(w.offsetY() == 0);

    w.step(Locomotion::Swim);                 // a different creature entirely
    CHECK(w.offsetX() == 0 && w.offsetY() == 0);   // back on the anchor to start from
    bool leftTheFloor = false;
    for (int i = 0; i < 200; ++i) { w.step(Locomotion::Swim); leftTheFloor |= w.offsetY() > 0; }
    CHECK(leftTheFloor);

    w.park();
    CHECK(w.offsetX() == 0 && w.offsetY() == 0);
}

// The seam, at the engine: a raised pet drifts around its shelf as the heartbeat
// runs, and an egg does not — an egg sits where it was laid, with its incubation
// countdown drawn directly above it.
void test_habitat_moves_a_pet_and_parks_an_egg() {
    Game g{StartMode::Hatched};
    uint32_t t = 0;
    CHECK(g.nav() == Game::Nav::Idle);
    bool moved = false;
    for (int i = 0; i < 200 && !moved; ++i) {
        g.tick(t += kHeartbeatMs);
        moved = g.petWander().offsetX() != 0;
    }
    CHECK(moved);

    Game egg{StartMode::FreshHatch};
    for (int i = 0; i < 200; ++i) {
        egg.tick(t += kHeartbeatMs);
        CHECK(egg.petWander().offsetX() == 0 && egg.petWander().offsetY() == 0);
    }
}

// ===========================================================================
// Sim-Battle + combat integration
// ===========================================================================
