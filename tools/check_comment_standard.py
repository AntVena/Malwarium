#!/usr/bin/env python3
"""Fail if a shipped comment points at the planning board instead of the code.

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

Change narration ("used to", "no longer", "now lives on") and bare § refs are equally
against the standard but are ordinary English besides, so they stay a human read —
a checker that cries wolf gets switched off. That is also why only the
parenthesised form of a row id is caught, and only for the prefixes that carry no
domain meaning. Bare, they collide head-on with the doc's own keep-anyway list; even
parenthesised, `S` is the S3 board, `C` is C5, `M` is an EAPOL handshake message and
`L` is a menu depth (L2/L3), so only `D`, `J` and `#` are safe to fail on.

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
        with open(path, encoding="utf-8", errors="replace") as fh:
            text = fh.read()
        for m in COMMENTS.finditer(text):
            line = text.count("\n", 0, m.start()) + 1
            for pat, why in PATTERNS:
                hit = pat.search(m.group())
                if hit:
                    yield os.path.relpath(path, REPO), line, hit.group().strip(), why
                    break


def main():
    found = list(offenders())
    if not found:
        print("comments clean (no planning-board pointers)")
        return 0
    print(f"{len(found)} comment(s) point at the planning board:\n")
    for path, line, tok, why in found:
        print(f"  {path}:{line}  {tok!r} — {why}")
    print("\nSay what the code IS, here and now. See docs/COMMENT_STANDARD.md.")
    return 1


if __name__ == "__main__":
    sys.exit(main())
