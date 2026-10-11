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
The wild malbeasts want a style of their OWN — and the opposite one. A malbeast is not a
creature-line member (it is a bare sprite-named spec in combat_factory.cpp, no creature row,
no mother colour), so nothing ties the roster together today, which is exactly how a style
drifts. This file is that tie.

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

2.  FILLED SILHOUETTE, not outline. This is the one axis where the malbeast is the Worm's
    opposite: a worm is an outline with one solid mass, a malbeast is ALL solid mass. At
    this scale a filled blocky body reads as a squat, heavy, dumb-strong thing — the beast
    — where an outline reads as an organism. Recipes union forms into one silhouette; there
    is no "interior" to draw into.

3.  ONE INK, on transparent. INK below, nothing emits another value — same reason the Worm
    file has the rule (a stray second colour costs real flash silently, and the mask store
    depends on it). The CORRUPTION — the glitch tear, the stray lit pixels, the colour — is
    NOT in the pixels: it is a render-time pass (core/render/glitch.h, drawSpriteGlitch)
    over this silhouette, so the sheet stays a cheap still and the glitch animates for free.
    A malbeast on the shelf is a blocky black cut-out; the shader is what corrupts it.

That division is the point of the whole approach: the EXPENSIVE, animated, scary half lives
in ~one engine function shared by every malbeast, and the per-creature half is a cheap,
static, one-ink blob a recipe can draw in a dozen lines. Adding the seventh malbeast is a
recipe here plus a stat row in combat_factory.cpp — no new frames, no new FX, no new art
commission for the glitch.

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
    """A w×h field of 4×4-pixel cells. Every primitive operates in THIS space, so a recipe
    cannot express anything finer than a nibble — the grid is the style."""

    def __init__(self, w, h):
        self.w, self.h = w, h
        self.on = [[False] * w for _ in range(h)]

    def set(self, x, y, v=True):
        if 0 <= x < self.w and 0 <= y < self.h:
            self.on[y][x] = v
        return self

    def get(self, x, y):
        return 0 <= x < self.w and 0 <= y < self.h and self.on[y][x]

    # --- forms (all inclusive of their bounds, all unioned in) ------------------------
    def box(self, x0, y0, x1, y1):
        """A solid nibble rectangle — the body, a limb segment, a blunt snout."""
        for y in range(min(y0, y1), max(y0, y1) + 1):
            for x in range(min(x0, x1), max(x0, x1) + 1):
                self.set(x, y)
        return self

    def disc(self, cx, cy, r):
        """A blocky filled disc — rounds a corner of the mass without leaving the grid. A
        true circle is a smoothness 4px blocks cannot deliver, so the test is coarse
        (nibble-centre inside r + a half-cell) and the result is deliberately chunky."""
        for y in range(cy - r, cy + r + 1):
            for x in range(cx - r, cx + r + 1):
                if (x - cx) ** 2 + (y - cy) ** 2 <= (r + 0.5) ** 2:
                    self.set(x, y)
        return self

    def legs(self, xs, y0, y1):
        """Thick stubby limbs: a 1-nibble-wide (=4px) column at each x, dropped y0→y1. The
        malbeast's limbs are always one nibble — the 'squares of 4 pixels' the brief asks
        for — so a leg is a column here and nothing thinner can be said."""
        for x in xs:
            self.box(x, y0, x, y1)
        return self

    def studs(self, cells):
        """Loose single nibbles — back bristles, a tusk, an ear, a tail nub. The jagged
        read that says 'aggressive / corrupted' rather than 'pet', spent one block at a
        time so it stays on the grid."""
        for (x, y) in cells:
            self.set(x, y)
        return self

    def carve(self, x0, y0, x1, y1):
        """Clear a nibble rect — a mouth gap, the notch between two legs."""
        for y in range(min(y0, y1), max(y0, y1) + 1):
            for x in range(min(x0, x1), max(x0, x1) + 1):
                self.set(x, y, False)
        return self

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
#  vocabulary; from here down is which blocks each creature is.
# ======================================================================================

def glitchhog():
    """GlitchHog — the tier-1 'net boar. Squat and front-heavy: a heavy body block, a blunt
    snout shoved out to the LEFT (Facing::Left) with a tusk jutting up off it, a low ear,
    four one-nibble legs, a run of back bristles and a tail nub. 14×12 nibbles = 56×48 px.
    The read is 'dumb-strong and angry', all mass, no interior — the glitch shader does the
    menace, this just has to be an unmistakable blocky beast."""
    n = Nib(14, 12)

    # Body — one heavy rounded block, the thing the eye lands on. Floored at the bottom
    # row (y=11); the bottom edge is the FLOOR and is the one edge a creature may sit on.
    n.box(4, 4, 11, 9)
    n.disc(5, 6, 2)        # round the front-lower corner so the snout grows out of it
    n.disc(10, 5, 2)       # round the haunch

    # Head / snout, shoved left and low. A blunt rectangular muzzle is the hog tell.
    n.box(1, 6, 5, 9)
    n.box(0, 7, 1, 8)      # the flat snout tip poking off the left edge's inside
    n.studs([(1, 5)])      # a short tusk jutting up off the muzzle
    n.studs([(5, 3), (6, 3)])   # a low pricked ear on top of the head

    # Back bristles — the jagged top line that reads as hackles / corruption.
    n.studs([(7, 3), (9, 3), (11, 3)])

    # Tail nub, back-right.
    n.studs([(12, 5), (12, 6)])

    # Four thick legs (one nibble each), dropped to the floor. Front pair under the chest,
    # back pair under the haunch, with a carved gap between so they read as four not a slab.
    n.legs([3, 5], 9, 11)        # front pair
    n.legs([9, 11], 9, 11)       # back pair
    n.carve(6, 10, 8, 11)        # belly gap between the pairs

    return n.bake()


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
        n = Nib(14, 12)
        # Re-run the recipe against a canvas we can ASCII: recipes bake and return pixels,
        # so for preview we just rebuild on a throwaway Nib via the same body. Simplest is
        # to re-derive from the baked pixels at nibble resolution.
        px, w, h = RECIPES[name]()
        for ny in range(h // NIB):
            for nx in range(w // NIB):
                if px[ny * NIB][nx * NIB]:
                    n.set(nx, ny)
        print(f"{name}  ({w}×{h}px, {w // NIB}×{h // NIB} nibbles)\n")
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
