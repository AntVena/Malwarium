# ALTAR VISTA — the sixth area, and the DARK WEB behind it

> **A design proposal, not shipped content.** Nothing here is in `kAreaList[]` yet, so by the
> rule in `areas/area_defs.h` the area does not exist. This file holds the pass that has to
> happen before it can: the brand, the five stretches, the court that holds them, the
> guardian, the threat it debuts, and the mode the portal at the end of it opens onto.
>
> When it ships, this file goes away. Everything durable in it belongs in
> `areas/altar_vista/area.cpp`'s own comments, beside the rows it explains — the same place
> every other area's reasoning lives. A design doc that outlives its implementation is a
> second statement of the same facts, and the two disagree the first time a row is tuned.

---

## 1. The one-line pitch

**The Castle was built on top of something older, and the something older still answers.**

`COMMENT CATACOMBS` is already the Castle's deepest stretch — the fifth sub-area of the fifth
area, the last floor before the endless zone. Dig through it and you are not in a new wing of
the keep. You are in a **temple**: an index older than every brand above it, still crawling,
still holding a complete copy of a 'net that has forgotten it exists. It knows every name
there ever was. That is what makes it holy, and that is what makes it a credential thief's
cathedral.

The area is **ALTAR VISTA**. Behind its altar is a door the index was never allowed through,
and that door is the **DARK WEB** — the second endless mode.

---

## 2. Why this area, and why here

Three things in the codebase already point at it, and none of them were written for this.

**The era slot is reserved and empty.** `AREA_NAMING.md §3.3` ends with: *"Phishing and
credential-theft hooks are deliberately unspent — they belong to an Area that doesn't exist
yet, and spending it early would leave that Area with nothing of its own."* Five areas have
spent P2P clients, the warez scene, bundlers and adware, the worm canon, and the file-lockers'
court. The sixth slot has been held open for exactly one era, and it is the one where the
crime is **taking someone's name**.

**The magic system is already built and under-used.** The CANT (`core/model/cant.h`) is a
26-letter substitution the guardians speak in, and a SIGIL is one letter of it the device has
learned to read. That is a spellbook. It is currently reachable from exactly one place — a
guardian met on the walk — and paid for in exactly one currency: a captured WPA handshake
(`Game::buySigil`, `shakesUnspent`). An area whose whole subject is *names you are not
supposed to know* is where the Cant stops being flavour on one screen and becomes a zone's
difficulty axis.

**The descent is already drawn.** No retcon is needed to get the player there. The Moors' fifth
stretch is `CASTLE CAUSEWAY` and it walks up to the keep; the keep's fifth stretch is
`COMMENT CATACOMBS` and it walks down. The ladder has been pointing at a floor below the
Castle since the Castle shipped.

### 2.1 The brand — AltaVista

| Criteria (`AREA_NAMING.md §4.1`) | |
|---|---|
| **Traceability** | AltaVista, 1995–2013. The index everyone used before the one everyone uses. Unmistakable. |
| **Gliding transform** | `Alta` → `Altar`. In a non-rhotic mouth the two are the same sound; in a rhotic one it is a single added consonant at a syllable boundary. Say "AltaVista" and "Altar Vista" back to back — nothing in the jaw moves. |
| **Uniqueness** | Not claimed. `kAreaList[]` holds LimeWire, The Pirate Bay, CNET, Napster+torrents, RapidShare. A search engine is a category none of them touch. |
| **Tone** | Dead brand, no living victim, and the joke is *affection*: the thing that read every page, sitting in the dark with nobody left to ask it. This is elegy, not a takedown. |

It also earns its own position twice over. AltaVista is the **oldest** brand on the ladder — it
predates every other area's source — which is exactly why the area is buried under all of them
and why it reads as ancient. And the pun hands us the altar for free.

**badge:** `ALTAR` (5 of the 7 the walk badge can spare).
**title:** `KEEPER OF NAMES` (15; the longest shipped is `CERTIFIED DOWNLOADER` at 20).

---

## 3. The five stretches

Terrain family: **consecrated ground**. Every landscape word is church or burial vocabulary, so
walking the list reads as one descent rather than five nouns (`§2.3`). Behavior halves are all
generic phishing/credential artifacts and would each survive being reassigned elsewhere (`§2.4`).

| # | Sub-area | Chars | The behavior it names |
|---|---|---|---|
| 0 | `LOGIN LYCHGATE` | 14 | The fake front door. A lychgate is where a churchyard receives the dead — you are met at it, and then you go in. |
| 1 | `KEYLOG CRYPT` | 12 | The thing under the floor that was reading while you typed. |
| 2 | `HARVEST OSSUARY` | 15 | An ossuary is where bones are stacked in bulk. A credential dump is a bone-pile of names. |
| 3 | `FORGERY FONT` | 12 | A font is the basin where you are *given* a name — and a typeface. Homoglyphs, forged wordmarks, the letter that is not the letter. |
| 4 | `SIGIL SANCTUM` | 13 | The innermost room, where a true name is written down. The signature stretch, and where the portal is. |

All five clear the 18-character ceiling `test_expl_names_fit_their_rows` holds.

---

## 4. The court

Hooks are drawn from the phishing/credential-theft slice (`§3.3`), the era that begins the
moment the keep's file-lockers stopped being the most profitable thing on the 'net. The
non-hook halves come from the temple's own vocabulary (`§3.4`), so the five read as one
building's staff: a verger, a sexton, a gleaner, a mirror, a scryer.

| Sub | Boss | Chars | Hook | Why the epithet |
|---|---|---|---|---|
| 0 | `BUGBEAR THE VERGER` | 18 | W32/Bugbear (2002) — mass-mailer with a keylogger in it | A verger walks ahead with a rod and keeps the door. Bugbear is *also* a folklore goblin, which the area gets for free. |
| 1 | `HAXDOOR THE SEXTON` | 18 | Haxdoor (2005) — rootkit + credential logger | A sexton digs the graves and keeps the crypt. It lives under the floor. |
| 2 | `SINOWAL THE GLEANER` | 19 | Sinowal / Mebroot (2007) — harvested credentials at industrial scale | Gleaning is taking what is left in the field after the harvest. |
| 3 | `KOOBFACE THE MIRRORED` | 21 | Koobface (2008) — social-network credential worm | Its name is a name said backwards, which is a spell in every folklore there is. "The Mirrored" is a quality, not a thing (`§3.2`). |
| 4 | `SPYEYE THE SCRYER` | 17 | SpyEye (2010) — the Zeus rival, which shipped a *Zeus-killer* module | A scryer reads what is not in front of it, in a glass. The signature boss. |
| — | `ZEUS THE NAMEGIVER` | 18 | Zeus / Zbot (2007) — the credential thief every later one copied | The banner. A god in a temple, and the thesis of the whole area: **to take a name is to take the thing**. |

Zeus on the banner and SpyEye at the sanctum is deliberate — SpyEye was built to kill Zeus, and
here it is the last thing standing between you and it.

All six clear the 22-character bar `test_expl_names_stay_scrollable` holds. None collides with
the raised roster or the wild malbeasts (`§3.6`) — worth re-running that gate at implementation
time, but `SPYEYE` in particular needs a look against the Phishing line's vocabulary, which the
gate cannot see.

---

## 5. What the area FIGHTS like

Each area's bosses share a family rider, and each area's wild pair is a weaker echo of it. The
Bayou pierces. The crossing freezes. The keep bills you.

**Altar Vista's family is THEFT OF SELF.** Not damage — *transfer*. Its moves take a piece of
you and keep it for the fight (`stealMaxHpPct`) or take your guard off and wear it
(`stealDefensePct`). Nothing else on the ladder owns that pair as an identity, and it is what
phishing actually is: the attacker does not break the door, the attacker becomes you at it.

> **Field discipline:** `stealPowerPct` is the Phishing LINE's, not a generic row's
> (`CONTENT_STANDARD.md`, and the boss pool's header in `content_moves.cpp`). Every row below
> stays off it, however tempting the theme makes it.

### 5.1 New moves

| id | Name | Carried by | What it does |
|---|---|---|---|
| `login_page` | Login Page | BUGBEAR | Strips `{stealDef}%` of the target's armor — the door already has your name on it. |
| `key_logger` | Key Logger | HAXDOOR | `{dot}`/turn for `{dotTurns}` — it was reading the whole time. |
| `harvest_dump` | Harvest Dump | SINOWAL | Takes `{stealMaxHp}%` of max Health for the fight. A hundred thousand names at once. |
| `homoglyph` | Homoglyph | KOOBFACE | Ignores `{pierce}%` armor. One letter is not the letter, and the wall checked the wrong one. |
| `scry_glass` | Scry Glass | SPYEYE | Strips `{stealDef}%` armor and takes `{stealMaxHp}%` max Health — it read your build before the fight. |
| `true_name` | True Name | ZEUS (area banner, final round only) | Ignores **all** armor **and** freezes `{lock}` turn. A thing called by its true name cannot refuse. |
| `cred_harvest` | Credential Harvest | wild ATTACK | Takes `{stealMaxHp}%` max Health. The family's cheap version, farmable at every rung. |
| `two_factor` | Two-Factor | wild DEFEND | A plain brace. The second factor. |
| `robots_txt` | Robots.txt | THE STILL CRAWLER (guardian) | Freezes `{lock}` turns, modest power. |

`robots_txt` deserves its own line, because it is the best move in the proposal and it is
almost free. The guardian family is DENIAL — *"a malbeast hurts you; a guardian rules against
you"* — and robots.txt is the purest denial ever written: **a line of text with no enforcement
behind it that everyone simply obeys.** It is a ward. It is a chalk circle. It is exactly what a
magic area's guardian should be able to do to you, and it really shipped.

### 5.2 The threat this area debuts — `web_inject`

Every area past the first hands its signature boss one rider that nothing else has, and pays
out its counter in its own loot table (the Bayou's `system_hang` answered by its own Watchdog
Timer). Altar Vista's is **WEB INJECT**, and it is the one genuinely new mechanic here:

> **For N turns, the pet's own move list is drawn in the CANT.**

Zeus's webinjects rewrote the bank's page inside your browser — the URL was right, the padlock
was right, and the form was theirs. Here the pet opens its move picker and the picker is in
somebody else's handwriting. You still have every move. You cannot read which is which.

Three reasons this is the right pick and not just a cute one:

1. **The code exists.** `CantCipher` is already width-preserving by design, precisely so a
   panel that fits at zero sigils fits at twenty-six. Running the pet's move labels through it
   cannot break a layout, which is the usual reason a mechanic like this gets rejected.
2. **It has a real skill floor and no wall.** A player who knows their kit's *positions* is
   barely slowed. A player who has been reading the rows is punished for exactly as long as the
   rider lasts. And a fluent pet reads straight through it — **sigils are the counter**, which
   is the first time the Cant has ever paid off in a fight.
3. **It has an item counter in-area,** per the standard: `out_of_band` ("Out-Of-Band"), a
   rank-6 mod in Altar Vista's own pool — *reads the page somewhere the page cannot reach; your
   kit stays legible*.

Open question worth settling before it is built: whether the cipher is rerolled per turn
(vicious) or fixed for the rider's duration (learnable within one fight). **Fixed** is the
better answer — it makes the rider a puzzle rather than a slot machine, and it matches how the
guardian cipher already behaves within one encounter.

---

## 6. The guardian — THE STILL CRAWLER

Every area names one thing that has been watching its network the whole time, met on the walk
and never on the ladder. The registers are getting crowded and this one has to dodge two
neighbours: the Moors' `THE INDEX KEEPER` already owns *being written down*, and the Castle's
`THE LAST RESOLVER` already owns *whether your name resolves at all*.

So Altar Vista's guardian owns neither listing nor naming. It owns **being read**.

It is what is left of the crawl: the spider that walked every page there ever was, holding a
complete copy of a dead 'net, with nobody to hand it to. It grades a pet on one question —
*will you let yourself be read* — and the unnerving part is that being liked by it is worse than
being refused.

```
{"THE STILL CRAWLER",
 {"robots_txt"},
 // lines[] — how it MEETS a pet
 {{"LET ME READ YOU. IT IS PAINLESS.", "IT WANTS TO LOOK AT YOU."},
  {"I HAVE EVERY PAGE THAT EVER WAS.", "IT IS ENORMOUSLY OLD."},
  {"NOBODY HAS ASKED ME IN YEARS.",    "IT HAS NOT MOVED IN YEARS."}},
 // outcomes[] — pleased, displeased, affront, boon
 {{"YOU ARE READ. I HAVE YOU NOW.",  "IT TAKES A COPY OF YOU."},
  {"I WILL NOT READ THAT AGAIN.",    "IT CLOSES OVER ITSELF."},
  {"I DO NOT READ THE WORDLESS.",    "IT DECLINES TO LOOK."},
  {"STAY. I WILL READ YOU ALOUD.",   "IT RECITES YOU BACK."}}},
```

Every line is inside the one-panel-line budget for `seen` and the two-line budget for `cant`
that `test_every_guardian_speaks_and_fits_the_panel` holds, and none is shared with another
guardian.

---

## 7. Shops and loot

- **Item shop: `ALMS & ARRAYS`** — what a temple hands out at the door. Staples plus the
  area's own consumable.
- **Mod shop: `WHAT THE INDEX KEPT`** — the rank-6 shelf, priced in Bits plus a stack of
  whichever consumable the mod is made out of, the way the keep's `THE GHOST IN THE MACHINE`
  charges.
- **Mod pool** must carry `out_of_band` (the `web_inject` counter, per §5.2) plus the rank-6
  rungs the last named area owes a player who walked the whole map. `kModPowerTiers` is
  `kAreaCount`, so a sixth rank opens automatically the moment the row lands — **and no
  existing mod's `powerTier` moves**, because this is an append and not a splice.
- **Wild loot** keeps the staple four (two snacks, the shield, the cleaner) like every other
  area's row.

---

## 8. THE DARK WEB — the second mode

This is the half that is a design risk, and it is worth separating cleanly from the area so the
area can ship without it.

### 8.1 Why it is not just a second Dive

The **DeepWeb Dive** is the *unindexed* web: endless descent, a depth streak, enemies rolled
from a stat budget and a move rung, a loss ends the run. Its axis is **stats and kit**, and its
job is to be the best farm in the game.

A second endless grind zone would be redundant and would cannibalise it. The **Dark Web** has to
be a different verb, and the fiction hands us one for nothing:

> The deep web is what the index *didn't* reach. The dark web is where the index was **told not
> to go** — and it obeyed.

The guardian at the top of this area is a crawler that stopped at a line of text. The portal
behind the altar is what is on the other side of that line. Down there **nothing has a public
name**: no listing, no resolver, no page that will tell you what it is. The only way to address
anything is to already know what it is called.

**So the Dark Web's axis is LEGIBILITY, and its currency is the Cant.**

### 8.2 v1 — THE VEIL (the shippable spine)

Reuse the Dive's loop wholesale: a virtual sector one past the ladder, endless depth streak,
enemies rolled to the pet, a loss ends the run. Change exactly one thing:

**Everything on the enemy's side of the screen is drawn in the Cant.** Its name, its move
announcements, the flavor line. Your own kit stays plain (that is `web_inject`'s job, not this
zone's). The cipher is per-run, not per-encounter, so a run is something you can learn your way
through.

What that buys:

- A player with no sigils fights blind and *survives on pattern* — you learn that the thing
  which opens with `XQFF ZBEZ` is the one that rots you, because it did it to you last time.
  That is a real, playable, atmospheric difficulty.
- Every sigil earned is visibly, immediately worth something down here. The Cant stops being a
  guardian minigame and becomes a stat.
- It costs no new combat mechanics at all — one cipher pass over labels the renderer already
  draws.

**And the payout is the thing that makes it matter: the Dark Web is the second source of
SIGILS.** Right now a sigil costs a captured WPA handshake, which needs the radio on, a network
in range, and a device the player is willing to leave listening. A player in a quiet flat has a
progression system they cannot advance. Milestone depths in the Dark Web paying a sigil fixes
that without devaluing the handshake, because the depths are deep and the radio is fast.

### 8.3 v2 — THE NAMING (the ambition, and a separate decision)

If v1 lands and the zone reads well, the full version gives combat a **third verb**.

Each encounter opens veiled. Beside FIGHT and ITEM there is **NAME IT**, and taking it puts
three candidate names on screen drawn in the Cant, exactly the way a shibboleth puts three
replies up. Get it right and the thing is **bound**: no fight, and it hands over its move or
walks with you as an escort for a few battles. Get it wrong and it opens on you for free.

The difficulty curve writes itself: deeper things have longer names, so how much of a name you
can *read* rather than guess is literally the skill check — one in three at zero sigils, a
reading at twenty-six.

This is where phishing and magic turn out to be the same story, which is the whole reason the
area exists: **taking something's true name is how you take the thing.** The pet spends five
stretches watching a temple do it to other people, and then does it itself.

Cost is honest: a nav state, a screen, an encounter branch, save state for what is bound. It is
an `L`, not an `S`, and it should not be bundled into the same change as the area.

### 8.4 Where the row goes

`kExplLeadRows` becomes 2: the Dive stays row 0 (`Game::openExplList` parks the cursor there,
and it must stay the fewest presses to the best farm), and the Dark Web sits directly under it
at row 1. `explRowIsDeepWeb(row)` is still `row == 0`, and the tail keeps `ROCK THE DOCK` where
it is for its own stated reason. Locked, it draws as `??????` like the other two specials — a
promise rather than a row that appears from nowhere.

Unlock: clearing Altar Vista's gauntlet. The portal is behind the altar, and the altar is the
fifth stretch.

---

## 9. What this costs, honestly

### 9.1 Free, because the ladder was built for it

Appending to the end of `kAreaList[]` is the cheap case and the standard says so. `kAreaCount`,
`kExplSectors`, `kModPowerTiers` and every fixed-size save-flag array follow from the list
itself. **No `ladderInserts` row and no positional migration** — `save.h` is explicit that
appending needs neither. No existing mod's `powerTier` moves. The sector glyph and backdrop are
keyed by area id, not rung, so nothing re-points.

### 9.2 Real edits that do not follow automatically

- **`castle_rapidscare/area.cpp`'s header comment becomes false.** It says the keep is *"last in
  `kAreaList`, which is what makes it the deepest named area before the endless DeepWeb Dive —
  and the reason its pool is the endgame one."* Both halves stop being true. The comment needs
  rewriting and the pool's *"endgame"* framing needs a look: the keep becomes the second-deepest
  shelf, and Altar Vista owes the player the rank-6 one.
- **Art.** `ICON_SECTOR_ALTAR_VISTA` plus a row in `ASSET_MANIFEST.md §J`. The backdrop can ship
  as `SceneId::None` — two shipped areas already do, and the standard calls that a legitimate
  half-step.
- **Gates to satisfy:** the badge width gate, `test_expl_names_fit_their_rows`,
  `test_expl_names_stay_scrollable`, `test_every_generic_move_is_carried` (every new move must
  be named by exactly one `teaches`/rider/wild slot or it is dead content),
  `test_every_area_has_a_guardian_with_its_own_move`, `test_wild_and_roster_names_disjoint`.
- **The EXPL list's difficulty pips grow to six.** `drawDifficulty` draws `kExplSectors` pips at
  8px from `kTextX` (≈24), so the `n/5 CLEARED` detail starts at ≈80 on a 224px canvas. It fits,
  with room for a seventh area — but it is now close enough to be worth a look on the device
  rather than a shrug.

### 9.3 The one decision this needs before it is built

**Adding a sixth area re-locks the DeepWeb Dive for every existing save.**
`allSectorsCleared()` walks `kAreaCount`, so a player who had cleared all five and was farming
the Dive finds it reading `??????` after the update, until they clear Altar Vista.

Nothing is lost — `bestDeepWebDepth_` persists, and their record is intact when it reopens. And
there is a reading where this is correct rather than a regression: the Dive is the *terminal*
zone, "unlocked once every real area is cleared", and the ladder just got longer. A new floor
under the endgame is content, not a takeaway.

But it *is* a live-save consequence with no technical fix that is not a lie (grandfathering
would have to mark an area cleared that nobody has walked), so it should be a decision somebody
makes on purpose. Recommendation: **accept it, and make the update note say so plainly.**

---

## 10. Build order

| Phase | What | Difficulty |
|---|---|---|
| 1 | The area: `altar_vista/area.cpp`, one `kAreaList[]` entry, the nine moves, the rank-6 mods incl. `out_of_band`, both shops, the guardian. Scene stays `None`. | **M** |
| 2 | `web_inject`'s rider — the Cant pass over the pet's own move picker. Small, self-contained, and the first thing the sigils have ever done in a fight. | **S–M** |
| 3 | Art: the sector glyph, then `SceneId::AltarVista` (~60 lines of palette-anchored tables, per the render pipeline doc). | **S** each |
| 4 | **DARK WEB v1 — the Veil.** Second lead row, the Dive's loop, a per-run cipher over the enemy side, sigils at milestone depths. | **M–L** |
| 5 | **DARK WEB v2 — the Naming.** The third combat verb. Own design pass, own change. | **L** |

Phase 1 stands entirely alone and is worth shipping on its own — it is the sixth rung of a
ladder that was built to grow one. Phases 4–5 are a mode, and a mode is a different kind of
promise.
