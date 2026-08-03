# esptool-js — vendored

Espressif's browser port of `esptool.py`. It speaks the ESP32 ROM/stub loader protocol over
Web Serial, which is the whole of what `../../flash/flash.js` needs and none of which is
worth reimplementing.

| | |
|---|---|
| Upstream | <https://github.com/espressif/esptool-js> |
| Version | **0.6.0** |
| File | `bundle.js` — the published `bundle.js`, byte-for-byte |
| SHA-256 | `7c361337d5bba7271cb0d9741f165a3b87137ff9284c13f112a6e197c48cd0da` |
| Licence | Apache-2.0 — `LICENSE`, which stays with the file |

`bundle.js` is a single self-contained ES module: it carries pako and the per-chip flasher
stubs inside it, and exports `ESPLoader` and `Transport` (among others). That is why the
bundle is vendored rather than the `lib/` tree — one file, one `<script type="module">`, no
resolver and no build step.

## Why vendored rather than a CDN

The rest of this project's web surface loads nothing from a third-party host, and the flasher
has the strongest claim of all of them: it is the page an operator reaches when the device is
already unreachable, and a CDN outage would be one more way for that to fail. Vendoring also
keeps the published bytes reviewable — the digest above is what the page executes.

## Updating it

```bash
npm pack esptool-js@<version>
tar xzf esptool-js-<version>.tgz
cp package/bundle.js package/LICENSE pages/vendor/esptool-js/
```

Then update the version and digest above, and re-read `../../flash/flash.js` against the
release notes: it calls `main()`, `writeFlash()` and `after()`, and `writeFlash`'s `fileArray`
takes `Uint8Array` data (it took binary strings before 0.5).
