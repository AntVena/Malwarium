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
> content glyphs) · **16** (status/button glyphs) · **12** (inline log glyphs) logical px.

| Asset ID | Slot | Concept | Logical size | States | Status | File |
|---|---|---|---|---|---|---|
| `ICON_STAT`  | 1 STAT  | Heart w/ graph line       | 28×28 | dim/bright | ☑ | `/assets/icons/ICON_STAT.png` |
| `ICON_ITEMS` | 2 ITEMS | USB drive                 | 28×28 | dim/bright | ☑ | `/assets/icons/ICON_ITEMS.png` |
| `ICON_TRAIN` | 3 TRAIN | Target reticle in terminal| 28×28 | dim/bright | ☑ | `/assets/icons/ICON_TRAIN.png` |
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
- **`SPR_PET_PINGCUB` is `▨`** — it has one idle frame and wants a second to match the 2-frame
  norm above. The drawing itself is final.
- **`SPR_PET_EGG_PHISH_HATCH` is deliberately the only egg file.** Frames 0–1 are the idle loop and
  0–7 the hatch one-shot (`Game::hatchCrackFrame` walks it). A separately-drawn single-frame
  `SPR_PET_EGG_PHISH` was byte-identical to frame 0, so shipping it too would only duplicate flash.
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

---

## E. Fonts & palette
System specced in `VISUAL_LANGUAGE.md`; the face and the hues are both delivered below. UI chrome
still references a `PAL_CORE` token rather than a literal, so a hue change reskins from one place.

| Asset ID | Element | Status | File |
|---|---|---|---|
| `FONT_UI` | primary UI **pixel** font — tabular digits + disambiguated `0/O 1/I 5/S 8/B` (`VISUAL_LANGUAGE.md §2`). **Face = Pixel Operator Mono**, its own 8px cut (CC0, licence beside the TTF). Rasterised to a glyph table by `tools/gen_font.py` — an authoring step, not a gate, so `gen_assets.py` stays pure-stdlib; the generator refuses any size the face would be antialiased at | ☑ | `/assets/fonts/PixelOperatorMono8.ttf` |
| `PAL_CORE` | core palette = **14 role tokens** (structural · semantic `calm`/`warn`/`hot` · rarity) + team pair. Hues set — danger-ascending, `accent` ≠ status | ☑ | `/assets/PAL_CORE.json` |

---

## F. Submenu chrome (shared)
Used by all 8 submenus (shared header/row/scrollbar chrome, built — see `src/core/ui/`). Author
on placeholder palette.

| Asset ID | Element | Logical size | Notes | Status | File |
|---|---|---|---|---|---|
| `UI_SUBMENU_HEADER` | Header band: title (left) + position indicator (right) | 224×24 | shorthand text + dots or `n/total` | ☑ | engine-drawn |
| `UI_HINT_BAND` | Contextual control hint — **self-contained screens** | 224×24 | a screen the player ARRIVED at (event, activity, modal, full-screen reader) names what's live on it, whether or not the mapping is standard; the carousel's own list/detail spine doesn't, because A/B/C is constant there. See the note under this table | ☐ | |
| `UI_PAGER_DOTS` | Page indicator for viewer/paged screens | ~24×8 | filled = current page | ☑ | engine-drawn |
| `UI_CURSOR_ROW` | Focused-row cursor marker `▸` | 12×28 | non-destructive over row | ☑ | engine-drawn |
| `UI_ROW_SEL` | Focused-row accent fill | 224×28 | semi-opaque highlight behind row | ☑ | engine-drawn |
| `UI_SCROLLBAR` | Slim right-edge scroll position bar | 4×176 | shows when list > 6 rows | ☑ | engine-drawn |
| `UI_LIST_HEADER` | Grouped-list section header row | 224×16 | non-selectable; cursor skips (e.g. `FOOD`/`BUFFS`) | ☑ | engine-drawn |
| `UI_PROGRESS_BAR` | Reusable horizontal progress/fill bar | ~180×12 | EXPL walk, MAINT defrag/AV, hold-to-commit. **= `UI_GAUGE` variant** (neutral, no zone colour) | ☐ | |

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
- **`ICON_ITEM_FULLY_STACKED_NACHOS` is `▨`** — it reuses `ICON_ITEM_TORTILLA_CHIP` rather than
  being bespoke. The only item icon that is a stand-in.

---

## I. TRAIN submenu
| Asset ID | Element | Logical size | Notes | Status | File |
|---|---|---|---|---|---|
| `ICON_TRAIN_SIM` | Sim-Battle (Dummy) row glyph | 20×20 | reserved placeholder row | ☑ | `/assets/icons/ICON_TRAIN_SIM.png` |

> TRAIN ships as a reserved placeholder row (action deferred). `·soon·` tag
> reuses §13 `locked`-state styling — no new tag art. Pedia-Challenge dropped. All
> minigame/reaction + turn-based battle screen art waits for the deferred minigame pass.

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
| `UI_SLOTS_USED` | Rack-slot usage indicator (`slots 2/4`) | ~44×12 | header widget; may reuse MODS slot-count style | ☐ | |

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
| `ICON_CFG_SYSINFO` | System Info row glyph | 20×20 | UPDATES borrows it too, pending its own | ☑ | `/assets/icons/ICON_CFG_SYSINFO.png` |
| `ICON_CFG_TAG` | HackerTag row glyph | 20×20 | | ☑ | `/assets/icons/ICON_CFG_TAG.png` |
| `ICON_CFG_UIMODE` | UI Mode row glyph | 20×20 | also the DEVICE group row + BRIGHTNESS | ☑ | `/assets/icons/ICON_CFG_UIMODE.png` |
| `ICON_CFG_TITLE` | TITLE row glyph (zone-Title picker) | 20×20 | v1 stopgap home for zone Titles; moves to Hacker HUD later | ☑ | `/assets/icons/ICON_CFG_TITLE.png` |
| `ICON_CFG_RADIO` | RADIO group row glyph | 20×20 | the four radio consents under one row; **reuses `ICON_SYS_WIFI` today**, which PEDIA AP + INTERNET also use — a distinct glyph would separate "the radio, as a place" from "a Wi-Fi service". A square-wave alternate is parked at `/assets/_attic/ICON_SYS_WIFI_ALT.png`; it needs a 20×20 redraw and a design call on whether that motif is the one that carries the split | ☐ | |
| `ICON_CFG_UPDATE` | UPDATES row glyph | 20×20 | **reuses `ICON_CFG_SYSINFO` today**; a download/arrow motif would clear it | ☐ | |
| `ICON_CFG_TRAVEL` | TRAVEL MODE row glyph (DEVICE group) | 20×20 | **reuses `ICON_CFG_SYSINFO` today**. A sleep motif — crescent, or a powered-down screen — would say what the row does; it is the one row in the group that is an action rather than a setting | ☐ | |
| `ICON_SYS_BATTERY` | Battery status glyph | 16×16 | System Info | ⌫ | `/assets/_attic/ICON_SYS_BATTERY.png` |
| `ICON_SYS_WIFI` | Wi-Fi AP status glyph | 16×16 | System Info; also the PEDIA AP / INTERNET / RADIO rows | ☑ | `/assets/icons/ICON_SYS_WIFI.png` |
| `ICON_SYS_SD` | SD-card status glyph | 16×16 | System Info | ☑ | `/assets/icons/ICON_SYS_SD.png` |
| `ICON_CFG_QR` | Pedia QR row glyph | 20×20 | **drawn but unconsumed** — the QR is reached from PEDIA AP, not a row of its own, so nothing renders this. Keep for a future row; it costs atlas space until then | ⌫ | `/assets/_attic/ICON_CFG_QR.png` |

> Pedia QR is engine-generated (QR lib) — no art. HackerTag editor uses `FONT_UI` + caret —
> no art. SD RECHECK has no glyph: it is the A press on System Info, beside the SD line it
> reports through. Factory Reset is **hidden** (no row glyph): revealed by hold-B on System
> Info, committed by hold-B-5s; reuses `UI_PROGRESS_BAR` for both hold bars.

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
| `UI_COUNTDOWN` | Lockout countdown digits/style | ~64×24 | `FONT_UI`-based; pairs w/ `FX_LOCKOUT_BAND` | ☐ | |

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

| Asset ID | Element | Logical size | Notes | Status | File |
|---|---|---|---|---|---|
| `UI_HEALTH_BAR` | Combat-Health row | ~208×24 | `UI_GAUGE` variant — transient Health | ☑ | engine-drawn |
| `UI_OVERRIDE_PIP` | Once-per-battle Exploit-override indicator | 16×16 | ready (bolt) / spent (×) | ☑ | `/assets/icons/ICON_OVERRIDE_PIP{,_SPENT}.png` |
| `UI_MOVE_CHANNEL` | Multi-turn move wind-up | ~120×12 | `UI_GAUGE` variant; override decision cue | ☑ | engine-drawn |
| `ICON_MOVE_SLOT` | Loadout equip-slot row glyph | 20×20 | filled / empty / locked variants | ☑ | `/assets/icons/ICON_MOVE_SLOT{,_EMPTY,_LOCKED}.png` |
| `ICON_MOVE_<ID>` | Per-move glyph | 20×20 | derived at draw time from the move id (`train_screen.cpp`), so a new move's icon needs no wiring — drop the PNG in `assets/icons/` and it lights up. TRAIN falls back to text for a move with none, which is why most of the roster has no glyph yet: `ls assets/icons/ICON_MOVE_*` against `content_moves.cpp` is the real count | ☑/▨ | `/assets/icons/` |
| `UI_DAMAGE_POPUP` | Floating damage number | — | `FONT_UI` tabular digits — **procedural, no art** | ⊘ | — |
| `SPR_DUMMY` | Sim-Battle training-dummy sprite | ≤128×64 | wired for both tiers in `simDummy()` (`src/core/model/combat.cpp`) — they are the same prop, and the tier reads off the level/stat rows | ▨ | `/assets/sprites/SPR_DUMMY.png` (56×48) |
| `ICON_EVENT_WIFI` | Wi-Fi network event glyph | 20×20 | **optional**; else reuse the EXPL Wi-Fi-globe motif | ⌫ | placeholder `/assets/_attic/ICON_EVENT_WIFI.png` |
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
not (`docs/MASTER_TODO.md §1i`). Author on
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
| `ICON_PEERS` | Hacker-face PEERS slot icon | 28×28 | dim/bright; met-operators motif (two figures / a handshake). The slot is **live** and renders text-only in the carousel without it — drop-in, no engine change | ☐ | |
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
| **Achievement icons** | Per-achievement glyphs, named on each row's `icon` field (`src/core/content/content_achievements.cpp`) | **One bespoke `ICON_ACH_*` glyph per row, no reuse.** Most are LADDERS, so they are one motif plus a centred tally — five skulls for the boss tiers, five depth arrows, four wifi arcs, three narrowing DEFRAG stacks — countable before anything else, which is what lets a grid of them read in grayscale. The three creature lines are sets rather than ladders, so they use the line's own mark (lock / hook / horse) under a footer: a bar for "raised them all", a chevron for "took one deep". `DEVTOOLS_INTRUDER` is deliberately a question mark over a redaction bar — the 'Pedia draws a row's icon even while the row is masked, so anything representational there would spoil the one achievement whose point is working it out. The three Backup Drive rows are the other exception: a SET, not a ladder, so all three carry the same platter and differ only in its fate — restored and carried up, restored and going back down, or split with the pieces scattered | ☑ | `/assets/icons/ICON_ACH_*.png` |

> **Malbeast sprites are `▨` stand-ins, but they are the shipping art** — `make pedia` copies them
> from `assets/` into `web/assets/` like any other sprite, so the site and the device show the same
> six frames. Final art replaces both at once.

---

Every asset-consuming system is built. The one open feature behind §R's rows is the Red/Blue
archetype layer (its optional `ICON_ARCHETYPE_*` art) — see the Hacker-face CREW section of
`docs/MASTER_TODO.md`. CREW,
PEERS and LINK all ship today: CREW and LINK have their icons, while PEERS renders its label
alone until `ICON_PEERS` is drawn.
