#include "core/model/save.h"

#include <array>
#include <cstring>

#include "core/content/areas/area_defs.h"   // kSubAreasPerArea — subRefarm's row width

namespace mal {

namespace {

// 4-byte magic ('M','A','L','1') — identifies a Malwarium save blob.
constexpr uint8_t kMagic[4] = {'M', 'A', 'L', '1'};

// --- Little-endian stream writer -------------------------------------------
struct Writer {
    std::vector<uint8_t>& out;
    void u8(uint8_t v) { out.push_back(v); }
    void u16(uint16_t v) {
        out.push_back(static_cast<uint8_t>(v));
        out.push_back(static_cast<uint8_t>(v >> 8));
    }
    void u32(uint32_t v) {
        for (int i = 0; i < 4; ++i) out.push_back(static_cast<uint8_t>(v >> (8 * i)));
    }
    void u64(uint64_t v) {
        for (int i = 0; i < 8; ++i) out.push_back(static_cast<uint8_t>(v >> (8 * i)));
    }
    void i32(int32_t v) { u32(static_cast<uint32_t>(v)); }
    void bytes(const char* p, int n) {
        out.insert(out.end(), reinterpret_cast<const uint8_t*>(p),
                   reinterpret_cast<const uint8_t*>(p) + n);
    }
};

// --- Bounds-checked stream reader ------------------------------------------
struct Reader {
    const uint8_t* p;
    size_t left;
    bool ok = true;
    uint8_t u8() {
        if (left < 1) { ok = false; return 0; }
        --left; return *p++;
    }
    uint16_t u16() {
        uint16_t lo = u8(), hi = u8();
        return static_cast<uint16_t>(lo | (hi << 8));
    }
    uint32_t u32() {
        uint32_t v = 0;
        for (int i = 0; i < 4; ++i) v |= static_cast<uint32_t>(u8()) << (8 * i);
        return v;
    }
    uint64_t u64() {
        uint64_t v = 0;
        for (int i = 0; i < 8; ++i) v |= static_cast<uint64_t>(u8()) << (8 * i);
        return v;
    }
    int32_t i32() { return static_cast<int32_t>(u32()); }
    void bytes(char* dst, int n) {
        if (left < static_cast<size_t>(n)) { ok = false; std::memset(dst, 0, n); return; }
        std::memcpy(dst, p, n);
        p += n; left -= n;
    }
};

void writeStack(Writer& w, const SaveStack& s) {
    w.bytes(s.id, kSaveIdCap);
    w.i32(s.qty);
}
void writeId(Writer& w, const SaveId& s) { w.bytes(s.id, kSaveIdCap); }
void writeLog(Writer& w, const SaveLogEntry& e) {
    w.u8(e.type);
    w.bytes(e.text, kSaveTextCap);
}
void writeStored(Writer& w, const SaveStoredPet& p) {
    w.bytes(p.id, kSaveIdCap);
    w.i32(p.hunger); w.i32(p.frag); w.i32(p.happy);
    w.i32(p.mistakes); w.i32(p.debuffs);
    w.u8(p.ghost);
    w.i32(p.generation);
}

// The retired-id table. Adding, flattening and retiring a row: save.h's `renamedIds`.
// std::array rather than a plain C array so a zero-row table is still well-formed.
constexpr std::array<RenamedId, 1> kRenamedIds{{
    // v54: the everyday snack lost the ghost cure to Unlinkguine and was renamed with
    // it, so the id stopped describing the row (content_items.cpp). The FIRST item id
    // to be renamed — every row before this one was a creature.
    {"airgap_snack", "dyno_nuggets", 54},
}};

// The newest version any row still rewrites — a blob at or above it needs no pass.
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
// Mod and move ids ride their own fields and stay untouched — nothing has renamed one.
// The table is flattened (no `to` is ever a `from`), so one pass over each is enough.
void renameRetiredIds(SaveData& d, uint16_t version) {
    renameId(d.activeId, version);
    for (SaveStoredPet& p : d.rack) renameId(p.id, version);
    for (SaveRecord& r : d.records) renameId(r.id, version);
    for (SaveId& s : d.seenCreatures) renameId(s.id, version);
    for (SaveId& s : d.raisedCreatures) renameId(s.id, version);
    for (SaveId& s : d.speciesDiveIds) renameId(s.id, version);
    for (SaveStack& s : d.items) renameId(s.id, version);
    for (SaveId& s : d.collectedItems) renameId(s.id, version);
}

// The mid-ladder splice table. Adding and retiring a row: save.h's `ladderInserts`.
// Empty since kOldestAcceptedVersion passed 43: the NET-SEA CROSSING row retired the
// moment no blob the codec still opens could predate the splice. std::array rather
// than a plain C array so a zero-row table is still well-formed.
constexpr std::array<LadderInsert, 0> kLadderInserts{};

// The newest version any row still shifts — a blob at or above it needs no pass.
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
    openByte(d.sectorCleared);
    openByte(d.bossUnlocked);
    openByte(d.subCleared);        // one bitmask byte per area, so still one cell
    openByte(d.subBossUnlocked);
    // subRefarm is row-major with kSubAreasPerArea counts per area — a whole ROW opens.
    const size_t row = static_cast<size_t>(at) * kSubAreasPerArea;
    if (row <= d.subRefarm.size())
        d.subRefarm.insert(d.subRefarm.begin() + row, kSubAreasPerArea, 0);
    // titlesUnlocked is a bitmask indexed by rung: everything at or above `at` moves up
    // one bit, leaving the new rung's Title correctly unearned.
    if (at < 32) {
        const uint32_t below = d.titlesUnlocked & ((1u << at) - 1u);
        const uint32_t atOrAbove = d.titlesUnlocked & ~((1u << at) - 1u);
        d.titlesUnlocked = below | (atOrAbove << 1);
    }
    // equippedTitle is a rung index (or -1 for none), so it moves with the Title it names.
    if (d.equippedTitle >= at) ++d.equippedTitle;
}

void applyLadderInserts(SaveData& d, uint16_t version) {
    // Oldest-first, so each row's atIndex reads against the ladder its predecessors built.
    for (const LadderInsert& l : kLadderInserts)
        if (version < l.sinceVersion) openLadderRung(d, l.atIndex);
}

// migrateRecipeRows (the v49 cooking-leaves-the-Rig-Shop migration, folding the two
// mid-table Browns rig rows into the recipe bitset) retired along with
// kOldestAcceptedVersion passing 49 — see save.h's v49 note for what it used to do.

}  // namespace

const RenamedId* renamedIds(int& count) {
    count = static_cast<int>(kRenamedIds.size());
    return kRenamedIds.data();
}

const LadderInsert* ladderInserts(int& count) {
    count = static_cast<int>(kLadderInserts.size());
    return kLadderInserts.data();
}

void serializeSaveInto(const SaveData& d, std::vector<uint8_t>& out) {
    // Empty the buffer without giving its memory back: a caller that hands the same
    // buffer to every save (Game::persistSave) then writes each one into an allocation
    // it already holds. Reserve the whole blob up front for a caller that doesn't. A
    // 256-byte start reaches a real ~15KB save by doubling seven times, and every step
    // allocates the new block while the old one is still live — so the peak demand is
    // ~1.5x the blob in ONE contiguous piece, paid at whatever moment the autosave
    // timer happens to fire. On the device that moment can land while the radio is
    // handing ownership over (the audit capture claims ~95KB as it arms), and an
    // allocation that fails there does not fail softly: the Arduino build has
    // exceptions off, so operator new's bad_alloc goes straight to std::terminate and
    // resets the device. Growth past the reservation still works — floor, not cap.
    out.clear();
    if (out.capacity() < kSaveReserveBytes) out.reserve(kSaveReserveBytes);
    Writer w{out};
    w.bytes(reinterpret_cast<const char*>(kMagic), 4);
    w.u16(kSaveVersion);

    w.bytes(d.activeId, kSaveIdCap);
    w.i32(d.hunger); w.i32(d.frag); w.i32(d.happy);
    w.i32(d.mistakes); w.i32(d.debuffs);
    w.u8(d.ghost);
    w.u32(d.timeInStageMs);
    w.i32(d.generation);

    w.i32(d.bits);
    w.bytes(d.hackerTag, kHackerTagMax + 1);
    w.u32(d.lifetimeUptimeMs);
    w.u32(d.lifetimeSteps);
    w.i32(d.petsRaised);

    w.u16(static_cast<uint16_t>(d.items.size()));
    for (const auto& s : d.items) writeStack(w, s);
    // The pre-v45 spare pool: one id cell per COPY. captureSave stopped filling it at
    // v45 (the pool ships as ownedModCounts in the v45 tail), so from here it always
    // writes a count of zero — but it still WRITES, because this format is append-only
    // and every position in it is load-bearing. Deleting a field mid-stream would
    // desync a v45 blob from the layout every older version's reader expects, which is
    // exactly what the migration tests forge by stamping the version word down.
    w.u16(static_cast<uint16_t>(d.ownedMods.size()));
    for (const auto& m : d.ownedMods) writeId(w, m);
    w.u16(static_cast<uint16_t>(d.equipped.size()));
    for (const auto& e : d.equipped) writeId(w, e);
    w.u16(static_cast<uint16_t>(d.log.size()));
    for (const auto& e : d.log) writeLog(w, e);
    w.u16(static_cast<uint16_t>(d.rack.size()));
    for (const auto& p : d.rack) writeStored(w, p);

    // v2: the move loadout + combat XP/level (appended after the v1 stream).
    w.u16(static_cast<uint16_t>(d.ownedMoves.size()));
    for (const auto& m : d.ownedMoves) writeId(w, m);
    w.u16(static_cast<uint16_t>(d.equippedMoves.size()));
    for (const auto& e : d.equippedMoves) writeId(w, e);
    w.i32(d.combatXp);
    w.i32(d.combatLevel);

    // v3: the ARCH records (RETIRED/CORRUPTED), appended after the v2 tail.
    w.u16(static_cast<uint16_t>(d.records.size()));
    for (const auto& rec : d.records) {
        w.bytes(rec.id, kSaveIdCap);
        w.u8(rec.status);
        w.i32(rec.generation);
    }

    // v4: Hacker Rank — networks-seen + the rank. The dedup pool byte this slot used
    // to carry was removed as of v33 (see save.h) — a v33+ writer never emits it.
    w.i32(d.networksSeen);
    w.i32(d.hackerRank);

    // v5: Audit-mode passive-scan runtime toggle.
    w.u8(d.netScanEnabled);

    // v6: Audit-mode handshake-capture runtime toggle.
    w.u8(d.auditCaptureEnabled);

    // v7: the real-radio Audit SHAKES ledger + the lifetime handshake count. Each
    // BSSID is a 48-bit key (low u32 + mid u16). (The sibling NETS/seenBssids vector
    // this tail used to carry was removed as of v33 — see save.h.)
    w.i32(d.handshakesSeen);
    w.u16(static_cast<uint16_t>(d.seenHandshakeBssids.size()));
    for (uint64_t k : d.seenHandshakeBssids) {
        w.u32(static_cast<uint32_t>(k));
        w.u16(static_cast<uint16_t>(k >> 32));
    }

    // v8: the EXPL sector-clear flags (one byte per sector), length-prefixed.
    w.u16(static_cast<uint16_t>(d.sectorCleared.size()));
    for (uint8_t c : d.sectorCleared) w.u8(c);

    // v9: the Boot-Sector incubation timer (remaining decrypt clock).
    w.u32(d.bootHatchRemainMs);

    // v10: the zone-completion Titles — unlocked bitmask + equipped index.
    w.u32(d.titlesUnlocked);
    w.i32(d.equippedTitle);

    // v11: the creature-level stat points, length-prefixed so the count is
    // self-describing (kLevelStatCount today). Sum equals combatLevel (the invariant).
    w.u16(static_cast<uint16_t>(d.statPoints.size()));
    for (int32_t p : d.statPoints) w.i32(p);

    // v12: the EXPL per-sector boss-unlock flags, length-prefixed.
    w.u16(static_cast<uint16_t>(d.bossUnlocked.size()));
    for (uint8_t b : d.bossUnlocked) w.u8(b);

    // v13: the EXPL per-sub-area clear + boss-unlock bitmasks — one byte per
    // area (bit s = sub-area s), each length-prefixed.
    w.u16(static_cast<uint16_t>(d.subCleared.size()));
    for (uint8_t b : d.subCleared) w.u8(b);
    w.u16(static_cast<uint16_t>(d.subBossUnlocked.size()));
    for (uint8_t b : d.subBossUnlocked) w.u8(b);

    // v14: the CFG screen-brightness level.
    w.i32(d.brightness);

    // v15: the EXPL per-sub-area re-farm win counts, length-prefixed
    // (flat row-major), each a u16 count.
    w.u16(static_cast<uint16_t>(d.subRefarm.size()));
    for (uint16_t c : d.subRefarm) w.u16(c);

    // v16: the per-pet defrag count. The active pet's tally, then a parallel
    // length-prefixed list of the rack pets' tallies (same order as d.rack), so the
    // count survives ARCH freeze/thaw without touching the mid-stream rack records.
    w.i32(d.defragCount);
    w.u16(static_cast<uint16_t>(d.rack.size()));
    for (const auto& p : d.rack) w.i32(p.defragCount);

    // v18: the per-spare rolled equip-level gates, parallel to ownedMods. Empty from
    // v45 on (the roll is gone), and written for the same append-only reason as the
    // list it parallels.
    w.u16(static_cast<uint16_t>(d.ownedModReqLevels.size()));
    for (int32_t lvl : d.ownedModReqLevels) w.i32(lvl);

    // v19: the Hacker SHOP account upgrades. One i32 for now — the
    // "Increase Bandwidth" purchase count. Appended, so a v18 reader ignores it.
    w.i32(d.bwUpgradeCount);

    // v20: the 'Pedia local-AP runtime toggle. Appended, so a
    // v19 reader ignores it; a pre-v20 blob reads back as OFF.
    w.u8(d.apEnabled);

    // v21: the per-pet care-mistake shield + once-per-lifetime item gates, and the
    // shop-pre-alloc fragAmountTier. Appended, so a v20 reader ignores it. (The full
    // 64-bit dedup mask this tail used to carry was removed as of v33 — see save.h.)
    w.u8(d.mistakeShieldActive);
    w.u8(d.shieldItemConsumed);
    w.u8(d.yubiConsumed);
    w.u8(d.fragAmountTier);

    // v22: the c "Reduce Explore Frag TRIGGER %" tier. Appended, so a v21
    // reader ignores it; a pre-v22 blob reads back as 0.
    w.u8(d.fragTriggerTier);

    // v23: the d/e one-time Hacker-SHOP unlocks. Appended, so a v22 reader
    // ignores them; a pre-v23 blob reads back as both false.
    w.u8(d.itemTabsUnlocked);
    w.u8(d.bulkOpenUnlocked);

    // v24: the move-slot rework's per-pet Attack/Defend slot typing,
    // length-prefixed like statPoints. Appended, so a v23 reader ignores it.
    w.u16(static_cast<uint16_t>(d.slotKinds.size()));
    for (uint8_t k : d.slotKinds) w.u8(k);

    // v25: the web-'Pedia reveal-state bookkeeping — seenCreatures (length-prefixed,
    // like ownedMods) and the malbeast seen/defeated bitmasks. Appended, so a v24 reader
    // ignores it.
    w.u16(static_cast<uint16_t>(d.seenCreatures.size()));
    for (const auto& s : d.seenCreatures) writeId(w, s);
    w.u16(d.malbeastSeen);
    w.u16(d.malbeastDefeated);
    // The v25 achievements u32, SUPERSEDED as of v40 by the wire-indexed bitset in the
    // v40 tail. The four bytes stay on the wire deliberately — this field sits
    // MID-STREAM, and dropping it would shift every later field, so a blob could no
    // longer be read as any earlier version. Nothing mid-stream is ever removed. The
    // codec still round-trips whatever it is handed (a faithful codec is the easier
    // thing to reason about); it is Game::captureSave that stopped filling it in, and
    // Game::applySave that only consults it when the v40 bitset is absent.
    w.u32(d.achievementsMask);

    // v26: per-stored-pet creature-level state, parallel to d.rack (mirrors
    // defragCount's v16 pattern) so a rack pet's level/stat points/slot typing
    // survive an ARCH Store/Deploy cycle instead of being silently dropped.
    w.u16(static_cast<uint16_t>(d.rack.size()));
    for (const auto& p : d.rack) {
        w.i32(p.combatLevel);
        w.i32(p.combatXp);
        for (int i = 0; i < kLevelStatCount; ++i) w.i32(p.statPoints[i]);
        for (int i = 0; i < kMaxMoveSlots; ++i) w.u8(p.slotKinds[i]);
    }

    // v27: the Hacker-SHOP rig-upgrade levels. Appended, so a v26 reader
    // ignores them; a pre-v27 blob reads back as all 0 (none bought).
    w.i32(d.rackSlotUpgradeCount);
    w.i32(d.scrapingClusterLevel);
    w.i32(d.dataMiningLevel);

    // v28: the Ambig-USB armed Trojan-divert flag. Appended, so a v27 reader
    // ignores it; a pre-v28 blob reads back as false (no divert armed).
    w.u8(d.forceTrojanDivert);

    // v29: per-stored-pet move + mod loadout, parallel to d.rack (mirrors v26's
    // pattern) so a rack pet's MOVES/MODS state survives an ARCH Store/Deploy cycle.
    w.u16(static_cast<uint16_t>(d.rack.size()));
    for (const auto& p : d.rack) {
        w.u16(static_cast<uint16_t>(p.ownedMoves.size()));
        for (const auto& m : p.ownedMoves) writeId(w, m);
        for (int i = 0; i < kMaxMoveSlots; ++i) writeId(w, p.equippedMoves[i]);
        for (int i = 0; i < kModSlots; ++i) writeId(w, p.equippedMods[i]);
    }

    // v30: the Backup Drive combat-shield buff's armed-until deadline.
    w.u32(d.backupShieldUntilMs);

    // v31: the f Hacker-SHOP Merge Hub unlocks. Appended, so a v30 reader
    // ignores them; a pre-v31 blob reads back as both 0 (bought neither).
    w.u8(d.mergeHubUnlocked);
    w.u32(d.recipesUnlocked);

    // v32: Rig Shop rows beyond the legacy 11 (rigLevelsExt). Appended, so a v31
    // reader ignores them; a pre-v32 blob reads back empty (those rows unbought).
    w.u16(static_cast<uint16_t>(d.rigLevelsExt.size()));
    for (uint16_t lvl : d.rigLevelsExt) w.u16(lvl);

    // v34: per-stored-pet evolution-timer elapsed-in-stage, parallel to d.rack
    // (mirrors defragCount's v16 pattern) so it survives an ARCH Store/Deploy cycle.
    w.u16(static_cast<uint16_t>(d.rack.size()));
    for (const auto& p : d.rack) w.u32(p.timeInStageMs);

    // v35: this pet's own best-ever DeepWeb Dive depth. The active pet's tally,
    // then a parallel list mapped onto d.rack by index (mirrors defragCount's v16
    // pattern) so it survives an ARCH Store/Deploy cycle.
    w.i32(d.bestDeepWebDepth);
    w.u16(static_cast<uint16_t>(d.rack.size()));
    for (const auto& p : d.rack) w.i32(p.bestDeepWebDepth);

    // v36: the Hacker CREW allegiance + the home network it hangs off. Player-level;
    // the crew rides as its content ID so the table can be reordered.
    w.bytes(d.crewId, kSaveIdCap);
    w.u64(d.homeNetworkKey);
    w.bytes(d.homeNetworkName, kSaveNetNameCap);

    // v37: the pet-to-pet LINK opt-in. The met operators ride the SD-backed
    // PeerLedger, not this blob — only the consent bit belongs here.
    w.u8(d.linkEnabled ? 1 : 0);

    // v38: reserved. Always 0 — the standing internet opt-in it held is gone, and
    // the byte only survives to keep the v39+ tail aligned (see save.h).
    w.u8(0);

    // v39: the raised-species tally, length-prefixed like seenCreatures.
    w.u16(static_cast<uint16_t>(d.raisedCreatures.size()));
    for (const auto& s : d.raisedCreatures) writeId(w, s);

    // v40: the achievement bitsets (indexed by wire number, length-prefixed so the
    // catalogue can grow without another version) + the counters the countable rows are
    // measured against.
    w.u16(static_cast<uint16_t>(d.achievementEarned.size()));
    for (uint8_t b : d.achievementEarned) w.u8(b);
    w.u16(static_cast<uint16_t>(d.achievementNotified.size()));
    for (uint8_t b : d.achievementNotified) w.u8(b);
    w.i32(d.bossWins);
    w.u16(static_cast<uint16_t>(d.collectedItems.size()));
    for (const auto& s : d.collectedItems) writeId(w, s);
    // Parallel id/depth lists — same shape the rack's parallel tails use, so the ids stay
    // a plain SaveId run and the depths a plain i32 run.
    w.u16(static_cast<uint16_t>(d.speciesDiveIds.size()));
    for (const auto& s : d.speciesDiveIds) writeId(w, s);
    for (int32_t v : d.speciesDiveDepths) w.i32(v);

    // v42: time already burned in the 5/5 recovery window, so neither a power cycle
    // nor an ARCH freeze can refund it. Awake-ms only — see the version note in save.h
    // for why that is the honest measure on a board with no RTC. The active pet's
    // value, then a parallel list mapped onto d.rack by index.
    w.u32(d.dyingElapsedMs);
    w.u16(static_cast<uint16_t>(d.rack.size()));
    for (const auto& p : d.rack) w.u32(p.dyingElapsedMs);

    // v44: DEFRAG minigame boards cleared, lifetime.
    w.i32(d.stackerWins);

    // v45: the owned-mod spare pool, a nibble of COUNT per ModDef::wire. Length-prefixed
    // like the achievement masks, and for the same reason: the array is only as long as
    // the highest wire actually held, so a roster that grows doesn't lengthen the saves
    // of players who own none of the new rows, and a longer array from a later build
    // reads back into a shorter view as "not held" rather than as a corrupt stream.
    w.u16(static_cast<uint16_t>(d.ownedModCounts.size()));
    for (uint8_t b : d.ownedModCounts) w.u8(b);

    // v47: the arcade's per-cabinet tallies, as parallel id/plays/wins runs (the
    // speciesDive shape) so the GAMES roster stays free to reorder.
    w.u16(static_cast<uint16_t>(d.arcadeIds.size()));
    for (const auto& s : d.arcadeIds) writeId(w, s);
    for (int32_t v : d.arcadePlays) w.i32(v);
    for (int32_t v : d.arcadeWins) w.i32(v);

    // v48: the DECRYPTOGRAM per-quote state, two bits per QuoteDef::wire. Length-
    // prefixed at its full in-memory width, like the v40 achievement bitsets — a pool
    // that grows into spare capacity needs no save change at all.
    w.u16(static_cast<uint16_t>(d.quoteStates.size()));
    for (uint8_t b : d.quoteStates) w.u8(b);

    // v50: the per-pet Bandwidth-regen upgrade — the active pet's minutes, then a
    // parallel list mapped onto the rack by index (the v42 dying-window shape).
    w.i32(d.bandwidthRegenBonusMin);
    w.u16(static_cast<uint16_t>(d.rack.size()));
    for (const auto& p : d.rack) w.i32(p.bandwidthRegenBonusMin);

    // v51: the whole owned-recipe bitset, length-prefixed like the v40 achievement
    // masks. Its first four bytes are also written above as the v31 u32, so a reader
    // that stops at v50 still gets the recipes it can name.
    w.u16(static_cast<uint16_t>(d.recipeOwned.size()));
    for (uint8_t b : d.recipeOwned) w.u8(b);

    // v53: ROCK THE DOCK's run in play — the seed the whole bracket is derived from, plus
    // the three bytes of state that seed cannot express (who is still standing, which
    // round, and the verdict of a finished run the operator has yet to dismiss).
    w.u32(d.tourneySeed);
    w.u8(d.tourneyAlive);
    w.u8(d.tourneyRound);
    w.u8(d.tourneyPhase);

    // v55: the arcade's per-cabinet high score, in the same order as the v47 arcadeIds
    // run above. Its own tail rather than a fourth column up there, so a pre-v55 build
    // reading this blob still finds the trio where it expects them and simply stops
    // before this.
    w.u16(static_cast<uint16_t>(d.arcadeBest.size()));
    for (int32_t v : d.arcadeBest) w.i32(v);

    // v56: the three lifetime tallies the achievement board's newest ladders read —
    // brackets taken, duels won, dishes cooked. Its own tail after v55's, so the two
    // stay independently skippable by a build that predates either.
    w.i32(d.tourneyWins);
    w.i32(d.pvpWins);
    w.i32(d.mergesCooked);

    // v57: the rest of the per-pet Epic-dish grants — the active pet's off-level stat
    // points and XP rate, then the same pair per rack pet, mapped by index. The v50 tail
    // above carries the first of these grants and is left exactly as it was, so a build
    // that stops at v56 still reads every field it knows.
    for (int32_t v : d.statBonus) w.i32(v);
    w.i32(d.xpRateBonusPct);
    w.u16(static_cast<uint16_t>(d.rack.size()));
    for (const auto& p : d.rack) {
        for (int32_t v : p.statBonus) w.i32(v);
        w.i32(p.xpRateBonusPct);
    }

    // v58: the chosen background, as its own wire number. One byte, and its own tail —
    // which backgrounds are OWNED is derived from the rest of this blob rather than
    // written beside it.
    w.u8(d.backgroundPick);

    // v59: the CANT — the learned-sigil mask and the shakes paid for it. Its own tail
    // after v58's, so a build that stops at either still reads every field it knows.
    w.u32(d.cantSigils);
    w.i32(d.shakesSpent);
}

std::vector<uint8_t> serializeSave(const SaveData& d) {
    std::vector<uint8_t> out;
    serializeSaveInto(d, out);
    return out;
}

bool deserializeSave(const std::vector<uint8_t>& blob, SaveData& out) {
    out = SaveData{};  // default = empty on any failure
    if (blob.size() < 6) return false;
    Reader r{blob.data(), blob.size()};
    char magic[4];
    r.bytes(magic, 4);
    if (!r.ok || std::memcmp(magic, kMagic, 4) != 0) { out = SaveData{}; return false; }
    const uint16_t version = r.u16();
    // Accept every version still in support, in BOTH directions.
    //
    // Older is the easy half (forward-compat): a v(N-1) blob simply lacks the newer
    // appended tails, which load with defaults.
    //
    // NEWER is the half that matters on a device. An OTA image boots once on trial and
    // the bootloader reverts it if the device resets before it commits — so the previous
    // firmware can find itself underneath a save the newer one already rewrote. Refusing
    // that blob does not fail safe: it deserializes as empty, and an empty save is a
    // fresh hatch with the pet, the rack and the records gone. Reading the prefix we
    // understand and ignoring a tail we don't loses only what the newer build added,
    // which is the worst a rollback should ever cost.
    //
    // What that rests on: THE STREAM IS APPEND-ONLY. Every field a later version adds
    // goes on the end behind its own `version >= N` gate, and a field that retires keeps
    // being consumed behind a `version < N` gate (see the v33 and v45 tails). A version
    // that inserts or drops a field mid-stream breaks this and must raise
    // kOldestAcceptedVersion instead. `r.ok` is the backstop either way — a stream that
    // desyncs runs short and is rejected below.
    if (version < kOldestAcceptedVersion) {
        out = SaveData{};
        return false;
    }

    SaveData d;
    r.bytes(d.activeId, kSaveIdCap);
    d.hunger = r.i32(); d.frag = r.i32(); d.happy = r.i32();
    d.mistakes = r.i32(); d.debuffs = r.i32();
    d.ghost = r.u8();
    d.timeInStageMs = r.u32();
    d.generation = r.i32();

    d.bits = r.i32();
    r.bytes(d.hackerTag, kHackerTagMax + 1);
    d.hackerTag[kHackerTagMax] = '\0';
    d.lifetimeUptimeMs = r.u32();
    d.lifetimeSteps = r.u32();
    d.petsRaised = r.i32();

    const uint16_t nItems = r.u16();
    for (uint16_t i = 0; i < nItems && r.ok; ++i) {
        SaveStack s; r.bytes(s.id, kSaveIdCap); s.id[kSaveIdCap - 1] = '\0';
        s.qty = r.i32(); d.items.push_back(s);
    }
    // The pre-v45 spare pool: one id cell per COPY. A v45 blob carries a count of zero
    // here and its pool in the v45 tail instead; anything older carries the real list,
    // which is what the migration tallies.
    const uint16_t nOwned = r.u16();
    for (uint16_t i = 0; i < nOwned && r.ok; ++i) {
        SaveId m; r.bytes(m.id, kSaveIdCap); m.id[kSaveIdCap - 1] = '\0';
        d.ownedMods.push_back(m);
    }
    const uint16_t nEquip = r.u16();
    for (uint16_t i = 0; i < nEquip && r.ok; ++i) {
        SaveId e; r.bytes(e.id, kSaveIdCap); e.id[kSaveIdCap - 1] = '\0';
        d.equipped.push_back(e);
    }
    const uint16_t nLog = r.u16();
    for (uint16_t i = 0; i < nLog && r.ok; ++i) {
        SaveLogEntry e; e.type = r.u8();
        r.bytes(e.text, kSaveTextCap); e.text[kSaveTextCap - 1] = '\0';
        d.log.push_back(e);
    }
    const uint16_t nRack = r.u16();
    for (uint16_t i = 0; i < nRack && r.ok; ++i) {
        SaveStoredPet p; r.bytes(p.id, kSaveIdCap); p.id[kSaveIdCap - 1] = '\0';
        p.hunger = r.i32(); p.frag = r.i32(); p.happy = r.i32();
        p.mistakes = r.i32(); p.debuffs = r.i32();
        p.ghost = r.u8();
        p.generation = r.i32();
        d.rack.push_back(p);
    }

    // v2 tail: the move loadout + combat XP/level. Absent in a v1 blob (the fields
    // keep their SaveData defaults; hasMoveData stays false so the loader seeds a
    // starting loadout). The reader stays bounds-checked, so a clean v1 blob is OK.
    if (version >= 2) {
        const uint16_t nOwnedMv = r.u16();
        for (uint16_t i = 0; i < nOwnedMv && r.ok; ++i) {
            SaveId m; r.bytes(m.id, kSaveIdCap); m.id[kSaveIdCap - 1] = '\0';
            d.ownedMoves.push_back(m);
        }
        const uint16_t nEquipMv = r.u16();
        for (uint16_t i = 0; i < nEquipMv && r.ok; ++i) {
            SaveId e; r.bytes(e.id, kSaveIdCap); e.id[kSaveIdCap - 1] = '\0';
            d.equippedMoves.push_back(e);
        }
        d.combatXp = r.i32();
        d.combatLevel = r.i32();
        d.hasMoveData = r.ok;
    }

    // v3 tail: the ARCH records. Absent in a v1/v2 blob (records stay empty).
    if (version >= 3) {
        const uint16_t nRec = r.u16();
        for (uint16_t i = 0; i < nRec && r.ok; ++i) {
            SaveRecord rec; r.bytes(rec.id, kSaveIdCap); rec.id[kSaveIdCap - 1] = '\0';
            rec.status = r.u8();
            rec.generation = r.i32();
            d.records.push_back(rec);
        }
    }

    // v4 tail: Hacker Rank. Absent in a v1/v2/v3 blob (starts at rank 0, no
    // networks seen — the honest default for a save that predates the system).
    if (version >= 4) {
        d.networksSeen = r.i32();
        d.hackerRank = r.i32();
    }

    // v5 tail: the Audit-mode passive-scan toggle. Absent in a v1..v4 blob — it
    // stays OFF (the authorized-use default: a migrated save never silently
    // enables the radio).
    if (version >= 5) {
        d.netScanEnabled = r.u8();
    }

    // v6 tail: the Audit-CAPTURE toggle. Absent in a v1..v5 blob — it stays OFF
    // (authorized-use default: a migrated save never silently arms capture).
    if (version >= 6) {
        d.auditCaptureEnabled = r.u8();
    }

    // v7 tail: the real-radio Audit SHAKES ledger + lifetime handshake count.
    // Absent in a v1..v6 blob — the ledger stays empty (the pre-v7 behaviour: a
    // reboot re-builds the dedup set from scratch, which could re-count).
    if (version >= 7) {
        d.handshakesSeen = r.i32();
        const uint16_t nHs = r.u16();
        for (uint16_t i = 0; i < nHs && r.ok; ++i) {
            uint64_t k = r.u32();
            k |= static_cast<uint64_t>(r.u16()) << 32;
            d.seenHandshakeBssids.push_back(k);
        }
    }

    // v8 tail: the EXPL sector-clear flags. Absent in a v1..v7 blob — the vector
    // stays empty, which the loader reads as "nothing cleared" (only sector 0 is
    // open, the correct default for a save that predates linear gating).
    if (version >= 8) {
        const uint16_t nSec = r.u16();
        for (uint16_t i = 0; i < nSec && r.ok; ++i)
            d.sectorCleared.push_back(r.u8());
    }

    // v9 tail: the Boot-Sector incubation timer. Absent in a v1..v8 blob — it stays
    // 0 (the migrated pet is treated as already hatched: no in-flight egg to resume).
    if (version >= 9) {
        d.bootHatchRemainMs = r.u32();
    }

    // v10 tail: the zone-completion Titles. Absent in a v1..v9 blob — nothing
    // unlocked, none equipped (a save that predates Titles has earned none).
    if (version >= 10) {
        d.titlesUnlocked = r.u32();
        d.equippedTitle = r.i32();
    }

    // v11 tail: the creature-level stat points. Absent in a v1..v10 blob —
    // statPoints stays empty and hasLevelData false, so the loader migrates the pet
    // to a fresh level 0 (the pre-v11 combatLevel was never applied to combat).
    if (version >= 11) {
        const uint16_t nStat = r.u16();
        for (uint16_t i = 0; i < nStat && r.ok; ++i)
            d.statPoints.push_back(r.i32());
        d.hasLevelData = r.ok;
    }

    // v12 tail: the EXPL per-sector boss-unlock flags. Absent in a
    // v1..v11 blob — bossUnlocked stays empty, so the loader migrates each flag from
    // sectorCleared[] (a cleared sector's boss was obviously already beaten).
    if (version >= 12) {
        const uint16_t nBoss = r.u16();
        for (uint16_t i = 0; i < nBoss && r.ok; ++i)
            d.bossUnlocked.push_back(r.u8());
    }

    // v13 tail: the EXPL per-sub-area clear + boss-unlock bitmasks. Absent in
    // a v1..v12 blob — both vectors stay empty, so the loader migrates a cleared area
    // to all 5 sub-areas cleared + unlocked (a beaten area had every sub-area beaten).
    if (version >= 13) {
        const uint16_t nSubC = r.u16();
        for (uint16_t i = 0; i < nSubC && r.ok; ++i)
            d.subCleared.push_back(r.u8());
        const uint16_t nSubB = r.u16();
        for (uint16_t i = 0; i < nSubB && r.ok; ++i)
            d.subBossUnlocked.push_back(r.u8());
    }

    // v14 tail: the CFG screen-brightness level. Absent in a v1..v13 blob —
    // d.brightness keeps its kBrightnessDefault (brightest) default from SaveData.
    if (version >= 14) {
        const int32_t b = r.i32();
        if (r.ok) d.brightness = b;
    }

    // v15 tail: the EXPL per-sub-area re-farm win counts. Absent in a
    // v1..v14 blob — subRefarm stays empty, which the loader reads as "nothing
    // depleted yet" (every count 0 → full drops until re-farmed).
    if (version >= 15) {
        const uint16_t nRf = r.u16();
        for (uint16_t i = 0; i < nRf && r.ok; ++i)
            d.subRefarm.push_back(r.u16());
    }

    // v16 tail: the per-pet defrag counts — the active pet's tally + a
    // parallel list mapped back onto the rack pets by index. Absent in a v1..v15 blob —
    // every tally stays 0 (a migrated save's pets read as never-defragged).
    if (version >= 16) {
        d.defragCount = r.i32();
        const uint16_t nDc = r.u16();
        for (uint16_t i = 0; i < nDc && r.ok; ++i) {
            const int32_t c = r.i32();
            if (i < d.rack.size()) d.rack[i].defragCount = c;
        }
    }

    // v17: no wire tail — the version alone flags the permanent-mod semantics, so
    // the loader knows `ownedMods` already excludes the equipped mods (no migration).
    if (version >= 17) d.hasPermanentModData = true;

    // v18 tail: the per-spare mod equip-level gates, parallel to ownedMods. v45 dropped
    // the rolled gate, so from that version on this reads back empty and the levels a
    // migrating blob does carry are discarded once the copies are tallied.
    if (version >= 18) {
        const uint16_t nReq = r.u16();
        for (uint16_t i = 0; i < nReq && r.ok; ++i)
            d.ownedModReqLevels.push_back(r.i32());
        d.hasModEquipLevelData = r.ok;
    }

    // v19 tail: the Hacker SHOP account upgrades. Absent in a v1..v18 blob —
    // bwUpgradeCount stays 0 (a migrated save has bought no upgrades).
    if (version >= 19) d.bwUpgradeCount = r.i32();

    // v20 tail: the 'Pedia local-AP toggle. Absent in a v1..v19 blob → stays OFF.
    if (version >= 20) d.apEnabled = r.u8();

    // v21 tail: the per-pet care-mistake shield + once-per-lifetime item gates, and
    // the shop-pre-alloc fragAmountTier. Absent in a v1..v20 blob → the per-pet flags
    // stay 0 (no shield, neither item consumed) and fragAmountTier 0.
    if (version >= 21) {
        d.mistakeShieldActive = r.u8();
        d.shieldItemConsumed = r.u8();
        d.yubiConsumed = r.u8();
        d.fragAmountTier = r.u8();
    }

    // v22 tail: the c "Reduce Explore Frag TRIGGER %" tier. Absent in a
    // v1..v21 blob → stays 0 (baseline trigger chance).
    if (version >= 22) d.fragTriggerTier = r.u8();

    // v23 tail: the d/e one-time Hacker-SHOP unlocks. Absent in a v1..v22
    // blob → both stay 0 (a migrated save has bought neither).
    if (version >= 23) {
        d.itemTabsUnlocked = r.u8();
        d.bulkOpenUnlocked = r.u8();
    }

    // v24 tail: the move-slot rework's per-pet Attack/Defend slot typing.
    // Absent in a v1..v23 blob — slotKinds stays empty and hasSlotKindData false,
    // so the loader leaves every slot Unset and Game::stampSlotKinds() re-derives
    // it from the loaded pet's CreatureDef::slotKinds (a deterministic migration).
    if (version >= 24) {
        const uint16_t nSk = r.u16();
        for (uint16_t i = 0; i < nSk && r.ok; ++i)
            d.slotKinds.push_back(r.u8());
        d.hasSlotKindData = r.ok;
    }

    // v25 tail: the web-'Pedia reveal-state bookkeeping. Absent in a v1..v24 blob —
    // seenCreatures stays empty and both masks stay 0 (nothing glimpsed/achieved yet,
    // the honest default for a save that predates this system).
    if (version >= 25) {
        const uint16_t nSeen = r.u16();
        for (uint16_t i = 0; i < nSeen && r.ok; ++i) {
            SaveId s; r.bytes(s.id, kSaveIdCap); s.id[kSaveIdCap - 1] = '\0';
            d.seenCreatures.push_back(s);
        }
        d.malbeastSeen = r.u16();
        d.malbeastDefeated = r.u16();
        // The superseded v25 achievements u32 (see the writer). A v25..v39 blob carries
        // real bits here, which Game::applySave migrates into the v40 bitset; a v40+ blob
        // carries 0 and the bitset in its own tail takes over.
        d.achievementsMask = r.u32();
    }

    // v26 tail: per-stored-pet creature-level state, parallel to d.rack. Absent in a
    // v1..v25 blob — every rack pet keeps its SaveStoredPet default (level 0, all-Unset
    // slots): no level history was ever recorded for a stored pet before this version.
    if (version >= 26) {
        const uint16_t nLv = r.u16();
        for (uint16_t i = 0; i < nLv && r.ok; ++i) {
            const int32_t lvl = r.i32();
            const int32_t xp = r.i32();
            int32_t stats[kLevelStatCount];
            for (int j = 0; j < kLevelStatCount; ++j) stats[j] = r.i32();
            uint8_t slots[kMaxMoveSlots];
            for (int j = 0; j < kMaxMoveSlots; ++j) slots[j] = r.u8();
            if (i < d.rack.size()) {
                d.rack[i].combatLevel = lvl;
                d.rack[i].combatXp = xp;
                for (int j = 0; j < kLevelStatCount; ++j) d.rack[i].statPoints[j] = stats[j];
                for (int j = 0; j < kMaxMoveSlots; ++j) d.rack[i].slotKinds[j] = slots[j];
            }
        }
    }

    // v27 tail: the Hacker-SHOP rig-upgrade levels. Absent in a v1..v26 blob
    // → all three stay 0 (a migrated save has bought none).
    if (version >= 27) {
        d.rackSlotUpgradeCount = r.i32();
        d.scrapingClusterLevel = r.i32();
        d.dataMiningLevel = r.i32();
    }

    // v28 tail: the Ambig-USB armed Trojan-divert flag. Absent in a v1..v27 blob →
    // stays false (no divert armed).
    if (version >= 28) d.forceTrojanDivert = r.u8();

    // v29 tail: per-stored-pet move + mod loadout, parallel to d.rack. Absent in a
    // v1..v28 blob — every rack pet keeps its SaveStoredPet default (empty
    // ownedMoves/equippedMoves/equippedMods), which Game::archDeployStored reads as
    // "no data" and re-seeds from the pet's line, same as a pre-v2 active pet did.
    if (version >= 29) {
        const uint16_t nRackLoadouts = r.u16();
        for (uint16_t i = 0; i < nRackLoadouts && r.ok; ++i) {
            std::vector<SaveId> owned;
            const uint16_t nOwnedMv = r.u16();
            for (uint16_t j = 0; j < nOwnedMv && r.ok; ++j) {
                SaveId m; r.bytes(m.id, kSaveIdCap); m.id[kSaveIdCap - 1] = '\0';
                owned.push_back(m);
            }
            SaveId equippedMoves[kMaxMoveSlots];
            for (int j = 0; j < kMaxMoveSlots; ++j) {
                r.bytes(equippedMoves[j].id, kSaveIdCap);
                equippedMoves[j].id[kSaveIdCap - 1] = '\0';
            }
            SaveId equippedMods[kModSlots];
            for (int j = 0; j < kModSlots; ++j) {
                r.bytes(equippedMods[j].id, kSaveIdCap);
                equippedMods[j].id[kSaveIdCap - 1] = '\0';
            }
            if (i < d.rack.size()) {
                d.rack[i].ownedMoves = std::move(owned);
                for (int j = 0; j < kMaxMoveSlots; ++j) d.rack[i].equippedMoves[j] = equippedMoves[j];
                for (int j = 0; j < kModSlots; ++j) d.rack[i].equippedMods[j] = equippedMods[j];
            }
        }
    }

    // v30 tail: the Backup Drive combat-shield buff's armed-until deadline. Absent in
    // a v1..v29 blob → stays 0 (no shield armed).
    if (version >= 30) d.backupShieldUntilMs = r.u32();

    // v31 tail: the f Hacker-SHOP Merge Hub unlocks. Absent in a v1..v30 blob →
    // both stay 0 (a migrated save has bought neither the Hub nor any recipe).
    if (version >= 31) {
        d.mergeHubUnlocked = r.u8();
        d.recipesUnlocked = r.u32();
    }

    // v32 tail: Rig Shop rows beyond the legacy 11 (rigLevelsExt). Absent in a
    // v1..v31 blob → stays empty (those rows read back as never bought).
    if (version >= 32) {
        const uint16_t nRig = r.u16();
        for (uint16_t i = 0; i < nRig && r.ok; ++i)
            d.rigLevelsExt.push_back(r.u16());
    }

    // v34 tail: per-stored-pet evolution-timer elapsed-in-stage, parallel to d.rack.
    // Absent in a v1..v33 blob → every rack pet keeps its SaveStoredPet default (0),
    // so a migrated save's stored pets thaw with a fresh in-stage clock (the old
    // Deploy behavior), not lost progress.
    if (version >= 34) {
        const uint16_t nTs = r.u16();
        for (uint16_t i = 0; i < nTs && r.ok; ++i) {
            const uint32_t t = r.u32();
            if (i < d.rack.size()) d.rack[i].timeInStageMs = t;
        }
    }

    // v35 tail: this pet's own best-ever DeepWeb Dive depth — the active pet's
    // tally + a parallel list mapped back onto the rack pets by index. Absent in a
    // v1..v34 blob → every pet reads back as 0 (never dived).
    if (version >= 35) {
        d.bestDeepWebDepth = r.i32();
        const uint16_t nBd = r.u16();
        for (uint16_t i = 0; i < nBd && r.ok; ++i) {
            const int32_t v = r.i32();
            if (i < d.rack.size()) d.rack[i].bestDeepWebDepth = v;
        }
    }

    // v36 tail: crew allegiance + home network. Absent in a v1..v35 blob → the operator
    // reads back unaffiliated with no home network designated.
    if (version >= 36) {
        r.bytes(d.crewId, kSaveIdCap);
        d.homeNetworkKey = r.u64();
        r.bytes(d.homeNetworkName, kSaveNetNameCap);
    }

    // v37 tail: the LINK opt-in. Absent in a v1..v36 blob → off, which is both the
    // default and the only honest reading of a save that predates the choice — a
    // migrated device must never start announcing itself because it was upgraded.
    if (version >= 37) d.linkEnabled = r.u8() != 0;

    // v38 tail: reserved. Read to advance the cursor, never acted on — a save that
    // still carries the old opt-in must not put this device back on the 'net.
    if (version >= 38) r.u8();

    // v39 tail: the raised-species tally. Absent in a v1..v38 blob — it reads back
    // empty here, and Game::applySave rebuilds what it can by unioning in every
    // species the save still points at. That recovery lives there rather than in this
    // codec because it is the same union a NATIVE v39 save needs (anything in the
    // rack or the archive was necessarily raised, however the blob came to say so).
    if (version >= 39) {
        const uint16_t nRaised = r.u16();
        for (uint16_t i = 0; i < nRaised && r.ok; ++i) {
            SaveId s; r.bytes(s.id, kSaveIdCap); s.id[kSaveIdCap - 1] = '\0';
            d.raisedCreatures.push_back(s);
        }
    }

    // v40 tail: the achievement bitsets + their counters. Absent in a v1..v39 blob — all
    // four read back empty here, and Game::applySave does the migration: the legacy u32
    // mask becomes the first bytes of `achievementEarned` (the original 14 rows hold wire
    // numbers 0-13), nothing is marked notified, and the counters are seeded from what
    // the rest of the save can honestly account for.
    if (version >= 40) {
        const uint16_t nEarned = r.u16();
        for (uint16_t i = 0; i < nEarned && r.ok; ++i)
            d.achievementEarned.push_back(r.u8());
        const uint16_t nNotified = r.u16();
        for (uint16_t i = 0; i < nNotified && r.ok; ++i)
            d.achievementNotified.push_back(r.u8());
        d.bossWins = r.i32();
        const uint16_t nColl = r.u16();
        for (uint16_t i = 0; i < nColl && r.ok; ++i) {
            SaveId s; r.bytes(s.id, kSaveIdCap); s.id[kSaveIdCap - 1] = '\0';
            d.collectedItems.push_back(s);
        }
        const uint16_t nDive = r.u16();
        for (uint16_t i = 0; i < nDive && r.ok; ++i) {
            SaveId s; r.bytes(s.id, kSaveIdCap); s.id[kSaveIdCap - 1] = '\0';
            d.speciesDiveIds.push_back(s);
        }
        for (uint16_t i = 0; i < nDive && r.ok; ++i)
            d.speciesDiveDepths.push_back(r.i32());
    }

    // v42 tail: the 5/5 recovery window already burned — the active pet's, then a
    // parallel list mapped back onto the rack pets by index. Absent in a v1..v41 blob
    // → 0 for every pet, so a migrating save enters its next dying window with the
    // full grace period. That is the forgiving reading, and the only one a save that
    // never recorded the figure can support.
    if (version >= 42) {
        d.dyingElapsedMs = r.u32();
        const uint16_t nDy = r.u16();
        for (uint16_t i = 0; i < nDy && r.ok; ++i) {
            const uint32_t v = r.u32();
            if (i < d.rack.size()) d.rack[i].dyingElapsedMs = v;
        }
    }

    // v44 tail: the played-defrag tally. Absent in a v1..v43 blob → 0, which starts the
    // ladder fresh. Deliberately NOT seeded from `defragCount`: that counts defrags of
    // every variant, so seeding from it would credit bought and rolled cleans as boards
    // played by hand — over-crediting, the direction a migration must never take.
    if (version >= 44) d.stackerWins = r.i32();

    // v45 tail: the counted mod pool. Absent in a v1..v44 blob → the array stays empty
    // and hasModCountData false, which is what routes the loader onto the migration that
    // tallies the old flat `ownedMods` list instead.
    if (version >= 45) {
        const uint16_t nCounts = r.u16();
        for (uint16_t i = 0; i < nCounts && r.ok; ++i) d.ownedModCounts.push_back(r.u8());
        d.hasModCountData = r.ok;
    }

    // v47 tail: the arcade tallies. Absent in a v1..v46 blob → empty, and every
    // cabinet reads as never played. There is nothing older to seed them from, and
    // inventing one would credit runs that never happened.
    if (version >= 47) {
        const uint16_t nCab = r.u16();
        for (uint16_t i = 0; i < nCab && r.ok; ++i) {
            SaveId s; r.bytes(s.id, kSaveIdCap); s.id[kSaveIdCap - 1] = '\0';
            d.arcadeIds.push_back(s);
        }
        for (uint16_t i = 0; i < nCab && r.ok; ++i) d.arcadePlays.push_back(r.i32());
        for (uint16_t i = 0; i < nCab && r.ok; ++i) d.arcadeWins.push_back(r.i32());
    }

    // v48 tail: the per-quote board state. Absent in a v1..v47 blob → empty, and every
    // quote reads as never played. There is nothing older to seed it from — the board
    // did not exist, so no quote can honestly be called solved.
    if (version >= 48) {
        const uint16_t nQuote = r.u16();
        for (uint16_t i = 0; i < nQuote && r.ok; ++i) d.quoteStates.push_back(r.u8());
    }

    // v50 tail: the Bandwidth-regen upgrade — the active pet's, then a parallel list
    // mapped back onto the rack pets by index. Absent in a v1..v49 blob → 0 for every
    // pet, which is the truth rather than a compromise: no dish granted it before v50.
    if (version >= 50) {
        d.bandwidthRegenBonusMin = r.i32();
        const uint16_t nBw = r.u16();
        for (uint16_t i = 0; i < nBw && r.ok; ++i) {
            const int32_t v = r.i32();
            if (i < d.rack.size()) d.rack[i].bandwidthRegenBonusMin = v;
        }
    }

    // v51 tail: the whole owned-recipe bitset.
    if (version >= 51) {
        const uint16_t nRecipe = r.u16();
        for (uint16_t i = 0; i < nRecipe && r.ok; ++i) d.recipeOwned.push_back(r.u8());
    }

    // v53 tail: ROCK THE DOCK's run. Absent in a v1..v52 blob → a zero seed, which is
    // exactly "no bracket in play" — no migration, because a run that never existed
    // cannot be reconstructed from anything else in the save.
    if (version >= 53) {
        d.tourneySeed = r.u32();
        d.tourneyAlive = r.u8();
        d.tourneyRound = r.u8();
        d.tourneyPhase = r.u8();
    }

    // v55 tail: the per-cabinet high score, positionally matched to the v47 arcadeIds
    // run. Absent in a v1..v54 blob → empty, and every cabinet reads as never scored.
    // There is nothing older to seed it from: a win tally says a run was good enough,
    // never how good, so inventing a best out of one would be a number nobody played.
    if (version >= 55) {
        const uint16_t nBest = r.u16();
        for (uint16_t i = 0; i < nBest && r.ok; ++i) d.arcadeBest.push_back(r.i32());
    }

    // v56 tail: the three lifetime tallies. Absent in a v1..v55 blob → 0 apiece, and
    // there is deliberately no migration: a dismissed bracket, a finished duel and an
    // eaten dish all leave nothing behind for an older save to be read back from, so a
    // seeded number here would be invented rather than recovered.
    if (version >= 56) {
        d.tourneyWins = r.i32();
        d.pvpWins = r.i32();
        d.mergesCooked = r.i32();
    }

    // v57 tail: the off-level stat points and the XP rate — the active pet's, then a
    // parallel list mapped back onto the rack pets by index (the v50 shape). Absent in a
    // v1..v56 blob → 0 for every pet, which is the truth rather than a compromise: no
    // dish granted one before this version.
    if (version >= 57) {
        for (int32_t& v : d.statBonus) v = r.i32();
        d.xpRateBonusPct = r.i32();
        const uint16_t nUp = r.u16();
        for (uint16_t i = 0; i < nUp && r.ok; ++i) {
            int32_t bonus[kLevelStatCount];
            for (int32_t& v : bonus) v = r.i32();
            const int32_t rate = r.i32();
            if (i < d.rack.size()) {
                for (int j = 0; j < kLevelStatCount; ++j) d.rack[i].statBonus[j] = bonus[j];
                d.rack[i].xpRateBonusPct = rate;
            }
        }
    }

    // v58 tail: the chosen background. Absent in a v1..v57 blob → 0, which is AUTO —
    // the pet decides, which is what every device did before there was a choice to make.
    if (version >= 58) d.backgroundPick = r.u8();

    // v59 tail: the CANT. Absent in a v1..v58 blob → no sigils learned and nothing
    // spent, which is the truth and not a compromise — every shake such a save
    // captured is still in the purse, waiting for the first guardian.
    if (version >= 59) {
        d.cantSigils = r.u32();
        d.shakesSpent = r.i32();
    }

    if (!r.ok) { out = SaveData{}; return false; }  // truncated -> empty
    if (version < newestRenameVersion()) renameRetiredIds(d, version);
    // After every tail is read, never between them: the shift moves whole ladder-indexed
    // vectors, so it has to see them at their final lengths.
    if (version < newestLadderInsertVersion()) applyLadderInserts(d, version);
    out = std::move(d);
    return true;
}

} // namespace mal
