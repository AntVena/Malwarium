// game_arch.cpp — the ARCH rack: freezing a pet, thawing one back, and the two
// screens that drive it (part of the Game unit split, core/app/game.h).
//
// ARCH is cold storage for pets. The rack holds live pets frozen mid-lifecycle —
// every stat they went down with comes back with them — while the RECORD rows below
// it are read-only headstones for pets that retired or corrupted. Freezing and
// thawing are the whole subsystem: what a rack slot must carry is exactly what
// PetModel cannot be rebuilt without, which is why freezePet lives here rather than
// on the model.
//
// Split out of game_config.cpp, which owns navigating the CFG screens; a rack is
// storage, not a setting, and the two only ever met because both hang off the same
// face.
#include "core/app/game.h"

#include <cstring>

#include "tunables.h"
#include "core/render/canvas.h"
#include "core/render/font.h"
#include "core/render/palette.h"

namespace mal {

void Game::onArchList(const ButtonEvent& ev) {
    // Row 0 = active pet; the next rows = frozen rack entries; the trailing rows =
    // RETIRED/CORRUPTED records (read-only). A walks (wraps), B opens the focused
    // entry, C backs to the carousel.
    const int rows = 1 + static_cast<int>(rack_.size()) +
                     static_cast<int>(records_.size());
    if (ev.button == Button::A) {
        listRow_ = (listRow_ + 1) % rows;
    } else if (ev.button == Button::B) {
        // Records open a read-only detail; live pets open the action set.
        if (!archRowIsRecord())
            archAction_ = archRowIsActive() ? ArchAction::Store : ArchAction::Deploy;
        archConfirm_ = false;
        nav_ = Nav::Detail;
    } else if (ev.button == Button::C) {
        nav_ = Nav::Cursor;
    }
}

void Game::onArchRecord(const ButtonEvent& ev) {
    // A RETIRED/CORRUPTED record is read-only: no actions, C backs.
    if (archRowIsRecord()) {
        if (ev.button == Button::C) nav_ = Nav::Submenu;
        return;
    }
    // Inline confirm for Store/Deploy: A toggles, B commits the choice, C aborts.
    if (archConfirm_) {
        if (ev.button == Button::A) archConfirmChoice_ ^= 1;
        else if (ev.button == Button::B) {
            if (archConfirmChoice_ == 1) {
                if (archAction_ == ArchAction::Store) archStoreActive();
                else if (archAction_ == ArchAction::Deploy)
                    archDeployStored(listRow_ - 1);
                else if (archAction_ == ArchAction::Release)
                    archReleaseStored(listRow_ - 1);
                return;   // the commit set nav_/state itself
            }
            archConfirm_ = false;     // Cancel -> stay in the record
        } else if (ev.button == Button::C) {
            archConfirm_ = false;
        }
        return;
    }

    const bool active = archRowIsActive();
    if (ev.button == Button::A) {
        // Cycle within the pet's action set. Active: Store → Sell → Store. Stored adds a
        // no-reward Release valve: Deploy → Sell → Release → Deploy.
        if (active) {
            archAction_ = (archAction_ == ArchAction::Store) ? ArchAction::Sell
                                                             : ArchAction::Store;
        } else {
            archAction_ = (archAction_ == ArchAction::Deploy)  ? ArchAction::Sell
                        : (archAction_ == ArchAction::Sell)    ? ArchAction::Release
                                                               : ArchAction::Deploy;
        }
    } else if (ev.button == Button::B) {
        if (archAction_ == ArchAction::Sell) return;   // Daemon-only gate (no Daemon yet)
        if (archAction_ == ArchAction::Store &&
            static_cast<int>(rack_.size()) >= rackSlots())
            return;                                     // rack full -> blocked (gate shown)
        archConfirm_ = true;                            // Store/Deploy/Release -> confirm
        archConfirmChoice_ = 0;                         // default Cancel
    } else if (ev.button == Button::C) {
        nav_ = Nav::Submenu;
    }
}

// Freeze a SaveStoredPet snapshot of a live pet (active save → rack, or the
// outgoing active during a Deploy swap): vitals, creature-level state
// (combatLevel/combatXp/statPoints/slotKinds), and the pet's own move + mod
// loadout (owned/equipped moves, installed mod slots) — a pet's MOVES/MODS state
// travels with it through the rack, like its level.
static SaveStoredPet freezePet(const CreatureDef* pet, const PetModel& m, int gen,
                               int defragCount, int combatLevel, int combatXp,
                               const int (&statPoints)[kLevelStatCount],
                               const Game::SlotKind (&slotKinds)[kMaxMoveSlots],
                               const MoveLoadout& moveLoadout, const Loadout& loadout,
                               uint32_t timeInStageMs, int bestDeepWebDepth,
                               uint32_t dyingElapsedMs, int bandwidthRegenBonusMin) {
    SaveStoredPet p;
    std::strncpy(p.id, pet->id, kSaveIdCap - 1);
    p.hunger = m.hunger();
    p.frag = m.fragmentation();
    p.happy = m.happiness();
    p.mistakes = m.careMistakes();
    p.debuffs = m.debuffs();
    p.ghost = m.hasGhost() ? 1 : 0;
    p.generation = gen;
    p.defragCount = defragCount;   // the defrag tally survives freeze/thaw
    p.combatLevel = combatLevel;
    p.combatXp = combatXp;
    for (int i = 0; i < kLevelStatCount; ++i) p.statPoints[i] = statPoints[i];
    for (int i = 0; i < kMaxMoveSlots; ++i)
        p.slotKinds[i] = static_cast<uint8_t>(slotKinds[i]);
    for (const char* mv : moveLoadout.owned()) {
        SaveId id; std::strncpy(id.id, mv, kSaveIdCap - 1);
        p.ownedMoves.push_back(id);
    }
    for (int i = 0; i < kMaxMoveSlots; ++i)
        if (const char* e = moveLoadout.equipped(i)) std::strncpy(p.equippedMoves[i].id, e, kSaveIdCap - 1);
    for (int i = 0; i < kModSlots; ++i)
        if (const char* e = loadout.equipped(i)) std::strncpy(p.equippedMods[i].id, e, kSaveIdCap - 1);
    p.timeInStageMs = timeInStageMs;   // evolution-timer progress survives freeze/thaw
    p.bestDeepWebDepth = bestDeepWebDepth;  // this pet's own DeepWeb record survives freeze/thaw
    p.dyingElapsedMs = dyingElapsedMs;      // a 5/5 pet thaws mid-window, not with a fresh one
    // Tiramisudo's permanent regen shave belongs to the CREATURE, so the rack keeps it:
    // storing a pet must never cost it an upgrade it was fed.
    p.bandwidthRegenBonusMin = bandwidthRegenBonusMin;
    return p;
}

void Game::noteRackDuplicates() {
    // SECOND_INSTANCE: the rack is holding two of one species at once. Called after
    // every mutation that can create such a pair — a freeze into a free slot, and the
    // swap, which can drop the active pet in beside a twin already on the shelf.
    //
    // The unlock it gates (the Worm egg line, Game::eggLineUnlocked) is therefore an
    // earned BIT rather than a live read of the rack: releasing one of the pair later
    // must not take the line back off line-select. archStoreActive calls this before
    // startHatch() for the same reason in the other direction — the freeze that earns
    // the line is usually the one whose line-select should already be offering it.
    for (size_t i = 0; i + 1 < rack_.size(); ++i)
        for (size_t j = i + 1; j < rack_.size(); ++j)
            if (std::strcmp(rack_[i].id, rack_[j].id) == 0) {
                unlockAchievement(ach::kSecondInstance);
                return;
            }
}

void Game::archStoreActive() {
    if (!pet_ || static_cast<int>(rack_.size()) >= rackSlots()) return;
    // Set the active pet aside into a free slot, then vacate the active save → the
    // contextual new-egg Hatch fires. Persist immediately so the stored pet
    // survives a reboot during the egg timer (the save now has an empty active +
    // a populated rack).
    rack_.push_back(freezePet(pet_, model_, generation_, defragCount_, combatLevel_,
                               combatXp_, statPoints_, slotKinds_, moveLoadout_, loadout_,
                               nowMs_ - stageEnteredMs_, bestDeepWebDepth_,
                               dyingElapsedMs_, bandwidthRegenBonusMin_));
    noteRackDuplicates();   // before startHatch: a line earned HERE belongs on THIS menu
    archConfirm_ = false;
    listRow_ = 0;
    startHatch();        // pet_ = nullptr -> line-select, then a fresh egg
    persistSave();
}

void Game::archDeployStored(int storedIdx) {
    if (storedIdx < 0 || storedIdx >= static_cast<int>(rack_.size()) || !pet_) return;
    // Slot-neutral swap: the deployed pet becomes active; the current active
    // freezes into the slot it vacated.
    const SaveStoredPet incoming = rack_[storedIdx];
    const CreatureDef* next = registry_.creature(incoming.id);
    if (!next) return;                              // unknown id — abort the swap
    rack_[storedIdx] = freezePet(pet_, model_, generation_, defragCount_, combatLevel_,
                                  combatXp_, statPoints_, slotKinds_, moveLoadout_, loadout_,
                                  nowMs_ - stageEnteredMs_, bestDeepWebDepth_,
                                  dyingElapsedMs_, bandwidthRegenBonusMin_);

    installPet(next);
    model_ = PetModel();
    model_.setHunger(incoming.hunger);
    model_.setFragmentation(incoming.frag);
    model_.setHappiness(incoming.happy);
    model_.setCareMistakes(incoming.mistakes);
    model_.setDebuffs(incoming.debuffs);
    model_.setGhost(incoming.ghost != 0);
    generation_ = incoming.generation;
    defragCount_ = incoming.defragCount;            // thaw the defrag tally
    bestDeepWebDepth_ = incoming.bestDeepWebDepth;  // thaw this pet's own DeepWeb record
    // Thaw the incoming pet's dying window mid-flight. dyingArmed_ is deliberately NOT
    // restored: it anchors against nowMs_, so the next tick re-arms it against the
    // current clock while this accumulator carries the only figure that means anything.
    dyingElapsedMs_ = incoming.dyingElapsedMs;
    dyingArmed_ = false;
    // ...and the incoming pet's own Bandwidth-regen upgrade, which is why the rig
    // regenerates at different speeds under different pets.
    bandwidthRegenBonusMin_ = incoming.bandwidthRegenBonusMin;
    // v26: thaw the incoming pet's creature-level state (was previously never
    // restored — the deployed pet silently inherited the outgoing pet's level).
    combatLevel_ = incoming.combatLevel;
    combatXp_ = incoming.combatXp;
    for (int i = 0; i < kLevelStatCount; ++i) statPoints_[i] = incoming.statPoints[i];
    for (int i = 0; i < kMaxMoveSlots; ++i)
        slotKinds_[i] = static_cast<SlotKind>(incoming.slotKinds[i]);
    // The incoming pet's own move + mod loadout: has-a, not shared. Empty
    // ownedMoves marks a pre-v29 stored pet with no recorded move state, so it
    // seeds its line's starting kit instead. The mod SPARE pool stays untouched
    // (player-level, like the ITEMS inventory) — only the installed slots are per-pet.
    if (!incoming.ownedMoves.empty()) {
        moveLoadout_ = MoveLoadout{};
        for (const auto& mv : incoming.ownedMoves)
            if (const MoveDef* def = registry_.move(mv.id)) moveLoadout_.grant(def->id);
        for (int i = 0; i < kMaxMoveSlots; ++i)
            if (const MoveDef* def = registry_.move(incoming.equippedMoves[i].id))
                moveLoadout_.equip(i, def->id);
    } else {
        moveLoadout_ = MoveLoadout::startingForLine(registry_, next->line);
    }
    loadout_.resetSlots();
    for (int i = 0; i < kModSlots; ++i)
        if (const ModDef* def = registry_.mod(incoming.equippedMods[i].id))
            loadout_.setEquipped(i, def->id);
    // the re-farm curve is per-pet — a swapped-in pet farms cleared areas
    // fresh (it isn't carried in the rack blob; a redeployed pet starts undepleted).
    for (auto& row : subRefarmCount_) for (auto& c : row) c = 0;
    stageEnteredMs_ = nowMs_ - incoming.timeInStageMs;  // thaw the evolution-timer progress
    lastModelMs_ = nowMs_;                          // no decay jump on the swap
    // Backfill any still-Unset slot from the newly-active pet's layout (covers a
    // pre-v26 stored pet with no recorded slot typing), then drop any equip that no
    // longer matches its slot's stamped kind.
    stampSlotKinds();
    enforceSlotKindInvariant();
    noteRackDuplicates();   // the swap can drop this pet in beside a twin on the shelf
    archConfirm_ = false;
    listRow_ = 0;
    nav_ = Nav::Idle;                              // show the newly-active pet
    persistSave();
}

void Game::archReleaseStored(int storedIdx) {
    // A no-reward Release valve: drop a stored pet from the rack entirely to
    // free a slot (e.g. a full rack of non-Daemon pets that can't be Sold). Leaves no
    // record (a plain release, not a CSF/retire) — the pet is simply gone.
    if (storedIdx < 0 || storedIdx >= static_cast<int>(rack_.size())) return;
    rack_.erase(rack_.begin() + storedIdx);
    archConfirm_ = false;
    listRow_ = 0;
    nav_ = Nav::Submenu;   // back to the (now shorter) rack list
    persistSave();
}

void Game::debugSeedRack(const char* creatureId) {
    const CreatureDef* c = registry_.creature(creatureId);
    if (!c || static_cast<int>(rack_.size()) >= rackSlots()) return;
    SaveStoredPet p;
    std::strncpy(p.id, c->id, kSaveIdCap - 1);
    p.hunger = 60; p.frag = 10; p.happy = 70; p.mistakes = 1;
    p.debuffs = 0; p.ghost = 0; p.generation = 1;
    rack_.push_back(p);
}

}  // namespace mal
