// mod_state.h — the live per-fight state of a pet's equipped MOD passives.
//
// A Combatant carries one ModStateSet holding an entry per ModEffect kind that needs
// state for the duration of a fight (a cap, a threshold pair, a one-shot's remaining
// use). Hooks in the damage pipeline read their own kind out of it — `mods.mag(
// ModEffect::MaxHitCapPct)`, `mods.spend(ModEffect::RaidMirror)` — so a mod's per-fight
// state lives under the same vocabulary content declares it with (ModDef::effectKind,
// core/content/defs.h) instead of a bespoke Combatant member per passive.
//
// Adding a passive mod is therefore: a content row, a ModRule row in mod_state.cpp
// saying how duplicate copies fold together, and the one hook that reads it.
#pragma once

#include <cstdint>

#include "core/content/defs.h"   // ModEffect + kModSlots (via tunables.h)

namespace mal {

// One live passive. `mag`/`mag2` are the aggregated ModDef::magnitude/magnitude2 — the
// same arity content declares, so a kind can never need a number it has no way to
// author. `pending` is the entry's own mutable counter for the kinds that have one.
struct ModState {
    ModEffect kind = ModEffect::None;
    int mag = 0;
    int mag2 = 0;
    int pending = 0;   // a one-shot's remaining uses; a deferred queue's outstanding
                       // amount (Load Balancer). 0 for kinds that need neither.
};

// How a second equipped copy of the same kind folds into the live entry. The rule is a
// property of the MECHANIC, not of any one mod row, which is why it lives in the rule
// table rather than on ModDef.
enum class ModCombine : uint8_t {
    Arm,        // magnitude-free — the passive is simply on, a second copy adds nothing
    Sum,        // magnitudes accumulate
    HighestMag, // strongest primary wins outright, carrying its own mag2
    LowestMag,  // tightest primary wins outright, carrying its own mag2
    HighestMag2,// strongest SECONDARY wins outright, carrying its own mag
    Replace,    // the last slot walked defines the entry
};

// The per-kind combine contract. `capMag` ceilings the primary magnitude (0 =
// uncapped); `oneShot` gives a fresh entry a single `pending` use for spend() to burn.
struct ModRule {
    ModEffect kind;
    ModCombine combine;
    int capMag;
    bool oneShot;
};

// The rule for `kind`, or nullptr when the kind keeps no per-fight entry — the stat
// kinds (PowerPct, MaxHealth, …) fold straight into the Combatant's base stats, and the
// post-battle kinds (PostBattleBits, FatigueFragCut) are read by the Game off ModDef.
const ModRule* modRule(ModEffect kind);

// At most one entry per kind, so the kModSlots equipped mods can never overflow it.
class ModStateSet {
public:
    ModState* find(ModEffect kind);
    const ModState* find(ModEffect kind) const;

    // Aggregated magnitudes, 0 when the kind isn't live — so a hook's own `> 0` guard
    // is the whole "is this mod equipped?" check.
    int mag(ModEffect kind) const;
    int mag2(ModEffect kind) const;

    // Whether a one-shot still has a use left — for a hook that has to TEST the passive
    // before deciding to consume it.
    bool armed(ModEffect kind) const;

    // Burn one pending use of a one-shot. True only if there was one to burn, so it
    // doubles as the "is it still armed?" test at the hook.
    bool spend(ModEffect kind);

    // Arm `kind` directly at resolved values, bypassing the equip loop's aggregation —
    // how the Game arms a passive from outside the mod table and how tests stage one.
    // A one-shot kind comes up with its single pending use.
    void arm(ModEffect kind, int mag = 0, int mag2 = 0);

    // Fold one equipped mod's magnitudes in per its ModRule. A kind with no rule (a
    // base-stat or post-battle effect) is ignored.
    void apply(ModEffect kind, int mag, int mag2);

    int size() const { return n_; }
    const ModState& at(int i) const { return e_[i]; }

private:
    ModState* slot(ModEffect kind);   // existing entry, or a fresh one (nullptr if full)
    ModState e_[kModSlots];
    int n_ = 0;
};

}  // namespace mal
