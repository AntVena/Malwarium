# Malwarium — Creature Content Standard

How one creature FAMILY is represented, so changing "everything about one evolution line" means
finding one folder — the same role [`AREA_CONTENT_STANDARD.md`](../areas/AREA_CONTENT_STANDARD.md)
plays for areas and [`../CONTENT_STANDARD.md`](../CONTENT_STANDARD.md) plays for the `content_*.cpp`
tables. Hold new/edited creature content to this.

## The shape

One family = one `CreatureLine` = one folder under `src/core/content/creatures/<line_id>/` holding
a `line.h` that defines its rows:

```
src/core/content/creatures/
  creature_lines.h       CreatureLine list + kCreatureLineCount/kCreatureCount + creatureLine()
  ransomware/line.h
  phishing/line.h
  trojan/line.h
```

`<line_id>` is the string every row in the file carries as `CreatureDef::line` — the same id
`moveAllowedForLine` gates the line's moves with. The folder name and the field are the same fact
stated once each; a row whose `line` disagrees with its folder is a bug the achievement counts
would report first (`AchSeries::LineRaised` reads the family's own count).

A `CreatureDef` row owns, in one place: its id and display name, its stage, its sprite sheet
(`spriteName`) and the named animation loops over that sheet (`clips`), its evolution edges
(`evolvesToId`, the Good/Bad branch pair, `evolvesToTrojanId`), its combat branch multipliers, its
line, its `hint`/`context` flavour, its per-slot move typing and its `locomotion`.

**One `line.h` per family is enough** — a family is a few dozen lines per chain. If one grows past
comfortable skimming (the same ~600-line instinct as the `game_*.cpp` module rule), split it by
chain INSIDE that same folder (e.g. `ransomware/canine.h`, `ransomware/ursine.h`, with `line.h`
concatenating them into the family's array) rather than inventing a different per-family layout.
Don't pre-split ahead of actual growth.

### Why headers, and why the roster has no flat form

The rows are `inline constexpr` arrays in headers rather than `extern` tables in `.cpp` units,
which is where this differs from `areas/`. An area needs no count — `kAreaList` holds one pointer
per area and derives `kAreaCount` from its own length. A family holds MANY rows, so its count has
to come from `sizeof` over the array, and `sizeof` only works where the array is defined. Defining
the rows in a header keeps the count derived at the definition and constant-initialised, so
nothing is hand-typed twice and no table needs dynamic initialisation on the device.

That is also why `ContentSource` hands the registry `creatureLines()` rather than a flat roster:
there is no contiguous array of every creature to hand it, and nothing wants one. Every consumer
was already a scan — `ContentRegistry::creature()` looks up by id, `allCreatures()` builds a vector
of pointers — so they walk families then rows.

## Adding a family

1. Make a folder `src/core/content/creatures/<line_id>/` with a `line.h` defining
   `inline constexpr CreatureDef k<Name>Creatures[]` and its `k<Name>CreatureCount` (see any
   existing `line.h` for the field order).
2. Add one entry to `kCreatureLines[]` in [`creature_lines.h`](creature_lines.h).
3. That's it for the roster. `kCreatureLineCount`, `kCreatureCount` and the 'Pedia's species list
   all follow — there is no second count anywhere to bump.
4. Give the line its moves and passive (`../LINE_MOVE_IDENTITIES.md`), an `EggLineDef` if it hatches
   from an egg (`content_evolution.cpp`), and art (`../../../../assets/CREATURE_VISUAL_RULES.md`).
5. Run `make pedia && make pedia-check` and commit the regenerated `web/data/pedia_data.js`.

### Order is free

Unlike the EXPL ladder, **nothing is keyed by a creature's position.** The registry addresses
creatures by id and a save records them by id (`save.h`'s `seenCreatures`), so a family may be
spliced in anywhere or reordered with no `kSaveVersion` bump and no migration. The one thing a
reorder changes is the order the web 'Pedia lists species in, because `tools/dump_content.cpp`
walks `kCreatureLines` in order.

### …but names are not

Position costs nothing to change. An **id** costs a save format. It is a wire value (`save.h`), so
renaming one means a `renamedIds` row plus a `kSaveVersion` bump, and the row then has to be
carried until `kOldestAcceptedVersion` rises past it. Two consequences worth planning around:

- **Get the name right before the creature ships,** not after. A placeholder that reaches a release
  is a rename row forever; one caught beforehand is a one-field edit.
- **A rename may be MANY-TO-ONE** — several retired ids landing on the same successor, which is
  what happens when a branch collapses. That is legal (the flattening rule only requires distinct
  `from`s), but it is the case where a blob can name one creature twice, and the seen/raised
  tallies are lists that something counts. `applySave` dedupes on load for exactly this reason.

**A display name is a portmanteau of a computing term and a creature word** — Vermicell, Nodeatode,
Cryptoad, Berserkernel, Keyloggerhead. Check the stem against the WHOLE roster, not just the line
you are adding to: the Worm line's good Daemon was very nearly *Longpoll*, which collides with the
Phishing line's Tadpoll on a suffix that already means something. `dump_content` will not catch it
and neither will the gates — the ids differ, so only a reader notices, and by then it has shipped.

## Clips

`CreatureDef::clips` declares up to `kMaxAnimClips` named loops over the creature's own sheet. A
clip names the sheet ROW it plays, how many of that row's columns it uses, and how many heartbeats
each frame holds:

```cpp
/*clips=*/{{"idle", /*row=*/0, /*frames=*/8, /*holdBeats=*/4},
           {"attack", /*row=*/1, /*frames=*/4}}
```

- **Clips are found by name, never by position** (`CreatureDef::clip()`), so order them however
  reads best and leave unused slots empty.
- **A creature that declares no clip is not broken.** It falls back to `sprite.h`'s `idleFrame()`
  breathe/blink heuristic on row 0, which is exactly what a single-frame placeholder wants —
  declaring a clip is how a MULTI-row sheet becomes reachable at all.
- **`holdBeats` is the speed dial.** Raise it to slow a loop down (`holdBeats=2` halves the speed)
  without redrawing frames or re-counting columns.
- **The sheet decides what's legal.** `tools/gen_assets.py` slices a `SPR_PET_*` sheet into rows of
  `PET_ROW_H`, up to 8 columns of `PET_FRAME_W` each; a clip naming a row or frame the sheet does
  not have draws nothing, and the blitter skips it rather than flickering the pet.
- **EVERY FRAME OF AN `attack` ROW MUST READ AS A COMPLETE STRIKE ON ITS OWN.** `frameAt()` indexes
  off the GLOBAL anim beat and nothing restarts a clip when a swing starts, so the swing's window
  (`kAttackHopPeriod`, `core/ui/combat_screen.cpp`) can open on any column of the row and end on
  any other. A windup-then-strike ordering will show its windup last as often as first, and on a
  short row may show nothing else. So attack frames differ in DETAIL — which limb leads, where the
  markings sit — never in PHASE, and `holdBeats` is set so the row walks its columns across that
  window about once. This is a drawing instruction before it is a data one: it decides what the
  artist puts in the cells, and no clip field can rescue a row drawn as a sequence.
