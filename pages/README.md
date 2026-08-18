# `pages/` — the publish host's own pages

The static pages served alongside the artifacts on GitHub Pages: a landing page at `/`, a
browser flasher at `/flash/`, and a playable demo of the device at `/play/`. `make pages`
copies this directory into `dist/`, which is what `.github/workflows/publish.yml` uploads, so
the site and the artifacts deploy as one thing and can never be a release apart.

Nothing here is downloaded by a device. `web/` is the bundle that ships to an SD card and
versions with the firmware; this is read by a person, in a browser, on a computer.

```
pages/
├── index.html · home.js   the landing page — what this is, and which of the two update paths to use
├── style.css              the shared look (see below)
├── fonts/                 PixelOperatorMono8.ttf — FONT_UI's own cut (CC0), copied from assets/fonts/
├── flash/                 the USB flasher: index.html is the operator's instructions, flash.js drives it
├── play/                  the browser demo — the shell around the wasm engine (see below)
└── vendor/esptool-js/     Espressif's loader, vendored — its README carries the version and digest
```

## The demo at `/play/`

`play/index.html` + `play.css` + `play.js` are the chrome; the game itself is the firmware
engine compiled to WebAssembly by `tools/build_web.sh`, which writes `malwarium.js` and
`malwarium.wasm` into this directory. Those two are **build output and not committed** — `make
pages` rebuilds them (the `demo` target), so the demo can never be a release behind the
firmware published beside it. Building needs the Emscripten SDK on PATH; the script says so and
stops if it is missing.

Nothing is re-implemented for the browser: `src/platform/web/` is a platform layer beside
`src/platform/esp32/` and `src/platform/host/`, supplying the same `IDisplay`/`ISaveStore`
seams against a canvas and localStorage. What the demo hides, and why, is
`include/demo_config.h`.

## Why a flasher exists at all

An over-the-air update writes into a spare app slot and nothing else. It structurally cannot
replace the **bootloader** or the **partition table** — the bootloader reads the table, and an
OTA only ever writes into a slot the table already defines (`partitions_malwarium.csv` says
this at length). So a release that changes either one reaches a device that has already shipped
only over USB, and until this page existed that meant installing PlatformIO.

`flash/flash.js` writes all four boot images at their fixed ESP32-S3 addresses, which is both
the fix for that and the recovery path for a device that no longer boots at all. The
instructions tell the operator to hold **A** while connecting, because A is GPIO0 — the
download-mode strap (`include/config.h`) — and a device held in download mode is reachable even
when the firmware on it is the reason it isn't.

## What it reads

`manifest.json`, the same file the device fetches on CFG → UPDATES. Both pages take the
firmware's **filename** from it and resolve it against their own origin rather than following
the absolute URL inside — the manifest is built for one publish host, and these pages are
served from whichever host is actually holding them. That is what makes a fork's Pages site
flash the fork's own firmware with no edit.

`make pages` also stages `bootloader.bin`, `partitions.bin` and `boot_app0.bin` into `dist/`
under exactly those names, which is the other half of the contract: the manifest names the app
image, and the three boot images are named by convention because they don't move with a release.

## Style

`style.css` is the 'Pedia's terminal-dark look, and binds every colour to a **PAL_CORE role
token** (`assets/PAL_CORE.json`) or a tint documented as derived from one — the same rule
`web/style.css` follows, and the same reason: one palette, one place it is defined. Status is
dual-coded (a glyph and a word, never colour alone), so the pages read in grayscale like the
device's screens have to.

It is a separate file from `web/style.css` on purpose. That one is downloaded by a device and
versioned with the bundle; this one never leaves the browser. They share a palette, not a file.

## Trying it locally

```bash
make pages BASE=http://localhost:8000
(cd dist && python3 -m http.server 8000)
```

The landing page and the flasher both work off `dist/`, so this serves the real thing. Web
Serial needs a secure context, which `localhost` counts as — so the flash button works here too,
against a board on the end of a real cable.
