# Malwarium — Asset Manifest

The visual assets the pet-side UI needs, and the constraints they're drawn to. Whoever picks up a
`☐ TODO` row draws it within those constraints, saves to `/assets/...`, and updates Status + File
in the same pass. **Empty `File` cells are the work to be done.**

> **This file records judgement, not inventory.** Where an asset id is *derived* from a content
> row — `ICON_ITEM_<ID>`, `ICON_MOD_<ID>`, `ICON_MOVE_<ID>`, a creature's `spriteName` — there is
> deliberately **no table here listing them**, because the roster already exists in
> `src/core/content/` and the drawings already exist in `assets/`. A third list pairing them is a
> copy that drifts silently, and it did: §C once claimed Goliauth was on a generic placeholder
> after the data had moved it to its own sprite. What belongs here is what no code carries — the
> art *rules* (how a rarity ramp stays countable in grayscale, why a branch pair shares a palette)
> and whether a delivered drawing is good enough (`▨` vs `☑`). If you find yourself adding a row
> per content row, the row belongs in `content_*.cpp` or nowhere.

**Status:** `☐` TODO · `✎` WIP · `▨` PLACEHOLDER (a correctly-sized/named stand-in is shipping;
final art still wanted) · `☑` DELIVERED · `⌫` PARKED (drawn, but nothing consumes it — see below) ·
`⊘` N/A (procedural / no art)

> **`assets/` IS the atlas.** `tools/gen_assets.py` walks the tree and compiles every PNG into
> flash, skipping any file or folder whose name starts with `_`. So the tree listing and the
> shipped sprite set are the same thing, and a `▨` stand-in ships exactly like final art does; the
> marker is about art quality, not about whether it's wired. An asset's id is its **basename** —
> `sprites/` · `icons/` · `ui/` organise the tree for readers and change nothing downstream, which
> is also why no two COMPILED PNGs may share a basename. An `_attic/` copy under a shipped
> basename is the one legal shadow — it is the superseded drawing, kept for reference and
> never compiled — so read a File cell as the live path, not as the only file of that name.
>
> **`⌫` rows are parked in `assets/_attic/`** — untracked, never compiled, costing nothing. Art
> lands there when it has no consumer: content comes first (a row in `content_*.cpp` wired to a
> `SPR_PET_GENERIC_*` stand-in or a generic icon fallback), and art replaces the stand-in after.
> A drawing with no content row can't reach the screen, so it waits in the attic rather than
> occupying flash. `assets/_attic/README.md` says how to bring one back.
>
> **This table is prose and can drift; the check can't.** `python3 tools/check_orphan_assets.py`
> fails if any compiled asset has no consumer — run it rather than trusting a `☑` here.
**Canvas:** author at 128×128 logical; ×1.75 to the 224×224 panel. **Palette + font hues
delivered** (`/assets/PAL_CORE.json`) — bind colours to `PAL_CORE` role tokens (never literal
hex). Sizes below are *logical* pixels.

**Design studies — NOT shippable.** An underscore-prefixed file (or folder) is a reference:
`gen_assets.py` skips it, so it never reaches flash. Park studies that way rather than deleting
them mid-decision, and clear them out once the decision they informed is shipped.

---

## A. Slot icons (carousel)
One per carousel slot. Each icon needs two states (idle dim /
focused bright) — supply one master, brightness handled in engine unless noted.

> **Icon size tiers (`VISUAL_LANGUAGE.md §3.1`):** every `ICON_*` snaps to **28** (slot icons) · **20** (row/
> content glyphs) · **16** (status/button glyphs) · **12** (inline log glyphs) · **8** (inline TEXT-ROW
> glyphs, one `FONT_UI` cell) logical px.

| Asset ID | Slot | Concept | Logical size | States | Status | File |
|---|---|---|---|---|---|---|
| `ICON_STAT`  | 1 STAT  | Heart w/ graph line       | 28×28 | dim/bright | ☑ | `/assets/icons/ICON_STAT.png` |
| `ICON_ITEMS` | 2 ITEMS | USB drive                 | 28×28 | dim/bright | ☑ | `/assets/icons/ICON_ITEMS.png` |
| `ICON_GAMES` | 3 GAMES | Arcade joystick lever     | 28×28 | dim/bright | ☑ | `/assets/icons/ICON_GAMES.png` |
| `ICON_EXPL`  | 4 EXPL  | Wi-Fi mesh globe, 6-frame rotation | 28×28 ×6 | dim/bright; spins while auto-progress is armed, else rests on frame 0 | ☑ | `/assets/icons/ICON_EXPL.png` |
| `ICON_MAINT` | 5 MAINT | Fragmented HDD            | 28×28 | dim/bright | ☑ | `/assets/icons/ICON_MAINT.png` |
| `ICON_MODS`  | 6 MODS  | Cracked CPU               | 28×28 | dim/bright | ☑ | `/assets/icons/ICON_MODS.png` |
| `ICON_ARCH`  | 7 ARCH  | Server rack               | 28×28 | dim/bright | ☑ | `/assets/icons/ICON_ARCH.png` |
| `ICON_CFG`   | 8 CFG   | Gear + terminal           | 28×28 | dim/bright | ☑ | `/assets/icons/ICON_CFG.png` |

> **Masters** are flat `ink`-fill (white) on transparent at logical 28×28; dim/bright is engine brightness (one file each).

---

## B. UI chrome
| Asset ID | Element | Logical size | Notes | Status | File |
|---|---|---|---|---|---|
| `UI_CURSOR_BOX`   | Focused-slot highlight box      | 56×40 (slot)  | accent frame; non-destructive over icon | ☑ | engine-drawn |
| `UI_TRACK_BG`     | Menu track strip (top & bottom) | 224×40        | **solid** (`track` token, S3 resolved) | ☑ | engine-drawn |
| `CAP_SLOT_LABEL`  | Focused category caption style  | ~224×10       | font/size ref from `VISUAL_LANGUAGE.md §2` | ☑ | engine-drawn |
| `UI_ALERT_HUNGER` | Idle low-power / hunger icon    | 20×20         | blink ~1Hz in engine | ☑ | `/assets/ui/UI_ALERT_HUNGER.png` |
| `UI_DEFOCUS_FADE` | Track fade-in/out treatment     | —             | engine alpha; mask only if needed | ⊘ | — |

---

## C. Pet sprite sheets
**One sheet per creature.** Max bounding box 128×64 logical. Every sheet provides the same
frame set so the engine animates any creature identically:

| Frame key | Purpose | Frames |
|---|---|---|
| `idle_a` / `idle_b` | breathe-bob base loop | 2 |
| `blink` | blink (overlay or swap frame) | 1 |
| `flourish_*` | mood flourishes (stretch/spin/hop etc.) | 1–4 |
| `droop` | low-Happiness slumped posture | 1 |
| `weak` | hungry sluggish posture | 1 |

Corruption, channel-shift, and ghost are **engine passes (`FX_*`)** applied on top — **no
per-creature art needed** for them.

### C.1 Which creatures still want art

**There is no roster table here, deliberately.** Every column one would have — creature, line,
stage, sprite id, evolution chain, which hatch gates it — is already a field on a `CreatureDef`
row under `src/core/content/creatures/` (one folder per evolution line), and a second copy in prose
is a copy that goes stale without anything failing. The one thing the code does not carry is whether a delivered
drawing is *good enough*, and that is all this section is for.

**The queue is derivable, so read it from the data, not from here:** a creature wired to a
`SPR_PET_GENERIC_{PROCESS,SCRIPT,DAEMON}` sprite is on a stage placeholder and wants its own
drawing; a creature with its own `SPR_PET_<NAME>` has been drawn.

```sh
grep -oE '"[a-z0-9_]+", *"[^"]+".*"SPR_PET_GENERIC_[A-Z]+"' src/core/content/creatures/*/line.h
```

Swapping a generic for final art is a one-field edit on that row plus the PNG — no engine change,
which is why gameplay ships first and the drawing follows.

**Art notes that live nowhere else.** Only judgement calls, not status:

- **Colour is the line's, not the creature's** — see `CREATURE_VISUAL_RULES.md §4`. Nothing about
  hue belongs in this file.
- **The Worm line has no mother colour at all** — it spends **1-bit line art** as its signature
  instead (`CREATURE_VISUAL_RULES.md §4`), which is the judgement call worth recording: the line
  puts more things on the screen than any other, so every one of them carries less. Every row from
  the Vermicell shell to `SPR_PET_NODEATODE` is an outline with one solid eye, drawn in the same
  vocabulary as the replica glyphs beside it (§C.4) — the parent and its copies are one style at
  two sizes rather than creature art with UI standing next to it. At worm scale the outline is also
  simply the better drawing: a filled silhouette at ~30×24 px says *tube*, and the segment chords
  across an outlined body are what say *worm*.
- **…and that style is held by a tool, not by a hand.** `tools/gen_worm_art.py` carries the line's
  drawing vocabulary — one ink, outline-never-silhouette, segment chords, exactly one solid mass,
  superellipses over ellipses — and each worm-line creature is a RECIPE over it. The vocabulary is
  body-plan agnostic on purpose, since the line's later rows are not all literally worms; what they
  share is the finishing pass, and a recipe cannot opt out of it. `SPR_PET_NODEATODE`,
  `SPR_PET_ROOTGRUB`, `SPR_PET_SHENLOOP` and `SPR_PET_THREADBORE` are generated today — every
  drawn row of the line — and the `worm_art_recipes` ctest fails if a committed sheet and its
  recipe ever disagree. The Vermicell shell and the replica glyphs are still hand-drawn and
  become recipes when next touched — their pixels are approved and shipped, so there is nothing
  to gain from moving them now.
- **Every worm-line sheet is 56×48, including the two Daemons.** The oversized 96×64 Daemon cell
  §7 of `CREATURE_VISUAL_RULES.md` allows is for SINGLE-frame creatures like Cryptoad: a
  multi-row sheet at that size cannot be cut by `gen_assets.py` at all, because `frame_width()`
  only splits 56px frames when the width divides by 56 and `frame_rows()` only finds rows when
  the height divides by 48 — a 4×4 sheet of 96×64 cells is 384×256 and satisfies neither, so it
  would compile as one 384×256 frame. It is also the wrong thing to want here: the line grows
  **heavier, not bigger** (§4 there), so a Worm Daemon buys its stage read with mass, length and
  posture inside the same cell every other row of the family uses.
- **The two Daemons carry no ground contact, and that is load-bearing.** Every crawling row of
  the line ends its recipe with a 1px bar on the shelf. Shenloop (`Swim`) and Threadbore (`Fly`)
  deliberately have no ink near the bottom of the cell, so the drawing agrees with the
  locomotion instead of leaving the habitat's wander to argue a planted sprite off the floor.
- **A wing is a limb carrying a membrane, not a shape stuck on a back.** Threadbore's wings were
  first drawn as one vane hinged flush to the body, and on a creature this fat most of that
  triangle is buried inside the silhouette — what is left above the back reads as a fin or a
  crest. What fixes it is the bare arm: the membrane starts a long way out from the flank, and
  the GAP between limb and body is what says *wing*. The fan beyond the wrist is `vane` used
  several times over, each panel taking the next panel's tip as its heel, so the union's outer
  boundary steps tip to tip and falling radii make it a scalloped trailing edge.
- **A mouth is two different forms, and picking the wrong one costs the creature.** `gape` is a
  jaw in PROFILE — it cuts a wedge from the head and routes the outline around it, so the
  silhouette still closes and the opening reads as hinged. `maw` is a mouth seen down its own
  AXIS — teeth stepping inward off the head's own rim, around a solid throat. Nodeatode has a
  face, so it gapes; Rootgrub's whole front is the opening, so it maws. Swapped, the first reads
  as a chipped edge and the second as a wound. Two small rules make either survive: a tooth is
  rooted just INSIDE the rim (flush leaves it floating, outside makes the creature spiky rather
  than toothed), and an even tooth count keeps the long/short fang alternation from landing two
  long teeth together at the wrap — equal-length spokes around a circle read as a flower.
- **`SPR_PET_PINGCUB` is `▨`** — it has one idle frame and wants a second to match the 2-frame
  norm above. The drawing itself is final.
- **`SPR_PET_WIRE_HEIR` is `▨`, and what it owes is COLOUR SEPARATION, not a redraw.** The drawing is
  the one that is wanted — a long low dachshund turned three-quarter to the viewer, its dark eye box
  carried down from Paypup, a plumed helm answering the horizontal body with a vertical, a heraldic
  device on the surcoat. What it does not yet do is `CREATURE_VISUAL_RULES §2`'s material rule: helm,
  surcoat and cloak sit in the same green as the animal, so the regalia reads as more creature rather
  than as cloth and steel over one. That is a recolour of existing pixels — the forms are already
  separated, only the hues are not — so it is a paint pass on the shipped file rather than another
  generation. `SPR_PET_PWNTHER` is the reference for how far the tones have to move apart.
- **`SPR_PET_BARKMAIL` is `▨`, and what wants redrawing is the MAIL rather than the dog.** Its
  source is 81×66 against the 56×48 Script cell, so it is width-bound decimated onto it and 622
  single-pixel features land on a deleted lattice line. Chain-mail *is* 1px texture, so it absorbs
  almost all of that: the rings degrade into a stipple that reads as an armour drape rather than as
  links. What survives is what matters — the silhouette, the stage read, and Paypup's visor band, the
  one detail carrying the evolution — which is why it ships. **The lesson generalises: decimation
  destroys thin structural LINES and merely coarsens TEXTURE**, so it is survivable exactly when the
  detail at risk is a value field rather than an outline. A redraw wants the mail drawn as a
  deliberately coarser pattern the cell can hold, not the same pattern sourced larger.
- **`SPR_PET_CONKITTENATE` is one 448×48 row, all eight columns of it idle.** It carries the cat
  branch's two-headed signature (`CREATURE_VISUAL_RULES §4`) on Kalico's machined finish held back to
  Process restraint — a few panel seams, one joint puck per haunch, a segmented tail — so the kitten
  reads as the cub Kalico grew out of rather than a different animal. The eight frames are the
  animation round trip below rather than eight drawings: one cell-scale frame in, a sheet out, which
  is why a Process creature can afford a full row where the norm is two frames.
- **A sheet spends either ROWS or COLUMN RANGES on its clips, and the two cost very differently.**
  Malbear puts `idle` and `attack` both on row 0 — 3 columns for the rest, all 8 for the swing — so
  its whole clip set costs one 448×48 row. `SPR_PET_KALICO` spends a row per clip instead, four of
  them, because its clips are genuinely different drawings rather than ranges of one motion. That is
  a real choice and not a free one: at RGB565 + alpha every row is 63 KB of flash (measured, not
  computed — `SPR_PET_PAYPUP` is 42 KB of `_rgb` plus 21 KB of `_a` in the linked image, and nothing
  compresses on the way in), so Kalico's sheet is 252 KB against Malbear's 63 KB. All four of
  Kalico's rows play: `idle`/`walk` split the habitat on whether the wander is moving the anchor, and
  `attack`/`hurt` pose the fight. Prefer Malbear's shape unless the art genuinely differs — a row
  nothing looks up pays the same rent as one that plays.
- **`SPR_PET_KALICO` is `▨` — the drawing is final, the resampling is not.** The source frames were
  60×60 and the cell is 56×48, so the sheet was decimated 4:5 onto the cell: one row and column in
  five deleted. The art is true 1:1 pixel work, so that lands on roughly a fifth of its
  single-pixel features — the outline survives, the finer interior detail does not, and it wants a
  hand pass or a re-source at cell scale. It also carries 133 colours where the line's other sheets
  carry 7–8; that costs nothing in flash, since sprites are stored per-pixel and never palettised,
  but it is why the cat does not yet sit next to Paypup as obviously one family.
- **Generated art reaches cell scale by FRAMING, not by asking for a size.** A generator's width and
  height are a hint — `generate_game_art` reports `size_behavior: "hint"` and returns a canvas of
  its own choosing. **That canvas is not a function of the request**: the same 96×64 ask returns
  100×100 on one call and 200×200 on the next, so nothing downstream may assume a scale, and the
  content box has to be measured every time. What the request does control is how much of that
  canvas the subject fills, so the lever is the prompt: ask for a small subject floating in a wide
  empty margin and the drawing arrives at project scale on its own, whatever the canvas around it. **The framing has to be
  MEASURED to hold** — "a generous margin" fills 90% of the frame anyway, while a quantity ("spans
  no more than half the width, a full quarter empty on the left and a quarter on the right") is
  what moves it. **Ask for far less than the number wanted**: the fraction asked for is a floor the
  subject overshoots, and the overshoot is large and not repeatable. Measured on the phishing
  Daemons — "half the width" returned 76%, "one third" returned 80% on one call and 85% on
  another. Budget for the miss by asking small and letting an under-filled cell stand, since
  under-filling costs nothing and overshooting costs a decimation pass.
  `SPR_PET_CONKITTENATE` has a 48×36 content box inside a 100×100 canvas and `SPR_PET_PWNTHER` an
  86×40 one inside 197×200; both centre into their cells untouched. **The number that matters is the
  content box, never the canvas** — the canvas is padding, and padding is free to discard.
- **A creature is SEATED in its cell, not centred in it — and the cell is a bounding box, not a
  target.** `drawHabitat` bottom-anchors the whole cell, so every logical px of transparent padding
  left under the feet is a px the creature hovers over the shelf: a 44-tall drawing centred in the
  64-tall Daemon cell floats 10 logical px, which is 17 on the panel. The shipped roster seats its
  feet one px off the cell floor and that is the number to reproduce. Nothing about this shows up in
  a sprite viewer — it is only visible against the shelf — so it is a packing rule rather than a
  drawing one. Under-filling the cell is otherwise fine and expected: a corgi is smaller than a
  panther, and the cell only says how much room the drawing is *allowed*, never how much it must use.
- **A dark-armoured creature has to clear the background, and the generator will not do it for you.**
  The habitat paints `PAPER` at luma 19, so a tone at or near it is a hole in the silhouette rather
  than a shadow. Asked for grey armour on a stealthy animal, the generator returns something close to
  37% pure black — which measures as a third of the body missing on the panel and looks perfectly
  fine on a white desktop. `SPR_PET_PWNTHER` therefore drops the line's `#101c12` ink from its
  palette entirely and floors at `#1b241f` (luma 33): still the darkest thing on the creature, still
  reading as an outline, but a clear step above the background it sits on. **Measure the darkest tone
  against `PAPER` before shipping any sprite that leans dark** — the eye cannot catch this on a
  monitor.
- **A punned creature has to draw BOTH halves of its pun.** The Phishing line's anglerfish chain
  first came back with plain angler's lures — a correct fish, and a creature indistinguishable
  from any other deep-sea predator. What makes it *phishing* is that the bait is an INTERFACE:
  `SPR_PET_CLICKBAIT` dangles a notification panel, `SPR_PET_SPAMWHALE` a popup above a mouth of
  glowing filter bars, `SPR_PET_BAITRACUDA` a dialog with a cursor on it and a rig of cursor
  arrows where an angler would carry hooks. The line's own lure organ is the slot the joke goes
  in, and it is inherited down the chain from `SPR_PET_PHISHLET`, so each row spends it on the
  interface its malware type actually baits with. Generation prompts do not volunteer this —
  asked for an anglerfish they draw an anglerfish — so the UI half has to be the loudest
  instruction in the prompt, described as a named widget rather than as "something techy".
- **The teal floor is `#123a40`, and the older phishing sheets predate it.** `SPR_PET_CLICKBAIT`,
  `SPR_PET_SPAMWHALE` and `SPR_PET_BAITRACUDA` measure 0% of their pixels within 12 luma of
  `PAPER`. `SPR_PET_PHISHLET`, `_TADPOLL`, `_CROAKEN` and `_GOLIAUTH` carry 19–34% at that
  measure, because they reuse `#06272b`/`#0d1414`/`#0a0f0f` — at or below the habitat background,
  so those pixels are holes rather than shadow (the same failure `SPR_PET_PWNTHER` fixed for the
  cat branch). Their recolour is a second pass, not a redraw. New art on this line floors at
  `#123a40`; a hand pass that samples ink off a shipped phishing sprite will reintroduce the
  fault, so measure the darkest tone rather than picking it.
- **`SPR_PET_BAITRACUDA` is `▨`, and what it owes is RESOLUTION.** Its source framed at 160×65
  against the 96×64 Daemon cell, so it is width-bound decimated at 3:5 — 874 single-pixel
  features land on a deleted lattice line, against the 622 `SPR_PET_BARKMAIL` ships with. The
  silhouette, the teeth and the cursor rig survive; the thin filaments the cursors hang from
  degrade to dotted lines. Re-sourcing at cell scale is the fix, and the framing lever below is
  how: this cut asked for a third of the canvas width and got four fifths.
- **An animation round trip keeps the pixel scale and loses the palette.** Feeding a cell-scale
  sprite to `animate_game_art` returns frames drawn at that same scale — a 48×36 subject comes back
  47×37 across eight frames — so the sheet crops into the cell with no resampling, which is what
  makes a multi-frame row affordable off one drawn frame. Two things do not survive the trip: every
  hex drifts a channel or two (`#46711a` returns as `#447019`), and the matte colour leaks a pixel or
  two per frame into the silhouette edge. Both are repaired by snapping the sheet back to the line's
  hexes before packing, and neither shows up by eye — the sheet looks right and measures wrong, so
  measure the palette rather than trusting it.
- **An egg line ships ONE egg file, not two.** `SPR_PET_EGG_PHISH_HATCH` and
  `SPR_PET_EGG_WORM_HATCH` are each an 8-frame `56×48` sheet that is both the idle loop (frames
  0–1) and the hatch one-shot (0–7, walked by `Game::hatchCrackFrame`). A separately-drawn
  single-frame egg was byte-identical to frame 0, so shipping it too would only duplicate flash.
- **`SPR_PET_EGG_WORM_HATCH` is `▨`, and 1-bit for the reason above** — the line's signature, not a
  simplification, and the same masks-on-`ink` economy the replicas run on (§C.4).
  A shell with the worm coiled on a track inside it and one loose byte ahead of
  its head: the Isolation Protocol (§C.5) seen from outside the egg. The byte is gone from frame
  5, eaten, which is the frame the shell gives way on — so the hatch reads as something the worm
  DID, not something a timer did to it. Two rules carry the look:
  - **Squared, not round.** Shell and track are both **superellipses at exponent 2.4**, not
    ellipses — flat sides and top with the corners rounded off. At 56×48 a true curve is a
    smoothness the pixels can't deliver, so the drawing commits to the blocky read instead of
    fighting for one.
  - **It squashes under the worm.** The shell is drawn TALL and narrow when the worm's head is at
    the top of its track and WIDE and short when it is at the bottom — the worm's weight is the
    only force in the picture, so the shell answers it. This is also what makes the two-frame idle
    loop work at all: the head moves ~200° per frame rather than creeping, because the head's
    HEIGHT is what drives the squash and two frames only read as motion if that height differs.
- **`SPR_PET_CACHEMUTT` is an enemy frame, not a pet.** No `CreatureDef` points at it; the Sim
  dummy, the EXPL bosses and the Lethal test enemy all borrow it (`game_combat.cpp`, `combat.cpp`).
- **Trojan pets re-skin their origin line** — see `CREATURE_VISUAL_RULES.md §4`.

### C.2 Wild malbeasts (`SPR_MALBEAST_*`)

Wild creatures for encounters/exploration (the 'net). Cell **56×48**, apex tier **64×56**; single
idle frame each, so a wild reads as a still until final sheets follow the §C frame template.
**Every one is currently `▨`** — sized and named correctly, drawn to placeholder quality.

The roster is not listed here either: each wild is a row in `wildMalbeast()`
(`src/core/model/combat.cpp`), which is the only place a wild's sprite is chosen and whose display
names are also the source of truth for `kWildMalbeastIds`, the 'Pedia's seen/defeated masks. A
native test asserts every wild's sprite both resolves AND is distinct from its siblings — sprites
resolve by string at draw time, so a wrong name draws nothing rather than erroring, and that test
is what makes the omission of a table here safe.

> **Sub-area and area bosses still borrow `SPR_PET_CACHEMUTT`** (`subAreaBoss`, same file). Which
> malbeast frame — or bespoke art — each named boss should wear is a design call, not a mechanical
> swap, so it is open.

### C.3 Clutch Pick — the Phishing egg's hatch minigame

The Phishing egg is laid into a raft of decoys and found by halving the clutch three times
(`src/core/app/game/game_eggpick.cpp`). Every decoy is baked into the backdrop; only the live tile
animates, so **motion is the only tell** and the puzzle passes the grayscale gate on its own. The
panel draws at **×2**, not the usual ×1.75 — 112 doubles to exactly the 224 active canvas and keeps
every 14px cell whole-pixel, so the live tile lands dead-on the decoy it replaces.

| Asset ID | Element | Logical size | Notes | Status | File |
|---|---|---|---|---|---|
| `BG_EGG_CLUTCH` | The raft of decoy eggs | 112×56 | 32 slots on a 14px cell grid, 8×4; odd rows shifted 7px, so their last cell wraps across both edges (the live egg never hides there) | ☑ | `/assets/ui/BG_EGG_CLUTCH.png` |
| `SPR_EGG_PHISH_MICRO` | The live egg's clutch tile | 14×14 | 2 frames, alternating on the heartbeat — the tell. Opaque, so it replaces a decoy cell exactly. Needs the `FRAME_W_OVERRIDES` row in `tools/gen_assets.py` to read as a 2-frame strip rather than one 28px image | ☑ | `/assets/sprites/SPR_EGG_PHISH_MICRO.png` |

> Reused, no new art: the aim/eliminate scrim and the aimed-half edge bar are engine fills over
> `PAL_CORE` tokens, and the verdict/round lines are `FONT_UI` text — no tag or overlay art.

### C.4 Worm replicas (`SPR_WORM_REPLICA_*`)

Two glyphs for the **whole Worm line**, not per creature — the same "one shared vocabulary"
economy the `ICON_ITEM_*` family runs on. A worm in combat is a parent plus up to
`kWormReplicaSlots` copies on one shelf (`content_passives.h`, drawn by `combat_screen.cpp`), so
per-creature copies would multiply every worm sheet by two and buy nothing: the read that matters
is **attacker vs defender**, and that is a shape difference, not a portrait.

Both are **1-bit masks on `ink`** — `gen_assets.py` detects that from the pixels rather than the
name, so they cost their own silhouette and nothing more. They are also why the line's parents are
drawn small (`CREATURE_VISUAL_RULES.md §4`): the room the copies stand in comes out of the parent's
cell.

| Asset ID | Element | Logical size | Notes | Status | File |
|---|---|---|---|---|---|
| `SPR_WORM_REPLICA_ATTACK` | Attacking copy | 16×8 ×6 | Round head over a thin stalk — the head implies teeth, and the chomp pair is the only frame where it opens. Needs the `FRAME_W_OVERRIDES` row in `tools/gen_assets.py` to read as a strip | ▨ | `/assets/sprites/SPR_WORM_REPLICA_ATTACK.png` |
| `SPR_WORM_REPLICA_DEFEND` | Defending copy | 16×8 ×6 | Body as wide as its head — a column, no taper, so the two kinds part at a glance in silhouette. Same `FRAME_W_OVERRIDES` row | ▨ | `/assets/sprites/SPR_WORM_REPLICA_DEFEND.png` |

Six frames each, in pairs the renderer picks between by combat state: **0–1 idle** (squiggle),
**2–3 attack** (chomp, played while its parent swings), **4–5 death** (dissolve, played over the
freed slot off `Combat::lastWormKill`).

**The same glyphs also walk the idle habitat.** `Combatant::wormReplicas` is combat state and is
wiped with the fight, so a worm at home has none; the habitat instead draws one ambient copy per
stage raised (`Game::idleCompanionCount`), each crawling the shelf off its own `IdleWander`. It
plays the idle pair only — nothing at home attacks or dies. The frame constants and the seating
helper are shared by both screens in `src/core/ui/worm_replicas.h`, which is what stops a copy
looking like one creature in a fight and another at home.

### C.5 Isolation Protocol — the Worm egg's hatch minigame

**No art at all, and that is the design.** The Worm egg turns its worm loose in a quarantine buffer
and the player steers it into loose bytes (`src/core/app/game/game_isolation.cpp`; the rules are
`src/core/model/isolation.h`). Every mark on the screen is an engine fill over a `PAL_CORE` token
on a 12px cell grid, so the whole screen costs zero flash and restyles with the theme:

| Element | How it is drawn | Why not art |
|---|---|---|
| The buffer wall | 2px `ink` frame **outside** the 16×11 play area | A cell touching the wall has to still be a whole cell — the wall is lethal, so where it starts is arithmetic, not a drawing |
| The worm's body | `ink` cells inset 1px | The 1px seam is what makes a coil countable |
| The worm's head | `accent`, filling its cell edge to edge | Size is the grayscale channel; the tint only repeats it, and accent here is focus, not a status |
| The loose byte | 2px **hollow** `ink` square | Solid-vs-hollow survives any brightness, which matters most on the one thing the player is aiming at |

> Renderable from the catalogue: `./tools/screens.sh` carries `isolation`, `isolation_crash` and
> `worm_egg` scenes (`tools/dump_frame.cpp`, which walks the buffer's Hamiltonian cycle so a frame
> can show a long coil mid-run rather than the three cells it opens with).

### C.6 CHROMATOPHORE — the Metamorphic egg's hatch minigame

**No art either, and for a stronger reason than C.5: the board is a colour, and a colour is a
token.** The Metamorphic egg wears one of three skins against the water it is drifting over
(`src/core/app/game/game_chroma.cpp`; the rules are `src/core/model/chromatophore.h`). The three
skins are the `camo` block in `PAL_CORE.json`, and every mark on the screen is derived from one of
them through `camoRampFromTone` (`src/core/render/camo.h`) — so the water, the chip and the
creature standing in it are provably the same colour, and a retune moves all three together.

| Element | How it is drawn | Why not art |
|---|---|---|
| The water | Ramp mid-tone fill + a per-skin texture (weed / grains / bands) in the ramp's own dark and lit tones | The texture IS the grayscale channel — three skins have to stay three with the hue taken away |
| The creature | `drawSpriteCamo` over the sprite the row already has, at the model's settle | The whole point is that it is the same creature in other colours; a second sheet per skin would be three more sheets per row |
| The sweep | 2px `ink` bar with a blend-to-ink lead, clipped to the water | It is a position, not a picture |
| The three chips | Ramp fill, `ink` frame on the worn one, name in the ramp's opposite end | Shape and word carry the state; the fill is the fast channel only |

The one drawable it wanted was its arcade cabinet glyph (`ICON_ARCADE_CHROMA`, §Q), which is drawn.

> Renderable from the catalogue: `./tools/screens.sh` carries `chroma` (hidden in the water),
> `chroma_half` (mid-repaint), `chroma_spotted`, `chroma_clean` and `meta_egg` — the last being
> the Polystaria egg back at idle, which is the one egg in the roster that DRIFTS
> (`Locomotion::Swim`) rather than sitting where it was laid.

---

## D. Engine effect passes (FX) — no art required

Listed so Design **skips** them; they're implemented per `src/core/render/RENDER_PIPELINE.md`.

| Asset ID | Effect | Stage | Driven by | Status |
|---|---|---|---|---|
| `FX_CORRUPTION` | channel-shift → scanline-tear → dropped-px → glitch-blocks | CORRUPTION | Fragmentation | ⊘ |
| `FX_GHOST`      | Replication Ghost (stipple double) | SPRITE_MODS | Worm frag / failed hatch | ⊘ |
| `FX_EVO_FLASH`  | evolution full-screen flash | SCREEN_FX | evolution event | ⊘ |
| `FX_LOCKOUT_BAND` | flashing red countdown band | SCREEN_FX | Lockout event | ⊘ |
| `FX_CRITICAL_FAIL` | critical-failure crash overlay (composes w/ maxed `FX_CORRUPTION`) | SCREEN_FX | Critical System Failure event | ⊘ |
| `FX_ABSORB` | a glyph breaking into blocks that stream behind the pet, + the pet's swallow flash | SPRITE_MODS (under) | feeding a food · the Wi-Fi event's network discovery · a beaten rival that fielded a move the pet lacks | ⊘ |
| `FX_SHRED` | a sprite shearing into sliding, streaking scanlines that fray out where they stood | SPRITE_MODS (under) | a beaten rival that fielded nothing new | ⊘ |
| `FX_CAMO` | the pet repainted tone-for-tone in the palette worn by the fighter opposite; the change arrives and leaves as a scatter behind a bright burn edge, and holds in between | SPRITE_BASE (in place) | a metamorphic pet whose live cast was rolled out of another line's pool | ⊘ |

> `FX_CAMO` shares the other two's scatter (`core/render/dissolve.h`) so a screen that can
> play more than one of them never looks like it swapped renderers mid-fight. What it does
> not share is a clock, or the having of one: the other two are MOMENTS that run out, while
> this is the pet's COLOUR for as long as it is channelling the borrowed line — it changes
> when the pet's cast changes and not otherwise. It samples its palette off the opponent
> rather than from any table, so a line's colours need no entry here and a lineless
> malbeast still answers with whatever is on it.

> `FX_ABSORB` and `FX_SHRED` are a PAIR, and the combat outro is where the pairing does
> work: one converges on the pet, the other flies apart, so which one closes a fight says
> whether that opponent was worth coming back for. They are dual-coded by SHAPE, not hue —
> the distinction survives a grayscale screenshot, which is why neither needs a caption.

---

## E. Fonts & palette
System specced in `VISUAL_LANGUAGE.md`; the face and the hues are both delivered below. UI chrome
still references a `PAL_CORE` token rather than a literal, so a hue change reskins from one place.

| Asset ID | Element | Status | File |
|---|---|---|---|
| `FONT_UI` | primary UI **pixel** font — tabular digits + disambiguated `0/O 1/I 5/S 8/B` (`VISUAL_LANGUAGE.md §2`). **Face = Pixel Operator Mono**, its own 8px cut (CC0, licence beside the TTF). Rasterised to a glyph table by `tools/gen_font.py` — an authoring step, not a gate, so `gen_assets.py` stays pure-stdlib; the generator refuses any size the face would be antialiased at | ☑ | `/assets/fonts/PixelOperatorMono8.ttf` |

> **One glyph is hand-drawn, not rasterised.** `gen_font.py`'s `OVERRIDES` table replaces the
> face's own cell for `&` (U+0026): Pixel Operator's ampersand at 8px differs from its own `S`
> in two pixels, so `Salted&Hashed Browns` read on the panel as `SALTEDSHASHED BROWNS`. The
> replacement keeps a closed top bowl and throws the leg out to the lower right — the pair of
> features that separates an ampersand from an `S` at any size. The bar for a second override is
> the same one: **illegible, not merely ugly**, because the committed table should be the face.
| `PAL_CORE` | core palette = role tokens in named groups (structural · semantic `calm`/`warn`/`hot` · fragmentation ramp · rarity · team pair · the five `decypher` code colours). Hues set — danger-ascending, `accent` ≠ status. A group exists where a set of colours is a VOCABULARY a theme has to move together, not where one hue was needed | ☑ | `/assets/PAL_CORE.json` |

---

## F. Submenu chrome (shared)
Used by all 8 submenus (shared header/row/scrollbar chrome, built — see `src/core/ui/`). Author
on placeholder palette.

| Asset ID | Element | Logical size | Notes | Status | File |
|---|---|---|---|---|---|
| `UI_SUBMENU_HEADER` | Header band: title (left) + position indicator (right) | 224×24 | shorthand text + dots or `n/total` | ☑ | engine-drawn |
| `UI_HINT_BAND` | Contextual control hint — **self-contained screens** | 224×24 | a screen the player ARRIVED at (event, activity, modal, full-screen reader) names what's live on it, whether or not the mapping is standard; the carousel's own list/detail spine doesn't, because A/B/C is constant there. See the note under this table. `widgets.h`'s `drawHintBand` + `kHintBandH` | ☑ | engine-drawn |
| `UI_PAGER_DOTS` | Page indicator for viewer/paged screens | ~24×8 | filled = current page | ☑ | engine-drawn |
| `UI_CURSOR_ROW` | Focused-row cursor marker `▸` | 12×28 | non-destructive over row | ☑ | engine-drawn |
| `UI_ROW_SEL` | Focused-row accent fill | 224×28 | semi-opaque highlight behind row | ☑ | engine-drawn |
| `UI_SCROLLBAR` | Slim right-edge scroll position bar | 4×176 | shows when list > 6 rows | ☑ | engine-drawn |
| `UI_LIST_HEADER` | Grouped-list section header row | 224×16 | non-selectable; cursor skips (e.g. `FOOD`/`BUFFS`) | ☑ | engine-drawn |
| `UI_PROGRESS_BAR` | Reusable horizontal progress/fill bar | ~180×12 | EXPL walk, MAINT defrag/AV, hold-to-commit. **= `UI_GAUGE` variant** (neutral, no zone colour). `widgets.h`'s `drawProgressBar`; a sprite could not carry it — the fill is a runtime fraction, and `churn` dithers its leading edge per beat | ☑ | engine-drawn |

> **On `UI_HINT_BAND`'s rule.** It was originally specced exception-only — a band ONLY
> where a screen broke the standard A/B/C contract. What shipped is broader and better:
> every self-contained context draws one (EXPL's zone and sub-area lists, the storefronts,
> the Warp picker, the post-encounter and combat screens, the duel screens, the Clutch
> Pick, the MOVE reader, CFG's TITLE picker), naming what's live even when the mapping is
> ordinary — because a player who just arrived somewhere shouldn't have to infer the
> controls from the layout. The carousel's own submenu spine (STAT/ITEMS/MODS/ARCH/CFG
> **lists**) still goes without, since A/B/C never changes there; an L3 picker hanging off
> one is a self-contained context and does draw a band. A band is still REQUIRED wherever
> the mapping is non-standard; it is no longer limited to those cases.
>
> Drawn by `widgets.h`'s `drawHintBand` — one filled strip pinned to the canvas foot. The
> fill is the load-bearing part, not the dimming: a hint set as bare text a row-pitch under
> a list reads as one more entry in the list.

---

## G. STAT submenu
| Asset ID | Element | Logical size | Notes | Status | File |
|---|---|---|---|---|---|
| `UI_CARE_PIPS` | Care-mistake pips | ~84×16 | 2 Good + 3 Bad + gate divider + 3 colour states + numeric (0–5) | ☑ | engine-drawn |
| `ICON_LOG_EVENT` | Hacker-Log glyph set (3) | 12×12 ea | `item` · `warn` · `combat` | ☑ | `/assets/icons/ICON_LOG_EVENT_{ITEM,WARN,COMBAT}.png` |

> STAT's Hunger/Frag/Happiness **gauge widgets** + Stage indicator are built; assets in §O below.

---

## H. ITEMS submenu

**No per-item roster.** An item's glyph id is *derived*, not recorded: `itemIcon()`
(`src/core/content/effect_text.cpp`) uppercases the item id into `ICON_ITEM_<ID>` and looks that
up. So the item list is `content_items.cpp`, the icon list is `assets/icons/`, and a table pairing
them here would be a third copy that can disagree with both. A row added without art draws a
blank — that is the prompt to draw one, and `check_orphan_assets.py` catches the reverse.

| Asset ID | Element | Logical size | Notes | Status | File |
|---|---|---|---|---|---|
| `UI_RARITY_TAG` | Rarity tag style (Common/Uncommon/Rare/Epic) | ~44×14 | ramp set: `rarity-common…epic`, dull→bright, read by position; tag style in board | ☑ | `/assets/PAL_CORE.json` |

**The art rules the ids don't carry:**

- **Cache rarity is chevrons, counted.** `ICON_ITEM_SEALED_CACHE_{COMMON,UNCOMMON,RARE,EPIC}` stack
  1–4 chevrons upward from the base cache, so the tier is *countable* and the ramp survives
  grayscale rather than relying on the rarity hue.
- **`ICON_ITEM_COMMEND_CACHE` inverts the fill instead** — hollow shell plus a star, not a fifth
  chevron — because a commendation cache is earned rather than found. It reads as a different KIND
  of container, not a higher tier. (A crowned "major" variant is parked in `_attic/`; `commend_cache`
  is a single item row, so there is no second tier for it to mark yet.)
- **The pantry was drawn as one batch.** The staple ingredients (`content_items.cpp`'s STAPLE
  INGREDIENTS block) plus the two dishes cooked from them are one coherent 20×20 food set, so the
  shelf reads as a pantry rather than twenty unrelated glyphs. Three are legible AS PAIRS: Fresh
  Macrol is a whole fish where Spoiled Macrol is its skeleton; C-Salt and Desalinated C-Salt share
  a cap and differ only in whether the body has anything in it; the two Browns are the same branded
  patty with and without salt above it. Distinctness was the brief — they sit in one scrolling
  list — so no two share a silhouette: tin · crumbs · shaker · cruet · holed flask · fish ·
  pouch-and-clock · cubes · vial · spuds · yolk-in-shell · leek · cereal box · mug · sachet ·
  apple · taproot · noodle bowl · patty.
- **The cooked dishes are the pantry's second and third batches** (fourteen of them now), drawn to the same brief and held to
  the same rule: no two silhouettes alike, in a list where they sit together. Where the staples are
  mostly upright containers, these lean on outlines nothing else in the bag has — a pair of
  DIAGONAL rolls (Cracquettes), a leaning parallelogram (Serial Bar), a long-handled pan seen from
  above with two yolks open in it (Hackshuka), a vented dome (Applet Turnover), a lidded takeaway
  cup (Vanilla Java Roast), and a pot with a gnu's horns sweeping off its rim (GNUlash — the one
  glyph that is a pun rather than a picture). **Macrol Fry-Up is the deliberate exception**: it is
  Fresh Macrol's own fish under the steam mark, because the recipe's whole claim is that it is the
  same fish, cooked, and no longer going off. Steam is the family's shared "it's hot" mark, carried
  over from `ICON_ITEM_JAVA`. The later batch keeps going the same way: chopsticks
  standing in a bowl (RAMen — Null Noodles is already a bowl, so the sticks are the whole
  silhouette), a slab of stacked layers (LANsagne), a dimpled torn flatbread (Forkaccia), a wide
  low plate heaped with grain (RISCotto) against Hackshuka's deep round pan, a dusted tumbler
  (Tiramisudo), two pleated pouches (Core Dumplings), and a SQUARE lidded bake with side handles
  (Cacherole) against GNUlash's round horned pot. Every near-miss in that list is a pair drawn
  apart on purpose: bowl/pan, pot/bake, layers/dimples.
- **`ICON_ITEM_FULLY_STACKED_NACHOS` is `▨`** — it reuses `ICON_ITEM_TORTILLA_CHIP` rather than
  being bespoke. The only item icon that is a stand-in.
- **`ICON_ITEM_BOOT_ACCELERATOR` and `ICON_ITEM_ROLLBACK` are the set's two BARE symbols** — no
  object body, just the mark. That is deliberate and it is a pairing: one skips a clock forward
  (a double fast-forward chevron, tips cut flat at 2px because a 1px apex is noise at this size),
  the other winds a level back (a curved rewind arrow). Everything else in the family is a thing
  you could hold. The egg accelerator and the Decryptogram are two items now, and the ticket kept
  `ICON_ITEM_DECRYPTOGRAM` — a sealed document, which is what a cryptogram is.

---

## I. LOADOUT hub (MODS submenu, L2)
| Asset ID | Element | Logical size | Notes | Status | File |
|---|---|---|---|---|---|
| `ICON_LOADOUT_MOVES` | MOVES row glyph | 20×20 | targeting reticle | ☑ | `/assets/icons/ICON_LOADOUT_MOVES.png` |
| `ICON_TRAIN_SIM` | PRACTISE (Sim-Battle) row glyph | 20×20 | the hub's one door to the dummy fight | ☑ | `/assets/icons/ICON_TRAIN_SIM.png` |

> The MODS row reuses §L's `ICON_MODS_SLOT` — the hub row and the list it opens are
> the same subject, so they carry the same glyph.

---

## J. EXPL submenu
| Asset ID | Element | Logical size | Notes | Status | File |
|---|---|---|---|---|---|
| ~~`UI_BANDWIDTH_METER`~~ | ~~Steps-remaining (Bandwidth) meter~~ | — | **RETIRED** — exploration is a toggleable idle-mode with unlimited auto-steps; no Bandwidth budget | ⊘ | — |
| `UI_EXPLORE_BADGE` | Idle explore-mode badge | 16×16 | `⟳` mode glyph + sub-area + `WINS n/10` / `BOSS READY`; dual-coded; on idle while exploring | ⌫ | `/assets/_attic/UI_EXPLORE_BADGE.png` (delivered as ICON_EXPLORE_BADGE; renamed to canonical ID on integration) |
| `ICON_EXPLORE_STATE` | Sub-area row state marker (EXPL) | 16×16 | *optional* — exploring/cleared/boss-ready/locked; else `FONT_UI` tags | ☐ | |
| `UI_DIFFICULTY_PIPS` | Sector difficulty tier (filled/empty diamonds) | ~30×10 | e.g. `◆◇◇` | ⌫ | placeholder `/assets/_attic/UI_DIFFICULTY_PIPS.png` |
| ~~`ICON_EXPL_PACKET`~~ | ~~Packet Capture row glyph~~ | — | **REMOVED** — Packet Capture minigame scrapped; "packet sniffing" is now the Wi-Fi explore event | ⊘ | — |
| `ICON_SECTOR_<AREA_ID>` | Per-area row glyph, EXPL zone picker | 20×20 | one per area, named on that area's own `AreaDef::icon`; the DeepWeb Dive has no AreaDef and names `ICON_SECTOR_DEEPWEB_DIVE` from `areas/deepweb_dive/area.cpp` instead | ☑ | `/assets/icons/ICON_SECTOR_*.png` |
| `BG_SECTOR_<AREA_ID>` | Per-area walk backdrop | 128×128 | optional; flat colour OK v1 | ⌫ | placeholders `/assets/_attic/BG_SECTOR_CITRUS_CIRCUIT.png` + `BG_SECTOR_PIRATE_BAYOU.png` |

> An area's identity (name/icon/backdrop) is swappable data; its difficulty is its rung.
> **Naming direction:** real-world malware-encounter places, punned (LimeWire → "Citrus
> Circuit").
>
> **`<AREA_ID>` is the area's own `AreaDef::id`, upper-cased — never its ladder position.**
> A rung is not an identity: splicing an area into the middle of `kAreaList`
> (areas/area_defs.h) renumbers every area above it, and an index-keyed file would keep
> resolving while pointing at its neighbour's picture. The id doesn't move, so the art
> can't. Each area names its glyph on its own row (`AreaDef::icon`), which is also what
> keeps it out of `check_orphan_assets.py`'s KEEP list. Areas wanting art:
> Pirate Bayou · Net-Sea Crossing · Napstorrent Moors · Castle Rapidscare.
> Only the first area is open at start; rest progression-gated. Packet Capture minigame + wild-
> encounter combat art are deferred. Walk reuses pet idle frames — no walk frame.

---

## K. MAINT submenu
| Asset ID | Element | Logical size | Notes | Status | File |
|---|---|---|---|---|---|
| `ICON_MAINT_DEFRAG` | Defragmentation row glyph | 20×20 | | ☑ | `/assets/icons/ICON_MAINT_DEFRAG.png` |
| `ICON_MAINT_AV` | Antivirus (AV) row glyph | 20×20 | | ☑ | `/assets/icons/ICON_MAINT_AV.png` |
| `ANIM_DEFRAG` | Defrag block-shuffle process anim | — | OPTIONAL/flavour; modal process visual, NOT a §4 pass; procedural OK | ☐ | |
| `ANIM_AV_SWEEP` | AV scan-sweep process anim | — | OPTIONAL/flavour; modal process visual, NOT a §4 pass; procedural OK | ☐ | |

> Both processes reuse `UI_PROGRESS_BAR`. Replication Ghost is the existing `FX_GHOST` pass —
> AV clears it; no new art. Animations are optional polish — progress bar suffices for v1.

---

## L. MODS submenu

**No per-mod roster, for the same reason as §H** — `mods_screen.cpp` builds `ICON_MOD_<UPPER ID>`
from the mod id, and `train_screen.cpp` does the same for `ICON_MOVE_<UPPER ID>`. The rosters are
`content_mods.cpp` and `content_moves.cpp`; the drawings are `assets/icons/`. Effect tags
(`+def`/`+spd`/`1-shot`) are `FONT_UI` text, so a new mod needs one PNG and no row here.

| Asset ID | Element | Logical size | Notes | Status | File |
|---|---|---|---|---|---|
| `ICON_MODS_SLOT` | Equip-slot row glyph (+ empty variant) | 20×20 | filled vs empty slot — chrome, not per-mod | ☑ | `/assets/icons/ICON_MODS_SLOT{,_EMPTY}.png` |

---

## M. ARCH submenu
| Asset ID | Element | Logical size | Notes | Status | File |
|---|---|---|---|---|---|
| `ICON_ARCH_SLOT` | Rack record glyph (+ retired variant) | 20×20 | active/frozen vs retired | ☑ | `/assets/icons/ICON_ARCH_SLOT{,_RETIRED}.png` |
| `UI_SLOTS_USED` | Rack-slot usage indicator (`slots 2/4`) | ~44×12 | header widget. `arch_screen.cpp` formats `SLOTS n/m` into `drawHeaderBand`'s right slot, so it reads off the live cap rather than a bare tunable — text, not art | ☑ | engine-drawn |

> Rack rows reuse `SPR_PET_*` idle frame as a thumbnail — no new art. `[ACTIVE]`/`[RETIRED]`
> are `FONT_UI` text tags; greying is engine dim. Stored pets consume rack slots; records
> don't. New-egg / line-select is a contextual modal, not an ARCH asset.

---

## N. CFG submenu
| Asset ID | Element | Logical size | Notes | Status | File |
|---|---|---|---|---|---|
The list is **six rows** — SYSTEM INFO · HACKERTAG · TITLE · DEVICE · RADIO · UPDATES — which is
exactly the viewport, so it never scrolls. DEVICE and RADIO are **group screens**: each draws its
own rows from the same `CfgRow` shape and reuses the glyphs below, so grouping needs no new art.

| Asset ID | Element | Logical size | Notes | Status | File |
|---|---|---|---|---|---|
| `ICON_CFG_SYSINFO` | System Info row glyph | 20×20 | the RADIO group's AUDIT row names it in `cfgGroupRows`, but nothing draws it — see the note under this table | ☑ | `/assets/icons/ICON_CFG_SYSINFO.png` |
| `ICON_CFG_TAG` | HackerTag row glyph | 20×20 | | ☑ | `/assets/icons/ICON_CFG_TAG.png` |
| `ICON_CFG_UIMODE` | UI Mode row glyph | 20×20 | also the DEVICE group row + BRIGHTNESS | ☑ | `/assets/icons/ICON_CFG_UIMODE.png` |
| `ICON_CFG_TITLE` | TITLE row glyph (zone-Title picker) | 20×20 | v1 stopgap home for zone Titles; moves to Hacker HUD later | ☑ | `/assets/icons/ICON_CFG_TITLE.png` |
| `ICON_CFG_RADIO` | RADIO group row glyph | 20×20 | the four radio consents under one row. A transmitter mast, not the square-wave alternate parked at `/assets/_attic/ICON_SYS_WIFI_ALT.png`: the split it has to carry is "the radio, as hardware" against "a Wi-Fi service", and a squared-off fan is still the fan `ICON_SYS_WIFI` draws on PEDIA AP + INTERNET. A mast also covers both consent axes at once — it is the thing that listens and the thing that transmits | ☑ | `/assets/icons/ICON_CFG_RADIO.png` |
| `ICON_CFG_UPDATE` | UPDATES row glyph | 20×20 | a refresh cycle — a ring opened at the top and fed an arrowhead. NOT a download arrow, which reads as the row's obvious motif right up until you set it beside `ICON_SECTOR_NAPSTORRENT_MOORS`: the Moors are the torrent area and the arrow-into-a-tray is theirs | ☑ | `/assets/icons/ICON_CFG_UPDATE.png` |
| `ICON_CFG_TRAVEL` | TRAVEL MODE row glyph (DEVICE group) | 20×20 | a crescent. It is the one row in the group that is an action rather than a setting, and the action is sleep | ☑ | `/assets/icons/ICON_CFG_TRAVEL.png` |
| `ICON_SYS_BATTERY` | Battery status glyph | 16×16 | System Info | ⌫ | `/assets/_attic/ICON_SYS_BATTERY.png` |
| `ICON_SYS_WIFI` | Wi-Fi AP status glyph | 16×16 | System Info; also the INTERNET row. Named by PEDIA AP too, and unrendered there for the same reason AUDIT's is. Also the thing the pet EATS on the EXPL Wi-Fi event (`FX_ABSORB`, §D), drawn there at the creature ×1.75 rather than at chrome size — so it is read as a subject, not as a status glyph, and a redraw has to hold up at both | ☑ | `/assets/icons/ICON_SYS_WIFI.png` |
| `ICON_SYS_SD` | SD-card status glyph | 16×16 | System Info | ☑ | `/assets/icons/ICON_SYS_SD.png` |
| `ICON_CFG_QR` | Pedia QR row glyph | 20×20 | **drawn but unconsumed** — the QR is reached from PEDIA AP, not a row of its own, so nothing renders this. Keep for a future row; it costs atlas space until then | ⌫ | `/assets/_attic/ICON_CFG_QR.png` |

> Pedia QR is engine-generated (QR lib) — no art. HackerTag editor uses `FONT_UI` + caret —
> no art. SD RECHECK has no glyph: it is the A press on System Info, beside the SD line it
> reports through. Factory Reset is **hidden** (no row glyph): revealed by hold-B on System
> Info, committed by hold-B-5s; reuses `UI_PROGRESS_BAR` for both hold bars.
>
> **The RADIO group's three rows draw no glyph at all.** `radioRow` (`cfg_screen.cpp`) renders
> the on-air mark, the label and the value, and never touches `CfgRow::icon` — the ●/○ pair is
> the one thing the screen exists to say, and an icon column beside it would compete with the
> mark for the same read. So PEDIA AP, LINK and AUDIT each name an icon in `cfgGroupRows` that
> is never drawn, and none of the three is a row waiting for art: a bespoke `ICON_CFG_AUDIT`
> (headphones — the axis that listens) was drawn and parked at `/assets/_attic/`, because
> giving it a home means redesigning the screen, not drawing a glyph.

---

## O. Stat visualisation
The **gauge language is defined once** here: `UI_GAUGE` is
the canonical segmented-bar primitive and `UI_PROGRESS_BAR` (§F) + `UI_BANDWIDTH_METER` (§J) are
its variants. Colour roles (calm/warn/hot) bind to `PAL_CORE` tokens (§E).

| Asset ID | Element | Logical size | Notes | Status | File |
|---|---|---|---|---|---|
| `UI_GAUGE` | Segmented-bar primitive (shared gauge) | ~120×12 (10 cells = 10%) | 3 zone states + ~1Hz Critical pulse | ☑ | engine-drawn |
| `UI_STAT_GAUGE` | Labelled vitals row: label + `UI_GAUGE` + numeric | ~208×24 | composition over `UI_GAUGE` | ☑ | engine-drawn |
| `UI_STAGE_INDICATOR` | 4-node lifecycle track (Boot→Process→Script→Daemon) | ~200×16 | current bright+named · past lit · future dim | ☑ | engine-drawn |

> `UI_CARE_PIPS` lives in §G. Gauges are STAT-screen chrome, **not**
> idle-pipeline passes — the Critical pulse is a UI repaint, distinct from the
> procedural `FX_CORRUPTION` sprite glitch.

---

## P. Event overlays
Modal interrupt-layer events (Hatch, Feeding, Lockout, Evolution, Critical System Failure); full-screen FX use the
**SCREEN_FX** pass (registered in §D). Most chrome is reused — only the rows
below are new art.

| Asset ID | Element | Logical size | Notes | Status | File |
|---|---|---|---|---|---|
| `ICON_BTN_A` | Button glyph `A` | 16×16 | reusable wherever a literal button is shown | ⌫ | `/assets/_attic/ICON_BTN_A.png` |
| `ICON_BTN_B` | Button glyph `B` | 16×16 | | ⌫ | `/assets/_attic/ICON_BTN_B.png` |
| `ICON_BTN_C` | Button glyph `C` | 16×16 | | ⌫ | `/assets/_attic/ICON_BTN_C.png` |
| `ICON_LINE_RANSOMWARE` | Line-select row glyph — Ransomware | 20×20 | one per creature line | ☑ | `/assets/icons/ICON_LINE_RANSOMWARE.png` |
| `ICON_LINE_WORM` | Line-select row glyph — Worm | 20×20 | add `ICON_LINE_*` per line as unlocked | ☑ | `/assets/icons/ICON_LINE_WORM.png` |
| `UI_COUNTDOWN` | Lockout countdown digits/style | ~64×24 | `FONT_UI`-based; pairs w/ `FX_LOCKOUT_BAND`. `modals.cpp`'s `drawLockoutModal` sets `00:SS` in the header band, flashing on the beat, over a `UI_PROGRESS_BAR` of the time left — a style over the shared font, never its own glyph set | ☑ | engine-drawn |

> Reused, no new art: `FX_LOCKOUT_BAND` / `FX_EVO_FLASH` / `FX_CRITICAL_FAIL` / `FX_GHOST` /
> `FX_CORRUPTION` (§D), `UI_HINT_BAND` (§F), `UI_STAT_GAUGE` / `UI_STAGE_INDICATOR` /
> `UI_CARE_PIPS` (§G/§O), `SPR_PET_*` egg + Stage-1 frames (§C), `ICON_ITEM_*` (§H). Worm
> isolate's "real egg" tint is engine, no art. **Egg-crack** = optional procedural overlay or a
> few `SPR_PET_*` frames (D-side, non-blocking).

---

## Q. Minigames & Combat
**Autonomous auto-battle** shared by wild encounters + Sim-
Battle. Combat Health/charge bars are **`UI_GAUGE` variants**, not new primitives;
combat hit/clash FX are **procedural activity visuals, NOT pipeline passes**.
Most chrome is reused — only the rows below are new, and most are optional polish.

**The GAMES arcade needs one glyph per cabinet** (`ArcadeGameDef::iconName`,
`content_arcade.cpp`) and nothing else: it draws the menu round a minigame and never the
game, so every screen inside a cabinet is that game's own. Most cabinets reuse a glyph
that already names their subject — `ICON_MAINT_DEFRAG` (§K) for the Stacker,
`ICON_LINE_WORM` for the Isolation Protocol, `ICON_LINE_RANSOMWARE` for Disk Decypher
and `ICON_ITEM_DECRYPTOGRAM` for the quote board — and only a cabinet whose subject has
no glyph anywhere else needs one drawn.

| Asset ID | Element | Logical size | Notes | Status | File |
|---|---|---|---|---|---|
| `ICON_ARCADE_CLUTCH` | SPOT THE PHISH cabinet row glyph | 20×20 | fish hook | ☑ | `/assets/icons/ICON_ARCADE_CLUTCH.png` |
| `ICON_ARCADE_CHROMA` | CHROMATOPHORE cabinet row glyph | 20×20 | one bell parted down the middle — the same creature wearing two skins at once, which is the board's whole idea. At this size one deliberate cut reads where a stripe pattern only reads as damage | ☑ | `/assets/icons/ICON_ARCADE_CHROMA.png` |
| `UI_HEALTH_BAR` | Combat-Health row | ~208×24 | `UI_GAUGE` variant — transient Health | ☑ | engine-drawn |
| `UI_OVERRIDE_PIP` | Once-per-battle Exploit-override indicator | 16×16 | ready (bolt) / spent (×) | ☑ | `/assets/icons/ICON_OVERRIDE_PIP{,_SPENT}.png` |

### Q.1 The combat VS grid (`ICON_FIGHT_*`)

The stat panel's VS page names each row with a glyph instead of a word (`combatVsGlyph`,
`core/ui/combat_screen.h`). They are the **8×8 tier** — one `FONT_UI` cell — because they sit
INSIDE a text row: every larger tier stands taller than the 11px a panel row gets, so an icon
drawn from one would cost the rows the grid exists to save.

Each is a flat 1-bit master, tinted at draw time by `combatVsColor` (`ui/theme.h`). Hues repeat
across the set on purpose — speed and a stun are both `warn`, a wall and a bubble both
`team-blue` — so the SHAPE is what separates them and the tint only ever repeats it. Every row
also keeps its word (`CombatVsRow::tag`), which is what draws if a master goes missing.

| Asset ID | Row | Concept | Tint | Status | File |
|---|---|---|---|---|---|
| `ICON_FIGHT_HP` | HP | heart | `calm` | ☑ | `/assets/icons/ICON_FIGHT_HP.png` |
| `ICON_FIGHT_PWR` | PWR | sword, point up | `team-red` | ☑ | `/assets/icons/ICON_FIGHT_PWR.png` |
| `ICON_FIGHT_DEF` | DEF | brick wall — DEF is the STANDING damage cut, so it is the structure that is always there | `team-blue` | ☑ | `/assets/icons/ICON_FIGHT_DEF.png` |
| `ICON_FIGHT_SPD` | SPD | double chevron | `warn` | ☑ | `/assets/icons/ICON_FIGHT_SPD.png` |
| `ICON_FIGHT_STUN` | STUN | squared spiral | `warn` | ☑ | `/assets/icons/ICON_FIGHT_STUN.png` |
| `ICON_FIGHT_DOT` | DOT | three bubbles, three sizes | `hot` | ☑ | `/assets/icons/ICON_FIGHT_DOT.png` |
| `ICON_FIGHT_SHLD` | SHLD | bubble with a highlight — the Obfuscation pool | `team-blue` | ☑ | `/assets/icons/ICON_FIGHT_SHLD.png` |
| `ICON_FIGHT_GRD` | GRD | shield, raised — GRD is the ONE-SHOT brace, spent on the next hit, which is what raising a shield is. Straight-sided so it does not read as the heart | `team-blue` | ☑ | `/assets/icons/ICON_FIGHT_GRD.png` |
| `ICON_FIGHT_RNSM` | RNSM | a note with a figure on it | `calm` | ☑ | `/assets/icons/ICON_FIGHT_RNSM.png` |
| `ICON_FIGHT_BKUP` | BKUP | floppy — shutter above, label below | `ink-dim` | ☑ | `/assets/icons/ICON_FIGHT_BKUP.png` |
| `ICON_FIGHT_TRAP` | TRAP | snare jaws | `ink-dim` | ☑ | `/assets/icons/ICON_FIGHT_TRAP.png` |
| `ICON_FIGHT_COPY` | COPY | two overlapping bodies | `ink-dim` | ☑ | `/assets/icons/ICON_FIGHT_COPY.png` |
| `UI_MOVE_CHANNEL` | Multi-turn move wind-up | ~120×12 | `UI_GAUGE` variant; override decision cue | ☑ | engine-drawn |
| `ICON_MOVE_SLOT` | Loadout equip-slot row glyph | 20×20 | filled / empty / locked variants | ☑ | `/assets/icons/ICON_MOVE_SLOT{,_EMPTY,_LOCKED}.png` |
| `ICON_MOVE_<ID>` | Per-move glyph | 20×20 | derived at draw time from the move id (`train_screen.cpp`), so a new move's icon needs no wiring — drop the PNG in `assets/icons/` and it lights up. MOVES falls back to text for a move with none, which is why most of the roster has no glyph yet: `ls assets/icons/ICON_MOVE_*` against `content_moves.cpp` is the real count | ☑/▨ | `/assets/icons/` |
| `UI_DAMAGE_POPUP` | Floating damage number | — | `FONT_UI` tabular digits — **procedural, no art** | ⊘ | — |
| `SPR_DUMMY` | Sim-Battle training-dummy sprite | ≤128×64 | wired for both tiers in `simDummy()` (`src/core/model/combat.cpp`) — they are the same prop, and the tier reads off the level/stat rows | ▨ | `/assets/sprites/SPR_DUMMY.png` (56×48) |
| `ICON_EVENT_WIFI` | Wi-Fi network event glyph | 20×20 | **optional**. The event draws `ICON_SYS_WIFI` for this today; a dedicated glyph would take its place as the thing the pet eats, so it is judged at creature scale mid-dissolve rather than as a 20×20 chrome mark — a fan of few, chunky strokes breaks into a better mouthful than a thin one | ⌫ | placeholder `/assets/_attic/ICON_EVENT_WIFI.png` |
| `UI_RANK_BADGE` | Hacker-Rank badge (CFG / HackerTag) | ~16–20 | **optional**; else plain `FONT_UI` rank text | ⌫ | placeholder `/assets/_attic/UI_RANK_BADGE.png` (20×20) |

> **Reused (no new art):** `UI_GAUGE` (§O), `UI_DIFFICULTY_PIPS` (§J), `UI_HINT_BAND` /
> `UI_CURSOR_ROW` / `UI_ROW_SEL` / `UI_LIST_HEADER` / inline confirm (§F), `UI_BANDWIDTH_METER` (§J),
> the loot/result overlay (reused for drops **and** rank-up), `SPR_PET_*` idle frames for
> both combatants + the awakened-guardian malbeast (§C), `FX_CORRUPTION` (§D) for the glitch/decoy/
> awakened tell, `ICON_TRAIN_SIM` (§I), `ICON_ITEM_SINKHOLE_TRAP` / `ICON_ITEM_TORTILLA_CHIP` (§H),
> `ICON_MOD_PACKET_SNIFFER` (§L), `FONT_UI` / `PAL_CORE` (§E).
> **Optional `SPR_PET_*` add:** a single `attack`/`lunge` pose frame on the §C frame template
> — not required for v1.

---

## R. Hacker face — Crew Selection
**Scope: the archetype submenu's own art** — the Hacker-face terminal centre-canvas treatment,
PROFILE/SHOP/VAULT/MERGE HUB, the CREW slot (enlistment + home network), PEERS and LINK (1v1
duels) are all built; the Red/Blue archetype system that would consume the per-archetype rows is
not (the Hacker-face CREW row on `docs/MASTER_TODO.md`). Author on
placeholder palette; team Red/Blue hues + status/accent separation are still unset. Most
chrome is reused from §F — only the rows below are new, and per-archetype icons are optional.

| Asset ID | Element | Logical size | Notes | Status | File |
|---|---|---|---|---|---|
| `ICON_CREW` | Hacker-face **slot icon** (CREW) | 28×28 | dim/bright; crew duality (sword+shield) | ☑ | `/assets/icons/ICON_CREW.png` |
| `ICON_TEAM_RED` | Team marker — Red · Operators | 16×16 | dual-coded glyph (sword) | ☑ | `/assets/icons/ICON_TEAM_RED.png` |
| `ICON_TEAM_BLUE` | Team marker — Blue · Guardians | 16×16 | dual-coded glyph (shield) | ☑ | `/assets/icons/ICON_TEAM_BLUE.png` |
| `ICON_ARCHETYPE_STATE` | Archetype row marker | 16×16 | active / unlocked / locked states; parked with the per-archetype set below, for the same reason | ⌫ | all 3 parked at `/assets/_attic/ICON_ARCHETYPE_{ACTIVE,UNLOCKED,LOCKED}.png` |
| `ICON_LOCK` | Generic small lock glyph | 12×12 | locked rows + unlock hint | ☑ | `/assets/icons/ICON_LOCK.png` |
| `ICON_ARCHETYPE_<name>` | Per-archetype icon (6) | 20×20 | **optional** polish; seed `BOTMASTER`·`INSIDER_THREAT`·`GHOST`·`ORCHESTRATOR`·`WATCHDOG`·`DISPATCHER`; v1 may run text-only | ⌫ | all 6 parked at `/assets/_attic/ICON_ARCHETYPE_*.png` — the archetype system that would draw them is unbuilt |
| `ICON_PROFILE` | Hacker-face PROFILE slot icon | 28×28 | dim/bright; hacker-status identity motif | ☑ | `/assets/icons/ICON_PROFILE.png` |
| `ICON_SHOP` | Hacker-face SHOP slot icon | 28×28 | dim/bright; storefront/marketplace motif | ☑ | `/assets/icons/ICON_SHOP.png` |
| `ICON_VAULT` | Hacker-face VAULT slot icon | 28×28 | dim/bright; safe/lockbox motif — sealed-cache decrypt | ☑ | `/assets/icons/ICON_VAULT.png` |
| `ICON_MRG` | Hacker-face MERGE HUB slot icon | 28×28 | dim/bright; two nodes branching into one. The slot starts inaccessible, so it usually draws under `ICON_SLOT_INACCESSIBLE` — the glyph is legible through the double-dim, which is what makes the SHOP purchase read as unlocking a real slot | ☑ | `/assets/icons/ICON_MRG.png` |
| `ICON_MRG` locked variants | Bespoke locked Merge glyphs (padlock badge · severed branch + padlock) | 28×28 | ⌫ parked at `/assets/_attic/ICON_MERGE_LOCKED{,_ALT}.png` — the carousel composites the shared `ICON_SLOT_INACCESSIBLE` marker over every inaccessible slot, so a per-slot locked master would make MRG the one exception to a device-wide convention | ⌫ | `/assets/_attic/ICON_MERGE_LOCKED{,_ALT}.png` |
| `ICON_PEERS` | Hacker-face PEERS slot icon | 28×28 | dim/bright; one device over a bus with three operators hanging off it. A figure motif would have collided with CREW's two busts and PROFILE's framed one, so the slot says *the roster of who this device has heard* instead of *people* | ☑ | `/assets/icons/ICON_PEERS.png` |
| `ICON_LINK` | Hacker-face LINK slot icon | 28×28 | dim/bright; two pets facing off across a bolt — the 1v1 duel slot | ☑ | `/assets/icons/ICON_LINK.png` |
| `ICON_SLOT_INACCESSIBLE` | Inaccessible-slot overlay (both faces) | 20×20 | composited over a 28×28 slot; no-entry/lock motif; distinct from row-level `ICON_LOCK`; used for undesigned Hacker slots + stage-locked pet slots | ☑ | `/assets/icons/ICON_SLOT_INACCESSIBLE.png` |
| `ICON_AUDIT_ARMED` | Audit-capture state — armed/sealed | 16×16 | pcap capture-policy state marker (STATUS §Audit; hot/seal/cooldown SM) | ⌫ | `/assets/_attic/ICON_AUDIT_ARMED.png` |
| `ICON_AUDIT_HOT` | Audit-capture state — hot/capturing | 16×16 | pcap capture-policy state marker (STATUS §Audit) | ☑ | `/assets/icons/ICON_AUDIT_HOT.png` |
| `ICON_AUDIT_COOLDOWN` | Audit-capture state — cooldown | 16×16 | pcap capture-policy state marker (STATUS §Audit) | ⌫ | `/assets/_attic/ICON_AUDIT_COOLDOWN.png` |

> **Reused (no new art):** `UI_SUBMENU_HEADER` / `UI_LIST_HEADER` / `UI_CURSOR_ROW` / `UI_ROW_SEL` /
> `UI_SCROLLBAR` / `UI_HINT_BAND` (§F), `FONT_UI` / `PAL_CORE` (§E). Role / ability name / `⚡` sigil /
> status tags (`[ACTIVE]` / `Active ✓` / `locked`) = `FONT_UI` text — no tag art.
> **Deferred (→ Design / future, no row yet):** Hacker-face **centre canvas / HUD reskin** (terminal-
> green + team accent) — still unspecced; likewise the other Hacker slots.

---

## S. Web 'Pedia (SD-served site)

The AP-hosted 'Pedia is a real static bundle + a live data feed: the site bundle plus the `tools/gen_pedia_data.py` sync generator, which reads the
content tables through the firmware's own code (`tools/dump_content.cpp`) plus
`src/core/model/combat.cpp` for the wild roster, via `make pedia` / `make pedia-check`
(repo-root `Makefile`).

| Asset | Element | Notes | Status | File |
|---|---|---|---|---|
| `web/index.html` / `style.css` / `app.js` | Site shell + terminal-themed styling + client logic | binds every colour to `PAL_CORE` role tokens (`web/assets/PAL_CORE.json`); no literal hex | ☑ | `/web/index.html`, `/web/style.css`, `/web/app.js` |
| `web/data/pedia_data.js` | **GENERATED** creature/malbeast/item/mod/move/achievement data | regenerated by `tools/gen_pedia_data.py`; never hand-edit — `make pedia` rewrites it, `make pedia-check` is the CI-staleness guard | ☑ | `/web/data/pedia_data.js` |
| `web/fixtures/pedia_state.js` | Dev-preview fixture standing in for the live `GET /pedia_state.json` | browser-preview only, not served on-device | ☑ | `/web/fixtures/pedia_state.js` |
| `web/assets/*` | Site copies of shipped `ICON_*`/`SPR_PET_*`/`PAL_CORE.json` assets | same files as `/assets/` (§A–§R above); duplicated into `/web/assets/` for the site bundle, not new art | ☑ | `/web/assets/` |
| **Typography** | `FONT_UI` (Pixel Operator Mono) | **font binary still TODO** — the site currently falls back to a system-mono stack; same open item as the on-device `FONT_UI` (`ASSET_MANIFEST §E`) | ☐ | — |
| **Achievement icons** | Per-achievement glyphs, named on each row's `icon` field (`src/core/content/content_achievements.cpp`) | **One bespoke `ICON_ACH_*` glyph per row, no reuse.** Most are LADDERS, so they are one motif plus a centred tally — five skulls for the boss tiers, five depth arrows, four wifi arcs, three narrowing DEFRAG stacks, three arcade joysticks — countable before anything else, which is what lets a grid of them read in grayscale. The four creature lines are sets rather than ladders, so they use the line's own mark (lock / hook / horse / rearing worm) under a footer: a bar for "raised them all", a chevron for "took one deep". `DEVTOOLS_INTRUDER` is deliberately a question mark over a redaction bar — the 'Pedia draws a row's icon even while the row is masked, so anything representational there would spoil the one achievement whose point is working it out. The three Backup Drive rows are the other exception: a SET, not a ladder, so all three carry the same platter and differ only in its fate — restored and carried up, restored and going back down, or split with the pieces scattered. The Metamorphic pair are a set of the same kind, both the line's bell: solid and parted for the cabinet it is played on (§Q), hollow for the hatch run that was never spotted, and behind full-width bars for the deep cabinet score | ☑ | `/assets/icons/ICON_ACH_*.png` |

> **Malbeast sprites are `▨` stand-ins, but they are the shipping art** — `make pedia` copies them
> from `assets/` into `web/assets/` like any other sprite, so the site and the device show the same
> six frames. Final art replaces both at once.

---

Every asset-consuming system is built. The one open feature behind §R's rows is the Red/Blue
archetype layer (its optional `ICON_ARCHETYPE_*` art) — see the Hacker-face CREW section of
`docs/MASTER_TODO.md`. CREW,
PEERS and LINK all ship today, each with its own icon.
