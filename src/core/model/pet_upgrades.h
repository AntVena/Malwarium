// pet_upgrades.h — the permanent upgrades a PET carries for the rest of its life.
//
// Everything else an item arms on a pet lapses: a shield is spent, a dive buff is
// consumed, a timer runs out. These do not. They are granted ONCE per pet by an Epic
// dish out of the MERGE HUB (content_items.cpp's cooked-dish block), they belong to the
// CREATURE rather than to the run — so an ARCH freeze/thaw keeps them (SaveStoredPet) —
// and the only way to have one twice is to raise a second pet.
//
// Grouped because they travel together everywhere: Game holds one of these, freezePet
// (game_arch.cpp) puts one on the shelf, the save codec writes one, and STAT's BUFFS
// page reads one to list what a pet is permanently carrying.
//
// Adding one: a field here, its ItemEffect::Kind + applier case (defs.h /
// Game::applyItemEffects), its line in the save codec's tail, and a BUFFS row.
#pragma once

#include "tunables.h"   // kLevelStatCount — the combat-stat axis statBonus is indexed on

namespace mal {

struct PetUpgrades {
    // MINUTES shaved off this pet's Bandwidth regen interval (Game::bandwidthRegenMinutes,
    // floored at kBandwidthRegenMinutesFloor). Tiramisudo.
    int bandwidthRegenMin = 0;
    // OFF-LEVEL combat-stat points: 0 power · 1 defense · 2 speed · 3 max-Health, the same
    // axis Game::statPoints_ uses (game_internal.h's levelStatName). Added to the earned
    // points wherever the pet is built into a fighter, so a point is a point in combat —
    // but counted in NEITHER the level (which stays == the sum of EARNED points) nor the
    // Rollback picker, which is what makes them unsheddable. A Rollback can therefore never
    // take a pet below what it was fed.
    int statBonus[kLevelStatCount] = {0};
    // PERCENT added to every XP award this pet takes (Game::addCombatXp). Compounds with
    // the player-level Passive XP Farming rig row rather than replacing it: that one pays
    // XP for hunger decay, this one raises what every source pays.
    int xpRatePct = 0;
};

}  // namespace mal
