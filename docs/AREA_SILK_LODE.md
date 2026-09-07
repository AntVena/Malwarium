# THE SILK LODE — the sixth area, and the DARKWEB CRAWL behind it

> **A design proposal, not shipped content.** Nothing here is in `kAreaList[]` yet, so by the
> rule in `areas/area_defs.h` the area does not exist. This file holds the pass that has to
> happen before it can.
>
> When it ships, this file goes away. Everything durable in it belongs in
> `areas/silk_lode/area.cpp`'s own comments, beside the rows it explains — the same place
> every other area's reasoning lives. A design doc that outlives its implementation is a
> second statement of the same facts, and the two disagree the first time a row is tuned.

---

## 1. The one-line pitch

**The whole vocabulary of this hobby is already arachnid, and nobody notices any more.**

A *web*. A *crawler*. A *spider* that walks it. A *net*. We have been using these words for
thirty years as though they were dead metaphors. They are not dead. Something has been
spinning, the whole time, under every area on the ladder — and the Castle's `COMMENT
CATACOMBS` bottom out on a shaft nobody in the keep remembers digging.

Follow it down and the rock is threaded. A seam of silk runs through it the way a lode of ore
does, and it goes everywhere, and it has a market on its back.

The walk down is the reveal. A stair, then a funnel, then fissures packed with code going bad in
place, then something that is not a tunnel at all but a throat — and then, at the bottom, a
building. Somebody put a shrine down here, and it was not us, and it is still being used.

The area is **THE SILK LODE**. The portal inside the shrine opens on the **DARKWEB CRAWL**.

---

## 2. The name

| Criteria (`AREA_NAMING.md §4.1`) | |
|---|---|
| **Traceability** | Silk Road, 2011–2013. The single most recognisable name the hidden internet has. You hear "The Silk Lode" and you have already heard the source. |
| **Gliding transform** | `Road` → `Lode`. One consonant at the front of the syllable, the vowel and the coda untouched. Say them back to back; nothing moves. |
| **Uniqueness** | Not claimed. `kAreaList[]` holds LimeWire, The Pirate Bay, CNET, Napster+torrents, RapidShare — five distribution brands. A market is a category none of them touch. |
| **Tone** | The target is the *marketplace behaviour* — escrow, vendor ratings, exit scams — not the people. No living individual is punned, named, or implied. |

**Reads as a place to someone who misses it entirely.** A lode is a vein of ore running through
rock. "The Silk Lode" is a seam of silk under a mountain, which is a perfectly good place and a
faintly horrible one. The word also carries its own free second meaning: a *lodestone* was the
first compass and was taken for magic — a thing that quietly draws everything toward it, which
is exactly what a web is for.

- **badge:** `SILK` (4 of the 7 the walk badge can spare)
- **title:** `THREADCUTTER` — in the myth, cutting the thread is how a life ends. You cut it.

### 2.1 What this pass deliberately does NOT spend

`AREA_NAMING.md §3.3` reserves phishing and credential-theft hooks for an area that does not
exist yet. **This proposal leaves them entirely untouched.** Nothing below nods at a banking
trojan, a fake login, or a credential dump — the hooks are botnets, hacker groups and the
market's own behaviour. The seventh area still has its era.

---

## 3. The five stretches — the descent

Not a terrain family so much as an ARC, because the shape of the walk is the reveal. It starts
architectural, stops being architectural, becomes briefly and unmistakably *anatomical*, and then
turns architectural again — at which point the player understands that someone built something
down here and it was not us.

| # | Sub-area | Chars | What it is |
|---|---|---|---|
| 0 | `DEADLINK STAIR` | 14 | Down out of the Castle's catacombs. Everything that ever 404'd fell somewhere, and it fell here. Man-made, and the last thing here that is. |
| 1 | `BOTNET FUNNEL` | 13 | The passage narrows. A funnel-web is a real thing a real spider builds, and so is a botnet — a thousand machines shaped into one throat. |
| 2 | `BITROT FISSURE` | 14 | Cracks in the rock, packed with code going bad in place. This is the stretch that is *visibly wrong* rather than merely dark. |
| 3 | `ZOMBIE GULLET` | 13 | It stops being a tunnel. A gullet is not a passage you found; it is a passage something has. |
| 4 | `ZERO DAY SHRINE` | 15 | And then there is a building. A shrine to the thing nobody has a patch for. The signature stretch, and where the portal is. |

**Why these and not the last set.** `ESCROW ADIT` and `CRAWLER GALLERY` were the problem you
named: an adit and a gallery are words for places you would *visit*, and the five together read
as a heritage trail rather than an incursion. `EXIT SCAM STOPE` had no sound in it —
"stope" is a dead syllable. Every name above is picked for hard consonants first
(DEADLINK, BITROT, ZOMBIE GULLET) and the two soft ones are soft on purpose: `FUNNEL` narrows
and `SHRINE` is the hush at the end.

All five keep the `§2.2` behaviour + place-noun pattern (dead links, botnets, bit rot, zombie
machines, zero-days — all generic artifacts, none brand-locked), and all five clear the
18-character authoring ceiling.

**On hinting at what comes next.** `DEAD DROP SUMP` didn't, and the Moors' `CASTLE CAUSEWAY` is
the precedent that it should. `ZERO DAY SHRINE` does it differently: rather than naming the next
place it names the fact that there *is* one, and that something has been worshipped in it. The
portal is inside the shrine, so the fifth stretch is the door rather than the road to it.

## 4. The court

A group is a *character* by construction, which is why they make such good boss rows — and
`§3.2` already blesses the plural shape as long as the boss is genuinely fought as more than one
round. **Two of these are gauntlets for exactly that reason**; the Castle's `THE EIGHT PWNS` is
the precedent.

| Sub | Boss | Chars | Rounds | Hook |
|---|---|---|---|---|
| 0 | `CULT OF THE DEAD CODE` | 21 | 2 — `THE FIRST RITE` (-1), then the banner | cDc. The most famous group there is, occult by their own choice long before anyone needed them to be. They went down these stairs first and left a manifesto and a dead link. |
| 1 | `MARIPOSA OF THE WEAVE` | 21 | 1 | Mariposa, 2009, ~12 million machines. *Mariposa* is Spanish for butterfly, which is the wrong thing to be in a funnel. |
| 2 | `CIH THE UNWRITER` | 16 | 1 | CIH / Chernobyl, 1998 — it overwrote the BIOS. A fissure full of code going bad, held by the thing that unwrites. |
| 3 | `NECURS THE RAISER` | 17 | 1 | Necurs, 2012–2019. The name was already halfway to necromancy; a gullet full of machines somebody else wakes up finishes the walk. **This is the Worm's boss** — see §5.4. |
| 4 | `THE 29A COVEN` | 13 | 3 — a coven is three | 29A, the VX group, 1995–2008. `29A` is hexadecimal for **666**. They did not need our help. The signature boss, and the shrine is theirs. |
| — | `MIRAI THE MANY-LEGGED` | 21 | the 5-round gauntlet | Mirai, 2016. The banner, and the payoff of every spider word in the area. |

`THE SHADOW BROKERS` came out to make room and is worth keeping in the drawer — it is a good
name and it wants a place where being *sold something that isn't there* is the point.

Two notes worth carrying into the area's own comments:

- **`THE 29A COVEN` leans hardest on the reader.** A player who knows VX history gets the best
  joke in the area; a player who does not reads a perfectly good villain and loses nothing, which
  is the whole of `§3.1`.
- **`MIRAI` means "future" in Japanese**, and it is the last thing you meet at the bottom of the
  oldest hole on the map.

`test_wild_and_roster_names_disjoint` still needs running at implementation time — `MARIPOSA`
in particular wants a look against the creature roster.

---

## 5. What the area FIGHTS like

### 5.1 The family: SEVERANCE

The Bayou pierces. The crossing freezes. The Moors rot. The keep bills you. **The Silk Lode cuts
the line between the operator and the pet** — not your Health, not your armor, your hand on the
wheel. Everything here catches something you were about to do.

### 5.2 The threat this area debuts — `c2_hijack`

> **For N turns the A+C Exploit picker is SHUFFLED and enciphered in the Cant, and every row the
> pet cannot read is greyed out and unselectable.**

A botnet's C2 is the channel its operator gives orders on. Hijack it and the swarm is still
alive, still fighting — it just is not taking your calls. That is exactly what the A+C picker is:
the one place a player's hand reaches into a fight that otherwise runs itself
(`game_combat.cpp`'s `openOverride` — the pet's moves, every combat-usable item in the bag, and
the crew Exploit). The rider does not blind you. It takes the wheel.

**The shuffle is the point, and it is why there is no floor.** Enciphering alone leaves muscle
memory intact — a player who knows the heal is row three does not need to read row three. So the
row ORDER is scrambled too, and there is deliberately **no "one row always stays readable" rule**:
if the pet cannot read the Cant, the pet cannot find what it wants, and that is the whole
mechanic rather than a failure of it. A zero-sigil pet loses the override for the duration.

That is survivable in a normal fight *because the pet still fights on its own* — the auto-battle
does not stop, so losing the picker costs you your interventions and not your turn. Which is the
right thing for this rider to take.

**Rules that still need settling:**

1. **Readable means fully readable.** A row is usable if every A–Z letter in its label is a
   learned sigil. Digits, spaces and punctuation are never enciphered (`cant.h` is explicit —
   they carry word shape), so a held picker keeps its *structure* while being useless, which is
   the correct kind of frustrating.
2. **The shuffle and the cipher hold for the rider's duration** rather than rerolling per open —
   otherwise a fight is not solvable even in principle, and the Cant's own encounter cipher is
   already stable within one meeting. (The Crawl breaks this rule on purpose; see §8.)
3. **Grey is not the only signal.** House rule is dual-coded and grayscale-safe, so a held row
   draws its enciphered label *and* a state tag, and the header carries the count of rows held.
4. **The reveal order is an authoring lever.** `cantRevealOrder()` is frequency-descending, so
   short labels of common letters come back first — a picker thaws in an order you can tune by
   naming things well.

`CantCipher` is already width-preserving by design, so enciphering labels in place cannot break
a layout — which is the usual reason a mechanic like this dies on a 224px canvas.

**The counter, in-area:** `crib_sheet` ("Crib Sheet") — a rank-6 mod in the Lode's own pool. In
cryptanalysis a *crib* is the known plaintext you break a cipher with; it is also what you
smuggle into an exam. *Your override picker stays legible and in order.*

### 5.3 New moves

| id | Name | Carried by | What it does |
|---|---|---|---|
| `dead_link` | Dead Link | CULT OF THE DEAD CODE | Takes `{stealMaxHp}%` of max Health. What was here is not here. |
| `funnel_web` | Funnel Web | MARIPOSA | Strips `{stealDef}%` armor and `{dot}`/turn — it only goes one way. |
| `unwrite` | Unwrite | CIH | `{dot}`/turn for `{dotTurns}`, ignores `{pierce}%` armor. It does not damage the file, it removes the fact of it. |
| `brood_sac` | Brood Sac | NECURS — **Worm line only, earned** | §5.4. |
| `raise_host` | Raise Host | NECURS | `{dot}`/turn for `{dotTurns}` turns. Something else wakes up every turn you are still here. |
| `polymorph` | Polymorph | THE 29A COVEN | Ignores `{pierce}%` armor, strips `{stealDef}%`. It is not the same thing twice. |
| `c2_hijack` | C2 Hijack | THE 29A COVEN (apex rider) | §5.2. |
| `legion` | Legion | MIRAI (banner, final round only) | Ignores all armor, freezes `{lock}` turn, and scrambles. "We are many" was never a metaphor; it was a count. |
| `snag_line` | Snag Line | wild ATTACK | Strips `{stealDef}%` armor — you brushed something that was already there. |
| `sheet_web` | Sheet Web | wild DEFEND | A plain brace. A real web type, and the flattest and most patient one. |
| `held_thread` | Held Thread | THE PATIENT WEAVER (guardian) | Freezes `{lock}` turns at modest power. |

`held_thread` keeps the guardian family's rule honestly — *a malbeast hurts you, a guardian rules
against you.* A weaver's ruling is not a blow. It is that **you have been standing on the line
this whole time and it simply has not let go.**

### 5.4 The Worm's own move — and the one rule that genuinely has to change

`brood_sac` is a spider's egg sac, carried by a botnet that raises the dead, learnable **only by
a Worm**, and only by beating `NECURS THE RAISER` in this area. A Worm walks into a spider's lode
and comes out carrying a brood.

I checked all three of the mechanisms this needs, and two of them already work:

- **`replicaSpawnPct` is already line-gated in the ENGINE.** `combat.cpp`'s `replicates()` tests
  the Worm line passive, and `applyEffect` will not spawn without it. So the field is inert on
  any other line by construction — nothing can leak.
- **`moveIsTeachable` already handles line-locked drops correctly.** A move whose `line` does not
  match the pet is *skipped* in the drop roll, with the reason spelled out on the function: the
  equip gate would refuse it anyway, so granting it would be a prize that can never be fielded.

So the authoring rule in `content_moves.cpp`'s boss-pool header (*"`replica*` … stays ZERO on
every row here"*) never actually forbade this. It is scoped to **generic** rows, and it is right
about those: a generic move carrying `replicaSpawnPct` would do nothing at all for four lines out
of five. What the doc lacks is not permission — it is the *category*. It should name a third
kind of row: **line-locked and EARNED**, distinct both from generic drops and from the kit a
line hatches with.

**The one thing that does need code** is that third kind existing at all. `MoveLoadout::
startingForLine` grants a pet **every** move matching its line, so a Worm already owns
`brood_sac` at hatch — and `moveIsTeachable` refuses anything already owned. So as things stand
the drop can never fire.

The fix is small and it opens a genuinely new design space: one flag on `MoveDef` (`earned`, or
`innateToLine = false`) that `startingForLine` skips. Nothing else moves —
`moveIsTeachable`'s line check already does the rest. One documentation edit follows it:
`test_every_generic_move_is_carried`'s exemption reads *"LINE moves are exempt … a hatch owns its
whole line kit, so they need no enemy to teach them"*, and an earned line move breaks that
premise. It is **not** exempt and must be carried by something, or it is unreachable — which is
precisely the bug that gate exists to catch.

### 5.5 Stealing health — the doc is wrong, the mechanic is fine

You are right that this reads as over-restricted, and it turns out to be a documentation bug
rather than a rule that needs loosening. The standard conflates two different fields:

- **`stealMaxHpPct` — taking Health — is already generic and already everywhere.** `seed_leech`
  (Bayou), `toll_charge` (Citrus), `bundle_wrap` (crossing) and `false_positive` (Castle) all
  take a slice of max Health for the fight. Nothing has ever forbidden it. The Lode's `dead_link`
  uses it with no rule change at all.
- **`stealPowerPct` — the Phishing frenzy feeder — is the one that is genuinely reserved,** and
  for a narrower reason than the doc gives. `combat.cpp`'s combo keys on the FIELD: a run of
  `stealPowerPct` casts made *with a shield bubble up* permanently banks flat damage, paid on
  every later steal-attack that fight. A generic row setting it would therefore hand the Phishing
  line an extra frenzy feeder **as a droppable prize** — which is a balance decision, not a
  correctness one.

So the edit is to say *that*, instead of the current blanket phrasing that makes health-stealing
sound off-limits. If you do want `stealPowerPct` opened up, the price is one more feeder in
Phishing's frenzy from a generic drop, and it should be a deliberate call rather than a side
effect of authoring this area.

---

## 6. The guardian — THE PATIENT WEAVER

The register was getting crowded and this one has to dodge four neighbours: the Moors' `THE
INDEX KEEPER` owns being written down, the Castle's `THE LAST RESOLVER` owns whether your name
resolves, the crossing's `THE DEEP LISTENER` owns being overheard, and the Bayou's `THE PORT
WARDEN` owns being admitted.

So this one owns **arrival**. It has never chased anything in its life. It put out a line, and
everything comes to it eventually, and it is not in any particular hurry about you.

```
{"THE PATIENT WEAVER",
 {"held_thread"},
 // lines[] — how it MEETS a pet
 {{"I DID NOT COME TO YOU.",        "IT HAS NOT MOVED AT ALL."},
  {"YOU WALKED ONTO THE LINE.",     "SOMETHING IS ALREADY TAUT."},
  {"EVERYTHING ARRIVES EVENTUALLY.","IT IS IN NO HURRY AT ALL."}},
 // outcomes[] — pleased, displeased, affront, boon
 {{"THEN I WILL LET THIS ONE GO.",  "A SINGLE THREAD GOES SLACK."},
  {"THE LINE WAS ALWAYS MINE.",     "EVERY THREAD DRAWS IN."},
  {"I DO NOT PARLEY WITH FOOD.",    "IT SIMPLY KEEPS WAITING."},
  {"SIT ON THE LINE A WHILE.",      "IT MAKES ROOM ON THE WEB."}}},
```

Every line is inside the one-panel-line budget for `seen` and the two-line budget for `cant`
that `test_every_guardian_speaks_and_fits_the_panel` holds, and none is shared with another
guardian. `I DO NOT PARLEY WITH FOOD` is the affront line and it is doing a lot of work: the
refusal band is the one where a guardian will not even ask an illiterate pet a riddle, and this
is a guardian that would not describe that as rudeness.

---

## 7. Shops and loot

- **Item shop: `THE FLY TRAP`** — a food shop, in a spider's lode, called that. The joke is the
  whole storefront.
- **Mod shop: `WHAT THE WEB CAUGHT`** — the rank-6 shelf. It did not stock anything; it just has
  things.
- **Mod pool** must carry `crib_sheet` (§5.2) plus the rank-6 rungs the deepest named area owes
  a player who walked the whole map. `kModPowerTiers` is `kAreaCount`, so a sixth rank opens
  automatically the moment the row lands — **and no existing mod's `powerTier` moves**, because
  this is an append and not a splice.
- **Wild loot** keeps the staple four (two snacks, the shield, the cleaner) like every other row.

---

## 8. The two endless zones, and swapping their jobs

This is the biggest change in the pass and it is mostly to *shipped* systems, not to the new
area.

### 8.1 The Dive stops being terminal

Today the DeepWeb Dive unlocks on `allSectorsCleared()`, which walks `kAreaCount`. That is a
**moving target**: every area ever added silently pushes the best farm in the game further away,
and re-locks it for anyone who had it. Adding the Silk Lode would do exactly that.

**Move the unlock to "the first time Net-Sea Crossing opens."** It stops moving, and the live-save
problem below disappears entirely rather than being accepted (§9.1).

One implementation note that matters: **key it by area ID, not by rung.** `area_defs.h` already
argues this case for `icon` and `scene` — *the rung is not an identity; splicing an area
renumbers every area above it.* A hardcoded `sectorCleared_[1]` would silently re-point the same
way. So: a named `kDeepWebUnlockAreaId = "net_sea_crossing"` beside the ladder, resolved through
an `areaIndexById()` helper, and the gate stated once by identity.

### 8.2 So the Dive has to get gentler, and the Crawl inherits what it was

A player arriving at Net-Sea is roughly level 20 with a thin kit and few mods. The Dive as tuned
today would eat them, and for a specific reason worth naming: a dive enemy takes its BODY from
the **tier-3 roster** and everything else from `applyDeepWebScale` — `e.level = petLevel`, plus a
budget of `effLevel + depth/8` stat points spent at random, plus the wild challenge buff. Against
a level-60 pet with a full kit that is the measured curve in `deepweb_dive/area.cpp`. Against a
level-20 pet with four moves it is not close.

**So the two zones swap jobs**, which is what you asked for and is also the cleaner story:

| | **DEEPWEB DIVE** | **DARKWEB CRAWL** |
|---|---|---|
| Unlocks | Net-Sea Crossing opens | the Silk Lode's gauntlet is cleared |
| Is | the mid-game farm | the terminal zone |
| Axis | stats and kit | stats, kit, **and legibility** |
| Tuning | a NEW, gentler curve | today's Dive constants, unchanged |

The Crawl keeping today's numbers is the valuable half: **that curve is measured, not guessed.**
The table in `deepweb_dive/area.h` (win rate by build across depths 0→2400, 80 seeded fights per
depth) is real data, and it moves to the Crawl with the constants it describes.

Levers for the Dive's softer curve, cheapest first — `kDeepWebEnemyLevelOffset` (0 → slightly
negative, so a shallow dive sits just under parity), `kDeepWebDepthPointsPerN` (8 → larger, a
gentler linear term), and pushing the rung depths out from `{0, 12, 40, 100, 180}`, since the
file's own note is that a rung which takes the pet's turns away is worth far more than the stat
points at the same depth.

> **The one caution, and it is a real one:** the Dive's current numbers are backed by a
> measurement. Re-tuning it by eye replaces a measured curve with a guessed one. The new curve
> wants the same treatment — seeded fights at the levels a Net-Sea player actually has, and the
> target stated up front (something like: an even-spread level-20 build survives its first ten
> encounters more often than not, and is finished well before depth 100).

### 8.3 The Crawl

Endless, terminal, and **grinding by design**: the Dive's whole loop and the Dive's old
constants, plus the thing that makes it the Silk Lode's own — **`c2_hijack` never lifts.**

Every fight down here runs with the picker shuffled and enciphered, and unlike the boss rider
(§5.2 rule 2) the Crawl **reshuffles per encounter**. There is no learning your way to the heal.
A pet with no sigils simply cannot use an item in the Crawl, and that is correct: the Cant is the
entry ticket, and the ladder spent six areas telling you so.

The reward side has to match. It pays deeper than the Dive on every axis — and it pays **sigils
at milestone depths**, which is the piece that matters beyond mood. A sigil currently costs a
captured WPA handshake, which needs the radio on, a network in range, and a device left
listening: **a player in a quiet flat has a progression system they cannot advance at all.** The
Crawl is the second door onto it, and it is a fitting one, because the zone that demands fluency
is the zone that teaches it.

### 8.4 Where the rows go

`kExplLeadRows` becomes 2. The Dive stays row 0 — `Game::openExplList` parks the cursor there and
it must remain the fewest presses to the best *available* grind — and the Crawl sits under it at
row 1. `explRowIsDeepWeb(row)` is still `row == 0`; `ROCK THE DOCK` keeps the tail.

Worth naming: the cursor-parking rule now fires from mid-game rather than post-game, since the
Dive opens at Net-Sea. That is probably an improvement — an operator with nothing to resume lands
on something they can actually do — but it is a behaviour change to a shipped default, not a
side effect to discover later.

---

## 9. What this costs, honestly

### 9.1 Free, because the ladder was built for it

Appending to the end of `kAreaList[]` is the cheap case and the standard says so. `kAreaCount`,
`kExplSectors`, `kModPowerTiers` and every fixed-size save-flag array follow from the list
itself. **No `ladderInserts` row and no positional migration** — `save.h` is explicit that
appending needs neither. No existing mod's `powerTier` moves. Glyph and backdrop are keyed by
area id rather than rung, so nothing re-points.

**And §8.1 retires the old blocker.** Moving the Dive's unlock off `allSectorsCleared()` means a
sixth area no longer re-locks the best farm in the game for every existing save. That was the one
decision this proposal could not make for itself, and it is now answered by a change that is
worth making on its own merits.

### 9.2 Changes to SHIPPED systems

These are the ones to weigh, because they touch things that already work:

| Change | Where | Risk |
|---|---|---|
| Dive unlock → Net-Sea, keyed by area id | `game.h`'s `allSectorsCleared` caller, `area_defs.h` | Low. Strictly opens earlier; nothing loses access. |
| Dive re-tuned gentler; Crawl inherits today's constants | `deepweb_dive/area.{h,cpp}` | **Medium — see §8.2.** Replaces a measured curve; wants re-measuring, not eyeballing. |
| `MoveDef::earned` so a line move can be learned rather than hatched with | `defs.h`, `move_loadout.cpp`, and the gate's exemption comment | Low, and additive — no existing row sets it. |
| Picker shuffle + cipher | `game_combat.cpp`'s `openOverride`, combat screen | Medium. New state, and the first thing sigils do in a fight. |
| The `stealPowerPct` wording | `content_moves.cpp` header, `CONTENT_STANDARD.md` | None — §5.5 is a doc correction, not a mechanic change. |
| EXPL cursor parks on the Dive from mid-game | `Game::openExplList` | Low, but a shipped default changing behaviour. |

### 9.3 Still not automatic

- **`castle_rapidscare/area.cpp`'s header comment becomes false** — *"last in `kAreaList` … the
  reason its pool is the endgame one."* Both halves stop being true.
- **One new `MoveDef` field** for the scramble, and one for `earned`.
- **Art.** `ICON_SECTOR_SILK_LODE` plus a row in `ASSET_MANIFEST.md §J`. The backdrop can ship as
  `SceneId::None` — two shipped areas already do.
- **Gates:** the badge width gate, `test_expl_names_stay_scrollable`,
  `test_every_generic_move_is_carried` (**and its exemption comment**, per §5.4),
  `test_every_area_has_a_guardian_with_its_own_move`, `test_wild_and_roster_names_disjoint`,
  `test_deepweb_dive` (the unlock condition it asserts moves).
- **The EXPL difficulty pips grow to six.** `drawDifficulty` draws `kExplSectors` pips at 8px
  from `kTextX` (≈24), so the `n/5 CLEARED` detail starts at ≈80 on a 224px canvas. It fits, with
  room for a seventh — but it is close enough now to be worth a look on the device.

---

## 10. Build order

| Phase | What | Difficulty |
|---|---|---|
| 1 | **The Dive's unlock and curve** (§8.1–8.2). Independent of everything else here, fixes a moving target that already exists, and wants its own measurement pass. Worth doing first and alone. | **M** |
| 2 | The area: `silk_lode/area.cpp`, one `kAreaList[]` entry, the moves, the rank-6 mods incl. `crib_sheet`, both shops, the guardian. Scene stays `None`. | **M** |
| 3 | `MoveDef::earned` + `brood_sac` (§5.4), and the `stealPowerPct` doc correction (§5.5). Small, additive, and opens the line-locked-and-earned category for every line, not just the Worm. | **S** |
| 4 | `c2_hijack` — the new field, the shuffle, the cipher pass over the A+C picker. | **M** |
| 5 | Art: the sector glyph, then `SceneId::SilkLode`. | **S** each |
| 6 | **DARKWEB CRAWL.** Second lead row, the Dive's old constants, the picker scrambled per encounter, sigils at milestone depths. | **M–L** |

Phase 1 is worth shipping on its own whatever happens to the rest — the Dive's unlock is a bug in
the shape of a design decision, and it gets worse with every area added. Phase 2 stands alone
after it. Phase 4 is what makes the area itself rather than a sixth difficulty tier.
