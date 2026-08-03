// move_loadout.h — the pet's equipped combat moves (TRAIN).
//
// Mirrors Loadout (MODS), with two combat-specific twists:
//  * an innate DEFAULT move that lives OUTSIDE the equip slots and is always
// available (so a fresh Boot pet is never actionless);
// * the number of USABLE slots grows with each evolution — slots are the
//    mechanical reward for raising a pet. The model keeps kMaxMoveSlots cells; the
//    unlocked prefix (slotsForStage) is what the pet can actually fight with.
// A move occupies only one slot — equipping it elsewhere MOVES it. Ids are stable
// content ids (borrowed from the content source, never copied).
#pragma once

#include <vector>

#include "core/content/defs.h"  // Stage

namespace mal {

class ContentRegistry;

class MoveLoadout {
public:
    // The always-available innate move id (outside the equip slots).
    const char* defaultMove() const { return defaultId_; }

    // Equipped move id in `slot` (0..kMaxMoveSlots-1), or nullptr if empty.
    const char* equipped(int slot) const;
    // The slot holding `id`, or -1 if it isn't equipped.
    int slotOf(const char* id) const;

    bool owns(const char* id) const;
    const std::vector<const char*>& owned() const { return owned_; }

    // Add `id` to the owned set if not already owned (save restore / future earn
    // source). `id` is a stable content-id pointer (borrowed, never copied).
    void grant(const char* id);

    // Equip `id` into `slot`, vacating any slot it already occupied (move).
    void equip(int slot, const char* id);
    // Empty a slot (the "— empty (unequip) —" picker option).
    void unequip(int slot);

    // How many slots a pet at `stage` has unlocked, clamped to kMaxMoveSlots.
    static int slotsForStage(Stage stage);
    // The Stage at which `slot` first becomes available (the "unlocks at" label).
    static Stage stageUnlockingSlot(int slot);

    // Seed (sample): owns the 3 generic roster moves, default =
    // Quick Jab, two equipped (one attack + one defend → an even lean). Generic
    // fixture for tests exercising loadout/combat mechanics that don't care which
    // line is involved — real gameplay uses startingForLine() instead (below).
    static MoveLoadout starting();

    // A pet's moves are its line's nature, not a lucky drop: a freshly hatched or
    // reset pet owns every move belonging to its own `line` (nullptr grants none).
    // Moves gated to a higher stage than the pet has reached stay owned-but-locked,
    // same as any other stage-gated move. The GENERIC pool (quick_jab aside) is no
    // longer part of anyone's starter kit — it's earned exclusively via the rare
    // wild-encounter move-drop roll, so a player who wants Buffer Overflow etc. has
    // to go tame a malbeast for it.
    static MoveLoadout startingForLine(const ContentRegistry& registry, const char* line);

private:
    const char* defaultId_ = "quick_jab";
    std::vector<const char*> owned_;
    std::vector<const char*> slots_;  // size kMaxMoveSlots; nullptr = empty
    void ensureSlots();
};

} // namespace mal
