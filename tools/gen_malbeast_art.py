#!/usr/bin/env python3
"""
gen_malbeast_art.py — the malbeast drawing vocabulary, and the sheets built from it: the wild
roster and the bosses.

Sibling to tools/gen_worm_art.py. Same contract: an AUTHORING step that OWNS the PNGs it
emits under assets/sprites/, run by hand when a recipe changes, `--check`ed in the gates so
a hand edit to a committed sheet (which the next run would silently revert) fails loudly
instead.

    python3 tools/gen_malbeast_art.py                 # write every recipe's sheet
    python3 tools/gen_malbeast_art.py --check         # committed art still matches? (gates)
    python3 tools/gen_malbeast_art.py --preview glitchhog   # ASCII a sheet to the terminal

Why a second tool, and not the Worm one
----------------------------------------
The Worm line spends a mother STYLE instead of a mother colour (CREATURE_VISUAL_RULES §4),
and gen_worm_art.py holds that style mechanically so a dozen sprites still read as one hand.
The wild malbeasts have a style of their OWN. A malbeast is not a creature-line member (it
is a bare sprite-named spec in combat_factory.cpp, no creature row, no mother colour), so
nothing else holds the roster to one style; the recipe rules below and the --check gate
do.

The malbeast style, as the only things a recipe can say
-------------------------------------------------------
1.  THE 4px NIBBLE GRID. Every recipe draws on a COARSE grid of 4×4-pixel cells
    ("nibbles") — a 56×48 sprite is a 14×12 nibble field — and bake() expands each set
    nibble to a solid 4×4 block. So a malbeast is low-def BY CONSTRUCTION: there is no way
    to say a 1px detail. This is the whole
    "thicker limbs, squares of 4 pixels, blocky like an original Digimon sprite" read, and
    it is held by the grid rather than by an eye.

2.  LINE ART, the original V-pet way. A closed one-cell outline with the detail drawn
    INSIDE it as more ink: a solid eye, a jaw or jowl line, a curl or two of hide, toes
    as broken cells along the floor. At one nibble a line is 4px thick, which is what
    keeps it reading as a deliberate stroke on the ×1.75 panel. Draw the filled shape
    first and trace its boundary, so the outline is always closed; an open outline reads
    as a broken sprite, not as a style.

    And every line is EDGE-CONNECTED: two inked cells may not touch at a corner alone. On
    paper a diagonal of single cells reads as a line; on the panel each nibble is a 7px
    block, the corner they share is one pixel, and the line reads as a row of separate
    blocks — a gap. A diagonal is drawn as a staircase whose steps share an edge (an extra
    cell on the inside of a curve, or on whichever side keeps a detail clear of the
    outline). grid() refuses a corner-only joint and names the cell.

3.  TWO TONES, on transparent: INK for every line and FILL for the body the outline
    encloses, which bake() works out — a recipe only ever draws lines. Nothing emits a
    third value, because a stray colour costs real flash silently: two tones and
    transparency store as a small indexed sheet, and every extra colour widens it. Light
    lines on a dark body: the line carries the silhouette against the dark scenes, and
    the body is filled so the creature reads as a solid thing, not as a wireframe the
    backdrop shows through. The fill is everything the outline closes off from the edge
    of the grid, so a gap in the outline lets it drain away: an OPEN MOUTH has to say
    where the air stops, with ':' cells — empty on the panel, a wall to the fill. Seal
    the gap, or the whole cavity, and the head behind it stays solid. The CORRUPTION —
    the glitch tear, the lit blocks — is NOT in the pixels: it is a render-time pass
    (core/render/glitch.h, drawSpriteGlitch) over this drawing, so the sheet stays a
    cheap still and the glitch animates for free.

So the animated part is one engine function shared by every malbeast, and a creature is
two two-tone frames drawn on graph paper. Adding a wild is a recipe here, a ROW_H_OVERRIDES
row in gen_assets.py, a roster row in combat_factory.cpp and an area's AreaDef::wilds entry.

The BOSSES are malbeasts too, drawn to the same rules on bigger cells: 64x56 (16x14
nibbles) for a sub-area boss and a Daemon-wide 96x64 (24x16) for an area boss, so the one
the gang answers to stands taller than they do. A boss sheet is SPR_BOSS_*, which
gen_assets.py reads as two rows by family, and its area's row names it (SubBossDef::sprite,
AreaDef::areaBossSprite).

The sheet: two ROWS of one frame each, row 0 the idle and row 1 the attack. That is the
whole animation budget — a wild never breathes or walks, it stands, and it lunges when it
swings (fightPose picks the row). Everything else a fight has to say (stunned, poisoned,
what kind of blow) belongs to the combat FX, not to more frames here.

Facing: declared on the SHEET in gen_assets.py's FACING table (a wild has no creature row to
carry it), and the drawing is authored to match. GlitchHog is Facing::Left — snout to the
left — so the combat screen seats it looking at the pet it faces.

Pure standard library, like both sibling generators, so a fresh clone with a stock Python 3
runs `--check` in the gates. The committed PNG is decoded with gen_assets.py's OWN decoder,
so what is compared is exactly what the atlas build will see.
"""

import argparse
import os
import struct
import sys
import zlib

# The line ink: every outline and every interior mark. LIGHT, the inverse of the pencil on
# the graph paper these are drawn on: the combat backdrops are all dark, and a dark outline
# sinks into them. A light line is what carries the silhouette against the scene, the way a
# lit LCD segment does.
INK = (232, 237, 242, 255)

# The body's fill: every cell the outline encloses (Nib.bake). A dark SLATE — still a solid
# body that hides the backdrop behind it (no wireframe), but a tone darker than the line so
# the outline and the marks inside it are what the eye reads first.
FILL = (58, 64, 80, 255)

# Baked pixel values: outside the drawing, a line, the enclosed body.
EMPTY, LINE, BODY = 0, 1, 2

# One nibble is 4×4 device pixels — the low-def quantum. Every logical size below is in
# nibbles; bake() multiplies by this to reach pixels.
NIB = 4


# --------------------------------------------------------------------------------------
#  The nibble canvas — a coarse boolean field a recipe paints, expanded ×NIB at bake().
# --------------------------------------------------------------------------------------
class Nib:
    """A w×h field of 4×4-pixel cells. A recipe draws in THIS space, so it cannot express
    anything finer than a nibble — the grid is the style."""

    def __init__(self, w, h):
        self.w, self.h = w, h
        self.on = [[False] * w for _ in range(h)]
        # Sealed air: empty cells the body fill may not pass through (a ':' on the grid).
        self.sealed = set()

    def set(self, x, y, v=True):
        if 0 <= x < self.w and 0 <= y < self.h:
            self.on[y][x] = v
        return self

    def get(self, x, y):
        return 0 <= x < self.w and 0 <= y < self.h and self.on[y][x]

    # --- bake: nibbles -> pixels, each nibble a solid NIB×NIB block of line or body ----
    def bake(self):
        """Nibbles -> pixels: each inked nibble a NIB×NIB block of LINE, each enclosed empty
        nibble a block of BODY. Enclosed means it cannot reach the grid's edge through empty
        nibbles that share an edge — so the fill follows the outline exactly, holes included:
        a loop's inside is body too, the way it is white on paper."""
        outside = set()
        open_ = lambda x, y: not self.on[y][x] and (x, y) not in self.sealed
        stack = [(x, y) for y in range(self.h) for x in range(self.w)
                 if (x in (0, self.w - 1) or y in (0, self.h - 1)) and open_(x, y)]
        while stack:
            c = stack.pop()
            if c in outside:
                continue
            outside.add(c)
            x, y = c
            for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
                if 0 <= nx < self.w and 0 <= ny < self.h and open_(nx, ny):
                    stack.append((nx, ny))
        W, H = self.w * NIB, self.h * NIB
        px = [[EMPTY] * W for _ in range(H)]
        for ny in range(self.h):
            for nx in range(self.w):
                if self.on[ny][nx]:
                    v = LINE
                elif (nx, ny) in outside or (nx, ny) in self.sealed:
                    v = EMPTY
                else:
                    v = BODY
                if v == EMPTY:
                    continue
                for dy in range(NIB):
                    for dx in range(NIB):
                        px[ny * NIB + dy][nx * NIB + dx] = v
        return px, W, H

    def ascii(self, on="█", off="·"):
        return "\n".join(
            "".join(on if self.on[y][x] else off for x in range(self.w))
            for y in range(self.h)
        )


# --------------------------------------------------------------------------------------
#  PNG out — line and body on transparent, 8-bit RGBA (gen_assets.py decodes exactly this,
#  and stores the sheet in whichever of its indexed forms is smallest).
# --------------------------------------------------------------------------------------
def to_png(px, w, h):
    raw = bytearray()
    for y in range(h):
        raw.append(0)  # filter: none
        for x in range(w):
            v = px[y][x]
            raw += bytes(INK if v == LINE else FILL if v == BODY else (0, 0, 0, 0))

    def chunk(tag, data):
        return (struct.pack(">I", len(data)) + tag + data
                + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF))

    return (b"\x89PNG\r\n\x1a\n"
            + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(bytes(raw), 9))
            + chunk(b"IEND", b""))


# ======================================================================================
#  RECIPES — one function per sheet, returning a baked (px, w, h). Everything above is the
#  vocabulary; from here down is which cells each creature is.
#
#  A recipe is graph paper: one ASCII grid per frame, '#' an inked nibble and '.' an empty
#  one, exactly how a V-pet sprite is drawn on squared paper. Frames stack as ROWS of the
#  sheet — row 0 the idle, row 1 the attack — because an AnimClip selects a row, and a
#  wild malbeast's two poses are picked by row (fightPose, ui/combat_screen.cpp).
# ======================================================================================

def grid(*rows):
    """One frame from graph-paper rows: '#' a line, '.' empty, ':' SEALED air — empty, but
    the body fill stops at it (rule 3). Every row must be the same width — a ragged grid
    is a typo, and a typo here is a pixel nobody meant."""
    w = len(rows[0])
    for r in rows:
        if len(r) != w:
            raise SystemExit(f"ragged grid row {r!r}: {len(r)} cells, expected {w}")
    n = Nib(w, len(rows))
    for y, r in enumerate(rows):
        for x, c in enumerate(r):
            if c == "#":
                n.set(x, y)
            elif c == ":":
                n.sealed.add((x, y))
            elif c != ".":
                raise SystemExit(f"grid cell {c!r} is not '#', '.' or ':'")
    # Rule 2's second half: no two inked cells meeting at a corner alone.
    for y in range(n.h - 1):
        for x in range(n.w):
            if not n.get(x, y):
                continue
            for dx in (1, -1):
                if n.get(x + dx, y + 1) and not n.get(x + dx, y) and not n.get(x, y + 1):
                    raise SystemExit(
                        f"corner-only joint between cells ({x},{y}) and ({x + dx},{y + 1}) "
                        f"(column,row from the grid's top-left): ink ({x},{y + 1}) or "
                        f"({x + dx},{y}) so the line steps on an edge")
    return n


def stack(*frames, swells=False):
    """Bake frames into one sheet, one frame per ROW (idle on top, attack below).

    The poses are one body, so their FILLS should be about the same size. A frame whose
    body is less than half the biggest pose's has had its fill drain out through a gap —
    nearly always an open mouth with no ':' seal (rule 3) — and is refused, naming the
    row, rather than shipping as a wireframe on the one beat it matters most.

    `swells=True` is the one way past that, for a creature whose pose change IS a change
    of size (Bloatfish inflates to strike). The recipe has to say so, so the check still
    holds for every sheet that does not."""
    w, h = frames[0].w * NIB, frames[0].h * NIB
    px, bodies = [], []
    for f in frames:
        if (f.w, f.h) != (frames[0].w, frames[0].h):
            raise SystemExit("every frame of a sheet must be the same size")
        fpx, _, _ = f.bake()
        px.extend(fpx)
        bodies.append(sum(row.count(BODY) for row in fpx))
    for i, b in enumerate(bodies):
        if not swells and b * 2 < max(bodies):
            raise SystemExit(
                f"sheet row {i} has a body of {b // (NIB * NIB)} cells against "
                f"{max(bodies) // (NIB * NIB)} in another pose: its outline is open "
                f"somewhere (an open mouth?) — seal the gap with ':' so the fill stays in")
    return px, w, h * len(frames)


def glitchhog():
    """GlitchHog — the tier-1 'net boar, facing LEFT. A closed one-cell outline with the
    detail drawn inside it: hackles along the back, a solid eye, a jowl line splitting head
    from body, one hide curl, a belly line, stubby legs. The attack lunges the whole body
    forward a cell with the head dropped and the jaw open."""
    idle = grid(
        "......#.#.#...",
        ".....#######..",
        "...###.....##.",
        "..##........#.",
        ".####.......#.",
        "##.##.#.....#.",
        "##....#..##.#.",
        ".##...#...#.#.",
        "..##....###.#.",
        "...##########.",
        "...##.##.##.##",
        "..###.##.##.##",
    )
    attack = grid(
        "..............",
        ".......#.#.##.",
        ".....########.",
        "...###......##",
        "...#.##......#",
        "..##.##.#....#",
        "###.....#..###",
        "#.......##..#.",
        ".:##.....####.",
        "#####.####.#..",
        "..#####.##.##.",
        ".###.##.##.##.",
    )
    return stack(idle, attack)


def segfault_pup():
    """Segfault Pup — the tier-1 runt, front-on (no facing: nothing to mirror). Floppy ears,
    two eyes, a nose, a collar line, four stubby paws. The attack bares a wide mouth with
    the brows dropped — a pup that bites."""
    idle = grid(
        ".##........##.",
        ".###......###.",
        ".#.########.#.",
        ".#..#....#..#.",
        ".##.#....#.##.",
        "..###.##.###..",
        "...#......#...",
        "..##.####.##..",
        "..#...#....#..",
        "..#.######.#..",
        "..#.#.##.#.#..",
        ".####.##.####.",
    )
    attack = grid(
        "..............",
        ".##........##.",
        ".###......###.",
        ".#.########.#.",
        ".#..#....#..#.",
        ".##...##...##.",
        "..#.######.#..",
        "..#.#.##.#.#..",
        "..#........#..",
        "..#........#..",
        "..##########..",
        ".####.##.####.",
    )
    return stack(idle, attack)


def packet_wraith():
    """Packet Wraith — tier 2, front-on, and HOVERING: the bottom row is empty, so it is
    the one wild that does not stand on the floor. A hood, two slot eyes, an envelope
    flap folded down its chest, tattered wisps. The attack flings its arms wide and opens
    the mouth."""
    idle = grid(
        "....######....",
        "...##....##...",
        "..###....###..",
        "..#.#....#.#..",
        ".####....####.",
        ".####.....###.",
        "..#.#....###..",
        "..#####.####..",
        "...#.######...",
        "...###..###...",
        "....#....#....",
        "..............",
    )
    attack = grid(
        "....######....",
        "...##....##...",
        "..###....###..",
        "#.#.#....#.#.#",
        "##############",
        "####.....#####",
        "..###....###..",
        "..#####.####..",
        "...#.######...",
        "..####..####..",
        "..#..#..#..#..",
        "..............",
    )
    return stack(idle, attack)


def cache_ghoul():
    """Cache Ghoul — tier 2, facing LEFT. A big skull of a head (the V-pet rule: the head
    is where the read is) with a sunken eye and teeth, a small hunched body and one long
    dangling claw arm. The attack thrusts the skull forward, jaw open, arm reaching."""
    idle = grid(
        "..######......",
        ".##....##.....",
        "#####...##....",
        "##.##....#....",
        ".###....##....",
        "..####.##.....",
        "....#..#......",
        "...###.#####..",
        "..##.####..#..",
        "..#..#######..",
        ".###..##.##...",
        ".#.#.###.###..",
    )
    attack = grid(
        "..............",
        ".######.......",
        "####..##......",
        "#.##...##.....",
        "###....##.....",
        ".##.#.##......",
        "..###.#.......",
        "...#..######..",
        "##.####.#..#..",
        "#..##.######..",
        "#.##...##.##..",
        ".......##.###.",
    )
    return stack(idle, attack)


def buffer_wyrm():
    """Buffer Wyrm — tier 3, facing LEFT. A wedge head on a three-cell neck with segment
    bars down it, rising out of a looped coil that spills to the edges of its cell — the
    overflow. The attack drives the head forward and down with the jaw split open."""
    idle = grid(
        "..............",
        ".#####........",
        "###..##.......",
        "####.###......",
        "...###.##.....",
        ".....#####....",
        "......#.###...",
        "...####...#...",
        "..##......##..",
        ".########..##.",
        ".##.#...#..##.",
        "..##########..",
    )
    attack = grid(
        "..............",
        "..............",
        "######........",
        "...#.###......",
        "..####.###....",
        ".....######...",
        "......#.#.#...",
        "...####...#...",
        "..##......##..",
        ".########..##.",
        ".##.#...#..##.",
        "..##########..",
    )
    return stack(idle, attack)


def kernel_leviathan():
    """Kernel Leviathan — the tier-3 apex, facing LEFT, on the bigger 64x56 cell (16x14
    nibbles). A heavy sea-beast: back spines, a solid eye, a long jaw line and a ringed
    core in its flank (the kernel), thick stumps of legs. The attack drops and opens the jaw."""
    idle = grid(
        "........#.#.#...",
        ".......########.",
        ".....####.#.#.#.",
        "...###........#.",
        ".####.........#.",
        "##.##....####.##",
        "#........#..#..#",
        "#####....#..#..#",
        ".###.....####..#",
        "..##...........#",
        "...##..###.###.#",
        "....#..#.#.#.#.#",
        "....#..#.#.#.#.#",
        "...#####.###.###",
    )
    attack = grid(
        "................",
        ".......#.#.#....",
        "......########..",
        "....###......#..",
        "..###........#..",
        "####....####.##.",
        "#.......#..#..##",
        ".:::::::#..#...#",
        ".:::::::####...#",
        "#####..........#",
        ".##...#######..#",
        "..##..#.#.#.#.##",
        "...#..#.#.#.#.#.",
        "..#####.###.###.",
    )
    return stack(idle, attack)


def bloatfish():
    """Bloatfish — Net-Sea Crossing, facing LEFT. A pufferfish named for bloatware: at rest a
    small fish, eye forward, gill line, forked tail, a few spines; when it swings it
    INFLATES into a round ball spined all the way round, which is the whole pose — the
    attack is the swell, not a lunge."""
    idle = grid(
        "..............",
        "..............",
        "..............",
        "....#..#......",
        "....####....#.",
        "..###..###.##.",
        ".###..##.####.",
        ".##...#..####.",
        "..###.####..#.",
        "....####......",
        ".....#.#......",
        "..............",
    )
    attack = grid(
        "......#.......",
        "...#.###.#....",
        "...###.###....",
        "..###....##...",
        "#.###.....#.##",
        "###.......###.",
        ".#....#.#..###",
        "###.......##.#",
        "..#...#.#.#...",
        "..###...###...",
        "....#####.....",
        "....#...#.....",
    )
    # The inflated pose is around four times the body of the calm one, on purpose.
    return stack(idle, attack, swells=True)


def trialobite():
    """Trialobite — Net-Sea Crossing, front-on (no facing: it is drawn from above, the way
    a fossil is read). A trilobite named for trialware: a horned head shield with two eyes,
    swept-back spines, a body split into the three lobes the animal is named for, ribs
    either side. The attack rears it up, spines flared and legs out."""
    idle = grid(
        "...#......#...",
        "...##....##...",
        "..##########..",
        ".##........##.",
        "###.#....#.###",
        "#.#........#.#",
        "#.##########.#",
        "..#..#..#..#..",
        "..####..####..",
        "...###..###...",
        "....##..##....",
        ".....####.....",
    )
    attack = grid(
        "..#........#..",
        "..##......##..",
        "..##########..",
        ".####....####.",
        "##..........##",
        "###:......:###",
        "#.#..#..#..#.#",
        "#.##########.#",
        "...#.#..#.#...",
        "..####..####..",
        "..#.#....#.#..",
        "....######....",
    )
    return stack(idle, attack)


def hoaxolotl():
    """Hoaxolotl — Napstorrent Moors, facing LEFT. An axolotl named for the hoax: the broad
    smiling head, the gill stalks fanning up behind it, a long low body, stubby legs and a
    finned tail. The attack flares the gills and opens the mouth (sealed air, rule 3)."""
    idle = grid(
        "..............",
        "...##.#.##....",
        "....#.#.#.....",
        "...#######....",
        ".###.....#....",
        "##.#.....##...",
        "#.........####",
        "#####......###",
        ".###########..",
        "..##....##....",
        ".###...###....",
        "..............",
    )
    attack = grid(
        "...#...#...#..",
        "...###.#..##..",
        ".....#.#.##...",
        "...#######....",
        ".###.....#....",
        "##.#.....#....",
        ":###.....#####",
        "##.........###",
        ".###########..",
        "..##....##....",
        ".###...###....",
        "..............",
    )
    return stack(idle, attack)


def fuzzard():
    """Fuzzard — Napstorrent Moors, facing LEFT. A buzzard named for fuzzing, the moorland's
    own raptor: perched, hook beak forward, folded wing marked on the body, tail feathers
    and talons on the floor. The attack throws both wings up into a V."""
    idle = grid(
        "..............",
        "...####.......",
        "..##..##......",
        ".####..#......",
        "##..#..##.....",
        "....#...##....",
        "....#.#####...",
        "...####..###..",
        "...##.#######.",
        "....#######.##",
        ".....#..#.....",
        "....##.##.....",
    )
    attack = grid(
        "#..........#..",
        "##........##..",
        "###.####.###..",
        ".####..#####..",
        "..###....##...",
        "####.....#....",
        "...#..####....",
        "...#.....##...",
        "...##.....###.",
        "....#######.##",
        "....#..#......",
        "...##.##......",
    )
    return stack(idle, attack)


def garbagoyle():
    """Garbagoyle — Castle Rapidscare, front-on (no facing: carved to be seen straight on). A
    gargoyle named for garbage collection: horns, folded stone wings narrow enough to be all
    line, glaring eyes, a fanged mouth, crouched on the ledge. The attack spreads the wings
    high and opens the mouth (sealed air, rule 3)."""
    idle = grid(
        "..#........#..",
        "..##......##..",
        "...########...",
        "#..#......#..#",
        "##.#.#..#.#.##",
        "######..######",
        "###.######.###",
        ".#####..#####.",
        "...#....###...",
        "...########...",
        "..###....###..",
        ".####....####.",
    )
    attack = grid(
        "##..#....#..##",
        ".##.##..##.##.",
        ".############.",
        "..##.#..#.##..",
        "..####..####..",
        "...#......#...",
        "...#.::::.#...",
        "...#......#...",
        "...##....##...",
        "....######....",
        "...###..###...",
        "..####..####..",
    )
    return stack(idle, attack)


def gatorway():
    """Gatorway — Castle Rapidscare, facing LEFT. A gator named for the gateway it keeps,
    out of the castle moat: low and long, eye bump, ridged back, a toothed jaw line, stubby
    legs, a tapering tail. The attack gapes the jaws (sealed air between them)."""
    idle = grid(
        "..............",
        "..............",
        "..............",
        "..............",
        "..##..#.#.#...",
        ".############.",
        "##..........##",
        "#####......###",
        ".###########..",
        "..##...##.....",
        ".###..###.....",
        "..............",
    )
    attack = grid(
        "..............",
        "..............",
        "#.............",
        "###...........",
        ".####.#.#.#...",
        "..###########.",
        "...#........##",
        "..##........##",
        "#############.",
        "#####...##....",
        "..##..###.....",
        "..............",
    )
    return stack(idle, attack)


def black_window():
    """Black Window — the Silk Lode, front-on (drawn from above, the way a spider is read; no
    facing). A black widow named for the pop-up window: legs radiating off a small head,
    and on the abdomen not the hourglass but a WINDOW — a cross of mullions splitting it
    into four panes. The attack lifts and spreads the legs and bares the fangs."""
    idle = grid(
        "..............",
        "..#...##...#..",
        "..##.####.##..",
        "#..####.###..#",
        "#####.########",
        "....###.##....",
        "#..##.#..##..#",
        "####..#...##..",
        "...########...",
        "..##..#..##...",
        "...#######....",
        ".....###......",
    )
    attack = grid(
        "##..........##",
        ".#...####...#.",
        ".##.##..##.##.",
        "..####::####..",
        "...##....##...",
        "....#....#....",
        "...##....##...",
        "..##..##..##..",
        "##############",
        "#.##..##..##.#",
        "...########...",
        ".....####.....",
    )
    return stack(idle, attack)


def terminite():
    """Terminite — the Silk Lode, facing LEFT. A soldier termite named for the terminal: the
    big armoured head with its mandibles forward, a narrow waist, a plump striped abdomen,
    three straight legs. The attack opens the mandibles (sealed air between them)."""
    idle = grid(
        "..............",
        "..............",
        "..............",
        "........#####.",
        "#####..##...##",
        ".#.###.##.#..#",
        ".#...####.#..#",
        ".#..##.##.#.##",
        "#####..######.",
        "...#..##.#....",
        "...#..#..#....",
        "..##.##.##....",
    )
    attack = grid(
        "..............",
        "..............",
        "#.............",
        "###.....#####.",
        "..###..##...##",
        ".#####.##.#..#",
        "..#..####.#..#",
        ".##.##.##.#.##",
        "..###..######.",
        "...#..##.#....",
        "...#..#..#....",
        "..##.##.##....",
    )
    return stack(idle, attack)


def anonymouse():
    """Anonymouse — the Darkweb Crawl, front-on (no facing: it looks straight at you,
    and you still can't say who it is). A mouse named for anonymity: two big round ears, a
    narrow head with a nose, a small body, the tail run out along the floor. The attack bares
    the teeth — two incisors over sealed air."""
    idle = grid(
        ".####....####.",
        "##..##..##..##",
        "#....####....#",
        "##..##..##..##",
        ".#####..#####.",
        "....#....#....",
        "...##.##.##...",
        "...#......#...",
        "...##....##...",
        "....######....",
        "...##....##..#",
        "...###########",
    )
    attack = grid(
        ".####....####.",
        "##..##..##..##",
        "#....####....#",
        "##..##..##..##",
        ".#####..#####.",
        "....#....#....",
        "...########...",
        "...#.#::#.#...",
        "...########...",
        "....######....",
        "...##....##..#",
        "...###########",
    )
    return stack(idle, attack)


def vulnture():
    """Vulnture — the Darkweb Crawl, front-on. A vulture named for the vulnerability
    it waits on: a small bald head on its collar, wings held open the way a vulture suns
    itself, feathered tips stepping down, talons under. The attack lifts both wings into a V."""
    idle = grid(
        "..............",
        ".....####.....",
        ".....#..#.....",
        "###..#..#..###",
        "#.##########.#",
        "#....#..#....#",
        "##...#..#...##",
        ".###.#..#.###.",
        "...###..###...",
        ".....#..#.....",
        ".....####.....",
        ".....#..#.....",
    )
    attack = grid(
        "##..........##",
        "###..####..###",
        "#.##.#..#.##.#",
        "#..###..###..#",
        "#...######...#",
        "#....#..#....#",
        "##...#..#...##",
        ".###.#..#.###.",
        "...###..###...",
        ".....#..#.....",
        ".....####.....",
        "....##..##....",
    )
    return stack(idle, attack)


def boss_benjamin():
    """Benjamin the False — Citrus Circuit's Fake File Flats, front-on. A mimic: a file icon
    with a folded corner and lines of text, its seam a row of teeth. The attack splits it
    open along the seam into a mouth (sealed air between the jaws)."""
    idle = grid(
        "................",
        "..#########.....",
        "..#.......##....",
        "..#.......###...",
        "..#.......####..",
        "..#.#####....#..",
        "..#..........#..",
        "..#.######...#..",
        "..############..",
        "..##.#.#.#.#.#..",
        "..#..........#..",
        "..##.######.##..",
        "...#.#....#.#...",
        "..####...####...",
    )
    attack = grid(
        "..#########.....",
        "..#.......##....",
        "..#.......###...",
        "..#.......####..",
        "..#.#####....#..",
        "..#..........#..",
        "..############..",
        "..##:#:#:#:#:#..",
        "..#::::::::::#..",
        "..#:#:#:#:#:##..",
        "..############..",
        "..##.######.##..",
        "...#.#....#.#...",
        "..####...####...",
    )
    return stack(idle, attack)


def boss_morpheus():
    """Morpheus the Mirage — the Buffering Bluffs, front-on. A moth, for the god of
    dreams: feelers, a narrow body, and an eye-spot ringed on each wing. The attack fills the
    eye-spots in, so the wings stare, and spreads the feelers."""
    idle = grid(
        "......#..#......",
        "###...#..#...###",
        "#.##..####..##.#",
        "#..####..####..#",
        "#.###.#..#.###.#",
        "#.#.#.#..#.#.#.#",
        "#.###.#..#.###.#",
        "#.....#..#.....#",
        "##....#..#....##",
        ".##...#..#...##.",
        "..#####..#####..",
        "......#..#......",
        "......####......",
        "................",
    )
    attack = grid(
        ".....##..##.....",
        "###...#..#...###",
        "#.##..####..##.#",
        "#..####..####..#",
        "#.###.#..#.###.#",
        "#.###.#..#.###.#",
        "#.###.#..#.###.#",
        "#.....#..#.....#",
        "##....#..#....##",
        ".##...#..#...##.",
        "..#####..#####..",
        "......#..#......",
        "......####......",
        "................",
    )
    return stack(idle, attack)


def boss_turing():
    """Turing the Unhalted — the 99% Cache, facing LEFT. A snail whose shell is a
    square spiral, the loading spinner that never finishes, on a long foot with its head
    raised and two eye stalks. The attack stretches the stalks and opens the mouth."""
    idle = grid(
        "................",
        "........######..",
        ".......##....##.",
        ".#..#.##########",
        ".#..#.#......#.#",
        ".#..#.#.####.#.#",
        "#####.#.#..#.#.#",
        "#...#.#.#....#.#",
        "##..#.##########",
        "#...#..#......#.",
        "#...####......##",
        "#..............#",
        "################",
        "................",
    )
    attack = grid(
        "................",
        ".#..#...######..",
        ".#..#..##....##.",
        ".#..#.##########",
        ".#..#.#......#.#",
        ".#..#.#.####.#.#",
        "#####.#.#..#.#.#",
        "###.#.#.#....#.#",
        "::#.#.##########",
        "###.#..#......#.",
        "#...####......##",
        "#..............#",
        "################",
        "................",
    )
    return stack(idle, attack)


def boss_sharman():
    """Sharman of the Fold — the Shared Folder, facing LEFT. A hermit crab living in a
    manila folder: the tab on top, the front flap's fold line, a big claw out of the open
    edge and legs underneath. The attack lifts the claw and opens the flap."""
    idle = grid(
        "................",
        ".....####.......",
        ".....#..#######.",
        "....###.......#.",
        ".####:#.......#.",
        "##..###.......#.",
        "#...###########.",
        "##..#.#.......#.",
        ".####.#.......#.",
        "......#.......#.",
        "......#########.",
        "......#..#..#...",
        ".....##.##.##...",
        "................",
    )
    attack = grid(
        ".####...........",
        "##..#.####......",
        "#...###..######.",
        "##..#.#.......#.",
        ".####.#.......#.",
        "......#########.",
        "......#.......#.",
        "......#.......#.",
        "......#.......#.",
        "......#.......#.",
        "......#########.",
        "......#..#..#...",
        ".....##.##.##...",
        "................",
    )
    return stack(idle, attack)


def boss_toneloc():
    """Toneloc the Tolltaker — Dial-Up Draw, facing LEFT. A toad, for the war dialer:
    squat and wide, one bulging eye on top, a grin across the front, a folded back leg with
    long toes. The attack opens the mouth and balloons the throat sac — the call."""
    idle = grid(
        "................",
        "................",
        "................",
        "...###..........",
        "...#.#..........",
        "..##.######.....",
        ".##.......####..",
        "##....#..#####..",
        "#######..#...##.",
        "#........#....#.",
        "##.......#....#.",
        ".##############.",
        "...##.......#.##",
        ".#####...#######",
    )
    attack = grid(
        "................",
        "................",
        "................",
        "...###..........",
        "...#.#..........",
        "..##.######.....",
        ".##.......####..",
        "######...#####..",
        ":::::#...#...##.",
        "######...#....#.",
        "#........#....#.",
        "#.....#########.",
        "##...##.....#.##",
        ".#####...#######",
    )
    return stack(idle, attack)


def boss_baron():
    """Baron Bonzi — Citrus Circuit's area boss, facing LEFT, on the Daemon-wide 96x64
    cell (24x16 nibbles). A knuckle-walking gorilla in a top hat: brow ridge and jutting
    muzzle, a hump of shoulder, a front arm planted on its knuckles. The attack drops the jaw
    in a roar and swings the arm forward."""
    idle = grid(
        "....#####...............",
        "....#...#...............",
        "....#####....######.....",
        "..###...######....###...",
        "...#................###.",
        "...#####..............#.",
        "####.#.....##########.#.",
        "##....................#.",
        "#.....................#.",
        "####..................#.",
        "...#.....#............#.",
        "...#.....#............#.",
        "...#....#########....##.",
        "...#....#.......#....#..",
        "...#....#.......#....#..",
        "..#######......########.",
    )
    attack = grid(
        "....#####...............",
        "....#...#...............",
        "....#####....######.....",
        "..###...######....###...",
        "...#................###.",
        "...#####..............#.",
        "####.#.....##########.#.",
        "##....................#.",
        "####..................#.",
        ":::#..................#.",
        "#####.................#.",
        "....#.................#.",
        "....#....########....##.",
        "#####....#......#....#..",
        "#..#.....#......#....#..",
        "##########.....########.",
    )
    return stack(idle, attack)


def boss_netbus_nipper():
    """Netbus Nipper — the Pirate Bayou's Leech Landing, front-on. A bayou crab, for the trojan
    that reached in and took the handle: a ridged shell, eyes on stalks, both claws up and
    shut. The attack springs the pincers open."""
    idle = grid(
        "................",
        "####........####",
        "####.##..##.####",
        "#..#..#..#..#..#",
        "####..#..#..####",
        "..############..",
        "..#..........#..",
        "..##.######.##..",
        "...#........#...",
        "...#..####..#...",
        "...##......##...",
        "....########....",
        "..#.#......#.#..",
        ".##.#......#.##.",
    )
    attack = grid(
        "####........####",
        "####........####",
        "##...##..##...##",
        ".###..#..#..###.",
        ".###..#..#..###.",
        "..############..",
        "..#..........#..",
        "..##.######.##..",
        "...#........#...",
        "...#..####..#...",
        "...##......##...",
        "....########....",
        "..#.#......#.#..",
        ".##.#......#.##.",
    )
    return stack(idle, attack)


def boss_suprnova_serpent():
    """Suprnova Serpent — Torrent Swamp, facing LEFT. A sea serpent with a star-burst crest:
    the head reared on its neck, coils breaking the water behind. The attack drops the head
    and opens the jaw on two fangs."""
    idle = grid(
        "...###..........",
        "....###.........",
        ".####.##........",
        "##..#..#........",
        "#......#........",
        "#####..#........",
        "....#..#........",
        "....#..#.....###",
        "....#..#.###.#.#",
        "....#..#.#.#.#.#",
        "....#..###.#####",
        "....##..#..#..##",
        "....############",
        "................",
    )
    attack = grid(
        "...###..........",
        "....###.........",
        "................",
        ".#######........",
        "#####..#........",
        "#:::#..#........",
        "#####..#........",
        "....#..#.....###",
        "....#..#.###.#.#",
        "....#..#.#.#.#.#",
        "....#..###.#####",
        "....##..#..#..##",
        "....############",
        "................",
    )
    return stack(idle, attack)


def boss_razor_kraken():
    """Razor Kraken — the Cracked Keys, front-on. A kraken: a domed mantle with two eyes, its
    arms hanging below. The attack rears the outer arms up and opens the beak."""
    idle = grid(
        "......####......",
        ".....##..##.....",
        "....##....##....",
        "....#......#....",
        "....#......#....",
        "....###..###....",
        "....#......#....",
        "...##......##...",
        "..############..",
        ".##.##.##.##.##.",
        ".##.##.##.##.##.",
        ".##.##.##.##.##.",
        "###.##.##.##.###",
        "##..##....##..##",
    )
    attack = grid(
        "......####......",
        ".....##..##.....",
        "##..##....##..##",
        "##..#......#..##",
        "###.#......#.###",
        "#.#.###..###.#.#",
        "#.#.#......#.#.#",
        "#.###.####.###.#",
        "#######::#######",
        "....########....",
        "....##.##.##....",
        "....##.##.##....",
        "...###.##.###...",
        "...##......##...",
    )
    return stack(idle, attack)


def boss_wight_of_drinkordie():
    """Wight of Drinkordie — Warez Marsh, front-on. A hooded wight: two lights in a dark face
    (sealed air), a robe falling to a ragged hem, a bottle held up in one hand. The attack
    flings both arms up and opens a howl."""
    idle = grid(
        "......####......",
        ".....##..##.....",
        "....########....",
        "....##::::##..##",
        "....###::###..##",
        "....##::::##..##",
        "....########.##.",
        "...##..##..####.",
        "...#...##.....#.",
        "...#...##...###.",
        "...#...##...#...",
        "...#...##...#...",
        "...##########...",
        "...#.#.#.#.#....",
    )
    attack = grid(
        "......####......",
        "##...##..##...##",
        "################",
        ".#...#::::#...#.",
        ".#...##::##...#.",
        ".#....#::#....#.",
        ".###..####..###.",
        "...#...##...#...",
        "...#...##...#...",
        "...#...##...#...",
        "...#...##...#...",
        "...#...##...#...",
        "...##########...",
        "...#.#.#.#.#....",
    )
    return stack(idle, attack)


def boss_sunken_seventh():
    """The Sunken Seventh — the Booty Cache, facing LEFT. An anglerfish, for the Sub7
    backdoor: a lure on a stalk over a round body, an underbite with teeth along the seam.
    The attack drops the jaw open."""
    idle = grid(
        ".###............",
        ".#####..........",
        "....#########...",
        "...##.......##..",
        "...#.........###",
        "...#..#......#.#",
        "...#.........#.#",
        "####.#.......#.#",
        "#######......#.#",
        "#............#.#",
        "####.........###",
        "...##.......##..",
        "....#########...",
        "................",
    )
    attack = grid(
        ".###............",
        ".#####..........",
        "....#########...",
        "...##.......##..",
        "...#.........###",
        "...#..#......#.#",
        "########.....#.#",
        ":#:#:#:#.....#.#",
        ":::::::#.....#.#",
        "::#:#:##.....#.#",
        "#######......###",
        "...##.......##..",
        "....#########...",
        "................",
    )
    return stack(idle, attack)


def boss_capn_cracker():
    """Cap'n Cracker — the Pirate Bayou's area boss, facing LEFT, on the 96x64 cell. A pirate
    parrot (Polly wants a cracker): a tricorn, an eyepatch on its strap, a hooked beak, a
    long tail, on a perch. The attack throws a wing up and opens the beak in a squawk."""
    idle = grid(
        "........#####...........",
        ".....#..#...#..#........",
        ".....####...####........",
        "......#......#..........",
        "...####.######..........",
        "..##..#..##..#..........",
        "..#...#......#..........",
        "..#####......#..........",
        "...##.##.....#####......",
        "....#..#.........#......",
        ".......#...#####.##.....",
        ".......#.......#..#.....",
        ".......#.......#..###...",
        ".......#.......########.",
        ".......#.......##:::####",
        "..###################...",
    )
    attack = grid(
        "........#####...########",
        ".....#..#...#.###.....#.",
        ".....####...####......#.",
        "......#........########.",
        "...##########.........#.",
        "..######.##...........#.",
        "..#::::#.......########.",
        "..######..............#.",
        "...##.##.........######.",
        "....#..#.........#......",
        ".......#.........##.....",
        ".......#..........#.....",
        ".......#........#####...",
        ".......#........#:#####.",
        ".......#........#:::####",
        "..###################...",
    )
    return stack(idle, attack)


def boss_candy_siren():
    """The Candy Siren — Net-Sea Crossing's Uninstall Undertow, front-on. The sweet that comes
    bundled with the installer: a wrapped candy with its wrapper twisted off both ends and a
    smiling face. The attack drops the smile open on teeth."""
    idle = grid(
        "................",
        "................",
        "##...######...##",
        "##..##....##..##",
        "#####......#####",
        "#..#..#..#..#..#",
        "#..#........#..#",
        "#..#.##..##.#..#",
        "#..#..####..#..#",
        "#####......#####",
        "##..##....##..##",
        "##...######...##",
        "................",
        "................",
    )
    attack = grid(
        "................",
        "................",
        "##...######...##",
        "##..##....##..##",
        "#####......#####",
        "#..#..#..#..#..#",
        "#..#........#..#",
        "#..#.######.#..#",
        "#..#.#:#::#.#..#",
        "################",
        "##..##....##..##",
        "##...######...##",
        "................",
        "................",
    )
    return stack(idle, attack)


def boss_vundo():
    """Vundo the Unending — the Popup Whirlpool, front-on. A jellyfish whose bell is a pop-up
    window: a title bar with its close box, a line of ad copy, tentacles trailing. The attack
    fans the tentacles out."""
    idle = grid(
        "..############..",
        "..#........###..",
        "..############..",
        "..#..........#..",
        "..#.########.#..",
        "..#..........#..",
        "..############..",
        "...#..#::#..#...",
        "...#..#::#..#...",
        "...#..#::#..#...",
        "...#..#::#..#...",
        "...##.#::#.##...",
        "....#.####.#....",
        "....#..##..#....",
    )
    attack = grid(
        "..############..",
        "..#........###..",
        "..############..",
        "..#..........#..",
        "..#.########.#..",
        "..#..........#..",
        "..#.########.#..",
        "#####..##..#####",
        "#...#..##..#...#",
        "#...#..##..#...#",
        "#...#..##..#...#",
        "##..##.##.##..##",
        ".#...#.##.#...#.",
        ".#...#.##.#...#.",
    )
    return stack(idle, attack)


def boss_superfish():
    """The Superfish — Tracker Trench, facing LEFT. A fish in a cape with an S on its chest
    and a forked tail. The attack opens the mouth and flares a fin."""
    idle = grid(
        "................",
        "........######..",
        ".......##.######",
        "....####..#::#.#",
        "..###.....######",
        ".###..###....#..",
        ".#....#......#..",
        ".###..###....#..",
        ".##.....#....#..",
        "..###.###.######",
        "....#######..#.#",
        ".............###",
        "................",
        "................",
    )
    attack = grid(
        "................",
        "........######..",
        ".......##.######",
        "....####..#::#.#",
        "..###.....######",
        ".###..###....#..",
        "####..#......#..",
        ":::#..###....#..",
        "####....#....#..",
        "..###.###.######",
        "....####..#..#.#",
        ".......#..#..###",
        ".......####.....",
        "................",
    )
    return stack(idle, attack)


def boss_zlob_conger():
    """Zlob Conger — Codec Reef, facing LEFT. A conger eel coming out of a hole in a cracked
    lump of reef (sealed air in the hole). The attack rears the head and opens the jaw."""
    idle = grid(
        "................",
        "................",
        "................",
        "#######.........",
        "#..##.#.........",
        "####..##........",
        "####...#.#######",
        "...###.###:::#.#",
        ".....#####:::###",
        ".....#####:::###",
        "........########",
        "........#...####",
        "........#......#",
        "........########",
    )
    attack = grid(
        "................",
        "#######.........",
        "#####.#.........",
        ":::#..#.........",
        "####..#.........",
        "..#...##........",
        "..##...#.#######",
        "...###.###:::#.#",
        ".....#####:::###",
        ".....#####:::###",
        "........########",
        "........#...####",
        "........#......#",
        "........########",
    )
    return stack(idle, attack)


def boss_green_button():
    """The Green Button — Sandbox Beach, facing LEFT. A turtle whose shell is a download
    button, a play arrow on its face. The attack darts the head out with the mouth open."""
    idle = grid(
        "................",
        "................",
        ".....########...",
        "....##......##..",
        "....#........#..",
        "....#..#.....#..",
        ".####..##....#..",
        ".##..........#..",
        ".#.....##....#..",
        ".####..#.....##.",
        "....##......###.",
        ".....########...",
        "....##....##....",
        "....##....##....",
    )
    attack = grid(
        "................",
        "................",
        ".....########...",
        "....##......##..",
        "....#........#..",
        "....#..#.....#..",
        "#####..##....#..",
        "####.........#..",
        ":::#...##....#..",
        "#####..#.....##.",
        "....##......###.",
        ".....########...",
        "....##....##....",
        "....##....##....",
    )
    return stack(idle, attack)


def boss_admiral_conduit():
    """Admiral Conduit — Net-Sea Crossing's area boss, front-on, on the 96x64 cell. An admiral
    grown fat on years of traffic: a bicorne worn athwart, a moustache, epaulettes, and a
    coat strained over his belly with two rows of buttons down it — the toolbar. He holds a
    spyglass to his eye; the attack raises a cutlass and bellows an order."""
    idle = grid(
        ".........######.........",
        ".....#####....#####.....",
        "...##################...",
        "##......#......#........",
        "###########..#.#........",
        "##...#..##.....#........",
        ".....#...#######........",
        ".....#....####.#........",
        "....##.........#####....",
        "....##.............#....",
        ".....#....#..#.....##...",
        ".....#..............#...",
        ".....##...#..#...####...",
        "......###.####.###......",
        "........#.#..#.#........",
        ".......####..####.......",
    )
    attack = grid(
        ".#.......######.........",
        ".#...#####....#####.....",
        ".####################...",
        ".#.#::::#......#........",
        ".####:::#.#..#.#........",
        "..#.#:::#......#........",
        "..#.#:::########........",
        "..#.#:::##::::##........",
        "..#.################....",
        "..####.............#....",
        ".....#....#..#.....##...",
        ".....#..............#...",
        ".....##...#..#...####...",
        "......###.####.###......",
        "........#.#..#.#........",
        ".......####..####.......",
    )
    return stack(idle, attack)


RECIPES = {
    "SPR_MALBEAST_GLITCHHOG": glitchhog,
    "SPR_MALBEAST_SEGFAULT_PUP": segfault_pup,
    "SPR_MALBEAST_PACKET_WRAITH": packet_wraith,
    "SPR_MALBEAST_CACHE_GHOUL": cache_ghoul,
    "SPR_MALBEAST_BUFFER_WYRM": buffer_wyrm,
    "SPR_MALBEAST_KERNEL_LEVIATHAN": kernel_leviathan,
    "SPR_MALBEAST_BLOATFISH": bloatfish,
    "SPR_MALBEAST_TRIALOBITE": trialobite,
    "SPR_MALBEAST_HOAXOLOTL": hoaxolotl,
    "SPR_MALBEAST_FUZZARD": fuzzard,
    "SPR_MALBEAST_GARBAGOYLE": garbagoyle,
    "SPR_MALBEAST_GATORWAY": gatorway,
    "SPR_MALBEAST_BLACK_WINDOW": black_window,
    "SPR_MALBEAST_TERMINITE": terminite,
    "SPR_MALBEAST_ANONYMOUSE": anonymouse,
    "SPR_MALBEAST_VULNTURE": vulnture,
    "SPR_BOSS_BENJAMIN": boss_benjamin,
    "SPR_BOSS_MORPHEUS": boss_morpheus,
    "SPR_BOSS_TURING": boss_turing,
    "SPR_BOSS_SHARMAN": boss_sharman,
    "SPR_BOSS_TONELOC": boss_toneloc,
    "SPR_BOSS_BARON_BONZI": boss_baron,
    "SPR_BOSS_NETBUS_NIPPER": boss_netbus_nipper,
    "SPR_BOSS_SUPRNOVA_SERPENT": boss_suprnova_serpent,
    "SPR_BOSS_RAZOR_KRAKEN": boss_razor_kraken,
    "SPR_BOSS_WIGHT_OF_DRINKORDIE": boss_wight_of_drinkordie,
    "SPR_BOSS_SUNKEN_SEVENTH": boss_sunken_seventh,
    "SPR_BOSS_CAPN_CRACKER": boss_capn_cracker,
    "SPR_BOSS_CANDY_SIREN": boss_candy_siren,
    "SPR_BOSS_VUNDO": boss_vundo,
    "SPR_BOSS_SUPERFISH": boss_superfish,
    "SPR_BOSS_ZLOB_CONGER": boss_zlob_conger,
    "SPR_BOSS_GREEN_BUTTON": boss_green_button,
    "SPR_BOSS_ADMIRAL_CONDUIT": boss_admiral_conduit,
}

# Short aliases for --preview / --only, so the CLI takes `glitchhog` not the full symbol.
ALIASES = {name.replace("SPR_MALBEAST_", "").replace("SPR_", "").lower(): name
           for name in RECIPES}


def path_for(sym):
    return os.path.join(os.path.dirname(__file__), "..", "assets", "sprites", sym + ".png")


def main():
    ap = argparse.ArgumentParser(description="Generate wild-malbeast and boss sprite sheets.")
    ap.add_argument("--check", action="store_true",
                    help="fail if any committed PNG differs from its recipe (gate)")
    ap.add_argument("--preview", metavar="NAME", nargs="?", const="",
                    help="print a recipe as ASCII nibbles instead of writing it")
    ap.add_argument("--only", metavar="NAME", help="limit to one recipe (alias or symbol)")
    args = ap.parse_args()

    def resolve(name):
        if name in RECIPES:
            return name
        if name in ALIASES:
            return ALIASES[name]
        sys.exit(f"no recipe for '{name}'; have {', '.join(sorted(ALIASES))}")

    if args.preview is not None:
        name = resolve(args.preview) if args.preview else sorted(RECIPES)[0]
        px, w, h = RECIPES[name]()
        # Back down to nibble resolution: every set nibble is a solid NIB block, so its
        # top-left pixel stands for the whole cell.
        glyph = {EMPTY: "·", LINE: "█", BODY: "░"}
        print(f"{name}  ({w}×{h}px, {w // NIB}×{h // NIB} nibbles; █ line, ░ body)\n")
        for ny in range(h // NIB):
            print("".join(glyph[px[ny * NIB][nx * NIB]] for nx in range(w // NIB)))
        return

    names = [resolve(args.only)] if args.only else sorted(RECIPES)
    bad = []
    for sym in names:
        px, w, h = RECIPES[sym]()
        data = to_png(px, w, h)
        p = path_for(sym)
        if args.check:
            if not os.path.exists(p):
                bad.append(f"{sym}: no committed PNG at {p} — run without --check")
            elif open(p, "rb").read() != data:
                bad.append(f"{sym}: committed PNG differs from recipe — "
                           f"rerun `python3 tools/gen_malbeast_art.py` to regenerate")
        else:
            with open(p, "wb") as f:
                f.write(data)
            print(f"wrote {sym}  ({w}×{h})")

    if bad:
        sys.exit("\n".join(bad))


if __name__ == "__main__":
    main()
