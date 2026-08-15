// test_pedia.cpp — native gates for the web 'Pedia state JSON and the roster it reads.
//
// One slice of the native suite; see test_gates.h for the shared fixtures and
// test_main.cpp for the roster that runs these. A save-vNN gate sits with the
// feature whose field it migrates, not in a migrations pile of its own.
#include "test_gates.h"

// MoveLoadout::grant is the guard the wild-win move-drop roll relies on
// (game.cpp): granting an already-owned move must be a no-op, never a
// duplicate entry in owned(). Unit-level, not a game loop — the property
// belongs to MoveLoadout itself, independent of any RNG.
void test_move_loadout_grant_no_duplicate() {
    MoveLoadout mv = MoveLoadout::starting();
    CHECK(!mv.owns("buffer_overflow"));
    mv.grant("buffer_overflow");
    CHECK(mv.owns("buffer_overflow"));
    const int n = static_cast<int>(mv.owned().size());
    mv.grant("buffer_overflow");                   // already owned -> no-op
    CHECK(static_cast<int>(mv.owned().size()) == n);
}

// The wild-win reward path (game.cpp applyCombatResult) wires an independent
// move-drop roll (kWildMoveDropPct) right next to the existing item-drop roll, and the
// pool is the DEFEATED ENEMY'S WHOLE KIT — you learn a move by beating something that
// knows it, whether or not it got round to using it on you.
//
// Asserted as a CONTRAST rather than a bare "something dropped", because that is what
// tells the enemy's kit apart from any fixed table: at sub 0 an area-0 wild swings only
// the innate jab, so a hundred wins teach nothing at all; at sub 2 the ladder is
// {quick_jab, packet_storm, buffer_overflow} and NOTHING outside those two teachable
// names may ever be learned there. The old fixed pool carried rootkit_strike and
// null_route, so it would fail the second half even while passing the first.
static bool learnedMoveNamed(Game& g, const char* displayName) {
    for (int k = 0; k < g.log().size(); ++k) {
        const LogEntry& e = g.log().at(k);
        if (e.type != LogEventType::ItemGained) continue;
        if (std::strncmp(e.text, "LEARNED ", 8) != 0) continue;
        if (!displayName || std::strcmp(e.text + 8, displayName) == 0) return true;
    }
    return false;
}

// True when every move this pet was taught came out of `allowed` — the enemy's own kit.
static bool learnedOnlyFrom(Game& g, std::vector<const char*> allowed) {
    for (int k = 0; k < g.log().size(); ++k) {
        const LogEntry& e = g.log().at(k);
        if (e.type != LogEventType::ItemGained) continue;
        if (std::strncmp(e.text, "LEARNED ", 8) != 0) continue;
        bool ok = false;
        for (const char* a : allowed)
            if (std::strcmp(e.text + 8, a) == 0) { ok = true; break; }
        if (!ok) return false;
    }
    return true;
}

// Grind wild wins in one armed sub-area, stopping early once something is learned.
static bool farmForAMove(Game& g, int area, int sub, int tries) {
    g.debugSetAutoProgress(false);
    for (int i = 0; i < tries; ++i) {
        g.debugArmExplore(area, sub);
        walkToAnyCombat(g);
        if (g.nav() != Game::Nav::Combat) continue;
        uint32_t t = 0;
        for (int j = 0; j < 400 && g.combat().outcome() == Combat::Outcome::Ongoing; ++j)
            g.tick(t += kHeartbeatMs);
        g.onButton(press(Button::B));                    // dismiss -> apply reward
        if (learnedMoveNamed(g, nullptr)) return true;
    }
    return false;
}

void test_wild_win_can_drop_a_move() {
    // The default pet is a Paypup — a Ransomware-LINE pet, so startingForLine gives it
    // its line kit and nothing generic. Sub 2's ladder is {quick_jab, packet_storm,
    // buffer_overflow}: the jab is innate and never taught, leaving exactly two names.
    Game deep{StartMode::Hatched};
    enterWalk(deep);
    CHECK(farmForAMove(deep, 0, 2, 40));
    CHECK(learnedOnlyFrom(deep, {"Packet Storm", "Buffer Overflow"}));
    CHECK(deep.moveLoadout().owns("packet_storm") ||
          deep.moveLoadout().owns("buffer_overflow"));
    CHECK(!learnedMoveNamed(deep, "Quick Jab"));        // innate, never a reward

    // Sub 0 keeps the tier-1 roster kit — the innate jab alone — so there is nothing
    // there to be taught, however long the pet farms it. This is the half that fails if
    // the pool ever goes back to a fixed table.
    Game shallow{StartMode::Hatched};
    enterWalk(shallow);
    CHECK(!farmForAMove(shallow, 0, 0, 40));
}

// wildMalbeast now rolls among 2 variants per sector tier instead of
// returning one fixed enemy — a pure-function check, no Game needed. Default
// variantRoll=0 (used by test_combat_force_enemy_first above) keeps returning
// the original tier-1 enemy, so that gate is unaffected.
void test_wild_malbeast_roster_variants() {
    CHECK(std::strcmp(wildMalbeast(1, 0).name, "GlitchHog") == 0);
    CHECK(std::strcmp(wildMalbeast(1, 0).name, wildMalbeast(1, 1).name) != 0);
    CHECK(std::strcmp(wildMalbeast(2, 0).name, wildMalbeast(2, 1).name) != 0);
    CHECK(std::strcmp(wildMalbeast(3, 0).name, wildMalbeast(3, 1).name) != 0);
}

// Namespace-disjoint guard: the wild-malbeast name pool must not
// intersect the raised roster's display names. A wild enemy that shares a name
// with a petware/malbeast in the / roster reads to a player as the same
// creature — this gate fails the instant a name collides, whichever side a future
// name is added on.
void test_wild_and_roster_names_disjoint() {
    ContentRegistry r = ContentRegistry::embedded();
    const auto roster = r.allCreatures();
    CHECK(!roster.empty());                       // guard against a vacuous pass
    for (int tier = 1; tier <= 3; ++tier)         // whole wild pool: 2 variants x 3 tiers
        for (uint32_t v = 0; v < 2; ++v) {
            const char* wild = wildMalbeast(tier, v).name;
            for (const CreatureDef* c : roster)
                CHECK(std::strcmp(wild, c->displayName) != 0);
        }
    // Boss enemy names are a third pool — hold the same invariant so a sub-area
    // boss (and the area gauntlet built from them) never reads as a roster creature the
    // player might raise. Cover every sub-area boss across both areas.
    for (int a = 0; a < kExplSectors; ++a)
        for (int sub = 0; sub < kExplSubAreas; ++sub)
            for (const CombatEnemy& round : subAreaBoss(a, sub).rounds)
                for (const CreatureDef* c : roster)
                    CHECK(std::strcmp(round.name, c->displayName) != 0);
}

// Evolution routing: a Script->Daemon weighted pool is the one hop that is NOT on
// the creature row, so it is the one the registry has to answer for. A Script with a
// pool draws from it per care-branch; everything else (linear hops, care branches
// without a pool, termini) reads its own row, which is what the chain tests walk.
void test_evolution_routing_tables() {
    ContentRegistry r = ContentRegistry::embedded();
    // Script->Daemon: the care-branch selects the pool; single entry today.
    const DaemonPoolDef* good = r.daemonPool("malbear", /*bad=*/false);
    const DaemonPoolDef* bad = r.daemonPool("malbear", /*bad=*/true);
    CHECK(good && good->count == 1 &&
          std::strcmp(good->entries[0].daemonId, "bruinforce") == 0);
    CHECK(bad && bad->count == 1 &&
          std::strcmp(bad->entries[0].daemonId, "berserkernel") == 0);
    // A pooled Script's row must still agree with its pool — the row is what the
    // registry falls back to, so a disagreement would be a silent routing change.
    const CreatureDef* malbear = r.creature("malbear");
    CHECK(malbear && std::strcmp(malbear->evolvesToGoodId, "bruinforce") == 0);
    CHECK(malbear && std::strcmp(malbear->evolvesToBadId, "berserkernel") == 0);
    // No pool for anything else: the row is the whole answer.
    CHECK(r.daemonPool("paypup", false) == nullptr);
    CHECK(r.daemonPool("barkmail", false) == nullptr);
}

// The dominant signal is computed from live care interactions: a
// fresh pet is Balanced (no interactions); feeding it tips the balance to
// Feeding. The tally that answers this is the input a signal-dependent evolution
// route would key off; nothing routes on it today, so this is what keeps it honest.
void test_dominant_signal_from_care() {
    Game g{StartMode::Hatched};                      // Paypup, no interactions yet
    CHECK(g.dominantSignal() == DominantSignal::Balanced);
    g.model().setHunger(40);
    enterSlot(g, SubmenuId::Items);
    g.onButton(press(Button::B));                    // open first food's detail
    g.onButton(press(Button::B));                    // Use -> feed (tally Feeding)
    CHECK(g.nav() == Game::Nav::ModalFeeding);
    CHECK(g.dominantSignal() == DominantSignal::Feeding);
}

// Battery SoC mapping (CFG "BATT" line): the portable helper the device reader
// leans on. Endpoints, clamping (a charging pack can read >4.2V), and monotonicity.
void test_battery_percent_from_mv() {
    CHECK(batteryPercentFromMilliVolts(4200) == 100);   // full
    CHECK(batteryPercentFromMilliVolts(3300) == 0);     // empty
    CHECK(batteryPercentFromMilliVolts(5000) == 100);   // clamp high (on charger)
    CHECK(batteryPercentFromMilliVolts(3000) == 0);     // clamp low
    const int mid = batteryPercentFromMilliVolts(3750); // ~halfway
    CHECK(mid >= 45 && mid <= 55);
    int prev = -1;                                       // non-decreasing across range
    for (int mv = 3000; mv <= 4300; mv += 50) {
        const int p = batteryPercentFromMilliVolts(mv);
        CHECK(p >= prev);
        prev = p;
    }
}

// Web 'Pedia slice -----------------------------------

// Game::setHackerTag: the one safe write the web 'Pedia is allowed. Validates
// length (1..kHackerTagMax) and charset (A-Z0-9_) before mutating, and a reject
// must leave the live tag untouched.
void test_set_hacker_tag_validates() {
    Game g{StartMode::Hatched};
    const std::string original = g.hackerTag();

    CHECK(g.setHackerTag("NEWTAG_1"));
    CHECK(std::strcmp(g.hackerTag(), "NEWTAG_1") == 0);

    // Reject: empty.
    CHECK(!g.setHackerTag(""));
    CHECK(std::strcmp(g.hackerTag(), "NEWTAG_1") == 0);   // unchanged

    // Reject: too long (> kHackerTagMax).
    std::string tooLong(kHackerTagMax + 1, 'A');
    CHECK(!g.setHackerTag(tooLong.c_str()));
    CHECK(std::strcmp(g.hackerTag(), "NEWTAG_1") == 0);

    // Reject: bad charset (lowercase, punctuation, space).
    CHECK(!g.setHackerTag("lower"));
    CHECK(!g.setHackerTag("BAD-TAG"));
    CHECK(!g.setHackerTag("HAS SPACE"));
    CHECK(std::strcmp(g.hackerTag(), "NEWTAG_1") == 0);

    // Exactly kHackerTagMax chars is the boundary — must be accepted.
    std::string maxLen(kHackerTagMax, 'Z');
    CHECK(g.setHackerTag(maxLen.c_str()));
    CHECK(std::strcmp(g.hackerTag(), maxLen.c_str()) == 0);

    CHECK(g.setHackerTag(original.c_str()));  // restore, tidy
}

// buildPediaStateJson: on a known (Hatched) state, the payload is well-formed
// enough to eyeball (balanced braces) and contains the fields a live device
// reload most needs — the tag, the bits, and the active pet's own "hatched"
// entry in `pets`.
void test_pedia_state_json_shape() {
    Game g{StartMode::Hatched, "paypup"};
    CHECK(g.setHackerTag("NETRUNNER_9"));

    const std::string json = buildPediaStateJson(g);
    CHECK(!json.empty());
    CHECK(json.front() == '{' && json.back() == '}');

    // Braces/brackets balance (a cheap parseable-ness smoke check without
    // pulling in a JSON library).
    int braces = 0, brackets = 0;
    for (char c : json) {
        if (c == '{') ++braces;
        else if (c == '}') --braces;
        else if (c == '[') ++brackets;
        else if (c == ']') --brackets;
    }
    CHECK(braces == 0 && brackets == 0);

    CHECK(json.find("\"hacker_tag\":\"NETRUNNER_9\"") != std::string::npos);
    CHECK(json.find("\"currency_bits\":") != std::string::npos);
    CHECK(json.find("\"active_pet\":{") != std::string::npos);
    CHECK(json.find("\"species\":\"paypup\"") != std::string::npos);
    // The active pet must reveal as "hatched" in the pets{} map.
    CHECK(json.find("\"paypup\":\"hatched\"") != std::string::npos);
    // A never-touched creature in the same line stays locked.
    CHECK(json.find("\"bruinforce\":\"locked\"") != std::string::npos);
    // A never-encountered malbeast stays locked; every achievement bit starts
    // incomplete on a fresh Hatched-seam Game.
    CHECK(json.find("\"glitchhog\":\"locked\"") != std::string::npos);
    CHECK(json.find("\"DEVTOOLS_INTRUDER\":\"incomplete\"") != std::string::npos);
}

// A fresh boot (FreshHatch — the real "empty save" first-boot path,
// redesign) lays the Boot-Sector egg immediately rather than leaving pet()
// null, so this exercises the "unhatched egg" shape rather than JSON null.
// buildPediaStateJson defensively emits `null` if pet() ever IS nullptr (no
// public path constructs that today, per Game::startHatch's every branch
// assigning pet_), so this gate covers the realistic case instead.
void test_pedia_state_json_fresh_hatch_egg() {
    Game g{StartMode::FreshHatch};
    pickFirstEggLine(g);
    CHECK(g.pet() != nullptr);          // the egg IS a creature (CryptoShell)
    CHECK(g.inEggPhase());
    const std::string json = buildPediaStateJson(g);
    CHECK(json.find("\"active_pet\":{") != std::string::npos);
    CHECK(json.find("\"species\":\"cryptoshell\"") != std::string::npos);
    CHECK(json.find("\"stage\":\"BOOT SECTOR\"") != std::string::npos);
    CHECK(json.find("\"cryptoshell\":\"hatched\"") != std::string::npos);
}

// The recipes{} block alone. items{} and recipes{} are keyed alike on purpose — a
// method is named by the dish it cooks — so a bare find() over the whole payload would
// happily match the wrong map's entry for the same id.
static std::string recipesBlock(const std::string& json) {
    const size_t k = json.find("\"recipes\":{");
    if (k == std::string::npos) return "";
    return json.substr(k, json.find('}', k) - k);
}

// The 'Pedia's two KITCHEN axes, which the FOOD tab masks independently: items{} is
// "ever held" (Game::itemCollected, the cuisine ladder's own tally) and recipes{} is
// whether the METHOD has been won. Holding a dish must not imply knowing how to cook
// it — several are sold on shelves — and eating one must not un-reveal it, which is
// exactly what keying items{} on current possession used to do.
void test_pedia_state_kitchen_axes() {
    Game g{StartMode::Hatched};
    uint32_t t = 0;

    // Nothing met, nothing known.
    CHECK(recipesBlock(buildPediaStateJson(g))
              .find("\"pwnzu_patched_noodles\":\"locked\"") != std::string::npos);

    // Meet the dish, then eat it to zero: it stays revealed.
    g.inventory().add("pwnzu_patched_noodles", 1);
    g.tick(t += kAchSweepIntervalMs);                    // sweepCollectedItems runs here
    CHECK(g.inventory().remove("pwnzu_patched_noodles", 1));
    CHECK(!g.inventory().has("pwnzu_patched_noodles"));
    {
        const std::string json = buildPediaStateJson(g);
        CHECK(json.find("\"pwnzu_patched_noodles\":\"unlocked\"") != std::string::npos);
        // ...and the method is still its own, unwon question.
        CHECK(recipesBlock(json).find("\"pwnzu_patched_noodles\":\"locked\"") !=
              std::string::npos);
    }

    // Win the method (index 0 IS that dish's row) and only the recipes{} half flips.
    g.debugWinRecipe(0);
    CHECK(recipesBlock(buildPediaStateJson(g))
              .find("\"pwnzu_patched_noodles\":\"known\"") != std::string::npos);
}

// Save v25: web-'Pedia reveal-state bookkeeping -----

// wildMalbeastIndex: name -> roster index via slugging; anything outside the
// fixed 6-entry roster (a boss/Sim-dummy/unknown name) misses.
void test_wild_malbeast_index_mapping() {
    CHECK(wildMalbeastIndex("GlitchHog") == 0);
    CHECK(wildMalbeastIndex("Segfault Pup") == 1);
    CHECK(wildMalbeastIndex("Packet Wraith") == 2);
    CHECK(wildMalbeastIndex("Cache Ghoul") == 3);
    CHECK(wildMalbeastIndex("Buffer Wyrm") == 4);
    CHECK(wildMalbeastIndex("Kernel Leviathan") == 5);
    CHECK(wildMalbeastIndex("PHISH PHRY") == -1);     // a sub-area boss name
    CHECK(wildMalbeastIndex("Basic Dummy") == -1);    // a Sim dummy
    CHECK(wildMalbeastIndex("Lethal") == -1);         // the debug unbeatable enemy
    CHECK(wildMalbeastIndex(nullptr) == -1);
}

// Save v24 -> v25: a pre-v25 blob (no tail) loads with seenCreatures empty and both
// malbeast masks + the achievements mask at 0 (mirrors the v20->v21 shield-default
// pattern) — the honest default for a save that predates this system.
void test_save_v24_to_v25_pedia_defaults() {
    SaveData a; std::strcpy(a.activeId, "paypup"); a.generation = 1;
    a.seenCreatures.push_back(SaveId{"malbear"});   // would round-trip IF the tail were read
    a.malbeastSeen = 0x3;
    a.malbeastDefeated = 0x1;
    a.achievementsMask = 0x5;
    auto blob = forgeLegacyNetworkBytes(a, 24);
    // Drop the v25 tail: seenCreatures (u16 count + 1*kSaveIdCap) + 2 malbeast u16s
    // + the achievements u32.
    const size_t v25TailBytes = 2 + kSaveIdCap + 2 + 2 + 4;
    blob.resize(blob.size() - v25TailBytes);
    blob[4] = 24; blob[5] = 0;                  // stamp the version word down to 24
    SaveData out;
    CHECK(deserializeSave(blob, out));          // a v24 blob still deserializes
    CHECK(out.seenCreatures.empty());
    CHECK(out.malbeastSeen == 0);
    CHECK(out.malbeastDefeated == 0);
    CHECK(out.achievementsMask == 0);
}

// Save v25 round-trip: seenCreatures + both malbeast masks survive a raw
// serialize/deserialize cycle, and a real Game::captureSave -> persistSave -> reload
// round-trips the same state through the public API. (The v25 achievements u32 that used
// to ride here is no longer written — v40 replaced it with a wire-indexed bitset, whose
// own round-trip and migration are gated separately.)
void test_save_v25_roundtrip() {
    SaveData a; std::strcpy(a.activeId, "paypup"); a.generation = 1;
    a.seenCreatures.push_back(SaveId{"malbear"});
    a.seenCreatures.push_back(SaveId{"berserkernel"});
    a.malbeastSeen = 0x2A;       // bits 1, 3, 5
    a.malbeastDefeated = 0x01;   // bit 0
    SaveData out;
    CHECK(deserializeSave(serializeSave(a), out));
    CHECK(out.seenCreatures.size() == 2);
    CHECK(std::strcmp(out.seenCreatures[0].id, "malbear") == 0);
    CHECK(std::strcmp(out.seenCreatures[1].id, "berserkernel") == 0);
    CHECK(out.malbeastSeen == 0x2A);
    CHECK(out.malbeastDefeated == 0x01);

    // Game-level round trip through captureSave/persistSave/applySave (mirrors the
    // v24 gate's "Live round-trip" section).
    MemSaveStore store;
    {
        Game g{StartMode::Hatched, "malbear", &store};
        g.markCreatureSeen("berserkernel");
        g.unlockAchievement("BIT_BARON");
        g.tick(kSaveAutosaveMs + kHeartbeatMs);   // autosave
    }
    Game g2(StartMode::Hatched, "paypup", &store);   // hatchedCreature ignored: store wins
    CHECK(g2.creatureSeen("berserkernel"));
    CHECK(g2.hasAchievement("BIT_BARON"));
}

// v27 — the rig-upgrade levels round-trip, and a pre-v27 blob (no tail)
// migrates all three to 0 (a migrated save has bought none).
void test_save_v27_roundtrip() {
    SaveData a; std::strcpy(a.activeId, "paypup"); a.generation = 1;
    a.rackSlotUpgradeCount = 3;
    a.scrapingClusterLevel = 7;
    a.dataMiningLevel = 12;
    SaveData out;
    CHECK(deserializeSave(serializeSave(a), out));
    CHECK(out.rackSlotUpgradeCount == 3);
    CHECK(out.scrapingClusterLevel == 7);
    CHECK(out.dataMiningLevel == 12);

    std::vector<uint8_t> blob = forgeLegacyNetworkBytes(a, 26);
    blob.resize(blob.size() - 3 * 4);   // drop the v27 tail (3 i32s)
    blob[4] = 26; blob[5] = 0;          // stamp the version word down to 26
    SaveData migrated;
    CHECK(deserializeSave(blob, migrated));   // a v26 blob still deserializes
    CHECK(migrated.rackSlotUpgradeCount == 0);
    CHECK(migrated.scrapingClusterLevel == 0);
    CHECK(migrated.dataMiningLevel == 0);

    // Game-level round trip through captureSave/persistSave/applySave.
    MemSaveStore store;
    {
        Game g{StartMode::Hatched, "malbear", &store};
        g.debugSetBits(100000000);
        g.debugBuyRackSlotUpgrade();
        g.debugBuyScrapingCluster();
        g.debugBuyDataMining();
        g.tick(kSaveAutosaveMs + kHeartbeatMs);   // autosave
    }
    Game g2(StartMode::Hatched, "paypup", &store);
    CHECK(g2.rackSlotUpgradeCount() == 1);
    CHECK(g2.scrapingClusterLevel() == 1);
    CHECK(g2.dataMiningLevel() == 1);
}

// v32 — rigLevelsExt (Rig Shop rows beyond the legacy 11) round-trips, and a
// pre-v32 blob (no tail) migrates it to empty (every such row unbought).
void test_save_v32_roundtrip() {
    SaveData a; std::strcpy(a.activeId, "paypup"); a.generation = 1;
    a.rigLevelsExt = {5, 9};
    SaveData out;
    CHECK(deserializeSave(serializeSave(a), out));
    CHECK(out.rigLevelsExt.size() == 2 && out.rigLevelsExt[0] == 5 && out.rigLevelsExt[1] == 9);

    std::vector<uint8_t> blob = forgeLegacyNetworkBytes(a, 31);
    blob.resize(blob.size() - 2 * 2 - 2);   // drop the v32 tail (u16 size + 2 u16s)
    blob[4] = 31; blob[5] = 0;              // stamp the version word down to 31
    SaveData migrated;
    CHECK(deserializeSave(blob, migrated));  // a v31 blob still deserializes
    CHECK(migrated.rigLevelsExt.empty());
}
