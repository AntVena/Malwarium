# `assets/` — the atlas

**Every PNG under this directory IS the shipped sprite set.** `tools/gen_assets.py` walks the tree
and compiles them into flash, skipping anything whose name — file or folder — starts with `_`. So a
correctly-named stand-in ships exactly like final art, `_*.png` are design studies that never ship,
and art with no consumer waits in `_attic/` (untracked, uncompiled) rather than costing flash.

**`fonts/` is a source, not art.** `FONT_UI`'s TTF lives there with its licence, and nothing
compiles it: `gen_assets.py` only ever walks for PNGs, and the glyph table that actually ships
is committed source (`src/core/render/font_glyphs.cpp`), rasterised on demand by
`tools/gen_font.py`. So the TTF costs no flash and has no consumer to be orphaned from — it is
the thing the table is regenerated FROM, which is why it is in the tree at all.

**A few PNGs have a generator that OWNS them.** The Worm line's sheets are drawn by
`tools/gen_worm_art.py` from a recipe, not by hand, because that line's signature is a *style*
rather than a colour (see [CREATURE_VISUAL_RULES.md](CREATURE_VISUAL_RULES.md) §4) and a style holds
across a dozen sprites only if something mechanical is holding it. Such a file is still the shipped
sprite — nothing else changes about how it compiles — but editing its pixels by hand is a change the
next run of the tool silently reverts, so the `worm_art_recipes` ctest fails on any drift between a
committed sheet and its recipe. Change the recipe, re-run the tool, commit the PNG it writes.

**An asset's id is its BASENAME, not its path.** `sprites/` · `icons/` · `ui/` are for readers only —
`SPR_PET_PAYPUP` resolves the same wherever the file sits, so art can be refiled without touching a
line of code. The price is that basenames must be globally unique; `gen_assets.py` raises on a
collision rather than letting one id mean two files. It is also the only thing that knows where a
PNG lives: `gen_pedia_data.py` and `check_orphan_assets.py` ask it (`asset_paths()`) instead of
composing paths, which is why the web bundle's layout follows this tree for free.

| Before you… | Read |
|---|---|
| Check what exists / what's still a placeholder | [ASSET_MANIFEST.md](ASSET_MANIFEST.md) — the master list, by consumer |
| Pick a colour, a size, or type | [VISUAL_LANGUAGE.md](VISUAL_LANGUAGE.md) — `PAL_CORE` role tokens, `FONT_UI` scale, icon size tiers, layout grid |
| Draw or judge a creature sprite | [CREATURE_VISUAL_RULES.md](CREATURE_VISUAL_RULES.md) — the shading law, the stage-evolution read, the three acceptance tests |

## Colour is three edits deep, and never a sweep

**Bind every colour to a `PAL_CORE.json` role token, never a literal hex** — and never re-invent a
colour that already has a token. That one binding is what makes the rest of this true:

| To change… | Edit |
|---|---|
| what a role *looks like* (Epic goes orange) | the token's hex in `PAL_CORE.json` |
| the whole interface at once (a colourblind or high-contrast set) | a `themes` block in `PAL_CORE.json` naming only the tokens it overrides, then the active index |
| what a game concept *means* (a fifth rarity, a new team) | `src/core/ui/theme.h` — the one place a domain value becomes a token |

No screen holds a colour opinion, so none of the three is a sweep. `ctest` enforces it:
`tools/check_palette_bindings.py` fails on any source that builds a colour from a literal instead
of asking `palColor()`, because a single one of those is a colour no theme can ever move.

**The tinting rule:** a tint is decoration, never information. It may only repeat a meaning the
same screen already carries some other way — a word, a count, a shape — and must never be the only
channel. The ITEMS row tints its icon by rarity *and* prints the rarity word beside it; the Sealed
Cache tiers are countable chevrons before they are colours. Both survive desaturation, which is the
test. `drawSpriteTinted` is exact on these masters because they're a single flat fill.

## What a sheet costs, and where the cost actually lands

`tools/gen_assets.py` derives each sheet's palette from its own pixels — an entry being one
distinct (RGB565, coverage) pair, with every transparent pixel collapsing to one — and then cuts
the sheet into a grid of square tiles, storing identical tiles once and giving each tile the bit
width it alone needs. Nothing is declared and nothing is quantised: repaint a sheet, add a colour,
and the palette follows on the next build.

Two consequences worth knowing before drawing:

**Repetition is nearly free, and empty space is free.** A tile of one colour costs one byte
however large the region, and a tile that appears twice is stored once. `SPR_PET_KEYLOGGERHEAD`
is 40 distinct tiles across 672 positions — a 448×96 sheet in 2.4 KB — because an animation row
redraws most of its body every frame. So extra frames of a mostly-static pose cost far less than
their pixel count suggests, and the padding around a small drawing in a big cell costs almost
nothing.

**A stray colour is charged locally, not sheet-wide.** Because each tile carries its own width, a
one-pixel off-model shade widens the tiles it lands in and no others. Folding 24 such colours out
of Cuttlefork is worth 208 B, not the several KB a sheet-wide width would have cost.

```bash
python3 tools/gen_assets.py --palettes           # what each sheet's palette holds, and its tail
python3 tools/snap_palette.py <png> [--dry-run]  # fold the tail into the colours it is drawn in
```

The snapper is therefore art hygiene rather than a saving — it squares off partial alpha and
repaints colours nobody else wears, keeping a sheet's palette to the tones someone chose. A
surviving pixel keeps its own 8-bit value, so the PNG stays the art. Look at the pixel count it
reports before committing: a sheet where the fold moves a visible fraction is one whose shading
the fold is eating, not one with drift.

**Per sheet, not per line.** One table shared by a family is genuinely the smaller table — the
union of five palettes is smaller than five palettes — but the table is not where the bytes are.
Across the whole tree the palettes come to 1.6 KB against 112 KB of tile data, and what a wider
union buys is a wider index, paid per pixel. Sharing wins only where the union widens nobody, which
is the Worm line — five sheets of one colour each — and there it is worth 4 bytes. That is the
ceiling of the idea, and it is why a mother colour is an authoring rule
([CREATURE_VISUAL_RULES.md](CREATURE_VISUAL_RULES.md) §4) rather than a storage one.

Author at 128×128 logical (rect 128×64); a pet sprite's max box is 128×64, the standard cell 56×48.
Prefixes, and the folder each files into: `ICON_*` menu/slot → `icons/` · `SPR_*` sprites →
`sprites/` · `UI_*` chrome and `BG_*` backdrops → `ui/` · `CAP_*` caption styles · `FX_*` procedural
passes with **no art** (listed so they get skipped).

Status legend: `☐` TODO · `✎` WIP · `▨` placeholder shipping · `☑` delivered · `⌫` parked in
`_attic/` · `⊘` N/A.

The manifest is prose and drifts; `python3 tools/check_orphan_assets.py` can't — it fails on any
compiled asset with no consumer. Trust the check over a `☑`.

**Release gate:** a grayscale screenshot of any screen must stay fully readable. Colour is never
the only carrier of a status meaning.
