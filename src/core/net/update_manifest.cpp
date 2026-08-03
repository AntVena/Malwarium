#include "core/net/update_manifest.h"

#include <cstring>

namespace mal {

namespace {

// A cursor over the manifest bytes. Every read is bounds-checked against `end`,
// because the input may be truncated mid-download — that is a normal outcome on a
// flaky network, not a corrupt-input edge case.
struct Scan {
    const char* p;
    const char* end;

    bool done() const { return p >= end; }
    char peek() const { return done() ? '\0' : *p; }

    void skipWs() {
        while (!done() && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) ++p;
    }
    // Consume `c` if it's next (after whitespace). The whole parser is built from
    // this, so a missing brace or comma fails where it happens rather than later.
    bool eat(char c) {
        skipWs();
        if (done() || *p != c) return false;
        ++p;
        return true;
    }
};

// Consume a JSON string. `out` may be null to discard it. Handles the three
// escapes our own fields can legitimately contain and REJECTS every other one —
// an unknown escape means this isn't our manifest, and guessing at it is how a
// login page gets read as an artifact.
//
// Overflowing `cap` is a FAILURE, never a truncation. A URL cut short at the
// buffer's edge would still look like a perfectly good row, and would then fetch
// the wrong thing (or something else entirely) instead of failing the check.
bool readString(Scan& s, char* out, size_t cap) {
    if (!s.eat('"')) return false;
    size_t n = 0;
    while (!s.done()) {
        const char c = *s.p++;
        if (c == '"') {
            if (out) out[n] = '\0';
            return true;
        }
        char ch = c;
        if (c == '\\') {
            if (s.done()) return false;
            const char e = *s.p++;
            if (e != '"' && e != '\\' && e != '/') return false;
            ch = e;
        }
        if (out) {
            if (n >= cap - 1) return false;   // doesn't fit -> reject the row
            out[n] = ch;
        }
        ++n;
    }
    return false;   // ran off the end inside a string — truncated input
}

// Consume a string we don't need to keep. Length-agnostic on purpose: skipping a
// field this firmware doesn't know about must never fail just because the value
// is long, or a later publish adding a field would strand older devices.
bool skipString(Scan& s) { return readString(s, nullptr, 0); }

// Read an object KEY. Unlike a value, an over-long key is not an error — it just
// can't be one of ours (they're all short), so it's kept but marked, and the
// caller routes it to skipValue like any other unknown field. Failing here
// instead would mean a later publish adding a long field name strands every
// device already in the field, which is the exact opposite of what a manifest is
// for. The truncation marker matters: without it a long key sharing our first N
// characters would be mistaken for the real thing.
bool readKey(Scan& s, char* out, size_t cap) {
    if (!s.eat('"')) return false;
    size_t n = 0;
    bool tooLong = false;
    while (!s.done()) {
        const char c = *s.p++;
        if (c == '"') {
            out[n] = '\0';
            if (tooLong) out[0] = '\0';   // can't match any key we know
            return true;
        }
        char ch = c;
        if (c == '\\') {
            if (s.done()) return false;
            const char e = *s.p++;
            if (e != '"' && e != '\\' && e != '/') return false;
            ch = e;
        }
        if (n >= cap - 1) tooLong = true;
        else out[n++] = ch;
    }
    return false;
}

// Read a non-negative JSON integer. Rejects an empty run of digits and anything
// that would overflow, rather than wrapping into a small plausible number.
bool readUint(Scan& s, uint32_t* out) {
    s.skipWs();
    const char* start = s.p;
    uint64_t v = 0;
    while (!s.done() && *s.p >= '0' && *s.p <= '9') {
        v = v * 10 + static_cast<uint64_t>(*s.p - '0');
        if (v > 0xFFFFFFFFull) return false;
        ++s.p;
    }
    if (s.p == start) return false;
    *out = static_cast<uint32_t>(v);
    return true;
}

int hexNibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// Exactly 64 hex characters into 32 bytes. Length is part of the contract: a
// short hash is a broken publish, and accepting it would silently weaken the only
// integrity check the download has.
bool readSha256(Scan& s, uint8_t out[32]) {
    char hex[80];
    if (!readString(s, hex, sizeof(hex))) return false;
    if (std::strlen(hex) != 64) return false;
    for (int i = 0; i < 32; ++i) {
        const int hi = hexNibble(hex[i * 2]);
        const int lo = hexNibble(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0) return false;
        out[i] = static_cast<uint8_t>((hi << 4) | lo);
    }
    return true;
}

// Skip one JSON value of any type, so a row carrying a field this firmware
// doesn't know about is still readable. Forward compatibility is the point: a
// later publish must be able to add a field without stranding older devices.
bool skipValue(Scan& s) {
    s.skipWs();
    if (s.done()) return false;
    const char c = s.peek();
    if (c == '"') return skipString(s);
    if (c == '{' || c == '[') {
        const char open = c, close = (c == '{') ? '}' : ']';
        int depth = 0;
        while (!s.done()) {
            const char d = *s.p;
            // Strings are skipped wholesale so a brace inside one can't unbalance
            // the depth count.
            if (d == '"') { if (!skipString(s)) return false; continue; }
            ++s.p;
            if (d == open) ++depth;
            else if (d == close && --depth == 0) return true;
        }
        return false;
    }
    // A bare literal (number, true/false/null) — run to the next structural char.
    while (!s.done() && *s.p != ',' && *s.p != '}' && *s.p != ']') ++s.p;
    return true;
}

// One artifact object. Every field this firmware needs must be present and sane;
// see the header on why a row that can't be verified is dropped rather than kept.
bool readArtifact(Scan& s, UpdateArtifact* a) {
    if (!s.eat('{')) return false;
    bool haveCode = false, haveSize = false, haveSha = false;

    if (s.eat('}')) return false;   // an empty object is not a usable row
    for (;;) {
        char key[24];
        if (!readKey(s, key, sizeof(key))) return false;
        if (!s.eat(':')) return false;

        if (std::strcmp(key, "id") == 0) {
            if (!readString(s, a->id, sizeof(a->id))) return false;
        } else if (std::strcmp(key, "version") == 0) {
            if (!readString(s, a->version, sizeof(a->version))) return false;
        } else if (std::strcmp(key, "url") == 0) {
            if (!readString(s, a->url, sizeof(a->url))) return false;
        } else if (std::strcmp(key, "code") == 0) {
            if (!readUint(s, &a->code)) return false;
            haveCode = true;
        } else if (std::strcmp(key, "size") == 0) {
            if (!readUint(s, &a->size)) return false;
            haveSize = true;
        } else if (std::strcmp(key, "sha256") == 0) {
            if (!readSha256(s, a->sha256)) return false;
            haveSha = true;
        } else if (!skipValue(s)) {
            return false;           // unknown field, unreadable value
        }

        if (s.eat(',')) continue;
        if (s.eat('}')) break;
        return false;
    }

    // A row is only usable if it says what it is, where it lives, how big it is,
    // how to verify it, and which version it claims to be.
    return a->id[0] != '\0' && a->url[0] != '\0' && a->version[0] != '\0' &&
           haveCode && haveSize && haveSha && a->size > 0;
}

}  // namespace

ManifestParse UpdateManifest::parse(const char* json, size_t len) {
    count_ = 0;
    for (auto& a : items_) a = UpdateArtifact{};
    if (!json || len == 0) return ManifestParse::Malformed;

    Scan s{json, json + len};
    if (!s.eat('{')) return ManifestParse::Malformed;

    // Walk the top-level object for "artifacts", tolerating (and skipping) any
    // other key a later publish might add alongside it.
    bool found = false;
    for (;;) {
        if (s.eat('}')) break;
        char key[24];
        if (!readKey(s, key, sizeof(key))) return ManifestParse::Malformed;
        if (!s.eat(':')) return ManifestParse::Malformed;
        if (std::strcmp(key, "artifacts") == 0) {
            found = true;
            break;
        }
        if (!skipValue(s)) return ManifestParse::Malformed;
        if (s.eat(',')) continue;
        if (s.eat('}')) break;
        return ManifestParse::Malformed;
    }
    if (!found) return ManifestParse::Malformed;

    if (!s.eat('[')) return ManifestParse::Malformed;
    if (s.eat(']')) return ManifestParse::NoArtifacts;

    bool overflowed = false;
    for (;;) {
        UpdateArtifact a;
        if (!readArtifact(s, &a)) return ManifestParse::Malformed;
        if (count_ < kMaxArtifacts) items_[count_++] = a;
        else overflowed = true;

        if (s.eat(',')) continue;
        if (s.eat(']')) break;
        return ManifestParse::Malformed;
    }

    // The closing brace is REQUIRED, not decoration. Without it a download cut at
    // its very last byte parses as a complete manifest — the one truncation the
    // artifact loop can't notice on its own, because every row it did read was
    // whole.
    if (!s.eat('}')) return ManifestParse::Malformed;

    if (count_ == 0) return ManifestParse::NoArtifacts;
    return overflowed ? ManifestParse::TooMany : ManifestParse::Ok;
}

const UpdateArtifact* UpdateManifest::byId(const char* id) const {
    if (!id) return nullptr;
    for (int i = 0; i < count_; ++i)
        if (std::strcmp(items_[i].id, id) == 0) return &items_[i];
    return nullptr;
}

bool artifactIsNewer(const UpdateArtifact& a, uint32_t installedCode) {
    return a.code > installedCode;
}

}  // namespace mal
