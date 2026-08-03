#include "core/net/peer_ledger.h"

#include <cstring>

namespace mal {

namespace {

// One record, written field by field (never a raw struct dump) so host/device
// padding never matters — the same shape NetworkLedger uses. Strings are written
// at their full field capacity rather than length-prefixed, because those caps are
// already the wire's fixed slots: a record is one constant size, so a truncated
// tail is detectable as a short read instead of silently reinterpreting bytes.
//
// Not a versioned save format. This file only round-trips with itself on one
// device, and it rebuilds by meeting people again, so a layout change means
// "delete and re-collect" rather than a migration.
bool writeEntry(FILE* f, const PeerLedger::Entry& e) {
    if (std::fwrite(&e.key, sizeof(e.key), 1, f) != 1) return false;
    if (std::fwrite(&e.timesMet, sizeof(e.timesMet), 1, f) != 1) return false;
    if (std::fwrite(&e.stage, sizeof(e.stage), 1, f) != 1) return false;
    if (std::fwrite(&e.rank, sizeof(e.rank), 1, f) != 1) return false;
    const uint8_t team = e.crewRed ? 1 : 0;
    if (std::fwrite(&team, sizeof(team), 1, f) != 1) return false;
    if (std::fwrite(e.tag, 1, kPeerTagCap, f) != kPeerTagCap) return false;
    if (std::fwrite(e.petName, 1, kPeerPetCap, f) != kPeerPetCap) return false;
    if (std::fwrite(e.crewName, 1, kPeerCrewCap, f) != kPeerCrewCap) return false;
    return true;
}

bool readEntry(FILE* f, PeerLedger::Entry& e) {
    if (std::fread(&e.key, sizeof(e.key), 1, f) != 1) return false;
    if (std::fread(&e.timesMet, sizeof(e.timesMet), 1, f) != 1) return false;
    if (std::fread(&e.stage, sizeof(e.stage), 1, f) != 1) return false;
    if (std::fread(&e.rank, sizeof(e.rank), 1, f) != 1) return false;
    uint8_t team = 0;
    if (std::fread(&team, sizeof(team), 1, f) != 1) return false;
    e.crewRed = team != 0;
    if (std::fread(e.tag, 1, kPeerTagCap, f) != kPeerTagCap) return false;
    if (std::fread(e.petName, 1, kPeerPetCap, f) != kPeerPetCap) return false;
    if (std::fread(e.crewName, 1, kPeerCrewCap, f) != kPeerCrewCap) return false;
    // A truncated or hand-mangled file must not hand unterminated strings to the
    // renderer; the read above fills the slots but cannot promise a NUL in them.
    e.tag[kPeerTagCap - 1] = '\0';
    e.petName[kPeerPetCap - 1] = '\0';
    e.crewName[kPeerCrewCap - 1] = '\0';
    return true;
}

void copyField(char* dst, const char* src, size_t cap) {
    std::memset(dst, 0, cap);
    if (!src) return;
    std::strncpy(dst, src, cap - 1);
    dst[cap - 1] = '\0';
}

}  // namespace

bool PeerLedger::load(const char* path) {
    entries_.clear();
    if (!path || path[0] == '\0') { path_[0] = '\0'; return false; }
    std::strncpy(path_, path, sizeof(path_) - 1);
    path_[sizeof(path_) - 1] = '\0';

    FILE* f = std::fopen(path_, "rb");
    if (!f) return false;
    Entry e;
    while (readEntry(f, e)) entries_.push_back(e);
    std::fclose(f);
    return true;
}

bool PeerLedger::lookup(uint64_t key, Entry* out) const {
    for (const auto& e : entries_) {
        if (e.key == key) {
            if (out) *out = e;
            return true;
        }
    }
    return false;
}

bool PeerLedger::record(uint64_t key, const PeerHello& hello) {
    for (auto& e : entries_) {
        if (e.key != key) continue;
        ++e.timesMet;
        // Refresh everything they broadcast: the point of meeting someone again is
        // seeing what changed since last time.
        copyField(e.tag, hello.tag, kPeerTagCap);
        copyField(e.petName, hello.petName, kPeerPetCap);
        copyField(e.crewName, hello.crewName, kPeerCrewCap);
        e.stage = hello.stage;
        e.rank = hello.rank;
        e.crewRed = hello.crewRed;
        writeThrough();
        return false;
    }

    Entry e;
    e.key = key;
    e.timesMet = 1;
    copyField(e.tag, hello.tag, kPeerTagCap);
    copyField(e.petName, hello.petName, kPeerPetCap);
    copyField(e.crewName, hello.crewName, kPeerCrewCap);
    e.stage = hello.stage;
    e.rank = hello.rank;
    e.crewRed = hello.crewRed;
    entries_.push_back(e);
    writeThrough();
    return true;
}

void PeerLedger::writeThrough() const {
    if (path_[0] == '\0') return;  // no backing file (SD absent) — RAM-only is fine

    // Write-and-rename, so a power cut mid-write leaves the previous roster intact
    // rather than a half-file that reads as a truncated record.
    char tmpPath[72];
    std::snprintf(tmpPath, sizeof(tmpPath), "%s.tmp", path_);

    FILE* f = std::fopen(tmpPath, "wb");
    if (!f) return;
    bool ok = true;
    for (const auto& e : entries_) {
        if (!writeEntry(f, e)) { ok = false; break; }
    }
    std::fclose(f);
    if (ok) std::rename(tmpPath, path_);
    else std::remove(tmpPath);
}

}  // namespace mal
