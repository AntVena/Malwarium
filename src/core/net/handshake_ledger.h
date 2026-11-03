// handshake_ledger.h — bounded, FIFO-rotating set of captured-handshake BSSID keys.
//
// PURE + HOST-TESTABLE, file-backed the same way network_ledger.h is: plain POSIX
// stdio, which resolves to the real filesystem on host and the mounted /sdcard VFS
// on-device. A missing file (no card) leaves the set empty and RAM-only, so capture
// still works for the session; it just does not persist through this file.
//
// Backs SHAKES dedup (Game::registerHandshake): credit is per distinct BSSID, so a
// repeat capture of a known AP never re-credits. At the cap the OLDEST key is
// evicted and the new one still credited, so a long-lived device keeps rewarding
// fresh captures. The SD file holds the full rotating history (kHandshakeDedupCap);
// the save blob carries only a smaller fallback mirror (kHandshakeDedupSaveCap, via
// Game::toSave) so a card-less reboot still remembers recent captures.
#pragma once

#include <cstdint>
#include <vector>

namespace mal {

// RAM/SD working-set size. The save fallback below is deliberately smaller so the
// in-save vector stays light while the card holds the longer history.
constexpr int kHandshakeDedupCap = 1024;
// Most-recent keys mirrored into the save blob when no card backs the ledger (and
// as a floor when one does). Sized to the NVS save budget, not the SD file.
constexpr int kHandshakeDedupSaveCap = 256;

class HandshakeLedger {
public:
    // Read an existing file into the in-RAM set, remembering `path` for later
    // write-throughs. A missing/unopenable file keeps whatever was already seeded
    // and returns false, so the first insert() write-through creates the file from
    // the save fallback. When the file opens it is authoritative: it was written as
    // a superset of the fallback, so adopting it loses no credited AP.
    bool load(const char* path);

    bool contains(uint64_t key) const;

    // Insert `key` if absent, evicting the oldest key first when at kHandshakeDedupCap.
    // Returns true when newly inserted (the caller credits SHAKES), false when it was
    // already present. Writes through to the file when a path is set.
    bool insert(uint64_t key);

    // Prime the set from the save-blob fallback, before load() runs at boot. Clamps
    // to kHandshakeDedupCap and does not write through (no path is set yet).
    void seed(const std::vector<uint64_t>& keys);

    // Forget every key and truncate the backing file. SHAKES is pet-level
    // (Game::resetForNewPet clears handshakesSeen_), so its dedup set resets with the
    // pet, unlike the device-level NetworkLedger which a wipe leaves alone.
    void clear();

    const std::vector<uint64_t>& keys() const { return keys_; }   // oldest first
    size_t size() const { return keys_.size(); }

private:
    void writeThrough() const;

    std::vector<uint64_t> keys_;   // FIFO: oldest at front, newest at back
    char path_[64] = {0};
};

}  // namespace mal
