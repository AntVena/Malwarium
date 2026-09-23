# assets/_attic — parked art

Drawings with no consumer yet. `tools/gen_assets.py` skips any path segment that starts with
`_`, so nothing here is compiled or costs flash, and `tools/check_orphan_assets.py` is what
sends an unconsumed PNG here instead of letting it ship.

**The folder is gitignored** (`/assets/_attic/` in `.gitignore`). Parking a drawing here keeps
it on the machine that drew it and nowhere else. To keep one for the next person, add it
past the ignore with `git add -f assets/_attic/<NAME>.png`; this README was added the same way.
An `⌫` row in `ASSET_MANIFEST.md` names the path a drawing was parked at, not a promise that
a fresh clone holds it.

A file here may share its basename with a live asset: it is the superseded drawing, kept for
reference. That is the one place two PNGs may share a name, because only one of them compiles.

## Bringing one back

1. Give it a consumer first: a content row that names it (`spriteName`, an achievement icon),
   an id it is derived from (`ICON_ITEM_<ID>` / `ICON_MOD_<ID>` / `ICON_MOVE_<ID>`), or an
   `ASSET_<NAME>` a screen draws.
2. Move it into the live tree (`icons/`, `sprites/` or `ui/`), renamed if its id changed.
3. `python3 tools/gen_assets.py && python3 tools/check_orphan_assets.py`, then flip its
   `ASSET_MANIFEST.md` row from `⌫` to `▨` or `☑` with the new path.
