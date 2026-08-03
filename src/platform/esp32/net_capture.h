// net_capture.h — device-tier promiscuous 802.11 EAPOL sniffer (RF half).
//
// The RF half of the Audit .pcap handshake-capture slice. Puts the S3 radio into
// promiscuous mode, hops the 2.4GHz channel set, recognises WPA 4-way-handshake
// EAPOL frames (via the pure core/net/eapol.h classifier), timestamps each into
// the host-tested PcapWriter, and lazily lands them on a /sdcard/audit_<network>.pcap
// file (SdPcapSink) named after the network they belong to — the SSID (via the
// scan's SsidCache) when known, else the BSSID hex. The file opens only once a
// frame for a genuinely-new (not-already-credited) BSSID actually arrives, never
// per arm/seal radio cycle (see drain()'s comment for the file-blowup this fixes),
// is written via a temp file, and only replaces an existing capture of the same
// network if the new one is LARGER (see placeSession) — a header-only/partial
// session is discarded rather than left on the card.
// When a BSSID's handshake first becomes crackable it calls
// Game::registerHandshake(bssid) — which dedups SHAKES + drives the AuditCapture
// policy SM hot. Modelled on net_sniff.h's radio lifecycle (radioOn_ gate, full
// WIFI_OFF power-down when not wanted, poll-driven, non-blocking).
//
// AUTHORIZED USE / PASSIVE ONLY. This ONLY records frames already on the air:
// no deauth, no injection, no forcing a re-key. It waits for a handshake that
// happens on its own (a client joining/roaming) and recognises it. Whether it
// runs at all is the persisted, default-OFF "AUDIT CAPTURE" opt-in gated by the
// AuditCapture SM; the RadioArbiter guarantees it never shares the radio with the
// passive scan.
//
// BATTERY: the radio is the dominant draw and promiscuous RX keeps it hot
// continuously. This owner is powered ONLY while auditCapture_.capturing(); the
// SM's arm-window bounds a listen session, the arbiter's low-battery guard seals
// it, and the RX callback filters to DATA frames + enqueues only EAPOL so the SD
// write burst per session is tiny (a handful of small frames, flushed on seal).
//
// COMPILE GATE: needs a Wi-Fi radio (NET_SNIFF_ENABLED) AND the SDMMC .pcap sink
// (SD_ENABLED && SD_USE_SDMMC — S3 only). A card-less / SPI-SD board gets the
// inert stub below so both boards still build.
//
// ⚠ ON-AIR VERIFICATION PENDING (docs): the esp_wifi
// promiscuous path, the Arduino-WiFi(scan)/IDF-esp_wifi(capture) hand-off, and
// the frame→.pcap bytes need an RF bench (board + phone/router). The pure parts
// (EAPOL classify, handshake tracking, capture policy) are host-tested; this glue
// is not, and is written defensively until measured.
#pragma once

#include <cstdint>

#include "config.h"
#include "core/app/game.h"

#if NET_SNIFF_ENABLED && SD_ENABLED && SD_USE_SDMMC
#  define MAL_NET_CAPTURE 1
#else
#  define MAL_NET_CAPTURE 0
#endif

// Verbose RX diagnostics: the "[cap] rx data=/eapol=" heartbeat + the per-EAPOL
// trace. These were decisive bringing the sniffer up on air (proving promiscuous
// RX hears traffic + which 4-way message arrived), but are noisy for a shipping
// build. Default ON for now; define NET_CAPTURE_DEBUG 0 (build flag / config.h) to
// silence them. The real signal lines — "[cap] armed / handshake / sealed" and the
// arbiter's "[radio] owner" — are NEVER gated.
#ifndef NET_CAPTURE_DEBUG
#  define NET_CAPTURE_DEBUG 1
#endif

#if MAL_NET_CAPTURE
#  include <Arduino.h>
#  include <sys/stat.h>
#  include <cstdio>
#  include <cstring>

#  include "esp_timer.h"
#  include "esp_wifi.h"

#  include "core/net/eapol.h"
#  include "core/net/pcap.h"
#  include "core/net/pcap_naming.h"
#  include "platform/esp32/pcap_sd_esp32.h"
#  include "platform/esp32/ssid_cache.h"

namespace mal {

class NetCapture {
public:
    void begin(Game* game) { game_ = game; }

    // Does the policy want capture running right now? (The arbiter also applies
    // the low-battery guard before granting the radio.)
    bool wants() const {
        return game_ && game_->auditCapture().capturing();
    }

    // Run one capture step, ASSUMING the arbiter granted this owner the radio.
    // Powers the radio up on the first call (a session file opens lazily from
    // drain(), only once something genuinely new is heard), hops channels on the
    // dwell cadence, and drains the RX ring to SD (the WiFi task only enqueues;
    // SD I/O happens here on the main task so the callback never blocks).
    void service(uint32_t nowMs) {
        if (!game_) return;
        if (!radioOn_) powerUp(nowMs);

        drain(nowMs);  // queued EAPOL frames -> .pcap + ledger (+ may channel-lock)

        // Channel hop: a handshake lands on the AP's channel, so we rotate the
        // 2.4GHz trio — UNLESS we just heard EAPOL and locked onto its channel to
        // catch the rest of the 4-way (drain() extends hopHoldUntil_). Every hop
        // is time-on-air (battery), so dwell is tuned.
        if (nowMs >= hopHoldUntil_ && nowMs - lastHopMs_ >= kAuditChannelDwellMs) {
            lastHopMs_ = nowMs;
            chanIdx_ = (chanIdx_ + 1) % kAuditChannelCount;
            esp_wifi_set_channel(kAuditChannels[chanIdx_], WIFI_SECOND_CHAN_NONE);
        }

        // Coarse RX heartbeat: confirms promiscuous RX is actually hearing traffic
        // (data climbing) and whether any of it is EAPOL — the key diagnostic when
        // a handshake doesn't register. Debug-gated (noisy for a shipping build).
#if NET_CAPTURE_DEBUG
        if (nowMs - lastStatMs_ >= kStatIntervalMs) {
            lastStatMs_ = nowMs;
            Serial.printf("[cap] rx data=%lu eapol=%lu ch=%d\n",
                          (unsigned long)rxData_, (unsigned long)rxEapol_,
                          kAuditChannels[chanIdx_]);
        }
#endif

        // Deliberate, coarse flush (SD write bursts are slow + costly): never
        // per-frame. Also flushed on seal (powerDown).
        if (nowMs - lastFlushMs_ >= kFlushIntervalMs) {
            lastFlushMs_ = nowMs;
            sink_.flush();
        }
    }

    // Fully power the radio down and seal the session file (arbiter gave the radio
    // to another owner, or capture is no longer wanted). Idempotent.
    void ensureDown() {
        if (!radioOn_) return;
        powerDown();
    }

    bool radioOn() const { return radioOn_; }

private:
    // --- RX path (runs in the WiFi task) ------------------------------------
    // Free-function trampoline required by esp_wifi_set_promiscuous_rx_cb; routes
    // to the single active instance. Kept minimal: classify + copy into the ring.
    static inline NetCapture* s_active = nullptr;  // C++17 inline: no separate defn
    static void rxTrampoline(void* buf, wifi_promiscuous_pkt_type_t type) {
        if (!s_active || type != WIFI_PKT_DATA) return;
        auto* pkt = static_cast<wifi_promiscuous_pkt_t*>(buf);
        uint32_t len = pkt->rx_ctrl.sig_len;
        if (len > 4) len -= 4;  // drop the trailing FCS (not part of the MPDU)
        s_active->onFrame(pkt->payload, len, pkt->rx_ctrl.channel);
    }

    void onFrame(const uint8_t* frame, uint32_t len, uint8_t chan) {
        ++rxData_;  // every promiscuous DATA frame (RX-alive diagnostic)
        // Only EAPOL-Key frames are captured — a focused, minimal, passive .pcap
        // (not the whole air), which also keeps the SD burst + RAM ring tiny.
        EapolKeyInfo info = parseEapolKey(frame, len);
        if (!info.isEapolKey) return;
        ++rxEapol_;

        const uint32_t store = len > kMaxFrame ? kMaxFrame : len;
        portENTER_CRITICAL(&mux_);
        const uint16_t next = static_cast<uint16_t>((head_ + 1) % kRingDepth);
        if (next != tail_) {  // drop on overflow rather than block the WiFi task
            Slot& s = ring_[head_];
            const int64_t us = esp_timer_get_time();  // monotonic since boot (no RTC)
            s.tsSec = static_cast<uint32_t>(us / 1000000);
            s.tsUsec = static_cast<uint32_t>(us % 1000000);
            s.len = static_cast<uint16_t>(store);
            s.origLen = len;
            s.msg = static_cast<int8_t>(info.msg);
            s.chan = chan;
            for (int i = 0; i < 6; ++i) s.bssid[i] = info.bssid[i];
            for (uint32_t i = 0; i < store; ++i) s.data[i] = frame[i];
            head_ = next;
        }
        portEXIT_CRITICAL(&mux_);
    }

    // --- Drain (runs on the main task) --------------------------------------
    // Avoids the pcap file-count blowup: a file is opened LAZILY, at most
    // once per powered session, only once a frame for a genuinely-new BSSID
    // (not already in the SHAKES ledger) is actually heard — and it's named
    // after that network (see openSessionFor), not a counter. A repeat capture
    // attempt for an already-credited AP is dropped before ever touching SD.
    // (Opening eagerly in powerUp() would mint a fresh audit_N.pcap on every
    // arm/seal radio cycle — most empty — and the SM's rearm cadence makes that
    // cycle frequent.)
    void drain(uint32_t nowMs) {
        for (;;) {
            Slot local;
            portENTER_CRITICAL(&mux_);
            if (tail_ == head_) { portEXIT_CRITICAL(&mux_); break; }  // empty
            local = ring_[tail_];
            tail_ = static_cast<uint16_t>((tail_ + 1) % kRingDepth);
            portEXIT_CRITICAL(&mux_);

            // Focus on ONE target network per powered session (the EAPOL channel
            // -lock above already implies this in practice — only one AP's 4-way
            // is realistically mid-flight on the locked channel at a time). A
            // frame for a second, different BSSID while a target is already
            // active is dropped rather than juggling a second open file.
            if (hasActiveBssid_ && std::memcmp(local.bssid, activeBssid_, 6) != 0)
                continue;

            // Already fully captured in a prior session (the v7 SHAKES ledger,
            // survives a reboot) — skip entirely: no file, no write, no re-credit.
            // This is what stops a repeat walk past a known AP from minting
            // another .pcap.
            if (!hasActiveBssid_ && game_->handshakeAlreadyCaptured(local.bssid)) {
#if NET_CAPTURE_DEBUG
                Serial.printf("[cap] skip known bssid=%02x:%02x:%02x:%02x:%02x:%02x\n",
                              local.bssid[0], local.bssid[1], local.bssid[2],
                              local.bssid[3], local.bssid[4], local.bssid[5]);
#endif
                continue;
            }

            if (!hasActiveBssid_) {
                std::memcpy(activeBssid_, local.bssid, 6);
                hasActiveBssid_ = true;
                openSessionFor(local.bssid);   // first genuinely-new frame -> open
            }

            if (sink_.isOpen()) {
                writer_.writeFrame(sink_, local.data, local.origLen, local.tsSec,
                                   local.tsUsec);
            }
            // Per-EAPOL trace (rare frames): shows we're hearing the handshake and
            // which 4-way message — the diagnostic for a partial/mistimed capture.
            // Debug-gated; the per-AP "[cap] handshake" credit line below is not.
#if NET_CAPTURE_DEBUG
            Serial.printf("[cap] eapol bssid=%02x:%02x:%02x:%02x:%02x:%02x msg=%d ch=%d\n",
                          local.bssid[0], local.bssid[1], local.bssid[2],
                          local.bssid[3], local.bssid[4], local.bssid[5],
                          local.msg, local.chan);
#endif

            // Heard EAPOL -> lock onto that AP's channel for a few seconds to catch
            // the rest of the 4-way instead of hopping away mid-exchange.
            if (local.chan >= 1 && local.chan <= 14) {
                esp_wifi_set_channel(local.chan, WIFI_SECOND_CHAN_NONE);
                hopHoldUntil_ = nowMs + kAuditEapolHoldMs;
            }

            // A completed/crackable handshake feeds the engine seam ONCE per AP:
            // registerHandshake dedups SHAKES + drives the AuditCapture SM hot.
            if (tracker_.observe(local.bssid, local.msg)) {
                game_->registerHandshake(local.bssid);
                Serial.printf("[cap] handshake bssid=%02x:%02x:%02x:%02x:%02x:%02x "
                              "-> SHAKES=%d\n",
                              local.bssid[0], local.bssid[1], local.bssid[2],
                              local.bssid[3], local.bssid[4], local.bssid[5],
                              game_->handshakesSeen());
            }
        }
    }

    // --- Radio + session lifecycle ------------------------------------------
    void powerUp(uint32_t nowMs) {
        // Self-contained IDF init (tolerant of an already-inited stack left by the
        // Arduino WiFi scan path — the arbiter powers that fully down first, but we
        // don't assume it deinited). ⚠ coexistence unverified on bench.
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        esp_wifi_init(&cfg);                       // ESP_ERR_WIFI_INIT_STATE = already up
        esp_wifi_set_storage(WIFI_STORAGE_RAM);
        esp_wifi_set_mode(WIFI_MODE_NULL);
        esp_wifi_start();

        // DATA-only filter: EAPOL is a data frame, and skipping mgmt/ctrl frames
        // cuts callback wakeups (battery) and ring churn.
        wifi_promiscuous_filter_t filter = {};
        filter.filter_mask = WIFI_PROMIS_FILTER_MASK_DATA;
        esp_wifi_set_promiscuous_filter(&filter);
        s_active = this;
        esp_wifi_set_promiscuous_rx_cb(&rxTrampoline);
        esp_wifi_set_promiscuous(true);
        chanIdx_ = 0;
        esp_wifi_set_channel(kAuditChannels[chanIdx_], WIFI_SECOND_CHAN_NONE);

        // NOTE: no file is opened here. openSessionFor() is called lazily from
        // drain() the first time a frame for a genuinely-new BSSID is heard — see
        // the drain() comment for why (this avoids the file-count blowup).
        radioOn_ = true;
        lastHopMs_ = nowMs;
        lastFlushMs_ = nowMs;
        lastStatMs_ = nowMs;
        hopHoldUntil_ = 0;
        hasActiveBssid_ = false;
        rxData_ = rxEapol_ = 0;
        Serial.printf("[cap] armed ch=%d free=%u largest=%u\n", kAuditChannels[chanIdx_],
                      (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
    }

    void powerDown() {
        esp_wifi_set_promiscuous(false);
        esp_wifi_set_promiscuous_rx_cb(nullptr);
        s_active = nullptr;
        drain(millis());         // flush any last queued frames before closing
        esp_wifi_stop();
        esp_wifi_deinit();
        radioOn_ = false;

        if (sink_.isOpen()) {
            sink_.flush();
            Serial.printf("[cap] sealed %s frames=%lu bytes=%lu\n", targetPath_,
                          (unsigned long)writer_.frames(),
                          (unsigned long)writer_.bytesWritten());
            sink_.close();
            placeSession();   // keep-if-larger: temp -> network-named final, or drop
        }
        tracker_.reset();
        hasActiveBssid_ = false;
        head_ = tail_ = 0;
    }

    // Lazily opens ONE file for this powered session, the first time a frame for
    // a genuinely-new (not-already-credited) BSSID is heard. The FINAL name is
    // derived from the network — the SSID (from the scan's SsidCache) suffixed
    // with the last BSSID octets, or the BSSID hex when the network was never
    // scanned (see pcap_naming.h) — but the session writes to a fixed TEMP file
    // first. On seal (placeSession) the temp only replaces the final file if it's
    // LARGER (or the final doesn't exist yet), so a fresh partial capture can
    // never clobber a bigger/better capture of the same network with a smaller one.
    void openSessionFor(const uint8_t bssid[6]) {
        const char* ssid = SsidCache::instance().get(bssid);   // nullptr if unscanned
        buildPcapFilename(targetPath_, sizeof(targetPath_), "/sdcard/audit_", bssid,
                          ssid);
        std::snprintf(tmpPath_, sizeof(tmpPath_), "/sdcard/audit_.tmp.pcap");
        writer_ = PcapWriter{};        // fresh counters per session
        if (sink_.open(tmpPath_)) writer_.begin(sink_);
        Serial.printf("[cap] session file=%s (via %s)\n",
                      sink_.isOpen() ? targetPath_ : "(open failed)", tmpPath_);
    }

    // On-disk size of `path`, or -1 if it doesn't exist. Used to decide whether a
    // just-sealed temp capture is worth keeping over an existing file.
    static long fileSize(const char* path) {
        struct stat st;
        return (::stat(path, &st) == 0) ? static_cast<long>(st.st_size) : -1;
    }

    // Move the sealed temp capture to its network-named target IFF it beats what's
    // already there (bigger = a more complete handshake). A temp with no frames
    // (header only) is discarded outright — that empty "pcap that never turns into
    // anything" would otherwise litter the card.
    void placeSession() {
        if (tmpPath_[0] == '\0') return;
        const bool worthKeeping = writer_.frames() > 0;
        const long tmpSz = fileSize(tmpPath_);
        const long tgtSz = fileSize(targetPath_);
        if (worthKeeping && (tgtSz < 0 || tmpSz > tgtSz)) {
            if (tgtSz >= 0) std::remove(targetPath_);     // FatFS rename won't clobber
            std::rename(tmpPath_, targetPath_);
            Serial.printf("[cap] kept %s (%ld bytes, prev %ld)\n", targetPath_, tmpSz,
                          tgtSz);
        } else {
            std::remove(tmpPath_);
            Serial.printf("[cap] discarded temp (%ld bytes) — kept %s (%ld)\n", tmpSz,
                          tgtSz >= 0 ? targetPath_ : "(none)", tgtSz);
        }
        tmpPath_[0] = targetPath_[0] = '\0';
    }

    // --- Ring buffer --------------------------------------------------------
    // EAPOL frames are small; cap well under snaplen so the ring is a few KB. On
    // overflow (a burst faster than the main loop drains) we drop, never block.
    static constexpr uint32_t kMaxFrame = 256;
    static constexpr int kRingDepth = 12;
    static constexpr uint32_t kFlushIntervalMs = 5000;
    static constexpr uint32_t kStatIntervalMs = 10000;  // RX heartbeat cadence
    struct Slot {
        uint8_t data[kMaxFrame];
        uint32_t origLen;   // true frame length (pcap orig_len; may exceed stored)
        uint32_t tsSec, tsUsec;
        uint16_t len;       // bytes stored in data[]
        uint8_t bssid[6];
        uint8_t chan;       // 802.11 channel the frame was received on
        int8_t msg;
    };

    Game* game_ = nullptr;
    bool radioOn_ = false;
    int chanIdx_ = 0;
    uint32_t lastHopMs_ = 0;
    uint32_t lastFlushMs_ = 0;
    uint32_t lastStatMs_ = 0;
    uint32_t hopHoldUntil_ = 0;   // pause hopping until this ms (EAPOL channel-lock)
    char targetPath_[64] = {0};   // final network-named path (kept if it wins)
    char tmpPath_[32] = {0};      // scratch file this session writes to first
    bool hasActiveBssid_ = false;    // this session already targeted a BSSID
    uint8_t activeBssid_[6] = {0};   // which one (only set when hasActiveBssid_)
    volatile uint32_t rxData_ = 0;   // promiscuous DATA frames seen (WiFi task)
    volatile uint32_t rxEapol_ = 0;  // of which EAPOL-Key

    PcapWriter writer_;
    SdPcapSink sink_;
    HandshakeTracker tracker_;

    Slot ring_[kRingDepth];
    volatile uint16_t head_ = 0;   // written by the WiFi task
    volatile uint16_t tail_ = 0;   // written by the main task
    portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
};

} // namespace mal

#else  // !MAL_NET_CAPTURE — inert stub (card-less / SPI-SD / radio-less board)

namespace mal {
class Game;
class NetCapture {
public:
    void begin(Game*) {}
    bool wants() const { return false; }
    void service(uint32_t) {}
    void ensureDown() {}
    bool radioOn() const { return false; }
};
} // namespace mal

#endif // MAL_NET_CAPTURE
