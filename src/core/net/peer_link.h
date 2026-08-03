// peer_link.h — the pet-to-pet HELLO beacon's wire format.
//
// PURE + HOST-TESTABLE, like eapol.h/pcap.h/network_ledger.h: this is only the
// encode/decode of the bytes another Malwarium broadcasts about itself. The radio
// that carries them is the device tier (platform/esp32/net_link.h, ESP-NOW); the
// roster that remembers them is PeerLedger (peer_ledger.h). Neither is needed to
// test this, and this file pulls in no Arduino/ESP-IDF header.
//
// A HELLO answers exactly one question — "who is standing next to me?" — with the
// operator's identity (HackerTag), what they are currently raising (species +
// stage), and who they run with (crew + team). It is a BROADCAST announcement: it
// asks for nothing and changes no state on the receiver beyond their met-list.
//
// UNTRUSTED INPUT. Every field of a decoded HELLO is attacker-controlled — it
// arrives unauthenticated from any device in range. decodePeerHello sanitizes
// accordingly (see its contract): callers may render a decoded struct and write it
// to the ledger directly, and must not assume the sender was honest about it.
//
// Sizing note: one HELLO is kPeerHelloSize bytes against ESP-NOW v1's 250-byte
// per-frame limit, so a beacon is always a single frame with room to grow. Growth
// goes through kPeerProtoVersion — append fields and bump it; decode rejects a
// version it does not understand rather than misreading its layout.
#pragma once

#include <cstddef>
#include <cstdint>

#include "tunables.h"

namespace mal {

constexpr uint8_t kPeerProtoVersion = 1;

// Field capacities INCLUDE the terminator, and are what the wire reserves — a
// longer source string is truncated at encode, never overruns.
constexpr int kPeerTagCap = kHackerTagMax + 1;
constexpr int kPeerPetCap = 24;
constexpr int kPeerCrewCap = 28;

// Total encoded length. Fixed: every field is a fixed-width slot, so there is no
// length-prefix parsing and a short/long frame is rejected outright.
constexpr size_t kPeerHelloSize = 4 + 1 + 1 + 1 + 1 + 1 + kPeerTagCap + kPeerPetCap +
                                  kPeerCrewCap;

// What one device says about itself. Built from the live Game state by
// Game::buildPeerHello and consumed by PeerLedger::record.
struct PeerHello {
    char tag[kPeerTagCap] = {0};     // the sender's HackerTag
    char petName[kPeerPetCap] = {0};  // species displayName; empty = no pet yet
    char crewName[kPeerCrewCap] = {0};  // empty = not enlisted
    uint8_t stage = 0;               // Stage as its underlying value
    uint8_t rank = 0;                // Hacker Rank, clamped into a byte at encode
    bool crewRed = false;            // the crew's team; meaningless when crewName is empty
};

// Serialize `in` into `out`. Returns kPeerHelloSize on success, or 0 when `out` is
// null or `cap` is too small. Strings are truncated to their field capacity.
size_t encodePeerHello(const PeerHello& in, uint8_t* out, size_t cap);

// Parse a received frame. Returns false — leaving `out` untouched — unless the
// frame is exactly kPeerHelloSize bytes, carries the Malwarium magic, and declares
// a protocol version this build understands.
//
// On success every string field of `out` is guaranteed NUL-terminated and to
// contain only printable ASCII: a hostile or corrupt sender cannot inject control
// bytes into the framebuffer or into the ledger file through this path. Bytes
// outside that set are dropped, so a garbage field decodes short or empty rather
// than rejecting the whole frame.
bool decodePeerHello(const uint8_t* in, size_t len, PeerHello* out);

// Pack a 6-byte MAC into the low 48 bits of a uint64, the same key shape
// Game::registerNetwork uses for a BSSID. This is a peer's stable identity: the
// PeerLedger key, and what tells a re-sighting from a new operator.
uint64_t packPeerKey(const uint8_t* mac);

// The inverse: write a packed key back out as 6 MAC bytes. The roster and the in-range
// snapshot both store peers by key, so this is how a screen turns "the operator I
// selected" into an address to send a duel frame to (game_pvp.cpp).
void unpackPeerKey(uint64_t key, uint8_t* mac);

}  // namespace mal
