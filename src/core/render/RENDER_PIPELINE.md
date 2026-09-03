# Render pipeline — logical canvas, pass order, effect budget

What every visual effect in `src/core/render/` and `game_render*.cpp` composes against. Read this
before adding an effect; the pass-order table below is the registry, and the rule at the bottom
says a new effect must join it.

## Logical canvas vs. physical panel

The engine decouples logical resolution from the panel. Art is authored at logical size and
upscaled to fit; there is **no real-time downscaling**.

| Panel | Aspect | Scale | Active canvas | Bezel buffer per edge |
|---|---|---|---|---|
| 128×64 | rect | ×1.0 | 128×64 | 0px |
| 128×128 | square | ×1.0 | 128×128 | 0px |
| **240×240** *(primary)* | square | ×1.75 | 224×224 | 8px |
| 320×240 | rect | ×1.75 | 224×112 | 48px / 64px |

Assets are authored at **128×128** (square) or **128×64** (rect) logical px; a pet sprite's max
bounding box is **128×64**. Upscaling is whole-number or clean fractional only — `CANVAS_SCALE_NUM`
/ `CANVAS_SCALE_DEN` in `include/config.h` express ×1.75 as 7/4 so it stays whole-number safe. The
8px buffer on the 240×240 target guards against bezel clipping on cheap display modules.

### Sprite scale is per-blit, and the combat stage uses that

The table above is the **canvas** scale, and it does not move. What a given *sprite* is drawn at is
a separate number: `drawSpriteUpscaled` and its siblings (`core/render/sprite.h`) take their own
`num`/`den`, and every screen but one hands them the canvas's.

The **combat stage is the one exception, and it draws its two fighters at 1/1** — one panel pixel
per authored pixel (`CombatStage`, `core/ui/combat_screen.h`). Two content-full Daemon cells want
336 of the 220px the stage has at ×1.75, so at the canvas scale one of them is cut off at a screen
edge; at 1/1 the same pair wants 192 and both stand whole, with the room left over spent on the
clash lane and on the lunge and recoil that carry a blow.

This is **not** downscaling in the sense the rule above forbids. Nothing is resampled: 1/1 is the
artist's own grid drawn untouched, exactly as ×1.75 is a lossless 2,2,2,1 expansion of it. What it
does mean is that a creature is drawn **smaller in a fight than in its habitat**, which is a
deliberate composition choice — the stage holds two creatures and room to move between them, where
the habitat holds one. The stage's scale is one constant, so a creature's drawn size never depends
on who it is fighting.

## Cadence

This is a retro vpet, not a real-time game: a **~4fps animation heartbeat (250ms/frame)** with
**event-driven repaint**. The screen is static by default and redraws only when an animation frame
advances or game state changes — there is no high-framerate render loop. Glitch effects may tick
faster (~6–8fps) *only while active*. This keeps the CPU mostly idle, makes SPI bandwidth a
non-issue, and is battery-friendly.

Effects are CPU framebuffer passes at the 128×128 logical canvas — the ESP32-S3 has no GPU and no
shader hardware. Passes stack over an offscreen `LGFX_Sprite`, which is then integer-upscaled and
DMA-blitted to the panel.

## Canonical pass order

```
1. BACKGROUND   clear / room backdrop (engine-drawn: `scene.h`)
2. SPRITE_MODS  pet-effects that sit UNDER the creature: FX_ABSORB's incoming blocks,
                FX_SHRED's sliding scanlines
3. SPRITE_BASE  current pet animation frame, in whatever colours it is currently wearing:
                FX_CAMO's borrowed palette, with the hit flash composed over it. A subject
                with no sheet is drawn here too rather than in a pass of its own —
                FX_SWARM is an area guardian's whole body (`render/swarm.h`)
4. SPRITE_MODS  overlay pet-effects (e.g. the Worm line's Replication Ghost)
5. CORRUPTION   frag-driven: channel-shift -> scanline-tear -> dropped-px -> glitch-blocks
6. SCREEN_FX    full-screen overlays: evolution flash, Lockout band, critical-failure crash, fades
   (then UPSCALE + DMA blit to panel)
```

A BACKGROUND is drawn rather than blitted, and `scene.h` is both the primitives and the
reason: a backdrop that sits behind text cannot hold a colour opinion, because `PAL_CORE`
is the one palette and a theme moves the interface by moving tokens. So a scene is painted
from one ramp between `paper` and a named token — dark by construction, unable to out-shout
`ink`, and free of flash. `sceneTone` is that ramp on `ink-dim`, which is what a place
reading in pure value uses; `sceneTint` is the same ramp anchored elsewhere, and it refuses
`accent` and the status hues outright, since a backdrop wearing a colour that already means
something is lying about the screen.

It is composed against a **SceneGround**: a horizon and a floor, both supplied by the
SCREEN, since the screen is what knows where its text stops and where its sprite's feet go.
The floors in play are 44 rows apart — `kCombatSpriteShelf` and `kLivingBottom` — so
**nothing in a scene is an absolute row**. Below the horizon a figure is an offset in rows;
above it, a fraction of whatever sky the ground leaves (`sceneSkyY`), because that is the
band whose size actually changes between screens.

A PLACE is one file under `render/scenes/` — art-direction tables and a handful of
primitive calls — named by a `SceneId` (`render/scene_id.h`) and reached through the
catalogue in `render/scenes.h`. Three kinds of owner name one, which is why the id cannot
hang off `AreaDef` alone:

- an **area**, on `AreaDef::scene`, exactly as it names its sector glyph;
- a **creature**, DERIVED rather than stored (`content/content_homes.h`) — a line with a
  place of its own overrides how its creatures move, and how they move is the floor, so
  every creature is somewhere and an evolution walks into a new place with nothing
  written down;
- the **operator**, who owns some of them and picks one (`content/content_backgrounds.h`,
  CFG → DEVICE → BACKGROUND). Ownership is derived from what the save already records —
  a creature raised, an area cleared, brackets taken at the arena, an achievement earned
  — so nothing about it is written down twice.

Two screens ask. `Game::habitatScene()` is where the pet lives: the operator's pick, or
their pet's own home when the pick is AUTO. `Game::stageScene()` is where a fight is
happening — the area being walked, or the habitat when the fight belongs to no area (a
duel, an arcade bout, the endless dive). **The area outranks the pick on purpose:** a
background is an opinion about home, an area is a fact about where the walk is, and a
screen that let a prize overwrite it could no longer tell you where you are. Both may
answer `SceneId::None`, which draws nothing and leaves the plain `paper` field.

One consequence for GATES: a screen is never blank now, so a check that asserted a region
was `paper` in order to say "no icon here" has to be made against a frame differing only
in the thing under test (`regionDiffers`, `test/test_native/test_gates.h`). `tools/dump_frame.cpp`'s `scene:<name>
floor:<row>` renders one on its own at either floor, which is the only way to see whether
a place actually reads — the native gate can hold portability, contrast and the anchor
rail, and nothing can hold composition.

SPRITE_MODS straddles SPRITE_BASE because a pet-effect's side of the creature is part of
what it means: a Replication Ghost is a copy standing in front, and a block streaming into
the pet has to pass BEHIND it or the creature reads as wearing its dinner rather than
eating it. An effect names which side it takes when it joins the table.

An effect that recolours the creature is IN SPRITE_BASE rather than either side of it —
there is one frame on the canvas, drawn in other colours, not a second thing layered over
a first. `FX_CAMO` is the creature's colour while it wears it, so the impact flash is
applied over the recoloured pixels rather than in place of them: the pet flashes in the
borrowed palette. A hit must stay legible on a camouflaged fighter, and a camouflaged
fighter must not appear to be undressed by being hit.

## Composition rules

Each pass takes a 0–1 intensity driven by a stat (corruption = f(Fragmentation)); intensity 0
early-outs and costs nothing. There is **no hard cap** on simultaneous effects — passes are
sub-millisecond — but a **shared distortion budget** soft-clamps combined glitch so overlapping
effects never turn to mush. Modal events (evolution, Lockout) take over the screen and suspend the
idle pipeline, so the alarming combinations are gated by game state rather than special-cased.

STAT-screen gauges are **chrome, not passes** — the Critical ~1Hz pulse is a UI repaint, distinct
from the procedural `FX_CORRUPTION` sprite glitch. Combat hit/clash FX are likewise activity
visuals, not passes.

### Orientation

A sheet declares which way its drawing is turned (`SpriteData::facing`, `sprite.h`; the table is
`FACING` in `tools/gen_assets.py`), and a screen that has an opinion about where a creature should
be looking asks for a mirror rather than for a second sheet. Mirroring is not a pass and costs
nothing: every blitter walks its destination unchanged and sources its column through
`spriteSrcX`, so a scatter keyed to a screen position never learns that the sprite turned round.

One consequence worth holding on to: **seating is measured mirrored** — `spriteContentX0` takes
the flag, because a drawing padded to one side of its cell moves that padding's width when it
turns. Which sheets are worth declaring at all is `assets/CREATURE_VISUAL_RULES.md` §2.

## The maintenance rule

No new visual effect ships without (a) a row in the table above naming its stage **and** the
stat/state that drives its intensity, and (b) an `FX_*` row in `assets/ASSET_MANIFEST.md §D` so
Design knows to skip it. An effect that isn't in the table has no defined composition order with
the ones that are.

An effect also has to name **what it is a function of**, and answer one question first:
is it a MOMENT or a STATE? A moment decays off a beat — that is what the combat screen's
`hitBeat` is, and it is right for a punch. A state is a standing value that holds until
the thing it describes changes, and it must not be built out of a beat at all.

Getting that backwards is not a tuning problem, it is a lie: `FX_CAMO` says which line the
pet is currently channelling, so a version of it that ran out on a timer described a swing
that had already finished, and one derived from `hitBeat` — which names the most recent
strike by *either* fighter and is reassigned to the other seat the moment the rival hits
back — was stripped off the pet by the commonest thing that can happen next. It is now a
level (`CombatCamo`, `camoAdvance`) eased toward a live reading of the pet's own last cast,
with no clock in it anywhere. `FX_ABSORB`/`FX_SHRED` are genuinely moments and keep their
own beat (`CombatOutro::beat`) — their own, because the strike clock is not theirs either.

`FX_SWARM` is the one effect that is a SUBJECT rather than something applied to one, and it
answers the moment-or-state question the same way `FX_CAMO` does: it is a **state**. What it
is a function of is a guardian's DISPOSITION — `FlockMood` (`core/model/flock.h`), set from
where the SHIBBOLETH has got to and how it came out — and it holds until the meeting moves
on. There is no beat in it anywhere.

Its motion is not derived, which makes it the exception to the rule the other passes keep,
and the exception is deliberate: a flock's frame is the last frame steered, and that
integration is the whole of why it reads as alive rather than as a field of sine waves. So
the flock is a MODEL (`core/model/flock.h`, beside `IdleWander`) advanced one step at a
time by the tick, and only the DRAW is a pure function — of the `SwarmView` handed to it,
which is what still lets a frame gate dump any step by stepping the model to it first. It
runs on the fast FX clock (`kFxAnimMs`) for the reason a dissolve does: at 4fps a mark
moving a pixel a step reads as a blink rather than as flight.

`FX_CAMO` has **two colour sources and two drivers**, which is the shape an effect takes
when a second screen wants it. The palette is either sampled off a creature (`camoRampFrom`,
the fight and the idle habitat) or built from one PAL_CORE token (`camoRampFromTone`, a
screen that means a specific colour and has no creature to sample). The level is either
`camoAdvance` eased toward a live predicate (the fight reads the pet's cast; the habitat's
resting drift reads a long cadence, `core/model/idle_camo.h`) or a caller's own settled
fraction — the CHROMATOPHORE hands it `Chromatophore::wearPct`, so the scatter on the
creature is the same number the board is about to score. Both remain STATES; neither source
or driver is allowed to be a beat, and the habitat's cadence is not an exception: it decides
WHEN the pet is wearing something, never how far along the scatter is.

WHICH creature the fight samples is `CamoTarget` (`core/ui/combat_screen.h`), ranked so the
more specific answer wins. A rolled move sitting in the rival's kit — or belonging to the
rival's line — makes the pet a copy of the fighter opposite, accent and all. A move from a
line the rival has nothing to do with makes the pet that LINE, sampled off the line's own
creature at the wearer's stage, which is the answer in every wild encounter: a malbeast
belongs to no line and fields only generic rows, so a rule that needed the fighter opposite
to be holding the move would leave the pet bare through the whole single-player game. The
target is resolved in the tick and turned into tones at the draw (`Game::camoRampForTarget`),
because ranking a sprite's colours is work the repaint already does once.

The sampled source is really the derived one **with the real colours laid over it**: the
ladder is built whole from the sprite's main colour first, then its actual tones are
placed on the rungs their own luminance names, commonest first. A ramp is therefore
always complete — the property the remap cannot do without — while still wearing as much
of the other creature as that creature actually has. The two failure modes this replaced
are worth naming, because both looked like damage rather than disguise: a palette
stretched over the whole value range flattens the pet into a silhouette, and one that
fills only part of the ladder flecks a borrowed tone over its shadows and leaves the
rest. A 1-bit opponent is the case that shows it — one white ink, so the pet keeps every
bit of its own shading and simply loses its colour.

Changing FROM one borrowed palette to another is the same pass with the old ramp passed as
`from`: un-flipped pixels wear the palette being left rather than the creature's own. A
fight uses it for the swap between two borrowed palettes and leaves it null at either end,
where the pet really is coming out of, or returning to, itself.

Two effects that recolour the same creature **compose or rank, and say which.** `FX_CAMO`
is what colour the pet *is*, so the impact flash blends over it; drawn as alternatives,
every hit taken read as cancelling the disguise.
