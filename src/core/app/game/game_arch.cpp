// game_arch.cpp (the ARCH rack): freezing a pet, thawing one back, and releasing one
// (part of the Game unit split, core/app/game.h). ArchScreen drives them.
//
// ARCH is cold storage for pets. The rack holds live pets frozen mid-lifecycle; what
// a rack slot keeps is the Pet record's decision (Pet::stored / Pet::thawed). The
// RECORD rows below are read-only entries for pets that retired or corrupted.
//
// Split out of game_config.cpp, which owns navigating the CFG screens; a rack is
// storage, not a setting, and the two only ever met because both hang off the same
// face.
#include "core/app/game.h"

#include <cstring>

#include "tunables.h"
#include "core/ui/arch_screen.h"   // ArchSaleSheet
#include "core/ui/carousel.h"

namespace mal {

ArchSaleSheet Game::archSaleSheet() const { return arch_.saleSheet(*this); }

void Game::archHatchNewEgg() {
    // The NEW EGG row's commit. With a pet to set aside this is archStoreActive: the
    // freeze and the hatch are one act. With no pet (the two-room state after a Store)
    // there is nothing to freeze and the egg is laid outright.
    if (pet_.creature) archStoreActive();
    else startHatch();
}

void Game::archReturnFromLineSelect() {
    // Back out of line-select into ARCH. Only ever offered with something on the rack,
    // so the player always has a pet to deploy from here, which is what keeps this from
    // being a way to end up with no pet at all.
    for (int i = 0; i < kCarouselSlots; ++i)
        if (carouselSlots()[i].id == SubmenuId::Arch) { summonCursor(i); break; }
    enterSubmenu();
    dirty_ = true;
}

// Freeze a SaveStoredPet snapshot of a live pet (active save → rack, or the
// outgoing active during a Deploy swap): vitals, creature-level state
// (combatLevel/combatXp/statPoints/slotKinds), and the pet's own move + mod
// loadout (owned/equipped moves, installed mod slots) (a pet's MOVES/MODS state
// travels with it through the rack, like its level), plus everything an Epic dish has
// permanently given it (core/model/pet_upgrades.h).
void Game::noteRackDuplicates() {
    // SECOND_INSTANCE: the rack is holding two of one species at once. Called after
    // every mutation that can create such a pair: a freeze into a free slot, and the
    // swap, which can drop the active pet in beside a twin already on the shelf.
    //
    // The unlock it gates (the Worm egg line, Game::eggLineUnlocked) is therefore an
    // earned BIT rather than a live read of the rack: releasing one of the pair later
    // must not take the line back off line-select. archStoreActive calls this before
    // startHatch() for the same reason: the freeze that earns a line is usually the
    // one whose line-select should offer it.
    for (size_t i = 0; i + 1 < op_.arch.rack.size(); ++i)
        for (size_t j = i + 1; j < op_.arch.rack.size(); ++j)
            if (std::strcmp(op_.arch.rack[i].id, op_.arch.rack[j].id) == 0) {
                unlockAchievement(ach::kSecondInstance);
                return;
            }
}

void Game::archStoreActive() {
    if (!pet_.creature || static_cast<int>(op_.arch.rack.size()) >= rackSlots()) return;
    // Set the active pet aside into a free slot, then vacate the active save → the
    // contextual new-egg Hatch fires. Persist immediately so the stored pet
    // survives a reboot during the egg timer (the save now has an empty active +
    // a populated rack).
    op_.arch.rack.push_back(pet_.stored(nowMs_));
    noteRackDuplicates();   // before startHatch: a line earned HERE belongs on THIS menu
    // ...and the shelf with nothing free left on it. Fired at the freeze instead of
    // swept off a count, because "full" compares against rackSlots(), which the player
    // can raise. A slot bought later must not un-earn the row.
    if (static_cast<int>(op_.arch.rack.size()) >= rackSlots()) unlockAchievement(ach::kNoVacancy);
    startHatch();        // pet_.creature = nullptr -> line-select, then a fresh egg
    persistSave();
}

void Game::archDeployStored(int storedIdx) {
    if (storedIdx < 0 || storedIdx >= static_cast<int>(op_.arch.rack.size())) return;
    // Slot-neutral swap: the deployed pet becomes active; the current active freezes into
    // the slot it vacated.
    //
    // With no active pet the swap is a plain thaw and the slot is freed. That happens
    // between an ARCH Store and the next egg, because line-select can be backed out of
    // (game_nav.cpp). There is nothing to freeze into the slot, and leaving the pet's
    // own frozen copy behind would duplicate it.
    const SaveStoredPet incoming = op_.arch.rack[storedIdx];
    const CreatureDef* next = registry_.creature(incoming.id);
    if (!next) return;                              // unknown id: abort the swap
    if (pet_.creature) op_.arch.rack[storedIdx] = pet_.stored(nowMs_);
    else op_.arch.rack.erase(op_.arch.rack.begin() + storedIdx);

    // The whole incoming pet replaces the outgoing one, so nothing the outgoing pet had
    // armed or spent carries over. The spare mod pool is the player's and stays.
    pet_ = Pet::thawed(incoming, registry_, nowMs_);
    installPet(next);
    lastModelMs_ = nowMs_;                          // no decay jump on the swap
    // Backfill any still-Unset slot from the newly-active pet's layout (covers a
    // pre-v26 stored pet with no recorded slot typing), then drop any equip that no
    // longer matches its slot's stamped kind.
    stampSlotKinds();
    enforceSlotKindInvariant();
    noteRackDuplicates();   // the swap can drop this pet in beside a twin on the shelf
    nav_ = Nav::Idle;                              // show the newly-active pet
    persistSave();
}

void Game::archReleaseStored(int storedIdx) {
    // A no-reward Release valve: drop a stored pet from the rack entirely to free a
    // slot (e.g. a full rack of non-Daemon pets that can't be Sold). Leaves no
    // record: a plain release, unlike a CSF or a sale.
    if (storedIdx < 0 || storedIdx >= static_cast<int>(op_.arch.rack.size())) return;
    op_.arch.rack.erase(op_.arch.rack.begin() + storedIdx);
    nav_ = Nav::Submenu;   // back to the (now shorter) rack list
    persistSave();
}

void Game::debugSeedRack(const char* creatureId, int combatLevel) {
    const CreatureDef* c = registry_.creature(creatureId);
    if (!c || static_cast<int>(op_.arch.rack.size()) >= rackSlots()) return;
    SaveStoredPet p;
    std::strncpy(p.id, c->id, kSaveIdCap - 1);
    p.hunger = 60; p.frag = 10; p.happy = 70; p.mistakes = 1;
    p.debuffs = 0; p.ghost = 0; p.generation = 1;
    p.combatLevel = combatLevel;
    p.statPoints[0] = combatLevel;
    op_.arch.rack.push_back(p);
}

}  // namespace mal
