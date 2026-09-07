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

The area is **THE SILK LODE**. The portal at the bottom of it opens on the **DARKWEB CRAWL**.

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

## 3. The five stretches

Terrain family: **the workings.** Somebody was digging, and broke into something that had
already been spinning. Every landscape word is mining vocabulary, so the five read as one
descent rather than five nouns (`§2.3`), and every behavior half is a generic market or botnet
artifact that would survive being reassigned elsewhere (`§2.4`).

| # | Sub-area | Chars | The behavior it names |
|---|---|---|---|
| 0 | `ESCROW ADIT` | 11 | An adit is the horizontal way *in* to a mine. Escrow is where a market holds what you paid before it decides you may pass. |
| 1 | `BOTNET WARREN` | 13 | A warren is a burrow with a thousand exits and one animal. So is a botnet. |
| 2 | `CRAWLER GALLERY` | 15 | A gallery is a mine passage — and a room of pinned specimens. Both readings are correct here. |
| 3 | `EXIT SCAM STOPE` | 15 | A stope is the void left behind after the ore is taken out. The exit scam is the market's own signature move, and this is the hole where it used to be. |
| 4 | `DEAD DROP SUMP` | 14 | The lowest point of the workings, where everything that falls ends up. The signature stretch, and where the portal is. |

All five clear the 18-character authoring ceiling in `AREA_NAMING.md §2.2`. (The shipped gate,
`test_expl_names_stay_scrollable`, is looser — pixel-measured against each row's own tag with a
2× scroll allowance, ≈26 characters — so 18 is the style rule and the gate is the backstop.)

---

## 4. The court

You said hacker groups make good boss names, and they do — a group is a *character* by
construction, and `§3.2` already blesses the plural shape as long as the boss is genuinely
fought as more than one round. **Three of these six are gauntlets for exactly that reason**, and
the Castle's `THE EIGHT PWNS` is the precedent.

The hooks come from the market-and-botnet slice: things that ran as a *swarm*, which is the same
shape as the thing spinning the lode.

| Sub | Boss | Chars | Rounds | Hook |
|---|---|---|---|---|
| 0 | `THE SHADOW BROKERS` | 18 | 2 — `THE FIRST LOT` (-1), then the banner | 2016. They held an auction and kept what you bid. A broker at the door of an escrow adit is the same job. |
| 1 | `NECURS THE RAISER` | 17 | 1 | Necurs, 2012–2019, one of the largest botnets ever run. The name was already halfway to necromancy; a warren full of machines somebody else wakes up finishes the walk. |
| 2 | `MARIPOSA OF THE WEAVE` | 21 | 1 | Mariposa, 2009, ~12 million machines. *Mariposa* is Spanish for butterfly. A butterfly, in a gallery, in a web. |
| 3 | `CULT OF THE DEAD CODE` | 21 | 2 — `THE FIRST RITE` (-1), then the banner | cDc. The most famous group there is, occult by their own choice long before anyone needed them to be, and a congregation that is *not there any more* is what an exit-scam stope is. |
| 4 | `THE 29A COVEN` | 13 | 3 — a coven is three | 29A, the VX group, 1995–2008. `29A` is hexadecimal for **666**. They did not need our help. The signature boss. |
| — | `MIRAI THE MANY-LEGGED` | 21 | the 5-round gauntlet | Mirai, 2016. The banner, and the payoff of every spider word in the area. |

Two notes worth keeping:

- **`THE 29A COVEN` leans hardest on the reader.** A player who knows VX history gets the best
  joke in the area; a player who does not reads "the 29A Coven" as a perfectly good villain and
  loses nothing, which is the whole of `§3.1`. It is the one name here I would not defend if the
  brief were "instantly legible to everybody" — but the brief is *nod*, and it nods hard.
- **`MIRAI` means "future" in Japanese**, and it is the last thing you meet at the bottom of the
  oldest hole on the map. That irony is intentional and should survive into the area's own
  comments.

All six clear the 22-character bar. `test_wild_and_roster_names_disjoint` still needs running at
implementation time — `MARIPOSA` in particular wants a look against the creature roster.

---

## 5. What the area FIGHTS like

### 5.1 The family: SEVERANCE

Each area's bosses share a rider and its wilds echo it weakly. The Bayou pierces. The crossing
freezes. The Moors rot. The keep bills you.

**The Silk Lode cuts the line between the operator and the pet.** Not your Health, not your
armor — your *hand on the wheel*. Everything here is a thread that catches something you were
about to do.

That is the right family for this area for a reason beyond flavour: it is the only one of the
five levers that has never been anyone's, because it did not exist yet. It needs one new engine
field, and §5.2 is what spends it.

> **Field discipline, checked against `content_moves.cpp`'s own boss-pool header:** `stack*`,
> `shieldPool`, `trap*`, `replica*` and `stealPowerPct` are LINE identity and stay zero on every
> generic row. That rules out the obvious idea — a botnet area that spawns replicas — and it is
> better that it does: `replica*` is the Worm's, and an area is not allowed to borrow a line's
> whole personality for a theme.

### 5.2 The threat this area debuts — `c2_hijack`

Every area past the first hands its signature boss one rider nothing else has, and pays out its
counter in its own loot table. The Silk Lode's is **C2 HIJACK**, and it is your mechanic:

> **For N turns the A+C Exploit picker is enciphered in the Cant, and every row the pet cannot
> read is greyed out and unselectable.**

A botnet's C2 is the channel its operator gives orders on. Hijack the C2 and the swarm is still
alive, still fighting — it just is not taking your calls any more. That is *exactly* what the
A+C picker is: the one place the player's hand reaches into a fight that otherwise runs itself
(`game_combat.cpp`'s `openOverride` — the pet's moves, the combat-usable items, and the crew
Exploit). The rider does not blind you. It takes the wheel.

**The rules, which need settling before it is built:**

1. **Readable means fully readable.** A row is usable if every A–Z letter in its label is a
   sigil the device has learned. Digits, spaces and punctuation are never enciphered (`cant.h`
   is explicit about this — they carry word shape), so a half-read picker still has legible
   *structure* while being unusable, which is the correct kind of frustrating.
2. **It never hard-locks.** At zero sigils every row would fail rule 1, which would be a
   softlock wearing a mechanic's clothes. **The most-readable row always stays usable**, ties
   broken by list order. The rider narrows the picker; it can never empty it.
3. **The cipher is fixed for the rider's duration**, not rerolled per turn. That makes it a
   puzzle you can solve inside one fight rather than a slot machine, and it matches how the
   guardian's cipher already behaves within one encounter.
4. **Grey is not the only signal.** House rule is dual-coded and grayscale-safe — the word
   carries the meaning, colour is emphasis. So a held row draws its enciphered label *and* a
   state tag, and the picker header carries the count of rows held.
5. **The reveal order is an authoring lever.** `cantRevealOrder()` is frequency-descending, so
   short labels made of common letters come back first. That means a picker does not go from
   all-locked to all-open — it thaws in a deliberate order you can tune by naming things well.

**Why this is the right rider and not just a cute one:** `CantCipher` is already
width-preserving *by design*, precisely so a panel that fits at zero sigils fits at twenty-six.
Enciphering labels in place cannot break a layout, which is the usual reason a mechanic like
this gets rejected on a 224px canvas.

**The counter, in-area, per the standard:** `crib_sheet` ("Crib Sheet") — a rank-6 mod in the
Lode's own pool. In cryptanalysis a *crib* is a known piece of plaintext you use to break the
cipher; it is also the thing you smuggle into an exam. *Your override picker stays legible.*

### 5.3 New moves

| id | Name | Carried by | What it does |
|---|---|---|---|
| `escrow_hold` | Escrow Hold | THE SHADOW BROKERS | Freezes `{lock}` turn and takes `{stealMaxHp}%` of max Health — it is holding it until it decides. |
| `botnet_wake` | Botnet Wake | NECURS | `{dot}`/turn for `{dotTurns}` turns. Something else wakes up every turn you are still here. |
| `drag_line` | Drag Line | MARIPOSA | Strips `{stealDef}%` armor and `{dot}`/turn. A spider's dragline is the thread it trails behind it and never lets go of. |
| `vanish_act` | Vanish Act | CULT OF THE DEAD CODE | Takes `{stealMaxHp}%` of max Health. Everything that was here is not here. |
| `polymorph` | Polymorph | THE 29A COVEN | Ignores `{pierce}%` armor and strips `{stealDef}%`. It is not the same thing twice. |
| `legion` | Legion | MIRAI (banner, final round only) | Ignores all armor, freezes `{lock}` turn, and scrambles. "We are many" was never a metaphor; it was a count. |
| `c2_hijack` | C2 Hijack | THE 29A COVEN (apex rider) | §5.2. |
| `snag_line` | Snag Line | wild ATTACK | Strips `{stealDef}%` armor — you brushed something that was already there. |
| `sheet_web` | Sheet Web | wild DEFEND | A plain brace. A real web type, and the flattest, most patient one. |
| `held_thread` | Held Thread | THE PATIENT WEAVER (guardian) | Freezes `{lock}` turns at modest power. |

`held_thread` is the guardian family's rule kept honestly: *a malbeast hurts you; a guardian
rules against you.* A weaver's ruling is not a blow. It is that **you have been standing on the
line this entire time and it simply has not let go.**

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

## 8. THE DARKWEB CRAWL — the second mode

Separated cleanly from the area so the area can ship without it.

### 8.1 The verb, and why it is not a second Dive

The **DEEPWEB DIVE** is the *unindexed* web: endless descent, a depth streak, enemies rolled off
a stat budget and a move rung. Its axis is **stats and kit**, and its job is to be the best farm
in the game. A second endless grind zone would cannibalise it.

The mirror is right there in the words. You **DIVE** into the deep web, because down is the only
direction it has. You **CRAWL** the dark web, because you are on a thread and a crawler is what
walks one. `DEEPWEB DIVE` / `DARKWEB CRAWL` is the pair.

**And the axis is legibility, not stats — because the mode simply IS the area's threat, applied
permanently.** `c2_hijack` takes your picker for three turns. Down here it never gives it back.
Every fight in the Crawl runs with the A+C picker enciphered, and your sigils are the only thing
that decides how much of your own kit you are allowed to use.

That is the cleanest possible relationship between an area and the mode behind it: the area
teaches you the threat as a boss rider, and the mode is what it would be like to live there.

### 8.2 v1 — what ships

Reuse the Dive's loop wholesale (virtual sector one past the ladder, endless depth streak,
enemies rolled to the pet, a loss ends the run). Change two things:

1. **The picker is scrambled for the whole run**, under §5.2's rules — including rule 2, which
   matters far more here than it does for a three-turn rider: a zero-sigil pet still always has
   one usable row, so the Crawl is *punishing* at low fluency and never impossible.
2. **Milestone depths pay SIGILS.** This is the half that makes the mode matter beyond mood.

On that second point: a sigil currently costs a captured WPA handshake, which needs the radio
on, a network in range, and a device left listening. **A player in a quiet flat has a
progression system they cannot advance at all.** The Crawl is the second door onto it. It does
not devalue the handshake — the radio is fast and the Crawl is deep — it just means the Cant is
reachable by playing the game as well as by owning the right neighbours.

### 8.3 v2 — the wrapping (a sketch, and a separate decision)

If v1 reads well: down here nothing arrives with its name showing. Every encounter is **wrapped**
— species, kit and level all hidden — and the label on the bundle is in the Cant. You may CUT it
down blind, or spend a turn to READ it and see exactly what you are about to fight before you
commit. A fluent pet reads for free.

Same currency, same fiction, one new screen instead of a new combat verb. It is an `L`, and it
should not ride in with v1.

### 8.4 Where the row goes

`kExplLeadRows` becomes 2. The Dive stays row 0 — `Game::openExplList` parks the cursor there
and it must stay the fewest presses to the best farm — and the Crawl sits directly under it at
row 1. `explRowIsDeepWeb(row)` is still `row == 0`, and `ROCK THE DOCK` keeps the tail for its
own stated reason. Locked, it draws `??????` like the other two specials.

Unlock: clearing the Silk Lode's gauntlet. The portal is at `DEAD DROP SUMP`.

---

## 9. What this costs, honestly

### 9.1 Free, because the ladder was built for it

Appending to the end of `kAreaList[]` is the cheap case and the standard says so. `kAreaCount`,
`kExplSectors`, `kModPowerTiers` and every fixed-size save-flag array follow from the list
itself. **No `ladderInserts` row and no positional migration** — `save.h` is explicit that
appending needs neither. No existing mod's `powerTier` moves. Glyph and backdrop are keyed by
area id rather than rung, so nothing re-points.

### 9.2 Real edits that do not follow automatically

- **`castle_rapidscare/area.cpp`'s header comment becomes false.** It says the keep is *"last in
  `kAreaList`, which is what makes it the deepest named area before the endless DeepWeb Dive —
  and the reason its pool is the endgame one."* Both halves stop being true; the keep becomes
  the second-deepest shelf and the Lode owes the player the rank-6 one.
- **One new `MoveDef` field** for the scramble (`c2_hijack`, `legion`, and the Crawl). Everything
  else in §5.3 rides existing levers.
- **Art.** `ICON_SECTOR_SILK_LODE` plus a row in `ASSET_MANIFEST.md §J`. The backdrop can ship as
  `SceneId::None` — two shipped areas already do, and the standard calls that a legitimate
  half-step.
- **Gates to satisfy:** the badge width gate, `test_expl_names_stay_scrollable`,
  `test_every_generic_move_is_carried` (every new move must be named by exactly one
  `teaches`/rider/wild slot or it is dead content),
  `test_every_area_has_a_guardian_with_its_own_move`, `test_wild_and_roster_names_disjoint`.
- **The EXPL list's difficulty pips grow to six.** `drawDifficulty` draws `kExplSectors` pips at
  8px from `kTextX` (≈24), so the `n/5 CLEARED` detail starts at ≈80 on a 224px canvas. It fits,
  with room for a seventh — but it is close enough now to be worth a look on the device.

### 9.3 The one decision this needs before it is built

**Adding a sixth area re-locks the DeepWeb Dive for every existing save.**
`allSectorsCleared()` walks `kAreaCount`, so a player who had cleared all five and was farming
the Dive finds it reading `??????` after the update until they clear the Lode.

Nothing is lost — `bestDeepWebDepth_` persists and their record is intact when it reopens. And
there is a fair reading where this is correct rather than a regression: the Dive is the
*terminal* zone, unlocked once every real area is cleared, and the ladder just got longer.

But it is a live-save consequence with no technical fix that is not a lie — grandfathering would
have to mark an area cleared that nobody has walked — so somebody should decide it on purpose.
Recommendation: **accept it, and have the update note say so plainly.**

---

## 10. Build order

| Phase | What | Difficulty |
|---|---|---|
| 1 | The area: `silk_lode/area.cpp`, one `kAreaList[]` entry, the ten moves, the rank-6 mods incl. `crib_sheet`, both shops, the guardian. Scene stays `None`. Ships alone. | **M** |
| 2 | `c2_hijack`'s rider — the new `MoveDef` field, the cipher pass over the A+C picker, and §5.2's five rules (especially rule 2). The first thing the sigils have ever done inside a fight. | **M** |
| 3 | Art: the sector glyph, then `SceneId::SilkLode` (~60 lines of palette-anchored tables, per `RENDER_PIPELINE.md`). | **S** each |
| 4 | **DARKWEB CRAWL v1.** Second lead row, the Dive's loop, the picker scrambled for the run, sigils at milestone depths. | **M–L** |
| 5 | **DARKWEB CRAWL v2 — the wrapping.** Own design pass, own change. | **L** |

Phase 1 stands entirely alone and is worth shipping on its own — it is the sixth rung of a
ladder built to grow one. Phase 2 is what makes the area *itself* rather than a sixth difficulty
tier, and it is small. Phases 4–5 are a mode, and a mode is a different kind of promise.
