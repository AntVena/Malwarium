# Malwarium: Item Content Standard

How the item table is laid out, so opening "the pantry" or "the dishes" means opening one file:
the same role [`../creatures/CREATURE_CONTENT_STANDARD.md`](../creatures/CREATURE_CONTENT_STANDARD.md)
plays for creatures. How a row READS (the effect vocabulary, magnitudes on the row, the
description voice) is still [`../CONTENT_STANDARD.md`](../CONTENT_STANDARD.md).

## The shape

One family = one `ItemFamily` = one header under `src/core/content/items/` defining its rows:

```
src/core/content/items/
  item_families.h   ItemFamily list + kItemCount + forEachItem()
  pools.h           the cache pools and the walk's kLootPool
  kit.h             Common/Uncommon keys, bells, caches and the snacks outside the pantry
  pantry.h          the raw staples the MERGE HUB cooks with
  dishes.h          every MERGE HUB output
  rare_kit.h        Rare/Epic shields, bells, dive modules, caches, chips, warp keys, USBs
```

A family is what a reader opens together. Food is filed by what it IS (a staple, a cooked dish)
rather than by rarity, because the tier only says how much a pet gets out of eating one. Everything
else is filed by rarity, kit then rare kit.

The rows are `inline constexpr` arrays in headers for the reason creatures are: the count comes from
`sizeof` where the array is defined, and nothing needs dynamic initialisation on the device. A
cache row points at its pool, so the pools are constant too and live in `pools.h`.

The family order in `kItemFamilies` is the order `ContentRegistry::allItems()` returns, which is
the order COLLECT and the 'Pedia list items in. Nothing else reads it: the registry and the save
address items by id.

## Reading the table

- By id, or as a list a screen walks: `ContentRegistry::item()` / `allItems()`.
- Without a registry (a count over content, a dump tool): `forEachItem()`.

There is no flat `kItems[]`. Every consumer was already a scan or a lookup.

## Adding a row or a family

- A row goes in the family a reader would look for it in. If it should turn up in the world, add
  it to a pool in `pools.h` (`../CONTENT_STANDARD.md` lists the other earn paths).
- A family is a new header defining `inline constexpr ItemDef k<Name>Items[]` and its count inside
  `namespace mal::item_table`, plus one line in `kItemFamilies`. `kItemCount` follows.
- Split a family when it stops being skimmable (the ~600-line instinct), not ahead of it.

After any row change, `make pedia && make pedia-check`.
