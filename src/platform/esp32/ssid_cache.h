// ssid_cache.h — device-tier BSSID→SSID map for naming audit .pcap files.
//
// The audit capture path (net_capture.h) hears only DATA/EAPOL frames, which
// carry a BSSID but never the human-readable SSID — so on its own it can only
// name a capture after the raw BSSID hex. The passive scan (net_sniff.h) DOES
// see SSIDs (WiFi.scanNetworks reports both), and the two owners are radio-
// arbitrated (never live at once), so the scan populates this small RAM cache
// and the capture reads it when opening a session file.
//
// Pure C++ (no Arduino) so it compiles on host + both boards. RAM-only — the
// mapping is a naming convenience, not persisted state. Fixed capacity with a
// round-robin victim; a full table just recycles the oldest slot.
#pragma once

#include <cstdint>
#include <cstring>

namespace mal {

class SsidCache {
public:
    static SsidCache& instance() {
        static SsidCache c;
        return c;
    }

    // Record (or refresh) the SSID for a BSSID. Empty/hidden SSIDs are ignored so
    // a hidden network never displaces a real name and never yields a blank name.
    void put(const uint8_t bssid[6], const char* ssid) {
        if (!bssid || !ssid || ssid[0] == '\0') return;
        for (auto& e : entries_) {  // already known → refresh in place
            if (e.used && std::memcmp(e.bssid, bssid, 6) == 0) {
                copySsid(e, ssid);
                return;
            }
        }
        Entry* slot = nullptr;  // first free slot, else the round-robin victim
        for (auto& e : entries_) {
            if (!e.used) { slot = &e; break; }
        }
        if (!slot) {
            slot = &entries_[victim_];
            victim_ = (victim_ + 1) % kCap;
        }
        std::memcpy(slot->bssid, bssid, 6);
        slot->used = true;
        copySsid(*slot, ssid);
    }

    // The SSID for a BSSID, or nullptr if never scanned. The returned pointer is
    // valid until the next put() that could evict the slot — copy it right away.
    const char* get(const uint8_t bssid[6]) const {
        if (!bssid) return nullptr;
        for (auto& e : entries_) {
            if (e.used && std::memcmp(e.bssid, bssid, 6) == 0) return e.ssid;
        }
        return nullptr;
    }

private:
    static constexpr int kCap = 32;
    static constexpr int kSsidMax = 33;  // 32-char SSID + NUL

    struct Entry {
        uint8_t bssid[6] = {0};
        char ssid[kSsidMax] = {0};
        bool used = false;
    };

    static void copySsid(Entry& e, const char* ssid) {
        std::strncpy(e.ssid, ssid, kSsidMax - 1);
        e.ssid[kSsidMax - 1] = '\0';
    }

    Entry entries_[kCap];
    int victim_ = 0;
};

}  // namespace mal
