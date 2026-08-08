#!/usr/bin/env python3
"""
gen_worm_art.py — the Worm line's 1-bit drawing vocabulary, and the sheets built from it.

This is an AUTHORING step, not a build step, and it owns the PNGs it emits: the
committed file under assets/sprites/ is what the atlas compiles, and this is what
that file is regenerated FROM. Run it by hand when a recipe changes:

    python3 tools/gen_worm_art.py                 # write every recipe's sheet
    python3 tools/gen_worm_art.py --check         # committed art still matches? (gates)
    python3 tools/gen_worm_art.py --preview idle  # ASCII a row to the terminal

Why a tool at all
-----------------
The Worm line is the one family that spends a mother STYLE instead of a mother colour
(assets/CREATURE_VISUAL_RULES.md §4), and a style is a thing a tool can hold and a
hand cannot hold consistently across a dozen sprites. Every worm-line creature is one
ink on transparent, drawn as an OUTLINE rather than a silhouette, with exactly one
solid mass in it. Those are mechanical properties. Left to per-sprite hand work they
drift — one creature picks up a second grey, another fills its body, and the line
stops reading as a family the moment there are enough of them to compare.

So the vocabulary below is the style, expressed as the only operations a worm-line
sheet is allowed to be made of, and a creature is a RECIPE over it. A recipe cannot
express a two-colour sprite or a filled silhouette, which is the point: the rule is
enforced by what you can say, not by a reviewer noticing.

Not all of them are worms
-------------------------
The vocabulary is deliberately body-plan agnostic. `tube` along a Bézier is what makes
a worm; `superellipse` is what makes the Vermicell shell; `disc`, `rect` and `poly` are
there so a node, a segmented crawler, a many-legged thing or a coiled shell are all the
same three lines of recipe with different forms unioned in. What they SHARE is the
finishing pass — outline, chords, one solid mass — and that is the part no recipe gets
to opt out of.

The same parameters saying opposite things is the test of whether that is working.
Nodeatode and Rootgrub are both `tube` along a Bézier with chords and a mouth: one is
thin, long and led by a head, the other short, thick and led by a hole. Nothing in the
vocabulary knows which is which — the recipes do, and they differ by about a dozen
numbers and one choice of mouth.

The five rules the vocabulary encodes
-------------------------------------
1.  ONE INK, on transparent. `INK` below, and nothing may emit another value. This is
    also what lets gen_assets.py store the sheet as a 1bpp mask rather than RGBA — the
    detection there is by pixel, so a stray second colour costs real flash silently.
2.  OUTLINE, NOT SILHOUETTE. Forms go into `Cell.body`, and only their 4-connectivity
    boundary is ever drawn. At worm scale a filled shape reads as a blob and an outline
    reads as an organism, because the interior is where the detail goes.
3.  SEGMENT CHORDS. Perpendicular rungs across a body at fixed spine parameters. This
    is the single strongest tell: without them an outlined tube reads as *tube*, and
    with them it reads as *worm*.
4.  EXACTLY ONE SOLID MASS. The only filled pixels in the cell, so it is what the
    viewer's eye lands on first — which makes WHERE a recipe spends it the strongest
    single statement it makes about the creature. Most rows spend it on an eye
    (`Cell.eye`); Rootgrub spends it on the throat at the back of its maw, and that one
    choice is most of why it reads as something that eats rather than something that
    looks. `Cell.solid` refuses a second, because "the one solid thing" stops being
    true the first time a recipe adds another.
5.  SQUARED, NOT ROUND. `superellipse` at exponent ~2.4 by default. At these sizes a
    true ellipse is a smoothness the pixels cannot deliver, so the drawing commits to
    the blocky read instead of fighting for one.

Pure standard library, on purpose — same reason gen_assets.py is. `--check` runs in the
gates, so a fresh clone with a stock Python 3 has to be able to run it, which rules out
Pillow. It decodes the committed PNG with gen_assets.py's OWN decoder, so what is
compared is exactly what the atlas build will see.
"""

import argparse
import math
import os
import struct
import sys
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gen_assets import decode_png_rgba  # noqa: E402  — the atlas's own reader

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SPRITES = os.path.join(REPO, "assets", "sprites")

# The line's one ink. Matches the shipped Worm assets exactly; changing it here
# re-inks the whole family and nothing else, which is the only reason it is a
# constant rather than a literal.
INK = (232, 237, 242, 255)


# ---------------------------------------------------------------------------
#  Mask — a 1-bit raster, and the set algebra over it
# ---------------------------------------------------------------------------
class Mask:
    """A w*h grid of set/unset pixels. Every form and every stroke is one of these."""

    __slots__ = ("w", "h", "px")

    def __init__(self, w, h):
        self.w, self.h = w, h
        self.px = bytearray(w * h)

    def get(self, x, y):
        # Off-canvas reads as EMPTY, which is what makes `outline` close a form that
        # runs off the edge of its cell instead of leaving it open along the bezel.
        if x < 0 or y < 0 or x >= self.w or y >= self.h:
            return 0
        return self.px[y * self.w + x]

    def set(self, x, y, v=1):
        if 0 <= x < self.w and 0 <= y < self.h:
            self.px[y * self.w + x] = v

    def any(self):
        return any(self.px)

    def count(self):
        return sum(self.px)

    def copy(self):
        m = Mask(self.w, self.h)
        m.px[:] = self.px
        return m

    def union(self, other):
        for i, v in enumerate(other.px):
            if v:
                self.px[i] = 1
        return self

    def subtract(self, other):
        for i, v in enumerate(other.px):
            if v:
                self.px[i] = 0
        return self

    def outline(self):
        """The 4-connectivity boundary: a set pixel with at least one unset neighbour.

        Rule 2, mechanised. Note this is the boundary of the WHOLE body at once, not of
        each form separately — two forms unioned into one body share a silhouette and
        the seam between them vanishes, which is how a head reads as continuous with
        the neck it sits on rather than as a ball parked next to a tube.
        """
        out = Mask(self.w, self.h)
        for y in range(self.h):
            for x in range(self.w):
                if not self.get(x, y):
                    continue
                if not (self.get(x - 1, y) and self.get(x + 1, y)
                        and self.get(x, y - 1) and self.get(x, y + 1)):
                    out.set(x, y)
        return out

    def inset(self, d):
        """Every set pixel at least `d` from an unset one — where an eye may safely go."""
        out = self.copy()
        for _ in range(d):
            keep = Mask(self.w, self.h)
            for y in range(self.h):
                for x in range(self.w):
                    if out.get(x, y) and (out.get(x - 1, y) and out.get(x + 1, y)
                                          and out.get(x, y - 1) and out.get(x, y + 1)):
                        keep.set(x, y)
            out = keep
        return out

    def ascii(self, on="#", off="."):
        return "\n".join("".join(on if self.get(x, y) else off for x in range(self.w))
                         for y in range(self.h))


# ---------------------------------------------------------------------------
#  Forms — what a body is made of. Body-plan agnostic by design.
# ---------------------------------------------------------------------------
def disc(mask, cx, cy, r):
    """A filled circle. The head of anything, and the brush `tube` paints with."""
    r2 = r * r
    for y in range(int(cy - r) - 1, int(cy + r) + 2):
        for x in range(int(cx - r) - 1, int(cx + r) + 2):
            if (x - cx) ** 2 + (y - cy) ** 2 <= r2:
                mask.set(x, y)
    return mask


def superellipse(mask, cx, cy, rx, ry, n=2.4):
    """A squared-off ellipse: |dx/rx|^n + |dy/ry|^n <= 1. Rule 5.

    n=2 is a true ellipse and n=inf is a rectangle; ~2.4 is flat sides and top with the
    corners knocked off, which is the shape that survives being 40px wide.
    """
    for y in range(int(cy - ry) - 1, int(cy + ry) + 2):
        for x in range(int(cx - rx) - 1, int(cx + rx) + 2):
            if rx <= 0 or ry <= 0:
                continue
            if abs((x - cx) / rx) ** n + abs((y - cy) / ry) ** n <= 1.0:
                mask.set(x, y)
    return mask


def rect(mask, x0, y0, x1, y1):
    for y in range(int(y0), int(y1) + 1):
        for x in range(int(x0), int(x1) + 1):
            mask.set(x, y)
    return mask


def tube(mask, path, r0, r1, steps=160):
    """Stamp discs along `path`, radius lerping r0 (t=0) -> r1 (t=1).

    A tapered limb, body or tail. Enough steps that the stamps overlap into one solid
    form — gaps would show up as notches in the outline, which is the only way this can
    go wrong and is invisible until you look at the boundary.
    """
    for i in range(steps + 1):
        t = i / steps
        x, y = path(t)
        disc(mask, x, y, r0 + (r1 - r0) * t)
    return mask


def poly(mask, pts, r=0.0):
    """A closed filled polygon, optionally thickened by stamping discs on its edges."""
    xs = [p[0] for p in pts]
    ys = [p[1] for p in pts]
    for y in range(int(min(ys)), int(max(ys)) + 1):
        for x in range(int(min(xs)), int(max(xs)) + 1):
            inside = False
            j = len(pts) - 1
            for i, (px, py) in enumerate(pts):
                qx, qy = pts[j]
                if (py > y) != (qy > y) and x < (qx - px) * (y - py) / (qy - py) + px:
                    inside = not inside
                j = i
            if inside:
                mask.set(x, y)
    if r > 0:
        for i in range(len(pts)):
            stroke(mask, pts[i], pts[(i + 1) % len(pts)], r)
    return mask


def vane_pts(root, length, angle, spread, taper=0.6):
    """The three corners of a vane. Geometry only — the form itself is `vane` below.

    Split out because the two halves of a wing go to different planes: the membrane is a
    BODY form, so only the union's outline survives and the wing shares a silhouette with
    whatever it grows from, while the ribs across it are `ink` and are drawn exactly as
    laid down. A recipe therefore needs these corners twice, and computing them twice by
    hand is how the ribs end up crossing a membrane that has since moved.
    """
    tip = (root[0] + math.cos(angle) * length,
           root[1] + math.sin(angle) * length)
    heel = (root[0] + math.cos(angle + spread) * length * taper,
            root[1] + math.sin(angle + spread) * length * taper)
    return [root, tip, heel]


def vane(mask, root, length, angle, spread, taper=0.6):
    """A membrane wing or fin: a flat triangular vane hinged at a point on the body.

    The one form the rest of this vocabulary cannot reach. `disc`, `tube` and
    `superellipse` are all round, and roundness is precisely what a wing is not — a wing
    is a flat thing stretched between a straight leading edge and a slack trailing one,
    and the rounded approximation of it reads as a flipper. `poly` can of course draw the
    triangle, but then the wing is four hand-placed corners per frame instead of an
    angle, which is not a form the line can say twice.

    It takes no phase and does no flapping. A frame's worth of flap is `angle` swung and
    `length` foreshortened, and those are two numbers the RECIPE already varies per frame
    exactly the way it varies a spine's control points. Baking a beat in here would put
    animation into the vocabulary, which no other form has ever carried.
    """
    return poly(mask, vane_pts(root, length, angle, spread, taper))


def stroke(mask, p0, p1, r=0.5):
    """A thick line segment, as a run of stamped discs."""
    dx, dy = p1[0] - p0[0], p1[1] - p0[1]
    n = max(2, int(math.hypot(dx, dy) * 3) + 1)
    for i in range(n + 1):
        t = i / n
        disc(mask, p0[0] + dx * t, p0[1] + dy * t, r)
    return mask


def line(mask, p0, p1):
    """A 1px line. Bresenham, so a chord or a lip is exactly one pixel thick."""
    x0, y0 = int(round(p0[0])), int(round(p0[1]))
    x1, y1 = int(round(p1[0])), int(round(p1[1]))
    dx, dy = abs(x1 - x0), -abs(y1 - y0)
    sx = 1 if x0 < x1 else -1
    sy = 1 if y0 < y1 else -1
    err = dx + dy
    while True:
        mask.set(x0, y0)
        if x0 == x1 and y0 == y1:
            return mask
        e2 = 2 * err
        if e2 >= dy:
            err += dy
            x0 += sx
        if e2 <= dx:
            err += dx
            y0 += sy


# ---------------------------------------------------------------------------
#  Paths — the spine a body is grown along
# ---------------------------------------------------------------------------
def bezier(p0, c0, c1, p1):
    """A cubic Bézier as a callable t -> (x, y).

    The two control points are the whole animation vocabulary for a crawling body:
    swinging them in OPPOSITE directions per frame puts a travelling S-wave down the
    spine, where swinging them together just rotates the creature rigidly. That
    distinction is the difference between a worm that moves and a worm that waves.
    """
    def f(t):
        u = 1 - t
        a, b, c, d = u * u * u, 3 * u * u * t, 3 * u * t * t, t * t * t
        return (a * p0[0] + b * c0[0] + c * c1[0] + d * p1[0],
                a * p0[1] + b * c0[1] + c * c1[1] + d * p1[1])
    return f


def catmull(pts, alpha=0.5):
    """A smooth path through EVERY point in `pts`, as a callable t -> (x, y).

    `bezier` steers a body with two control points the curve only leans toward, which is
    the right handle for a spine with ONE bend in it and no handle at all for a spine
    with three: a single cubic cannot double back on itself, so a coiled body has to be
    said point by point. The signature is deliberately identical, so `tube`, `tangent`
    and `Cell.chords` neither know nor care which kind of path they were handed.

    CENTRIPETAL (alpha=0.5) rather than uniform — knots spaced by the square root of
    chord length. A uniform Catmull-Rom through unevenly spaced points overshoots into a
    cusp or a small loop wherever the spacing changes sharply, and a cusp inside a body
    that is being stamped with discs surfaces as a bulge in the outline rather than as an
    obvious error, so it would be found by eye and blamed on the drawing. Centripetal is
    the parameterisation that provably cannot produce one.

    `t` runs 0..1 over the SEGMENTS rather than over arc length, so a widely spaced pair
    of points is travelled faster than a close one. Chord parameters get picked by eye
    against the drawing anyway, and the alternative is an arc-length table nothing else
    here would use.
    """
    p = list(pts)
    if len(p) < 2:
        raise ValueError("a path needs at least two points")
    # A reflected point off each end, so the curve has a tangent AT its first and last
    # point instead of starting one segment short of them.
    p = ([(2 * p[0][0] - p[1][0], 2 * p[0][1] - p[1][1])] + p
         + [(2 * p[-1][0] - p[-2][0], 2 * p[-1][1] - p[-2][1])])
    segs = len(p) - 3

    def knots(q):
        ks = [0.0]
        for i in range(3):
            d = math.hypot(q[i + 1][0] - q[i][0], q[i + 1][1] - q[i][1]) ** alpha
            ks.append(ks[-1] + max(d, 1e-6))   # coincident points would divide by zero
        return ks

    def f(t):
        u = min(max(t, 0.0), 1.0) * segs
        i = min(int(u), segs - 1)
        q = p[i:i + 4]
        t0, t1, t2, t3 = knots(q)
        tt = t1 + (t2 - t1) * (u - i)

        def mix(a, b, ta, tb):
            w = (tb - tt) / (tb - ta)
            return (w * a[0] + (1 - w) * b[0], w * a[1] + (1 - w) * b[1])

        a1, a2, a3 = mix(q[0], q[1], t0, t1), mix(q[1], q[2], t1, t2), \
            mix(q[2], q[3], t2, t3)
        return mix(mix(a1, a2, t0, t2), mix(a2, a3, t1, t3), t1, t2)
    return f


def sample(path, n):
    """`n` evenly-parameterised points off a path callable — the bridge back to a list.

    Paths and point lists are the two ways a spine gets said here, and each can do one
    thing the other cannot: a path is smooth and can be walked at any t, a list can be
    bent point by point by `wave`. Sampling one into the other is what lets a body carry
    a big sweep AND a ripple riding on it, which no single curve of either kind draws.
    """
    return [path(i / (n - 1.0)) for i in range(n)]


def wave(pts, amp, phase, period=3.2):
    """Push every INTERIOR point of a polyline off its own normal, in a travelling wave.

    The polyline counterpart of swinging a Bézier's two control points in quadrature: a
    bulge that walks from one end of the body to the other as `phase` advances, rather
    than a lean that visits two shapes and goes back. Ends are held exactly, because on a
    resting creature it is the SPINE that moves and the head that holds station — a head
    that travels with the wave has turned an undulation into a bob.
    """
    out = [pts[0]]
    for i in range(1, len(pts) - 1):
        (ax, ay), (bx, by) = pts[i - 1], pts[i + 1]
        d = math.hypot(bx - ax, by - ay) or 1.0
        nx, ny = -(by - ay) / d, (bx - ax) / d
        k = amp * math.sin(2 * math.pi * i / period - phase)
        out.append((pts[i][0] + nx * k, pts[i][1] + ny * k))
    out.append(pts[-1])
    return out


def tangent(path, t, eps=1e-3):
    """The unit direction the path is heading at t — where a head faces."""
    t0, t1 = max(0.0, t - eps), min(1.0, t + eps)
    (x0, y0), (x1, y1) = path(t0), path(t1)
    d = math.hypot(x1 - x0, y1 - y0) or 1.0
    return ((x1 - x0) / d, (y1 - y0) / d)


# ---------------------------------------------------------------------------
#  Cell — one frame under construction, and the finishing pass that IS the style
# ---------------------------------------------------------------------------
class Cell:
    """One sprite frame.

    Three planes, and the split between them is the style rather than an implementation
    convenience:

      body — filled forms. NEVER drawn as filled; only `outline()` of the union shows.
      ink  — strokes drawn exactly as laid down: chords, lips, teeth, the ground plant.
      cut  — outline pixels to remove. What opens a mouth in a head that is a closed
             ring: a change of SHAPE rather than a nick, which is the difference
             between a jaw and a damaged sprite.

    `solid()` is the one filled mass (rule 4) and refuses a second, because "the eye is
    the only solid thing" stops being true the first time a recipe adds one more.
    """

    def __init__(self, w, h):
        self.w, self.h = w, h
        self.body = Mask(w, h)
        self.ink = Mask(w, h)
        self.cut = Mask(w, h)
        self._solid = None

    def solid(self, x, y, w=2, h=2):
        """The single solid mass. Rule 4 — one per cell, and the read lands on it."""
        if self._solid is not None:
            raise ValueError("a worm-line cell gets exactly one solid mass (the eye)")
        self._solid = (x, y, w, h)
        rect(self.ink, x, y, x + w - 1, y + h - 1)
        return self

    def eye(self, center, facing, r, size=2, along=0.30, across=0.30):
        """Seat the solid mass inside a head, guaranteed clear of its outline.

        Placed in the head's OWN frame: `along` toward where it is facing, `across`
        perpendicular to that (positive is the head's upper side). Both are fractions
        of the head radius. Above-and-forward is where an eye goes on a creature with
        a mouth at the front — offsetting along the facing axis alone walks it either
        into the jaw or back down the neck, and at ten pixels across there is no room
        to be wrong about which.

        Then pulled back toward the centre until every pixel is strictly interior. An
        eye touching the head's own outline reads as a dent in the silhouette rather
        than as a feature, which is the one failure this seating exists to prevent.
        """
        head = disc(Mask(self.w, self.h), center[0], center[1], r)
        safe = head.inset(2)
        px, py = facing[1], -facing[0]          # the head's "up", whichever way it points
        for pull in [1.0, 0.8, 0.6, 0.4, 0.2, 0.0]:
            ox = center[0] + (facing[0] * along + px * across) * r * pull
            oy = center[1] + (facing[1] * along + py * across) * r * pull
            x = int(round(ox - (size - 1) / 2.0))
            y = int(round(oy - (size - 1) / 2.0))
            if all(safe.get(x + i, y + j) for i in range(size) for j in range(size)):
                return self.solid(x, y, size, size)
        raise ValueError("no interior seat for the eye — the head is too small")

    def chords(self, path, ts, radius_at, overhang=0.9):
        """Rule 3: perpendicular rungs across the body at fixed spine parameters.

        Drawn a touch PAST the body wall on both sides (`overhang`) so each rung meets
        the outline rather than stopping a pixel short of it — a chord that floats free
        inside the body reads as noise, and one that lands on the wall reads as a
        segment boundary.
        """
        for t in ts:
            cx, cy = path(t)
            tx, ty = tangent(path, t)
            nx, ny = -ty, tx
            r = radius_at(t) + overhang
            line(self.ink, (cx - nx * r, cy - ny * r), (cx + nx * r, cy + ny * r))
        return self

    def gape(self, center, r, facing, amount, teeth=1, spread=1.0):
        """Open a jaw in a head that is otherwise a closed ring.

        A wedge is CUT from the ring and the two cut edges are then drawn back in as
        radial lines converging on a hinge just off the head's centre — so the outline
        still CLOSES around the opening. That is the whole trick: an outline with a
        piece missing reads as a damaged sprite, and the same outline routed around a
        notch reads as a mouth. Lips pointing outward from the cut do not achieve it;
        at ten pixels across, the eye needs the shape to be enclosed to name it.

        `amount` is 0 (shut) to 1 (widest). `spread` scales how far round the head the
        jaw reaches, so a mouth can be the creature's whole front rather than a notch
        in it. `teeth` is ticks PER JAW, set perpendicular to the jaw line and pointing
        into the opening: one is a fang, four is a maw. They are drawn on the jaw edges
        rather than floating in the gap because a tooth that does not meet the jaw it
        grows from reads as grit in the mouth.
        """
        if amount <= 0:
            return self
        a0 = math.atan2(facing[1], facing[0])
        half = math.radians((14 + 30 * amount) * spread)

        # Cut the ring between the jaws. The band is generous on either side of r so a
        # ring drawn 2px thick where it meets the neck is cleared through.
        for y in range(int(center[1] - r) - 2, int(center[1] + r) + 3):
            for x in range(int(center[0] - r) - 2, int(center[0] + r) + 3):
                dx, dy = x - center[0], y - center[1]
                if abs(math.hypot(dx, dy) - r) > 2.0:
                    continue
                da = abs((math.atan2(dy, dx) - a0 + math.pi) % (2 * math.pi) - math.pi)
                if da < half:
                    self.cut.set(x, y)

        hinge = (center[0] + math.cos(a0) * r * 0.2,
                 center[1] + math.sin(a0) * r * 0.2)
        for sign in (-1, 1):
            a = a0 + sign * half
            rim = (center[0] + math.cos(a) * r, center[1] + math.sin(a) * r)
            line(self.ink, hinge, rim)
            # Into the gape is the jaw's perpendicular rotated toward the mouth axis,
            # which flips with the jaw — so the two rows of teeth face each other
            # instead of both combing the same way.
            ix, iy = sign * math.sin(a), -sign * math.cos(a)
            depth = 1 + int(round(1.4 * amount)) if teeth > 1 else 1
            for k in range(teeth):
                t = (k + 1) / (teeth + 1)
                bx = hinge[0] + (rim[0] - hinge[0]) * t
                by = hinge[1] + (rim[1] - hinge[1]) * t
                line(self.ink, (bx, by), (bx + ix * depth, by + iy * depth))
        return self

    def maw(self, center, r, teeth=7, depth=2.6, phase=0.0, stagger=0.45,
            facing=None, arc=2 * math.pi):
        """A mouth seen down its own AXIS: teeth stepping inward off the head's own rim.

        `gape` above is a mouth in profile — a jaw that hinges. This is the other one, a
        ring of teeth around an opening you are looking into, and the two are not
        interchangeable: at this size a profile jaw on a creature whose whole front is
        its mouth reads as a chipped edge, and an axial maw on a creature with a face
        reads as a wound. Which one a recipe reaches for is the single biggest thing it
        says about what the creature is.

        No rim is drawn — the head's own outline already is one. The teeth are radial
        spokes stepping IN from it, so the mouth is a hole bounded by the silhouette
        rather than a shape floating inside it, and `phase` rotates them, which is a
        chew when it moves frame to frame.

        `stagger` shortens every other tooth. Spokes of equal length around a circle
        read as a FLOWER, and no amount of menace elsewhere in the drawing argues the
        viewer out of it; alternating long and short is what makes the same spokes read
        as fangs. It is the one parameter here that is doing work no geometry demands —
        and it is why `teeth` wants to be EVEN, so the alternation closes instead of
        landing two long teeth side by side at the wrap.

        `arc` (with `facing`) keeps teeth off the part of the rim that is BURIED in the
        body. Where a head sits deep on a thick neck, part of its circle is interior to
        the merged silhouette and has no rim at all — teeth there root on nothing and
        float inside the creature, reading as swallowed debris. Which arc is exposed is
        the recipe's business, since only it knows how deep the head is set.
        """
        for k in range(teeth):
            a = phase + 2 * math.pi * k / teeth
            if facing is not None:
                off = abs((a - math.atan2(facing[1], facing[0]) + math.pi)
                          % (2 * math.pi) - math.pi)
                if off > arc / 2:
                    continue
            d = depth * (1.0 - stagger * (k % 2))
            # Rooted just INSIDE the nominal radius. Flush with it leaves some teeth
            # a pixel clear of the rasterised rim, reading as debris in the mouth;
            # outside it they break the silhouette and the maw reads as spiky rather
            # than as toothed, which is a different creature entirely.
            line(self.ink,
                 (center[0] + math.cos(a) * (r - 0.9),
                  center[1] + math.sin(a) * (r - 0.9)),
                 (center[0] + math.cos(a) * (r - 0.9 - d),
                  center[1] + math.sin(a) * (r - 0.9 - d)))
        return self

    def bake(self):
        """body -> outline, less the cut, plus the ink. The whole finishing pass."""
        return self.body.outline().subtract(self.cut).union(self.ink)


# ---------------------------------------------------------------------------
#  Sheet — cells laid out in rows, and the PNG they become
# ---------------------------------------------------------------------------
class Sheet:
    """A grid of frame cells. Row = animation clip, column = frame within it.

    A short row simply leaves its tail cells transparent: the clip on the creature's
    content row says how many frames it actually plays (CreatureDef::clips), so an
    unfilled cell costs its 1bpp share of the sheet and nothing else.
    """

    def __init__(self, cols, rows, cw, ch):
        self.cols, self.rows, self.cw, self.ch = cols, rows, cw, ch
        self.mask = Mask(cols * cw, rows * ch)

    def place(self, col, row, cell):
        m = cell.bake() if isinstance(cell, Cell) else cell
        ox, oy = col * self.cw, row * self.ch
        for y in range(self.ch):
            for x in range(self.cw):
                if m.get(x, y):
                    self.mask.set(ox + x, oy + y)
        return self

    def png(self):
        w, h = self.mask.w, self.mask.h
        raw = bytearray()
        for y in range(h):
            raw.append(0)  # filter: none
            for x in range(w):
                raw += bytes(INK if self.mask.get(x, y) else (0, 0, 0, 0))

        def chunk(tag, data):
            return (struct.pack(">I", len(data)) + tag + data
                    + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF))

        return (b"\x89PNG\r\n\x1a\n"
                + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))
                + chunk(b"IDAT", zlib.compress(bytes(raw), 9))
                + chunk(b"IEND", b""))


# ===========================================================================
#  RECIPES — one function per sheet. Everything above is the vocabulary; from
#  here down is creature design, and a new worm-line creature is a new function.
# ===========================================================================
CW, CH = 56, 48       # the pet cell (gen_assets.py PET_FRAME_W / PET_ROW_H)
GROUND = 46.0         # the shelf line inside the cell; a crawler never leaves it


def _nodeatode_cell(head, c0, c1, mouth=0.0):
    """One Nodeatode frame: a planted tail, a tapered spine, a hollow head, one eye.

    The worm occupies roughly 30x24 of its 56x48 cell — the line's draw-small rule
    (assets/CREATURE_VISUAL_RULES.md §4) kept literally, because the replicas beside it
    need the room. All four poses share this body; only the spine and the jaw move.
    """
    tail = (19.0, GROUND)
    # Thick enough that the outline's two walls stay a clear 3px apart along the whole
    # body. A thinner tube collapses into a single line as it tapers, and a worm drawn
    # as one line is a piece of string — the gap between the walls IS the body.
    r_tail, r_neck = 2.6, 4.6
    # The head SWELLS as the jaw opens. Not decoration: a shut head is 10px across, and
    # ten pixels will not hold a mouth and an eye at once without the two becoming one
    # smudge. Growing it buys the room the open pose needs, and a head that fills as it
    # lunges is what the motion looks like anyway.
    head_r = 5.2 + 1.7 * mouth
    cell = Cell(CW, CH)
    spine = bezier(tail, c0, c1, head)

    tube(cell.body, spine, r_tail, r_neck)
    disc(cell.body, head[0], head[1], head_r)

    # Four rungs. This is the pass that makes it a worm rather than a tube.
    cell.chords(spine, (0.26, 0.44, 0.62, 0.79),
                lambda t: r_tail + (r_neck - r_tail) * t, overhang=0.35)

    facing = tangent(spine, 1.0)
    cell.gape(head, head_r, facing, mouth)
    # Forward and up when the mouth is shut; as the jaw opens the eye gives ground on
    # the facing axis and climbs, so the two features never contend for the same pixels.
    cell.eye(head, facing, head_r,
             along=0.30 - 0.34 * mouth, across=0.30 + 0.22 * mouth)

    # The ground contact, drawn identically in every frame. A crawler that plants
    # harder on some frames than others reads as a flicker, not as a footfall — and a
    # bar this short is the only mark in the cell that says which way is down.
    line(cell.ink, (tail[0] - 2, GROUND + 1), (tail[0] + 3, GROUND + 1))
    return cell


def nodeatode():
    """SPR_PET_NODEATODE — the Worm line's Process pet.

    Four rows of four 56x48 frames, matching the clips declared on its content row in
    src/core/content/creatures/worm/line.h:

      0  idle    4 frames — a travelling S-wave; the head holds station and the SPINE
                            moves, because a crawler that bobs has left the floor.
      1  attack  4 frames — coil, open, strike, snap. The head carries ~15px forward.
      2  droop   2 frames — head down, spine slack. The unhappy pose.
      3  weak    2 frames — collapsed onto the shelf, barely a curve left in it.
    """
    sheet = Sheet(4, 4, CW, CH)

    # Idle. The head holds station and the two control points move in QUADRATURE — c0
    # on the sine, c1 on the cosine — so the bulge travels from the head end down to
    # the tail instead of the body leaning as a unit. Driving them in exact opposition
    # instead is the obvious thing and is wrong twice over: it is a standing wave, not
    # a travelling one, and over four frames it only visits two distinct shapes.
    head = (35.0, 25.0)
    amp = 3.0
    for i in range(4):
        a = 2 * math.pi * i / 4
        c0 = (21.0 + amp * math.sin(a), 37.0)
        c1 = (30.0 - amp * math.cos(a), 29.5)
        sheet.place(i, 0, _nodeatode_cell(head, c0, c1))

    # Attack. Coiled back and tense, then thrown forward; the jaw opens on the way out
    # and snaps on contact, so the widest gape is mid-flight rather than at the end.
    for i, (hd, c0, c1, mouth) in enumerate([
        ((29.0, 21.0), (22.5, 38.0), (26.0, 26.0), 0.0),
        ((32.0, 23.0), (22.0, 37.0), (28.5, 27.5), 0.55),
        ((38.5, 28.0), (21.0, 36.5), (32.0, 29.5), 1.0),
        ((42.0, 31.0), (20.5, 37.5), (35.0, 32.5), 0.45),
    ]):
        sheet.place(i, 1, _nodeatode_cell(hd, c0, c1, mouth))

    # Droop. The head comes down and forward and the spine goes slack — the same body
    # with the tension taken out of it, which is what makes it read as the same animal
    # in a worse mood rather than as a second creature.
    for i, (hd, c0, c1) in enumerate([
        ((34.0, 35.0), (22.0, 41.0), (29.0, 37.5)),
        ((34.5, 36.5), (22.5, 41.5), (29.5, 39.0)),
    ]):
        sheet.place(i, 2, _nodeatode_cell(hd, c0, c1))

    # Weak. Almost nothing left standing: the head is on the shelf and the curve has
    # gone out of the body. Two frames apart by a single pixel — a shallow breath.
    for i, (hd, c0, c1) in enumerate([
        ((30.0, 41.0), (22.0, 44.0), (26.0, 42.0)),
        ((30.0, 42.0), (22.0, 44.5), (26.0, 43.0)),
    ]):
        sheet.place(i, 3, _nodeatode_cell(hd, c0, c1))

    return sheet


def _rootgrub_cell(head, c0, c1, mouth, teeth=8, phase=0.0):
    """One Rootgrub frame: a thick short body reared off the shelf under a big maw.

    The Script row, and the same vocabulary saying the opposite thing to Nodeatode's.
    Where the Process worm is thin and long and led by a head, this is SHORT and FAT
    and led by a mouth.

    WIDTH is how it reads as a whole stage more than what it came from. It has barely
    more spine than Nodeatode and no more cell, so length and scale are both spent —
    the only axis left is girth, and the taper runs 6.2 to 8.4 where the Process row
    runs 2.6 to 4.6. Roughly double the body on the same footprint, which is also why
    the line's draw-small rule survives the promotion.

    FEWER segments, for the same reason. Rungs close together subdivide a body and make
    it read as articulated and therefore light; three widely spaced ones leave big
    unbroken panels of flank between them, and the panels are what carry the mass.
    Nodeatode has four rungs on a thin body; this has three on a fat one.

    Its single solid mass is the THROAT, not an eye (see `Cell.solid`). A sandworm's
    whole face is the hole, and a creature whose one filled shape sits at the back of
    an open mouth reads as something that eats — which is the branch this row is on.
    """
    tail = (24.0, GROUND + 1)
    # The taper RUNS OUT before the head, so the maw sits on the body as a distinct
    # bulb. Nodeatode's head is barely wider than its neck and reads as a continuation;
    # here the neck has to give ground for the mouth to be a thing the body carries.
    r_tail, r_neck = 6.2, 8.4
    head_r = 10.4 + 1.5 * mouth
    cell = Cell(CW, CH)
    spine = bezier(tail, c0, c1, head)

    tube(cell.body, spine, r_tail, r_neck)
    disc(cell.body, head[0], head[1], head_r)

    cell.chords(spine, (0.22, 0.46, 0.70),
                lambda t: r_tail + (r_neck - r_tail) * t, overhang=0.3)

    facing = tangent(spine, 1.0)
    # Seen down its own axis, because on this creature the mouth IS the front. Teeth
    # are sized off the head rather than in absolute px, so the maw stays in proportion
    # as it dilates instead of the fangs shrinking into a wider ring. The head also
    # sits DEEP on this neck — a fat body swallows most of a circle — so the ring
    # stops short of where the body enters it.
    cell.maw(head, head_r, teeth=teeth, depth=head_r * (0.24 + 0.16 * mouth),
             phase=phase, facing=facing, arc=math.radians(280))
    # The one solid mass is the THROAT, where every other row of the line spends it on
    # an eye. A creature whose single filled shape sits at the back of an open mouth
    # reads as something that eats, which is the branch this row is on.
    throat = 4 + (1 if mouth > 0.7 else 0)
    cell.solid(int(round(head[0] - (throat - 1) / 2.0)),
               int(round(head[1] - (throat - 1) / 2.0)), throat, throat)

    line(cell.ink, (tail[0] - 5, GROUND + 1), (tail[0] + 6, GROUND + 1))
    return cell


def rootgrub():
    """SPR_PET_ROOTGRUB — the Worm line's Script pet, and its fork in the road.

    Four rows of four 56x48 frames, same clip set as the Process row above:

      0  idle    4 frames — reared off the shelf, maw working. It never fully shuts:
                            the mouth is the silhouette, so closing it would cost the
                            creature its read on the one screen it is seen on most.
      1  attack  4 frames — rear back, gape wide, drive down, close.
      2  droop   2 frames — settled onto the shelf, maw slack.
      3  weak    2 frames — collapsed, barely reared at all.
    """
    sheet = Sheet(4, 4, CW, CH)

    # Idle. A short body has little spine to run a wave down, so the motion is in the
    # MAW instead — it opens and closes on the loop while the body sways a pixel or two
    # under it. Same rule as Nodeatode's: whatever moves, the shelf contact does not.
    for i in range(4):
        a = 2 * math.pi * i / 4
        head = (34.0 + 1.2 * math.sin(a), 25.0 - 0.8 * math.cos(a))
        c0 = (24.0, 40.0)
        c1 = (28.0 + 1.4 * math.sin(a), 31.0)
        sheet.place(i, 0, _rootgrub_cell(head, c0, c1, 0.42 + 0.20 * math.sin(a),
                                         phase=a * 0.25))

    # Attack. It does not lunge the way the Process row does — it rears and comes DOWN,
    # which is what a mouth that size is for.
    for i, (hd, c0, c1, mouth) in enumerate([
        ((33.0, 22.0), (24.0, 40.0), (27.0, 28.0), 0.30),
        ((34.0, 20.0), (24.0, 39.0), (28.0, 26.0), 1.00),
        ((38.0, 26.0), (24.5, 40.0), (32.0, 30.0), 1.00),
        ((39.0, 30.0), (25.0, 41.0), (34.0, 34.0), 0.35),
    ]):
        sheet.place(i, 1, _rootgrub_cell(hd, c0, c1, mouth, phase=i * 0.22))

    # Droop. Down off its rear, maw hanging half open with nothing behind it.
    for i, (hd, c0, c1) in enumerate([
        ((39.0, 33.0), (25.0, 44.0), (32.0, 37.0)),
        ((39.5, 34.5), (25.0, 44.5), (32.5, 38.5)),
    ]):
        sheet.place(i, 2, _rootgrub_cell(hd, c0, c1, 0.30, teeth=8))

    # Weak. Flat on the shelf. The maw is nearly shut, which for this creature is the
    # strongest thing the sheet can say about how badly it is doing.
    for i, (hd, c0, c1) in enumerate([
        ((40.0, 38.0), (26.0, 46.0), (33.0, 41.0)),
        ((40.0, 39.0), (26.0, 46.5), (33.0, 42.0)),
    ]):
        sheet.place(i, 3, _rootgrub_cell(hd, c0, c1, 0.14, teeth=6))

    return sheet


def _shenloop_cell(pts, whisker=1.0, level=0.75):
    """One Shenloop frame: a long clawless serpent coiled in on itself, off the floor.

    The good Daemon, and the row where nearly every habit of the line so far gets
    dropped. There is NO taper — `tube` is handed the same radius twice, because a
    serpent is near-constant from end to end and the taper that made Nodeatode read as
    led by its head is exactly what a snake does not have. There is no head BULB either,
    only enough swell to seat an eye in; on this creature the head is where the body
    stops, not a mass the body carries.

    And there is no mouth, of either kind. The maw belongs to the branch that eats, and
    lending it here would collapse the fork the two Daemons exist to be — a clawless
    serpent that also has a maw is just Threadbore drawn thin. What it has instead is
    LENGTH, doubled back on itself: the same footprint as the Script row holding three
    times the body, which is where its stage read comes from now that width has been
    spent by the other branch.

    Its one solid mass is an eye again, per `Cell.solid` — Rootgrub's throat was the
    exception, and this row is why it had to be one. A creature with no talons that holds
    a connection open and waits at the far end of it is a thing that LOOKS.
    """
    # Constant, and thick enough for the outline's two walls to stay a clear 3px apart
    # around the ribbon's tightest bend. The head is the SMALLEST in the line in absolute
    # terms — under Nodeatode's — and it is a swell rather than a bulb: the body does not
    # taper into it, it simply widens far enough to seat an eye and a pair of barbels and
    # stops. Any more and the creature is led by its head, which is the Process row's read
    # and not this one's.
    r, head_r = 3.1, 4.8
    cell = Cell(CW, CH)
    spine = catmull(pts)

    # More steps than a Bézier body needs. This spine is roughly three times as long, and
    # the stamps have to overlap on the OUTSIDE of the sharpest bend or the outline picks
    # up notches there — which is the only place on this drawing they could appear.
    tube(cell.body, spine, r, r, steps=320)
    head = pts[-1]
    disc(cell.body, head[0], head[1], head_r)

    # Seven rungs where Rootgrub has three, and that inversion is the whole argument
    # between the two Daemons. Rungs close together subdivide a body and make it read as
    # articulated and therefore LIGHT; this creature wants to be read as light, so it
    # takes the segmentation the fat branch gave up.
    cell.chords(spine, (0.08, 0.20, 0.32, 0.44, 0.56, 0.67, 0.78),
                lambda t: r, overhang=0.35)

    # The head is LEVELLED off the neck instead of pointing wherever the spine happens to
    # arrive, and that one rotation is most of the creature's stage read. §2 of
    # CREATURE_VISUAL_RULES is explicit that the levers are posture rather than parts —
    # a head carried along its own neck reads as a body going somewhere, and the same
    # head held level on a rising neck reads as one that arrived and is now waiting,
    # which is exactly what the row's flavour says it does. Fins down the back would have
    # bought the dragon read too, and would have been the parts-list §1 bans.
    tx, ty = tangent(spine, 1.0)
    a = math.atan2(ty, tx) * (1.0 - level)
    facing = (math.cos(a), math.sin(a))
    cell.eye(head, facing, head_r, along=0.34, across=0.28)

    # Barbels — the eastern-dragon tell, and the reason this row does not need horns, a
    # mane or a crest to be read as one. Two `line` runs each rather than a form: nothing
    # else in the line has whiskers, and a primitive that one creature uses is a
    # primitive that has not earned its name yet.
    if whisker > 0:
        a0 = math.atan2(facing[1], facing[0])
        snout = (head[0] + facing[0] * head_r * 0.9,
                 head[1] + facing[1] * head_r * 0.9)
        for side in (-1, 1):
            # ONE straight run each, flaring off the snout. A curled barbel was the first
            # thing tried and it reads as an antenna: the curl needs a second segment,
            # and the second segment is long enough to be a limb on a creature that must
            # not appear to have any. Straight and divergent is what a viewer names as
            # whiskers at this size, and it is also all the drawing can afford.
            a = a0 + side * 0.75
            line(cell.ink, snout,
                 (snout[0] + math.cos(a) * 5.5 * whisker,
                  snout[1] + math.sin(a) * 5.5 * whisker))

    # No ground plant, and nothing near the bottom of the cell. Every crawling row of the
    # line ends with a bar on the shelf; this one is a swimmer (Locomotion::Swim, on its
    # content row) and the habitat drifts it on both axes, so a contact mark would be a
    # floor it is demonstrably not on.
    return cell


def shenloop():
    """SPR_PET_SHENLOOP — the Worm line's good Daemon, and the branch that grew UP.

    Four rows of four 56x48 frames, the same clip set as the two rows below it:

      0  idle    4 frames — a wave travelling the length of the coil, head holding
                            station. The one motion a body this long can make for free.
      1  attack  4 frames — the coil GATHERS and then unwinds, throwing the head out.
                            It has no mouth to strike with, so the body is the strike.
      2  droop   2 frames — the coil sags open and the head comes down.
      3  weak    2 frames — nearly uncoiled, sunk low, holding almost no shape.

    Every pose in all four rows is the same three numbers — where the tail is, where the
    head is, and how much wave is in between — so there are no hand-placed spines here at
    all. That is not tidiness: a body of ten points hand-placed four times is a body that
    stops being the same animal by the fourth frame, and this creature's whole read is
    that there is a great deal of ONE continuous thing in the cell.
    """
    sheet = Sheet(4, 4, CW, CH)

    def ribbon(tail, head, amp, phase):
        """A serpent between two points, with `amp` of travelling wave in between.

        The body FOLDS rather than crossing over itself, and the difference is not
        stylistic: two lengths of a 3px-wide outlined body laid across each other merge
        into a knot at this scale, and the read goes from "long" to "tangled" with no
        way back. A wave doubles back three times and never touches itself, which buys
        the same length for none of the confusion.
        """
        axis = [(tail[0] + (head[0] - tail[0]) * i / 9.0,
                 tail[1] + (head[1] - tail[1]) * i / 9.0) for i in range(10)]
        return wave(axis, amp, phase, period=6.0)

    # Idle. Reared: the axis climbs three quarters of the cell's height, so the body is a
    # column with a fold in it rather than a line crossing the floor diagonally. That is
    # where the stage read lives — a serpent laid out flat is going somewhere and is a
    # long worm while it does, and the same body stood up and holding still is a creature
    # that has arrived and is waiting, which is what this row's flavour text says it does.
    #
    # The wave travels and the ENDS hold station, per `wave`'s contract, so what moves is
    # the length of the body while the head keeps looking at one thing. For a creature
    # with no limbs and no ground contact that is not one idle option among several — it
    # is the only motion available at all.
    for i in range(4):
        sheet.place(i, 0, _shenloop_cell(
            ribbon((9.0, 40.0), (43.0, 10.0), 6.8, 2 * math.pi * i / 4)))

    # Attack. A serpent strikes by SPENDING its wave: it gathers by pulling its reach in
    # and driving the amplitude up, then arrives by flattening the same body along a
    # longer axis. Nothing is added to the drawing to make it lunge — the length that was
    # folded up is the length that reaches, which is the one thing about a snake worth
    # animating and is three numbers here.
    #
    # `level` is the other half of it. The head is carried level at rest and gives that up
    # on the way out, so the strike is the one moment the creature commits to a direction
    # — and the recovery frame takes the level back, which is what makes the whole clip
    # read as returning to the pose above rather than ending somewhere new.
    for i, (tl, hd, amp, lv, wk) in enumerate([
        ((12.0, 40.0), (38.0, 16.0), 7.4, 1.00, 1.0),
        ((13.0, 41.0), (35.0, 19.0), 8.0, 1.00, 1.0),
        ((8.0, 39.0), (44.0, 18.0), 3.2, 0.35, 0.8),
        ((8.0, 38.0), (45.0, 21.0), 2.2, 0.55, 0.6),
    ]):
        sheet.place(i, 1, _shenloop_cell(ribbon(tl, hd, amp, 0.6 * i),
                                         whisker=wk, level=lv))

    # Droop. The rear comes down and the wave goes soft: the same column with the height
    # taken out of it, which on a creature whose posture IS its stage is the cheapest
    # possible way to say it is not holding itself well.
    for i in range(2):
        sheet.place(i, 2, _shenloop_cell(
            ribbon((10.0, 41.0), (38.0, 23.0), 4.6 - 0.4 * i, 0.9 + i * math.pi),
            level=0.85))

    # Weak. Barely reared and barely waved, head sagging off the level. For a creature
    # whose whole read is length held up against nothing, having none of it left is the
    # strongest thing this sheet can say about how badly it is doing.
    for i in range(2):
        sheet.place(i, 3, _shenloop_cell(
            ribbon((11.0, 42.0), (36.0, 31.0), 2.6 - 0.3 * i, 0.4 + i * 0.8),
            whisker=0.55, level=0.45))

    return sheet


def _threadbore_cell(head, c0, c1, mouth, flap, teeth=8, phase=0.0):
    """One Threadbore frame: Rootgrub with everything that was not mouth spent on mouth.

    The bad Daemon, and the only row here that is a straight continuation rather than an
    argument — `_rootgrub_cell` with its numbers pushed until they stop being sensible,
    which is the joke the creature is. The taper runs 7.6 to 10.2 where the Script row
    runs 6.2 to 8.4, on a spine SHORTER than that row's, so the body ends up wider than
    it is long and the flavour text on its content row is a literal description.

    TWO chords where Rootgrub has three and Nodeatode four. That progression is the
    line's whole statement about mass: rungs close together subdivide a body and make it
    read as articulated and therefore light, so each stage of the fat branch removes one
    and lets the unbroken panel of flank between them carry more. Two is the floor —
    at one there is no segmentation left and rule 3 stops being satisfied at all.

    It keeps the THROAT as its one solid mass, where Shenloop went back to an eye. That
    is the fork stated in the place it costs the most to state: both Daemons inherit
    exactly one thing from Rootgrub, and which one they inherit is the entire branch.
    """
    tail = (16.0, 34.0)
    r_tail, r_neck = 7.6, 10.2
    head_r = 12.0 + 1.6 * mouth
    cell = Cell(CW, CH)
    spine = bezier(tail, c0, c1, head)

    tube(cell.body, spine, r_tail, r_neck)
    disc(cell.body, head[0], head[1], head_r)

    # The wings. A wing is a LIMB carrying a membrane, and drawing it as one shape rooted
    # on the back was the version that failed: a triangle hinged flush to a body this fat
    # has most of itself buried inside the silhouette, and the sliver left above the back
    # reads as a fin or a crest. What fixes it is the bare arm — the membrane starts a
    # long way out from the body, so the gap between limb and flank is what says the thing
    # is a wing rather than something growing off it.
    #
    # The fan is where `vane` earns being general instead of being a wing function. Each
    # entry below is one finger panel: a vane from the wrist out to its own tip, with the
    # NEXT panel's tip as its heel, so consecutive panels share an edge and the union's
    # outer boundary steps from tip to tip. Falling radii make that boundary a scalloped
    # trailing edge, which is the thing a viewer actually names as a bat wing — and it is
    # the same primitive four times over rather than a hand-placed polygon per frame.
    fan = ((-0.20, 1.00), (-0.62, 0.88), (-1.05, 0.74), (-1.50, 0.58), (-1.95, 0.42))
    # Near wing, then the far one — shorter, held more upright and rooted closer to the
    # head so it clears the near wing's silhouette instead of tangling with it. A 1-bit
    # outline has no other way to say there are two of something behind each other, and
    # two identical wings simply collapse into one shape.
    for t, scale, tilt, armr in ((0.62, 1.00, 0.00, 1.0), (0.72, 0.70, -0.22, 0.9)):
        bx, by = spine(t)
        rr = r_tail + (r_neck - r_tail) * t
        shoulder = (bx, by - rr * 0.55)
        # `flap` is the one number the recipe varies per frame: the arm swings and the
        # whole wing foreshortens with it, which is what a wing seen side-on does. Neither
        # `vane` nor `stroke` knows a beat exists — see `vane`'s docstring.
        a = -math.pi / 2 - 0.45 - tilt + 0.38 * flap
        k = scale * (0.88 + 0.12 * math.cos(flap * 1.1))
        wrist = (shoulder[0] + math.cos(a) * 14.5 * k,
                 shoulder[1] + math.sin(a) * 14.5 * k)
        stroke(cell.body, shoulder, wrist, r=armr)
        tips = [(a + da, 15.5 * k * f) for da, f in fan]
        for (a0, l0), (a1, l1) in zip(tips, tips[1:]):
            vane(cell.body, wrist, l0, a0, a1 - a0, taper=l1 / l0)
        # The fingers, into `ink`. Without them the membrane is a blank shape and reads as
        # a sail; these are what say it is stretched over something, and they are the
        # wing's answer to the segment chords the body carries. Only the interior ones —
        # the outer two are already the membrane's own boundary.
        for a1, l1 in tips[1:-1]:
            line(cell.ink, wrist,
                 (wrist[0] + math.cos(a1) * l1, wrist[1] + math.sin(a1) * l1))

    cell.chords(spine, (0.26, 0.56),
                lambda t: r_tail + (r_neck - r_tail) * t, overhang=0.3)

    facing = tangent(spine, 1.0)
    # Deeper teeth than the Script row's, on a wider head. Rootgrub's maw already IS its
    # face, so "bigger" here cannot mean a bigger share of the silhouette — it has to be
    # reach into the hole, or the promotion is a scale-up and nothing else.
    cell.maw(head, head_r, teeth=teeth, depth=head_r * (0.30 + 0.20 * mouth),
             phase=phase, facing=facing, arc=math.radians(255))
    throat = 5 + (1 if mouth > 0.7 else 0)
    cell.solid(int(round(head[0] - (throat - 1) / 2.0)),
               int(round(head[1] - (throat - 1) / 2.0)), throat, throat)

    # No ground plant, and the body is held clear of the bottom of the cell. Every
    # crawling row of the line ends with a bar on the shelf; this one FLIES
    # (Locomotion::Fly, on its content row) and the habitat holds it at an altitude, so
    # ink anywhere near y = CH-1 would read as crawling no matter what the engine does
    # with the anchor.
    return cell


def threadbore():
    """SPR_PET_THREADBORE — the Worm line's bad Daemon, and the branch that grew OUT.

    Four rows of four 56x48 frames, the same clip set as every drawn row of the line:

      0  idle    4 frames — a hover. The wings beat through the loop and the body hangs
                            under them barely moving, which is what too little wing
                            working too hard looks like.
      1  attack  4 frames — a downbeat, a gape, and it arrives on top of the target.
      2  droop   2 frames — sagging between beats, maw slack, wings half folded.
      3  weak    2 frames — barely airborne, wings almost shut, mouth nearly closed.
    """
    sheet = Sheet(4, 4, CW, CH)

    # Idle. The wings run a full beat over the four frames while the body moves about a
    # pixel — deliberately the wrong way round for a flier this heavy, because the reason
    # to draw wings this small is to be caught working. The maw chews on the same loop,
    # which it inherits from the Script row along with the throat.
    for i in range(4):
        a = 2 * math.pi * i / 4
        head = (37.0 + 0.8 * math.sin(a), 30.0 - 0.7 * math.cos(a))
        sheet.place(i, 0, _threadbore_cell(
            head, (18.0, 33.0), (25.0, 32.0 + 0.5 * math.sin(a)),
            0.44 + 0.18 * math.sin(a), flap=math.sin(a), phase=a * 0.25))

    # Attack. It does not lunge and it does not rear — it gets ABOVE and comes down, and
    # the wings are what put it there. The gape is widest on the way down rather than at
    # the end, so the mouth arrives already open.
    for i, (hd, c0, c1, mouth, flap) in enumerate([
        ((36.0, 26.0), (18.0, 30.0), (25.0, 28.0), 0.32, -0.9),
        ((36.0, 23.0), (18.0, 28.0), (25.0, 25.0), 1.00, -1.5),
        ((38.0, 31.0), (18.5, 33.0), (27.0, 32.0), 1.00, 1.2),
        ((38.0, 33.0), (19.0, 35.0), (28.0, 34.0), 0.36, 0.4),
    ]):
        sheet.place(i, 1, _threadbore_cell(hd, c0, c1, mouth, flap, phase=i * 0.22))

    # Droop. Sagging between beats with the wings half folded and nothing behind the
    # mouth. It cannot sink far — a flier that settles onto the shelf has stopped being
    # one — so the mood is carried by the wings and the slack jaw rather than by height.
    for i in range(2):
        sheet.place(i, 2, _threadbore_cell(
            (37.0, 31.5 + 1.0 * i), (19.0, 35.0), (27.0, 34.0 + i),
            0.28, flap=1.0 + 0.15 * i, teeth=8))

    # Weak. Wings nearly shut and the maw almost closed — which on this creature is the
    # strongest statement the sheet can make, exactly as it is on the row below it.
    for i in range(2):
        sheet.place(i, 3, _threadbore_cell(
            (36.0, 32.5 + 0.5 * i), (19.0, 36.0), (27.0, 35.0 + i),
            0.12, flap=1.7, teeth=6))

    return sheet


# Every sheet this tool owns. A creature is added by writing its recipe above and one
# row here; nothing else in the repo needs to know the tool exists, because what ships
# is the committed PNG either way.
RECIPES = {
    "SPR_PET_NODEATODE": nodeatode,
    "SPR_PET_ROOTGRUB": rootgrub,
    "SPR_PET_SHENLOOP": shenloop,
    "SPR_PET_THREADBORE": threadbore,
}


# ---------------------------------------------------------------------------
#  CLI
# ---------------------------------------------------------------------------
def alpha_plane(path):
    """The committed sheet's set/unset pixels, read the way the atlas reads them."""
    w, h, rgba = decode_png_rgba(path)
    return w, h, bytes(1 if rgba[i * 4 + 3] else 0 for i in range(w * h))


def sheet_plane(sheet):
    return sheet.mask.w, sheet.mask.h, bytes(1 if v else 0 for v in sheet.mask.px)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[1])
    ap.add_argument("--check", action="store_true",
                    help="compare against the committed PNGs; nonzero exit on drift")
    ap.add_argument("--only", metavar="ASSET", help="just this one asset id")
    ap.add_argument("--preview", metavar="ROW", nargs="?", const="0",
                    help="ASCII one row of frames to stdout instead of writing")
    args = ap.parse_args()

    names = [args.only] if args.only else sorted(RECIPES)
    for name in names:
        if name not in RECIPES:
            sys.exit(f"no recipe for {name}; have {', '.join(sorted(RECIPES))}")
        sheet = RECIPES[name]()
        path = os.path.join(SPRITES, name + ".png")

        if args.preview is not None:
            row = int(args.preview)
            print(f"--- {name} row {row}")
            for y in range(row * sheet.ch, (row + 1) * sheet.ch):
                print("".join("#" if sheet.mask.get(x, y) else "."
                              for x in range(sheet.mask.w)))
            continue

        if args.check:
            if not os.path.exists(path):
                sys.exit(f"{name}: no committed PNG at {path} — run without --check")
            want, got = alpha_plane(path), sheet_plane(sheet)
            if want != got:
                if want[:2] != got[:2]:
                    sys.exit(f"{name}: committed sheet is {want[0]}x{want[1]}, "
                             f"the recipe draws {got[0]}x{got[1]}")
                n = sum(a != b for a, b in zip(want[2], got[2]))
                sys.exit(f"{name}: committed PNG and its recipe disagree on {n} px — "
                         f"re-run `python3 tools/gen_worm_art.py` and commit the result")
            print(f"  {name}: matches its recipe ({sheet.mask.count()} ink px)")
            continue

        with open(path, "wb") as f:
            f.write(sheet.png())
        print(f"wrote {os.path.relpath(path, REPO)}  "
              f"{sheet.mask.w}x{sheet.mask.h}, {sheet.cols}x{sheet.rows} cells, "
              f"{sheet.mask.count()} ink px")


if __name__ == "__main__":
    main()
