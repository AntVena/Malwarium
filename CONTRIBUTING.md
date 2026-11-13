# Contributing to Malwarium

Players start at `README.md`, which gets a device working. This file is for developers: the
constraints the code is written against, where the standards live, and how to run the gates.

## Hard constraints (non-negotiable)

ST7789 **240×240** RGB565 · active canvas **224×224** (8px bezel, no UI in it) · author at
**128×128 logical** (rect 128×64) · **×1.75 integer canvas upscale, no resampling** (a sprite's
scale is set per blit, and the combat stage draws its fighters at 1/1; see `RENDER_PIPELINE.md`)
· ~4fps event-driven repaint (redraw on change) · 2-frame loops are the norm · pet cell **56×48
logical** (max 128×64) · buttons **A=Next · B=Accept · C=Cancel**, with **A+C** the Exploit
chord (Hacker face, combat override, egg crack).

**Each button's HOLD is the bigger version of its tap**, and there are only three: hold **A**
to repeat the step, hold **B** for the screen's second action (the ITEMS type filter, the
MOVES show-all, the VAULT bulk-open, the CFG factory reset), hold **C** to walk a list
backward. A hold never means something unrelated to the tap under it, and C's tap is Cancel
on every screen, with no exceptions. The row-stepping half is implemented once, in
[`game_listnav.cpp`](src/core/app/game/game_listnav.cpp); a screen joins it with one
`listTarget` entry (how it steps, what C does there) and steps with `stepRow` rather than its
own cursor arithmetic. B's tap/hold pair is routed the same way, in
[`game_holds.cpp`](src/core/app/game/game_holds.cpp): a screen joins with one `holdTarget`
entry naming its hold and release. A screen type that owns its list or hold joins with
`screenList` or `screenHold`. A confirm before an action that cannot be taken back is an
`InlineConfirm`. A tap/hold button settles on the
RELEASE edge, so anything driving one (a gate, `tools/dump_frame.cpp`) must send both edges.

**Dual-coding release gate:** every status meaning carries a non-colour channel, so **a
grayscale screenshot of any screen stays fully readable.** The pipeline all of this describes
is [`src/core/render/RENDER_PIPELINE.md`](src/core/render/RENDER_PIPELINE.md).

**Locked vocabulary:** *Malwarium* = the device/habitat · *malbeast* = a wild creature ·
*petware* = a tamed pet · *the 'net* = the world. You **tame by raising**, never by capturing.

## The `game/` module rule

`Game` is one class split across `src/core/app/game/game_*.cpp` units (all including
`core/app/game.h`). New behaviour goes in the matching unit: feeding → `game_items`, draws →
`game_render`, and so on. If a unit would pass ~600 lines, add a new one: split **at** the
growth, by concern, not ahead of it. Helpers shared by two or more units go in
`game_internal.h` (private), never `game.h`.

## Structure

Write the shape you would choose for the job from scratch. New code tends to copy its nearest
sibling, so check the sibling against these rules before copying it. When existing code falls
short of them, fix it or add a row to the board.

A bug fix or speed-up rewrites the code it touches, so it meets the same bar. Before landing
one, ask of each function and type you changed whether it is how you would write it today.
If it is not, bring it there in the same change. Then look one layer up and one layer down for
the same smell (a copied loop, a rebuild per call, a flag argument): fix what is small, and add
a board row for what is not. The smallest patch that makes the symptom go away is not done.

- **A list is one table.** Pages, rows, slots and tabs live in one ordered table. The table's
  order is the order on screen, its length is the count, and code finds an entry by its name
  or its object. A number that means "the third page" is a second copy of the table.
- **One home per shared thing.** Drawing goes in `core/ui/widgets.h` and screen geometry in
  `core/ui/layout.h` (`listScrollTop`, the row grid). `game_internal.h` holds what only `game/`
  uses. Look for a helper before writing one. If it sits in a header your code can't include,
  move it to one every caller can reach, then call it.
- **One reason to change per type.** A feature owns its state and its input handlers in its own
  type (a record, a run or a screen, below), and `Game` composes them and routes input to the
  active one. `Game` itself breaks this rule today; board section 1j extracts its state one
  type at a time.
- **Dependencies point down the layers:** `app`, `ui`, `model`, `content`, then `render`, `net`,
  `audio` and `device`. A header includes from its own layer or a lower one. An interface
  belongs to the layer that calls it, and the platform implements it: `core/device/platform.h`
  is the interface, and `src/platform/<target>/` holds the implementations.
- **A dispatcher only dispatches.** It is a `switch` over the vocabulary whose cases call the
  code that owns each case. Headers, layout constants and per-screen state live in that code.
- **Route input by state.** Behaviour that differs per screen reaches the screen through the
  same dispatch as its taps. Guards that each work out which screen is up from
  `face_`/`nav_`/a sub-view drift apart.
- **Every input has exactly one owner at any moment.** A press goes to the screen that is up
  and to nothing else; no input is broadcast for several handlers to share. A UI gesture from
  another sensor (a shake or tilt off the IMU, a touch) is shaped into a gesture by the
  platform layer, the way buttons are debounced, and reaches the screen through that same
  dispatch. A world event (a radio frame, a network sighting, a status snapshot) goes to the
  run or record that owns it through its own `Game` entry point (`onPvpFrame`,
  `registerNetwork`), whatever screen is up, and the screen reads the result.
- **Share what is the same, and explain what differs.** When two sites draw or compute the
  same thing, they call one function. When one site must differ, its comment says why. A
  comment reading "same as X" means the two are kept equal by hand.
- **Few arguments, and no flags.** A screen's draw function takes the framebuffer and, once it
  needs more than four other inputs, one view struct (`CombatView`, `StoryPageView`). A widget
  primitive takes its geometry and colours as plain arguments. Neither takes a `bool`: use an
  enum the call site names (`Overflow::Scroll`), or a named field in the view struct.
- **Small functions.** A long `switch` over a vocabulary is fine. Any other function that
  needs a scroll to read is several steps, and each step gets a name.
- **Name a number once, where its reader is.** This is the `tunables.h` rule applied to
  layout. A magnitude one screen reads lives on that screen. One that two screens share is a
  single named constant.

### Where state lives: records, runs and screens

Three kinds of type hold the game's state. `Game` holds one of each record, the active run and
the active screen, and routes input and ticks to them. A field added directly to `Game` belongs
to one of the three and goes there instead.

- **A record** is something with an identity that the save keeps: the `Pet`, the `Operator`
  (the player, across every pet) and the device `Settings`. It owns its data and the rules
  over it, and it maps itself to and from the save. Code reads a record by const reference
  and changes it through the record's own methods. Storing a pet on the ARCH rack, hatching a
  new egg and a factory reset each move or replace a whole record. `PetModel`, `Inventory`
  and `PetUpgrades` are parts of the Pet record.
- **A record is grouped by lifetime.** Everything about a pet is on `Pet`, however long it
  lasts: what it keeps for life (upgrades, Bond, palate, a USB patch, a once-per-life grant
  and its effect), its progress through the current life (level, stage timer, care mistakes),
  and what is armed on it for now (a Backup Drive, an Ambig-USB divert, a DeepWeb arming, a
  Lockout). Each lifetime is its own part of the record, and the lifetime decides which part,
  never which record. So a pet's state is never found on the `Operator`, a run or a screen.
  The part also decides what a move does to it: storing a pet on the rack keeps the first two
  parts and clears the armed part, since anything in it can be armed again.
- **A run** is one activity with a start and an end: an explore walk, a fight, the boss
  gauntlet, a tourney, a PvP session, an arcade game. It owns its clocks, its random draws and
  its result, and is destroyed when it ends. A run that must survive a reboot maps itself to
  the save the way a record does. `Combat`, `Cryptogram` and `DiskDecryption` are runs.
- **A screen** is the UI over records and runs: its cursor, scroll, confirm state, input
  handlers and draw. It holds nothing that must outlive it, so leaving a screen loses no
  progress. It reads records by const reference and reaches the game through a narrow host
  interface. `ItemsMenu` and `RigShopScreen` are screens.

To place a new field, ask whose it is:

| It belongs to | Put it on |
|---|---|
| this creature, for life or for now | `Pet` (the part for its lifetime) |
| the player, whichever pet is active | `Operator` |
| the hardware | `Settings` |
| one activity, while it runs, and no pet's | that run |
| one screen, while it is open | that screen |

A record's save mapping lives on the record, so adding a field is one edit to the type and one
to its mapping. Freeze, thaw, reset and the save all go through that mapping.

## Comments and doc prose

**A comment says only what the code cannot: *why* it is this way, and *where else* it is
used.** Keep it to a line or two. Never restate what the code does. A restatement goes stale
when the code changes, and then it disagrees with the code and reads like a bug. If deleting a
comment would not leave a reader confused, delete it. A comment never cites the planning board
([`docs/MASTER_TODO.md`](docs/MASTER_TODO.md)); its cards stay out of the code.

So: no planning or milestone IDs, no dates or session tags, no change-narration ("the old X",
"used to", "now lives on"). Describe the **current** state. Reference other code only when a
reader needs to go there, and name the symbol over the file. A rule saying a comment is
*allowed* never makes one *required*: the deletion test above still decides. Docs follow the
same rule: cite how and where, never when. `git log` is the changelog, and prose that narrates
change competes with it and loses.

Full rules and the keep-anyway exceptions (EAPOL `M1–M4`, board and pin names, world
`Area 0–3`, living how-to standards): [`docs/COMMENT_STANDARD.md`](docs/COMMENT_STANDARD.md).
`python3 tools/check_comment_standard.py` is the mechanical check, and it runs in the gates.

**Write plainly.** Comments and docs are technical instructions. State the fact, the reason
and the place, in the fewest plain words that carry them. No em dashes: use a period, colon,
comma or parentheses (the comment gate fails on one). No contrast framing ("X,
not Y", "not just X but Y"): state X, and name an alternative only when a reader would
otherwise choose it, with the reason it fails. No sentences that sound like insight but
carry no fact ("which is the whole point", "and that is the trade"). If a sentence can be
deleted without losing information, delete it. Details and examples:
[`docs/COMMENT_STANDARD.md`](docs/COMMENT_STANDARD.md) under *Write plainly*.

Transient state gets no file. A session summary or handoff is not a document; anything durable
becomes a row on the board.

## Where the standards live

Each lives in the folder it governs. This table is for finding them cold.

| If the task involves… | Read first |
|---|---|
| Re-orienting, "what's open" | [`docs/MASTER_TODO.md`](docs/MASTER_TODO.md) |
| The cross-cutting picture: repo map, stack, carousel, care model, currency, radio, releasing | [`docs/ORIENTATION.md`](docs/ORIENTATION.md) |
| Adding a milestone-sized feature | the board (open work) + [`docs/TEST_STRATEGY.md`](docs/TEST_STRATEGY.md) ("done" criteria) |
| Content (items, mods, moves, creatures) or a balance constant | [`src/core/content/README.md`](src/core/content/README.md) |
| An EXPL area, its bosses/shop/mod pool, or adding a new one | [`src/core/content/areas/README.md`](src/core/content/areas/README.md) |
| Naming a creature, item, area or sub-area | the naming docs beside the tables, routed from those two |
| Art, palette, sprites, what exists vs. what's a placeholder | [`assets/README.md`](assets/README.md) |
| Any UI screen | [`assets/VISUAL_LANGUAGE.md`](assets/VISUAL_LANGUAGE.md) for the system; a shipped screen's behaviour is `src/core/ui/` + `game_render*.cpp`, which is the contract |
| A render pass or visual effect | [`src/core/render/RENDER_PIPELINE.md`](src/core/render/RENDER_PIPELINE.md) |
| Pins, board peripherals, bring-up | [`include/config.h`](include/config.h) is the pin authority; [`src/platform/esp32/HARDWARE.md`](src/platform/esp32/HARDWARE.md) for the board |

**Source types.** `assets/` and `PAL_CORE.json` are final data. Reproduce them exactly, and
use the existing token for any colour that has one. `include/tunables.h` holds cross-cutting
balance only; a magnitude one entity reads lives on that entity's row. A shipped screen is its
own contract.

## Building and testing

Two tiers, and neither substitutes for the other. The native suite is fast and covers
`src/core`; the S3 build is the only thing that compiles `src/platform/esp32`, so a green
native run says nothing about whether the firmware still links.

```bash
./tools/gates.sh
```

Quiet on success, and on failure it prints only the lines that explain it. Between the two tiers
the full run also checks that `web/data/pedia_data.js` is fresh (`make pedia-check`), because a
content or sprite edit moves it without touching anything under `web/`; run `make pedia` and
commit the result when it fails. `--native` skips both that check and the firmware build for a
tight edit loop; `--verbose` streams everything. The same checks run in CI on every push
(`.github/workflows/gates.yml`).

`src/generated/` is compiled from `assets/` by `tools/gen_assets.py` and is not committed. The
gates regenerate it, so a fresh clone needs no extra step.

**Editor diagnostics.** The two tiers build on two toolchains, and clangd can infer neither, so
each pins its own compile database: `.clangd` at the root points at `build/` (what the gates
configure) and `src/platform/esp32/.clangd` at `build-device/`. Run `./tools/compiledb.sh` to
regenerate both after a fresh clone and whenever the include set changes. With them in place a
clangd error is a real error on either tier; there is no "device code always looks broken" to
read past.

**From a Claude Code on the web session.** `.claude/hooks/session-start.sh` installs what `tools/`
imports and runs the codegen, so a fresh container can build and test on arrival. Run
`./tools/gates.sh --native` there: the sandbox network policy refuses the PlatformIO registry, so
the S3 tier cannot fetch its toolchain and cannot run at all. CI runs both tiers on pushes to
`claude/**`, `ccr-*` and `main`, so a remote session's push is checked without a pull request. A
session has no write on `refs/tags/*`, which costs nothing: releases here are cut by dispatching
`publish.yml`, not by tagging (see *Releasing*). Pages deploys only from `main`, so **dispatch
publish after the work lands there**. Dispatched on a branch, it fails with an empty deploy job
and no log.

Neither tier can see two things drawn on top of each other, or a panel clipping its own
text. `./tools/screens.sh` renders every screen to one contact sheet so you can look for
both. It fails nothing. Run it after touching the layout grid, the font, or a shared
widget.

Commit verified work as logical units. A message should carry the motivation and the *why*, not
a restatement of the diff.

## Releasing

**Work lands on `main`, as a release.** No pull request unless one is asked for, and nothing
parked on a side branch waiting for review: finish it, run the gates (`./tools/gates.sh`), bump
the versions, cut the release.

**A release goes live only when `publish.yml` runs.** `publish.yml` is what builds the
firmware artifact, writes the manifest a device polls, stages the boot images `/flash/` writes
and deploys the lot to Pages. Skip it and the release is not live: nothing is offered to any
device, and the only symptom is a version nobody is running.

**Dispatch `publish.yml` on `main` as soon as the release commit lands, every time, without
asking.** Nobody reviews a release before it ships here, so there is nothing to wait for.
Tags are not used: `make pages` reads the version from `include/version.h` and
`web/VERSION`, never from the ref, so a dispatch publishes exactly what a tag would, and the
release commit is the repo's record of it. (The workflow still answers a pushed `v*` tag;
nothing needs one.)

The process, the two changes that need more care than a tag can undo, and how to verify what is
actually served are in [`docs/ORIENTATION.md`](docs/ORIENTATION.md) under *Releasing*.
