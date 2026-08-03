// inventory.h — the player's consumable stacks, keyed by item id.
//
// Runtime state (quantities), distinct from the static ItemDef content. The
// ITEMS submenu reads this against the ContentRegistry to build its grouped
// list; feeding/buffs/Lockout resolution decrement through it. Ids are the
// stable content ids (string literals owned by the content source), so this
// holds borrowed `const char*` pointers and never copies the id text.
#pragma once

#include <vector>

namespace mal {

class Inventory {
public:
    struct Stack {
        const char* id;  // stable content id (borrowed)
        int qty;
    };

    int count(const char* id) const;
    bool has(const char* id) const { return count(id) > 0; }

    void add(const char* id, int n = 1);
    // Remove up to `n`; returns true if the full amount was removed.
    bool remove(const char* id, int n = 1);

    const std::vector<Stack>& stacks() const { return stacks_; }

    // Starting inventory (matches the sample shelf).
    static Inventory starting();

private:
    Stack* find(const char* id);
    const Stack* find(const char* id) const;
    std::vector<Stack> stacks_;
};

} // namespace mal
