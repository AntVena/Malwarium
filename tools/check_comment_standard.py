#!/usr/bin/env python3
"""Fail if a shipped comment describes the planning board, or the change, instead of the code.

docs/COMMENT_STANDARD.md's central rule: a comment orients a reader *in the moment*,
so it names other CODE freely and never names a planning card. The failure it guards
against is not style, it's rot — docs/MASTER_TODO.md is a Kanban board whose sections
are renumbered every time a card lands, so a comment citing "§1c" is describing a
different item within a session or two, and the reader has no way to tell.

Only the unambiguous half of that doc's strip-on-sight list is mechanised here:

  * MASTER_TODO / Next-phase / Feedback #N   — the board, by name.
  * FB-<ID> row ids, Phase N milestone tags  — cards, by number.
  * (D3), (J.21b)                            — row ids in their PARENTHESISED form.
  * #12, "Move-slot rework #11"              — bare card numbers (hex colours survive).
  * (PO), "PO asked/noted/said", "cowork decided" — attribution.
  * ISO dates                                — time provenance; git log is the changelog.
  * "it used to be", "now lives in", "superseded" — CHANGE NARRATION (see below).

Bare § refs stay a human read: they are ordinary English besides, and a checker that
cries wolf gets switched off. That is also why only the parenthesised form of a row id
is caught, and only for the prefixes that carry no domain meaning. Bare, they collide
head-on with the doc's own keep-anyway list; even parenthesised, `S` is the S3 board,
`C` is C5, `M` is an EAPOL handshake message and `L` is a menu depth (L2/L3), so only
`D`, `J` and `#` are safe to fail on.

CHANGE NARRATION is mechanised only where the phrase is PAST-TENSE about the code
itself, which is the half that cannot also be a present-tense fact. "no longer", "the
old X" and "was removed" are deliberately absent: `a name the tables no longer answer
to`, `the old firmware the next power cycle restores` and `returns true if the full
amount was removed` are all current state, and they outnumber the real findings several
to one. What is caught is a subject that USED TO do something, a thing that NOW LIVES
somewhere, one that SUPERSEDES or REPLACES THE OLD, and a comment dating itself to the
session that wrote it.

Against those, two exemptions carry the standard's own "the old wire format is literally
the subject" exception:

  * MIGRATION_UNITS — save.cpp/save.h/game_persist.cpp describe formats they no longer
    write, which is what they are for.
  * Any comment naming a save version (`v33`, `v51`) — the same exception wherever it
    lands, which is how a save-migration TEST comment keeps its wire-format note.

Only comment text is scanned. A date inside a JSON fixture and a *capture* session in
a field name are both legitimate, and both look exactly like a violation in raw grep.

Run: python3 tools/check_comment_standard.py
"""
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# The code that ships. src/generated/ is built, not written.
ROOTS = ["src", "include", "test"]
SKIP = {"src/generated"}
EXTS = (".cpp", ".h", ".hpp", ".c", ".ino")

PATTERNS = [
    (re.compile(r"MASTER_TODO|Next-phase|Feedback #\d"), "names the planning board"),
    (re.compile(r"\bFB-[A-Z]+\d|\bPhase \d"), "cites a planning card / milestone id"),
    (re.compile(r"\([DJ]\.?\d+[a-z]?\)"), "cites a planning row id"),
    # A bare #N card number. The one thing in this repo that legitimately carries a '#'
    # inside a comment is a hex colour (`#14171c`), so the lookahead excludes any six-hex
    # run before the digits are read — including an all-numeric one, which is the case a
    # "not followed by a hex char" test would have let through.
    (re.compile(r"#(?![0-9a-fA-F]{6})\d{1,3}\b"), "cites a planning card number"),
    (re.compile(r"\(PO\)|\bPO (?:asked|noted|said)|cowork decided"), "attributes a decision"),
    (re.compile(r"\b20\d\d-\d\d-\d\d\b"), "dates the code (git log is the changelog)"),
]

# Change narration, past-tense-about-the-code only — the half that cannot double as a
# present-tense fact. Held apart from PATTERNS because EXEMPT_VERSION_NOTE and
# MIGRATION_UNITS apply to these and to nothing else.
NARRATION = [
    # A SUBJECT that used to do something. The bare phrase is not enough: "used to
    # re-derive" is a past participle plus an infinitive and describes what the code
    # does now, so the subject pronoun is what separates the two readings.
    (re.compile(r"\b(?:it|they|we|this|that|these|those|which|there|one|nobody|nothing"
                r"|everything)\s+used\s+to\b", re.I), "narrates what the code used to do"),
    (re.compile(r"\bused\s+to\s+be\b", re.I), "narrates what the code used to be"),
    (re.compile(r"\bnow\s+lives?\s+(?:on|in|at|under|beside|with)\b", re.I),
     "narrates a move (say where it IS, not that it moved)"),
    # Past participle only. Present-tense "supersedes" is a live relation between two
    # things that both exist — "the reply that supersedes the frame being repeated".
    (re.compile(r"\bsuperseded\b", re.I), "narrates a replacement"),
    (re.compile(r"\breplace[sd]?\s+the\s+old\b", re.I), "narrates a replacement"),
    (re.compile(r"\buntil\s+recently\b|\bshipped\s+on\b", re.I), "dates the code"),
    # "this session ships X" — the session that wrote a line is the one thing a reader
    # opening it cold can never be in.
    (re.compile(r"\bthis\s+session\s+(?:ships?|shipped|adds?|added|introduces?|introduced"
                r"|moves?|moved|renames?|renamed|removes?|removed|brings?|brought)\b", re.I),
     "dates the code to the session that wrote it"),
]

# Units whose SUBJECT is a format they no longer write (COMMENT_STANDARD.md's
# "Legit exceptions"), so narration there is current-state about old bytes.
MIGRATION_UNITS = {
    "src/core/model/save.cpp",
    "src/core/model/save.h",
    "src/core/app/game/game_persist.cpp",
}

# The same exception wherever it lands: a comment that names a save version is a
# wire-format note, which is what the exception is for.
EXEMPT_VERSION_NOTE = re.compile(r"\bv\d{1,3}\b")

# // to end of line, and /* ... */ across lines. Close enough on real source: the
# strings that could fool it would have to CONTAIN a comment opener.
COMMENTS = re.compile(r"//[^\n]*|/\*.*?\*/", re.S)


def sources():
    for root in ROOTS:
        for dirpath, _, files in os.walk(os.path.join(REPO, root)):
            rel = os.path.relpath(dirpath, REPO)
            if any(rel == s or rel.startswith(s + os.sep) for s in SKIP):
                continue
            for f in files:
                if f.endswith(EXTS):
                    yield os.path.join(dirpath, f)


def offenders():
    for path in sources():
        rel = os.path.relpath(path, REPO).replace(os.sep, "/")
        with open(path, encoding="utf-8", errors="replace") as fh:
            text = fh.read()
        migration = rel in MIGRATION_UNITS
        for m in COMMENTS.finditer(text):
            line = text.count("\n", 0, m.start()) + 1
            body = m.group()
            rules = list(PATTERNS)
            if not migration and not EXEMPT_VERSION_NOTE.search(body):
                rules += NARRATION
            for pat, why in rules:
                hit = pat.search(body)
                if hit:
                    yield rel, line, hit.group().strip(), why
                    break


def main():
    found = list(offenders())
    if not found:
        print("comments clean (no board pointers, no change narration)")
        return 0
    print(f"{len(found)} comment(s) describe the board or the change, not the code:\n")
    for path, line, tok, why in found:
        print(f"  {path}:{line}  {tok!r} — {why}")
    print("\nSay what the code IS, here and now. See docs/COMMENT_STANDARD.md.")
    return 1


if __name__ == "__main__":
    sys.exit(main())
