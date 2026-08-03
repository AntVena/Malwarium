// net_sta.h — device-tier association with the operator's own home network.
//
// The only place in the firmware that calls WiFi.begin(ssid, pass) — everything
// else that touches the radio either listens (net_sniff.h / net_capture.h),
// broadcasts to whoever is in range (net_link.h), or is its own access point
// (ap_server.h). This one JOINS someone else's network, which is what an update
// check needs and the one thing that leaves the room.
//
// Radio ownership: a RadioArbiter owner, and the TOP one — above the AP. That is
// deliberate and the opposite of a standing toggle winning: this owner only ever
// wants the radio while an update job the operator just started is running
// (Game::netConnectWanted — a stored network AND a live job, nothing else). A job
// somebody is waiting on outranks a toggle somebody left on, and it cannot linger,
// because there is no permission to leave switched on: the job ending is the radio
// going back. The practical consequence is worth knowing: checking for updates
// while the 'Pedia AP is up will drop the AP, so a phone browsing the 'Pedia loses
// its connection until the check finishes.
//
// AP and STA want the Wi-Fi stack in incompatible modes and cannot be co-resident
// here, which is exactly what the arbiter's stand-down-then-service ordering
// guarantees. Provisioning has the opposite need — the AP must be up before there
// are any credentials to use — so it does its scanning inside the AP owner in
// WIFI_AP_STA (ap_server.h) and NEVER auto-connects; joining is a separate, later
// act that only an update job performs. That is why storing credentials and using
// them are two different flows rather than one.
//
// Credentials come from net_credentials.h. Nothing here prints the passphrase.
#pragma once

#include <cstdint>
#include <cstring>

#include "config.h"
#include "core/app/game.h"
#include "core/app/net_status.h"
#include "platform/esp32/net_credentials.h"

#if STA_ENABLED
#  include <WiFi.h>
#endif

namespace mal {

class NetSta {
public:
    // Wire the engine seam. Does NOT power the radio — the RadioArbiter decides
    // when this owner may run (service) vs must stand down (ensureDown).
    void begin(Game* game) { game_ = game; }

    // Whether an association is wanted right now. Never a standing intent: see the
    // banner + Game::netConnectWanted().
    bool wants() const {
#if STA_ENABLED
        return game_ && game_->netConnectWanted();
#else
        return false;
#endif
    }

    // Run one association step, ASSUMING the arbiter has granted this owner the
    // radio. Brings the radio up in STA and starts the join on the first call, then
    // polls it, pushing each transition into the engine (Game::setNetStatus) for the
    // UPDATES screen to render. Non-blocking.
    void service(uint32_t nowMs) {
#if STA_ENABLED
        if (!game_) return;
        if (!joining_) {
            char ssid[NetCredentials::kSsidCap];
            char pass[NetCredentials::kPassCap];
            if (!creds_.load(ssid, sizeof(ssid), pass, sizeof(pass))) {
                // Provisioning was wiped between the ask and the grant. Report the
                // failure rather than associating with nothing; the engine ends the
                // request on a terminal failure, so this doesn't spin.
                push(NetState::Failed, nullptr);
                Serial.println("[sta] no stored network");
                return;
            }
            WiFi.mode(WIFI_STA);
            WiFi.begin(ssid, pass);
            // SSID only. The passphrase never reaches serial, here or anywhere.
            std::memset(pass, 0, sizeof(pass));
            joining_ = true;
            startedMs_ = nowMs;
            push(NetState::Connecting, nullptr);
            Serial.printf("[sta] joining %s\n", ssid);
            return;
        }

        if (WiFi.status() == WL_CONNECTED) {
            if (!online_) {
                online_ = true;
                const String ip = WiFi.localIP().toString();
                push(NetState::Online, ip.c_str());
                Serial.printf("[sta] online %s\n", ip.c_str());
            }
            return;
        }

        // Not connected (yet). WL_CONNECT_FAILED / WL_NO_SSID_AVAIL are reported by
        // some paths and not others, so the timeout is the authority — it covers a
        // wrong passphrase, an out-of-range AP and a DHCP that never answers alike.
        if (online_) {
            // Dropped after being up. Report it as a failure so the screen stops
            // claiming an address the device no longer holds.
            online_ = false;
            push(NetState::Failed, nullptr);
            Serial.println("[sta] link lost");
            return;
        }
        if (nowMs - startedMs_ >= static_cast<uint32_t>(STA_CONNECT_TIMEOUT_MS)) {
            push(NetState::Failed, nullptr);
            Serial.println("[sta] join timed out");
        }
#else
        (void)nowMs;
#endif
    }

    // Fully drop the association and power the radio down (arbiter hands the radio
    // on, or the request ended). Idempotent; safe to call every loop. Deliberately
    // does NOT push a status: the engine's last reading is what the screen is still
    // showing the operator, and a tear-down is not new information about the attempt.
    void ensureDown() {
#if STA_ENABLED
        if (joining_) {
            WiFi.disconnect(/*wifioff=*/true, /*eraseap=*/false);
            WiFi.mode(WIFI_OFF);
            joining_ = false;
            online_ = false;
        }
#endif
    }

private:
#if STA_ENABLED
    void push(NetState s, const char* ip) {
        NetStatus st;
        st.state = s;
        if (ip) {
            std::strncpy(st.ip, ip, sizeof(st.ip) - 1);
            st.ip[sizeof(st.ip) - 1] = '\0';
        }
        game_->setNetStatus(st);
    }

    NetCredentials creds_;
    bool joining_ = false;    // WiFi.begin has been issued and the radio is up
    bool online_ = false;     // ...and it reached WL_CONNECTED
    uint32_t startedMs_ = 0;
#endif

    Game* game_ = nullptr;
};

} // namespace mal
