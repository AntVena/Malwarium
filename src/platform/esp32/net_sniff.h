// net_sniff.h — device-tier passive network-discovery scan.
//
// Feeds the engine's lifetime NETS counter (Game::networksSeen_) from the REAL
// radio: a periodic, PASSIVE Wi-Fi scan that counts distinct networks the device
// can actually hear (dedup by BSSID lives engine-side, Game::registerNetwork).
// Before this, that counter was simulated — it only ticked from the in-game
// explore event's fake pool (game.cpp startWifiEvent, "no real radio, ").
//
// AUTHORIZED USE — PASSIVE ONLY. This scans/listens; it never deauths, injects,
// or captures a handshake (that's the separate Audit.pcap slice,, not
// built here). Whether it runs at all is a RUNTIME choice, asked as
// Game::radioScanWanted(): the player's persisted "AUDIT SCAN" toggle in CFG
// (Game::netScanEnabled(), save v5), plus the transient stretches the engine raises it
// itself for a screen that needs live results. The NET_SNIFF_ENABLED macro is only a
// board-capability gate (is a Wi-Fi radio present / audit feature built).
// Not wanted ⇒ no radio activity at all (the radio is powered down).
//
// The scan is ASYNC (WiFi.scanNetworks(async)) so a 2–3s scan never stalls the
// ~4fps render loop; poll() just harvests a completed scan.
//
// DUTY-CYCLED RADIO. A sweep needs the radio for ~2 to 3s but recurs only every
// NET_SNIFF_SCAN_INTERVAL_MS (30s). So wants() is schedule-aware: it claims the
// radio only while a sweep is in flight or the next one is due, and releases it the
// instant a sweep harvests. Between sweeps the arbiter sees no owner and powers the
// radio fully down. That is the point: the main loop can then light-sleep the SoC
// through the gap (platform/esp32/main.cpp) instead of holding a ~90mA radio hot for
// a ~2.5s/30s duty. msUntilNextSweep() hands the loop the gap length so it wakes on
// the normal cadence rather than at the far longer idle-sleep fallback. Ambient LINK,
// which borrowed the between-sweeps hot window, now gets one piggyback per sweep
// (radio_arbiter.h) rather than a continuously-open receiver.
#pragma once

#include <cstdint>

#include "config.h"
#include "core/app/game.h"
#include "platform/esp32/ssid_cache.h"

#if NET_SNIFF_ENABLED
#  include <WiFi.h>
#endif

namespace mal {

class NetSniff {
public:
    // Wire the engine seam. Does NOT power the radio — the RadioArbiter decides
    // when this owner may run (service) vs must stand down (ensureDown), so a
    // device with Audit off (or capture owning the radio) never radiates here.
    void begin(Game* game) { game_ = game; }

    // Whether the scan should OWN the radio this instant. Two gates, both required:
    // the engine wants scanning at all (Game::radioScanWanted(): the CFG "AUDIT SCAN"
    // toggle plus the transient raise for a screen that needs live results, the CREW
    // home-network picker), AND there is work to do right now, i.e. a sweep is in
    // flight or the rescan interval has elapsed. Between sweeps the second gate is
    // false, so the arbiter stands this owner down and powers the radio off for the
    // gap (see the banner). The arbiter reads this to decide the radio owner; keeps the
    // read in one place rather than duplicated at the call site.
    bool wants(uint32_t nowMs) const {
#if NET_SNIFF_ENABLED
        return game_ && game_->radioScanWanted() && (scanning_ || dueForSweep(nowMs));
#else
        (void)nowMs;
        return false;
#endif
    }

    // ms until the scan next needs the radio: 0 when it wants it now (mid-sweep or the
    // interval has elapsed), the remaining gap when dormant, and UINT32_MAX ("no bound")
    // when scanning isn't wanted at all. The main loop clamps its idle light-sleep to
    // this so a powered-down gap still ends on the rescan cadence (platform/esp32/main.cpp).
    uint32_t msUntilNextSweep(uint32_t nowMs) const {
#if NET_SNIFF_ENABLED
        if (!game_ || !game_->radioScanWanted()) return UINT32_MAX;
        if (scanning_ || dueForSweep(nowMs)) return 0;
        return NET_SNIFF_SCAN_INTERVAL_MS - (nowMs - lastScanMs_);
#else
        (void)nowMs;
        return UINT32_MAX;
#endif
    }

    // Run one scan step, ASSUMING the arbiter has granted this owner the radio.
    // Brings the radio up in STA (disconnected — we only scan, never associate)
    // and harvests a completed async scan into the engine (one registerNetwork
    // per heard AP; engine dedups by BSSID). Non-blocking: a running scan just
    // returns. No-op when compiled out.
    void service(uint32_t nowMs) {
#if NET_SNIFF_ENABLED
        if (!game_) return;
        if (!radioOn_) {
            WiFi.mode(WIFI_STA);
            WiFi.disconnect(false, false);
            radioOn_ = true;
            lastScanMs_ = nowMs - NET_SNIFF_SCAN_INTERVAL_MS;  // sweep on this same call
            Serial.println("[sniff] radio up (STA, scan-only)");
        }

        if (scanning_) {
            const int n = WiFi.scanComplete();
            if (n == WIFI_SCAN_RUNNING) return;      // still scanning

            // A NEGATIVE result here is normally NOT a failure — it's "the results
            // haven't landed yet". The wrapper clears its scanning bit as soon as the
            // driver stops sweeping, but only fills in the count when the scan-done
            // event is dispatched later from the WiFi task, and WIFI_SCAN_FAILED is
            // what it reports in between. Calling scanDelete() on that transient wipes
            // the very results that were about to arrive, and since a wiped scan reads
            // as failed on the next poll too, the sweep could never yield anything.
            // So wait it out, and only treat a negative as real once a sweep has had
            // far longer than one could legitimately take.
            if (n < 0) {
                if (nowMs - scanStartMs_ < NET_SNIFF_SCAN_TIMEOUT_MS) return;
                WiFi.scanDelete();
                scanning_ = false;
                lastScanMs_ = nowMs - NET_SNIFF_SCAN_INTERVAL_MS + NET_SNIFF_RETRY_MS;
                return;
            }

            for (int i = 0; i < n; ++i) {
                uint8_t* bssid = WiFi.BSSID(i);      // 6 bytes, valid until scanDelete
                if (!bssid) continue;
                // Named String (not just its .c_str()) so the buffer stays alive
                // across both calls below — a temporary's c_str() pointer would
                // dangle the moment the temporary is destroyed.
                String ssid = WiFi.SSID(i);
                game_->registerNetwork(bssid, ssid.c_str());
                // Stash BSSID→SSID so the audit capture (net_capture.h) can name a
                // .pcap after the network name, not the raw BSSID.
                SsidCache::instance().put(bssid, ssid.c_str());
            }
            Serial.printf("[sniff] sweep heard=%d\n", n);
            WiFi.scanDelete();                       // free results for the next sweep
            scanning_ = false;
            lastScanMs_ = nowMs;
            return;
        }

        if (dueForSweep(nowMs)) {
            // async = true (non-blocking), show_hidden = false (passive discovery).
            WiFi.scanNetworks(true, false);
            scanStartMs_ = nowMs;
            scanning_ = true;
        }
#else
        (void)nowMs;
#endif
    }

    // Fully power the radio down (arbiter hands the radio to another owner, or no
    // owner wants it). Idempotent; safe to call every loop.
    void ensureDown() {
#if NET_SNIFF_ENABLED
        if (radioOn_) {
            if (scanning_) { WiFi.scanDelete(); scanning_ = false; }
            WiFi.mode(WIFI_OFF);
            radioOn_ = false;
        }
#endif
    }

    bool radioOn() const {
#if NET_SNIFF_ENABLED
        return radioOn_;
#else
        return false;
#endif
    }

    // The radio is up and no sweep is in flight — the gap between sweeps, which is
    // the only safe moment to lend the hot radio to another job. The RadioArbiter
    // reads this to hand the pet-to-pet LINK its ambient window (net_link.h), so
    // announcing costs that window rather than a whole power cycle. False while a
    // sweep is running: parking the channel mid-sweep would corrupt its results.
    bool idleBetweenSweeps() const {
#if NET_SNIFF_ENABLED
        return radioOn_ && !scanning_;
#else
        return false;
#endif
    }

private:
#if NET_SNIFF_ENABLED
    // A fresh sweep is due: the interval has elapsed since the last one harvested. The
    // single definition wants() and service() both read, so the "when to sweep" line
    // can't drift between the arbiter's claim and the actual sweep.
    bool dueForSweep(uint32_t nowMs) const {
        return nowMs - lastScanMs_ >= NET_SNIFF_SCAN_INTERVAL_MS;
    }
#endif

    Game* game_ = nullptr;
#if NET_SNIFF_ENABLED
    bool radioOn_ = false;
    bool scanning_ = false;
    uint32_t lastScanMs_ = 0;
    uint32_t scanStartMs_ = 0;   // deadline base for the "results haven't landed" wait
#endif
};

} // namespace mal
