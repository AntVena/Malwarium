// pet.h: the Pet record (CONTRIBUTING, "Where state lives"). Everything about one
// creature, grouped by how long it lasts. Game holds the active pet as Game::pet_; ARCH's
// rack holds stored pets as SaveStoredPet.
//
// The part a field sits in decides what moving the pet does to it: storing a pet on the
// rack keeps the lifetime and current-life parts and clears PetArmed, since everything in
// it can be armed again.
#pragma once

#include <cstdint>
#include <vector>

#include "tunables.h"   // kLevelStatCount, kMaxMoveSlots
#include "core/content/areas/area_defs.h"   // kAreaCount, kSubAreasPerArea: the re-farm grid
#include "core/content/defs.h"
#include "core/model/loadout.h"
#include "core/model/move_loadout.h"
#include "core/model/pet_model.h"
#include "core/model/pet_upgrades.h"

namespace mal {

// A move slot's stamped kind: Unset until Game::stampSlotKinds first fills it, then
// locked for the pet's life.
enum class SlotKind : uint8_t { Unset, Attack, Defend };

// The USB port. A device is applied to the pet like an injection, so its patch is the
// pet's, and only an Eject-USB or a new egg removes it.
struct UsbPatch {
    // Bad-USB and Signed-USB write the same field, so the most recent one fires. Read
    // through Game::effectiveCareBranch wherever the care budget would pick a branch, and
    // consumed at the evolution it steers (completeEvolution).
    BranchOverride branch = BranchOverride::None;
    // Sandbox/Hypervisor-USB: 1 = none; 2 or 4 stretches this stage's evolution dwell
    // (evolveDwellMs) and multiplies every XP award (scaleXp) by that much. Armable at
    // Process only and spent at the evolution it stretched. While above 1 the port is
    // full and no USB item is usable (itemUsable).
    int soakFactor = 1;
    // Halt-USB: evolveEligible() is false while it is in. Never consumed at a boundary,
    // because none arrives while it holds.
    bool hold = false;
};

// Once-per-life item grants, and the shield one of them arms (save v21).
struct PetGrants {
    bool shieldActive = false;   // Restore Point: blocks the next care mistake (addCareMistake)
    bool shieldSpent = false;    // the Restore Point has been used on this pet
    bool yubiSpent = false;      // the Yubi-Cookie has been used on this pet
};

// Effects armed on the pet for now, by items a player can use again.
struct PetArmed {
    // Ambig-USB (save v28): guarantees the next Process->Script Trojan divert
    // (fireEvolution), consumed there whether or not the pet has a divert target.
    bool trojanDivert = false;
    // Backup Drive (save v30): the lifetimeUptimeMs() deadline the death-save is armed
    // until, 0 = off. A fight that spends it clears it early; using the item again
    // overwrites it. A Sim Battle fights without it.
    uint32_t backupShieldUntilMs = 0;
    // The DeepWeb Dive arming. The multiplier (Deep-Learning Module/Core, x2/x4) resets
    // wherever the dive streak does and does not stack. The start depth is armed by a
    // Bell and consumed when startDeepWebDive begins a fresh dive: -1 = none, or
    // Game::kDeepWebStartDepthUseBest for the Checkpoint Bell.
    int deepWebDepthMultiplier = 1;
    int deepWebStartDepth = -1;
    // DIVE PAY (ItemEffect DiveStartBonusPct / DiveStepBonusPct). The Bell's half waits in
    // pendingDiveStartBonusPct until its dive begins; Game::clearDiveDepthBuffs spends
    // the two live halves wherever a dive ends.
    int pendingDiveStartBonusPct = 0;
    int diveStartBonusPct = 0;
    int diveStepBonusPct = 0;
    // Stacking foods (Polltatoes) eaten since the pet last lost a Hunger point to decay.
    // The next decay tick ends the run, so it is not saved.
    int stackingFoodRun = 0;
    // The Lockout: Hunger hit zero and the modal is counting down.
    bool lockoutActive = false;
    uint32_t lockoutDeadlineMs = 0;
    bool lockoutReminded = false;
    // Post-clear wild wins per sub-area: the decay input for its diminishing drop chances
    // (refarmDropScalePct). Saved for the active pet (save v15), so each pet finds a
    // cleared area an undepleted training ground.
    uint16_t subRefarmCount[kAreaCount][kSubAreasPerArea] = {};
};

struct Pet {
    // Null between an ARCH Store and the next egg, and while a line is being chosen.
    const CreatureDef* creature = nullptr;
    const EggLineDef* line = nullptr;   // the egg line that laid it
    PetModel model;

    // --- Kept for life ----------------------------------------------------------
    PetUpgrades upgrades;
    // Every distinct food it has been fed (save v62), written where a dish is eaten
    // (startFeeding, via markFoodEaten): holding a dish is not tasting it.
    std::vector<const ItemDef*> foodsEaten;
    PetGrants grants;
    UsbPatch usb;
    // Its deepest DeepWeb Dive (save v35), which the Checkpoint Bell targets, and its
    // deepest DarkWeb Crawl, kept apart because a crawl depth is worth more.
    int bestDeepWebDepth = 0;
    int bestDarkWebDepth = 0;
    int generation = 0;    // its ordinal among every egg laid
    int defragCount = 0;   // successful defrags (save v16)

    // --- Progress through this life ---------------------------------------------
    int level = 0;   // always the sum of statPoints
    int xp = 0;      // banked toward the next level
    // Earned combat-stat points: 0 power · 1 defense · 2 speed · 3 max-Health. A level-up
    // adds one at random; Rollback sheds one and Repartition moves one (save v11).
    int statPoints[kLevelStatCount] = {0};
    SlotKind slotKinds[kMaxMoveSlots] = {SlotKind::Unset, SlotKind::Unset, SlotKind::Unset,
                                         SlotKind::Unset};
    MoveLoadout moves;
    Loadout mods;
    uint32_t stageEnteredMs = 0;      // game-ms the current stage began
    uint32_t bootHatchRemainMs = 0;   // the egg's incubation clock (save v9)
    bool eggOfferMade = false;        // the egg was offered an accelerator; not saved
    // The 5/5 dying window. The anchor is in game-ms and means nothing across a boot, so
    // only the accumulated elapsed time is saved (save v42).
    bool dyingArmed = false;
    uint32_t dyingEnteredMs = 0;
    uint32_t dyingElapsedMs = 0;
    uint32_t failingAlertAtMs = 0;    // in dyingElapsedMs time

    // --- Armed for now -----------------------------------------------------------
    PetArmed armed;
};

}  // namespace mal
