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
#include "core/ui/arch_screen.h"
#include "core/ui/carousel.h"

namespace mal {

std::vector<ArchRow> Game::archRows() const {
    return buildArchRows(registry_, archGroup_, pet_, generation_, rack_, records_);
}

ArchRow Game::archFocusedRow() const {
    const auto rows = archRows();
    if (listRow_ < 0 || listRow_ >= static_cast<int>(rows.size())) return ArchRow{};
    return rows[listRow_];
}

void Game::onArchPicker(const ButtonEvent& ev) {
    // The L2 group picker: NEW EGG, ACTIVE, one row per creature family, RECORDS. A
    // walks (wraps), B opens, C backs out — the ITEMS type-picker's gestures exactly,
    // because they are the same screen doing the same job one level up.
    const auto tiles = buildArchPickerRows(registry_, pet_, rack_, records_);
    const int n = static_cast<int>(tiles.size());
    if (n <= 0) { if (ev.button == Button::C) nav_ = Nav::Cursor; return; }
    if (archPickRow_ < 0 || archPickRow_ >= n) archPickRow_ = 0;

    if (ev.button == Button::A) {
        archPickRow_ = (archPickRow_ + 1) % n;
    } else if (ev.button == Button::B) {
        archGroup_ = tiles[archPickRow_].group;
        archConfirm_ = false;
        archConfirmChoice_ = 0;
        // NEW EGG is an ACTION, so it opens its own L3 confirm rather than a list. An
        // EMPTY group still opens — the list says "- NOTHING HERE -" and C walks back,
        // which reads better than a dead button on a row the screen just drew.
        if (archGroup_.kind == ArchGroup::Kind::NewEgg) {
            nav_ = Nav::Detail;
        } else {
            archScreen_ = ArchScreen::List;
            listRow_ = 0;
            archAction_ = archGroup_.kind == ArchGroup::Kind::Active ? ArchAction::Store
                                                                     : ArchAction::Deploy;
        }
    } else if (ev.button == Button::C) {
        // With no active pet the carousel is not somewhere to be — the player is between
        // a Store and the egg that follows it, and the only two rooms are this one and
        // line-select. C returns to the choice rather than out to a habitat with nothing
        // living in it (the mirror of the modal's own C, game_nav.cpp).
        //
        // startHatch() rather than a nav_ poke, because it is the same call that put the
        // player here and re-derives what is on offer. It cannot lay an egg by surprise:
        // it only skips straight to layEgg with ONE unlocked line, and with one line
        // there is no line-select to have been backed out of — so this state is
        // unreachable unless the modal was up, which means two or more.
        if (!pet_ && !rack_.empty()) startHatch();
        else nav_ = Nav::Cursor;
    }
}

void Game::onArchList(const ButtonEvent& ev) {
    // One group's rows. A walks (wraps), B opens the focused entry, C backs to the
    // picker. Which LIST a row came from and where it sits in it is on the row itself
    // (ArchRow), so nothing here re-derives a section from a bare cursor.
    const auto rows = archRows();
    const int n = static_cast<int>(rows.size());
    if (ev.button == Button::A) {
        if (n > 0) listRow_ = (listRow_ + 1) % n;
    } else if (ev.button == Button::B) {
        if (n <= 0) return;                       // an empty group has nothing to open
        const ArchRow& r = rows[listRow_ % n];
        // Records open a read-only detail; live pets open the action set.
        if (r.kind != ArchRow::Kind::Record)
            archAction_ = r.kind == ArchRow::Kind::Active ? ArchAction::Store
                                                          : ArchAction::Deploy;
        archConfirm_ = false;
        nav_ = Nav::Detail;
    } else if (ev.button == Button::C) {
        archScreen_ = ArchScreen::Picker;
    }
}

void Game::archHatchNewEgg() {
    // The NEW EGG row's commit. With a pet to set aside this IS archStoreActive — the
    // freeze and the hatch are one act, and always were; the row just stopped hiding it
    // behind a pet's record. With no pet (the two-room state after a Store) there is
    // nothing to freeze and the egg is laid outright.
    archConfirm_ = false;
    archScreen_ = ArchScreen::Picker;
    if (pet_) archStoreActive();
    else startHatch();
}

void Game::archReturnFromLineSelect() {
    // Back out of line-select into ARCH. Only ever offered with something on the rack,
    // so the player always has a pet to deploy from here — which is what keeps this from
    // being a way to end up with no pet at all.
    for (int i = 0; i < kCarouselSlots; ++i)
        if (carouselSlots()[i].id == SubmenuId::Arch) { summonCursor(i); break; }
    enterSubmenu();
    dirty_ = true;
}

void Game::onArchRecord(const ButtonEvent& ev) {
    // The NEW EGG confirm is its own L3: no action set to cycle, just the prompt.
    if (archOnNewEgg()) {
        const bool rackFull = pet_ && static_cast<int>(rack_.size()) >= rackSlots();
        if (archConfirm_) {
            if (ev.button == Button::A) archConfirmChoice_ ^= 1;
            else if (ev.button == Button::B) {
                if (archConfirmChoice_ == 1) { archHatchNewEgg(); return; }
                archConfirm_ = false;
            } else if (ev.button == Button::C) {
                archConfirm_ = false;
            }
            return;
        }
        if (ev.button == Button::B) {
            if (rackFull) return;                 // no free slot -> blocked (gate shown)
            archConfirm_ = true;
            archConfirmChoice_ = 0;               // default Cancel
        } else if (ev.button == Button::C) {
            nav_ = Nav::Submenu;
        }
        return;
    }

    const ArchRow row = archFocusedRow();
    // A RETIRED/CORRUPTED record is read-only: no actions, C backs. So is an empty
    // group, which has no row to act on at all.
    if (row.kind == ArchRow::Kind::Record || !row.def) {
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
                    archDeployStored(row.index);
                else if (archAction_ == ArchAction::Release)
                    archReleaseStored(row.index);
                return;   // the commit set nav_/state itself
            }
            archConfirm_ = false;     // Cancel -> stay in the record
        } else if (ev.button == Button::C) {
            archConfirm_ = false;
        }
        return;
    }

    const bool active = row.kind == ArchRow::Kind::Active;
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
// travels with it through the rack, like its level — plus everything an Epic dish has
// permanently given it (core/model/pet_upgrades.h).
static SaveStoredPet freezePet(const CreatureDef* pet, const PetModel& m, int gen,
                               int defragCount, int combatLevel, int combatXp,
                               const int (&statPoints)[kLevelStatCount],
                               const Game::SlotKind (&slotKinds)[kMaxMoveSlots],
                               const MoveLoadout& moveLoadout, const Loadout& loadout,
                               uint32_t timeInStageMs, int bestDeepWebDepth,
                               uint32_t dyingElapsedMs, const PetUpgrades& upgrades) {
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
    // The Epic-dish upgrades belong to the CREATURE, so the rack keeps them: storing a
    // pet must never cost it an upgrade it was fed.
    p.bandwidthRegenBonusMin = upgrades.bandwidthRegenMin;
    for (int i = 0; i < kLevelStatCount; ++i) p.statBonus[i] = upgrades.statBonus[i];
    p.xpRateBonusPct = upgrades.xpRatePct;
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
                               dyingElapsedMs_, upgrades_));
    noteRackDuplicates();   // before startHatch: a line earned HERE belongs on THIS menu
    // ...and the shelf with nothing free left on it. Fired at the freeze rather than
    // swept off a count, because "full" is a comparison against rackSlots() — a ceiling
    // the player buys — so there is no fixed rung to hold it to, and a slot bought
    // afterwards must not read as the row having come undone.
    if (static_cast<int>(rack_.size()) >= rackSlots()) unlockAchievement(ach::kNoVacancy);
    archConfirm_ = false;
    listRow_ = 0;
    startHatch();        // pet_ = nullptr -> line-select, then a fresh egg
    persistSave();
}

void Game::archDeployStored(int storedIdx) {
    if (storedIdx < 0 || storedIdx >= static_cast<int>(rack_.size())) return;
    // Slot-neutral swap: the deployed pet becomes active; the current active freezes into
    // the slot it vacated.
    //
    // With NO active pet the swap is a plain thaw and the slot is FREED instead — the
    // state a player is in between an ARCH Store and the egg that follows it, now that
    // line-select can be backed out of (game_nav.cpp). There is nothing to freeze into
    // the slot, and leaving the pet's own frozen copy behind would duplicate it.
    const SaveStoredPet incoming = rack_[storedIdx];
    const CreatureDef* next = registry_.creature(incoming.id);
    if (!next) return;                              // unknown id — abort the swap
    if (pet_) {
        rack_[storedIdx] = freezePet(pet_, model_, generation_, defragCount_, combatLevel_,
                                      combatXp_, statPoints_, slotKinds_, moveLoadout_,
                                      loadout_, nowMs_ - stageEnteredMs_, bestDeepWebDepth_,
                                      dyingElapsedMs_, upgrades_);
    } else {
        rack_.erase(rack_.begin() + storedIdx);
    }

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
    // The USB port empties on a pet swap. It is NOT frozen with either pet — a device is
    // plugged into the rig, not into the creature — but it steers a boundary the incoming
    // pet is standing at a different distance from, and a soak is Process-only by gate
    // (Game::itemUsable), which a swap would otherwise walk straight around. Emptying is
    // the honest reading of both: what was plugged in was plugged in for the pet that has
    // just gone into the rack — the hold included, which is also the way out of a pet
    // parked at a stage by a player who has no Eject-USB to hand.
    clearUsbPort();
    // The palate empties with the port, and for a plainer reason: the rack record has no
    // room for one. A stored pet's tasted set would be a list of up to every food on the
    // shelf, per slot, against a 256KB save — so what a frozen pet carries is everything
    // BUT that, and a thawed pet's plate reads as unrecorded rather than as its
    // predecessor's.
    petFoodsEaten_.clear();
    bestDeepWebDepth_ = incoming.bestDeepWebDepth;  // thaw this pet's own DeepWeb record
    // Thaw the incoming pet's dying window mid-flight. dyingArmed_ is deliberately NOT
    // restored: it anchors against nowMs_, so the next tick re-arms it against the
    // current clock while this accumulator carries the only figure that means anything.
    dyingElapsedMs_ = incoming.dyingElapsedMs;
    dyingArmed_ = false;
    // ...and the incoming pet's own permanent upgrades, which is why the rig regenerates
    // at different speeds — and the same rig fights at different strengths — under
    // different pets.
    upgrades_.bandwidthRegenMin = incoming.bandwidthRegenBonusMin;
    for (int i = 0; i < kLevelStatCount; ++i)
        upgrades_.statBonus[i] = incoming.statBonus[i];
    upgrades_.xpRatePct = incoming.xpRateBonusPct;
    // v26: thaw the incoming pet's creature-level state. Without it the deployed pet
    // silently inherits the outgoing pet's level.
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
