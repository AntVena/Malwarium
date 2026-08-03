// bringup_min.cpp — backlight retest (env: waveshare_s3_154_min).
//
// Earlier sweep was invalid for GPIO6: lcd.init() attaches LEDC to PIN_LCD_BL,
// so digitalWrite(6) was overridden. This: (A) tests GPIO6 via LovyanGFX's own
// setBrightness (the real backlight path), then (B) frees pin6/7 from LEDC and
// does a clean digitalWrite sweep across candidates. Panel is filled white so a
// lit backlight is unmistakable.
#include <Arduino.h>

#include "platform/esp32/lgfx_config.h"

mal::LgfxWaveshareS3 lcd;

const int kPins[] = {6, 7, 11, 21, 38, 39, 40, 45, 46, 47, 48};
const int kNum = sizeof(kPins) / sizeof(kPins[0]);

void setup() {
    // Hold the battery power latch (BAT_EN) so this diagnostic survives on
    // battery too — otherwise the rail drops during the 15s serial wait below.
#ifdef PIN_POWER_HOLD
    pinMode(PIN_POWER_HOLD, OUTPUT);
    digitalWrite(PIN_POWER_HOLD, POWER_HOLD_ACTIVE_HIGH ? HIGH : LOW);
#endif

    Serial.begin(115200);
    uint32_t t = millis();
    while (!Serial && millis() - t < 15000) delay(50);
    delay(200);

    Serial.println();
    Serial.println("=== BL RETEST ===");
    Serial.flush();
    lcd.init();
    lcd.fillScreen(lcd.color565(255, 255, 255));

    // Phase A: the real LovyanGFX backlight path (Light_PWM on PIN_LCD_BL=6).
    Serial.println("[A] lcd.setBrightness(255) on PIN_LCD_BL=6 for 6s -- WATCH");
    Serial.flush();
    lcd.setBrightness(255);
    delay(6000);
    lcd.setBrightness(0);
    Serial.println("[A] done");

    // Free pins from LEDC so the manual digitalWrite sweep is valid.
    ledcDetachPin(6);
    ledcDetachPin(7);
    Serial.println("[B] starting clean digitalWrite sweep");
    Serial.flush();
}

void loop() {
    for (int i = 0; i < kNum; ++i) {
        int p = kPins[i];
        pinMode(p, OUTPUT);
        Serial.printf(">> GPIO %d = HIGH (3s)\n", p);
        Serial.flush();
        digitalWrite(p, HIGH);
        delay(3000);
        Serial.printf(">> GPIO %d = LOW  (3s)\n", p);
        Serial.flush();
        digitalWrite(p, LOW);
        delay(3000);
        pinMode(p, INPUT);
    }
    Serial.println("[B] sweep complete; repeating");
    Serial.flush();
}
