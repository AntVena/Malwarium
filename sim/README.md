# Wokwi simulation tier — recipe + why it is deferred

> **The Wokwi *screenshot* tier is DEFERRED.** The Simulation tier's **native-unit row is
> delivered** (`pio test -e native`, the same cases the gates run through ctest). The Wokwi
> PNG-regression row is not, and the **host-tier render + grayscale gates stand in as the
> acceptance proxy** for layout/rendering; the **real board is the timing authority**
> (`docs/TEST_STRATEGY.md` — Wokwi timing is not cycle-accurate). Rationale below.

## Why deferred (not "couldn't be bothered")

1. **No `WOKWI_CLI_TOKEN` / no `wokwi-cli` in the build environment.** Headless Wokwi
   needs a token from a (free) wokwi.com account; neither the token nor the CLI binary
   is available here, so the tier cannot be run or its baselines captured.
2. **No true ST7789 240×240 part in Wokwi.** Wokwi's SPI-TFT part is `wokwi-ili9341`
   (240×**320**). Our panel is ST7789 240×240 driven by LovyanGFX. A baseline captured
   against a wrong-resolution, different-controller emulation would be of uncertain
   fidelity (LovyanGFX's ST7789 init vs. Wokwi's ILI9341 command emulation is unverified).
3. **The host tier already does more.** `pio test`/ctest run the *real* compositor and
   assert grayscale readability + event-driven repaint on **every** screen in-process —
   strictly more than a PNG diff would give, and without the resolution mismatch. Wokwi's
   unique value (rough MCU timing) is explicitly non-authoritative; the device covers it.

Net: the marginal value of the Wokwi screenshot tier over `host gates + on-device smoke`
is low, and the two blockers above make it un-runnable here regardless.

## To stand it up later

1. **Install + auth:**
   ```sh
   curl -L https://wokwi.com/ci/install.sh | sh   # installs wokwi-cli
   export WOKWI_CLI_TOKEN=...                      # from https://wokwi.com (account → CI token)
   ```
2. **Build the firmware:** `pio run -e wokwi_sim` (→ `.pio/build/wokwi_sim/firmware.{bin,elf}`;
   `wokwi.toml` at repo root already points here).
3. **Author `diagram.json`** (intentionally NOT committed — it must be validated live in the
   Wokwi UI first, because of the ST7789 caveat above). Build it in the Wokwi editor with:
   - **Board:** `board-esp32-s3-devkitc-1`
   - **Display:** the closest SPI-TFT part Wokwi offers (today `wokwi-ili9341`; switch to a
     native ST7789 part if/when Wokwi ships one). Confirm LovyanGFX actually paints before
     trusting any baseline.
   - **3 push-buttons** (active-low to GND, internal pull-ups) on the verified pins from
     `include/config.h`:

     | Role | GPIO | Button |
     |------|------|--------|
     | A / NEXT   | 0 | leftmost (also BOOT strap) |
     | B / ACCEPT | 5 | middle |
     | C / CANCEL | 4 | rightmost |

   - **Display SPI pins** (also `include/config.h`): SCK 38, MOSI 39, DC 45, RST 40, CS 21,
     backlight 46 (plain GPIO HIGH, no PWM). MISO unused (-1).
4. **Scenario:** drive the carousel + a couple of submenus via scripted button presses and
   capture screenshots; diff against committed baselines. See `sim/scenario.example.yaml`
   for the press-sequence shape (mirrors `dump_frame`'s screen list).
5. **Wire into CI** as the `Simulation` tier alongside `pio test -e native`.

## Optional but recommended: the serial test-hook

`TEST_STRATEGY.md` calls for a debug-build **serial test-hook** (inject INPUT_NEXT/ACCEPT/
CANCEL tokens; dump current screen/state, optionally a per-region framebuffer CRC). It is
**not built yet.** It would let Wokwi scenarios *assert state* (not just diff pixels) and is
the same mechanism the on-device automated re-smoke would use. Scope it together with whichever
of {Wokwi, device automation} gets stood up first — it underpins both.
