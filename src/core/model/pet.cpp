#include "core/model/pet.h"

#include <algorithm>
#include <cstring>
#include <utility>

#include "core/content/registry.h"
#include "core/model/save.h"

namespace mal {

namespace {

void copyId(char* out, const char* id) {
    if (id) std::strncpy(out, id, kSaveIdCap - 1);
}

}  // namespace

SaveStoredPet Pet::stored(uint32_t nowMs) const {
    SaveStoredPet p;
    copyId(p.id, creature ? creature->id : nullptr);
    p.hunger = model.hunger();
    p.frag = model.fragmentation();
    p.happy = model.happiness();
    p.mistakes = model.careMistakes();
    p.debuffs = model.debuffs();
    p.ghost = model.hasGhost() ? 1 : 0;

    p.generation = generation;
    p.defragCount = defragCount;
    p.bestDeepWebDepth = bestDeepWebDepth;
    p.bestDarkWebDepth = bestDarkWebDepth;
    p.bandwidthRegenBonusMin = upgrades.bandwidthRegenMin;
    for (int i = 0; i < kLevelStatCount; ++i) p.statBonus[i] = upgrades.statBonus[i];
    p.xpRateBonusPct = upgrades.xpRatePct;
    for (int k = 0; k < kBondSources; ++k)
        p.bondBp[k] = static_cast<uint16_t>(upgrades.bondBp[k]);
    if (upgrades.favouriteFood) copyId(p.favouriteFood, upgrades.favouriteFood->id);
    p.favouriteFound = upgrades.favouriteFound ? 1 : 0;
    // Reserved to the exact count, as Game::captureSave's collections are, so building a
    // save does not leave the heap in pieces.
    p.foodsEaten.reserve(foodsEaten.size());
    for (const ItemDef* d : foodsEaten) {
        SaveId id;
        copyId(id.id, d->id);
        p.foodsEaten.push_back(id);
    }
    p.shieldActive = grants.shieldActive ? 1 : 0;
    p.shieldSpent = grants.shieldSpent ? 1 : 0;
    p.yubiSpent = grants.yubiSpent ? 1 : 0;
    p.usbBranch = static_cast<uint8_t>(usb.branch);
    p.usbSoak = static_cast<uint8_t>(usb.soakFactor);
    p.usbHold = usb.hold ? 1 : 0;

    p.combatLevel = level;
    p.combatXp = xp;
    for (int i = 0; i < kLevelStatCount; ++i) p.statPoints[i] = statPoints[i];
    for (int i = 0; i < kMaxMoveSlots; ++i) p.slotKinds[i] = static_cast<uint8_t>(slotKinds[i]);
    p.ownedMoves.reserve(moves.owned().size());
    for (const char* mv : moves.owned()) {
        SaveId id;
        copyId(id.id, mv);
        p.ownedMoves.push_back(id);
    }
    for (int i = 0; i < kMaxMoveSlots; ++i) copyId(p.equippedMoves[i].id, moves.equipped(i));
    for (int i = 0; i < kModSlots; ++i) copyId(p.equippedMods[i].id, mods.equipped(i));
    p.timeInStageMs = nowMs - stageEnteredMs;
    p.bootHatchRemainMs = bootHatchRemainMs;
    p.dyingElapsedMs = dyingElapsedMs;
    return p;
}

Pet Pet::thawed(const SaveStoredPet& s, const ContentRegistry& reg, uint32_t nowMs) {
    Pet p;
    p.creature = reg.creature(s.id);
    // The line is only read while the pet is an incubating egg, so it is derived for one
    // and left null otherwise.
    p.line = p.creature && p.creature->stage == Stage::BootSector
                 ? reg.eggLineForCreature(p.creature->id)
                 : nullptr;
    p.model.setHunger(s.hunger);
    p.model.setFragmentation(s.frag);
    p.model.setHappiness(s.happy);
    p.model.setCareMistakes(s.mistakes);
    p.model.setDebuffs(s.debuffs);
    p.model.setGhost(s.ghost != 0);

    p.generation = s.generation;
    p.defragCount = s.defragCount;
    p.bestDeepWebDepth = s.bestDeepWebDepth;
    p.bestDarkWebDepth = s.bestDarkWebDepth;
    p.upgrades.bandwidthRegenMin = s.bandwidthRegenBonusMin;
    for (int i = 0; i < kLevelStatCount; ++i) p.upgrades.statBonus[i] = s.statBonus[i];
    p.upgrades.xpRatePct = s.xpRateBonusPct;
    for (int k = 0; k < kBondSources; ++k)
        p.upgrades.bondBp[k] = std::min<int>(s.bondBp[k], kBondSourceCapBp);
    p.upgrades.favouriteFood = reg.item(s.favouriteFood);
    p.upgrades.favouriteFound = s.favouriteFound != 0;
    for (const SaveId& id : s.foodsEaten)
        if (const ItemDef* d = reg.item(id.id)) p.foodsEaten.push_back(d);
    p.grants.shieldActive = s.shieldActive != 0;
    p.grants.shieldSpent = s.shieldSpent != 0;
    p.grants.yubiSpent = s.yubiSpent != 0;
    // A branch number this build has no override for reads as none.
    p.usb.branch = s.usbBranch <= static_cast<uint8_t>(BranchOverride::Bad)
                       ? static_cast<BranchOverride>(s.usbBranch)
                       : BranchOverride::None;
    p.usb.soakFactor = s.usbSoak < 1 ? 1 : s.usbSoak;
    p.usb.hold = s.usbHold != 0;

    p.level = s.combatLevel;
    p.xp = s.combatXp;
    for (int i = 0; i < kLevelStatCount; ++i) p.statPoints[i] = s.statPoints[i];
    for (int i = 0; i < kMaxMoveSlots; ++i) p.slotKinds[i] = static_cast<SlotKind>(s.slotKinds[i]);
    // An empty owned list marks a record with no move state (a pre-v29 rack pet), which
    // seeds its line's starting kit. Otherwise the innate move is granted first, since an
    // older record may not list it and every pet owns it.
    if (!s.ownedMoves.empty()) {
        p.moves.grant(p.moves.defaultMove());
        for (const SaveId& mv : s.ownedMoves)
            if (const MoveDef* def = reg.move(mv.id)) p.moves.grant(def->id);
        for (int i = 0; i < kMaxMoveSlots; ++i)
            if (const MoveDef* def = reg.move(s.equippedMoves[i].id)) p.moves.equip(i, def->id);
    } else {
        p.moves = MoveLoadout::startingForLine(reg, p.creature ? p.creature->line : nullptr);
    }
    p.mods.resetSlots();
    for (int i = 0; i < kModSlots; ++i)
        if (const ModDef* def = reg.mod(s.equippedMods[i].id)) p.mods.setEquipped(i, def->id);
    p.stageEnteredMs = nowMs - s.timeInStageMs;
    p.bootHatchRemainMs = s.bootHatchRemainMs;
    // The dying window resumes from its accumulated time; dyingArmed stays false so the
    // next tick re-anchors it against the current clock.
    p.dyingElapsedMs = s.dyingElapsedMs;
    return p;
}

Pet Pet::laid(const EggLineDef* line, MoveLoadout moves, ModSlots mods, uint32_t nowMs) {
    Pet p;
    p.line = line;
    p.moves = std::move(moves);
    p.mods = std::move(mods);
    p.stageEnteredMs = nowMs;
    p.bootHatchRemainMs = kBootHatchMs;
    return p;
}

SaveArmedPet PetArmed::saved() const {
    SaveArmedPet s;
    s.trojanDivert = trojanDivert ? 1 : 0;
    s.backupShieldUntilMs = backupShieldUntilMs;
    s.subRefarm.reserve(static_cast<size_t>(kAreaCount) * kSubAreasPerArea);
    for (const auto& row : subRefarmCount)
        for (uint16_t c : row) s.subRefarm.push_back(c);
    return s;
}

PetArmed PetArmed::loaded(const SaveArmedPet& s) {
    PetArmed a;
    a.trojanDivert = s.trojanDivert != 0;
    a.backupShieldUntilMs = s.backupShieldUntilMs;
    for (int i = 0; i < kAreaCount; ++i)
        for (int j = 0; j < kSubAreasPerArea; ++j) {
            const size_t idx = static_cast<size_t>(i) * kSubAreasPerArea + j;
            a.subRefarmCount[i][j] = idx < s.subRefarm.size() ? s.subRefarm[idx] : 0;
        }
    return a;
}

}  // namespace mal
