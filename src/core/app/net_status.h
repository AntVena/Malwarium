// net_status.h — portable home-network association state the platform tier feeds in.
//
// Joining a network is device-tier work (platform/esp32/net_sta.h drives
// WiFi.begin), but the *shape* of the reading lives here so the UPDATES screen and
// the native tests can read it without a radio. The device layer fills a NetStatus
// as the attempt progresses and hands it to Game::setNetStatus(). It is the first
// step a job reports (CONNECTING → CHECKING → the verdict), because the job is the
// only thing that ever asks for an association. Host builds never push one, so it
// stays Idle.
//
// The stored credentials are NEVER represented here: this carries the association
// OUTCOME only (state + the address we were given), so nothing that renders it or
// serialises it can leak a passphrase.
#pragma once

namespace mal {

enum class NetState : unsigned char {
    Idle,        // no attempt in flight
    Connecting,  // associating / waiting on DHCP
    Online,      // associated, address acquired
    Failed,      // the attempt gave up (wrong passphrase, out of range, no DHCP)
};

struct NetStatus {
    NetState state = NetState::Idle;
    char ip[16] = {0};   // dotted quad once Online, empty otherwise
};

} // namespace mal
