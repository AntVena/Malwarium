# Creature Visual Rules — stage evolution + shading law

The standard every pet sprite is drawn to across the four stages. Gold standards:
**`SPR_PET_PAYPUP`** and **Malbear** — every new creature is held to their bar, and the shipped
`assets/sprites/SPR_PET_PAYPUP.png` and `assets/sprites/SPR_PET_MALBEAR.png` are the reference.

---

## 0. The four-stage read

Each stage **iterates the same concept**, never restarts it. The pet grows because the player
helped it.

| Stage | Role | Read |
|---|---|---|
| **1 · Boot Sector (Egg)** | A *hint* of the malware type | Sealed, you only sense what's inside (CryptoShell = a vault). |
| **2 · Process (Pet)** | The concept *embodied as a pet* | Cute, round, open. The idea made adorable. |
| **3 · Script (Now)** | *Showing off newly-earned potential* | Same concept, more resolved + more capable; starts hinting where it goes. |
| **4 · Daemon (Monster)** | The *fully-realised virus, tamed* | The payoff: scale + the back-pocket move held in reserve. Where fur / full plating pay off. |

**Cute → threatening arc:** Process = *cute* · Script = *cocky / cool (the turn)* · Daemon =
*threatening (the arrival)*. Script is the turn, not the destination.

---

## 1. The one principle (evolution read)

> **Same skeleton, more resolved.**

A Script keeps its Process ancestor's exact silhouette landmarks — head-to-body ratio, the
signature feature, the ear shape — but baby-fat softness firms into **defined planes** and the
features gain **intent**. Same creature, a year older and surer of itself. **Not** a new animal,
**not** just an inflated one. *If you can't see the cub in it, it's wrong.*

- **DNA is loose-ish:** one Script can sit above several Process cousins as a shared path.
- **One strong holistic idea per creature** — never a parts-list. (Bolting on sharp eyes +
  fangs + spikes produces a parts-list, not a creature. Banned.)

---

## 2. Power through posture, not parts

Script can't lean on size (only a hair larger than Process). "More capable" is carried by **how
it holds itself**:

| Lever | Process | Script |
|---|---|---|
| Centre of gravity | High, bouncy, light contact — almost a head with legs | Drops low, weight settles forward; a clear body the head sits ON |
| Gaze | Wide, open, eager | Half-lidded, knowing — "doesn't need to try" |
| Silhouette mass | One soft round blob | Body / head split, shoulder bulk — adolescent weight, not added pixels |

---

## 3. The shading law (the Paypup standard)

**Light wraps the form; it never stacks in rows.** Every tone is decided by the surface *facing
the light*, not by its height on the sprite.

1. **One light, fixed for the whole roster: top, slightly left.**
2. **Shade by surface normal** — highlight pools on the single highest rounded mass; core shadow
   on the turn-away and **under every overhang**; the silhouette has a clear top / front / under.
3. **Up to 4 tones per local colour** for the bigger Script masses, plus an optional 1px
   specular tick. (Process can sit at 3.)
4. **Cast shadows** where one form overhangs another (brow over eyes, head onto body, chin under
   jaw). A 1px contact shadow seats the feet.
5. **The acceptance test:** cover the eyes — the body must still read as a solid object. And the
   **grayscale test** (§5).

**The failure mode (Cryptoad v1):** four flat horizontal strips of lightening green — no
topography for light to describe, so it flattens and reads *less* evolved despite being bigger.

---

## 4. Colour, anatomy, texture

**A species has a mother colour, and every creature on that line wears it.** Ransomware is green,
Phishing is blue. It is the line's signature, not any one creature's — so two creatures being the
same hue tells you they share a line, and nothing else. A Good/Bad branch pair is the same colour
because it is one line, not because a branch is supposed to match.

**The range inside that colour widens as the stage advances.** An early creature can be carried by
a few shades; a Daemon needs more of them to hold the extra plating and texture without going
muddy. So "more shades of the mother colour" is what growth looks like, not a shift to a new hue.

**A Trojan wears the colour of the line it diverted from**, since it is pretending to be one —
right hue, one small "wrong" tell. That makes a Trojan's brief a re-skin of its origin line rather
than a new silhouette, and it follows from the rule above rather than being a separate one.

**This is a rule of thumb, not a gate.** Accents — metallic parts, eyes, a tell — sit outside it
freely, and plenty of shipped sprites already break it that way. Nothing enforces it and nothing
should: the enforced test is §5's grayscale read, and if flash or RAM ever demanded it, drawing
every sprite in greyscale and colouring it through a per-line mapping at draw time would be a
perfectly good reason to drop the rule entirely.

- **Colour shift within the line:** deep, slightly-more-saturated base reads "advanced" — **but
  keep the highlight tone bright** so the lighter shades still live on lit crowns and plates.
  Don't lock the light palette away.
- **Back-pocket anatomy:** **exactly one** dramatic new piece per creature, hinting the Daemon
  (Malbear's dorsal crest + shoulder plates; the Daemon gets the full plating). Smaller bits
  (claws, beaks) are free but must read **"becoming," not "deadly."** Hold the real escalation
  for Daemon.
- **Texture (fur / scales):** an **overlay pass on top of** the sculpted form, never a
  replacement for it — so it never flattens. Deploy **only where the concept demands it**;
  default is the smooth, hard-rounded finish (reads a touch robotic while staying animal). Fur is
  a finishing touch, not a requirement — and it gives the Daemon something to escalate into.

---

## 5. Acceptance tests (must pass)

- **Grayscale test (the hard one):** strip the colour — silhouette, brow-shadow, value steps and
  posture must still carry the read. If it only works in colour, it fails. **A creature must NOT
  collapse into one flat untextured blob in B&W** — adjacent masses need distinct value steps and
  a core-shadow seam between them.
- **Cover-the-eyes test:** the body alone still reads as a solid, sculpted object.
- **Silhouette test:** the black silhouette reads as *one clear idea* in a single glance.

---

## 6. Per-creature checklist (hold to the Paypup / Malbear bar)

1. One strong silhouette idea — what is it in a single glance?
2. Cub DNA visible — you can see the Process ancestor grown up.
3. One top-left light; 3–4 value steps that **wrap** the form.
4. Clear head / body split; masses read as distinct, not melted into one blob.
5. Glow-eye read under a defining brow.
6. Exactly one back-pocket idea, restrained.
7. Passes grayscale + cover-the-eyes + silhouette.

---

## 7. State of the roster

- **LOCKED:** Malbear (Sulk × Edge, Smooth finish; deep base + acid-bright crown; dorsal crest +
  shoulder plates; idle = a slow shoulder-roll "huff"). Gold standard alongside Paypup.
- **APPROVED back/shell:** Keyloggerhead (keycap shell + blinking cursor-cap).
- **NEEDS REDRAW (too blobby / fail grayscale):** Keyloggerhead front, FireWallaby, Brute Badger,
  Cryptoad re-sculpt. Concepts are right; execution must hit the Paypup/Malbear bar.
- **Engine cell:** 56×48 logical (Script), 96×64 (Daemon e.g. Cryptoad). ×1.75 to panel, no
  downscaling.

---

## 8. The redraw brief (refinement pass)

> **Malwarium — redraw pass: hit the Paypup/Malbear bar, kill the blob.**
>
> Read first: this file. Gold standards: `assets/sprites/SPR_PET_PAYPUP.png` and
> `assets/sprites/SPR_PET_MALBEAR.png`. Locked — don't touch them.
>
> **The problem to fix:** the latest roster sculpts (Cryptoad, FireWallaby, Brute Badger,
> Keyloggerhead front) have gone **blobby** — in black-and-white they collapse into one flat,
> untextured shape. Paypup and Malbear don't: they have a few **decisive planes** and a clear
> internal value structure that survives grayscale.
>
> **Key takeaways to apply:**
> 1. **Fewer, more decisive masses.** Stop fusing everything into one soft lump. Each major mass
>    (head, shoulder, haunch, shell) keeps its own crown highlight and a **core-shadow seam**
>    where it meets the next, so they read as distinct forms — not a melted blob.
> 2. **Grayscale-first.** Block each creature in 3–4 greys and make it read *before* adding hue.
>    If the B&W is mush, the colour won't save it. Ship a side-by-side colour/grayscale of every
>    redraw.
> 3. **Define key edges.** A 1px darker accent where two forms overlap, a crisp lit edge on the
>    lead plane. Paypup's visor brow is the model — one hard, deliberate shadow edge.
> 4. **Hold the one-idea + cub-DNA discipline** (§1) and the per-creature checklist (§6). Don't
>    re-litigate concepts — Cryptoad stays the greedy crypto-vault toad, etc.
> 5. **Posture over parts; one back-pocket piece; texture only where earned.**
>
> **Do:** redraw Cryptoad, FireWallaby, Brute Badger and Keyloggerhead's front to the gold-
> standard bar, each with a grayscale proof. Keep them as single frames. Don't mass-produce
> animation yet. When the four pass the grayscale test, lock them and start frame sets.
