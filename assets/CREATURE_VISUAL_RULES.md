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
| Gaze | Unguarded — it hasn't learned to hold anything back | Half-lidded, knowing — "doesn't need to try" |
| Silhouette mass | One soft round blob | Body / head split, shoulder bulk — adolescent weight, not added pixels |

**A creature TURNS TOWARD the viewer; it does not stand in flat profile.** The standing pose is
three-quarter: the body reads across the cell so its length and build are legible, but it is rotated
far enough that some of the chest shows and the head is turned out with BOTH eyes on the viewer. Flat
profile loses the face, which is where every line keeps its signature — the dog branch's dark eye
box, a cat's twin heads — and a creature that never looks at you never engages you. A true side view
is not wasted work, though: it is the pose a WALK cycle wants, so a profile drawing belongs in a
clip row rather than in the idle the habitat rests on.

**The gaze row is a DIRECTION, not an eye shape.** What has to read is that a Script's eyes have
settled relative to the Process form standing next to it — the contrast between the two stages is
the whole content of the row. A creature whose concept is demure starts further along that axis and
its Script leans harder into it; a cat can be half-lidded at Process and still read as cute, and
forcing wide eyes onto it to satisfy the table buys nothing the silhouette and posture were not
already carrying. Wide-and-eager is one way to draw *unguarded*, not the requirement.

### Which way it faces, and who decides

A drawing with a side to it — a profile walk row, a fish, any body that reads across its cell —
**declares that side**, as `Facing::Left` or `Facing::Right` in `tools/gen_assets.py`'s `FACING`
table. It rides on the SHEET rather than on a creature row because a wild malbeast is built from a
sprite-named spec and has no creature row at all (`makeEnemyCombatant`,
`src/core/model/combat_factory.cpp`), and the reader is `SpriteData::facing`
(`src/core/render/sprite.h`).

**Draw it whichever way it wants to be drawn.** Nothing here asks for a canonical direction, and
the table is not a to-do list of sprites to redraw: a screen that seats a creature against an
opponent mirrors it into the seat (`drawFighter`, `src/core/ui/combat_screen.cpp` — the local pet
looks right, its rival left), and the idle habitat turns it to the way its wander is actually
travelling. What the table buys is that a creature is never shown its opponent's back.

**Most rows declare nothing, and that is correct.** §2's turned-to-the-viewer standing pose has no
side to mirror, so `Facing::None` is the default and is never turned — Paypup, Malbear, Pingcub and
the whole Metamorphic octopus branch all sit there. Declare a side only when the drawing has one.
Two things make a row a bad candidate even when it does: a readable asymmetric detail that must not
come out backwards, and anything small enough that a facing does not read at all (the 16×8 replica
glyphs).

---

## 3. The shading law (the Paypup standard)

**Light wraps the form; it never stacks in rows.** Every tone is decided by the surface *facing
the light*, not by its height on the sprite.

1. **One light: top, slightly left.**
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

**A line may spend a mother STYLE instead of a mother colour**, and the Worm is the one that does
— see below. What the rule actually asks for is a signature the eye can carry from one creature to
the next; a hue is the cheapest way to buy one, not the only way.

**The range inside that colour widens as the stage advances.** An early creature can be carried by
a few shades; a Daemon needs more of them to hold the extra plating and texture without going
muddy. So "more shades of the mother colour" is what growth looks like, not a shift to a new hue.

**Metamorphic is warm coral-red**, and it is the line where the rule above stops being a matter
of taste. `FX_CAMO` (`src/core/render/camo.h`) builds a ladder out of whatever creature this line
is copying, then repaints each of the wearer's pixels with the rung its OWN luminance lands on —
so the number of luminance bands a Metamorphic sheet occupies is a hard cap on how many borrowed
tones can ever show on it. A creature carrying most of its body on three rungs shows three tones
of an eight-tone palette however richly the thing it is copying was drawn. Widening this line's
range is therefore not polish; it is the difference between the signature ability working and
not. `kCamoRampMax` is 8, which is the number to draw against.

**The ladder saturates, and on this line that is correct.** Eight is the ceiling, so every stage
past the egg is drawn to reach it: a Process form wearing seven borrowed tones where it could wear
eight is giving up camouflage to keep a tidy count, and the count is not what the player sees. The
Boot form is the one deliberate exception — it keeps a single dark and no cool shadow, because a
sealed shell shows nothing of what is inside.

**What widens with the stage is what the rungs are SPENT ON.** Process is smooth-skinned and takes
no specular (§3 already caps it lower). Script is modelled as the bundle of tubes it is, and only
hints the tell. Daemon adds the specular crown and wears the tell at full strength. Once the count
is full the stage reads through vocabulary — texture, specular, how much tell — which is a better
carrier for §0's arc than a number was.

**The tell is violet, and it owns a rung nothing else does.** Blue-ringed-octopus warning rings —
an annulus, with the creature's own skin showing through the middle and its own skin DARKENED
around the outside, never a coloured outline. They stay legible THROUGH a disguise for one reason:
every pixel is still repainted, but the rung the rings sit on carries no body mass, so they always
come back as a tone the rest of the creature is not wearing. That is why the effect needs no
exemption and `camo.h`'s "no index, no palette metadata, no per-creature table" stands. The ring's
two violets share that one rung on purpose — it gains a shade in its own colours without spending
a second camouflage slot. Keep sprite violet desaturated and clear of `PAL_CORE`'s fragmentation
pair (frag-lo `#7a3df0`, frag-hi `#ff5cb8`); the vocabularies must not collide.

**"Owns a rung" is arithmetic, so a new tone on this line has to be placed rather than picked.**
`camoRampFrom` bins by luminance into `kCamoRampMax` bands of 32, so the tell's two violets (144
and 158) are both band 4, 128-159. Any tone added to a Metamorphic sheet that lands in that band
puts body mass on the rings' rung and the disguise stops giving them back — which is the one
failure the paragraph above exists to prevent. So compute the luminance before choosing the hue.
Two tones landing in the SAME band is the other half of the same arithmetic and costs something
different: `#ffe6cc` (234) and `#e9ff8f` (235) are both band 7, one apart, which is why the eye
accent and the top highlight are indistinguishable to a luminance-weighted snap — see the round
trip notes in [ASSET_MANIFEST.md](ASSET_MANIFEST.md) §C for what that does to a resampled sheet.

**Tentaclone carries teal bioluminescence, and it is a BRANCH mark rather than the line's.**
`#3cd6c8` at luminance 166 is band 5, clear of the tell's band and clear of both the eye's
yellow-green and the rings' violet, so the creature's three signals stay separable: rings on the
hood, eyes on the face, a glowing stripe down the centre of every hanging arm with coral showing
along both its edges. It sits on top of the mother colour the way the Ransomware cat branch's
second head does, so it says Bad branch and not Metamorphic. Whether Syncaelia answers it with a
mark of its own is open, and is a question for whenever that sheet is next drawn.

**A Trojan wears the colour of the line it diverted from**, since it is pretending to be one —
right hue, one small "wrong" tell. That makes a Trojan's brief a re-skin of its origin line rather
than a new silhouette, and it follows from the rule above rather than being a separate one.

Read it as *the line's signature*, not literally as its hue, and the Worm case falls out on its
own: USBasilisk and Coaxeel diverted out of the line that spends a STYLE, so they wear the style —
1-bit, outlined, segmented, drawn by `tools/gen_worm_art.py` alongside the two Daemons they were
substituted for. Their "wrong" tell is where the one solid mass goes. Every Worm row spends it on a
face (an eye, or Rootgrub's throat); these two spend it on a **contact** — the tongue inside a
type-A plug, the bare conductor past a cut jacket.

**A tell that small is not enough on its own, and the second half of it is GEOMETRY.** The solid
mass is two pixels; a player deciding at a glance whether the thing on the shelf is the Daemon they
were raising never gets to it. What separates these two at that glance is that a Trojan out of this
line carries a shape no grown body has — USBasilisk's whole head is the USB trident, square terminal
above and round below and a type-A plug where the logo puts its arrow, held rigid and axis-aligned
while the body under it sways; Coaxeel's rungs LEAN, in one repeated lay rather than perpendicular,
which is a braid and not a segmentation, and its jacket ends in a square cut with the core standing
out of it. Right angles and repeated straight lays are the disguise slipping. The rest of the line
is round, organic and jointed, and that contrast is the read the two pixels only confirm.

**A BRANCH may carry a signature of its own on top of the line's.** Ransomware's cat branch —
Conkittenate, Kalico, and the Pwnther/Breecheetah pair — is **two-headed**, every row of it, because
the branch is named for concatenation and two things stuck together is the joke the whole chain
inherits. It sits on top of the mother colour rather than replacing it: a cat is green like every
other ransomware creature, and two-headed like no other line. Draw the second head as a full head on
the same neck, not a mirrored decal — the silhouette is the read, so it has to survive the §5
grayscale test on shape alone. The dog branch of the same line has one head, which is the point:
the signature separates branches inside a line that colour cannot, since colour is the line's.

### The Worm exception: draw it small, and draw it 1-bit

**A Worm creature reads SMALLER in its cell than any other line's creature at the same stage** —
noticeably so, not by a few pixels. It is the one line whose sprite budget is not its own: a worm
in combat is a parent plus up to `kWormReplicaSlots` replicas (`content_passives.h`), and every
one of them is drawn on the same shelf. The room the replicas stand in has to come from the
parent, so the parent gives it up by design.

This does not soften the stage arc (§0) — a Worm Daemon still reads as the payoff against a Worm
Process. It is scaled against its own line, not against the roster. **A worm grows by getting
HEAVIER, not by getting bigger:** Rootgrub is barely wider in its cell than Nodeatode and reads as
a whole stage more, because the mass moved — thin and long became short and thick, and a head
became a mouth. That is the move this line has instead of the extra cell room every other line
spends on its Script. And it is why the line's
back-pocket idea (§4) is best spent on something the SWARM reads at a glance rather than on
detail the shrunken parent cannot carry.

**The Worm has no mother colour. Its signature is 1-bit line art plus copies**, which is the same
exception read from the other end: the line already puts more things on the screen than any other,
so it pays for them by asking each one to carry less. A worm is an outline with a solid eye — every
row of the family, from the Vermicell shell down, and the shared replica glyphs beside them
(`SPR_WORM_REPLICA_ATTACK` / `_DEFEND`) drawn in exactly the same vocabulary. The parent and its
copies are one drawing style at two sizes, not creature art with UI standing next to it, which is
what makes a crowd of them read as one family rather than as clutter.

At worm scale the outline also simply holds more shape than a filled silhouette does: a Nodeatode
is roughly 30x24 px of its 56x48 cell, and at that size the segment chords across the body are what
say *worm* rather than *tube*. §5's grayscale test is free for this line — there was never a colour
channel carrying meaning to lose.

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
- **Silhouette test:** the black silhouette reads as *one clear idea* in a single glance — and
  **run it against the rest of the line, not on its own.** A row can pass in isolation and still
  be a failure, because the question a player actually asks is *which creature is this*, and a
  silhouette that matches its cousin's answers nothing. Fill every row of a line black, put them
  side by side, and any two that share an envelope are a bug in whichever one moved last.

  What makes this bite is that **an interior feature contributes nothing at all to a silhouette.**
  Rootgrub and Threadbore are both "a tube with a maw", and the maw is drawn *inside* the head —
  so filled in black they were the same kidney bean and the only thing separating a Script from
  the Daemon it grows into was a pair of wings. Nodeatode and Coaxeel were both thin squiggles,
  three stages apart. The fix is never more detail; it is a different **envelope**: Rootgrub's
  mouth became a flat-rimmed bell so the mouth is in the outline, Threadbore's head became a wide
  slab, Coaxeel became a coil — the one shape in the line with a hole in it — and Shenloop became
  a doubling-back S with a spade tail. Six rows, six envelopes, and the tool that draws them has a
  `--preview` for exactly this.

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
- **APPROVED 1-bit:** Vermicell, Nodeatode, Rootgrub, Shenloop and Threadbore — the whole Worm
  line, its style-instead-of-hue exception above — plus the two Trojan Daemons it diverts into,
  USBasilisk and Coaxeel. Held to the outline + segment-chords + one solid mass vocabulary, not to
  §3's shading law.
  That vocabulary is `tools/gen_worm_art.py`, which is where a new creature in this style is
  drawn: the style is mechanical enough to be code, and a recipe over it cannot express a second
  ink or a filled silhouette. Read the tool before drawing any of these by hand.
  The two Worm Daemons are what §2's *posture, not parts* looks like when a line has no cell room
  left to spend: Shenloop is a tapered body doubled back through an S with its head carried level
  off the neck, and a dorsal ridge was tried and cut for being exactly the parts-list §1 bans. Its
  sibling takes the opposite lever — width, a bigger maw, and one fewer segment chord than the row
  below it, because widely spaced rungs leave unbroken panels of flank and the panels are what
  carry mass.
  The two Trojan rows are held to the same vocabulary and then made to disagree with it. Each is
  built on ONE manufactured idea rather than a body plus an ornament: USBasilisk is a reared column
  whose head IS the USB glyph — the fork's two horn terminals double as the crown a basilisk is named
  for, so there is no separate plate on the neck, and a cobra hood spread behind the head was tried
  and cut for unioning with the neck into one amoeba with no step in the silhouette to read as a
  flare.
  Coaxeel is a coil with a helical lay instead of segments and a square-cut jacket at the far end;
  a graded three-step taper down to the conductor was tried and cut for reading as a shrimp's
  rostrum, since on a curled body anything that narrows toward a point is a tail. One hard band at
  full jacket width, then nothing but a thin rod, is what says the jacket came OFF.
  **Two reads had to be bought back after the fact, and both were the same mistake.** Rootgrub and
  Threadbore's maw is spokes around a solid mass, which is also the diagram of an EYE, and at this
  size the eye wins — both shipped reading as a fuzzy eyeball. The fix is a *lip*: a two-pixel
  rim, the heaviest ink in the cell, drawn inside the silhouette with the teeth rooted on it and
  the whole mouth set forward on the head so there is cheek behind it. Shenloop shipped reading as
  a caterpillar because posture was asked to carry the species on its own — a levelled head does
  say *waiting*, but a round bulb with an eye in it is a Nodeatode head, and the viewer names the
  creature off the head. It needed the head to become a head: a skull with a squared muzzle
  projecting off it, a backswept horn pair as its one back-pocket piece, and asymmetric barbels.
  The lesson for the next row here is that posture decides what a creature is *doing* and the head
  decides what it *is*, and no amount of the first will supply the second.
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
