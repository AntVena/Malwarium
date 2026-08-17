#!/usr/bin/env python3
"""Pack a set of per-clip SpriteCook strips into one Malwarium pet sheet.

Input:  N strips, each `frames` columns of a square source cell (e.g. 480x60).
Output: one sheet, `frames` columns x N rows of the project cell (56x48), which is
        what gen_assets.py's frame_width()/frame_rows() cut a SPR_PET_* sheet into.
        Row order is the row order of --rows, which is the order the CreatureDef's
        AnimClip rows must declare.

The interesting argument is --mode, which is the whole question of how source
pixels become cell pixels:

  decimate  uniform nearest-neighbour scale of the WHOLE source cell down to fit
            the target height, then centre-pad to the target width. Registration
            is preserved for free (every frame gets the same lattice), proportions
            are preserved, and the loss is a regular grid of deleted rows and
            columns. Lossy. This is the mode for 60x60 source.

  crop      no resampling at all: take the union content box across every frame of
            every strip, and centre it in the target cell. Lossless, and errors if
            the content does not fit. This is the mode for source that is already
            at project scale but has the wrong canvas around it.

--report prints what each mode actually cost, per strip: colours lost, opaque
pixels lost, and — for decimate — how many single-pixel features fell on a deleted
lattice line, which is the number that decides whether hand-repair is a touch-up or
a redraw.
"""

import argparse
import collections
import os
import sys

from PIL import Image

# The project cell, overridable with --cell: 56x48 is the Script/Process pet cell,
# 96x64 the Daemon one (CREATURE_VISUAL_RULES §7).
CELL_W, CELL_H = 56, 48
# Logical px left bare under a creature's feet inside its cell.
FLOOR_GAP = 1


def load_strip(path, frames):
    im = Image.open(path).convert("RGBA")
    w, h = im.size
    if w % frames:
        sys.exit(f"{os.path.basename(path)}: width {w} is not {frames} whole frames")
    return im, w // frames, h


def kept_lattice(src, dst):
    """Which source indices survive PIL's NEAREST resize from `src` to `dst`.

    Derived by resizing an index ramp rather than reimplementing PIL's rounding,
    so the damage report describes the resize that actually runs.
    """
    ramp = Image.new("I", (src, 1))
    ramp.putdata(range(src))
    return set(ramp.resize((dst, 1), Image.NEAREST).getdata())


def thin_runs_on(im, dropped_cols, dropped_rows):
    """Count 1px opaque runs that sit entirely on a deleted row or column."""
    px = im.load()
    w, h = im.size
    lost = 0
    for y in range(h):
        x = 0
        while x < w:
            c = px[x, y]
            n = 1
            while x + n < w and px[x + n, y] == c:
                n += 1
            if n == 1 and c[3] > 0 and x in dropped_cols:
                lost += 1
            x += n
    for x in range(w):
        y = 0
        while y < h:
            c = px[x, y]
            n = 1
            while y + n < h and px[x, y + n] == c:
                n += 1
            if n == 1 and c[3] > 0 and y in dropped_rows:
                lost += 1
            y += n
    return lost


def union_box(strips, frames):
    """Content box shared by every frame, in source-cell coordinates."""
    box = None
    for im, fw, fh in strips:
        for i in range(frames):
            b = im.crop((i * fw, 0, (i + 1) * fw, fh)).getbbox()
            if b is None:
                continue
            box = b if box is None else (
                min(box[0], b[0]), min(box[1], b[1]),
                max(box[2], b[2]), max(box[3], b[3]),
            )
    return box


def pack_decimate(strips, frames, report):
    _, (_, src_w, fh) = strips[0]
    scale_h = CELL_H
    scale_w = round(src_w * CELL_H / fh)
    if scale_w > CELL_W:
        scale_w, scale_h = CELL_W, round(fh * CELL_W / src_w)
    ox, oy = (CELL_W - scale_w) // 2, (CELL_H - scale_h) // 2

    keep_x = kept_lattice(src_w, scale_w)
    keep_y = kept_lattice(fh, scale_h)
    drop_x = set(range(src_w)) - keep_x
    drop_y = set(range(fh)) - keep_y
    if report:
        print(f"  lattice: {src_w}x{fh} -> {scale_w}x{scale_h}, "
              f"dropping {len(drop_x)} cols / {len(drop_y)} rows, "
              f"placed at +{ox},+{oy} in the {CELL_W}x{CELL_H} cell")

    out = []
    for name, (im, fw, fh) in strips:
        row = Image.new("RGBA", (CELL_W * frames, CELL_H), (0, 0, 0, 0))
        before = collections.Counter()
        after = collections.Counter()
        thin = 0
        for i in range(frames):
            src = im.crop((i * fw, 0, (i + 1) * fw, fh))
            dst = src.resize((scale_w, scale_h), Image.NEAREST)
            row.paste(dst, (i * CELL_W + ox, oy))
            before.update(p for p in src.get_flattened_data() if p[3] > 8)
            after.update(p for p in dst.get_flattened_data() if p[3] > 8)
            thin += thin_runs_on(src, drop_x, drop_y)
        if report:
            lost_c = len(set(before) - set(after))
            print(f"  {name:<28} colours {len(before):>3} -> {len(after):<3} "
                  f"(lost {lost_c})   opaque px {sum(before.values()):>5} -> "
                  f"{sum(after.values()):<5}   1px features on a deleted line: {thin}")
        out.append(row)
    return out


def pack_crop(strips, frames, report):
    box = union_box([s for _, s in strips], frames)
    if box is None:
        sys.exit("every frame is empty")
    bw, bh = box[2] - box[0], box[3] - box[1]
    if report:
        print(f"  union content box {bw}x{bh} at {box[:2]} (no resampling)")
    if bw > CELL_W or bh > CELL_H:
        sys.exit(f"content {bw}x{bh} does not fit the {CELL_W}x{CELL_H} cell — "
                 f"use --mode decimate, or re-source at project scale")
    # Centred across, but SEATED down: the habitat bottom-anchors the whole cell
    # (drawHabitat, game_render.cpp), so padding left under the feet becomes float —
    # the creature hovers that many logical px over the shelf. The shipped roster sits
    # its feet one px off the cell floor, so that is the gap reproduced here.
    ox = (CELL_W - bw) // 2
    # Content as tall as the cell has no room for the gap; seating it anyway would
    # push the crown off the top edge, so the floor gap yields to the drawing.
    oy = max(0, CELL_H - bh - FLOOR_GAP)

    out = []
    for name, (im, fw, fh) in strips:
        row = Image.new("RGBA", (CELL_W * frames, CELL_H), (0, 0, 0, 0))
        for i in range(frames):
            src = im.crop((i * fw + box[0], box[1], i * fw + box[2], box[3]))
            row.paste(src, (i * CELL_W + ox, oy))
        if report:
            print(f"  {name:<28} placed at +{ox},+{oy}, lossless")
        out.append(row)
    return out


def verify(path, frames, rows, max_colours):
    im = Image.open(path).convert("RGBA")
    w, h = im.size
    alphas = sorted({p[3] for p in im.get_flattened_data()})
    colours = {p for p in im.get_flattened_data() if p[3] > 8}
    ok = True
    print(f"\nverify {os.path.basename(path)}  {w}x{h}")
    for label, cond, detail in [
        (f"width cuts into {CELL_W}px frames", w % CELL_W == 0, f"{w} % {CELL_W} = {w % CELL_W}"),
        (f"height cuts into {CELL_H}px rows", h % CELL_H == 0, f"{h} % {CELL_H} = {h % CELL_H}"),
        ("frame/row count as asked", (w // CELL_W, h // CELL_H) == (frames, rows),
         f"got {w // CELL_W}x{h // CELL_H}, wanted {frames}x{rows}"),
        ("alpha is binary", alphas in ([0, 255], [0], [255]), f"values {alphas}"),
        (f"colours within the {max_colours} this cell ships",
         len(colours) <= max_colours, f"{len(colours)} opaque"),
    ]:
        print(f"  [{'ok' if cond else '!!'}] {label:<36} {detail}")
        ok &= cond
    return ok


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--src", default=".", help="directory holding the strips")
    ap.add_argument("--out", required=True, help="packed sheet to write")
    ap.add_argument("--rows", required=True,
                    help="clip=file.png,... in row order, e.g. "
                         "idle=character_idle.png,attack=character_attack.png")
    ap.add_argument("--frames", type=int, default=8, help="columns per strip")
    ap.add_argument("--mode", choices=("decimate", "crop"), default="decimate")
    ap.add_argument("--preview", metavar="PNG",
                    help="also write an 8x nearest upscale for eyeballing")
    ap.add_argument("--quiet", action="store_true", help="skip the damage report")
    ap.add_argument("--max-colours", type=int, default=8, dest="max_colours",
                    help="opaque-colour cap; a Daemon carries more than a Script "
                         "(CREATURE_VISUAL_RULES §4)")
    ap.add_argument("--cell", metavar="WxH",
                    help="target cell, default 56x48; a Daemon is 96x64")
    a = ap.parse_args()
    if a.cell:
        global CELL_W, CELL_H
        CELL_W, CELL_H = (int(v) for v in a.cell.lower().split("x"))

    rows = []
    for pair in a.rows.split(","):
        clip, _, fn = pair.partition("=")
        if not fn:
            sys.exit(f"--rows wants clip=file.png, got {pair!r}")
        rows.append((clip, load_strip(os.path.join(a.src, fn), a.frames)))

    print(f"packing {len(rows)} rows x {a.frames} frames, mode={a.mode}")
    packer = pack_decimate if a.mode == "decimate" else pack_crop
    packed = packer(rows, a.frames, not a.quiet)

    sheet = Image.new("RGBA", (CELL_W * a.frames, CELL_H * len(rows)), (0, 0, 0, 0))
    for i, row in enumerate(packed):
        sheet.paste(row, (0, i * CELL_H))
    sheet.save(a.out)

    if a.preview:
        sheet.resize((sheet.width * 8, sheet.height * 8), Image.NEAREST).save(a.preview)
        print(f"  preview -> {a.preview}")

    good = verify(a.out, a.frames, len(rows), a.max_colours)
    print(f"\n{a.out}")
    print("clips, in the row order to declare on the CreatureDef:")
    for i, (clip, _) in enumerate(rows):
        print(f"  {{\"{clip}\", /*row*/ {i}, /*frames*/ {a.frames}, /*holdBeats*/ 1}},")
    return 0 if good else 1


if __name__ == "__main__":
    sys.exit(main())
