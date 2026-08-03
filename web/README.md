# Web 'Pedia — static bundle

The SD-served encyclopedia site. Self-contained: no CDN, no build step, no framework.
Open `index.html` from disk and it runs on the sample fixture.

## Files

| File | What |
|---|---|
| `index.html` | the one page (hash routing — the ESP32 serves a single HTML file) |
| `style.css` | terminal-dark-wiki style system; every colour binds to a PAL_CORE token |
| `app.js` | hydration + views (vanilla JS) |
| `data/pedia_data.js` | **GENERATED** content pack — run `tools/gen_pedia_data.py` after editing the firmware content tables |
| `fixtures/pedia_state.js` | sample per-player state (every reveal state represented) |
| `assets/` | **SYNCED** copies of the repo's canonical PNGs — `tools/gen_pedia_data.py` copies in exactly what the generated data references, so this never drifts from `/assets` |
| `fonts/` | *(you add)* `PixelOperatorMono.ttf` — then re-add the `url('fonts/PixelOperatorMono.ttf')` src line noted in style.css's @font-face; until then a system-mono fallback stack applies |

## Engineering integration

1. Serve this folder from SD at `/pedia`.
2. Serve live state at `GET /pedia_state.json` (shape: `docs/ORIENTATION.md`; keys are
   content ids, e.g. `"paypup": "hatched"`). The site paints first, then probes it from
   any `http(s)` origin — it cannot key off the device IP, because the AP's wildcard DNS
   means the phone may arrive under any hostname. Off-device the probe 404s and the
   fixture stays. The fixture is a DESK convenience and is deliberately not staged onto
   the card, so on-device the site must survive it being absent — hence the empty-state
   floor in `app.js`.
   Extensions used by the site: `active_pet`, `malbeasts`, `mods`, `archive`
   (documented inline in `fixtures/pedia_state.js`).
3. Handle the two POSTs (both fail gracefully offline):
   - `POST /api/tag` `{"tag":"A-Z0-9_ ≤12"}` — HackerTag rename (the site's one write).
   - `POST /api/achievement/DEVTOOLS_INTRUDER` — the honeytoken callback (§03 arch).

## Keeping data in sync

Content tables reach the site through the FIRMWARE'S OWN code: `tools/dump_content.cpp`
links the real tables and prints them as JSON, and `tools/gen_pedia_data.py` consumes
that (it still regexes `combat.cpp` for the wild roster, which is a function body rather
than a table). Descriptions are templates the device expands from each row's magnitudes
(`core/content/effect_text.h`), so running the device's own expander is what stops the
site's numbers from disagreeing with the panel's — and each entry carries a `stats`
field, the derived readout of every magnitude the row hands the engine.

```
make pedia      # builds tools/dump_content, regenerates the data, syncs assets
```

That one command also **copies every asset the data references into `assets/`** — an art
path present in the data is, by construction, present in the bundle. Pass
`--no-sync-assets` to write the data file alone.

Roster lore (hints/context) is read straight off each `CreatureDef` row — a creature whose
row has no hint emits a TODO + warning instead of blocking.

Sprite sheets are grids of frames, and each entry carries `cellW`/`cellH` (one frame — what
the site shows) plus `sheetW`/`sheetH` (the whole image). The frame grid comes from
`tools/gen_assets.py`'s `frame_width`/`frame_rows`, the same functions that cut sheets for
the firmware, so the site and the device agree on where a frame ends.

## Design rules baked in

- Reveal states: `locked` (lock card, no text) · `seen` (frag-ramp glitch
  silhouette + hint, name/context masked, stage shown / line masked — D-P2) ·
  `hatched`/`unlocked` (full record). Wild malbeasts: seen on encounter,
  full on first win (D-P3). Evolution arrows respect target reveal state (D-P1).
- Green-lead terminal (`calm`), cyan `accent` strictly links/focus (D-P4).
- Dual-coding: every state = glyph + fill treatment, never colour alone —
  the site passes a grayscale screenshot.
- CRT: 1px/3px scanlines + edge vignette, derived tints, `pointer-events:none`,
  removed at print. Content survives with the effect off.
