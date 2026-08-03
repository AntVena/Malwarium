#include "core/net/pvp_link.h"

#include <cstring>

#include "core/net/wire_field.h"

namespace mal {

namespace {

// Frame lead-in. A DIFFERENT magic from the HELLO beacon's "MLWM" so the two formats
// can share one ESP-NOW receive callback and be told apart on the first four bytes —
// a duel frame is never misread as a beacon, or the reverse.
constexpr uint8_t kMagic[4] = {'M', 'L', 'W', 'P'};

// Header offsets. Written out rather than derived from a packed struct so host and
// device compilers can never disagree about padding.
constexpr size_t kOffMagic = 0;
constexpr size_t kOffVersion = 4;
constexpr size_t kOffType = 5;
constexpr size_t kOffSession = 6;
constexpr size_t kOffBody = kPvpHeaderSize;

static_assert(kOffSession + 4 == kPvpHeaderSize, "header map must fill the header");

void writeU16(uint8_t* out, uint16_t v) {
    out[0] = static_cast<uint8_t>(v & 0xFF);
    out[1] = static_cast<uint8_t>((v >> 8) & 0xFF);
}

uint16_t readU16(const uint8_t* in) {
    return static_cast<uint16_t>(in[0] | (static_cast<uint16_t>(in[1]) << 8));
}

void writeU32(uint8_t* out, uint32_t v) {
    for (int i = 0; i < 4; ++i) out[i] = static_cast<uint8_t>((v >> (8 * i)) & 0xFF);
}

uint32_t readU32(const uint8_t* in) {
    uint32_t v = 0;
    for (int i = 0; i < 4; ++i) v |= static_cast<uint32_t>(in[i]) << (8 * i);
    return v;
}

// Lay the header down and return the offset the body starts at. Callers have already
// checked `cap`.
size_t writeHeader(uint8_t* out, PvpFrameType type, uint32_t session) {
    std::memcpy(out + kOffMagic, kMagic, sizeof(kMagic));
    out[kOffVersion] = kPvpProtoVersion;
    out[kOffType] = static_cast<uint8_t>(type);
    writeU32(out + kOffSession, session);
    return kOffBody;
}

// A fighter's fixed-width block. Order here is the ONLY definition of the layout —
// pvpCommitHash hashes the same bytes, so the two stay in step by construction.
void writeFighter(uint8_t* out, const PvpFighter& f) {
    size_t o = 0;
    writeWireField(out + o, f.tag, kPvpTagCap);
    o += kPvpTagCap;
    writeWireField(out + o, f.creatureId, kPvpIdCap);
    o += kPvpIdCap;
    for (int i = 0; i < kMaxMoveSlots; ++i) {
        writeWireField(out + o, f.moveIds[i], kPvpIdCap);
        o += kPvpIdCap;
    }
    for (int i = 0; i < kModSlots; ++i) {
        writeWireField(out + o, f.modIds[i], kPvpIdCap);
        o += kPvpIdCap;
    }
    writeU16(out + o, f.level);
    o += 2;
    for (int i = 0; i < 4; ++i) out[o + i] = f.statPoints[i];
}

void readFighter(const uint8_t* in, PvpFighter* f) {
    size_t o = 0;
    readWireField(in + o, f->tag, kPvpTagCap);
    o += kPvpTagCap;
    readWireField(in + o, f->creatureId, kPvpIdCap);
    o += kPvpIdCap;
    for (int i = 0; i < kMaxMoveSlots; ++i) {
        readWireField(in + o, f->moveIds[i], kPvpIdCap);
        o += kPvpIdCap;
    }
    for (int i = 0; i < kModSlots; ++i) {
        readWireField(in + o, f->modIds[i], kPvpIdCap);
        o += kPvpIdCap;
    }
    f->level = readU16(in + o);
    o += 2;
    for (int i = 0; i < 4; ++i) f->statPoints[i] = in[o + i];
}

// The declared length of a frame type, or 0 for a type this build doesn't know.
size_t frameSize(PvpFrameType type) {
    switch (type) {
        case PvpFrameType::Invite: return kPvpInviteSize;
        case PvpFrameType::Accept: return kPvpAcceptSize;
        case PvpFrameType::Decline: return kPvpDeclineSize;
        case PvpFrameType::Start: return kPvpStartSize;
        case PvpFrameType::Bye: return kPvpByeSize;
    }
    return 0;
}

// Encode INVITE and ACCEPT — identical bodies (a rules byte then a fighter), differing
// only in which side of the handshake they are.
size_t encodeOffer(PvpFrameType type, uint32_t session, const PvpFighter& f,
                   uint8_t* out, size_t cap) {
    if (!out || cap < kPvpInviteSize) return 0;
    std::memset(out, 0, kPvpInviteSize);
    size_t o = writeHeader(out, type, session);
    out[o++] = kPvpRulesVersion;
    writeFighter(out + o, f);
    return kPvpInviteSize;
}

}  // namespace

size_t encodePvpInvite(uint32_t session, const PvpFighter& f, uint8_t* out, size_t cap) {
    return encodeOffer(PvpFrameType::Invite, session, f, out, cap);
}

size_t encodePvpAccept(uint32_t session, const PvpFighter& f, uint8_t* out, size_t cap) {
    return encodeOffer(PvpFrameType::Accept, session, f, out, cap);
}

size_t encodePvpDecline(uint32_t session, PvpDeclineReason reason, uint8_t* out,
                        size_t cap) {
    if (!out || cap < kPvpDeclineSize) return 0;
    std::memset(out, 0, kPvpDeclineSize);
    const size_t o = writeHeader(out, PvpFrameType::Decline, session);
    out[o] = static_cast<uint8_t>(reason);
    return kPvpDeclineSize;
}

size_t encodePvpStart(uint32_t session, uint32_t seed, uint32_t commit, uint8_t* out,
                      size_t cap) {
    if (!out || cap < kPvpStartSize) return 0;
    std::memset(out, 0, kPvpStartSize);
    const size_t o = writeHeader(out, PvpFrameType::Start, session);
    writeU32(out + o, seed);
    writeU32(out + o + 4, commit);
    return kPvpStartSize;
}

size_t encodePvpBye(uint32_t session, uint8_t* out, size_t cap) {
    if (!out || cap < kPvpByeSize) return 0;
    std::memset(out, 0, kPvpByeSize);
    writeHeader(out, PvpFrameType::Bye, session);
    return kPvpByeSize;
}

bool isPvpFrame(const uint8_t* in, size_t len) {
    return in && len >= kPvpHeaderSize &&
           std::memcmp(in + kOffMagic, kMagic, sizeof(kMagic)) == 0;
}

bool decodePvpFrame(const uint8_t* in, size_t len, PvpFrame* out) {
    if (!in || !out) return false;
    if (len < kPvpHeaderSize) return false;
    if (std::memcmp(in + kOffMagic, kMagic, sizeof(kMagic)) != 0) return false;
    // Unknown version: the field maps below are only valid for layouts this build
    // knows. Refusing beats guessing — a newer peer's duel is unreachable, not garbled.
    if (in[kOffVersion] != kPvpProtoVersion) return false;

    const PvpFrameType type = static_cast<PvpFrameType>(in[kOffType]);
    const size_t want = frameSize(type);
    // An unknown type reports size 0, which no frame can match — so it falls out here
    // alongside a wrong-length frame of a known type.
    if (want == 0 || len != want) return false;

    PvpFrame f;
    f.type = type;
    f.session = readU32(in + kOffSession);
    switch (type) {
        case PvpFrameType::Invite:
        case PvpFrameType::Accept:
            f.rules = in[kOffBody];
            readFighter(in + kOffBody + 1, &f.fighter);
            break;
        case PvpFrameType::Decline: {
            const uint8_t r = in[kOffBody];
            // An unrecognised reason still means "no" — degrade to the plain refusal
            // rather than dropping a frame whose only job is to end the wait.
            f.reason = r <= static_cast<uint8_t>(PvpDeclineReason::Mismatch)
                           ? static_cast<PvpDeclineReason>(r)
                           : PvpDeclineReason::Refused;
            break;
        }
        case PvpFrameType::Start:
            f.seed = readU32(in + kOffBody);
            f.commit = readU32(in + kOffBody + 4);
            break;
        case PvpFrameType::Bye:
            break;
    }
    *out = f;
    return true;
}

bool pvpHostIsLocal(uint64_t localKey, uint64_t remoteKey) {
    return localKey < remoteKey;
}

uint32_t pvpCommitHash(uint32_t seed, const PvpFighter& host, const PvpFighter& guest) {
    // FNV-1a over the seed then both fighters' WIRE bytes. Hashing the encoded form
    // (rather than the struct) is what makes this comparable across the link at all:
    // it is the only representation both devices provably agree on, and it reuses
    // writeFighter so the hash can never drift from the layout it is guarding.
    uint32_t h = 2166136261u;
    auto feed = [&h](const uint8_t* p, size_t n) {
        for (size_t i = 0; i < n; ++i) {
            h ^= p[i];
            h *= 16777619u;
        }
    };

    uint8_t sbuf[4];
    writeU32(sbuf, seed);
    feed(sbuf, sizeof(sbuf));

    uint8_t fbuf[kPvpFighterSize];
    writeFighter(fbuf, host);
    feed(fbuf, sizeof(fbuf));
    writeFighter(fbuf, guest);
    feed(fbuf, sizeof(fbuf));
    return h;
}

}  // namespace mal
