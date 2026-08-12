#include "core/model/inventory.h"

#include <cstring>

namespace mal {

Inventory::Stack* Inventory::find(const char* id) {
    for (auto& s : stacks_)
        if (std::strcmp(s.id, id) == 0) return &s;
    return nullptr;
}
const Inventory::Stack* Inventory::find(const char* id) const {
    for (const auto& s : stacks_)
        if (std::strcmp(s.id, id) == 0) return &s;
    return nullptr;
}

int Inventory::count(const char* id) const {
    const Stack* s = find(id);
    return s ? s->qty : 0;
}

void Inventory::add(const char* id, int n) {
    if (n <= 0) return;
    if (Stack* s = find(id)) s->qty += n;
    else stacks_.push_back({id, n});
}

bool Inventory::remove(const char* id, int n) {
    Stack* s = find(id);
    if (!s || s->qty < n) return false;
    s->qty -= n;
    return true;
}

Inventory Inventory::starting() {
    Inventory inv;
    // The sample shelf — enough to exercise FOOD / BUFFS / QUEST groups.
    inv.add("airgap_snack", 3);
    inv.add("tortilla_chip", 2);
    inv.add("yubi_cookie", 1);
    inv.add("restore_point", 1);  // care-mistake shield (save v21)
    inv.add("decrypt_key", 1);
    inv.add("backup_drive", 1);
    inv.add("boot_accelerator", 1);   // egg decrypt accelerator
    return inv;
}

} // namespace mal
