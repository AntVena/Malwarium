# Malwarium — Claude entry point (routing table)

ESP32 handheld virtual pet — a *containment habitat* for malware. Hatch a sealed egg and
raise the creature through **Boot Sector → Process → Script → Daemon** by feeding it,
de-fragging it, keeping it happy. Tone: lighthearted infosec/demoscene punnery.
**Locked vocabulary:** *Malwarium* = device/habitat · *malbeast* = wild creature ·
*petware* = tamed pet · *the 'net* = the world. You **tame by raising**, never capturing.

## Hard constraints (non-negotiable)

ST7789 **240×240** RGB565 · active canvas **224×224** (8px bezel, no UI in it) · author at
**128×128 logical** (rect 128×64) · **×1.75 integer upscale, no downscaling** · ~4fps
event-driven repaint (redraw on change) · 2-frame loops are the norm · pet cell **56×48
logical** (max 128×64) · buttons **A=Next · B=Accept · C=Cancel**, with **A+C** the Exploit
chord (Hacker face, combat override, egg crack). **Dual-coding release gate:** every status
meaning carries a non-colour channel — **a grayscale screenshot of any screen must stay fully
readable.** The pipeline these describe: `src/core/render/RENDER_PIPELINE.md`.

## The `game/` module rule

`Game` is one class split across `src/core/app/game/game_*.cpp` units (all include
`core/app/game.h`). New behaviour goes in the matching unit (feeding → `game_items`,
draws → `game_render`, …); if a unit would pass ~600 lines, add a new one — split **at** the
growth, by concern, not ahead of it. Helpers shared by 2+ units go in `game_internal.h`
(private), never `game.h`.

## Comments & doc prose

A comment orients a reader *in the moment*: **what** a thing is for, **how** to use it, or **what
consumes it** (worth saying when that's another file). It **never points back at the planning
board** — `docs/MASTER_TODO.md` is a Kanban board that decides *what* to build, and the card
doesn't go in the code. So no planning/milestone IDs (`FB-*`/`D#`/`S#`/`M#`/`Phase N`), no `(PO)`,
no dates/session tags, no change-narration ("the old X", "used to", "now lives on") — describe the
**current** state, and reference *other code* (files, symbols) freely. Docs are forward-facing too:
cite how/where, never when (`git log` is the changelog). Full rules + the keep-anyway exceptions
(EAPOL `M1–M4`, board/pin names, world `Area 0–3`, living how-to standards):
`docs/COMMENT_STANDARD.md`.

## Routing table — read the pointed file before working

Standards live in the folder they govern, and each of those folders has its own `CLAUDE.md` that
loads when you work there. This table is for finding them cold.

| If the task involves… | Read first |
|---|---|
| Re-orienting, "what's open" | `docs/MASTER_TODO.md` — the one board |
| The cross-cutting picture: repo map, stack, carousel, care model, currency, radio | `docs/ORIENTATION.md` |
| Adding a milestone-sized feature | `docs/MASTER_TODO.md` (open work) + `docs/TEST_STRATEGY.md` ("done" criteria) |
| Content — items, mods, moves, creatures, or a balance constant | `src/core/content/CLAUDE.md` |
| An EXPL area, its bosses/shop/mod pool, or adding a new one | `src/core/content/areas/CLAUDE.md` |
| Naming a creature, item, area or sub-area | the naming docs beside the tables, routed from those two |
| Art, palette, sprites, what exists vs. what's a placeholder | `assets/CLAUDE.md` |
| Any UI screen | `assets/VISUAL_LANGUAGE.md` for the system; a shipped screen's behaviour is `src/core/ui/` + `game_render*.cpp`, which is the contract |
| A render pass or visual effect | `src/core/render/RENDER_PIPELINE.md` |
| Pins, board peripherals, bring-up | `include/config.h` is the pin authority; `src/platform/esp32/HARDWARE.md` for the board |
| Writing comments or doc prose | `docs/COMMENT_STANDARD.md` |

**Source types:** `assets/` + `PAL_CORE.json` = final data, reproduce exactly, never re-invent a
colour · `include/tunables.h` = cross-cutting balance only, single-entity magnitudes live on their
row · a shipped screen is its own contract.

## Sub-agents — team-lead policy

The main session owns design, decisions, and cross-file reasoning.

**A spawn costs ~60k tokens before the agent does any work.** Each pre-defined agent is *warm* —
it starts with this file and the routed docs already loaded, so it never re-derives the repo, and
that preload is a fixed floor whether the task is one command or fifty. The test is therefore not
"is this read-heavy?" but **"does delegating save more than ~60k tokens of parent context?"** Most
of the time it doesn't — do the work inline.

**Gates are the clearest thing NOT to delegate.** Redirect the output and read it only on failure;
that costs tens of tokens inline against ~60k to hand it over:

```bash
LOG="${TMPDIR:-/tmp}/malwarium-gates.log"
cmake -S . -B build > "$LOG" 2>&1 && cmake --build build >> "$LOG" 2>&1 && ctest --test-dir build --output-on-failure >> "$LOG" 2>&1; echo "exit=$?"
```

Grep `"$LOG"` for `error:` / `FAIL` **only** when the exit code is non-zero. Same shape for
`pio run -e waveshare_s3_154`. This is the default for the build→test→commit loop.

**Worth a spawn:** work whose raw material is genuinely huge and whose answer is small — a sweep
where the alternative is reading dozens of files into the parent. Three are pre-defined in
`.claude/agents/`: **`gate-runner`** (Haiku — the native/S3 gates, failures only; prefer the
inline recipe above), **`subsystem-mapper`** (Sonnet, read-only — "how/where does X work" across
the `game_*.cpp` spine → `file:line` + the seam), **`doc-surveyor`** (Sonnet, read-only — sweeps
the docs for a decision/status question → answer + citations).

Sub-agents return **distilled findings**, never raw output; review every summary before acting on
it. Don't spawn one for anything answerable in a couple of greps. **Never** spawn a sub-agent to
summarise something you're going to have to read in a few turns anyway — that's double-handling.

## Session workflow

`/sync-docs` at the end of any session that changed what's built or blocked ·
`/handoff` before pausing >1 hour · `/design-brief` to generate an art handoff ·
**"let's do a maintenance run"** to pick up the oldest item in `docs/MAINTENANCE.md` (self-review:
docs cleanup, architecture review, design-consistency pass, stale-reference sweep, …).
Commit verified work as logical units; messages capture prompt, motivation, and why.

**No doc for transient state.** A session brief, handoff or summary goes in the chat as a copy
block — never a file. Anything durable becomes a row on `docs/MASTER_TODO.md`.

**Build/test cycle: S3 + native only.** The normal build→test→commit→push→flash loop builds
`waveshare_s3_154` (primary target) + the native gates.
