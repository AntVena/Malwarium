// save_store_esp32.h — device ISaveStore backed by NVS (Arduino Preferences).
// The opaque save blob lives under NVS_NAMESPACE/NVS_SAVE_KEY, in
// NVS_SAVE_PARTITION (config.h) — a dedicated partition on boards that carry
// one (partitions_malwarium.csv), or NULL (the stock `nvs` partition) on
// boards that don't. Preferences is opened per call (cheap) so the namespace
// handle is never left dangling across the long idle periods between saves.
#pragma once

#include <Preferences.h>

#include <vector>

#include "config.h"
#include "platform/platform.h"

namespace mal {

class NvsSaveStore : public ISaveStore {
public:
    std::vector<uint8_t> load() override {
        std::vector<uint8_t> out;
        Preferences p;
        if (!p.begin(NVS_NAMESPACE, /*readOnly=*/true, NVS_SAVE_PARTITION)) return out;
        const size_t n = p.getBytesLength(NVS_SAVE_KEY);
        if (n > 0) {
            out.resize(n);
            if (p.getBytes(NVS_SAVE_KEY, out.data(), n) != n) out.clear();
        }
        p.end();
        return out;
    }

    bool save(const std::vector<uint8_t>& data) override {
        Preferences p;
        if (!p.begin(NVS_NAMESPACE, /*readOnly=*/false, NVS_SAVE_PARTITION)) {
            Serial.println("[save] NVS open (rw) FAILED — save blob NOT written");
            return false;
        }
        const size_t wrote = p.putBytes(NVS_SAVE_KEY, data.data(), data.size());
        p.end();
        // A short write (or 0) means the on-flash blob is now STALE relative to RAM —
        // the very failure mode that reads as "progress silently reverted" after the
        // next reboot/reflash (NVS is untouched by reflashing; only a failed write
        // here can desync it from live state). Surfaced on the same boot-line tier as
        // the "[save] loaded NVS" confirmation, so it's visible without a debugger.
        if (wrote != data.size())
            Serial.printf("[save] NVS write FAILED: wrote %u/%u bytes\n",
                          (unsigned)wrote, (unsigned)data.size());
        return wrote == data.size();
    }

    void clear() override {
        Preferences p;
        if (!p.begin(NVS_NAMESPACE, /*readOnly=*/false, NVS_SAVE_PARTITION)) return;
        p.remove(NVS_SAVE_KEY);
        p.end();
    }
};

} // namespace mal
