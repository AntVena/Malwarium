# Malwarium — EXPL Area Content Standard

How one explorable AREA of the EXPL ladder (Citrus Circuit, The Pirate Bayou, …) is
represented, so changing "everything about one area" means finding one folder — the same
role the module rule in `CLAUDE.md` plays for the `game_*.cpp` units and
`src/core/content/CONTENT_STANDARD.md` plays for `content_*.cpp`. Hold new/edited area content to this.

## The shape

One area = one `AreaDef` (`src/core/content/areas/area_defs.h`) = one folder under
`src/core/content/areas/<area_id>/` holding an `area.cpp` that defines it:

```
src/core/content/areas/
  area_defs.h              AreaDef struct + kAreaList/kAreaCount + the area() accessor
  citrus_circuit/area.cpp
  pirate_bayou/area.cpp
  napstorrent_moors/area.cpp
  deepweb_dive/area.{h,cpp}   the terminal endless zone — see below, not an AreaDef
```

An `AreaDef` row owns, in one place: the area's name/tier/Title, its 5 sub-area names, its
5 sub-area boss names + area-boss banner, its signature boss's threat-move rider
(`apexThreatMoveId`), its storefront (`AreaShopDef` — name, stocked item(s), restock
count, and the price charged for each), and its mod-loot pool. `expl_screen.cpp` (the EXPL
list UI), `combat.cpp` (boss composition), and `game_explore.cpp` (mod-loot rolls + the shop
event) all read an area through `mal::area(idx)` rather than owning any of this data
themselves.

**One `area.cpp` per area is enough** — each area's data is a few dozen lines. If one area's
file grows past the point of comfortable skimming (the same ~600-line instinct as the
`game_*.cpp` module rule), split it by concern INSIDE that same folder (e.g.
`pirate_bayou/bosses.cpp`, `pirate_bayou/shop.cpp`) rather than inventing a different
per-area layout — don't pre-split ahead of actual growth.

## Adding a new area

1. Make a folder `src/core/content/areas/<id>/` with an `area.cpp` defining
   `const AreaDef kArea<Name> = {...};` (see any existing area.cpp for the field order).
2. Declare `extern const AreaDef kArea<Name>;` and add one entry to `kAreaList[]` in
   `area_defs.h`, at the ladder position the new area unlocks at.
3. That's it for identity/content. `kAreaCount`, `kExplSectors` (expl_screen.h), and every
   fixed-size save-flag array in `game.h` (`sectorCleared_`, `subCleared_`, …) all follow
   automatically — there is no second count anywhere to remember to bump.
4. Add art (`ICON_SECTOR_<id>`/`BG_SECTOR_<id>`, `assets/ASSET_MANIFEST.md` §J) and
   check the naming rules before claiming a brand: `AREA_NAMING.md`.

There is deliberately no separate "area count" constant to hand-maintain anywhere else:
`kAreaCount` is `sizeof(kAreaList)/sizeof(kAreaList[0])`, so an area that isn't in the list
doesn't exist yet (inert, not silently broken) rather than being clamped into cloning
another area's content.

## What stays OUT of an AreaDef (cross-cutting, not per-area)

Per `src/core/content/CONTENT_STANDARD.md` rule 2: a magnitude used by every area stays a shared
tunable, not a per-row duplicate. Boss Health/speed scaling
(`kSubBossHealthBase/Step`/`kSubBossSpeedBase`), the mod rarity-drop-weight ladder
(`kModRarityWeight`), and the wild sub-area ramp (`kWildSubAreaHealthStep`, `combat.cpp`'s
`applyWildSubAreaRamp`) all read the SAME formula for every area, keyed only by that area's
own `tier`/index — they live in `tunables.h`/`combat.cpp` and are not duplicated per area.
The wild-malbeast tier roster (`combat.cpp`'s `wildMalbeast`) is similarly TIER-keyed, not
per-area, and stays central.

`AreaDef::tier` is the one place an area's difficulty tier is declared — `combat.cpp`'s boss
composition reads it directly rather than re-deriving a tier from a separate formula, so
there is only the one field for boss scaling and the EXPL list to ever disagree about.

## Shop stock and price

`AreaShopDef::stock`/`price`/`price2` are properties of that area's OWN shop, not a single
global — bumping one area's stock or price is a one-line edit on that area's own
`area.cpp`, not a change to shared code or every other shop. `stock` initializes from the
shared `kShopStock` default (`tunables.h`) since every shop's restock count happens to
match; an area that needs a different count just sets its own literal instead.

`AreaShopDef::price`/`price2` are what a player actually PAYS at that shop — read by
`game_explore.cpp`'s buy flow and drawn by `drawShop` (`game_render.cpp`). This is
deliberately a SEPARATE number from `ItemDef::bitsPrice` (`defs.h`), which is the item's own
reference price: the 'Pedia's listed price, and the generic "is this item shop-sellable at
all" signal. The two can diverge on purpose — a shop is free to charge above or below an
item's listed price — so don't expect them to stay equal; there is no rule enforcing that,
by design.

## DeepWeb Dive is not an AreaDef

The DeepWeb Dive is the always-last endless zone (`kDeepWebSector`, one past the real
ladder) — it has no sub-area ladder, no boss, no shop, so an `AreaDef` doesn't fit it. Its
mod pool (`kAreaModsDeepWeb`) and its six endless-scaling constants
(`kDeepWebEnemyLevelOffset` and friends) live in `areas/deepweb_dive/{area.cpp,area.h}`
instead — moved out of the cross-cutting `tunables.h` since nothing outside the dive reads
them, following the same "single-entity magnitude lives on that entity" rule as an AreaDef.

## Why areas aren't a `ContentSource`

Creatures/items/mods/moves are registry-mediated, addressed by string id
(`ContentSource`/`ContentRegistry`, `content_source.h`) — a shape built for a future
SD-card override (`docs/ORIENTATION.md`). Areas stay a parallel, simpler compiled-in
pattern instead of joining that registry, because every area call site is INDEX-based (area
0..N-1, matching the save-flag arrays), not an id-string lookup — converting the calling
convention everywhere for no behavioral gain isn't warranted while nothing consumes areas by
id. Promoting `AreaDef`/`kAreaList` into a `ContentSource::areas()` virtual later (for the
SD-served-zone story) is a small additive step from this shape: one new virtual method, one
`EmbeddedContent` override returning the existing list — not a rewrite of this doc's pattern.
