// operator.h: the Operator record (CONTRIBUTING, "Where state lives"). The player, across
// every pet: who they are on the 'net, their wallet and spare mods, the Rig Shop account,
// what they have earned, played and been shown, how far up the EXPL ladder they are, the
// Cant, and the ARCH shelf of stored pets and records. Game holds it as Game::op_ and reads
// it through Game::operatorRecord().
//
// Every part lasts until a WIPE EVERYTHING factory reset, which replaces the whole record
// (Game::wipeDeviceProgress). Operator::saved and Operator::loaded are its only mapping to
// the save.
//
// It sits in app/ rather than model/ because the Rig Shop table its queries read
// (game_rig_shop.h) is an app-layer header.
#pragma once

#include <cstdint>
#include <vector>

#include "tunables.h"   // kHackerTagMax, kStartBits
#include "core/app/game_rig_shop.h"
#include "core/content/content_achievements.h"   // kAchievementWireCap
#include "core/content/content_arcade.h"         // kArcadeMaxCabinets
#include "core/content/content_quotes.h"         // kQuoteStateBytes
#include "core/content/areas/area_defs.h"        // kAreaCount, kSubAreasPerArea
#include "core/content/recipe_wire.h"            // kMergeRecipeWireBytes
#include "core/content/story.h"                  // kStoryWireCap
#include "core/model/cant.h"                     // SigilSet
#include "core/model/loadout.h"
#include "core/model/save.h"
#include "core/render/scene_id.h"

namespace mal {

class ContentRegistry;
struct CreatureDef;
struct ItemDef;

// The one-time onboarding tips: the briefing in front of each egg line's hatch game, and
// the idle nudge that says which button opens the menu. A Tip's value is its bit in
// OperatorReveals::tipsSeen (save v66): append, never renumber or reuse.
enum class Tip : uint8_t {
    HatchDecrypt = 0,     // the Ransomware egg's DISK DECRYPTION
    HatchClutch = 1,      // the Phishing egg's Clutch Pick
    HatchIsolation = 2,   // the Worm egg's Isolation Protocol
    HatchChroma = 3,      // the Metamorphic egg's CHROMATOPHORE
    MenuOpened = 4,       // the carousel has been summoned at least once
    CareFed = 5,          // the first time FED's "!" is up when the menu opens
    CareFrag = 6,         // ...FRAG's (or a ghost's)
    CareHappy = 7,        // ...HAPPY's
    FirstWalk = 8,        // the first walk the player arms from EXPL
    FirstError = 9,       // the first care error a pet has taken
    NearTheLine = 10,     // a pet one error short of failing (kCareDying - 1)
    FirstCache = 11,      // the first sealed cache or Decryptogram in the bag
};
inline constexpr int kTipWireCap = 32;   // bits in the persisted set
inline constexpr int kAchBytes = kAchievementWireCap / 8;

// Everything bought on the Rig Shop, and the switches over its SERVICES. A row's level
// is its purchase count, or 0/1 for a one-time unlock; rows are indexed by RigRow.
struct RigAccount {
    int level[kMaxRigUpgrades] = {};
    // The OFF half is stored, and the ASK half, so that zero reads as what a save from
    // before either switch existed means: every bought service running, at YES.
    static_assert(kMaxRigUpgrades <= 32, "servicesOff is one bit per rig row");
    uint32_t servicesOff = 0;   // set = an owned service switched off
    uint32_t servicesAsk = 0;   // set = an askable service at ASK; read while not off
    // The Merge Hub's known recipes, one bit per MergeRecipe::wire, read and written only
    // through Game::recipeOwned and grantRecipe. Never bought: the Decryptogram's prize
    // ladder is the only writer. Kept beside the Hub's row because the save writes both
    // in one section.
    uint8_t recipesOwned[kMergeRecipeWireBytes] = {};

    bool owned(int row) const { return level[row] > 0; }
    // Anything left to buy on `row`. The SHOP's A-cycle, its render loop and
    // Game::buyRigUpgrade all defer to it, so the list can't show a row the cursor skips.
    bool offered(int row) const {
        return row >= 0 && row < kRigUpgradeCount &&
               level[row] < kRigUpgrades[row].maxLevel;
    }
    // Price of the next purchase on `row`, from the row's own curve (game_rig_shop.h).
    int costFor(int row) const {
        const RigUpgradeDef& d = kRigUpgrades[row];
        return rigUpgradeCost(d.costStart, level[row], d.curve, d.costStep);
    }

    // A SERVICE is a row that acts between events without being asked (the two backup
    // auto-arms, Disk Maintenance's auto-defrag). Owning one is a purchase; running it is
    // a switch, because a service spends the player's Bits or a decision they might want
    // back, so every effect site asks featureActive, never the level.
    bool serviceOwned(int row) const {
        return row >= 0 && row < kRigUpgradeCount && kRigUpgrades[row].service &&
               level[row] > 0;
    }
    // Is `row`'s effect live: owned, and not switched off? A row that is not a service has
    // no switch, so owning it is the whole answer. An askable service at ASK counts as
    // live: it runs and hands the decision back, and serviceMode says which.
    bool featureActive(int row) const {
        if (row < 0 || row >= kRigUpgradeCount || level[row] <= 0) return false;
        if (!kRigUpgrades[row].service) return true;
        return (servicesOff & (1u << row)) == 0;
    }
    ServiceMode serviceMode(int row) const {
        if (!featureActive(row)) return ServiceMode::Off;
        if (kRigUpgrades[row].askable && (servicesAsk & (1u << row)) != 0)
            return ServiceMode::Ask;
        return ServiceMode::On;
    }
    // The word the SERVICES board prints for a row's switch: ON/OFF for a two-way service,
    // YES/ASK/NO for an askable one, whose question is "use them for me?".
    const char* serviceWord(int row) const {
        const ServiceMode m = serviceMode(row);
        if (!kRigUpgrades[row].askable) return m == ServiceMode::Off ? "OFF" : "ON";
        return m == ServiceMode::On ? "YES" : m == ServiceMode::Ask ? "ASK" : "NO";
    }
    // The owned services in table order, which is what the SERVICES board lists. Returns
    // how many were written.
    int serviceRows(int* out, int max) const {
        int n = 0;
        for (int i = 0; i < kRigUpgradeCount && n < max; ++i)
            if (serviceOwned(i)) out[n++] = i;
        return n;
    }
    // The SHOP carries its SERVICES head slot only once there is a service to switch, so
    // the head slot never opens an empty board.
    bool servicesOffered() const {
        for (int i = 0; i < kRigUpgradeCount; ++i)
            if (serviceOwned(i)) return true;
        return false;
    }
    // Owned services that are running: the head slot's "2/3 ON".
    int servicesRunning() const {
        int n = 0;
        for (int i = 0; i < kRigUpgradeCount; ++i)
            if (serviceOwned(i) && featureActive(i)) ++n;
        return n;
    }
    // Step an owned service's switch: On/Off, or YES -> ASK -> NO -> YES on an askable
    // one. False, and nothing stored, on a row that is not an owned service.
    bool toggleService(int row);
};

// The lifetime counts the countable achievements measure, and the arcade's and the
// Decryptogram's standing.
struct OperatorTallies {
    int bossWins = 0;       // boss rounds won; a gauntlet round counts as one
    // Stacker boards cleared by hand in MAINT. The operator's, since it measures skill
    // rather than whichever pet was on the shelf.
    int stackerWins = 0;
    int tourneyWins = 0;    // ROCK THE DOCK brackets taken
    int pvpWins = 0;        // LINK duels won, counted at the verdict
    int mergesCooked = 0;   // dishes cooked at the MERGE HUB
    // Per GAMES roster row (content_arcade.h's index order).
    int arcadePlays[kArcadeMaxCabinets] = {0};
    int arcadeWins[kArcadeMaxCabinets] = {0};
    int arcadeBest[kArcadeMaxCabinets] = {0};   // high score
    // Two bits per QuoteDef::wire (quoteStateGet / quoteStateSet).
    uint8_t quoteStates[kQuoteStateBytes] = {0};
};

// What the operator has been shown or has earned, as the 'Pedia, the achievements, the
// STORY and the tips read it.
struct OperatorReveals {
    // Creatures faced in combat but never raised: stable registry pointers.
    std::vector<const CreatureDef*> seenCreatures;
    // Bitmasks over the wild-malbeast roster (combat.h's kWildMalbeastIds).
    uint16_t malbeastSeenMask = 0;
    uint16_t malbeastDefeatedMask = 0;
    // Indexed by AchievementDef::wire, never by table position, so the catalogue can be
    // regrouped and still load an old save. `notified` is kept apart so an update that
    // retro-awards a back catalogue announces every row instead of absorbing them.
    uint8_t achEarned[kAchBytes] = {0};
    uint8_t achNotified[kAchBytes] = {0};
    // One bit per StoryChapterDef::wire, so a chapter fires once on this device however
    // many pets walk past its beat.
    uint8_t storyRead[kStoryWireCap / 8] = {};
    uint8_t tipsSeen[kTipWireCap / 8] = {};      // one bit per Tip
};

// How far up the EXPL ladder the operator has fought, and the Titles that awards.
struct OperatorWorld {
    bool sectorCleared[kAreaCount] = {};   // the area's gauntlet beaten
    bool subCleared[kAreaCount][kSubAreasPerArea] = {};        // the sub-area's boss beaten
    // A 10-win streak opened the sub-area's manual boss trigger. The streak itself is
    // run state and not kept.
    bool subBossUnlocked[kAreaCount][kSubAreasPerArea] = {};
    uint32_t titlesUnlocked = 0;   // bit i = area i's Title
    int equippedTitle = -1;        // the shown Title's area, or -1 for none

    bool areaCleared(int a) const { return a >= 0 && a < kAreaCount && sectorCleared[a]; }
    bool subAreaCleared(int a, int s) const {
        return a >= 0 && a < kAreaCount && s >= 0 && s < kSubAreasPerArea && subCleared[a][s];
    }
    bool subBossReady(int a, int s) const {
        return a >= 0 && a < kAreaCount && s >= 0 && s < kSubAreasPerArea &&
               subBossUnlocked[a][s];
    }
    bool titleUnlocked(int a) const {
        return a >= 0 && a < kAreaCount && (titlesUnlocked & (1u << a)) != 0;
    }
};

// One species' deepest DeepWeb Dive. Kept per species rather than as one device
// high-water mark, so the record outlives the pet that set it, "take three different
// species deep" is answerable, and a line's record is the max over its members.
struct SpeciesDive {
    const CreatureDef* def = nullptr;
    int depth = 0;
};

// What the operator has ever held, raised and taken deep. Borrowed registry pointers,
// like OperatorReveals::seenCreatures. Nothing here is ever removed: spending an item
// does not un-collect it, and a species evolved past stays raised.
struct OperatorShelves {
    std::vector<const ItemDef*> collectedItems;
    std::vector<SpeciesDive> speciesDives;
    std::vector<const CreatureDef*> raisedCreatures;

    bool itemCollected(const char* id) const;
    bool creatureRaised(const char* id) const;
    int deepestDive(const char* creatureId) const;   // 0 = never dived
    int deepestDiveEver() const;
    // Record `depth` against `c` if it beats that species' standing record. True when
    // the shelf changed.
    bool recordDive(const CreatureDef* c, int depth);
};

// The Cant (core/model/cant.h) and the SHAKES that buy it. A sigil costs a captured
// handshake, so the learned set, the lifetime capture count and what has been paid out
// of it are the whole of what the guardian ladder remembers. Unspent is derived.
struct OperatorCant {
    SigilSet sigils = 0;
    int handshakesSeen = 0;   // distinct WPA handshakes captured, deduped by BSSID
    int shakesSpent = 0;      // never above handshakesSeen

    int shakesUnspent() const {
        const int n = handshakesSeen - shakesSpent;
        return n > 0 ? n : 0;
    }
    // The next letter in the fixed reveal order (learnSigil), so "four sigils" names the
    // same four letters wherever they were learned.
    void learnNextSigil() { sigils = learnSigil(sigils); }
};

// The ARCH shelf. A rack pet is kept as Pet::stored wrote it: it does not age while
// stored, nothing reads it but the shelf's rows, and Pet::thawed rebuilds it on Deploy.
// A record is the lasting trace of a pet that died or was sold, and holds no slot.
struct OperatorArch {
    std::vector<SaveStoredPet> rack;
    std::vector<SaveRecord> records;
};

struct Operator {
    // --- Who they are on the 'net -----------------------------------------------
    char hackerTag[kHackerTagMax + 1] = "NETRUNNER_99";
    // Rank is stored beside the count it derives from, so a threshold retune never
    // demotes a save. The per-network history is the SD-backed NetworkLedger.
    int networksSeen = 0;
    int hackerRank = 0;
    // A crew needs a home network: joinCrew refuses without one, and loading re-applies
    // that. crewIndex is a kCrews row, -1 = none; the save stores the crew's id.
    int crewIndex = -1;
    uint64_t homeNetworkKey = 0;   // packed 48-bit BSSID, 0 = none
    char homeNetworkName[kSaveNetNameCap] = {0};
    int petsRaised = 0;            // eggs hatched
    uint32_t uptimeBase = 0;       // uptime banked by earlier boots (lifetimeUptimeMs)
    uint32_t lifetimeSteps = 0;    // explore steps, every pet and run

    // --- What they hold ----------------------------------------------------------
    int bits = kStartBits;
    ModPool modPool = ModPool::starting();   // the spares no pet has installed
    RigAccount rig;

    OperatorTallies tallies;
    OperatorReveals reveals;
    OperatorWorld world;
    OperatorShelves shelves;
    OperatorCant cant;
    OperatorArch arch;
    // The chosen habitat background, SceneId::None for AUTO. Owning one is derived
    // (Game::backgroundOwned), so only the pick is kept.
    SceneId backgroundPick = SceneId::None;

    // This record as the save keeps it, at game-ms `nowMs`. A spare mod the running
    // content no longer defines has no wire to write to and is dropped.
    SaveOperator saved(const ContentRegistry& reg, uint32_t nowMs) const;
    // The record a save holds. Ids the running content no longer defines are dropped (a
    // record keeps its id, since the shelf resolves an unknown one to "?"), and an older
    // blob's layouts are migrated. `activeMods` is the active pet's installed mods, which
    // a pre-v17 save still listed among the spares.
    static Operator loaded(const SaveOperator& s, const SaveId (&activeMods)[kModSlots],
                           const ContentRegistry& reg);
};

}  // namespace mal
