#include "core/net/tar_reader.h"

#include <cstring>

namespace mal {

namespace {

// Fixed offsets in a POSIX tar header. Named rather than inlined because the
// whole reader is these five numbers.
constexpr size_t kOffName = 0;
constexpr size_t kOffSize = 124;
constexpr size_t kOffType = 156;
constexpr size_t kLenName = 100;
constexpr size_t kLenSize = 12;

// tar stores sizes as NUL/space-terminated octal. Anything that isn't octal digits
// is a broken header, not a zero-length file — say so rather than extracting an
// empty file and moving the stream cursor to the wrong place.
bool readOctal(const uint8_t* p, size_t len, uint32_t* out) {
    uint64_t v = 0;
    size_t i = 0;
    while (i < len && (p[i] == ' ' || p[i] == '0')) {
        if (p[i] == '0') break;      // leading zeros are digits, leading spaces are not
        ++i;
    }
    size_t digits = 0;
    for (; i < len; ++i) {
        const uint8_t c = p[i];
        if (c == '\0' || c == ' ') break;
        if (c < '0' || c > '7') return false;
        v = v * 8 + static_cast<uint64_t>(c - '0');
        if (v > 0xFFFFFFFFull) return false;
        ++digits;
    }
    if (digits == 0) return false;
    *out = static_cast<uint32_t>(v);
    return true;
}

bool allZero(const uint8_t* p, size_t len) {
    for (size_t i = 0; i < len; ++i)
        if (p[i]) return false;
    return true;
}

}  // namespace

bool tarPathSafe(const char* name) {
    if (!name || name[0] == '\0') return false;
    if (name[0] == '/') return false;               // absolute — not ours to place
    // Walk components. A ".." anywhere climbs out of the root, whether it leads,
    // trails, or hides in the middle.
    const char* seg = name;
    for (const char* p = name;; ++p) {
        if (*p == '\\') return false;               // see the header on why not a plain char
        if (*p == '/' || *p == '\0') {
            const size_t n = static_cast<size_t>(p - seg);
            if (n == 2 && seg[0] == '.' && seg[1] == '.') return false;
            if (*p == '\0') break;
            seg = p + 1;
        }
    }
    return true;
}

bool parseTarHeader(const uint8_t* block, size_t len, TarEntry* out) {
    if (!block || !out || len < kTarBlock) return false;
    *out = TarEntry{};

    // The archive ends in zero blocks. Report that as an outcome, not a parse
    // failure — it is how a well-formed archive says it is finished.
    if (allZero(block, kTarBlock)) {
        out->kind = TarEntryKind::End;
        return true;
    }

    std::memcpy(out->name, block + kOffName, kLenName);
    out->name[kLenName - 1] = '\0';                 // a name filling the field isn't terminated
    if (!tarPathSafe(out->name)) {
        out->kind = TarEntryKind::Invalid;
        return true;                                // parsed fine; the NAME is the problem
    }

    if (!readOctal(block + kOffSize, kLenSize, &out->size)) {
        out->kind = TarEntryKind::Invalid;
        return true;
    }

    switch (block[kOffType]) {
        case '\0':
        case '0':
            out->kind = TarEntryKind::File;
            break;
        case '5':
            out->kind = TarEntryKind::Directory;
            out->size = 0;                          // a directory entry carries no data
            break;
        default:
            // Links, devices, PAX/GNU extension records: nothing the bundle uses.
            // Skipped rather than rejected, so a tar written by a slightly
            // different tool still extracts the files that matter.
            out->kind = TarEntryKind::Skip;
            break;
    }
    return true;
}

}  // namespace mal
