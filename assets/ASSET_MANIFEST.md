# Malwarium — Asset Manifest

Master list of every visual asset the pet-side UI needs. **Claude Design** pulls `☐ TODO`
rows, produces the art within the constraints, saves to `/assets/...`, and updates Status +
File. **Empty `File` cells are the work to be done.**

**Status:** `☐` TODO · `✎` WIP · `▨` PLACEHOLDER (a correctly-sized/named stand-in is shipping;
final art still wanted) · `☑` DELIVERED · `⌫` PARKED (drawn, but nothing consumes it — see below) ·
`⊘` N/A (procedural / no art)

> **`assets/` IS the atlas.** `tools/gen_assets.py` walks the tree and compiles every PNG into
> flash, skipping any file or folder whose name starts with `_`. So the tree listing and the
> shipped sprite set are the same thing, and a `▨` stand-in ships exactly like final art does; the
> marker is about art quality, not about whether it's wired. An asset's id is its **basename** —
> `sprites/` · `icons/` · `ui/` organise the tree for readers and change nothing downstream, which
> is also why no two PNGs may share a basename.
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

**Design studies — NOT shippable.** Underscore-prefixed files are references, never ship/convert
them: `assets/_contact_sheet.png` (icon/sprite contact sheet), `assets/_explore_paypup_acid.png` /
`_clinical.png` / `_locker.png` (Paypup palette exploration — Acid is the chosen lead),
and the Malbear shading studies.

---

## A. Slot icons (carousel)  — Area 2 / 6

One per carousel slot. Each icon needs two states (idle dim /
focused bright) — supply one master, brightness handled in engine unless noted.

> **Icon size tiers (`05 §3.1`):** every `ICON_*` snaps to **28** (slot icons) · **20** (row/
> content glyphs) · **16** (status/button glyphs) · **12** (inline log glyphs) logical px.

| Asset ID | Slot | Concept | Logical size | States | Status | File |
|---|---|---|---|---|---|---|
| `ICON_STAT`  | 1 STAT  | Heart w/ graph line       | 28×28 | dim/bright | ☑ | `/assets/ICON_STAT.png` |
| `ICON_ITEMS` | 2 ITEMS | USB drive                 | 28×28 | dim/bright | ☑ | `/assets/ICON_ITEMS.png` |
| `ICON_TRAIN` | 3 TRAIN | Target reticle in terminal| 28×28 | dim/bright | ☑ | `/assets/ICON_TRAIN.png` |
| `ICON_EXPL`  | 4 EXPL  | Wi-Fi mesh globe          | 28×28 | dim/bright | ☑ | `/assets/ICON_EXPL.png` |
| `ICON_MAINT` | 5 MAINT | Fragmented HDD            | 28×28 | dim/bright | ☑ | `/assets/ICON_MAINT.png` |
| `ICON_MODS`  | 6 MODS  | Cracked CPU               | 28×28 | dim/bright | ☑ | `/assets/ICON_MODS.png` |
| `ICON_ARCH`  | 7 ARCH  | Server rack               | 28×28 | dim/bright | ☑ | `/assets/ICON_ARCH.png` |
| `ICON_CFG`   | 8 CFG   | Gear + terminal           | 28×28 | dim/bright | ☑ | `/assets/ICON_CFG.png` |

> **Masters** are flat `ink`-fill (white) on transparent at logical 28×28; dim/bright is engine brightness (one file each).

---

## B. UI chrome  — Area 2 / 6

| Asset ID | Element | Logical size | Notes | Status | File |
|---|---|---|---|---|---|
| `UI_CURSOR_BOX`   | Focused-slot highlight box      | 56×40 (slot)  | accent frame; non-destructive over icon | ☑ | engine-drawn |
| `UI_TRACK_BG`     | Menu track strip (top & bottom) | 224×40        | **solid** (`track` token, S3 resolved) | ☑ | engine-drawn |
| `CAP_SLOT_LABEL`  | Focused category caption style  | ~224×10       | font/size ref from Area 6 | ☑ | engine-drawn |
| `UI_ALERT_HUNGER` | Idle low-power / hunger icon    | 20×20         | blink ~1Hz in engine | ☑ | `/assets/UI_ALERT_HUNGER.png` |
| `UI_DEFOCUS_FADE` | Track fade-in/out treatment     | —             | engine alpha; mask only if needed | ⊘ | — |

---

## C. Pet sprite sheets  — Areas 1 / 5

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

### C.1 Sheet checklist (status per creature)

Full roster: the `CreatureDef` rows in `src/core/content/content_creatures.cpp`. Seeded below; add a
row per creature as each is scheduled. `SPR_PET_<NAME>`.

| Asset ID | Line / Stage | Creature | Status | File |
|---|---|---|---|---|
| `SPR_PET_CRYPTOSHELL` | L1 · Boot Sector (egg) | CryptoShell | ☑ | `/assets/SPR_PET_CRYPTOSHELL.png` (4 hatch frames) |
| `SPR_PET_PAYPUP`      | L1 · Process | Paypup     | ☑ | `/assets/SPR_PET_PAYPUP.png` (8 frames; lead palette = Acid, pending owner pick) |
| `SPR_PET_PHISHLET`    | L1 · Process | Phishlet   | ☑ | `/assets/SPR_PET_PHISHLET.png` — final anglerfish art; wired 2026-07-18 as the deep-dive (DEEPWEB_DEPTH_64) catch on the Phishing egg (see the anglerfish-line note below) |
| `SPR_PET_TADPOLL`     | L1 · Process | Tadpoll    | ☑ | `/assets/SPR_PET_TADPOLL.png` (112×48, 2 frames) — final tadpole art for the **Phishing-line Tadpoll** |
| `SPR_PET_CACHEMUTT`   | L1 · Process | CacheMutt  | ☑ | `/assets/SPR_PET_CACHEMUTT.png` (112×48, 2 frames) — final already delivered; not in placeholder set |
| `SPR_PET_PINGCUB`     | L1 · Process | Pingcub    | ⌫ | renamed from PingPong (committed 2026-07-01, `docs/CREATURE_IDEAS.md`); placeholder `/assets/_attic/SPR_PET_PINGCUB.png` |
| `SPR_PET_RINGWYRM`    | L2 · Boot Sector (egg) | Ringwyrm | ⌫ | placeholder `/assets/_attic/SPR_PET_RINGWYRM.png` |
| `SPR_PET_BRUINFORCE` | L4 · Daemon (Good branch) | Bruinforce | ☑ | 56×48; `/assets/SPR_PET_BRUINFORCE.png` — the durable Good-branch frame: squared up, guarding |
| `SPR_PET_BERSERKERNEL` | L4 · Daemon (Bad branch) | Berserkernel | ☑ | 56×48; `/assets/SPR_PET_BERSERKERNEL.png` — the glass-cannon Bad-branch frame: same bear reared up and roaring, red eyes, claws out. Deliberately the SAME green palette as its Good sibling — the branch reads from posture and face, not hue, which is what keeps it legible in grayscale (`08 §5`) |
| `SPR_PET_CROAKEN`     | Script (Phishing line) | Croaken | ☑ | `/assets/SPR_PET_CROAKEN.png` (224×48, 4 frames) — final toad-kraken art on the standard 56×48 pet cell, wired to the **Phishing-line Croaken** (Script) |
| `SPR_PET_EGG_PHISH_HATCH` | Boot Sector (egg, Phishing line) | Phrogspawn | ☑ | `/assets/SPR_PET_EGG_PHISH_HATCH.png` (448×48, 8 frames) — the shared Phishing egg shell. Frames 0–1 are the idle loop, 0–7 the hatch one-shot (`Game::hatchCrackFrame` walks it). Deliberately the ONLY egg file: the separately-delivered single-frame `SPR_PET_EGG_PHISH` is byte-identical to frame 0, so shipping it too would just duplicate flash |
| … | | *(remaining L1 Script/Daemon + full L2 — same template)* | ☐ | |

> **New lines (2026-07-17) using generic-stage placeholders** (PO: gameplay first — final art is
> follow-up polish, no engine change to swap in). Wired in `embedded_content.cpp`, rendering via
> `assets/SPR_PET_GENERIC_*`: **cat Ransomware** — `conkittenate` (→ `_GENERIC_PROCESS`),
> `kalico` (`_GENERIC_SCRIPT`), `pwnther` + `breecheetah` (`_GENERIC_DAEMON`); **frog Phishing** —
> `goliauth` (`_GENERIC_DAEMON`). `phrogspawn`/`tadpoll`/`croaken` use the final
> `SPR_PET_EGG_PHISH_HATCH`/`SPR_PET_TADPOLL`/`SPR_PET_CROAKEN` above. The generics are pulled into
> the atlas by `gen_assets.py` (which also handles non-56px single-frame Daemon sprites).

> **Anglerfish line (2026-07-18) — a 2nd Phishing hatch outcome, DEEP-DIVE-gated.** A second
> Process pet on the `phishing` line joins the Phrogspawn egg's random hatch pool, but only once
> **`DEEPWEB_DEPTH_64`** (the 2nd DeepWeb-depth milestone) is earned — then the egg hatches Tadpoll
> **or** Phishlet at 50/50 (`Game::hatchProcessUnlocked` is the gate; depth-8 unlocks the line,
> depth-64 unlocks this rarer catch). Chain: **`phishlet`** (Process — uses the FINAL anglerfish
> `SPR_PET_PHISHLET`, no longer a `▨` placeholder) → **`clickbait`** (Script, `_GENERIC_SCRIPT`) → a
> Good/Bad Daemon branch **`spamwhale`** (durable, `_GENERIC_DAEMON`) / **`baitracuda`** (glass
> cannon, `_GENERIC_DAEMON`), carrying the same §1.10 power/Frag multipliers as the Bruinforce/Berserkernel pair.
> Final art for ClickBait + both Daemons is follow-up polish (drop-in, no engine change).

### C.3 Clutch Pick — the Phishing egg's hatch minigame

The Phishing egg is laid into a raft of decoys and found by halving the clutch three times
(`src/core/app/game/game_eggpick.cpp`). Every decoy is baked into the backdrop; only the live tile
animates, so **motion is the only tell** and the puzzle passes the grayscale gate on its own. The
panel draws at **×2**, not the usual ×1.75 — 112 doubles to exactly the 224 active canvas and keeps
every 14px cell whole-pixel, so the live tile lands dead-on the decoy it replaces.

| Asset ID | Element | Logical size | Notes | Status | File |
|---|---|---|---|---|---|
| `BG_EGG_CLUTCH` | The raft of decoy eggs | 112×56 | 32 slots on a 14px cell grid, 8×4; odd rows shifted 7px, so their last cell wraps across both edges (the live egg never hides there) | ☑ | `/assets/BG_EGG_CLUTCH.png` |
| `SPR_EGG_PHISH_MICRO` | The live egg's clutch tile | 14×14 | 2 frames, alternating on the heartbeat — the tell. Opaque, so it replaces a decoy cell exactly. Needs the `FRAME_W_OVERRIDES` row in `tools/gen_assets.py` to read as a 2-frame strip rather than one 28px image | ☑ | `/assets/SPR_EGG_PHISH_MICRO.png` |

> Reused, no new art: the aim/eliminate scrim and the aimed-half edge bar are engine fills over
> `PAL_CORE` tokens, and the verdict/round lines are `FONT_UI` text — no tag or overlay art.

> **Trojan line — reached by cross-line divert, NOT an egg (see `src/core/content/LINE_MOVE_IDENTITIES.md §3`).**
> A Process pet with a Trojan divert target has a ~10% chance to evolve into a Trojan instead of its
> normal successor. A Trojan **looks like its origin line** (right colour, a small "wrong" tell), so
> its art brief is a re-skin of the source line's palette. First cut: **`keyloggerhead`** (Script) uses
> the existing (placeholder-empty) `SPR_PET_KEYLOGGERHEAD` frame; **`placeholder_daemon`** (Daemon,
> literal name "Placeholder") uses `SPR_PET_GENERIC_DAEMON`. Follow-up art: the "Phishing-blue turtle
> with a tell" Keyloggerhead sprite + a real, renamed Trojan Daemon — both drop-in, no engine change.

### C.2 Wild malbeasts (`SPR_MALBEAST_*`)

Wild creatures for encounters/exploration (the 'net). Cell 56×48 (apex tier 64×56); single idle
frame each, so a wild reads as a still until final sheets follow the §C frame template.

Each one is wired to its own row in `wildMalbeast()` (`src/core/model/combat.cpp`), which is the
only place a wild's sprite is chosen. That function's display names are also the source of truth
for `kWildMalbeastIds`, the 'Pedia's seen/defeated masks — so renaming a wild moves both. A native
test asserts every wild's sprite both resolves AND is distinct from its siblings, which is the
condition that used to fail silently: sprites resolve by string at draw time, so a wrong name
draws nothing rather than erroring.

| Asset ID | Tier | Creature | Status | File |
|---|---|---|---|---|
| `SPR_MALBEAST_GLITCHHOG`        | 1 | GlitchHog        | ▨ | `/assets/SPR_MALBEAST_GLITCHHOG.png` |
| `SPR_MALBEAST_SEGFAULT_PUP`     | 1 | Segfault Pup     | ▨ | `/assets/SPR_MALBEAST_SEGFAULT_PUP.png` |
| `SPR_MALBEAST_PACKET_WRAITH`    | 2 | Packet Wraith    | ▨ | `/assets/SPR_MALBEAST_PACKET_WRAITH.png` |
| `SPR_MALBEAST_CACHE_GHOUL`      | 2 | Cache Ghoul      | ▨ | `/assets/SPR_MALBEAST_CACHE_GHOUL.png` |
| `SPR_MALBEAST_BUFFER_WYRM`      | 2 | Buffer Wyrm      | ▨ | `/assets/SPR_MALBEAST_BUFFER_WYRM.png` |
| `SPR_MALBEAST_KERNEL_LEVIATHAN` | 3 | Kernel Leviathan | ▨ | 64×56; `/assets/SPR_MALBEAST_KERNEL_LEVIATHAN.png` |

> **Sub-area and area bosses still borrow `SPR_PET_CACHEMUTT`** (`subAreaBoss` in the same file) —
> the same defect this section just fixed, one layer up. Which malbeast frame (or bespoke art) each
> named boss should wear is a design call, not a mechanical swap, so it is left open.

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

## E. Fonts & palette — Area 6

System specced in `05_visual_language.md`; **hues/face still pending Design (D2)** — keep the
"don't hard-code colours" caveat until delivered.

| Asset ID | Element | Status | File |
|---|---|---|---|
| `FONT_UI` | primary UI **pixel** font — type scale + tabular digits + disambiguated glyphs (`05 §2`). **Face = Pixel Operator Mono** (bitmap, monospace⇒tabular, slashed 0, ESP32-GFX–portable) | ☑ | `assets/VISUAL_LANGUAGE.md` |
| `PAL_CORE` | core palette = **14 role tokens** (structural · semantic `calm`/`warn`/`hot` · rarity) + team pair. Hues set (D2) — danger-ascending, `accent` ≠ status | ☑ | `/assets/PAL_CORE.json` |

---

## F. Submenu chrome (shared) — Area 3

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
| `UI_PROGRESS_BAR` | Reusable horizontal progress/fill bar | ~180×12 | EXPL walk, MAINT defrag/AV, hold-to-commit. **= `UI_GAUGE` variant** (neutral, no zone colour) — see `03 §0.2` | ☐ | |

> **On `UI_HINT_BAND`'s rule.** It was originally specced exception-only — a band ONLY
> where a screen broke the standard A/B/C contract. What shipped is broader and better:
> every self-contained context draws one (`drawExplList`, `drawShop`, `drawWarpPicker`,
> `drawPostEncounter`, the combat screen, the duel screens, the Clutch Pick, the MOVE
> reader), naming what's live even when the mapping is ordinary — because a player who
> just arrived somewhere shouldn't have to infer the controls from the layout. The
> carousel's own submenu spine (STAT/ITEMS/MODS/ARCH/CFG lists) still goes without,
> since A/B/C never changes there. A band is still REQUIRED wherever the mapping is
> non-standard; it is no longer limited to those cases.

---

## G. STAT submenu — Area 3

| Asset ID | Element | Logical size | Notes | Status | File |
|---|---|---|---|---|---|
| `UI_CARE_PIPS` | Care-mistake pips | ~84×16 | 2 Good + 3 Bad + gate divider + 3 colour states + numeric (0–5) | ☑ | engine-drawn |
| `ICON_LOG_EVENT` | Hacker-Log glyph set (3) | 12×12 ea | `item` · `warn` · `combat` | ☑ | `/assets/ICON_LOG_EVENT_{ITEM,WARN,COMBAT}.png` |

> STAT's Hunger/Frag/Happiness **gauge widgets** + Stage indicator are built; assets in §O below.

---

## H. ITEMS submenu — Area 3

| Asset ID | Element | Logical size | Notes | Status | File |
|---|---|---|---|---|---|
| `UI_RARITY_TAG` | Rarity tag style (Common/Uncommon/Rare/Epic) | ~44×14 | ramp set (D14): `rarity-common…epic`, dull→bright, read by position; tag style in board | ☑ | `/assets/PAL_CORE.json` |
| `ICON_ITEM_AIRGAP_SNACK` | Air-Gapped Snack icon | 20×20 | Food | ☑ | `/assets/ICON_ITEM_AIRGAP_SNACK.png` |
| `ICON_ITEM_DECRYPT_KEY` | Decryption Key icon | 20×20 | Quest/utility | ☑ | `/assets/ICON_ITEM_DECRYPT_KEY.png` |
| `ICON_ITEM_BACKUP_DRIVE` | Backup Drive icon | 20×20 | Buff (combat shield) | ☑ | `/assets/ICON_ITEM_BACKUP_DRIVE.png` |
| `ICON_ITEM_TORTILLA_CHIP` | Tor-Tilla Chip icon | 20×20 | Buff | ☑ | `/assets/ICON_ITEM_TORTILLA_CHIP.png` |
| `ICON_ITEM_YUBI_COOKIE` | Yubi-Cookie icon | 20×20 | Buff | ☑ | `/assets/ICON_ITEM_YUBI_COOKIE.png` |
| `ICON_ITEM_SINKHOLE_TRAP` | Sinkhole Trap icon | 20×20 | Buff/utility | ☑ | `/assets/ICON_ITEM_SINKHOLE_TRAP.png` |
| `ICON_ITEM_NULL_NOODLES` | Null Noodles icon | 20×20 | Consumable (defrag/anti-food, `09 §4.2`) | ☑ | `/assets/ICON_ITEM_NULL_NOODLES.png` |
| `ICON_ITEM_R007_B33R` | R007_B33R icon | 20×20 | Consumable (junk food, `09 §4.2`) | ☑ | `/assets/ICON_ITEM_R007_B33R.png` |
| `ICON_ITEM_SEALED_CACHE` | Sealed Cache icon | 20×20 | Quest (openable, `09 §3`); base + rarity-tinted variants below | ☑ | `/assets/ICON_ITEM_SEALED_CACHE.png` |
| `ICON_ITEM_SEALED_CACHE_COMMON` | Sealed Cache — Common | 20×20 | rarity-ramp variant of Sealed Cache (`09 §3`) — **1 chevron**, stacked upward from the base, so the tier is countable and the ramp survives grayscale | ☑ | `/assets/ICON_ITEM_SEALED_CACHE_COMMON.png` |
| `ICON_ITEM_SEALED_CACHE_UNCOMMON` | Sealed Cache — Uncommon | 20×20 | rarity-ramp variant of Sealed Cache (`09 §3`) — **2 chevrons**, stacked upward from the base, so the tier is countable and the ramp survives grayscale | ☑ | `/assets/ICON_ITEM_SEALED_CACHE_UNCOMMON.png` |
| `ICON_ITEM_SEALED_CACHE_RARE` | Sealed Cache — Rare | 20×20 | rarity-ramp variant of Sealed Cache (`09 §3`) — **3 chevrons**, stacked upward from the base, so the tier is countable and the ramp survives grayscale | ☑ | `/assets/ICON_ITEM_SEALED_CACHE_RARE.png` |
| `ICON_ITEM_SEALED_CACHE_EPIC` | Sealed Cache — Epic | 20×20 | rarity-ramp variant of Sealed Cache (`09 §3`) — **4 chevrons**, stacked upward from the base, so the tier is countable and the ramp survives grayscale | ☑ | `/assets/ICON_ITEM_SEALED_CACHE_EPIC.png` |
| `ICON_ITEM_ACCESS_TOKEN` | Access Token icon | 20×20 | Quest (warp → shop, `09 §5`) | ☑ | `/assets/ICON_ITEM_ACCESS_TOKEN.png` |
| `ICON_ITEM_SAFE_MODE_KEY` | Safe-Mode Key icon | 20×20 | Quest (warp → safe rest, `09 §5`) | ☑ | `/assets/ICON_ITEM_SAFE_MODE_KEY.png` |
| `ICON_ITEM_DECRYPTOGRAM` | Decryptogram icon | 20×20 | Quest — **starting item**, speeds egg decryption (`04 §1.5`) | ☑ | `/assets/ICON_ITEM_DECRYPTOGRAM.png` |
| `ICON_ITEM_ROLLBACK` | Rollback icon | 20×20 | Quest/utility — stat picker: shed a chosen stat by 1 (−1 level) to re-roll (`06 §6.2`) | ☑ | `/assets/ICON_ITEM_ROLLBACK.png` |
| `ICON_ITEM_PWNZU_SAUCE` | Pwnzu Sauce icon | 20×20 | Food — Merge Hub ingredient (`pwnzu_patched_noodles`, `07 §14`) | ☑ | `/assets/ICON_ITEM_PWNZU_SAUCE.png` |
| `ICON_ITEM_OSI_DIP` | OSI Dip icon | 20×20 | Food — Merge Hub ingredient (`fully_stacked_nachos`, `07 §14`) | ☑ | `/assets/ICON_ITEM_OSI_DIP.png` |
| `ICON_ITEM_PWNZU_PATCHED_NOODLES` | Pwnzu-Patched Noodles icon | 20×20 | Food — Merge Hub output, null_noodles + pwnzu_sauce (`07 §14`) | ☐ | |
| `ICON_ITEM_FULLY_STACKED_NACHOS` | Fully-Stacked Nachos icon | 20×20 | Food — Merge Hub output, tortilla_chip + osi_dip; fills every stat (`07 §14`) | ▨ | `/assets/ICON_ITEM_FULLY_STACKED_NACHOS.png` (reused `ICON_ITEM_TORTILLA_CHIP` — placeholder, not bespoke) |
| `ICON_ITEM_COMMEND_CACHE` | Commendation Cache icon | 20×20 | Quest — the ACHIEVEMENT reward container, earned and never found. **Inverts the fill** (hollow shell + star) instead of adding a chevron, so it reads as a different KIND of cache rather than a fifth rarity step | ☑ | `/assets/ICON_ITEM_COMMEND_CACHE.png` |
| `ICON_ITEM_COMMEND_CACHE` major variant | Crowned/rayed Commendation Cache | 20×20 | ⌫ parked at `/assets/_attic/ICON_ITEM_SEALED_CACHE_COMMENDATION_MAJOR.png` — `commend_cache` is a single item row (`content_items.cpp`), so there is no second tier for it to mark. Promote it if a higher commendation container is ever authored | ⌫ | `/assets/_attic/ICON_ITEM_SEALED_CACHE_COMMENDATION_MAJOR.png` |

### H.1 The pantry — drawn as one batch

The **staple ingredients** (`content_items.cpp`'s STAPLE INGREDIENTS block) plus the two dishes
cooked from them ship as a single coherent 20×20 food set, drawn together so the shelf reads as a
pantry rather than twenty unrelated glyphs. Three want to be legible AS PAIRS, and are: Fresh
Macrol is a whole fish where Spoiled Macrol is its skeleton, C-Salt and Desalinated C-Salt share a
cap and differ only in whether the body has anything in it, and the two Browns are the same branded
patty with and without salt above it.

Distinctness was the brief — these sit in one scrolling list — so no two share a silhouette:
tin · crumbs · shaker · cruet · holed flask · fish · pouch-and-clock · cubes · vial · spuds ·
yolk-in-shell · leek · cereal box · mug · sachet · apple · taproot · noodle bowl · patty.

| Asset ID | For | Size | Status | Diff to integrate |
|---|---|---|---|---|
| `ICON_ITEM_*` (pantry batch, 22) | staple ingredients + the two Browns | 20×20 ea | ☑ | shipped |

> Add an `ICON_ITEM_*` row per new item as the roster grows. Every shipped item now has its
> own `ICON_ITEM_<ID>`, so `itemIcon()` is a straight name lookup with no borrowed-glyph table
> behind it — a row added without art shows a blank, which is the prompt to draw one.

---

## I. TRAIN submenu — Area 3

| Asset ID | Element | Logical size | Notes | Status | File |
|---|---|---|---|---|---|
| `ICON_TRAIN_SIM` | Sim-Battle (Dummy) row glyph | 20×20 | reserved placeholder row | ☑ | `/assets/ICON_TRAIN_SIM.png` |

> TRAIN ships as a reserved placeholder row (action deferred — QUESTIONS S11). `·soon·` tag
> reuses §13 `locked`-state styling — no new tag art. Pedia-Challenge dropped. All
> minigame/reaction + turn-based battle screen art waits for the deferred minigame pass.

---

## J. EXPL submenu — Area 3

| Asset ID | Element | Logical size | Notes | Status | File |
|---|---|---|---|---|---|
| ~~`UI_BANDWIDTH_METER`~~ | ~~Steps-remaining (Bandwidth) meter~~ | — | **RETIRED** — exploration is a toggleable idle-mode with unlimited auto-steps; no Bandwidth budget (`09 §1`/`§7`) | ⊘ | — |
| `UI_EXPLORE_BADGE` | Idle explore-mode badge | 16×16 | `⟳` mode glyph + sub-area + `WINS n/10` / `BOSS READY`; dual-coded; on idle while exploring (`09 §1.6`) | ⌫ | `/assets/_attic/UI_EXPLORE_BADGE.png` (delivered as ICON_EXPLORE_BADGE; renamed to canonical ID on integration) |
| `ICON_EXPLORE_STATE` | Sub-area row state marker (EXPL) | 16×16 | *optional* — exploring/cleared/boss-ready/locked; else `FONT_UI` tags (`09 §7`) | ☐ | |
| `UI_DIFFICULTY_PIPS` | Sector difficulty tier (filled/empty diamonds) | ~30×10 | e.g. `◆◇◇` | ⌫ | placeholder `/assets/_attic/UI_DIFFICULTY_PIPS.png` |
| ~~`ICON_EXPL_PACKET`~~ | ~~Packet Capture row glyph~~ | — | **REMOVED** — Packet Capture minigame scrapped; "packet sniffing" is now the Wi-Fi explore event (`06 §4`); real pcap = doc 07 | ⊘ | — |
| `ICON_SECTOR_<id>` | Per-sector row glyph (keyed by sector id) | 20×20 | seed `sector[0]` delivered; add per sector | ☑ | `/assets/ICON_SECTOR_0.png` |
| `BG_SECTOR_<id>` | Per-sector walk backdrop (keyed by sector id) | 128×128 | optional; flat colour OK v1; seed `sector[0]` | ⌫ | placeholders `/assets/_attic/BG_SECTOR_0.png` (Citrus Circuit) + `BG_SECTOR_1.png` (Pirate Bayou) |

> Sectors are indexed by difficulty; identity (name/icon/backdrop) is swappable data keyed by
> sector id — add `ICON_SECTOR_<id>`/`BG_SECTOR_<id>` per sector as authored. **Naming
> direction:** real-world malware-encounter places, punned (LimeWire → "Citrus Circuit").
> Only `sector[0]` is open at start; rest progression-gated. Packet Capture minigame + wild-
> encounter combat art are deferred (QUESTIONS S11). Walk reuses pet idle frames — no walk frame.

---

## K. MAINT submenu — Area 3

| Asset ID | Element | Logical size | Notes | Status | File |
|---|---|---|---|---|---|
| `ICON_MAINT_DEFRAG` | Defragmentation row glyph | 20×20 | | ☑ | `/assets/ICON_MAINT_DEFRAG.png` |
| `ICON_MAINT_AV` | Antivirus (AV) row glyph | 20×20 | | ☑ | `/assets/ICON_MAINT_AV.png` |
| `ANIM_DEFRAG` | Defrag block-shuffle process anim | — | OPTIONAL/flavour; modal process visual, NOT a §4 pass; procedural OK | ☐ | |
| `ANIM_AV_SWEEP` | AV scan-sweep process anim | — | OPTIONAL/flavour; modal process visual, NOT a §4 pass; procedural OK | ☐ | |

> Both processes reuse `UI_PROGRESS_BAR`. Replication Ghost is the existing `FX_GHOST` pass —
> AV clears it; no new art. Animations are optional polish — progress bar suffices for v1.

---

## L. MODS submenu — Area 3

| Asset ID | Element | Logical size | Notes | Status | File |
|---|---|---|---|---|---|
| `ICON_MODS_SLOT` | Equip-slot row glyph (+ empty variant) | 20×20 | filled vs empty slot | ☑ | `/assets/ICON_MODS_SLOT{,_EMPTY}.png` |
| `ICON_MOD_FIREWALL_PATCH` | Firewall Patch icon | 20×20 | +def | ☑ | `/assets/ICON_MOD_FIREWALL_PATCH.png` |
| `ICON_MOD_CLOCK_SPEED_BOOST` | Clock-Speed Boost icon | 20×20 | +spd (initiative) | ☑ | `/assets/ICON_MOD_CLOCK_SPEED_BOOST.png` |
| `ICON_MOD_QUANTISATION` | Quantisation icon | 20×20 | +spd −dmg (shrink-the-model speed/power tradeoff, `06 §1.8`) | ⌫ | `/assets/_attic/ICON_MOD_QUANTISATION.png` |
| `ICON_MOD_PACKET_SNIFFER` | Packet Sniffer icon | 20×20 | +Bits from explore loot | ☑ | `/assets/ICON_MOD_PACKET_SNIFFER.png` |
| `ICON_MOD_RAID_MIRROR` | RAID Mirror icon | 20×20 | one-shot | ☑ | `/assets/ICON_MOD_RAID_MIRROR.png` |

> Effect tags (`+def`/`+spd`/`1-shot`) are `FONT_UI` text — no tag art. Doc 07 defensive mods
> (Honeypot/IDS) join the same slot system later (out of pet-side scope). Add `ICON_MOD_*` per
> new mod as the roster grows.

---

## M. ARCH submenu — Area 3

| Asset ID | Element | Logical size | Notes | Status | File |
|---|---|---|---|---|---|
| `ICON_ARCH_SLOT` | Rack record glyph (+ retired variant) | 20×20 | active/frozen vs retired | ☑ | `/assets/ICON_ARCH_SLOT{,_RETIRED}.png` |
| `UI_SLOTS_USED` | Rack-slot usage indicator (`slots 2/4`) | ~44×12 | header widget; may reuse MODS slot-count style | ☐ | |

> Rack rows reuse `SPR_PET_*` idle frame as a thumbnail — no new art. `[ACTIVE]`/`[RETIRED]`
> are `FONT_UI` text tags; greying is engine dim. Stored pets consume rack slots; records
> don't (S22). New-egg / line-select is a contextual modal (Area 5), not an ARCH asset.

---

## N. CFG submenu — Area 3

| Asset ID | Element | Logical size | Notes | Status | File |
|---|---|---|---|---|---|
The list is **six rows** — SYSTEM INFO · HACKERTAG · TITLE · DISPLAY · RADIO · UPDATES — which is
exactly the viewport, so it never scrolls. DISPLAY and RADIO are **group screens**: each draws its
own rows from the same `CfgRow` shape and reuses the glyphs below, so grouping needs no new art.

| Asset ID | Element | Logical size | Notes | Status | File |
|---|---|---|---|---|---|
| `ICON_CFG_SYSINFO` | System Info row glyph | 20×20 | UPDATES borrows it too, pending its own | ☑ | `/assets/ICON_CFG_SYSINFO.png` |
| `ICON_CFG_TAG` | HackerTag row glyph | 20×20 | | ☑ | `/assets/ICON_CFG_TAG.png` |
| `ICON_CFG_UIMODE` | UI Mode row glyph | 20×20 | also the DISPLAY group row + BRIGHTNESS | ☑ | `/assets/ICON_CFG_UIMODE.png` |
| `ICON_CFG_TITLE` | TITLE row glyph (zone-Title picker) | 20×20 | v1 stopgap home for zone Titles (`09 §6`); moves to Hacker HUD later | ☑ | `/assets/ICON_CFG_TITLE.png` |
| `ICON_CFG_RADIO` | RADIO group row glyph | 20×20 | the four radio consents under one row; **reuses `ICON_SYS_WIFI` today**, which PEDIA AP + INTERNET also use — a distinct glyph would separate "the radio, as a place" from "a Wi-Fi service". A square-wave alternate is parked at `/assets/_attic/ICON_SYS_WIFI_ALT.png`; it needs a 20×20 redraw and a design call on whether that motif is the one that carries the split | ☐ | |
| `ICON_CFG_UPDATE` | UPDATES row glyph | 20×20 | **reuses `ICON_CFG_SYSINFO` today** — the only remaining icon collision in the list; a download/arrow motif would clear it | ☐ | |
| `ICON_SYS_BATTERY` | Battery status glyph | 16×16 | System Info | ⌫ | `/assets/_attic/ICON_SYS_BATTERY.png` |
| `ICON_SYS_WIFI` | Wi-Fi AP status glyph | 16×16 | System Info; also the PEDIA AP / INTERNET / RADIO rows | ☑ | `/assets/ICON_SYS_WIFI.png` |
| `ICON_SYS_SD` | SD-card status glyph | 16×16 | System Info | ☑ | `/assets/ICON_SYS_SD.png` |
| `ICON_CFG_QR` | Pedia QR row glyph | 20×20 | **drawn but unconsumed** — the QR is reached from PEDIA AP, not a row of its own, so nothing renders this. Keep for a future row; it costs atlas space until then | ⌫ | `/assets/_attic/ICON_CFG_QR.png` |

> Pedia QR is engine-generated (QR lib) — no art. HackerTag editor uses `FONT_UI` + caret —
> no art. SD RECHECK has no glyph: it is the A press on System Info, beside the SD line it
> reports through. Factory Reset is **hidden** (no row glyph): revealed by hold-B on System
> Info, committed by hold-B-5s; reuses `UI_PROGRESS_BAR` for both hold bars (S23 resolved).

---

## O. Stat visualisation (Area 4)

The **gauge language is defined once** here: `UI_GAUGE` is
the canonical segmented-bar primitive and `UI_PROGRESS_BAR` (§F) + `UI_BANDWIDTH_METER` (§J) are
its variants (`03 §0.2`). Author on placeholder palette; colour roles (calm/warn/hot) bind in
Area 6.

| Asset ID | Element | Logical size | Notes | Status | File |
|---|---|---|---|---|---|
| `UI_GAUGE` | Segmented-bar primitive (shared gauge) | ~120×12 (10 cells = 10%) | 3 zone states + ~1Hz Critical pulse | ☑ | engine-drawn |
| `UI_STAT_GAUGE` | Labelled vitals row: label + `UI_GAUGE` + numeric | ~208×24 | composition over `UI_GAUGE` | ☑ | engine-drawn |
| `UI_STAGE_INDICATOR` | 4-node lifecycle track (Boot→Process→Script→Daemon) | ~200×16 | current bright+named · past lit · future dim | ☑ | engine-drawn |

> `UI_CARE_PIPS` lives in §G (spec now `03 §3`). Gauges are STAT-screen chrome, **not**
> idle-pipeline passes — the Critical pulse is a UI repaint, distinct from the
> procedural `FX_CORRUPTION` sprite glitch.

---

## P. Event overlays (Area 5)

Modal interrupt-layer events (Hatch, Feeding, Lockout, Evolution, Critical System Failure); full-screen FX use the
**SCREEN_FX** pass (registered in §D). Most chrome is reused — only the rows
below are new art.

| Asset ID | Element | Logical size | Notes | Status | File |
|---|---|---|---|---|---|
| `ICON_BTN_A` | Button glyph `A` | 16×16 | reusable wherever a literal button is shown | ⌫ | `/assets/_attic/ICON_BTN_A.png` |
| `ICON_BTN_B` | Button glyph `B` | 16×16 | | ⌫ | `/assets/_attic/ICON_BTN_B.png` |
| `ICON_BTN_C` | Button glyph `C` | 16×16 | | ⌫ | `/assets/_attic/ICON_BTN_C.png` |
| `ICON_LINE_RANSOMWARE` | Line-select row glyph — Ransomware | 20×20 | one per creature line | ☑ | `/assets/ICON_LINE_RANSOMWARE.png` |
| `ICON_LINE_WORM` | Line-select row glyph — Worm | 20×20 | add `ICON_LINE_*` per line as unlocked | ☑ | `/assets/ICON_LINE_WORM.png` |
| `UI_COUNTDOWN` | Lockout countdown digits/style | ~64×24 | `FONT_UI`-based; pairs w/ `FX_LOCKOUT_BAND` | ☐ | |

> Reused, no new art: `FX_LOCKOUT_BAND` / `FX_EVO_FLASH` / `FX_CRITICAL_FAIL` / `FX_GHOST` /
> `FX_CORRUPTION` (§D), `UI_HINT_BAND` (§F), `UI_STAT_GAUGE` / `UI_STAGE_INDICATOR` /
> `UI_CARE_PIPS` (§G/§O), `SPR_PET_*` egg + Stage-1 frames (§C), `ICON_ITEM_*` (§H). Worm
> isolate's "real egg" tint is engine, no art. **Egg-crack** = optional procedural overlay or a
> few `SPR_PET_*` frames (D-side, non-blocking).

---

## Q. Minigames & Combat (Area 7)

**Autonomous auto-battle** shared by wild encounters + Sim-
Battle. Combat Health/charge bars are **`UI_GAUGE` variants** (`03 §0.2`), not new primitives;
combat hit/clash FX are **procedural activity visuals, NOT pipeline passes**.
Most chrome is reused — only the rows below are new, and most are optional polish.

| Asset ID | Element | Logical size | Notes | Status | File |
|---|---|---|---|---|---|
| `UI_HEALTH_BAR` | Combat-Health row | ~208×24 | `UI_GAUGE` variant — transient Health | ☑ | engine-drawn |
| `UI_OVERRIDE_PIP` | Once-per-battle Exploit-override indicator | 16×16 | ready (bolt) / spent (×) | ☑ | `/assets/ICON_OVERRIDE_PIP{,_SPENT}.png` |
| `UI_MOVE_CHANNEL` | Multi-turn move wind-up | ~120×12 | `UI_GAUGE` variant; override decision cue | ☑ | engine-drawn |
| `ICON_MOVE_SLOT` | Loadout equip-slot row glyph | 20×20 | filled / empty / locked variants | ☑ | `/assets/ICON_MOVE_SLOT{,_EMPTY,_LOCKED}.png` |
| `ICON_MOVE_<name>` | Per-move glyph (roster) | 20×20 | all six shipping; the glyph name is built at draw time as `ICON_MOVE_` + the move id uppercased (`train_screen.cpp`), so a new move's icon needs no wiring — drop it in `assets/` and it lights up. TRAIN falls back to text for a move with none | ☑/▨ | `/assets/ICON_MOVE_{PACKET_STORM,FORK_BOMB,CHECKSUM_GUARD,BUFFER_OVERFLOW,ROOTKIT_STRIKE,NULL_ROUTE}.png` (last three are `▨` stand-ins) |
| `UI_DAMAGE_POPUP` | Floating damage number | — | `FONT_UI` tabular digits — **procedural, no art** | ⊘ | — |
| `SPR_DUMMY` | Sim-Battle training-dummy sprite | ≤128×64 | wired for both tiers in `simDummy()` (`src/core/model/combat.cpp`) — they are the same prop, and the tier reads off the level/stat rows | ▨ | `/assets/SPR_DUMMY.png` (56×48) |
| `ICON_EVENT_WIFI` | Wi-Fi network event glyph | 20×20 | **optional** (D17); else reuse the EXPL Wi-Fi-globe motif | ⌫ | placeholder `/assets/_attic/ICON_EVENT_WIFI.png` |
| `UI_RANK_BADGE` | Hacker-Rank badge (CFG / HackerTag) | ~16–20 | **optional** (D18); else plain `FONT_UI` rank text | ⌫ | placeholder `/assets/_attic/UI_RANK_BADGE.png` (20×20) |

> **Reused (no new art):** `UI_GAUGE` (§O), `UI_DIFFICULTY_PIPS` (§J), `UI_HINT_BAND` /
> `UI_CURSOR_ROW` / `UI_ROW_SEL` / `UI_LIST_HEADER` / inline confirm (§F), `UI_BANDWIDTH_METER` (§J),
> the loot/result overlay (`02 §4.2`, reused for drops **and** rank-up), `SPR_PET_*` idle frames for
> both combatants + the awakened-guardian malbeast (§C), `FX_CORRUPTION` (§D) for the glitch/decoy/
> awakened tell, `ICON_TRAIN_SIM` (§I), `ICON_ITEM_SINKHOLE_TRAP` / `ICON_ITEM_TORTILLA_CHIP` (§H),
> `ICON_MOD_PACKET_SNIFFER` (§L), `FONT_UI` / `PAL_CORE` (§E).
> **Optional `SPR_PET_*` add:** a single `attack`/`lunge` pose frame on the §C frame template
> (`06 §1.6`, D15) — not required for v1.

---

## R. Hacker face — Crew Selection (Area 8, partial)

**Scope: the archetype submenu's own art** — the Hacker-face terminal centre-canvas treatment,
PROFILE/SHOP/VAULT/MERGE HUB, the CREW slot (enlistment + home network), PEERS and LINK (1v1
duels) are all built; the Red/Blue archetype system that would consume the per-archetype rows is
not (`docs/MASTER_TODO.md §1h`). Author on
placeholder palette; team Red/Blue hues + status/accent separation pending Design (S64/D20). Most
chrome is reused from §F — only the rows below are new, and per-archetype icons are optional.

| Asset ID | Element | Logical size | Notes | Status | File |
|---|---|---|---|---|---|
| `ICON_CREW` | Hacker-face **slot icon** (CREW) | 28×28 | dim/bright; crew duality (sword+shield) | ☑ | `/assets/ICON_CREW.png` |
| `ICON_TEAM_RED` | Team marker — Red · Operators | 16×16 | dual-coded glyph (sword) | ☑ | `/assets/ICON_TEAM_RED.png` |
| `ICON_TEAM_BLUE` | Team marker — Blue · Guardians | 16×16 | dual-coded glyph (shield) | ☑ | `/assets/ICON_TEAM_BLUE.png` |
| `ICON_ARCHETYPE_STATE` | Archetype row marker | 16×16 | active / unlocked / locked states | ☑ | `/assets/ICON_ARCHETYPE_{ACTIVE,UNLOCKED,LOCKED}.png` |
| `ICON_LOCK` | Generic small lock glyph | 12×12 | locked rows + unlock hint | ☑ | `/assets/ICON_LOCK.png` |
| `ICON_ARCHETYPE_<name>` | Per-archetype icon (6) | 20×20 | **optional** polish; seed `BOTMASTER`·`INSIDER_THREAT`·`GHOST`·`ORCHESTRATOR`·`WATCHDOG`·`DISPATCHER`; v1 may run text-only | ⌫ | all 6 parked at `/assets/_attic/ICON_ARCHETYPE_*.png` — the archetype system that would draw them is unbuilt |
| `ICON_PROFILE` | Hacker-face PROFILE slot icon | 28×28 | dim/bright; hacker-status identity motif (`07 §9`) | ☑ | `/assets/ICON_PROFILE.png` |
| `ICON_SHOP` | Hacker-face SHOP slot icon | 28×28 | dim/bright; storefront/marketplace motif (`07 §10`) | ☑ | `/assets/ICON_SHOP.png` |
| `ICON_VAULT` | Hacker-face VAULT slot icon | 28×28 | dim/bright; safe/lockbox motif — sealed-cache decrypt (`07 §11`) | ☑ | `/assets/ICON_VAULT.png` |
| `ICON_MRG` | Hacker-face MERGE HUB slot icon | 28×28 | dim/bright; two nodes branching into one. The slot starts inaccessible, so it usually draws under `ICON_SLOT_INACCESSIBLE` — the glyph is legible through the double-dim, which is what makes the SHOP purchase read as unlocking a real slot | ☑ | `/assets/ICON_MRG.png` |
| `ICON_MRG` locked variants | Bespoke locked Merge glyphs (padlock badge · severed branch + padlock) | 28×28 | ⌫ parked at `/assets/_attic/ICON_MERGE_LOCKED{,_ALT}.png` — the carousel composites the shared `ICON_SLOT_INACCESSIBLE` marker over every inaccessible slot, so a per-slot locked master would make MRG the one exception to a device-wide convention | ⌫ | `/assets/_attic/ICON_MERGE_LOCKED{,_ALT}.png` |
| `ICON_PEERS` | Hacker-face PEERS slot icon | 28×28 | dim/bright; met-operators motif (two figures / a handshake). The slot is **live** and renders text-only in the carousel without it — drop-in, no engine change | ☐ | |
| `ICON_LINK` | Hacker-face LINK slot icon | 28×28 | dim/bright; two pets facing off across a bolt — the 1v1 duel slot | ☑ | `/assets/ICON_LINK.png` |
| `ICON_SLOT_INACCESSIBLE` | Inaccessible-slot overlay (both faces) | 20×20 | composited over a 28×28 slot; no-entry/lock motif; distinct from row-level `ICON_LOCK`; used for undesigned Hacker slots + stage-locked pet slots (`07 §12`) | ☑ | `/assets/ICON_SLOT_INACCESSIBLE.png` |
| `ICON_AUDIT_ARMED` | Audit-capture state — armed/sealed | 16×16 | pcap capture-policy state marker (STATUS §Audit; hot/seal/cooldown SM) | ⌫ | `/assets/_attic/ICON_AUDIT_ARMED.png` |
| `ICON_AUDIT_HOT` | Audit-capture state — hot/capturing | 16×16 | pcap capture-policy state marker (STATUS §Audit) | ☑ | `/assets/ICON_AUDIT_HOT.png` |
| `ICON_AUDIT_COOLDOWN` | Audit-capture state — cooldown | 16×16 | pcap capture-policy state marker (STATUS §Audit) | ⌫ | `/assets/_attic/ICON_AUDIT_COOLDOWN.png` |

> **Reused (no new art):** `UI_SUBMENU_HEADER` / `UI_LIST_HEADER` / `UI_CURSOR_ROW` / `UI_ROW_SEL` /
> `UI_SCROLLBAR` / `UI_HINT_BAND` (§F), `FONT_UI` / `PAL_CORE` (§E). Role / ability name / `⚡` sigil /
> status tags (`[ACTIVE]` / `Active ✓` / `locked`) = `FONT_UI` text — no tag art.
> **Deferred (→ Design / future, no row yet):** Hacker-face **centre canvas / HUD reskin** (terminal-
> green + team accent, `07 §8` / D22) — owner left this to Design; the other Hacker slots.

---

## S. Web 'Pedia (SD-served site)

The AP-hosted 'Pedia as a real static bundle + a live data feed, landed
2026-07-13/14. Design's site delivery + the `tools/gen_pedia_data.py` sync generator, which reads the
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
| **Achievement icons** | Per-achievement glyphs, named on each row's `icon` field (`src/core/content/content_achievements.cpp`) | **60 rows, 60 bespoke `ICON_ACH_*` glyphs.** Most are LADDERS, so they are one motif plus a centred tally — five skulls for the boss tiers, five depth arrows, four wifi arcs — countable before anything else, which is what lets a grid of them read in grayscale. The three creature lines are sets rather than ladders, so they use the line's own mark (lock / hook / horse) under a footer: a bar for "raised them all", a chevron for "took one deep". `DEVTOOLS_INTRUDER` is deliberately a question mark over a redaction bar — the 'Pedia draws a row's icon even while the row is masked, so anything representational there would spoil the one achievement whose point is working it out | ☑ | `/assets/icons/ICON_ACH_*.png` |

> **Malbeast sprites are `▨` stand-ins, but they are the shipping art** — `make pedia` copies them
> from `assets/` into `web/assets/` like any other sprite, so the site and the device show the same
> six frames. Final art replaces both at once.

---

Every asset-consuming system is built. The one open feature behind §R's rows is the Red/Blue
archetype layer (its optional `ICON_ARCHETYPE_*` art) — see `docs/MASTER_TODO.md §1h`. CREW,
PEERS and LINK all ship today: CREW and LINK have their icons, while PEERS renders its label
alone until `ICON_PEERS` is drawn.
