# Contributing to Malwarium

`README.md` is the player on-ramp — start there if you want the device working. This file is
the developer one: the constraints the code is written against, where the standards live, and
how to run the gates.

## Hard constraints (non-negotiable)

ST7789 **240×240** RGB565 · active canvas **224×224** (8px bezel, no UI in it) · author at
**128×128 logical** (rect 128×64) · **×1.75 integer upscale, no downscaling** · ~4fps
event-driven repaint (redraw on change) · 2-frame loops are the norm · pet cell **56×48
logical** (max 128×64) · buttons **A=Next · B=Accept · C=Cancel**, with **A+C** the Exploit
chord (Hacker face, combat override, egg crack).

**Dual-coding release gate:** every status meaning carries a non-colour channel — **a grayscale
screenshot of any screen must stay fully readable.** The pipeline all of this describes is
[`src/core/render/RENDER_PIPELINE.md`](src/core/render/RENDER_PIPELINE.md).

**Locked vocabulary:** *Malwarium* = the device/habitat · *malbeast* = a wild creature ·
*petware* = a tamed pet · *the 'net* = the world. You **tame by raising**, never by capturing.

## The `game/` module rule

`Game` is one class split across `src/core/app/game/game_*.cpp` units (all including
`core/app/game.h`). New behaviour goes in the matching unit — feeding → `game_items`, draws →
`game_render`, and so on. If a unit would pass ~600 lines, add a new one: split **at** the
growth, by concern, not ahead of it. Helpers shared by two or more units go in
`game_internal.h` (private), never `game.h`.

## Comments and doc prose

A comment orients a reader *in the moment*: **what** a thing is for, **how** to use it, or
**what consumes it** (worth saying when that's another file). It never points back at the
planning board — [`docs/MASTER_TODO.md`](docs/MASTER_TODO.md) decides *what* to build, and the
card does not go in the code.

So: no planning or milestone IDs, no dates or session tags, no change-narration ("the old X",
"used to", "now lives on"). Describe the **current** state, and reference other code — files,
symbols — freely. Docs are forward-facing for the same reason: cite how and where, never when.
`git log` is the changelog, and prose that narrates change competes with it and loses.

Full rules and the keep-anyway exceptions (EAPOL `M1–M4`, board and pin names, world
`Area 0–3`, living how-to standards): [`docs/COMMENT_STANDARD.md`](docs/COMMENT_STANDARD.md).
`python3 tools/check_comment_standard.py` is the mechanical check, and it runs in the gates.

Transient state gets no file. A session summary or handoff is not a document; anything durable
becomes a row on the board.

## Where the standards live

Each lives in the folder it governs. This table is for finding them cold.

| If the task involves… | Read first |
|---|---|
| Re-orienting, "what's open" | [`docs/MASTER_TODO.md`](docs/MASTER_TODO.md) — the one board |
| The cross-cutting picture: repo map, stack, carousel, care model, currency, radio, releasing | [`docs/ORIENTATION.md`](docs/ORIENTATION.md) |
| Adding a milestone-sized feature | the board (open work) + [`docs/TEST_STRATEGY.md`](docs/TEST_STRATEGY.md) ("done" criteria) |
| Content — items, mods, moves, creatures, or a balance constant | [`src/core/content/README.md`](src/core/content/README.md) |
| An EXPL area, its bosses/shop/mod pool, or adding a new one | [`src/core/content/areas/README.md`](src/core/content/areas/README.md) |
| Naming a creature, item, area or sub-area | the naming docs beside the tables, routed from those two |
| Art, palette, sprites, what exists vs. what's a placeholder | [`assets/README.md`](assets/README.md) |
| Any UI screen | [`assets/VISUAL_LANGUAGE.md`](assets/VISUAL_LANGUAGE.md) for the system; a shipped screen's behaviour is `src/core/ui/` + `game_render*.cpp`, which is the contract |
| A render pass or visual effect | [`src/core/render/RENDER_PIPELINE.md`](src/core/render/RENDER_PIPELINE.md) |
| Pins, board peripherals, bring-up | [`include/config.h`](include/config.h) is the pin authority; [`src/platform/esp32/HARDWARE.md`](src/platform/esp32/HARDWARE.md) for the board |

**Source types.** `assets/` and `PAL_CORE.json` are final data — reproduce them exactly, and
never re-invent a colour that already has a token. `include/tunables.h` holds cross-cutting
balance only; a magnitude one entity reads lives on that entity's row. A shipped screen is its
own contract.

## Building and testing

Two tiers, and neither substitutes for the other. The native suite is fast and covers
`src/core`; the S3 build is the only thing that compiles `src/platform/esp32`, so a green
native run says nothing about whether the firmware still links.

```bash
./tools/gates.sh
```

Quiet on success, and on failure it prints only the lines that explain it. `--native` skips the
firmware build for a tight edit loop; `--verbose` streams everything. The same two tiers run in
CI on every push (`.github/workflows/gates.yml`).

`src/generated/` is compiled from `assets/` by `tools/gen_assets.py` and is not committed — the
gates regenerate it, so a fresh clone needs no extra step.

Commit verified work as logical units. A message should carry the motivation and the *why*, not
a restatement of the diff.

## Releasing

Tagging `v*` publishes to GitHub Pages, which is what devices check. The process, the two
changes that need more care than a tag can undo, and how to verify what is actually served are
in [`docs/ORIENTATION.md`](docs/ORIENTATION.md) under *Releasing*.
