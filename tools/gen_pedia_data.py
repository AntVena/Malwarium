#!/usr/bin/env python3
"""gen_pedia_data.py — regenerate web/data/pedia_data.js from the firmware content tables.

Sources:
  tools/dump_content.cpp (compiled)       -> creatures, egg lines, items, mods, moves
  src/core/model/combat.cpp               -> wildMalbeast() roster (plain regex)

The content tables arrive as JSON from the firmware's OWN code rather than being
re-parsed here, because a regex over an initializer list has to index each row's fields
by position — so reordering a struct field silently changes what the site publishes —
and because descriptions are templates the firmware expands from each row's magnitudes
(core/content/effect_text.h). Running the device's expander is the only way the site's
numbers cannot disagree with the device's. `make pedia` compiles the tool first; point
--dump-content elsewhere to use a build you already have.

Creature snarky hints + infosec context ride along on each CreatureDef row
(content_creatures.cpp owns them — the game is the single source of truth for its own
flavour text, the same way ItemDef carries `effect`). Do NOT re-key copy here.

Also COPIES every asset the generated data references into the served bundle
(web/assets/), so the site can never reference art the card doesn't carry. Pass
--no-sync-assets to write only the data file.

Usage (from the repo root):
  python3 tools/gen_pedia_data.py [--repo .] [--out web/data/pedia_data.js]

Add a creature/item/move to its content_*.cpp table, run `make pedia` (which rebuilds
the dump tool first), done — the 'Pedia stays in sync. If a creature row has no hint the
script emits a TODO hint and warns.
"""
import argparse, filecmp, json, re, shutil, subprocess, sys
from pathlib import Path

# The frame grid a sprite sheet is cut on is gen_assets.py's fact — it is what the
# firmware renders — so read it from there rather than restating it here.
sys.path.insert(0, str(Path(__file__).resolve().parent))
import gen_assets

STAGE = {"BootSector": (1, "Boot Sector"), "Process": (2, "Process"),
         "Script": (3, "Script"), "Daemon": (4, "Daemon")}

_ATLAS = {}


def asset_path(repo, name):
    """Where `name`.png lives in the bundle, or None if nothing compiles it.

    Which PNGs exist and where they sit is gen_assets.py's fact — assets/ is filed
    into subfolders for readability while an asset's id stays its basename — so the
    site asks rather than composing a path. That is also what stops the two from
    drifting: the site can only ever show art the firmware carries.
    """
    key = str(repo)
    if key not in _ATLAS:
        _ATLAS[key] = gen_assets.asset_paths(repo)
    return _ATLAS[key].get(name)


def sprite_geometry(repo, name, warnings):
    """Locate `name`.png and describe its frame grid for web/app.js.

    Returns the bundle-relative path plus the CELL size (one frame — what the site
    shows) and the SHEET size (the whole image — what the site scales the background
    to). Both are needed: a sheet is a grid of frames, so sizing the background by
    the cell alone squashes the whole strip into one frame's box.
    """
    rel = asset_path(repo, name)
    if rel:
        w, h = gen_assets.png_size(repo / rel)
        fw = gen_assets.frame_width(name, w)
        rows = gen_assets.frame_rows(name, h)
        return {"sprite": rel,
                "cellW": fw, "cellH": h // rows, "sheetW": w, "sheetH": h}
    warnings.append(f"sprite '{name}' has no PNG anywhere under assets/")
    return {"sprite": f"assets/sprites/{name}.png", "cellW": 56, "cellH": 48,
            "sheetW": 56, "sheetH": 48}


def sync_assets(repo, data, dest, warnings):
    """Copy every asset the generated data references into the served bundle.

    The site is served from a card that carries only what was staged onto it, so an
    asset that exists in the repo but not in `dest` renders as nothing. Driving the
    copy off the reference set is what keeps the two from drifting: an art path that
    appears in the data is, by construction, in the bundle.

    The sweep runs both ways. A sprite that gets renamed or retired stops being
    referenced but doesn't stop EXISTING, so without the prune below the bundle only
    ever grows, quietly carrying art nothing draws onto a card with finite room. Only
    .png files are pruned — PAL_CORE.json and anything else hand-staged in the bundle
    isn't reference-driven and isn't ours to remove.
    """
    refs = {c["sprite"] for c in data["creatures"]}
    refs |= {b["sprite"] for b in data["malbeasts"]}
    refs |= set(data["meta"]["lineIcons"].values())
    refs.add(data["meta"]["lockIcon"])
    for group in ("items", "mods", "moves", "achievements"):
        refs |= {e["icon"] for e in data[group]}
    copied = 0
    for rel in sorted(refs):
        src, dst = repo / rel, dest / rel
        if not src.exists():
            warnings.append(f"referenced asset '{rel}' does not exist in the repo")
            continue
        if dst.exists() and filecmp.cmp(src, dst, shallow=False):
            continue
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(src, dst)
        copied += 1

    kept = {(dest / rel).resolve() for rel in refs}
    pruned = 0
    for stale in sorted((dest / "assets").rglob("*.png")):
        if stale.resolve() in kept:
            continue
        stale.unlink()
        pruned += 1
    return len(refs), copied, pruned

# The firmware's itemIcon() (src/core/ui/items_screen.cpp) resolves an item's glyph by
# CONVENTION — ICON_ITEM_<UPPER ID> — and only names an item explicitly where it borrows
# another item's art. So the site derives the same way (item_icon below) and mirrors only
# those borrowings: a new item needs a row here ONLY if it reuses someone else's glyph.
ITEM_ICON_REUSE = {  # id -> the asset it borrows, named not pathed (see asset_path)
    "disk_scrubber": "ICON_MAINT_DEFRAG",
    "restore_point": "ICON_ITEM_YUBI_COOKIE",
    "ambig_usb": "ICON_ITEM_ACCESS_TOKEN",
    "deep_learning_module": "ICON_ITEM_BACKUP_DRIVE",
    "deep_learning_core": "ICON_ITEM_BACKUP_DRIVE",
    "backdoor_bell": "ICON_ITEM_BACKUP_DRIVE",
    "rootkit_bell": "ICON_ITEM_BACKUP_DRIVE",
    "kernel_bell": "ICON_ITEM_BACKUP_DRIVE",
    "zeroday_bell": "ICON_ITEM_BACKUP_DRIVE",
}
ITEM_ICON_GENERIC = "ICON_ITEMS"


def item_icon(repo, item_id):
    """The glyph the device would draw for `item_id`, or None if it has no art.

    Mirrors itemIcon(): the naming convention first, then the explicit reuses, then
    nothing — a None here is a real gap in assets/, not a missing table row.
    """
    return (asset_path(repo, f"ICON_ITEM_{item_id.upper()}")
            or asset_path(repo, ITEM_ICON_REUSE.get(item_id, "")))
MOD_ICONS = {
    "clock_speed_boost": "ICON_MOD_CLOCK_SPEED_BOOST",
    "packet_sniffer": "ICON_MOD_PACKET_SNIFFER",
    "firewall_patch": "ICON_MOD_FIREWALL_PATCH",
    "raid_mirror": "ICON_MOD_RAID_MIRROR",
}
MOVE_ICONS = {
    "packet_storm": "ICON_MOVE_PACKET_STORM",
    "fork_bomb": "ICON_MOVE_FORK_BOMB",
    "checksum_guard": "ICON_MOVE_CHECKSUM_GUARD",
}
MOD_TIERS = {1: "Citrus Circuit", 2: "The Pirate Bayou", 3: "Napstorrent Moors",
             4: "Castle Rapidscare / DeepWeb Dive"}
CONTEXT_USE = {"LockoutOnly": "LOCKOUT ONLY", "PreEncounter": "PRE-ENCOUNTER"}
MOVE_GROUPS = {
    "core": "CORE MOVES", "ransomware": "RANSOMWARE LINE",
    "phishing": "PHISHING LINE — dormant", "threat": "THREAT SIGNATURES — wielded against you",
}
THREAT_MOVES = {"system_hang", "data_rot", "nag_screen"}

def strip_comments(src):
    src = re.sub(r"/\*.*?\*/", "", src, flags=re.S)
    return re.sub(r"//[^\n]*", "", src)


def load_content(tool):
    """Run the compiled dump tool and return its JSON (the firmware's own view)."""
    if not Path(tool).exists():
        sys.exit(f"error: {tool} not found — build it first:\n"
                 f"  make pedia   (or see the compile line in the repo-root Makefile)")
    out = subprocess.run([str(tool)], capture_output=True, text=True, check=True).stdout
    return json.loads(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", default=".", type=Path)
    ap.add_argument("--out", default="web/data/pedia_data.js", type=Path)
    ap.add_argument("--bundle", default="web", type=Path,
                    help="served bundle root that referenced assets are copied into")
    ap.add_argument("--no-sync-assets", dest="sync_assets", action="store_false",
                    help="write the data file only, leaving the bundle's assets alone")
    ap.add_argument("--dump-content", default="build/dump_content", type=Path,
                    help="the compiled tools/dump_content.cpp that publishes the tables")
    args = ap.parse_args()
    repo = args.repo

    content = load_content(args.dump_content)
    warnings = []

    # ---- creatures ----
    creatures = []
    for c in content["creatures"]:
        stnum, stname = STAGE.get(c["stage"], (0, c["stage"]))
        hint, context = c["hint"], c["context"]
        if not hint:
            warnings.append(f"creature '{c['id']}' has no hint field — add one to content_creatures.cpp")
            hint, context = "TODO: add a hint to this creature's row.", context or "TODO"
        # A branch node names a Good AND a Bad successor; a linear hop names one.
        linear, good, bad = c["evolvesToId"], c["evolvesToGoodId"], c["evolvesToBadId"]
        entry = {
            "id": c["id"], "name": c["name"], "stage": stnum, "stageName": stname,
            "line": c["line"],
            "evolvesTo": [x for x in ([linear] if linear else [good, bad]) if x],
            "hint": hint, "context": context,
        }
        entry.update(sprite_geometry(repo, c["sprite"], warnings))
        if good and bad:
            entry["branchSplit"] = True
        if c["id"].endswith("_good"):
            entry["branch"] = "good"
        if c["id"].endswith("_bad"):
            entry["branch"] = "bad"
        creatures.append(entry)

    # ---- creature lines ----
    # kEggLines names the lines you can HATCH into, which is not every line the
    # roster has: the Trojan family is reached by cross-line divert and has no egg,
    # so a line-keyed site section built from eggs alone would drop its creatures
    # entirely. Seed from the egg table (it owns the display names), then add any
    # line a creature row claims. An icon is emitted only where the art exists.
    lines = {e["id"]: e["name"] for e in content["eggLines"]}
    for c in creatures:
        if c["line"] and c["line"] not in lines:
            lines[c["line"]] = c["line"].capitalize()
    line_icons = {}
    for lid in lines:
        name = f"ICON_LINE_{lid.upper()}"
        icon = asset_path(repo, name)
        if icon:
            line_icons[lid] = icon
        else:
            warnings.append(f"line '{lid}' has no {name} — its section renders text-only")

    # ---- items ----
    # `effect` and `stats` arrive already rendered by the firmware's own expander, so
    # the site shows the exact two strings the device's ITEMS detail screen draws.
    items = []
    for d in content["items"]:
        icon = item_icon(repo, d["id"])
        it = {"id": d["id"], "name": d["name"], "type": d["type"], "rarity": d["rarity"],
              "effect": d["effect"], "stats": d["stats"],
              "icon": icon or asset_path(repo, ITEM_ICON_GENERIC)}
        if icon is None:
            warnings.append(f"item '{d['id']}' has no glyph in assets/ — using generic ICON_ITEMS")
        if d["context"] in CONTEXT_USE:
            it["use"] = CONTEXT_USE[d["context"]]
        if d["use"] == "DecryptEgg":
            it["use"] = "EGG PHASE"
        if d["bits"]:
            it["bits"] = d["bits"]
        items.append(it)

    # ---- mods ----
    mods = []
    for d in content["mods"]:
        m = {"id": d["id"], "name": d["name"], "tag": d["tag"], "rarity": d["rarity"],
             "tier": d["tier"], "effect": d["effect"], "stats": d["stats"],
             "icon": asset_path(repo, MOD_ICONS.get(d["id"], "ICON_MODS_SLOT"))}
        if d["id"] not in MOD_ICONS:
            m["iconFallback"] = True
        if d["oneShot"]:
            m["oneShot"] = True
        if d["line"]:
            m["line"] = d["line"]
        if d["requiresLine"]:
            m["requiresLine"] = d["requiresLine"]
        mods.append(m)

    # ---- moves ----
    moves = []
    for d in content["moves"]:
        group = "threat" if d["id"] in THREAT_MOVES else (d["line"] or "core")
        icon = asset_path(repo, MOVE_ICONS.get(
            d["id"], "ICON_MOVE_SLOT_LOCKED" if group in ("phishing", "threat")
            else "ICON_MOVE_SLOT"))
        mv = {"id": d["id"], "name": d["name"], "kind": d["kind"], "power": d["power"],
              "turns": d["turns"], "minStage": STAGE.get(d["minStage"], (0, d["minStage"]))[1],
              "desc": d["effect"], "stats": d["stats"], "group": group, "icon": icon}
        if d["id"] == "quick_jab":
            mv["innate"] = True
        moves.append(mv)

    # ---- wild malbeasts (combat.cpp) ----
    combat = strip_comments((repo / "src/core/model/combat.cpp").read_text(encoding="utf-8", errors="replace"))
    fn = combat[combat.index("CombatEnemy wildMalbeast"):]
    fn = fn[: fn.index("\n}") + 2]
    sectors = {1: "Citrus Circuit", 2: "The Pirate Bayou", 3: "Napstorrent Moors"}
    beasts = []
    for m in re.finditer(r'\{"([^"]+)",\s*"([^"]+)",\s*(\d+),\s*(\d+),\s*(\d+),\s*\{([^}]*)\}', fn):
        name, _sprite, tier, hp, pow_ = m.group(1), m.group(2), int(m.group(3)), int(m.group(4)), int(m.group(5))
        mv = [x.strip().strip('"') for x in m.group(6).split(",") if x.strip()]
        bid = re.sub(r"[^a-z0-9]+", "_", name.lower()).strip("_")
        beast = {"id": bid, "name": name, "tier": tier, "sector": sectors.get(tier, "?"),
                 "hp": hp, "pow": pow_, "moves": mv}
        beast.update(sprite_geometry(repo, "SPR_MALBEAST_" + bid.upper(), warnings))
        beasts.append(beast)

    # ---- achievements ----
    # Straight through from the firmware's own table (content_achievements.cpp), trigger
    # prose already expanded by the device's own expander — so adding an achievement is a
    # C++ row and nothing here. `hidden` rows keep their masked trigger text.
    achievements = []
    for a in content["achievements"]:
        icon = asset_path(repo, a["icon"])
        if not icon:
            warnings.append(f"achievement '{a['key']}' names icon '{a['icon']}', "
                            "which no PNG under assets/ provides")
        row = {"key": a["key"], "name": a["name"], "trigger": a["trigger"],
               "icon": icon}
        if a.get("goal"):
            row["goal"] = a["goal"]
        if a.get("hidden"):
            row["hidden"] = True
        achievements.append(row)

    data = {
        "meta": {"generated_from": "tools/dump_content.cpp", "site_version": "v1",
                 "lines": lines, "lineIcons": line_icons,
                 # The padlock every not-yet-unlocked row draws instead of its own
                 # glyph. Published rather than composed in web/app.js, so where the
                 # PNG lives stays one fact (asset_path) and sync_assets sees it as a
                 # reference — otherwise the bundle only carries it by the accident
                 # of some achievement row happening to use the same glyph.
                 "lockIcon": asset_path(repo, "ICON_LOCK")},
        "creatures": creatures,
        "malbeasts": beasts,
        "items": items,
        "mods": mods,
        "modTiers": {str(k): v for k, v in MOD_TIERS.items()},
        "moves": moves,
        "moveGroups": MOVE_GROUPS,
        "achievements": achievements,
    }

    out = args.out
    out.parent.mkdir(parents=True, exist_ok=True)
    js = ("// pedia_data.js — GENERATED by tools/gen_pedia_data.py — DO NOT hand-edit.\n"
          "window.PEDIA_DATA = " + json.dumps(data, indent=2, ensure_ascii=False) + ";\n")
    out.write_text(js, encoding="utf-8")

    print(f"wrote {out}  ({len(creatures)} creatures, {len(beasts)} malbeasts, "
          f"{len(items)} items, {len(mods)} mods, {len(moves)} moves)")

    if args.sync_assets:
        total, copied, pruned = sync_assets(repo, data, args.bundle, warnings)
        print(f"synced {args.bundle}/assets  ({total} referenced, {copied} updated, "
              f"{pruned} pruned)")

    for w in warnings:
        print("  WARN:", w, file=sys.stderr)


if __name__ == "__main__":
    main()
