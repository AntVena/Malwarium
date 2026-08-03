#!/usr/bin/env python3
"""Fail if any engine source invents a colour instead of asking the palette for one.

This is the check behind a promise: that adding a whole new theme — a
colourblind-friendly set, a high-contrast set — is a block in assets/PAL_CORE.json
and a choice of index, never a sweep through the screens. That promise only holds
while EVERY colour reaches the framebuffer through palColor() (core/render/palette.h),
because that one indirection is where a theme is substituted. A single literal
sneaks past it, and from then on the interface has a colour that no theme can move.

What counts as inventing one:

  * rgb565(...) built purely from number literals — a new colour out of thin air.
  * a 16-bit hex literal assigned to something typed Rgb565.

What is fine, and common:

  * palColor(Pal::X) and anything in core/ui/theme.h built on it.
  * rgb565(...) DERIVED from an existing colour — dimming, lerping, ramping. Those
    move with their source, so they follow a theme swap for free.

Run: python3 tools/check_palette_bindings.py
"""
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Where UI colour decisions get made. The generated palette is the one place a
# literal is not only allowed but required — it IS the table.
ROOTS = ["src/core/ui", "src/core/app", "src/core/render", "src/platform"]
SKIP = {"src/generated"}

# rgb565( ... ) whose whole argument list is numbers, separators and whitespace.
LITERAL_RGB = re.compile(r"\brgb565\s*\(\s*[0-9xXa-fA-F,\s+\-*/()]*\s*\)")
# An identifier is what tells a derived colour apart from a made-up one.
HAS_NAME = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")
LITERAL_ASSIGN = re.compile(r"\bRgb565\b[^=;\n]*=\s*0x[0-9a-fA-F]{3,4}\b")


def offenders():
    for root in ROOTS:
        for dirpath, _, files in os.walk(os.path.join(REPO, root)):
            rel = os.path.relpath(dirpath, REPO)
            if any(rel.startswith(s) for s in SKIP):
                continue
            for name in files:
                if not name.endswith((".c", ".cpp", ".h", ".hpp")):
                    continue
                path = os.path.join(dirpath, name)
                with open(path, errors="replace") as f:
                    lines = f.readlines()
                for n, line in enumerate(lines, 1):
                    code = line.split("//", 1)[0]
                    for m in LITERAL_RGB.finditer(code):
                        args = m.group(0)[m.group(0).index("(") + 1 : -1]
                        # A cast or a variable means it came from somewhere real.
                        if HAS_NAME.search(args):
                            continue
                        yield os.path.relpath(path, REPO), n, m.group(0).strip()
                    for m in LITERAL_ASSIGN.finditer(code):
                        yield os.path.relpath(path, REPO), n, m.group(0).strip()


def main():
    found = list(offenders())
    if not found:
        print("check_palette_bindings: ok — every colour comes from palColor()")
        return 0
    print("check_palette_bindings: colours that no theme can reach:\n")
    for path, line, text in found:
        print(f"  {path}:{line}  {text}")
    print(
        "\nBind these to a PAL_CORE role token (core/render/palette.h), or map the "
        "concept in core/ui/theme.h if it is a game meaning rather than a role."
    )
    return 1


if __name__ == "__main__":
    sys.exit(main())
