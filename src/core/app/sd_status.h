// sd_status.h — portable microSD presence the platform tier feeds in.
//
// Mounting the card is device-tier work (SD_MMC/SDIO GPIO, sd_esp32.h), but the
// *shape* of the reading lives here so the CFG "SD" line and any tests can read it
// on the native host. The device layer fills an SdStatus at boot from SdCard and
// hands it to Game::setSdStatus(); the CFG System Info "SD" line renders it.
// Host/card-less builds leave present=false → "ABSENT" (dual-coded, same as the
// BATT "-" absent state).
#pragma once

#include <cstdint>

namespace mal {

struct SdStatus {
    bool present = false;   // card mounted AND its filesystem is usable (round-trip)
    uint32_t sizeMB = 0;    // capacity in MB, 0 when absent/unknown
};

} // namespace mal
