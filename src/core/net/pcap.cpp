#include "core/net/pcap.h"

namespace mal {

namespace {
// Little-endian scalar writers into a byte buffer at an offset.
inline void putU16(uint8_t* p, uint16_t v) {
    p[0] = static_cast<uint8_t>(v);
    p[1] = static_cast<uint8_t>(v >> 8);
}
inline void putU32(uint8_t* p, uint32_t v) {
    p[0] = static_cast<uint8_t>(v);
    p[1] = static_cast<uint8_t>(v >> 8);
    p[2] = static_cast<uint8_t>(v >> 16);
    p[3] = static_cast<uint8_t>(v >> 24);
}
} // namespace

void writePcapGlobalHeader(uint8_t out[kPcapGlobalHeaderLen], uint32_t snaplen,
                           uint32_t linkType) {
    putU32(out + 0, kPcapMagic);
    putU16(out + 4, kPcapVersionMajor);
    putU16(out + 6, kPcapVersionMinor);
    putU32(out + 8, 0);          // thiszone: GMT-to-local offset, always 0
    putU32(out + 12, 0);         // sigfigs: timestamp accuracy, always 0
    putU32(out + 16, snaplen);   // max captured length per packet
    putU32(out + 20, linkType);  // data link type (105 = IEEE 802.11)
}

void writePcapRecordHeader(uint8_t out[kPcapRecordHeaderLen], uint32_t tsSec,
                           uint32_t tsUsec, uint32_t inclLen, uint32_t origLen) {
    putU32(out + 0, tsSec);
    putU32(out + 4, tsUsec);
    putU32(out + 8, inclLen);    // bytes of this record actually present in the file
    putU32(out + 12, origLen);   // length this packet had on the wire
}

bool PcapWriter::begin(PcapSink& sink) {
    if (begun_) return true;
    uint8_t hdr[kPcapGlobalHeaderLen];
    writePcapGlobalHeader(hdr, snaplen_, kPcapLinkTypeIEEE80211);
    if (!sink.write(hdr, sizeof(hdr))) return false;
    bytes_ += sizeof(hdr);
    begun_ = true;
    return true;
}

bool PcapWriter::writeFrame(PcapSink& sink, const uint8_t* frame, uint32_t len,
                            uint32_t tsSec, uint32_t tsUsec) {
    if (!begun_ || frame == nullptr) return false;
    const uint32_t incl = (len > snaplen_) ? snaplen_ : len;  // cap, keep origLen
    uint8_t rec[kPcapRecordHeaderLen];
    writePcapRecordHeader(rec, tsSec, tsUsec, incl, len);
    if (!sink.write(rec, sizeof(rec))) return false;
    if (incl > 0 && !sink.write(frame, incl)) return false;
    bytes_ += sizeof(rec) + incl;
    ++frames_;
    return true;
}

} // namespace mal
