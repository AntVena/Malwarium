#!/usr/bin/env python3
"""
gen_malbeast_art.py — the wild-malbeast drawing vocabulary, and the sheets built from it.

Sibling to tools/gen_worm_art.py. Same contract: an AUTHORING step that OWNS the PNGs it
emits under assets/sprites/, run by hand when a recipe changes, `--check`ed in the gates so
a hand edit to a committed sheet (which the next run would silently revert) fails loudly
instead.

    python3 tools/gen_malbeast_art.py                 # write every recipe's sheet
    python3 tools/gen_malbeast_art.py --check         # committed art still matches? (gates)
    python3 tools/gen_malbeast_art.py --preview glitchhog   # ASCII a sheet to the terminal

Why a second tool, and not the Worm one
----------------------------------------
The Worm line spends a mother STYLE instead of a mother colour (CREATURE_VISUAL_RULES §4),
and gen_worm_art.py holds that style mechanically so a dozen sprites still read as one hand.
The wild malbeasts want a style of their OWN. A malbeast is not a creature-line member (it
is a bare sprite-named spec in combat_factory.cpp, no creature row, no mother colour), so
nothing ties the roster together today, which is exactly how a style drifts. This file is
that tie.

The malbeast style, as the only things a recipe can say
-------------------------------------------------------
1.  THE 4px NIBBLE GRID. Every recipe draws on a COARSE grid of 4×4-pixel cells
    ("nibbles") — a 56×48 sprite is a 14×12 nibble field — and bake() expands each set
    nibble to a solid 4×4 block. So a malbeast is low-def BY CONSTRUCTION: there is no way
    to say a 1px detail, the way there is no way to say a second colour. This is the whole
    "thicker limbs, squares of 4 pixels, blocky like an original Digimon sprite" read, and
    it is held by the grid rather than by an eye. (It also makes the sheet almost free to
    store: a field of identical 4×4 blocks is what gen_assets.py's tile-dedup form is for,
    and a one-ink field is a 1-bit mask regardless.)

2.  LINE ART, the original V-pet way. A closed one-cell outline with the detail drawn
    INSIDE it as more ink: a solid eye, a jaw or jowl line, a curl or two of hide, toes
    as broken cells along the floor. At one nibble a line is 4px thick, which is what
    keeps it reading as a deliberate stroke on the ×1.75 panel. Draw the filled shape
    first and trace its boundary, so the outline is always closed; an open outline reads
    as a broken sprite, not as a style.

3.  ONE INK, on transparent. INK below, nothing emits another value — same reason the Worm
    file has the rule (a stray second colour costs real flash silently, and the mask store
    depends on it). The CORRUPTION — the glitch tear, the stray lit pixels, the colour — is
    NOT in the pixels: it is a render-time pass (core/render/glitch.h, drawSpriteGlitch)
    over this drawing, so the sheet stays a cheap still and the glitch animates for free.
    A malbeast on the shelf is a plain blocky line drawing; the shader is what corrupts it.

That division is the point of the whole approach: the EXPENSIVE, animated, scary half lives
in ~one engine function shared by every malbeast, and the per-creature half is two cheap
one-ink frames a recipe draws on graph paper. Adding the seventh malbeast is a recipe here
plus a stat row in combat_factory.cpp — no new FX, no new art commission for the glitch.

The sheet: two ROWS of one frame each, row 0 the idle and row 1 the attack. That is the
whole animation budget — a wild never breathes or walks, it stands, and it lunges when it
swings (fightPose picks the row). Everything else a fight has to say (stunned, poisoned,
what kind of blow) belongs to the combat FX, not to more frames here.

Facing: declared on the SHEET in gen_assets.py's FACING table (a wild has no creature row to
carry it), and the drawing is authored to match. GlitchHog is Facing::Left — snout to the
left — so the combat screen seats it looking at the pet it faces.

Pure standard library, like both sibling generators, so a fresh clone with a stock Python 3
runs `--check` in the gates. The committed PNG is decoded with gen_assets.py's OWN decoder,
so what is compared is exactly what the atlas build will see.
"""

import argparse
import os
import struct
import sys
import zlib

# The one ink, on transparent. Matches gen_worm_art.py's INK so the two 1-bit families read
# as the same material under the palette — the colour a set mask bit takes is the engine's
# business (SpriteData::ink / the glitch shader's tint), not this file's.
INK = (232, 237, 242, 255)

# One nibble is 4×4 device pixels — the low-def quantum. Every logical size below is in
# nibbles; bake() multiplies by this to reach pixels.
NIB = 4


# --------------------------------------------------------------------------------------
#  The nibble canvas — a coarse boolean field a recipe paints, expanded ×NIB at bake().
# --------------------------------------------------------------------------------------
class Nib:
    """A w×h field of 4×4-pixel cells. A recipe draws in THIS space, so it cannot express
    anything finer than a nibble — the grid is the style."""

    def __init__(self, w, h):
        self.w, self.h = w, h
        self.on = [[False] * w for _ in range(h)]

    def set(self, x, y, v=True):
        if 0 <= x < self.w and 0 <= y < self.h:
            self.on[y][x] = v
        return self

    def get(self, x, y):
        return 0 <= x < self.w and 0 <= y < self.h and self.on[y][x]

    # --- bake: nibbles -> a pixel mask, each set nibble a solid NIB×NIB block ----------
    def bake(self):
        W, H = self.w * NIB, self.h * NIB
        px = [[False] * W for _ in range(H)]
        for ny in range(self.h):
            for nx in range(self.w):
                if not self.on[ny][nx]:
                    continue
                for dy in range(NIB):
                    for dx in range(NIB):
                        px[ny * NIB + dy][nx * NIB + dx] = True
        return px, W, H

    def ascii(self, on="█", off="·"):
        return "\n".join(
            "".join(on if self.on[y][x] else off for x in range(self.w))
            for y in range(self.h)
        )


# --------------------------------------------------------------------------------------
#  PNG out — one ink on transparent, 8-bit RGBA (gen_assets.py decodes exactly this), so
#  the atlas stores the sheet as a 1-bit mask. Identical encoder to gen_worm_art.py.
# --------------------------------------------------------------------------------------
def to_png(px, w, h):
    raw = bytearray()
    for y in range(h):
        raw.append(0)  # filter: none
        for x in range(w):
            raw += bytes(INK if px[y][x] else (0, 0, 0, 0))

    def chunk(tag, data):
        return (struct.pack(">I", len(data)) + tag + data
                + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF))

    return (b"\x89PNG\r\n\x1a\n"
            + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(bytes(raw), 9))
            + chunk(b"IEND", b""))


# ======================================================================================
#  RECIPES — one function per sheet, returning a baked (px, w, h). Everything above is the
#  vocabulary; from here down is which cells each creature is.
#
#  A recipe is graph paper: one ASCII grid per frame, '#' an inked nibble and '.' an empty
#  one, exactly how a V-pet sprite is drawn on squared paper. Frames stack as ROWS of the
#  sheet — row 0 the idle, row 1 the attack — because an AnimClip selects a row, and a
#  wild malbeast's two poses are picked by row (fightPose, ui/combat_screen.cpp).
# ======================================================================================

def grid(*rows):
    """One frame from graph-paper rows. Every row must be the same width — a ragged grid
    is a typo, and a typo here is a pixel nobody meant."""
    w = len(rows[0])
    for r in rows:
        if len(r) != w:
            raise SystemExit(f"ragged grid row {r!r}: {len(r)} cells, expected {w}")
    n = Nib(w, len(rows))
    for y, r in enumerate(rows):
        for x, c in enumerate(r):
            if c == "#":
                n.set(x, y)
            elif c != ".":
                raise SystemExit(f"grid cell {c!r} is neither '#' nor '.'")
    return n


def stack(*frames):
    """Bake frames into one sheet, one frame per ROW (idle on top, attack below)."""
    w, h = frames[0].w * NIB, frames[0].h * NIB
    px = []
    for f in frames:
        if (f.w, f.h) != (frames[0].w, frames[0].h):
            raise SystemExit("every frame of a sheet must be the same size")
        fpx, _, _ = f.bake()
        px.extend(fpx)
    return px, w, h * len(frames)


def glitchhog():
    """GlitchHog — the tier-1 'net boar, facing LEFT. A closed one-cell outline with the
    detail drawn inside it: hackles along the back, a solid eye, a jowl line splitting head
    from body, one hide curl, a belly line, stubby legs. The attack lunges the whole body
    forward a cell with the head dropped and the jaw open."""
    idle = grid(
        "......#.#.#...",
        ".....#.#.#.#..",
        "...##.......#.",
        "..#.........#.",
        ".#.##.......#.",
        "#..##.#.....#.",
        "#.....#..##.#.",
        ".##...#...#.#.",
        "..##.....##.#.",
        "...#.####..##.",
        "...##.##.##.##",
        "..###.##.##.##",
    )
    attack = grid(
        "..............",
        ".......#.#.#..",
        "......#.#.#.#.",
        "....##.......#",
        "...#.##......#",
        "..#..##.#....#",
        "##......#..###",
        "#.......#...#.",
        "..##......##..",
        "##.##.####.#..",
        "..##.##.##.##.",
        ".###.##.##.##.",
    )
    return stack(idle, attack)


RECIPES = {
    "SPR_MALBEAST_GLITCHHOG": glitchhog,
}

# Short aliases for --preview / --only, so the CLI takes `glitchhog` not the full symbol.
ALIASES = {name.replace("SPR_MALBEAST_", "").lower(): name for name in RECIPES}


def path_for(sym):
    return os.path.join(os.path.dirname(__file__), "..", "assets", "sprites", sym + ".png")


def main():
    ap = argparse.ArgumentParser(description="Generate wild-malbeast sprite sheets.")
    ap.add_argument("--check", action="store_true",
                    help="fail if any committed PNG differs from its recipe (gate)")
    ap.add_argument("--preview", metavar="NAME", nargs="?", const="",
                    help="print a recipe as ASCII nibbles instead of writing it")
    ap.add_argument("--only", metavar="NAME", help="limit to one recipe (alias or symbol)")
    args = ap.parse_args()

    def resolve(name):
        if name in RECIPES:
            return name
        if name in ALIASES:
            return ALIASES[name]
        sys.exit(f"no recipe for '{name}'; have {', '.join(sorted(ALIASES))}")

    if args.preview is not None:
        name = resolve(args.preview) if args.preview else sorted(RECIPES)[0]
        px, w, h = RECIPES[name]()
        # Back down to nibble resolution: every set nibble is a solid NIB block, so its
        # top-left pixel stands for the whole cell.
        n = Nib(w // NIB, h // NIB)
        for ny in range(n.h):
            for nx in range(n.w):
                if px[ny * NIB][nx * NIB]:
                    n.set(nx, ny)
        print(f"{name}  ({w}×{h}px, {n.w}×{n.h} nibbles)\n")
        print(n.ascii())
        return

    names = [resolve(args.only)] if args.only else sorted(RECIPES)
    bad = []
    for sym in names:
        px, w, h = RECIPES[sym]()
        data = to_png(px, w, h)
        p = path_for(sym)
        if args.check:
            if not os.path.exists(p):
                bad.append(f"{sym}: no committed PNG at {p} — run without --check")
            elif open(p, "rb").read() != data:
                bad.append(f"{sym}: committed PNG differs from recipe — "
                           f"rerun `python3 tools/gen_malbeast_art.py` to regenerate")
        else:
            with open(p, "wb") as f:
                f.write(data)
            print(f"wrote {sym}  ({w}×{h})")

    if bad:
        sys.exit("\n".join(bad))


if __name__ == "__main__":
    main()
