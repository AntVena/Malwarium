# Malwarium — the board

> **The one list of what's open.** Planning/design decisions, code TODOs, and art we'll need soon.
> Each item carries a **difficulty** (S/M/L/XL) so work can be dispatched without re-deriving
> context.
>
> **Anything completed and tested is DELETED from this list, not checked off.** This is a planning
> board, not a log — the code and `git log` are the only living history we need, and kept-around
> history just dilutes searches. Same for entries in flight: don't annotate what changed, describe
> what's left.
>
> Don't reference this file from a code comment. It's transient by design; the comment isn't.

---

## How to read the difficulty column

**Difficulty:** `S` ≈ an afternoon, localized · `M` ≈ a session, one subsystem · `L` ≈ multi-file,
needs a save bump or new data model · `XL` ≈ architectural / cross-cutting / needs a design pass first.

**When testing something here, ask first whether a test is warranted** — does another test already
cover it? Does it matter if it fails? If it is worth one, manufacture the starting state rather than
building it up organically.

---

## 1. Code

### 1a. Ready to build

| ID | Item | Where | Diff | Notes |
|---|---|---|---|---|
| FB-UI4 | **Evolution reveal animation** — flash the pet silhouette + the possible-evolution silhouettes repeatedly before landing. | Evolve overlay. Needs the branch-candidate set + silhouette rendering. | L | Depends on the eye/silhouette metadata below. Design-heavy. |
| NA | **Bench travel mode ON BATTERY.** Entering the sleep and waking on B+C are confirmed on a board — the device goes down, comes back, and the USB link drops and re-enumerates across the gap as a deep-sleep reset should. That run was USB-TETHERED, which leaves the one thing it could not test: **the power latch**. On USB, VBUS feeds VSYS directly, so `PIN_POWER_HOLD` is not what holds the rail up and `gpio_hold_en` over it was never exercised. If the hold does not survive on battery, travel mode is a power-off that only PWR undoes. Also open from the same run: that a SINGLE button re-sleeps without booting (the wake gate's software chord), that the pet returns with the hunger/happiness/evolve timer it went down with, and the current draw asleep — which is the whole point. | `platform/esp32/main.cpp`'s `travelDeepSleep` / `travelWakeGate`. | S | Needs a battery and a way to read µA — no code expected, this is a measurement. |
| FB-DSGN7 | **Iconic per-line move effects** (Ransomware: gamma pulse + scaled white/green silhouette layers, +1px layer per Evo-level of the move). | Move-fx system, per-line. | L | Needs a creature-line taxonomy + the effect system first. Cosmetic only. |
| NA | **Sub-area bosses that are themselves gauntlets** — `subAreaBoss` returns a length-1 `BossGauntlet`, so a sub-area boss is always exactly one fight; only the AREA boss is multi-round. Castle Rapidscare's THE EIGHT PWNS wants to be a minor gauntlet, and its JOKER VIRUS wants to *loop back* into another Pwns run after it falls. | `combat.cpp`'s `subAreaBoss`/`areaBoss`, plus the round plumbing in `game_explore.cpp` (`startBossRound`/`finishBossRound`). | L | The re-entrant loop is the novel part — the carried-Health round machinery is linear today, with no notion of a round that re-queues an earlier one. Needs a design pass on how a loop terminates and what it pays. |
| NA | **Confirm on device that a save lands while audit capture is armed.** The write path no longer asks for the blob in one piece: `Game` owns the serialize buffer and `captureSave` reserves every collection to its exact count, so an ordinary save is judged against `kSaveHeapFloorBytes` (12KB) rather than the reservation. Built and green on the native + S3 gates; the hardware run that proves `held` stays at 0 through a whole capture session has not been done. | `game_persist.cpp`'s `persistSave` and the `kSaveHeapFloorBytes` / `kSaveGrowHeapFloorBytes` pair. | S | `HEAP_TRACE_ENABLED` (config.h) → 1, then CFG → RADIO → AUDIT → SCAN+CAP, 'Pedia AP on and off again. Success = `held=0` with `floor=12288` and no boot banner. Read the log, not the panel — a reset is fast enough to look like nothing happened. |
| NA | **Capture arming costs ~70KB and the AP ~58KB**, against ~126KB free with the radio idle. The device works, and the save no longer needs a big contiguous block, but that was the only thing standing on this — anything else that grows will hit the same wall. Worth a pass at what the capture path actually needs. | `net_capture.h`'s `powerUp` (`esp_wifi_init` + promiscuous + the pcap SD buffers). | M | Measured on device, not estimated: `[ap] down free=126408` → `[cap] armed free=56188`. |

### 1a-ii. Evolution routing — one weighted edge list per creature

`CreatureDef` carries four optional successor pointers (`evolvesToId`, `evolvesToGoodId`,
`evolvesToBadId`, `evolvesToTrojanId`) and `kDaemonPools` carries a fifth route in a table beside
them — five mechanisms for one question. [`CONTENT_STANDARD.md`](../src/core/content/CONTENT_STANDARD.md)
rule 1 asks for the opposite shape: a typed list on the row, not optional fields bolted onto the
shared struct. The target is one array per creature —

```cpp
struct EvolutionEdge { const char* toId; EvoWhen when; uint8_t signal; uint8_t weight; };
```

— where `EvoWhen` is `Always | CareGood | CareBad | Signal | TrojanDivert`, and
`Game::evolutionTargetId` becomes "filter the row's edges to those whose condition holds, then draw
by weight". That subsumes the Daemon pool, the care branch, the linear hop and the Trojan divert
(today a separate `if` in `fireEvolution`), and makes a signal-dependent route expressible for the
first time — `Game::dominantSignal` computes the key and nothing consumes it.

**Do it when a chain actually needs a weight or a signal branch**, not before: every pool is
single-entry today, so building the general mechanism now means designing it with the least
information about what it has to carry. Consumer surface is small — `evolutionTargetId`, one
registry accessor, one `ContentSource` virtual, one test. No save concern; routing is not persisted.
Diff **M**.

### 1b. Cooking — the open follow-ups

The pantry, per-item drop weights and the N-ingredient Merge Hub are built. What the first cut left:

- **Drop weights are unmeasured.** The pantry's numbers are authored by flavour, not play data. The
  walk-pool thinning constant (`kStapleWalkWeight`) especially is a guess at how much staple a player
  should wade through to reach a diving bell. Diff **M** (needs measurement runs).
- **The pantry is line-agnostic.** Every staple drops everywhere. Once per-area food sets land (§2c)
  the two systems should meet — a `LootEntry` weight override per area pool is already the mechanism,
  so that's content, not code.

### 1c. Sprite storage — indexed colour

The 1-bit mask half is done: `gen_assets.py` detects an asset that carries nothing a bitmap
would lose (every alpha 0 or 255, every opaque pixel one colour) and emits a packed mask +
its `ink` instead of RGB565 + alpha. 169 of 195 assets qualified — the whole `ICON_*`/`UI_*`
family and more besides — for **202 KB of flash**. Every creature and malbeast sheet is
multi-colour and correctly stayed full storage.

What's left is those sheets, which are where the bytes actually are: up to 19 distinct
colours each. 8-bit indexing is safe (~3×); 4-bit needs a per-sprite colour reduction and
real art review. Not urgent — flash sits at 24% and tinting doesn't depend on it. Diff **L**.

### 1c-ii. Tinting — a second theme

**A second theme is a design pass, not a build.** The machinery takes N themes today and
`PAL_CORE.json` documents the block shape; authoring a colourblind-friendly set (moving the
red/green semantic pair onto a blue/orange axis) needs hue decisions, plus a CFG row to select it
and a save field to remember it. Diff **M**.

### 1e. FONT_UI — ingested and rendering; one copy decision left

The face is in (`assets/fonts/PixelOperatorMono8.ttf`, Pixel Operator by Jayvee Enaguas, CC0,
licence beside it) and the whole code side is built on the **`font-ui` branch**: `tools/gen_font.py`
rasterises it into a committed glyph table, `font.cpp` renders from that, and `font5x7.*` is gone.
The generator is deliberately not in the gates — rasterising a TTF needs Pillow, and
`gen_assets.py`'s pure-stdlib rule is what lets a fresh clone regenerate the atlas — so the
committed table is what builds. It refuses any size the face would be antialiased at, which is
the one defect nobody can spot reading a hex table.

**What it costs, measured rather than estimated.** The face is crisp only at the size it was drawn
for; 12px comes out with 32 grey levels, which VISUAL_LANGUAGE §2.1 rules out. At its own 8px cut
the advance is 8px against the placeholder's 6 — and 8 is not padding: `M`, `W`, `%` and `#` really
do span 7 columns, so tightening it would run adjacent letters together. Every string is therefore
**a third wider**, and the suite's own copy-fit checks catch it: **51 failing checks**, all of them
"does this name still fit its column".

**The fork, and it is a copy decision, not a code one:**

- Leave the UI's row tags as they are → **37 world strings overflow**, so the areas, sub-areas,
  bosses and storefronts get renamed. Naming is a design pass (`AREA_NAMING.md`), not a refactor.
- Shorten the EXPL row's own chrome — `> FIGHT BOSS` → `BOSS`, `CLEARED` → `DONE` → **5 overflow**,
  all on the detail line, and all 5 clear it if the `TITLE: ` / `BOSS: ` prefixes go too. Zero
  content renames. But it is a shipped screen's copy, and a shipped screen is its own contract:
  the prefix is what says whether a detail line is naming a Title or a boss.

Nothing else is blocking. Pick the lever and the branch closes.

### 1f. Standing stubs / interim mechanics to revisit

Intentional simplifications. None is a bug; each is a "confirm as v1 or revise".

- **"LINK" names two different things.** The CFG **RADIO → LINK** row is consent to BROADCAST
  identity over ESP-NOW; the Hacker face's **LINK** slot is the 1v1 duel surface that consent
  enables. Adjacent screens, same word, different referents. Naming call, not code: renaming either
  ripples through the docs and the peer/duel screens' copy. Diff **S** (taste, then a
  mechanical rename).

### 1g. Test-infrastructure gaps

- **No serial test-hook / no automated on-device gameplay verification.** Every device check to date
  is "flash, read the boot line, confirm no crash loop" — nobody has walked the buttons through
  EXPL/combat/Wi-Fi/rank-up on the real panel in a long time. Diff **M** (harness design). A
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
  one flag is what was deliberately removed. Diff **S** (design, not storage).
- **❓ Multi-frame sprites in the 'Pedia — NOT REPRODUCED, needs a repro.** Every one of the 26
  creature/malbeast sheets was rendered through `sprStyle`'s own formula and each cropped frame 0
  correctly, at every geometry the roster has (1/2/4/8-frame strips, the 2-row KeyloggerHead sheet,
  and the oversized 96×64 and 64×56 single cells) — so the "only Malbear's dimensions work" reading
  doesn't hold against the current data. Two candidates for what was actually seen, both real:
  frame 0 of a multi-purpose sheet is not always the idle pose (`phrogspawn` shows a hatch swirl,
  `cachemutt` a crying frame), and the 'Pedia is static where the device animates. If that's the
  complaint it wants the anim-clip table exported to the web and is **M**, not a bug fix.
  **Needs the PO to say which screen and which creature looked wrong.**
- **`WORM_WHISPERER` has no firing site**, and what's missing is not the unlock CALL: the Worm
  line has no content rows at all, so there is nothing to hatch. It needs a creature line —
  sprites, an evolution chain, an egg line — before it needs a line of code. That also unblocks
  `ICON_LINE_WORM`, which is drawn and has nothing to label (§2b). Diff **L**, content-led.
  (`AIR_GAPPED` is done: a defrag that fails on an already-Critical disk raises a Replication
  Ghost, and the Air-Gapped Snack cuts it loose.)
- **No on-device browser.** The home-screen banner is the whole feedback channel. If achievements
  ever want a device-side list, the Hacker face's PROFILE slot is the natural home. Diff **M**.
- **Unverified:** on-device serving of the SD-hosted bundle + the live endpoints
  (`GET /pedia_state.json`, `POST /api/tag`) on a real board.
- **Polish:** the real `FONT_UI` face for the web bundle (a system-mono fallback today).

### 1i. Hacker-face CREW — enlistment shipped, Red/Blue archetype layer open

- **The Red/Blue archetype + PvP contest layer.** Archetypes (Operators: Botmaster/Insider
  Threat/Ghost vs. Guardians: Orchestrator/Watchdog/Dispatcher), the capture broadcast-window
  lifecycle, and target-cooldown + Honeypot/IDS defensive mods have no code. Diff **XL** —
  needs a first design pass reconciling the ability list against everything that's shipped since.
- **More crews + Red-side crews.** Adding one is a `kCrews[]` row; a new ability shape is one
  `CrewExploitKind` entry plus one case in `Combat::applyCrewExploit`. What's missing is the *earn*
  model — today every crew is joinable the moment a home network exists. Diff **M**.
- **What "defender of that network" buys you.** The home network is recorded and shown but has no
  mechanical consequence beyond gating enlistment. Diff **M**.

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

PSRAM is ON (`platformio.ini`'s `board_build.arduino.memory_type = qio_opi`), which is what makes
the manifest fetch reliable at all: mbedTLS's TLS session buffers are a large-enough allocation
(the Arduino core's `CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL` sends anything over 4KB to PSRAM) that
internal SRAM alone — split by the ~98KB framebuffer plus the Wi-Fi stack's own reserve — could
plateau its largest contiguous free block around 16KB and fail `WiFiClientSecure::connect` with
`start_ssl_client: -32512` regardless of total free heap. A device flashed before this landed is
still on the old bootloader and needs the USB/browser-flasher path once, same as any bootloader
change — an app-only OTA can't carry it.

**Open:**

- **Bench the failure paths.** Every one is handled in code and none has been exercised on a board;
  the laptop-publish flow makes each a one-line edit to `dist/`. Truncate an artifact (expect
  `Truncated`, slot never made bootable) · flip a byte after publishing (expect `Corrupt` at the
  digest check) · stop the server mid-download (expect `Truncated` via the stall timeout) · pull the
  card during a web install (expect `WriteFailed`, and the version marker still reading the OLD
  version, since it's written last). **And the rollback gate itself**, which is the one with no
  native stand-in: publish a build that panics in `setup()`, confirm the device installs it, boots
  it once, and comes back on the previous firmware. Diff **M** (needs a board + a publish
  host). Fold in the **re-provisioning self-present** while a board is on the bench: a device whose
  last CONNECT failed should pop the captive portal onto `/setup` for a phone joining its AP. It
  compiles and the logic is one condition in `handleProbe`, but no phone has met it.

- **Bench the browser flasher, ERASE path.** The normal (ERASE off) run is now proven on a board:
  holding **A** (GPIO0, the download strap) while connecting enumerated as `USB JTAG/serial debug
  unit` (Espressif VID `0x303a`) for Chrome's picker, `default_reset` synced on the S3's native USB,
  all four images wrote, and the board came back up with its save intact and passing its own update
  check. Still open: the same run with ERASE on — a full-chip wipe hasn't been exercised. One
  platform note from the run: the flasher needs a real Chrome/Edge window with actual Web Serial
  support; an embedded/automation-driven browser pane can present `navigator.serial` without
  implementing the OS device-picker UI behind it, in which case `requestPort()` just hangs with no
  error to catch. Diff **S** (needs a board). The device-side half — CFG →
  UPDATES → FLASH OVER USB drawing the code — is native-gated and rendered, not yet scanned.
- **Hosting is settled and live** — GitHub Pages, deployed by `.github/workflows/publish.yml` on a
  `v*` tag; see ORIENTATION's *Releasing*. `https://antvena.github.io/Malwarium/manifest.json` has
  served a real publish, verified by fetching it back and re-parsing the served bytes with the
  device's own parser. The same deploy now carries `pages/` and the three boot images, so the host
  answers both halves. `UPDATE_MANIFEST_URL` is compiled IN under `BOARD_WAVESHARE_S3_154`
  (config.h) and names that Pages address — confirmed by finding the string in a published
  `mal-*.bin`. That is what makes "flash from the browser, then let the device fetch its own
  'Pedia onto a blank card" a complete path for someone with no toolchain: nothing has to be
  typed into the device but a Wi-Fi password. A stored override from the setup page still wins
  when set, and the empty default applies only to a board block naming no address of its own.
  Nothing open here.
- **Credentials sit in NVS in PLAINTEXT** (stock ESP32 NVS is unencrypted). Stated plainly rather
  than implied-protected; NVS encryption is a separate feature and is not enabled. Revisit only if
  the threat model changes.

---

## 2. Art

Engine slots for these exist (they render via placeholder or text today), so most are **drop-in the
moment they're drawn**.
Sizes are logical px; bind colour to `PAL_CORE` tokens. Inventory: `assets/ASSET_MANIFEST.md`.

### 2a. Ready-now, zero-friction

| Asset | For | Size | Diff to integrate |
|---|---|---|---|
| `UI_HINT_BAND` | contextual control-hint band | 224×24 | S |
| `UI_PROGRESS_BAR` | reusable fill bar (EXPL/MAINT/hold-to-commit) | ~180×12 | S |
| `UI_SLOTS_USED` | ARCH rack-slot usage indicator (`slots 2/4`) | ~44×12 | S |
| `UI_COUNTDOWN` | Lockout countdown digit style | ~64×24 | S — folds into FONT_UI work |
| `ICON_EXPLORE_STATE` | optional sub-area row state marker | 16×16 | S |
| `ICON_CFG_RADIO` | RADIO group row — reuses `ICON_SYS_WIFI` today | 20×20 | S |
| `ICON_CFG_UPDATE` | UPDATES row — reuses `ICON_CFG_SYSINFO` today | 20×20 | S |
| `ICON_CFG_TRAVEL` | DEVICE group's TRAVEL MODE row — reuses `ICON_CFG_SYSINFO` today | 20×20 | S |
| `ICON_PEERS` | Hacker-face PEERS slot — renders text-only today | 28×28 | S |

### 2a-ii. Template pet sheet — one row per default animation

No starting point exists for a new creature sheet today. `gen_assets.py` already slices a pet sheet
into independent rows of up to 8 56×48 columns each (`frame_rows`/`PET_ROW_H`), and a creature's
`CreatureDef::clips` already name an arbitrary row + frame count on its own row — so this is purely
an authoring aid, no ingestion change needed. A template just needs one labeled row per default
animation (idle, attack, plus the locomotion cycle §2c wants), each row 8 cells wide with
placeholder ink or guide marks, so an artist opens one file and knows where "walk" goes without
reading a content header. Lives beside `CREATURE_VISUAL_RULES.md` (which currently stops at "keep
them as single frames... start frame sets" with no sheet layout to start from), and the clip rules
it documents are in `src/core/content/creatures/CREATURE_CONTENT_STANDARD.md` § *Clips*.

MALBEAR is the case that shows why it's wanted: its 8-column sheet declares `idle` as frames 0-2
and `attack` as all 8 of the same row, because the sheet arrived with no row plan saying which
columns were which. A template makes that a drawing instruction rather than a guess. Diff **S**.

### 2b. Placeholder → final art

These read fine by name + pips today; final art is polish, dropped in where it lands. Roughly
high→low value:

- **Three move glyphs** still placeholder: `ICON_MOVE_BUFFER_OVERFLOW`, `_ROOTKIT_STRIKE`,
  `_NULL_ROUTE` (TRAIN falls back to text without them).
- **`ICON_SECTOR_CITRUS_CIRCUIT` is a generic map pin** where its four siblings are motif
  glyphs (skull, sail, download arrow, keep) — the family reads as four zones plus a marker.
  A redraw on the area's own LimeWire-era motif is pure legibility polish; it ships as is.
- **`ICON_LINE_WORM` is drawn and has nothing to label** — the Worm line has no content rows at
  all, so it waits on a creature line, not on wiring. Held in `check_orphan_assets.py`'s KEEP list
  meanwhile.
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

These come with the features above rather than ahead of them. Every "backdrop" below means
`BG_SECTOR_<AREA_ID>` — the area's own id, upper-cased, which is the name its row already asks
for. The `ICON_SECTOR_*` half of each family is drawn and live on the EXPL zone picker.

- **Net-Sea Crossing area art** (shipped mechanically, art pending): the backdrop
  (open water, shipping lanes, landfall at Sandbox Beach) and the `FLOATING POINT` / `THE HARDENED
  SHELL` storefront motifs. Its five mods are drawn — the whole `ICON_MOD_*` family is, so no area
  owes one. Like the keep, it fights with the shared tier roster and has no malbeasts of its own.
- **Napstorrent Moors area art** (shipped mechanically, art pending): the backdrop
  (marshy → castle progression), the `MOOR-TO-MOOR` storefront motif.
- **Castle Rapidscare art** (shipped mechanically, art pending): the backdrop,
  castle-themed malbeasts + a `COUNT COPYLEFT` apex, and the `SPAM & SCRAM` / `THE GHOST IN THE
  MACHINE` storefront motifs. The keep also fights with the tier-3 wild roster today — it has no
  malbeasts of its own, since a new `SPR_MALBEAST_*` grows `kWildMalbeastCount` and with it the
  'Pedia's seen/defeated masks.
- **Rarity-tiered, area-themed foods** — per-area food sets across Citrus Circuit / Pirate Bayou /
  Napstorrent with a rarity ramp, where the **best** food carries an effect that fits the area theme
  (Citrus Circuit might trade levels to revert a care mistake; Pirate Bayou the inverse). A
  meaningful new `ICON_ITEM_*` batch plus a mechanic. Diff **L**.
- **Best foods = a once-per-lifetime permanent buff.** Needs a per-pet "lifetime buff consumed" flag
  + a permanent stat modifier. Mechanic + design, pairs with the above. Diff **M**.
- **Locomotion poses to match the resting motion.** Every creature declares how it gets around
  (`CreatureDef::locomotion`) and the habitat already moves it that way — a walker ambles along the
  shelf, a swimmer drifts through the box, a flier holds an altitude (`core/model/idle_wander.h`).
  The POSE is still the breathe/blink idle for all three, so a drifting tadpole is a standing
  tadpole that slides. Wants an extra clip row per mover on the existing sheets (swim cycle,
  wingbeat, step cycle), keyed off the same field — sheet rows, not new sprites. `Fly` has no
  creature on it yet either; the first flier is a roster question, not a code one. Diff **M**.
- **Per-line move-fx assets / silhouette + eye-anchor data** (FB-DSGN7, FB-UI4) — eye-pixel metadata
  per sprite + the layered gamma-pulse treatment. Largely procedural + data, not flat icons.
- **Branching-roster sprites** — full `SPR_PET_*` sheets for the named alternates once the roster
  naming session lands.

---

## 3. Size / reviewability watch

Same rule as the `game_*.cpp` units: split *at* ~600 lines, not before, and split by concern
rather than by line count. `expl_screen.cpp` (754), `cfg_screen.cpp` (669) and `save.cpp` (936)
are each past the number and each still ONE concern — save.cpp is long because the format is
flat, which is not a second responsibility.

One has grown to roughly double the rule and has a real seam, so the "if they keep growing"
condition has fired. It is not a mechanical move, which is why it is a row rather than done:

- **`combat.cpp` (1212)** — the turn engine is lines 21–788; from `makePlayerCombatant`
  (line 790) on, ~420 lines are combatant/encounter FACTORIES: the wild-malbeast roster and its
  ramps, the sim dummy, the Bits/XP reward curves, `makeEnemyCombatant`. That tail is a
  `combat_factory.cpp`. Diff **M** — the two halves share statics that would have to be sorted
  out first.

Two more are past the rule and were not on this watch at all:

- **`game.h` (2681)** — the umbrella header. It is not a unit that grew a second concern; it is
  one class's declaration, so the line count is arguably honest. The cost is its **36 includes**:
  it hands every TU that includes it 9 UI screen headers plus the whole model/net/render stack.
  Two consequences. The `game_*.cpp` units re-include ~5 headers each (`carousel.h`,
  `items_screen.h`, `maint_screen.h`, `modals.h`, `train_screen.h`) that `game.h` already
  provides — ~60 redundant lines, free to delete, but only cosmetic. The real one is that **the
  unused-include sweep cannot be run while this stands**: strip any include from any `src/core`
  TU and it still compiles, because `game.h` supplied the symbol transitively. A mechanical
  strip-and-build sweep says 295 of them are removable, which is a measurement of `game.h`, not
  of the includes. Diff **L** — the question is whether `Game` can declare against forward
  declarations and push the screen headers down into the `.cpp` units that draw. Until then the
  maintenance pile's *Unused-include sweep* has no signal to work with.
- **`test_main.cpp` (14746, 466 cases)** — the largest file in the repo by an order of magnitude,
  and healthy by every check that exists: every defined `test_*` is registered, nothing is
  skipped or disabled, and the suite is green. But one file holding the whole native tier means
  a reviewer cannot find the tests for a subsystem except by grep, and the `RUN()` list at the
  bottom is a 467-line macro nobody can diff usefully. Splitting by subsystem
  (`test_combat.cpp`, `test_save.cpp`, …) is mechanical, since registration is already a macro
  list. Diff **M** — the volume, not the difficulty.

---

## If picking up cold

1. **FONT_UI (§1e)** — built and waiting on the `font-ui` branch; what is left is one decision
   about EXPL's row copy, not code.
2. **Net-Sea Crossing art (§2c)** — the area ships mechanically; it is the only rung with no
   backdrop or malbeasts of its own.
