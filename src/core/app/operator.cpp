#include "core/app/operator.h"

#include <cstring>

#include "core/content/content_backgrounds.h"
#include "core/content/content_crews.h"
#include "core/content/content_tables.h"   // kModWireCap
#include "core/content/registry.h"

namespace mal {

namespace {

SaveId saveId(const char* id) {
    SaveId s;
    if (id) std::strncpy(s.id, id, kSaveIdCap - 1);
    return s;
}

// Copy what overlaps of a length-prefixed set and leave the rest zero. A shorter set is
// an older save, and a longer one a build with a bigger catalogue: the bits this build
// can name come back, and the rest return on the build that has them.
template <size_t N>
void loadBits(uint8_t (&out)[N], const std::vector<uint8_t>& in) {
    for (size_t i = 0; i < N; ++i) out[i] = i < in.size() ? in[i] : 0;
}

}  // namespace

bool RigAccount::toggleService(int row) {
    if (!serviceOwned(row)) return false;
    const uint32_t bit = 1u << static_cast<unsigned>(row);
    if (!kRigUpgrades[row].askable) {
        servicesOff ^= bit;
        return true;
    }
    // Three positions, stepped in the order the board reads them.
    switch (serviceMode(row)) {
        case ServiceMode::On:  servicesAsk |= bit; break;
        case ServiceMode::Ask: servicesAsk &= ~bit; servicesOff |= bit; break;
        case ServiceMode::Off: servicesOff &= ~bit; break;
    }
    return true;
}

// Each collection is reserved to its exact count first, for the reason Game::captureSave
// gives.
SaveOperator Operator::saved(const ContentRegistry& reg, uint32_t nowMs) const {
    SaveOperator s;
    std::strncpy(s.hackerTag, hackerTag, kHackerTagMax);
    s.networksSeen = networksSeen;
    s.hackerRank = hackerRank;
    if (const CrewDef* c = crew(crewIndex)) std::strncpy(s.crewId, c->id, kSaveIdCap - 1);
    s.homeNetworkKey = homeNetworkKey;
    std::strncpy(s.homeNetworkName, homeNetworkName, kSaveNetNameCap - 1);
    s.petsRaised = petsRaised;
    s.lifetimeUptimeMs = uptimeBase + nowMs;
    s.lifetimeSteps = lifetimeSteps;

    s.bits = bits;
    for (const OwnedMod& m : modPool.owned())
        if (const ModDef* def = reg.mod(m.id))
            saveSetModCount(s.ownedModCounts, def->wire, m.count);

    // Rows below kRigRowExtBase predate the v32 tail and keep a named field each; every
    // row from there up rides rigLevelsExt by position.
    s.bwUpgradeCount = rig.level[kRigRowBandwidth];
    s.fragAmountTier = static_cast<uint8_t>(rig.level[kRigRowFragReduce]);
    s.fragTriggerTier = static_cast<uint8_t>(rig.level[kRigRowFragTrigger]);
    s.itemTabsUnlocked = rig.level[kRigRowItemTabs] ? 1 : 0;
    s.bulkOpenUnlocked = rig.level[kRigRowBulkOpen] ? 1 : 0;
    s.rackSlotUpgradeCount = rig.level[kRigRowRackSlot];
    s.scrapingClusterLevel = rig.level[kRigRowScraping];
    s.dataMiningLevel = rig.level[kRigRowDataMining];
    s.mergeHubUnlocked = rig.level[kRigRowMergeHub] ? 1 : 0;
    s.rigLevelsExt.reserve(kRigUpgradeCount - kRigRowExtBase);
    for (int i = kRigRowExtBase; i < kRigUpgradeCount; ++i)
        s.rigLevelsExt.push_back(static_cast<uint16_t>(rig.level[i]));
    s.rigServicesOff = rig.servicesOff;
    s.rigServicesAsk = rig.servicesAsk;
    // The whole recipe set goes out as the v51 bitset, and its first four bytes also as
    // the v31 u32, which is all a reader older than the bitset can hold.
    s.recipeOwned.assign(rig.recipesOwned, rig.recipesOwned + kMergeRecipeWireBytes);
    s.recipesUnlocked = 0;
    for (int i = 0; i < 4; ++i)
        s.recipesUnlocked |= static_cast<uint32_t>(rig.recipesOwned[i]) << (i * 8);

    s.seenCreatures.reserve(reveals.seenCreatures.size());
    for (const CreatureDef* c : reveals.seenCreatures)
        s.seenCreatures.push_back(saveId(c->id));
    s.malbeastSeen = reveals.malbeastSeenMask;
    s.malbeastDefeated = reveals.malbeastDefeatedMask;
    // The bitsets go out at their full in-memory width, so a catalogue that grows into
    // spare capacity needs no save change.
    s.achievementEarned.assign(reveals.achEarned, reveals.achEarned + kAchBytes);
    s.achievementNotified.assign(reveals.achNotified, reveals.achNotified + kAchBytes);
    s.storyRead.assign(reveals.storyRead, reveals.storyRead + sizeof(reveals.storyRead));
    s.tipsSeen.assign(reveals.tipsSeen, reveals.tipsSeen + sizeof(reveals.tipsSeen));

    s.bossWins = tallies.bossWins;
    s.stackerWins = tallies.stackerWins;
    s.tourneyWins = tallies.tourneyWins;
    s.pvpWins = tallies.pvpWins;
    s.mergesCooked = tallies.mergesCooked;
    // One row per cabinet that has been played, id-keyed: the save carries history, not
    // the roster.
    for (int i = 0; i < arcadeGameCount() && i < kArcadeMaxCabinets; ++i) {
        if (tallies.arcadePlays[i] == 0) continue;
        s.arcadeIds.push_back(saveId(arcadeGames()[i].id));
        s.arcadePlays.push_back(tallies.arcadePlays[i]);
        s.arcadeWins.push_back(tallies.arcadeWins[i]);
        s.arcadeBest.push_back(tallies.arcadeBest[i]);
    }
    s.quoteStates.assign(tallies.quoteStates, tallies.quoteStates + kQuoteStateBytes);

    // AUTO writes 0, which is also what a pick with no row writes, since AUTO is the safe
    // reading.
    if (const BackgroundDef* b = backgroundFor(backgroundPick)) s.backgroundPick = b->wire;
    return s;
}

namespace {

// The spare pool. A v45+ save keeps a count per mod wire; its counts were capped when
// written, so each is its own ceiling here (modStorageCap would take copies from a player
// whose MOD STORAGE purchase was lost). An older save keeps one id per copy and is tallied
// up to the base cap, which is right because no migrating save owns MOD STORAGE. A pre-v17
// one still listed the installed mods among the spares, so those are skipped.
ModPool loadedModPool(const SaveOperator& s, const SaveId (&activeMods)[kModSlots],
                      const ContentRegistry& reg) {
    ModPool pool;
    if (s.hasModCountData) {
        for (int wire = 0; wire < kModWireCap; ++wire) {
            const int n = saveModCount(s.ownedModCounts, wire);
            if (n <= 0) continue;
            const ModDef* def = reg.modByWire(wire);
            if (!def) continue;
            for (int i = 0; i < n; ++i) pool.grant(def->id, n);
        }
        return pool;
    }
    for (const SaveId& m : s.ownedMods) {
        const ModDef* def = reg.mod(m.id);
        if (!def) continue;
        if (!s.hasPermanentModData) {
            bool inSlot = false;
            for (const SaveId& e : activeMods)
                if (std::strcmp(e.id, m.id) == 0) { inSlot = true; break; }
            if (inSlot) continue;
        }
        pool.grant(def->id, kModCopyCapBase);   // surplus copies simply don't fit
    }
    return pool;
}

RigAccount loadedRig(const SaveOperator& s) {
    RigAccount r;
    r.level[kRigRowBandwidth] = s.bwUpgradeCount < 0 ? 0 : s.bwUpgradeCount;
    r.level[kRigRowFragReduce] = s.fragAmountTier;
    r.level[kRigRowFragTrigger] = s.fragTriggerTier;
    r.level[kRigRowItemTabs] = s.itemTabsUnlocked ? 1 : 0;
    r.level[kRigRowBulkOpen] = s.bulkOpenUnlocked ? 1 : 0;
    r.level[kRigRowRackSlot] = s.rackSlotUpgradeCount < 0 ? 0 : s.rackSlotUpgradeCount;
    r.level[kRigRowScraping] = s.scrapingClusterLevel < 0 ? 0 : s.scrapingClusterLevel;
    r.level[kRigRowDataMining] = s.dataMiningLevel < 0 ? 0 : s.dataMiningLevel;
    r.level[kRigRowMergeHub] = s.mergeHubUnlocked ? 1 : 0;
    for (int i = kRigRowExtBase; i < kRigUpgradeCount; ++i) {
        const size_t idx = static_cast<size_t>(i - kRigRowExtBase);
        r.level[i] = idx < s.rigLevelsExt.size() ? s.rigLevelsExt[idx] : 0;
    }
    r.servicesOff = s.rigServicesOff;
    r.servicesAsk = s.rigServicesAsk;
    // The bitset is authoritative: save.cpp seeds it from the v31 u32 for a blob older
    // than v51, so this reads one field however old the save is.
    loadBits(r.recipesOwned, s.recipeOwned);
    return r;
}

OperatorReveals loadedReveals(const SaveOperator& s, const ContentRegistry& reg) {
    OperatorReveals v;
    // Deduplicated because a rename row can retire two ids onto one successor
    // (save.h's renamedIds), and these lists are tallies.
    for (const SaveId& id : s.seenCreatures)
        if (const CreatureDef* c = reg.creature(id.id)) {
            bool have = false;
            for (const CreatureDef* seen : v.seenCreatures) have = have || seen == c;
            if (!have) v.seenCreatures.push_back(c);
        }
    v.malbeastSeenMask = s.malbeastSeen;
    v.malbeastDefeatedMask = s.malbeastDefeated;
    loadBits(v.achEarned, s.achievementEarned);
    loadBits(v.achNotified, s.achievementNotified);
    if (s.achievementEarned.empty() && s.achievementsMask != 0) {
        // Pre-v40: the legacy u32, whose bit i is wire number i. `notified` stays clear,
        // so an upgraded device announces everything it has earned.
        for (int i = 0; i < 4 && i < kAchBytes; ++i)
            v.achEarned[i] = static_cast<uint8_t>((s.achievementsMask >> (8 * i)) & 0xFF);
    }
    loadBits(v.storyRead, s.storyRead);
    loadBits(v.tipsSeen, s.tipsSeen);
    // A v66+ save always writes the tips at full width, so an empty set is an older
    // device already in use: its operator has opened the menu, played the Ransomware
    // hatch every device starts on, cared for a pet and walked it. A line it has never
    // laid still gets its briefing.
    if (s.tipsSeen.empty())
        for (Tip t : {Tip::MenuOpened, Tip::HatchDecrypt, Tip::CareFed, Tip::CareFrag,
                      Tip::CareHappy, Tip::FirstWalk, Tip::FirstError, Tip::NearTheLine,
                      Tip::FirstCache}) {
            const int w = static_cast<int>(t);
            v.tipsSeen[w / 8] |= static_cast<uint8_t>(1u << (w % 8));
        }
    return v;
}

OperatorTallies loadedTallies(const SaveOperator& s) {
    OperatorTallies t;
    t.bossWins = s.bossWins;
    t.stackerWins = s.stackerWins;
    t.tourneyWins = s.tourneyWins;
    t.pvpWins = s.pvpWins;
    t.mergesCooked = s.mergesCooked;
    // A row naming a cabinet this build no longer has lands nowhere. A short list (an
    // older save, or one from a build with fewer cabinets) reads 0.
    for (size_t i = 0; i < s.arcadeIds.size(); ++i) {
        const int row = arcadeGameIndexById(s.arcadeIds[i].id);
        if (row < 0 || row >= kArcadeMaxCabinets) continue;
        t.arcadePlays[row] = i < s.arcadePlays.size() ? s.arcadePlays[i] : 0;
        t.arcadeWins[row] = i < s.arcadeWins.size() ? s.arcadeWins[i] : 0;
        t.arcadeBest[row] = i < s.arcadeBest.size() ? s.arcadeBest[i] : 0;
    }
    loadBits(t.quoteStates, s.quoteStates);
    return t;
}

}  // namespace

Operator Operator::loaded(const SaveOperator& s, const SaveId (&activeMods)[kModSlots],
                          const ContentRegistry& reg) {
    Operator o;
    std::strncpy(o.hackerTag, s.hackerTag, sizeof(o.hackerTag) - 1);
    o.hackerTag[sizeof(o.hackerTag) - 1] = '\0';
    o.networksSeen = s.networksSeen;
    o.hackerRank = s.hackerRank;
    // A crew that no longer exists loads as none rather than as whatever now holds its
    // row, and no save loads into a crew without a home network.
    o.homeNetworkKey = s.homeNetworkKey;
    std::strncpy(o.homeNetworkName, s.homeNetworkName, sizeof(o.homeNetworkName) - 1);
    o.homeNetworkName[sizeof(o.homeNetworkName) - 1] = '\0';
    o.crewIndex = o.homeNetworkKey != 0 ? crewIndexById(s.crewId) : -1;
    o.petsRaised = s.petsRaised;
    o.uptimeBase = s.lifetimeUptimeMs;
    o.lifetimeSteps = s.lifetimeSteps;

    o.bits = s.bits;
    o.modPool = loadedModPool(s, activeMods, reg);
    o.rig = loadedRig(s);
    o.reveals = loadedReveals(s, reg);
    o.tallies = loadedTallies(s);
    // Ownership is the game's to check (Game::restoreBackgroundPick); a wire from a newer
    // build has no row here and reads as AUTO.
    const BackgroundDef* b = backgroundByWire(s.backgroundPick);
    o.backgroundPick = b ? b->scene : SceneId::None;
    return o;
}

}  // namespace mal
