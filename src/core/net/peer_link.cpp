#include "core/net/peer_link.h"

#include <cstring>

#include "core/net/wire_field.h"

namespace mal {

namespace {

// Frame lead-in. Present so a stray non-Malwarium broadcast that happens to be the
// right length is rejected on content rather than parsed as a peer.
constexpr uint8_t kMagic[4] = {'M', 'L', 'W', 'M'};

// Field offsets. Written out rather than derived from a packed struct so host and
// device compilers can never disagree about padding — the same reason
// NetworkLedger writes its records field by field.
constexpr size_t kOffMagic = 0;
constexpr size_t kOffVersion = 4;
constexpr size_t kOffFlags = 5;
constexpr size_t kOffStage = 6;
constexpr size_t kOffRank = 7;
constexpr size_t kOffReserved = 8;   // pad to keep the string block byte-aligned
constexpr size_t kOffTag = 9;
constexpr size_t kOffPet = kOffTag + kPeerTagCap;
constexpr size_t kOffCrew = kOffPet + kPeerPetCap;

static_assert(kOffCrew + kPeerCrewCap == kPeerHelloSize, "field map must fill the frame");

constexpr uint8_t kFlagCrewRed = 0x01;

}  // namespace

size_t encodePeerHello(const PeerHello& in, uint8_t* out, size_t cap) {
    if (!out || cap < kPeerHelloSize) return 0;

    std::memset(out, 0, kPeerHelloSize);
    std::memcpy(out + kOffMagic, kMagic, sizeof(kMagic));
    out[kOffVersion] = kPeerProtoVersion;
    out[kOffFlags] = in.crewRed ? kFlagCrewRed : 0;
    out[kOffStage] = in.stage;
    out[kOffRank] = in.rank;
    out[kOffReserved] = 0;

    writeWireField(out + kOffTag, in.tag, kPeerTagCap);
    writeWireField(out + kOffPet, in.petName, kPeerPetCap);
    writeWireField(out + kOffCrew, in.crewName, kPeerCrewCap);
    return kPeerHelloSize;
}

bool decodePeerHello(const uint8_t* in, size_t len, PeerHello* out) {
    if (!in || !out) return false;
    // Exact length: every field is fixed-width, so a short frame would read past
    // the buffer and a long one is not something this version produced.
    if (len != kPeerHelloSize) return false;
    if (std::memcmp(in + kOffMagic, kMagic, sizeof(kMagic)) != 0) return false;
    // Unknown version: the field map below is only valid for layouts this build
    // knows. Refusing beats guessing — a newer peer is invisible, not garbled.
    if (in[kOffVersion] != kPeerProtoVersion) return false;

    PeerHello h;
    h.crewRed = (in[kOffFlags] & kFlagCrewRed) != 0;
    h.stage = in[kOffStage];
    h.rank = in[kOffRank];
    readWireField(in + kOffTag, h.tag, kPeerTagCap);
    readWireField(in + kOffPet, h.petName, kPeerPetCap);
    readWireField(in + kOffCrew, h.crewName, kPeerCrewCap);
    *out = h;
    return true;
}

uint64_t packPeerKey(const uint8_t* mac) {
    if (!mac) return 0;
    uint64_t key = 0;
    for (int i = 0; i < 6; ++i) key = (key << 8) | mac[i];
    return key;
}

void unpackPeerKey(uint64_t key, uint8_t* mac) {
    if (!mac) return;
    for (int i = 0; i < 6; ++i) mac[i] = static_cast<uint8_t>((key >> (8 * (5 - i))) & 0xFF);
}

}  // namespace mal
