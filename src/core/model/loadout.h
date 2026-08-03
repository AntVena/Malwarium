// loadout.h — the player's equipped mods (MODS). Mods are PERMANENT
// (D3): a mod is CONSUMED from the available inventory when equipped and
// lives permanently in its slot — it cannot be unequipped, only OVERWRITTEN by
// another mod, and the overwritten one is DISCARDED (never returned to inventory).
// Runtime state: which mod id sits in each of the kModSlots slots, plus the pool of
// still-available (un-equipped) mods. `owned_` is a multiset — a rare earn source
// may hand you a duplicate spare of a mod you've already installed. Ids are stable
// content ids (borrowed from the content source, never copied).
#pragma once

#include <vector>

namespace mal {

// An un-equipped spare in the pool. `reqLevel` is the PER-INSTANCE rolled required
// pet-level to equip it: a mod's power tier sets a nominal band, and each
// dropped copy rolls its own gate within ±50% of that band, so a lucky drop lets a
// lower-level pet field a stronger mod. Ids are stable content-ids (borrowed).
struct OwnedMod {
    const char* id;
    int reqLevel;
};

class Loadout {
public:
    // Equipped mod id in `slot` (0..kModSlots-1), or nullptr if the slot is empty.
    const char* equipped(int slot) const;

    // The slot holding `id`, or -1 if it isn't equipped (combat reads passives here).
    int slotOf(const char* id) const;

    // At least one un-equipped copy of `id` is available to equip.
    bool owns(const char* id) const;
    const std::vector<OwnedMod>& owned() const { return owned_; }

    // How many un-equipped spare copies of `id` are held.
    int countOf(const char* id) const;

    // The LOWEST rolled required level among held spares of `id` (the best copy the
    // player could field), or -1 if none is held. The equip gate compares this against
    // the pet's level; equip() then consumes that best copy.
    int reqLevelFor(const char* id) const;

    // Add one copy of `id` to the available pool (save restore / rare earn source).
    // Allows duplicates (a multiset). `reqLevel` is the rolled per-instance equip gate
    // (0 = freely equippable — the default for the starting seed + pre-v18 migration).
    void grant(const char* id, int reqLevel = 0);

    // CONSUME one available copy of `id` (the LOWEST-reqLevel spare — the best copy)
    // and install it permanently into `slot` (D3). The mod previously in `slot` is
    // DISCARDED — not returned to the pool. Inert if no copy is available, or if
    // `id` already occupies a DIFFERENT slot (a mod holds one slot per pet — no
    // stacking the same passive twice). No unequip. The equip-LEVEL gate is
    // enforced by the caller (Game, which knows the pet level).
    void equip(int slot, const char* id);

    // Restore-only: place `id` into `slot` WITHOUT consuming a copy from the pool
    // (the equipped mods were already consumed when first installed; a save stores
    // the slot occupants and the spare pool separately).
    void setEquipped(int slot, const char* id);

    // Uninstall every slot WITHOUT returning the mods to the spare pool — installed
    // mods are consumed permanently, so a pet swap doesn't refund them. Used before
    // installing a different pet's own slot contents (the spare pool stays shared).
    void resetSlots();

    // Seed (sample): two mods installed, two held as spares.
    static Loadout starting();

private:
    std::vector<OwnedMod> owned_;     // available (un-equipped) spares, multiset
    std::vector<const char*> slots_;  // size kModSlots; nullptr = empty
    void ensureSlots();
};

} // namespace mal
