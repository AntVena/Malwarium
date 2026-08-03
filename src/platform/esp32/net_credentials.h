// net_credentials.h — device-tier home-network credentials, backed by NVS.
//
// The SSID + passphrase the device associates with (platform/esp32/net_sta.h),
// written by the phone-driven setup page the SoftAP serves (ap_server.h) and read
// back on every connect attempt. Opens NVS_WIFI_NAMESPACE in NVS_SAVE_PARTITION
// per call, exactly like NvsSaveStore (save_store_esp32.h) — cheap, and no handle
// is left dangling across the long idle stretches between attempts.
//
// DELIBERATELY NOT IN THE SAVE BLOB. The save round-trips through the ARCH rack
// and is described to the web 'Pedia, and src/core is platform-agnostic — a
// passphrase belongs in neither. Its own namespace also means a Factory Reset of
// the save leaves the network provisioning intact.
//
// The passphrase is stored in PLAINTEXT: stock ESP32 NVS is not encrypted, so
// anyone who can read the flash can read it back. NVS encryption is a separate
// feature and is not enabled here. Nothing in this codebase prints a stored
// passphrase to serial or serialises it into pedia_state.json — keep it that way:
// provisioned() is the only thing the UI ever needs to know.
#pragma once

#include <cstddef>
#include <cstring>

#include "config.h"

#if STA_ENABLED
#  include <Preferences.h>
#endif

namespace mal {

class NetCredentials {
public:
    // Longest values the Wi-Fi stack accepts: a 32-char SSID and a 63-char WPA2
    // passphrase, each plus a terminator. Callers size their buffers from these.
    static constexpr size_t kSsidCap = 33;
    static constexpr size_t kPassCap = 64;

    // Is a network stored? The only credential fact the UI is allowed to see —
    // Game::setNetProvisioned() carries exactly this bool into the engine so the
    // UPDATES screen can tell "nowhere to go" apart from "nowhere to look".
    bool provisioned() const {
#if STA_ENABLED
        char ssid[kSsidCap];
        return loadSsid(ssid, sizeof(ssid)) && ssid[0] != '\0';
#else
        return false;
#endif
    }

    // Fill both buffers from NVS. False (and untouched buffers) when nothing is
    // stored — the caller must not attempt an association in that case.
    bool load(char* ssid, size_t ssidCap, char* pass, size_t passCap) const {
#if STA_ENABLED
        // Zero FIRST: a missing key leaves the caller's buffer untouched, so an
        // absent passphrase would otherwise be read as whatever the stack held.
        ssid[0] = '\0';
        pass[0] = '\0';
        Preferences p;
        if (!p.begin(NVS_WIFI_NAMESPACE, /*readOnly=*/true, NVS_SAVE_PARTITION)) return false;
        p.getString(NVS_WIFI_SSID_KEY, ssid, ssidCap);
        // An open network legitimately has no passphrase, so only the SSID decides
        // whether this is a usable credential.
        p.getString(NVS_WIFI_PASS_KEY, pass, passCap);
        p.end();
        return ssid[0] != '\0';
#else
        (void)ssid; (void)ssidCap; (void)pass; (void)passCap;
        return false;
#endif
    }

    // Replace the stored network. An empty SSID is rejected rather than written,
    // so a mis-typed submit can never quietly deprovision a working device. A null
    // or empty passphrase stores an open network.
    bool store(const char* ssid, const char* pass) {
#if STA_ENABLED
        if (!ssid || ssid[0] == '\0' || std::strlen(ssid) >= kSsidCap) return false;
        if (pass && std::strlen(pass) >= kPassCap) return false;
        Preferences p;
        if (!p.begin(NVS_WIFI_NAMESPACE, /*readOnly=*/false, NVS_SAVE_PARTITION)) return false;
        // The SSID write is the one that decides success — an open network stores an
        // empty passphrase, whose write legitimately reports zero bytes.
        const bool ok = p.putString(NVS_WIFI_SSID_KEY, ssid) > 0;
        p.putString(NVS_WIFI_PASS_KEY, pass ? pass : "");
        p.end();
        return ok;
#else
        (void)ssid; (void)pass;
        return false;
#endif
    }

    // Forget the stored network (deprovision). This is the ONLY thing standing
    // between the device and the 'net: there is no separate permission bit to clear
    // alongside it, because an update job raises and drops its own association.
    void clear() {
#if STA_ENABLED
        Preferences p;
        if (!p.begin(NVS_WIFI_NAMESPACE, /*readOnly=*/false, NVS_SAVE_PARTITION)) return;
        p.remove(NVS_WIFI_SSID_KEY);
        p.remove(NVS_WIFI_PASS_KEY);
        p.end();
#endif
    }

private:
#if STA_ENABLED
    bool loadSsid(char* ssid, size_t cap) const {
        Preferences p;
        if (!p.begin(NVS_WIFI_NAMESPACE, /*readOnly=*/true, NVS_SAVE_PARTITION)) return false;
        const size_t n = p.getString(NVS_WIFI_SSID_KEY, ssid, cap);
        p.end();
        return n > 0;
    }
#endif
};

} // namespace mal
