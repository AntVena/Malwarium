// game_arch.cpp (the ARCH rack): freezing a pet, thawing one back, and the two
// screens that drive it (part of the Game unit split, core/app/game.h).
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
#include "core/ui/arch_screen.h"
#include "core/ui/carousel.h"

namespace mal {

std::vector<ArchRow> Game::archRows() const {
    return buildArchRows(registry_, archGroup_, pet_.creature, pet_.generation, rack_, records_);
}

ArchRow Game::archFocusedRow() const {
    const auto rows = archRows();
    if (listRow_ < 0 || listRow_ >= static_cast<int>(rows.size())) return ArchRow{};
    return rows[listRow_];
}

void Game::onArchPicker(const ButtonEvent& ev) {
    // The L2 group picker: NEW EGG, ACTIVE, one row per creature family, RECORDS. A
    // walks (wraps), B opens, C backs out: the ITEMS type-picker's gestures, because
    // this is the same kind of screen one level up.
    const auto tiles = buildArchPickerRows(registry_, pet_.creature, rack_, records_);
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
        // empty group still opens, showing "- NOTHING HERE -", so B never does
        // nothing on a row the screen drew.
        if (archGroup_.kind == ArchGroup::Kind::NewEgg) {
            nav_ = Nav::Detail;
        } else {
            archScreen_ = ArchScreen::List;
            listRow_ = 0;
            archAction_ = archGroup_.kind == ArchGroup::Kind::Active ? ArchAction::Store
                                                                     : ArchAction::Deploy;
        }
    } else if (ev.button == Button::C) {
        // With no active pet there is no carousel. The player is between a Store and the
        // next egg, and can be only here or on line-select. C returns to the choice
        // rather than out to a habitat with nothing living in it (the mirror of the
        // modal's own C, game_nav.cpp).
        //
        // startHatch() rather than a nav_ poke, because it is the same call that put the
        // player here and re-derives what is on offer. It cannot lay an egg by surprise:
        // it only skips straight to layEgg with ONE unlocked line, and with one line
        // there is no line-select to have been backed out of, so this state needs the
        // modal, which means two or more lines.
        if (!pet_.creature && !rack_.empty()) startHatch();
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
        // Records open a read-only detail; live pets open the action set, or the SELL
        // sheet when it was the SELL counter that listed them.
        if (r.kind != ArchRow::Kind::Record)
            archAction_ = r.kind == ArchRow::Kind::Active ? ArchAction::Store
                                                          : ArchAction::Deploy;
        archSaleOffer_ = SaleOffer::Bits;
        archConfirm_ = false;
        nav_ = Nav::Detail;
    } else if (ev.button == Button::C) {
        archScreen_ = ArchScreen::Picker;
    }
}

void Game::archHatchNewEgg() {
    // The NEW EGG row's commit. With a pet to set aside this is archStoreActive: the
    // freeze and the hatch are one act. With no pet (the two-room state after a Store)
    // there is nothing to freeze and the egg is laid outright.
    archConfirm_ = false;
    archScreen_ = ArchScreen::Picker;
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

void Game::onArchRecord(const ButtonEvent& ev) {
    // The NEW EGG confirm is its own L3: no action set to cycle, just the prompt.
    if (archOnNewEgg()) {
        const bool rackFull = pet_.creature && static_cast<int>(rack_.size()) >= rackSlots();
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

    if (archGroup_.kind == ArchGroup::Kind::Sell) { onArchSale(ev); return; }

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
        // Cycle within the pet's action set. Active has Store alone; a stored pet adds
        // the no-reward Release valve beside Deploy.
        if (!active)
            archAction_ = archAction_ == ArchAction::Deploy ? ArchAction::Release
                                                            : ArchAction::Deploy;
    } else if (ev.button == Button::B) {
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
    for (size_t i = 0; i + 1 < rack_.size(); ++i)
        for (size_t j = i + 1; j < rack_.size(); ++j)
            if (std::strcmp(rack_[i].id, rack_[j].id) == 0) {
                unlockAchievement(ach::kSecondInstance);
                return;
            }
}

void Game::archStoreActive() {
    if (!pet_.creature || static_cast<int>(rack_.size()) >= rackSlots()) return;
    // Set the active pet aside into a free slot, then vacate the active save → the
    // contextual new-egg Hatch fires. Persist immediately so the stored pet
    // survives a reboot during the egg timer (the save now has an empty active +
    // a populated rack).
    rack_.push_back(pet_.stored(nowMs_));
    noteRackDuplicates();   // before startHatch: a line earned HERE belongs on THIS menu
    // ...and the shelf with nothing free left on it. Fired at the freeze instead of
    // swept off a count, because "full" compares against rackSlots(), which the player
    // can raise. A slot bought later must not un-earn the row.
    if (static_cast<int>(rack_.size()) >= rackSlots()) unlockAchievement(ach::kNoVacancy);
    archConfirm_ = false;
    listRow_ = 0;
    startHatch();        // pet_.creature = nullptr -> line-select, then a fresh egg
    persistSave();
}

void Game::archDeployStored(int storedIdx) {
    if (storedIdx < 0 || storedIdx >= static_cast<int>(rack_.size())) return;
    // Slot-neutral swap: the deployed pet becomes active; the current active freezes into
    // the slot it vacated.
    //
    // With no active pet the swap is a plain thaw and the slot is freed. That happens
    // between an ARCH Store and the next egg, because line-select can be backed out of
    // (game_nav.cpp). There is nothing to freeze into the slot, and leaving the pet's
    // own frozen copy behind would duplicate it.
    const SaveStoredPet incoming = rack_[storedIdx];
    const CreatureDef* next = registry_.creature(incoming.id);
    if (!next) return;                              // unknown id: abort the swap
    if (pet_.creature) rack_[storedIdx] = pet_.stored(nowMs_);
    else rack_.erase(rack_.begin() + storedIdx);

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
    archConfirm_ = false;
    listRow_ = 0;
    nav_ = Nav::Idle;                              // show the newly-active pet
    persistSave();
}

void Game::archReleaseStored(int storedIdx) {
    // A no-reward Release valve: drop a stored pet from the rack entirely to free a
    // slot (e.g. a full rack of non-Daemon pets that can't be Sold). Leaves no
    // record: a plain release, unlike a CSF or a sale.
    if (storedIdx < 0 || storedIdx >= static_cast<int>(rack_.size())) return;
    rack_.erase(rack_.begin() + storedIdx);
    archConfirm_ = false;
    listRow_ = 0;
    nav_ = Nav::Submenu;   // back to the (now shorter) rack list
    persistSave();
}

void Game::debugSeedRack(const char* creatureId, int combatLevel) {
    const CreatureDef* c = registry_.creature(creatureId);
    if (!c || static_cast<int>(rack_.size()) >= rackSlots()) return;
    SaveStoredPet p;
    std::strncpy(p.id, c->id, kSaveIdCap - 1);
    p.hunger = 60; p.frag = 10; p.happy = 70; p.mistakes = 1;
    p.debuffs = 0; p.ghost = 0; p.generation = 1;
    p.combatLevel = combatLevel;
    p.statPoints[0] = combatLevel;
    rack_.push_back(p);
}

}  // namespace mal
