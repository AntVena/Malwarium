// network_ledger.h — persistent ledger of real Wi-Fi networks a pet has walked past.
//
// PURE + HOST-TESTABLE, like eapol.h/pcap.h. Backs the EXPL Wi-Fi event's real-
// network discovery (Game::resolveNetworkDiscovery, game_net.cpp): for each real
// BSSID sighted, tracks a display name and how many times it's been credited, so
// the event can tell a genuinely-new network from a familiar one and infer a
// player's "home turf" from sighting counts (the most-seen networks).
//
// Uses plain POSIX stdio (fopen/fread/fwrite), which resolves to the regular
// filesystem on host and to the mounted /sdcard VFS on-device (the same calls
// SdCard::selfTest() already uses there) — so this class needs no Arduino/ESP-IDF
// dependency and no device-only stub. A missing/unset file is not an error: the
// in-RAM index just starts empty and every mutator still works, so on a device
// with no SD card this degrades to a session-only (RAM-only) tracker rather than
// failing outright.
#pragma once

#include <cstdint>
#include <cstdio>
#include <vector>

namespace mal {

class NetworkLedger {
public:
    struct Entry {
        uint64_t key = 0;       // packed 48-bit BSSID (see Game::registerNetwork)
        char name[33] = {0};    // SSID, or a BSSID-derived fallback label
        uint32_t count = 0;     // times this network has been credited (>=1)
    };

    // Read an existing ledger file into the in-RAM index. Remembers `path` for
    // later write-throughs. A missing file (no SD, first boot) leaves the index
    // empty and returns false — not an error; every mutator below still works
    // in-RAM regardless of whether this ever succeeds.
    bool load(const char* path);

    bool lookup(uint64_t key, Entry* out) const;

    // Records a genuinely-new network at count 1 and write-throughs to disk (if a
    // path is set). `name` may be empty; the caller (Game) always resolves a
    // display name — real SSID or a BSSID fallback — before calling this.
    void recordNew(uint64_t key, const char* name);

    // Bumps an existing network's count. If `name` is non-empty and looks like a
    // better name than what's stored (i.e. the stored name was empty), the name is
    // upgraded. No-ops if `key` isn't already known (call recordNew first).
    void recordSeenAgain(uint64_t key, const char* name);

    // True if fewer than `n` OTHER entries have a strictly higher count than
    // `key`'s (ties all qualify). False if `key` isn't known.
    bool inTopN(uint64_t key, int n) const;

    // Every known network, in insertion order. The CREW screen's home-network
    // picker (Game::crewNetworkRows) enumerates this to offer the operator the
    // networks their pet has actually walked past.
    const std::vector<Entry>& entries() const { return entries_; }

    size_t size() const { return entries_.size(); }

private:
    void writeThrough() const;

    std::vector<Entry> entries_;
    char path_[64] = {0};
};

}  // namespace mal
