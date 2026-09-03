# Malwarium — Orientation (the cross-cutting picture)

The parts of the system that belong to no single folder: what the game is, how the repo is laid
out, the stack, the shape of the carousel, the care/evolution model, and the radio design. Anything
that *does* have an owning folder lives there instead — this file points at it rather than
restating it.

---

## What it is

A whimsical, hacker-themed virtual pet on custom ESP32-S3 hardware — Tamagotchi/Digimon reframed
through infosec and demoscene culture. The tone is lighthearted and punny: it celebrates hacker
curiosity without stress or hostility.

**Premise:** running Malwarium, a skilled hacker can safely venture into the 'net and turn wild
**malbeasts** into mastered **petware**. The device is the containment habitat — a vivarium for
malware, where specimens are raised and curated. You tame by *raising*, never by capturing.

| Term | Meaning |
|---|---|
| **Malwarium** | the system / device / habitat itself (proper noun) |
| **malbeast** | a wild, unmastered creature encountered in the 'net |
| **petware** | a tamed creature — the player's raised pet |
| **the 'net** | the explorable world |

Use these consistently across UI, assets and the 'Pedia.

---

## Phasing

| Phase | Theme | Status |
|---|---|---|
| **1** | Render pipeline · idle canvas · carousel · STAT/ITEMS/MAINT · gauges & care · Decryption Hatch · Paypup | ✅ shipped |
| **2** | Remaining submenus (GAMES/EXPL/MODS/ARCH/CFG) · more Process pets · persistence | ✅ shipped |
| **3** | Script & Daemon stages · evolution branching · combat / loadout | ✅ shipped |
| **4** | Radio / sensing / exploration | passive Wi-Fi sensing, audit/pcap capture, CREW enlistment + home network, ESP-NOW discovery (PEERS) and 1v1 duels (LINK) ✅ shipped; the Red/Blue archetype system is the one open piece |

Open work: `docs/MASTER_TODO.md`.

---

## Where everything lives

```
malwarium/
├── CONTRIBUTING.md      ← constraints, standards routing, the gates (read first)
├── README.md            ← the player on-ramp: build one, flash it, play
├── assets/              ← FINAL art + PAL_CORE.json — this directory IS the atlas
│   ├── README.md           the atlas rules + when to read which art doc
│   ├── ASSET_MANIFEST.md · VISUAL_LANGUAGE.md · CREATURE_VISUAL_RULES.md
│   └── _attic/             drawn but consumerless art (untracked, uncompiled)
├── docs/                ← the cross-cutting docs
│   ├── ORIENTATION.md      this file
│   ├── MASTER_TODO.md      the ONE board of open work
│   ├── TEST_STRATEGY.md    test tiers + the two release gates
│   ├── COMMENT_STANDARD.md how to write a comment that won't rot
│   └── MAINTENANCE.md      the recurring self-review pile
├── include/             ← config.h (the verified pinout) · tunables.h (cross-cutting balance)
├── src/core/app/        ← the engine spine: Game, split across game/game_*.cpp
├── src/core/content/    ← content tables, one file per type + their standards & naming docs
│   └── areas/              one folder per EXPL area
├── src/core/model/      ← save codec, combat model
├── src/core/net/        ← audit capture, peer ledger, update manifest, tar reader
├── src/core/render/     ← framebuffer, sprites, upscaler, FONT_UI + RENDER_PIPELINE.md
├── src/core/ui/         ← the shipped screens, on layout.h's grid
├── src/platform/esp32/  ← drivers, radio arbiter, AP server + HARDWARE.md
├── tools/               ← gen_assets.py, gen_font.py (by hand, not a gate), gen_pedia_data.py,
│                          make_web_tar.py, screens.sh, host helpers
├── .github/workflows/   ← gates (every push) · publish (a v* tag → GitHub Pages)
├── pages/               ← the publish host's own pages: a landing page + the USB flasher
└── web/                 ← the SD-served 'Pedia bundle
```

**A shipped screen's contract is the screen.** `src/core/ui/` and `game_render*.cpp` are the truth
about how a screen behaves; there is no parallel document to keep in sync. What *is* documented is
the system a new screen is authored against — the visual language, the asset manifest, this file.

---

## Firmware stack

- **Build:** PlatformIO + Arduino-ESP32, C++17. Per-board envs in one `platformio.ini`; the normal
  cycle builds `waveshare_s3_154` plus the native gates.
- **Display:** **LovyanGFX** — `LGFX_Sprite` offscreen buffers, raw buffer access and DMA
  `pushSprite` fit the hand-rolled framebuffer effects. Arduino_GFX / TFT_eSPI are fallbacks; LVGL
  is overkill for pixel art. The render model itself: `src/core/render/RENDER_PIPELINE.md`.
- **SD:** `SD_MMC` 4-bit SDIO — the slot is not wired for SPI-SD.
- **Audio:** I2S (ES8311 + NS4150B), `config.h`-gated so a codec-less board still compiles.
- **Board:** `src/platform/esp32/HARDWARE.md`; the pinout itself is `include/config.h`.
- **Tests:** three tiers + two release gates — `docs/TEST_STRATEGY.md`.

Don't re-litigate these without a concrete reason.

---

## The carousel

The menu is hidden during idle play; A or C summons it at slot 1, and 5s of inactivity hides it
again. A advances the cursor, C reverses it (or exits a submenu). Slots are assigned by
click-distance, so the most-used categories are the cheapest to reach.

```
[TOP TRACK]     [1 STAT] [2 ITEMS] [3 GAMES] [4 EXPL]
                     [ VIRTUAL PET CANVAS ]
[BOTTOM TRACK]  [5 MAINT] [6 MODS] [7 ARCH] [8 CFG]
```

| Slot | Distance | Category | Holds |
|---|---|---|---|
| 1 STAT | 0 (summon) | Status | vitals, care pips, the last few Hacker-Log events |
| 2 ITEMS | 1 fwd | Consumables | inventory browser + item detail |
| 3 GAMES | 2 fwd | Arcade | the minigames, replayable for Bits + Happiness |
| 4 EXPL | 3 fwd | Explore | the area ladder, walk mode, events |
| 5 MAINT | 4 either | Maintenance | Defrag, AV scan |
| 6 MODS | 3 back | Loadout | the MODS/MOVES/PRACTISE hub — hardware equips, move slots, Sim-Battle |
| 7 ARCH | 2 back | Archive | the pet rack + records, Deploy |
| 8 CFG | 1 back | Config | system info, HackerTag, title, device, radio, updates |

**A+C on the top-level carousel flips to the parallel Hacker face** (PROFILE / CREW / SHOP / VAULT /
MERGE HUB / PEERS / LINK), which is where device-level identity and radio social features live —
except while explore-mode is running, where the chord opens that walk's control overlay instead.
That overlay is a cursor list, like combat's Exploit picker — the chord is the way in, and plain
A/B/C drive it from there. Its rows are Network Ping · Warp · **auto-progress** · Stop explore.
Auto-progress steps the area ladder by itself, positionally, so a finished ladder keeps rotating
instead of stopping at the frontier. The EXPL globe on the carousel turns while it's armed.

The EXPL list is **bracketed by two rows that are not areas**. Above the ladder, the **DeepWeb
Dive** — the endless terminal zone, unlocked by clearing every area, where enemies scale to the
pet and the win streak *is* the depth. Below it, **ROCK THE DOCK**, an eight-operator single-
elimination bracket held in The Pirate Bayou and opened by reaching that water. Its screen is one
Nav with three reads — the BRACKET (the field, who is left, who is next), a **SCOUT** sheet on
hold-B showing any entrant's whole kit, and a paged **BRIEFING** on the A+C chord, which is the
only thing that tells an operator what an arena bout even is. Both readers flow through the shared
name-plus-prose page (`core/ui/prose_page.h`), and the scout sheet is literally STAT's LOADOUT page
pointed at somebody else's pet, so a rival's moves are described in the move's own words — with
that pet's own drawing beside the caption, since an opponent you can only read is one you have to
imagine.

The bracket is drawn as a **tree that empties**: the ties run in a gutter beside the names, a
round to a column, and a knocked-out entrant collapses from a row to a struck stub. The height
that frees up is what stands the next opponent — a real creature cell at 1/1, on a drawn harbour —
at the foot of the field, beside what it is, what Exploit it carries and the health it fires that
Exploit at. So the screen tightens as the draw does, and the layout is
`core/ui/tourney_screen.h`.

The arena is the one place the pet fights **petware** rather than malbeasts: each entrant is a creature off the
hatchable roster at a level up to the arena's cap, carrying a rolled move kit (line-exclusive
moves stay inside their line), rolled mods it is high enough to equip, the per-level stat points a
raise would have bought it, and a **crew Exploit it fires on its own** at a rolled Health trigger.
Both sides get Exploits, which is what a linked duel cannot offer — there the picker would pause
one device and not the other, so a duel grants none, and the arena is where a full pet-vs-pet
fight with the meta layer live can actually be played. The matches the operator is not in are
resolved by stepping the same engine headlessly, so the bracket is climbed rather than sorted, and
the whole run persists as **one seed plus a survivor bitmask** — every entrant is derived from it.
Stakes are Safe and nothing pays until the bracket is taken, at which point the title pays once;
a loss ends the run, which is the only thing at risk.

Opponents having real loadouts is also what pushed the **mid-combat panel** to two pages. B now
CYCLES it — closed → STATE → KIT → closed — because it answers two questions and neither fits
beside the other: what is happening to these two right now (the live leans, absorbs and
afflictions) and what they can DO (each side's equipped kit, and the Exploit a rival is carrying
but has not fired). The STATE readout is a token set (`combatStateTokens`) the draw wraps rather
than one string drawn into a 24-character box, so a fight with a shield, a brace, a ransom bill,
traps, a rot and a stun all live reports every one of them instead of cutting whatever ran past
the edge.

---

## The care model

Four stats per pet: **Hunger** (depletes; critical triggers Lockout), **Fragmentation** (corruption;
drives the glitch passes), **Happiness**, and **Stage**. Balance numbers are named constants in
`include/tunables.h` — test against the constants, never a hard-coded number.

**Care mistakes run on a 5-step budget**, surfaced only in STAT:

| Mistakes | Meaning |
|---|---|
| 0–2 | **Good branch** — defensive/utility Daemons |
| 3–4 | **Bad branch** — aggressive Daemons. A risk/reward trade, *not* a penalty: higher combat strength, higher Fragmentation-gain multiplier. A glass cannon. |
| 5 | **Dying** — accelerated ageing toward Critical System Failure unless recovered |

**Lockout.** A critical stat takes over the UI with a flashing countdown; the player resolves it by
feeding or by spending Bits. Failing costs 2 mistakes instead of 1 — which lands a clean pet exactly
at the Good ceiling, one slip from Bad.

**Stage durations escalate** — the pet demands a longer, more attentive raise as it matures:
egg → Process under an hour, Process → Script 8h, Script → Daemon 32h. So a clean raise reaches the
final form in roughly 40 hours. Each stage also **unlocks combat move slots**, which is the
mechanical reward for raising well.

**Soft-death, not a death timer.** As total lifetime approaches ~128 hours, Hunger decay and
Fragmentation penalties scale up: neglect bites harder the older the pet is. Recovery stays possible
throughout, it just costs more attention. The mistake budget governs *which branch and when dying
triggers*; the lifetime ramp raises the stakes with age.

**Hatching** is per-line: each family's `EggLineDef::hatchGame` picks the minigame, and every one of
them is played once, the instant the egg is laid. Ransomware plays **Disk Decryption**, a five-colour
code board where cracking the key halves the incubation clock; Phishing plays the **Clutch Pick**, a
game of nerve where only the live egg animates among identical decoys; the Worm plays the
**Isolation Protocol**, which eats the clock a byte at a time; the Metamorphic line plays the
**CHROMATOPHORE**, where three buttons wear three colours and the egg has to be wearing the water
when the sweep arrives. None of them can lose you the pet — a bad run costs only the bonus. In the
last stretch of any incubation the Exploit chord then cracks the shell on demand, so an egg never
hatches off-screen. Every one of them is replayable, off its stakes, in the GAMES arcade.

Rosters, evolution routing and per-stage flavour are data on `CreatureDef` rows, one folder per
evolution line under `src/core/content/creatures/` — not a table in a doc.

---

## Currency

**Bits.** Starting balance 30. Sinks: satisfying a Lockout's currency demand, and area shops.
Earned from combat, loot caches, network events and Sim-Battle upkeep.

The combat payout scales to the opponent's stage-rank **R** (Process 2, Script 3, Daemon 4 — a Boot
Sector is never fought). A normal opponent pays a random integer in **[R, R²]**; a **boss** rolls
that range **R times and sums**; a **gauntlet** accrues and pays one lump at the end.

---

## Achievements

The roster is data (`src/core/content/content_achievements.cpp`) — one row names where its progress
comes from, the goal, and the payout, and the engine sweeps them. They are **player-level**: they
survive a pet's death and a new egg, like the HackerTag and earned Titles.

There is no achievement browser on the device — three buttons and 224px don't make one worth having
— so an unlock reaches the player two ways: a **banner on the idle home screen**, flashed the next
time the player is actually looking at it (one earned mid-fight waits rather than being spent on an
empty room), and the **web 'Pedia**, which is where they're read. Deeper rungs pay a **Commendation
Cache**, a container that is only ever earned and never found.

---

## The 'Pedia and the AP

The device hosts a standalone AP; no internet is required. A QR on screen points a phone at the
served site. With an SD card present that's the full bundle from `web/`; without one it degrades to
a plain-text fallback, and the core game loop is unaffected.

State is served as JSON with **string keys, not integer flags**, so it stays readable and
forward-compatible. A creature or item reads as `locked` (hidden behind a placeholder), `seen`
(glitch silhouette + a snarky hint, name masked) or `hatched`/`unlocked` (full reveal). An item's
`unlocked` means **ever held**, not held right now — the same lifetime tally the cuisine ladder
counts, so a consumable does not un-reveal itself by being consumed.

**Food gets its own section, on two independent axes.** Meeting a dish (`items`) decrypts what it
does; winning its **method** off a Decryptogram (`recipes`, `known`/`locked`) decrypts what it
takes. Neither implies the other — dishes are sold on shelves, and methods are never sold at any
price — so a row masks each half separately and says which one it is waiting on. That second gate
is why food has a page of its own rather than a section of ITEMS: it makes a dish an earned entry
rather than a line item.

**The DevTools "exploit" is intentional.** The full dataset is sent to the client and CSS classes
control visibility, so a curious player who opens F12 and strips the `locked` rules can peek at
entries they haven't earned. It's a low-stakes in-universe reward for hacker curiosity, and it
matches what the project is about. Don't "fix" it.

---

## Radio, sensing and pet-to-pet

Two principles: **awareness over safety-rails** (you can be hacked by nearby pets; defence is
equipped, not a default shield) and **consent by presence** (anything that harms your pet needs
another player physically present and powered on — co-presence is the consent token, and absent
friends can only do favours).

**Two orthogonal consents, never one ladder.** The escalating **AUDIT** dial governs how hard the
radio *listens*: Off → Scan (passive discovery, feeds NETS and Hacker Rank) → Scan+Capture (adds
passive WPA-handshake capture). **LINK** governs whether the device *transmits* — ESP-NOW discovery,
broadcasting the operator's HackerTag, pet and crew. Both default OFF and persist separately,
because announcing yourself is a different thing to agree to than listening.

**Reaching the 'net is not a third consent — it has no standing setting at all.** Pressing CHECK NOW
on **UPDATES** is the directive to go online; the job raises the association, reports it as its own
first step (`CONNECTING → CHECKING → the verdict`), and drops it on any terminal outcome. Each job
reconnects, so a firmware install after a check opens its own window. Nothing survives the job and
nothing survives a reboot, so the device is never left sitting on someone's network. What IS
persisted is a stored network — capability, not permission, written only by the setup portal.

The `RadioArbiter` grants exactly one owner at a time: **UPDATE > AP > LINK > CAPTURE > SCAN**. The
CFG **RADIO** screen draws that contention directly: the toggles that are on are indented under a
radio mark, filled for the one on the air and hollow for the ones behind it, with a running update
job drawn in above them for as long as it holds the radio.

**Capture is passive only** — listen and scan, never deauth or inject, at any level. Captures land
as `.pcap` on SD for off-device cracking. A handshake is only stealable by another audit-mode pet
when its BSSID is **co-visible to both devices live**, which is what proves a shared authorization
context; a fresh capture is hot and broadcasting briefly, then goes quiet, and toggling audit off
seals it and starts a re-arm cooldown.

**What the radio finds turns into the walk, and what it FAILS to find does too.** A sighting is
queued, not credited, and an EXPL Wi-Fi event drains one — that resolution is what ROUTES the whole
event, so the headline, the sweep and the flavor lines all state the same fact. An empty queue is
not a penalty: every third dry event summons the area's **GUARDIAN** instead, and a guardian puts a
**SHIBBOLETH** to the pet — a riddle drawn in the **CANT**, the guardians' language, with three
replies drawn the same way (`src/core/app/game/game_shibboleth.cpp`).

**A meeting is three screens, and the riddle is the middle one.** It opens on the **HAIL** — the
thing arrives, does something the pet can see, speaks, and the foot states how much of that the pet
reads and what a sigil costs — and it closes on the **VERDICT**: what the guardian made of the
answer, what that paid or cost, and which button leads where. Every fluency band passes through
both, so a refusal is a refusal the player watched rather than a boss that appeared, and a lost
riddle is something the guardian *did* rather than a fight with no stated cause. Each area authors
its guardian's reaction per outcome (`GuardianDef::outcomes`,
`src/core/content/areas/area_defs.h`) in the same said-twice pair its greetings use.

**The Cant is legibility as a progression.** A letter the pet has learned — a **SIGIL** — is drawn
as itself; every other is drawn as some other, under a substitution rerolled per encounter. So the
same riddle clarifies in place as sigils come in, and because a substitution preserves width, a
riddle that fits the panel at zero sigils fits at twenty-six. Everything the guardian SAYS rides
that cipher — its greeting, its question and its verdict alike; what the pet can SEE it doing is
always plain, and the two are authored as one moment
said twice — so a pet that reads nothing still comes away with the gesture, and the words arrive
later underneath something it already understood. Answering correctly always pays
Happiness and sheds Fragmentation. Learning a sigil is **separate and costs one unspent SHAKE** — a
captured handshake is the only thing a guardian trades its own language for. That split is the two
consents held apart on purpose: the AUDIT dial paces how fast the Cant is learned, and gates none
of the walk's rewards behind itself.

**Discovery is deliberately unauthenticated** — the worst a spoofed beacon achieves is a fake name
in a met-list. Heard peers never enter the ESP-NOW peer table, so the roster is bounded by storage
rather than by the 20-peer limit.

**Duels keep that bargain by keeping the stakes at zero.** A unicast handshake agrees on two fighter
specs and one seed, and then nothing else crosses the air: combat resolves deterministically from
the seed, so both devices rebuild the same fight and step it locally with the same winner. Nothing
is persisted, no reward or penalty is paid, and no Exploit is available mid-fight — the picker would
pause one device and the other cannot be paused. That bargain ends the moment a duel is worth
something: rewards, ladders, trades and consensual mischief are where signing and nonces become
mandatory, along with target cooldowns and the Honeypot/IDS defensive mods.

**Crews** are a player-level allegiance on the Red/Blue axis that outlives any one pet. Enlisting
requires a **home network** — one chosen explicitly from everything the device can see, live scan
and walked history alike — designating the operator its defender. The CREW screen is **four views**
rather than one list, because allegiance is the piece of player identity this device asks you to
author: a hub that states who you are and offers the two sides, a side's own roster, a crew's own
page, and the home-network picker as a modal over the hub. The side is dual-coded down all of them
in one order of authority — the word, then the glyph, then the tint. Each crew grants a signature PVE
Exploit as an extra row in the combat A+C picker, and the Exploit is where the crew's whole
character lives: Red's bank damage into Power or copy the malbeast's own buffs; Blue's negate hits
outright, snap the pet's stats back to what it walked in with, or hold a death save that pays the
overkill back as Power. Each is one `CrewExploitKind` and one case in the single applier — three
metering shapes (charges, turns, or sticky-for-the-fight) cover the vocabulary, so a new crew never
widens the Combatant. The **Red/Blue archetype layer** (Operators:
Botmaster/Insider Threat/Ghost vs. Guardians: Orchestrator/Watchdog/Dispatcher) and the contest
around a capture are the open piece — power there must live in the *contest*, never in the capture
itself.

---

## Releasing

The source is public on GitHub (`AntVena/Malwarium`) under **GPLv3** — a habitat you cannot open
is not one you own, and the licence is where that is enforceable rather than merely meant.

Two workflows in `.github/workflows/`. **gates** runs the normal cycle on every push and pull
request: the native suite, then the S3 firmware build, because `src/platform/esp32` compiles
nowhere else. **publish** fires on a `v*` tag and deploys to **GitHub Pages**, which is the
publish host because a device needs exactly one thing — stable URLs that return bytes. No API, no
rate limit, no cross-host redirect, and the manifest *is* the version pointer, so publishing is
overwriting one file:

```
https://antvena.github.io/Malwarium/manifest.json
```

The same deploy carries **`pages/`** — a landing page and a **Web Serial flasher** at
`/flash/` — plus the three boot images an OTA can never write (`bootloader.bin`,
`partitions.bin`, `boot_app0.bin`). They ride along rather than deploying separately because
the flasher offers whatever the manifest beside it names, and a site one release ahead of its
host would offer a version nobody is holding. `pages/README.md` is that directory's own
standard; `make pages` is the whole publish, and what CI runs.

Cutting a release is `git tag vX.Y.Z && git push origin vX.Y.Z`. **Bump both versions first** —
`include/version.h` and `web/VERSION`. A device installs only what beats what it already runs, so
an unbumped publish is one nobody receives, and bumping both means never having to work out which
one moved. `make manifest` validates its own output with the device's parser before anything is
served, so CI cannot publish a manifest the device would reject — which from the operator's side
is indistinguishable from a dead network.

Each deploy replaces the whole site, so only the current artifacts exist and older URLs 404. That
costs nothing here: rollback is trial-boot to the inactive OTA slot, not a re-download. The web
bundle is written by `tools/make_web_tar.py` rather than `tar` so its bytes depend on its contents
and nothing else — a digest published over an archive that moves with the build is a race the
device loses as `Corrupt`.

**When to publish: whenever the gates are green.** Shipping is pull-based — a device checks, sees a
higher code, and asks its operator — so a release interrupts nobody and needs no permission. Two
changes are the exception, because no later tag undoes them: a **save-format change** (trial-boot
rollback can put the previous firmware back underneath a save the new one already rewrote) and a
**partition-table or bootloader change** (an OTA writes neither, so every device needs USB).

The second of those is what `/flash/` is for. It writes all four boot images, so a table or
bootloader change costs a remote operator a cable and a browser rather than a toolchain — the
difference between an awkward release and a recall. It is still a change to confirm before
tagging, because it reaches nobody who doesn't plug in. Everything else ships.

---

## Read order for a cold start

`CONTRIBUTING.md` → `docs/MASTER_TODO.md` (what's open) → this file → then the `README.md` in
whichever folder you're about to work in.
