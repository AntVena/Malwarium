// pcap_naming.h — filename derivation for the Audit.pcap capture.
//
// PURE + HOST-TESTABLE. Turns a captured network's identity into a bounded,
// filesystem-safe .pcap filename. Extracted so a capture session is named after
// the NETWORK it belongs to (a stable, re-derivable name) rather than a
// sequential session counter — the counter scheme minted one file per arm/seal
// radio cycle regardless of whether anything was actually captured that cycle,
// which is what filled the SD card (see net_capture.h's session lifecycle for
// the fix that stops re-deriving the same name on every rearm).
//
// The promiscuous EAPOL-only capture path (net_capture.h) only sees DATA frames
// (WIFI_PROMIS_FILTER_MASK_DATA) — it never hears the management/beacon frames
// that would carry an SSID. The SSID is instead supplied by the passive scan
// (net_sniff.h → SsidCache), which does see it; net_capture.h looks it up when
// naming a session and falls back to the BSSID form when the network was never
// scanned (or is hidden).
#pragma once

#include <cstddef>
#include <cstdint>

namespace mal {

// Sanitizes `raw` into `out` (bounded, NUL-terminated): keeps [A-Za-z0-9_-],
// maps every other byte (spaces, punctuation, control bytes) to '_', and stops
// after `maxLen` output characters or `outCap - 1`, whichever is smaller. A
// null or empty `raw` writes an empty string. Returns the number of characters
// written (excluding the NUL) so a caller can tell an empty result apart from a
// sanitized one.
size_t sanitizeForFilename(const char* raw, char* out, size_t outCap, size_t maxLen);

// Builds a bounded, NUL-terminated pcap path: "<prefix><name>.pcap". When `ssid`
// sanitizes to something non-empty, `name` is that SSID suffixed with "_" + the
// last three BSSID octets in hex (e.g. "HomeWiFi_6736b6") so two APs sharing an
// SSID never collide onto one file; otherwise `name` is the full BSSID lowercase
// hex (12 chars, no separators) — always available, so naming never falls through
// to a counter. `prefix` may be null (treated as empty). Truncates rather than
// overflows `out`.
void buildPcapFilename(char* out, size_t outCap, const char* prefix,
                        const uint8_t bssid[6], const char* ssid);

}  // namespace mal
