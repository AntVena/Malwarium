#include "core/model/save.h"

#include <array>
#include <cstring>

#include "core/content/areas/area_defs.h"   // kSubAreasPerArea: subRefarm's row width

namespace mal {

namespace {

// 4-byte magic ('M','A','L','1'): identifies a Malwarium save blob.
constexpr uint8_t kMagic[4] = {'M', 'A', 'L', '1'};

// --- The two directions of one stream ---------------------------------------
// codec() below names every field once, in wire order, and runs over either of these.
// Both offer the same verbs; a Writer emits the field it is handed, a Reader fills it.
// `since(v)` asks whether the stream carries the tail a version added: always, when
// writing the current version.

struct Writer {
    static constexpr bool kReading = false;
    std::vector<uint8_t>& out;
    bool ok = true;

    bool since(uint16_t) const { return true; }

    template <class T> void u8(const T& v) { out.push_back(static_cast<uint8_t>(v)); }
    template <class T> void u16(const T& v) {
        const uint16_t x = static_cast<uint16_t>(v);
        out.push_back(static_cast<uint8_t>(x));
        out.push_back(static_cast<uint8_t>(x >> 8));
    }
    template <class T> void u32(const T& v) {
        const uint32_t x = static_cast<uint32_t>(v);
        for (int i = 0; i < 4; ++i) out.push_back(static_cast<uint8_t>(x >> (8 * i)));
    }
    template <class T> void u64(const T& v) {
        const uint64_t x = static_cast<uint64_t>(v);
        for (int i = 0; i < 8; ++i) out.push_back(static_cast<uint8_t>(x >> (8 * i)));
    }
    template <class T> void i32(const T& v) { u32(static_cast<int32_t>(v)); }
    // A 48-bit key (a BSSID): the low u32, then the next u16.
    void u48(const uint64_t& k) { u32(static_cast<uint32_t>(k)); u16(static_cast<uint16_t>(k >> 32)); }
    // A fixed character cell, written whole.
    void raw(const char* p, int n) {
        out.insert(out.end(), reinterpret_cast<const uint8_t*>(p),
                   reinterpret_cast<const uint8_t*>(p) + n);
    }
    // A fixed character cell the reader terminates (every id and text field).
    template <size_t N> void id(const char (&s)[N]) { raw(s, static_cast<int>(N)); }
    // A byte kept only to hold its place in the stream.
    void reserved8() { u8(0); }

    // A u16 element count: the size of what is about to be written.
    uint16_t count(size_t n) { u16(n); return static_cast<uint16_t>(n); }
    // The elements of a run whose count went first (count above).
    template <class V, class F> void fill(const V& v, uint16_t, F each) {
        for (const auto& e : v) each(e);
    }
    // A length-prefixed run.
    template <class V, class F> void list(const V& v, F each) { fill(v, count(v.size()), each); }
    // A length-prefixed run over a fixed array.
    template <class T, size_t N, class F> void row(const T (&a)[N], F each) {
        count(N);
        for (const T& e : a) each(e);
    }
    // A run parallel to the stored rack, one entry per rack pet in rack order.
    template <class F> void rack(const std::vector<SaveStoredPet>& r, F each) { list(r, each); }
};

// Bounds-checked: a read past the end zeroes what it fills and clears `ok`, and every
// run stops at the first failed read, so a corrupt count cannot grow a vector past
// what the blob holds. deserializeSave rejects any stream that ends !ok.
struct Reader {
    static constexpr bool kReading = true;
    const uint8_t* p;
    size_t left;
    uint16_t version;
    bool ok = true;

    bool since(uint16_t v) const { return version >= v; }

    uint8_t byte() {
        if (left < 1) { ok = false; return 0; }
        --left; return *p++;
    }
    uint16_t take16() { uint16_t lo = byte(), hi = byte(); return static_cast<uint16_t>(lo | (hi << 8)); }
    uint32_t take32() {
        uint32_t v = 0;
        for (int i = 0; i < 4; ++i) v |= static_cast<uint32_t>(byte()) << (8 * i);
        return v;
    }
    uint64_t take64() {
        uint64_t v = 0;
        for (int i = 0; i < 8; ++i) v |= static_cast<uint64_t>(byte()) << (8 * i);
        return v;
    }

    template <class T> void u8(T& v) { v = static_cast<T>(byte()); }
    template <class T> void u16(T& v) { v = static_cast<T>(take16()); }
    template <class T> void u32(T& v) { v = static_cast<T>(take32()); }
    template <class T> void u64(T& v) { v = static_cast<T>(take64()); }
    template <class T> void i32(T& v) { v = static_cast<T>(static_cast<int32_t>(take32())); }
    void u48(uint64_t& k) {
        k = take32();
        k |= static_cast<uint64_t>(take16()) << 32;
    }
    void raw(char* dst, int n) {
        if (left < static_cast<size_t>(n)) { ok = false; std::memset(dst, 0, n); return; }
        std::memcpy(dst, p, n);
        p += n; left -= n;
    }
    template <size_t N> void id(char (&s)[N]) { raw(s, static_cast<int>(N)); s[N - 1] = '\0'; }
    void reserved8() { byte(); }

    uint16_t count(size_t) { return take16(); }
    template <class V, class F> void fill(V& v, uint16_t n, F each) {
        v.clear();
        for (uint16_t i = 0; i < n && ok; ++i) {
            typename V::value_type e{};
            each(e);
            v.push_back(e);
        }
    }
    template <class V, class F> void list(V& v, F each) { fill(v, count(0), each); }
    // Entries past the array are read and dropped; cells past the run stay as they were.
    template <class T, size_t N, class F> void row(T (&a)[N], F each) {
        const uint16_t n = count(0);
        for (uint16_t i = 0; i < n && ok; ++i) {
            if (i < N) { each(a[i]); continue; }
            T spare{};
            each(spare);
        }
    }
    // Mapped onto the rack by index: an entry past the rack is read and dropped.
    template <class F> void rack(std::vector<SaveStoredPet>& r, F each) {
        const uint16_t n = count(0);
        for (uint16_t i = 0; i < n && ok; ++i) {
            if (i < r.size()) { each(r[i]); continue; }
            SaveStoredPet spare;
            each(spare);
        }
    }
};

// The retired-id table. Adding, flattening and retiring a row: save.h's `renamedIds`.
// std::array rather than a plain C array so a zero-row table is still well-formed.
constexpr std::array<RenamedId, 3> kRenamedIds{{
    // v54: the everyday snack lost the ghost cure to Unlinkguine and was renamed with
    // it, so the id stopped describing the row (items/kit.h). The FIRST item id
    // to be renamed: every row before this one was a creature.
    {"airgap_snack", "dyno_nuggets", 54},
    // v70: the dish is a bruschetta (a Buguette under oil, garlic and tomato), and its
    // old name's tech half, ChromeOS's Linux container, was too obscure to land.
    {"crostini", "brusshetta", 70},
    // v73: the Trojan Daemon shipped under a stand-in id before it had a name.
    {"placeholder_daemon", "leatherbackdoor", 73},
}};

// The newest version any row still rewrites: a blob at or above it needs no pass.
constexpr uint16_t newestRenameVersion() {
    uint16_t v = 0;
    for (const RenamedId& r : kRenamedIds)
        if (r.sinceVersion > v) v = r.sinceVersion;
    return v;
}

void renameId(char* id, uint16_t version) {
    for (const RenamedId& r : kRenamedIds)
        if (version < r.sinceVersion && std::strcmp(id, r.from) == 0) {
            std::memset(id, 0, kSaveIdCap);          // the whole cell, not just the tail
            std::strncpy(id, r.to, kSaveIdCap - 1);
            return;                                  // flattened table -> one hop is enough
        }
}

// Every field that stores a content id a row might rewrite. The creature fields came
// first; the two ITEM-id fields joined them at v54, when the first item was renamed.
// Both item fields matter and for different reasons: `items` is what the operator is
// holding, and `collectedItems` is the ever-held set the earn-path achievements read,
// so missing it would quietly un-collect an item the player had already met.
//
// Mod and move ids have their own fields and are untouched, since none has been
// renamed. The table is flattened (no `to` is ever a `from`), so one pass over each
// is enough.
void renameRetiredIds(SaveData& d, uint16_t version) {
    renameId(d.active.id, version);
    for (SaveStoredPet& p : d.op.rack) renameId(p.id, version);
    for (SaveRecord& r : d.op.records) renameId(r.id, version);
    for (SaveId& s : d.op.seenCreatures) renameId(s.id, version);
    for (SaveId& s : d.op.raisedCreatures) renameId(s.id, version);
    for (SaveId& s : d.op.speciesDiveIds) renameId(s.id, version);
    for (SaveStack& s : d.items) renameId(s.id, version);
    for (SaveId& s : d.op.collectedItems) renameId(s.id, version);
    renameId(d.op.diveBellItem.id, version);
    renameId(d.op.diveDeviceItem.id, version);
    for (SaveId& s : d.active.foodsEaten) renameId(s.id, version);
}

// The mid-ladder splice table. Adding and retiring a row: save.h's `ladderInserts`.
// Empty since kOldestAcceptedVersion passed 43: the NET-SEA CROSSING row retired the
// moment no blob the codec still opens could predate the splice. std::array rather
// than a plain C array so a zero-row table is still well-formed.
constexpr std::array<LadderInsert, 0> kLadderInserts{};

// The newest version any row still shifts: a blob at or above it needs no pass.
constexpr uint16_t newestLadderInsertVersion() {
    uint16_t v = 0;
    for (const LadderInsert& l : kLadderInserts)
        if (l.sinceVersion > v) v = l.sinceVersion;
    return v;
}

// Open one blank rung at `at` in every ladder-indexed field. A vector shorter than `at`
// is left alone rather than padded: it simply never recorded that far, and the loaders
// already read a short vector as "nothing cleared past its end".
void openLadderRung(SaveData& d, int at) {
    if (at < 0) return;
    auto openByte = [at](std::vector<uint8_t>& v) {
        if (at <= static_cast<int>(v.size())) v.insert(v.begin() + at, 0);
    };
    openByte(d.op.sectorCleared);
    openByte(d.op.bossUnlocked);
    openByte(d.op.subCleared);        // one bitmask byte per area, so still one cell
    openByte(d.op.subBossUnlocked);
    // subRefarm is row-major with kSubAreasPerArea counts per area: a whole ROW opens.
    const size_t row = static_cast<size_t>(at) * kSubAreasPerArea;
    std::vector<uint16_t>& refarm = d.activeArmed.subRefarm;
    if (row <= refarm.size()) refarm.insert(refarm.begin() + row, kSubAreasPerArea, 0);
    // titlesUnlocked is a bitmask indexed by rung: everything at or above `at` moves up
    // one bit, leaving the new rung's Title correctly unearned.
    if (at < 32) {
        const uint32_t below = d.op.titlesUnlocked & ((1u << at) - 1u);
        const uint32_t atOrAbove = d.op.titlesUnlocked & ~((1u << at) - 1u);
        d.op.titlesUnlocked = below | (atOrAbove << 1);
    }
    // equippedTitle is a rung index (or -1 for none), so it moves with the Title it names.
    if (d.op.equippedTitle >= at) ++d.op.equippedTitle;
}

void applyLadderInserts(SaveData& d, uint16_t version) {
    // Oldest-first, so each row's atIndex reads against the ladder its predecessors built.
    for (const LadderInsert& l : kLadderInserts)
        if (version < l.sinceVersion) openLadderRung(d, l.atIndex);
}

// migrateRecipeRows (the v49 cooking-leaves-the-Rig-Shop migration, folding the two
// mid-table Browns rig rows into the recipe bitset) retired along with
// kOldestAcceptedVersion passing 49: see save.h's v49 note for what it used to do.

}  // namespace

const RenamedId* renamedIds(int& count) {
    count = static_cast<int>(kRenamedIds.size());
    return kRenamedIds.data();
}

const LadderInsert* ladderInserts(int& count) {
    count = static_cast<int>(kLadderInserts.size());
    return kLadderInserts.data();
}

namespace {

// v72's rack palates: a dictionary of every food any rack pet has eaten, then one bitset
// per pet over it. The one tail whose two directions are different code: the writer
// builds the dictionary, the reader expands it.
void palates(Writer& w, const std::vector<SaveStoredPet>& rack) {
    std::vector<const SaveId*> dict;
    auto dictIndex = [&](const SaveId& s) -> int {
        for (size_t i = 0; i < dict.size(); ++i)
            if (std::strncmp(dict[i]->id, s.id, kSaveIdCap) == 0) return static_cast<int>(i);
        return -1;
    };
    for (const SaveStoredPet& p : rack)
        for (const SaveId& s : p.foodsEaten)
            if (dictIndex(s) < 0) dict.push_back(&s);
    w.count(dict.size());
    for (const SaveId* s : dict) w.id(s->id);
    w.count(rack.size());
    for (const SaveStoredPet& p : rack) {
        std::vector<uint8_t> bits((dict.size() + 7) / 8, 0);
        for (const SaveId& s : p.foodsEaten) {
            const int i = dictIndex(s);
            bits[static_cast<size_t>(i) / 8] |= static_cast<uint8_t>(1u << (i % 8));
        }
        w.list(bits, [&](uint8_t b) { w.u8(b); });
    }
}

void palates(Reader& r, std::vector<SaveStoredPet>& rack) {
    std::vector<SaveId> dict;
    r.list(dict, [&](SaveId& s) { r.id(s.id); });
    const uint16_t nPal = r.count(0);
    for (uint16_t i = 0; i < nPal && r.ok; ++i) {
        const uint16_t nBytes = r.count(0);
        for (uint16_t b = 0; b < nBytes && r.ok; ++b) {
            const uint8_t byte = r.byte();
            for (int bit = 0; bit < 8; ++bit) {
                const size_t k = static_cast<size_t>(b) * 8 + bit;
                if (((byte >> bit) & 1) && k < dict.size() && i < rack.size())
                    rack[i].foodsEaten.push_back(dict[k]);
            }
        }
    }
}

// --- The format ---------------------------------------------------------------
// One function, so each tail's fields are written and read by the same lines and the two
// directions cannot drift. `D` is `const SaveData` when writing and `SaveData` when
// reading; the few places the directions genuinely differ say so with `IO::kReading`.
//
// THE STREAM IS APPEND-ONLY. Every field a later version adds goes on the end behind
// its own `since(N)`, and nothing mid-stream is ever removed: a retired field keeps its
// bytes (a reserved byte, an empty run) so every older reader still finds what follows.
// kOldestAcceptedVersion is 53, so every tail up to v53 is in every accepted blob and
// carries no gate.
//
// The active pet's and the Operator's fields go where each version first put them,
// not in records of their own: see SaveData::active and SaveOperator.
template <class IO, class D>
void codec(IO& io, D& d) {
    auto& a = d.active;
    auto& armed = d.activeArmed;
    auto& op = d.op;
    auto idCell = [&](auto& s) { io.id(s.id); };
    auto u8 = [&](auto& v) { io.u8(v); };
    auto u16 = [&](auto& v) { io.u16(v); };
    auto i32 = [&](auto& v) { io.i32(v); };

    // v1: the active pet, the operator, the bag, the mod spares, the slots, the log and
    // the rack's records.
    io.id(a.id);
    io.i32(a.hunger); io.i32(a.frag); io.i32(a.happy);
    io.i32(a.mistakes); io.i32(a.debuffs);
    io.u8(a.ghost);
    io.u32(a.timeInStageMs);
    io.i32(a.generation);
    io.i32(op.bits);
    io.id(op.hackerTag);
    io.u32(op.lifetimeUptimeMs);
    io.u32(op.lifetimeSteps);
    io.i32(op.petsRaised);
    io.list(d.items, [&](auto& s) { io.id(s.id); io.i32(s.qty); });
    // The pre-v45 spare pool, one id cell per COPY. Game::captureSave leaves it empty
    // (the pool ships as ownedModCounts in the v45 tail), but the run keeps its place.
    io.list(op.ownedMods, idCell);
    io.row(a.equippedMods, idCell);
    io.list(d.log, [&](auto& e) { io.u8(e.type); io.id(e.text); });
    io.list(op.rack, [&](auto& p) {
        io.id(p.id);
        io.i32(p.hunger); io.i32(p.frag); io.i32(p.happy);
        io.i32(p.mistakes); io.i32(p.debuffs);
        io.u8(p.ghost);
        io.i32(p.generation);
    });

    // v2: the move loadout and combat XP/level.
    io.list(a.ownedMoves, idCell);
    io.row(a.equippedMoves, idCell);
    io.i32(a.combatXp);
    io.i32(a.combatLevel);

    // v3: the ARCH records (RETIRED/CORRUPTED).
    io.list(op.records, [&](auto& rec) { io.id(rec.id); io.u8(rec.status); io.i32(rec.generation); });

    // v4: Hacker Rank.
    io.i32(op.networksSeen);
    io.i32(op.hackerRank);
    // v5, v6: the audit scan and capture toggles.
    io.u8(d.settings.netScanEnabled);
    io.u8(d.auditCaptureEnabled);
    // v7: the SHAKES ledger's save-blob fallback and the lifetime handshake count. Each
    // BSSID is a 48-bit key.
    io.i32(op.handshakesSeen);
    io.list(d.seenHandshakeBssids, [&](auto& k) { io.u48(k); });
    // v8: the EXPL sector-clear flags.
    io.list(op.sectorCleared, u8);
    // v9: the Boot-Sector incubation clock.
    io.u32(a.bootHatchRemainMs);
    // v10: the zone Titles: unlocked mask and equipped index.
    io.u32(op.titlesUnlocked);
    io.i32(op.equippedTitle);
    // v11: the creature-level stat points.
    io.row(a.statPoints, i32);
    // v12, v13: the per-area boss unlocks, and the per-sub-area clear and boss bitmasks.
    io.list(op.bossUnlocked, u8);
    io.list(op.subCleared, u8);
    io.list(op.subBossUnlocked, u8);
    // v14: screen brightness.
    io.i32(d.settings.brightness);
    // v15: the per-sub-area re-farm win counts, flat row-major.
    io.list(armed.subRefarm, u16);
    // v16: the defrag tally, the active pet's then the rack's.
    io.i32(a.defragCount);
    io.rack(op.rack, [&](auto& p) { io.i32(p.defragCount); });
    // v17 carries no bytes: the version alone says `ownedMods` excludes the equipped mods.
    if constexpr (IO::kReading) op.hasPermanentModData = true;
    // v18: the per-spare equip-level gates, parallel to ownedMods. Empty from v45 on.
    io.list(op.ownedModReqLevels, i32);
    if constexpr (IO::kReading) op.hasModEquipLevelData = io.ok;
    // v19 to v23: the SHOP's account upgrades, the AP toggle, the per-pet shield and
    // once-per-life gates, and the SHOP's one-time unlocks.
    io.i32(op.bwUpgradeCount);
    io.u8(d.settings.apEnabled);
    io.u8(a.shieldActive);
    io.u8(a.shieldSpent);
    io.u8(a.yubiSpent);
    io.u8(op.fragAmountTier);
    io.u8(op.fragTriggerTier);
    io.u8(op.itemTabsUnlocked);
    io.u8(op.bulkOpenUnlocked);
    // v24: the per-slot Attack/Defend typing. Unset slots are re-derived on load
    // (Game::stampSlotKinds).
    io.row(a.slotKinds, u8);
    // v25: the 'Pedia reveal state. The u32 achievements mask is superseded by the v40
    // bitset; Game::applySave reads it only when that is absent.
    io.list(op.seenCreatures, idCell);
    io.u16(op.malbeastSeen);
    io.u16(op.malbeastDefeated);
    io.u32(op.achievementsMask);
    // v26: the rack's level, XP, stat points and slot typing.
    io.rack(op.rack, [&](auto& p) {
        io.i32(p.combatLevel);
        io.i32(p.combatXp);
        for (auto& v : p.statPoints) io.i32(v);
        for (auto& k : p.slotKinds) io.u8(k);
    });
    // v27: the rig-upgrade levels.
    io.i32(op.rackSlotUpgradeCount);
    io.i32(op.scrapingClusterLevel);
    io.i32(op.dataMiningLevel);
    // v28: the Ambig-USB divert.
    io.u8(armed.trojanDivert);
    // v29: the rack's move and mod loadouts.
    io.rack(op.rack, [&](auto& p) {
        io.list(p.ownedMoves, idCell);
        for (auto& m : p.equippedMoves) io.id(m.id);
        for (auto& m : p.equippedMods) io.id(m.id);
    });
    // v30: the Backup Drive's deadline.
    io.u32(armed.backupShieldUntilMs);
    // v31: the Merge Hub. The recipe u32 is the first four bytes of the v51 bitset.
    io.u8(op.mergeHubUnlocked);
    io.u32(op.recipesUnlocked);
    // v32: Rig Shop rows beyond the first 11.
    io.list(op.rigLevelsExt, u16);
    // v34: the rack's in-stage clocks.
    io.rack(op.rack, [&](auto& p) { io.u32(p.timeInStageMs); });
    // v35: the deepest DeepWeb Dive, the active pet's then the rack's.
    io.i32(a.bestDeepWebDepth);
    io.rack(op.rack, [&](auto& p) { io.i32(p.bestDeepWebDepth); });
    // v36: the crew and its home network. Raw cells: the reader keeps all their bytes.
    io.raw(op.crewId, kSaveIdCap);
    io.u64(op.homeNetworkKey);
    io.raw(op.homeNetworkName, kSaveNetNameCap);
    // v37: the LINK opt-in.
    io.u8(d.settings.linkEnabled);
    // v38: reserved. Read and never acted on: a save that still carries the old internet
    // opt-in must not put this device back on the 'net.
    io.reserved8();
    // v39: the raised-species tally. Game::applySave unions in every species the rest of
    // the save points at.
    io.list(op.raisedCreatures, idCell);
    // v40: the achievement bitsets and counters, and the per-species dive depths as
    // parallel id and depth runs. Game::applySave migrates a blob without them.
    io.list(op.achievementEarned, u8);
    io.list(op.achievementNotified, u8);
    io.i32(op.bossWins);
    io.list(op.collectedItems, idCell);
    {
        const uint16_t n = io.count(op.speciesDiveIds.size());
        io.fill(op.speciesDiveIds, n, idCell);
        io.fill(op.speciesDiveDepths, n, i32);
    }
    // v42: the 5/5 window already burned, the active pet's then the rack's.
    io.u32(a.dyingElapsedMs);
    io.rack(op.rack, [&](auto& p) { io.u32(p.dyingElapsedMs); });
    // v44: Stacker boards cleared by hand.
    io.i32(op.stackerWins);
    // v45: the counted mod pool, a nibble per ModDef::wire. Without it the loader tallies
    // the flat v1 `ownedMods` list instead.
    io.list(op.ownedModCounts, u8);
    if constexpr (IO::kReading) op.hasModCountData = io.ok;
    // v47: the arcade's per-cabinet tallies, as parallel id, plays and wins runs.
    {
        const uint16_t n = io.count(op.arcadeIds.size());
        io.fill(op.arcadeIds, n, idCell);
        io.fill(op.arcadePlays, n, i32);
        io.fill(op.arcadeWins, n, i32);
    }
    // v48: the DECRYPTOGRAM's per-quote state, two bits per QuoteDef::wire.
    io.list(op.quoteStates, u8);
    // v50: the Bandwidth-regen grant, the active pet's then the rack's.
    io.i32(a.bandwidthRegenBonusMin);
    io.rack(op.rack, [&](auto& p) { io.i32(p.bandwidthRegenBonusMin); });
    // v51: the whole owned-recipe bitset.
    io.list(op.recipeOwned, u8);
    // v53: ROCK THE DOCK's run: the seed the bracket derives from, and what it cannot.
    io.u32(d.tourney.seed);
    io.u8(d.tourney.alive);
    io.u8(d.tourney.round);
    io.u8(d.tourney.phase);

    // Every tail from here is absent from a blob older than the version that added it,
    // and reads back as the SaveData default.
    if (io.since(55)) io.list(op.arcadeBest, i32);   // positional with v47's arcadeIds
    if (io.since(56)) {
        io.i32(op.tourneyWins);
        io.i32(op.pvpWins);
        io.i32(op.mergesCooked);
    }
    if (io.since(57)) {   // the off-level stat points and XP rate an Epic dish grants
        for (auto& v : a.statBonus) io.i32(v);
        io.i32(a.xpRateBonusPct);
        io.rack(op.rack, [&](auto& p) {
            for (auto& v : p.statBonus) io.i32(v);
            io.i32(p.xpRateBonusPct);
        });
    }
    if (io.since(58)) io.u8(op.backgroundPick);
    if (io.since(59)) {   // the CANT
        io.u32(op.cantSigils);
        io.i32(op.shakesSpent);
    }
    if (io.since(60)) {   // the USB port
        io.u8(a.usbBranch);
        io.u8(a.usbSoak);
        io.u8(a.usbHold);
        // A MULTIPLIER on both the evolution dwell and every XP award, so a 0 from a
        // damaged blob would stop the pet learning anything at all.
        if constexpr (IO::kReading)
            if (a.usbSoak < 1) a.usbSoak = 1;
    }
    if (io.since(61)) io.u32(op.rigServicesOff);
    if (io.since(62)) io.list(a.foodsEaten, idCell);
    if (io.since(63)) io.i32(a.bestDarkWebDepth);
    if (io.since(64)) io.id(d.settings.theme);   // by NAME; Game::load resolves it
    if (io.since(65)) io.list(op.storyRead, u8);
    if (io.since(66)) io.list(op.tipsSeen, u8);
    if (io.since(67)) {
        io.u8(d.settings.uiMode);
        io.u8(d.settings.careAlerts);
    }
    if (io.since(68)) {
        io.u8(d.settings.soundMode);
        io.u8(d.settings.volume);
    }
    if (io.since(69)) io.u32(op.rigServicesAsk);
    if (io.since(71)) io.u8(d.settings.exploreSound);
    if (io.since(72)) {   // favourite dishes, and the rack's palates
        io.id(a.favouriteFood);
        io.u8(a.favouriteFound);
        io.rack(op.rack, [&](auto& p) {
            io.id(p.favouriteFood);
            io.u8(p.favouriteFound);
        });
        palates(io, op.rack);
    }
    if (io.since(74)) {   // Bond, the active pet's then the rack's
        for (auto& bp : a.bondBp) io.u16(bp);
        io.rack(op.rack, [&](auto& p) { for (auto& bp : p.bondBp) io.u16(bp); });
    }
    if (io.since(75)) {   // the rack's remaining kept fields
        io.rack(op.rack, [&](auto& p) {
            io.i32(p.bestDarkWebDepth);
            io.u8(p.shieldActive);
            io.u8(p.shieldSpent);
            io.u8(p.yubiSpent);
            io.u8(p.usbBranch);
            io.u8(p.usbSoak);
            io.u8(p.usbHold);
            io.u32(p.bootHatchRemainMs);
        });
    }
    if (io.since(76)) {   // the build that wrote this, and the FRESH PATCH deadline
        io.u32(d.firmwareCode);
        io.u32(armed.patchBuffUntilMs);
    }
    if (io.since(77)) io.u8(op.autoProgress);
    if (io.since(78)) {   // the DIVE PREP loadout
        io.u8(op.diveAutoRedive);
        io.u8(op.diveBellKind);
        io.id(op.diveBellItem.id);
        io.u8(op.diveDeviceKind);
        io.id(op.diveDeviceItem.id);
    }
    if (io.since(79)) io.u8(op.useEncounterBypass);
}

}  // namespace

void serializeSaveInto(const SaveData& d, std::vector<uint8_t>& out) {
    // Empty the buffer without giving its memory back: a caller that hands the same
    // buffer to every save (Game::persistSave) then writes each one into an allocation
    // it already holds. Reserve the whole blob up front for a caller that doesn't. A
    // 256-byte start reaches a real ~15KB save by doubling seven times, and every step
    // allocates the new block while the old one is still live, so the peak demand is
    // ~1.5x the blob in ONE contiguous piece, paid at whatever moment the autosave
    // timer happens to fire. On the device that moment can land while the radio is
    // handing ownership over (the audit capture claims ~95KB as it arms), and an
    // allocation that fails there does not fail softly: the Arduino build has
    // exceptions off, so operator new's bad_alloc goes straight to std::terminate and
    // resets the device. Growth past the reservation still works: floor, not cap.
    out.clear();
    if (out.capacity() < kSaveReserveBytes) out.reserve(kSaveReserveBytes);
    Writer w{out};
    w.raw(reinterpret_cast<const char*>(kMagic), 4);
    w.u16(kSaveVersion);
    codec(w, d);
}

std::vector<uint8_t> serializeSave(const SaveData& d) {
    std::vector<uint8_t> out;
    serializeSaveInto(d, out);
    return out;
}

bool deserializeSave(const std::vector<uint8_t>& blob, SaveData& out) {
    out = SaveData{};  // default = empty on any failure
    if (blob.size() < 6) return false;
    if (std::memcmp(blob.data(), kMagic, 4) != 0) return false;
    const uint16_t version = static_cast<uint16_t>(blob[4] | (blob[5] << 8));
    // Accept every version still in support, in BOTH directions.
    //
    // Older is the easy half (forward-compat): a v(N-1) blob simply lacks the newer
    // appended tails, which load with defaults.
    //
    // NEWER is the half that matters on a device. An OTA image boots once on trial and
    // the bootloader reverts it if the device resets before it commits, so the previous
    // firmware can find itself underneath a save the newer one already rewrote. Refusing
    // that blob does not fail safe: it deserializes as empty, and an empty save is a
    // fresh hatch with the pet, the rack and the records gone. Reading the prefix we
    // understand and ignoring a tail we don't loses only what the newer build added,
    // which is the worst a rollback should ever cost.
    //
    // What that rests on is codec()'s append-only rule. A version that inserts or drops
    // a field mid-stream breaks it and must raise kOldestAcceptedVersion instead. `ok` is
    // the backstop either way: a stream that desyncs runs short and is rejected below.
    if (version < kOldestAcceptedVersion) return false;

    SaveData d;
    Reader r{blob.data() + 6, blob.size() - 6, version};
    codec(r, d);
    if (!r.ok) return false;  // truncated -> empty
    if (version < newestRenameVersion()) renameRetiredIds(d, version);
    // After every tail is read, never between them: the shift moves whole ladder-indexed
    // vectors, so it has to see them at their final lengths.
    if (version < newestLadderInsertVersion()) applyLadderInserts(d, version);
    out = std::move(d);
    return true;
}

} // namespace mal
