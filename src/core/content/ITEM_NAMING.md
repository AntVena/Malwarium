# Item Naming Guidelines (Functional-Clarity Standard)

> **Authoritative reference for naming all items** — food, buffs, quest items, equipment, and any future categories.
> When a new item name is needed, run it through this standard before proposing it.

> **Note:** Item naming is **not** governed by the creature Phonetic-Integration Standard (`CREATURE_NAMING.md`).
> Items serve a different role in the vocabulary — they must communicate *function* at a glance, not *identity* through phonetic blending.

---

## 1. The Core Constraint: Immediate Functional Recognition

The item's primary function or effect should be **obvious or strongly suggested** within a reading of one or two words.

The "One-Glance" Rule: A player scanning their inventory should infer the item's purpose without needing to read its description. This is a functional priority — **clarity over cleverness.**

**Good example:** `Decryption Key` — tells you exactly what it does. No ambiguity.
**Good example:** `Backup Drive` — the function (backup) is the first word.

---

## 2. The Tech-Anchor Rule (Not the Anti-Swallow)

Whereas creatures must **retain the animal identity** when blended with tech terms, items must **retain the tech term identity**. The infosec pun should be in the tech component, not in the object category.

**Acceptable patterns:**

| Pattern | Example | How It Works |
|---|---|---|
| **Tech + Object** | Decryption Key, Backup Drive, Sinkhole Trap | Straight noun+noun. Tech term is first (or clearly dominant). |
| **Portmanteau + Object** | Tor-Tilla Chip, Yubi-Cookie | Tech term is blended/hyphenated into the object word, then an object word anchors it. |
| **Modifier + Object** | Force-Pulled Pork | Tech concept as adjective + object noun. |

All three patterns are legal, but they are **not equally good** — see §2a, which is the rule that actually predicts whether a name lands.

**Bad example:** `Snack` — the tech term is gone. Fails.
**Bad example:** `The Good Thing` — no tech reference at all. Fails.

---

## 2a. Food Names FUSE; They Don't Modify

The patterns above describe what is *legal*. This describes what is *good*, and for FOOD it is close to a law — read the shipped pantry and it states itself:

> `Grepefruit` · `Shellots` · `Linkguine` · `RAIDicchio` · `Parsenips` · `Pingapple` · `Archichoke` · `Cronstarch` · `Polltatoes` · `Mozillarella` · `IMAPle Syrup` · `Bitroot` · `Kaliflower` · `AWKra` · `Unlinkguine` · `Jailapeno`

**The tech term belongs INSIDE the food word, not in front of it.** Every one of those is a real, specific dish or ingredient with a tech term smuggled into it, usually by changing one or two letters — the food stays fully readable and the pun arrives in the same syllables. That simultaneity is the whole effect, and no amount of polish on a Modifier + Object name reproduces it.

**Modifier + Object is therefore the weakest shape available for a food, and needs a reason.** It has one: a name that must stay tied to a mechanic the art and achievements are already built around. Absent that, fuse it.

**A corollary, not a separate rule:** the object half must still name a *specific* dish or ingredient, never a meta-category. `Chip`, `Pork`, `Cornstarch`, `Cookie`, `Leftovers` are foods; `Snack`, `Treat`, `Meal`, `Dish`, `Item` are aisles. This matters most in the weak shape, because a Modifier + Object name has nothing else holding it up.

**The worked example this rule was written from.** `Air-Gapped Snack` failed on the corollary — "snack" is an aisle — and was renamed to `Air-Gapped Almonds`, which cleared that bar and *still* read wrong beside the shelf. The corollary had caught the smaller of the two faults: the name was Modifier + Object with no blending, so "air-gapped" simply sat in front of an arbitrary noun. It ultimately became two items: `Dyno Nuggets` (dino nuggets; a *dyno* is the container a process runs in) for the everyday ration, and `Unlinkguine` (linguine; `unlink()` severs a reference) for the ghost cure. Both fuse; neither needed a modifier.

**Bare tech words that are already food** are the one honourable exception, and the roster has a real category of them: `Spam`, `Java`, `Cocoa`, `Squash`, `Snap Peas`, `String Beans`. No blending is required when the word arrives pre-blended.

### Non-negotiable: ASCII only

The panel font is ASCII 32..126 (`src/core/render/font_glyphs.cpp`). A character outside it draws as a **blank cell** and mis-measures `textWidth`, which counts bytes — so a display name is `Jailapeno`, never `Jailapeño`, however much the pun wants the tilde.

---

## 3. No Ambiguous Portmanteaus

Creatures benefit from phonetic blending because the player *learns* the creature through play and lore. Items are **consumed on the fly** — there is no learning curve for an item name. The player sees it in their inventory and must decide *now* whether to use it.

**Therefore: if a portmanteau creates ambiguity about the item's function, use the straight noun+noun form instead.**

**Bad example:** `Cryptobite` — is it food? A tool? A currency? Ambiguous.
**Good example:** `Decryption Key` — unambiguous. It's a key for decryption.

---

## 4. Verification Checklist (Must-Pass All Six)

| Criteria | Question to Ask | Pass / Fail |
|---|---|---|
| **Functional Readability** | Can a player guess the item's purpose from the name alone, without reading the description? | ☐ Yes / ☐ No |
| **Tech Anchor Present** | Is the infosec pun / tech term clearly identifiable in the name? | ☐ Yes / ☐ No |
| **No Ambiguity** | Does the name avoid being a generic noun (`Snack`, `Drive`, `Chip`) without a distinguishing tech modifier? | ☐ Yes / ☐ No |
| **Object Specificity** (§2a) | Does the object noun name a specific dish/ingredient, not a meta-category word (`Snack`, `Treat`, `Meal`, `Item`)? | ☐ Yes / ☐ No |
| **Fusion** (§2a) | For a FOOD: is the tech term fused *into* the dish word rather than parked in front of it — and if it isn't, is there a stated reason? | ☐ Yes / ☐ No |
| **ASCII Only** (§2a) | Is every character in the range 32..126, so the panel font can draw it? | ☐ Yes / ☐ No |

---

## 5. Approved Examples (Benchmarks)

| Name | Pattern | Why It Passes |
|---|---|---|
| **Decryption Key** | Tech + Object | Immediate clarity. "Key for decryption" is unambiguous. |
| **Backup Drive** | Tech + Object | "Drive used for backup" — function is the first word. |
| **Force-Pulled Pork** | Modifier + Object | "Force-pulled" modifies "pork" — the tech concept is front and center, and "pork" is a specific cut, not a category (§2a). |
| **Sinkhole Trap** | Tech + Object | "Trap that sinkholes" — clear function. |
| **Tor-Tilla Chip** | Portmanteau + Object | "Tor" is clearly visible in "Tor-Tilla"; "chip" grounds it as an edible item. |
| **Yubi-Cookie** | Portmanteau + Object | "Yubi" is clearly visible; "cookie" grounds it. The tech pun is preserved. |
| **Unlinkguine** | Fused (§2a) | The best shape: `unlink()` lives inside "linguine", so the dish and the pun arrive together. Reads as pasta at a glance. |
| **Dyno Nuggets** | Fused (§2a) | "Dino nuggets" with a *dyno* — the container a process runs in — swapped into it. One letter. |

---

## 6. Anti-Patterns (What to Avoid)

| Anti-Pattern | Example | Why It Fails |
|---|---|---|
| **Vanished Tech** | "Snack", "Cookie" (used bare, alone) | The tech term is gone entirely. No pun survives. |
| **Pure Object** | "Drive", "Key" | Generic noun with no tech modifier. Could be anything. |
| **Generic Object Anchor** (§2a) | "Air-Gapped Snack" | Tech term present and the shape legal, but "snack" is a meta-category, not a dish. |
| **Unfused Modifier** (§2a) | "Air-Gapped Almonds" | The corollary fix for the row above, and still weak: a legal Modifier + Object with no blending at all, so the tech term merely sits in front of an arbitrary noun. Split into `Dyno Nuggets` + `Unlinkguine`, both fused. |
| **Non-ASCII** (§2a) | "Jailapeño" | The panel font is 32..126; the tilde draws as a blank cell and mis-measures the row. Shipped as `Jailapeno`. |
| **Over-blended Portmanteau** | "Cryptobite" | The blend makes the function ambiguous. Items can't rely on player discovery. |
| **Two-Tech Collision** | "Ransomware Recovery" | Two tech terms with no object noun. Hard to parse quickly in inventory. |
| **Redundant Tech** | "Backup Storage Drive" | Three tech-adjacent words with no object anchor. Cluttered, not clearer. |

---

## 7. Consistency with Creature Names

Items and creatures **coexist in the same world**, so avoid naming collisions or confusion:

- An item name should not closely resemble a creature name in both structure *and* the tech term used (e.g., don't name a creature **Ransomsub** and an item **Ransom Key** — the tech term "ransom" alone is too generic).
- If a tech term is already used as the primary pun in a creature name, consider a different tech term or a different angle (adjective, secondary word) for items.
- **Exception:** The same tech term can appear in both categories if the context makes the distinction clear (e.g., `Backup Drive` the item vs. a hypothetical creature whose name uses "backup" in a different way).

This rule is a **soft guideline** — it matters more when names are close enough to cause real confusion, and less when the distinction is obvious from context.

---

## 8. Application Scope

This standard applies to:

- **All item names** across all categories (food, buffs, quest items, consumables, equipment).
- **All rarities** (Common, Uncommon, Rare, Epic) — naming quality is independent of rarity.

This standard does **not** apply to:

- **Creature names** — governed by `CREATURE_NAMING.md` (Phonetic-Integration Standard).
- **World area names** — governed by `areas/AREA_NAMING.md` (Site-Pun Standard),
  cross-referenced from the area roster in `areas/`.
- **Move / combat ability names** — separate convention TBD.
