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
     it. So a description doesn't explain the item at all: it is flavour (see
     *Description voice* below), and the arithmetic is generated.
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
  (rule 3) — though a description in the voice below rarely wants one.

## Description voice

The readout grid under a description already says what the item does, in numbers. The
description is for what the grid can't carry: the joke, the feel, a little character.
It is flavour, and it should read like something a friend says about the food, not a
narrator hinting at a secret.

**The pattern.** One warm, everyday sentence (two at most) that makes complete sense as
a remark about the object itself, with the tech meaning of the name sitting underneath.

> Tiramisudo: "Guaranteed to put lasting pep in your step but whether or not it's on the
> menu depends who's asking."
> Hackshuka: "Sometimes it's fun to just grab some reg-eggs, whatever's in the fridge,
> and make it work."
> Cod Review: "Quality control on this dish is rigorous. If the fish is too fishy, the
> shipment gets cancelled."
> Null Noodles: "It tastes like nothing."

### The rules

1. **No mechanics.** No numbers, no stat names, no instructions ("use it before a
   dive", "cook it into something"), no "once per pet". A `{token}` is still how a
   number gets in if one is ever truly needed (rule 3), but the grid has it already.
2. **It must make sense as food (or as the object) on its own.** A tech catchphrase
   with no food reading fails: "Looks good to me. Ship it." does; Cod Review's line
   passes because a fish supplier really would cancel a fishy shipment.
3. **Play the name's tech half in a fresh sense; never say it.** Hackshuka evokes
   hacking as making-do; Tiramisudo never says "sudo". The tech word, its expansion
   ("secure shell" for SSH) and its obvious synonyms stay out of the line. Repeating
   the FOOD half of the name ("salt", "beans") is fine.
4. **A pun of its own, or none.** Every line carries its own wordplay ("veggie patch",
   "secret ingredient") or is simply, plainly true of the food ("It tastes like
   nothing."). Simple beats forced.
5. **Ingredients only when they are the pun.** "reg-eggs" earns its place; a recipe
   list or a hint about what to cook does not.
6. **No fake mystery.** No "nobody…", "somebody…", "no one knows…", no trailing
   ellipsis, no setup the line never pays off, no riddle that only lands if you already
   know what the item does.
7. **Every reference is anchored.** "The" and "it" point at something the line itself
   or the object supplies — the dish's own jar, tin or tray. "The long trip", "the
   table", "the bakery" assume a scene the reader was never given.
8. **No comma the sentence doesn't need.** The habit to break is "X, and Y" on every
   line. Keep a comma only where grammar or an idiom needs it: a list, "First come,
   first served", Given/When/Then.
9. **Vary the shape across the table.** Don't open half the shelf with "Every…" or
   "Comes with…", and never give two rows the same line.
10. **Warm, not knowing.** Kitchen talk, second person welcome, kind rather than
    cynical. Short is fine — one line is fine.
11. **If no line can work, fix the name.** A tech half too obscure to land (Crostini,
    ChromeOS's container) is a naming problem (ITEM_NAMING.md), not a prose problem:
    it became BruSSHetta.

### What a gate can and can't check

`test_effect_text_fits_its_screen_budget` measures every line against the panel (26
characters a line; a shop listing gets three). A script can also catch commas, the
"nobody" family, a trailing ellipsis, a digit, and a line repeating a word of its own
name. Rules 2, 3 (synonyms and expansions), 4, 7 and 10 need a reader. So write a batch,
run the mechanical checks, then read every line against this list before it ships, and
try two or three lines on someone before writing two hundred.
