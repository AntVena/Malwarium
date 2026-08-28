#!/usr/bin/env python3
"""check_hint_bands.py — no footer hint is wider than the canvas it is centred in.

drawHintBand (src/core/ui/widgets.cpp) centres its text across the full 224px active
canvas. A hint one character too long is therefore cut at BOTH ends, and the leading
character is a button letter — the one part of the line a player cannot infer from the
rest. The widget clamps so an overrun loses its tail instead, but a clipped hint is
still a hint that does not say what it means.

Nothing else notices: the string is a literal, the frame still renders, and the loss
is four pixels at each end of a band nobody re-reads once the screen is familiar.

The font is FONT_UI at its 8px cut, monospaced, one cell per character (core/render/
font.h) — so a hint's width is exactly 8 * len, and this needs no renderer to measure
it. It reads the literals where they are written, so there is no second list to keep
in step.

Registered as the `hint_bands` ctest (CMakeLists.txt); exits non-zero and names every
hint that overruns, with the width it needs.
"""
import re
import sys
from pathlib import Path

SRC = Path(__file__).resolve().parent.parent / "src"

# core/render/font.h — every glyph is one 8x8 cell and advances 8px.
ADVANCE = 8
# core/render/canvas.h — the active canvas the band spans.
ACTIVE_W = 224

# Both ways a hint reaches the band: passed straight to it, or built as a `hint`
# variable first (the screens with a state-dependent band — combat, expl, train).
CALL = re.compile(r"drawHintBand\(\s*fb\s*,\s*(.+?)\);", re.S)
HINT_VAR = re.compile(r"\b\w*[hH]int\w*\s*=\s*((?:[^;]|\n)*?);")
LITERAL = re.compile(r'"((?:[^"\\]|\\.)*)"')


def hints():
    """(file, line, text) for every hint-band string literal in the tree."""
    for src in sorted(SRC.rglob("*.cpp")):
        text = src.read_text()
        for pattern in (CALL, HINT_VAR):
            for m in pattern.finditer(text):
                line = text.count("\n", 0, m.start()) + 1
                for s in LITERAL.findall(m.group(1)):
                    yield src, line, s


def main() -> int:
    over = []
    for src, line, s in hints():
        width = len(s) * ADVANCE
        if width > ACTIVE_W:
            rel = src.relative_to(SRC.parent)
            over.append(
                f"  {rel}:{line}: {width}px ({len(s)} chars) — "
                f"{ACTIVE_W // ADVANCE} chars is the ceiling\n"
                f"      {s!r}"
            )

    if over:
        print("hint bands wider than the canvas (they are cut at both ends):")
        print("\n".join(over))
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
