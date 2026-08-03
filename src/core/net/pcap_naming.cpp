#include "core/net/pcap_naming.h"

#include <cstdio>
#include <cstring>

namespace mal {

size_t sanitizeForFilename(const char* raw, char* out, size_t outCap, size_t maxLen) {
    if (!out || outCap == 0) return 0;
    size_t n = 0;
    if (raw) {
        for (size_t i = 0; raw[i] != '\0' && n < maxLen && n + 1 < outCap; ++i) {
            const char c = raw[i];
            const bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                            (c >= '0' && c <= '9') || c == '_' || c == '-';
            out[n++] = ok ? c : '_';
        }
    }
    out[n] = '\0';
    return n;
}

void buildPcapFilename(char* out, size_t outCap, const char* prefix,
                        const uint8_t bssid[6], const char* ssid) {
    if (!out || outCap == 0) return;
    char name[32] = {0};
    const size_t n = sanitizeForFilename(ssid, name, sizeof(name), 16);
    if (n == 0) {
        // No usable SSID (hidden/empty/never scanned): fall back to the full
        // BSSID hex — always available, deterministic, collision-free per network.
        std::snprintf(name, sizeof(name), "%02x%02x%02x%02x%02x%02x", bssid[0],
                      bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
    } else {
        // Human-readable SSID, disambiguated by the last 3 BSSID octets so two
        // APs sharing an SSID never collide onto one file (e.g. "HomeWiFi_6736b6").
        char suffix[8];
        std::snprintf(suffix, sizeof(suffix), "_%02x%02x%02x", bssid[3], bssid[4],
                      bssid[5]);
        const size_t room = sizeof(name) - std::strlen(suffix) - 1;
        if (std::strlen(name) > room) name[room] = '\0';  // keep the suffix intact
        std::strncat(name, suffix, sizeof(name) - std::strlen(name) - 1);
    }
    std::snprintf(out, outCap, "%s%s.pcap", prefix ? prefix : "", name);
}

}  // namespace mal
