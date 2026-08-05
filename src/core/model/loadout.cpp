#include "core/model/loadout.h"

#include <cstring>

#include "tunables.h"

namespace mal {

void Loadout::ensureSlots() {
    if (static_cast<int>(slots_.size()) != kModSlots)
        slots_.assign(kModSlots, nullptr);
}

const char* Loadout::equipped(int slot) const {
    if (slot < 0 || slot >= static_cast<int>(slots_.size())) return nullptr;
    return slots_[slot];
}

int Loadout::slotOf(const char* id) const {
    if (!id) return -1;
    for (int i = 0; i < static_cast<int>(slots_.size()); ++i)
        if (slots_[i] && std::strcmp(slots_[i], id) == 0) return i;
    return -1;
}

bool Loadout::owns(const char* id) const { return countOf(id) > 0; }

int Loadout::countOf(const char* id) const {
    if (!id) return 0;
    for (const OwnedMod& m : owned_)
        if (std::strcmp(m.id, id) == 0) return m.count;
    return 0;
}

bool Loadout::grant(const char* id, int cap) {
    if (!id || cap < 1) return false;
    for (OwnedMod& m : owned_)
        if (std::strcmp(m.id, id) == 0) {
            if (m.count >= cap) return false;   // full — the caller decides what that's worth
            ++m.count;
            return true;
        }
    owned_.push_back({id, 1});
    return true;
}

void Loadout::equip(int slot, const char* id) {
    ensureSlots();
    if (slot < 0 || slot >= kModSlots || !id) return;
    // One slot per id per pet — refuse rather than let the same passive stack.
    for (int i = 0; i < kModSlots; ++i)
        if (i != slot && slots_[i] && std::strcmp(slots_[i], id) == 0) return;
    // Consume one copy (D3: consume-on-equip). Inert if none is held. A mod spent down
    // to nothing leaves the pool rather than sitting in it at zero, so `owned_` stays a
    // list of what the player actually has — which is what the picker draws from.
    for (auto it = owned_.begin(); it != owned_.end(); ++it) {
        if (std::strcmp(it->id, id) != 0) continue;
        if (--it->count <= 0) owned_.erase(it);
        slots_[slot] = id;                  // any mod already here is DISCARDED (permanent)
        return;
    }
}

void Loadout::setEquipped(int slot, const char* id) {
    ensureSlots();
    if (slot < 0 || slot >= kModSlots || !id) return;
    slots_[slot] = id;                      // restore path: install without consuming
}

void Loadout::resetSlots() {
    ensureSlots();
    for (auto& s : slots_) s = nullptr;
}

Loadout Loadout::starting() {
    Loadout l;
    // sample: one spare each. Both are tier-1/5 rows whose own equip level is what
    // gates them — the seed grants copies, never a gate of its own.
    l.owned_ = {{"packet_sniffer", 1}, {"raid_mirror", 1}};
    l.ensureSlots();
    l.setEquipped(0, "firewall_patch");             // two installed permanently
    l.setEquipped(1, "clock_speed_boost");
    return l;
}

} // namespace mal
