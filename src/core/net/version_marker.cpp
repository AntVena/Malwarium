#include "core/net/version_marker.h"

#include <cstdio>
#include <cstring>

namespace mal {

namespace {

// Narrow [b,e) to its non-blank span. Strips the '\r' of a CRLF line for free,
// since a line is handed here already split on '\n'.
void trim(const char*& b, const char*& e) {
    auto blank = [](char c) { return c == ' ' || c == '\t' || c == '\r'; };
    while (b < e && blank(*b)) ++b;
    while (e > b && blank(e[-1])) --e;
}

bool spanIs(const char* b, const char* e, const char* lit) {
    const size_t n = std::strlen(lit);
    return static_cast<size_t>(e - b) == n && std::memcmp(b, lit, n) == 0;
}

// [b,e) as a non-negative integer. Leaves `*out` untouched on any failure, so a
// garbled line can't overwrite a good value read from an earlier one.
bool readUint(const char* b, const char* e, uint32_t* out) {
    if (b == e) return false;
    uint64_t v = 0;
    for (const char* p = b; p < e; ++p) {
        if (*p < '0' || *p > '9') return false;
        v = v * 10 + static_cast<uint64_t>(*p - '0');
        if (v > 0xFFFFFFFFull) return false;
    }
    *out = static_cast<uint32_t>(v);
    return true;
}

}  // namespace

bool parseVersionMarker(const char* text, size_t len, VersionMarker* out) {
    if (!out) return false;
    *out = VersionMarker{};
    if (!text || len == 0) return false;

    bool haveCode = false;
    const char* p = text;
    const char* const end = text + len;

    while (p < end) {
        const char* nl = p;
        while (nl < end && *nl != '\n') ++nl;
        const char* b = p;
        const char* e = nl;
        p = (nl < end) ? nl + 1 : end;

        trim(b, e);
        if (b == e || *b == '#') continue;

        const char* eq = b;
        while (eq < e && *eq != '=') ++eq;
        if (eq == e) continue;              // not key=value — not a line of ours

        const char* kb = b;
        const char* ke = eq;
        trim(kb, ke);
        const char* vb = eq + 1;
        const char* ve = e;
        trim(vb, ve);

        if (spanIs(kb, ke, "code")) {
            if (readUint(vb, ve, &out->code)) haveCode = true;
        } else if (spanIs(kb, ke, "version")) {
            // A version too long to hold is DROPPED, not clipped: an empty
            // display string reads as "unknown", which is true, where a clipped
            // one reads as a real version that was never published. Nothing is
            // decided on this field, so losing it costs only the prompt's wording.
            const size_t n = static_cast<size_t>(ve - vb);
            if (n < sizeof(out->version)) {
                std::memcpy(out->version, vb, n);
                out->version[n] = '\0';
            }
        }
        // Any other key belongs to a publish this firmware predates — skip it.
    }

    if (!haveCode) {
        *out = VersionMarker{};
        return false;
    }
    return true;
}

size_t formatVersionMarker(const VersionMarker& m, char* out, size_t cap) {
    if (!out || cap == 0) return 0;
    const int n = std::snprintf(out, cap, "code=%u\nversion=%s\n",
                                static_cast<unsigned>(m.code), m.version);
    if (n < 0 || static_cast<size_t>(n) >= cap) {
        out[0] = '\0';
        return 0;
    }
    return static_cast<size_t>(n);
}

}  // namespace mal
