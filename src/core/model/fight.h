// fight.h: one fight, a run (CONTRIBUTING, "Where state lives"). From the moment a fight
// opens (a wild encounter, a gauntlet round, PRACTISE, an arena bout, a linked duel) until
// finishCombat hands it back to whoever opened it. Combat is the turn engine; this adds
// the clocks the device paces it with and the result the post-encounter readout shows.
// Runtime-only: a reboot ends it, and the save keeps nothing of it.
//
// Game holds the last fight after it ends: the post-encounter readout draws its result
// once finishCombat has moved on (after a loss has already ended the walk), and
// Game::combat() still answers for the verdict a caller just dismissed. The next start
// replaces it whole, so no fight inherits a clock or a result from the one before.
//
// The combat stage's own motion (the sprite anim clock and the FX_CAMO disguise) is not
// here: it lives while the Combat screen is up, which spans a gauntlet's rounds, and it
// clears when the screen closes.
#pragma once

#include <cstdint>

#include "core/model/combat.h"
#include "tunables.h"

namespace mal {

// Who opened the fight, so finishCombat returns there: the LOADOUT hub (Sim), the
// explore habitat (Wild), the gauntlet's next round (Boss), the bracket (Tourney) or
// the LINK screen (Duel).
enum class FightCaller { Sim, Wild, Boss, Tourney, Duel };

// What applyCombatResult measured around the reward path, for the post-encounter readout
// (Nav::PostEncounter): Bandwidth and Fragmentation just before and just after, whether a
// Bandwidth charge shielded the fatigue tax, and the stat points and level this fight's
// XP reached. Filled for a wild or Sim fight; only a wild one is shown.
struct FightResult {
    int bwBefore = 0;
    int bwAfter = 0;
    int fragBefore = 0;
    int fragAfter = 0;
    bool shielded = false;
    int statGain[kLevelStatCount] = {0};
    int levelAfter = 0;
    // The readout's auto-dismiss deadline (kPostEncounterMs), in real ms like
    // kSdIconRevealMs rather than in beats.
    uint32_t readoutUntilMs = 0;
};

struct Fight {
    Combat combat;
    FightCaller caller = FightCaller::Sim;
    // Heartbeats since the last autonomous turn: paces auto-battle (combatBeatsForTurn).
    int turnBeat = 0;
    // Heartbeats the verdict has been held, which is what auto-dismisses a hands-off
    // walk's fight (kExploreRevealHoldBeats).
    int resultBeat = 0;
    // Anim ticks (kCombatAnimMs) since the last Combat::step: drawCombat's impact punch,
    // flash and damage pop decay off it.
    int hitBeat = 0;
    FightResult result;

    // The caller then opens `combat` (Combat::begin, or beginPvpBattle for a duel).
    static Fight started(FightCaller caller) {
        Fight f;
        f.caller = caller;
        return f;
    }
};

}  // namespace mal
