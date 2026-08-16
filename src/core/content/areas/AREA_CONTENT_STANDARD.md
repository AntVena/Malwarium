# Malwarium — EXPL Area Content Standard

How one explorable AREA of the EXPL ladder (Citrus Circuit, The Pirate Bayou, …) is
represented, so changing "everything about one area" means finding one folder — the same
role the module rule in `CONTRIBUTING.md` plays for the `game_*.cpp` units and
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

An `AreaDef` row owns, in one place: the area's name/Title, the name of its sector glyph
(`icon` — keyed by area id, so art follows identity rather than rung), its 5 sub-area names, its
5 sub-area bosses (`SubBossDef` — a banner, plus the rounds it is fought as) + area-boss
banner, its signature boss's threat-move rider
(`apexThreatMoveId`), its storefront (`AreaShopDef` — name, stocked item(s), restock
count, and the price charged for each), and its mod-loot pool. `expl_screen.cpp` (the EXPL
list UI), `combat.cpp` (boss composition), and `game_explore.cpp` (mod-loot rolls + the shop
event) all read an area through `mal::area(idx)` rather than owning any of this data
themselves.

**A sub-area boss is a round LIST, and most rows leave it empty.** `{"MYDOOM LICH"}` is the
whole row for a boss that is one fight — the empty `rounds` means "one round, named by the
banner", so only a rung that actually wants escorts pays any authoring cost for them. A row
that does want them spells each round's name and its `rung`, a DELTA on that sub-area's own
depth (`0` = the boss at full strength, `-1` = drawn one rung shallower). Escorts are
therefore never a second stat block to keep in step with the boss they guard — they are the
same curve, evaluated a rung back. Rounds run back-to-back with carried Health on the same
plumbing the area boss uses. Name every round to `AREA_NAMING.md §3`: an escort is a name the
player reads.

The AREA boss composes each sub-area's boss **proper** — that banner at its own rung, never
its escorts — so the finale stays exactly `kSubAreasPerArea` rounds however many rounds a
single rung grows.

## What a boss TEACHES

`SubBossDef::teaches` names the moves a boss carries on top of the depth spine
`subBossEnemy` builds, and `AreaDef::areaBossMoveId` does the same for the area banner (on
the gauntlet's final round only, so it costs all five stages). **This list is the only thing
that makes a move reachable.** A drop is drawn from the defeated enemy's kit
(`Game::rollEnemyMoveDrop`), so a move no boss names cannot be earned anywhere in the game —
it is not rare, it is dead, and nothing on the `MoveDef` row says so.
`test_every_generic_move_is_carried` is what holds that line; it also rejects a `teaches` id
that resolves to nothing, which would be the same bug approached from the other end.

**Kit size is the budget, not flash.** `Combat::chooseMove` is uniform over the kit, so each
move added to a boss is a slice of that boss's turns spent doing it — which is why
`kMaxBossTeaches` is 2 and why a kit may hold at most one Defend (a second brace is a boss
bracing half the time instead of fighting; the same gate checks it). Give a boss one move
that is worth beating it for, rather than a grab-bag.

Author moves themselves to `src/core/content/CONTENT_STANDARD.md`; note that several
`MoveDef` fields look generic but carry a LINE's identity (`stackPower*` is Ransomware's,
`stealPowerPct` feeds the Phishing frenzy on any row that sets it) — the boss pool's header
comment in `content_moves.cpp` lists what a generic row may safely use.

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
3. That's it for identity/content. `kAreaCount`, `kExplSectors` (expl_screen.h),
   `kModPowerTiers`, the area's difficulty (`areaTier`), and every fixed-size save-flag
   array in `game.h` (`sectorCleared_`, `subCleared_`, …) all follow automatically — there
   is no second count anywhere to remember to bump.
4. Add art (`ICON_SECTOR_<AREA_ID>`/`BG_SECTOR_<AREA_ID>`, `assets/ASSET_MANIFEST.md` §J),
   name the glyph in the row's `icon` field, and check the naming rules before claiming a
   brand: `AREA_NAMING.md`.

### If it lands anywhere but the END of the list

Appending is free. **Splicing into the middle is not**, because two things are keyed by
ladder POSITION rather than by area id, and both go wrong silently:

- **Every persisted EXPL flag.** `sectorCleared`, `bossUnlocked`, `subCleared`,
  `subBossUnlocked`, `subRefarm`, `titlesUnlocked` and `equippedTitle` are all indexed by
  rung, so an existing save's progress would shift onto the wrong areas — handing the
  player the new area pre-cleared and taking back the deepest one they beat. Bump
  `kSaveVersion` and add one row to `ladderInserts` (`core/model/save.h`) **in the same
  commit**; the codec then opens a blank rung and everything downstream reads a migrated
  save as if it had always had one.
- **Each mod's `powerTier`** (`content_mods.cpp`). A rank is a ladder depth spelled out on
  the row, so every mod authored at or below the seam moves up one rank. The native gate
  catches this — it checks each mod's rank against the shallowest pool that drops it —
  but it is an edit to make, not something that follows.

The sector ART family is not affected: `ICON_SECTOR_*`/`BG_SECTOR_*` are keyed by the
area's own `id` and named on its row (`AreaDef::icon`), so a splice moves an area's rung
without moving its picture. See the note under `ASSET_MANIFEST.md` §J.

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

An area's difficulty tier is **not on the row at all** — `areaTier(idx)` derives it from
ladder position (`area_defs.h`), and `combat.cpp`'s boss composition and the EXPL list both
read that one function. Order is the sole statement of depth: reorder `kAreaList[]`, or
splice an area into the middle of it, and every enemy above the seam re-scales to its new
depth with nothing to re-tune. An authored `tier` field would restate the same fact, and the
two would disagree the first time the ladder moved — which is exactly what inserting an area
does.

The corollary is that **an area's own `area.cpp` must not name its ladder position or tier**,
in a comment or anywhere else: that is the one fact about an area which its file does not own.

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
mod pool (`kAreaModsDeepWeb`), its endless-scaling constants (`kDeepWebEnemyLevelOffset`
and friends) and its move rungs live in `areas/deepweb_dive/{area.cpp,area.h}`
instead — moved out of the cross-cutting `tunables.h` since nothing outside the dive reads
them, following the same "single-entity magnitude lives on that entity" rule as an AreaDef.

It is also the only zone whose enemies are **rolled rather than authored**. A dive enemy
takes a body from the tier-3 roster and gets everything else from depth: a BUDGET of stat
points spent at random across the same four stats a pet levels (`applyDeepWebScale`), and a
KIT drawn from the depth's move rung (`deepWebMoveIds`). Two rules constrain that, and both
exist to protect something outside the dive:

- **Boss signatures are gated to `kDeepWebBossMoveDepth`.** A boss is meant to be the first
  place its move is ever seen; a zone handing the same move out early would retire the hunt
  the whole roster is built around.
- **A rolled enemy answers to the player's own curves** — `levelDefenseCutPct` and the
  never-immune clamp — so the dive can never field a wall the player could not have built.

Rung pacing is authored per depth rather than derived from the zone's log curve, because
what a rung does to a fight is not proportional to what a stat point does. That is measured,
not assumed; the numbers and the reason are on `kDeepWebMoveRungDepths`.

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
