// pvp_battle.h — turning two exchanged fighter specs into one identical fight.
//
// This is the piece that makes a linked duel work without broadcasting a single turn.
// Combat resolution is deterministic from a seed (combat.h), so two devices that build
// byte-identical Combatants and call begin() with the same seed independently play out
// the same fight, hit for hit, and reach the same winner. Everything here exists to
// make "byte-identical" true.
//
// THE HOST OWNS THE PLAYER SLOT, ON BOTH SCREENS. Combat is not a symmetric structure
// — it has a player_ and an enemy_ — so the two devices must agree on which fighter
// goes where, or they would resolve mirror-image fights. The rule is fixed:
// beginPvpBattle always seats the HOST as player_ (pvpHostIsLocal, core/net/pvp_link.h).
// The guest's screen therefore renders its own pet on the "enemy" row, which
// combat_screen.h's CombatSides relabelling exists to caption honestly. Sides are a
// display concern; resolution never flips.
//
// PURE + HOST-TESTABLE: no radio, no Game. Feed it two PvpFighter specs and a seed and
// it hands back a running Combat, which is exactly how the determinism is asserted in
// the native tests.
#pragma once

#include <cstdint>

#include "core/model/combat.h"
#include "core/net/pvp_link.h"

namespace mal {

class ContentRegistry;

// Rebuild a Combatant from a wire spec: resolve the species, moves and mods against
// the local registry, then fold in the level stat points (applyLevelStatPoints).
//
// Deliberately the SAME path for the local and the remote fighter. A device does not
// shortcut its own side from live state — it encodes its pet to a spec, and builds from
// that. Anything the spec cannot carry therefore cannot silently apply to one side
// only, which is what would otherwise desync the two screens.
//
// An unresolvable species yields a combatant with no moves and zero Health; callers
// check `canBuildPvpCombatant` first rather than fighting a hollow pet.
Combatant makePvpCombatant(const ContentRegistry& reg, const PvpFighter& f);

// Whether `f` names a species this build knows. False means the two devices are
// carrying different content and the duel must be refused, not attempted — the fights
// would diverge. (The rules-version check in the handshake catches the common case of
// mismatched builds; this catches the rest.)
bool canBuildPvpCombatant(const ContentRegistry& reg, const PvpFighter& f);

// Seat both fighters and start the fight. Host is always player_ (see the header note).
// Stakes are Safe — a first-cut duel costs nothing, so a loss must not touch
// Fragmentation — and the Exploit allowance is zero, because the A+C picker pauses the
// fight for a human and there is no way to pause the OTHER device's copy of it.
void beginPvpBattle(Combat& combat, const ContentRegistry& reg, const PvpFighter& host,
                    const PvpFighter& guest, uint32_t seed);

}  // namespace mal
