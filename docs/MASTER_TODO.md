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

**Sub-area bosses that are themselves gauntlets** — `subAreaBoss` returns a length-1 `BossGauntlet`, so a sub-area boss is always exactly one fight; only the AREA boss is multi-round. Castle Rapidscare's THE EIGHT PWNS wants to be a minor gauntlet, and its JOKER VIRUS wants to *loop back* into another Pwns run after it falls. | `combat.cpp`'s `subAreaBoss`/`areaBoss`, plus the round plumbing in `game_explore.cpp` (`startBossRound`/`finishBossRound`). | L | The re-entrant loop is the novel part — the carried-Health round machinery is linear today, with no notion of a round that re-queues an earlier one. Needs a design pass on how a loop terminates and what it pays. |

**The wild-win item drop pool is a literal array in combat logic.** `wildLootPool` is four hardcoded ids, with a fifth spliced in by `if (exploreSector_ == 0)` / `== kDeepWebSector` — a per-area loot table living in the engine, where every other pool is a `LootEntry` list beside the rows it draws from ([`CONTENT_STANDARD.md`](../src/core/content/CONTENT_STANDARD.md) rule 1). Wants to be an area-authored pool on `AreaDef`, drawn by the shared `rollLootEntry`. | `game_combat.cpp`'s wild-win drop roll; `content/areas/<area>/area.cpp` for the new pool field. | M | The area-splice branch is the tell: adding a third area-exclusive ingredient today means a third `else if` in combat. |

**Two content-unlock gates are id branches.** `Game::eggLineUnlocked` and `Game::hatchProcessUnlocked` each `strcmp` against `"phishing"` / `"phishlet"` and answer with a `hasAchievement` check, so "this row is earned, not given" is engine knowledge rather than something the row states ([`CONTENT_STANDARD.md`](../src/core/content/CONTENT_STANDARD.md) rule 1). Wants an achievement-id field on `EggLineDef` and `CreatureDef`, with each gate reading whatever its row names. | `game_pedia.cpp`; `defs.h`. | S | `EggLineDef` is a 2-row table and trivial. `CreatureDef`'s aggregate initialisers run positionally through `clips`, so its new member has to go after that — awkward reading order for a gate field, which is the only real cost. |

**Capture arming costs ~70KB and the AP ~58KB**, against ~126KB free with the radio idle. The device works, and the save no longer needs a big contiguous block, but that was the only thing standing on this — anything else that grows will hit the same wall. Worth a pass at what the capture path actually needs. | `net_capture.h`'s `powerUp` (`esp_wifi_init` + promiscuous + the pcap SD buffers). | M | Measured on device, not estimated: `[ap] down free=126408` → `[cap] armed free=56188`. |

**The arcade records plays and wins, never a best score.** `arcadePlays_`/`arcadeWins_` (save v47)
are what the GAMES list and the cabinet's `W-L` line read, so a player has no way to see the run
they are trying to beat — and every cabinet already reports a score to `finishArcadeRun`, which
simply drops it after paying on it. Wants a third parallel tally beside the two, surfaced on the
cabinet page. | `game_arcade.cpp`'s `finishArcadeRun`; `arcade_screen.cpp`; the v47 tail in
`save.cpp`. | S | The only judgement call is whether a WinLose cabinet (the Clutch) shows anything
at all, since its score is always 0 or absent. |

**The DECRYPTOGRAM's prize is a LADDER, not a per-quote answer.** A first solve pays the next
MERGE HUB recipe the operator can't cook (`quotePrizeLadder`), then falls back to `+1 bandwidth`
once the kitchen is complete — so the prize varies by how far in you are, but still not by WHICH
quote came up, and `kQuoteWinBits` is one number for the whole pool. A marquee quote worth a rack
slot, or a throwaway worth half the Bits, still wants an optional reward field on `QuoteDef`
resolved off the row when present. | `content_quotes.h`'s ladder + fallback;
`game_cryptogram.cpp`'s `quoteFirstSolvePrize`. | S | Do it when a row actually wants a different
prize — until then a per-row field is the same literal pasted three hundred times. |

**A crew cannot be DISCOVERED.** `QuoteReward::Kind` has room for it and it is one of the prizes
the board was designed to hand over ("you find a crew to join"), but crews are ungated today —
every row in `content_crews.cpp` is enlistable from the first boot, so there is nothing for a
prize to unlock. Wants a discovery axis on `CrewDef` first, then one `Kind` and one applier case. |
`content_crews.h`; `game_crew.cpp`'s roster filter; `QuoteReward::Kind`. | M | The gating axis is
the real work; the prize is three lines once it exists. |

### 1a-ii. Evolution routing — one weighted edge list per creature

`CreatureDef` carries five optional successor pointers (`evolvesToId`, `evolvesToGoodId`,
`evolvesToBadId`, `evolvesToTrojanId`, `evolvesToTrojanBadId`) and `kDaemonPools` carries a sixth
route in a table beside them — six mechanisms for one question, and the fifth was added purely so
one divert could land on a care branch, which an edge list would have expressed for free. [`CONTENT_STANDARD.md`](../src/core/content/CONTENT_STANDARD.md)
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

The pantry, per-item drop weights, the N-ingredient Merge Hub and its eleven recipes are built, and
a recipe is won off a Decryptogram rather than bought. What the first cut left:

- **Drop weights are unmeasured.** The pantry's numbers are authored by flavour, not play data. The
  walk-pool thinning constant (`kStapleWalkWeight`) especially is a guess at how much staple a player
  should wade through to reach a diving bell. Diff **M** (needs measurement runs).
- **The pantry is line-agnostic.** Every staple drops everywhere. Once per-area food sets land (§2c)
  the two systems should meet — a `LootEntry` weight override per area pool is already the mechanism,
  so that's content, not code.

### 1c. Sprite storage — indexed colour - Break-Glass-Flash-Memory-Saving Lever

The 1-bit mask half is done: `gen_assets.py` detects an asset that carries nothing a bitmap
would lose (every alpha 0 or 255, every opaque pixel one colour) and emits a packed mask +
its `ink` instead of RGB565 + alpha. 169 of 195 assets qualified — the whole `ICON_*`/`UI_*`
family and more besides — for **202 KB of flash**. Every creature and malbeast sheet is
multi-colour and correctly stayed full storage.

What's left is those sheets, which are where the bytes actually are — and they are far cheaper
to index than "8-bit, ~3×" suggests, because two facts compound. **The palettes are already
tiny:** measured across the 42 shipped sheets, 40 need **4 bits or fewer** and 20 need 3 or
fewer, so the per-sprite colour reduction 4-bit was thought to require is a no-op for all but
two. **And the alpha plane disappears rather than shrinking:** 39 of 42 sheets are binary
alpha, so transparent is just an entry in the palette, and the separate `a` byte — a third of
current storage — stops being stored at all.

Together that is **6.6×, not 3×**: every sheet on the device goes 1,750,296 B → 265,024 B
(262,916 B of packed indices + 2,108 B of palettes). Per-sheet palettes are what to build; a
single shared table per LINE was measured and is worse, because a family's union is bigger than
any member (phishing 26 colours against a 12-colour worst sheet, so 5-bit; ransomware 260) while
the overhead it saves is the 2 KB above. A shared table only becomes attractive if the art is
first snapped to a canonical per-line palette — phishing's 26 is drift, not intent — and its
real prize would be enforcing the mother-colour rule mechanically rather than saving flash.

Two sheets fall back to 8-bit (`SPR_PET_KALICO` 133 colours, `SPR_PET_BRUINFORCE` 71) and three
carry partial alpha (`_BRUINFORCE`, `_KEYLOGGERHEAD`, `_TADPOLL`), so those either keep an alpha
plane or get thresholded. Both Kalico and Bruinforce are wanted for a hand pass anyway
(`ASSET_MANIFEST.md` §C.1).

Diff **M**, not L: `SpriteData` already carries TWO storage forms behind a discriminator, the
generator already DETECTS the 1-bit case rather than keying off names, and `spriteColorAt` /
`spriteAlphaAt` are the only code in the tree that touches `rgb`/`a` at all. A third form is
those two accessors plus an emit path. The draw-time cost — one palette indirection per pixel,
against today's direct read — is the part that is not yet measured.

**What decides when this gets pulled is the ANIMATION standard, not the roster size.** Measured
off the ELF: the image is 33% of the 0x790000 app slot, assets are 47% of the image, and pet
sheets are 92% of the assets — every other family together (icons, UI, backdrops, malbeasts)
is under 8%, because the 1-bit pass above already took them. Storage is 3 B per logical px, so
one 8-frame `448×48` row costs 64,512 B and a creature's cost is decided entirely by how many
rows it keeps. Taking all 28 creatures to Malbear's one-row shape lands the image at 41% and
needs nothing here. Taking them to `SPR_PET_KALICO`'s four rows needs 7.2 MB of pet art alone
and **overflows the slot by 762 KB** — and an OTA has to fit the same-sized second slot, so
that ceiling is hard and the partition table cannot move it (`partitions_malwarium.csv`).
Indexed storage is what buys the four-row standard: it puts that same roster at 49%. So the
order is *decide the per-creature row budget first*; this row is only urgent if the answer is
"more than one".

### 1c-ii. Tinting — a second theme

**A second theme is a design pass, not a build.** The machinery takes N themes today and
`PAL_CORE.json` documents the block shape; authoring a colourblind-friendly set (moving the
red/green semantic pair onto a blue/orange axis) needs hue decisions, plus a CFG row to select it
and a save field to remember it. Diff **M**. The `decryption` block is the part that most wants one:
its five code colours are a vocabulary a player has to tell apart at a glance, and while every cell
carries its initial as the grayscale channel, five hues that read as five is the whole board.

### 1f. Standing stubs / interim mechanics to revisit

Intentional simplifications. None is a bug; each is a "confirm as v1 or revise".

- **Only the header-band title claims Bold.** `FontFace::Bold` exists and costs a layout
  nothing (VISUAL_LANGUAGE §2.3), so the question is now where else emphasis is earned rather
  than whether it is available. Candidates: an item/mod/move detail page's NAME, and the
  selected row in a list. The rule that keeps it worth having is that one thing per screen
  outranks the rest — confirm as v1 or extend it deliberately, one surface at a time. Diff
  **S** per surface, taste before code.
- **The derived bold is a smear, not a drawn cut.** 7 cells (`% @ M W _ m w`) already span the
  box and thicken into their own counters. It reads as bold rather than damage on every title
  that ships, so this is polish, not a defect. Pixel Operator's family carries its own Bold
  under the same CC0 — sourcing that cut and pointing `gen_font.py` at it would fix all 7 and
  changes nothing above it, PROVIDED the bold's advance is still 8. If it isn't, deriving stays
  correct and this row closes unbuilt. Diff **S**, sourcing before code.
- **An empty MOVES slot marks its fallback move with shape, not a word.** The empty-slot glyph
  (distinct from the locked one beside it) plus a dimmed name are the two non-colour channels
  carrying it, which clears the grayscale gate — but the row has no width left for the word
  "DEFAULT" beside the accepted-kind tag. Confirm as v1 or find it room.
- **The STAT LOADOUT page shows four entries.** A move description wraps to three lines at
  `FONT_UI`'s advance, and the row pitch is sized to hold all three rather than cut the last
  mid-word. Revisit if the page starts reading as a scroll rather than a list.
  The separation pass adds a second complaint to the same row: because the pitch is reserved
  rather than measured, a two-line description spends its slack AFTER itself, so the gap lands
  inside the MOVES group while the group's own seams — `MOVES` to its first entry, one entry to
  the next — get nothing. The dim `MOVES`/`MODS` labels are carrying the whole grouping alone.
  Measuring the pitch per entry and spending a fixed gap at the seams instead is the fix, and it
  is a real layout change rather than a constant. Diff **S**.
- **`items_screen` keeps its own copy of the list-window offset.** `listScrollTop`
  (`core/ui/layout.h`) is the shared one, unit-tested and drawn through by ARCH and CFG; the
  ITEMS list still inlines the same arithmetic at its two sites, so a fix to one window rule
  reaches three of the four lists. Mechanical. Diff **S**.
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
  `tools/screens.sh` covers the LOOKING half (one contact sheet of every screen, no baselines),
  which is what catches two things drawn over each other — the one failure a string-width
  budget cannot express.

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
- **The Worm line has no `FULL_LINE_WORM` row**, where the other three lines each have one. It
  costs a new `wire` bit and an `ICON_ACH_FULL_LINE_WORM` glyph. **Unblocked** — it used to want
  the line's two placeholder Script successors designed first, because a "seen every row" badge
  over rows called *Worm Placeholder I* and *II* is a badge for reading a stub; every row of the
  line is now named and drawn (Vermicell · Nodeatode · Rootgrub · Shenloop · Threadbore). Diff
  **S**. The one judgement call left is whether Rootgrub's Trojan divert counts against it: a
  raise that diverts never reaches Shenloop or Threadbore, so a player who keeps diverting can be
  a row short through no fault of their own, and the badge should almost certainly ignore the
  Trojan rows the way `LineRaised` already reads one family's own count. (The line itself is
  raiseable now: the Vermicell egg hatches it through the Isolation Protocol, `WORM_WHISPERER`
  fires on a clean run, and `SECOND_INSTANCE` — two of one species in the ARCH rack — is what puts
  the line on the menu.)
- **The Worm line's balance is unmeasured.** Every number on it — `kWormReplicaSlots`, the three
  targeting weights, the per-move spawn chances and the two magnitudes each replica reads — is a
  first cut chosen for internal consistency, not a calibration pass against a real fight. The
  fastest way to find the cliff: a full board is a **hard** floor of 1-in-13 that any given hit
  reaches the parent at all, on a line that also cannot be out-actioned, so if the passive is
  broken it will be broken there. Diff **S**, one balance sitting.
- **The Isolation Protocol's difficulty is unmeasured.** `kIsolationStepMs`, `kIsolationGrowth`
  and the 16×11 buffer are a first cut set to one identity — 30 bytes eats the whole incubation
  clock and leaves the worm across a third of the board — never played against a real thumb on a
  real panel. The question to answer in one sitting: is a clean run (and with it
  `WORM_WHISPERER`) *hard*, or is it *unreachable* at 220ms a step on the S3's buttons? Diff **S**,
  one balance sitting on device.
- **No on-device browser.** The home-screen banner is the whole feedback channel. If achievements
  ever want a device-side list, the Hacker face's PROFILE slot is the natural home. Diff **M**.
- **Unverified:** on-device serving of the SD-hosted bundle + the live endpoints
  (`GET /pedia_state.json`, `POST /api/tag`) on a real board.

### 1i. Hacker-face CREW — enlistment shipped, Red/Blue archetype layer open

- **The Red/Blue archetype + PvP contest layer.** Archetypes (Operators: Botmaster/Insider
  Threat/Ghost vs. Guardians: Orchestrator/Watchdog/Dispatcher), the capture broadcast-window
  lifecycle, and target-cooldown + Honeypot/IDS defensive mods have no code. Diff **XL** —
  needs a first design pass reconciling the ability list against everything that's shipped since.
- **The crew roster is five, both sides, five ability shapes.** Shipped: Deniers of Service
  (negate), Shell Smashers (bank damage as Power), Injection Protection (reset + floor the stat
  leans), Syntax Errorist (copy the enemy's self-buffs), The Last Responders (turn-metered death
  save that rallies on the overkill). Adding another is still a `kCrews[]` row; a new ability shape
  is one `CrewExploitKind` entry plus one case in `Combat::applyCrewExploit`. What's missing is the
  *earn* model — every crew is joinable the moment a home network exists. Diff **M**.
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

### 2a-i. The sprite-packing tools live outside the repo

`sheetpack.py` (cell packing, the crop-vs-decimate choice and its damage report, the 1px floor
gap) and `quantize.py` (palette snap + binary alpha) are what every generated sprite passes
through, and both sit untracked in a downloads folder. Nothing reproduces a shipped sheet
without them, and the rules they enforce — `ASSET_MANIFEST.md` §C.1's framing lever, cell
seating and decimation trade-off — are written down in prose but held in code nowhere the repo
can see. Promote both into `tools/`, the way `gen_worm_art.py` already holds the Worm line's
drawing vocabulary. Diff **S** (move + a header each).

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
  `_NULL_ROUTE` (MOVES falls back to text without them).
- **`ICON_SECTOR_CITRUS_CIRCUIT` is a generic map pin** where its four siblings are motif
  glyphs (skull, sail, download arrow, keep) — the family reads as four zones plus a marker.
  A redraw on the area's own LimeWire-era motif is pure legibility polish; it ships as is.
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
rather than by line count. `save.cpp` (963), `combat.cpp` (823), `expl_screen.cpp` (757) and
`cfg_screen.cpp` (668) are each past the number and each still ONE concern — save.cpp is long
because the format is flat, which is not a second responsibility, and combat.cpp is the turn
engine alone now that the factories have their own unit beside it.

One more is past the rule and was not on this watch at all:

- **`game.h` (2730)** — the umbrella header. It is not a unit that grew a second concern; it is
  one class's declaration, so the line count is arguably honest. The cost is its includes, and
  **the UI half is now off**: the nine `core/ui/*_screen.h` headers are gone, replaced by
  `core/ui/ui_state.h` — the ids `Game` actually holds as members (`SubmenuId`, `CfgScreen`,
  `ItemFilter`, `UiMode`, `MaintKind`, `ArchAction`, `HackerSlotId`, `FeedVitals`), split out on
  the rule that an id naming where the player IS is engine state while a `draw*` signature is
  not. A screen header now lives in the `game_*.cpp` unit that calls it. 36 → 28 direct
  includes, 51 → 43 transitive.
  **What's left is the model/net/render stack**: `combat.h`, `save.h`, `registry.h`,
  `framebuffer.h`, `platform.h` and the rest still reach every TU that includes `game.h`, so the
  unused-include sweep has no signal outside `core/ui` — where it has now run, leaving 38
  load-bearing includes of the 96 the units carried. The same question applies to each: can
  `Game` declare against a forward
  declaration, or does it hold the type by value? Diff **L**, and unlike the UI slice these are
  held by value almost everywhere, so the answer is likely "no" for most and the honest outcome
  may be that `game.h` is as thin as it gets.

---

## If picking up cold

1. **Net-Sea Crossing art (§2c)** — the area ships mechanically; it is the only rung with no
   backdrop or malbeasts of its own.
2. **The template pet sheet (§2a-ii)** — an authoring aid with no ingestion change behind it:
   one labeled row per default animation, so a new creature sheet starts from a plan.
