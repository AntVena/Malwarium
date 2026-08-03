#!/usr/bin/env python3
"""Fail if any compiled asset has nothing that can draw it.

assets/ IS the atlas (tools/gen_assets.py), so every PNG under it is
RGB565 data linked into the firmware whether or not a single line of code names it.
Art drawn ahead of the content that would use it therefore costs flash silently and
forever. This is the guard: run it and the tree can't quietly refill with sprites for
creatures nobody added.

    python3 tools/check_orphan_assets.py            # report + non-zero on orphans
    python3 tools/check_orphan_assets.py --list     # just print the orphan names

Park an orphan in assets/_attic/ (untracked, not compiled — see its README), or give
it a consumer. An asset can be consumed three ways, all of which count here:

  1. Named literally in source, as `ASSET_<NAME>` or as a "<NAME>" string on a content
     row (a CreatureDef spriteName, a CombatEnemy sprite, an achievement icon path).
  2. Built at draw time from a content id — items_screen/mods_screen/train_screen
     format `ICON_ITEM_`/`ICON_MOD_`/`ICON_MOVE_` + the id, uppercased. Those never
     appear literally anywhere, so they're resolved here against the actual ids.
  3. Listed in KEEP below, for art that is deliberately compiled without a caller.

Content ids come from web/data/pedia_data.js rather than a build, so this runs on a
clean checkout. `make pedia-check` is what keeps that file honest.
"""

import argparse
import json
import os
import re
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Which PNGs are compiled is gen_assets.py's fact, not one to restate here.
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gen_assets

# Compiled deliberately, with no caller — do not report these.
KEEP = {
    # The stand-in a creature is wired to while its real art doesn't exist yet. One
    # per lifecycle stage, kept as a complete set: the stage that happens to have no
    # undrawn creature TODAY is the one that will need it for the next line, and the
    # set is what lets content land before art.
    "SPR_PET_GENERIC_EGG",
    "SPR_PET_GENERIC_PROCESS",
    "SPR_PET_GENERIC_SCRIPT",
    "SPR_PET_GENERIC_DAEMON",
    # Drawn art whose real consumer is not built. Each was borrowed by an achievement
    # row until those got bespoke glyphs, which is what surfaced them: an achievement
    # pointing at ICON_SECTOR_0 was never the sector icon being USED, it was a
    # placeholder standing on top of art nobody had wired. Parking them in _attic/
    # would drop them from version control (it is gitignored), and each is finished
    # art for a feature that is coming, so they stay compiled — about 1.2 KB total.
    "ICON_LINE_WORM",           # the Worm line has no content rows yet
    "ICON_SECTOR_0",            # AREA_CONTENT_STANDARD.md asks for it; no screen reads it
}

# Directories holding hand-written code. src/generated/ is excluded on purpose: it's
# built FROM assets/, so it names every asset and would call all of them used.
SOURCE_DIRS = ["src/core", "src/platform", "tools", "include", "test", "web"]

ASSET_TOKEN = r"(ASSET_)?(SPR|ICON|UI|BG|FX|CAP|ANIM|FONT|PAL)_[A-Z0-9_]+"

# Draw-time name construction: prefix + uppercased content id.
DYNAMIC = (("items", "ICON_ITEM_"), ("mods", "ICON_MOD_"), ("moves", "ICON_MOVE_"))


def compiled_assets():
    """Exactly what gets compiled, asked of the tool that compiles it."""
    return list(gen_assets.asset_paths())


def referenced_assets():
    names = set()

    out = subprocess.run(
        ["grep", "-rhoE", ASSET_TOKEN]
        + [d for d in SOURCE_DIRS if os.path.isdir(os.path.join(REPO, d))],
        cwd=REPO, capture_output=True, text=True,
    ).stdout
    for tok in out.split():
        names.add(tok[len("ASSET_"):] if tok.startswith("ASSET_") else tok)

    data = pedia_data()
    for key, prefix in DYNAMIC:
        for row in data.get(key, []):
            names.add(prefix + row["id"].upper())
    return names


def pedia_data():
    path = os.path.join(REPO, "web", "data", "pedia_data.js")
    with open(path, encoding="utf-8") as fh:
        text = fh.read()
    body = re.sub(r"^.*?window\.PEDIA_DATA\s*=\s*", "", text, count=1, flags=re.S)
    return json.loads(body.rstrip().rstrip(";"))


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--list", action="store_true",
                    help="print orphan names only, one per line")
    args = ap.parse_args()

    compiled = compiled_assets()
    refs = referenced_assets()
    orphans = [a for a in compiled if a not in refs and a not in KEEP]

    if args.list:
        print("\n".join(orphans))
        return 1 if orphans else 0

    if not orphans:
        print(f"no orphan assets ({len(compiled)} compiled, {len(KEEP)} kept by name)")
        return 0

    print(f"{len(orphans)} compiled asset(s) have no consumer, costing flash:\n")
    for a in orphans:
        print(f"  {a}")
    print("\nGive each one a consumer, or move it to assets/_attic/ "
          "(untracked, not compiled).")
    return 1


if __name__ == "__main__":
    sys.exit(main())
