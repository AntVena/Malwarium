// item_families.h: the item table, one entry per item FAMILY.
//
// kItemFamilies is the ONE place the item families and their order are declared:
// kItemCount is derived from it, so adding a family is "write its header, add one line
// here". `embedded_content.cpp` hands this list to the registry, which reads items
// family by family (ContentRegistry::item / allItems).
//
// Order is presentation only: the registry and the save address items by id. What a
// reorder changes is the order allItems() returns, which is the order the COLLECT shelf
// and the 'Pedia list items in.
//
// See ITEM_CONTENT_STANDARD.md for the folder shape and how to add a family.
#pragma once

#include "core/content/defs.h"
#include "core/content/items/dishes.h"
#include "core/content/items/kit.h"
#include "core/content/items/pantry.h"
#include "core/content/items/rare_kit.h"

namespace mal {

inline constexpr ItemFamily kItemFamilies[] = {
    {"kit", item_table::kKitItems, item_table::kKitItemsCount},
    {"pantry", item_table::kPantryItems, item_table::kPantryItemsCount},
    {"dishes", item_table::kDishItems, item_table::kDishItemsCount},
    {"rare_kit", item_table::kRareKitItems, item_table::kRareKitItemsCount},
};
inline constexpr int kItemFamilyCount = sizeof(kItemFamilies) / sizeof(kItemFamilies[0]);

// Every item in the build, across all families. Derived, never authored.
inline constexpr int totalItemCount() {
    int n = 0;
    for (const ItemFamily& f : kItemFamilies) n += f.count;
    return n;
}
inline constexpr int kItemCount = totalItemCount();

// Calls `fn(const ItemDef&)` on every row, family by family. For readers that need the
// table without a registry (a pure count over content, the dump tools).
template <class Fn>
void forEachItem(Fn&& fn) {
    for (const ItemFamily& f : kItemFamilies)
        for (int i = 0; i < f.count; ++i) fn(f.rows[i]);
}

}  // namespace mal
