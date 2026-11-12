// loadout.h: mods, as two records' parts (CONTRIBUTING, "Where state lives").
//
// ModSlots is the pet's: which mod is installed in each of its kModSlots slots. ModPool is
// the player's: the un-installed spares, shared by whichever pet is active. A mod is
// PERMANENT: installing it CONSUMES one spare from the pool, and it is never uninstalled,
// only OVERWRITTEN, and the overwritten one is discarded. Ids are stable content ids
// (borrowed from the content source, never copied).
#pragma once

#include <vector>

namespace mal {

// Un-installed spares of one mod. A count per mod, because copies are interchangeable:
// the equip-level gate belongs to the mod (ModDef::equipLevel). `count` is >= 1; a mod
// that reaches zero leaves the pool entirely.
struct OwnedMod {
    const char* id;
    int count;
};

class ModPool {
public:
    // At least one spare of `id` is held.
    bool owns(const char* id) const { return countOf(id) > 0; }
    const std::vector<OwnedMod>& owned() const { return owned_; }
    int countOf(const char* id) const;

    // Add one spare of `id`, up to `cap` copies of that mod. Returns false when the pool
    // was already at the cap and nothing was added: the caller decides what the drop is
    // worth, because the pool knows nothing of Bits. `cap` is the player's MOD STORAGE
    // allowance (modCopyCap, tunables); a cap below 1 grants nothing.
    bool grant(const char* id, int cap);

    // Spend one spare of `id`. False, and nothing changes, if none is held.
    bool take(const char* id);

    // The sample pool a new device starts with: two spares.
    static ModPool starting();

private:
    std::vector<OwnedMod> owned_;
};

class ModSlots {
public:
    // Installed mod id in `slot` (0..kModSlots-1), or nullptr if the slot is empty.
    const char* equipped(int slot) const;

    // The slot holding `id`, or -1 if it isn't installed (combat reads passives here).
    int slotOf(const char* id) const;

    // Install `id` into `slot`, spending one spare from `pool`. Whatever was in `slot` is
    // DISCARDED. Inert if `pool` holds no copy, or if `id` already occupies a DIFFERENT
    // slot (a mod holds one slot per pet, so a passive never stacks). The equip-LEVEL gate
    // is the caller's (Game, which knows the pet level).
    void install(int slot, const char* id, ModPool& pool);

    // Place `id` into `slot` without spending a spare: the restore path, since a save
    // stores the installed mods and the pool separately, and the way a fighter is built
    // with a known kit.
    void setEquipped(int slot, const char* id);

    // Empty every slot. The mods are not returned to any pool: installing consumed them.
    void resetSlots();

    // The sample kit a new device's first pet starts with: two mods installed.
    static ModSlots starting();

private:
    std::vector<const char*> slots_;  // size kModSlots; nullptr = empty
    void ensureSlots();
};

// What a MODS screen reads together: the active pet's slots and the player's pool.
struct ModHoldings {
    const ModSlots& slots;
    const ModPool& pool;
};

} // namespace mal
