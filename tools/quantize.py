#!/usr/bin/env python3
"""Snap a source image onto a creature's palette and hand it back device-ready.

The other half of `sheetpack.py`. That one owns GEOMETRY — which pixels land in which
56x48 cell, and what the trip down costs. This one owns COLOUR and COVERAGE: the tones a
sprite is allowed to use, and the hard alpha edge the blitters need. Neither knows about
the other, so a sheet can be re-snapped without repacking and repacked without re-snapping.

Run it before sheetpack when the source is generated or painted art; skip it entirely for
a sheet that is already on-palette.

WHY EACH PASS EXISTS — every one of these is a bug that shipped or nearly did:

  SHARPEN BEFORE THE RESIZE (--height), AT A STRENGTH THAT FOLLOWS THE RATIO. Generated
  source arrives two to four times the size of the cell it has to live in. A plain
  area-average at 4:1 melts adjacent forms into one flat mass — two tentacles side by
  side become a paddle. An unsharp pass on the SOURCE first keeps the seam that tells
  them apart. Order matters: sharpening after the resize just amplifies what the average
  already threw away. But the pass only rescues a steep trip: on a gentle one it is the
  thing doing the damage, driving every edge to the palette's extremes so ink and
  highlight both grow while the mid tones between them vanish. So it runs at full
  strength from 2:1 and ramps to nothing at 1:1.

  AREA-AVERAGE, NOT NEAREST. Nearest-neighbour deletes whole rows and columns, and thin
  detail sits on exactly those lines: one measured egg lost 3,983 one-pixel features that
  way, and its whole surface pattern with them. sheetpack's `decimate` mode is nearest by
  design — it preserves REGISTRATION across frames, which matters more there. Here nothing
  is being registered against anything, so the better filter wins.

  LUMINANCE-WEIGHTED SNAP. Nearest colour in plain RGB happily swaps a mid tone for one a
  step darker, which flattens the value structure the shading is made of. Weighting
  luminance triple keeps the steps apart, so the form survives the collapse to six tones.

  THE ACCENT CHANNEL (--accent). The one that actually bit. A line's eye colour is often
  its only non-body hue, and it is small. Average a three-pixel eye into the red around it
  and the result is a muddy colour that snaps to red — the eyes VANISH, silently, at every
  scale ratio, and nothing about the output looks broken. So an accent is masked in the
  source, carried down on its own channel, and stamped back afterwards. A source block
  holding any accent keeps it.
  The mask is "bright, and not red-dominant" rather than "green above red": the accent has
  shipped both as yellow-green (#E9FF8F) and as a paler yellow where green EQUALS red, and
  a rule keyed on green winning misses the second one completely.

  BINARY ALPHA. gen_assets.py wants a hard mask, and the x1.75 blitter smears anything
  soft. Generated art arrives with a partial-alpha fringe — a couple of hundred pixels on
  a creature-sized sprite.

  ONE OUTLINE INK (--outline). The roster does not agree with itself here: measured on the
  silhouette edge, SPR_PET_PAYPUP is one tone at 100% and SPR_PET_MALBEAR is six tones at
  ~68%. The gold standard states a rule the rest only half-follows. This forces the
  version Paypup keeps. See MASTER_TODO 2a-ii for the sweep that settles it roster-wide.

Usage:
    quantize.py in.png out.png '#170A0C' '#3D0F16' ... [flags]
"""

import argparse
import sys

try:
    from PIL import Image, ImageFilter
except ImportError:                                    # pragma: no cover
    sys.exit("quantize.py needs Pillow (unlike gen_assets.py, which is stdlib-only)")


def parse_hex(h):
    h = h.lstrip("#")
    if len(h) != 6:
        sys.exit(f"not a #rrggbb colour: {h!r}")
    return tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))


def luminance(c):
    return 0.2126 * c[0] + 0.7152 * c[1] + 0.0722 * c[2]


def distance(a, b):
    """Luminance-weighted, so a snap never trades a value step for a hue match."""
    dl = (luminance(a) - luminance(b)) / 255.0
    return 3.0 * dl * dl + sum(((a[i] - b[i]) / 255.0) ** 2 for i in range(3))


def is_accent(px, accents, tol=60):
    """Near one of the declared accents, OR bright and not red-dominant.

    The second half is what catches an accent the caller spelled slightly differently
    from how the source actually rendered it — the normal case for generated art, where
    one eye comes back as a scatter of neighbouring golds no two of which match the hex.

    Tuned against two failures, not guessed. Requiring green to beat red missed a pale
    yellow eye where they were EQUAL; requiring blue merely below green then swept up the
    near-neutral highlights. Green not far under red, and blue clearly under green, keeps
    every gold and olive the eye actually rendered as while leaving the body's pale red
    crown (#FFA98C, whose green sits far below its red) alone.
    """
    r, g, b, a = px
    if a < 128:
        return False
    for c in accents:
        if abs(r - c[0]) + abs(g - c[1]) + abs(b - c[2]) <= tol:
            return True
    return bool(accents) and g > 55 and g >= r - 55 and b < g - 30


def accent_mask(im, accents):
    m = Image.new("L", im.size, 0)
    src, dst = im.load(), m.load()
    for y in range(im.height):
        for x in range(im.width):
            if is_accent(src[x, y], accents):
                dst[x, y] = 255
    return m


def snap(im, palette, alpha_cut):
    out = im.copy()
    p = out.load()
    for y in range(out.height):
        for x in range(out.width):
            r, g, b, a = p[x, y]
            if a < alpha_cut:
                p[x, y] = (0, 0, 0, 0)
            else:
                p[x, y] = (*min(palette, key=lambda c: distance((r, g, b), c)), 255)
    return out


def outline(im, ink):
    """Force every silhouette-boundary pixel to one ink."""
    out = im.copy()
    src, dst = im.load(), out.load()
    for y in range(im.height):
        for x in range(im.width):
            if src[x, y][3] <= 8:
                continue
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nx, ny = x + dx, y + dy
                if not (0 <= nx < im.width and 0 <= ny < im.height) or src[nx, ny][3] <= 8:
                    dst[x, y] = (*ink, 255)
                    break
    return out


def edge_tones(im):
    """Distinct colours on the silhouette edge — 1 is consistent, more is not."""
    src = im.load()
    tones = set()
    for y in range(im.height):
        for x in range(im.width):
            if src[x, y][3] <= 8:
                continue
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nx, ny = x + dx, y + dy
                if not (0 <= nx < im.width and 0 <= ny < im.height) or src[nx, ny][3] <= 8:
                    tones.add(src[x, y][:3])
                    break
    return tones


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("src")
    ap.add_argument("out")
    ap.add_argument("palette", nargs="+", help="the body ramp, as #rrggbb")
    ap.add_argument("--accent", action="append", default=[],
                    help="a colour carried on its own channel so it can't average away; "
                         "repeatable. Keep the line's eye colour OUT of the body ramp and "
                         "pass it here — that is what lets a recolour effect repaint the "
                         "body and leave the eyes alone.")
    ap.add_argument("--height", type=int,
                    help="sharpen, then area-average down to this many px tall")
    ap.add_argument("--outline", metavar="HEX",
                    help="force every silhouette-boundary pixel to this ink")
    ap.add_argument("--alpha-cut", type=int, default=128, dest="alpha_cut",
                    help="alpha at or above this is opaque; below is cut (default 128)")
    ap.add_argument("--quiet", action="store_true")
    a = ap.parse_args()

    palette = [parse_hex(h) for h in a.palette]
    accents = [parse_hex(h) for h in a.accent]
    im = Image.open(a.src).convert("RGBA")
    before = im.size
    n_before = len({p[:3] for p in im.get_flattened_data() if p[3] > 8})
    partial = sum(1 for p in im.get_flattened_data() if 0 < p[3] < 255)

    mask = accent_mask(im, accents) if accents else None
    if a.height:
        w = round(im.width * a.height / im.height)
        # The sharpen's strength FOLLOWS THE RATIO, because the pass is a rescue for a
        # steep trip down and damage on a gentle one. Full strength from 2:1 — the
        # regime it was written for and measured in — then ramped to nothing at 1:1.
        # Radius 3 at 180% on an image barely being resized does not preserve a seam,
        # it drives every edge to the palette's extremes: measured on a Metamorphic
        # Daemon strip at 1.11:1, the fixed pass took the deep tones from 37% of the
        # drawing to 18%, grew the ink from 21% to 33%, and invented a 19% highlight
        # where the source had none. At the ramped strength the same trip reproduces
        # the source's value structure to within a point.
        ratio = im.height / a.height
        percent = 180 if ratio >= 2 else max(0, round(180 * (ratio - 1)))
        lit = (im.filter(ImageFilter.UnsharpMask(radius=3, percent=percent, threshold=2))
               if percent else im)
        im = lit.resize((w, a.height), Image.BOX)
        if mask is not None:
            mask = mask.resize((w, a.height), Image.BOX)

    out = snap(im, palette, a.alpha_cut)
    kept = 0
    if mask is not None and accents:
        op, mp = out.load(), mask.load()
        for y in range(out.height):
            for x in range(out.width):
                if mp[x, y] > 40 and op[x, y][3] > 8:
                    op[x, y] = (*accents[0], 255)
                    kept += 1
    if a.outline:
        out = outline(out, parse_hex(a.outline))
    out.save(a.out)

    if not a.quiet:
        alphas = sorted({p[3] for p in out.get_flattened_data()})
        tones = edge_tones(out)
        print(f"{a.src} {before[0]}x{before[1]} -> {a.out} {out.width}x{out.height}")
        print(f"  colours {n_before} -> {len({p[:3] for p in out.get_flattened_data() if p[3] > 8})}")
        print(f"  partial-alpha pixels {partial} -> "
              f"{sum(1 for p in out.get_flattened_data() if 0 < p[3] < 255)}  (alpha values {alphas})")
        if accents:
            print(f"  accent pixels recovered: {kept}"
                  + ("   !! none — check the accent colour" if kept == 0 else ""))
        print(f"  silhouette-edge tones: {len(tones)}"
              + ("  (consistent)" if len(tones) == 1 else "  (mixed — see --outline)"))


if __name__ == "__main__":
    main()
