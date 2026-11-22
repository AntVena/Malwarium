# Malwarium: Maintenance Routine

A recurring self-review pile. Start a session with **"let's do a maintenance run"**: pick the item
below with the oldest "Last run" date, do it fully, then bump its date and commit. The date is the
only breadcrumb this file keeps; what the run actually found goes in the commit, and anything left
open becomes a row on `docs/MASTER_TODO.md`.

**Philosophy:** docs describe **current state and open work**. `git log` is the changelog. The
specific rule: cite **how/where**, never **when**. Leave out *when* something was decided: a date
doesn't change what a reader should do. *How/where* (the mechanism, the file:line, the structural
reason something is the path of least resistance) is worth keeping, because it lets a reader judge
whether the reasoning still holds, and it usually carries the *why* for free. Verify a doc's
claims against the code before relying on them. Nothing keeps a doc in sync with the code.
Maintenance runs keep the docs that way through small, regular passes.

## The pile

### Docs cleanup / decision-history sweep. Last run: 2026-11-21
Two surfaces, two rules, and one prose style across both.

**Docs (`docs/`, and the standards beside the code):** cull dated historical narration (session logs, "shipped on <date>",
"supersedes the section above"), session-provenance framing ("this session found…"), and migration
narration ("X now lives on…", "the old Y", "previously applied inside…", "[retired]"). Keep
still-open items and the rare bare "Last updated: <date>" freshness marker.

**Code comments (`src/`, `test/`, `tools/`, `include/`):** enforce `docs/COMMENT_STANDARD.md`. A
comment says what the thing is FOR / how to use it / what consumes it, and never points back at the
planning board. Sweep for the strip-list there (`§`-refs, spec-doc names, `FB-*`/`D#`/`S#`/`M#`/`Phase
N` IDs, `(PO)`, `backlog`/`Feedback`/`MASTER_TODO`, dates/session tags, change-narration) and honour
its keep-anyway exceptions (other-code refs, living how-to standards, `vNN` save notes + migration
tests, and the domain terms that only look like refs: EAPOL `M1`–`M4`, `S3`/`C5`, `D0`–`D3`, world
`Area 0`–`3`). Cross-check any claim against the actual code before trusting it.

**Prose style (docs and comments):** hold both to COMMENT_STANDARD's *Write plainly*. Comments
hold no em dashes (the gate fails on one), and neither should docs: rewrite any you find. Rewrite
contrast framing ("X, not Y", "rather than", "not just") into a plain statement where the
contrast carries no decision, and delete sentences that add emphasis without a fact. Docs
first: they are read more often than any one comment.

### Self-architecture review. Last run: 2026-11-21
Check the `game_*.cpp` module split for size creep past ~600 lines. Look for cross-file duplication,
dead code, and orphaned helpers that should have moved to `game_internal.h` or been deleted. Verify
actual file sizes, don't trust what a doc says they are. Sources are globbed (`CMakeLists.txt`,
`build_src_filter`), so a new unit needs no build-file edit, and a split costs only the time to
review it. When a unit is big but one concern (a dispatcher, a flat save mapping, a
screen-per-function render file), leave it and say so: length that follows from the number of cases
is not creep.

**Audit against CONTRIBUTING's *Structure* section.** Its rules say how code here is
written; this run finds where the code falls short. Look for:

- the same widget or calculation at more than one site (grep its SHAPE, since copies go by
  different names);
- a helper in a header its would-be callers can't include;
- a dispatcher that does its cases' work;
- copies that have drifted apart;
- an include that points up the layers (`app`, `ui`, `model`, `content`, then `render`,
  `net`, `audio`, `device`): `git grep '#include "core/<higher layer>/' -- src/core/<lower layer>`.
  The last four are peers; `core/device/platform.h` takes the two leaf vocabulary headers
  `core/render/color.h` and `core/audio/sound.h` from them, which is expected;
- a draw function with more than four inputs, or a `bool` parameter;
- a type with more than one reason to change. `Game` is the standing case, tracked on the
  board; a new feature added straight into `Game` is a finding.

Verify a refactor that touches drawing against `./tools/screens.sh` rendered BEFORE the
change. The frames are deterministic, so `diff -rq` of the two directories is a
byte-for-byte proof.

**Measure functions as well as files.** Mass collects inside individual functions, where a
file-size check cannot see it: a unit can sit near 600 while one function in it runs past 400.
`python3 tools/smells.py long` lists every function over 80 lines with its `case` count, then
apply the same one-concern test. The dispatcher exception is the one to apply honestly:
**count the `case` labels before granting it.** A long `switch` over a vocabulary is fine; a
long if-chain is accumulated special cases wearing the same length, and the two are
indistinguishable by line count alone.

**The smells, by name.** These are the shapes that have turned up here, each with the case
that showed it. Run `python3 tools/smells.py` for the finders; the judgement is yours.

- **A formula written out where it is used.** The LCG step was typed in 66 places and the
  derived-sprite name (prefix plus upper-cased id) in six, each copy a place a change can miss.
  `smells.py repeated` lists any statement in four or more files. A copy that sits next to a
  comment saying it must match another is the same smell, admitted.
- **A pair kept equal by hand.** Two functions that must mirror each other (the save codec's
  writer and reader) drift with nothing to catch it but a round trip. State the pair once.
- **A function that is its own steps.** A function past a screen whose body is a sequence of
  commented blocks wants each block named (`applyCombatResult` became five calls).
- **A dispatcher doing its cases' work.** A `switch` whose cases run fifty lines each
  (`onPvpFrame`) is a dispatcher only once each case is one call.
- **A flag argument.** A `bool` that picks a behaviour reads as a bare `true` at the call
  site; an enum the call site names says what it picks (`Opener::Enemy`). `smells.py flags`
  lists every `bool` parameter; one that carries a value (`won`, `ok`, a setter's `on`) is fine.
- **Feature envy.** `Game` reaching into several of one record's fields to run a rule (the
  dying window) means the rule belongs on the record.
- **Bookkeeping by hand.** Code that counts its own separators, indices or commas (the 'Pedia
  JSON's first-flags) wants a small type that keeps the count.
- **A second copy of a roster.** An if-chain over content ids (`"worm"`, the four strike
  sheets) is a table the content already holds. Ask the row, or derive the name.

Fix what is small in the run, and put the rest on the board with its size.

### Design consistency pass. Last run: 2026-11-21
Spot-check shipped screens against the system they're authored to (`assets/VISUAL_LANGUAGE.md`,
`assets/CREATURE_VISUAL_RULES.md`) and against each other. Look for a screen that solves a
problem differently from its siblings. Fix the standard to match reality, or if the drift looks
like an undocumented real decision, surface it explicitly.

**A rule serves the player, so it bends before a screen does.** When several screens break a
written rule the same way and read clearer for it (A SWITCH on the action pickers, C DELETE in
the text editor), the rule was too narrow: write the case into the standard. Change the screens
only where the departure is the less clear of the two.

### Screen separation pass. Last run: 2026-11-21
Take the contact sheet (`./tools/screens.sh`) and go screen by screen through every one that
stacks **multiple distinct row groups**, asking one question: can a reader tell where one group
ends and the next begins, without reading the words?

The trap this run exists to catch is **emphasis spent where separation was already free**. The
item detail page is the worked example: its `ITEMS` banner carries `FontFace::Bold`, but that
banner already sits at the top of the screen with a rule under it. It was never the thing a
reader could lose. The groups below it (the readout grid, the prose, `HAVE`, the action line)
are the ones running together with nothing between them, and they get no help. Emphasis landed
where it was cheapest to apply rather than where the screen needed it.

Use these four levers, in this order. The cheapest one that works wins:

- **Spacing and grouping.** Bands whose height is reserved regardless of content (the detail
  pages' `kDetailPanelTop`/`Bottom`) leave dead gaps mid-screen and none at the seams.
- **Rules and indentation**, which the header band already uses.
- **`Pal::INK` vs `INK_DIM`**, the separation that costs nothing and survives grayscale.
- **`FontFace::Bold` last**, and only where the other three don't reach. A screen gets one
  emphasised element (VISUAL_LANGUAGE §2.3), so a new use takes that slot.

Where a screen reads fine, say so and move on; the output that matters is the handful
that don't. Anything needing a real layout change becomes a row on `docs/MASTER_TODO.md` rather
than being done inside the run.

Two things the sheet will show you that are NOT findings. A name cut mid-word on a list row is
usually `drawTextMarquee` caught mid-scroll. The Hacker face's SHOP/CREW rows look truncated in
a still and scroll on the device. And a band that ends short of the screen is only a defect if
the group BELOW it has nothing separating it; empty canvas under the last group is just empty
canvas.

### Stale cross-reference sweep. Last run: 2026-11-21
Grep for links/citations across the docs (file paths, line numbers, `D#`/`S#`/`C#`/`FB-*` row
IDs), and verify they still resolve to something real. Fix or remove dangling references.

**Check symbols and section numbers first; paths rarely break.** A path-shaped scan is almost all
noise here, because the docs cite by basename and partial path on purpose (`game.h`,
`core/ui/theme.h`), and because four classes of unresolvable path are correct: generated artifacts
(`malwarium.wasm`, `manifest.json`, the boot images), the untracked `_attic/`, a hypothetical path
introduced by *e.g.* (`pirate_bayou/bosses.cpp`. An illustration of a split that has not
happened), and `diagram.json`, which `sim/README.md` says is deliberately uncommitted. Resolve a
citation by SUFFIX against the tracked set before believing it dangles.

The two that pay are the ones a mechanical grep for links never reaches:

- **Symbol citations**: a backticked identifier is a claim about code, and code is renamed under
  docs that are not rebuilt. Extract the constants, *Type::member* pairs, camelCase names and
  *fn()* calls out of the backticks and check each against the tree. Look for near-misses: a name
  that reads right but resolves to nothing, one noun off what the code calls it (a *RampFor*
  where the member is a *SpriteFor*, a *Tokens* where the type is a *Grid*). Note a miss in prose
  rather than pasting the dead name back in, or this entry becomes the next run's own dangling
  symbol. When one is wrong, read the surrounding prose too: a citation that stale usually
  describes the superseded DESIGN as well, not only the old name.
- **`§N` section refs**, which have to be resolved against the TARGET doc's headings, not the
  citing one. Two conventions look like breakage and are not: `VISUAL_LANGUAGE §0.4` addresses
  the fourth numbered item inside §0 rather than a subsection, and a bare `§N` in a sentence
  naming another doc belongs to that doc. The breakage that does happen is a number pointing at
  the wrong section: a lettered manifest still cited as `§4` for what is now `§D`, or `§4` used
  to mean *stage 4* in a doc where `§` means section.

Also check that a doc which ROUTES to another for a process actually finds it there: the routing
line and the section it points at are two places one process is described, and only one of them
gets updated.

### Test/gate health check. Last run: 2026-11-21
Run the gates. Confirm native gates and the S3 build are actually green, not assumed green from
a doc. Look for test debt: tests asserting behaviour that no longer occurs in
real play.

**Check that each gate RUNS.** Every harness or command
`docs/TEST_STRATEGY.md` names has to appear in `tools/gates.sh` or `.github/workflows/gates.yml`,
and the workflow's branch filter has to match the branch a session pushes to. If either is
missing, the gate is green only because it never ran.

**A test that returns before its first `CHECK` passes vacuously** whenever its precondition is
false. Scan for early `return`s ahead of any `CHECK`, and make a compile-time precondition
checked both ways rather than skipped.

**For behaviour no longer in real play, look for production code that only tests call**: a
non-debug `Game` method with no caller under `src/`. Most are read-only accessors over state play
produces, and those are fine. The ones that matter carry LOGIC nothing in play runs,
which the tests then hold in place.

### Content/tunables standard drift. Last run: 2026-11-21
Check new content against `src/core/content/CONTENT_STANDARD.md`: structured effect vocabulary (no new
scalar-field sprawl or `if (id == "...")` branches), one-file-per-type under
`src/core/content/content_*.cpp`, and (the easy grep) no single-entity magnitude in
`tunables.h`. Sweep: `grep -nE 'constexpr .* k\w+ *=' include/tunables.h` and flag any whose
comment names ONE item/mod/move/creature. Inline it onto that row. Verify claims against code.

### Unused-include sweep. Last run: 2026-11-21
The clangd "included header X is not used directly" warnings are noise to read past. The
`game_*.cpp` units collect the most, because a new unit copies its sibling's include block.

**What a scan can test.** `game.h` takes `core/ui/ui_state.h` for the screen ids `Game` holds and
no screen header, so stripping a screen header is a real test of whether a unit draws through it.
`game_render.cpp` is the render dispatcher and keeps every screen header; every other unit keeps
only the screens it enters. Nothing `game.h` includes pulls `canvas.h`, `font.h`, `palette.h`,
`sprite.h` or `generated/assets.h`, so those are testable in every unit.
Re-run the sweep when a unit is split or a header moves.

**What a scan cannot test** is the `core/model` half: `game.h` hands every TU `combat.h`,
`save.h`, `registry.h`, `framebuffer.h` and `platform.h`, so "it still compiles without it" proves
nothing about those until `game.h` stops including them.

**Grep a header's constants as well as its functions and types.** A symbol scan built from
function names alone would clear `canvas.h` out of `game_core.cpp`, which uses it only for
`kHeartbeatMs`/`kCombatAnimMs`. The build is the gate; the scan is only the finder.

The finder doesn't need clangd. `clang-tidy -p build -checks='-*,misc-include-cleaner'` over
the `src/` entries of `build/compile_commands.json` runs the same analysis, which is what a
session without clangd (a remote one) reaches for. Its blind spot is the one `game.h` creates:
a type `game.h` only forward-declares, used through a `Game` member that returns it by value
(`statSources()`'s `StatSources`), is instantiated in the calling unit without the
unit ever naming it, so the scan calls the header unused and the build disagrees. A
template whose body sits in `game_internal.h` (`screenHold<>`) is the same blind spot at
link time. Put the reason on the include line when the build says keep, or the next run
strips it again.

Method: trust the clangd `unused-includes` diagnostics as the finder, **verify a real host build
still compiles after each removal**, and note that the native gate is authoritative. Don't
remove a device-only header while building host-only, or vice-versa. The device tier has its own
compile database (`tools/compiledb.sh`, `src/platform/esp32/.clangd`), so its diagnostics are
worth the same trust as the host's; a file lighting up whole means the database is stale. Removable is not the same as
wrong: an include a unit uses DIRECTLY stays even when some other header happens to supply it, and
the trap in a mechanical scan is a symbol that only appears in a comment.

### Asset manifest accuracy audit. Last run: 2026-11-21
Cross-check `assets/ASSET_MANIFEST.md` status markers (☑/▨/☐) against what's actually in
`assets/` and wired into `embedded_content.cpp`. Flag mismatches in either direction
(claimed-delivered-but-missing, or shipped-but-still-marked-TODO). Audit the **File** column
as well as the marker: a path goes stale when files move between `icons/`, `sprites/` and
`ui/`, and no status marker shows it. An `_attic/` file may share a basename with a live one
(the superseded drawing), and a naive stem→path map resolves it to the parked copy. The attic
is gitignored, so a `⌫` path is checkable only against `git ls-files assets/_attic`: a
missing one means the drawing never left the machine that parked it, which is not something a
manifest edit can fix.

**Three ways a marker check cries wolf**, all of which look like "claimed ☑, not compiled":
`engine-drawn` rows (`UI_GAUGE`, `UI_SCROLLBAR`, `UI_HEALTH_BAR` …) are delivered as code and are
correctly absent from `assets.h`. Read the File column before flagging; a row id can be a FAMILY
name that expands (`ICON_LOG_EVENT` → `_ITEM`/`_WARN`/`_COMBAT`, `UI_OVERRIDE_PIP` →
`ICON_OVERRIDE_PIP{,_SPENT}`); and `assets.h` declares sprites as `ASSET_<ID>`, so an id-prefix
grep that forgets a prefix (`BG_`, `FX_`) invents missing rows. Check the expansions and the
`ASSET_` form before believing any mismatch.

**Also ask whether the row should exist at all.** Any asset whose id is derived from a content
row (`ICON_ITEM_<ID>`, `ICON_MOD_<ID>`, `ICON_MOVE_<ID>`, a creature's `spriteName`) must NOT
have a per-row table in the manifest. That is a hand-maintained mirror of `content_*.cpp` that
drifts without failing anything. The manifest's lede states the rule; enforce it. A row earns its
place only by carrying an art *rule* or a quality judgement no code holds.

---

Add a new entry here when a new recurring quality dimension comes up. Keep each entry to *what it
checks*, *where*, and what currently blocks it.
