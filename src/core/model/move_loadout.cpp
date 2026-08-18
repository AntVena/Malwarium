#include "core/model/move_loadout.h"

#include <cstring>

#include "core/content/registry.h"
#include "tunables.h"

namespace mal {

void MoveLoadout::ensureSlots() {
    if (static_cast<int>(slots_.size()) != kMaxMoveSlots)
        slots_.assign(kMaxMoveSlots, nullptr);
}

const char* MoveLoadout::equipped(int slot) const {
    if (slot < 0 || slot >= static_cast<int>(slots_.size())) return nullptr;
    return slots_[slot];
}

int MoveLoadout::slotOf(const char* id) const {
    if (!id) return -1;
    for (int i = 0; i < static_cast<int>(slots_.size()); ++i)
        if (slots_[i] && std::strcmp(slots_[i], id) == 0) return i;
    return -1;
}

bool MoveLoadout::owns(const char* id) const {
    for (const char* m : owned_)
        if (std::strcmp(m, id) == 0) return true;
    return false;
}

bool MoveLoadout::isInnate(const char* id) const {
    return id && defaultId_ && std::strcmp(id, defaultId_) == 0;
}

void MoveLoadout::grant(const char* id) {
    if (id && !owns(id)) owned_.push_back(id);
}

void MoveLoadout::equip(int slot, const char* id) {
    ensureSlots();
    // The innate is owned but never occupies a slot — it is what an EMPTY slot falls
    // back to, so slotting it would spend a slot on what that slot already does.
    if (slot < 0 || slot >= kMaxMoveSlots || !id || !owns(id) || isInnate(id)) return;
    const int prev = slotOf(id);            // a move lives in only one slot — move it
    if (prev >= 0) slots_[prev] = nullptr;
    slots_[slot] = id;
}

void MoveLoadout::unequip(int slot) {
    ensureSlots();
    if (slot < 0 || slot >= kMaxMoveSlots) return;
    slots_[slot] = nullptr;
}

int MoveLoadout::slotsForStage(Stage stage) {
    int n = kMoveSlotsByStage[stageIndex(stage)];
    if (n > kMaxMoveSlots) n = kMaxMoveSlots;
    return n;
}

Stage MoveLoadout::stageUnlockingSlot(int slot) {
    // First Stage whose unlocked count covers this slot index (the "unlocks at"
    // affordance). Daemon is the cap if nothing earlier grants it.
    for (int s = 0; s < 4; ++s)
        if (kMoveSlotsByStage[s] >= slot + 1) return static_cast<Stage>(s);
    return Stage::Daemon;
}

MoveLoadout MoveLoadout::starting() {
    MoveLoadout l;
    l.owned_ = {"quick_jab", "packet_storm", "checksum_guard", "fork_bomb"};
    l.ensureSlots();
    // Equip the one Attack move into slot 0; slot 1 is left to the Quick Jab per-slot
    // fallback. The Paypup line is Attack/Attack at Process (slotKinds), so the owned
    // DEFEND move (checksum_guard) can't sit in either Process slot — it stays OWNED
    // and equips into the first Defend slot the pet earns (slot 2 at Script); fork_bomb
    // stays owned for the player to field in the second Attack slot when they want more
    // punch than the jab.
    l.equip(0, "packet_storm");
    return l;
}

MoveLoadout MoveLoadout::startingForLine(const ContentRegistry& registry, const char* line) {
    MoveLoadout l;
    l.ensureSlots();
    l.grant(l.defaultId_);   // the innate is part of what every pet has
    const MoveDef* firstAttack = nullptr;
    if (line) {
        for (const MoveDef* m : registry.allMoves()) {
            if (!m->line || std::strcmp(m->line, line) != 0) continue;
            l.owned_.push_back(m->id);
            if (!firstAttack && m->kind == MoveDef::Kind::Attack) firstAttack = m;
        }
    }
    // Auto-equip the first owned Attack into slot 0 (mirrors starting()'s lean); an
    // empty slot already falls back to the innate Quick Jab, so this is a nicety,
    // not a correctness requirement.
    if (firstAttack) l.equip(0, firstAttack->id);
    return l;
}

} // namespace mal
