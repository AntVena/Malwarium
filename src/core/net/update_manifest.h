// update_manifest.h — the published list of downloadable artifacts, parsed.
//
// PURE + HOST-TESTABLE, like eapol.h/pcap.h/network_ledger.h — no radio, no HTTP,
// no Arduino. Hand it the bytes of a fetched manifest and it hands back a small
// fixed list of artifacts; fetching them is the device tier's job.
//
// A LIST, not a firmware entry. The SD-hosted web 'Pedia bundle goes stale
// independently of the firmware, so "is there an update" is a per-artifact
// question — each row carries its own version, so the check can offer one, both,
// or neither. Artifacts are matched by `id` ("firmware", "web"), never by
// position, so publishing a new kind of artifact can't shift the meaning of the
// existing ones on a device that predates it.
//
// WHAT THE HASH IS AND ISN'T. Each row carries a SHA-256, and there is
// deliberately NO code signing. That means the hash protects against a corrupted
// or truncated DOWNLOAD — the realistic failure on a flaky home network, and the
// one that would otherwise brick a device mid-flash. It does NOT protect against
// a hostile network: the manifest travels the same unauthenticated channel as the
// artifacts, so anyone who can rewrite one can rewrite both. That is an accepted
// trade here, not an oversight.
//
// The parser is strict on purpose. This input arrives over somebody else's
// network, where a captive portal answering with an HTML login page instead of
// the manifest is an ordinary Tuesday — so anything that isn't the expected shape
// is rejected outright rather than half-read into a plausible-looking artifact.
#pragma once

#include <cstddef>
#include <cstdint>

namespace mal {

// One downloadable thing. Fixed-size fields: this is parsed on a device where a
// heap allocation per manifest row buys nothing.
struct UpdateArtifact {
    char id[16] = {0};        // "firmware" | "web" — what the row IS
    char version[16] = {0};   // display string ("0.4.2"), shown in the prompt
    uint32_t code = 0;        // packed monotonic int; THIS is what gets compared
    char url[160] = {0};      // absolute, so moving hosts is a manifest edit
    uint32_t size = 0;        // bytes, so the device can refuse before it starts
    uint8_t sha256[32] = {0}; // integrity of the download (see the banner)
};

enum class ManifestParse : uint8_t {
    Ok,
    Malformed,    // not JSON, or not the shape we publish (portal page, truncation)
    NoArtifacts,  // valid JSON, empty list — nothing is published yet
    TooMany,      // more rows than kMaxArtifacts; the ones that fit are usable
};

class UpdateManifest {
public:
    // Four is two more than we publish. A device that meets a manifest from a
    // later firmware keeps the rows it understands rather than failing the check.
    static constexpr int kMaxArtifacts = 4;

    // Parse `len` bytes at `json` (need not be NUL-terminated). Any row missing a
    // field, carrying a malformed SHA-256, or claiming a zero size is REJECTED —
    // without signing the hash is the only integrity check there is, so a row that
    // can't be verified is not installable and must never reach the caller.
    // Leaves the previous contents cleared regardless of outcome.
    ManifestParse parse(const char* json, size_t len);

    int count() const { return count_; }
    const UpdateArtifact* artifact(int i) const {
        return (i >= 0 && i < count_) ? &items_[i] : nullptr;
    }
    // Look a row up by what it is. The only way callers should reach an artifact.
    const UpdateArtifact* byId(const char* id) const;

private:
    UpdateArtifact items_[kMaxArtifacts];
    int count_ = 0;
};

// Whether `a` is worth offering, given what's installed. One comparison point, so
// the firmware check and the web-bundle check can't drift apart — and so "newer"
// always means strictly newer (re-offering an equal build is how an update prompt
// becomes noise the operator learns to dismiss).
bool artifactIsNewer(const UpdateArtifact& a, uint32_t installedCode);

}  // namespace mal
