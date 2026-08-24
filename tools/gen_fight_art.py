#!/usr/bin/env python3
"""
gen_fight_art.py — the combat screen's 1-bit vocabulary: strike marks and status glyphs.

This is an AUTHORING step, not a build step, and it owns the PNGs it emits: the
committed files under assets/ui/ and assets/icons/ are what the atlas compiles, and
this is what they are regenerated FROM. Run it by hand when a recipe changes:

    python3 tools/gen_fight_art.py                    # write every recipe
    python3 tools/gen_fight_art.py --check            # committed art still matches?
    python3 tools/gen_fight_art.py --preview worm     # ASCII one sheet to the terminal

Why a tool at all
-----------------
Same reason the pantry has one (tools/gen_item_icons.py): these are one-ink shapes at a
size where hand work drifts, and they are only worth anything AS A SET. A strike mark
answers "what just hit me" by silhouette alone, so the five of them have to be five
clearly different shapes drawn to one weight — one getting a 1px stroke and the next a
3px one is the whole read gone. The vocabulary below is that weight, expressed as the
only forms a fight mark is allowed to be made of.

The rules it enforces, because they cannot be expressed otherwise:
  * one ink, full alpha or none — every sheet here is TINTED at draw time, so a grey
    would be a grey of whatever colour the screen picked;
  * strokes are 2px minimum, because a 1px line vanishes against a creature;
  * a mark is drawn as though the blow travels RIGHT, and the engine mirrors it for a
    blow going the other way (SpriteData::facing, core/render/sprite.h) — so every
    recipe here bows, points and trails one way only.

PAIRS, and why every source has exactly two
-------------------------------------------
Fighters trade blows for a whole fight, so a source that owned one mark showed the same
drawing every few seconds and the mark stopped reading as an event. Each source gets a
pair, and the engine walks it on the fight's own swing count — so no two blows in a row
draw the same frame, which is the sequence a player actually watches.

The pair is not always two variants of one form — it is whatever says the line twice.
Ransomware varies its slash; Phishing spends its two on different halves of the same
idea (the lure, then the strike that follows it).

Metamorphic has no pair, and cannot want one: none of its rows casts itself. Every
wildcard rolls a move out of another line or the common pool (content_moves.cpp), so a
Metamorphic pet already shows the mark of whatever it rolled — the same thing FX_CAMO
does with that line's colours.
"""

import argparse
import os
import sys

try:
    from PIL import Image
except ImportError:  # pragma: no cover - the tool is optional, the gate is not
    print("gen_fight_art: needs Pillow (pip install pillow)", file=sys.stderr)
    sys.exit(2)

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
UI_DIR = os.path.join(REPO, "assets", "ui")
ICON_DIR = os.path.join(REPO, "assets", "icons")

# A strike mark's cell. Wider and taller than the clash lane it travels down (26..58 px,
# ui/combat_screen.cpp) on purpose: a blow that stopped dead at the lane edge would read
# as a mark BETWEEN the fighters rather than as one landing ON one of them.
STRIKE_W, STRIKE_H = 28, 44

# A status glyph's cell — the size the fight strip already draws (8x8, one per condition,
# tinted). Two frames is the whole animation budget: the strip sits under a fighter's feet
# and anything busier there competes with the fight above it.
GLYPH_W, GLYPH_H = 8, 8

# The clear border a STRIKE cell keeps. A mark floats inside a cell it is smaller than —
# it travels down the lane, so the cell is travel room, not the drawing. Ink on the cell
# edge means the shape was drawn bigger than the cell and silently CUT, which at this
# size takes the point off a slash or a whole barb off a harpoon and leaves something
# that still looks deliberate. Enforced in main(); the 8x8 status glyphs are exempt,
# since a glyph that small IS its cell.
MARGIN = 1


# --- The canvas -------------------------------------------------------------

class G:
    """A one-ink grid. `set` draws, `clear` cuts detail back out of a mass."""

    def __init__(self, w, h):
        self.w, self.h = w, h
        self.g = [[0] * w for _ in range(h)]

    def px(self, x, y, on=1):
        x, y = int(round(x)), int(round(y))
        if 0 <= x < self.w and 0 <= y < self.h:
            self.g[y][x] = on

    def rect(self, x0, y0, x1, y1, on=1):
        for y in range(int(min(y0, y1)), int(max(y0, y1)) + 1):
            for x in range(int(min(x0, x1)), int(max(x0, x1)) + 1):
                self.px(x, y, on)

    def disc(self, cx, cy, r, on=1):
        """A superellipse rather than a circle — blockier, which is the house look."""
        for y in range(self.h):
            for x in range(self.w):
                dx = abs((x - cx) / max(r, 0.5))
                dy = abs((y - cy) / max(r, 0.5))
                if dx ** 2.6 + dy ** 2.6 <= 1.0:
                    self.px(x, y, on)

    def ring(self, cx, cy, r, w=2):
        """A bubble: a disc with a smaller one cut back out of it."""
        self.disc(cx, cy, r, 1)
        self.disc(cx, cy, r - w, 0)

    def stroke(self, pts, w=2, on=1):
        """A thick polyline — the 2px minimum is the point of it."""
        r = max(int(w) // 2, 0)
        for i in range(len(pts) - 1):
            x0, y0 = pts[i]
            x1, y1 = pts[i + 1]
            steps = int(max(abs(x1 - x0), abs(y1 - y0), 1) * 3)
            for s in range(steps + 1):
                x = x0 + (x1 - x0) * s / steps
                y = y0 + (y1 - y0) * s / steps
                for dy in range(-r, r + 1 if int(w) % 2 else r):
                    for dx in range(-r, r + 1 if int(w) % 2 else r):
                        self.px(x + dx, y + dy, on)

    def shear(self, k):
        """Tilt: shift each row sideways in proportion to its distance from centre.

        What turns one drawing into a pair for free — a skull that leans one way and then
        the other is the same skull, and at 8px a one-pixel lean is the whole tilt.
        """
        out = [[0] * self.w for _ in range(self.h)]
        cy = (self.h - 1) / 2.0
        for y in range(self.h):
            d = int(round((y - cy) * k))
            for x in range(self.w):
                if self.g[y][x] and 0 <= x + d < self.w:
                    out[y][x + d] = 1
        self.g = out

    def ascii(self):
        return "\n".join("".join("#" if c else "." for c in row) for row in self.g)

    def image(self):
        im = Image.new("RGBA", (self.w, self.h), (255, 255, 255, 0))
        px = im.load()
        for y in range(self.h):
            for x in range(self.w):
                if self.g[y][x]:
                    px[x, y] = (255, 255, 255, 255)
        return im


def sheet(frames):
    """Lay a pair out as a horizontal 2-frame strip, which is how the engine reads it."""
    w, h = frames[0].w, frames[0].h
    im = Image.new("RGBA", (w * len(frames), h), (255, 255, 255, 0))
    for i, f in enumerate(frames):
        im.alpha_composite(f.image(), (i * w, 0))
    return im


# --- The strike vocabulary --------------------------------------------------
#
# Five forms, one per source, and nothing shares one. Two sources drawn from the same
# form would answer "what just hit me" with the same shape, which is the only question
# the mark is on screen to answer.

def gash(g, cx, cy, half, bow=6, thick=4):
    """A tapered crescent — a cut, bowing and thickening through its belly.

    Both ends come to a point, so the stroke reads as something swept rather than as a
    bar someone laid down. `half` is what separates a Ransomware slash from a common one:
    the FORM is shared between those two, the reach is not.
    """
    for t in range(-half, half + 1):
        fall = half * half - t * t
        b = bow * fall // (half * half)
        body = 1 + thick * fall // (half * half)
        for k in range(body):
            g.px(cx + b + k, cy + t)


def barb(g, tipx, cy, back, spread):
    """A harpoon: a shaft, a long open head, and two barbs flared off the back of it.

    Phishing's half of its pair. It has to stay a HARPOON and not become an arrow, which
    is a question of how open the head is: a short head at 3px stroke closes its own
    interior and the whole thing flattens into a UI glyph pointing right. So the head
    reaches most of the way back down the shaft, which keeps a hole in the middle of it
    and leaves room for the barbs to read as barbs.
    """
    g.stroke([(MARGIN + 6, cy), (tipx, cy)], w=3)              # shaft
    for side in (-1, +1):
        g.stroke([(tipx, cy), (back, cy + side * spread)], w=3)         # head
        g.stroke([(back, cy + side * spread),
                  (back - 5, cy + side * (spread + 5))], w=2)           # flared barb
        # Streaks trailing off the barbs. Without them the head is a clean triangle on a
        # stick, which is the shape of a UI arrow — a thing that points rather than a
        # thing that was thrown. The motion is what makes it a thrust.
        for k in (0, 1):
            y = cy + side * (spread + 2 + k * 6)
            g.stroke([(back - 6 - k * 3, y), (MARGIN + 1 + k, y)], w=2)


def jaws(g, cx, cy, w, h, gap, tooth):
    """Two opposing toothed arcs. `gap` 0 is shut, and shut is the one that just fired.

    Trojan's form, off its own passive: an armed trap hijacking the turn
    (LinePassive::ExecOverride) is a thing that was waiting and then closed.

    `h` is the arch, and it is what actually says open or shut — not `gap`. The arcs bow
    AWAY from each other, hinged at their corners the way a mouth is, so a deep arch is a
    mouth open at the middle however close the two bases are set. Shut means flattening
    the arch until the teeth meet along the whole bite.
    """
    for side in (-1, +1):
        base = cy + side * gap
        # The arc: a jaw line that curves back toward the hinge at both ends.
        pts = []
        for i in range(-w, w + 1, 2):
            fall = (w * w - i * i) / float(w * w)
            pts.append((cx + i, base + side * (h * fall)))
        g.stroke(pts, w=3)
        # Teeth, hanging off the arc into the gap — three a side, biggest at the middle.
        for i in (-w // 2, 0, w // 2):
            fall = (w * w - i * i) / float(w * w)
            y0 = base + side * (h * fall)
            g.stroke([(cx + i, y0), (cx + i, y0 - side * (tooth + int(2 * fall)))], w=2)


def spit(g, pts):
    """Projectile blobs with a speed tick trailing each one.

    Worm's form. The line is the one that puts SEVERAL of a thing on the screen at once
    (its replicas), so its mark is several of a thing too. The tick is clamped to the
    cell rather than sized off the blob: a trail that ran out of the frame would be cut
    to a stub on whichever blob happened to sit furthest back.
    """
    for (x, y, r) in pts:
        g.disc(x, y, r)
        tail = max(MARGIN + 1, x - r - 2 - (r + 2))
        if x - r - 2 > tail:
            g.stroke([(x - r - 2, y), (tail, y)], w=2)


# --- The recipes ------------------------------------------------------------

def strike_common(v):
    """Small slashes. The floor the other four are read against, so it stays modest:
    two short cuts, then three shorter ticks."""
    g = G(STRIKE_W, STRIKE_H)
    if v == 0:
        gash(g, 9, 16, 8, bow=4, thick=3)
        gash(g, 14, 29, 8, bow=4, thick=3)
    else:
        gash(g, 8, 13, 6, bow=3, thick=2)
        gash(g, 13, 22, 6, bow=3, thick=2)
        gash(g, 18, 31, 6, bow=3, thick=2)
    return g


def strike_ransomware(v):
    """Big slashes — the same form as common, spent at full reach. The line that locks
    your files hits like something with claws, and it is the loudest mark in the set."""
    g = G(STRIKE_W, STRIKE_H)
    if v == 0:
        # Spaced so the three bellies never touch: a slash's whole read is that it is one
        # of SEVERAL parallel cuts, and three that merge in the middle are one fat blob.
        gash(g, 1, 22, 19, bow=6, thick=3)
        gash(g, 9, 20, 18, bow=6, thick=3)
        gash(g, 17, 24, 17, bow=6, thick=3)
    else:
        # Crossed: two full-height cuts leaning opposite ways, an X torn through the lane.
        for t in range(-20, 21):
            fall = 400 - t * t
            body = 1 + 4 * fall // 400
            for k in range(body):
                g.px(6 + (t + 20) * 14 // 40 + k, 22 + t)
                g.px(6 + (20 - t) * 14 // 40 + k, 22 + t)
    return g


def strike_phishing(v):
    """The lure, then the strike. Bubbles rising off the bait on one frame and the barbed
    thrust on the other — the two halves of getting somebody to come to you."""
    g = G(STRIKE_W, STRIKE_H)
    if v == 0:
        g.ring(8, 31, 6)
        g.ring(16, 21, 5)
        g.ring(21, 12, 4)
        g.disc(24, 6, 2)
    else:
        barb(g, 25, 22, back=13, spread=8)
    return g


def strike_trojan(v):
    """A trap that was waiting, and then was not. Open on one frame, shut with the impact
    ticks thrown off it on the other."""
    g = G(STRIKE_W, STRIKE_H)
    if v == 0:
        jaws(g, 14, 22, 9, 7, gap=9, tooth=4)
    else:
        # Shut, not fused. The seam and the interlocked teeth are what say "closed on
        # something": with the arcs any nearer they weld into one oval and the trap
        # reads as an eye.
        jaws(g, 14, 22, 9, 2, gap=4, tooth=3)
        # Struck sparks, thrown out of the closed bite.
        for (dx, dy) in ((-8, -11), (8, -10), (-7, 11), (9, 10)):
            g.stroke([(14 + dx, 22 + dy), (14 + dx * 13 // 10, 22 + dy * 13 // 10)], w=2)
    return g


def strike_worm(v):
    """Little projectiles, several at once. Three fat ones, then a looser spread of five —
    the count is what changes, because a swarm is what this line is."""
    g = G(STRIKE_W, STRIKE_H)
    if v == 0:
        spit(g, [(21, 13, 5), (15, 24, 4), (10, 34, 4)])
    else:
        spit(g, [(22, 9, 4), (17, 18, 3), (21, 27, 3), (14, 31, 3), (10, 38, 3)])
    return g


def glyph_dot(v):
    """A skull, leaning. Damage over time is the condition that finishes you if nothing
    changes, and a skull says that where three anonymous dots said nothing at all.

    Drawn as a solid cranium with the sockets CUT back out of it, so the shape survives
    being tinted flat. The lean is a one-pixel shear (G.shear) and is the whole of the
    pair: the same skull rocking, which reads as something ticking rather than as two
    different glyphs taking turns.
    """
    g = G(GLYPH_W, GLYPH_H)
    g.rect(2, 0, 5, 0)          # rounded crown
    g.rect(1, 1, 6, 4)          # cranium
    g.rect(2, 2, 2, 3, 0)       # eye sockets, cut out
    g.rect(5, 2, 5, 3, 0)
    g.rect(2, 5, 5, 5)          # jaw
    g.rect(2, 6, 4, 6)          # teeth...
    g.rect(3, 6, 3, 6, 0)       # ...with the gap between them cut
    g.shear(0.25 if v == 0 else -0.25)
    return g


def star(g, cx, cy, r):
    """A sparkle: a filled diamond, which at this size is the only star there is room for.

    Bars crossing would have been a plus — the sides have to come IN between the points
    for the eye to call it a star, and below about 7px across there is no way to do that
    and still have a body left. The diamond keeps the four points and spends nothing.
    """
    for j in range(-r, r + 1):
        for i in range(-r, r + 1):
            if abs(i) + abs(j) <= r:
                g.px(cx + i, cy + j)


def glyph_stun(v):
    """Stars going round. Two of them swapping size and corner across the pair, which is
    the cartoon read for a head that has stopped working — a spiral held still was just a
    shape, and said "stunned" only because it was captioned STUN elsewhere.
    """
    g = G(GLYPH_W, GLYPH_H)
    big, small = ((2, 2), (6, 6)) if v == 0 else ((5, 5), (1, 1))
    star(g, big[0], big[1], 2)
    star(g, small[0], small[1], 1)
    return g


# One row per sheet: asset id -> (directory, cell, the two frame recipes).
RECIPES = {
    "UI_STRIKE_COMMON":     (UI_DIR, strike_common),
    "UI_STRIKE_RANSOMWARE": (UI_DIR, strike_ransomware),
    "UI_STRIKE_PHISHING":   (UI_DIR, strike_phishing),
    "UI_STRIKE_TROJAN":     (UI_DIR, strike_trojan),
    "UI_STRIKE_WORM":       (UI_DIR, strike_worm),
    "ICON_FIGHT_DOT":       (ICON_DIR, glyph_dot),
    "ICON_FIGHT_STUN":      (ICON_DIR, glyph_stun),
}


def render(asset_id):
    _, recipe = RECIPES[asset_id]
    return [recipe(0), recipe(1)]


def asset_path(asset_id):
    return os.path.join(RECIPES[asset_id][0], asset_id + ".png")


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--check", action="store_true",
                    help="fail if any committed sheet differs from its recipe")
    ap.add_argument("--preview", metavar="ID",
                    help="ASCII one sheet's two frames and exit (id, or its short name)")
    args = ap.parse_args()

    if args.preview:
        want = args.preview.upper()
        hit = [a for a in RECIPES if a == want or a.endswith("_" + want)]
        if not hit:
            print("no recipe for %r" % args.preview, file=sys.stderr)
            return 2
        for i, f in enumerate(render(hit[0])):
            print("%s frame %d:" % (hit[0], i))
            print(f.ascii())
            print()
        return 0

    bad = []
    for asset_id in sorted(RECIPES):
        frames = render(asset_id)
        # The margin rule (MARGIN above). Only the strike cells, which are travel room
        # around a smaller drawing; a 8x8 status glyph is allowed to be its whole cell.
        if frames[0].w == STRIKE_W:
            for i, f in enumerate(frames):
                ink = [(x, y) for y in range(f.h) for x in range(f.w) if f.g[y][x]]
                if not ink:
                    bad.append("%s frame %d: empty" % (asset_id, i))
                    continue
                xs, ys = [p[0] for p in ink], [p[1] for p in ink]
                if (min(xs) < MARGIN or min(ys) < MARGIN
                        or max(xs) > f.w - 1 - MARGIN or max(ys) > f.h - 1 - MARGIN):
                    bad.append("%s frame %d: ink on the cell edge (x %d-%d, y %d-%d of "
                               "%dx%d) — the drawing is being cut"
                               % (asset_id, i, min(xs), max(xs), min(ys), max(ys),
                                  f.w, f.h))
        want = sheet(frames)
        path = asset_path(asset_id)
        if args.check:
            if not os.path.exists(path):
                bad.append("%s: missing" % os.path.basename(path))
                continue
            have = Image.open(path).convert("RGBA")
            if have.size != want.size or have.tobytes() != want.tobytes():
                bad.append("%s: differs from its recipe" % os.path.basename(path))
        elif not bad:
            want.save(path)

    if bad and not args.check:
        for line in bad:
            print("gen_fight_art: %s" % line, file=sys.stderr)
        return 1

    if args.check:
        for line in bad:
            print("gen_fight_art: %s" % line, file=sys.stderr)
        if bad:
            print("gen_fight_art: run tools/gen_fight_art.py to regenerate",
                  file=sys.stderr)
            return 1
        print("gen_fight_art: %d sheets match" % len(RECIPES))
    else:
        print("gen_fight_art: wrote %d sheets" % len(RECIPES))
    return 0


if __name__ == "__main__":
    sys.exit(main())
