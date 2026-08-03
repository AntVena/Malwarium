# Malwarium — Maintenance Routine

A recurring self-review pile. Start a session with **"let's do a maintenance run"** — pick the
item below with the oldest "Last run" date, do it for real (not a superficial pass), then update
its date + a one-line note of what changed. Commit the result.

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

### Docs cleanup / decision-history sweep — Last run: 2026-07-19
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

### Self-architecture review — Last run: never
Check the `game_*.cpp` module split for size creep past ~600 lines. Look for cross-file duplication, dead code, and orphaned helpers
that should have moved to `game_internal.h` or been deleted. Verify actual file sizes, don't trust
what a doc says they are.

### Design consistency pass — Last run: never
Spot-check shipped screens against the system they're authored to (`assets/VISUAL_LANGUAGE.md`,
`assets/CREATURE_VISUAL_RULES.md`) and against each other — a screen that solves a problem its
siblings solve differently is the drift worth catching. Fix the standard to match reality, or if
the drift looks like an undocumented real decision, surface it explicitly.

### Stale cross-reference sweep — Last run: 2026-07-18
Grep for links/citations across the docs — file paths, line numbers, `D#`/`S#`/`C#`/`FB-*` row
IDs — and verify they still resolve to something real. Fix or remove dangling references.

### Test/gate health check — Last run: never
Run the gates. Confirm native gates and the S3 build are actually green, not assumed green from
a doc. Look for test debt — tests asserting behaviour that no longer occurs in
real play.

### Content/tunables standard drift — Last run: 2026-07-19
Check new content against `src/core/content/CONTENT_STANDARD.md`: structured effect vocabulary (no new
scalar-field sprawl or `if (id == "...")` branches), one-file-per-type under
`src/core/content/content_*.cpp`, and — the easy grep — no single-entity magnitude in
`tunables.h`. Sweep: `grep -nE 'constexpr .* k\w+ *=' include/tunables.h` and flag any whose
comment names ONE item/mod/move/creature — inline it onto that row. Verify claims against code.

### Unused-include sweep — Last run: never
This is a small device — every `#include` we don't use is wasted flash/RAM (headers pull in code
and data), and the clangd "included header X is not used directly" warnings are noise we burn tokens
reading past. Periodically prune them. The `game_*.cpp` units are the worst offenders: they all
carry the same broad render/UI header block copied from `game_render.cpp`, most of which a given
unit (e.g. `game_persist.cpp`, `game_combat.cpp`) doesn't use. Trust the clangd
`unused-includes` diagnostics as the finder, but **verify a real host build still compiles after
each removal** (a header may satisfy a transitive dependency clangd can't see) — the native gate is
authoritative. Don't remove a device-only header while building host-only, and vice-versa.

### Asset manifest accuracy audit — Last run: never
Cross-check `assets/ASSET_MANIFEST.md` status markers (☑/▨/☐) against what's actually in
`assets/` and wired into `embedded_content.cpp`. Flag mismatches in either direction
(claimed-delivered-but-missing, or shipped-but-still-marked-TODO).

---

Add a new entry here when a new recurring quality dimension comes up. Keep each entry to *what it
checks* and *where* — not a running log of past findings; the "Last run" date is the only
breadcrumb this file keeps, the fix itself belongs in the commit.
