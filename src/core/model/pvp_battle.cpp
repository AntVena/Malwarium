#include "core/model/pvp_battle.h"

#include "core/content/registry.h"
#include "core/model/loadout.h"
#include "core/model/move_loadout.h"

namespace mal {

bool canBuildPvpCombatant(const ContentRegistry& reg, const PvpFighter& f) {
    return f.creatureId[0] != '\0' && reg.creature(f.creatureId) != nullptr;
}

Combatant makePvpCombatant(const ContentRegistry& reg, const PvpFighter& f) {
    const CreatureDef* pet = f.creatureId[0] ? reg.creature(f.creatureId) : nullptr;
    if (!pet) return Combatant{};   // caller gates on canBuildPvpCombatant

    // Rehydrate the two loadouts, then hand them to the SAME makePlayerCombatant every
    // PVE fight uses. Going through the real builder rather than re-deriving stats here
    // is the whole point: a duel's two devices must apply the branch multipliers, the
    // stage scaling, the move fallbacks and every mod effect identically, and the only
    // way to guarantee that is for there to be one implementation.
    //
    // Equipping stores the id POINTER, so it must be the registry's stable copy — the
    // spec's own char arrays live on a transient wire struct.
    MoveLoadout moves;
    for (int i = 0; i < kMaxMoveSlots; ++i) {
        if (!f.moveIds[i][0]) continue;
        // An id this build doesn't know leaves the slot empty, which makePlayerCombatant
        // fills with the innate default — the same shape as an unequipped slot, so an
        // unknown move costs a slot's worth of kit instead of desyncing the fight.
        if (const MoveDef* m = reg.move(f.moveIds[i])) {
            moves.grant(m->id);
            moves.equip(i, m->id);
        }
    }

    // setEquipped, not equip: the remote player already paid for these mods on their own
    // device. This is a restore, and consuming from an empty spare pool would install
    // nothing.
    Loadout mods;
    for (int i = 0; i < kModSlots; ++i) {
        if (!f.modIds[i][0]) continue;
        if (const ModDef* m = reg.mod(f.modIds[i])) mods.setEquipped(i, m->id);
    }

    Combatant c = makePlayerCombatant(reg, *pet, moves, mods);
    const int statPoints[4] = {f.statPoints[0], f.statPoints[1], f.statPoints[2],
                               f.statPoints[3]};
    applyLevelStatPoints(c, statPoints);
    // The level is carried for display only (the combat screen's "Lv" readout) — a duel
    // pays no XP, so nothing scales off it.
    c.level = f.level;
    c.hasLevel = true;
    return c;
}

void beginPvpBattle(Combat& combat, const ContentRegistry& reg, const PvpFighter& host,
                    const PvpFighter& guest, uint32_t seed) {
    combat.begin(makePvpCombatant(reg, host), makePvpCombatant(reg, guest),
                 Combat::Stakes::Safe, seed, /*forceEnemyFirst=*/false,
                 /*carryPlayerHealth=*/-1, /*exploitUses=*/0);
}

}  // namespace mal
