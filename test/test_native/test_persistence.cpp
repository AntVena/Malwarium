// test_persistence.cpp: native gates for the save format, its migrations and ARCH.
//
// One slice of the native suite; see test_gates.h for the shared fixtures and
// test_main.cpp for the roster that runs these. A save-vNN gate sits with the
// feature whose field it migrates, not in a migrations pile of its own.
#include "test_gates.h"

#include "core/content/content_backgrounds.h"   // kBackgrounds: a pick the round trip can name
#include "core/render/palette.h"                // palThemeByName: the settings gates name a set

// The LOADOUT hub's back-out chain, and EXPL's nested list.
void test_loadout_expl_nav() {
    Framebuffer fb(kActiveW, kActiveH);
    // MODS opens on the hub; MOVES is one row in, and its move picker one more. C
    // walks all three back out in order (picker -> slot list -> hub -> carousel),
    // so the hub costs exactly one press and never swallows the one that leaves.
    { Game g{StartMode::Hatched}; enterLoadoutTab(g, 1);
      CHECK(g.nav() == Game::Nav::Submenu);
      g.render(fb); CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));
      tapB(g);                                       // slot 1 -> move picker
      CHECK(g.nav() == Game::Nav::Detail);
      g.render(fb); CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));
      tapC(g);                  // back to the slot list
      CHECK(g.nav() == Game::Nav::Submenu);
      tapC(g);                  // back to the hub: still L2
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
    // Three-level nav: entering parks on the ACTIVITY PICKER's STORY row; B opens it
    // onto the area-0 header, B DRILLS into the area, then B arms its first open
    // sub-area 1 → the IDLE habitat. Re-open and A cycles to sub-area 2. Area 1's subs
    // stay LOCKED until area 0 clears (its header still isn't enterable-to-arm: a
    // locked area is inert).
    { Game g{StartMode::Hatched}; enterSubmenuId(g, SubmenuId::Expl);
      CHECK(g.nav() == Game::Nav::Submenu);
      g.render(fb); CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));
      g.onButton(press(Button::B));                  // STORY -> the area list
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
    std::strcpy(a.active.id, "paypup");
    a.active.hunger = 42; a.active.frag = 13; a.active.happy = 88; a.active.mistakes = 3;
    a.active.debuffs = 2; a.active.ghost = 1; a.active.timeInStageMs = 123456; a.active.generation = 4;
    a.op.bits = 777; std::strcpy(a.op.hackerTag, "L33T_HAXX0R");
    a.op.lifetimeUptimeMs = 99999; a.op.lifetimeSteps = 12; a.op.petsRaised = 4;
    a.items.push_back(SaveStack{"dyno_nuggets", 3});
    a.items.push_back(SaveStack{"yubi_cookie", 1});
    a.op.ownedMods.push_back(SaveId{"firewall_patch"});
    a.active.equippedMods[0] = SaveId{"firewall_patch"};
    SaveLogEntry le; le.type = static_cast<uint8_t>(LogEventType::ItemUsed);
    std::strcpy(le.text, "USED YUBI-COOKIE"); a.log.push_back(le);
    SaveStoredPet sp; std::strcpy(sp.id, "cryptoshell");
    sp.hunger = 60; sp.generation = 2; sp.defragCount = 9;  // v16: rack pet's tally
    a.op.rack.push_back(sp);
    a.active.ownedMoves.push_back(SaveId{"packet_storm"});
    a.active.ownedMoves.push_back(SaveId{"fork_bomb"});
    a.active.equippedMoves[0] = SaveId{"packet_storm"};
    a.active.combatXp = 35; a.active.combatLevel = 2;
    a.op.sectorCleared = {1, 0};                 // v8: sector 0 cleared, sector 1 not
    a.active.bootHatchRemainMs = 424242;             // v9: an egg mid-incubation
    a.op.titlesUnlocked = 0x3; a.op.equippedTitle = 1;  // v10: both Titles, sector 1 equipped
    a.op.bossUnlocked = {1, 1};                   // v12: both sectors' bosses unlocked
    a.op.subCleared = {0x07, 0x00};               // v13: area 0 subs 1-3 cleared, area 1 none
    a.op.subBossUnlocked = {0x0F, 0x01};          // v13: area 0 subs 1-4 unlocked, area 1 sub 1
    a.settings.brightness = 2;                          // v14: a non-default backlight level
    a.activeArmed.subRefarm.assign(kExplSectors * kExplSubAreas, 0);  // v15: per-sub re-farm counts
    a.activeArmed.subRefarm[2] = 7; a.activeArmed.subRefarm[9] = 3;
    a.active.defragCount = 12;                        // v16: the active pet's defrag tally
    a.op.bwUpgradeCount = 5;                       // v19: Hacker SHOP bandwidth upgrades bought
    a.apEnabled = 1;                            // v20: 'Pedia local-AP toggle

    SaveData b;
    CHECK(deserializeSave(serializeSave(a), b));
    CHECK(std::strcmp(b.active.id, "paypup") == 0);
    CHECK(b.active.hunger == 42 && b.active.frag == 13 && b.active.happy == 88 && b.active.mistakes == 3);
    CHECK(b.active.debuffs == 2 && b.active.ghost == 1);
    CHECK(b.active.timeInStageMs == 123456u && b.active.generation == 4);
    CHECK(b.op.bits == 777 && std::strcmp(b.op.hackerTag, "L33T_HAXX0R") == 0);
    CHECK(b.op.lifetimeUptimeMs == 99999u && b.op.lifetimeSteps == 12u && b.op.petsRaised == 4);
    CHECK(b.items.size() == 2 && std::strcmp(b.items[0].id, "dyno_nuggets") == 0 &&
          b.items[0].qty == 3);
    CHECK(b.op.ownedMods.size() == 1);
    CHECK(std::strcmp(b.active.equippedMods[0].id, "firewall_patch") == 0 && b.active.equippedMods[1].id[0] == '\0');
    CHECK(b.op.hasPermanentModData);   // v17: the permanent-mod semantics marker
    CHECK(b.log.size() == 1 && std::strcmp(b.log[0].text, "USED YUBI-COOKIE") == 0);
    CHECK(b.op.rack.size() == 1 && std::strcmp(b.op.rack[0].id, "cryptoshell") == 0 &&
          b.op.rack[0].generation == 2);
    CHECK(b.active.ownedMoves.size() == 2 && std::strcmp(b.active.ownedMoves[0].id, "packet_storm") == 0);
    CHECK(std::strcmp(b.active.equippedMoves[0].id, "packet_storm") == 0 &&
          b.active.equippedMoves[1].id[0] == '\0');
    CHECK(b.active.combatXp == 35 && b.active.combatLevel == 2);
    CHECK(b.op.sectorCleared.size() == 2 && b.op.sectorCleared[0] == 1 &&
          b.op.sectorCleared[1] == 0);
    CHECK(b.active.bootHatchRemainMs == 424242u);
    CHECK(b.op.titlesUnlocked == 0x3u && b.op.equippedTitle == 1);
    CHECK(b.op.bossUnlocked.size() == 2 && b.op.bossUnlocked[0] == 1 &&
          b.op.bossUnlocked[1] == 1);
    CHECK(b.op.subCleared.size() == 2 && b.op.subCleared[0] == 0x07 && b.op.subCleared[1] == 0x00);
    CHECK(b.op.subBossUnlocked.size() == 2 && b.op.subBossUnlocked[0] == 0x0F &&
          b.op.subBossUnlocked[1] == 0x01);
    CHECK(b.settings.brightness == 2);                  // v14: the backlight level round-trips
    CHECK(b.activeArmed.subRefarm.size() == static_cast<size_t>(kExplSectors * kExplSubAreas) &&
          b.activeArmed.subRefarm[2] == 7 && b.activeArmed.subRefarm[9] == 3);   // v15: re-farm counts round-trip
    CHECK(b.active.defragCount == 12);                          // v16: active defrag tally
    CHECK(b.op.rack.size() == 1 && b.op.rack[0].defragCount == 9);  // v16: rack pet tally
    CHECK(b.op.bwUpgradeCount == 5);                         // v19: SHOP bandwidth upgrades
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
    CHECK(out.active.id[0] == '\0');                         // left defaulted
    std::vector<uint8_t> junk = {'X', 'X', 'X', 'X', 1, 0};
    CHECK(!deserializeSave(junk, out));                     // bad magic
    SaveData a; std::strcpy(a.active.id, "paypup");
    auto blob = serializeSave(a);
    blob[4] = 0; blob[5] = 0;                               // below kOldestAcceptedVersion
    CHECK(!deserializeSave(blob, out));
    auto trunc = serializeSave(a); trunc.resize(10);        // cut mid-stream
    CHECK(!deserializeSave(trunc, out));
    CHECK(out.active.id[0] == '\0');
}

// A blob from a NEWER build still loads. This is the OTA rollback: an image boots on
// trial, rewrites the save at its own version, and the bootloader can then put the
// previous firmware back underneath it. Rejecting that blob would deserialize as empty
// (a fresh hatch with the pet, the rack and the records gone), so the reader takes the
// prefix it understands and drops only the tail it doesn't.
void test_save_from_a_newer_build_still_loads() {
    SaveData a;
    std::strcpy(a.active.id, "paypup");
    a.active.hunger = 61; a.active.happy = 43; a.op.bits = 777;
    auto blob = serializeSave(a);

    // Stamp it a version ahead and append a tail this build knows nothing about: the
    // exact shape every version bump here adds.
    const uint16_t future = kSaveVersion + 1;
    blob[4] = static_cast<uint8_t>(future);
    blob[5] = static_cast<uint8_t>(future >> 8);
    for (int i = 0; i < 32; ++i) blob.push_back(static_cast<uint8_t>(0xA5));

    SaveData out;
    CHECK(deserializeSave(blob, out));
    CHECK(std::strcmp(out.active.id, "paypup") == 0);        // the pet survives a rollback
    CHECK(out.active.hunger == 61);
    CHECK(out.active.happy == 43);
    CHECK(out.op.bits == 777);
}

// A v75 blob with every field of the active pet set (built from a SaveData by the v75
// codec). Each field sits where the version that added it put it, which is what lets an
// older build read the pet after an OTA rollback.
static const char* const kActivePetV75Blob =
    "4d414c314b007061797075700000000000000000000000000000000000002a00"
    "00000d0000005800000003000000020000000140e20100040000000000000000"
    "0000000000000000000000000000000000000000040000000000000003006669"
    "726577616c6c5f70617463680000000000000000000000000000000000000000"
    "0000000000000000000000000000636c6f636b5f73706565645f626f6f737400"
    "000000000000000000000300717569636b5f6a61620000000000000000000000"
    "000000007061636b65745f73746f726d000000000000000000000000666f726b"
    "5f626f6d6200000000000000000000000000000004007061636b65745f73746f"
    "726d000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000023000000030000000000"
    "0000000000000000000000000000000000000000000000000000ffffffff0400"
    "01000000000000000200000000000000000000000000030000000a0000000000"
    "070000000000000000000000000000000c000000000000000000000000010101"
    "0000000004000102000000000000000000000000000000000000000000000000"
    "000001000028db0b000000000000000000000900000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000008813000000000000000000000000000002000000000000"
    "0000000000000000000000000000000000000000000001000000000000000000"
    "0000010000001900000000000000000000000000000204010000000002007370"
    "616d000000000000000000000000000000000000000064796e6f5f6e75676765"
    "7473000000000000000000000000040000000000000000000000000000000000"
    "00000000000000000000000000000001020200000000017370616d0000000000"
    "0000000000000000000000000000000100000000000064003200102700000000";

static std::vector<uint8_t> activePetV75Blob() {
    std::vector<uint8_t> out;
    for (const char* p = kActivePetV75Blob; p[0] && p[1]; p += 2)
        out.push_back(static_cast<uint8_t>(std::stoi(std::string(p, 2), nullptr, 16)));
    return out;
}

// The v75 blob reads into SaveData::active and activeArmed field by field, and writes
// back with the same bytes up to its end: the record moved in SaveData, not on the wire.
void test_save_active_pet_reads_from_a_v75_blob() {
    const std::vector<uint8_t> blob = activePetV75Blob();
    CHECK(blob.size() > 6 && blob[4] == 75 && blob[5] == 0);
    SaveData d;
    CHECK(deserializeSave(blob, d));
    const SaveStoredPet& a = d.active;
    CHECK(std::strcmp(a.id, "paypup") == 0);
    CHECK(a.hunger == 42 && a.frag == 13 && a.happy == 88 && a.mistakes == 3);
    CHECK(a.debuffs == 2 && a.ghost == 1);
    CHECK(a.timeInStageMs == 123456u && a.generation == 4);
    CHECK(std::strcmp(a.equippedMods[0].id, "firewall_patch") == 0);
    CHECK(a.equippedMods[1].id[0] == '\0');
    CHECK(std::strcmp(a.equippedMods[2].id, "clock_speed_boost") == 0);
    CHECK(a.ownedMoves.size() == 3 && std::strcmp(a.ownedMoves[2].id, "fork_bomb") == 0);
    CHECK(std::strcmp(a.equippedMoves[0].id, "packet_storm") == 0);
    CHECK(a.equippedMoves[1].id[0] == '\0');
    CHECK(a.combatXp == 35 && a.combatLevel == 3);
    CHECK(a.statPoints[0] == 1 && a.statPoints[1] == 0 && a.statPoints[2] == 2);
    CHECK(a.slotKinds[0] == 1 && a.slotKinds[1] == 2 && a.slotKinds[2] == 0);
    CHECK(a.defragCount == 12);
    CHECK(a.shieldActive == 1 && a.shieldSpent == 1 && a.yubiSpent == 1);
    CHECK(a.bestDeepWebDepth == 9 && a.bestDarkWebDepth == 4);
    CHECK(a.dyingElapsedMs == 5000u);
    CHECK(a.bandwidthRegenBonusMin == 2);
    CHECK(a.statBonus[0] == 1 && a.statBonus[3] == 1 && a.xpRateBonusPct == 25);
    CHECK(a.usbBranch == 2 && a.usbSoak == 4 && a.usbHold == 1);
    CHECK(a.foodsEaten.size() == 2 && std::strcmp(a.foodsEaten[1].id, "dyno_nuggets") == 0);
    CHECK(std::strcmp(a.favouriteFood, "spam") == 0 && a.favouriteFound == 1);
    CHECK(a.bondBp[0] == 100 && a.bondBp[1] == 50 && a.bondBp[2] == 10000);
    CHECK(d.activeArmed.trojanDivert == 1);
    CHECK(d.activeArmed.backupShieldUntilMs == 777000u);
    CHECK(d.activeArmed.subRefarm.size() == 10 && d.activeArmed.subRefarm[2] == 7);

    // Past the version word, a newer writer only ever appends.
    const std::vector<uint8_t> again = serializeSave(d);
    CHECK(again.size() >= blob.size());
    CHECK(std::equal(blob.begin() + 6, blob.end(), again.begin() + 6));
}

// ...and boots into the Pet record through Pet::thawed, with the armed part the save
// keeps.
void test_save_active_pet_loads_from_a_v75_blob() {
    MemSaveStore store;
    store.save(activePetV75Blob());
    Game g(StartMode::FreshHatch, "malbear", &store);
    const Pet& p = g.petRecord();
    CHECK(p.creature && std::strcmp(p.creature->id, "paypup") == 0);
    CHECK(p.model.hunger() == 42 && p.model.fragmentation() == 13 && p.model.happiness() == 88);
    CHECK(p.model.careMistakes() == 3 && p.model.hasGhost());
    CHECK(p.generation == 4 && p.level == 3 && p.xp == 35);
    CHECK(p.statPoints[0] == 1 && p.statPoints[2] == 2);
    CHECK(p.slotKinds[0] == SlotKind::Attack && p.slotKinds[1] == SlotKind::Defend);
    CHECK(p.moves.owns("quick_jab") && p.moves.owns("packet_storm") && p.moves.owns("fork_bomb"));
    CHECK(p.moves.equipped(0) && std::strcmp(p.moves.equipped(0), "packet_storm") == 0);
    CHECK(p.mods.equipped(0) && std::strcmp(p.mods.equipped(0), "firewall_patch") == 0);
    CHECK(p.mods.equipped(1) == nullptr);
    CHECK(p.mods.equipped(2) && std::strcmp(p.mods.equipped(2), "clock_speed_boost") == 0);
    CHECK(p.defragCount == 12 && p.bestDeepWebDepth == 9 && p.bestDarkWebDepth == 4);
    CHECK(p.dyingElapsedMs == 5000u && !p.dyingArmed);
    CHECK(p.grants.shieldActive && p.grants.shieldSpent && p.grants.yubiSpent);
    CHECK(p.usb.branch == BranchOverride::Bad && p.usb.soakFactor == 4 && p.usb.hold);
    CHECK(p.upgrades.bandwidthRegenMin == 2 && p.upgrades.xpRatePct == 25);
    CHECK(p.upgrades.statBonus[0] == 1 && p.upgrades.statBonus[3] == 1);
    CHECK(p.upgrades.favouriteFood && std::strcmp(p.upgrades.favouriteFood->id, "spam") == 0);
    CHECK(p.upgrades.favouriteFound);
    CHECK(p.upgrades.bondBp[0] == 100 && p.upgrades.bondBp[2] == 10000);
    CHECK(p.foodsEaten.size() == 2);
    CHECK(p.armed.trojanDivert && p.armed.backupShieldUntilMs == 777000u);
    CHECK(p.armed.subRefarmCount[0][2] == 7);
}

// A pet with something in every part survives a save and a reboot whole: its record
// compares equal as a rack record, and the save keeps its Backup Drive and divert. A
// DeepWeb arming does not survive a reboot.
void test_save_active_pet_round_trips_through_a_reboot() {
    MemSaveStore store;
    Pet before;
    {
        Game g{StartMode::Hatched, "paypup", &store};
        for (const char* id : {"restore_point", "yubi_cookie", "backup_drive", "ambig_usb",
                               "bad_usb", "tiramisudo", "deep_learning_module"}) {
            g.inventory().add(id, 1);
            g.debugUseItem(id);
        }
        g.debugTasteFoods(3);
        g.debugSetFavouriteFood("spam");
        g.debugSetBestDarkWebDepth(5);
        g.debugGrantMove("buffer_overflow");
        CHECK(g.petRecord().grants.shieldActive && g.petRecord().grants.yubiSpent);
        CHECK(g.backupShieldArmed() && g.petRecord().armed.trojanDivert);
        CHECK(g.debugDepthMultiplier() == 2);
        before = g.petRecord();
        CHECK(g.saveNow());
    }
    Game g(StartMode::FreshHatch, "malbear", &store);
    const Pet& after = g.petRecord();
    // Stored at each pet's own stage start, so the two clocks line up.
    SaveData x, y;
    x.op.rack = {before.stored(before.stageEnteredMs)};
    y.op.rack = {after.stored(after.stageEnteredMs)};
    CHECK(serializeSave(x) == serializeSave(y));
    CHECK(after.armed.trojanDivert);
    CHECK(after.armed.backupShieldUntilMs == before.armed.backupShieldUntilMs);
    CHECK(g.backupShieldArmed());
    CHECK(g.debugDepthMultiplier() == 1);
}

// A new egg is a whole new Pet: nothing of the pet before it carries over, and it owns
// its own line's moves. The device's first egg keeps the sample mods; a later one has none.
void test_new_egg_is_a_fresh_pet_record() {
    {
        Game g{StartMode::FreshHatch};
        pickFirstEggLine(g);
        const Pet& p = g.petRecord();
        CHECK(g.inEggPhase());
        CHECK(p.mods.equipped(0) && std::strcmp(p.mods.equipped(0), "firewall_patch") == 0);
        CHECK(p.mods.equipped(1) && std::strcmp(p.mods.equipped(1), "clock_speed_boost") == 0);
    }
    Game g{StartMode::Hatched, "paypup"};
    for (const char* id : {"restore_point", "yubi_cookie", "backup_drive", "ambig_usb",
                           "bad_usb", "tiramisudo", "deep_learning_module"}) {
        g.inventory().add(id, 1);
        g.debugUseItem(id);
    }
    g.debugTasteFoods(3);
    g.debugSetFavouriteFood("spam");
    g.debugSetBestDarkWebDepth(5);
    g.debugGrantMove("buffer_overflow");
    CHECK(g.debugDepthMultiplier() == 2);

    enterArchNewEgg(g);
    pickFirstEggLine(g);
    const Pet& p = g.petRecord();
    CHECK(g.inEggPhase() && p.line);
    CHECK(p.generation == 2 && g.rackCount() == 1);
    CHECK(p.level == 0 && p.xp == 0);
    for (int i = 0; i < kLevelStatCount; ++i) CHECK(p.statPoints[i] == 0);
    CHECK(!p.moves.owns("buffer_overflow"));
    CHECK(p.moves.owned() == MoveLoadout::startingForLine(g.content(), p.line->id).owned());
    for (int i = 0; i < kModSlots; ++i) CHECK(p.mods.equipped(i) == nullptr);
    CHECK(p.upgrades.bandwidthRegenMin == 0 && p.upgrades.xpRatePct == 0);
    CHECK(!p.upgrades.favouriteFood && !p.upgrades.favouriteFound);
    CHECK(p.foodsEaten.empty());
    CHECK(!p.grants.shieldActive && !p.grants.shieldSpent && !p.grants.yubiSpent);
    CHECK(p.usb.branch == BranchOverride::None && p.usb.soakFactor == 1 && !p.usb.hold);
    CHECK(!p.armed.trojanDivert && p.armed.backupShieldUntilMs == 0);
    CHECK(p.armed.deepWebDepthMultiplier == 1 && !p.armed.lockoutActive);
    CHECK(p.bestDeepWebDepth == 0 && p.bestDarkWebDepth == 0);
    CHECK(p.defragCount == 0 && p.dyingElapsedMs == 0);
}

// A v75 blob with every Operator field set, written by the build before SaveData grouped
// them into SaveOperator. Kept as bytes so the gate below reads what that build wrote.
static const char* const kOperatorV75Blob =
    "4d414c314b007061797075700000000000000000000000000000000000000000"
    "000000000000000000000000000000000000000000000000000000697a00005a"
    "455230434f4f4c000000000006120f00e1100000060000000000010066697265"
    "77616c6c5f706174636800000000000000000000030000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "0000000004000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "000000000000000000000000000000004d000000040000000000000000000000"
    "00000000000000000000ffffffff040000000000000000000000000000000000"
    "0000000000000300000000000000000000000100030000000700000000000000"
    "02030101040000000000020070686973686c6574000000000000000000000000"
    "000000006b616c69636f0000000000000000000000000000000000000f0f0101"
    "050000000000090000000b0000000d0000000000000000000001050100000b00"
    "0100000002000300000004000100010001000100010000000000000000007368"
    "656c6c5f736d61736865727300000000000000000000f6e5d4c3b2a10000484f"
    "4d45424153450000000000000000000000000000000000000000000000000000"
    "0000000300030010030001001015000000000000000000000000000800000002"
    "0042500200737461636b65720000000000000000000000000000000000667261"
    "6767657200000000000000000000000000000000000a00000004000000060000"
    "000100000003001b000300000000000005000501000080000000000000000200"
    "840300007800000002000000050000000c000000000000000000000000000000"
    "0000000000000000000003000000000000000000010000020000000000000000"
    "0000000000000000000000000000000000000000000000000100070400110100"
    "0000010202000008000100000000000000000000000000000000000000000000"
    "00000000000000000000000000000000000000";

static std::vector<uint8_t> blobFromHex(const char* hex) {
    std::vector<uint8_t> out;
    for (const char* p = hex; p[0] && p[1]; p += 2)
        out.push_back(static_cast<uint8_t>(std::stoi(std::string(p, 2), nullptr, 16)));
    return out;
}

static std::vector<uint8_t> operatorV75Blob() { return blobFromHex(kOperatorV75Blob); }

// The v75 blob reads into SaveData::op field by field, and writes back with the same bytes
// up to its end: the fields moved in SaveData, not on the wire.
void test_save_operator_reads_from_a_v75_blob() {
    const std::vector<uint8_t> blob = operatorV75Blob();
    CHECK(blob.size() > 6 && blob[4] == 75 && blob[5] == 0);
    SaveData d;
    CHECK(deserializeSave(blob, d));
    const SaveOperator& o = d.op;
    CHECK(o.petsRaised == 6 && o.lifetimeUptimeMs == 987654u && o.lifetimeSteps == 4321u);
    CHECK(o.bits == 31337 && std::strcmp(o.hackerTag, "ZER0COOL") == 0);
    CHECK(std::strcmp(o.crewId, "shell_smashers") == 0);
    CHECK(o.homeNetworkKey == 0xA1B2C3D4E5F6ull && std::strcmp(o.homeNetworkName, "HOMEBASE") == 0);
    CHECK(o.networksSeen == 77 && o.hackerRank == 4);
    CHECK(o.ownedMods.size() == 1 && std::strcmp(o.ownedMods[0].id, "firewall_patch") == 0);
    CHECK(o.ownedModReqLevels.size() == 1 && o.ownedModReqLevels[0] == 3);
    CHECK(o.hasModCountData && o.hasPermanentModData);
    CHECK(saveModCount(o.ownedModCounts, 0) == 2 && saveModCount(o.ownedModCounts, 1) == 4);
    CHECK(saveModCount(o.ownedModCounts, 3) == 5);
    CHECK(o.bwUpgradeCount == 7 && o.fragAmountTier == 2 && o.fragTriggerTier == 3);
    CHECK(o.itemTabsUnlocked == 1 && o.bulkOpenUnlocked == 1);
    CHECK(o.rackSlotUpgradeCount == 9 && o.scrapingClusterLevel == 11 && o.dataMiningLevel == 13);
    CHECK(o.mergeHubUnlocked == 1 && o.recipesUnlocked == 0x105u);
    CHECK(o.recipeOwned.size() == 5 && o.recipeOwned[0] == 0x05 && o.recipeOwned[4] == 0x80);
    CHECK(o.rigLevelsExt.size() == 11 && o.rigLevelsExt[2] == 2 && o.rigLevelsExt[5] == 4);
    CHECK(o.rigServicesOff == (1u << 9) && o.rigServicesAsk == (1u << 19));
    CHECK(o.seenCreatures.size() == 2 && std::strcmp(o.seenCreatures[1].id, "kalico") == 0);
    CHECK(o.malbeastSeen == 0x0F0F && o.malbeastDefeated == 0x0101);
    CHECK(o.achievementsMask == 0x5u);
    CHECK(o.achievementEarned.size() == 3 && o.achievementEarned[2] == 0x10);
    CHECK(o.achievementNotified.size() == 3 && o.achievementNotified[0] == 0x01);
    CHECK(o.storyRead.size() == 1 && o.storyRead[0] == 0x07);
    CHECK(o.tipsSeen.size() == 4 && o.tipsSeen[0] == 0x11 && o.tipsSeen[1] == 0x01);
    CHECK(o.bossWins == 21 && o.stackerWins == 8 && o.tourneyWins == 2);
    CHECK(o.pvpWins == 5 && o.mergesCooked == 12);
    CHECK(o.arcadeIds.size() == 2 && std::strcmp(o.arcadeIds[1].id, "fragger") == 0);
    CHECK(o.arcadePlays[0] == 10 && o.arcadeWins[0] == 6 && o.arcadeBest[0] == 900);
    CHECK(o.arcadePlays[1] == 4 && o.arcadeWins[1] == 1 && o.arcadeBest[1] == 120);
    CHECK(o.quoteStates.size() == 3 && o.quoteStates[0] == 0x1B);
    CHECK(o.backgroundPick == 3);

    // Past the version word, a newer writer only ever appends.
    const std::vector<uint8_t> again = serializeSave(d);
    CHECK(again.size() >= blob.size());
    CHECK(std::equal(blob.begin() + 6, blob.end(), again.begin() + 6));
}

// ...and boots into the Operator record through Operator::loaded.
void test_save_operator_loads_from_a_v75_blob() {
    MemSaveStore store;
    store.save(operatorV75Blob());
    Game g(StartMode::FreshHatch, "malbear", &store);
    const Operator& o = g.operatorRecord();
    const ContentRegistry& reg = g.content();
    CHECK(std::strcmp(o.hackerTag, "ZER0COOL") == 0 && o.bits == 31337);
    CHECK(o.petsRaised == 6 && o.uptimeBase == 987654u && o.lifetimeSteps == 4321u);
    CHECK(o.networksSeen == 77 && o.hackerRank == 4);
    CHECK(o.crewIndex == crewIndexById("shell_smashers") && o.crewIndex >= 0);
    CHECK(o.homeNetworkKey == 0xA1B2C3D4E5F6ull && std::strcmp(o.homeNetworkName, "HOMEBASE") == 0);
    // Wire 0 names no mod in this build, so its copies have nowhere to land.
    CHECK(reg.modByWire(0) == nullptr && o.modPool.owned().size() == 2);
    CHECK(o.modPool.countOf(reg.modByWire(1)->id) == 4);
    CHECK(o.modPool.countOf(reg.modByWire(3)->id) == 5);
    CHECK(o.rig.level[kRigRowBandwidth] == 7 && o.rig.level[kRigRowFragReduce] == 2);
    CHECK(o.rig.level[kRigRowFragTrigger] == 3 && o.rig.owned(kRigRowItemTabs));
    CHECK(o.rig.owned(kRigRowBulkOpen) && o.rig.level[kRigRowRackSlot] == 9);
    CHECK(o.rig.level[kRigRowScraping] == 11 && o.rig.level[kRigRowDataMining] == 13);
    CHECK(o.rig.owned(kRigRowMergeHub) && o.rig.level[kRigRowDiskMaintenance] == 2);
    CHECK(o.rig.level[kRigRowAutoDiveGear] == 1);
    CHECK(o.rig.serviceOwned(kRigRowAutoBackup) && !o.rig.featureActive(kRigRowAutoBackup));
    CHECK(o.rig.serviceMode(kRigRowAutoDiveGear) == ServiceMode::Ask);
    CHECK(o.rig.recipesOwned[0] == 0x05 && o.rig.recipesOwned[4] == 0x80);
    CHECK(o.reveals.seenCreatures.size() == 2);
    CHECK(o.reveals.malbeastSeenMask == 0x0F0F && o.reveals.malbeastDefeatedMask == 0x0101);
    CHECK(o.reveals.achEarned[0] == 0x03 && o.reveals.achEarned[2] == 0x10);
    CHECK(o.reveals.achNotified[0] == 0x01 && o.reveals.storyRead[0] == 0x07);
    CHECK(o.reveals.tipsSeen[0] == 0x11 && o.reveals.tipsSeen[1] == 0x01);
    CHECK(o.tallies.bossWins == 21 && o.tallies.stackerWins == 8);
    CHECK(o.tallies.tourneyWins == 2 && o.tallies.pvpWins == 5 && o.tallies.mergesCooked == 12);
    const int stacker = arcadeGameIndexById("stacker");
    const int fragger = arcadeGameIndexById("fragger");
    CHECK(o.tallies.arcadePlays[stacker] == 10 && o.tallies.arcadeBest[stacker] == 900);
    CHECK(o.tallies.arcadeWins[fragger] == 1 && o.tallies.arcadeBest[fragger] == 120);
    CHECK(o.tallies.quoteStates[0] == 0x1B && o.tallies.quoteStates[2] == 0x03);
}

// A v75 blob from the same build with the rest of the player-level fields set: the ARCH
// rack and records, the SHAKES count, world progress, the shelves and the Cant. Its
// bossWins is 0, so loading seeds it from the clears.
static const char* const kOperatorWorldV75Blob =
    "4d414c314b007061797075700000000000000000000000000000000000003200"
    "0000000000003200000000000000000000000000000000030000000000000000"
    "0000000000000000000000000000000000000000000000000000000003000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000020070686973686c65740000000000000000000000000000"
    "00003c0000000a00000046000000010000000000000000010000006b616c6963"
    "6f00000000000000000000000000000000000028000000000000000000000000"
    "0000000000000000020000000000040000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "00000000000000000000000000000000000000000000000002006d616c626561"
    "7200000000000000000000000000000000000101000000627275696e666f7263"
    "6500000000000000000000000000000002000000000000000000000000000900"
    "0000000004000100010000000000050000000200000004000000000000000000"
    "000000000000000004000101000004001f051f0004001f0a1f00030000000000"
    "0000000002000000000000000000000000000000000000000000000004000000"
    "0000000000000000000000000200040000000000000004000000000000000000"
    "0000000000000000000002000000000000000000000002000000000000000000"
    "0000000000000000000000000000000000000002000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000020000000000000000000000"
    "0000020006000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000100636f6e6b697474656e617465000000"
    "0000000000000000000000000000000000020064796e6f5f6e75676765747300"
    "0000000000000000000000726f6c6c6261636b00000000000000000000000000"
    "000000020070686973686c657400000000000000000000000000000000636f6e"
    "6b697474656e6174650000000000000000000000000c00000005000000000000"
    "0002000000000000000000000000000000000000000000000002000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000020000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000015000000040000"
    "0000010000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000010202000000000100000000000000000000000000"
    "0000000000000000000000000200000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "0000020000000000000000000000020000000000000000000000000002000000"
    "0000000000000100000000000000000000000000010000000000";

// It reads into SaveData::op field by field, and writes back with the same bytes up to
// its end.
void test_save_operator_world_reads_from_a_v75_blob() {
    const std::vector<uint8_t> blob = blobFromHex(kOperatorWorldV75Blob);
    CHECK(blob.size() > 6 && blob[4] == 75 && blob[5] == 0);
    SaveData d;
    CHECK(deserializeSave(blob, d));
    const SaveOperator& o = d.op;
    CHECK(o.rack.size() == 2 && std::strcmp(o.rack[0].id, "phishlet") == 0);
    CHECK(o.rack[0].combatLevel == 4 && o.rack[0].bestDeepWebDepth == 6);
    CHECK(std::strcmp(o.rack[1].id, "kalico") == 0 && o.rack[1].statPoints[1] == 2);
    CHECK(o.records.size() == 2 && std::strcmp(o.records[0].id, "malbear") == 0);
    CHECK(o.records[0].status == 1 && o.records[1].generation == 2);
    CHECK(o.handshakesSeen == 9);
    CHECK(o.sectorCleared.size() == 4 && o.sectorCleared[0] == 1 && o.sectorCleared[2] == 1);
    CHECK(o.titlesUnlocked == 0x5u && o.equippedTitle == 2);
    CHECK(o.bossUnlocked.size() == 4 && o.bossUnlocked[1] == 1);
    CHECK(o.subCleared.size() == 4 && o.subCleared[1] == 0x05);
    CHECK(o.subBossUnlocked.size() == 4 && o.subBossUnlocked[1] == 0x0A);
    CHECK(o.raisedCreatures.size() == 1 &&
          std::strcmp(o.raisedCreatures[0].id, "conkittenate") == 0);
    CHECK(o.collectedItems.size() == 2 && std::strcmp(o.collectedItems[1].id, "rollback") == 0);
    CHECK(o.speciesDiveIds.size() == 2 && std::strcmp(o.speciesDiveIds[0].id, "phishlet") == 0);
    CHECK(o.speciesDiveDepths.size() == 2 && o.speciesDiveDepths[0] == 12);
    CHECK(o.cantSigils == 0x15u && o.shakesSpent == 4);

    const std::vector<uint8_t> again = serializeSave(d);
    CHECK(again.size() >= blob.size());
    CHECK(std::equal(blob.begin() + 6, blob.end(), again.begin() + 6));
}

// ...and boots into the record's parts, with the steps that read outside the record
// (the rack and records folded into the raised species, the active pet tallied) applied.
void test_save_operator_world_loads_from_a_v75_blob() {
    MemSaveStore store;
    store.save(blobFromHex(kOperatorWorldV75Blob));
    Game g(StartMode::FreshHatch, "malbear", &store);
    const Operator& o = g.operatorRecord();
    const ContentRegistry& reg = g.content();
    CHECK(o.world.sectorCleared[0] && !o.world.sectorCleared[1] && o.world.sectorCleared[2]);
    CHECK(o.world.subCleared[1][0] && !o.world.subCleared[1][1] && o.world.subCleared[1][2]);
    CHECK(o.world.subBossUnlocked[1][1] && o.world.subBossUnlocked[1][3]);
    CHECK(!o.world.subBossUnlocked[1][0]);
    CHECK(o.world.titlesUnlocked == 0x5u && o.world.equippedTitle == 2);
    // Two cleared areas and twelve cleared sub-areas.
    CHECK(o.tallies.bossWins == 14);
    CHECK(o.shelves.collectedItems.size() == 2 && o.shelves.itemCollected("rollback"));
    CHECK(o.shelves.deepestDive("phishlet") == 12 && o.shelves.deepestDive("conkittenate") == 5);
    const char* raised[] = {"conkittenate", "phishlet", "kalico", "malbear", "bruinforce",
                            "paypup"};
    CHECK(o.shelves.raisedCreatures.size() == 6);
    for (size_t i = 0; i < 6 && i < o.shelves.raisedCreatures.size(); ++i)
        CHECK(o.shelves.raisedCreatures[i] == reg.creature(raised[i]));
    CHECK(o.cant.sigils == 0x15u && o.cant.handshakesSeen == 9 && o.cant.shakesSpent == 4);
    CHECK(g.shakesUnspent() == 5);
    CHECK(o.arch.rack.size() == 2 && std::strcmp(o.arch.rack[1].id, "kalico") == 0);
    CHECK(o.arch.records.size() == 2 && std::strcmp(o.arch.records[1].id, "bruinforce") == 0);
}

// An Operator with something in every part maps to the save and back unchanged.
void test_save_operator_round_trips_through_its_mapping() {
    Game g{StartMode::Hatched, "paypup"};
    const ContentRegistry& reg = g.content();
    Operator o;
    std::strcpy(o.hackerTag, "R00TKIT");
    o.networksSeen = 12;
    o.hackerRank = 2;
    o.homeNetworkKey = 0x0102030405ull;
    std::strcpy(o.homeNetworkName, "LAN");
    o.crewIndex = crewIndexById("shell_smashers");
    o.petsRaised = 3;
    o.uptimeBase = 5000;
    o.lifetimeSteps = 77;
    o.bits = 4242;
    o.modPool.grant(reg.modByWire(1)->id, kModCopyCapBase);
    for (int i = 0; i < kRigUpgradeCount; ++i) o.rig.level[i] = 1 + (i % 3);
    o.rig.servicesOff = 1u << kRigRowContinuousBackup;
    o.rig.servicesAsk = 1u << kRigRowAutoDiveGear;
    o.rig.recipesOwned[1] = 0x42;
    o.reveals.seenCreatures.push_back(reg.creature("kalico"));
    o.reveals.malbeastSeenMask = 0x30;
    o.reveals.malbeastDefeatedMask = 0x10;
    o.reveals.achEarned[1] = 0x81;
    o.reveals.achNotified[1] = 0x01;
    o.reveals.storyRead[0] = 0x03;
    o.reveals.tipsSeen[1] = 0x02;
    o.tallies.bossWins = 9;
    o.tallies.stackerWins = 4;
    o.tallies.tourneyWins = 1;
    o.tallies.pvpWins = 6;
    o.tallies.mergesCooked = 2;
    o.tallies.arcadePlays[1] = 3;
    o.tallies.arcadeWins[1] = 2;
    o.tallies.arcadeBest[1] = 55;
    o.tallies.quoteStates[0] = 0x0C;
    o.backgroundPick = kBackgrounds[1].scene;
    o.world.sectorCleared[1] = true;
    o.world.subCleared[2][3] = true;
    o.world.subBossUnlocked[2][4] = true;
    o.world.titlesUnlocked = 0x2;
    o.world.equippedTitle = 1;
    o.shelves.collectedItems.push_back(reg.item("rollback"));
    o.shelves.speciesDives.push_back({reg.creature("phishlet"), 8});
    o.shelves.raisedCreatures.push_back(reg.creature("malbear"));
    o.cant.sigils = 0x3;
    o.cant.handshakesSeen = 7;
    o.cant.shakesSpent = 2;
    SaveStoredPet stored;
    std::strcpy(stored.id, "kalico");
    stored.combatLevel = 3;
    o.arch.rack.push_back(stored);
    SaveRecord record;
    std::strcpy(record.id, "paypup");
    record.status = static_cast<uint8_t>(RecordStatus::Corrupted);
    o.arch.records.push_back(record);

    SaveData x;
    x.op = o.saved(reg, 0);
    SaveData y;
    CHECK(deserializeSave(serializeSave(x), y));
    SaveData z;
    z.op = Operator::loaded(y.op, y.active.equippedMods, reg).saved(reg, 0);
    CHECK(serializeSave(x) == serializeSave(z));
}

// A v75 blob with every device setting set, written by the build before SaveData grouped
// them into SaveSettings. Its theme is a chip set, and the blob's collected shelf holds
// that chip, so the theme is unlocked when it loads.
static const char* const kSettingsV75Blob =
    "4d414c314b007061797075700000000000000000000000000000000000003200"
    "0000000000000000000000000000000000000000000000010000000000000000"
    "0000000000000000000000000000000000000000000000000000000003000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000400000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "000000000000000000000000000000000000ffffffff04000000000000000000"
    "0000000000000000000000000000010000000000000000000000000000000000"
    "0000000000000000040000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000100616d6265725f747562650000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "000000000000000000000000000000000000010000000000000000000000616d"
    "6265720000000000000000000000000000000000000000000000020001040000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "000000000000000000000000";

// It reads into SaveData::settings field by field, and writes back with the same bytes
// up to its end.
void test_save_settings_reads_from_a_v75_blob() {
    const std::vector<uint8_t> blob = blobFromHex(kSettingsV75Blob);
    CHECK(blob.size() > 6 && blob[4] == 75 && blob[5] == 0);
    SaveData d;
    CHECK(deserializeSave(blob, d));
    const SaveSettings& s = d.settings;
    CHECK(s.brightness == 1 && std::strcmp(s.theme, "amber") == 0);
    CHECK(s.uiMode == 2 && s.careAlerts == 0);
    CHECK(s.soundMode == 1 && s.volume == 4 && s.exploreSound == 0);
    const std::vector<uint8_t> again = serializeSave(d);
    CHECK(again.size() >= blob.size());
    CHECK(std::equal(blob.begin() + 6, blob.end(), again.begin() + 6));
}

// ...and boots into the Settings record, with the theme applied.
void test_save_settings_loads_from_a_v75_blob() {
    MemSaveStore store;
    store.save(blobFromHex(kSettingsV75Blob));
    Game g(StartMode::FreshHatch, "malbear", &store);
    const Settings& s = g.settingsRecord();
    CHECK(s.brightness == 1 && s.uiMode == UiMode::TextOnly && !s.careAlerts);
    CHECK(s.soundMode == SoundMode::AlertsOnly && s.volume == 4 && !s.exploreSound);
    CHECK(s.theme == palThemeByName("amber") && palThemeIndex() == s.theme);
    setPalTheme(0);
}

// Settings with every field off its default map to the save and back unchanged, and a
// value from a corrupt or newer blob reads as one this build has.
void test_save_settings_round_trip_and_clamp() {
    Settings v;
    v.setBrightness(0);
    v.setUiMode(UiMode::IconsOnly);
    v.setCareAlerts(false);
    v.setSoundMode(SoundMode::All);
    v.setVolume(1);
    v.setExploreSound(false);
    v.theme = palThemeByName("high-contrast");
    SaveData x;
    x.settings = v.saved();
    SaveData y;
    CHECK(deserializeSave(serializeSave(x), y));
    SaveData z;
    z.settings = Settings::loaded(y.settings).saved();
    CHECK(serializeSave(x) == serializeSave(z));

    SaveSettings bad;
    bad.brightness = 99;
    bad.uiMode = 7;
    bad.soundMode = 9;
    bad.volume = 200;
    std::strcpy(bad.theme, "no-such-set");
    const Settings r = Settings::loaded(bad);
    CHECK(r.brightness == kBrightnessLevels - 1 && r.uiMode == UiMode::IconsLabel);
    CHECK(r.soundMode == SoundMode::Off && r.volume == kVolumeLevels - 1 && r.theme == 0);
    Settings c;
    CHECK(!c.setVolume(kVolumeDefault) && c.setVolume(-3) && c.volume == 0);
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

// A plain reboot (a fresh Game over the same store: the ARCH rack never enters
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
      // The wipe replaced the raised pet with a freshly-laid egg (persisted, v9):
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

    enterArchNewEgg(g);                                    // ARCH's NEW EGG row
    pickFirstEggLine(g);                                   // vacated -> line-select; pick Ransomware
    CHECK(g.nav() == Game::Nav::Idle);                     // active vacated -> new egg at idle
    CHECK(g.rackCount() == 1 && g.pet() && g.inEggPhase());
    CHECK(std::strcmp(g.pet()->id, "cryptoshell") == 0);
    CHECK(g.generation() == 2);                            // lifetime ordinal advanced

    enterArchStoredPet(g, "paypup");
    archConfirmAction(g);                                  // Deploy -> confirm -> commit
    CHECK(g.nav() == Game::Nav::Idle);
    CHECK(g.pet() && std::strcmp(g.pet()->id, "paypup") == 0);   // Paypup is active again
    CHECK(g.generation() == 1);                            // its own generation rode along
    CHECK(g.rackCount() == 1);                             // slot-neutral: CryptoShell stored
}

// ARCH is a PICKER over groups, not one flat list: NEW EGG first, then ACTIVE, then one
// row per creature family, then RECORDS. The shelf holds 64 now, so which family a pet is
// on is the axis a player who keeps one of everything actually navigates by.
void test_arch_picker_groups_the_rack_by_line() {
    Game g{StartMode::Hatched, "paypup"};        // ransomware
    g.debugSeedRack("tadpoll");                  // phishing
    g.debugSeedRack("cryptoshell");              // ransomware

    enterSubmenuId(g, SubmenuId::Arch);
    CHECK(g.archScreen() == Game::ArchScreen::Picker);
    const auto tiles = buildArchPickerRows(ContentRegistry::embedded(), g.pet(),
                                           g.rack(), g.records());
    // NEW EGG leads, ACTIVE follows, SELL and RECORDS trail: the families sit between.
    CHECK(tiles.size() == static_cast<size_t>(4 + kCreatureLineCount));
    CHECK(tiles.front().group.kind == ArchGroup::Kind::NewEgg);
    CHECK(tiles[1].group.kind == ArchGroup::Kind::Active && tiles[1].count == 1);
    CHECK(tiles.back().group.kind == ArchGroup::Kind::Records);
    // Two ransomware pets are on the shelf but only ONE of them is stored: the active
    // one belongs to ACTIVE, which is the split the whole picker is for.
    int ransom = 0, phish = 0;
    for (const ArchPickRow& t : tiles) {
        if (t.group.kind != ArchGroup::Kind::Line) continue;
        if (std::strcmp(t.label, "RANSOMWARE") == 0) ransom = t.count;
        if (std::strcmp(t.label, "PHISHING") == 0) phish = t.count;
    }
    CHECK(ransom == 1 && phish == 1);

    // And the rows behind a family are that family's, resolved to real creatures.
    enterArchStoredPet(g, "tadpoll");
    CHECK(g.nav() == Game::Nav::Detail);
    CHECK(g.archFocusedPetId() && std::strcmp(g.archFocusedPetId(), "tadpoll") == 0);
    CHECK(g.archGroup().kind == ArchGroup::Kind::Line);
}

// NEW EGG is a ROW on the picker, not an action buried in the active pet's record. It
// still costs the same thing it always did (the pet you are raising goes to the rack),
// which is what its own L3 says before anything is committed.
void test_arch_new_egg_row_stores_then_hatches() {
    Game g{StartMode::Hatched, "paypup"};
    CHECK(g.rackCount() == 0);
    enterSubmenuId(g, SubmenuId::Arch);
    CHECK(g.archPickRow() == 0);                  // it opens ON the row
    g.onButton(press(Button::B));                 // -> the NEW EGG confirm screen
    CHECK(g.nav() == Game::Nav::Detail);
    CHECK(g.archGroup().kind == ArchGroup::Kind::NewEgg);
    Framebuffer fb(kActiveW, kActiveH);
    g.render(fb);
    CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));   // it reads in grayscale

    // Cancel keeps the pet exactly where it was: a confirm that defaults to Cancel is
    // the whole reason this is a screen and not a button.
    g.onButton(press(Button::B));                 // -> confirm prompt (default Cancel)
    g.onButton(press(Button::B));                 // commit Cancel
    CHECK(g.rackCount() == 0 && g.pet() && !g.inEggPhase());

    g.onButton(press(Button::B));                 // -> confirm again
    g.onButton(press(Button::A));                 // Cancel -> Confirm
    g.onButton(press(Button::B));                 // commit
    pickFirstEggLine(g);
    CHECK(g.rackCount() == 1 && g.inEggPhase());
    CHECK(std::strcmp(g.rack()[0].id, "paypup") == 0);   // set aside, not lost
}

// A Daemon's sale price: flat to par, so no level gates a sale, then the Rig Shop's
// kLogStep shape over the levels past it, and never lower for a higher level.
void test_daemon_sale_bits_flat_to_par_then_log_steps() {
    CHECK(daemonSaleBits(0) == kDaemonSaleBits);
    CHECK(daemonSaleBits(kDaemonSaleParLevel) == kDaemonSaleBits);
    CHECK(daemonSaleBits(kDaemonSaleParLevel + 1) == kDaemonSaleBits);
    CHECK(daemonSaleBits(kDaemonSaleParLevel + 2) == 2 * kDaemonSaleBits);
    CHECK(daemonSaleBits(kDaemonSaleParLevel + 8) == 8 * kDaemonSaleBits);
    CHECK(daemonSaleBits(kDaemonSaleParLevel + 40) == 32 * kDaemonSaleBits);
    for (int l = 1; l <= kModEquipLevelMax; ++l)
        CHECK(daemonSaleBits(l) >= daemonSaleBits(l - 1));
}

// SELL is its own row on the picker, just above the RECORDS it feeds, and it lists every
// STORED Daemon whatever its family, not the earlier stages, and not the active pet.
void test_arch_sell_counter_lists_only_stored_daemons() {
    Game g{StartMode::Hatched, "extorgi"};
    g.debugSeedRack("paypup");
    g.debugSeedRack("goliauth");
    g.debugSeedRack("coaxeel");
    const auto tiles = buildArchPickerRows(ContentRegistry::embedded(), g.pet(), g.rack(),
                                           g.records());
    CHECK(tiles.size() == static_cast<size_t>(4 + kCreatureLineCount));
    CHECK(tiles[tiles.size() - 2].group.kind == ArchGroup::Kind::Sell);
    CHECK(tiles[tiles.size() - 2].count == 2);

    enterArchSellList(g);
    CHECK(g.archScreen() == Game::ArchScreen::List && g.archRowCount() == 2);
    Framebuffer fb(kActiveW, kActiveH);
    g.render(fb);
    CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));
}

// The Bits sale: priced on the stored pet's level, and the Daemon leaves a RETIRED
// record behind. With no twin active and nothing to fix, the patches are shown closed.
void test_arch_sell_for_bits_leaves_a_retired_record() {
    Game g{StartMode::Hatched, "paypup"};
    g.debugSeedRack("goliauth", kDaemonSaleParLevel + 8);
    const int before = g.bits();
    enterArchSale(g, "goliauth");
    CHECK(g.nav() == Game::Nav::Detail);
    const ArchSaleSheet sh = g.archSaleSheet();
    CHECK(sh.daemon && std::strcmp(sh.daemon->id, "goliauth") == 0);
    CHECK(sh.focus == SaleOffer::Bits && sh.offers[0].available);
    CHECK(!sh.offers[static_cast<int>(SaleOffer::Reimage)].available);
    CHECK(!sh.offers[static_cast<int>(SaleOffer::Hotfix)].available);
    Framebuffer fb(kActiveW, kActiveH);
    g.render(fb);
    CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));

    archConfirmAction(g);
    CHECK(g.bits() == before + 8 * kDaemonSaleBits);
    CHECK(g.rackCount() == 0);
    CHECK(g.records().size() == 1);
    CHECK(std::strcmp(g.records()[0].id, "goliauth") == 0);
    CHECK(g.records()[0].status == static_cast<uint8_t>(RecordStatus::Retired));
    CHECK(g.nav() == Game::Nav::Submenu);         // back at the (now empty) counter
}

// A closed offer is on the A cycle, so its row can say what it wants, but B on it opens
// no confirm and sells nothing.
void test_arch_sell_closed_offer_cannot_be_taken() {
    Game g{StartMode::Hatched, "paypup"};
    g.debugSeedRack("goliauth");
    enterArchSale(g, "goliauth");
    for (int i = 0; i < 2; ++i) {
        g.onButton(press(Button::A));             // -> Reimage, then Hotfix
        g.onButton(press(Button::B));
        CHECK(!g.archSaleSheet().confirmOpen);
    }
    g.onButton(press(Button::A));                 // wraps to Bits
    CHECK(g.archSaleSheet().focus == SaleOffer::Bits);
    CHECK(g.rackCount() == 1 && g.records().empty());
}

// HOTFIX spends the Daemon on the active pet's care errors, whatever species it is.
void test_arch_sell_hotfix_clears_the_active_pets_errors() {
    Game g{StartMode::Hatched, "paypup"};
    g.model().setCareMistakes(4);
    g.debugSeedRack("goliauth");
    const int bits = g.bits();
    enterArchSale(g, "goliauth");
    g.onButton(press(Button::A));
    g.onButton(press(Button::A));
    CHECK(g.archSaleSheet().focus == SaleOffer::Hotfix);
    archConfirmAction(g);
    CHECK(g.model().careMistakes() == 0);
    CHECK(g.bits() == bits);                      // one payout per sale
    CHECK(g.rackCount() == 0 && g.records().size() == 1);
}

// REIMAGE: a second Extorgi sends the active one back to Barkmail with a clean error
// log and everything else it earned, so the Script->Daemon branch can be raised again.
void test_arch_sell_reimages_an_active_twin_back_to_its_script() {
    Game g{StartMode::Hatched, "extorgi"};
    g.debugAddCombatXp(1200);
    const int lvl = g.combatLevel();
    CHECK(lvl >= 1);
    g.model().setCareMistakes(3);
    g.debugSeedRack("extorgi");
    enterArchSale(g, "extorgi");
    g.onButton(press(Button::A));
    CHECK(g.archSaleSheet().focus == SaleOffer::Reimage);
    CHECK(g.archSaleSheet().offers[static_cast<int>(SaleOffer::Reimage)].available);
    archConfirmAction(g);

    CHECK(g.pet() && std::strcmp(g.pet()->id, "barkmail") == 0);
    CHECK(g.model().careMistakes() == 0);
    CHECK(g.combatLevel() == lvl);
    CHECK(g.moveLoadout().equipped(MoveLoadout::slotsForStage(Stage::Script)) == nullptr);
    CHECK(g.slotKind(MoveLoadout::slotsForStage(Stage::Script)) == Game::SlotKind::Unset);
    CHECK(g.hasNextEvolution());
    CHECK(g.rackCount() == 0 && g.records().size() == 1);
    CHECK(g.nav() == Game::Nav::Idle);
}

// A Daemon reached by a Trojan divert reimages to the Script it was diverted OFF, which
// hands it back its old family as well as its branch.
void test_arch_reimage_sends_a_diverted_daemon_home() {
    Game g{StartMode::Hatched, "coaxeel"};
    g.debugSeedRack("coaxeel");
    enterArchSale(g, "coaxeel");
    g.onButton(press(Button::A));
    archConfirmAction(g);
    CHECK(g.pet() && std::strcmp(g.pet()->id, "rootgrub") == 0);
    CHECK(std::strcmp(g.pet()->line, "worm") == 0);
}

// Laying an egg used to LOCK YOU IN: line-select disabled C because "an empty save has no
// pet to return to". Reaching it from an ARCH Store makes that false (the pet is on the
// shelf), so C now backs out to ARCH, where deploying it back is one confirm away.
void test_line_select_backs_out_to_arch_when_the_rack_has_a_pet() {
    Game g{StartMode::Hatched, "paypup"};
    // Two unlocked lines, so the hatch actually STOPS on the choice: with one line
    // startHatch skips the modal outright and there is nothing to cancel out of.
    g.unlockAchievement(ach::kDeepWebDepth8);     // unlocks Phishing
    enterArchNewEgg(g);
    CHECK(g.inLineSelect() && g.rackCount() == 1 && !g.pet());

    tapC(g);                                      // back out: no egg laid
    CHECK(!g.inLineSelect());
    CHECK(g.nav() == Game::Nav::Submenu && g.archScreen() == Game::ArchScreen::Picker);
    CHECK(g.rackCount() == 1 && !g.pet());        // still parked between the two rooms

    // C from the picker returns to the choice rather than dropping the player into a
    // habitat with nothing living in it: the two rooms are the only two rooms.
    tapC(g);
    CHECK(g.inLineSelect());
    tapC(g);
    CHECK(g.nav() == Game::Nav::Submenu);

    // Deploying with no active pet is a plain thaw: the pet comes back and the slot is
    // freed, rather than the swap a Deploy normally is. Walked from the picker the gate
    // is already standing on: backing out to the carousel is exactly what this state
    // does not offer.
    archOpenStoredPet(g, "paypup");
    archConfirmAction(g);
    CHECK(g.pet() && std::strcmp(g.pet()->id, "paypup") == 0);
    CHECK(g.rackCount() == 0);                    // freed, not left holding a copy
}

// ...and with an EMPTY rack the old rule stands exactly as it was: there is nowhere to go
// back to, so the choice can't be cancelled.
void test_line_select_cannot_be_cancelled_with_an_empty_rack() {
    Game g;                                       // fresh save
    g.unlockAchievement(ach::kDeepWebDepth8);     // a real choice to be stuck on
    g.resetToHatch();
    CHECK(g.inLineSelect() && g.rackCount() == 0);
    tapC(g);
    CHECK(g.inLineSelect());                      // C is still inert here
    pickFirstEggLine(g);
    CHECK(g.pet() != nullptr);
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
    g.render(fb); CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));   // the group picker
    enterArchStoredPet(g, "cryptoshell");                  // its record (Deploy)
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
// always in the air, and a swimmer is neither pulled down nor holding a height.
// It drifts on both axes at once, and then holds station.
//
// That last clause is load-bearing rather than descriptive. The habitat swaps a
// creature's "walk" clip in for as long as the wander is travelling, so a mover that
// retargets the beat it arrives never shows its idle at all, whatever is authored on
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
// also gives up the 2px shelf bob the habitat draws on top of the drift, which is
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
// new occupant itself, otherwise a walker that evolved into a swimmer would keep
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

// The worm flourish (core/model/idle_fork.h): rare, the extras arrive one at a time,
// and all of them die on the same beat. A worm forks and keeps its copies; a line
// without Replication does neither.
void test_idle_fork_runs_whole_and_only_for_replicators() {
    IdleFork f;
    for (int i = 0; i < 1000; ++i) { f.step(false); CHECK(f.stage() == ForkStage::Rest); }

    int rest = 0;
    while (f.stage() == ForkStage::Rest) { f.step(true); ++rest; }
    CHECK(rest == kIdleForkRestBeats);
    int shown = 0;
    while (f.stage() == ForkStage::Spawn) {
        CHECK(f.extras() >= shown && f.extras() <= shown + 1);
        shown = f.extras();
        f.step(true);
    }
    CHECK(shown == kIdleForkExtras);
    CHECK(f.stage() == ForkStage::Look);
    while (f.stage() == ForkStage::Look) f.step(true);
    for (int b = 0; b < kIdleForkDieBeats; ++b) {
        CHECK(f.stage() == ForkStage::Die);
        CHECK(f.extras() == kIdleForkExtras);
        f.step(true);
    }
    CHECK(f.stage() == ForkStage::Rest);
    CHECK(f.extras() == 0);

    Game worm{StartMode::Hatched, "nodeatode"};
    Game other{StartMode::Hatched, "paypup"};
    CHECK(worm.idleCompanionCount() == 1);
    CHECK(other.idleCompanionCount() == 0);
    uint32_t t = 0;
    bool wormForked = false, otherForked = false;
    for (int i = 0; i < kIdleForkRestBeats + 2; ++i) {
        worm.tick(t += kHeartbeatMs);
        other.tick(t);
        wormForked |= worm.petFork().stage() != ForkStage::Rest;
        otherForked |= other.petFork().stage() != ForkStage::Rest;
    }
    CHECK(wormForked);
    CHECK(!otherForked);
}

// The relay (core/model/idle_relay.h): after its rest the byte hops once per copy, in
// order, each copy chomps on the beats right after its catch, and then it rests again.
// No copies, no relay.
void test_idle_relay_hops_in_order_and_chomps_on_catch() {
    IdleRelay none;
    for (int i = 0; i < 500; ++i) { none.step(0); CHECK(none.hop() == -1); }

    IdleRelay r;
    int rest = 0;
    while (r.hop() == -1) { r.step(3); ++rest; }
    CHECK(rest == kIdleRelayRestBeats);
    for (int h = 0; h < 3; ++h) {
        for (int b = 0; b < kIdleRelayHopBeats; ++b) {
            CHECK(r.hop() == h);
            CHECK(r.beatInHop() == b);
            CHECK(r.chomping() == (h > 0 && b < kIdleRelayChompBeats ? h - 1 : -1));
            r.step(3);
        }
    }
    for (int b = 0; b < kIdleRelayChompBeats; ++b) {
        CHECK(r.hop() == -1);
        CHECK(r.chomping() == 2);
        r.step(3);
    }
    CHECK(r.chomping() == -1);
    for (int i = 0; i < kIdleRelayRestBeats - 1; ++i) { r.step(3); CHECK(r.hop() == -1); }
}

// The disguise (core/model/idle_disguise.h): rare; the glitch shows the creature torn,
// then the prop torn; the prop gives itself away with its eyes; the next disguise is a
// different prop. A Trojan holds still while disguised and a line without the flag
// never disguises.
void test_idle_disguise_swaps_tells_and_holds_still() {
    IdleDisguise none;
    for (int i = 0; i < 1000; ++i) { none.step(false); CHECK(!none.showsProp() && !none.torn()); }

    IdleDisguise d;
    int rest = 0;
    while (d.stage() == DisguiseStage::Rest) { d.step(true); ++rest; }
    CHECK(rest == kIdleDisguiseRestBeats);
    const int firstSlot = d.slot();
    CHECK(d.torn() && !d.showsProp());          // the creature tears first...
    d.step(true);
    CHECK(d.torn() && d.showsProp());           // ...then the prop arrives torn
    d.step(true);
    bool eyes = false;
    while (d.stage() == DisguiseStage::Hidden) {
        CHECK(d.showsProp() && !d.torn());
        eyes |= d.propFrame() == 3;
        d.step(true);
    }
    CHECK(eyes);
    CHECK(d.torn() && d.showsProp());
    d.step(true);
    CHECK(d.torn() && !d.showsProp());
    d.step(true);
    CHECK(d.stage() == DisguiseStage::Rest);
    CHECK(d.slot() != firstSlot);

    Game trojan{StartMode::Hatched, "keyloggerhead"};
    Game other{StartMode::Hatched, "paypup"};
    uint32_t t = 0;
    bool trojanHid = false, otherHid = false, movedWhileHidden = false;
    int heldX = 0;
    for (int i = 0; i < kIdleDisguiseRestBeats + kIdleDisguiseHiddenBeats; ++i) {
        trojan.tick(t += kHeartbeatMs);
        other.tick(t);
        otherHid |= other.petDisguise().showsProp();
        if (trojan.petDisguise().stage() == DisguiseStage::Hidden) {
            if (!trojanHid) heldX = trojan.petWander().offsetX();
            trojanHid = true;
            movedWhileHidden |= trojan.petWander().offsetX() != heldX;
        }
    }
    CHECK(trojanHid);
    CHECK(!otherHid);
    CHECK(!movedWhileHidden);
}

// The harvest (core/model/idle_harvest.h): rare, the streams come before the hold, and
// only a line whose resting trick it is ever does it.
void test_idle_harvest_streams_then_holds_for_harvesters_only() {
    IdleHarvest h;
    int rest = 0;
    while (h.stage() == HarvestStage::Rest) { h.step(true); ++rest; }
    CHECK(rest == kIdleHarvestRestBeats);
    for (int b = 0; b < kIdleHarvestStreamBeats; ++b) {
        CHECK(h.stage() == HarvestStage::Stream);
        h.step(true);
    }
    for (int b = 0; b < kIdleHarvestHoldBeats; ++b) {
        CHECK(h.stage() == HarvestStage::Hold);
        h.step(true);
    }
    CHECK(h.stage() == HarvestStage::Rest);

    Game ransom{StartMode::Hatched, "paypup"};
    Game other{StartMode::Hatched, "keyloggerhead"};
    uint32_t t = 0;
    bool ransomHeld = false, otherHeld = false;
    for (int i = 0; i < kIdleHarvestRestBeats + kIdleHarvestStreamBeats + 1; ++i) {
        ransom.tick(t += kHeartbeatMs);
        other.tick(t);
        ransomHeld |= ransom.petHarvest().stage() == HarvestStage::Hold;
        otherHeld |= other.petHarvest().stage() != HarvestStage::Rest;
    }
    CHECK(ransomHeld);
    CHECK(!otherHeld);
}

// The mail skim (core/model/idle_feeding.h): after its rest, scraps sink in one at a
// time; a pet standing where the model says to stand bites each one; an uneaten scrap
// does not outlive the feed; and the squat and the login follow before it rests again.
// Driven at the model, with the pet teleported to its stand point, so the gate is about
// the rules rather than the chase pace.
void test_idle_feeding_letters_sink_and_get_eaten() {
    IdleFeeding f;
    for (int i = 0; i < kIdleFeedRestBeats; ++i) { f.step(true, 0, 0); CHECK(!f.active()); }
    int petX = 0, petY = 0;
    bool sawBite = false, sawSquat = false, sawGrant = false;
    for (int i = 0; i < kIdleFeedMaxBeats + kIdleSquatBeats + kIdleLoginBeats + 10 &&
                    (f.active() || i == 0);
         ++i) {
        f.step(true, petX, petY);
        if (const Mail* m = f.target()) {
            petX = IdleFeeding::standX(*m);
            petY = IdleFeeding::standY(*m);
        }
        sawBite |= f.biting();
        sawSquat |= f.squatted();
        sawGrant |= f.stage() == FeedStage::Login && f.loginStep() == LoginStep::Granted;
    }
    CHECK(sawBite);
    CHECK(sawSquat);
    CHECK(sawGrant);
    CHECK(!f.active());   // it all ended, so every scrap went somewhere

    IdleFeeding none;
    for (int i = 0; i < 1000; ++i) { none.step(false, 0, 0); CHECK(!none.active()); }

    Game phish{StartMode::Hatched, "tadpoll"};
    Game other{StartMode::Hatched, "paypup"};
    uint32_t t = 0;
    bool phishFed = false, otherFed = false;
    for (int i = 0; i < 2 * kIdleFeedRestBeats; ++i) {
        phish.tick(t += kHeartbeatMs);
        other.tick(t);
        phishFed |= phish.petFeeding().feeding();
        otherFed |= other.petFeeding().feeding();
    }
    CHECK(phishFed);
    CHECK(!otherFed);
}

// The lure (core/model/idle_lure.h) takes turns with the skim: it never runs while a
// skim does, it comes round for a skimming line only, and its pet holds still while the
// bait is out.
void test_idle_lure_takes_turns_with_the_skim() {
    Game phish{StartMode::Hatched, "tadpoll"};
    Game other{StartMode::Hatched, "paypup"};
    uint32_t t = 0;
    bool lured = false, otherLured = false, held = true;
    int lastX = 0;
    for (int i = 0; i < 2 * kIdleFeedRestBeats; ++i) {
        phish.tick(t += kHeartbeatMs);
        other.tick(t);
        const bool on = phish.petLure().active();
        CHECK(!(on && phish.petFeeding().active()));
        if (on && lured) held &= phish.petWander().offsetX() == lastX;
        lured |= on;
        lastX = phish.petWander().offsetX();
        otherLured |= other.petLure().active();
    }
    CHECK(lured);
    CHECK(held);
    CHECK(!otherLured);
}

// A shapeshifter's vanish (core/model/idle_vanish.h) and mimic (core/model/idle_mimic.h)
// both come round, never at once and never over a colour drift, and only for that line.
void test_idle_shapeshift_takes_turns_with_the_drift() {
    Game shifter{StartMode::Hatched, "morphopus"};
    Game other{StartMode::Hatched, "paypup"};
    uint32_t t = 0;
    bool vanished = false, mimicked = false, otherShifted = false;
    for (int i = 0; i < 3 * kIdleVanishRestBeats; ++i) {
        shifter.tick(t += kHeartbeatMs);
        other.tick(t);
        const bool v = shifter.petVanish().active(), m = shifter.petMimic().active();
        CHECK(!(v && m));
        CHECK(!((v || m) && shifter.petCamo().level() > 0));
        vanished |= v;
        mimicked |= m;
        otherShifted |= other.petVanish().active() || other.petMimic().active();
    }
    CHECK(vanished);
    CHECK(mimicked);
    CHECK(!otherShifted);
}

// The seam, at the engine: a raised pet drifts around its shelf as the heartbeat
// runs, and an egg does not. An egg sits where it was laid, with its incubation
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
