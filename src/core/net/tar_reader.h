// tar_reader.h — read one entry header out of an uncompressed tar stream.
//
// PURE + HOST-TESTABLE, like update_manifest.h — no filesystem. Hand it a
// 512-byte block and it tells you what the next entry is; creating directories
// and writing bytes is the device tier's job (platform/esp32/net_update.h).
//
// WHY TAR. The web 'Pedia bundle is ~50 small files, so it has to travel as one
// artifact, and uncompressed tar is the only container a device can unpack with no
// decompressor at all — the whole reader is a fixed-offset header parse.
//
// THE PART THAT MATTERS IS parseTarHeader's PATH CHECK. An archive names its own
// destinations, so an entry called "../../boot/whatever" would let a bad (or
// merely corrupt) bundle write anywhere the device can reach. Every name is
// therefore validated against escaping the extraction root BEFORE the caller ever
// opens a file, and an entry that fails is rejected outright rather than
// sanitised — a name we had to rewrite is not a name we understood.
#pragma once

#include <cstddef>
#include <cstdint>

namespace mal {

// Every tar structure is a multiple of this, headers and data padding alike.
constexpr size_t kTarBlock = 512;

enum class TarEntryKind : uint8_t {
    End,       // the all-zero block that terminates the archive
    File,      // regular file; `size` bytes of data follow, padded to kTarBlock
    Directory, // create it; no data follows
    Skip,      // something this reader doesn't handle — jump `size` and continue
    Invalid,   // not a tar header, or a name that tries to escape the root
};

struct TarEntry {
    TarEntryKind kind = TarEntryKind::Invalid;
    char name[100] = {0};
    uint32_t size = 0;
};

// Interpret one kTarBlock-sized header at `block`. `len` must be at least
// kTarBlock; anything shorter is Invalid (a truncated archive, not a short entry).
bool parseTarHeader(const uint8_t* block, size_t len, TarEntry* out);

// Whether `name` may be joined to an extraction root and written. Rejects absolute
// paths, any "..' component, backslashes (a path separator on the machine that
// built the archive is not one here, and treating it as a plain character is how a
// name sneaks past a check that only looks for '/'), and empty names. This is the
// rule parseTarHeader enforces; it is exposed so a caller can re-check a path it
// assembled itself.
bool tarPathSafe(const char* name);

// Bytes to advance past an entry's data: its size rounded up to a whole block.
constexpr uint32_t tarDataSpan(uint32_t size) {
    return (size + static_cast<uint32_t>(kTarBlock) - 1) /
           static_cast<uint32_t>(kTarBlock) * static_cast<uint32_t>(kTarBlock);
}

}  // namespace mal
