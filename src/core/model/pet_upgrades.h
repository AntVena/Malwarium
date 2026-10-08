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
//
// The FAVOURITE DISH rides here too, though no item grants it: it is rolled once per
// pet from every Rare-or-better food (Game::ensureFavouriteFood), hidden until the pet
// is fed it, and from then on is a standing stat grant like the dishes' — so it travels
// through the rack, the save and the BUFFS page on exactly the same path.
#pragma once

#include "tunables.h"   // kLevelStatCount — the combat-stat axis statBonus is indexed on
#include "core/content/defs.h"

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
    // This pet's favourite dish (nullptr = not rolled yet) and whether it has been fed
    // it. Once found it adds kFavouriteFoodStatBonus to every combat stat, counted with
    // statBonus in combat but kept apart from it, so it never closes an Epic dish's
    // once-per-life gate (lifetimeGrantSpent below reads statBonus alone).
    const ItemDef* favouriteFood = nullptr;
    bool favouriteFound = false;
};

// Points the favourite dish adds to each combat stat once the pet has been fed it.
inline constexpr int kFavouriteFoodStatBonus = 3;

// Can `d` be rolled as a pet's favourite? Every Food of Rare rarity or better, so a
// favourite is something worth finding rather than the first snack in the bag — and
// only one the ITEMS flow actually feeds (Game::useItem's Food branch), so it can be
// found at all.
inline bool favouriteFoodEligible(const ItemDef& d) {
    return d.type == ItemDef::Type::Food && d.use == ItemDef::Use::Consume &&
           itemCategory(d) == ItemDef::Category::Food && d.rarity >= ItemDef::Rarity::Rare;
}

// Every ONCE-PER-PET gate a content row can be held to, in one value: the permanent
// grants above, plus the two that arm rather than upgrade (a Yubi-Cookie's care-mistake
// wipe and a Restore Point's shield — Game holds those as their own flags, because
// unlike the upgrades they do not travel with the pet through the rack).
//
// It exists so ONE function can answer "has this pet already taken what that row grants"
// for the engine, the ITEMS screens and the 'Pedia alike. Game::petLifetimeGates() fills
// it; nothing else should need to know which flag belongs to which item.
struct PetLifetimeGates {
    PetUpgrades upgrades;
    bool careMistakeWipeSpent = false;   // ItemEffect::Kind::RemoveCareMistakeOnce
    bool mistakeShieldSpent = false;     // ItemEffect::Kind::ClearMistakeShieldOnce
};

// Has this pet already spent what `d` grants once in a life? False for any row carrying
// no such effect — there is nothing to have spent — which is why a caller that wants to
// DISPLAY the fact asks itemIsOncePerPetLifetime(d) first: "no grant" and "grant already
// taken" are different things and must never read the same.
inline bool lifetimeGrantSpent(const ItemDef& d, const PetLifetimeGates& g) {
    for (const ItemEffect& e : d.effects) {
        switch (e.kind) {
            case ItemEffect::Kind::RemoveCareMistakeOnce:
                if (g.careMistakeWipeSpent) return true;
                break;
            case ItemEffect::Kind::ClearMistakeShieldOnce:
                if (g.mistakeShieldSpent) return true;
                break;
            case ItemEffect::Kind::BandwidthRegenBonusMin:
                if (g.upgrades.bandwidthRegenMin > 0) return true;
                break;
            case ItemEffect::Kind::StatPointPower:
            case ItemEffect::Kind::StatPointDefense:
            case ItemEffect::Kind::StatPointSpeed:
            case ItemEffect::Kind::StatPointHealth: {
                const int stat = statPointEffectIndex(e.kind);
                if (stat >= 0 && g.upgrades.statBonus[stat] > 0) return true;
                break;
            }
            case ItemEffect::Kind::XpRateBonusPct:
                if (g.upgrades.xpRatePct > 0) return true;
                break;
            default:
                break;
        }
    }
    return false;
}

}  // namespace mal
