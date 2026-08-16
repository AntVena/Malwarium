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
// v2 appends the move loadout + combat XP/level; v3 appends the ARCH
// records (RETIRED/CORRUPTED, from Critical System Failure); v4 appends the
// Hacker Rank progression (lifetime networks-seen + its dedup pool + the
// derived rank); v5 appends the Audit-mode passive-scan runtime
// toggle (— the authorized-use opt-in, replaces the compile-time
// NET_SNIFF_ENABLED default); v6 appends the Audit-CAPTURE runtime toggle (
// the passive WPA-handshake ->.pcap opt-in, a sibling of the scan toggle);
// v7 appends the real-radio Audit dedup ledgers (the seen-BSSID sets behind NETS +
// the lifetime handshake count behind SHAKES) so a reboot never re-credits a
// network/handshake already counted ("leave the house and come home"); v8 appends
// the EXPL sector-clear flags (— one byte per sector, so linear
// complete-to-advance gating survives a reboot); v9 appends the Boot-Sector
// incubation timer (the freshly hatched egg's remaining decrypt clock, so an
// unhatched egg survives a reboot mid-incubation, redesign); v10 appends the
// zone-completion Titles (— the unlocked-Title bitmask + the equipped
// Title index, player-level so they persist across pets, like the sector-clear flags);
// v11 appends the creature-level stat points (— the per-pet earned combat-stat
// points behind the XP level; combatXp/combatLevel move to the 0-based geometric model
// and a pre-v11 blob's vestigial linear level is dropped, migrating the pet to a fresh
// level 0); v12 appends the EXPL per-sector boss-unlock flags (— a 10-win
// streak unlocks a sector's manual boss trigger; one byte per sector, player-level
// like sectorCleared[]). A pre-v12 blob has none stored, so the loader migrates
// bossUnlocked[i] = sectorCleared[i] (a cleared sector's boss was obviously beaten).
// v13 reworks EXPL into nested sub-areas: the unit of progress moves from the
// sector to the SUB-AREA, so it appends per-area 5-bit bitmasks for sub-area CLEARED
// and sub-area BOSS-UNLOCK state (one byte per area). A pre-v13 blob has none stored,
// so the loader migrates a CLEARED area → all 5 sub-areas cleared + all 5 sub-bosses
// unlocked (the honest default: a beaten area had every piece beaten). The pre-v13
// per-sector bossUnlocked byte is no longer used at runtime (superseded by the
// per-sub flags) but is still serialized so the stream stays version-layered.
// v14 appends the CFG screen-brightness level (— a 0-based backlight index,
// kBrightnessLevels steps). A pre-v14 blob has none stored, so the loader defaults it
// to kBrightnessDefault (brightest — no surprise dimming on a migrated save).
// v15 appends the EXPL per-sub-area RE-FARM win counts — a flat row-major
// list (area*kExplSubAreas+sub) of how many wild wins have been farmed in each sub-area
// AFTER it was cleared, which decays its non-Bits drop chances. A pre-v15 blob has none
// stored, so the loader defaults every count to 0 (a migrated save's cleared areas
// start un-depleted — full drops until re-farmed).
// v16 appends the per-pet DEFRAG COUNT: the ACTIVE pet's successful-defrag
// tally, then a parallel list of the RACK pets' tallies (same order as the rack), so
// the count stays consistent through ARCH freeze/thaw. Appended (not folded into the
// mid-stream rack records) to keep the rack byte-compatible. A pre-v16 blob has none
// stored → every tally defaults to 0.
// v17 does NOT add wire fields — it REINTERPRETS the existing mod collections for the
// permanent-mod model (D3). Pre-v17, mods were swappable and `ownedMods`
// listed EVERY owned mod, equipped ones included. From v17, a mod is consumed when
// equipped, so `ownedMods` holds only the un-equipped SPARES and `equipped` holds the
// permanently-installed mods. The version alone is the marker (hasPermanentModData);
// the loader migrates a pre-v17 blob by dropping any owned id that also sits in a slot
// (otherwise a migrated save would gift a duplicate spare of each installed mod).
// v18 appends the PER-SPARE mod equip-level gates (— mods into combat). Each
// un-equipped spare in `ownedMods` now carries a rolled required pet-level (a mod's
// power tier sets a nominal band; each dropped copy rolls its own gate within it). To
// stay byte-compatible with the v17 `ownedMods` cells, the levels ride a PARALLEL tail
// (`ownedModReqLevels`, one i32 per owned spare, same order) — the same pattern v16's
// defrag tallies used. A pre-v18 blob has none stored → every spare defaults to req 0
// (freely equippable: a mod earned under the old model isn't retroactively gated).
// v21 appends a trailing block: the per-pet care-mistake SHIELD state
// (mistakeShieldActive — a Restore-Point buff blocks the next positive care
// mistake; shieldItemConsumed — the once-per-lifetime gate on the Restore Point;
// yubiConsumed — the once-per-lifetime gate on the Yubi-Cookie −1 removal), and
// two player-level fields: fragAmountTier (PRE-ALLOCATED for a later shop task,
// left 0) and — widening an existing field, not appending — networkSeenMask grows
// from uint8_t to uint64_t so the deduped Wi-Fi pool can reach the top Hacker-Rank
// tier (kNetworkDedupPoolSize raised to 40). A pre-v21 blob has no v21 tail → the
// per-pet flags default to 0 (no shield, neither once-per-lifetime item consumed)
// and fragAmountTier to 0; the legacy 8-bit networkSeenMask migrates into the low
// 8 bits of the widened field. The per-pet flags reset on a new egg (startHatch);
// networkSeenMask + fragAmountTier are player-level (survive lifecycles).
// v23 appends the d/e Hacker-SHOP one-time account unlocks: itemTabsUnlocked
// (arms the ITEMS hold-B FOOD/BUFFS/QUEST filter cycle) and bulkOpenUnlocked (arms
// the VAULT hold-B "open every cache of this rarity" gesture) — player-level booleans,
// no tiers, siblings of fragAmountTier/fragTriggerTier. A pre-v23 blob has no tail →
// both default to false (a migrated save has bought neither).
// v24 appends the move-slot rework's (#12) per-pet Attack/Defend SLOT TYPING: the
// stamped SlotKind (0 Unset · 1 Attack · 2 Defend) for each of the kMaxMoveSlots move
// slots on the ACTIVE pet, length-prefixed like statPoints. Per-pet (reset on a new
// egg alongside statPoints/combatLevel — the single new-egg chokepoint, Game::
// startHatch); NOT reset on an ARCH Deploy swap (mirrors the pre-existing gap that
// moveLoadout_/statPoints_ aren't per-stored-pet either). A pre-v24 blob has no tail
// → hasSlotKindData stays false and every slot loads Unset, so Game::stampSlotKinds()
// deterministically re-derives it from the loaded pet's CreatureDef::slotKinds — a
// clean migration, not a guess.
// v25 appends the web-'Pedia reveal-state bookkeeping: three
// tails, all player-level (survive a pet reset, like titlesUnlocked). `seenCreatures`
// is a length-prefixed vector of creature ids the player has FACED IN COMBAT without
// ever raising (today: a duel opponent's species, Game::markCreatureSeen) — "hatched"
// still wins over "seen" in the 'Pedia's pets{} map. `malbeastSeen`/
// `malbeastDefeated` are bitmasks over the fixed 6-entry wild-malbeast roster
// (combat.h kWildMalbeastIds; bit i = that roster index), set the moment a wild
// encounter rolls that malbeast (seen) or a live wild fight against it is WON
// (defeated) — sub/area bosses and Sim dummies never touch these bits.
// `achievementsMask` is one bit per Game::Achievement (10 ids today, matching
// web/data/pedia_data.js). A pre-v25 blob has no tail →
// seenCreatures stays empty and both masks default to 0 (nothing glimpsed, nothing
// achieved — the honest default for a save that predates this system).
// v26 appends the per-STORED-PET creature-level state: a rack pet's
// combatLevel/combatXp/statPoints/slotKinds were never captured on Store and never
// restored on Deploy — only the currently-active pet's level lived anywhere, so
// swapping a pet through the rack silently dropped its level (or worse, the newly
// active pet inherited whatever level state the previously-active pet happened to
// have, since those fields are plain Game members that Deploy never touched). This
// closes the gap: SaveStoredPet gains combatLevel/combatXp/statPoints/slotKinds,
// serialized in a PARALLEL tail (mirrors defragCount's v16 pattern) so the mid-stream
// rack records stay byte-compatible. A pre-v26 blob has no tail → every already-stored
// pet migrates to level 0 / all-Unset slots (no level history was ever recorded for a
// stored pet before this version — nothing worse than the pre-fix behaviour).
// v27 appends the Hacker-SHOP rig-upgrade levels: rackSlotUpgradeCount (ARCH
// rack capacity = kRackSlots + this), scrapingClusterLevel (+combat-Bits % per level),
// and dataMiningLevel (+cache-Bits % per level). Player-level, persist across pets like
// bwUpgradeCount. A pre-v27 blob has no tail → all three default to 0 (a migrated save
// has bought none).
// v28 appends forceTrojanDivert — the Ambig-USB's armed effect (guarantees the pet's
// next Process->Script Trojan divert, replacing the kTrojanDivertPct roll). Per-pet,
// like mistakeShieldActive; resets on a new egg. A pre-v28 blob has no tail → stays
// false (no divert armed).
// v29 appends the per-STORED-PET move + mod loadout — a pet's MOVES/MODS state
// has-a home on the pet, not on the player. SaveStoredPet gains `ownedMoves` (this
// pet's earned move pool, length-prefixed) + `equippedMoves` (kMaxMoveSlots) +
// `equippedMods` (kModSlots), serialized in a parallel tail (mirrors v26) so
// mid-stream rack records stay byte-compatible. The mod SPARE pool stays
// player-level (SaveData::ownedMods) — mods are found like items; only the
// installed slots belong to a specific pet. A pre-v29 stored pet has an empty
// tail: Game::archDeployStored reads an empty `ownedMoves` as unset and seeds the
// pet's line-starting kit, and its mod slots load empty.
// v30 appends backupShieldUntilMs — the Backup Drive buff's armed-until deadline
// (Game::lifetimeUptimeMs(), 0 = inactive). Per-pet, like mistakeShieldActive; resets
// on a new egg. A pre-v30 blob has no tail → stays 0 (no shield armed).
// v31 appends the f Hacker-SHOP Merge Hub unlock (mergeHubUnlocked — makes the MRG
// carousel slot accessible) and recipesUnlocked, a bitmask over game_internal.h's
// kMergeRecipes table (bit = MergeRecipe::wire, see v49 for the widening). Both
// player-level, survive lifecycles like itemTabsUnlocked. A pre-v31 blob has no tail →
// both default to 0 (has neither the Hub nor any recipe).
// v32 appends rigLevelsExt — a forward-compatible tail for every Rig Shop row from
// kRigRowExtBase up (game_rig_shop.h's kRigUpgrades), so a new row persists without
// another named field + version bump: index i in the vector = rig row
// (kRigRowExtBase + i), value = its purchased level. Player-level, survives lifecycles. A pre-v32 blob (or one
// short of a later-added row) has no tail/entry → that row defaults to level 0
// (never bought). Rows 0-10 keep using their own named fields above (unchanged).
// v33 REMOVES the v4/v21 networkSeenMask byte+u64 and the v7 seenBssids vector —
// real-network dedup + history moved off the save blob entirely, onto the SD-backed
// NetworkLedger (core/net/network_ledger.h); the EXPL Wi-Fi event is now the only
// place a real network is credited (Game::resolveNetworkDiscovery), never the
// background scan. `networksSeen`/`hackerRank` (the small derived counters) are
// unaffected and keep serializing exactly as before. A v4..v32 blob still carries
// those bytes on the wire — the reader keeps consuming them (version-gated `< 33`)
// so every later field stays byte-aligned, it just no longer stores them anywhere;
// a v33+ writer never emits them. seenHandshakeBssids/handshakesSeen (the unrelated
// SHAKES audit-capture ledger) are untouched.
// v34: per-stored-pet evolution-timer elapsed-in-stage, parallel to d.rack (mirrors
// defragCount's v16 pattern) so a rack pet's evolution progress survives an ARCH
// Store/Deploy cycle instead of being reset on Deploy.
// v35: bestDeepWebDepth — this pet's own highest-ever DeepWeb Dive depth
// (exploreStreak_ while inDeepWebDive()), read back by the Zero-Day Bell
// (SetDeepWebStartDepthToBest) to warp a fresh dive to THIS pet's frontier, never a
// device-wide max. Per-pet, reset on a new egg; the active pet's value plus a
// parallel list mapped onto d.rack by index (mirrors defragCount's v16 pattern) so
// it survives an ARCH Store/Deploy cycle. A pre-v35 blob has no tail → every pet
// reads back as 0 (never dived).
// v36 appends the Hacker-face CREW state: `crewId` (the enlisted crew's content id,
// "" = unaffiliated) plus the HOME NETWORK the membership hangs off — `homeNetworkKey`
// (a packed 48-bit BSSID, 0 = none designated) and `homeNetworkName` (its cached
// label, so the UI reads right even on a boot with no SD-backed ledger). All three are
// player-level and survive a pet reset, like the HackerTag. The crew is stored by ID,
// not row index, so content_crews.h's table can be reordered. A pre-v36 blob has no
// tail → no crew, no home network (the honest default for a save predating the system).
// v37 appends `linkEnabled` — the pet-to-pet LINK opt-in (default OFF), a player-level
// CFG toggle alongside netScanEnabled/apEnabled. Kept SEPARATE from the audit-scan bit
// on purpose: that one consents to listening, this one consents to broadcasting the
// operator's identity to anyone in range. A pre-v37 blob has no tail → LINK off, which
// is both the default and the only safe reading of a save that predates the choice.
// v38 appends `netReserved` — RESERVED, always written 0 and ignored on load.
// It carried a standing internet/STA opt-in, and no longer does: reaching the 'net is
// no longer something the device can be left permitted to do. An update job raises the
// association it needs and drops it when it ends (Game::netConnectWanted), so the
// permission and the job have exactly one lifetime between them and nothing about it
// survives a reboot. The byte stays because v39..v42 were appended behind it and the
// tail is read positionally — dropping it would misalign every later field. A save
// written when the bit still meant something reads back as reserved, which lands that
// device off the 'net: the safe direction, and the only honest one.
// v39 appends `raisedCreatures` — the device's running tally of every species the
// operator has RAISED, as opposed to the ones they happen to hold right now. Without
// it the only record a creature ever existed is that something still points at it
// (the active pet, an ARCH rack slot, an ARCH record), so a species is forgotten the
// instant its successor stamps over it at evolution: walk a line to Daemon and every
// rung behind it goes back to unknown. It cannot be derived after the fact either —
// a Script has several possible Process ancestors, so working backwards from what is
// held would credit species that were never owned. Player-level, like `seenCreatures`
// next to it: it survives a pet's death, a new egg, and a Factory-Reset-free rebuild
// of the rack. A pre-v39 blob has no tail → it reads back empty, and Game::applySave
// unions in the active pet + rack + records, which is the most that can honestly be
// recovered from a record that never kept this (anything evolved past left no trace
// in the old format at all).
// v40 supersedes the v25 `achievementsMask` u32 with a length-prefixed BITSET (the u32
// stays on the wire, written as 0 — it is mid-stream, and this format never removes a
// mid-stream field) and adds the counters the expanded achievement catalogue is measured
// against. The mask had to go because 32 rows is not many; the bitset is indexed by
// AchievementDef::wire (a number
// assigned per row and never reused — content_achievements.h), NOT by table position, so
// the catalogue can be reordered or regrouped without a migration. The legacy 14 rows
// hold wire numbers 0-13 in their original enum order, so a v25..v39 blob's mask migrates
// bit-for-bit into the first four bytes. `achievementNotified` is the parallel
// "has its banner been shown" set — a pre-v40 blob has none, which is deliberate: an
// upgraded device announces everything it has ever earned, including the rows this
// version retro-awards on first boot. Also appended: `bossWins` (lifetime boss rounds
// won, seeded on migration from the cleared sub-areas/areas — an honest lower bound, the
// most a save that never counted them can say), `collectedItems` (every item id ever
// held, behind the cuisine/rarity rows; seeded from what the bag holds at load), and
// `speciesDives` (deepest DeepWeb Dive per species — a record that outlives the pet,
// where v35's bestDeepWebDepth is the pet's own; seeded from the active pet's).
// v41 adds no bytes. It marks the point after which every creature id on the wire is
// the id the content tables actually use: a blob written before it is run through
// the rename table below, which rewrites retired ids across every creature-id-bearing
// field (active pet, rack, records, seen/raised, species dives). The version is what
// lets that pass be SKIPPED for anything newer, and what lets a rename row retire —
// without it the alias would have to live forever in the content tables, where a stale
// id would read as a second name for a creature rather than as a wire-format concern.
// v42 appends `dyingElapsedMs` — how much of the 5/5 recovery window the pet has
// already burned. The single deliberate exception to the "reboot resets the clock"
// rule (core/net/audit_capture.h states it for the penalty windows): Lockout and the
// audit cooldown are punishments, so refunding them on reboot only costs a little
// cheese, but this window is the last step before PERMANENT loss — refunding it makes
// the game's only death path opt-out for anyone who notices the pulse and power-cycles.
// It still needs no RTC, because it is time AWAKE at 5/5, not wall time: a device that
// is off is not counting, so being switched off neither kills the pet nor buys it a
// reprieve. The active pet's value, then a parallel list mapped onto d.rack by index
// (mirrors defragCount's v16 pattern) — a rack pet's `mistakes` already survives
// freeze/thaw, so without the tail an ARCH Store/Deploy would restart the window and
// simply move the cheese. Pre-v42 → 0, reading a migrating save as entering fresh.
// deserialize accepts every version from kOldestAcceptedVersion up: an older blob
// loads with the newer fields defaulted (forward-compat is the contract). Health is
// transient and never serialized.
// v43 adds no bytes. It marks the splice of NET-SEA CROSSING into the middle of the EXPL
// ladder (rung 2, between The Pirate Bayou and Napstorrent Moors). Every persisted EXPL
// field is positional, so a v42-or-older blob has its flags from rung 2 on describing the
// area one rung to their left; the version is what lets `ladderInserts` (below) open a
// blank rung in exactly those saves and skip the pass for anything newer.
// v44 appends `stackerWins` — how many DEFRAG minigame boards the operator has cleared by
// hand. Player-level, and the counter behind the played-defrag ladder: it cannot ride on
// `defragCount` (v16), which is the ACTIVE PET's tally of defrags of every variant and
// resets with the pet, where this is a record of the operator's own skill. Nothing else in
// a save can stand in for it either — a bought or rolled defrag leaves the same trace a
// played one does — so a pre-v44 blob reads back 0 and starts the ladder fresh rather than
// being seeded from a number that would over-credit every Quick defrag ever run.
// v45 replaces the owned-mod spare pool. It was a flat list — one 24-byte id cell per
// COPY held, plus the v18 parallel i32 of that copy's rolled equip level — and it had no
// cap, because mods drop from milestones and the only sink is equipping one. On a
// measured device it had grown to 424 copies of 24 mods (132 of them the same mod):
// 11,876 bytes, 64% of the entire save, growing 28 bytes a drop forever.
//
// It is now a COUNT PER MOD, a nibble each over ModDef::wire (ownedModCounts,
// saveModCount) — 17 bytes at the current roster, and flat: it grows with the SIZE of
// the mod table, never with play. The rolled per-copy level went with it. That roll was
// never visible: the picker lists mods by type and shows one gate, so a copy could never
// be chosen over another, and the level is now the mod's own (modEquipLevel, authored on
// the row). Copies are bounded by kModCopyCapBase, raised by the Rig Shop's MOD
// STORAGE row.
//
// A v44-or-older blob migrates by tallying `ownedMods` per id and clamping to the base
// cap; the surplus and the rolled levels are dropped. Nothing player-visible is lost —
// no screen has ever shown a copy, only a type and a count.
// v46 adds no bytes. It marks the Worm line collapsing its two Script placeholders into
// one designed Script row (Rootgrub) and moving the care branch down to the Daemon, the
// shape every other line already had. Both retired ids are in the rename table below, so
// a pre-v46 blob holding either arrives on Rootgrub; the version is what lets that pass
// be skipped for anything newer, and what will let those two rows retire.
// v47 appends the GAMES arcade's per-cabinet tallies — how many runs each cabinet has
// seen and how many were won — as parallel id/plays/wins runs, the shape speciesDive
// already uses. Keyed by the cabinet's content id rather than by roster position, so
// the GAMES list can be reordered or extended without a migration. Pre-v47 → empty,
// which is the truth: nobody has played a cabinet that did not exist.
// v48 appends the DECRYPTOGRAM board's per-quote state — a 2-BIT field per
// QuoteDef::wire (content_quotes.h), four to a byte, length-prefixed like the v40
// achievement bitsets and for the same reason: the pool is expected to reach the
// hundreds, and a save should only be as long as the quotes that device has actually
// met. The four states are the whole record a quote needs, because the loss ratchet and
// the win are one axis: 0 never played (and so HARD), 1 lost once (MEDIUM), 2 lost twice
// or more (EASY, the floor), 3 SOLVED. A pre-v48 blob has no tail → every quote reads
// back as never played, which is the honest default for a device that had no board.
// v50 appends the per-pet BANDWIDTH-REGEN upgrade — minutes shaved off this pet's
// Bandwidth regen interval by a Tiramisudo (ItemEffect::BandwidthRegenBonusMin), as the
// active pet's value then a parallel list mapped onto d.rack by index. The same shape
// as v42's dying window, and for the same reason: it is per-pet state that must ALSO
// survive an ARCH freeze, since the upgrade belongs to the creature rather than the run.
// A pre-v50 blob has no tail → 0 for every pet, which is the truth (no dish that grants
// it existed).
// v49 adds no bytes. It marks COOKING moving off the Rig Shop: a MERGE HUB recipe is
// won off a solved Decryptogram, never bought, so the four rows that used to sell one
// are gone from kRigUpgrades and every recipe now lives in the v31 `recipesUnlocked`
// mask outright (bit = MergeRecipe::wire). Two things move on the wire without changing
// its shape, both handled by `migrateRecipeRows` below:
//   * the mask widens from the 2 bits that mirrored rows 9/10 to one per recipe, and
//   * `rigLevelsExt` is positional, so deleting the two mid-table Browns recipe rows
//     shifts every row after them — an older blob's entries are remapped by hand.
// A v49+ blob needs neither pass. This is also the version to raise
// kOldestAcceptedVersion past once every device is current, which retires that function.
// v52 widens kQuoteWireCap (content_quotes.h) from 256 to 512, which doubles the
// quoteStates array. The wire format does not change: the array has been length-
// prefixed since v48, so a longer one loads into a shorter build's view as "the
// quotes it knows" and a shorter one reads back as "never played". The version moves
// anyway, because the header that owns the cap asks for a note when it does.
// v53 appends ROCK THE DOCK's run state — the arena bracket held in The Pirate Bayou
// (core/model/tournament.h). Five bytes for a whole eight-operator tournament, because
// every entrant is DERIVED from the run seed rather than stored: the seed, the
// survivor bitmask, the round, and the verdict a finished run is still showing. A
// pre-v53 blob has no tail → a zero seed, which reads as "no run", which is the truth
// for a device whose ladder had no arena on it.
constexpr uint16_t kSaveVersion = 53;

// The oldest blob deserialize will read. Raising it is how a device stops carrying
// migration weight for saves nobody can still be holding — and it is the ONLY thing
// that retires a rename row (see `renamedIds`). Raising it strands every save older
// than it, so it moves on a deliberate compatibility call, never as a side effect.
//
// Raised to 53 (the version v0.25.0 already wrote) once every live device had been
// confirmed on v0.25.0 or later — a device on that firmware or newer has autosaved at
// v53 at least once, so nothing still holds a save older than the floor. That retired
// both rows in `renamedIds` (sinceVersion 41 and 46) and the one in `ladderInserts`
// (sinceVersion 43), and `migrateRecipeRows` (v49) along with them — see save.cpp.
constexpr uint16_t kOldestAcceptedVersion = 53;

// Content ids a blob may still carry under a name the tables no longer answer to.
// A content id is a wire value, so a rename is a FORMAT concern and is handled here
// rather than as a permanent alias in the content tables — an alias there would read
// as a second legitimate name for a creature, and would never be safe to remove.
//
// `sinceVersion` is the first kSaveVersion whose blobs never write `from`, so a row
// applies only to a blob older than it.
//
// Two rules keep the table finite and mechanical:
//
//   RETIREMENT — a row may be deleted exactly when
//   `kOldestAcceptedVersion > sinceVersion`: at that point no blob the codec will
//   even open can still carry the old id. Nothing else justifies dropping one, and
//   a row that HAS become droppable must be dropped — the native gate fails on a
//   dead row, so raising the floor tells you which rows to delete.
//
//   FLATTENING — every `from` is distinct and no `to` is ever also a `from`, so one
//   substitution per id is always enough. Renaming something that is already a `to`
//   means editing that row's `to` in place (its `sinceVersion` stays: an ancient blob
//   still lands on the current name) AND adding a row for the newly-retired name.
struct RenamedId {
    const char* from;
    const char* to;
    uint16_t sinceVersion;
};
const RenamedId* renamedIds(int& count);

// An area SPLICED INTO THE MIDDLE of the EXPL ladder (kAreaList, areas/area_defs.h).
//
// Every persisted EXPL field is POSITIONAL — indexed by ladder rung, never by area id —
// so a blob written before the splice describes, from `atIndex` on, the area now one rung
// to its LEFT. Read verbatim it would hand the player the new area already cleared and
// silently take the deepest one back. That makes a mid-ladder insert a FORMAT concern, so
// like a renamed content id it is handled here rather than in the game layer: the codec
// opens a blank rung in each positional field and everything downstream reads a save that
// looks exactly like one written after the splice. Appending to the END of the ladder
// needs no row — nothing shifts, and the new rung defaults to uncleared on its own.
//
// The fields this has to move are every ladder-indexed one in SaveData: `sectorCleared`,
// `bossUnlocked`, `subCleared`, `subBossUnlocked`, `subRefarm` (row-major, one row per
// area), the `titlesUnlocked` bitmask, and the `equippedTitle` index.
//
// `atIndex` is the rung the area landed on, in the ladder AS IT WAS at that moment. Rows
// apply oldest-first, so each one reads against the ladder its predecessors already built
// and no row has to be restated when a later insert lands ahead of it.
//
// `sinceVersion` is the first kSaveVersion whose blobs already carry the slot, so a row
// applies only to a blob older than it — the same contract as `renamedIds`, and the same
// RETIREMENT rule: a row may be deleted exactly when `kOldestAcceptedVersion >
// sinceVersion`, at which point no blob the codec will open still predates the splice.
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

    // v17: permanent-mod semantics (D3) ----------------------
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

    // --- v24: move-slot rework — per-pet Attack/Defend slot typing (#12) -----------
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
    // SUPERSEDED as of v51 by `recipeOwned` below, which holds the whole set. This u32
    // stays on the wire at its v31 position carrying wires 0-31, because a v31..v50
    // reader is still entitled to the recipes it can name — an OTA that rolls back must
    // not cost a player the kitchen it understands.
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
