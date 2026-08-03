// lgfx_config.h — LovyanGFX device definition for the Waveshare ESP32-S3-LCD-1.54.
//
// ST7789, 240x240, 4-wire SPI. All pins come from include/config.h (never
// hardcoded here) so a new board is a config edit, not a driver edit. This is
// the only file that knows about LovyanGFX bus details; the rest of the engine
// sees IDisplay (platform.h).
#pragma once

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

#include "config.h"

namespace mal {

// Concrete LovyanGFX device: ST7789 over the board's hardware SPI bus.
// Backlight is NOT managed here — the demo drives PIN_LCD_BL as a plain GPIO
// (digitalWrite HIGH), so Esp32Display owns it directly (no Light_PWM).
class LgfxWaveshareS3 : public lgfx::LGFX_Device {
    lgfx::Panel_ST7789 panel_;
    lgfx::Bus_SPI bus_;

public:
    LgfxWaveshareS3() {
        {  // SPI bus (uses the SPI2/FSPI host on the S3; GPIO matrix routes pins).
            auto cfg = bus_.config();
            cfg.spi_host = SPI2_HOST;
            cfg.spi_mode = 0;
            cfg.freq_write = 40000000;  // 40 MHz write clock
            cfg.freq_read = 16000000;
            cfg.spi_3wire = false;
            cfg.use_lock = true;
            cfg.dma_channel = SPI_DMA_CH_AUTO;
            cfg.pin_sclk = PIN_LCD_SCK;
            cfg.pin_mosi = PIN_LCD_MOSI;
            cfg.pin_miso = PIN_LCD_MISO;   // -1 (write-only panel)
            cfg.pin_dc = PIN_LCD_DC;
            bus_.config(cfg);
            panel_.setBus(&bus_);
        }
        {  // Panel.
            auto cfg = panel_.config();
            cfg.pin_cs = PIN_LCD_CS;
            cfg.pin_rst = PIN_LCD_RST;
            cfg.pin_busy = -1;
            cfg.panel_width = DISPLAY_WIDTH;
            cfg.panel_height = DISPLAY_HEIGHT;
            cfg.offset_x = 0;
            cfg.offset_y = 0;
            cfg.offset_rotation = DISPLAY_ROTATION;
            cfg.readable = false;
            cfg.invert = true;    // IPS ST7789 240x240 module (demo passes IPS=true)
            cfg.rgb_order = false;
            cfg.dlen_16bit = false;
            cfg.bus_shared = false;
            panel_.config(cfg);
        }
        setPanel(&panel_);
    }
};

} // namespace mal
