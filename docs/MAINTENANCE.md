# Malwarium — Maintenance Routine

A recurring self-review pile. Start a session with **"let's do a maintenance run"** — pick the
item below with the oldest "Last run" date, do it for real (not a superficial pass), then bump its
date and commit. The date is the only breadcrumb this file keeps; what the run actually found goes
in the commit, and anything left open becomes a row on `docs/MASTER_TODO.md`.

**Philosophy:** docs describe **current state and forward-facing work**, not a changelog — `git
log` is the changelog. The specific rule: cite **how/where**, never **when**. *When* something was
decided rarely matters — a decision can be revisited at any time regardless of its age, so a date
doesn't change what a reader should do. *How/where* — the mechanism, the file:line, the structural
reason something is the path of least resistance — is worth keeping, because it lets a reader judge
whether the reasoning still holds, and it usually carries the *why* for free. Don't trust a doc's
own narrative — verify claims against the actual code before relying on them; a doc can go stale
the moment it's written and nothing enforces it silently. A maintenance run is the mechanism that
keeps this from drifting back into changelog-shaped docs — regular, bounded passes instead of one
overdue cleanup.

## The pile

### Docs cleanup / decision-history sweep — Last run: 2026-08-29
Two surfaces, two rules.

**Docs (`docs/`, and the standards beside the code):** cull dated historical narration (session logs, "shipped on <date>",
"supersedes the section above"), session-provenance framing ("this session found…"), and migration
narration ("X now lives on…", "the old Y", "previously applied inside…", "[retired]"). Keep
genuinely still-open items and the rare bare "Last updated: <date>" freshness marker.

**Code comments (`src/`, `test/`, `tools/`, `include/`):** enforce `docs/COMMENT_STANDARD.md` — a
comment says what the thing is FOR / how to use it / what consumes it, and never points back at the
planning board. Sweep for the strip-list there (`§`-refs, spec-doc names, `FB-*`/`D#`/`S#`/`M#`/`Phase
N` IDs, `(PO)`, `backlog`/`Feedback`/`MASTER_TODO`, dates/session tags, change-narration) and honour
its keep-anyway exceptions (other-code refs, living how-to standards, `vNN` save notes + migration
tests, and the domain terms that only look like refs: EAPOL `M1`–`M4`, `S3`/`C5`, `D0`–`D3`, world
`Area 0`–`3`). Cross-check any claim against the actual code before trusting it.

### Self-architecture review — Last run: 2026-08-29
Check the `game_*.cpp` module split for size creep past ~600 lines. Look for cross-file duplication, dead code, and orphaned helpers
that should have moved to `game_internal.h` or been deleted. Verify actual file sizes, don't trust
what a doc says they are. Sources are globbed (`CMakeLists.txt`, `build_src_filter`), so a new
unit needs no build-file edit — the cost of a split is the reading, not the plumbing. When a
unit is big but genuinely one concern (a dispatcher, a flat save mapping, a screen-per-function
render file), leave it and say so: length that follows from the number of cases is not creep.

**Measure FUNCTIONS too, not just files.** The unit rule has been holding while the mass moved
inside individual functions, where a file-size check cannot see it — the units sit near 600 while
single functions run past 400. Get the shape from the gap between definitions
(`grep -nE '^[A-Za-z_].*::.*\(' <unit>`), then apply the same one-concern test. The dispatcher
exception is the one to apply honestly: **count the `case` labels before granting it.** A long
`switch` over a vocabulary is fine; a long if-chain is accumulated special cases wearing the same
length, and the two are indistinguishable by line count alone.

### Design consistency pass — Last run: 2026-08-15
Spot-check shipped screens against the system they're authored to (`assets/VISUAL_LANGUAGE.md`,
`assets/CREATURE_VISUAL_RULES.md`) and against each other — a screen that solves a problem its
siblings solve differently is the drift worth catching. Fix the standard to match reality, or if
the drift looks like an undocumented real decision, surface it explicitly.

### Screen separation pass — Last run: 2026-08-15
Take the contact sheet (`./tools/screens.sh`) and go screen by screen through every one that
stacks **multiple distinct row groups**, asking one question: can a reader tell where one group
ends and the next begins, without reading the words?

The trap this run exists to catch is **emphasis spent where separation was already free**. The
item detail page is the worked example: its `ITEMS` banner carries `FontFace::Bold`, but that
banner already sits at the top of the screen with a rule under it — it was never the thing a
reader could lose. The groups below it (the readout grid, the prose, `HAVE`, the action line)
are the ones running together with nothing between them, and they get no help. Emphasis landed
where it was cheapest to apply rather than where the screen needed it.

Four levers, and the run should reach for them in this order — the cheapest one that works wins:

- **Spacing and grouping.** Bands whose height is reserved regardless of content (the detail
  pages' `kDetailPanelTop`/`Bottom`) leave dead gaps mid-screen and none at the seams.
- **Rules and indentation**, which the header band already uses.
- **`Pal::INK` vs `INK_DIM`**, the separation that costs nothing and survives grayscale.
- **`FontFace::Bold` last**, and only where the other three don't reach — one thing per screen
  claims emphasis (VISUAL_LANGUAGE §2.3), so every new use spends a budget the screen has one of.

Where a screen genuinely reads fine, say so and move on; the output that matters is the handful
that don't. Anything needing a real layout change becomes a row on `docs/MASTER_TODO.md` rather
than being done inside the run.

Two things the sheet will show you that are NOT findings. A name cut mid-word on a list row is
usually `drawTextMarquee` caught mid-travel, not a clip — the Hacker face's SHOP/CREW rows all
look truncated in a still and scroll on the device. And a band that ends short of the screen is
only a defect if the group BELOW it has nothing separating it; empty canvas under the last group
is just empty canvas.

### Stale cross-reference sweep — Last run: 2026-08-15
Grep for links/citations across the docs — file paths, line numbers, `D#`/`S#`/`C#`/`FB-*` row
IDs — and verify they still resolve to something real. Fix or remove dangling references.

### Test/gate health check — Last run: 2026-08-29
Run the gates. Confirm native gates and the S3 build are actually green, not assumed green from
a doc. Look for test debt — tests asserting behaviour that no longer occurs in
real play.

### Content/tunables standard drift — Last run: 2026-08-15
Check new content against `src/core/content/CONTENT_STANDARD.md`: structured effect vocabulary (no new
scalar-field sprawl or `if (id == "...")` branches), one-file-per-type under
`src/core/content/content_*.cpp`, and — the easy grep — no single-entity magnitude in
`tunables.h`. Sweep: `grep -nE 'constexpr .* k\w+ *=' include/tunables.h` and flag any whose
comment names ONE item/mod/move/creature — inline it onto that row. Verify claims against code.

### Unused-include sweep — Last run: 2026-08-15
The clangd "included header X is not used directly" warnings are noise we burn tokens reading
past, and the `game_*.cpp` units are the worst offenders: they all carry the same broad render
header block copied from `game_render.cpp`, most of which a given unit doesn't use.

**The `core/ui` half is swept.** `game.h` no longer includes a screen header (it takes
`core/ui/ui_state.h` for the ids `Game` holds), which is what made stripping one a genuine test of
whether the unit draws through it. What's left is load-bearing: `game_render.cpp` keeps all eleven
because it IS the render dispatcher, and every other unit keeps only the screens it enters —
`game_config.cpp` its `cfg_screen.h`, `game_explore.cpp`/`game_net.cpp`/`game_lifecycle.cpp` their
`expl_screen.h`, the four `layout.h`/`theme.h`/`widgets.h` units their drawing primitives. Re-run
it when a unit is split or a screen header moves.

**The `core/render` DRAWING headers are swept too, and the block was narrower than it looked.**
`game.h` hands out exactly one `core/render` header — `framebuffer.h` — and nothing it includes
pulls `canvas.h`, `font.h`, `palette.h`, `sprite.h` or `generated/assets.h`. Those five were
therefore always testable, and the broad block copied from `game_render.cpp` is gone from every
unit that did not draw: 43 includes across 11 units, both tiers green.

What remains carries its weight — `game_render.cpp` (the dispatcher, all five), `game_crew`,
`game_eggpick`, `game_peers`, `game_pvp`, `game_hacker`, `game_isolation`, `game_merge` (the
subsets they draw with), and `game_core`/`game_lifecycle` (`sprite.h`, plus `canvas.h` in
`game_core` for the tick cadences).

**What IS still blocked** is the `core/model` half: `game.h` hands every TU `combat.h`, `save.h`,
`registry.h`, `framebuffer.h` and `platform.h`, so "it still compiles without it" proves nothing
about those. Leave that half for whoever lands the next `game.h` slice.

Method note earned the hard way: **grep the header's CONSTANTS, not just its functions and
types.** A symbol scan built from function names alone cleared `canvas.h` out of `game_core.cpp`,
which uses it only for `kHeartbeatMs`/`kCombatAnimMs` — the native build caught it, which is
exactly why the build is the gate and the scan is only the finder.

Method: trust the clangd `unused-includes` diagnostics as the finder, **verify a real host build
still compiles after each removal**, and note that the native gate is authoritative — don't
remove a device-only header while building host-only, or vice-versa. The device tier has its own
compile database now (`tools/compiledb.sh`, `src/platform/esp32/.clangd`), so its diagnostics are
worth the same trust as the host's; a file lighting up whole means the database is stale. Removable is not the same as
wrong: an include a unit uses DIRECTLY stays even when some other header happens to supply it, and
the trap in a mechanical scan is a symbol that only appears in a comment.

### Asset manifest accuracy audit — Last run: 2026-08-15
Cross-check `assets/ASSET_MANIFEST.md` status markers (☑/▨/☐) against what's actually in
`assets/` and wired into `embedded_content.cpp`. Flag mismatches in either direction
(claimed-delivered-but-missing, or shipped-but-still-marked-TODO). Audit the **File** column
too, not just the marker — every concrete path had gone stale against the `icons/`/`sprites/`/
`ui/` split, which no status marker would have caught. Watch for the two basenames that exist
in both a live folder and `_attic/`: a naive stem→path map resolves them to the parked copy.

**Three ways a marker check cries wolf**, all of which look like "claimed ☑, not compiled":
`engine-drawn` rows (`UI_GAUGE`, `UI_SCROLLBAR`, `UI_HEALTH_BAR` …) are delivered as code and are
correctly absent from `assets.h` — read the File column before flagging; a row id can be a FAMILY
name that expands (`ICON_LOG_EVENT` → `_ITEM`/`_WARN`/`_COMBAT`, `UI_OVERRIDE_PIP` →
`ICON_OVERRIDE_PIP{,_SPENT}`); and `assets.h` declares sprites as `ASSET_<ID>`, so an id-prefix
grep that forgets a prefix (`BG_`, `FX_`) invents missing rows. Check the expansions and the
`ASSET_` form before believing any mismatch.

**The stronger check is: should this row exist at all?** Any asset whose id is derived from a
content row (`ICON_ITEM_<ID>`, `ICON_MOD_<ID>`, `ICON_MOVE_<ID>`, a creature's `spriteName`) must
NOT have a per-row table in the manifest — that is a hand-maintained mirror of `content_*.cpp`
that drifts without failing anything. The manifest's lede states the rule; enforce it. A row
earns its place only by carrying an art *rule* or a quality judgement no code holds.

---

Add a new entry here when a new recurring quality dimension comes up. Keep each entry to *what it
checks*, *where*, and what currently blocks it — not a running log of past findings.
