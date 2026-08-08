#!/usr/bin/env python3
"""check_test_roster.py — every native gate is defined once and listed once.

The native suite is per-subject units (`test/test_native/test_<subject>.cpp`) over one
roster (`test_main.cpp`'s MAL_RUN_ALL_GATES). The roster generates each gate's
declaration, so a roster entry naming nothing fails to LINK — but the other direction
has no compiler behind it: a gate that is written and never listed simply doesn't run,
and a green suite says nothing about it. That is what this checks.

While the suite was one file the gates were `static`, and an unlisted one showed up as
an unused-function warning. Splitting the file cost that, because a gate now needs
external linkage to reach main(). This is the same guard made explicit.

Run by ctest (see CMakeLists.txt); exits non-zero and names both directions.
"""
import re
import sys
from pathlib import Path

TEST_DIR = Path(__file__).resolve().parent.parent / "test" / "test_native"
ROSTER = TEST_DIR / "test_main.cpp"

# A gate definition: `void test_name() {` at column 0. Anything indented is a call.
DEFINITION = re.compile(r"^void (test_[a-z0-9_]+)\(\)", re.M)
LISTED = re.compile(r"\bRUN\((test_[a-z0-9_]+)\)")


def main() -> int:
    roster_text = ROSTER.read_text()
    listed = LISTED.findall(roster_text)

    defined: dict[str, str] = {}
    duplicates: list[str] = []
    for src in sorted(TEST_DIR.glob("test_*.cpp")):
        for name in DEFINITION.findall(src.read_text()):
            if name in defined:
                duplicates.append(f"{name} — in both {defined[name]} and {src.name}")
            defined[name] = src.name

    problems: list[str] = []
    problems += [f"  defined twice: {d}" for d in duplicates]

    seen: set[str] = set()
    for name in listed:
        if name in seen:
            problems.append(f"  listed twice in the roster: {name}")
        seen.add(name)

    for name in sorted(set(defined) - seen):
        problems.append(
            f"  NEVER RUNS: {name} is defined in {defined[name]} but is not in "
            f"MAL_RUN_ALL_GATES — add it there, in milestone order."
        )
    for name in sorted(seen - set(defined)):
        problems.append(f"  roster names a gate nothing defines: {name}")

    if problems:
        print("test roster is out of step:")
        print("\n".join(problems))
        return 1

    print(f"test roster ok — {len(defined)} gates, all listed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
