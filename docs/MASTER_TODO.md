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

**Rollback is filed as a Buff and it is not one.** Every other Buff does something TO the pet
and is spent doing it; Rollback opens a picker and hands the player a lever over the stat RNG,
which is a different kind of object and reads wrong sitting in the same band as Pwnzu Sauce.
`ItemDef::Type` is `{Food, Buff, Quest}`, so this wants a fourth member rather than a re-label —
and the type is not cosmetic: it drives the inventory's fixed use-frequency order (`itemTypeOrder`)
and the ITEMS hold-B type filter, so a new band has to earn its place in both. Worth checking what
else is sitting in Buff for want of somewhere better before deciding whether the band is
`Tool` alone or a wider re-cut. |
`content_items.cpp`'s `rollback` row + the `Type` enum in `defs.h`; `itemTypeOrder`; the ITEMS
filter in `game_items.cpp`. | M | The row's own comment currently argues the opposite ("a level
re-roll BUFF, not a quest item") — that reasoning was about it not being a QUEST item, and it
answered the wrong question. |

**Capture arming costs ~70KB and the AP ~58KB**, against ~126KB free with the radio idle. The device works, and the save no longer needs a big contiguous block, but that was the only thing standing on this — anything else that grows will hit the same wall. Worth a pass at what the capture path actually needs. | `net_capture.h`'s `powerUp` (`esp_wifi_init` + promiscuous + the pcap SD buffers). | M | Measured on device, not estimated: `[ap] down free=126408` → `[cap] armed free=56188`. |

**Polymorph pays past a ceiling the level table enforces.** `polymorphPay` (`combat.cpp`)
spends an absorbed move as one stat point in `applyLevelStatPoints`' own vocabulary, and every
clamp that vocabulary answers to is applied there — except the brace cap: `defenseMultPct` is
added raw, where `kLevelDefenseBraceCapPct` holds a levelled pet at +200. So a Metamorphic pet
that absorbs Defend rows long enough reaches a brace multiplier no amount of levelling can buy,
which is the failure `kLevelDefenseBraceCapPct` exists to prevent ("a turtle with unbounded
absorb takes a whole turn to become unkillable for the next one, forever"). The fix is the cap
plus the overflow every other clamp site already pays (`capOverflowHealth`), so the payment
still lands. |
`combat.cpp`'s `polymorphPay`; `kLevelDefenseBraceCapPct` in `tunables.h`. | S | The absorb
side is bounded (`kPolymorphAbsorbCap`), so this is a slow climb rather than an exploit — but
it is a ceiling that means one thing for a levelled pet and nothing for an absorbing one. |

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

### 2a. `quantize.py` — rebuilt and promoted ✔

Was missing: it only ever existed in a session scratchpad, which is cleaned between sessions.
Rewritten from its documented call signature and promoted into `tools/` beside `sheetpack.py`,
which it splits work with — **`sheetpack.py` owns geometry, `quantize.py` owns colour and
coverage**, and neither knows about the other, so a sheet can be re-snapped without repacking.

It carries four passes that each exist because of a bug that shipped or nearly did: sharpen
BEFORE an area-average downscale (a plain average at 4:1 melts adjacent forms together);
area-average rather than nearest (nearest deletes whole lattice lines — one egg lost 3,983
one-pixel features that way); a luminance-weighted snap (plain RGB nearest trades away value
steps); and an **accent channel**, which is the one that actually bit — a small eye averaged
into the body around it snaps to the body colour and the eyes vanish silently at every scale
ratio, with nothing about the output looking broken.

`--outline` implements the §2a-ii convention for a single sheet. Verified against
`SPR_PET_SYNCAELIA`: reproduces it from source and, in doing so, found and removed a stray
bright pixel a hand-rolled version of the accent rule had promoted out of a dark brown one.

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

### 2a-iv. A Daemon sheet cannot hold a grid — `frame_rows` only measures 48

**IMPORTANT.** `gen_assets.frame_rows()` finds rows only when a sheet's height divides by
`PET_ROW_H` (48), so a Daemon cell 64 logical px tall reads as ONE row however many are drawn.
The width half of the same problem is already solved — `FRAME_W_OVERRIDES` names a sheet whose
frame width is not 56 — so this is the missing symmetric half: a `ROW_H_OVERRIDES` beside it,
read by `frame_rows` exactly the way the width table is read by `frame_width`.

What it costs today is that **the Metamorphic Daemons are capped at one clip each.**
`SPR_PET_TENTACLONE` and `SPR_PET_SYNCAELIA` are single rows of eight 64-tall cells and declare
`idle` only, while their own Process parent Cuttlefork spends four rows on idle/walk/attack/hurt.
A clip names a row and a frame COUNT and always starts at column 0 (`AnimClip`,
`core/content/defs.h`), so column ranges cannot substitute: Malbear's idle-is-a-prefix-of-attack
shape works only where the clips are ranges of ONE motion, and this line's whole argument is that
its clips are different SCULPTS. So `attack` and `hurt` for either Daemon are blocked on this,
not on art.

It is also what the Worm note in `assets/ASSET_MANIFEST.md` §C.1 is describing when it says a
multi-row sheet at the oversized cell "cannot be cut by `gen_assets.py` at all" — true today,
and a tool limit rather than a law. | `tools/gen_assets.py` (`frame_rows` + a table beside
`FRAME_W_OVERRIDES`); `gen_pedia_data.py` follows for free since it already asks gen_assets for
the grid. | S | Do it before the next Daemon animation pass. The change is small and local; what
makes it important is that every Daemon clip beyond `idle` waits on it. |

### 2a-iii. Every limb must end inside its own frame

**A limb cut off by the frame edge reads as a rendering bug on the device, not as a crop.** At
x1.75 a tentacle or a tail that simply stops on a flat vertical line looks like the blitter
failed, and no amount of good drawing above it recovers the read. So the rule is: a creature
must fit, whole, inside its own frame — the FRAME is sized to the creature, never the creature
trimmed to the frame. The cell may grow (`gen_assets.py` reads any `SPR_PET_` sheet whose width
is not a multiple of 56 as one oversized frame, up to the 128x64 sprite box), which is the
release valve that makes the rule always satisfiable.

The bottom edge is exempt and only the bottom edge: that one is the FLOOR, and a creature
standing on the shelf belongs on it.

Measured over the roster, counting opaque pixels on a frame's left, right and top edges: **24
sheets are clean, 12 are not.** The two worst — `SPR_PET_GOLIAUTH` (109 left, 109 right) and
`SPR_PET_CROAKEN` (33/33/34) — were fixed by the redraw that 2a-0 called for, and the fix is the
pattern: neither was cropped, both were re-composed inside a cell chosen to fit them, which is
why Goliauth now sits in the 96x64 Daemon box rather than the 56px cell it was overflowing. The
mildest remaining are one or two stray pixels on `SPR_PET_MALBEAR` and `SPR_PET_BRUINFORCE`. Some of these are deliberate — a boss filling its cell — so the sweep is a
judgement per sheet, not a blanket re-crop, and that is why this is not already a gate. |
`assets/` + `tools/quantize.py`; the check itself is ~20 lines over `gen_assets.frame_width` /
`frame_rows`. | M | Do it in the same pass as 2a-ii — both walk every pixel of every sheet, and
both end in the same place: a mechanical check in the gates once the roster agrees with itself.
Until then this is a drawing rule enforced by eye, which is how it got broken twice. |

### 2a-ii. Outline consistency pass over the whole roster

**The roster does not agree with itself about outlines.** Measured on the silhouette edge —
every opaque pixel touching transparency — the shipped sprites split three ways:
`SPR_PET_PAYPUP` is **one tone at 100%**, `SPR_PET_CROAKEN` is one tone at 96%, and
`SPR_PET_MALBEAR` and `SPR_PET_BRUINFORCE` are ~67% over five or six tones. So the gold
standard states a rule the rest of the roster only half-follows, and nothing writes it down.

Two things to settle, in this order. **Pick the convention** — a single dark ink on every
boundary pixel (what Paypup does, and what `SPR_PET_SYNCAELIA` now does) or no forced outline
at all. Either is defensible; having both is what reads as sloppy at x1.75. Then **sweep the
roster to match**, which is mechanical: for each sprite, recolour boundary pixels to the
line's darkest tone. Worth doing in the same pass as any re-quantise, since both walk every
pixel of every sheet.

The tool for it already exists: `tools/quantize.py --outline '#hex'` forces the convention on
one sheet (§2a). What is left is the DECISION and then the sweep across the roster. Diff **M**.

### 2b. Placeholder → final art

These read fine by name + pips today; final art is polish, dropped in where it lands. Roughly
high→low value:

- **Six wild malbeasts** (`SPR_MALBEAST_*`) and **`SPR_DUMMY`**.
- **Process alternates:** `SPR_PET_PHISHLET`, `SPR_PET_CIPHADPOLE`, `SPR_PET_PINGCUB`; **Boot L2:**
  `SPR_PET_RINGWYRM`.
- **Optional polish:** `UI_RANK_BADGE`, `ICON_EVENT_WIFI`, `UI_DIFFICULTY_PIPS`, a boss-tell marker
  on the charge bar, a `UI_TITLE_TAG` badge, a `SPR_PET_*` attack-pose frame.
- **Three of the five lines have no `ICON_LINE_*`.** Ransomware and Worm are drawn; Phishing,
  Trojan and Metamorphic are not, so their 'Pedia sections render text-only where the other two
  carry a glyph (`gen_pedia_data.py` warns per missing line). One 20×20 each, same slot as the
  two that exist.
- **Six archetype icons** (`ICON_ARCHETYPE_*`) — cosmetic accompaniment to §1i; parked in `_attic/`.

### 2c. New art implied by unbuilt features

These come with the features above rather than ahead of them. The `ICON_SECTOR_*` glyph family is
drawn and live on the EXPL zone picker.

**A backdrop is not a sheet.** A place is engine-drawn — one file under `core/render/scenes/`,
palette-anchored tables composed against a `SceneGround` the screen supplies — named by a
`SceneId` and reached through `core/render/scenes.h`. An area names its own on `AreaDef::scene`
beside the glyph it already names; a prize background names one from wherever prizes are held,
which is why the id could never hang off `AreaDef` alone. The whole contract is in
`core/render/RENDER_PIPELINE.md`, and `tools/dump_frame.cpp`'s `scene:<name> floor:<row>` is how
you look at one.

- **The four places still unauthored.** Three for the ladder — Net-Sea Crossing (open water and
  shipping lanes, landfall at Sandbox Beach), Napstorrent Moors (marsh into castle country), and
  DeepWeb Dive (no horizon, no floor, no silhouette — relay rings receding to a vanishing point,
  which is the scene that proves the primitives are optional) — and one prize, Sunset Colonnade,
  which has no earner picked out yet and should not be authored until it does. Each is ~60 lines
  against the primitives that already exist. Built so far: Citrus Circuit, The Pirate Bayou,
  Castle Rapidscare, Grid Horizon, Mainframe Row, The Line, The CRT Bench, Ground Station, Trace
  City — beside the six a CREATURE is at home in (`content/content_homes.h`), which are not on the
  authoring list because they belong to a line or a locomotion rather than to the ladder.
  Every one of the fifteen is an ownable background (`content/content_backgrounds.h`), so a new
  place arrives with a row there and something that earns it. Diff **S** each.
- **More achievement-paid places.** `BackgroundSource::Achieve` makes a prize backdrop one content
  row over an achievement id, and four families now pay one out (recipes, the rig, the spectrum,
  the steps). The ones still paying only Bits and a cache are the ARCADE cabinets, the DeepWeb
  depth ladder, the Decryptograms, the bestiary and the LINK peers — each an obvious room. The
  cost is the scene, not the plumbing; the picker's mask is 32 rows wide. Diff **S** each.
- **A glyph for the BACKGROUND row.** It borrows `ICON_CFG`, the generic gear, because the CFG
  family has no picture for "the place your pet stands". One 20x20 beside the other
  `ICON_CFG_*`. Cosmetic; the row reads by its label today. Diff **S**.
- **The five GUARDIANS have no faces.** Each area names one (`AreaDef::guardian`) and all five
  fight in `SPR_PET_CACHEMUTT`, the generic boss frame every authored boss borrows — which reads
  as a malbeast, and a guardian is explicitly not one: it is the thing that has been watching the
  place, and it would rather talk. The SHIBBOLETH screen shows no creature at all today, so the
  only place a face would land is the fight. Wants a family that reads as *watching* rather than
  as *prowling*, and the same family across all five with the area's own motif on it, since what
  differs between them is jurisdiction and not species. Diff **M** — five sheets, or one sheet
  with five palettes. |
  `content/areas/*/area.cpp`'s `guardian` row names the banner; the sprite is named in
  `combat_factory.cpp`'s `guardianEnemy`. |
- **Net-Sea Crossing area art** (shipped mechanically, art pending): the `FLOATING POINT` / `THE
  HARDENED SHELL` storefront motifs. Its twelve mods are drawn — the whole `ICON_MOD_*` family is,
  so no area owes one. Like the keep, it fights with the shared tier roster and has no malbeasts of
  its own.
- **Napstorrent Moors area art** (shipped mechanically, art pending): the `MOOR-TO-MOOR`
  storefront motif.
- **Castle Rapidscare art** (shipped mechanically, art pending): castle-themed malbeasts + a
  `COUNT CONFICKER` apex, and the `SPAM & SCRAM` / `THE GHOST IN THE MACHINE` storefront motifs.
  The keep also fights with the tier-3 wild roster today — it has no malbeasts of its own, since a
  new `SPR_MALBEAST_*` grows `kWildMalbeastCount` and with it the 'Pedia's seen/defeated masks.
- **Rarity-tiered, area-themed foods** — per-area food sets across Citrus Circuit / Pirate Bayou /
  Napstorrent with a rarity ramp, where the **best** food carries an effect that fits the area theme
  (Citrus Circuit might trade levels to revert a care mistake; Pirate Bayou the inverse). A
  meaningful new `ICON_ITEM_*` batch plus a mechanic. The permanent once-per-pet payoff such a
  set's best rows would hang off already exists — `core/model/pet_upgrades.h`, six Epic dishes
  today — so what is left here is the per-area rows and their art. Diff **L**.
- **Locomotion poses to match the resting motion.** Every creature declares how it gets around
  (`CreatureDef::locomotion`) and the habitat already moves it that way — a walker ambles along the
  shelf, a swimmer drifts through the box, a flier holds an altitude, and a `Static` row (every egg
  but the jellyfish one) sits exactly where it was put (`core/model/idle_wander.h`).
  The POSE is still the breathe/blink idle for all of them, so a drifting tadpole is a standing
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
rather than by line count. `save.cpp` (1139), `combat.cpp` (1564), `expl_screen.cpp` (829),
`combat_factory.cpp` (749) and `cfg_screen.cpp` (722) are each past the number and each still
ONE concern at UNIT level — save.cpp is long because the format is flat, which is not a second
responsibility, and combat.cpp is the turn engine alone.

**The unit rule is holding; the mass has moved inside individual functions, where it does not
look.** One is still past the point a reviewer can hold it in their head, and the "it is a
dispatcher, its length follows from the number of cases" defence does not cover it:

- **`Combat::applyEffect` (404 lines, `combat.cpp:337`)** — **zero `case` labels**: a sequential
  if-chain over effect mechanics, not a dispatch table, so its length follows from accumulated
  special cases rather than from a vocabulary. The steal track is lifted
  (`Combat::applyStealTrack`); what remains is the mitigation chain, which genuinely needs the
  locals it accumulates, plus the Trojan trap, the ransom/seizure pair, the two thorns and the
  three crew Exploits hanging off a landed hit. | S per family, M in total. | One family lifted
  per pass, not a rewrite — take the ones the chain's locals do not reach. |

---

## If picking up cold

1. **Net-Sea Crossing art (§2c)** — the area ships mechanically; it is the only rung with no
   backdrop or malbeasts of its own.
2. **The template pet sheet (§2a-i)** — an authoring aid with no ingestion change behind it:
   one labeled row per default animation, so a new creature sheet starts from a plan.
