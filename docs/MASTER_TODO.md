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

**Capture arming costs ~70KB and the AP ~58KB**, against ~126KB free with the radio idle. The device works, and the save no longer needs a big contiguous block, but that was the only thing standing on this — anything else that grows will hit the same wall. Worth a pass at what the capture path actually needs. | `net_capture.h`'s `powerUp` (`esp_wifi_init` + promiscuous + the pcap SD buffers). | M | Measured on device, not estimated: `[ap] down free=126408` → `[cap] armed free=56188`. |

**No enemy's KIT carries a DEFEND move**, so — drops being drawn from what the enemy knows — the
two generic braces, `null_route` and `checksum_guard`, are unobtainable for a line pet, which is
every pet.
A line pet still has its own line's braces, so this is dead content rather than a hole in the
kit, but it is the sharpest single symptom of the row below. | `combat_factory.cpp`'s
`subAreaBoss` (or `wildMalbeast` / `kLadder`). | S | Now a one-line change: boss rounds pay a move
drop, so pushing a brace onto a sub-boss's kit makes it learnable immediately. Put it on a
sub-boss rather than a wild — `kLadder` is explicitly ordered by EFFECTIVE per-turn damage, and a
wild that spends turns bracing deals less per turn, so it would soften the rung it sits on. A
sub-boss sits outside the ladder and pays that cost nowhere. |

**The wild roster's whole vocabulary is five attacks.** `wildMalbeast` gives tiers 1/2/3
`{quick_jab}` / `{quick_jab, packet_storm}` / `{packet_storm, fork_bomb}`, and the sub-area ladder
in `applyWildSubAreaScale` overrides with the same handful again — keyed by DEPTH, not by which
malbeast it is. So a Packet Wraith and a Cache Ghoul at the same rung are mechanically one fight,
and now that drops come from the enemy's kit, no two malbeasts are worth farming differently.
Wants distinctive kit per creature, not per rung. | `combat_factory.cpp`'s `wildMalbeast` +
`kLadder`; `content_moves.cpp`. | L | The content pass, and the biggest of the three. The ladder's
ordering constraint is real and documented — rungs are sorted by EFFECTIVE per-turn damage, so a
long-channel move LOWERS a rung's average — and per-creature kit has to keep that ramp intact
while making the creatures read apart. |

**A crew cannot be DISCOVERED.** `QuoteReward::Kind` has room for it and it is one of the prizes
the board was designed to hand over ("you find a crew to join"), but crews are ungated today —
every row in `content_crews.cpp` is enlistable from the first boot, so there is nothing for a
prize to unlock. Wants a discovery axis on `CrewDef` first, then one `Kind` and one applier case. |
`content_crews.h`; `game_crew.cpp`'s roster filter; `QuoteReward::Kind`. | M | The gating axis is
the real work; the prize is three lines once it exists. |

### 1b. A separation pass over every screen

**There are only three levers for making one thing read apart from another**: put it in a
HEADER, set it BOLD, or spend SCREEN SPACE on a gap. Two of them are free and one is not —
a 224×224 panel has a fixed amount of space and every gap is taken from something else on
the page. So the failure mode is not "this screen is hard to read", it is **spending a
header or a bolding on a distinction that did not need it, and having nothing left but
pixels when a more important one turns up.** That cost lands one screen later than the
decision that caused it, which is why it wants a pass rather than a fix.

The pass: walk every screen, name which lever each distinction is currently using, and check
that the page's most important separation is not the one paying in space. Where the levers
stand today — `drawHeaderBand` is the only thing claiming Bold; VISUAL_LANGUAGE §4.1's
dim-means-READOUT split is the detail pages' free lever and the one that survives grayscale;
the STAT LOADOUT group seams are space spent deliberately, on the page with the least of it.
Diff **M**, taste before code.

### 1c. Evolution routing — one weighted edge list per creature

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

### 1d. Sprite storage — indexed colour - Break-Glass-Flash-Memory-Saving Lever

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

### 1e. Tinting — a second theme

**A second theme is a design pass, not a build.** The machinery takes N themes today and
`PAL_CORE.json` documents the block shape; authoring a colourblind-friendly set (moving the
red/green semantic pair onto a blue/orange axis) needs hue decisions, plus a CFG row to select it
and a save field to remember it. Diff **M**. The `decryption` block is the part that most wants one:
its five code colours are a vocabulary a player has to tell apart at a glance, and while every cell
carries its initial as the grayscale channel, five hues that read as five is the whole board.

### 1f. Standing stubs / interim mechanics to revisit

Intentional simplifications. None is a bug; each is a "confirm as v1 or revise".

- **The derived bold is a smear, not a drawn cut.** 7 cells (`% @ M W _ m w`) already span the
  box and thicken into their own counters. It reads as bold rather than damage on every title
  that ships, so this is polish, not a defect. Pixel Operator's family carries its own Bold
  under the same CC0 — sourcing that cut and pointing `gen_font.py` at it would fix all 7 and
  changes nothing above it, PROVIDED the bold's advance is still 8. If it isn't, deriving stays
  correct and this row closes unbuilt. Diff **S**, sourcing before code.

### 1g. Test-infrastructure gaps

- **No serial test-hook / no automated on-device gameplay verification.** Every device check to date
  is "flash, read the boot line, confirm no crash loop" — nobody has walked the buttons through
  EXPL/combat/Wi-Fi/rank-up on the real panel in a long time. Diff **M** (harness design). A
  human bench pass is also owed.
- **`check_comment_standard.py` gates the mechanical half of the standard and misses the half that
  actually rots.** It fails on board names, `FB-*`/`Phase N` ids, attributions and dates — all of
  which stay clean because they are gated — while *change narration* ("the old X", "used to", "now
  lives on", "this session ships") is unchecked prose, and it is where essentially every finding of
  a docs-cleanup maintenance pass comes from. A phrase list would catch most of it mechanically and
  turn a recurring manual sweep into a build failure. | `tools/check_comment_standard.py`; the
  strip-list in `docs/COMMENT_STANDARD.md` is already the spec. | S | The catch is false positives,
  and they are concentrated and skippable rather than scattered: `save.cpp`/`save.h`/
  `game_persist.cpp` describe old wire formats as their actual subject (an explicit standard
  exception), and present-tense uses ("a row that no longer exists", "used to re-derive") read as
  hits on a naive substring match. Scope it to past-tense constructions and exempt the migration
  units, or it will cry wolf and get muted. |

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
- **The achievement banner doesn't linger long enough.** The home-screen banner is the whole
  feedback channel, so an achievement whose name outruns the time the banner is up is simply lost.
  Wants a marquee plus a minimum time on screen derived from the name's length — characters ×
  marquee speed × a balancing factor — rather than one constant that has to suit every name.
  Diff **S**.
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

The device-side path is built and has installed on hardware over the air, both artifacts, with no
USB involved: a firmware install boots on trial and rolls itself back unless it reaches the main
loop, paints a frame and stays up for `OTA_PROVE_MS`. Publishing is CI-driven off a `v*` tag —
ORIENTATION's *Releasing* is the whole story, and the security trade (SHA-256 per artifact, no code
signing, `setInsecure()`) is written down in `update_manifest.h`.

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

---

## 2. Art

Engine slots for most of these exist (they render via placeholder or text today), so they are
**drop-in the moment they're drawn**. Sizes are logical px; bind colour to `PAL_CORE` tokens.
Inventory: `assets/ASSET_MANIFEST.md`.

### 2a. The sprite-packing tools live outside the repo

`sheetpack.py` (cell packing, the crop-vs-decimate choice and its damage report, the 1px floor
gap) and `quantize.py` (palette snap + binary alpha) are what every generated sprite passes
through, and both sit untracked in a downloads folder. Nothing reproduces a shipped sheet
without them, and the rules they enforce — `ASSET_MANIFEST.md` §C.1's framing lever, cell
seating and decimation trade-off — are written down in prose but held in code nowhere the repo
can see. Promote both into `tools/`, the way `gen_worm_art.py` already holds the Worm line's
drawing vocabulary. Diff **S** (move + a header each).

### 2a-i. Template pet sheet — one row per default animation

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

- **The pantry's 149 glyphs are generated, not drawn.** `tools/gen_item_icons.py` holds a form
  vocabulary and one recipe per item, and the gates check the committed PNGs against it. They read
  as a set and they read at 20px, which is the bar; what they are not is individually observed
  drawing, so a dish whose joke lives in a specific shape (Twisted Pairetzels, Pretzel; Punchcard
  Punch, the card) is better served than one riding a shared form (eight `dome` items differ only
  by how many slashes are cut into the crust). Worth a pass with a real eye, form by form, and the
  tool is the place to do it — a redrawn form fixes every item using it. Diff **M**.
- **Six wild malbeasts** (`SPR_MALBEAST_*`) and **`SPR_DUMMY`**.
- **Process alternates:** `SPR_PET_PHISHLET`, `SPR_PET_CIPHADPOLE`, `SPR_PET_PINGCUB`; **Boot L2:**
  `SPR_PET_RINGWYRM`.
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
- **Per-line move-fx assets / silhouette + eye-anchor data** — eye-pixel metadata
  per sprite + the layered gamma-pulse treatment. Largely procedural + data, not flat icons.
- **Branching-roster sprites** — full `SPR_PET_*` sheets for the named alternates once the roster
  naming session lands.

---

## 3. Size / reviewability watch

Same rule as the `game_*.cpp` units: split *at* ~600 lines, not before, and split by concern
rather than by line count. `save.cpp` (1097), `combat.cpp` (1100), `expl_screen.cpp` (750) and
`cfg_screen.cpp` (660) are each past the number and each still ONE concern at UNIT level —
save.cpp is long because the format is flat, which is not a second responsibility, and combat.cpp
is the turn engine alone, with the factories in `combat_factory.cpp` (400) beside it.

**The unit rule is holding; the mass has moved inside individual functions, where it does not
look.** Two are past the point a reviewer can hold one in their head, and the "it is a
dispatcher, its length follows from the number of cases" defence covers neither:

- **`Combat::applyEffect` (433 lines, `combat.cpp:141`)** — the sharp one. It has **zero `case`
  labels**: it is a sequential if-chain over effect mechanics, not a dispatch table, so length
  here follows from accumulated special cases rather than from a vocabulary. The likeliest seam
  is per-effect-family helpers, matching the structured effect vocabulary the content standard
  already asks rows to be written in. | S per family, L in total. | Worth doing incrementally —
  one family lifted per pass, not a rewrite. |
- **`Game::tick` (434 lines, `game_core.cpp:76`)** — 24 labelled sections in a flat sequence
  (achievements sweep · duel upkeep · decay · capture policy · combat anim · cursor repeat ·
  autosave · evolution · lockout · idle collapse · …), 18 top-level control blocks, almost no
  interleaving between them. The comments already name the split. | M | Each section is a
  candidate `tickX()` private method; the ordering between them is the only real constraint. |

---

## If picking up cold

1. **Net-Sea Crossing art (§2c)** — the area ships mechanically; it is the only rung with no
   backdrop or malbeasts of its own.
2. **The template pet sheet (§2a-i)** — an authoring aid with no ingestion change behind it:
   one labeled row per default animation, so a new creature sheet starts from a plan.
