#!/usr/bin/env python3
"""
gen_assets.py  —  Malwarium build-time asset + palette codegen.

Converts the shippable PNGs in assets/ into C++ sprite arrays, and PAL_CORE.json
into a role-token palette table, written to src/generated/. A sheet is stored in
whichever of SpriteData's four forms is smallest (core/render/sprite.h): a 1-bit mask,
a deduplicated tile grid over a derived palette, flat `bpp`-bit indices into one, or
full RGB565 + alpha.

Design notes
------------
* Pure standard library (zlib only) — no Pillow / external deps, so any team
  member can regenerate assets with a stock Python 3. All shippable PNGs are
  8-bit RGBA, non-interlaced, filter-method 0 (verified), which keeps the
  decoder lean.
* Underscore-prefixed files (assets/_*.png) are design studies and are skipped.
* Pet sprite sheets (SPR_PET_*) are horizontal strips of 56x48 logical frames;
  everything else is treated as a single frame. A pet sheet whose height is a
  clean multiple of 48 is a vertical stack of that many ROWS (each its own
  56xN-frame strip) rather than one tall frame — see PET_ROW_H below.

Run from the repo root:  python3 tools/gen_assets.py
`--palettes` prints what each sheet's derived palette holds instead of generating, which
is the survey behind a tools/snap_palette.py pass.
"""

import os
import re
import sys
import json
import zlib
import struct

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ASSETS_DIR = os.path.join(REPO, "assets")
OUT_DIR = os.path.join(REPO, "src", "generated")


def asset_paths(repo=None):
    """The atlas: every compiled asset's name -> its repo-relative PNG path.

    assets/ IS the atlas — every PNG under it is RGB565 data linked into the
    firmware whether or not a line of code names it, so the tree listing and the
    shipped set are the same thing. Art with no consumer belongs in assets/_attic/
    instead; any path part starting with `_` is skipped, which covers both the attic
    and a design study parked loose at the top level.

    The subfolders (sprites/ · icons/ · ui/) are for readers only: an asset's id is
    its BASENAME, so art can be refiled without renaming anything downstream. The
    cost of that is that two PNGs sharing a basename would be two files claiming one
    id, which raises here rather than silently resolving to whichever sorted last.

    This is the one place that knows where an asset lives. tools/gen_pedia_data.py
    and tools/check_orphan_assets.py both read it instead of globbing themselves —
    the latter against its own `--repo`, hence the override.
    """
    repo = REPO if repo is None else os.path.abspath(repo)
    found = {}
    for root, dirs, files in os.walk(os.path.join(repo, "assets")):
        dirs[:] = sorted(d for d in dirs if not d.startswith((".", "_")))
        for fn in sorted(files):
            if not fn.endswith(".png") or fn.startswith("_"):
                continue
            name = fn[: -len(".png")]
            rel = os.path.relpath(os.path.join(root, fn), repo).replace(os.sep, "/")
            if name in found:
                raise ValueError(
                    f"{name}: two PNGs claim this name ({found[name]} and {rel}). "
                    "An asset id is its basename, so basenames must be unique "
                    "across all of assets/.")
            found[name] = rel
    return dict(sorted(found.items()))


PET_FRAME_W = 56          # spec: pet sprite cell is 56x48 logical
PET_ROW_H = 48            # a pet sheet's height, if a clean multiple of this, is that many rows
PET_PREFIX = "SPR_PET_"

# Which way the DRAWING in a sheet is turned (SpriteData::facing, core/render/sprite.h).
# An authorial fact about the art, so it is declared here rather than measured: no pass
# over pixels can tell a head from a tail.
#
# ONLY the sheets with a side to them appear. Everything absent is Facing::None and is
# never mirrored, which is right for the three-quarter turned-to-the-viewer standing
# pose assets/CREATURE_VISUAL_RULES.md §2 asks every creature for (Paypup, Malbear,
# Pingcub, the whole Metamorphic octopus branch), for every egg and stand-in, and for
# all of the icon/UI family. The Worm replica glyphs are absent deliberately: 16x8 of
# 1-bit outline is below the size at which a facing reads at all, and the rank they
# stand in is already seated toward the enemy by the caller (ui/worm_replicas.h).
#
# Add a row when a new sheet's body reads across its cell. Getting one wrong turns a
# creature away from its opponent, which is exactly what this table exists to prevent —
# check it against the sheet, not against the creature's cousins.
FACING = {
    # Head to the right of the cell.
    "SPR_PET_BAITRACUDA": "Right",
    "SPR_PET_BREECHEETAH": "Right",
    "SPR_PET_CLICKBAIT": "Right",
    "SPR_PET_CROAKEN": "Right",
    "SPR_PET_GOLIAUTH": "Right",
    "SPR_PET_PWNTHER": "Right",
    "SPR_PET_SPAMWHALE": "Right",
    "SPR_PET_GENERIC_DAEMON": "Right",
    # The whole Worm line, drawn head-to-upper-right by tools/gen_worm_art.py — the one
    # family that is mechanically consistent, because a tool draws it.
    "SPR_PET_NODEATODE": "Right",
    "SPR_PET_ROOTGRUB": "Right",
    "SPR_PET_SHENLOOP": "Right",
    "SPR_PET_THREADBORE": "Right",
    "SPR_PET_USBASILISK": "Right",
    "SPR_PET_COAXEEL": "Right",

    # The strike marks, every one drawn as though the blow travels right; the screen
    # mirrors them for a blow going the other way.
    "UI_STRIKE_COMMON": "Right",
    "UI_STRIKE_RANSOMWARE": "Right",
    "UI_STRIKE_PHISHING": "Right",
    "UI_STRIKE_TROJAN": "Right",
    "UI_STRIKE_WORM": "Right",

    # Head to the left of the cell.
    "SPR_PET_BARKMAIL": "Left",
    "SPR_PET_CUTTLEFORK": "Left",
    "SPR_PET_EXTORGI": "Left",
    "SPR_PET_KALICO": "Left",
    "SPR_PET_KEYLOGGERHEAD": "Left",
    "SPR_PET_PHISHLET": "Left",
    "SPR_PET_TADPOLL": "Left",
    "SPR_PET_WIRE_HEIR": "Left",
    "SPR_PET_GENERIC_SCRIPT": "Left",
    # Every wild malbeast with a side to it faces left, and a wild always holds the
    # right-hand seat, so the line already looks into the fight and mirrors nowhere.
    "SPR_MALBEAST_BUFFER_WYRM": "Left",
    "SPR_MALBEAST_CACHE_GHOUL": "Left",
    "SPR_MALBEAST_GLITCHHOG": "Left",
    "SPR_MALBEAST_KERNEL_LEVIATHAN": "Left",
}

# Non-pet sheets that are horizontal frame strips rather than one wide image,
# keyed by asset name -> one frame's width. See frame_width().
FRAME_W_OVERRIDES = {
    "SPR_EGG_PHISH_MICRO": 14,   # 28x14 = the clutch tile's 2-frame swim loop
    "ICON_EXPL": 28,             # 168x28 = the globe's 6-frame rotation
    # 96x8 = the Worm replicas' six 16x8 frames, in idle/attack/death pairs. Not
    # SPR_PET_ sheets: a replica is one glyph shared by the whole line rather than a
    # creature with a 56x48 cell, so it needs the override to be a strip at all.
    "SPR_WORM_REPLICA_ATTACK": 16,
    "SPR_WORM_REPLICA_DEFEND": 16,
    # The combat screen's strike marks: 56x44 = two 28x44 frames, the pair a source
    # alternates between (tools/gen_fight_art.py). Wider than tall, so without the
    # override each would read as one very wide single frame.
    "UI_STRIKE_COMMON": 28,
    "UI_STRIKE_RANSOMWARE": 28,
    "UI_STRIKE_PHISHING": 28,
    "UI_STRIKE_TROJAN": 28,
    "UI_STRIKE_WORM": 28,
    # 16x8 = the two ANIMATED fight-status glyphs, a skull that rocks and a pair of
    # stars going round. The rest of the ICON_FIGHT_* family is a single 8x8 cell and
    # needs no row here.
    "ICON_FIGHT_DOT": 8,
    "ICON_FIGHT_STUN": 8,
    # 768x64 = eight 96x64 frames. An ANIMATED sheet at the oversized Daemon cell needs
    # the override for the same reason a Script sheet does not: the rule below cuts a
    # SPR_PET_ sheet on 56px only when the width divides by it, and 768 does not, so
    # without this the whole strip reads as one very wide single frame.
    "SPR_PET_WIRE_HEIR": 96,
    # 512x64 = eight 64x64 frames, the same case one cell narrower. 64 does not divide
    # by 56 either, so the Metamorphic Daemon needs the row here to animate at all.
    "SPR_PET_TENTACLONE": 64,
    # 528x48 = eight 66x48 frames. The Script cell here is wider than the standard 56
    # because the creature is an eight-armed splay that reads across, and 66 does not
    # divide by 56, so the strip needs naming the same way the two above do.
    "SPR_PET_MORPHOPUS": 66,
    # 568x64 = eight 71x64 frames, the widest Daemon cell on the line. The frame was
    # already 71 while the sheet was a single drawing — the cell is sized to the
    # creature (MASTER_TODO 2a-iii) — so animating it changed the width and nothing
    # else, and the row here is what stops the eight cells reading as one wide frame.
    "SPR_PET_SYNCAELIA": 71,
}

# The HEIGHT half of the table above, keyed the same way: asset name -> one row's height.
# See frame_rows(), which reads this exactly as frame_width() reads FRAME_W_OVERRIDES.
#
# It exists because the two halves of the grid are not symmetric by default. A sheet whose
# width is not a multiple of PET_FRAME_W falls back to ONE WIDE FRAME, which is the right
# guess for an oversized Daemon cell; a sheet whose height is not a multiple of PET_ROW_H
# falls back to ONE ROW, which is the right guess for exactly the same reason — and that is
# the problem, because a Daemon cell taller than 48 can then never hold a second row
# however many are drawn. So a Daemon is capped at one clip until it names its row height
# here, and `attack` or `hurt` on any of them starts with a row in this table.
ROW_H_OVERRIDES = {
    # 568x128 = two rows of eight 71x64 cells: the idle hover, and the strike.
    "SPR_PET_SYNCAELIA": 64,
}


# ---------------------------------------------------------------------------
#  Minimal PNG decoder (8-bit RGBA, non-interlaced, filter method 0)
# ---------------------------------------------------------------------------
def _paeth(a, b, c):
    p = a + b - c
    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    if pb <= pc:
        return b
    return c


def png_size(path):
    """Return (width, height) from a PNG's IHDR, without decoding pixels.

    Consumed by tools/gen_pedia_data.py, which needs each sheet's pixel size to
    describe the same frame grid to the web 'Pedia that frame_width()/frame_rows()
    cut for the firmware — but has no use for the pixels themselves.
    """
    with open(path, "rb") as f:
        head = f.read(24)
    if head[:8] != b"\x89PNG\r\n\x1a\n" or head[12:16] != b"IHDR":
        raise ValueError(f"{path}: not a PNG")
    return struct.unpack(">II", head[16:24])


def decode_png_rgba(path):
    """Return (width, height, bytearray of RGBA rows top-to-bottom)."""
    with open(path, "rb") as f:
        data = f.read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(f"{path}: not a PNG")
    pos = 8
    width = height = bitdepth = colortype = interlace = None
    idat = bytearray()
    while pos < len(data):
        (length,) = struct.unpack(">I", data[pos:pos + 4])
        ctype = data[pos + 4:pos + 8]
        chunk = data[pos + 8:pos + 8 + length]
        pos += 12 + length  # length + type + data + crc
        if ctype == b"IHDR":
            width, height, bitdepth, colortype, _comp, _filt, interlace = \
                struct.unpack(">IIBBBBB", chunk)
        elif ctype == b"IDAT":
            idat += chunk
        elif ctype == b"IEND":
            break
    if bitdepth != 8 or colortype != 6 or interlace != 0:
        raise ValueError(
            f"{path}: unsupported PNG (bd={bitdepth} ct={colortype} "
            f"interlace={interlace}); expected 8-bit RGBA non-interlaced")

    raw = zlib.decompress(bytes(idat))
    bpp = 4                       # RGBA
    stride = width * bpp
    out = bytearray(height * stride)
    prev = bytearray(stride)
    src = 0
    for y in range(height):
        ftype = raw[src]; src += 1
        line = bytearray(raw[src:src + stride]); src += stride
        if ftype == 0:
            pass
        elif ftype == 1:          # Sub
            for i in range(bpp, stride):
                line[i] = (line[i] + line[i - bpp]) & 0xFF
        elif ftype == 2:          # Up
            for i in range(stride):
                line[i] = (line[i] + prev[i]) & 0xFF
        elif ftype == 3:          # Average
            for i in range(stride):
                a = line[i - bpp] if i >= bpp else 0
                line[i] = (line[i] + ((a + prev[i]) >> 1)) & 0xFF
        elif ftype == 4:          # Paeth
            for i in range(stride):
                a = line[i - bpp] if i >= bpp else 0
                c = prev[i - bpp] if i >= bpp else 0
                line[i] = (line[i] + _paeth(a, prev[i], c)) & 0xFF
        else:
            raise ValueError(f"{path}: bad filter type {ftype}")
        out[y * stride:(y + 1) * stride] = line
        prev = line
    return width, height, out


# ---------------------------------------------------------------------------
#  Colour helpers
# ---------------------------------------------------------------------------
def rgb565(r, g, b):
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def luminance(r, g, b):
    """Perceptual luminance 0..1 (Rec.709 on sRGB 8-bit, simple form)."""
    return (0.2126 * r + 0.7152 * g + 0.0722 * b) / 255.0


def hex_to_rgb(h):
    h = h.lstrip("#")
    return int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16)


def ident(name):
    return re.sub(r"[^0-9a-zA-Z]", "_", name).upper()


# ---------------------------------------------------------------------------
#  Sprite generation
# ---------------------------------------------------------------------------
def frame_width(name, img_w):
    # An explicit override wins: it's the only way to declare a multi-frame strip
    # for a sheet that isn't a SPR_PET_ creature cell, since for everything else a
    # wider-than-tall image is normally one frame (BG_EGG_CLUTCH is 112x56 and must
    # stay a single 112-wide panel). Add a row here when a non-pet sheet animates.
    if name in FRAME_W_OVERRIDES:
        return FRAME_W_OVERRIDES[name]
    # Pet sprite SHEETS are horizontal strips of 56px frames. A pet sprite whose
    # width is NOT a clean multiple of 56 is a single oversized frame — a larger
    # Daemon cell (up to the 128x64 max sprite box; e.g. Cryptoad 80x52, the
    # generic Daemon placeholder 96x64) — so treat the whole width as one frame.
    if name.startswith(PET_PREFIX) and img_w % PET_FRAME_W == 0:
        return PET_FRAME_W
    return img_w


def frame_rows(name, img_h):
    # An explicit override wins, for the reason frame_width's does: it is the only way
    # to declare a row STACK on a sheet whose cell is not PET_ROW_H tall, which is every
    # oversized Daemon cell. Add a row to ROW_H_OVERRIDES when such a sheet grows a
    # second clip.
    if name in ROW_H_OVERRIDES:
        row_h = ROW_H_OVERRIDES[name]
        # Checked rather than floor-divided in silence: a stack that does not divide
        # evenly drops its last row, and a dropped row is invisible — every other row
        # still renders, and only the clip that named the missing one draws nothing.
        if img_h % row_h:
            raise SystemExit(
                f"{name}: height {img_h} is not whole rows of "
                f"{row_h} (ROW_H_OVERRIDES)")
        return img_h // row_h
    # A pet sheet's height that's a clean multiple of PET_ROW_H is a vertical
    # stack of that many rows; which row plays which named loop is declared on the
    # creature's own content row (CreatureDef::clips, core/content/defs.h);
    # anything else (single-row sheets, and oversized single-frame Daemon
    # cells like Cryptoad's 80x52) is one row.
    if name.startswith(PET_PREFIX) and img_h % PET_ROW_H == 0:
        return img_h // PET_ROW_H
    return 1


def facing(name):
    # The C++ enumerator for this sheet's declared facing; unlisted is the never-mirrored
    # default. See FACING above.
    return "Facing::" + FACING.get(name, "None")


def gen_sprites():
    # Everything asset_paths() finds, in one flat namespace. That includes the
    # SPR_PET_GENERIC_* stage stand-ins: a creature whose final art isn't drawn yet is
    # wired to one of those, so content never waits on art.
    decls, defs, table = [], [], []
    paths = asset_paths()
    # A FACING key that names nothing is a typo that would silently leave a creature
    # turned away from its opponent — the one failure this table exists to stop — and it
    # would never show up as an error anywhere else, so it is caught here.
    unknown = sorted(set(FACING) - set(paths))
    if unknown:
        raise ValueError("FACING names assets that do not exist: " + ", ".join(unknown))
    for name, rel in paths.items():
        path = os.path.join(REPO, rel)
        w, h, rgba = decode_png_rgba(path)
        fw = frame_width(name, w)
        if w % fw != 0:
            raise ValueError(f"{name}: width {w} not a multiple of frame {fw}")
        rows = frame_rows(name, h)
        if h % rows != 0:
            raise ValueError(f"{name}: height {h} not a multiple of {rows} rows")
        cell_h = h // rows
        sym = "ASSET_" + ident(name)
        rgb_vals, a_vals = [], []
        # The drawn band inside a frame cell, folded across every frame and row
        # (SpriteData::contentX0/contentX1) — measured here, in the pass that is already
        # touching every pixel, because it is a fact about the art and nothing at
        # runtime should be re-deriving it. A fully transparent sheet keeps the whole
        # frame, which is what the reader's fallback answers anyway.
        cx0, cx1 = fw, 0
        for i in range(0, len(rgba), 4):
            r, g, b, a = rgba[i], rgba[i + 1], rgba[i + 2], rgba[i + 3]
            rgb_vals.append(rgb565(r, g, b))
            a_vals.append(a)
            if a:
                x = (i // 4) % w % fw
                cx0, cx1 = min(cx0, x), max(cx1, x + 1)
        if cx1 <= cx0:
            cx0, cx1 = 0, fw
        decls.append(f"extern const SpriteData {sym};")
        ink = mask_ink(rgb_vals, a_vals)
        pal = None if ink is not None else sheet_palette(rgb_vals, a_vals)
        head = (sym, w, h, cell_h, fw, rows, cx0, cx1, facing(name))
        if ink is not None:
            defs.append(_emit_mask(sym, w, h, cell_h, fw, rows, cx0, cx1, facing(name),
                                   a_vals, ink))
        elif pal is not None:
            # Both palette forms cost the same pixels; take whichever stores them smaller.
            idx = pixel_indices(rgb_vals, a_vals, pal)
            tiled = best_tiling(w, h, idx, len(pal))
            flat_cost = ((w * index_bpp(len(pal)) + 7) // 8) * h
            if tiled and tiled[0] < flat_cost:
                defs.append(_emit_tiled(*head, pal, tiled))
            else:
                defs.append(_emit_indexed(*head, pal, idx))
        else:
            defs.append(_emit_sprite(sym, w, cell_h, fw, rows, cx0, cx1,
                                     facing(name), rgb_vals, a_vals))
        table.append((name, sym, w, h, fw, w // fw, rows))
    return decls, defs, table


def mask_ink(rgb_vals, a_vals):
    """The one colour this image is drawn in, or None if it needs full storage.

    An image qualifies as a 1-bit mask when it carries no information a bitmap would
    lose: every pixel is fully transparent or fully opaque, and every opaque pixel is
    the same colour. Most of the ICON_*/UI_* family is exactly that — a flat fill on
    transparent, because dim/bright is engine brightness and the colour is applied at
    draw time — so it spends RGB565 + alpha, 24 bits per pixel, to carry one.

    Detected rather than keyed off the name, so the emitted pixels are identical either
    way and an icon that ever gains a gradient silently falls back to full storage
    instead of being quietly flattened.
    """
    ink = None
    for rgb, a in zip(rgb_vals, a_vals):
        if a == 0:
            continue
        if a != 255:
            return None                  # a soft edge is real information
        if ink is None:
            ink = rgb
        elif rgb != ink:
            return None                  # more than one colour
    return ink                           # None for a fully transparent image: no ink


def sheet_palette(rgb_vals, a_vals):
    """This sheet's distinct (colour, coverage) pairs, most-used first, or None.

    The palette is DERIVED, never declared: whatever pairs the pixels actually hold are
    the palette, so a repainted or extended sheet needs no table kept in step with it and
    a new colour costs one more entry — and, only when the count crosses a power of two,
    one more bit per pixel. Ordering by frequency is what makes that cheap to look at: the
    tail of a creature's palette is where stray anti-aliasing and off-model pixels collect,
    and `--palettes` prints it.

    Every fully transparent pixel collapses to one entry regardless of the RGB under it, so
    a sheet's own background never widens the palette. None means more than 256 entries —
    an image an 8-bit index cannot address, which keeps full storage.
    """
    counts = {}
    for rgb, a in zip(rgb_vals, a_vals):
        key = (0, 0) if a == 0 else (rgb, a)
        counts[key] = counts.get(key, 0) + 1
    if len(counts) > 256:
        return None
    return [k for k, _ in sorted(counts.items(), key=lambda kv: (-kv[1], kv[0]))]


def index_bpp(n):
    """Bits needed to address `n` palette entries."""
    b = 1
    while (1 << b) < n:
        b += 1
    return b


def pixel_indices(rgb_vals, a_vals, pal):
    """The sheet as palette indices, row-major — what both palette forms actually store."""
    lut = {key: i for i, key in enumerate(pal)}
    return [lut[(0, 0) if a == 0 else (rgb, a)]
            for rgb, a in zip(rgb_vals, a_vals)]


def pack_bits(values, width, count):
    """`count` fields of `width` bits, MSB first, packed into whole bytes."""
    out = bytearray((count * width + 7) // 8)
    for i, v in enumerate(values):
        base = i * width
        for k in range(width):
            if v & (1 << (width - 1 - k)):
                out[(base + k) >> 3] |= 0x80 >> ((base + k) & 7)
    return out


def build_tiling(w, h, idx, edge):
    """-> (blob, tmap) for a grid of `edge`-square tiles, identical tiles stored once.

    A tile's first byte is the width IT needs, which is what lets the empty margin around
    a drawing — one palette entry all through — cost a byte however large it is. `tmap`
    holds byte offsets rather than tile ids, so two positions sharing pixels share an
    offset and there is no second table to walk.
    """
    cols, rows_ = (w + edge - 1) // edge, (h + edge - 1) // edge
    blob, tmap, seen = bytearray(), [], {}
    for ty in range(rows_):
        for tx in range(cols):
            cell = tuple(idx[y * w + x] if (x < w and y < h) else 0
                         for y in range(ty * edge, ty * edge + edge)
                         for x in range(tx * edge, tx * edge + edge))
            at = seen.get(cell)
            if at is None:
                at = seen[cell] = len(blob)
                tb = index_bpp(max(cell) + 1) if max(cell) else 0
                blob.append(tb)
                if tb:
                    blob += pack_bits(cell, tb, edge * edge)
            tmap.append(at)
    blob.append(0)                       # pad: spriteTileIndexAt reads a two-byte window
    return blob, tmap


def best_tiling(w, h, idx, entries):
    """-> (pixel bytes, edge, blob, tmap) for the cheapest tile size, or None.

    Tile size is a real per-sheet choice rather than one constant: resampled art with
    little repetition prefers a 4px tile, where the grid is fine enough to still find
    matches, while cleanly drawn animation prefers 8px, where each match is worth more.
    """
    best = None
    for edge in (4, 8, 16):
        blob, tmap = build_tiling(w, h, idx, edge)
        if len(blob) > 0xFFFF:           # a tmap entry is a uint16 byte offset
            continue
        cost = len(blob) + len(tmap) * 2
        if best is None or cost < best[0]:
            best = (cost, edge, blob, tmap)
    return best


def _emit_sprite(sym, w, cell_h, fw, rows, cx0, cx1, face, rgb_vals, a_vals):
    rgb_arr = ", ".join(f"0x{v:04x}" for v in rgb_vals)
    a_arr = ", ".join(str(v) for v in a_vals)
    return (
        f"static const uint16_t {sym}_rgb[] = {{{rgb_arr}}};\n"
        f"static const uint8_t {sym}_a[] = {{{a_arr}}};\n"
        f"const SpriteData {sym} = {{ {w}, {cell_h}, {fw}, {w // fw}, {rows}, "
        f"{cx0}, {cx1}, {face}, {sym}_rgb, {sym}_a }};\n"
    )


def _emit_mask(sym, w, h, cell_h, fw, rows, cx0, cx1, face, a_vals, ink):
    """Pack the alpha plane as 1bpp: row-major, MSB first, each row padded to a byte.

    Rows are byte-aligned so a row starts on a byte boundary and the reader's index
    math needs no carry across rows — see spriteAlphaAt in core/render/sprite.h, which
    is the only thing that unpacks this.
    """
    stride = (w + 7) // 8
    packed = bytearray(stride * h)
    for y in range(h):
        for x in range(w):
            if a_vals[y * w + x]:
                packed[y * stride + (x >> 3)] |= 0x80 >> (x & 7)
    bits_arr = ", ".join(str(b) for b in packed)
    return (
        f"static const uint8_t {sym}_bits[] = {{{bits_arr}}};\n"
        f"const SpriteData {sym} = {{ {w}, {cell_h}, {fw}, {w // fw}, {rows}, "
        f"{cx0}, {cx1}, {face}, nullptr, nullptr, {sym}_bits, 0x{ink:04x} }};\n"
    )


def _palette_arrays(sym, pal):
    return (f"static const uint16_t {sym}_pal[] = "
            f"{{{', '.join(f'0x{c:04x}' for c, _ in pal)}}};\n"
            f"static const uint8_t {sym}_palA[] = "
            f"{{{', '.join(str(a) for _, a in pal)}}};\n")


def _emit_indexed(sym, w, h, cell_h, fw, rows, cx0, cx1, face, pal, idx):
    """Pack the sheet as `bpp`-bit palette indices, row-major, MSB first.

    Rows are padded to a whole byte like the mask form, for the same reason — a row starts
    on a byte boundary, so the reader's index math needs no carry between rows. Within a
    row an index may straddle a byte, which is why one pad byte follows the array: it is
    what makes the two-byte window spriteIndexAt reads in-bounds on the last pixel.
    """
    bpp = index_bpp(len(pal))
    stride = (w * bpp + 7) // 8
    packed = bytearray()
    for y in range(h):
        packed += pack_bits(idx[y * w:(y + 1) * w], bpp, w)
    packed.append(0)
    return (
        f"static const uint8_t {sym}_bits[] = "
        f"{{{', '.join(str(b) for b in packed)}}};\n"
        + _palette_arrays(sym, pal) +
        f"const SpriteData {sym} = {{ {w}, {cell_h}, {fw}, {w // fw}, {rows}, "
        f"{cx0}, {cx1}, {face}, nullptr, nullptr, {sym}_bits, 0, "
        f"{sym}_pal, {sym}_palA, {bpp} }};\n"
    )


def _emit_tiled(sym, w, h, cell_h, fw, rows, cx0, cx1, face, pal, tiled):
    """Emit the tile blob and the grid's offset map. See build_tiling for the layout."""
    _cost, edge, blob, tmap = tiled
    shift = edge.bit_length() - 1
    return (
        f"static const uint8_t {sym}_tiles[] = "
        f"{{{', '.join(str(b) for b in blob)}}};\n"
        f"static const uint16_t {sym}_tmap[] = "
        f"{{{', '.join(str(o) for o in tmap)}}};\n"
        + _palette_arrays(sym, pal) +
        f"const SpriteData {sym} = {{ {w}, {cell_h}, {fw}, {w // fw}, {rows}, "
        f"{cx0}, {cx1}, {face}, nullptr, nullptr, nullptr, 0, "
        f"{sym}_pal, {sym}_palA, 0, {sym}_tiles, {sym}_tmap, {shift} }};\n"
    )


# ---------------------------------------------------------------------------
#  Palette generation (PAL_CORE.json -> role tokens)
# ---------------------------------------------------------------------------
def gen_palette():
    """-> (tokens, themes)

    tokens: [(token_name, base_hex)] in Pal-enum order, from the role groups.
    themes: [(theme_name, [hex per token])] — index 0 is always the base set, so
    a build with no "themes" block still emits a valid one-theme table.

    A theme names only the tokens it CHANGES; everything else falls through to
    base. That keeps a variant honest about what it is actually restating, and
    means a new token added to the base groups doesn't silently go missing from
    every variant.
    """
    with open(os.path.join(ASSETS_DIR, "PAL_CORE.json")) as f:
        pal = json.load(f)
    tokens = []  # (token_name, hex)
    for group, entries in pal.items():
        if group in ("_meta", "themes"):
            continue
        for key, val in entries.items():
            if key.startswith("_"):
                continue
            tokens.append((key, val["hex"]))

    themes = [("base", [h for _, h in tokens])]
    known = {name for name, _ in tokens}
    for theme, overrides in pal.get("themes", {}).items():
        if theme.startswith("_"):
            continue
        unknown = [k for k in overrides if not k.startswith("_") and k not in known]
        if unknown:
            raise SystemExit(f"PAL_CORE theme '{theme}' overrides unknown tokens: {unknown}")
        themes.append((theme, [overrides.get(name, hexv) for name, hexv in tokens]))
    return tokens, themes


def emit_palette_header(tokens, themes):
    lines = [
        "// AUTO-GENERATED by tools/gen_assets.py — do not edit.",
        "#pragma once",
        "#include <cstdint>",
        "",
        "namespace mal {",
        "",
        "// PAL_CORE role tokens. Bind every UI colour to one of these names, never to",
        "// a literal — that binding is what lets a whole theme be swapped underneath",
        "// the entire interface without touching a single drawing call.",
        "enum class Pal : uint8_t {",
    ]
    for name, _ in tokens:
        lines.append(f"    {ident(name)},")
    lines.append("    Count")
    lines.append("};")
    lines.append("")
    lines.append(f"constexpr int kPalThemeCount = {len(themes)};")
    lines.append("")
    lines.append("// Theme names, indexed as kPalRgb565's first subscript.")
    lines.append("inline const char* const kPalThemeNames[] = {")
    for name, _ in themes:
        lines.append(f'    "{name}",')
    lines.append("};")
    lines.append("")
    lines.append("// RGB565 per [theme][token]. Read it through palColor(), never directly:")
    lines.append("// that indirection is the theme switch.")
    lines.append("inline const uint16_t kPalRgb565[kPalThemeCount][static_cast<int>(Pal::Count)] = {")
    for theme, hexes in themes:
        lines.append(f"    {{  // {theme}")
        for (name, _), hexv in zip(tokens, hexes):
            r, g, b = hex_to_rgb(hexv)
            lines.append(f"        0x{rgb565(r, g, b):04x}, // {name} {hexv}")
        lines.append("    },")
    lines.append("};")
    lines.append("")
    lines.append("// Perceptual luminance 0..1 per [theme][token] (for the grayscale gate).")
    lines.append("inline const float kPalLum[kPalThemeCount][static_cast<int>(Pal::Count)] = {")
    for theme, hexes in themes:
        lines.append(f"    {{  // {theme}")
        for (name, _), hexv in zip(tokens, hexes):
            r, g, b = hex_to_rgb(hexv)
            lines.append(f"        {luminance(r, g, b):.4f}f, // {name}")
        lines.append("    },")
    lines.append("};")
    lines.append("")
    lines.append("} // namespace mal")
    lines.append("")
    return "\n".join(lines)


def emit_assets_header(decls, table):
    lines = [
        "// AUTO-GENERATED by tools/gen_assets.py — do not edit.",
        "#pragma once",
        '#include "core/render/sprite.h"',
        "",
        "namespace mal {",
        "",
    ]
    lines += decls
    lines.append("")
    lines.append("// Resolve a sprite by its asset name (the AssetSource seam: an")
    lines.append("// SD-backed source resolves the same names against card art later).")
    lines.append("struct AssetEntry { const char* name; const SpriteData* data; };")
    lines.append("inline const AssetEntry kAssetTable[] = {")
    for name, sym, *_ in table:
        lines.append(f'    {{"{name}", &{sym}}},')
    lines.append("};")
    lines.append(f"inline constexpr int kAssetCount = {len(table)};")
    lines.append("")
    lines.append("} // namespace mal")
    lines.append("")
    return "\n".join(lines)


def emit_assets_source(defs):
    lines = [
        "// AUTO-GENERATED by tools/gen_assets.py — do not edit.",
        '#include "generated/assets.h"',
        "",
        "namespace mal {",
        "",
    ]
    lines += defs
    lines.append("} // namespace mal")
    lines.append("")
    return "\n".join(lines)


def report_palettes():
    """Print every sheet's derived palette, biggest first — the survey behind a fold.

    A sheet's cost is `ceil(log2(entries))` bits per pixel, so what matters is not the
    palette's size but which side of a power of two it sits on, and that is decided by a
    tail of one- and two-pixel entries rather than by the colours the art is drawn in.
    The `drift` column is how many entries are worn by fewer than 50 pixels: fold them
    and the sheet may drop a bit. tools/snap_palette.py is what does the folding.
    """
    for name, rel in sorted(asset_paths().items()):
        w, h, rgba = decode_png_rgba(os.path.join(REPO, rel))
        rgb_vals = [rgb565(rgba[i], rgba[i + 1], rgba[i + 2]) for i in range(0, len(rgba), 4)]
        a_vals = [rgba[i + 3] for i in range(0, len(rgba), 4)]
        if mask_ink(rgb_vals, a_vals) is not None:
            continue                              # a mask has one colour and no tail
        counts = {}
        for rgb, a in zip(rgb_vals, a_vals):
            key = (0, 0) if a == 0 else (rgb, a)
            counts[key] = counts.get(key, 0) + 1
        n = len(counts)
        bpp = index_bpp(n) if n <= 256 else 24
        drift = sum(1 for c in counts.values() if c < 50)
        floor = index_bpp(max(1, n - drift)) if n <= 256 else 24
        print(f"{name:34} {w}x{h:<4} {n:4} entries  {bpp} bpp  "
              f"drift {drift:3}  -> {floor} bpp")
        for (c, a), cnt in sorted(counts.items(), key=lambda kv: -kv[1]):
            print(f"    0x{c:04x} a={a:<3} {cnt:6}")


def main():
    if "--palettes" in sys.argv[1:]:
        return report_palettes()
    os.makedirs(OUT_DIR, exist_ok=True)
    decls, defs, table = gen_sprites()
    tokens, themes = gen_palette()

    with open(os.path.join(OUT_DIR, "pal_core.h"), "w") as f:
        f.write(emit_palette_header(tokens, themes))
    with open(os.path.join(OUT_DIR, "assets.h"), "w") as f:
        f.write(emit_assets_header(decls, table))
    with open(os.path.join(OUT_DIR, "assets.cpp"), "w") as f:
        f.write(emit_assets_source(defs))

    print(f"gen_assets: {len(table)} sprites, {len(tokens)} palette tokens "
          f"x {len(themes)} theme(s) "
          f"-> {os.path.relpath(OUT_DIR, REPO)}/")
    for name, sym, w, h, fw, frames, rows in table:
        if frames > 1 or rows > 1:
            rowdesc = f", {rows} rows" if rows > 1 else ""
            print(f"  {name}: {w}x{h}  ({frames} frames of {fw}x{h // rows}{rowdesc})")


if __name__ == "__main__":
    sys.exit(main())
