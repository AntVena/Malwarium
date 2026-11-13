# Malwarium: the board

> **The one list of what's open.** Planning/design decisions, code TODOs, and art we'll need soon.
> Each item carries a **difficulty** (S/M/L/XL) so work can be dispatched without re-deriving
> context.
>
> **Anything completed and tested is DELETED from this list, not checked off.** This is a planning
> board, not a log: the code and `git log` are the only living history we need, and kept-around
> history just dilutes searches. Same for entries in flight: don't annotate what changed, describe
> what's left.
>
> Don't reference this file from a code comment. It's transient by design; the comment isn't.

---

## How to read the difficulty column

**Difficulty:** `S` ≈ an afternoon, localized · `M` ≈ a session, one subsystem · `L` ≈ multi-file,
needs a save bump or new data model · `XL` ≈ architectural / cross-cutting / needs a design pass first.

**When testing something here, ask first whether a test is warranted**: does another test already
cover it? Does it matter if it fails? If it is worth one, manufacture the starting state rather than
building it up organically.

---

## 1. Code

### 1a. Ready to build

**The two endless zones have no STORY.** Every ladder area's four chapters are written (wires
1-24); DEEPWEB DIVE and DARKWEB CRAWL still carry a zeroed `AreaStoryDef` (`kStoryDeepWeb`,
`kStoryDarkWeb`), which reads as "nothing to say yet" and fires nothing. Only the ARRIVAL beat
can ever fire there: there is no gauntlet down there to open or close. Each wants an arrival
that pays off the thread the ladder ends on. |
*The story* in [`AREA_CONTENT_STANDARD.md`](../src/core/content/areas/AREA_CONTENT_STANDARD.md);
`test_story_chapters_fit_their_page` is the gate. | S each | Wire numbers continue past 24. |

**Capture arming costs ~70KB and the AP ~58KB**, against ~126KB free with the radio idle. The device works, but with ~56KB left once capture is armed, anything else that grows will hit the same wall. Worth a pass at what the capture path actually needs. | `net_capture.h`'s `powerUp` (`esp_wifi_init` + promiscuous + the pcap SD buffers). | M | Measured on device, not estimated: `[ap] down free=126408` → `[cap] armed free=56188`. |

**A crew cannot be DISCOVERED.** `QuoteReward::Kind` has room for it and it is one of the prizes
the board was designed to hand over ("you find a crew to join"), but crews are ungated today:
every row in `content_crews.cpp` is enlistable from the first boot, so there is nothing for a
prize to unlock. Wants a discovery axis on `CrewDef` first, then one `Kind` and one applier case. |
`content_crews.h`; `game_crew.cpp`'s roster filter; `QuoteReward::Kind`. | M | The gating axis is
the real work; the prize is three lines once it exists. |

**The two endless cabinets' BEST rows are guesses.** CHROMATOPHORE's DEEP_COVER (20 passes) and
ISOLATION PROTOCOL's BUFFER_GLUTTON (60 bytes) were set before anyone had played either cabinet
seriously, and they are the only ladders here that cannot be deepened by adding a rung: a BEST is
one run, so the right goal is a number off real high scores, not a step up from the last one.
Collect those, then retune the two rows and decide whether either wants a rung above. |
`content_achievements.cpp`'s `ArcadeCabinetBest` rows. | S | A ladder past five rungs wraps its
tally (`tools/gen_ach_icons.py`'s `tally`), and the bitset holds 256 wires, of which 204 are
spent. |

**The ITEMS detail page is the one detail page with no hint band.** `drawItemDetail` leaves
the band off on purpose ("so the panel gets the room", per the comment over
`kDetailPanelTop`), while MODS, DEFRAG and AV detail all draw one. The footer's lowest line
ends at y 204, above where a band starts (208), so the room argument no longer holds at
today's constants. Decide: add `B <verb>  C BACK` like its siblings, or record the exception
in VISUAL_LANGUAGE §4.1. | `items_screen.cpp`'s `drawItemDetail`; VISUAL_LANGUAGE §4.1 and
§4.2. | S | A design call, which is why the design pass left it. |

**ARCH TRADE IN separates its question from its answers with dead space.** "GONE FOR GOOD.
GET ONE:" points at the offers, but the focused offer's description sits under it and the
offer rows start at y 148, below a band held empty for the confirm box (`kConfirmBoxY`)
that only opens on B. The page reads as two islands. Put the offers directly under the
question with the description beneath them, and open the confirm over the description. |
`arch_screen.cpp`'s sale sheet. | S | Check `arch_sale`, `arch_sale_twin` and
`arch_sale_confirm` on the contact sheet. |

### 1g. Test-infrastructure gaps

- **No serial test-hook / no automated on-device gameplay verification.** Every device check to date
  is "flash, read the boot line, confirm no crash loop": nobody has walked the buttons through
  EXPL/combat/Wi-Fi/rank-up on the real panel in a long time. Diff **M** (harness design).

### 1i. Hacker-face CREW: enlistment shipped, Red/Blue archetype layer open

- **The Red/Blue archetype + PvP contest layer.** Archetypes (Operators: Botmaster/Insider
  Threat/Ghost vs. Guardians: Orchestrator/Watchdog/Dispatcher), the capture broadcast-window
  lifecycle, and target-cooldown + Honeypot/IDS defensive mods have no code. Diff **XL**:
  needs a first design pass reconciling the ability list against everything that's shipped since.
- **The crew roster is seven, both sides, seven ability shapes.** Shipped: Deniers of Service
  (negate), Shell Smashers (bank damage as Power), Injection Protection (reset + floor the stat
  leans), Syntax Errorist (copy the enemy's self-buffs), The Last Responders (turn-metered death
  save that rallies on the overkill), Hot Spares (the next casts resolve twice) and Shakedown Artists (a
  turn-metered rake of damage dealt back as Health). Adding another is still a `kCrews[]` row; a new ability shape
  is one `CrewExploitKind` entry plus one case in `Combat::applyCrewExploit`. What's missing is the
  *earn* model: every crew is joinable the moment a home network exists. Diff **M**.
- **What "defender of that network" buys you.** The home network is recorded and shown but has no
  mechanical consequence beyond gating enlistment. Diff **M**.

### 1j. Architecture: records, runs and screens

`Game` holds every feature's state as its own fields (about 430 of them in `game.h`), so every
`game_*.cpp` unit can touch every field and the 48 units are a physical split only. The
CONTRIBUTING section *Where state lives* says where each field goes: a record (`Pet`,
`Operator`, `Settings`), a run, or a screen. These rows move it there, one type at a time.

Every row lands green on its own: the native suite passes and a `tools/screens.sh` render is
byte-identical to one taken before it, unless the row says otherwise. Each deletes the `debug*`
hooks (about 100 on `Game` today) that its tests no longer need, and moves the tests onto the
new type. `ItemsMenu` and `RigShopScreen` are the screens to copy: state and handlers in their
own type, the game reached through `ItemsHost` and `RigShopHost`. `ItemsMenu` also shows how a
screen keeps derived rows without a stale cache (it keys them on `Inventory::stamp`).

#### Records

**RESET PET restarts part of the `Operator`.** `resetToHatch` puts the HackerTag, the
lifetime counters, Bits, the spare mods and the arcade tallies back to a fresh operator's
and empties the ARCH rack and records (`OperatorArch`), while the CFG screen says RESET
PET "KEEPS PEDIA PROGRESS" and `executeFactoryReset` says it keeps every player-level
tally. Decide which is meant: either a pet reset leaves the record alone, or the part it
restarts is a lifetime of its own on `Operator`. | `game_reset.cpp`, `game_config.cpp`'s
`executeFactoryReset`, `cfg_screen.cpp` | S |
`test_cfg_factory_reset_scopes_differ` states the answer. |

#### Runs

Each owns its clocks (the `last*Ms_` fields on `Game` are mostly one run's step clock), its
random draws and its result, and is gone when it ends. Rows are M unless marked.

- **The tourney**: `tourney*` (11 fields).
- **A PvP session**: `pvp*` (19 fields).
- **The minigame runs**: the arcade run (`arcadeRun_`, score and result fields), the egg pick
  (`eggPick*`, 11 fields), the MAINT boards, the cryptogram run. One row each, S to M.
- **The reveals**: cache yield, bulk yield and the feeding modal's snapshot. S.

#### Screens

- **The rest, one screen per row**: CFG (`cfg*`, 15 fields), ARCH (`arch*`), CREW (`crew*`),
  the MODS and MOVES pickers (`mod*`, `move*` cursors and confirms), the area SHOP, VAULT,
  the arcade list, the story reader, tip cards, and the ITEMS detail page and its Use path
  (`onItemsDetail`, `useItem`, `detailItem_`), which are still `Game`'s. Each one that
  arms B moves its `HoldFocus` case's hold and release entry points onto the new type.
- **The combat screen**: `combatStatsPage_`, the stage's anim clock (`combatAnimBeat_`,
  `lastCombatAnimMs_`) and the camo (`combatCamo*`). They live while the Combat screen is
  up, across a gauntlet's rounds, so they stayed off `Fight`. It reads `Game::fight()`.
- **The encounter intro**: `encounterEnemy_`, `encounterChoice_`, `encounterBypass_`. It
  hands the wild to `startWildCombat` and keeps nothing once a fight opens. S.

**List rows say what they are with bool flags.** `InvRow` carries `header` and `notice`, so a
row can claim to be a notice without being a header, and every reader has to know which
pair is legal. The STAT, prose and collect rows repeat the `header` flag. One row-kind enum
(item, header, notice), shared by the list screens and read by `stepSelectableRow`, makes
the illegal pairs unwritable. | `core/ui/items_screen.h`, `stat_screen`, `prose_page`,
`collect_screen`, the native tests that read `.header` | M | Native suite plus a
`tools/screens.sh` render diffed against one taken before it. |

---

## 2. Art

Engine slots for most of these exist (they render via placeholder or text today), so they are
**drop-in the moment they're drawn**. Sizes are logical px; bind colour to `PAL_CORE` tokens.
Inventory: `assets/ASSET_MANIFEST.md`.

### 2a. One silhouette ink across the roster, then a frame-edge gate

**The roster does not agree with itself about outlines.** Measured on the silhouette edge
(every opaque pixel touching transparency) the shipped sprites split several ways:
`SPR_PET_PAYPUP` is **one tone at 100%**, `SPR_PET_CROAKEN` is one tone at 96%,
`SPR_PET_SYNCAELIA` 83% over four tones, and `SPR_PET_MALBEAR` and `SPR_PET_BRUINFORCE` ~67%
over five or six. The convention is a single dark ink on every boundary pixel, as Paypup does.

**Sweep the roster to it**: for each sheet, recolour boundary pixels to the line's darkest
tone. `tools/quantize.py --outline '#hex'` already does this for one sheet. The sweep must
count the frame's own left, right and top edges as NOT transparent. Otherwise it paints ink
over a cut and hides it from the gate below.

**Then gate it**: on a frame's left, right or top edge, any opaque pixel that is not the
silhouette ink means a cut. The bottom edge is the floor and is exempt. A creature must fit
whole inside its frame; touching the edge on its outline is fine. The check is ~20 lines over
`gen_assets.frame_width` / `frame_rows`, and it replaces judging cuts by eye. |
`assets/`; `tools/quantize.py`. | M | Re-run it after animating any sheet: the still a sheet
was approved from proves nothing about its rows. |

### 2b. Placeholder → final art

These read fine by name + pips today; final art is polish, dropped in where it lands. Roughly
high→low value:

- **`SPR_PET_CROAKEN` and `SPR_PET_GOLIAUTH` `attack` rows**, for a generator pass. A mouth or
  tongue strike is a redraw, not a hand edit: neither frog has a mouth drawn at the front of its
  face to open or to throw a tongue from. Their idle, hop and hurt rows ship, so a swing holds
  the idle until then.
- **Process alternates:** `SPR_PET_PHISHLET`, `SPR_PET_CIPHADPOLE`, `SPR_PET_PINGCUB`; **Boot L2:**
  `SPR_PET_RINGWYRM`.
- **Optional polish:** `UI_RANK_BADGE`, `ICON_EVENT_WIFI`, drawn diamonds for `UI_DIFFICULTY_PIPS` (engine squares
  today), a boss-tell marker
  on the charge bar, a `UI_TITLE_TAG` badge, a `SPR_PET_*` attack-pose frame.
- **Six archetype icons** (`ICON_ARCHETYPE_*`): cosmetic accompaniment to §1i; parked in `_attic/`.

### 2c. New art implied by unbuilt features

These come with the features above rather than ahead of them. The `ICON_SECTOR_*` glyph family is
drawn and live on the EXPL zone picker.

**A backdrop is not a sheet.** A place is engine-drawn. One file under `core/render/scenes/`,
palette-anchored tables composed against a `SceneGround` the screen supplies. Named by a
`SceneId` and reached through `core/render/scenes.h`. An area names its own on `AreaDef::scene`
beside the glyph it already names; a prize background names one from wherever prizes are held,
which is why the id could never hang off `AreaDef` alone. The whole contract is in
`core/render/RENDER_PIPELINE.md`, and `tools/dump_frame.cpp`'s `scene:<name> floor:<row>` is how
you look at one.

- **The one place still unauthored** is a prize, Sunset Colonnade, which has no earner picked
  out yet and should not be authored until it does. ~60 lines against the primitives that
  already exist. Built so far: Citrus Circuit, The Pirate Bayou, Net-Sea Crossing, Napstorrent
  Moors, Castle Rapidscare, The Silk Lode, The DeepWeb Dive, Grid Horizon, Mainframe Row, The
  Line, The CRT Bench, Ground Station, Trace City, The Cipher Hut, The Specimen Hall, The LAN
  Party, Neon Subnet, Paywall Ridge, Zero Day Shrine.
  Beside the six a CREATURE is at home in (`content/content_homes.h`), which are not on the
  authoring list because they belong to a line or a locomotion rather than to the ladder.
  Every one of the twenty-five is an ownable background (`content/content_backgrounds.h`), so a
  new place arrives with a row there and something that earns it. Diff **S**.
- **Backdrop brightness, and one colour for lights. Once the screens are locked.** Every scene
  is held well under `ink` (`test_scene.cpp`), so on the value ramp a scene's brightest "light"
  is a mid grey: a lit window, a buoy lamp or a worm copy reads by contrast and shape (a dark
  housing, a halo, rays) rather than by glowing. Once the screens a backdrop sits under stop
  moving, judge on a board whether the ladder's scenes want their tones lifting, and whether a
  single sanctioned accent hue for small lights (a few pixels a scene, inside the accent budget,
  never a hue that already means a state) is worth adding to the rail. Decide on the device,
  not the contact sheet: the panel's x1.75 changes what reads. Diff **S** (taste first).
- **Net-Sea Crossing area art** (shipped mechanically, art pending): the `FLOATING POINT` / `THE
  HARDENED SHELL` storefront motifs. Its twelve mods are drawn, as is the whole `ICON_MOD_*`
  family, so no area owes one. Like the keep, it fights with the shared tier roster and has no malbeasts of
  its own.
- **Napstorrent Moors area art** (shipped mechanically, art pending): the `MOOR-TO-MOOR`
  storefront motif.
- **Castle Rapidscare art** (shipped mechanically, art pending): castle-themed malbeasts + a
  `COUNT CONFICKER` apex, and the `SPAM & SCRAM` / `THE GHOST IN THE MACHINE` storefront motifs.
  The keep also fights with the tier-3 wild roster today: it has no malbeasts of its own, since a
  new `SPR_MALBEAST_*` grows `kWildMalbeastCount` and with it the 'Pedia's seen/defeated masks.
- **Rarity-tiered, area-themed foods**: per-area food sets across Citrus Circuit / Pirate Bayou /
  Napstorrent with a rarity ramp, where the **best** food carries an effect that fits the area theme
  (Citrus Circuit might trade levels to revert a care mistake; Pirate Bayou the inverse). A
  meaningful new `ICON_ITEM_*` batch plus a mechanic. The permanent once-per-pet payoff such a
  set's best rows would hang off already exists (`core/model/pet_upgrades.h`, six Epic dishes
  today), so what is left here is the per-area rows and their art. Diff **L**.
- **Locomotion poses to match the resting motion.** Every creature declares how it gets around
  (`CreatureDef::locomotion`) and the habitat already moves it that way: a walker ambles along the
  shelf, a swimmer drifts through the box, a flier holds an altitude, and a `Static` row (every egg
  but the jellyfish one) sits exactly where it was put (`core/model/idle_wander.h`).
  The clip the habitat swaps in for as long as the wander is travelling is named `walk` whatever
  the locomotion is, and twelve creatures own one: the four drawn Metamorphic rows, Conkittenate
  and Kalico, and every row `tools/gen_worm_art.py` draws (the Worm line's four movers and the two
  Trojans diverted out of it). Every other mover poses its breathe/blink idle while it travels, so
  a drifting tadpole is a standing tadpole that slides. Wants an extra clip row per mover on the
  existing sheets (swim cycle, step cycle), keyed off the same field: sheet rows, not new
  sprites. Diff **M**.
- **Per-line move-fx assets / silhouette + eye-anchor data**: eye-pixel metadata
  per sprite + the layered gamma-pulse treatment. Largely procedural + data, not flat icons.
- **Branching-roster sprites**: full `SPR_PET_*` sheets for the named alternates once the roster
  naming session lands.

---

## 3. Size / reviewability watch

Same rule as the `game_*.cpp` units: split *at* ~600 lines, not before, and split by concern
rather than by line count. `save.cpp` (1342), `combat.cpp` (2016), `combat_factory.cpp` (1148),
`expl_screen.cpp` (1154) and `cfg_screen.cpp` (919) are each past the number and each still
ONE concern at UNIT level: save.cpp is long because the format is flat, which is not a second
responsibility, and combat.cpp is the turn engine alone.

The units past 600 are one concern each and stay: `game_render` (one function per screen),
`game_persist` (the flat save mapping), `game_core` (the tick clocks), `game_combat` (the fight's
lifecycle, entry to settle, plus the `debug*` combat hooks §1j retires), `game_explore` (the
walk), `game_lifecycle` (one pet's life: hatch, evolution, lockout and CSF), `game_items` (the
ITEMS feature; `applyItemEffects` is a 30-case switch over `ItemEffect::Kind`), and
`game_config`, `game_pvp` (one screen each, just over).

**The function rule.** A long function that is an if-chain rather than a switch over a
vocabulary gets split into its named steps.

- **`Combat::mitigate` (80 lines) keeps its locals, and should.** The pierce ladder, the brace
  and the min-1 floor are one negotiation over a single value; splitting them further means
  handing the pieces a struct of each other. If it grows again the split to reach for is
  pierce RESOLUTION (a pure `int -> int` over the three pierce sources), rather than another
  slice off the end. | `combat.cpp`'s `mitigate`. | S, if it is ever needed. | The phase order
  around it is gated (`test_pipeline_*` in `test_combat.cpp`), so a future split has something
  to fail against rather than only a comment to contradict. |
