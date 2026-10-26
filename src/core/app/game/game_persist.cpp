#include "core/app/game.h"

#include <cstdio>
#include <cstring>

#include "tunables.h"
#include "core/content/content_backgrounds.h"
#include "core/content/content_tournament.h"
#include "core/render/palette.h"   // palThemeByName — the saved theme is resolved by name

namespace mal {

// Persistence ----------------------------------------

namespace {

SaveId saveId(const char* id) {
    SaveId s;
    if (id) std::strncpy(s.id, id, kSaveIdCap - 1);
    return s;
}

template <typename Def>
void captureIds(std::vector<SaveId>& out, const std::vector<const Def*>& defs) {
    out.reserve(defs.size());
    for (const Def* def : defs) out.push_back(saveId(def->id));
}

}  // namespace

// Every collection below is reserved to the exact count before it is filled. The
// count is always known from the live container, and a vector that grows into place
// instead allocates a whole doubling ladder, which costs nothing in total but leaves
// the free heap in pieces, and the size of the largest PIECE is what persistSave's
// guard reads. Reserving is what keeps building a save from being the thing that
// makes the save look unaffordable.
//
// One section per restore* below, in the same order, so each pair reads side by side.
// Every capture is a read, so unlike applySave the order carries no dependency.
SaveData Game::captureSave() const {
    SaveData d;
    captureOperator(d);
    captureBag(d);
    captureHistory(d);
    capturePetProgress(d);
    captureRadio(d);
    captureWorldProgress(d);
    captureRig(d);
    captureReveals(d);
    captureRecords(d);
    captureShelves(d);
    captureActivePet(d);
    captureSettings(d);
    captureBackgroundAndCant(d);
    return d;
}

void Game::captureOperator(SaveData& d) const {
    d.generation = generation_;
    d.petsRaised = petsRaised_;
    d.lifetimeUptimeMs = uptimeBase_ + nowMs_;
    d.lifetimeSteps = lifetimeSteps_;
    d.bits = bits_;
    std::strncpy(d.hackerTag, hackerTag_, kHackerTagMax);

    // v36: the crew rides as its content ID (content_crews.h), so the table can be
    // reordered.
    if (const CrewDef* c = activeCrew())
        std::strncpy(d.crewId, c->id, kSaveIdCap - 1);
    d.homeNetworkKey = homeNetworkKey_;
    std::strncpy(d.homeNetworkName, homeNetworkName_, kSaveNetNameCap - 1);

    // v4: Hacker Rank. Real-network history lives in the SD-backed networkLedger_.
    d.networksSeen = networksSeen_;
    d.hackerRank = hackerRank_;
}

void Game::captureBag(SaveData& d) const {
    d.items.reserve(inventory_.stacks().size());
    for (const auto& s : inventory_.stacks()) {
        SaveStack ss;
        std::strncpy(ss.id, s.id, kSaveIdCap - 1);
        ss.qty = s.qty;
        d.items.push_back(ss);
    }
    // v45: the spare pool as a count per mod WIRE. A mod the running content no longer
    // defines has no wire to write to and is dropped here rather than at load. The pool
    // in RAM was built from the registry, so this can only miss a row retired underneath
    // a live game, which is a developer state, not a player one.
    for (const OwnedMod& m : loadout_.owned())
        if (const ModDef* def = registry_.mod(m.id))
            saveSetModCount(d.ownedModCounts, def->wire, m.count);
    d.equipped.reserve(kModSlots);
    for (int i = 0; i < kModSlots; ++i) d.equipped.push_back(saveId(loadout_.equipped(i)));
}

void Game::captureHistory(SaveData& d) const {
    // Log, chronological oldest-first (at(0) is newest), so a restore replays it.
    d.log.reserve(static_cast<size_t>(log_.size()));
    for (int i = log_.size() - 1; i >= 0; --i) {
        const LogEntry& e = log_.at(i);
        SaveLogEntry se;
        se.type = static_cast<uint8_t>(e.type);
        std::strncpy(se.text, e.text, kSaveTextCap - 1);
        d.log.push_back(se);
    }
    d.rack = rack_;
    d.records = records_;                             // v3: ARCH RETIRED/CORRUPTED records
}

// The rack pets' own copies of the per-pet fields here ride on d.rack, stamped by
// game_arch.cpp's freezePet.
void Game::capturePetProgress(SaveData& d) const {
    // v2: the move loadout (owned + per-slot equipped) + combat XP/level. Health is
    // transient (resets each fight) and is deliberately NOT captured.
    d.ownedMoves.reserve(moveLoadout_.owned().size());
    for (const char* m : moveLoadout_.owned()) d.ownedMoves.push_back(saveId(m));
    d.equippedMoves.reserve(kMaxMoveSlots);
    for (int i = 0; i < kMaxMoveSlots; ++i)
        d.equippedMoves.push_back(saveId(moveLoadout_.equipped(i)));
    d.combatXp = combatXp_;
    d.combatLevel = combatLevel_;
    // v11: the creature-level stat points. Sum == combatLevel (the invariant).
    d.statPoints.assign(statPoints_, statPoints_ + kLevelStatCount);

    // v24: the per-pet Attack/Defend slot typing.
    d.slotKinds.reserve(kMaxMoveSlots);
    for (int i = 0; i < kMaxMoveSlots; ++i)
        d.slotKinds.push_back(static_cast<uint8_t>(slotKinds_[i]));

    d.defragCount = defragCount_;                     // v16
    d.bestDeepWebDepth = bestDeepWebDepth_;           // v35
    d.bestDarkWebDepth = bestDarkWebDepth_;

    // v42: the 5/5 recovery window already burned. The one timed window that persists:
    // every other one (Lockout, the audit hot/cooldown) resets on reboot by the rule in
    // core/net/audit_capture.h, but refunding THIS one makes the only death path
    // optional.
    d.dyingElapsedMs = dyingElapsedMs_;

    // v9: the Boot-Sector incubation clock so an unhatched egg resumes after a
    // reboot instead of restarting (or hatching early).
    d.bootHatchRemainMs = bootHatchRemainMs_;

    // v21: the care-mistake shield + once-per-lifetime item gates (reset on a new egg).
    d.mistakeShieldActive = mistakeShieldActive_ ? 1 : 0;
    d.shieldItemConsumed = shieldItemConsumed_ ? 1 : 0;
    d.yubiConsumed = yubiConsumed_ ? 1 : 0;

    // v28 and v60: the USB port. The Ambig-USB Trojan divert, the Bad-USB/Signed-USB
    // branch override, the Sandbox/Hypervisor-USB soak factor and the Halt-USB's hold,
    // all reset on a new egg.
    d.forceTrojanDivert = forceTrojanDivert_ ? 1 : 0;
    d.evolveBranchOverride = static_cast<uint8_t>(evolveBranchOverride_);
    d.evolveSoakFactor = static_cast<uint8_t>(evolveSoakFactor_);
    d.evolveHold = evolveHold_ ? 1 : 0;

    // v30: the Backup Drive combat-shield buff's armed-until deadline.
    d.backupShieldUntilMs = backupShieldUntilMs_;

    // v50/v57: this pet's permanent Epic-dish upgrades (core/model/pet_upgrades.h), and
    // (v72) the favourite dish.
    d.bandwidthRegenBonusMin = upgrades_.bandwidthRegenMin;
    for (int i = 0; i < kLevelStatCount; ++i) d.statBonus[i] = upgrades_.statBonus[i];
    d.xpRateBonusPct = upgrades_.xpRatePct;
    if (upgrades_.favouriteFood)
        std::strncpy(d.favouriteFood, upgrades_.favouriteFood->id, kSaveIdCap - 1);
    d.favouriteFound = upgrades_.favouriteFound ? 1 : 0;
}

// The radio toggles persist as intent only: hot/cooldown timers are runtime-only (no RTC).
void Game::captureRadio(SaveData& d) const {
    d.netScanEnabled = netScanEnabled_ ? 1 : 0;       // v5
    d.apEnabled = apEnabled_ ? 1 : 0;                 // v20
    // v37: only the LINK consent bit. The met operators live in the SD-backed
    // PeerLedger (core/net/peer_ledger.h).
    d.linkEnabled = linkEnabled_;
    d.auditCaptureEnabled = auditCapture_.enabled() ? 1 : 0;   // v6

    // v7: the real-radio SHAKES dedup ledger + the lifetime handshake count, so
    // unique-credit survives a reboot (never re-count on return).
    d.seenHandshakeBssids = seenHandshakeBssids_;
    d.handshakesSeen = handshakesSeen_;
}

void Game::captureWorldProgress(SaveData& d) const {
    // v8: the EXPL AREA-clear flags, one byte per area so the linear
    // complete-to-advance ladder survives a reboot.
    d.sectorCleared.assign(kAreaCount, 0);
    for (int i = 0; i < kAreaCount; ++i)
        d.sectorCleared[i] = sectorCleared_[i] ? 1 : 0;

    // v12: the legacy per-sector boss-unlock byte. Superseded at runtime by the v13
    // per-sub flags, but still written (the stream is version-layered), derived as
    // "any sub-area boss unlocked in this area" so it stays self-consistent.
    d.bossUnlocked.assign(kAreaCount, 0);
    // v13: the EXPL per-sub-area clear + boss-unlock bitmasks, one byte per
    // area, bit s = sub-area s.
    d.subCleared.assign(kAreaCount, 0);
    d.subBossUnlocked.assign(kAreaCount, 0);
    for (int a = 0; a < kAreaCount; ++a)
        for (int s = 0; s < kSubAreasPerArea; ++s) {
            if (subCleared_[a][s]) d.subCleared[a] |= static_cast<uint8_t>(1u << s);
            if (subBossUnlocked_[a][s]) {
                d.subBossUnlocked[a] |= static_cast<uint8_t>(1u << s);
                d.bossUnlocked[a] = 1;              // legacy v12: any sub unlocked
            }
        }

    // v10: the zone-completion Titles, the unlocked bitmask + the equipped index.
    d.titlesUnlocked = titlesUnlocked_;
    d.equippedTitle = equippedTitle_;

    // v15: the EXPL per-sub-area re-farm win counts, flat row-major
    // (area*kSubAreasPerArea + sub), so a depleted area stays depleted for future pets.
    d.subRefarm.assign(static_cast<size_t>(kAreaCount) * kSubAreasPerArea, 0);
    for (int a = 0; a < kAreaCount; ++a)
        for (int s = 0; s < kSubAreasPerArea; ++s)
            d.subRefarm[a * kSubAreasPerArea + s] = subRefarmCount_[a][s];
}

// Rows below kRigRowExtBase map positionally onto legacy per-row save fields;
// rigLevel_ is the in-memory source of truth (game_rig_shop.h). Bandwidth itself isn't
// saved, only the purchased cap.
void Game::captureRig(SaveData& d) const {
    d.bwUpgradeCount = rigLevel_[kRigRowBandwidth];                          // v19
    d.fragAmountTier = static_cast<uint8_t>(rigLevel_[kRigRowFragReduce]);   // v21
    d.fragTriggerTier = static_cast<uint8_t>(rigLevel_[kRigRowFragTrigger]); // v22
    d.itemTabsUnlocked = rigLevel_[kRigRowItemTabs] ? 1 : 0;                 // v23
    d.bulkOpenUnlocked = rigLevel_[kRigRowBulkOpen] ? 1 : 0;
    d.rackSlotUpgradeCount = rigLevel_[kRigRowRackSlot];                     // v27
    d.scrapingClusterLevel = rigLevel_[kRigRowScraping];
    d.dataMiningLevel = rigLevel_[kRigRowDataMining];

    // v31: the Merge Hub unlock, and (v49) the whole owned-recipe mask. Recipes own
    // their state outright rather than mirroring rig rows, so the mask is written
    // straight through.
    d.mergeHubUnlocked = rigLevel_[kRigRowMergeHub] ? 1 : 0;
    // The whole set goes out as the v51 bitset; its first four bytes ALSO go out as the
    // v31 u32, which is all a reader that predates the bitset can hold.
    d.recipeOwned.assign(recipesOwned_, recipesOwned_ + kMergeRecipeWireBytes);
    d.recipesUnlocked = 0;
    for (int i = 0; i < 4; ++i)
        d.recipesUnlocked |= static_cast<uint32_t>(recipesOwned_[i]) << (i * 8);

    // v32: every rig row from kRigRowExtBase up, a forward-compatible tail so a new
    // Rig Shop row never needs a save.h edit. Mirrors the subRefarm vector pattern:
    // length-prefixed, bounds-checked on read, short/missing defaults every level to 0.
    d.rigLevelsExt.reserve(kRigUpgradeCount - kRigRowExtBase);
    for (int i = kRigRowExtBase; i < kRigUpgradeCount; ++i)
        d.rigLevelsExt.push_back(static_cast<uint16_t>(rigLevel_[i]));

    // v61: which owned services are stopped. Its own field rather than a level, because
    // a switch is not a purchase: the level says what was bought, this says what runs.
    d.rigServicesOff = rigServicesOff_;
    d.rigServicesAsk = rigServicesAsk_;   // v69
}

void Game::captureReveals(SaveData& d) const {
    // v25: web-'Pedia reveal state, glimpsed-but-not-hatched creature ids and the
    // malbeast seen/defeated masks.
    captureIds(d.seenCreatures, seenCreatures_);
    d.malbeastSeen = malbeastSeenMask_;
    d.malbeastDefeated = malbeastDefeatedMask_;

    // v40: the achievement bitsets (by wire number). They go out at their full
    // in-memory width, so a catalogue that grows into spare capacity needs no save
    // change at all.
    d.achievementEarned.assign(achEarned_, achEarned_ + kAchBytes);
    d.achievementNotified.assign(achNotified_, achNotified_ + kAchBytes);
    // v65: the STORY read-set, the same shape and for the same reason: a bitset over
    // wire numbers, written at its full in-memory width.
    d.storyRead.assign(storyRead_, storyRead_ + sizeof(storyRead_));
    d.tipsSeen.assign(tipsSeen_, tipsSeen_ + sizeof(tipsSeen_));   // v66, same shape
}

void Game::captureRecords(SaveData& d) const {
    // v40 onward: the counters behind the countable achievement rows.
    d.bossWins = bossWins_;
    d.stackerWins = stackerWins_;   // v44
    d.tourneyWins = tourneyWins_;   // v56
    d.pvpWins = pvpWins_;           // v56
    d.mergesCooked = mergesCooked_; // v56
    // v53: ROCK THE DOCK's run in play. Four bytes plus a bitmask, because every entrant is
    // derived from the seed (core/model/tournament.h) rather than written down.
    d.tourneySeed = tourneySeed_;
    d.tourneyAlive = tourneyAlive_;
    d.tourneyRound = tourneyRound_;
    d.tourneyPhase = static_cast<uint8_t>(tourneyPhase_);
    // v47: one row per cabinet that has actually been played, id-keyed. A cabinet with
    // no runs writes nothing: the save carries history, not the roster.
    for (int i = 0; i < arcadeGameCount() && i < kArcadeMaxCabinets; ++i) {
        if (arcadePlays_[i] == 0) continue;
        d.arcadeIds.push_back(saveId(arcadeGames()[i].id));
        d.arcadePlays.push_back(arcadePlays_[i]);
        d.arcadeWins.push_back(arcadeWins_[i]);
        d.arcadeBest.push_back(arcadeBest_[i]);
    }
    // v48: the DECRYPTOGRAM per-quote states, at their full in-memory width, so a pool
    // that grows into spare capacity needs no save change at all.
    d.quoteStates.assign(quoteStates_, quoteStates_ + kQuoteStateBytes);
}

void Game::captureShelves(SaveData& d) const {
    captureIds(d.collectedItems, collectedItems_);
    captureIds(d.petFoodsEaten, petFoodsEaten_);
    d.speciesDiveIds.reserve(speciesDives_.size());
    d.speciesDiveDepths.reserve(speciesDives_.size());
    for (const SpeciesDive& s : speciesDives_) {
        if (!s.def) continue;
        d.speciesDiveIds.push_back(saveId(s.def->id));
        d.speciesDiveDepths.push_back(s.depth);
    }
    captureIds(d.raisedCreatures, raisedCreatures_);   // v39
}

void Game::captureActivePet(SaveData& d) const {
    if (pet_) std::strncpy(d.activeId, pet_->id, kSaveIdCap - 1);
    d.hunger = model_.hunger();
    d.frag = model_.fragmentation();
    d.happy = model_.happiness();
    d.mistakes = model_.careMistakes();
    d.debuffs = model_.debuffs();
    d.ghost = model_.hasGhost() ? 1 : 0;
    d.timeInStageMs = nowMs_ - stageEnteredMs_;   // elapsed in stage (reboot-safe)
}

// Device-level CFG preferences: what the device looks and sounds like, which no pet owns.
void Game::captureSettings(SaveData& d) const {
    d.brightness = brightness_;                       // v14
    d.uiMode = static_cast<uint8_t>(uiMode_);         // v67
    d.careAlerts = careAlerts_ ? 1 : 0;
    d.soundMode = static_cast<uint8_t>(soundMode_);   // v68
    d.volume = static_cast<uint8_t>(volume_);
    d.exploreSound = exploreSound_ ? 1 : 0;           // v71
    // v64: the chosen PAL_CORE theme, stored by NAME (see save.h's version note).
    std::snprintf(d.theme, sizeof(d.theme), "%s", themeName());
}

void Game::captureBackgroundAndCant(SaveData& d) const {
    // v58: the chosen background, by wire. AUTO writes 0, which is also what a pick
    // this build somehow has no row for writes, since AUTO is the safe reading.
    if (const BackgroundDef* b = backgroundFor(backgroundPick_)) d.backgroundPick = b->wire;
    // v59: the CANT. The learned mask and what was paid for it; how many shakes are
    // still SPENDABLE is derived from this and the v7 lifetime count, never stored.
    d.cantSigils = cantSigils_;
    d.shakesSpent = shakesSpent_;
}

// Player-level identity and standing. A Store-vacated save (no active pet) still carries
// all of it.
void Game::restoreOperator(const SaveData& d) {
    generation_ = d.generation;
    petsRaised_ = d.petsRaised;
    uptimeBase_ = d.lifetimeUptimeMs;
    lifetimeSteps_ = d.lifetimeSteps;
    bits_ = d.bits;
    std::strncpy(hackerTag_, d.hackerTag, sizeof(hackerTag_) - 1);
    hackerTag_[sizeof(hackerTag_) - 1] = '\0';

    // Crew + home network (v36). The crew resolves from its stored ID, so a save
    // written against a crew that no longer exists loads as unaffiliated rather than
    // pointing at whatever now occupies that row. A pre-v36 blob carries neither.
    crewIndex_ = crewIndexById(d.crewId);
    homeNetworkKey_ = d.homeNetworkKey;
    std::strncpy(homeNetworkName_, d.homeNetworkName, sizeof(homeNetworkName_) - 1);
    homeNetworkName_[sizeof(homeNetworkName_) - 1] = '\0';
    // Membership can't outlive the network it defends — the same invariant
    // setHomeNetwork enforces, re-applied here so no blob can load into a crew
    // without a home network.
    if (homeNetworkKey_ == 0) crewIndex_ = -1;

    // Hacker Rank (v4): the counter restores verbatim; rank is stored rather than
    // re-derived so a future threshold retune doesn't retroactively demote a save.
    // Real-network dedup/history lives in networkLedger_ now (loaded separately
    // from SD, Game::loadNetworkLedger), not this save blob.
    networksSeen_ = d.networksSeen;
    hackerRank_ = d.hackerRank;
}

void Game::restoreBag(const SaveData& d) {
    // Inventory: resolve each saved id to the registry's stable pointer (the
    // borrowed-id contract); drop ids no longer present in content.
    inventory_ = Inventory{};
    for (const auto& s : d.items) {
        const ItemDef* def = registry_.item(s.id);
        if (def && s.qty > 0) inventory_.add(def->id, s.qty);
    }
    // Loadout (D3 permanent mods): grant the un-equipped spares, then install the
    // slot occupants WITHOUT consuming (they were consumed when first equipped, and a
    // v17+ save stores spares and slots disjointly). A pre-v17 blob predates the split
    // — its ownedMods still listed the equipped mods, so drop any owned id that also
    // sits in a slot, else the migrated save would gift a duplicate spare of each.
    loadout_ = Loadout{};
    if (d.hasModCountData) {
        // v45: a count per wire. The stored count was already capped when it was written,
        // so it is its own ceiling here — passing modStorageCap() instead would silently
        // confiscate copies from a player who had bought MOD STORAGE and then, somehow,
        // lost the purchase. A wire the running content no longer defines resolves to
        // nullptr and its copies are dropped, which is what retiring a mod should do.
        for (int wire = 0; wire < kModWireCap; ++wire) {
            const int n = saveModCount(d.ownedModCounts, wire);
            if (n <= 0) continue;
            const ModDef* def = registry_.modByWire(wire);
            if (!def) continue;
            for (int i = 0; i < n; ++i) loadout_.grant(def->id, n);
        }
    } else {
        // v44 and older: a flat list, one entry per COPY, with a rolled equip level
        // alongside that no longer means anything. Tally per id and keep up to the BASE
        // cap — not modStorageCap(), because the MOD STORAGE row ships with this version
        // and no migrating save can have bought it, so the base is both correct and
        // independent of whether rigLevel_ has been restored yet.
        for (const auto& m : d.ownedMods) {
            const ModDef* def = registry_.mod(m.id);
            if (!def) continue;
            if (!d.hasPermanentModData) {
                bool inSlot = false;
                for (const auto& e : d.equipped)
                    if (std::strcmp(e.id, m.id) == 0) { inSlot = true; break; }
                if (inSlot) continue;             // migrate: equipped mod isn't a spare
            }
            loadout_.grant(def->id, kModCopyCapBase);   // surplus copies simply don't fit
        }
    }
    for (int i = 0; i < static_cast<int>(d.equipped.size()) && i < kModSlots; ++i)
        if (const ModDef* def = registry_.mod(d.equipped[i].id))
            loadout_.setEquipped(i, def->id);
}

void Game::restoreHistory(const SaveData& d) {
    // Event log: replay oldest-first into a fresh ring.
    log_ = EventLog{};
    for (const auto& e : d.log)
        log_.push(static_cast<LogEventType>(e.type), e.text);
    // Rack: only entries whose creature still exists.
    rack_.clear();
    for (const auto& p : d.rack)
        if (registry_.creature(p.id)) rack_.push_back(p);
    // ARCH records (v3): historical RETIRED/CORRUPTED entries. Kept even if the
    // creature id is gone from content (a record is a name + status, not a live
    // pet); the UI resolves an unknown id to "?".
    records_ = d.records;
}

// The active pet's own state: moves, level, slot typing, tallies, flags and upgrades.
void Game::restorePetProgress(const SaveData& d) {
    // Move loadout (v2). A v1 blob carried no move data (hasMoveData == false), so
    // the pet is seeded with its line's starting kit — a graceful forward-compat
    // migration rather than stripping it to only the innate default move.
    if (d.hasMoveData) {
        moveLoadout_ = MoveLoadout{};
        // The innate is part of what every pet owns, and a blob written before that was
        // true simply doesn't list it. Granted first, unconditionally — nothing about
        // the format changes, a pet restored from any blob just has what it has always
        // been able to swing.
        moveLoadout_.grant(moveLoadout_.defaultMove());
        for (const auto& m : d.ownedMoves)
            if (const MoveDef* def = registry_.move(m.id)) moveLoadout_.grant(def->id);
        for (int i = 0; i < static_cast<int>(d.equippedMoves.size()) && i < kMaxMoveSlots; ++i)
            if (const MoveDef* def = registry_.move(d.equippedMoves[i].id))
                moveLoadout_.equip(i, def->id);
    } else {
        const CreatureDef* activeDef = registry_.creature(d.activeId);
        moveLoadout_ = MoveLoadout::startingForLine(registry_, activeDef ? activeDef->line : nullptr);
    }
    // Creature levels (v11). A v11+ blob restores the 0-based level + XP bucket
    // + earned stat points verbatim. A pre-v11 blob carried only the vestigial 1-based
    // combatLevel (never applied to combat) — migrate it to a fresh level 0 rather than
    // reinterpret it, so no phantom points appear (hasLevelData distinguishes the two).
    if (d.hasLevelData) {
        combatXp_ = d.combatXp;
        combatLevel_ = d.combatLevel;
        for (int i = 0; i < kLevelStatCount; ++i)
            statPoints_[i] = i < static_cast<int>(d.statPoints.size()) ? d.statPoints[i] : 0;
    } else {
        combatXp_ = 0;
        combatLevel_ = 0;
        for (int i = 0; i < kLevelStatCount; ++i) statPoints_[i] = 0;
    }

    // Per-pet Attack/Defend slot typing (v24). A v24+ blob
    // restores the stamped SlotKind verbatim; a pre-v24 blob leaves every slot
    // Unset (hasSlotKindData false), and the block below (after the active pet is
    // resolved) deterministically re-derives it via stampSlotKinds().
    for (int i = 0; i < kMaxMoveSlots; ++i)
        slotKinds_[i] = (d.hasSlotKindData && i < static_cast<int>(d.slotKinds.size()))
                            ? static_cast<SlotKind>(d.slotKinds[i])
                            : SlotKind::Unset;

    // Active pet's defrag tally (v16). A pre-v16 blob carries 0 (the pet
    // reads as never-defragged); the rack pets' tallies rode in on rack_ above.
    defragCount_ = d.defragCount;

    // Active pet's best-ever DeepWeb Dive depth (v35). A pre-v35 blob carries 0
    // (never dived); the rack pets' values rode in on rack_ above.
    bestDeepWebDepth_ = d.bestDeepWebDepth;
    bestDarkWebDepth_ = d.bestDarkWebDepth;

    // Active pet's spent 5/5 recovery window (v42). A pre-v42 blob carries 0, so the
    // pet gets a full window on its next brush with death; the rack pets' values rode
    // in on rack_ above. dyingArmed_ stays false — the accumulator is the durable half,
    // and the first tick at 5/5 re-anchors it against the current clock.
    dyingElapsedMs_ = d.dyingElapsedMs;

    // Boot-Sector incubation clock (v9). A pre-v9 blob carries 0 → the loaded pet
    // is treated as fully hatched (inEggPhase() is false for a 0 clock), so a
    // migrated save is never re-frozen as an egg.
    bootHatchRemainMs_ = d.bootHatchRemainMs;

    // v21: the per-pet care-mistake shield + once-per-lifetime item gates (reset on a
    // new egg; a pre-v21 blob defaults them to 0: no shield, neither item consumed).
    mistakeShieldActive_ = d.mistakeShieldActive != 0;
    shieldItemConsumed_ = d.shieldItemConsumed != 0;
    yubiConsumed_ = d.yubiConsumed != 0;

    // v28: the Ambig-USB armed Trojan-divert flag (per-pet; a pre-v28 blob defaults
    // it to false — no divert armed).
    forceTrojanDivert_ = d.forceTrojanDivert != 0;

    // v60: the branch override and the soak (per-pet; a pre-v60 blob defaults to an empty
    // port). Both are clamped rather than trusted: an unknown override number reads as no
    // override, and the factor is floored at 1 the way the codec floors it, so nothing a
    // blob can say makes the pet un-evolvable or its XP worthless.
    evolveBranchOverride_ = d.evolveBranchOverride == 1   ? BranchOverride::Good
                            : d.evolveBranchOverride == 2 ? BranchOverride::Bad
                                                          : BranchOverride::None;
    evolveSoakFactor_ = d.evolveSoakFactor < 1 ? 1 : d.evolveSoakFactor;
    evolveHold_ = d.evolveHold != 0;

    // v30: the Backup Drive combat-shield buff's armed-until deadline (per-pet; a
    // pre-v30 blob defaults it to 0 — no shield armed).
    backupShieldUntilMs_ = d.backupShieldUntilMs;

    // v50/v57: the active pet's permanent Epic-dish upgrades (an older blob → 0: no dish
    // granted one). The rack's own values come back with the stored records themselves.
    upgrades_.bandwidthRegenMin = d.bandwidthRegenBonusMin;
    for (int i = 0; i < kLevelStatCount; ++i) upgrades_.statBonus[i] = d.statBonus[i];
    upgrades_.xpRatePct = d.xpRateBonusPct;
    // v72: the favourite. An id this build has no item for leaves it unrolled, and the
    // next meal rolls a fresh one; a found flag stands either way, being a grant.
    upgrades_.favouriteFood = registry_.item(d.favouriteFood);
    upgrades_.favouriteFound = d.favouriteFound != 0;
}

void Game::restoreRadio(const SaveData& d) {
    // Audit-mode passive-scan toggle (v5). A pre-v5 blob defaults it OFF (never
    // silently arm the radio on a migrated save).
    netScanEnabled_ = d.netScanEnabled != 0;

    // 'Pedia local-AP toggle (v20). A pre-v20 blob defaults it OFF (never silently
    // stand up an AP on a migrated save).
    apEnabled_ = d.apEnabled != 0;

    // Pet-to-pet LINK toggle (v37). A pre-v37 blob defaults it OFF — a migrated
    // device must never start broadcasting its operator's identity because it was
    // upgraded. Unlike capture/scan there is no invariant to normalize: LINK is
    // independent of the audit ladder, since listening and announcing are separate
    // consents.
    linkEnabled_ = d.linkEnabled;

    // The v38 slot is deliberately not read back: it held a standing internet opt-in,
    // and this device now goes online only for the duration of a job somebody started
    // (see Game::netConnectWanted). A save carrying the old bit lands off the 'net.

    // Audit-capture toggle (v6). A pre-v6 blob defaults it OFF. Rebuild the policy
    // SM from the persisted intent — the hot/cooldown clock starts fresh (no RTC,
    // like Lockout): a reboot clears any in-flight PENALTY window. The 5/5 CSF
    // recovery window is the one exception; see dyingElapsedMs_ above.
    auditCapture_ = AuditCapture{};
    auditCapture_.setEnabled(d.auditCaptureEnabled != 0, nowMs_);

    // Normalize the escalating-audit invariant on load: a legacy save from before
    // the single AuditMode control could hold capture-on with scan-off (the two
    // toggles were independent). Capture depends on the scan, so pull scan up to
    // match rather than run capture without its discovery feed.
    if (auditCapture_.enabled() && !netScanEnabled_) netScanEnabled_ = true;

    // Audit SHAKES dedup ledger (v7). Restore the seen-handshake-BSSID set + the
    // lifetime handshake count so a reboot never re-credits a handshake already
    // counted. A pre-v7 blob carries an empty ledger (dedup rebuilds from scratch
    // that session). (The sibling NETS/seenBssids set this used to also restore
    // was removed as of v33 — see save.h.)
    seenHandshakeBssids_ = d.seenHandshakeBssids;
    handshakesSeen_ = d.handshakesSeen;
}

// Areas, sub-areas, re-farm counts and the zone Titles they award.
void Game::restoreWorldProgress(const SaveData& d) {
    // Area-clear flags (v8). A pre-v8 blob carries an empty vector → nothing cleared
    // (only area 0 open — the correct default for a save that predates linear gating).
    // Extra saved entries (a future area shrink) are ignored.
    for (int i = 0; i < kAreaCount; ++i)
        sectorCleared_[i] = i < static_cast<int>(d.sectorCleared.size()) &&
                            d.sectorCleared[i] != 0;

    // Per-sub-area clear + boss-unlock flags (v13). A v13+ blob restores the
    // per-area bitmasks verbatim. A pre-v13 blob carries empty vectors → MIGRATE: a
    // cleared AREA had every sub-area beaten, so mark all 5 sub-areas cleared + their
    // bosses unlocked; an uncleared area's sub-areas all default false (the honest
    // default — the legacy per-sector bossUnlocked byte doesn't map to a specific
    // sub-area, so an in-flight unlock is dropped rather than mis-attributed).
    const bool haveSub = !d.subCleared.empty() || !d.subBossUnlocked.empty();
    for (int a = 0; a < kAreaCount; ++a) {
        const uint8_t cm = a < static_cast<int>(d.subCleared.size())
                               ? d.subCleared[a] : 0;
        const uint8_t bm = a < static_cast<int>(d.subBossUnlocked.size())
                               ? d.subBossUnlocked[a] : 0;
        for (int s = 0; s < kSubAreasPerArea; ++s) {
            if (haveSub) {
                subCleared_[a][s] = (cm & (1u << s)) != 0;
                subBossUnlocked_[a][s] = (bm & (1u << s)) != 0;
            } else {
                subCleared_[a][s] = sectorCleared_[a];       // cleared area → all sub cleared
                subBossUnlocked_[a][s] = sectorCleared_[a];  // ...and all bosses unlocked
            }
        }
    }

    // Zone-completion Titles (v10). A pre-v10 blob carries 0 unlocked / -1 equipped
    // (nothing earned — the correct default for a save that predates Titles). Mask
    // off bits above the live sector count, and drop a stale equip whose Title isn't
    // actually unlocked (a future sector shrink), so equippedTitleName() stays honest.
    titlesUnlocked_ = d.titlesUnlocked & ((1u << kAreaCount) - 1u);
    equippedTitle_ = d.equippedTitle;
    if (!titleUnlocked(equippedTitle_)) equippedTitle_ = -1;

    // Per-sub-area re-farm counts (v15). A v15+ blob restores the flat
    // row-major list; a pre-v15 blob carries an empty vector → every count defaults to
    // 0 (a migrated save's cleared areas start un-depleted, full drops until farmed).
    for (int a = 0; a < kAreaCount; ++a)
        for (int s = 0; s < kSubAreasPerArea; ++s) {
            const size_t idx = static_cast<size_t>(a) * kSubAreasPerArea + s;
            subRefarmCount_[a][s] = idx < d.subRefarm.size() ? d.subRefarm[idx] : 0;
        }
}

// Every Rig Shop row, the Merge Hub's recipes, and the service switches.
void Game::restoreRig(const SaveData& d) {
    // Rig Shop account upgrades (v19). A pre-v19 blob carries 0 (bought none).
    rigLevel_[kRigRowBandwidth] = d.bwUpgradeCount < 0 ? 0 : d.bwUpgradeCount;
    // v21-v23: the frag tiers and the two one-time unlocks (older blobs default to 0).
    rigLevel_[kRigRowFragReduce] = d.fragAmountTier;
    // v22: the c Reduce-Explore-Frag-TRIGGER tier (a pre-v22 blob defaults it to 0).
    rigLevel_[kRigRowFragTrigger] = d.fragTriggerTier;
    // v23: the d/e one-time Rig Shop unlocks (a pre-v23 blob defaults both false).
    rigLevel_[kRigRowItemTabs] = d.itemTabsUnlocked ? 1 : 0;
    rigLevel_[kRigRowBulkOpen] = d.bulkOpenUnlocked ? 1 : 0;

    // v27: the Rig Shop rig-upgrade levels. A pre-v27 blob defaults all 0
    // (a migrated save has bought none).
    rigLevel_[kRigRowRackSlot] = d.rackSlotUpgradeCount < 0 ? 0 : d.rackSlotUpgradeCount;
    rigLevel_[kRigRowScraping] = d.scrapingClusterLevel < 0 ? 0 : d.scrapingClusterLevel;
    rigLevel_[kRigRowDataMining] = d.dataMiningLevel < 0 ? 0 : d.dataMiningLevel;

    // v31: the Merge Hub unlock and the owned-recipe mask (a pre-v31 blob defaults both
    // to 0 — no Hub, no recipes). The mask arrives already in v49's layout: a pre-v49
    // blob had two of its recipes riding rig rows instead, and save.cpp's reader folds
    // those into the bits before handing the struct over.
    rigLevel_[kRigRowMergeHub] = d.mergeHubUnlocked ? 1 : 0;
    // The bitset is authoritative — save.cpp has already seeded it from the u32 for any
    // blob that predates v51, so this reads one field however old the save is. A set
    // longer than this build understands is truncated, which is what "a shorter view"
    // means: those recipes exist in the blob and come back on the build that has them.
    for (uint8_t& b : recipesOwned_) b = 0;
    for (size_t i = 0; i < d.recipeOwned.size() && i < kMergeRecipeWireBytes; ++i)
        recipesOwned_[i] = d.recipeOwned[i];

    // v32: every rig row from kRigRowExtBase up (a pre-v32 blob carries an empty vector
    // → every such row defaults to 0 — a migrated save has bought none of them).
    for (int i = kRigRowExtBase; i < kRigUpgradeCount; ++i) {
        const size_t idx = static_cast<size_t>(i - kRigRowExtBase);
        rigLevel_[i] = idx < d.rigLevelsExt.size() ? d.rigLevelsExt[idx] : 0;
    }

    // v61: the stopped-service mask (pre-v61 → 0, everything the save bought running).
    rigServicesOff_ = d.rigServicesOff;
    rigServicesAsk_ = d.rigServicesAsk;   // v69: pre-v69 -> 0, every askable row at YES

    // Bandwidth isn't persisted (it refills on reboot), so re-seed the live pool to the
    // restored cap and a loaded save boots with a full farm pool.
    bandwidth_ = bandwidthMax();
}

// What the 'Pedia, the achievements, the story and the tips have revealed.
void Game::restoreReveals(const SaveData& d) {
    // web-'Pedia reveal state (v25). seenCreatures resolves each saved id to the
    // registry's stable pointer, dropping unknown ids (mirrors rack_/records_); the
    // masks restore verbatim.
    //
    // The dedupe is not belt-and-braces: a blob can legitimately carry two ids that are
    // now ONE creature, because a rename row may retire several ids onto the same
    // successor (save.h's `renamedIds` — the Worm line's two Script placeholders both
    // land on Rootgrub). These lists are tallies, and AchSeries::LineRaised COUNTS the
    // raised one per line, so a duplicate does not merely waste a slot — it awards a
    // line achievement for a creature the operator raised once. The mark* helpers are
    // idempotent for the same reason; this path bypasses them, so it has to say so.
    seenCreatures_.clear();
    for (const auto& s : d.seenCreatures)
        if (const CreatureDef* c = registry_.creature(s.id))
            if (!creatureSeen(c->id)) seenCreatures_.push_back(c);
    malbeastSeenMask_ = d.malbeastSeen;
    malbeastDefeatedMask_ = d.malbeastDefeated;

    // v40: the achievement bitsets. A blob may be shorter than this build's capacity
    // (an older save) or longer (a save written by a build with a bigger catalogue) —
    // copy what overlaps and leave the rest zero either way, which is what makes the
    // length prefix worth having.
    for (uint8_t& b : achEarned_) b = 0;
    for (uint8_t& b : achNotified_) b = 0;
    for (size_t i = 0; i < d.achievementEarned.size() && i < kAchBytes; ++i)
        achEarned_[i] = d.achievementEarned[i];
    for (size_t i = 0; i < d.achievementNotified.size() && i < kAchBytes; ++i)
        achNotified_[i] = d.achievementNotified[i];
    // v65: the STORY read-set, loaded the same way and clamped the same way — a blob
    // from a build with more chapters than this one keeps the bits this build can hold
    // and drops the rest, which is exactly "the chapters it knows about".
    for (uint8_t& b : storyRead_) b = 0;
    for (size_t i = 0; i < d.storyRead.size() && i < sizeof(storyRead_); ++i)
        storyRead_[i] = d.storyRead[i];
    // v66: the onboarding tips, the same way. A v66 blob always writes the set at its
    // full width, so an EMPTY one is a pre-v66 save: a device already in use, whose
    // operator has opened the menu, played the Ransomware hatch every device starts on,
    // cared for a pet and walked it. Those are taken as seen; a line it has never laid
    // still gets its briefing.
    for (uint8_t& b : tipsSeen_) b = 0;
    for (size_t i = 0; i < d.tipsSeen.size() && i < sizeof(tipsSeen_); ++i)
        tipsSeen_[i] = d.tipsSeen[i];
    if (d.tipsSeen.empty())
        for (Tip t : {Tip::MenuOpened, Tip::HatchDecrypt, Tip::CareFed, Tip::CareFrag,
                      Tip::CareHappy, Tip::FirstWalk, Tip::FirstError,
                      Tip::NearTheLine, Tip::FirstCache}) {
            const int w = static_cast<int>(t);
            tipsSeen_[w / 8] |= static_cast<uint8_t>(1u << (w % 8));
        }
    if (d.achievementEarned.empty() && d.achievementsMask != 0) {
        // Pre-v40: the legacy u32 mask, whose bit i IS wire number i (the original 14
        // rows kept their enum order as their wire numbers, precisely so this is a copy
        // and not a mapping table). `notified` stays clear on purpose — an upgraded
        // device parades everything it has ever earned, which is also how the rows this
        // version retro-awards on the first sweep get announced.
        for (int i = 0; i < 4 && i < kAchBytes; ++i)
            achEarned_[i] = static_cast<uint8_t>((d.achievementsMask >> (8 * i)) & 0xFF);
    }
    achBannerWire_ = -1;
    achBannerCount_ = 0;
}

// The tallies the countable achievements measure, the arena run, and the arcade and
// quote records. bossWins seeds from world progress, so this runs after
// restoreWorldProgress.
void Game::restoreRecords(const SaveData& d) {
    // v40: the counters the countable rows are measured against.
    bossWins_ = d.bossWins;
    if (bossWins_ == 0) {
        // Pre-v40 nothing counted boss rounds, but the clear flags (already loaded
        // above) prove a floor: every cleared sub-area and every cleared area had its
        // boss beaten at least once. Seeding from them means an established device
        // doesn't have to re-beat content it has already finished to earn the early
        // rungs — and it can only ever UNDER-count, which is the safe direction.
        for (int a = 0; a < kAreaCount; ++a) {
            if (sectorCleared_[a]) ++bossWins_;
            for (int s = 0; s < kSubAreasPerArea; ++s)
                if (subCleared_[a][s]) ++bossWins_;
        }
    }
    // v44: boards cleared by hand. No pre-v44 seed — see the version note in save.h for
    // why defragCount is not one.
    stackerWins_ = d.stackerWins;
    // v56's three tallies. No pre-v56 fallback on purpose (save.cpp's tail note): each
    // counts an event that leaves nothing behind, so an older blob has nothing to seed
    // them from and a guess would read as a record the operator never set.
    tourneyWins_ = d.tourneyWins;
    pvpWins_ = d.pvpWins;
    mergesCooked_ = d.mergesCooked;
    // v53: the arena run. A blob written before the arena existed reads back a zero
    // seed, which IS "no bracket in play"; a phase byte naming a value this build does
    // not have falls back to Ready, so a stale save resumes a fightable run rather than
    // a screen stuck on a verdict it cannot dismiss.
    tourneySeed_ = d.tourneySeed;
    tourneyAlive_ = d.tourneyAlive;
    tourneyRound_ = d.tourneyRound < kTourneyRounds
                        ? d.tourneyRound : static_cast<uint8_t>(kTourneyRounds - 1);
    tourneyPhase_ = d.tourneyPhase <= static_cast<uint8_t>(TourneyPhase::Champion)
                        ? static_cast<TourneyPhase>(d.tourneyPhase)
                        : TourneyPhase::Ready;
    // v47: the arcade tallies, resolved back through the roster — a row naming a
    // cabinet this build no longer has simply doesn't land anywhere.
    for (int i = 0; i < kArcadeMaxCabinets; ++i) {
        arcadePlays_[i] = 0;
        arcadeWins_[i] = 0;
        arcadeBest_[i] = 0;
    }
    for (size_t i = 0; i < d.arcadeIds.size(); ++i) {
        const int row = arcadeGameIndexById(d.arcadeIds[i].id);
        if (row < 0 || row >= kArcadeMaxCabinets) continue;
        arcadePlays_[row] = i < d.arcadePlays.size() ? d.arcadePlays[i] : 0;
        arcadeWins_[row] = i < d.arcadeWins.size() ? d.arcadeWins[i] : 0;
        // Short (a pre-v55 blob, or one written by a build with fewer cabinets) reads 0
        // rather than falling off the end: a device that never recorded a best has none.
        arcadeBest_[row] = i < d.arcadeBest.size() ? d.arcadeBest[i] : 0;
    }
    // v48: the per-quote board states. Copy what overlaps and leave the rest zero, the
    // way the achievement bitsets do — which is what makes the length prefix worth
    // having. A pre-v48 blob carries none, so every quote reads back as never played.
    for (uint8_t& b : quoteStates_) b = 0;
    for (size_t i = 0; i < d.quoteStates.size() && i < kQuoteStateBytes; ++i)
        quoteStates_[i] = d.quoteStates[i];
}

// The collected-items shelf, the palate, the dive records and the raised-species tally.
void Game::restoreShelves(const SaveData& d) {
    collectedItems_.clear();
    for (const auto& s : d.collectedItems)
        if (const ItemDef* it = registry_.item(s.id)) collectedItems_.push_back(it);
    sweepCollectedItems();     // fold in whatever the bag holds — seeds a pre-v40 save
    // The palate is NOT swept from the bag for the same reason it is not written there:
    // a pre-v62 save has no record of what its pet ate, and inventing one out of what it
    // happens to be carrying would light cells for meals that never happened.
    petFoodsEaten_.clear();
    for (const auto& s : d.petFoodsEaten)
        if (const ItemDef* it = registry_.item(s.id)) petFoodsEaten_.push_back(it);
    speciesDives_.clear();
    for (size_t i = 0; i < d.speciesDiveIds.size(); ++i) {
        const int depth = i < d.speciesDiveDepths.size() ? d.speciesDiveDepths[i] : 0;
        if (const CreatureDef* c = registry_.creature(d.speciesDiveIds[i].id))
            speciesDives_.push_back({c, depth});
    }
    if (d.speciesDiveIds.empty() && d.bestDeepWebDepth > 0)
        // Pre-v40: one pet's own record is the only dive history that was ever kept, so
        // credit it to that pet's species. Everything deeper that earlier pets did is
        // simply not recoverable — nothing recorded it.
        if (const CreatureDef* c = registry_.creature(d.activeId))
            speciesDives_.push_back({c, d.bestDeepWebDepth});

    // v39: the raised-species tally, resolved the same way. Then union in every
    // species the save still POINTS at — the ARCH rack and the ARCH records hold
    // pets that were necessarily raised to get there, so they belong in the tally
    // whether or not the blob said so. That is what rebuilds a pre-v39 save (which
    // carries no tally at all) and what keeps a native one honest. The active pet
    // is covered by installPet below.
    raisedCreatures_.clear();
    for (const auto& s : d.raisedCreatures)
        if (const CreatureDef* c = registry_.creature(s.id))
            if (!creatureRaised(c->id)) raisedCreatures_.push_back(c);
    for (const auto& p : d.rack) markCreatureRaised(p.id);
    for (const auto& r : d.records) markCreatureRaised(r.id);
}

void Game::restoreActivePet(const SaveData& d) {
    // Active pet (may be empty for a Store-vacated save).
    installPet(registry_.creature(d.activeId));
    // hatchLine_ isn't independently persisted; re-derive it from the restored pet
    // whenever it's still an incubating Boot-Sector egg, so completeHatch() rolls the
    // correct line's Process pool instead of falling back to the Ransomware default
    // after any save/load that lands mid-incubation.
    hatchLine_ = pet_ && pet_->stage == Stage::BootSector
                     ? registry_.eggLineForCreature(pet_->id)
                     : nullptr;
    if (pet_) {
        model_ = PetModel();
        model_.setHunger(d.hunger);
        model_.setFragmentation(d.frag);
        model_.setHappiness(d.happy);
        model_.setCareMistakes(d.mistakes);
        model_.setDebuffs(d.debuffs);
        model_.setGhost(d.ghost != 0);
        stageEnteredMs_ = nowMs_ - d.timeInStageMs;   // reconstruct the in-stage clock
        lastModelMs_ = nowMs_;
        // Backfill any slot the v24 tail didn't cover (a pre-v24 blob leaves
        // ALL of them Unset here), then drop any restored equip that no longer
        // matches its slot's stamped kind — pre-release, so enforcing the
        // invariant cleanly on load beats grandfathering a wrong-typed equip.
        stampSlotKinds();
        enforceSlotKindInvariant();
    }
}

// CFG preferences. Runs after restoreShelves because setThemePick validates the
// theme against the collected-items shelf: applied any earlier, an earned set reads
// as locked and the load falls back to base.
void Game::restoreSettings(const SaveData& d) {
    // Screen-brightness level (v14). A pre-v14 blob carries kBrightnessDefault
    // (brightest). Clamp defensively so a corrupt value can't drive the backlight
    // out of range.
    brightness_ = d.brightness;
    // v67: UI MODE and CARE ALERTS. A mode number this build has no mode for reads as
    // the default rather than as a cast past the enum's end.
    uiMode_ = d.uiMode <= static_cast<uint8_t>(UiMode::TextOnly) ? static_cast<UiMode>(d.uiMode)
                                                                 : UiMode::IconsLabel;
    careAlerts_ = d.careAlerts != 0;
    // v68
    soundMode_ = d.soundMode < kSoundModeCount ? static_cast<SoundMode>(d.soundMode)
                                               : SoundMode::Off;
    volume_ = d.volume < kVolumeLevels ? d.volume : kVolumeLevels - 1;
    exploreSound_ = d.exploreSound != 0;   // v71
    if (brightness_ < 0) brightness_ = 0;
    if (brightness_ >= kBrightnessLevels) brightness_ = kBrightnessLevels - 1;

    // The chosen theme (v64), by name. An empty name — every pre-v64 blob — and a name
    // this build ships no set for both resolve to base, and going through setThemePick
    // is what APPLIES it: the palette index and this member move together, so the
    // interface that repaints after a load is drawn in the set the save named.
    setThemePick(palThemeByName(d.theme));
}

void Game::restoreBackgroundAndCant(const SaveData& d) {
    // v58 — the chosen background, validated rather than trusted: a background the
    // operator does not own is not a state this game should be able to boot into, and a
    // wire from a NEWER build has no row here at all. Both land on AUTO.
    //
    // LAST, and that ordering is load-bearing: ownership is derived from the raised
    // species, the cleared areas and the bracket tally, and the first of those is
    // rebuilt from the rack and the records well below where the tail is read. Asked any
    // earlier, every earned background would read as unowned and the pick would be
    // quietly thrown away.
    {
        const BackgroundDef* b = backgroundByWire(d.backgroundPick);
        backgroundPick_ = (b && backgroundOwned(b->scene)) ? b->scene : SceneId::None;
    }

    // v59: the CANT. `shakesSpent` is clamped to the lifetime handshake count restored
    // above, so a blob that somehow claims more spent than were ever captured reads back
    // as an empty purse rather than a negative one — shakesUnspent() would floor it
    // anyway, and clamping here means the next save writes the corrected figure.
    cantSigils_ = d.cantSigils;
    shakesSpent_ = d.shakesSpent < 0 ? 0
                 : (d.shakesSpent > handshakesSeen_ ? handshakesSeen_ : d.shakesSpent);
}

void Game::applySave(const SaveData& d) {
    // Order matters where a step validates against an earlier one: the boss tally seeds
    // from world progress, installPet tallies into the raised species, and the settings
    // and background validate against what everything above restored.
    restoreOperator(d);
    restoreBag(d);
    restoreHistory(d);
    restorePetProgress(d);
    restoreRadio(d);
    restoreWorldProgress(d);
    restoreRig(d);
    restoreReveals(d);
    restoreRecords(d);
    restoreShelves(d);
    restoreActivePet(d);
    restoreSettings(d);
    restoreBackgroundAndCant(d);
    lastSaveMs_ = nowMs_;
    saveDirty_ = false;
}

void Game::persistSave() {
    if (!store_) return;
    // An explicit persistSave() ASSERTS there is something worth writing, so it says so
    // before anything below can turn it away. Every guard past this point leaves the
    // flag set, which is what makes the next tick try again — the contract
    // kSaveHeapFloorBytes already documents, and which the ten "persist immediately"
    // call sites (a laid egg, a hatch, a retire) would otherwise each have to remember
    // to arrange for themselves. The autosave path reaches here only when it already
    // wanted a write; a landed one clears the flag on the way out regardless.
    saveDirty_ = true;
    // Defer rather than risk the write. A save fires off a timer, so it can land while
    // something else holds the heap — on device, the audit capture arming is the
    // measured case. Leaving saveDirty_ set means the next tick tries again, so the
    // only cost is that the write lands a few seconds later; attempting it without
    // room costs a device reset (setHeapProbe).
    //
    // TWO CHECKS, because the write allocates in two stages. captureSave() builds the
    // SaveData's own vectors, and those are easily large enough to spend the headroom
    // the first check approved — so the second has to be asked AFTER the SaveData
    // exists, not before. It is the one standing between the allocator and everything
    // below it.
    if (!saveHasHeadroom()) { noteSaveDeferred(); return; }
    const SaveData captured = captureSave();
    // The second check is the one whose firing is otherwise unobservable: the SaveData
    // whose allocation triggered it is freed on the way out, so any sampler looking at
    // the heap afterwards sees a healthy number and concludes nothing happened.
    // Counting it is the only evidence that the guard works rather than that the
    // device merely got lucky.
    if (!saveHasHeadroom()) { noteSaveDeferred(); return; }
    // Size the buffer when this save needs more of it than the last one left behind —
    // the first write after boot, and any later one whose blob has outgrown it. Dropped
    // before it is re-reserved so the peak is the new buffer alone and never both at
    // once, and reached only from behind kSaveGrowHeapFloorBytes, which saveHeapFloor()
    // demands precisely because this line is here. Every other save skips it and writes
    // into memory this object already holds.
    if (saveBlob_.capacity() < saveBlobTargetBytes()) {
        std::vector<uint8_t>().swap(saveBlob_);
        saveBlob_.reserve(saveBlobTargetBytes());
    }
    serializeSaveInto(captured, saveBlob_);
    lastBlobSize_ = saveBlob_.size();   // the buffer got sized and filled either way
    // A store that REFUSED the write leaves the blob on flash stale relative to RAM,
    // which is the failure that reads as "progress silently reverted" a reboot later —
    // the pet you just hatched replaced by the one before it. Treating it as a write
    // that landed is what makes it silent: the flag would clear, nothing would retry,
    // and saveNow() would tell the travel-sleep path it was safe to power down on a
    // save that never happened. So the flag stays set. lastSaveMs_ still advances,
    // which paces the retries at kSaveDebounceMs rather than one per beat — a store
    // that just said no is not one to hammer, and a short write is usually transient.
    if (!store_->save(saveBlob_)) {
        ++saveWritesFailed_;
        lastSaveMs_ = nowMs_;
        return;
    }
    savesDeferred_ = 0;
    saveWritesFailed_ = 0;
    lastSaveMs_ = nowMs_;
    saveDirty_ = false;
}

bool Game::saveNow() {
    if (!store_) return true;   // nothing to lose: no store, no state to strand
    // Unconditional: the state is about to stop existing in RAM, so "nothing has
    // changed since the last autosave" is not good enough — passive decay moves the
    // model without marking itself, and persistSave's own opening line covers that.
    // The return value is the flag persistSave clears ONLY on a write that landed, so
    // false means the state is still only in RAM — whether the heap guard turned the
    // write away or the store refused it. Either way the caller must not power down.
    persistSave();
    return !saveDirty_;
}

}  // namespace mal
