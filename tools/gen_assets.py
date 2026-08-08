#!/usr/bin/env python3
"""
gen_assets.py  —  Malwarium build-time asset + palette codegen.

Converts the shippable PNGs in assets/ into RGB565 + alpha C++ arrays, and
PAL_CORE.json into a role-token palette table, written to src/generated/.

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
    # A pet sheet's height that's a clean multiple of PET_ROW_H is a vertical
    # stack of that many rows; which row plays which named loop is declared on the
    # creature's own content row (CreatureDef::clips, core/content/defs.h);
    # anything else (single-row sheets, and oversized single-frame Daemon
    # cells like Cryptoad's 80x52) is one row.
    if name.startswith(PET_PREFIX) and img_h % PET_ROW_H == 0:
        return img_h // PET_ROW_H
    return 1


def gen_sprites():
    # Everything asset_paths() finds, in one flat namespace. That includes the
    # SPR_PET_GENERIC_* stage stand-ins: a creature whose final art isn't drawn yet is
    # wired to one of those, so content never waits on art.
    decls, defs, table = [], [], []
    for name, rel in asset_paths().items():
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
        for i in range(0, len(rgba), 4):
            r, g, b, a = rgba[i], rgba[i + 1], rgba[i + 2], rgba[i + 3]
            rgb_vals.append(rgb565(r, g, b))
            a_vals.append(a)
        decls.append(f"extern const SpriteData {sym};")
        ink = mask_ink(rgb_vals, a_vals)
        if ink is None:
            defs.append(_emit_sprite(sym, w, cell_h, fw, rows, rgb_vals, a_vals))
        else:
            defs.append(_emit_mask(sym, w, h, cell_h, fw, rows, a_vals, ink))
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


def _emit_sprite(sym, w, cell_h, fw, rows, rgb_vals, a_vals):
    rgb_arr = ", ".join(f"0x{v:04x}" for v in rgb_vals)
    a_arr = ", ".join(str(v) for v in a_vals)
    return (
        f"static const uint16_t {sym}_rgb[] = {{{rgb_arr}}};\n"
        f"static const uint8_t {sym}_a[] = {{{a_arr}}};\n"
        f"const SpriteData {sym} = {{ {w}, {cell_h}, {fw}, {w // fw}, {rows}, "
        f"{sym}_rgb, {sym}_a }};\n"
    )


def _emit_mask(sym, w, h, cell_h, fw, rows, a_vals, ink):
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
        f"nullptr, nullptr, {sym}_bits, 0x{ink:04x} }};\n"
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


def main():
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
