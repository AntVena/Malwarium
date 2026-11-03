#include "core/net/handshake_ledger.h"

#include <cstdio>
#include <cstring>

namespace mal {

bool HandshakeLedger::load(const char* path) {
    if (!path || path[0] == '\0') { path_[0] = '\0'; return false; }
    std::strncpy(path_, path, sizeof(path_) - 1);
    path_[sizeof(path_) - 1] = '\0';

    FILE* f = std::fopen(path_, "rb");
    if (!f) return false;   // no file yet: a later insert() write-through creates it
    keys_.clear();
    uint64_t key = 0;
    while (std::fread(&key, sizeof(key), 1, f) == 1) {
        keys_.push_back(key);
        if (static_cast<int>(keys_.size()) > kHandshakeDedupCap)
            keys_.erase(keys_.begin());   // a file longer than the cap: keep the newest
    }
    std::fclose(f);
    return true;
}

bool HandshakeLedger::contains(uint64_t key) const {
    for (uint64_t k : keys_)
        if (k == key) return true;
    return false;
}

bool HandshakeLedger::insert(uint64_t key) {
    if (contains(key)) return false;
    if (static_cast<int>(keys_.size()) >= kHandshakeDedupCap)
        keys_.erase(keys_.begin());   // evict the oldest rather than refuse the new
    keys_.push_back(key);
    writeThrough();
    return true;
}

void HandshakeLedger::seed(const std::vector<uint64_t>& keys) {
    keys_ = keys;
    const int over = static_cast<int>(keys_.size()) - kHandshakeDedupCap;
    if (over > 0) keys_.erase(keys_.begin(), keys_.begin() + over);
}

void HandshakeLedger::clear() {
    keys_.clear();
    writeThrough();   // truncate the file so a reboot does not resurrect the set
}

void HandshakeLedger::writeThrough() const {
    if (path_[0] == '\0') return;   // no backing file (SD absent): RAM-only is fine

    char tmpPath[72];
    std::snprintf(tmpPath, sizeof(tmpPath), "%s.tmp", path_);

    FILE* f = std::fopen(tmpPath, "wb");
    if (!f) return;
    bool ok = true;
    for (uint64_t k : keys_) {
        if (std::fwrite(&k, sizeof(k), 1, f) != 1) { ok = false; break; }
    }
    std::fclose(f);
    if (ok) std::rename(tmpPath, path_);
    else std::remove(tmpPath);
}

}  // namespace mal
