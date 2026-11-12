#include "core/model/loadout.h"

#include <cstring>

#include "tunables.h"

namespace mal {

int ModPool::countOf(const char* id) const {
    if (!id) return 0;
    for (const OwnedMod& m : owned_)
        if (std::strcmp(m.id, id) == 0) return m.count;
    return 0;
}

bool ModPool::grant(const char* id, int cap) {
    if (!id || cap < 1) return false;
    for (OwnedMod& m : owned_)
        if (std::strcmp(m.id, id) == 0) {
            if (m.count >= cap) return false;   // full: the caller decides what that's worth
            ++m.count;
            return true;
        }
    owned_.push_back({id, 1});
    return true;
}

bool ModPool::take(const char* id) {
    if (!id) return false;
    // A mod spent down to zero leaves the pool, so `owned_` stays a list of what the
    // player actually has, which is what the picker draws from.
    for (auto it = owned_.begin(); it != owned_.end(); ++it) {
        if (std::strcmp(it->id, id) != 0) continue;
        if (--it->count <= 0) owned_.erase(it);
        return true;
    }
    return false;
}

ModPool ModPool::starting() {
    // One spare each. Packet Sniffer is the one mod authored at equipLevel 0
    // (content_mods.cpp): a fresh pet has to be able to install SOMETHING the moment it
    // hatches, so retuning that row's gate upward breaks the first thing a player does
    // with this screen. RAID Mirror is a deep row shown early, to be grown into.
    ModPool p;
    p.owned_ = {{"packet_sniffer", 1}, {"raid_mirror", 1}};
    return p;
}

void ModSlots::ensureSlots() {
    if (static_cast<int>(slots_.size()) != kModSlots)
        slots_.assign(kModSlots, nullptr);
}

const char* ModSlots::equipped(int slot) const {
    if (slot < 0 || slot >= static_cast<int>(slots_.size())) return nullptr;
    return slots_[slot];
}

int ModSlots::slotOf(const char* id) const {
    if (!id) return -1;
    for (int i = 0; i < static_cast<int>(slots_.size()); ++i)
        if (slots_[i] && std::strcmp(slots_[i], id) == 0) return i;
    return -1;
}

void ModSlots::install(int slot, const char* id, ModPool& pool) {
    ensureSlots();
    if (slot < 0 || slot >= kModSlots || !id) return;
    for (int i = 0; i < kModSlots; ++i)
        if (i != slot && slots_[i] && std::strcmp(slots_[i], id) == 0) return;
    if (pool.take(id)) slots_[slot] = id;   // any mod already here is DISCARDED
}

void ModSlots::setEquipped(int slot, const char* id) {
    ensureSlots();
    if (slot < 0 || slot >= kModSlots || !id) return;
    slots_[slot] = id;
}

void ModSlots::resetSlots() {
    ensureSlots();
    for (auto& s : slots_) s = nullptr;
}

ModSlots ModSlots::starting() {
    ModSlots s;
    s.setEquipped(0, "firewall_patch");
    s.setEquipped(1, "clock_speed_boost");
    return s;
}

} // namespace mal
