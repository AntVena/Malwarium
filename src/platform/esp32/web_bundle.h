// web_bundle.h — which web 'Pedia bundle is on the card.
//
// The SD /web folder the AP serves (ap_server.h) is stamped by a marker file
// sitting beside it, so an update check can tell "a bundle is published" apart
// from "yours is older than it". This is the device tier of that stamp: the file
// I/O against the SD mount (sd_esp32.h mounts it at /sdcard). The format, and the
// reasoning behind it, live in core/net/version_marker.h.
//
// TWO WRITERS, ONE SHAPE. The on-device installer writes the marker for the bundle
// it just downloaded; the repo's `make pedia-sd` target copies the committed
// web/VERSION onto a card populated by hand. Both lay down the same two lines, so
// a hand-staged card and an OTA-updated one are indistinguishable at check time.
//
// Plain POSIX stdio, exactly like ApServer's file streaming — no card, no folder
// or no marker all land as "unknown" (code 0), which is the safe direction.
#pragma once

#include <cstdio>

#include "core/net/version_marker.h"

namespace mal {

// Where the stamp lives: inside the served bundle, so staging or replacing the
// folder carries its own version with it rather than leaving a stale one behind.
constexpr const char* kWebBundleMarkerPath = "/sdcard/web/VERSION";

// Read the installed bundle's stamp. False (and a zeroed `out`) when the card,
// the bundle or the marker is absent — the caller compares `out->code` either way.
inline bool readWebBundleVersion(VersionMarker* out,
                                 const char* path = kWebBundleMarkerPath) {
    if (!out) return false;
    *out = VersionMarker{};
    FILE* f = std::fopen(path, "rb");
    if (!f) return false;
    char buf[kVersionMarkerCap * 4];   // room for the comment lines a staged card carries
    const size_t n = std::fread(buf, 1, sizeof(buf), f);
    std::fclose(f);
    return parseVersionMarker(buf, n, out);
}

// Stamp the bundle currently on the card. Call this only AFTER the bundle's files
// are in place: the marker is the claim that they are, and a stamp written ahead
// of the install would make a failed one look complete on the next check.
inline bool writeWebBundleVersion(const VersionMarker& m,
                                  const char* path = kWebBundleMarkerPath) {
    char buf[kVersionMarkerCap];
    const size_t n = formatVersionMarker(m, buf, sizeof(buf));
    if (n == 0) return false;
    FILE* f = std::fopen(path, "wb");
    if (!f) return false;
    const size_t w = std::fwrite(buf, 1, n, f);
    std::fclose(f);
    return w == n;
}

}  // namespace mal
