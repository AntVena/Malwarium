# Visual Language

The shared visual system every pet-side screen draws on: the **palette** (`PAL_CORE`), the
**font** (`FONT_UI`), the **icon system**, and the **layout grid**. Every screen defers its
colour/font/silhouette craft here so it is decided **once**.

This doc is a **system spec + constraints brief**, not finished pixels: it fixes the *roles,
sizes, tiers, and rules* that any particular hue, font face or silhouette then has to satisfy.
Architecture constants (×1.75 scale, 224×224 active + 8px bezel, the §4
pass-order) live in `src/core/render/RENDER_PIPELINE.md` — **referenced, not duplicated**. Assets resolve into
`ASSET_MANIFEST.md §E` (+ the icon tiers note).

---

## 0. Principles

1. **Low-res first.** Everything must read at 128×128 logical *before* the ×1.75 upscale — if a
   glyph or icon isn't legible small, no scaling saves it.
2. **At-a-glance.** This is a ~4fps glance-and-go device, not a screen you study. One signal per
   channel; silhouettes and contrast over detail.
3. **High contrast, low literacy.** Strong figure/ground separation; clear letterforms; the
   shorthand label stays available. Cheap panels wash out — design over-contrasted, not subtle.
4. **Colour is reinforcing, never sole.** Every status meaning carries a **non-colour channel
   too** (position / shape / motion), so the UI survives a monochrome panel and colour-blind
   vision (§1.3). This formalises the same zone-colour rule the stat gauges use, for the whole UI.
5. **Restraint.** Small fixed palette, few sizes, consistent metaphor family (hardware /
   infosec motifs) — the retro vpet read comes from discipline, not decoration.

---

## 1. Palette — `PAL_CORE`

### 1.1 Hardware reality

The framebuffer is **RGB565** (16-bit) on a 262K-capable ST7789. Plenty of
colour headroom, so the limit is **design discipline, not hardware**: `PAL_CORE` is a small,
fixed set of **named role tokens**. UI chrome only ever references a token, never a literal —
so a hue change reskins the whole UI from one place. (Pet sprites carry their own
per-creature colours within the same discipline; they are **not** part of `PAL_CORE`.)

### 1.2 Token model (roles, not hex)

| Group | Token | Used by | Notes |
|---|---|---|---|
| **Structural** | `paper` | base background | the "off" field; sprites/chrome sit on it |
| | `ink` | text, lines, default-lit gauge cells | primary foreground; must hit strong contrast on `paper` |
| | `ink-dim` | unfocused icons (50%), future stage nodes, gated rows | engine-derived from `ink` (manifest §A) — not a separate hue |
| | `track` | carousel track strips (`UI_TRACK_BG`) | **solid** for legibility |
| | `accent` | focus/selection: `UI_CURSOR_BOX`, `UI_ROW_SEL`, `UI_CURSOR_ROW` | one hero colour; never reused for a status meaning |
| **Semantic (status zones)** | `calm` | gauge OK, care pips 0–2 (Good), healthy | luminance-ordered, **danger-ascending** |
| | `warn` | gauge Caution | |
| | `hot` | gauge Critical, care pips dying, Lockout band, Critical System Failure | always = danger, regardless of fill polarity |
| **Rarity** | `rarity-common` … `rarity-epic` | `UI_RARITY_TAG` (4 tiers); the ITEMS row ladder + its tinted glyph; VAULT cache rows | a dull→bright ramp; tier readable by ramp position, not hue memory |

Target size: **~12–16 UI tokens total** (structural + semantic + rarity), plus the creature
palette outside `PAL_CORE`. Small enough for a coherent retro read; large enough to cover
every chrome need across the shipped screens.

### 1.3 Rules

- **`accent` ≠ any status token.** Selection must never be confusable with "danger" — focus and
  alarm are different languages.
- **Status is dual-coded** (§0.4): `calm`/`warn`/`hot` never carry meaning **alone**. They ride
  on top of: gauge **fill level**, care-pip **count + gate**, and the Critical **pulse**. A
  grayscale screenshot of any screen must still be readable — that's the acceptance test.
- **Contrast budget:** `ink`-on-`paper` is the highest-contrast pair (text/numerics legibility
  on washed-out panels). `warn`/`hot` must also clear contrast on `paper` since they tint text
  (gauge numerics, Lockout digits).
- **Brightness/dim states are engine-derived**, not extra tokens — one master value per token
  (manifest §A), dimmed in engine.

---

## 2. Typography — `FONT_UI`

### 2.1 Requirements

A single **pixel/bitmap** family (no anti-aliased vector — it smears at this size). Must be:

- **Legible at the logical sizes below**, after the whole-canvas ×1.75 upscale.
- **Disambiguated glyphs:** distinct `0/O`, `1/I/l`, `5/S`, `8/B` — load-bearing for gauge
  numerics, the Lockout countdown, the HackerTag editor (`A–Z 0–9 _`), and the FW version string.
- **Tabular (fixed-width) digits** so gauge values and countdowns don't reflow as they change.
- **Generous x-height, no condensed forms** — low-literacy legibility (§0.3).

### 2.2 Type scale (logical px, pre-upscale)

| Role | ~Size | Used by |
|---|---|---|
| Title / header | 12 | `UI_SUBMENU_HEADER`, modal titles |
| Body / row | 10–12 | list rows, item effect text, record fields |
| Caption / hint | 8–10 | `CAP_SLOT_LABEL`, `UI_HINT_BAND`, tags |
| Numeric | 10–12 (tabular) | gauge values, `UI_COUNTDOWN`, counts |

One family at a few sizes — not multiple faces. Any replacement face must satisfy §2.1 at
every size above.

---

## 3. Icon system

### 3.1 Size tiers (logical)

Every icon snaps to one of **four tiers**; new icons pick the nearest. This keeps optical
weight consistent across the UI.

| Tier | Used by |
|---|---|
| **28×28** | carousel slot icons (8) — `ICON_STAT…ICON_CFG` (manifest §A) |
| **20×20** | submenu row glyphs + content icons — `ICON_ITEM_*`, `ICON_MOD_*`, `ICON_SECTOR_*`, `ICON_LINE_*`, `ICON_MAINT_*`, `ICON_TRAIN_SIM`, `ICON_EXPL_PACKET`, `UI_ALERT_HUNGER` |
| **16×16** | status + control glyphs — `ICON_SYS_*` (battery/wifi/SD), `ICON_BTN_A/B/C` |
| **12×12** | inline log glyphs — `ICON_LOG_EVENT` (`item`/`warn`/`combat`) |

### 3.2 Silhouette + state rules

- **Reads as a silhouette before colour.** Recognisable in a single flat fill, so it survives
  the `ink-dim` state and a monochrome panel.
- **Single visual weight**, consistent corner/line treatment across the set — one hand.
- **One master per icon;** dim/bright (focused/unfocused) is engine brightness, not two files
  (manifest §A).
- **Shared metaphor family:** hardware + infosec motifs (drives, CPUs, racks, packets) so the
  set feels like one world.

### 3.3 The 8 slot icons

Concepts are **fixed** by the carousel (`docs/ORIENTATION.md`): Heart-w/-graph = STAT, USB =
ITEMS, reticle = TRAIN, Wi-Fi globe = EXPL, fragmented HDD = MAINT, cracked CPU = MODS, rack =
ARCH, gear+terminal = CFG). Size 28×28, one master each, dim/bright in engine.

---

## 4. Layout grid

The per-screen grids, consolidated into one table — the zones to draw art against.
[`src/core/ui/layout.h`](../src/core/ui/layout.h) is the same grid as numbers, and it is what
every screen reads: `kMargin` · `kTitleY` · `kHeaderRule` · `kRowTop` · `kRowH` · `kRowIcon` ·
`kVisibleRows` · `kLineH`. A screen that wants a value off this grid names it for itself
(`kTrackRowTop`, `kPickRowH`) rather than redefining one of those, so a deviation is visible
where it happens.

| Screen type | Vertical zones (logical, within 224×224 active) | Columns |
|---|---|---|
| **Canvas / carousel** | top track **40** · living area **144** · bottom track **40** | 4 × **56** per track |
| **Submenu (list/viewer)** | header band **24** (title at **6**, rule at **22**) · content **~200** · rows **28** → **6 rows** | full width |
| **Hacker sub-screen** | the submenu band, plus a context line at **28** ruled off at **44** | full width |
| **STAT vitals** | name+stage **~28** · gauge rows **~24** ea · care **~22** | full width |
| **Modal event** | full **224×224**, no header/track chrome | — |

Global: **8px bezel** (no UI) every edge; whole-canvas **×1.75** integer upscale; **no
downscaling**. Author all UI to land on clean logical-pixel boundaries so the
upscale stays crisp.

---

## 5. UI Mode + caption

The carousel/label presentation is user-selectable in CFG — the visual system must
render all three:

| UI Mode | Slot/row renders as | Notes |
|---|---|---|
| **Icons + Label** *(default)* | icon **+** full-word caption | low-literacy default |
| **Icons only** | icon alone | once the icon set is learned |
| **Text only** | shorthand text (`STAT`/`ITEMS`/…) | for text-first users |

- **Caption = full word** (`ITEMS`, not `ITM`) — clarity over terseness.
  `CAP_SLOT_LABEL` shows one caption, on the focused category only.
- Every category therefore needs **both** an icon and a shorthand (already true across the docs),
  so any mode is a pure render switch — no missing assets.

---

## 6. Assets → `ASSET_MANIFEST.md`

This system **locks existing manifest rows** rather than adding art — no art ID originates here:

- **§E `FONT_UI` / `PAL_CORE`:** `PAL_CORE` is the §1.2 token set (14 roles), delivered as
  `assets/PAL_CORE.json`; `FONT_UI` is the §2 pixel family + type scale, delivered as Pixel
  Operator Mono. UI chrome binds to a token, never a literal hex — that is what keeps a hue
  change a one-file reskin.
- **§A slot icons:** concepts fixed (§3.3); the **28×28 + dim/bright** spec is confirmed.
- **Icon size tiers (§3.1)** are a manifest note, so every future `ICON_*` row picks a tier.

---
