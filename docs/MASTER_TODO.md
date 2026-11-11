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

**The Hacker-face hint line has no widget.** The rule at `kActiveH - 26`, the dim secondary
verbs left and the accent primary verb right are drawn by hand at eight sites, and the copies
have already disagreed about which verb takes the accent (PvP had `C BACK` there over its own
`B FIGHT`). One `drawHackerHint(fb, secondary, primary, VerbState)` beside `drawHintBand`
makes the slot unarguable. | `game_crew`, `game_hacker`, `game_merge`, `game_peers`,
`game_pvp`, `rig_shop_screen.cpp`; `core/ui/widgets.h`. | S | Byte-identical against a
`tools/screens.sh` render taken before. |

**ARCH TRADE IN separates its question from its answers with dead space.** "GONE FOR GOOD.
GET ONE:" points at the offers, but the focused offer's description sits under it and the
offer rows start at y 148, below a band held empty for the confirm box (`kConfirmBoxY`)
that only opens on B. The page reads as two islands. Put the offers directly under the
question with the description beneath them, and open the confirm over the description. |
`arch_screen.cpp`'s sale sheet. | S | Check `arch_sale`, `arch_sale_twin` and
`arch_sale_confirm` on the contact sheet. |

**BUFFS keeps its own copy of the prose flow.** `BuffRow`, `buffRowHeights`, `buffFitCount`,
`buffWindowCount` and the draw loop in `drawBuffsScreen` repeat `prose_page.cpp` for a row
that adds only a timer to the name line. The copies have already needed the same heading
change made twice. Give `ProseRow` a name-line suffix and draw BUFFS through
`drawProseRows`. | `stat_screen.cpp`; `core/ui/prose_page.h`. | S | Byte-identical against
a `tools/screens.sh` render taken before. |

### 1g. Test-infrastructure gaps

- **No serial test-hook / no automated on-device gameplay verification.** Every device check to date
  is "flash, read the boot line, confirm no crash loop": nobody has walked the buttons through
  EXPL/combat/Wi-Fi/rank-up on the real panel in a long time. Diff **M** (harness design).
- **`tools/screens.sh` misses four screens, so a refactor of them has no pixel check.** The
  cache-yield reveal (`dump_frame`'s `cache` scene renders the idle habitat instead), the VAULT
  bulk-open reveal and the ARCH new-egg confirm have no `dump_frame` scene; the Wi-Fi event has
  one (`wifi new|fond|hometurf`) but no catalogue line. Add the scenes and list all four in
  `screens.sh`. Diff **S**.

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

### 1j. Architecture: SOLID and the Dependency Rule

Found by auditing the tree against Robert C. Martin's rules (single responsibility,
interface segregation, the Dependency Rule, small functions with few arguments). The
CONTRIBUTING *Structure* section states the rules these rows bring the code up to.

**`Game` holds every feature's state, so every unit can touch every field.** `game.h` is
about 4,700 lines declaring roughly 1,000 member functions and 640 data members, and 48
`game_*.cpp` units implement them. The split into files is physical only. Three problems
follow from it. Holds are routed by guessing which screen is up, because there is no screen
object to ask: `tickHeldGestures` is a ladder of guarded `if`s, and on release `game_nav.cpp`
calls all seven `*ReleaseB()` functions in turn, each returning early unless its own
four-way condition holds. Tests reach state through 100 `debug*` hooks on the production
class. The `nav_` vocabulary is tested in 129 places and switched on in 5. The fix is to
extract feature objects that own their state and handlers, one slice at a time, with
`Game` composing them and dispatching tap, hold and release to the active one.

SHOP and the ITEMS lists are done and are the pattern to copy: `RigShopScreen`
(`game/rig_shop_screen.h`) and `ItemsMenu` (`game/items_menu.h`) own their views and
cursors, and reach the game only through `RigShopHost` and `ItemsHost`, which `Game`
implements privately. `ItemsMenu` also shows how a screen keeps derived rows without a
stale-cache bug: it keys them on `Inventory::stamp`. The ITEMS detail page and its Use path
are still `Game`'s (`onItemsDetail`, `useItem`, `detailItem_`). Next, in order:

1. The Rig Shop ACCOUNT: `rigLevel_`, `rigServicesOff_` and `rigServicesAsk_` and the pure
   queries over them (`rigFeatureActive`, `rigServiceMode` and the rest in `game.h`) become a
   `RigAccount` value type. That shrinks `RigShopHost` and takes those queries out of `game.h`.
2. Hold and release routing. `RigShopScreen` already takes `releaseB` and `openServiceInfo`;
   give the active feature a hold and a release entry point and route to it, then move the
   other hold screens (VAULT, ITEMS, MOVES, MODS, the bracket, STAT index) to the same shape.
3. VAULT, CREW, the tourney and the other Hacker slots, each already in its own unit.

Each slice deletes the `debug*` hooks its tests no longer need. |
`core/app/game.h`; `game_core.cpp`'s `tickHeldGestures`; `game_nav.cpp`'s release edge. |
XL overall, M per slice | Verify each slice with the native suite and a `tools/screens.sh`
render diffed byte-for-byte against one taken before it. |

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
- **Optional polish:** `UI_RANK_BADGE`, `ICON_EVENT_WIFI`, `UI_DIFFICULTY_PIPS`, a boss-tell marker
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
rather than by line count. `save.cpp` (1342), `combat.cpp` (1978), `combat_factory.cpp` (1148),
`expl_screen.cpp` (1112) and `cfg_screen.cpp` (925) are each past the number and each still
ONE concern at UNIT level: save.cpp is long because the format is flat, which is not a second
responsibility, and combat.cpp is the turn engine alone.

The units past 600 are one concern each and stay: `game_render` (one function per screen),
`game_persist` (the flat save mapping), `game_core` (the tick clocks), `game_combat` (the fight's
lifecycle, entry to settle, plus the `debug*` combat hooks §1j retires), `game_explore` (the
walk), and `game_config`, `game_pvp` (one screen each, just over).

Two units have crossed 600 without that judgement. `game_lifecycle` (779) holds the care
lockout, hatch, evolution, CSF, the device wipe and titles: at least the titles and the
wipe/reset are separate concerns and split off cleanly. `game_items` (627) is one concern,
but `applyItemEffects` runs about 210 lines and needs the function rule below applied.

`content_items.cpp` (2,021 lines, 231 rows) is past the point CONTENT_STANDARD rule 4 names
for a folder: 195 of its rows are FOOD, and a reader opens food a family at a time (the
dishes, their ingredients). Split it the way creatures and areas already are, with one list
naming the members so no count is kept by hand.

**The function rule.** What is left is a note rather than a task:

- **`Combat::mitigate` (80 lines) keeps its locals, and should.** The pierce ladder, the brace
  and the min-1 floor are one negotiation over a single value; splitting them further means
  handing the pieces a struct of each other. If it grows again the split to reach for is
  pierce RESOLUTION (a pure `int -> int` over the three pierce sources), rather than another
  slice off the end. | `combat.cpp`'s `mitigate`. | S, if it is ever needed. | The phase order
  around it is gated (`test_pipeline_*` in `test_combat.cpp`), so a future split has something
  to fail against rather than only a comment to contradict. |
