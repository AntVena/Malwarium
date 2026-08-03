# `assets/` — the atlas

**Every PNG under this directory IS the shipped sprite set.** `tools/gen_assets.py` walks the tree
and compiles them into flash, skipping anything whose name — file or folder — starts with `_`. So a
correctly-named stand-in ships exactly like final art, `_*.png` are design studies that never ship,
and art with no consumer waits in `_attic/` (untracked, uncompiled) rather than costing flash.

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
