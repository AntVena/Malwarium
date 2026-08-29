#!/usr/bin/env python3
"""
snap_palette.py — fold a sprite sheet's stray colours into the ones it is drawn in.

A sheet's flash cost is decided by how many distinct (colour, coverage) pairs its pixels
hold: `tools/gen_assets.py` derives a palette from them and spends
`ceil(log2(entries))` bits per pixel, so the count only matters where it crosses a power
of two. Sheets exported from a paint tool routinely carry a tail of pairs that are one or
two pixels each — a half-transparent edge pixel, an off-by-one shade left by a
resample — and that tail is what pushes a sheet over the line. Cuttlefork holds 11
colours a reader could name and 24 more that account for 29 pixels between them, which
is what makes it 6-bit instead of 4-bit and costs 10.8 KB.

This rewrites the PNG so the tail is gone:

  * every pixel below `--alpha` coverage becomes fully transparent, and every pixel at or
    above it becomes fully opaque — a soft edge is information at photographic scale and
    noise at 56x48, where the shading law (assets/CREATURE_VISUAL_RULES.md §3) asks for
    named tones rather than a gradient;
  * every colour worn by fewer than `--floor` pixels is repainted with the nearest
    surviving colour, by squared distance in the 8-bit RGB the art carries.

Nothing here picks colours for the art. What survives is what the sheet was already
mostly drawn in, which is why the result is visually identical and why the tool is safe
to re-run: a sheet already free of drift is rewritten byte-identically and reports zero.

Run it on a sheet, look at the reported pixel count and the byte saving, and commit the
PNG if both read right:

    python3 tools/snap_palette.py assets/sprites/SPR_PET_CUTTLEFORK.png
    python3 tools/snap_palette.py assets/sprites/*.png --dry-run

`--dry-run` reports without writing, which is how to survey the whole tree for the sheets
worth spending a pass on. `tools/gen_assets.py --palettes` prints the palette a sheet
currently holds, in the order this tool ranks it.
"""

import argparse
import os
import struct
import sys
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gen_assets as gen


def write_png_rgba(path, w, h, rgba):
    """Write 8-bit RGBA, non-interlaced, filter 0 — the one form gen_assets.py reads."""
    raw = bytearray()
    stride = w * 4
    for y in range(h):
        raw.append(0)                                  # filter: None
        raw += rgba[y * stride:(y + 1) * stride]

    def chunk(tag, body):
        return (struct.pack(">I", len(body)) + tag + body
                + struct.pack(">I", zlib.crc32(tag + body) & 0xFFFFFFFF))

    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)))
        f.write(chunk(b"IDAT", zlib.compress(bytes(raw), 9)))
        f.write(chunk(b"IEND", b""))


def rgb_dist(a, b):
    return (a[0] - b[0]) ** 2 + (a[1] - b[1]) ** 2 + (a[2] - b[2]) ** 2


def snap(rgba, alpha_cut, floor):
    """-> (new rgba, pixels changed).

    Colours are bucketed by RGB565 — what actually reaches flash — so two 8-bit shades
    that quantise together are one entry here and neither counts as drift nor gets
    rewritten. A surviving pixel keeps its own 8-bit value untouched: the PNG stays the
    art, and the quantisation stays the generator's business.
    """
    counts = {}
    reps = {}
    for i in range(0, len(rgba), 4):
        if rgba[i + 3] < alpha_cut:
            continue
        key = gen.rgb565(rgba[i], rgba[i + 1], rgba[i + 2])
        counts[key] = counts.get(key, 0) + 1
        reps.setdefault(key, (rgba[i], rgba[i + 1], rgba[i + 2]))
    keep = [c for c, n in counts.items() if n >= floor]
    if not keep:                                       # a sheet drawn entirely in drift
        keep = list(counts) or [0]

    # One resolution per drift colour, not per pixel: a sheet holds tens of colours and
    # tens of thousands of pixels. Nearest by squared distance in the 8-bit values the
    # art actually carries, so the fold follows the drawing rather than the quantiser.
    kept = set(keep)
    resolved = {c: reps[min(keep, key=lambda k: rgb_dist(reps[c], reps[k]))]
                for c in counts if c not in kept}

    out = bytearray(rgba)
    changed = 0
    for i in range(0, len(rgba), 4):
        a = rgba[i + 3]
        if a < alpha_cut:
            out[i:i + 4] = b"\x00\x00\x00\x00"
            changed += 1 if a else 0     # already-clear pixels are not a repaint
            continue
        c = gen.rgb565(rgba[i], rgba[i + 1], rgba[i + 2])
        if c in resolved:
            out[i:i + 3] = bytes(resolved[c])
        out[i + 3] = 255
        changed += 1 if (c in resolved or a != 255) else 0
    return out, changed


def entries(rgba):
    """Distinct (colour, coverage) pairs — the palette gen_assets.py would derive."""
    seen = set()
    for i in range(0, len(rgba), 4):
        a = rgba[i + 3]
        seen.add((0, 0) if a == 0 else (gen.rgb565(rgba[i], rgba[i + 1], rgba[i + 2]), a))
    return len(seen)


def sheet_bytes(w, h, n):
    bpp = 1
    while (1 << bpp) < n:
        bpp += 1
    if bpp > 8:
        return w * h * 3
    return ((w * bpp + 7) // 8) * h + n * 3 + 1


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("png", nargs="+")
    ap.add_argument("--floor", type=int, default=16,
                    help="repaint colours worn by fewer than this many pixels (default 16)")
    ap.add_argument("--alpha", type=int, default=128,
                    help="coverage at or above which a pixel becomes fully opaque "
                         "(default 128)")
    ap.add_argument("--dry-run", action="store_true",
                    help="report without writing")
    args = ap.parse_args()

    total = 0
    for path in args.png:
        w, h, rgba = gen.decode_png_rgba(path)
        before = entries(rgba)
        out, changed = snap(rgba, args.alpha, args.floor)
        after = entries(out)
        saved = sheet_bytes(w, h, before) - sheet_bytes(w, h, after)
        total += saved
        name = os.path.basename(path)
        if before == after:
            print(f"{name}: {before} entries, already clean")
            continue
        print(f"{name}: {before} -> {after} entries, {changed} px repainted "
              f"({changed * 100.0 / (w * h):.3f}%), {saved} B")
        if not args.dry_run:
            write_png_rgba(path, w, h, out)
    if len(args.png) > 1:
        print(f"total: {total} B")


if __name__ == "__main__":
    sys.exit(main())
