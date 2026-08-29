// save.h — the versioned, platform-agnostic save blob.
//
// SaveData is the engine-native snapshot of everything that must survive a
// reboot; serialize()/deserialize() turn it into the opaque byte blob an
// ISaveStore persists. The format is a hand-rolled little-endian stream behind a
// magic + version header so it is forward-compatible: a missing save or a
// version/magic mismatch deserializes as "empty" and the boot falls back to the
// Decryption Hatch. Ids are fixed-cell strings (content ids are short + stable);
// variable collections are length-prefixed. Embedded-friendly — small (<1KB),
// no exceptions, bounds-checked reads.
#pragma once

#include <cstdint>
#include <vector>

#include "tunables.h"  // kHackerTagMax, kSaveReserveBytes

namespace mal {

// Max content-id length incl. the NUL terminator (longest live id today is
// "clock_speed_boost" = 17). Bump with a version if an id ever needs more.
constexpr int kSaveIdCap = 24;
constexpr int kSaveTextCap = 28;     // matches EventLog's LogEntry.text
// THE VERSION LOG. Every entry says what a version put on the wire and what a blob written
// before it reads back as. deserialize accepts every version from kOldestAcceptedVersion
// up, an older blob loading with the newer fields defaulted — forward-compat is the
// contract. Health is transient and never serialized.
//
// Three shapes recur, and an entry names which it used:
//   APPEND          — new fields at the end of the stream; a pre-vN blob simply stops short.
//   PARALLEL TAIL   — a per-stored-pet field written as a list mapped onto d.rack by index,
//                     so the mid-stream rack records stay byte-compatible (first used by v16).
//   NO BYTES        — a marker version: the format is unchanged and the number exists only
//                     to gate a migration pass (a rename, a ladder splice) for older blobs.
//
// A mid-stream field is NEVER removed. One that stopped meaning anything keeps being read
// and discarded (v33, v38, v40), because every later field is positioned behind it.
//
// v2  APPEND move loadout + combat XP/level.
// v3  APPEND the ARCH records (RETIRED/CORRUPTED, from Critical System Failure).
// v4  APPEND Hacker Rank: lifetime networks-seen, its dedup pool, the derived rank.
// v5  APPEND the Audit passive-scan runtime toggle (the authorized-use opt-in).
// v6  APPEND the Audit-CAPTURE runtime toggle (passive WPA handshake -> .pcap).
// v7  APPEND the real-radio Audit dedup ledgers — the seen-BSSID sets behind NETS and the
//     lifetime handshake count behind SHAKES — so a reboot never re-credits one already
//     counted.
// v8  APPEND the EXPL sector-clear flags, one byte per sector.
// v9  APPEND the Boot-Sector incubation timer, so an unhatched egg survives a reboot
//     mid-incubation.
// v10 APPEND the zone-completion Titles: the unlocked bitmask + the equipped index.
//     Player-level, so they persist across pets.
// v11 APPEND the per-pet earned combat-stat points. combatXp/combatLevel move to the
//     0-based geometric model, and a pre-v11 blob's vestigial linear level is dropped —
//     migrating the pet to a fresh level 0.
// v12 APPEND per-sector boss-unlock flags. Pre-v12 migrates bossUnlocked[i] =
//     sectorCleared[i]: a cleared sector's boss was obviously beaten. Superseded at
//     runtime by v13's per-sub flags, but still serialized so the stream stays layered.
// v13 APPEND per-area 5-bit bitmasks for sub-area CLEARED and sub-area BOSS-UNLOCK — the
//     unit of progress moves from the sector to the SUB-AREA. Pre-v13 migrates a cleared
//     area to all five subs cleared and all five sub-bosses unlocked.
// v14 APPEND the CFG screen-brightness level. Pre-v14 defaults to kBrightnessDefault, so
//     a migrated save is never surprise-dimmed.
// v15 APPEND per-sub-area RE-FARM win counts, row-major (area*kExplSubAreas+sub) — wins
//     farmed AFTER a sub was cleared, which decay its non-Bits drop chances. Pre-v15 → 0,
//     so a migrated save's cleared areas start un-depleted.
// v16 PARALLEL TAIL, the pattern every later per-stored-pet field follows: the active
//     pet's successful-defrag tally, then the rack pets' in rack order, so the count stays
//     consistent through ARCH freeze/thaw. Pre-v16 → 0.
// v17 NO BYTES. Reinterprets the mod collections for the permanent-mod model: a mod is
//     consumed when equipped, so `ownedMods` holds only un-equipped SPARES and `equipped`
//     the permanently-installed ones. The version alone is the marker
//     (hasPermanentModData); the loader drops any owned id that also sits in a slot, or a
//     migrated save would gift a duplicate spare of everything installed.
// v18 PARALLEL TAIL of per-spare equip-level gates (`ownedModReqLevels`, one i32 per owned
//     spare), keeping the v17 `ownedMods` cells byte-compatible. Pre-v18 → req 0. Removed
//     again by v45.
// v21 APPEND per-pet care-mistake SHIELD state (mistakeShieldActive; shieldItemConsumed
//     and yubiConsumed, the once-per-lifetime gates on the Restore Point and the
//     Yubi-Cookie) plus player-level fragAmountTier. Also WIDENS networkSeenMask from
//     uint8_t to uint64_t so the dedup pool can reach the top Hacker-Rank tier; the legacy
//     8 bits migrate into the low 8. The per-pet flags reset on a new egg.
// v23 APPEND the Hacker-SHOP account unlocks itemTabsUnlocked (arms the ITEMS hold-B
//     filter cycle) and bulkOpenUnlocked (arms the VAULT hold-B bulk open). Player-level.
//     Pre-v23 → false.
// v24 APPEND the per-pet SlotKind stamp for each move slot (0 Unset · 1 Attack · 2
//     Defend), length-prefixed. Per-pet, reset on a new egg; NOT reset on an ARCH Deploy,
//     mirroring moveLoadout_/statPoints_. Pre-v24 loads every slot Unset, and
//     Game::stampSlotKinds re-derives them from CreatureDef::slotKinds.
// v25 APPEND the web-'Pedia reveal state, all player-level: `seenCreatures` (species faced
//     in combat but never raised — "hatched" still wins over "seen"), and the
//     `malbeastSeen`/`malbeastDefeated` bitmasks over the fixed 6-entry wild roster
//     (combat.h kWildMalbeastIds). Bosses and Sim dummies never touch those bits. Also
//     `achievementsMask`, superseded by v40.
// v26 PARALLEL TAIL of per-stored-pet combatLevel/combatXp/statPoints/slotKinds. Before
//     it, only the active pet's level lived anywhere, so a rack swap dropped it — or the
//     newly active pet inherited the previous one's. Pre-v26 → level 0, all-Unset.
// v27 APPEND the Rig Shop levels rackSlotUpgradeCount (rack capacity = kRackSlots + this),
//     scrapingClusterLevel (+combat-Bits %) and dataMiningLevel (+cache-Bits %).
//     Player-level. Pre-v27 → 0.
// v28 APPEND forceTrojanDivert, the Ambig-USB's armed effect: guarantees the next
//     Process->Script Trojan divert instead of rolling kTrojanDivertPct. Per-pet, resets
//     on a new egg. Pre-v28 → false.
// v29 PARALLEL TAIL of the per-stored-pet move + mod loadout: `ownedMoves`,
//     `equippedMoves`, `equippedMods`. A pet's MOVES/MODS belong to the pet. The mod SPARE
//     pool stays player-level — mods are found like items, and only the installed slots
//     belong to a creature. A pre-v29 stored pet has an empty tail, which
//     Game::archDeployStored reads as unset and seeds with the line-starting kit.
// v30 APPEND backupShieldUntilMs, the Backup Drive buff's deadline against
//     Game::lifetimeUptimeMs (0 = inactive). Per-pet, resets on a new egg. Pre-v30 → 0.
// v31 APPEND mergeHubUnlocked (makes the MRG carousel slot accessible) and
//     recipesUnlocked, a bitmask over game_internal.h's kMergeRecipes by
//     MergeRecipe::wire. Player-level. Pre-v31 → 0. Widened by v49.
// v32 APPEND rigLevelsExt, a forward-compatible tail covering every Rig Shop row from
//     kRigRowExtBase up (game_rig_shop.h): index i = row (kRigRowExtBase + i), value = its
//     purchased level, so a new row persists without another named field. Rows 0-10 keep
//     their own named fields. A missing tail or entry → level 0.
// v33 REMOVES the v4/v21 networkSeenMask and the v7 seenBssids vector: real-network dedup
//     and history moved onto the SD-backed NetworkLedger (core/net/network_ledger.h), and
//     the EXPL Wi-Fi event became the only place a network is credited
//     (Game::resolveNetworkDiscovery). A v4..v32 blob still carries those bytes and the
//     reader still consumes them (version-gated `< 33`) so later fields stay aligned; a
//     v33+ writer never emits them. The derived `networksSeen`/`hackerRank` counters and
//     the unrelated SHAKES ledger are untouched.
// v34 PARALLEL TAIL of per-stored-pet evolution elapsed-in-stage, so a rack pet's progress
//     survives an ARCH Store/Deploy instead of resetting on Deploy.
// v35 PARALLEL TAIL of bestDeepWebDepth, this pet's own highest dive depth. Read by the
//     Zero-Day Bell (SetDeepWebStartDepthToBest) to warp a fresh dive to THIS pet's
//     frontier, never a device-wide max. Per-pet, reset on a new egg. Pre-v35 → 0.
// v36 APPEND the Hacker-face CREW state: `crewId` ("" = unaffiliated) plus the home network
//     the membership hangs off — `homeNetworkKey` (packed 48-bit BSSID, 0 = none) and
//     `homeNetworkName` (its cached label, so the UI reads right with no SD ledger).
//     Player-level. Stored by crew ID, so content_crews.h can be reordered. Pre-v36 → none.
// v37 APPEND `linkEnabled`, the pet-to-pet LINK opt-in. Kept SEPARATE from the audit-scan
//     bit on purpose: that consents to listening, this to broadcasting the operator's
//     identity to anyone in range. Pre-v37 → off, the only safe reading of a save that
//     predates the choice.
// v38 APPEND `netReserved` — RESERVED, always written 0 and ignored on load. It carried a
//     standing STA opt-in, and no longer does: an update job raises the association it
//     needs and drops it when it ends (Game::netConnectWanted), so nothing about that
//     permission survives a reboot. The byte stays because v39+ were appended behind it
//     and the tail is read positionally.
// v39 APPEND `raisedCreatures`, the running tally of every species RAISED as opposed to
//     currently held. Without it, a species is forgotten the instant its successor stamps
//     over it at evolution, and it cannot be derived after the fact — a Script has several
//     possible Process ancestors, so working backwards would credit species never owned.
//     Player-level. Pre-v39 → empty, and Game::applySave unions in the active pet, rack and
//     records, which is the most that can honestly be recovered.
// v40 SUPERSEDES the v25 `achievementsMask` u32 with a length-prefixed BITSET (the u32
//     stays on the wire, written 0, being mid-stream). Indexed by AchievementDef::wire — a
//     number assigned per row and never reused (content_achievements.h) — NOT by table
//     position, so the catalogue can be reordered without a migration. The legacy 14 rows
//     hold wires 0-13 in their original enum order, so a v25..v39 mask migrates bit-for-bit
//     into the first four bytes. Also APPEND `achievementNotified` (the parallel
//     banner-shown set; pre-v40 has none, so an upgraded device announces everything it has
//     ever earned), `bossWins` (seeded on migration from cleared subs/areas, an honest
//     lower bound), `collectedItems` (seeded from what the bag holds), and `speciesDives`
//     (deepest dive per species, where v35 is the pet's own; seeded from the active pet).
// v41 NO BYTES. Marks the point after which every creature id on the wire is one the
//     content tables use: an older blob is run through the rename table below, across
//     every creature-id-bearing field. The version is what lets that pass be skipped for
//     anything newer, and what lets a rename row eventually retire.
// v42 PARALLEL TAIL of `dyingElapsedMs` — how much of the 5/5 recovery window the pet has
//     burned. The one deliberate exception to "a reboot resets the clock": Lockout and the
//     audit cooldown are punishments, but this window is the last step before PERMANENT
//     loss, and refunding it would make the only death path opt-out for anyone who
//     power-cycles. It needs no RTC because it is time AWAKE at 5/5, so a device that is
//     off neither kills the pet nor buys it a reprieve. Pre-v42 → 0.
// v43 NO BYTES. Marks the splice of NET-SEA CROSSING into EXPL rung 2. Every persisted EXPL
//     field is positional, so an older blob's flags from rung 2 on describe the area one
//     rung to their left; `ladderInserts` below opens a blank rung in exactly those saves.
// v44 APPEND `stackerWins`, DEFRAG boards cleared BY HAND. Player-level. It cannot ride on
//     v16's `defragCount`, which is the active pet's tally of defrags of every variant, and
//     nothing else in a save can stand in for it — a bought or rolled defrag leaves the same
//     trace a played one does. Pre-v44 → 0 rather than a number that would over-credit.
// v45 REPLACES the owned-mod spare pool. It was one 24-byte id cell per COPY held plus
//     v18's parallel i32, uncapped, because mods drop from milestones and the only sink is
//     equipping one: on a measured device it reached 424 copies of 24 mods — 11,876 bytes,
//     64% of the save, growing 28 bytes a drop forever. It is now a COUNT PER MOD, a nibble
//     each over ModDef::wire (ownedModCounts, saveModCount): 17 bytes at the current
//     roster, and flat — it grows with the SIZE of the mod table, never with play.
//
//     The rolled per-copy level went with it and was never visible: the picker lists mods
//     by type and shows one gate, so a copy could never be chosen over another. The level
//     is now the mod's own (modEquipLevel, authored on the row), and copies are bounded by
//     kModCopyCapBase, raised by the Rig Shop's MOD STORAGE row. A v44-or-older blob
//     migrates by tallying `ownedMods` per id and clamping to the base cap; the surplus and
//     the rolled levels are dropped.
// v46 NO BYTES. Marks the Worm line collapsing its two Script placeholders into Rootgrub
//     and moving the care branch down to the Daemon. Both retired ids are in the rename
//     table below, so an older blob holding either arrives on Rootgrub.
// v47 APPEND the GAMES arcade's per-cabinet tallies as parallel id/plays/wins runs, the
//     shape speciesDives uses. Keyed by cabinet content id rather than roster position, so
//     the list can be reordered. Pre-v47 → empty.
// v48 APPEND the DECRYPTOGRAM board's per-quote state: a 2-BIT field per QuoteDef::wire
//     (content_quotes.h), four to a byte, length-prefixed like v40's bitsets — the pool is
//     expected to reach the hundreds, and a save should only be as long as the quotes that
//     device has met. The loss ratchet and the win are one axis: 0 never played (HARD), 1
//     lost once (MEDIUM), 2 lost twice or more (EASY, the floor), 3 SOLVED. Pre-v48 → never
//     played.
// v49 NO BYTES. Marks COOKING moving off the Rig Shop: a MERGE HUB recipe is won off a
//     solved Decryptogram, never bought, so the four rows that sold one are gone from
//     kRigUpgrades and every recipe lives in v31's `recipesUnlocked` outright. Two things
//     move without changing the wire's shape, both handled by `migrateRecipeRows` below:
//     the mask widens from the 2 bits mirroring rows 9/10 to one per recipe, and
//     `rigLevelsExt` is positional, so deleting the two mid-table Browns rows shifts every
//     row after them. This is the version to raise kOldestAcceptedVersion past once every
//     device is current, which retires that function.
// v50 PARALLEL TAIL of the per-pet BANDWIDTH-REGEN upgrade — minutes shaved off this pet's
//     regen interval by a Tiramisudo (ItemEffect::BandwidthRegenBonusMin). Per-pet and
//     frozen with the pet, the upgrade belonging to the creature rather than the run.
//     Pre-v50 → 0.
// v52 WIDENS kQuoteWireCap (content_quotes.h) from 256 to 512, doubling the quoteStates
//     array. The wire format does not change — the array has been length-prefixed since
//     v48, so a longer one loads into a shorter build's view as "the quotes it knows". The
//     version moves because the header that owns the cap asks for a note when it does.
// v53 APPEND ROCK THE DOCK's run state (core/model/tournament.h). Five bytes for a whole
//     eight-operator bracket, every entrant being DERIVED from the run seed rather than
//     stored: the seed, the survivor bitmask, the round, and a finished run's verdict.
//     Pre-v53 → a zero seed, which reads as "no run".
// v54 NO BYTES. Renames one ITEM id (`airgap_snack` -> `dyno_nuggets`, see `renamedIds`) —
//     the first rename that is not a creature's, which is why `renameRetiredIds` now sweeps
//     the inventory and the ever-collected set too. The version exists so the rename row has
//     a `sinceVersion` to retire against.
// v55 APPEND the arcade's per-cabinet HIGH SCORE — a fourth run, written as its own tail at
//     the END of the blob rather than beside v47's trio, which is what lets a pre-v55 build
//     read a v55 save and simply not see it. It arrives with the two ENDLESS cabinets (the
//     CHROMATOPHORE and the Isolation buffer), where a run with no finish line has a score
//     and nothing else. Pre-v55 → 0.
// v56 APPEND three lifetime tallies as their own tail, the shape v55 used. They arrive
//     together because they are one change — the achievement board reaching the three
//     subsystems that kept no record at all (ROCK THE DOCK, the LINK duel, the MERGE HUB's
//     stove). None can be reconstructed from anything else in the blob: a bracket leaves no
//     trace once its run is dismissed, a duel pays nothing, and a cooked dish is
//     indistinguishable from a dropped one the moment it is eaten. Pre-v56 → 0.
// v57 APPEND the rest of the per-pet EPIC-DISH grants in v50's shape — the active pet's
//     off-level stat points and XP rate, then a parallel list onto d.rack. A second tail
//     rather than a widening of v50's, because a tail is what a pre-v57 build can stop
//     before. Pre-v57 → 0.
// v58 APPEND ONE BYTE: which background the operator has chosen (content_backgrounds.h) as
//     that row's `wire`, or 0 for AUTO. Its own tail after v57's. Only the CHOICE is here —
//     which backgrounds are OWNED is derived by Game::backgroundOwned from facts the blob
//     already records (a creature raised, an area cleared, brackets taken), so there is no
//     second copy to fall out of step. Pre-v58 → 0, which is AUTO.
//
// v59: the CANT — which sigils of the guardians' language this device has learned, and
//     how many SHAKES have been paid out for them (core/model/cant.h,
//     game_shibboleth.cpp). Two fields and its own tail: `cantSigils` is a 26-bit mask
//     (bit i = the letter 'A'+i reads plain) and `shakesSpent` is the purse's other
//     half — the lifetime handshake count is already here from v7, and what is SPENDABLE
//     is that minus this, so there is no third number to keep in step. Pre-v59 → 0 and
//     0: a save from before the Cant existed has learned none of it and spent nothing,
//     which leaves every shake it ever captured available.
constexpr uint16_t kSaveVersion = 59;

// The oldest blob deserialize will read, and the ONLY thing that retires a rename row
// (see `renamedIds`). Raising it is how a device stops carrying migration weight for saves
// nobody can still hold — and it strands every save older than it, so it moves on a
// deliberate compatibility call about what firmware is in the field, never as a side
// effect of a version bump.
constexpr uint16_t kOldestAcceptedVersion = 53;

// Content ids a blob may still carry under a name the tables no longer answer to. A
// content id is a wire value, so a rename is a FORMAT concern and lives here rather than
// as a permanent alias in the content tables, where it would read as a second legitimate
// name and never be safe to remove.
//
// `sinceVersion` is the first kSaveVersion whose blobs never write `from`, so a row applies
// only to an older blob. Two rules keep the table finite:
//
//   RETIREMENT — a row may be deleted exactly when `kOldestAcceptedVersion > sinceVersion`,
//   at which point no blob the codec will open can carry the old id. A row that HAS become
//   droppable must be dropped: the native gate fails on a dead row.
//
//   FLATTENING — every `from` is distinct and no `to` is ever also a `from`, so one
//   substitution per id is always enough. Renaming something already a `to` means editing
//   that row's `to` in place (its `sinceVersion` stays) AND adding a row for the
//   newly-retired name.
struct RenamedId {
    const char* from;
    const char* to;
    uint16_t sinceVersion;
};
const RenamedId* renamedIds(int& count);

// An area SPLICED INTO THE MIDDLE of the EXPL ladder (kAreaList, areas/area_defs.h).
//
// Every persisted EXPL field is POSITIONAL — indexed by ladder rung, never by area id — so
// a blob written before the splice describes, from `atIndex` on, the area now one rung to
// its LEFT. Read verbatim it would hand the player the new area already cleared and take
// the deepest one back, which makes a mid-ladder insert a FORMAT concern: the codec opens a
// blank rung in each positional field and everything downstream reads a save that looks
// like one written after the splice. Appending to the END of the ladder needs no row.
//
// The ladder-indexed fields are `sectorCleared`, `bossUnlocked`, `subCleared`,
// `subBossUnlocked`, `subRefarm` (row-major), the `titlesUnlocked` bitmask and the
// `equippedTitle` index.
//
// `atIndex` is the rung the area landed on in the ladder AS IT WAS then. Rows apply
// oldest-first, so each reads against the ladder its predecessors built. `sinceVersion` and
// the RETIREMENT rule are `renamedIds`' contract exactly.
struct LadderInsert {
    int atIndex;
    uint16_t sinceVersion;
};
const LadderInsert* ladderInserts(int& count);

// Max home-network display-name length incl. the NUL — matches the ledger's own
// Entry::name cell (core/net/network_ledger.h) and Game::PendingNetwork::name.
constexpr int kSaveNetNameCap = 33;

// A borrowed/owned content id, fixed cell (avoids per-entry heap on embedded).
struct SaveId {
    char id[kSaveIdCap] = {0};
};

struct SaveStack {
    char id[kSaveIdCap] = {0};
    int32_t qty = 0;
};

struct SaveLogEntry {
    uint8_t type = 0;                 // LogEventType
    char text[kSaveTextCap] = {0};
};

// A frozen pet in the ARCH rack: no decay in storage, so its vitals are
// preserved verbatim and restored on Deploy.
struct SaveStoredPet {
    char id[kSaveIdCap] = {0};
    int32_t hunger = 0, frag = 0, happy = 0, mistakes = 0, debuffs = 0;
    uint8_t ghost = 0;
    int32_t generation = 0;
    int32_t defragCount = 0;   // v16: serialized in a parallel tail, not here
    // v26: creature-level state, also serialized in a parallel tail — so a
    // pet's level/earned stats/move-slot typing survive an ARCH Store/Deploy cycle.
    int32_t combatLevel = 0;
    int32_t combatXp = 0;
    int32_t statPoints[kLevelStatCount] = {0};
    uint8_t slotKinds[kMaxMoveSlots] = {0};
    // v29: this pet's own move + mod loadout, serialized in a parallel tail (see
    // the v29 note above) — empty ownedMoves marks a pre-v29 pet (no data to thaw).
    std::vector<SaveId> ownedMoves;
    SaveId equippedMoves[kMaxMoveSlots];
    SaveId equippedMods[kModSlots];
    // v34: elapsed time-in-stage at freeze, serialized in a parallel tail — so a
    // pet's evolution-timer progress survives an ARCH Store/Deploy cycle instead of
    // restarting on Deploy.
    uint32_t timeInStageMs = 0;
    // v35: this pet's own best-ever DeepWeb Dive depth at freeze, serialized in a
    // parallel tail (mirrors defragCount's v16 pattern) — survives an ARCH
    // Store/Deploy cycle instead of resetting on Deploy.
    int32_t bestDeepWebDepth = 0;
    // v42: the 5/5 recovery window burned at freeze, in a parallel tail. `mistakes`
    // above already freezes and thaws, so a pet stored at 5/5 comes back at 5/5 —
    // without this its window would restart on Deploy, and freezing would be the
    // same reprieve a power cycle used to be.
    uint32_t dyingElapsedMs = 0;
    // v50: minutes shaved off this pet's Bandwidth regen interval, in a parallel tail.
    // Frozen with the pet because the upgrade is the creature's, not the rig's — a pet
    // put on the shelf and deployed again keeps what it was fed.
    int32_t bandwidthRegenBonusMin = 0;
    // v57: the rest of what an Epic dish grants for life — the OFF-LEVEL combat-stat
    // points (0 power · 1 defense · 2 speed · 3 max-Health) and the XP rate. Frozen with
    // the pet for the same reason the line above is, and carried in the same parallel
    // tail. These are deliberately NOT folded into `statPoints`: that array's sum is the
    // pet's level, and a granted point is not a level.
    int32_t statBonus[kLevelStatCount] = {0};
    int32_t xpRateBonusPct = 0;
};

// The permanent status of an ARCH record: a greyed, read-only entry that
// does NOT consume a rack slot. Corrupted = lost to Critical System Failure;
// Retired = a Daemon retired/sold (deferred — the enum reserves the value).
enum class RecordStatus : uint8_t { Retired = 0, Corrupted = 1 };

// An ARCH [RETIRED]/[CORRUPTED] record: the lasting trace of a pet that
// died (Critical System Failure) or retired. No vitals — the pet is gone; just the
// species id + which generation + the status, resolved to a name via the registry.
struct SaveRecord {
    char id[kSaveIdCap] = {0};
    uint8_t status = 0;        // RecordStatus
    int32_t generation = 0;
};

// The full snapshot. Defaults describe an empty save (no active pet).
struct SaveData {
    // Active pet: id + vitals/care + the reboot-safe in-stage clock + generation.
    char activeId[kSaveIdCap] = {0};
    int32_t hunger = 0, frag = 0, happy = 0, mistakes = 0, debuffs = 0;
    uint8_t ghost = 0;
    uint32_t timeInStageMs = 0;       // elapsed time-in-stage (not absolute millis)
    int32_t generation = 0;

    // Economy / identity / lifetime counters.
    int32_t bits = 0;
    char hackerTag[kHackerTagMax + 1] = {0};
    uint32_t lifetimeUptimeMs = 0;
    uint32_t lifetimeSteps = 0;       // exploration — tracked, 0 for now
    int32_t petsRaised = 0;

    // Collections.
    std::vector<SaveStack> items;
    // v1..v44 ONLY, read on migration and never written: the spare pool as a flat list
    // of ids, one cell per COPY, each with its own rolled equip level alongside. See the
    // v45 note in the banner for why both are gone.
    std::vector<SaveId> ownedMods;
    std::vector<int32_t> ownedModReqLevels;
    // v45: the spare pool as a COUNT PER MOD, packed a nibble each over ModDef::wire —
    // low nibble = even wire, high = odd (saveModCount below). Copies of a mod are
    // interchangeable, so a save has to remember how many, not which; 4 bits is what
    // bounds kModCopyCapMax.
    std::vector<uint8_t> ownedModCounts;
    std::vector<SaveId> equipped;     // one cell per equip slot ("" = empty)
    std::vector<SaveLogEntry> log;    // chronological, oldest first
    std::vector<SaveStoredPet> rack;

    // v2: combat -----------------------------------------------
    std::vector<SaveId> ownedMoves;     // acquired moves (default is innate, excluded)
    std::vector<SaveId> equippedMoves;  // one cell per move slot ("" = empty)
    int32_t combatXp = 0;      // v11: XP banked toward the NEXT level (0-based model)
    int32_t combatLevel = 0;   // v11: 0-based creature level (was a 1-based vestige)
    // Set by deserialize: true when the blob carried v2 move data. A v1 blob leaves
    // this false so the loader can seed the starting move loadout (graceful migrate).
    bool hasMoveData = false;

    // v3: ARCH records -------------------------------
    std::vector<SaveRecord> records;   // RETIRED/CORRUPTED greyed records (no slot)

    // v4: Hacker Rank -----------------------------------
    int32_t networksSeen = 0;          // lifetime unique Wi-Fi networks
    int32_t hackerRank = 0;            // derived rank, persisted alongside it
    // networkSeenMask (the v4/v21 dedup pool) is REMOVED as of v33 — real-network
    // dedup + history now lives entirely in the SD-backed NetworkLedger
    // (core/net/network_ledger.h), not the save blob. See the v33 note above.

    // v5: Audit-mode passive-scan runtime toggle --------
    // The authorized-use opt-in the player flips in CFG (default OFF). Persisted
    // so the choice survives a reboot; drives whether the device-tier radio does
    // any passive WiFi.scanNetworks() at all. NOT hacker XP — XP is derived
    // (networksSeen * kHackerRankXpPerNetwork), so it needs no field of its own.
    uint8_t netScanEnabled = 0;

    // v6: Audit-mode handshake-CAPTURE runtime toggle ----
    // The authorized-use opt-in for the passive WPA-handshake -> .pcap capture
    // path (a sibling of netScanEnabled). Default OFF; persisted so the choice
    // survives a reboot. Only the toggle intent is stored — the hot-broadcast /
    // re-arm-cooldown timers are runtime-only (no RTC; reset on reboot).
    uint8_t auditCaptureEnabled = 0;

    // v7: Audit dedup ledgers -------------------
    // SHAKES (seenHandshakeBssids + handshakesSeen) never re-credits a handshake
    // already captured — so leaving and returning to a known place grants nothing
    // new. Each entry is a 48-bit packed BSSID (6 bytes). Empty on a pre-v7 blob.
    // (The sibling NETS/seenBssids dedup set was REMOVED as of v33 — see the v33
    // note above; SHAKES is unrelated and unaffected.)
    std::vector<uint64_t> seenHandshakeBssids;  // SHAKES dedup (per-network handshake)
    int32_t handshakesSeen = 0;                 // lifetime unique handshakes captured

    // v8: EXPL sector-clear flags -----------------------
    // One byte per sector (1 = its boss/gauntlet has been cleared). Drives the
    // linear complete-to-advance gate (sector N unlocks once sector N-1 is
    // cleared) and persists it across a reboot. Empty on a
    // pre-v8 blob → nothing cleared (the honest default: only sector 0 is open).
    std::vector<uint8_t> sectorCleared;

    // v9: Boot-Sector incubation timer (redesign) -----------------
    // The remaining incubation clock (game-ms) for an unhatched egg (activeId is
    // the Boot-Sector creature). 0 = not incubating (already hatched, or the save
    // predates the egg-at-idle redesign). A pre-v9 blob defaults to 0, so a
    // migrated pet is treated as fully hatched (the honest default: no in-flight
    // egg to resume). Persisted so an egg survives a reboot mid-incubation.
    uint32_t bootHatchRemainMs = 0;

    // v10: zone-completion Titles --------------------------
    // Player-level (persist across pets, like sectorCleared[]). A clear grants the
    // sector's Title: `titlesUnlocked` is a bitmask (bit i = sector i's Title
    // earned); `equippedTitle` is the shown Title's sector index, or -1 for none.
    // A pre-v10 blob defaults to 0 unlocked / none equipped (the honest default:
    // a save that predates Titles has earned none).
    uint32_t titlesUnlocked = 0;
    int32_t equippedTitle = -1;

    // v11: creature-level stat points ---------------------------
    // The per-pet EARNED combat-stat points behind the XP level (power / defense
    // / speed / max-Health, kLevelStatCount entries). Their sum equals combatLevel
    // (the level == total earned points invariant). Empty on a pre-v11 blob →
    // hasLevelData stays false, and the loader migrates the pet to a fresh level 0
    // (the pre-v11 level was vestigial — never applied to combat — so nothing of
    // value is lost). combatXp/combatLevel above carry the 0-based geometric model.
    std::vector<int32_t> statPoints;
    // Set by deserialize: true when the blob carried the v11 level tail. A pre-v11
    // blob leaves this false so the loader resets the level system to 0 rather than
    // reinterpreting the old 1-based combatLevel as a 0-based one.
    bool hasLevelData = false;

    // v12: EXPL per-sector boss-unlock flags ------------------
    // One byte per sector: has a 10-win streak unlocked that sector's manual boss
    // trigger? Player-level, like sectorCleared[]. Empty on a pre-v12 blob → the
    // loader migrates bossUnlocked[i] = sectorCleared[i] (a cleared sector's boss
    // was obviously beaten, so it stays reachable).
    std::vector<uint8_t> bossUnlocked;

    // v13: EXPL per-sub-area clear + boss-unlock bitmasks --------
    // The unit of progress is now the SUB-AREA. One byte per area, bit s = sub-area
    // s's state: `subCleared` (its boss beaten) and `subBossUnlocked` (a 10-win
    // streak unlocked its manual boss trigger). Player-level, like sectorCleared[].
    // Empty on a pre-v13 blob → the loader migrates a cleared area to all 5 sub-areas
    // cleared + unlocked (a beaten area had every sub-area beaten).
    std::vector<uint8_t> subCleared;
    std::vector<uint8_t> subBossUnlocked;

    // v14: CFG screen-brightness level ------------------------
    // The persisted backlight level as a 0-based index (0..kBrightnessLevels-1).
    // A pre-v14 blob defaults to kBrightnessDefault (brightest) so a migrated save
    // never boots surprisingly dim.
    int32_t brightness = kBrightnessDefault;

    // v15: EXPL per-sub-area re-farm win counts --------------
    // Flat row-major (area*kExplSubAreas + sub) count of post-clear wild wins in each
    // sub-area — the decay input for its diminishing non-Bits drop chances. Player-
    // level, like subCleared[]. Empty on a pre-v15 blob → every count defaults to 0
    // (nothing depleted yet). Length is self-describing (length-prefixed on the wire).
    std::vector<uint16_t> subRefarm;

    // v16: per-pet defrag count ------------------------------
    // The ACTIVE pet's successful-defrag tally. The rack pets' tallies ride on
    // SaveStoredPet::defragCount (serialized in a parallel v16 tail). Pre-v16 → 0.
    int32_t defragCount = 0;

    // v17: permanent-mod semantics ----------------------
    // Set by deserialize: true when the blob is v17+, i.e. `ownedMods` already excludes
    // the equipped mods (the permanent-mod model). A pre-v17 blob leaves this false so
    // the loader migrates by dropping owned ids that also appear in `equipped` (the old
    // swappable model kept equipped mods in the owned pool). No wire field of its own.
    bool hasPermanentModData = false;

    // v18: per-spare mod equip-level gates -------------------
    // Set by deserialize: true when the blob carried the v18 parallel tail. Read only on
    // the v44-and-older migration path now, and never written — v45 dropped the rolled
    // gate, so the levels this flag guards no longer mean anything to a live save.
    bool hasModEquipLevelData = false;

    // v45: the counted mod pool ------------------------------
    // Set by deserialize: true when the blob carried the v45 nibble array. False means a
    // v44-or-older blob, and the loader migrates by tallying `ownedMods` per id and
    // clamping each to kModCopyCapBase — the surplus copies are dropped, which is no
    // loss the player can see: the picker has only ever listed mods by type.
    bool hasModCountData = false;

    // v19: Hacker SHOP account upgrades ----------------------
    // Player-level, persists across pets like the HackerTag. bwUpgradeCount = how many
    // "Increase Bandwidth" upgrades were bought (each raises the farming-pool cap by
    // kBandwidthUpgradeStep). Pre-v19 → 0
    // (a migrated save has bought none). Bandwidth itself is NOT persisted (it refills
    // on reboot); only the purchased CAP increase is.
    int32_t bwUpgradeCount = 0;

    // v20: 'Pedia local-AP runtime toggle ------------
    // The player's opt-in for the standalone Wi-Fi Access Point that serves the
    // 'Pedia landing page (a sibling of netScanEnabled). Default OFF; persisted so
    // the choice survives a reboot. Drives whether the device-tier RadioArbiter
    // brings up SoftAP + the HTTP server at all — and, being the radio's top
    // owner, it stands the audit scan/capture down while on and hands the radio
    // back to whichever of those is still toggled on when it goes off. Pre-v20 → 0.
    uint8_t apEnabled = 0;

    // --- v21: care-mistake shield + once-per-lifetime item gates + shop pre-alloc --
    // Per-pet (reset on a new egg, alongside statPoints). `mistakeShieldActive` = a
    // Restore-Point buff is armed and will block the NEXT positive care mistake;
    // `shieldItemConsumed` = the Restore Point's once-per-lifetime gate is spent;
    // `yubiConsumed` = the Yubi-Cookie's once-per-lifetime −1 removal is spent. Pre-v21
    // → 0 (no shield, neither item consumed).
    uint8_t mistakeShieldActive = 0;
    uint8_t shieldItemConsumed = 0;
    uint8_t yubiConsumed = 0;
    // Player-level (survives lifecycles, like hackerRank). PRE-ALLOCATED for a
    // later shop task; unused otherwise. Pre-v21 → 0.
    uint8_t fragAmountTier = 0;

    // v22: Hacker-SHOP "Reduce Explore Frag TRIGGER %" upgrade (c) ------
    // Player-level (survives lifecycles, like fragAmountTier). The purchased tier
    // (0..kFragTriggerMaxTier) that lowers the battle-fatigue TRIGGER chance —
    // sibling to fragAmountTier (which lowers the frag AMOUNT). Pre-v22 → 0.
    uint8_t fragTriggerTier = 0;

    // v23: Hacker-SHOP one-time unlocks (d/e) ---------------------------
    // Player-level (survive lifecycles, like fragTriggerTier). No tiers — just
    // owned or not. itemTabsUnlocked arms the ITEMS hold-B type-filter cycle;
    // bulkOpenUnlocked arms the VAULT hold-B bulk-open. Pre-v23 → both false.
    uint8_t itemTabsUnlocked = 0;
    uint8_t bulkOpenUnlocked = 0;

    // --- v24: move-slot rework — per-pet Attack/Defend slot typing -----------
    // The stamped Game::SlotKind (0 Unset · 1 Attack · 2 Defend) for each of the
    // kMaxMoveSlots move slots, length-prefixed (mirrors statPoints). Per-pet, reset
    // on a new egg. Pre-v24 → slotKinds stays empty and hasSlotKindData false, so the
    // loader leaves every slot Unset for Game::stampSlotKinds() to re-derive.
    std::vector<uint8_t> slotKinds;
    bool hasSlotKindData = false;

    // --- v25: web-'Pedia reveal-state bookkeeping ---------------------------
    // Creature ids glimpsed-but-not-hatched (Game::markCreatureSeen). Pre-v25 → empty.
    std::vector<SaveId> seenCreatures;
    // Bitmasks over the fixed 6-entry wild-malbeast roster (combat.h::kWildMalbeastIds).
    // Pre-v25 → both 0 (nothing seen/defeated).
    uint16_t malbeastSeen = 0;
    uint16_t malbeastDefeated = 0;
    // The v25 achievements bitmask, one bit per legacy achievement. SUPERSEDED as of v40
    // by `achievementEarned` below: a v25..v39 blob carries real bits here and the loader
    // migrates them (the legacy rows hold wire numbers 0-13, so it is a straight byte
    // copy); a v40+ writer emits 0. The four bytes stay on the wire because the field is
    // mid-stream — see the writer's note on never removing one.
    uint32_t achievementsMask = 0;

    // v27: Hacker-SHOP rig-upgrade levels ---------------------------
    // Player-level (survive lifecycles, like bwUpgradeCount). Pre-v27 → all 0 (a
    // migrated save has bought none).
    int32_t rackSlotUpgradeCount = 0;
    int32_t scrapingClusterLevel = 0;
    int32_t dataMiningLevel = 0;

    // v28: Ambig-USB's armed effect -------------------------------------------
    // Per-pet (reset on a new egg, like mistakeShieldActive). Guarantees the pet's
    // next Process->Script Trojan divert. Pre-v28 → 0 (no divert armed).
    uint8_t forceTrojanDivert = 0;

    // v30: the Backup Drive combat-shield buff --------------------------------
    // Per-pet (reset on a new egg, like mistakeShieldActive). The lifetimeUptimeMs()
    // deadline the buff is armed until; 0 = inactive. Consumed early (set back to 0)
    // the moment it negates a hit in combat, regardless of time remaining.
    uint32_t backupShieldUntilMs = 0;

    // v31: the Merge Hub unlock + the recipes known -----------------------------
    // Player-level (survive lifecycles, like itemTabsUnlocked). mergeHubUnlocked makes
    // the MRG carousel slot accessible; recipesUnlocked is a bitmask over
    // game_internal.h's kMergeRecipes, bit = MergeRecipe::wire. Pre-v31 → both 0 (no
    // Hub, no recipes). Pre-v49 the mask only carried its first two bits and the rest
    // of cooking rode the rig rows — see migrateRecipeRows.
    //
    // The whole set lives in `recipeOwned` below (v51). This u32 stays on the wire at its
    // v31 position carrying wires 0-31, so a v31..v50 reader still gets the recipes it can
    // name — an OTA rollback must not cost a player the kitchen it understands.
    uint8_t mergeHubUnlocked = 0;
    uint32_t recipesUnlocked = 0;

    // v32: Rig Shop rows from kRigRowExtBase up ---------------------------
    // Player-level (survive lifecycles, like bwUpgradeCount). Index i = rig row
    // (kRigRowExtBase + i), value = its purchased level. Pre-v32 → empty (every such
    // row defaults to 0 — a migrated save has bought none of them).
    std::vector<uint16_t> rigLevelsExt;

    // v36: Hacker CREW allegiance + the home network it hangs off -------------
    // Player-level (survive lifecycles, like hackerTag). `crewId` is a content id
    // (content_crews.h), empty = unaffiliated. `homeNetworkKey` is a packed 48-bit
    // BSSID (0 = none designated) and `homeNetworkName` its cached display label.
    // Pre-v36 → no crew, no home network.
    char crewId[kSaveIdCap] = {0};
    uint64_t homeNetworkKey = 0;
    char homeNetworkName[kSaveNetNameCap] = {0};

    // v37: the pet-to-pet LINK opt-in --------------------------------------
    // Player-level, default OFF. Consent to BROADCAST identity, which is why it is
    // its own bit rather than riding netScanEnabled's consent to listen. The met
    // operators themselves are not here — they live in the SD-backed PeerLedger
    // (core/net/peer_ledger.h), like the network history.
    bool linkEnabled = false;

    // v38: RESERVED ----------------------------------------------------------
    // Held a standing internet/STA opt-in. Nothing reads it: the association is
    // raised and dropped by the update job that needs it, so there is no permission
    // left to persist. Kept as a placeholder because the v39+ tail sits behind it.
    bool netReserved = false;

    // --- v39: the device's raised-species tally ------------------------------
    // Every creature id the operator has ever raised, whether or not anything still
    // points at it. Player-level (survives a pet reset, like seenCreatures above).
    // Pre-v39 → seeded by the loader from the active pet + rack + records.
    std::vector<SaveId> raisedCreatures;

    // --- v40: achievements + the counters they are measured against ----------
    // Two bitsets indexed by AchievementDef::wire (content_achievements.h), stored
    // length-prefixed so the catalogue can outgrow any fixed width: a longer set loads
    // into a shorter build harmlessly, and a shorter one reads back as "not earned".
    // Player-level, like titlesUnlocked. Pre-v40 → migrated from achievementsMask above,
    // with nothing marked notified so an upgraded device announces its whole history.
    std::vector<uint8_t> achievementEarned;
    std::vector<uint8_t> achievementNotified;
    // Lifetime boss ROUNDS won (a gauntlet round counts as one). Pre-v40 → seeded by the
    // loader from cleared sub-areas + areas, the most a save that never counted can say.
    int32_t bossWins = 0;
    // Every item id that has ever been held. Not "in the bag now" (which the 'Pedia's
    // items{} map already reports) — this is what the collection achievements count, and
    // spending an item must not un-collect it. Pre-v40 → seeded from the held stacks.
    std::vector<SaveId> collectedItems;
    // The deepest DeepWeb Dive reached by each SPECIES: parallel id/depth lists, so a
    // record outlives the individual pet that set it. Distinct from bestDeepWebDepth
    // (v35), which is one pet's own and is what the Zero-Day Bell warps to. Pre-v40 →
    // seeded from the active pet's own record.
    std::vector<SaveId> speciesDiveIds;
    std::vector<int32_t> speciesDiveDepths;

    // How much of the 5/5 recovery window this pet has already spent, in ms AWAKE at
    // 5/5 — not wall time, which a board with no RTC cannot measure across a boot.
    // Per-pet; zeroed the moment care recovers below 5/5, so a later brush with death
    // starts clean. Pre-v42 → 0.
    uint32_t dyingElapsedMs = 0;

    // --- v44: the played-defrag tally ----------------------------------------
    // DEFRAG minigame boards cleared, lifetime. Player-level (survives a pet reset, like
    // bossWins above); distinct from the per-pet `defragCount`, which counts defrags of
    // every variant. Pre-v44 → 0; nothing in an older blob can be honestly read as this.
    int32_t stackerWins = 0;

    // --- v47: the arcade's per-cabinet tallies -------------------------------
    // Three parallel runs: a cabinet's content id, its lifetime run count, and how many
    // of those runs were won. Player-level, like stackerWins above. Id-keyed, so the
    // GAMES roster (content_arcade.cpp) can be reordered and a retired cabinet's row
    // simply stops resolving. Pre-v47 → empty.
    std::vector<SaveId> arcadeIds;
    std::vector<int32_t> arcadePlays;
    std::vector<int32_t> arcadeWins;

    // --- v55: the arcade's per-cabinet HIGH SCORE ----------------------------
    // A fourth run parallel to arcadeIds above, written as its own tail at the end of
    // the blob rather than beside them, so a pre-v55 build still parses a v55 save.
    // Empty (or short) → 0, which is the truth for a device that has only ever played
    // cabinets that had no score to keep.
    std::vector<int32_t> arcadeBest;

    // --- v56: three more lifetime tallies ------------------------------------
    // Player-level, like bossWins/stackerWins above, and written as one tail at the end
    // of the blob rather than beside them. Pre-v56 → 0; see the version note in the
    // banner for why none of the three can be honestly seeded from an older save.
    int32_t tourneyWins = 0;    // ROCK THE DOCK brackets taken
    int32_t pvpWins = 0;        // LINK duels won
    int32_t mergesCooked = 0;   // dishes cooked at the MERGE HUB
    // The chosen background's BackgroundDef::wire, or 0 for AUTO (the pet decides).
    // A wire this build has no row for reads back as AUTO — see backgroundByWire.
    uint8_t backgroundPick = 0;

    // --- v48: the DECRYPTOGRAM board's per-quote state -----------------------
    // Two bits per QuoteDef::wire, low pair first within each byte. Player-level, like
    // the achievement bitsets it borrows its length-prefixed shape from; pack/unpack
    // through quoteStateGet/quoteStateSet below so no call site does the shifting.
    // Pre-v48 → empty, and every quote reads as never played.
    std::vector<uint8_t> quoteStates;

    // --- v50: the ACTIVE pet's Bandwidth-regen upgrade -----------------------
    // Minutes shaved off its regen interval by a Tiramisudo. Per-pet (reset on a new
    // egg, like mistakeShieldActive) AND frozen with the pet — the rack pets' own
    // values ride on SaveStoredPet::bandwidthRegenBonusMin, a parallel tail. Pre-v50 →
    // 0, which is the truth: nothing granted it before this version.
    int32_t bandwidthRegenBonusMin = 0;

    // --- v57: the ACTIVE pet's other permanent Epic-dish grants --------------
    // Off-level combat-stat points and the XP rate (core/model/pet_upgrades.h). Same
    // shape and the same reason as the v50 pair above: per-pet, reset on a new egg, and
    // frozen with the pet, so the rack's own values ride on SaveStoredPet::statBonus /
    // ::xpRateBonusPct in a parallel tail. Pre-v57 → 0, which is the truth.
    int32_t statBonus[kLevelStatCount] = {0};
    int32_t xpRateBonusPct = 0;

    // --- v51: the owned-recipe bitset ----------------------------------------
    // One bit per MergeRecipe::wire (game_internal.h), low bit first within each byte,
    // stored length-prefixed so the kitchen can outgrow any fixed width — the same
    // shape and the same reason as the v40 achievement bitsets. Supersedes the v31
    // `recipesUnlocked` u32 above, which still carries wires 0-31 for readers that
    // predate this. Player-level. Pre-v51 → seeded by the loader from that u32, so an
    // upgraded device keeps every recipe it had.
    std::vector<uint8_t> recipeOwned;

    // --- v53: ROCK THE DOCK's run in play ----------------------------------------
    // The whole bracket, because the seed IS the bracket (core/model/tournament.h):
    // `tourneySeed` 0 means no run, `tourneyAlive` is one bit per original slot,
    // `tourneyRound` is 0-based, and `tourneyPhase` is Game::TourneyPhase — persisted
    // so a device put down on a verdict is still showing it after a reboot. Player-
    // level, not per-pet: the operator entered the draw, and a run outlives a pet the
    // way the HackerTag and the earned Titles do. Pre-v53 → a zero seed (no run).
    uint32_t tourneySeed = 0;
    uint8_t tourneyAlive = 0;
    uint8_t tourneyRound = 0;
    uint8_t tourneyPhase = 0;

    // v35: this pet's own best-ever DeepWeb Dive depth ------------------------
    // Per-pet (reset on a new egg, like mistakeShieldActive). The active pet's
    // value; the rack pets' own values ride on SaveStoredPet::bestDeepWebDepth
    // (a parallel tail, mirrors defragCount's v16 pattern). Pre-v35 → 0 (never
    // dived).
    int32_t bestDeepWebDepth = 0;

    // --- v59: the CANT -------------------------------------------------------
    // Player-level, not per-pet: the operator's device learned this language, and it
    // outlives a pet the way the HackerTag, the Titles and the network ledger do. See
    // the version note above for why SPENT rather than UNSPENT is the field.
    uint32_t cantSigils = 0;
    int32_t shakesSpent = 0;
};

// Read/write one mod's spare count in the v45 packed pool (SaveData::ownedModCounts) by
// its ModDef::wire. Two mods to a byte: even wires in the low nibble, odd in the high,
// so a count is 0..15 (kModCopyCapMax) and the whole pool is one byte per two mods
// however many copies are held. Out-of-range or unwritten wires read as 0 — the same
// "not held" a shorter array from an older build reads as.
inline int saveModCount(const std::vector<uint8_t>& packed, int wire) {
    if (wire < 0) return 0;
    const size_t byte = static_cast<size_t>(wire) / 2;
    if (byte >= packed.size()) return 0;
    return (wire & 1) ? (packed[byte] >> 4) : (packed[byte] & 0x0F);
}

// Set one mod's count, growing `packed` to reach it. Counts above 15 clamp rather than
// wrap into the neighbouring mod's nibble, which is the one way this could silently
// hand the player copies of something they never earned.
inline void saveSetModCount(std::vector<uint8_t>& packed, int wire, int count) {
    if (wire < 0 || count <= 0) return;
    if (count > 15) count = 15;
    const size_t byte = static_cast<size_t>(wire) / 2;
    if (byte >= packed.size()) packed.resize(byte + 1, 0);
    if (wire & 1) packed[byte] = static_cast<uint8_t>((packed[byte] & 0x0F) | (count << 4));
    else          packed[byte] = static_cast<uint8_t>((packed[byte] & 0xF0) | count);
}

// Encode `data` into the on-disk blob (magic + version + stream), replacing whatever
// `out` held and keeping the capacity it already has. Writing into a caller-owned
// buffer is what lets the device pay the blob-sized allocation once and reuse it for
// every write (Game::persistSave), rather than at the moment a save fires — which on
// a board with the radio up may be the moment there is no such block to be had.
void serializeSaveInto(const SaveData& data, std::vector<uint8_t>& out);

// Encode `data` into a fresh buffer, for callers with none to reuse.
std::vector<uint8_t> serializeSave(const SaveData& data);

// Decode `blob` into `out`. Returns false (and leaves `out` defaulted) on an
// empty blob, a bad magic, an unknown version, or a truncated stream.
bool deserializeSave(const std::vector<uint8_t>& blob, SaveData& out);

} // namespace mal
