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
| **Modifier + Object** | Air-Gapped Snack | Tech concept as adjective + object noun. |

**All three patterns are valid.** The portmanteau style (Tor-Tilla, Yubi-Cookie) is a *stylistic variation*, not a requirement. The tech term must remain identifiable in either form.

**Bad example:** `Snack` — the tech term is gone. Fails.
**Bad example:** `The Good Thing` — no tech reference at all. Fails.

---

## 3. No Ambiguous Portmanteaus

Creatures benefit from phonetic blending because the player *learns* the creature through play and lore. Items are **consumed on the fly** — there is no learning curve for an item name. The player sees it in their inventory and must decide *now* whether to use it.

**Therefore: if a portmanteau creates ambiguity about the item's function, use the straight noun+noun form instead.**

**Bad example:** `Cryptobite` — is it food? A tool? A currency? Ambiguous.
**Good example:** `Decryption Key` — unambiguous. It's a key for decryption.

---

## 4. Verification Checklist (Must-Pass All Three)

| Criteria | Question to Ask | Pass / Fail |
|---|---|---|
| **Functional Readability** | Can a player guess the item's purpose from the name alone, without reading the description? | ☐ Yes / ☐ No |
| **Tech Anchor Present** | Is the infosec pun / tech term clearly identifiable in the name? | ☐ Yes / ☐ No |
| **No Ambiguity** | Does the name avoid being a generic noun (`Snack`, `Drive`, `Chip`) without a distinguishing tech modifier? | ☐ Yes / ☐ No |

---

## 5. Approved Examples (Benchmarks)

| Name | Pattern | Why It Passes |
|---|---|---|
| **Decryption Key** | Tech + Object | Immediate clarity. "Key for decryption" is unambiguous. |
| **Backup Drive** | Tech + Object | "Drive used for backup" — function is the first word. |
| **Air-Gapped Snack** | Modifier + Object | "Air-gapped" modifies "snack" — the tech concept is front and center. |
| **Sinkhole Trap** | Tech + Object | "Trap that sinkholes" — clear function. |
| **Tor-Tilla Chip** | Portmanteau + Object | "Tor" is clearly visible in "Tor-Tilla"; "chip" grounds it as an edible item. |
| **Yubi-Cookie** | Portmanteau + Object | "Yubi" is clearly visible; "cookie" grounds it. The tech pun is preserved. |

---

## 6. Anti-Patterns (What to Avoid)

| Anti-Pattern | Example | Why It Fails |
|---|---|---|
| **Vanished Tech** | "Snack", "Cookie" | The tech term is gone entirely. No pun survives. |
| **Pure Object** | "Drive", "Key" | Generic noun with no tech modifier. Could be anything. |
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
