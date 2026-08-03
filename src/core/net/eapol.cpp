#include "core/net/eapol.h"

#include <cstring>

namespace mal {

namespace {

// --- 802.11 Frame Control bit fields (little-endian byte pair) --------------
// byte0: [ver:2][type:2][subtype:4]  byte1: [ToDS FromDS MoreFrag Retry ...]
constexpr uint8_t kTypeData = 0x02;   // frame type 2 = data (EAPOL rides data)
constexpr uint8_t kSubtypeQoSFlag = 0x08;  // data subtype bit -> QoS data (+2 hdr)
constexpr uint8_t kFcFromDs = 0x02;
constexpr uint8_t kFcToDs = 0x01;

// --- EAPOL-Key "Key Information" flags (16-bit, big-endian on the wire) ------
constexpr uint16_t kKiPairwise = 0x0008;  // Key Type: 1 = pairwise (4-way), 0 = group
constexpr uint16_t kKiInstall  = 0x0040;
constexpr uint16_t kKiAck      = 0x0080;
constexpr uint16_t kKiMic      = 0x0100;
constexpr uint16_t kKiSecure   = 0x0200;

// Map the Key Information flags to a 4-way handshake message number, or 0 if the
// combination isn't one of the four pairwise messages (e.g. a group rekey).
int classify4Way(uint16_t ki) {
    if (!(ki & kKiPairwise)) return 0;  // group-key handshake, not the 4-way
    const bool install = ki & kKiInstall;
    const bool ack     = ki & kKiAck;
    const bool mic     = ki & kKiMic;
    const bool secure  = ki & kKiSecure;
    // M1: AP->STA, ANonce, no MIC yet.           Ack, !MIC, !Install
    if (ack && !mic && !install) return 1;
    // M3: AP->STA, install the PTK.              Ack, MIC, Install
    if (ack && mic && install) return 3;
    // M2: STA->AP, SNonce + MIC, not yet secure. MIC, !Ack, !Secure, !Install
    if (mic && !ack && !install && !secure) return 2;
    // M4: STA->AP, ack the install, secure set.  MIC, !Ack, Secure, !Install
    if (mic && !ack && !install && secure) return 4;
    return 0;
}

} // namespace

EapolKeyInfo parseEapolKey(const uint8_t* mpdu, uint32_t len) {
    EapolKeyInfo info;
    if (!mpdu || len < 24) return info;  // shorter than the base MAC header

    const uint8_t fc0 = mpdu[0];
    const uint8_t fc1 = mpdu[1];
    const uint8_t type = (fc0 >> 2) & 0x03;
    const uint8_t subtype = (fc0 >> 4) & 0x0F;
    if (type != kTypeData) return info;             // only data frames carry EAPOL
    if (subtype & 0x04) return info;                // Null/QoS-Null: no payload

    const bool toDs = fc1 & kFcToDs;
    const bool fromDs = fc1 & kFcFromDs;

    // MAC header length: base 24, +6 for addr4 (WDS), +2 for QoS control.
    uint32_t hdr = 24;
    if (toDs && fromDs) hdr += 6;                   // 4-address WDS frame
    if (subtype & kSubtypeQoSFlag) hdr += 2;        // QoS data control field

    // BSSID selection from the DS bits (infrastructure addressing rules).
    const uint8_t* bssid;
    if (!toDs && !fromDs)      bssid = mpdu + 16;   // addr3 (IBSS / same-DS)
    else if (!toDs && fromDs)  bssid = mpdu + 10;   // addr2 (from the AP)
    else if (toDs && !fromDs)  bssid = mpdu + 4;    // addr1 (to the AP)
    else                       bssid = mpdu + 10;   // WDS: transmitter addr2

    // Need the LLC/SNAP (8) + EAPOL header (4) after the MAC header.
    if (len < hdr + 8 + 4) return info;
    const uint8_t* llc = mpdu + hdr;
    // LLC/SNAP for EtherType-encapsulated payload: AA AA 03 00 00 00 <ethertype>.
    static const uint8_t kSnap[6] = {0xAA, 0xAA, 0x03, 0x00, 0x00, 0x00};
    if (std::memcmp(llc, kSnap, 6) != 0) return info;
    const uint16_t ethertype = (static_cast<uint16_t>(llc[6]) << 8) | llc[7];
    if (ethertype != kEtherTypeEapol) return info;

    const uint8_t* eapol = llc + 8;                 // 802.1X header
    // eapol[0]=version, eapol[1]=packet type, eapol[2..3]=body length (big-endian).
    if (eapol[1] != kEapolTypeKey) return info;     // only EAPOL-Key (the 4-way)

    // EAPOL-Key body: [descriptor type:1][key information:2]... Need 3 bytes.
    if (len < hdr + 8 + 4 + 3) return info;
    const uint8_t* body = eapol + 4;
    const uint16_t keyInfo = (static_cast<uint16_t>(body[1]) << 8) | body[2];

    info.isEapolKey = true;
    info.msg = classify4Way(keyInfo);
    std::memcpy(info.bssid, bssid, 6);
    return info;
}

bool HandshakeTracker::observe(const uint8_t* bssid, int msg) {
    if (msg < 1 || msg > 4 || !bssid) return false;
    const uint8_t bit = static_cast<uint8_t>(1u << msg);

    Ap* ap = nullptr;
    for (int i = 0; i < count_; ++i) {
        if (std::memcmp(aps_[i].bssid, bssid, 6) == 0) { ap = &aps_[i]; break; }
    }
    if (!ap) {
        if (count_ >= kMaxAps) return false;        // table full: ignore new APs
        ap = &aps_[count_++];
        std::memcpy(ap->bssid, bssid, 6);
        ap->mask = 0;
        ap->credited = false;
    }
    ap->mask |= bit;
    if (!ap->credited && crackable(ap->mask)) {
        ap->credited = true;
        return true;                                // first crackable moment
    }
    return false;
}

void HandshakeTracker::reset() {
    count_ = 0;
    std::memset(aps_, 0, sizeof(aps_));
}

} // namespace mal
