// pcap.h — classic libpcap (pcap) file writer for the Audit capture slice.
//
// PURE + HOST-TESTABLE. This knows nothing about the radio or SD: it turns
// captured 802.11 frames into the on-disk libpcap byte stream (a 24-byte global
// header once, then a 16-byte record header + the frame bytes per packet). The
// device tier drives it with real EAPOL frames from promiscuous mode and points
// its sink at a file on SD; the native gates drive it with a memory sink and
// assert the exact bytes. Byte order is little-endian (the classic "a1b2c3d4"
// magic — a reader auto-detects endianness from it).
//
// AUTHORIZED USE / PASSIVE ONLY: the writer is a sink for frames the device
// already heard; it neither transmits nor injects. The capture policy (when it is
// allowed to run) lives in audit_capture.h.
#pragma once

#include <cstddef>
#include <cstdint>

namespace mal {

// --- libpcap classic format constants --------------------------------------
constexpr uint32_t kPcapMagic = 0xa1b2c3d4u;   // microsecond-resolution magic
constexpr uint16_t kPcapVersionMajor = 2;
constexpr uint16_t kPcapVersionMinor = 4;
// DLT_IEEE802_11 = 105: raw 802.11 MPDUs with no radiotap header. Wireshark reads
// EAPOL handshakes straight out of this link type.
constexpr uint32_t kPcapLinkTypeIEEE80211 = 105;
// Snap length declared in the header + the per-frame capture cap. A WPA 4-way
// handshake's EAPOL frames are well under this; 2324 is the classic 802.11 MPDU
// ceiling, so full frames are captured, never truncated.
constexpr uint32_t kPcapSnaplen = 2324;

constexpr int kPcapGlobalHeaderLen = 24;
constexpr int kPcapRecordHeaderLen = 16;

// Fill a 24-byte classic libpcap global (file) header, little-endian.
void writePcapGlobalHeader(uint8_t out[kPcapGlobalHeaderLen],
                           uint32_t snaplen = kPcapSnaplen,
                           uint32_t linkType = kPcapLinkTypeIEEE80211);

// Fill a 16-byte per-record (per-packet) header, little-endian. inclLen is the
// number of frame bytes actually stored (<= origLen, capped at snaplen).
void writePcapRecordHeader(uint8_t out[kPcapRecordHeaderLen], uint32_t tsSec,
                           uint32_t tsUsec, uint32_t inclLen, uint32_t origLen);

// Abstract byte sink: the seam that keeps the writer host-testable. Native gates
// implement it over a std::vector; the device implements it over a stdio FILE on
// the mounted SD card. write() returns false on any short/failed write.
struct PcapSink {
    virtual bool write(const uint8_t* data, size_t n) = 0;
    virtual ~PcapSink() = default;
};

// Streams a .pcap to a sink: the global header once (begin), then one record per
// captured frame (writeFrame). Frames longer than the snaplen are stored
// truncated to snaplen with origLen recording the true length (standard libpcap
// semantics), so a reader still sees the real frame size.
class PcapWriter {
public:
    explicit PcapWriter(uint32_t snaplen = kPcapSnaplen) : snaplen_(snaplen) {}

    // Emit the global header. Must be called once before writeFrame(). Idempotent
    // guard: a second call is a no-op success (the header is only written once).
    bool begin(PcapSink& sink);

    // Append one captured frame (record header + up to snaplen frame bytes).
    // Fails (without counting the frame) if begin() has not run or the sink write
    // fails partway.
    bool writeFrame(PcapSink& sink, const uint8_t* frame, uint32_t len,
                    uint32_t tsSec, uint32_t tsUsec);

    bool begun() const { return begun_; }
    uint32_t frames() const { return frames_; }        // records written
    uint32_t bytesWritten() const { return bytes_; }   // total bytes emitted
    uint32_t snaplen() const { return snaplen_; }

private:
    uint32_t snaplen_;
    uint32_t frames_ = 0;
    uint32_t bytes_ = 0;
    bool begun_ = false;
};

} // namespace mal
