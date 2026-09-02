#!/usr/bin/env python3
"""
gen_item_icons.py — the pantry's 1-bit icon vocabulary, and the glyphs built from it.

It owns one non-food family too: the USB devices (§ "the USB port" below). They earn a
place here for exactly the reason the pantry did — five drawings that have to read as one
set and differ only in the mark cut into a shared body, which is what a recipe table is
for and what hand work drifts away from. Nothing else outside the pantry belongs here
unless it has the same shape of problem.

This is an AUTHORING step, not a build step, and it owns the PNGs it emits: the
committed files under assets/icons/ are what the atlas compiles, and this is what
they are regenerated FROM. Run it by hand when a recipe changes:

    python3 tools/gen_item_icons.py                 # write every recipe's glyph
    python3 tools/gen_item_icons.py --check         # committed art still matches?
    python3 tools/gen_item_icons.py --preview spam  # ASCII one glyph to the terminal

Why a tool at all
-----------------
An ITEMS glyph is 20x20, one ink, no anti-aliasing: pure white on transparent, tinted
at draw time (items_screen.h's itemIcon). At that size a food is legible only as a
SILHOUETTE with one or two cuts in it, and there are enough of them that hand work
drifts — one gets a 1px stem, the next a 3px one, and the shelf stops reading as a
set. So the vocabulary below is the style, expressed as the only shapes a pantry glyph
is allowed to be made of, and an item is a RECIPE over it.

The rules the vocabulary enforces, because they cannot be expressed otherwise:
  * one ink, full alpha or none — no greys to lose on a tint;
  * strokes are 2px, because a 1px line disappears under the x1.75 upscale;
  * the drawing sits in rows 1..17 with a 1px margin, so nothing clips against a
    neighbouring cell;
  * every form is a closed mass with its detail CUT OUT of it rather than drawn on
    top, which is what keeps a 20px shape readable when it is tinted a single colour.

A FORM is the food's outline. A recipe picks one and varies it — how many segments,
where the cut goes, what sits on top — so two dishes sharing a form still read apart.
The forms are deliberately food-agnostic: `dome` is a loaf, a bun, a scoop or a hill
of rice depending on what a recipe cuts into it.
"""

import argparse
import os
import sys

try:
    from PIL import Image
except ImportError:  # pragma: no cover - the tool is optional, the gate is not
    print("gen_item_icons: needs Pillow (pip install pillow)", file=sys.stderr)
    sys.exit(2)

W = H = 20
ICON_DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                        "assets", "icons")


# --- The canvas -------------------------------------------------------------

class G:
    """A 20x20 one-ink grid. `set` draws, `clear` cuts detail back out of a mass."""

    def __init__(self):
        self.g = [[0] * W for _ in range(H)]

    def px(self, x, y, on=1):
        if 0 <= x < W and 0 <= y < H:
            self.g[y][x] = on

    def rect(self, x0, y0, x1, y1, on=1):
        for y in range(min(y0, y1), max(y0, y1) + 1):
            for x in range(min(x0, x1), max(x0, x1) + 1):
                self.px(x, y, on)

    def sellipse(self, cx, cy, rx, ry, n=3.0, on=1):
        """A superellipse — blockier than a circle, which is the house look at 20px."""
        for y in range(H):
            for x in range(W):
                dx = abs((x - cx) / max(rx, 0.5))
                dy = abs((y - cy) / max(ry, 0.5))
                if dx ** n + dy ** n <= 1.0:
                    self.px(x, y, on)

    def disc(self, cx, cy, r, on=1):
        self.sellipse(cx, cy, r, r, 2.6, on)

    def poly(self, pts, on=1):
        """Even-odd scanline fill over integer points."""
        for y in range(H):
            xs = []
            for i in range(len(pts)):
                x0, y0 = pts[i]
                x1, y1 = pts[(i + 1) % len(pts)]
                if y0 == y1:
                    continue
                if min(y0, y1) <= y < max(y0, y1):
                    xs.append(x0 + (y - y0) * (x1 - x0) / (y1 - y0))
            xs.sort()
            for i in range(0, len(xs) - 1, 2):
                for x in range(int(round(xs[i])), int(round(xs[i + 1])) + 1):
                    self.px(x, y, on)

    def stroke(self, pts, w=2, on=1):
        """A thick polyline — the 2px minimum is the point of it."""
        r = max(w // 2, 0)
        for i in range(len(pts) - 1):
            x0, y0 = pts[i]
            x1, y1 = pts[i + 1]
            steps = max(abs(x1 - x0), abs(y1 - y0), 1) * 3
            for s in range(steps + 1):
                x = x0 + (x1 - x0) * s / steps
                y = y0 + (y1 - y0) * s / steps
                for dy in range(-r, r + 1 if w % 2 else r):
                    for dx in range(-r, r + 1 if w % 2 else r):
                        self.px(int(round(x)) + dx, int(round(y)) + dy, on)

    def hline(self, x0, x1, y, w=2, on=1):
        self.rect(x0, y, x1, y + w - 1, on)

    def vline(self, x, y0, y1, w=2, on=1):
        self.rect(x, y0, x + w - 1, y1, on)

    def ascii(self):
        return "\n".join("".join("#" if c else "." for c in row) for row in self.g)

    def image(self):
        im = Image.new("RGBA", (W, H), (255, 255, 255, 0))
        px = im.load()
        for y in range(H):
            for x in range(W):
                if self.g[y][x]:
                    px[x, y] = (255, 255, 255, 255)
        return im


# --- The forms --------------------------------------------------------------
#
# Each takes the grid and a few numbers. None of them knows what food it is; the
# recipes below decide that by which cuts they ask for.

def bowl(g, rim=8, deep=16, contents=1):
    """A bowl seen side-on: contents band, then the vessel under it."""
    g.poly([(2, deep - 6), (17, deep - 6), (14, deep), (5, deep)])
    g.hline(2, 17, rim, 2)
    if contents:
        g.sellipse(9.5, rim - 1, 6.5, 2.4, 3.0)
        g.rect(4, rim - 3, 15, rim - 3, 0)
    g.rect(7, deep + 1, 12, deep + 2)


def plate(g, mound=3):
    """A wide plate with a heap on it — the generic 'a dish' form."""
    g.sellipse(9.5, 8 - mound // 2, 6.5, 2.0 + mound * 0.7, 3.0)
    g.hline(1, 18, 13, 2)
    g.poly([(1, 13), (18, 13), (15, 16), (4, 16)])


def dome(g, top=3, wide=7, scores=0):
    """A rounded mass on a flat base: loaf, bun, scoop, hill of rice.

    `scores` is what keeps eight of these apart — the diagonal slashes a baker cuts
    into a crust. Zero of them is a smooth scoop, three is unmistakably a loaf.
    """
    g.sellipse(9.5, 11, wide, 11 - top, 3.0)
    g.rect(0, 15, 19, 19, 0)
    g.hline(9 - wide, 10 + wide, 14, 2)
    for i in range(scores):
        x = 9 - (scores - 1) * 2 + i * 4
        g.stroke([(x - 2, 12 - top), (x + 1, 8 - top)], 1, 0)


def cup(g, handle=1, tall=0):
    top = 5 - tall
    g.poly([(4, top), (15, top), (13, 16), (6, 16)])
    g.hline(3, 16, top, 2)
    if handle:
        g.stroke([(15, top + 3), (18, top + 4), (18, top + 7), (15, top + 8)], 2)


def glass(g, stem=0, fill=1):
    if stem:
        g.poly([(5, 3), (15, 3), (12, 11), (8, 11)])
        g.vline(9, 11, 15, 2)
        g.hline(6, 13, 16, 2)
        if fill:
            g.rect(7, 5, 12, 7, 0)
    else:
        g.poly([(5, 3), (14, 3), (13, 17), (6, 17)])
        g.hline(4, 15, 3, 2)
        if fill:
            g.rect(7, 6, 11, 7, 0)


def bottle(g, neck=2, cap=1):
    g.rect(9 - neck, 2, 10 + neck, 6)
    g.sellipse(9.5, 12, 6, 6, 3.2)
    g.rect(0, 18, 19, 19, 0)
    if cap:
        g.hline(9 - neck - 1, 10 + neck + 1, 1, 2)


def jar(g, lid=1, contents=2):
    g.rect(4, 5, 15, 17)
    if lid:
        g.hline(3, 16, 2, 3)
    for i in range(contents):
        g.disc(7 + (i % 2) * 5, 9 + i * 3, 1.6, 0)


def leafy(g, blades=3):
    """A leaf cluster on a short stalk."""
    for i in range(blades):
        off = (i - (blades - 1) / 2) * 4
        g.poly([(9 + int(off), 15), (7 + int(off), 8 - abs(int(off))),
                (9 + int(off), 3 + abs(int(off)) // 2), (11 + int(off), 8 - abs(int(off)))])
    g.vline(9, 13, 17, 2)


def root(g, tops=2, fat=4):
    g.poly([(9 - fat, 6), (10 + fat, 6), (9, 17)])
    for i in range(tops):
        off = (i - (tops - 1) / 2) * 4
        g.stroke([(9 + int(off), 6), (9 + int(off) * 2, 1)], 2)


def fruit(g, r=6, stem=1, leaf=1):
    g.sellipse(9.5, 11, r, r - 0.5, 3.0)
    if stem:
        g.vline(9, 3, 6, 2)
    if leaf:
        g.poly([(11, 4), (15, 2), (15, 5)])


def berries(g, n=3):
    spots = [(6, 12, 3.4), (13, 12, 3.4), (9.5, 7, 3.4), (9.5, 15, 3.0)]
    for i in range(min(n, len(spots))):
        cx, cy, r = spots[i]
        g.disc(cx, cy, r)
    for i in range(min(n, len(spots))):
        cx, cy, r = spots[i]
        g.disc(cx, cy - r + 1, 0.9, 0)


def fish(g, tail=1):
    g.sellipse(8.5, 11, 6.5, 4.2, 3.0)
    g.poly([(6, 7), (10, 3), (11, 7)])
    if tail:
        g.poly([(14, 11), (19, 6), (19, 16)])
    g.disc(4.5, 10, 1.3, 0)


def bird(g, legs=1):
    g.sellipse(9.5, 11, 7, 5, 3.0)
    g.sellipse(9.5, 6, 3.5, 2.6, 3.0)
    if legs:
        g.stroke([(6, 16), (5, 18)], 2)
        g.stroke([(13, 16), (14, 18)], 2)
    g.rect(0, 18, 19, 19, 0)


def nut(g, halves=1):
    """A kernel — narrow at the top, broad at the bottom, with a seam. Deliberately
    NOT a circle: `fruit` already owns the round silhouette."""
    g.poly([(9, 2), (15, 9), (14, 16), (5, 16), (4, 9)])
    g.sellipse(9.5, 13, 5.5, 4, 3.0)
    if halves:
        g.stroke([(9, 4), (9, 16)], 2, 0)
    else:
        g.stroke([(6, 8), (13, 11)], 1, 0)


def tubes(g, n=3, diag=1, cut=0):
    for i in range(n):
        x = 3 + i * 5
        if diag:
            g.stroke([(x, 15), (x + 4, 5)], 3)
            g.px(x + 4, 5, 0)
            g.px(x + 3, 5, 0)
        else:
            g.vline(x, 4, 16, 3)
            g.rect(x, 4, x + 2, 5, 0)
    # `cut` severs every strand on one line — the difference between a pasta that
    # is joined end to end and one that has been unlinked. Erased rather than
    # drawn, per the vocabulary's rule that detail is cut OUT of a closed mass.
    if cut:
        g.rect(0, 9, 19, 10, 0)


def nest(g, rows=3):
    g.sellipse(9.5, 11, 8, 5.5, 3.0)
    for i in range(rows):
        g.hline(4, 15, 8 + i * 2, 1, 0)


def block(g, faces=1):
    g.rect(3, 6, 14, 17)
    g.poly([(3, 6), (8, 2), (19, 2), (14, 6)])
    g.poly([(14, 6), (19, 2), (19, 13), (14, 17)])
    if faces:
        g.stroke([(14, 6), (14, 17)], 1, 0)
        g.stroke([(3, 6), (14, 6)], 1, 0)


def cone(g, stick=0, scoops=1):
    if stick:
        g.rect(8, 4, 11, 15)
        g.sellipse(9.5, 5, 4.5, 4, 3.0)
        g.vline(9, 15, 19, 2)
    else:
        g.poly([(3, 9), (16, 9), (9.5, 18)])
        for i in range(scoops):
            g.disc(9.5 - i * 3, 6 - i * 2, 4.0)


def wedge(g, layers=2):
    g.poly([(2, 16), (17, 16), (17, 6), (2, 16)])
    g.poly([(2, 16), (17, 6), (17, 4), (4, 15)])
    for i in range(1, layers):
        g.stroke([(4 + i, 16 - i), (17, 13 - i * 3)], 1, 0)


def ring(g, n=2):
    for i in range(n):
        cx = 6 + i * 7
        cy = 8 + i * 4
        g.disc(cx, cy, 5.2)
        g.disc(cx, cy, 2.2, 0)


def skewer(g, n=3):
    g.stroke([(2, 17), (18, 3)], 2)
    for i in range(n):
        t = 0.2 + i * (0.6 / max(n - 1, 1))
        cx = 2 + (16) * t
        cy = 17 - (14) * t
        g.sellipse(cx, cy, 3.4, 3.0, 3.0)
        g.px(int(cx), int(cy), 0)


def links(g, n=2):
    for i in range(n):
        g.stroke([(3, 7 + i * 6), (10, 5 + i * 6), (16, 9 + i * 6)], 4)


def steak(g, bone=1):
    g.sellipse(10, 11, 7, 5.5, 3.0)
    if bone:
        g.stroke([(4, 5), (4, 17)], 2)
        g.disc(4, 5, 2.0)
        g.disc(4, 17, 2.0)
    g.stroke([(11, 8), (13, 13)], 1, 0)


def waffle(g, cells=3):
    g.sellipse(9.5, 10, 8, 7, 3.4)
    step = 14 // (cells + 1)
    for i in range(1, cells + 1):
        g.hline(2, 17, 3 + i * step, 1, 0)
        g.vline(2 + i * step, 3, 17, 1, 0)


def pod(g, peas=3):
    g.poly([(2, 13), (7, 5), (13, 4), (18, 8), (13, 15), (6, 16)])
    for i in range(peas):
        g.disc(5.5 + i * 3.4, 12 - i * 2.4, 1.5, 0)


def sweet(g, twist=1):
    g.sellipse(9.5, 10, 4.6, 4.2, 3.0)
    if twist:
        g.poly([(5, 10), (1, 6), (1, 14)])
        g.poly([(14, 10), (19, 6), (19, 14)])
    g.stroke([(8, 8), (11, 12)], 1, 0)


def pot(g, lid=1):
    g.rect(3, 7, 16, 17)
    if lid:
        g.hline(2, 17, 5, 2)
        g.rect(9, 2, 10, 4)
    g.rect(0, 9, 2, 12)
    g.rect(17, 9, 19, 12)


def tin(g, ring_pull=1):
    g.rect(5, 4, 14, 17)
    g.sellipse(9.5, 4, 5, 2, 3.0)
    if ring_pull:
        g.disc(9.5, 4, 1.6, 0)
    g.hline(5, 14, 9, 1, 0)


def flatbread(g, holes=4):
    g.sellipse(9.5, 10, 8, 6.5, 3.2)
    spots = [(6, 8), (13, 8), (9, 12), (14, 13), (5, 13)]
    for i in range(min(holes, len(spots))):
        g.disc(spots[i][0], spots[i][1], 1.2, 0)


def dumpling(g, pleats=3):
    g.sellipse(9.5, 12, 7, 5, 3.0)
    g.poly([(3, 10), (16, 10), (14, 6), (5, 6)])
    for i in range(pleats):
        g.vline(5 + i * 4, 6, 9, 1, 0)


def nugget(g, legs=2, plates=3):
    """A dino-shaped nugget. The SILHOUETTE is the whole joke, so this is one closed
    mass with no interior detail — a breaded nugget has no eye to draw, and a 1px one
    would vanish under the x1.75 upscale even if it did.

    A STEGOSAURUS, and the choice is the whole reason it reads. At 20px a dinosaur has
    room for exactly one unmistakable feature, and the plate ridge is it: a long neck
    over a round body drew a horse, and a big skull over two legs drew a dog. The
    plates say "dinosaur" with nothing else having to.

    Two rules the shape has to keep, both learned by breaking them:
      * plates are TRAPEZOIDS, never triangles — a 1px apex reads as dirt, not a plate;
      * their tops must clear the back with a gap between them, or they merge into one
        wall and the ridge becomes a single spike.
    """
    g.sellipse(10, 12, 4.6, 2.4, 3.0)                  # body, low and long
    g.sellipse(2.5, 13, 1.9, 1.6, 3.0)                 # small head, low and forward
    g.stroke([(4, 13), (6, 12)], 3)                    # short neck
    g.poly([(14, 11), (18, 12), (18, 14), (14, 14)])   # thick blunt tail
    if legs:
        g.vline(7, 14, 17, 3)
        g.vline(12, 14, 17, 3)
    ridge = [((4, 11), (5, 7), (7, 7), (8, 11)),       # tops at y7 / y6 / y7
             ((8, 11), (9, 6), (11, 6), (12, 11)),
             ((12, 11), (13, 7), (15, 7), (16, 11))]
    for spine in ridge[:plates]:
        g.poly(list(spine))


def bun(g, cross=1):
    g.sellipse(9.5, 11, 7, 6, 3.0)
    g.rect(0, 17, 19, 19, 0)
    if cross:
        g.hline(4, 15, 10, 2, 0)
        g.vline(9, 6, 16, 2, 0)


def pretzel(g):
    g.disc(6, 12, 4.6)
    g.disc(13, 12, 4.6)
    g.disc(6, 12, 2.0, 0)
    g.disc(13, 12, 2.0, 0)
    g.stroke([(6, 8), (9.5, 3), (13, 8)], 3)
    g.disc(9.5, 5, 1.2, 0)


def egg(g, cracked=0):
    g.sellipse(9.5, 11, 5.5, 7, 3.0)
    if cracked:
        g.stroke([(4, 10), (7, 12), (10, 9), (13, 12), (16, 10)], 1, 0)


def stack(g, n=3):
    for i in range(n):
        y = 16 - i * 4
        w = 8 - i
        g.sellipse(9.5, y, w, 2.0, 3.0)


def bar(g, segs=3):
    g.rect(2, 7, 17, 14)
    for i in range(1, segs):
        g.vline(2 + i * (15 // segs), 7, 14, 1, 0)
    g.hline(2, 17, 10, 1, 0)


def drop(g, n=1):
    """A droplet: heavy and round at the BOTTOM, pointed at the top. The other way
    up it reads as an arrow, which is the one thing a food glyph must not do."""
    for i in range(n):
        cx = 9.5 + (i - (n - 1) / 2) * 7
        r = 4.6 if n == 1 else 3.6
        g.sellipse(cx, 13, r, r, 2.8)
        g.poly([(cx - r + 1, 12), (cx + r - 1, 12), (cx, 3)])


def pepper(g, stalk=1, slit=0):
    g.sellipse(9.5, 12, 5.5, 6, 3.0)
    if stalk:
        g.vline(9, 2, 7, 2)
        g.hline(6, 13, 5, 2)
    # `slit` is what separates one pepper from another on a shelf that has more than
    # one: a cut down the body, so a chili reads apart from a bell at 20px.
    if slit:
        g.vline(9, 10, 16, 1, 0)


def mushroom(g, spots=2):
    g.sellipse(9.5, 9, 8, 5, 3.0)
    g.rect(0, 10, 19, 19, 0)
    g.rect(7, 10, 12, 17)
    for i in range(spots):
        g.disc(6 + i * 7, 7, 1.3, 0)


def sprig(g, ears=5):
    """An ear of grain: one stalk, grains in pairs up it. Blobs, not hairs — a 1px
    whisker vanishes at this size, which is what made the first version noise."""
    g.vline(9, 10, 18, 2)
    for i in range(ears):
        y = 15 - i * 2
        if y < 2:
            break
        g.sellipse(7, y, 2.2, 1.3, 3.0)
        g.sellipse(12, y - 1, 2.2, 1.3, 3.0)


def card(g, holes=4):
    g.rect(2, 5, 17, 15)
    for i in range(holes):
        g.rect(4 + i * 3, 8, 5 + i * 3, 9, 0)
        g.rect(4 + i * 3, 11, 5 + i * 3, 12, 0)


def wafer(g, layers=3):
    for i in range(layers):
        g.rect(3, 5 + i * 4, 16, 7 + i * 4)


def seeds(g, n=6):
    spots = [(5, 7), (10, 5), (15, 8), (7, 12), (13, 13), (10, 16), (4, 15)]
    for i in range(min(n, len(spots))):
        g.sellipse(spots[i][0], spots[i][1], 2.2, 1.4, 3.0)


def spiral(g, turns=3):
    g.sellipse(9.5, 10, 8, 8, 3.2)
    for i in range(turns):
        r = 6.5 - i * 2.2
        g.sellipse(9.5, 10, r, r, 3.2, 0)
        g.sellipse(9.5, 10, r - 1, r - 1, 3.2, 1)


# --- The USB port -----------------------------------------------------------
#
# One body, five marks. Every USB item steers the same boundary (the pet's next
# evolution), and the shelf has to say so at a glance: same silhouette, and the ONE
# difference between two of them is what has been cut into the body. So the mark is the
# whole recipe, and the two that carry a factor say it by how many holes they have —
# countable in grayscale, which a difference of shading would not be.
#
# The body reproduces the hand-drawn Ambig-USB exactly (that glyph shipped before this
# form existed); bringing it under the recipe is what makes the other four its siblings
# rather than four drawings that resemble it.

def _cut(g, pts, w=2):
    """Cut a stroke out of the USB BODY only — never the connector or the outline.

    Drawn on a scratch grid and applied through the body window, so a mark can be
    authored at its natural length without eating the 2px wall that holds the shape
    together. The window is the body's interior: rows 10..15, columns 5..14.
    """
    m = G()
    m.stroke(pts, w, 1)
    for y in range(10, 16):
        for x in range(5, 15):
            if m.g[y][x]:
                g.px(x, y, 0)


def usb(g, mark="slot", pips=0):
    """A USB stick seen face-on: hollow connector, shouldered body, one mark cut in."""
    g.rect(6, 1, 13, 6)          # connector...
    g.rect(8, 3, 11, 5, 0)       # ...hollowed, so it reads as contacts rather than a tab
    g.rect(4, 7, 15, 8)          # the shoulder where it widens into the body
    g.rect(3, 9, 16, 15)
    g.rect(4, 16, 15, 16)        # ...tucked at the bottom, matching the shoulder
    if mark == "slot":
        g.rect(5, 12, 14, 12, 0)         # the plain seam: an ordinary stick
    elif mark == "cross":
        _cut(g, [(6, 11), (13, 15)])     # struck through — the branch is forced BAD
        _cut(g, [(13, 11), (6, 15)])
    elif mark == "tick":
        _cut(g, [(7, 12), (9, 14), (13, 10)])   # signed off — the branch is forced GOOD
    elif mark == "pips":
        # The soak pair, and the pip COUNT is the factor: two holes for x2, four for x4.
        spots = [(6, 11), (11, 11), (6, 14), (11, 14)] if pips == 4 else [(6, 12), (11, 12)]
        for (x, y) in spots[:pips]:
            g.rect(x, y, x + 2, y + 1, 0)


# --- The recipes ------------------------------------------------------------
#
# One row per item id. The FORM carries the read; the arguments are what keeps two
# dishes on the same form apart. Ordered the way the pantry is: staples, then the
# dishes that cook out of them.

RECIPES = {
    # --- staples: produce -------------------------------------------------
    "self_signed_flour":       (bottle, dict(neck=3, cap=1)),
    "shellots":                (root,   dict(tops=3, fat=3)),
    "linkguine":               (tubes,  dict(n=4, diag=1)),
    # The severed twin of linkguine above — same strands, cut across.
    "unlinkguine":             (tubes,  dict(n=4, diag=1, cut=1)),
    "jailapeno":               (pepper, dict(stalk=1, slit=1)),
    # The everyday ration, and the most-seen food glyph in the game — it gets its own
    # form rather than a shared one, because the dino shape IS the name.
    "dyno_nuggets":            (nugget, dict(legs=2, plates=3)),
    "churned_butter":          (block,  dict(faces=1)),
    "bytesteak_tomatoes":      (fruit,  dict(r=6, stem=1, leaf=1)),
    "gherkins":                (pod,    dict(peas=0)),
    "cruds":                   (bowl,   dict(rim=7, deep=16, contents=1)),
    "bootmeal":                (bowl,   dict(rim=8, deep=16, contents=1)),
    "garlic_escapes":          (leafy,  dict(blades=3)),
    "grepefruit":              (fruit,  dict(r=7, stem=0, leaf=0)),
    "red_herring":             (fish,   dict(tail=1)),
    "papaya":                  (fruit,  dict(r=5, stem=1, leaf=1)),
    "mozillarella":            (dome,   dict(top=4, wide=6, scores=0)),
    "imaple_syrup":            (bottle, dict(neck=2, cap=1)),
    "double_precision_cream":  (bottle, dict(neck=3, cap=0)),
    "cocoa":                   (bar,    dict(segs=3)),
    "rubber_ducks":            (bird,   dict(legs=0)),
    "honeypot_yogurt":         (jar,    dict(lid=1, contents=1)),
    "parsenips":               (root,   dict(tops=2, fat=4)),
    "romaine":                 (leafy,  dict(blades=5)),
    "bitroot":                 (root,   dict(tops=3, fat=5)),
    "swiss_chard":             (leafy,  dict(blades=4)),
    "string_beans":            (pod,    dict(peas=0)),
    "snap_peas":               (pod,    dict(peas=3)),
    "squash":                  (fruit,  dict(r=7, stem=1, leaf=0)),
    "raidicchio":              (dome,   dict(top=2, wide=7, scores=2)),
    "awkra":                   (pod,    dict(peas=2)),
    "kaliflower":              (mushroom, dict(spots=3)),
    "archichoke":              (spiral, dict(turns=3)),
    "flatpak_choi":            (leafy,  dict(blades=2)),
    "capsicum":                (pepper, dict(stalk=1)),
    "peppermint":              (leafy,  dict(blades=3)),
    "nixtamal":                (sprig,  dict(ears=6)),
    "pingapple":               (fruit,  dict(r=5, stem=0, leaf=1)),
    "plaintain":               (links,  dict(n=1)),
    "cloudberries":            (berries, dict(n=4)),
    "lintils":                 (seeds,  dict(n=7)),
    "perl_barley":             (sprig,  dict(ears=4)),
    "basicmati_rice":          (seeds,  dict(n=6)),
    "vpenne":                  (tubes,  dict(n=3, diag=0)),
    "unmonitored_oats":        (bowl,   dict(rim=9, deep=17, contents=1)),
    "yamls":                   (root,   dict(tops=1, fat=5)),
    "epoch_dates":             (seeds,  dict(n=4)),
    "dotfigs":                 (fruit,  dict(r=6, stem=1, leaf=0)),
    "apiricot":                (fruit,  dict(r=5, stem=1, leaf=0)),
    "raspberry_pis":           (berries, dict(n=3)),
    "table_grapes":            (berries, dict(n=4)),
    "lambda_chops":            (steak,  dict(bone=1)),
    "file_mignon":             (steak,  dict(bone=0)),
    "minified_beef":           (dome,   dict(top=6, wide=8, scores=0)),
    "saasage":                 (links,  dict(n=2)),
    "packed_sardines":         (tin,    dict(ring_pull=1)),
    "natto":                   (bowl,   dict(rim=7, deep=15, contents=1)),
    "paramesan":               (wedge,  dict(layers=1)),
    "macadamia":               (nut,    dict(halves=0)),
    "cache_ews":               (nut,    dict(halves=1)),
    "chia_seeds":              (seeds,  dict(n=5)),
    "cinnamon":                (sprig,  dict(ears=3)),
    "squid_ink":               (bottle, dict(neck=1, cap=1)),
    "leaf_node_tea":           (cup,    dict(handle=1, tall=0)),
    "mixins":                  (glass,  dict(stem=0, fill=1)),
    "silicon_wafers":          (wafer,  dict(layers=3)),
    "nibbles":                 (seeds,  dict(n=4)),
    "marshalled_mallows":      (stack,  dict(n=3)),
    "humbugs":                 (sweet,  dict(twist=1)),
    "burp_sweets":             (sweet,  dict(twist=0)),
    "peer_drops":              (drop,   dict(n=2)),

    # --- the third service ------------------------------------------------
    "portridge":               (bowl,   dict(rim=8, deep=16, contents=1)),
    "halloumi_world":          (block,  dict(faces=0)),
    "nan_bread":               (flatbread, dict(holes=4)),
    "chrootons":               (seeds,  dict(n=6)),
    "gzipacho":                (bowl,   dict(rim=6, deep=15, contents=1)),
    "lossy_lassi":             (glass,  dict(stem=0, fill=1)),
    "cod_review":              (fish,   dict(tail=0)),
    "recursive_turducken":     (bird,   dict(legs=1)),
    "peking_duck_typing":      (bird,   dict(legs=1)),
    "semaphreddo":             (wedge,  dict(layers=3)),
    "spaghetti_code":          (nest,   dict(rows=4)),
    "emacsaroni":              (bowl,   dict(rim=7, deep=16, contents=1)),
    "bisectuits":              (stack,  dict(n=3)),
    "quicksortbet":            (cone,   dict(stick=0, scoops=2)),

    # --- the fourth service: bread and bakery -----------------------------
    "buguette":                (links,  dict(n=1)),
    "chapati":                 (flatbread, dict(holes=3)),
    "corrumpets":              (flatbread, dict(holes=5)),
    "packettone":              (dome,   dict(top=2, wide=7, scores=1)),
    "hot_swapped_buns":        (bun,    dict(cross=1)),
    "current_buns":            (bun,    dict(cross=0)),
    "config_rolls":            (links,  dict(n=2)),
    "crostini":                (wafer,  dict(layers=2)),
    "payloaf":                 (dome,   dict(top=4, wide=8, scores=3)),
    "firewaffle":              (waffle, dict(cells=3)),

    # --- the fourth service: the pans -------------------------------------
    "chownder":                (bowl,   dict(rim=7, deep=16, contents=1)),
    "cronsomme":               (bowl,   dict(rim=9, deep=16, contents=0)),
    "wanton_soup":             (dumpling, dict(pleats=3)),
    "piperogi":                (dumpling, dict(pleats=4)),
    "queuesadilla":            (wedge,  dict(layers=2)),
    "ravioli_code":            (block,  dict(faces=1)),
    "idleys":                  (stack,  dict(n=2)),
    "ms_dosa":                 (spiral, dict(turns=2)),
    "arpas":                   (flatbread, dict(holes=2)),
    "kafkofta":                (skewer, dict(n=3)),
    "kernel_panini":           (wafer,  dict(layers=3)),
    "racelette":               (pot,    dict(lid=0)),
    "scrambled_regeggs":       (egg,    dict(cracked=1)),
    "char_grilled_array":      (skewer, dict(n=4)),
    "tarballs":                (berries, dict(n=3)),
    "bashed_potatoes":         (plate,  dict(mound=4)),
    "onion_rings":             (ring,   dict(n=2)),
    "flash_fried_chips":       (tubes,  dict(n=4, diag=1)),
    "twisted_pairetzels":      (pretzel, dict()),
    "jitter_fritters":         (berries, dict(n=4)),
    "shashimi":                (wafer,  dict(layers=2)),

    # --- the fourth service: the plates -----------------------------------
    "spare_ribs":              (links,  dict(n=3)),
    "rested_steak":            (steak,  dict(bone=1)),
    "privilege_escalope":      (plate,  dict(mound=2)),
    "force_pulled_pork":       (plate,  dict(mound=5)),
    "rubber_duck_confit":      (bird,   dict(legs=0)),
    "vacuum_sealed_leftovers": (block,  dict(faces=0)),
    "disk_platter":            (ring,   dict(n=1)),
    "serverless_platter":      (plate,  dict(mound=3)),
    "pickle_jar":              (jar,    dict(lid=1, contents=3)),

    # --- the fourth service: condiments -----------------------------------
    "ai_oli":                  (drop,   dict(n=1)),
    "vinaigrette":             (bottle, dict(neck=1, cap=0)),
    "malwarmalade":            (jar,    dict(lid=1, contents=2)),
    "signal_jam":              (jar,    dict(lid=0, contents=2)),

    # --- the fourth service: puddings -------------------------------------
    "pop3sicle":               (cone,   dict(stick=1)),
    "mergingue":               (dome,   dict(top=1, wide=6, scores=0)),
    "declair":                 (links,  dict(n=1)),
    "profilerole":             (stack,  dict(n=3)),
    "coboler":                 (wedge,  dict(layers=2)),
    "clustard":                (bowl,   dict(rim=8, deep=15, contents=1)),
    "bashlava":                (wedge,  dict(layers=4)),
    "deflated_souffle":        (cup,    dict(handle=0, tall=1)),
    "fork_bombe":              (dome,   dict(top=1, wide=8, scores=2)),
    "optical_mousse":          (cup,    dict(handle=0, tall=0)),
    "cherry_picked_tart":      (wedge,  dict(layers=1)),
    "raspberry_pie":           (dome,   dict(top=5, wide=8, scores=3)),
    "rainbow_tablet":          (bar,    dict(segs=4)),
    "mint_choc_chip":          (cone,   dict(stick=0, scoops=1)),
    "candied_yamls":           (root,   dict(tops=2, fat=5)),

    # --- the fourth service: what you drink with them ---------------------
    "flat_white":              (cup,    dict(handle=1, tall=1)),
    "mockachino":              (cup,    dict(handle=1, tall=0)),
    "blockchai":               (cup,    dict(handle=0, tall=0)),
    "syn_ack_shake":           (glass,  dict(stem=0, fill=0)),
    "buffer_overfloat":        (glass,  dict(stem=1, fill=1)),
    "hard_cidr":               (bottle, dict(neck=2, cap=0)),
    "port_80":                 (glass,  dict(stem=1, fill=0)),
    "fizzbuzz":                (tin,    dict(ring_pull=1)),
    "punchcard_punch":         (card,   dict(holes=4)),

    # --- the USB port -----------------------------------------------------
    # One body, one mark each. The two branch overrides are a PAIR and read as one:
    # struck through vs. signed off, same stick. The soak pair counts its factor in
    # holes, so x2 and x4 are told apart by counting rather than by reading.
    "ambig_usb":               (usb,    dict(mark="slot")),
    "bad_usb":                 (usb,    dict(mark="cross")),
    "signed_usb":              (usb,    dict(mark="tick")),
    "sandbox_usb":             (usb,    dict(mark="pips", pips=2)),
    "hypervisor_usb":          (usb,    dict(mark="pips", pips=4)),
}


def render(item_id):
    form, kwargs = RECIPES[item_id]
    g = G()
    form(g, **kwargs)
    return g


def asset_path(item_id):
    return os.path.join(ICON_DIR, "ICON_ITEM_%s.png" % item_id.upper())


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--check", action="store_true",
                    help="fail if any committed glyph differs from its recipe")
    ap.add_argument("--preview", metavar="ID", help="ASCII one glyph and exit")
    args = ap.parse_args()

    if args.preview:
        if args.preview not in RECIPES:
            print("no recipe for %r" % args.preview, file=sys.stderr)
            return 2
        print(render(args.preview).ascii())
        return 0

    bad = []
    for item_id in sorted(RECIPES):
        want = render(item_id).image()
        path = asset_path(item_id)
        if args.check:
            if not os.path.exists(path):
                bad.append("%s: missing" % os.path.basename(path))
                continue
            have = Image.open(path).convert("RGBA")
            if have.size != want.size or have.tobytes() != want.tobytes():
                bad.append("%s: differs from its recipe" % os.path.basename(path))
        else:
            want.save(path)

    if args.check:
        for line in bad:
            print("gen_item_icons: %s" % line, file=sys.stderr)
        if bad:
            print("gen_item_icons: run tools/gen_item_icons.py to regenerate",
                  file=sys.stderr)
            return 1
        print("gen_item_icons: %d glyphs match" % len(RECIPES))
    else:
        print("gen_item_icons: wrote %d glyphs to assets/icons/" % len(RECIPES))
    return 0


if __name__ == "__main__":
    sys.exit(main())
