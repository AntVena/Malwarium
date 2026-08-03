// sd_esp32.h — device-tier microSD bring-up (SD_MMC, 4-bit SDIO).
//
// Mounts the card over the 4-bit SDIO bus using the pins in
// config.h, and offers a write/read round-trip self-test. This is the
// prerequisite for the Audit.pcap capture slice and, later, the SD-
// backed 'Pedia store — but it stands alone: a clean mount + round-trip at boot
// is the whole acceptance for this step.
//
// Config-gated: the whole SDIO path compiles only when SD_ENABLED && SD_USE_SDMMC
// (the reference Waveshare S3). A card-less or SPI-SD board — one whose block
// sets SD_USE_SDMMC 0 — gets an inert stub, so it still builds. Every method is safe to
// call whether or not a card is present — begin() returns false and boot stays
// clean (no crash loop) when the slot is empty or the pins are wrong.
//
// WHY THE IDF DRIVER, NOT Arduino SD_MMC: this board's TF slot has NO external
// pull-ups on CMD/DAT — it relies on the SoC's INTERNAL pull-ups (the vendor
// 07_sd_card_test demo mounts it with nothing external, on Arduino core 3.x).
// Arduino-ESP32 2.0.x's SD_MMC.begin() wrapper builds its slot_config with
// flags=0 and NEVER sets SDMMC_SLOT_FLAG_INTERNAL_PULLUP (only core 3.x does),
// and the IDF SDMMC driver then actively *disables* the pin pull-ups — so card
// init never completes and cardType()==CARD_NONE even though the pins (CLK16/
// CMD15/D0-17/D1-18/D2-13/D3-14, identical to the vendor demo) are correct. We
// pin core 2.0.x for the rest of the device tier, so rather than bump the whole
// core we mount through esp_vfs_fat_sdmmc_mount() directly with the pull-up flag
// set — exactly what core 3.x does. File I/O then uses POSIX stdio on /sdcard.
#pragma once

#include <cstdint>
#include <cstring>

#include "config.h"
#if defined(__has_include) && __has_include(<sdkconfig.h>)
#  include <sdkconfig.h>          // CONFIG_IDF_TARGET_* (device build only)
#endif

// The 4-bit SDIO path drives the ESP32-S3's GPIO-matrix SDMMC host directly, so
// it only compiles on that SoC. Any other target — the classic ESP32 (no GPIO-
// matrix SDMMC fields) or a SPI-SD board — gets the inert stub. Gate
// on the actual build TARGET, not the board macro, so it stays correct even where
// board selection is overridden by build flags.
#if SD_ENABLED && SD_USE_SDMMC && defined(CONFIG_IDF_TARGET_ESP32S3)
#  define MAL_SD_SDMMC 1
#else
#  define MAL_SD_SDMMC 0
#endif

#if MAL_SD_SDMMC
#  include <Arduino.h>            // Serial (boot logging)
#  include <cstdio>              // POSIX file I/O on the mounted VFS
#  include "driver/sdmmc_host.h"
#  include "esp_vfs_fat.h"
#  include "sdmmc_cmd.h"
#endif

namespace mal {

class SdCard {
public:
#if MAL_SD_SDMMC
    // Mount the card over SDIO with internal pull-ups enabled. Returns true on a
    // successful mount. false = no card / wrong pins / bad card — caller logs and
    // continues (SD is optional for the core game).
    bool begin() {
        if (card_) return true;  // already mounted

        sdmmc_host_t host = SDMMC_HOST_DEFAULT();
        host.slot = SDMMC_HOST_SLOT_1;
        host.max_freq_khz = SDMMC_FREQ_DEFAULT;  // 20 MHz — conservative for init

        sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
        slot.clk = static_cast<gpio_num_t>(PIN_SD_CLK);
        slot.cmd = static_cast<gpio_num_t>(PIN_SD_CMD);
        slot.d0 = static_cast<gpio_num_t>(PIN_SD_D0);
        slot.d1 = static_cast<gpio_num_t>(PIN_SD_D1);
        slot.d2 = static_cast<gpio_num_t>(PIN_SD_D2);
        slot.d3 = static_cast<gpio_num_t>(PIN_SD_D3);
        slot.width = SD_BUS_WIDTH;  // 4
        // The one line the Arduino 2.0.x SD_MMC wrapper omits (see header note) —
        // without it card init never even starts on this pull-up-less TF slot.
        slot.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

        const esp_vfs_fat_mount_config_t mnt = {
            /*format_if_mount_failed=*/false,  // NEVER format the user's card
            /*max_files=*/5,
            /*allocation_unit_size=*/0,
        };

        host.flags = SDMMC_HOST_FLAG_4BIT;
        lastErr_ = esp_vfs_fat_sdmmc_mount(kMount, &host, &slot, &mnt, &card_);
        if (lastErr_ != ESP_OK) {
            card_ = nullptr;
            // Retry 1-bit (uses only CLK/CMD/D0) — some slots/cards are fussy in
            // 4-bit but come up in 1-bit.
            host.flags = SDMMC_HOST_FLAG_1BIT;
            slot.width = 1;
            lastErr_ = esp_vfs_fat_sdmmc_mount(kMount, &host, &slot, &mnt, &card_);
        }
        mounted_ = (lastErr_ == ESP_OK);
        if (!mounted_) card_ = nullptr;
        return mounted_;
    }

    // Runtime re-check: unmount (if mounted) then mount again, so a card inserted
    // AFTER boot is picked up without a reboot (CFG "SD RECHECK" -> Game seam ->
    // main.cpp). Returns the new mounted state. The caller must guarantee no open
    // file handle survives (main.cpp seals a live .pcap capture first) — unmounting
    // with the sink still open would corrupt it.
    bool recheck() {
        if (card_) {
            esp_vfs_fat_sdcard_unmount(kMount, card_);  // pairs with the mount below
            card_ = nullptr;
            mounted_ = false;
        }
        return begin();
    }

    // esp_err_t of the last begin() (for the boot log). ESP_FAIL after a card is
    // seated == the card initialised but its filesystem isn't FAT-mountable
    // (exFAT / unformatted) — reformat FAT32; the pins/pull-ups are fine.
    const char* lastError() const { return esp_err_to_name(lastErr_); }

    bool mounted() const { return mounted_; }

    // Card capacity in MB (0 if unmounted). Handy for a one-line boot report.
    uint32_t sizeMB() const {
        if (!mounted_ || !card_) return 0;
        const uint64_t bytes = static_cast<uint64_t>(card_->csd.capacity) *
                               card_->csd.sector_size;
        return static_cast<uint32_t>(bytes / (1024ULL * 1024ULL));
    }

    // Write a marker file, read it back, and compare — the round-trip that proves
    // the filesystem is actually usable (not just mounted). Leaves the file behind
    // as a visible artifact. Returns true only on a byte-exact match.
    bool selfTest(const char* path = "/sdcard/malwarium_sd_test.txt") {
        if (!mounted_) return false;
        static const char kPayload[] = "malwarium-sd-ok";
        {
            FILE* f = std::fopen(path, "wb");
            if (!f) return false;
            std::fwrite(kPayload, 1, std::strlen(kPayload), f);
            std::fclose(f);
        }
        char buf[sizeof(kPayload) + 4] = {0};
        {
            FILE* f = std::fopen(path, "rb");
            if (!f) return false;
            const size_t n = std::fread(buf, 1, sizeof(buf) - 1, f);
            buf[n] = '\0';
            std::fclose(f);
        }
        return std::strcmp(buf, kPayload) == 0;
    }

private:
    sdmmc_card_t* card_ = nullptr;
    bool mounted_ = false;
    esp_err_t lastErr_ = ESP_OK;
    static constexpr const char* kMount = "/sdcard";
#else
    // Inert stub for boards without a 4-bit SDIO card (keeps main.cpp uniform).
    bool begin() { return false; }
    bool recheck() { return false; }
    bool mounted() const { return false; }
    uint32_t sizeMB() const { return 0; }
    bool selfTest(const char* = nullptr) { return false; }
    const char* lastError() const { return "disabled"; }
#endif
};

} // namespace mal
