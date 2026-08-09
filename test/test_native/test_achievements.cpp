// test_achievements.cpp — native gates for the achievement catalogue and its ladder.
//
// One slice of the native suite; see test_gates.h for the shared fixtures and
// test_main.cpp for the roster that runs these. A save-vNN gate sits with the
// feature whose field it migrates, not in a migrations pile of its own.
#include "test_gates.h"

// --- Achievement catalogue + machinery ------------------------------------------

// The mod table carries save wire numbers for the same reason the achievement table does
// — the owned-mod pool is stored as a count per wire — so it needs the same guard. A
// duplicated number would make two mods share a slot in that array, which reads back as
// the wrong mod in the player's pool rather than as any kind of error.
void test_mod_table_wires_are_unique_and_in_range() {
    CHECK(kModsCount > 0);
    for (int i = 0; i < kModsCount; ++i) {
        const ModDef& m = kMods[i];
        CHECK(m.id && m.id[0]);
        CHECK(m.displayName && m.displayName[0]);
        // The pool array is sized by this cap; a row past it would never persist.
        CHECK(m.wire >= 0 && m.wire < kModWireCap);
        for (int j = i + 1; j < kModsCount; ++j) {
            if (kMods[j].wire == m.wire)
                std::printf("  DUPLICATE MOD WIRE %d: %s / %s\n", m.wire, m.id, kMods[j].id);
            CHECK(kMods[j].wire != m.wire);
            CHECK(std::strcmp(kMods[j].id, m.id) != 0);
        }
    }
    // And the registry has to agree, since that is the lookup the save actually uses.
    ContentRegistry reg = ContentRegistry::embedded();
    for (int i = 0; i < kModsCount; ++i) {
        const ModDef* byWire = reg.modByWire(kMods[i].wire);
        CHECK(byWire != nullptr);
        CHECK(std::strcmp(byWire->id, kMods[i].id) == 0);
    }
    CHECK(reg.modByWire(kModWireCap) == nullptr);      // out of range resolves to nothing
    CHECK(reg.modByWire(-1) == nullptr);
}

// Table integrity. These are the invariants the whole system leans on, and every one of
// them is the sort of thing a hand-edited table breaks silently: a duplicated wire number
// would make two rows share a save bit, a renamed id would strand a call site, and a
// kGoalAll row on an unbounded series would ship an achievement nobody can ever earn.
void test_achievement_table_is_well_formed() {
    CHECK(kAchievementCount > 0);
    for (int i = 0; i < kAchievementCount; ++i) {
        const AchievementDef& d = kAchievements[i];
        CHECK(d.id && d.id[0]);
        CHECK(d.displayName && d.displayName[0]);
        CHECK(d.trigger && d.trigger[0]);
        CHECK(d.icon && d.icon[0]);
        // The bitset is sized by this cap; a row past it would silently never persist.
        CHECK(d.wire >= 0 && d.wire < kAchievementWireCap);
        // Unique wire, unique id.
        for (int j = i + 1; j < kAchievementCount; ++j) {
            if (kAchievements[j].wire == d.wire)
                std::printf("  DUPLICATE WIRE %d: %s / %s\n", d.wire, d.id,
                            kAchievements[j].id);
            CHECK(kAchievements[j].wire != d.wire);
            CHECK(std::strcmp(kAchievements[j].id, d.id) != 0);
        }
        // Every row must be reachable: an Event row by a call site, anything else by a
        // goal that resolves to a positive number.
        if (d.series != AchSeries::Event) {
            const int goal = achievementGoal(d);
            if (goal <= 0)
                std::printf("  UNEARNABLE ROW (goal resolves to %d): %s\n", goal, d.id);
            CHECK(goal > 0);
        }
        // A series that takes a subject must have been given one.
        if (d.series == AchSeries::LineRaised || d.series == AchSeries::ItemCollected ||
            d.series == AchSeries::DeepWebDepthLine)
            CHECK(d.key && d.key[0]);
        // Reward ids must name real items — a typo here is a reward that never arrives.
        ContentRegistry r = ContentRegistry::embedded();
        for (const AchievementReward& rw : d.rewards)
            if (rw.kind == AchievementReward::Kind::Item)
                CHECK(rw.id && r.item(rw.id) != nullptr);
        // The trigger template must fully resolve — no stray braces reaching a reader.
        const EffectText txt = achievementTrigger(d);
        if (std::strchr(txt.c_str(), '{'))
            std::printf("  UNRESOLVED TRIGGER TOKEN: %s -> %s\n", d.id, txt.c_str());
        CHECK(std::strchr(txt.c_str(), '{') == nullptr);
    }
    // The ids the engine fires by hand all exist. This is the check that makes the
    // `ach::` constants worth having.
    for (const char* id : {ach::kFirstBruteForce, ach::kSurvivedLockout, ach::kFlawlessRun,
                           ach::kGoneRogue, ach::kWormWhisperer, ach::kAirGapped,
                           ach::kDevtoolsIntruder, ach::kTrojanUnleashed, ach::kFirstDuel,
                           ach::kBackUpAndDriven, ach::kNeededMoreBackup,
                           ach::kShatteredPlatter, ach::kPerfectDefrag,
                           ach::kHangingByABit,
                           ach::kDeepWebDepth8, ach::kDeepWebDepth64}) {
        if (!achievementById(id)) std::printf("  MISSING ACHIEVEMENT ID: %s\n", id);
        CHECK(achievementById(id) != nullptr);
    }
    // The legacy fourteen keep wire numbers 0-13 in their original enum order — that is
    // what makes the pre-v40 u32 mask a straight byte copy rather than a mapping table.
    const char* kLegacyWireOrder[] = {
        "FIRST_BRUTE_FORCE", "SURVIVED_LOCKOUT", "FLAWLESS_RUN", "GONE_ROGUE",
        "WORM_WHISPERER", "FULL_PEDIA_L1", "BIT_BARON", "AIR_GAPPED", "GENERATION_X",
        "DEVTOOLS_INTRUDER", "DEEPWEB_DEPTH_8", "DEEPWEB_DEPTH_64", "DEEPWEB_DEPTH_256",
        "TROJAN_UNLEASHED"};
    for (int i = 0; i < 14; ++i) {
        const AchievementDef* d = achievementById(kLegacyWireOrder[i]);
        CHECK(d && d->wire == i);
    }
}

// The three Backup Drive achievements are cut from one mapping (backupDriveAchievement):
// what the drive did, crossed with how the fight ended. Asserted directly rather than by
// staging three fights, because the mapping IS the thing that can be wrong.
//
// Compared by std::strcmp, not `==`: the two sides are `inline constexpr const char*`
// string-literal pointers read from DIFFERENT translation units (this file and
// game_combat.cpp). The variable itself is one address (C++17 guarantees that), but
// nothing guarantees the LITERAL each TU's copy of that address points at is the same
// object across TUs — only linkers that happen to fold identical string constants make
// raw `==` pass. That held on macOS/ld64 (folds by default) and failed on Linux/GCC's
// default linker, which doesn't — the same cross-TU hazard achievementById() already
// avoids by looking up ids with strcmp instead of pointer identity.
static bool sameAchId(const char* got, const char* want) {
    return got && want && std::strcmp(got, want) == 0;
}
void test_backup_drive_achievement_mapping() {
    using BU = Combatant::BackupUse;
    using O = Combat::Outcome;
    // No drive spent earns nothing, however the fight went.
    for (O o : {O::Win, O::Lose, O::Fled, O::Ongoing})
        CHECK(backupDriveAchievement(BU::None, o) == nullptr);
    // Saved and went on to win / to lose anyway.
    CHECK(sameAchId(backupDriveAchievement(BU::Restored, O::Win), ach::kBackUpAndDriven));
    CHECK(sameAchId(backupDriveAchievement(BU::Restored, O::Lose), ach::kNeededMoreBackup));
    // Fleeing settles nothing: the pet is alive and the story isn't over.
    CHECK(backupDriveAchievement(BU::Restored, O::Fled) == nullptr);
    // The restore that wasn't enough — including on a mutual KO, which resolves as a Win
    // even though the pet never got back up.
    CHECK(sameAchId(backupDriveAchievement(BU::Overwhelmed, O::Lose), ach::kShatteredPlatter));
    CHECK(sameAchId(backupDriveAchievement(BU::Overwhelmed, O::Win), ach::kShatteredPlatter));
}

// A ladder unlocks off its series' progress with no per-row code, pays the row's own
// rewards, and does it exactly once.
void test_achievement_ladder_unlocks_and_pays() {
    Game g{StartMode::Hatched};
    uint32_t t = 0;
    g.tick(t += kHeartbeatMs);
    CHECK(!g.hasAchievement("BOSS_FIRST"));
    const int bits0 = g.bits();
    g.debugAddBossWins(1);
    g.tick(t += kAchSweepIntervalMs);
    CHECK(g.hasAchievement("BOSS_FIRST"));
    CHECK(g.bits() == bits0 + achBitsReward("BOSS_FIRST"));
    CHECK(!g.hasAchievement("BOSS_10"));            // the next rung is still open

    // Idempotent: sweeping again neither re-pays nor re-announces.
    const int bits1 = g.bits();
    g.tick(t += kAchSweepIntervalMs);
    g.tick(t += kAchSweepIntervalMs);
    CHECK(g.bits() == bits1);

    // A row carrying an item reward puts it in the bag. BOSS_10's rung pays a cache.
    const int caches0 = g.inventory().count("sealed_cache_common");
    g.debugAddBossWins(9);
    g.tick(t += kAchSweepIntervalMs);
    CHECK(g.hasAchievement("BOSS_10"));
    CHECK(g.inventory().count("sealed_cache_common") == caches0 + 1);
}

// kGoalAll is the sentinel that makes "all of them" an ordinary row: it resolves against
// the size of the set the series counts over, so the row keeps meaning "all" when the set
// grows instead of freezing at a hand-typed number.
void test_achievement_goal_all_tracks_the_set_size() {
    const AchievementDef* subsAll = achievementById("SUBS_ALL");
    CHECK(subsAll && subsAll->goal == kGoalAll);
    CHECK(achievementGoal(*subsAll) == kAreaCount * kSubAreasPerArea);
    const AchievementDef* beasts = achievementById("MALBEAST_ALL");
    CHECK(beasts && achievementGoal(*beasts) == kWildMalbeastCount);
    // ...and the prose quotes the resolved number, so it can't drift from the check.
    char want[64];
    std::snprintf(want, sizeof(want), "%d", achievementGoal(*subsAll));
    CHECK(std::strstr(achievementTrigger(*subsAll).c_str(), want) != nullptr);
}

// The banner is the only way an unlock reaches the player, so the rules around it matter:
// it only appears on the idle home screen, it retires on its own, and a row is marked
// announced ONLY once its banner has actually been shown.
void test_achievement_banner_announces_on_the_home_screen() {
    Game g{StartMode::Hatched};
    uint32_t t = 0;
    g.tick(t += kHeartbeatMs);
    while (g.achBanner()) {                       // drain anything the start state earned
        g.tick(t += kAchBannerMs);
        g.tick(t += kHeartbeatMs);
    }
    g.unlockAchievement(ach::kSurvivedLockout);
    CHECK(g.achPendingNotify() == 1);
    CHECK(g.achBanner() == nullptr);              // not until a tick raises it

    g.tick(t += kHeartbeatMs);
    const AchievementDef* shown = g.achBanner();
    CHECK(shown && std::strcmp(shown->id, ach::kSurvivedLockout) == 0);
    CHECK(g.achBannerCount() == 1);
    CHECK(g.achPendingNotify() == 1);             // still pending until it retires

    g.tick(t += kAchBannerMs);
    CHECK(g.achBanner() == nullptr);
    CHECK(g.achPendingNotify() == 0);             // shown, so now it counts as announced
}

// An unlock earned while the player is elsewhere waits for them: nothing is announced to
// an empty room, which is what makes "shown" a safe thing to persist.
void test_achievement_banner_waits_for_the_home_screen() {
    Game g{StartMode::Hatched};
    uint32_t t = 0;
    g.tick(t += kHeartbeatMs);
    while (g.achBanner()) { g.tick(t += kAchBannerMs); g.tick(t += kHeartbeatMs); }
    g.onButton(press(Button::A));                 // summon the carousel — no longer idle
    CHECK(g.nav() != Game::Nav::Idle);
    g.unlockAchievement(ach::kFlawlessRun);
    g.tick(t += kHeartbeatMs);
    CHECK(g.achBanner() == nullptr);              // held back
    CHECK(g.achPendingNotify() == 1);
    g.onButton(press(Button::C));                 // back out to the habitat
    while (g.nav() != Game::Nav::Idle) g.tick(t += kHeartbeatMs);
    g.tick(t += kHeartbeatMs);
    CHECK(g.achBanner() != nullptr);              // ...and delivered on arrival
}

// A backlog too long to parade past one at a time collapses into a single summary, so a
// firmware update that retro-awards a whole catalogue doesn't lock up the home screen.
void test_achievement_banner_collapses_a_burst() {
    Game g{StartMode::Hatched};
    uint32_t t = 0;
    g.tick(t += kHeartbeatMs);
    while (g.achBanner()) { g.tick(t += kAchBannerMs); g.tick(t += kHeartbeatMs); }
    int unlocked = 0;
    for (int i = 0; i < kAchievementCount && unlocked <= kAchBannerBurstMax; ++i) {
        const AchievementDef& d = kAchievements[i];
        if (g.hasAchievement(d)) continue;
        g.unlockAchievement(d.id);
        ++unlocked;
    }
    CHECK(g.achPendingNotify() > kAchBannerBurstMax);
    g.tick(t += kHeartbeatMs);
    CHECK(g.achBannerCount() > 1);                // one banner speaking for the lot
    g.tick(t += kAchBannerMs);
    CHECK(g.achPendingNotify() == 0);             // and it clears the whole backlog
}

// v40 round-trip + the pre-v40 migration, which is the part an upgraded device actually
// walks through: the legacy u32 mask becomes the first bytes of the bitset, nothing is
// marked announced (so the whole history parades), and the counters are seeded from what
// the rest of the save can honestly account for.
void test_save_v40_achievements_roundtrip_and_migration() {
    {   // Raw round-trip of all four v40 fields.
        SaveData a; std::strcpy(a.activeId, "paypup"); a.generation = 1;
        a.achievementEarned.assign(8, 0);
        a.achievementEarned[0] = 0x05;            // wires 0 and 2
        a.achievementNotified.assign(8, 0);
        a.achievementNotified[0] = 0x01;          // wire 0 announced, wire 2 not
        a.bossWins = 37;
        a.collectedItems.push_back(SaveId{"osi_dip"});
        a.speciesDiveIds.push_back(SaveId{"malbear"});
        a.speciesDiveDepths.push_back(312);
        SaveData out;
        CHECK(deserializeSave(serializeSave(a), out));
        CHECK(out.achievementEarned.size() == 8 && out.achievementEarned[0] == 0x05);
        CHECK(out.achievementNotified.size() == 8 && out.achievementNotified[0] == 0x01);
        CHECK(out.bossWins == 37);
        CHECK(out.collectedItems.size() == 1);
        CHECK(std::strcmp(out.collectedItems[0].id, "osi_dip") == 0);
        CHECK(out.speciesDiveIds.size() == 1 && out.speciesDiveDepths.size() == 1);
        CHECK(out.speciesDiveDepths[0] == 312);
    }
    {   // A pre-v40 blob: the legacy mask migrates bit-for-bit, and the achievement it
        // encodes is re-announced because nothing was ever marked as shown.
        SaveData a; std::strcpy(a.activeId, "paypup"); a.generation = 1;
        a.achievementsMask = (1u << 1) | (1u << 6);   // SURVIVED_LOCKOUT + BIT_BARON
        a.subCleared.assign(kAreaCount, 0);
        a.subCleared[0] = 0x03;                        // two sub-areas cleared
        SaveStack held; std::strcpy(held.id, "airgap_snack"); held.qty = 2;
        a.items.push_back(held);                       // something already in the bag
        std::vector<uint8_t> blob = serializeSave(a);
        blob[4] = 39; blob[5] = 0;                     // stamp back to v39
        MemSaveStore store; store.save(blob);
        Game g(StartMode::Hatched, "paypup", &store);
        CHECK(g.hasAchievement("SURVIVED_LOCKOUT"));
        CHECK(g.hasAchievement("BIT_BARON"));
        CHECK(!g.hasAchievement("FLAWLESS_RUN"));
        CHECK(g.achPendingNotify() >= 2);              // the upgrade announces its history
        // Boss wins seed from the clears: two sub-areas beaten is two bosses beaten.
        CHECK(g.bossWins() == 2);
        // And what the bag already holds counts as collected, so the collection ladders
        // don't start an established device back at zero.
        CHECK(g.itemCollected("airgap_snack"));
    }
}

// The species dive record outlives the pet that set it, and is what the per-line and
// "different species" depth rows count off.
void test_species_dive_records_feed_the_depth_rows() {
    Game g{StartMode::Hatched, "malbear"};
    CHECK(g.deepestDiveEver() == 0);
    g.debugRecordSpeciesDive("malbear", 300);
    g.debugRecordSpeciesDive("pingcub", 70);
    CHECK(g.speciesDeepestDive("malbear") == 300);
    CHECK(g.deepestDiveEver() == 300);
    g.debugRecordSpeciesDive("malbear", 120);          // a shallower run never demotes it
    CHECK(g.speciesDeepestDive("malbear") == 300);

    uint32_t t = 0;
    g.tick(t += kAchSweepIntervalMs);
    CHECK(g.hasAchievement("DEEPWEB_DEPTH_256"));
    CHECK(!g.hasAchievement("DEEPWEB_DEPTH_512"));
    // Both are ransomware-line species, so the line record follows the deeper of them.
    CHECK(g.hasAchievement("DEEP_LINE_RANSOMWARE"));
    // Two species past depth 64 is not yet three.
    CHECK(!g.hasAchievement("DEEP_BENCH_3"));
}

// The collection ladders count what has EVER been held, not what is in the bag now —
// spending an item must never un-earn an achievement.
void test_collected_items_survive_being_spent() {
    Game g{StartMode::Hatched};
    uint32_t t = 0;
    g.tick(t += kHeartbeatMs);
    CHECK(g.itemCollected("airgap_snack"));            // on the starting shelf
    const int before = g.itemsCollected();
    while (g.inventory().count("airgap_snack") > 0)
        g.inventory().remove("airgap_snack", 1);
    g.tick(t += kAchSweepIntervalMs);
    CHECK(g.itemCollected("airgap_snack"));            // still collected
    CHECK(g.itemsCollected() == before);
}

// pets{} counts a creature the player still HOLDS in any of the three places a
// pet can sit — active, frozen in the ARCH rack, or an ARCH record. Those are all
// paths that install or record a pet, so all three land in the raised tally too.
void test_pedia_state_json_rack_and_record_hatched() {
    SaveData a;
    std::strcpy(a.activeId, "paypup");
    a.generation = 3;
    SaveStoredPet stored; std::strcpy(stored.id, "pingcub");
    a.rack.push_back(stored);
    SaveRecord rec; std::strcpy(rec.id, "malbear");
    rec.status = static_cast<uint8_t>(RecordStatus::Corrupted);
    rec.generation = 2;
    a.records.push_back(rec);
    MemSaveStore store; store.save(serializeSave(a));
    Game g(StartMode::Hatched, "paypup", &store);
    CHECK(g.rack().size() == 1 && g.records().size() == 1);

    const std::string json = buildPediaStateJson(g);
    CHECK(json.find("\"paypup\":\"hatched\"") != std::string::npos);     // active
    CHECK(json.find("\"pingcub\":\"hatched\"") != std::string::npos);  // racked
    CHECK(json.find("\"malbear\":\"hatched\"") != std::string::npos);    // record
    CHECK(json.find("\"archive\":[{") != std::string::npos);
}

// End-to-end trigger #1: a cracked DISK DECRYPTION board flips FIRST_BRUTE_FORCE
// (Game::finishDecryption). Waiting an egg out does NOT — the achievement is named for
// breaking a key, and an egg that hatched on its own clock never had one to break.
void test_pedia_first_brute_force_achievement() {
    { Game g(StartMode::FreshHatch);
      if (g.inLineSelect()) g.onButton(press(Button::B));
      CHECK(g.inDecryption());
      CHECK(!g.hasAchievement(ach::kFirstBruteForce));
      crackDecryption(g);
      CHECK(g.hasAchievement(ach::kFirstBruteForce)); }

    { Game g(StartMode::FreshHatch);
      pickFirstEggLine(g);                        // settles the board WITHOUT cracking it
      uint32_t t = 1000;
      g.tick(t);
      g.tick(t += kBootHatchMs + kHeartbeatMs);   // wait out incubation -> hatch to Process
      CHECK(g.pet() && g.pet()->stage != Stage::BootSector);
      CHECK(!g.hasAchievement(ach::kFirstBruteForce)); }
}

// End-to-end trigger #2: a real Good-branch and a real Bad-branch Daemon
// evolution (Game::completeEvolution) — FLAWLESS_RUN/GONE_ROGUE fire on the correct
// branch only, and the sibling NOT taken stays LOCKED. Seeing is a fight; an
// evolution cinematic is not one, so the branch it reveals is not a reveal.
void test_pedia_evolution_achievements_leave_the_sibling_locked() {
    { // Good branch (0 mistakes): FLAWLESS_RUN fires, GONE_ROGUE does not, and the
      // Bad sibling (berserkernel) is never marked.
        Game g{StartMode::Hatched, "malbear"};
        g.model().setCareMistakes(0);
        CHECK(g.model().careBranch() == CareBranch::Good);
        g.debugTriggerEvolution();
        uint32_t t = 0;
        advanceToReveal(g, t);
        g.onButton(press(Button::B));
        CHECK(g.pet() && std::strcmp(g.pet()->id, "bruinforce") == 0);
        CHECK(g.hasAchievement(ach::kFlawlessRun));
        CHECK(!g.hasAchievement(ach::kGoneRogue));
        CHECK(!g.creatureSeen("berserkernel"));
    }
    { // Bad branch (3+ mistakes): GONE_ROGUE fires, FLAWLESS_RUN does not, and the
      // Good sibling (bruinforce) is never marked.
        Game g{StartMode::Hatched, "malbear"};
        g.model().setCareMistakes(kCareGoodMax + 1);
        CHECK(g.model().careBranch() == CareBranch::Bad);
        g.debugTriggerEvolution();
        uint32_t t = 0;
        advanceToReveal(g, t);
        g.onButton(press(Button::B));
        CHECK(g.pet() && std::strcmp(g.pet()->id, "berserkernel") == 0);
        CHECK(g.hasAchievement(ach::kGoneRogue));
        CHECK(!g.hasAchievement(ach::kFlawlessRun));
        CHECK(!g.creatureSeen("bruinforce"));
    }
}
