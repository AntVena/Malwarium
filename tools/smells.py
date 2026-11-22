#!/usr/bin/env python3
"""smells.py: the finder for the self-architecture review (docs/MAINTENANCE.md).

Prints three lists over src/core and src/platform. Each is a place to look and none is a
verdict: the review decides, by the rules in CONTRIBUTING's *Structure*.

  long      functions over --lines (default 80), longest first, with their `case` count.
            A long switch over a vocabulary is fine; a long if-chain, or a switch whose
            cases do their own work, is several steps waiting for names.
  repeated  one statement written out in several files (default 4+). The engine's LCG
            step was in 66 places before it had a name; this is the list that finds the
            next one.
  flags     parameters of type bool. A setter's `bool on` is a value and fine; a bool
            that picks a behaviour reads as a bare true/false at the call site and wants
            an enum the call site names.

  python3 tools/smells.py [long|repeated|flags] [--lines N] [--files N]
"""
import argparse
import collections
import re
import subprocess

ROOTS = ["src/core", "src/platform"]


def sources():
    out = subprocess.check_output(["git", "ls-files", *ROOTS], text=True).split()
    return [f for f in out if f.endswith((".cpp", ".h")) and "/generated/" not in f]


def code_only(line):
    line = re.sub(r"//.*", "", line)
    line = re.sub(r'"([^"\\]|\\.)*"', '""', line)
    return re.sub(r"'([^'\\]|\\.)*'", "''", line)


DEF = re.compile(
    r"^(\s{0,4})(?!if\b|for\b|while\b|switch\b|return\b|else\b|case\b|do\b)"
    r"[A-Za-z_][\w:<>,*&~\s]*?\b([A-Za-z_~][\w:~]*)\s*\(")


def functions(path):
    lines = open(path, errors="ignore").read().split("\n")
    i = 0
    while i < len(lines):
        m = DEF.match(lines[i])
        head = lines[i].lstrip()
        if not m or head.startswith(("//", "#", "}")) or ";" in code_only(lines[i]):
            i += 1
            continue
        sig, j = lines[i], i
        while "{" not in sig and j < i + 8 and j + 1 < len(lines) and ";" not in lines[j]:
            j += 1
            sig += " " + lines[j].strip()
        if "{" not in sig or ";" in sig.split("{")[0]:
            i += 1
            continue
        depth, k, opened = 0, j, False
        while k < len(lines):
            for ch in code_only(lines[k]):
                if ch == "{":
                    depth, opened = depth + 1, True
                elif ch == "}":
                    depth -= 1
            if opened and depth <= 0:
                break
            k += 1
        body = lines[i:k + 1]
        cases = sum(1 for x in body if re.match(r"\s*(case\b|default:)", x))
        yield m.group(2), i + 1, k - i + 1, cases
        i = k + 1


def report_long(limit):
    rows = []
    for f in sources():
        for name, line, n, cases in functions(f):
            if n > limit:
                rows.append((n, cases, f"{f}:{line}", name))
    for n, cases, where, name in sorted(rows, reverse=True):
        print(f"{n:5d} lines {cases:3d} cases  {where}  {name}")


def report_repeated(min_files):
    seen = collections.defaultdict(set)
    for f in sources():
        for line in open(f, errors="ignore"):
            s = " ".join(code_only(line).split())
            # A statement worth naming: long enough to carry a rule, and not a brace,
            # an include, or a host interface's pure-virtual verb (each host restates its own).
            if len(s) < 28 or not s.endswith(";") or s.startswith(("#", "return", "}", "virtual")):
                continue
            seen[s].add(f)
    rows = [(len(fs), s) for s, fs in seen.items() if len(fs) >= min_files]
    for n, s in sorted(rows, reverse=True):
        print(f"{n:3d} files  {s}")


def report_flags():
    pat = re.compile(r"\b(\w+)\s*\(([^;{}()]*\bbool\s+(\w+)[^;{}()]*)\)\s*(const)?\s*(override)?\s*[;{]")
    for f in sources():
        if not f.endswith(".h"):
            continue
        for n, line in enumerate(open(f, errors="ignore"), 1):
            m = pat.search(code_only(line))
            if m and not re.match(r"set[A-Z]", m.group(1)) and m.group(3) not in ("on", "v"):
                print(f"{f}:{n}  {m.group(1)}(... bool {m.group(3)} ...)")


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("which", nargs="?", choices=["long", "repeated", "flags"])
    ap.add_argument("--lines", type=int, default=80)
    ap.add_argument("--files", type=int, default=4)
    a = ap.parse_args()
    for name, fn in (("long", lambda: report_long(a.lines)),
                     ("repeated", lambda: report_repeated(a.files)),
                     ("flags", report_flags)):
        if a.which in (None, name):
            print(f"== {name}")
            fn()


if __name__ == "__main__":
    main()
