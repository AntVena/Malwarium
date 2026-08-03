// pcap_sd_esp32.h — device-tier SD sink for the Audit.pcap writer.
//
// The concrete PcapSink that lands captured frames on the mounted microSD: a thin
// stdio wrapper (the card is mounted via esp_vfs_fat at /sdcard, so POSIX file I/O
// works). Pairs the portable, host-tested PcapWriter (core/net/pcap.h) with real
// hardware — the writer builds the bytes, this sink flushes them to a file.
//
// This is the write half of the capture path; the RF/promiscuous half (sniffing
// 802.11, detecting EAPOL, timestamping frames, and calling PcapWriter::writeFrame
// + Game::auditCapture().noteHandshake()) is DEVICE-ONLY and still to be wired +
// on-air verified against a live AP/phone/router on an RF bench (deferred with a
// note in docs). Requires SD mounted (sd_esp32.h) first.
//
// AUTHORIZED USE / PASSIVE ONLY — this only records frames already heard.
#pragma once

#include "config.h"

#if SD_ENABLED && SD_USE_SDMMC
#  include <cstdio>
#  include <unistd.h>   // fsync — commit FatFS dir entry + data to the card

#  include "core/net/pcap.h"

namespace mal {

// A PcapSink backed by an open stdio FILE on the mounted SD card. A .pcap has ONE
// global header followed by records, so each session gets its OWN fresh file and
// is opened WRITE (truncate) — never append. Appending to an existing .pcap (e.g.
// reusing a filename across reboots) concatenates a second global header mid-
// stream, which is an invalid capture; the caller derives the path from the
// network being captured (net_capture.h names it after the SSID/BSSID, opened
// lazily once per session into a temp file — see its drain()/openSessionFor()/
// placeSession()), so a re-capture attempt for the SAME network keeps whichever
// capture is larger rather than accumulating or clobbering the better one.
class SdPcapSink : public PcapSink {
public:
    // Create/truncate `path` on the card. Returns false if the card isn't mounted
    // or the file can't be opened. Path is a full VFS path, e.g. "/sdcard/cap.pcap".
    bool open(const char* path) {
        close();
        f_ = std::fopen(path, "wb");
        return f_ != nullptr;
    }

    bool write(const uint8_t* data, size_t n) override {
        if (!f_) return false;
        return std::fwrite(data, 1, n, f_) == n;
    }

    // Make the file durable, not just buffered. fflush() only pushes the stdio
    // buffer into FatFS; FatFS defers the directory-entry (file-size) + data
    // commit until f_sync/f_close, so WITHOUT the fsync a card-pull or power loss
    // mid-session leaves the .pcap at 0 bytes on disk. fsync() maps to f_sync and
    // commits it. Called only on the coarse (~5s) flush + on seal, so the extra
    // cost is bounded and the worst-case loss on an unexpected removal is the last
    // flush interval, not the whole session.
    void flush() {
        if (!f_) return;
        std::fflush(f_);
        ::fsync(fileno(f_));
    }

    void close() {
        if (f_) {
            std::fclose(f_);
            f_ = nullptr;
        }
    }

    bool isOpen() const { return f_ != nullptr; }

    ~SdPcapSink() override { close(); }

private:
    FILE* f_ = nullptr;
};

} // namespace mal

#endif // SD_ENABLED && SD_USE_SDMMC
