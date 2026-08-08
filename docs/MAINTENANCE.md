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

### Docs cleanup / decision-history sweep — Last run: 2026-08-05
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

### Self-architecture review — Last run: 2026-08-05
Check the `game_*.cpp` module split for size creep past ~600 lines. Look for cross-file duplication, dead code, and orphaned helpers
that should have moved to `game_internal.h` or been deleted. Verify actual file sizes, don't trust
what a doc says they are. Sources are globbed (`CMakeLists.txt`, `build_src_filter`), so a new
unit needs no build-file edit — the cost of a split is the reading, not the plumbing. When a
unit is big but genuinely one concern (a dispatcher, a flat save mapping, a screen-per-function
render file), leave it and say so: length that follows from the number of cases is not creep.

### Design consistency pass — Last run: 2026-08-05
Spot-check shipped screens against the system they're authored to (`assets/VISUAL_LANGUAGE.md`,
`assets/CREATURE_VISUAL_RULES.md`) and against each other — a screen that solves a problem its
siblings solve differently is the drift worth catching. Fix the standard to match reality, or if
the drift looks like an undocumented real decision, surface it explicitly.

### Screen separation pass — Last run: never
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

### Stale cross-reference sweep — Last run: 2026-08-05
Grep for links/citations across the docs — file paths, line numbers, `D#`/`S#`/`C#`/`FB-*` row
IDs — and verify they still resolve to something real. Fix or remove dangling references.

### Test/gate health check — Last run: 2026-08-05
Run the gates. Confirm native gates and the S3 build are actually green, not assumed green from
a doc. Look for test debt — tests asserting behaviour that no longer occurs in
real play.

### Content/tunables standard drift — Last run: 2026-07-19
Check new content against `src/core/content/CONTENT_STANDARD.md`: structured effect vocabulary (no new
scalar-field sprawl or `if (id == "...")` branches), one-file-per-type under
`src/core/content/content_*.cpp`, and — the easy grep — no single-entity magnitude in
`tunables.h`. Sweep: `grep -nE 'constexpr .* k\w+ *=' include/tunables.h` and flag any whose
comment names ONE item/mod/move/creature — inline it onto that row. Verify claims against code.

### Unused-include sweep — Last run: never (blocked)
The clangd "included header X is not used directly" warnings are noise we burn tokens reading
past, and the `game_*.cpp` units are the worst offenders: they all carry the same broad render/UI
header block copied from `game_render.cpp`, most of which a given unit doesn't use.

**This is blocked on `game.h`'s 36 includes** (`MASTER_TODO.md §3`). Strip-and-rebuild says 295
includes across `src/core` are removable, and that number is meaningless — `game.h` supplies
almost everything transitively, so "it still compiles" proves nothing about whether an include is
needed. The one subset that IS mechanically safe is the ~60 lines the `game_*.cpp` units
re-include that `game.h` already provides; that is cosmetic and can ride along with any other
edit. Do the `game.h` row first, then this entry has a real signal.

When it unblocks: trust the clangd `unused-includes` diagnostics as the finder, **verify a real
host build still compiles after each removal**, and note that the native gate is authoritative —
don't remove a device-only header while building host-only, or vice-versa.

### Asset manifest accuracy audit — Last run: 2026-08-05
Cross-check `assets/ASSET_MANIFEST.md` status markers (☑/▨/☐) against what's actually in
`assets/` and wired into `embedded_content.cpp`. Flag mismatches in either direction
(claimed-delivered-but-missing, or shipped-but-still-marked-TODO). Audit the **File** column
too, not just the marker — every concrete path had gone stale against the `icons/`/`sprites/`/
`ui/` split, which no status marker would have caught. Watch for the two basenames that exist
in both a live folder and `_attic/`: a naive stem→path map resolves them to the parked copy.

**The stronger check is: should this row exist at all?** Any asset whose id is derived from a
content row (`ICON_ITEM_<ID>`, `ICON_MOD_<ID>`, `ICON_MOVE_<ID>`, a creature's `spriteName`) must
NOT have a per-row table in the manifest — that is a hand-maintained mirror of `content_*.cpp`
that drifts without failing anything. The manifest's lede states the rule; enforce it. A row
earns its place only by carrying an art *rule* or a quality judgement no code holds.

---

Add a new entry here when a new recurring quality dimension comes up. Keep each entry to *what it
checks*, *where*, and what currently blocks it — not a running log of past findings.
