# Board reference — Waveshare ESP32-S3-LCD-1.54

The silicon and peripherals this platform tier drives. **`include/config.h` is the pin authority** —
it holds the verified pinout (bring-up result, committed, not a per-machine template) with a note
per pin on how it was confirmed and what it is wired into. Don't copy pin numbers out of here or
anywhere else; read them there.

## Primary target

| Component | Specification |
|---|---|
| Board | Waveshare ESP32-S3-LCD-1.54 (SKU 33866/33867), or Touch variant (33868/33869) |
| MCU | ESP32-S3R8 — Xtensa LX7 dual-core, up to 240MHz |
| RAM | 512KB SRAM + 8MB PSRAM |
| Flash | 16MB NOR |
| Display | 1.54" 240×240, 262K colours, ST7789, 4-wire SPI |
| Inputs | 3 physical buttons — KEY_MINUS (also the BOOT/download strap), KEY_PWR, KEY_PLUS |
| Storage | onboard TF (MicroSD) slot — **SD_MMC 4-bit SDIO, not SPI** |
| Power | 3.7V LiPo via MX1.25 2-pin; onboard charge/discharge management |
| Wireless | 2.4GHz Wi-Fi 802.11 b/g/n + Bluetooth 5 LE, integrated antenna |
| Audio | ES8311 codec + NS4150B amp + speaker; ES7210 ADC + dual mics |
| IMU | QMI8658 — 3-axis accel + 3-axis gyro |
| Touch | CST816 capacitive over I2C (Touch variant only) |
| Debug | USB Type-C, I2C pad, UART pad |

Two board behaviours bite during bring-up and are worth knowing before you read `config.h`:

- **The power latch.** On battery the board powers itself through a P-MOSFET latch. Pressing PWR
  closes it long enough for the CPU to boot; firmware must then drive `PIN_POWER_HOLD` HIGH to
  *hold* it. Miss that and the rail collapses when the button is released — the panel flashes for
  under a tenth of a second and dies. USB is immune (VBUS feeds VSYS directly).
- **The BOOT button is a game button.** KEY_MINUS doubles as the download-mode strap, so holding it
  at reset means "flash me", not "press A".

## Porting

All board-specific values live in `include/config.h`, which carries pre-filled blocks per board;
the engine and web layer stay board-agnostic. Porting = editing that one file. `tools/pin_finder.ino`
discovers which GPIO an unlabelled button is wired to on a new board, and `main.cpp`'s
`BRINGUP_PINSCAN` build flag runs the same scan in-firmware.

`waveshare_s3_154` is the only board block `config.h` carries, and the normal build cycle is that
env plus the native gates.

## What the simulator can't tell you

Wokwi covers the MCU, panel and buttons only — not the IMU, audio/I2S, SD_MMC, battery ADC, Wi-Fi
AP, or ESP-NOW. Anything in the peripheral list above, and the ~4fps timing budget, is confirmed on
a real board or not at all.
