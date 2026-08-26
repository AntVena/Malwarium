#!/usr/bin/env python3
"""
gen_ach_icons.py — the achievement board's 1-bit glyphs, and the grammar they share.

An AUTHORING step, not a build step, and it owns the PNGs it emits: the committed files
under assets/icons/ are what the web 'Pedia serves, and this is what they are
regenerated FROM. Sibling to gen_item_icons.py, same shape:

    python3 tools/gen_ach_icons.py                    # write every glyph
    python3 tools/gen_ach_icons.py --check            # committed art still matches?
    python3 tools/gen_ach_icons.py --preview DOCK_5   # ASCII one glyph to the terminal

Why a tool at all
-----------------
The 'Pedia draws achievements as a GRID, and a grid only reads if the rows share a
grammar. assets/ASSET_MANIFEST.md (§875) states it: one bespoke glyph per row, no
reuse, and most rows are LADDERS — one motif plus a countable tally, so a reader sees
which rung a row is before they read anything. That is a rule about the SET, which is
exactly the kind of rule hand-drawing drifts away from one file at a time.

So the grammar lives here as code rather than as a convention people remember:

  * 20x20, one ink, full alpha or none — pure white on transparent, tinted at draw
    time. No greys: they survive neither the tint nor the x1.75 upscale.
  * strokes are 2px, for the same reason the pantry's are — a 1px line disappears.
  * the MOTIF occupies rows 0..13. Rows 14..19 belong to the footer, and a motif that
    reaches into them is what makes a tally stop reading as a count.
  * the FOOTER is one of three, and it is what the row's own kind decides:
      tally(n)  a ladder rung — n marks, 2px wide, pitch 4, centred (start x = 11-2n)
      bar()     "all of them" — a full-width rule under the motif
      chevron() "took one deep" — a narrowing wedge
    Those three are measured off the shipped art (BOSS_FIRST..BOSS_100 for the tally,
    FULL_LINE_TROJAN for the bar, DEEP_LINE_TROJAN for the chevron), so a glyph made
    here lands in the same grid as one drawn before this file existed.

A MOTIF is authored as ASCII below, which is the only honest way to hand-place pixels
at this size: what you read in the source is what the device draws. A motif is shared
by every row on its ladder — that is the point of a ladder — and the footer is what
tells the rungs apart.
"""

import argparse
import os
import sys

try:
    from PIL import Image
except ImportError:
    sys.stderr.write("gen_ach_icons.py needs Pillow: pip install pillow\n")
    raise SystemExit(2)

SIZE = 20
MOTIF_ROWS = 14          # rows 0..13; 14..19 are the footer's
INK = (255, 255, 255, 255)
BARE = (255, 255, 255, 0)

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ICONS = os.path.join(REPO, "assets", "icons")


# --- The three footers -------------------------------------------------------
# Measured off the shipped family rather than invented, so a new rung sits in the
# same grid as the ones beside it. Each returns rows keyed by y.

def tally(n):
    """A ladder rung: `n` marks, 2px wide, pitch 4, centred. Rows 15..17."""
    rows = {}
    start = 11 - 2 * n           # BOSS_FIRST..BOSS_100 solve to exactly this
    if start < 0:
        raise ValueError("tally(%d) runs off the cell" % n)
    for y in (15, 16, 17):
        line = [" "] * SIZE
        for i in range(n):
            x = start + 4 * i
            line[x] = line[x + 1] = "#"
        rows[y] = "".join(line)
    return rows


def bar():
    """"All of them": a full-width rule. Rows 16..17, x 2..17 (FULL_LINE_TROJAN)."""
    line = "".join("#" if 2 <= x <= 17 else " " for x in range(SIZE))
    return {16: line, 17: line}


def chevron():
    """"Took one deep": a narrowing wedge. Rows 15..17 (DEEP_LINE_TROJAN)."""
    rows = {}
    for i, y in enumerate((15, 16, 17)):
        lo, hi = 2 + i, 15 - i
        rows[y] = "".join("#" if lo <= x <= hi else " " for x in range(SIZE))
    return rows


def plain():
    """No footer — a one-off Event row, which has no rung to be on."""
    return {}


# --- Inheriting a motif ------------------------------------------------------

def inherit(icon_name):
    """The motif rows (0..13) of an already-shipped glyph.

    A new rung on an EXISTING ladder must carry that ladder's own motif, and copying
    the pixels is the only way to guarantee it still does after someone retouches the
    original. Returns a marker the builder resolves, so the read happens at render
    time rather than at import time.
    """
    return ("@inherit", icon_name)


def from_item(item_id, drop_rows=()):
    """A dish's own inventory glyph (ICON_ITEM_<ID>), centred in the motif band.

    A "cook this one dish" row is about a specific plate, and the player already knows
    that plate by sight off the ITEMS list — so the honest picture is the one they have
    been looking at, read here rather than redrawn. Same reason inherit() exists: two
    hand-drawn pictures of one thing drift, and a copy that is taken at render time
    cannot.

    The only work is the fit. Item glyphs are composed in a 20x20 cell; a motif gets
    rows 0..13, so the content box is centred VERTICALLY in the band and x is left
    exactly as the item drew it (the item is already composed across the full width,
    and re-centring an odd-width glyph would knock a symmetric one off its axis).

    A dish taller than the band names the SOURCE rows to drop. Always a REPEAT — one
    more rib, one more inch of stem — never a feature, because a shortened rack still
    reads as a rack and a rack missing its top does not.
    """
    return ("@item", item_id, tuple(drop_rows))


# --- The motifs --------------------------------------------------------------
# Rows 0..13. Authored as ASCII: '#' is ink, anything else is bare.

MOTIFS = {}

# ROCK THE DOCK — a deck on pilings under a mooring bollard. The arena's identity is
# the DOCK, and the tally carries the rung, which is the division of labour every
# ladder here uses.
MOTIFS["dock"] = """
.......####.........
.......####.........
......######........
..################..
..################..
..################..
....##........##....
....##........##....
....##........##....
....##........##....
....##........##....
....##........##....
....##........##....
....................
"""

# UNDERDOG — the short fighter, marked, beside the tall one it put out. The mark is
# what makes it a result rather than two bars: without it this is a chart.
MOTIFS["underdog"] = """
....................
....##..............
...####.............
....##..............
..........########..
..........########..
..........########..
..........########..
..######..########..
..######..########..
..######..########..
..######..########..
..######..########..
..######..########..
"""

# PUNCHING UP — an arrow through a broken ceiling. The break is the point: the bar is
# the Daemon band the row is about arriving without.
MOTIFS["punchup"] = """
.####...####...####.
.####...####...####.
....................
.........##.........
........####........
.......######.......
......########......
.....##########.....
....############....
........####........
........####........
........####........
........####........
........####........
"""

# ENLISTED — a shield cleaved down the middle. Two sides, and the whole of enlisting is
# picking one. Reads as the CREW slot's shield (ICON_CREW) at a fifth of the cell.
MOTIFS["enlisted"] = """
....................
..#######..#######..
..#######..#######..
..#######..#######..
..#######..#######..
..#######..#######..
..#######..#######..
...######..######...
....#####..#####....
.....####..####.....
......###..###......
.......##..##.......
........####........
........####........
"""

# The Metamorphic line's own mark, the bell — the same mark NEVER_SEEN and DEEP_COVER
# carry (§875: "both the line's bell"). Solid here: the hollow one is spoken for, it
# means the hatch run that was never spotted.
MOTIFS["bell"] = """
....................
....................
........####........
......########......
.....##########.....
....############....
....############....
...##############...
...##############...
..################..
..################..
...##..##..##..##...
...##..##..##..##...
....................
"""


# DUEL WON — one fighter standing, one toppled. The FIRST_DUEL glyph says a duel was
# FOUGHT (two packets crossing); this ladder counts the ones that ended, so what it
# draws is the ending rather than the exchange.
MOTIFS["duel_won"] = """
....................
....................
..######............
..######............
..######............
..######............
..######............
..######............
..######............
..######............
..######............
..######..##########
..######..##########
....................
"""

# THE STOVE — a pan over flame. Deliberately not the CUISINE bowl: that ladder counts
# dishes HELD, and this one counts dishes cooked, which is a stove and not a table.
MOTIFS["stove"] = """
....................
....................
.................##.
..#################.
..#################.
..###############...
...#############....
....###########.....
....................
......##......##....
.....####....####...
.....####....####...
......##......##....
....................
"""

# THE QUOTE BOARD — a framed grid with cells open and closed, which is what a
# Decryptogram mid-solve actually looks like.
MOTIFS["cipher"] = """
....................
..################..
..##............##..
..##..####..##..##..
..##..####..##..##..
..##............##..
..##..##..####..##..
..##..##..####..##..
..##............##..
..##..####..##..##..
..##..####..##..##..
..##............##..
..################..
....................
"""

# PEERS — a bus with operators hanging off it, the same read ICON_PEERS uses for the
# slot: the roster of who this device has heard, not a crowd of figures.
MOTIFS["peers"] = """
....................
....................
..################..
..################..
...##....##....##...
...##....##....##...
..####..####..####..
..####..####..####..
..####..####..####..
..####..####..####..
....................
....................
....................
....................
"""

# STEPS — two prints on a trail. Sparse on purpose: a walk is the one explore row that
# measures distance rather than what was found at the end of it.
MOTIFS["steps"] = """
....................
..####..............
.######.............
.######.............
.######.............
..####..............
..##..##............
....................
..........####......
.........######.....
.........######.....
.........######.....
..........####......
..........##..##....
"""

# THE RACK — a frame of stacked units. Distinct from SECOND_INSTANCE's two side-by-side
# stacks, which is a glyph about a PAIR; this one is about how full the rack is.
MOTIFS["rack"] = """
....................
..################..
..##............##..
..##..########..##..
..##............##..
..##..########..##..
..##............##..
..##..########..##..
..##............##..
..##..########..##..
..##............##..
..################..
....................
....................
"""

# SEEN — an eye. The third bestiary axis needed a mark that is not a creature: raising
# and defeating both already own one, and only this row is about merely looking.
MOTIFS["eye"] = """
....................
....................
......########......
....############....
..####........####..
.###....####....###.
.##....######....##.
.##....######....##.
.###....####....###.
..####........####..
....############....
......########......
....................
....................
"""

# A TITLE — a medal on its ribbon. Earned for a cleared zone, but drawn as the receipt
# rather than the zone, so it cannot be mistaken for the AREA rows beside it.
MOTIFS["title"] = """
....................
.......######.......
.....##########.....
....####....####....
....##........##....
....##........##....
....####....####....
.....##########.....
.......######.......
......##....##......
.....##......##.....
.....##......##.....
....##........##....
....................
"""

# --- The five arcade cabinets ------------------------------------------------
# One per cabinet, each wearing its own GAME's mark — the shape TOWER_OF_FRAGGLE
# already set for a cabinet row. Not a cabinet silhouette with a mark inside it: at
# 20px the screen would be eight pixels across and every one of them would read alike.

# SPOT THE PHISH — a raft of eggs with the live one standing proud of the rest.
MOTIFS["cab_clutch"] = """
....................
.......####.........
..##...####...##....
.####..####..####...
.####..####..####...
.####..####..####...
..##...####...##....
.......####.........
....................
..################..
..################..
....................
....................
....................
"""

# ISOLATION PROTOCOL — the worm doubled back on itself, and the byte it is going for.
MOTIFS["cab_isolation"] = """
....................
..##############....
..##############....
..##................
..##................
..##############....
..##############....
................##..
................##..
..##############....
..##############....
....................
..........####......
..........####......
"""

# DISK DECRYPTION — a padlock over three tumblers, which is the board's whole shape:
# three slots, and a key either falls or it does not.
MOTIFS["cab_decryption"] = """
....................
......########......
....############....
....####....####....
....####....####....
..################..
..################..
..####..####..####..
..####..####..####..
..################..
..################..
..################..
....................
....................
"""

# DECRYPTOGRAM — letters standing open in a line with the closed cells between them.
# The QUOTES ladder draws the whole framed board; this is one line off it.
MOTIFS["cab_cryptogram"] = """
....................
....................
..####..####..####..
..####..####..####..
....................
..####..##....####..
..####..##....####..
....................
..##....####..####..
..##....####..####..
....................
..####..####..##....
..####..####..##....
....................
"""

# CHROMATOPHORE — the line's bell, PARTED. The set of three bell glyphs differ only in
# treatment (ASSET_MANIFEST.md §875): solid is the line, hollow is the hatch run that
# was never spotted, and parted is the cabinet it is played on.
MOTIFS["cab_chroma"] = """
....................
........##.##.......
......####.####.....
.....#####.#####....
....######.######...
....######.######...
...#######.#######..
...#######.#######..
..########.########.
..########.########.
...##..##...##..##..
...##..##...##..##..
....................
....................
"""

# --- The kitchen's older ladders ---------------------------------------------
# These rows shipped before this file existed and have been wearing each other's art
# since. Their motifs are new; the CUISINE rungs inherit, because that ladder's own
# motif is already drawn and only its later rungs were missing.

# METHODS KNOWN — a chef's hat. The kitchen now has three ladders and they must not be
# one picture: CUISINE draws the BOWL (dishes held), SERVICE draws the STOVE (dishes
# cooked), and this draws the person who was taught, which is what a recipe is.
MOTIFS["recipes"] = """
....................
.....##########.....
...##############...
..################..
..################..
..################..
...##############...
....############....
....############....
....##..##..##......
....############....
....############....
....................
....................
"""

# TIRAMISUDO — layers under a dusting. The only food that permanently upgrades the pet,
# so it is the one dish worth drawing as something built rather than served.
MOTIFS["tiramisudo"] = """
..##...##...##...##.
....................
..################..
..################..
....................
..################..
..################..
....................
..################..
..################..
....................
..################..
..################..
....................
"""

# PORTRIDGE — a bowl, steaming, and nothing else. The method that changes nothing, so
# the glyph is deliberately the plainest thing in the pantry.
MOTIFS["portridge"] = """
....................
....................
.......##...##......
......##...##.......
....................
..################..
..################..
..################..
...##############...
...##############...
....############....
.....##########.....
......########......
....................
"""

# RECURSIVE TURDUCKEN — three domes, each inside the last. The row is called Base Case
# and the joke only works if a reader can count the nesting, so the sizes step hard.
MOTIFS["turducken"] = """
....................
....................
........####........
.......######.......
......########......
....................
......########......
.....##########.....
....############....
....................
....############....
...##############...
..################..
....................
"""

# --- The roster --------------------------------------------------------------
# One row per glyph: the achievement id it is drawn for, its motif, and its footer.
# A ladder is N entries over ONE motif — which is the whole reason the motifs above
# are named for subjects rather than for rows.

GLYPHS = [
    # ROCK THE DOCK's ladder: three rungs, one dock, a countable tally.
    ("ICON_ACH_DOCK_FIRST", "dock", tally(1)),
    ("ICON_ACH_DOCK_5", "dock", tally(2)),
    ("ICON_ACH_DOCK_25", "dock", tally(3)),
    # ...and its two SHAPE rows, which are not on that ladder and so carry no tally.
    ("ICON_ACH_DOCK_UNDERDOG", "underdog", plain()),
    ("ICON_ACH_DOCK_PUNCHING_UP", "punchup", plain()),
    # Enlisting: a one-off, like the two above.
    ("ICON_ACH_CREW_ENLISTED", "enlisted", plain()),
    # The Metamorphic line's pair, matching how the other four lines are drawn: the
    # line's mark under a bar for "raised them all", under a chevron for "took one deep".
    ("ICON_ACH_FULL_LINE_METAMORPHIC", "bell", bar()),
    ("ICON_ACH_DEEP_LINE_METAMORPHIC", "bell", chevron()),

    # --- The ladders the same change opened, one motif each ------------------
    ("ICON_ACH_DUEL_WIN_1", "duel_won", tally(1)),
    ("ICON_ACH_DUEL_WIN_10", "duel_won", tally(2)),
    ("ICON_ACH_DUEL_WIN_50", "duel_won", tally(3)),

    ("ICON_ACH_SERVICE_1", "stove", tally(1)),
    ("ICON_ACH_SERVICE_25", "stove", tally(2)),
    ("ICON_ACH_SERVICE_100", "stove", tally(3)),

    ("ICON_ACH_QUOTES_1", "cipher", tally(1)),
    ("ICON_ACH_QUOTES_10", "cipher", tally(2)),
    ("ICON_ACH_QUOTES_50", "cipher", tally(3)),
    ("ICON_ACH_QUOTES_ALL", "cipher", bar()),

    ("ICON_ACH_PEERS_1", "peers", tally(1)),
    ("ICON_ACH_PEERS_10", "peers", tally(2)),
    ("ICON_ACH_PEERS_50", "peers", tally(3)),

    ("ICON_ACH_STEPS_1K", "steps", tally(1)),
    ("ICON_ACH_STEPS_10K", "steps", tally(2)),
    ("ICON_ACH_STEPS_100K", "steps", tally(3)),

    ("ICON_ACH_RACK_3", "rack", tally(1)),
    ("ICON_ACH_RACK_6", "rack", tally(2)),

    ("ICON_ACH_SEEN_12", "eye", tally(1)),
    ("ICON_ACH_SEEN_ALL", "eye", bar()),

    ("ICON_ACH_TITLE_FIRST", "title", tally(1)),
    ("ICON_ACH_TITLES_ALL", "title", bar()),

    # --- The five cabinets that had no row of their own ----------------------
    ("ICON_ACH_CAB_CLUTCH", "cab_clutch", plain()),
    ("ICON_ACH_CAB_ISOLATION", "cab_isolation", plain()),
    ("ICON_ACH_CAB_DECRYPTION", "cab_decryption", plain()),
    ("ICON_ACH_CAB_CRYPTOGRAM", "cab_cryptogram", plain()),
    ("ICON_ACH_CAB_CHROMA", "cab_chroma", plain()),

    # --- Rungs added to ladders that already shipped -------------------------
    # These INHERIT rather than redraw: the motif belongs to the ladder, so a new rung
    # taking a copy is the only way it still matches after the original is retouched.
    ("ICON_ACH_STACK_100", inherit("ICON_ACH_STACK_50"), tally(4)),
    ("ICON_ACH_SPECIES_ALL", inherit("ICON_ACH_SPECIES_12"), bar()),
    ("ICON_ACH_DEEP_LINE_WORM", inherit("ICON_ACH_FULL_LINE_WORM"), chevron()),

    # --- The kitchen's older debt --------------------------------------------
    # CUISINE's three middle rungs, taking the ladder's own bowl off its first rung.
    ("ICON_ACH_CUISINE_25", inherit("ICON_ACH_CUISINE_3"), tally(3)),
    ("ICON_ACH_CUISINE_60", inherit("ICON_ACH_CUISINE_3"), tally(4)),
    ("ICON_ACH_CUISINE_120", inherit("ICON_ACH_CUISINE_3"), tally(5)),
    # ...and its capstone, which is REDRAWN rather than left alone. It shipped wearing
    # tally(3), which was right when it was the third rung of three and is now the same
    # picture as CUISINE_25. A capstone is a bar in this grammar anyway — "all of them"
    # is not a count — so the fix and the convention are the same edit.
    ("ICON_ACH_CUISINE_ALL", inherit("ICON_ACH_CUISINE_3"), bar()),

    ("ICON_ACH_RECIPES_1", "recipes", tally(1)),
    ("ICON_ACH_RECIPES_10", "recipes", tally(2)),
    ("ICON_ACH_RECIPES_30", "recipes", tally(3)),
    ("ICON_ACH_RECIPES_60", "recipes", tally(4)),
    ("ICON_ACH_RECIPES_ALL", "recipes", bar()),

    ("ICON_ACH_COOK_TIRAMISUDO", "tiramisudo", plain()),
    ("ICON_ACH_COOK_PORTRIDGE", "portridge", plain()),
    ("ICON_ACH_COOK_TURDUCKEN", "turducken", plain()),

    # The rest of the EPIC tier. These take the DISH itself off the ITEMS list rather
    # than getting a motif of their own — see from_item. Two are a row or three taller
    # than the band, and both are stacks of a repeated element, so the repeat is what
    # gives way: one rib off the bottom of the rack, one inch off the glass's stem.
    ("ICON_ACH_COOK_ESCALOPE", from_item("privilege_escalope"), plain()),
    ("ICON_ACH_COOK_SPARE_RIBS", from_item("spare_ribs", (17, 18, 19)), plain()),
    ("ICON_ACH_COOK_RACELETTE", from_item("racelette"), plain()),
    ("ICON_ACH_COOK_OVERFLOAT", from_item("buffer_overfloat", (15,)), plain()),
    ("ICON_ACH_COOK_PROFILEROLE", from_item("profilerole"), plain()),
]


def _read_rows(path, what):
    """A shipped PNG as SIZE rows of ASCII — the tool's one way of reading art back."""
    if not os.path.exists(path):
        raise ValueError("%s: no such glyph to take a motif from" % what)
    im = Image.open(path).convert("RGBA")
    if im.size != (SIZE, SIZE):
        raise ValueError("%s is %dx%d, want %dx%d" % ((what,) + im.size + (SIZE, SIZE)))
    return ["".join("#" if im.getpixel((x, y))[3] > 127 else "."
                    for x in range(SIZE)) for y in range(SIZE)]


def motif_rows(motif_name):
    """The motif's 14 ASCII rows, whether authored here or lifted off a shipped PNG."""
    if isinstance(motif_name, tuple) and motif_name[0] == "@inherit":
        rows = _read_rows(os.path.join(ICONS, motif_name[1] + ".png"),
                          "inherit('%s')" % motif_name[1])
        return rows[:MOTIF_ROWS]
    if isinstance(motif_name, tuple) and motif_name[0] == "@item":
        _, item_id, drop = motif_name
        what = "from_item('%s')" % item_id
        rows = _read_rows(os.path.join(ICONS, "ICON_ITEM_%s.png" % item_id.upper()),
                          what)
        kept = [r for y, r in enumerate(rows) if y not in drop]
        ink = [y for y, r in enumerate(kept) if "#" in r]
        if not ink:
            raise ValueError("%s: nothing left to centre" % what)
        body = kept[ink[0]:ink[-1] + 1]
        if len(body) > MOTIF_ROWS:
            raise ValueError("%s: %d rows of ink, band holds %d — name %d more "
                             "drop_rows (a repeat, not a feature)"
                             % (what, len(body), MOTIF_ROWS, len(body) - MOTIF_ROWS))
        top = (MOTIF_ROWS - len(body)) // 2
        blank = "." * SIZE
        return [blank] * top + body + [blank] * (MOTIF_ROWS - top - len(body))
    return MOTIFS[motif_name].strip("\n").split("\n")


def build(motif_name, footer):
    """Compose a motif and a footer into a 20x20 bitmap of bools."""
    raw = motif_rows(motif_name)
    if len(raw) != MOTIF_ROWS:
        raise ValueError("motif '%s' is %d rows, want %d — rows 14+ belong to the "
                         "footer" % (motif_name, len(raw), MOTIF_ROWS))
    grid = [[False] * SIZE for _ in range(SIZE)]
    for y, line in enumerate(raw):
        if len(line) != SIZE:
            raise ValueError("motif '%s' row %d is %d wide, want %d"
                             % (motif_name, y, len(line), SIZE))
        for x, ch in enumerate(line):
            grid[y][x] = (ch == "#")
    for y, line in footer.items():
        for x, ch in enumerate(line):
            if ch == "#":
                grid[y][x] = True
    return grid


def to_image(grid):
    im = Image.new("RGBA", (SIZE, SIZE), BARE)
    px = im.load()
    for y in range(SIZE):
        for x in range(SIZE):
            if grid[y][x]:
                px[x, y] = INK
    return im


def ascii_art(grid):
    return "\n".join("".join("#" if c else "." for c in row) for row in grid)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[1])
    ap.add_argument("--check", action="store_true",
                    help="verify the committed PNGs match what this would write")
    ap.add_argument("--preview", metavar="ID",
                    help="print one glyph as ASCII instead of writing anything")
    args = ap.parse_args()

    if args.preview:
        want = args.preview.upper()
        for name, motif, footer in GLYPHS:
            if name.endswith(want) or name == want:
                print(name)
                print(ascii_art(build(motif, footer)))
                return 0
        sys.stderr.write("no glyph matching '%s'\n" % args.preview)
        return 2

    stale = []
    for name, motif, footer in GLYPHS:
        im = to_image(build(motif, footer))
        path = os.path.join(ICONS, name + ".png")
        if args.check:
            if not os.path.exists(path):
                stale.append(name + " (missing)")
                continue
            if list(Image.open(path).convert("RGBA").getdata()) != list(im.getdata()):
                stale.append(name + " (differs)")
            continue
        im.save(path)
        print("wrote assets/icons/%s.png" % name)

    if args.check:
        if stale:
            sys.stderr.write("achievement glyphs are stale:\n  "
                             + "\n  ".join(stale) + "\n")
            return 1
        print("achievement glyphs in sync (%d)" % len(GLYPHS))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
