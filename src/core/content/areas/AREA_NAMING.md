# World Area Naming Guidelines (Site-Pun Standard)

> **Authoritative reference for naming every location in the 'net** — Areas and Sub-areas alike.
> When a new location name is needed, run it through this standard before proposing it.

> **Note:** This is a **two-tier** standard, distinct from both the creature Phonetic-Integration
> Standard (`04_creature_naming_conventions.md`) and the item Functional-Clarity Standard
> (`05_item_naming_conventions.md`). Areas and Sub-areas are named by **different rules from each
> other** — that's the core of this doc.

---

## 0. The Two-Tier Model

The 'net is a ladder of **AREAS** (`kAreaList[]` in `area_defs.h`), each holding **5 numbered
SUB-AREAS**. The two tiers name completely differently:

| Tier | Names after | Traceable to one brand? | Reusable across areas? |
|---|---|---|---|
| **Area** | one specific real site/service/brand | **Yes — always** | No — one area, one source, never reused |
| **Sub-area** | a behavior or artifact of that era | **No — never** | Yes — generic enough to plausibly recur |

**Rule of thumb:** if you can point at one company or product being punned, it's an Area name. If
it's describing a genre of scummy behavior ("bundled installers," "pop-ups," "cheat codes"), it's
a Sub-area name.

---

## 1. Area Names — The Direct-Reference Standard

An Area name is a **direct, traceable pun on one specific real site/service/brand**, built the
same way the creature standard blends an animal and a tech term (`04 §1`) — except here the two
ingredients are **the brand name** and **an evocative place-type word or adjacent real word**.

### 1.1 The Gliding Rule (reused from `04 §1`)

The transform — a swapped letter, a swapped syllable, a homophone, an inserted sound — must
require *zero change in mouth positioning* at the seam. Say it out loud. If you hesitate mid-word,
it fails.

| Source | Transform | Result |
|---|---|---|
| The Pirate Bay | Bay → Bayou | **Pirate Bayou** *(canon, shipped)* |
| Napster | Napster + torrent, blended on the shared "-ster/-ter-" sound | **Napstorrent Moors** *(canon, shipped)* |
| SourceForge | Forge → Gorge (single consonant swap) | **Source Gorge** |
| AOL | inserted "W" | **AWOL Isles** |
| RapidShare | Share → Scare | **Castle Rapidscare** *(canon, shipped)* |
| Ask Jeeves / Ask Toolbar | Jeeves → Thieves | **Ask Thieves** |
| RealPlayer | Real → Reel (homophone), Player → Slayer (rhyme) | **Reel Slayer** |
| isoHunt | Hunt → Haunt | **ISO Haunt** |
| Demonoid | Demon → Daemon (also our own lifecycle stage) | **Daemonoid** |
| CNET / Download.com | sound-swap reorder ("see-net" → "net-sea") | **Net-Sea Shallows** |
| GeoCities | Cities → (de) City, geode twist | **Geode City** |

### 1.2 Traceability Is Mandatory

Unlike creature names (where the animal must survive, `04 §2`), here it's the **brand** that must
survive recognizably. A player who knows the site should get the reference within a beat; a player
who doesn't should still read it as a plausible place name. If the source is only guessable from
the *aesthetic*, not the *name itself*, it's not passing this standard — it's closer to the older,
looser "Citrus Circuit" pattern (thematic, not a direct pun; grandfathered as canon, not a template
for new Areas).

### 1.3 One Source, One Area, No Reuse

Each Area burns its source brand permanently — don't split one brand across two Areas, and don't
let two Areas point at the same brand. Check `kAreaList[]` before
proposing a new one.

### 1.4 Tone Guardrail

This is affectionate, nostalgic parody of an era, not a takedown of a named person or a mean-spirited
joke at a living victim's expense. Keep the target the **site/service's own infamous behavior**
(bundling, nagware, fake buttons), not real individuals. Don't reproduce a brand's actual logo/wordmark
in shipped art — the pun carries the reference; the visuals should be original.

---

## 2. Sub-area Names — The Generic-Descriptor Standard

A Sub-area name pairs a **malware behavior or artifact** (bundleware, pop-ups, cheat codes, nag
screens, fake seeds) with a **landscape/place noun** (Canyon, Bog, Plaza, Cove, Row, Ridge). It
never references a specific brand — that's what makes it portable.

### 2.1 Why Genericness Matters

Sub-areas describe *what happens there*, not *whose site it was*. Because the behavior isn't
brand-locked, the same flavor of Sub-area could plausibly recur under different Areas (a "pop-up"
themed Sub-area isn't unique to one site — every adware-era site had them). This is the opposite
instinct from Area naming, where reuse is forbidden.

### 2.2 The Behavior + Landscape Pattern

A Sub-area name also has a hard **18-character** ceiling: the EXPL list draws it beside a
right-aligned state tag, and a longer name overdraws the tag. `test_expl_names_fit_their_rows`
enforces it, so a too-long candidate fails the build rather than shipping a broken row.

| Behavior/artifact | + Landscape word | = Sub-area |
|---|---|---|
| bundled installer junk | + Bog | **Bundleware Bog** |
| pop-up ad ambushes | + Plaza | **Pop-Up Plaza** |
| corrupted/renamed files | + Marsh | **Mislabel Marsh** |

### 2.3 Terrain Coherence — the landscape word must match the Area's environment

The landscape half of the pattern isn't a free pick from *any* terrain word — it should match the
**physical environment the Area's name evokes**, so the five Sub-areas read as one continuous place,
not five random nouns. This is why Napstorrent Moors' Sub-areas (Seeder Shallows, Leecher Fen, The
Shared Bog, Spectre Swamp, Castle Causeway) work: every landscape word is wetland, so walking the
list *feels* like the marsh journey the Area's flavor text describes. A grassland word (Steppes), a
canyon word (Quarry), or a coastal word (Delta) dropped into that same list would break the
illusion even though each individually passes the Behavior + Landscape pattern.

**Practical check:** name the Area's terrain family in one word first (moor → wetland; bayou →
swamp/coastal; a grove → forest; a gorge → canyon/rock), then draw every Sub-area's landscape word
from that family. Forest-family words: Thicket, Canopy, Underbrush, Bracken, Bramble, Coppice,
Hollow, Wildwood, Deadfall, Grove itself sparingly (it's the Area's own name — use it at most once,
if ever, to avoid sounding redundant). If a candidate Sub-area's landscape word doesn't belong to
the Area's terrain family, swap it for one that does — the behavior/artifact half stays put.

### 2.4 No Brand Leakage

If a proposed Sub-area name only makes sense next to one specific Area (i.e., it's secretly a
second pun on that Area's brand), it's misclassified — either fold it into the Area name itself or
genericize it. Test: could this Sub-area name plausibly be reassigned to a *different* Area without
sounding wrong? If yes, it passes.

---

## 3. Verification Checklists

### 3.1 Area Checklist (must pass all four)

| Criteria | Question | Pass/Fail |
|---|---|---|
| **Traceability** | Does the name point at exactly one real, identifiable site/service/brand? | ☐ |
| **Gliding Transform** | Does the pun swap/blend on a shared sound, with no mouth-position break? | ☐ |
| **Uniqueness** | Is this brand not already claimed by another Area (check the reserved list)? | ☐ |
| **Tone** | Is it affectionate nostalgia about the site's behavior, not a jab at a person? | ☐ |

### 3.2 Sub-area Checklist (must pass all four)

| Criteria | Question | Pass/Fail |
|---|---|---|
| **Genericness** | Is the name free of any brand-specific reference? | ☐ |
| **Behavior Clarity** | Does the name clearly name one malware-era behavior/artifact? | ☐ |
| **Portability** | Could this name be reassigned to a different Area without sounding wrong? | ☐ |
| **Terrain Coherence** | Does the landscape word belong to this Area's terrain family (§2.3), matching its four siblings? | ☐ |

---

## 4. Anti-Patterns

| Anti-Pattern | Example | Why It Fails |
|---|---|---|
| **Brand pun demoted to Sub-area** | "Bundleware Bog" proposed as an *Area* | It's a behavior, not a traceable single source — Area tier needs one brand. |
| **Sub-area promoted to Area** | "Cheat Code Canyon" as a top-level Area | No specific brand is being punned — it's a category, belongs nested under a cheat-distribution Area. |
| **Untraceable Area** | A vague "download culture" Area with no gliding pun on a named brand | Fails traceability (§1.2) — reads as generic flavor text, not a pun. |
| **Reused brand across Areas** | Two Areas both punning LimeWire | Fails §1.3 — one source, one Area. |
| **Brand leakage into Sub-area** | A Sub-area name that's really a second Napster pun, dropped under a different Area | Fails §2.3 portability test. |

---

## 5. Approved Examples (Benchmarks)

The shipped ladder is `kAreaList[]` in `area_defs.h` — that list is the canon, and an area not in
it doesn't exist yet. Check it before claiming a brand: one source, one Area, never reused.

---

## 6. Application Scope

This standard applies to:

- **All Area names** — the top-level 'net locations.
- **All Sub-area names** — the five nested locations per Area.

This standard does **not** apply to:

- **Creature names** — governed by `04_creature_naming_conventions.md`.
- **Item names** — governed by `05_item_naming_conventions.md`.
- **Storefront / shop names** (e.g. Byte to Eat, Pier-to-Peer) — these follow the item doc's
  Tech-Anchor spirit loosely but aren't formally specced; treat as a soft extension of Area flavor,
  not a third tier.
