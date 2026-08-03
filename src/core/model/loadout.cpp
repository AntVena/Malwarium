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

bool Loadout::owns(const char* id) const {
    if (!id) return false;
    for (const OwnedMod& m : owned_)
        if (std::strcmp(m.id, id) == 0) return true;
    return false;
}

int Loadout::reqLevelFor(const char* id) const {
    if (!id) return -1;
    int best = -1;
    for (const OwnedMod& m : owned_)
        if (std::strcmp(m.id, id) == 0)
            if (best < 0 || m.reqLevel < best) best = m.reqLevel;   // lowest = best copy
    return best;
}

int Loadout::countOf(const char* id) const {
    if (!id) return 0;
    int n = 0;
    for (const OwnedMod& m : owned_)
        if (std::strcmp(m.id, id) == 0) ++n;
    return n;
}

void Loadout::grant(const char* id, int reqLevel) {
    if (!id) return;
    if (reqLevel < 0) reqLevel = 0;
    owned_.push_back({id, reqLevel});       // multiset — duplicate spares allowed
}

void Loadout::equip(int slot, const char* id) {
    ensureSlots();
    if (slot < 0 || slot >= kModSlots || !id) return;
    // One slot per id per pet — refuse rather than let the same passive stack.
    for (int i = 0; i < kModSlots; ++i)
        if (i != slot && slots_[i] && std::strcmp(slots_[i], id) == 0) return;
    // Consume the LOWEST-reqLevel copy — the best one the player could field (D3:
    // consume-on-equip). Inert if none is held.
    auto best = owned_.end();
    for (auto it = owned_.begin(); it != owned_.end(); ++it)
        if (std::strcmp(it->id, id) == 0)
            if (best == owned_.end() || it->reqLevel < best->reqLevel) best = it;
    if (best == owned_.end()) return;
    owned_.erase(best);
    slots_[slot] = id;                      // any mod already here is DISCARDED (permanent)
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
    // sample: two spares held, freely equippable (reqLevel 0 — the seed isn't a
    // rolled drop; the earn path is what rolls a real equip-level gate).
    l.owned_ = {{"packet_sniffer", 0}, {"raid_mirror", 0}};
    l.ensureSlots();
    l.setEquipped(0, "firewall_patch");             // two installed permanently
    l.setEquipped(1, "clock_speed_boost");
    return l;
}

} // namespace mal
