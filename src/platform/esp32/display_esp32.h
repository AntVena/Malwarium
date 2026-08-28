// display_esp32.h — IDisplay backed by LovyanGFX on the device panel.
//
// Mirrors the host's composePanel(): the engine hands us the already-upscaled
// 224x224 active canvas; we centre it in the 240 panel behind the fixed 8px
// bezel. The bezel is painted once (it never changes), so present() is just a
// DMA push of the active region — keeping the ~4fps repaint cheap.
#pragma once

#include "core/render/canvas.h"
#include "core/render/color.h"
#include "core/render/palette.h"
#include "platform/esp32/lgfx_config.h"
#include "platform/platform.h"

namespace mal {

class Esp32Display : public IDisplay {
    LgfxWaveshareS3 lcd_;
    uint8_t duty_ = 255;   // last-applied backlight duty (0..255); restored on wake

public:
    void begin() {
        lcd_.init();
        lcd_.setColorDepth(16);
        // Our framebuffer is little-endian RGB565; pushImage defaults to the
        // opposite byte order, so swap to render true colours.
        lcd_.setSwapBytes(true);
#if HAS_BACKLIGHT_PWM
        // LEDC-driven dimmable backlight — the shipped path on this board (config.h).
        ledcSetup(BACKLIGHT_LEDC_CHANNEL, BACKLIGHT_LEDC_FREQ_HZ, BACKLIGHT_LEDC_BITS);
        ledcAttachPin(PIN_LCD_BL, BACKLIGHT_LEDC_CHANNEL);
        ledcWrite(BACKLIGHT_LEDC_CHANNEL, duty_);
#else
        // Backlight is a plain GPIO on this board (PIN_LCD_BL=46): HIGH = on.
        pinMode(PIN_LCD_BL, OUTPUT);
        digitalWrite(PIN_LCD_BL, HIGH);
#endif
        // Paint the static bezel once; the active region is overwritten each frame.
        lcd_.fillScreen(palColor(Pal::PAPER));
    }

    // Apply a backlight duty (0..255). With PWM this dims; without, it degrades to
    // on/off (any non-zero duty = full on) so the CFG level still does *something*
    // and the panel is never left dark by a mid-range level. Remembers the duty so
    // wake() can restore it after a sleep blanked the backlight.
    void setBacklight(uint8_t duty) {
        duty_ = duty;
#if HAS_BACKLIGHT_PWM
        ledcWrite(BACKLIGHT_LEDC_CHANNEL, duty_);
#else
        digitalWrite(PIN_LCD_BL, duty_ > 0 ? HIGH : LOW);
#endif
    }

    // active points at a kActiveW x kActiveH RGB565 buffer (w/h == 224).
    void present(const Rgb565* active, int w, int h) override {
        lcd_.startWrite();
        lcd_.pushImage(kBezel, kBezel, w, h, active);
        lcd_.endWrite();
    }

    // Idle power saving (device tier only — the engine is unaware). Backlight
    // off + panel sleep is the big battery win on an LCD.
    void sleep() {
#if HAS_BACKLIGHT_PWM
        ledcWrite(BACKLIGHT_LEDC_CHANNEL, 0);
#else
        digitalWrite(PIN_LCD_BL, LOW);
#endif
        lcd_.sleep();
    }
    void wake() {
        lcd_.wakeup();
        setBacklight(duty_);   // restore the user's brightness, not a blind full-on
    }
};

} // namespace mal
