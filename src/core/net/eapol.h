// eapol.h — pure 802.11 EAPOL / WPA 4-way-handshake classifier (RF half).
//
// PURE + HOST-TESTABLE. This is the *brain* the device-tier promiscuous sniffer
// (platform/esp32/net_capture.h) feeds every captured 802.11 MPDU into. It knows
// nothing about the radio, channels, SD, or timing — it takes a raw frame buffer
// (DLT_IEEE802_11, no radiotap — exactly what goes into pcap.h) and answers two
// questions: "is this an EAPOL-Key frame, and if so which of the WPA 4-way
// messages (1..4) is it?", and "which BSSID is it for?". A tiny stateful tracker
// then accumulates the per-BSSID messages and reports when a *crackable* handshake
// has first been observed, so the device only calls Game::registerHandshake() once
// per AP. All of that is deterministic byte-parsing, so it's unit-tested natively
// with synthesized frames — the device file stays thin RF+SD glue on top.
//
// AUTHORIZED USE / PASSIVE ONLY: this only *classifies* frames the radio already
// heard. Nothing here transmits, injects, deauths, or forces a handshake — it
// waits for one that happens on its own and recognises it.
#pragma once

#include <cstddef>
#include <cstdint>

namespace mal {

// One classified 802.11 frame. `msg` is the 4-way handshake message number
// (1..4) or 0 when the frame is not a pairwise 4-way EAPOL-Key (non-EAPOL,
// group rekey, malformed, or truncated). `bssid` is valid only when `isEapolKey`.
struct EapolKeyInfo {
    bool isEapolKey = false;   // a valid 802.1X EAPOL-Key frame was parsed
    int  msg = 0;              // 4-way handshake message 1..4, else 0
    uint8_t bssid[6] = {0};    // the AP this key exchange belongs to
};

// EtherType carried by the LLC/SNAP header when the data frame is 802.1X/EAPOL.
constexpr uint16_t kEtherTypeEapol = 0x888E;
// 802.1X packet type for an EAPOL-Key frame (the 4-way handshake rides these).
constexpr uint8_t kEapolTypeKey = 0x03;

// Parse a raw 802.11 MPDU (no radiotap) and classify it. Returns a zeroed
// (isEapolKey=false) result for anything that isn't a pairwise EAPOL-Key data
// frame: management/control frames, non-EAPOL data, group-key rekeys, or a buffer
// too short to hold the headers. Never reads past `len`.
EapolKeyInfo parseEapolKey(const uint8_t* mpdu, uint32_t len);

// Accumulates 4-way handshake messages per BSSID and reports the first moment a
// *crackable* pairing has been seen for an AP. Crackable here = we have M2 plus
// one of its neighbours (M1 or M3) — enough for an offline dictionary attack
// (ANonce+SNonce+MIC). Stateful but pure: no clock, no radio. The device tier
// owns one of these per capture session and resets it when the radio powers down.
class HandshakeTracker {
public:
    // Bounded so a busy channel can't grow this unboundedly (v1: extra APs past
    // the cap are ignored — a breadth counter, not an exhaustive audit).
    static constexpr int kMaxAps = 16;

    // 4-way message bit flags (msg 1..4 -> bit 1..4).
    static constexpr uint8_t kM1 = 1u << 1;
    static constexpr uint8_t kM2 = 1u << 2;
    static constexpr uint8_t kM3 = 1u << 3;
    static constexpr uint8_t kM4 = 1u << 4;

    // Record one classified message for `bssid`. Returns true EXACTLY ONCE per
    // BSSID — on the transition where that AP's handshake first becomes crackable.
    // A msg of 0 (non-4-way) or an unknown/full table is inert (returns false).
    bool observe(const uint8_t* bssid, int msg);

    // Drop all accumulated state (call when a capture session ends / radio down).
    void reset();

    int trackedAps() const { return count_; }

private:
    struct Ap {
        uint8_t bssid[6];
        uint8_t mask;      // OR of kM1..kM4 seen so far
        bool credited;     // already returned true once
    };
    static bool crackable(uint8_t mask) {
        // M2 is mandatory (it carries the supplicant nonce + MIC); pair it with an
        // authenticator message (M1 or M3) for a usable capture.
        return (mask & kM2) && ((mask & kM1) || (mask & kM3));
    }
    Ap aps_[kMaxAps] = {};
    int count_ = 0;
};

} // namespace mal
