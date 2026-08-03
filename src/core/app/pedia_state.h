// pedia_state.h — builds the web 'Pedia state JSON payload from
// LIVE Game state. Portable (host + device, no ESP32 dependency): the AP server
// (src/platform/esp32/ap_server.h) calls this to answer GET /pedia_state.json, and
// the native gates call it directly so the shape is verifiable without a device.
//
// Shape target: web/fixtures/pedia_state.js (the design-QA sample fixture the
// front end falls back to when there's no live device). This is a v1/first-cut
// builder — several blocks are stubbed pending tracking that doesn't exist yet
// (see the `// TODO(pedia-v2):` comments in pedia_state.cpp).
#pragma once

#include <string>

namespace mal {

class Game;

// Build the full 'Pedia JSON payload for the CURRENT state of `g`. Hand-rolled
// JSON (no library dependency) — every string field is escaped, so free text
// (the HackerTag) is always safe to embed.
std::string buildPediaStateJson(const Game& g);

} // namespace mal
