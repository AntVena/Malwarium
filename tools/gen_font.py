#!/usr/bin/env python3
"""
gen_font.py — rasterise FONT_UI into the glyph table src/core/render/font_glyphs.cpp.

This is an AUTHORING step, not a build step. It runs by hand when the face or its
size changes, and its output is committed like any other source file:

    python3 tools/gen_font.py

It owns the TABLES only. The renderer over them (drawText, the word-wrap rule)
is hand-written in font.cpp and knows nothing about which face it is drawing.

The two cuts
------------
FontFace::Regular is what the TTF rasterises to. FontFace::Bold is DERIVED from
it here rather than pulled from a second TTF: each scanline is smeared one
column to the right (`b | (b >> 1)`). A separate bold face would come with its
own advance and its own cell, and every layout constant in src/core/ui/layout.h
is built on this one — deriving keeps kFontW / kFontH / kFontAdvance shared, so
a caller switching face moves no pixel it did not ask to move. `bold_cost`
below reports what that costs, and the answer goes in the generated file.

Why it is not part of the gates
-------------------------------
Rasterising a TTF needs Pillow + FreeType. gen_assets.py is deliberately pure
standard library so anyone with a stock Python 3 can regenerate the atlas, and
the gates depend on that. Keeping the font's one-off conversion here preserves
it: the committed table is what builds, the TTF beside it is what the table came
from, and neither the gates nor a fresh clone need Pillow to compile.

Why 8px, and why this face
--------------------------
VISUAL_LANGUAGE.md §2.1 rules out anti-aliased rendering — it smears at this
size. Pixel Operator is a PIXEL font, so it is crisp only at the size it was
drawn for and integer multiples of it. PixelOperatorMono8 is the 8px cut, and at
8px it rasterises to pure on/off with no grey levels at all. `check_crisp` below
enforces that, so a face or size that quietly antialiases fails here rather than
on the panel — which is the one thing a reader cannot spot in a hex table.

Mono is what buys tabular digits (§2.1): every glyph advances 8px, so a gauge
value or a countdown never reflows as it changes. The 16px cut of the same
family is the other crisp option if the type scale ever wants a second role.
"""

import os
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TTF = os.path.join(REPO, "assets", "fonts", "PixelOperatorMono8.ttf")
OUT = os.path.join(REPO, "src", "core", "render", "font_glyphs.cpp")

SIZE = 8       # the face's own design size — see the module note above
CELL = 8       # mono advance and cell height at that size
FIRST = 32     # ' '
LAST = 126     # '~'


def rasterise():
    """Every codepoint FIRST..LAST as CELL rows of bits, bit(CELL-1) leftmost."""
    try:
        from PIL import Image, ImageDraw, ImageFont
    except ImportError:
        sys.exit("gen_font.py needs Pillow (pip install pillow). It is an "
                 "authoring tool, not a build dependency — see the module note.")

    font = ImageFont.truetype(TTF, SIZE)
    ascent, _ = font.getmetrics()

    out = []
    for cp in range(FIRST, LAST + 1):
        img = Image.new("L", (CELL, CELL), 0)
        # anchor="ls" puts the origin on the BASELINE, so every glyph sits on the
        # same line and descenders fall below it instead of each cell being
        # top-aligned to its own ink.
        ImageDraw.Draw(img).text((0, ascent), chr(cp), font=font, fill=255,
                                 anchor="ls")
        px = img.load()
        check_crisp(cp, px)
        rows = []
        for y in range(CELL):
            bits = 0
            for x in range(CELL):
                if px[x, y]:
                    bits |= 1 << (CELL - 1 - x)
            rows.append(bits)
        out.append((cp, rows))
    return out


def check_crisp(cp, px):
    levels = {px[x, y] for y in range(CELL) for x in range(CELL)}
    grey = sorted(levels - {0, 255})
    if grey:
        sys.exit(f"U+{cp:04X} ({chr(cp)!r}) rasterised with grey levels {grey} — "
                 f"the face is being antialiased at {SIZE}px, which "
                 f"VISUAL_LANGUAGE.md §2.1 rules out. Use the cut drawn for "
                 f"this size.")


def smear(rows):
    """One cell's scanlines widened a column to the right — the BOLD cut."""
    return [r | (r >> 1) for r in rows]


def bold_cost(glyphs):
    """The cells the smear is not free on, as two lists of labels.

    Every cell keeps its 8px box, but a scanline already reaching an edge has
    nowhere free to widen into: ink in the leftmost column means the glyph
    spans the cell and thickens into its own counters, and ink in the rightmost
    column is a bit shifted off the cell entirely. Read off the rasterised
    table rather than written down, so the note in the generated file can never
    drift from the glyphs it names.
    """
    crowded = [chr(cp) for cp, rows in glyphs if any(r & 0x80 for r in rows)]
    clipped = [chr(cp) for cp, rows in glyphs if any(r & 0x01 for r in rows)]
    return crowded, clipped


def cell_label(cp):
    c = chr(cp)
    if c == "'":
        return "\\'"
    if c == "\\":
        return "\\\\"
    return c


def table(name, glyphs, transform=lambda rows: rows):
    lines = [f"const uint8_t {name}[{len(glyphs)}][{CELL}] = {{"]
    for cp, rows in glyphs:
        body = ",".join(f"0x{r:02X}" for r in transform(rows))
        lines.append(f"    {{{body}}},  // '{cell_label(cp)}'")
    lines.append("};")
    return lines


def emit(glyphs):
    crowded, clipped = bold_cost(glyphs)
    quoted = " ".join(f"'{c}'" for c in crowded)
    lost = " ".join(f"'{c}'" for c in clipped)
    # The cells that come out whole are the ones the smear does NOT crowd —
    # `clipped` is a subset of `crowded` (a glyph can only lose its rightmost
    # column if it already spans the cell), so counting off the wrong set
    # would claim six crowded glyphs as clean.
    whole = len(glyphs) - len(crowded)
    lines = [
        "// font_glyphs.cpp — FONT_UI's glyph tables, one per FontFace.",
        "//",
        "// GENERATED by tools/gen_font.py from assets/fonts/PixelOperatorMono8.ttf",
        "// (Pixel Operator by Jayvee Enaguas, CC0 1.0 — LICENSE sits beside the",
        "// TTF). Change the face or the size in the generator and re-run it; never",
        "// edit these tables by hand.",
        "//",
        "// kGlyphs is the rasterised face. kGlyphsBold is derived from it, one",
        "// scanline at a time, so both cuts share kFontW / kFontH / kFontAdvance",
        "// exactly and switching face moves nothing a layout was built on.",
        "//",
        "// The renderer that reads these lives in font.cpp and is face-independent.",
        "",
        '#include "core/render/font_glyphs.h"',
        "",
        "#include <cctype>",
        "",
        "namespace mal {",
        "",
        "namespace {",
        "",
        f"// One byte per scanline, bit{CELL - 1} = leftmost column. Codepoints",
        f"// {FIRST}..{LAST} are contiguous, so a lookup is one subtraction.",
        f"constexpr int kFirstGlyph = {FIRST};",
        f"constexpr int kLastGlyph = {LAST};",
        "",
    ]
    lines += table("kGlyphs", glyphs)
    lines += [
        "",
        "// FontFace::Bold — each scanline of the face above smeared a column to",
        "// the right (b | (b >> 1)). The face insets its ink from the left, so",
        f"// {whole} of {len(glyphs)} cells have a free column to widen into and come out",
        "// whole. The exceptions are the glyphs that already span the cell:",
        "//",
        f"//     {quoted}",
        "//",
        f"// which thicken into their own counters, and of those {lost} loses its",
        "// rightmost column off the edge. Accepted as-is: no glyph is hand-tuned,",
        "// so the derivation stays one rule a reader can apply by eye.",
    ]
    lines += table("kGlyphsBold", glyphs, smear)
    lines += [
        "",
        "// Indexed by FontFace, in declaration order: choosing a face is a lookup,",
        "// not a branch the drawing loops carry.",
        f"const uint8_t (*const kFaces[])[{CELL}] = {{kGlyphs, kGlyphsBold}};",
        "",
        "}  // namespace",
        "",
        "// Uppercasing HERE rather than in the table is what keeps the UI's all-caps",
        "// voice one rendering decision instead of five hundred call sites'",
        "// spelling. The tables carry the face's real lowercase, so letting copy",
        "// speak in mixed case is a one-line change to this function — deliberate,",
        "// which is the point.",
        "const uint8_t* fontGlyph(char c, FontFace face) {",
        "    int cp = std::toupper(static_cast<unsigned char>(c));",
        "    if (cp < kFirstGlyph || cp > kLastGlyph) return nullptr;",
        "    return kFaces[static_cast<int>(face)][cp - kFirstGlyph];",
        "}",
        "",
        "}  // namespace mal",
        "",
    ]
    return "\n".join(lines)


def main():
    glyphs = rasterise()
    with open(OUT, "w") as f:
        f.write(emit(glyphs))
    print(f"wrote {OUT} — {len(glyphs)} glyphs x 2 faces, "
          f"{len(glyphs) * CELL * 2} bytes")


if __name__ == "__main__":
    main()
