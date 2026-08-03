# Malwarium — Test Strategy

Defines the three test tiers and the two release gates every screen/sprite must clear. Built on
the stack in `docs/ORIENTATION.md`.

---

## The three tiers

| Tier | Command / tool | Covers | Hardware |
|---|---|---|---|
| **Native unit** | `pio test -e native` / `ctest` (dual-mode via `PIO_UNIT_TESTING`, same `test/test_native/test_main.cpp`) | Pure game logic — stat decay, care-budget transitions, hatch sequence, state machines | None (runs on PC) |
| **Host preview** | CMake + SDL2, `./build/malwarium_host` | Game logic *and* the render pipeline on the PC — layout/rendering verifiable without hardware. The grayscale gate runs in-process against the rendered framebuffer (`test_grayscale_gate`); `tools/dump_frame` writes PPM panels for eyeballing/baselines. | None |
| **On-device** | Claude Code over USB | Drivers, ~4fps timing, SD_MMC, audio/IMU/radio | 1 board (2 for radio) |

**Simulation (Wokwi)** is deferred — no `WOKWI_CLI_TOKEN`/`wokwi-cli` in the build env and no true
ST7789 240×240 Wokwi part; the host tier's grayscale/repaint gates stand in for layout/render
acceptance, and the real board is the timing authority. Recipe + rationale: `sim/README.md`.

**The serial test-hook** (debug builds injecting synthetic `INPUT_NEXT/ACCEPT/CANCEL` tokens and
dumping current screen/state over serial, so navigation is assertable without physical buttons) is
still **unbuilt** — see `docs/MASTER_TODO.md §1f`.

---

## Two release gates (apply to *every* screen)

These are not optional polish — a screen is not "done" until both pass.

1. **Grayscale readability.** A grayscale screenshot of the screen must stay fully readable:
   every status meaning is dual-coded (gauge fill level, pip count + gate divider, the Critical
   ~1Hz pulse), so colour is never the only carrier. Automate it: desaturate the rendered PNG and
   assert the key elements are still distinguishable (`assets/VISUAL_LANGUAGE.md`,
   `PAL_CORE.json` rules).
2. **Legible before upscale.** The screen must read at **128-logical** size *before* the ×1.75
   upscale — if it isn't legible pre-scale, no scaling saves it. Render the logical framebuffer
   and eyeball/diff it, not just the 224×224 output.

For creature sprites, add the sprite gates from `assets/CREATURE_VISUAL_RULES.md §5`:
**grayscale** (distinct value steps, not one flat blob), **cover-the-eyes** (body still reads as
a solid object), **silhouette** (one clear idea at a glance).

---

## Notes / limits

- **Wokwi simulates MCU + display + buttons only** — not IMU, audio/I2S, SD_MMC, battery ADC,
  Wi-Fi, or ESP-NOW. It's authoritative for UI/layout/logic; timing and the hardware layer must
  be confirmed on a real board.
- Balance constants (decay rates, countdown durations, sequence lengths) are named `config.h`/
  `tunables.h` constants so they can be tuned without code changes — test against the constants,
  not hard-coded numbers.
