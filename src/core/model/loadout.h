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

// Un-equipped spares of one mod. A COUNT, not a list of instances: copies of a mod are
// interchangeable, because the equip-level gate belongs to the mod
// (ModDef::equipLevel) and not to the copy — the picker lists mods by type
// and never had a way to offer one copy over another. Ids are stable content-ids
// (borrowed), so the pool costs one pointer and one int per mod OWNED rather than per
// copy held. `count` is >= 1; a mod that reaches zero leaves the pool entirely.
struct OwnedMod {
    const char* id;
    int count;
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

    // Add one copy of `id` to the available pool (save restore / earn source), up to
    // `cap` copies of that one mod. Returns FALSE when the pool was already at the cap
    // and nothing was added — the caller decides what a drop with nowhere to go is
    // worth, because Loadout has no idea Bits exist. `cap` is the player's current MOD
    // STORAGE allowance (modCopyCap, tunables); a cap below 1 grants nothing.
    bool grant(const char* id, int cap);

    // CONSUME one available copy of `id` and install it permanently into `slot` (D3).
    // Copies are interchangeable, so which one goes is not a question. The mod previously in `slot` is
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
