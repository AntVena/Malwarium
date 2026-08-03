# Malwarium — Comment & Doc-Prose Standard

How to write comments (and `docs/` prose) so they orient a developer **in the moment** and
don't rot. Hold every new or edited comment to this. It's also the forward-facing half of the
`docs/MAINTENANCE.md` "docs cleanup" dimension — this doc is how you avoid making that mess in the
first place.

## The principle

A comment earns its place by answering one of three things:

1. **What is this FOR?**
2. **What consumes it / what workflow does it enable?** (worth saying when that collaborator lives
   in *another* file.)
3. **HOW do I use it?** (e.g. a header comment on the items table showing how to wire a new item.)

That's the whole job: help the person reading this code *right now*.

**A comment must never point back at the planning board.** `docs/MASTER_TODO.md` is the planning surface — it
decide *what* to build, like cards on a Kanban board. You don't write the card number on the code.
Once the thing is built the card is irrelevant to a reader of the code (it might belong in the
git-commit message — never in the comment). The code plus `git log` are the living history; a pointer
to a waterfall doc just rots the moment that doc moves, and adds nothing a reader-in-the-moment needs.

The unambiguous half of that is a build gate: `tools/check_comment_standard.py` runs as the ctest
`comment_standard` and fails on a board name, an `FB-*`/`Phase N` id, an attribution or a date in
any comment under `src/`, `include/` or `test/`. Everything below it can't check — change narration
especially — is still on you.

**Same rule for doc prose itself: forward-facing state, not a changelog.** Cite
*how/where* (the mechanism, a `file:line`, a sibling symbol) — never *when* (a date, a session, a
milestone). `git log` is the changelog.

## Don't write (strip on sight)

- **Planning-board pointers** — `§`-section refs into the board, design `Area N` labels.
- **Planning / row IDs** — `FB-*`, `D#`, `S#`, `M#` and `Phase N` milestone/build labels,
  `backlog …`, `Feedback #N`, `MASTER_TODO`, `Next-phase`.
- **Attribution** — `(PO)`, "the PO asked for", "PO noted", "cowork decided".
- **Time / session provenance** — dates (`2026-07-19`), "this session", "reworked <date>",
  "redesign (…)", "shipped on …".
- **Change narration** — "the old X", "used to", "no longer", "previously applied inside", "now
  lives on", "replaces the old", "superseded by", "[retired]". Describe the **current** state
  instead; if the *why* still matters, state it as a present fact ("opened lazily to avoid a
  file-count blowup"), not as a change story ("we used to open eagerly, which…").

## Do write

- What the thing is for, how to use it, worked examples.
- The **why** behind a non-obvious choice — phrased as current-state rationale, not history.
- **References to other code** — a sibling file, a class/function/tunable name — when the
  collaborator or consumer lives elsewhere. `resolveMaint() → addCareMistakeShielded`,
  `game_hacker.cpp`, `dev_config.h`, `Nav::PostEncounter` all orient you *in the codebase* and stay.

## Legit exceptions (they look like refs, but keep them)

- **Living "how to add X" standards** — `src/core/content/CONTENT_STANDARD.md`, `src/core/content/LINE_MOVE_IDENTITIES.md`,
  and this doc. Pointing at these *is* "how to use," so it's allowed.
- **Save-schema `vNN` version notes** and **save-migration test comments** — the old wire format is
  literally the test's subject, so describing it is current-state, not narration.
- **Domain terms that collide with ref syntax** — the WPA 4-way handshake messages `M1`–`M4`
  (`eapol.*`), board names (`S3` / `C5`), SDIO data pins (`D0`–`D3`), and the in-game world areas
  (`Area 0`–`Area 3`, e.g. Citrus Circuit). These are the *thing itself*, not a reference to a card.

## When in doubt

Read the comment as a developer who just opened the file cold. If a token only helps someone
cross-referencing a planning doc, cut it. If it helps you understand or use the code right here,
keep it.
