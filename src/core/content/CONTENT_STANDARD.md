# Malwarium — Content & Tunables Standard

How content (creatures, items, mods, moves) and its balance numbers are laid out, so a
reader can **skim one file's headers to get the picture** and **go to one place to see a
thing and every lever it pulls**. Hold all content to it.

## The four rules

1. **Content is data with a structured *effect vocabulary*, not scalar-field sprawl or
   `if (id == "...")` branches.** A thing's effects are a scannable list of typed
   `kind + magnitude` entries on its row — the same shape `ModEffect` (combat) and
   `ItemEffect` (on-Use pet levers) already use. A new mechanic adds a *Kind* (one enum
   entry + one case in the central applier), **not** another optional bool/int bolted onto
   the shared struct, and **never** a hardcoded per-id special-case in game logic.
   - *Why:* the struct stays narrow and skimmable; behaviour lives in one `switch`, which
     is itself the map of every way the thing reaches the rest of the game.
   - *Exemplars:* `ItemEffect` → `Game::applyItemEffects` (`game_items.cpp`);
     `ModEffect` → combat appliers (`combat.cpp`/`game_combat.cpp`).

2. **A magnitude used by exactly one entity lives *on that entity*, never in `tunables.h`.**
   `tunables.h` is for **cross-cutting** balance shared by multiple systems (hunger decay,
   zone thresholds, stage durations, drop-weight ladders). If a constant's comment names
   one item/mod/move/creature, it's misfiled — inline it onto that row. This holds *even
   while a value is being balance-tuned*: this is an agile project, everything is always
   being tuned, so "it's not final yet" is never a reason to globalise a one-thing number.
   - *"If it acts on something else, go to the something else."* A row's structured
     `effects` list is what it does to the pet; trailing named hand-off fields
     (`combatHeal`, `preEncounterXp`, `bitsPrice`, `walkWarp`, `use`) point at the *other*
     systems it reaches, each applied by that system.

3. **A description never restates a number — it names the field that holds it.** The
   `effect` string on `ItemDef`/`ModDef`/`MoveDef` is a **template over its own row**:
   write `{mag}` / `{hunger}` / `{pierce}` and `core/content/effect_text.h` substitutes
   the value from the same row, so retuning a magnitude retunes the sentence and the two
   cannot drift. A hand-typed digit in prose is the thing this rule exists to stop; the
   native gate `test_effect_text_templates_resolve` fails on a token that doesn't
   resolve, so a renamed field breaks the build rather than shipping stale copy.
   - *You don't have to cite every number.* Each screen also draws a `statLine()` derived
     straight from the row's structured effects (`ItemEffect` kinds / `ModEffect` /
     a move's riders), which reports every magnitude whether or not the prose mentions
     it. A description's job is what the thing is FOR; the arithmetic is generated.
   - *Adding a token:* extend the per-type table in `effect_text.cpp` — and for an item,
     give the new `ItemEffect::Kind` a name in `itemEffectToken()` beside its applier
     case, so a new mechanic reaches the prose the same way it reaches the pet.

4. **One content type per file** — and, once a type's rows group into families a reader would
   want to open one at a time, one FOLDER per family. Each table lives in its own
   `src/core/content/content_<type>.cpp` unit (declared in `content_tables.h`, assembled by
   `embedded_content.cpp`). Split *at* growth, keep each unit skimmable. Adding a type = a new
   `content_<type>.cpp` + one `extern` pair in `content_tables.h` + one accessor in
   `embedded_content.cpp`. Adding a row = edit that one table.
   - *Types that outgrew one file:* EXPL areas (one folder per area,
     `areas/AREA_CONTENT_STANDARD.md`) and creatures (one folder per evolution line,
     `creatures/CREATURE_CONTENT_STANDARD.md`). Both keep a single list naming the members —
     `kAreaList` / `kCreatureLines` — from which every count derives, so the split never adds a
     number to keep in sync. Reach for it when one file stops being skimmable, not before.
   - *Downstream:* the web 'Pedia reads these tables through the firmware's own code —
     `tools/dump_content.cpp` links them and prints JSON, which `tools/gen_pedia_data.py`
     consumes. So a struct-shape change needs no matching edit in the generator; run
     `make pedia && make pedia-check` and commit the regenerated data.
   - *Registry-mediated vs. compiled-in:* the four id-keyed types above go through
     `ContentSource`/`ContentRegistry`. Content whose call sites don't need the registry's
     SD-override story keeps its own compiled-in table + accessor — EXPL areas
     (`content/areas/area_defs.h`, `src/core/content/areas/AREA_CONTENT_STANDARD.md`) and crews
     (`content_crews.h`), both index-addressed, and achievements
     (`content_achievements.h`), which are id-addressed but whose triggers are engine-side,
     so there is nothing a content pack could author on its own. One file per type either way.

## Ids are readable words, and a rename is a codec concern

A content id is a lowercase word a human can read in the table (`bruinforce`, `null_noodles`) —
not an opaque number. The cost is that a save can be holding an id a rename has retired; the
answer to that is the save codec's rename table (`core/model/save.h`'s `renamedIds`), which
rewrites retired ids on load and states when a row may be deleted.

So: **rename freely, and never leave an alias behind in a content table.** An alias there would
read as a second legitimate name for the thing and could never be safely removed, which is the
trade opaque ids were the alternative to — and it buys nothing the codec isn't already doing.
Add the rename row, and check the flattening rule in `save.h` if the old name is one the table
has already rewritten once.

## Known remaining violations (apply the standard as these are touched)

Not yet cleaned up — do it opportunistically when working nearby, not as a big-bang pass.

None outstanding.

## When you add content

- New item effect on the pet → add an `ItemEffect::Kind` + a case in `applyItemEffects`;
  put the magnitude on the row. Don't add a field to `ItemDef` for it.
- New item that isn't food, a buff, or a plain carried tool → leave `type` as the coarse
  Food/Buff/Quest bucket and name its ITEMS type-picker tile with `ItemDef::Category`
  (`Keys` for a key/token). The default `Derive` resolves Food→FOOD, Buff→BUFFS,
  Quest→TOOLS, so only rows that break that pattern say anything.
- New combat effect → add a `ModEffect`/`MoveDef` field or kind; magnitude on the row.
- New MOD → its equip gate and its relationship to a creature line are both authored on the row,
  and both have rules the table's own header states: `equipLevel` against a dense ladder (a tier
  picks the area, not the level), and a LINE mod's shape following its effect — soft
  `line`/`affinityBonus` on a generic kind, hard `requiresLine` only on a line-passive amplifier
  that would be inert off-line. See `content_mods.cpp`'s header before adding a row;
  `test_mod_equip_ladder_is_ordered_and_dense` is what fails if the ladder grows a hole.
- New balance number → ask "does more than one entity read this?" No → on the row. Yes →
  `tunables.h`.
- New item that should turn up in the world → add it to a pool (`content_items.cpp`'s
  `kLootPool` / the `kCachePool*` sets) as a bare `{"id"}` and stop. Reach for
  `ItemDef::dropWeight` only when this item should be scarcer or commoner than its
  RARITY-mates everywhere, and for a `LootEntry` weight only when it should differ in
  ONE pool. The three levels answer three different questions — how valuable (rarity),
  how common (dropWeight), how common HERE (the pool row) — so don't overload rarity to
  express scarcity.
- Writing the row's description → put `{token}`s where the numbers go, never digits
  (rule 3). If nothing in the prose wants a number inline, write pure flavour and let
  the derived stat line carry the magnitudes.
