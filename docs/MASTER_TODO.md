# Malwarium — the board

> **The one list of what's open.** Planning/design decisions, code TODOs, and art we'll need soon.
> Each item carries a **difficulty** (S/M/L/XL) and a **model recommendation** so work can be
> dispatched without re-deriving context.
>
> **Anything completed and tested is DELETED from this list, not checked off.** This is a planning
> board, not a log — the code and `git log` are the only living history we need, and kept-around
> history just dilutes searches. Same for entries in flight: don't annotate what changed, describe
> what's left.
>
> Don't reference this file from a code comment. It's transient by design; the comment isn't.

---

## How to read the difficulty / model columns

**Difficulty:** `S` ≈ an afternoon, localized · `M` ≈ a session, one subsystem · `L` ≈ multi-file,
needs a save bump or new data model · `XL` ≈ architectural / cross-cutting / needs a design pass first.

**Model fit:**

- **Sonnet** — the shape is known: data entry into existing tables, drop-in art integration, porting
  an existing UI pattern, a localized mechanic with named tunables. Most `S`/`M` items with a clear
  "Where" line.
- **Opus** — judgment or novelty required: architectural changes, anything touching the save schema
  non-mechanically, a first design pass for an unbuilt system, balance passes needing measurement +
  taste, or reconciling a contradiction between intent and what shipped. Anything flagged 🏛 or ❓.

**When testing something here, ask first whether a test is warranted** — does another test already
cover it? Does it matter if it fails? If it is worth one, manufacture the starting state rather than
building it up organically.

---

## 1. Code

### 1a. Ready to build

| ID | Item | Where | Diff | Model | Notes |
|---|---|---|---|---|---|
| FB-MECH6-b | **Stacker defrag minigame** — the item-free defrag variant (skill instead of the Defrag Tool, same guaranteed clean). | New minigame; the MAINT defrag flow already has the two-variant scaffold. | M | Opus | Fresh build — needs interaction design + a new screen. |
| FB-UI4 | **Evolution reveal animation** — flash the pet silhouette + the possible-evolution silhouettes repeatedly before landing. | Evolve overlay. Needs the branch-candidate set + silhouette rendering. | L | Opus | Depends on the eye/silhouette metadata below. Design-heavy. |
| FB-CFG4 | **Deep-sleep travel mode** — a true pause for "I'm travelling and don't want to babysit a vpet." | New CFG action + **freeze the game clock**: record the sleep instant, stop the model tick, and on wake rebase so the elapsed gap is **not** credited to hunger/decay/growth/egg-accel/audit/CSF. Wake only via the hacker chord. | XL | Opus | Approach confirmed; still architectural, since everything keys off `nowMs_`. No timer should observe any elapsed time across the sleep. Distinct from the idle *screen* sleep, which keeps ticking. Secondary goal: save as much battery as possible while away. |
| FB-DSGN7 | **Iconic per-line move effects** (Ransomware: gamma pulse + scaled white/green silhouette layers, +1px layer per Evo-level of the move). | Move-fx system, per-line. | L | Opus | Needs a creature-line taxonomy + the effect system first. Cosmetic only. |
| NA | **Sub-area bosses that are themselves gauntlets** — `subAreaBoss` returns a length-1 `BossGauntlet`, so a sub-area boss is always exactly one fight; only the AREA boss is multi-round. Castle Rapidscare's THE EIGHT PWNS wants to be a minor gauntlet, and its JOKER VIRUS wants to *loop back* into another Pwns run after it falls. | `combat.cpp`'s `subAreaBoss`/`areaBoss`, plus the round plumbing in `game_explore.cpp` (`startBossRound`/`finishBossRound`). | L | Opus | The re-entrant loop is the novel part — the carried-Health round machinery is linear today, with no notion of a round that re-queues an earlier one. Needs a design pass on how a loop terminates and what it pays. |
| NA | **Confirm on device that a save lands while audit capture is armed.** The write path no longer asks for the blob in one piece: `Game` owns the serialize buffer and `captureSave` reserves every collection to its exact count, so an ordinary save is judged against `kSaveHeapFloorBytes` (12KB) rather than the reservation. Built and green on the native + S3 gates; the hardware run that proves `held` stays at 0 through a whole capture session has not been done. | `game_persist.cpp`'s `persistSave` and the `kSaveHeapFloorBytes` / `kSaveGrowHeapFloorBytes` pair. | S | Sonnet | `HEAP_TRACE_ENABLED` (config.h) → 1, then CFG → RADIO → AUDIT → SCAN+CAP, 'Pedia AP on and off again. Success = `held=0` with `floor=12288` and no boot banner. Read the log, not the panel — a reset is fast enough to look like nothing happened. |
| NA | **Capture arming costs ~70KB and the AP ~58KB**, against ~126KB free with the radio idle. The device works, and the save no longer needs a big contiguous block, but that was the only thing standing on this — anything else that grows will hit the same wall. Worth a pass at what the capture path actually needs. | `net_capture.h`'s `powerUp` (`esp_wifi_init` + promiscuous + the pcap SD buffers). | M | Opus | Measured on device, not estimated: `[ap] down free=126408` → `[cap] armed free=56188`. |
| NA | **PSRAM is disabled.** The board carries 8MB (`platformio.ini` leaves it off — "nothing needs it yet"), and it is the biggest available lever on the row above: Wi-Fi needs internal DRAM for DMA, the pcap and save buffers do not. Enabling it changes the *bootloader*, which an OTA never writes — so it can only reach a shipped device through the browser flasher (`pages/flash/`), and switching it on means asking a remote operator to plug in. | `platformio.ini` (`-DBOARD_HAS_PSRAM`, `board_build.arduino.memory_type`). No configured pin uses GPIO 33-37, so an octal part looks unobstructed — unverified. | M | Opus | No longer blocked on tooling, only on the cost of the ask. **Do the flasher bench run below first** — it is what proves the escape hatch works before a change depends on it. Confirm with `ESP.getPsramSize()` on the boot line if it is ever switched on. |

### 1b. Cooking — the open follow-ups

The pantry, per-item drop weights and the N-ingredient Merge Hub are built. What the first cut left:

- **Drop weights are unmeasured.** The pantry's numbers are authored by flavour, not play data. The
  walk-pool thinning constant (`kStapleWalkWeight`) especially is a guess at how much staple a player
  should wade through to reach a diving bell. Diff **M** / Opus (needs measurement runs).
- **The pantry is line-agnostic.** Every staple drops everywhere. Once per-area food sets land (§2c)
  the two systems should meet — a `LootEntry` weight override per area pool is already the mechanism,
  so that's content, not code.

### 1c. Icon storage — 1-bit masks

Every `ICON_*`/`UI_*` master is a single flat `ink` fill with **zero partial-alpha pixels** (74 files
measured), because dim/bright is engine brightness. They compile to RGB565 + alpha all the same, so
33,344 pixels cost **98 KB of flash to carry one bit each** — as 1bpp masks that is 4.1 KB, a 24×
saving with no art risk, since there is nothing but the mask to preserve.

Where the bytes actually are, though, is the **sprites**: 180,360 px = 528 KB of the 626 KB atlas,
up to 19 distinct colours each. 8-bit indexing is safe there (~3×, ~350 KB); 4-bit needs a per-sprite
colour reduction and real art review.

Neither is urgent — flash isn't tight, and tinting no longer depends on either (it ships). Diff
**M** / Sonnet for the icon masks, **L** / Opus for the sprites.

### 1c-ii. Tinting — a second theme

**A second theme is a design pass, not a build.** The machinery takes N themes today and
`PAL_CORE.json` documents the block shape; authoring a colourblind-friendly set (moving the
red/green semantic pair onto a blue/orange axis) needs hue decisions, plus a CFG row to select it
and a save field to remember it. Diff **M** / Opus.

### 1d. New explore area — NET-C, sailing around the Pirate Bayou

Adding an area is a mechanical folder-add (`src/core/content/areas/AREA_CONTENT_STANDARD.md`). Diff
**M** / **Sonnet** for the build. No save bump — explore vectors are length-prefixed.

**This one INSERTS mid-ladder**, which the castle didn't: every save-flag array is indexed by
ladder position, so slotting NET-C in ahead of a shipped area re-points an existing save's cleared
flags at the wrong area. Decide first whether it appends after Castle Rapidscare (free) or inserts
(needs a save migration that shifts the explore vectors). Diff **L** / Opus if it inserts.

A NET-C ladder position ahead of the castle also wants the mod `powerTier` bands re-checked — the
tiers currently run 1/2/3 for the three early areas with 4 shared by the castle and the DeepWeb
Dive, and an inserted area has no band of its own.

Sailing around Pirate Bayou (sub-area names cap at **18 characters** — past that the EXPL row draws
over its own state tag; `test_expl_names_fit_their_rows` fails the build on it, so TRACKER TIMEOUT
TRENCH needs shortening before it ships):
* Uninstall Undertow
* Whirlpool
* Tracker Timeout Trench
* Codec Reef
* Sandbox Beach
Shops
- Mod shop: The Hardened Shell
- Food shop: Floating Point
Bosses
- Undecided, pick 5 placeholders

### 1e. FONT_UI integration (drawn, not wired)

`FONT_UI` (Pixel Operator Mono) is delivered as art/spec but code still renders every screen through
the built-in 5×7 placeholder (`src/core/render/font5x7.*`). Highest per-screen leverage of any single
change — one integration lifts typography device-wide, and tabular digits improve every gauge, stat
and timer at once. The manifest marks it `☑` delivered, which is about the art: **the code side is
unbuilt.** Diff **M** / **Opus** (bitmap-font pipeline into LovyanGFX + re-verifying every grayscale
and layout gate is fiddly and cross-cutting).

### 1f. Standing stubs / interim mechanics to revisit

Intentional simplifications. None is a bug; each is a "confirm as v1 or revise".

- **"LINK" names two different things.** The CFG **RADIO → LINK** row is consent to BROADCAST
  identity over ESP-NOW; the Hacker face's **LINK** slot is the 1v1 duel surface that consent
  enables. Adjacent screens, same word, different referents. Naming call, not code: renaming either
  ripples through the docs and the peer/duel screens' copy. Diff **S** / Opus (taste, then a
  mechanical rename).

### 1g. Test-infrastructure gaps

- **No serial test-hook / no automated on-device gameplay verification.** Every device check to date
  is "flash, read the boot line, confirm no crash loop" — nobody has walked the buttons through
  EXPL/combat/Wi-Fi/rank-up on the real panel in a long time. Diff **M** / Opus (harness design). A
  human bench pass is also owed.
- **No Wokwi screenshot-regression tier** — deferred by decision; the host tier covers most of it.

### 1h. Web 'Pedia

Every reveal tier is backed by persisted state. **RAISED** = any pet that was, or could have been,
active on this device — written at the single `Game::installPet` seam, so hatch, evolution, an ARCH
Deploy and the Trojan divert are all covered by construction. **SEEN** = faced in a fight; a menu,
hint or cinematic doesn't count, which leaves a **duel opponent** as the only creature-species
writer (every PVE combatant is a name-only `CombatEnemy` from a pool deliberately disjoint from the
roster, and the wild half keeps its own roster-keyed masks).

**Open:**

- **A solo operator's "seen" tier is empty until they duel.** If the Daemon branch-sibling reveal is
  worth persisting it wants its own tier ("teased") rather than sharing this bit — two meanings on
  one flag is what was deliberately removed. Diff **S** / Opus (design, not storage).
- **❓ Multi-frame sprites in the 'Pedia — NOT REPRODUCED, needs a repro.** Every one of the 26
  creature/malbeast sheets was rendered through `sprStyle`'s own formula and each cropped frame 0
  correctly, at every geometry the roster has (1/2/4/8-frame strips, the 2-row KeyloggerHead sheet,
  and the oversized 96×64 and 64×56 single cells) — so the "only Malbear's dimensions work" reading
  doesn't hold against the current data. Two candidates for what was actually seen, both real:
  frame 0 of a multi-purpose sheet is not always the idle pose (`phrogspawn` shows a hatch swirl,
  `cachemutt` a crying frame), and the 'Pedia is static where the device animates. If that's the
  complaint it wants the anim-clip table exported to the web and is **M** / Opus, not a bug fix.
  **Needs the PO to say which screen and which creature looked wrong.**
- **Two achievement rows have no firing site**: `WORM_WHISPERER` and `AIR_GAPPED`. Neither is a
  missing unlock CALL — both preconditions are absent. The Worm line has no content rows at all, and
  the Replication Ghost is a modelled state nothing ever enters: `PetModel::ghost_` has a getter, a
  setter and a save field, but no code anywhere sets it true. `AIR_GAPPED` also promises a cure by
  **Air-Gapped Snack**, while the only thing that clears the flag is `applyAntivirus` — so the row's
  copy and the model disagree about the mechanic even once a ghost can exist. Deciding what raises
  a ghost, and whether the snack is what settles it, is design. Diff **M** / Opus.
- **No on-device browser.** The home-screen banner is the whole feedback channel. If achievements
  ever want a device-side list, the Hacker face's PROFILE slot is the natural home. Diff **M** / Opus.
- **Unverified:** on-device serving of the SD-hosted bundle + the live endpoints
  (`GET /pedia_state.json`, `POST /api/tag`) on a real board.
- **Polish:** the real `FONT_UI` face for the web bundle (a system-mono fallback today), and bespoke
  achievement icons (every row reuses a shipped item/line/slot glyph).

### 1i. Hacker-face CREW — enlistment shipped, Red/Blue archetype layer open

- **The Red/Blue archetype + PvP contest layer.** Archetypes (Operators: Botmaster/Insider
  Threat/Ghost vs. Guardians: Orchestrator/Watchdog/Dispatcher), the capture broadcast-window
  lifecycle, and target-cooldown + Honeypot/IDS defensive mods have no code. Diff **XL** / Opus —
  needs a first design pass reconciling the ability list against everything that's shipped since.
- **More crews + Red-side crews.** Adding one is a `kCrews[]` row; a new ability shape is one
  `CrewExploitKind` entry plus one case in `Combat::applyCrewExploit`. What's missing is the *earn*
  model — today every crew is joinable the moment a home network exists. Diff **M** / Opus.
- **What "defender of that network" buys you.** The home network is recorded and shown but has no
  mechanical consequence beyond gating enlistment. Diff **M** / Opus.

### 1j. Over-the-air updates — shipping; the failure paths are what's left

The device-side path exists and is native-gated: the job-scoped STA association, a phone-driven
setup portal, a strict manifest parser, the CFG **UPDATES** screen (connect → check → per-artifact
verdict → yes/no → download → SHA-256 → install), firmware via `Update.h` into the inactive OTA slot
and the web bundle via uncompressed tar onto SD. Settled and NOT open: going online is scoped to a
running job and never persisted, so there is no internet toggle to leave on; always check → ask →
explicit yes, never auto-install; integrity by per-artifact SHA-256 with **no code signing** and
`setInsecure()` — the hash guards a corrupted download, not a hostile network, and that trade is
written down in `update_manifest.h`.

Both halves have installed on hardware over the air, firmware and web bundle, with no USB involved.
A firmware install now boots on trial and rolls itself back unless it reaches the main loop, paints
a frame and stays up for `OTA_PROVE_MS`.

Publishing is CI-driven off a `v*` tag (ORIENTATION's *Releasing*). The web bundle is written by
`tools/make_web_tar.py` rather than `tar` so it is byte-reproducible: the manifest publishes a
SHA-256 over it, and an archive whose bytes move with the build turns that digest check into a race
against the next publish — a device that fetched the manifest before a rebuild and the artifact
after one would report `Corrupt` for what is really the same content.

**Open:**

- **Bench the failure paths.** Every one is handled in code and none has been exercised on a board;
  the laptop-publish flow makes each a one-line edit to `dist/`. Truncate an artifact (expect
  `Truncated`, slot never made bootable) · flip a byte after publishing (expect `Corrupt` at the
  digest check) · stop the server mid-download (expect `Truncated` via the stall timeout) · pull the
  card during a web install (expect `WriteFailed`, and the version marker still reading the OLD
  version, since it's written last). **And the rollback gate itself**, which is the one with no
  native stand-in: publish a build that panics in `setup()`, confirm the device installs it, boots
  it once, and comes back on the previous firmware. Diff **M** / Opus (needs a board + a publish
  host). Fold in the **re-provisioning self-present** while a board is on the bench: a device whose
  last CONNECT failed should pop the captive portal onto `/setup` for a phone joining its AP. It
  compiles and the logic is one condition in `handleProbe`, but no phone has met it.

- **Bench the browser flasher on a board.** `pages/flash/` (vendored `esptool-js`) writes all four
  boot images at their S3 addresses and is verified end-to-end *up to* the serial port: the pages
  render, the manifest hydrates, the artifacts fetch and the firmware's SHA-256 matches what the
  manifest publishes. No board has been on the other end of it. What a run has to answer: that
  holding **A** (GPIO0, the download strap) while connecting enumerates a port Chrome will offer;
  that `default_reset` → `UsbJtagSerialReset` syncs on the S3's native USB; that a flash at
  `baudrate == romBaudrate` finishes in a tolerable time; and that the board comes back with its
  **save intact** when ERASE is left off, since that is the promise the page makes in bold. Then
  the same run with ERASE on. Diff **M** / Opus (needs a board). The device-side half — CFG →
  UPDATES → FLASH OVER USB drawing the code — is native-gated and rendered, not yet scanned.
- **Hosting is settled and live** — GitHub Pages, deployed by `.github/workflows/publish.yml` on a
  `v*` tag; see ORIENTATION's *Releasing*. `https://antvena.github.io/Malwarium/manifest.json` has
  served a real publish, verified by fetching it back and re-parsing the served bytes with the
  device's own parser. The same deploy now carries `pages/` and the three boot images, so the host
  answers both halves. `UPDATE_MANIFEST_URL` is still compiled EMPTY on purpose: a stored override
  is the only source, it is reachable from the device's own setup page, and the shipped device is
  already using it. Nothing open here.
- **Credentials sit in NVS in PLAINTEXT** (stock ESP32 NVS is unencrypted). Stated plainly rather
  than implied-protected; NVS encryption is a separate feature and is not enabled. Revisit only if
  the threat model changes.

---

## 2. Art

Engine slots for these exist (they render via placeholder or text today), so most are **drop-in the
moment they're drawn** — Sonnet integration jobs once the art lands. The art itself is Design's.
Sizes are logical px; bind colour to `PAL_CORE` tokens. Inventory: `assets/ASSET_MANIFEST.md`.

### 2a. Ready-now, zero-friction

| Asset | For | Size | Diff to integrate |
|---|---|---|---|
| `UI_HINT_BAND` | contextual control-hint band | 224×24 | S / Sonnet |
| `UI_PROGRESS_BAR` | reusable fill bar (EXPL/MAINT/hold-to-commit) | ~180×12 | S / Sonnet |
| `UI_SLOTS_USED` | ARCH rack-slot usage indicator (`slots 2/4`) | ~44×12 | S / Sonnet |
| `UI_COUNTDOWN` | Lockout countdown digit style | ~64×24 | S / Sonnet — folds into FONT_UI work |
| `ICON_EXPLORE_STATE` | optional sub-area row state marker | 16×16 | S / Sonnet |
| `ICON_CFG_RADIO` | RADIO group row — reuses `ICON_SYS_WIFI` today | 20×20 | S / Sonnet |
| `ICON_CFG_UPDATE` | UPDATES row — reuses `ICON_CFG_SYSINFO` today | 20×20 | S / Sonnet |
| `ICON_PEERS` | Hacker-face PEERS slot — renders text-only today | 28×28 | S / Sonnet |

### 2b. Placeholder → final art

These read fine by name + pips today; final art is polish, integrated drop-in (Sonnet). Roughly
high→low value:

- **Three move glyphs** still placeholder: `ICON_MOVE_BUFFER_OVERFLOW`, `_ROOTKIT_STRIKE`,
  `_NULL_ROUTE` (TRAIN falls back to text without them).
- **Two finished glyphs have no screen to draw them**, and neither is wiring-blocked — both wait on
  something else. `ICON_SECTOR_0` is the area icon `AREA_CONTENT_STANDARD.md` asks every area for,
  but it's the only one drawn: wiring it lights sector 0 and leaves 1–3 bare, so it wants the
  matching `ICON_SECTOR_1..3` first (§2c). `ICON_LINE_WORM` needs the Worm line to have content at
  all. Both are held in `check_orphan_assets.py`'s KEEP list meanwhile. Diff **S** / Sonnet once
  their blocker clears.
- **Six wild malbeasts** (`SPR_MALBEAST_*`) and **`SPR_DUMMY`**.
- **Process alternates:** `SPR_PET_PHISHLET`, `SPR_PET_CIPHADPOLE`, `SPR_PET_PINGCUB`; **Boot L2:**
  `SPR_PET_RINGWYRM`.
- **Redraws:** `ICON_MOD_CLOCK_SPEED_BOOST` (reads as a broken clock part), `ICON_TRAIN_SIM`
  (ambiguous bullseye-dummy). Both ship; pure legibility polish.
- **Optional polish:** `UI_RANK_BADGE`, `ICON_EVENT_WIFI`, `UI_DIFFICULTY_PIPS`, a boss-tell marker
  on the charge bar, a `UI_TITLE_TAG` badge, richer per-sub-area `BG_SECTOR_*` backdrops, a
  `SPR_PET_*` attack-pose frame.
- **Six archetype icons** (`ICON_ARCHETYPE_*`) — cosmetic accompaniment to §1i; parked in `_attic/`.

### 2c. New art implied by unbuilt features

Design has to originate these; they come with the features above.

- **Napstorrent Moors area art** (shipped mechanically, art pending): `ICON_SECTOR_2`, `BG_SECTOR_2`
  (marshy → castle progression), the `MOOR-TO-MOOR` storefront motif.
- **Castle Rapidscare art** (shipped mechanically, art pending): `ICON_SECTOR_3`, `BG_SECTOR_3`,
  castle-themed malbeasts + a `COUNT COPYLEFT` apex, and the `SPAM & SCRAM` / `THE GHOST IN THE
  MACHINE` storefront motifs. The keep also fights with the tier-3 wild roster today — it has no
  malbeasts of its own, since a new `SPR_MALBEAST_*` grows `kWildMalbeastCount` and with it the
  'Pedia's seen/defeated masks.
- **Rarity-tiered, area-themed foods** — per-area food sets across Citrus Circuit / Pirate Bayou /
  Napstorrent with a rarity ramp, where the **best** food carries an effect that fits the area theme
  (Citrus Circuit might trade levels to revert a care mistake; Pirate Bayou the inverse). A
  meaningful new `ICON_ITEM_*` batch plus a mechanic. Diff **L** / Opus.
- **Best foods = a once-per-lifetime permanent buff.** Needs a per-pet "lifetime buff consumed" flag
  + a permanent stat modifier. Mechanic + design, pairs with the above. Diff **M** / Opus.
- **Per-line move-fx assets / silhouette + eye-anchor data** (FB-DSGN7, FB-UI4) — eye-pixel metadata
  per sprite + the layered gamma-pulse treatment. Largely procedural + data, not flat icons.
- **Branching-roster sprites** — full `SPR_PET_*` sheets for the named alternates once the roster
  naming session lands.

---

## 3. Size / reviewability watch

`combat.cpp`, `expl_screen.cpp`, `cfg_screen.cpp` and `save.cpp` are large but still
single-responsibility — revisit only if they keep growing. Same rule as the `game_*.cpp` units: split
*at* ~600 lines, not before, and split by concern rather than by line count.

---

## 4. If picking up cold

1. **FB-CFG4** (deep-sleep pause) — the last big Opus-class item.
2. **The NET-C area (§1d)** — well-trodden pattern, but settle the insert-vs-append question first.
