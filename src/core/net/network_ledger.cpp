#include "core/net/network_ledger.h"

#include <cstdio>
#include <cstring>

namespace mal {

namespace {
// A fixed record: u64 key, u32 count, u8 nameLen, then nameLen name bytes
// (explicit field writes, not a raw struct dump, so host/device compiler padding
// never matters). Not a versioned save format — this file only ever round-trips
// with itself on one device, so a format change just means "delete and re-scan"
// (the ledger rebuilds through walking, unlike the save blob).
bool writeEntry(FILE* f, const NetworkLedger::Entry& e) {
    if (std::fwrite(&e.key, sizeof(e.key), 1, f) != 1) return false;
    if (std::fwrite(&e.count, sizeof(e.count), 1, f) != 1) return false;
    const uint8_t nameLen = static_cast<uint8_t>(std::strlen(e.name));
    if (std::fwrite(&nameLen, sizeof(nameLen), 1, f) != 1) return false;
    if (nameLen > 0 && std::fwrite(e.name, 1, nameLen, f) != nameLen) return false;
    return true;
}

bool readEntry(FILE* f, NetworkLedger::Entry& e) {
    if (std::fread(&e.key, sizeof(e.key), 1, f) != 1) return false;
    if (std::fread(&e.count, sizeof(e.count), 1, f) != 1) return false;
    uint8_t nameLen = 0;
    if (std::fread(&nameLen, sizeof(nameLen), 1, f) != 1) return false;
    std::memset(e.name, 0, sizeof(e.name));
    if (nameLen > 0) {
        const uint8_t cap = static_cast<uint8_t>(sizeof(e.name) - 1);
        const uint8_t toRead = nameLen < cap ? nameLen : cap;
        if (std::fread(e.name, 1, toRead, f) != toRead) return false;
        if (nameLen > toRead) std::fseek(f, nameLen - toRead, SEEK_CUR);  // skip overflow
    }
    return true;
}
}  // namespace

bool NetworkLedger::load(const char* path) {
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

bool NetworkLedger::lookup(uint64_t key, Entry* out) const {
    for (const auto& e : entries_) {
        if (e.key == key) {
            if (out) *out = e;
            return true;
        }
    }
    return false;
}

void NetworkLedger::recordNew(uint64_t key, const char* name) {
    Entry e;
    e.key = key;
    e.count = 1;
    if (name) {
        std::strncpy(e.name, name, sizeof(e.name) - 1);
        e.name[sizeof(e.name) - 1] = '\0';
    }
    entries_.push_back(e);
    writeThrough();
}

void NetworkLedger::recordSeenAgain(uint64_t key, const char* name) {
    for (auto& e : entries_) {
        if (e.key != key) continue;
        ++e.count;
        if (name && name[0] != '\0' && e.name[0] == '\0') {
            std::strncpy(e.name, name, sizeof(e.name) - 1);
            e.name[sizeof(e.name) - 1] = '\0';
        }
        writeThrough();
        return;
    }
}

bool NetworkLedger::inTopN(uint64_t key, int n) const {
    uint32_t myCount = 0;
    bool found = false;
    for (const auto& e : entries_) {
        if (e.key == key) { myCount = e.count; found = true; break; }
    }
    if (!found) return false;
    int higherCount = 0;
    for (const auto& e : entries_) {
        if (e.key != key && e.count > myCount) ++higherCount;
    }
    return higherCount < n;
}

void NetworkLedger::writeThrough() const {
    if (path_[0] == '\0') return;  // no backing file (SD absent) — RAM-only is fine

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
