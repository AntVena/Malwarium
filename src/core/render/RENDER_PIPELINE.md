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
1. BACKGROUND   clear / room backdrop
2. SPRITE_MODS  pet-effects that sit UNDER the creature: FX_ABSORB's incoming blocks,
                FX_SHRED's sliding scanlines
3. SPRITE_BASE  current pet animation frame
4. SPRITE_MODS  overlay pet-effects (e.g. the Worm line's Replication Ghost)
5. CORRUPTION   frag-driven: channel-shift -> scanline-tear -> dropped-px -> glitch-blocks
6. SCREEN_FX    full-screen overlays: evolution flash, Lockout band, critical-failure crash, fades
   (then UPSCALE + DMA blit to panel)
```

SPRITE_MODS straddles SPRITE_BASE because a pet-effect's side of the creature is part of
what it means: a Replication Ghost is a copy standing in front, and a block streaming into
the pet has to pass BEHIND it or the creature reads as wearing its dinner rather than
eating it. An effect names which side it takes when it joins the table.

## Composition rules

Each pass takes a 0–1 intensity driven by a stat (corruption = f(Fragmentation)); intensity 0
early-outs and costs nothing. There is **no hard cap** on simultaneous effects — passes are
sub-millisecond — but a **shared distortion budget** soft-clamps combined glitch so overlapping
effects never turn to mush. Modal events (evolution, Lockout) take over the screen and suspend the
idle pipeline, so the alarming combinations are gated by game state rather than special-cased.

STAT-screen gauges are **chrome, not passes** — the Critical ~1Hz pulse is a UI repaint, distinct
from the procedural `FX_CORRUPTION` sprite glitch. Combat hit/clash FX are likewise activity
visuals, not passes.

## The maintenance rule

No new visual effect ships without (a) a row in the table above naming its stage **and** the
stat/state that drives its intensity, and (b) an `FX_*` row in `assets/ASSET_MANIFEST.md §D` so
Design knows to skip it. An effect that isn't in the table has no defined composition order with
the ones that are.
