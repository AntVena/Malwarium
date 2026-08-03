// version_marker.h — the "which build is this?" stamp that ships beside an
// installed file bundle.
//
// PURE + HOST-TESTABLE, like update_manifest.h — no filesystem, no radio. Hand it
// the bytes of a marker file and it hands back the two facts an update check
// needs; reading and writing the file is the device tier's job
// (platform/esp32/web_bundle.h).
//
// WHY IT EXISTS. Firmware knows its own version because the number is compiled in
// (include/version.h). The SD-hosted web 'Pedia bundle has no such luxury — the
// files on the card are just files, so without a stamp beside them a check can
// only say "a bundle is published", never "yours is older than it". The marker is
// that stamp: an installer writes it next to the bundle it just laid down, and the
// next check reads it back and compares with artifactIsNewer (update_manifest.h).
//
// FORMAT — one `key=value` per line, `#` starts a comment line:
//
//     code=402
//     version=0.4.2
//
// `code` is the packed monotonic integer and the ONLY field ever compared.
// `version` is the display string, carried alongside so a prompt can say
// "0.4.2 -> 0.5.0" instead of "402 -> 500". Both are copied verbatim from the
// UpdateArtifact row that was installed, so the marker describes exactly what the
// device believes it put on the card.
//
// Unknown keys are IGNORED, so a later publish can add a field without stranding
// a device that predates it.
//
// A MISSING OR UNREADABLE MARKER IS NOT AN ERROR — it reads as code 0, which makes
// any published bundle newer, which offers a re-install. That is the safe
// direction: the worst outcome is re-downloading a bundle the card may already
// have, and the install replaces the unknown stamp with a known one. Note this is
// the OPPOSITE stance to update_manifest.h's deliberately strict parse — there a
// half-read row could be installed and must be rejected; here a half-read marker
// can only cost a redundant download.
#pragma once

#include <cstddef>
#include <cstdint>

namespace mal {

// What is installed. `version` is sized to match UpdateArtifact::version, since
// every marker is written from one.
struct VersionMarker {
    uint32_t code = 0;
    char version[16] = {0};
};

// Bytes a formatted marker can occupy, terminator included — callers size their
// write buffers from this rather than guessing.
constexpr size_t kVersionMarkerCap = 64;

// Parse `len` bytes at `text` (need not be NUL-terminated). Tolerates CRLF, blank
// lines, surrounding whitespace, comments and unknown keys. Returns true only when
// a `code` was read: everything else is decoration, and a marker without one says
// nothing about what is installed. `out` is cleared first either way, so a false
// return still leaves a usable code-0 marker.
bool parseVersionMarker(const char* text, size_t len, VersionMarker* out);

// Render `m` into `out`, NUL-terminated. Returns the length written, or 0 if it
// would not fit (leaving an empty string rather than a half-written marker — a
// truncated stamp would read back as a different, plausible version).
size_t formatVersionMarker(const VersionMarker& m, char* out, size_t cap);

}  // namespace mal
