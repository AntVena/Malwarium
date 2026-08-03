// =============================================================================
//  pin_finder.ino  —  discover which GPIO a button is wired to
// -----------------------------------------------------------------------------
//  Use this to find the PLUS and PWR button GPIOs on the Waveshare board
//  (BOOT is already known to be GPIO0), or any free/attached buttons on a board
//  you're porting to.
//
//  HOW TO USE
//    1. Flash this sketch to the board (Arduino IDE: select your ESP32-S3
//       board; or `pio run -e waveshare_s3_154 -t upload` if wired into PIO).
//    2. Open Serial Monitor at 115200 baud.
//    3. Press ONE button at a time. The sketch prints the GPIO number(s)
//       that went LOW, e.g.  "Pressed: GPIO 0".
//    4. Note which physical button gave which number, and paste those into
//       config.h (PIN_BTN_PLUS, PIN_BTN_PWR).
//
//  NOTES
//    - It scans only "safe" GPIOs and skips pins already used by the display,
//      SD card, audio and I2C on the Waveshare board, so it won't fight them.
//    - Buttons here are assumed active-LOW with internal pull-ups (matches the
//      board). If a button reads as "always pressed", it may be on an
//      input-only pin without a pull-up (e.g. the classic ESP32's GPIO34-39) — add a 10k
//      pull-up to 3V3.
//    - PWR also powers the board: a LONG press will turn the board off. Use
//      SHORT taps when probing PWR.
// =============================================================================

#include <Arduino.h>

// Candidate pins to watch. Trimmed to avoid pins used by LCD/SD/audio/I2C on
// the Waveshare ESP32-S3-LCD-1.54. Edit freely for other boards.
const int candidatePins[] = {
  0,                      // BOOT (expected)
  11, 19, 20, 21,         // commonly-free S3 GPIOs / pads
  38, 39, 40, 43, 44,     // exposed UART / spare pads
  45, 46, 47, 48
};
const int numPins = sizeof(candidatePins) / sizeof(candidatePins[0]);

bool lastState[64];

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("=== Pin Finder ===");
  Serial.println("Press one button at a time. Watching these GPIOs:");
  for (int i = 0; i < numPins; i++) {
    int p = candidatePins[i];
    pinMode(p, INPUT_PULLUP);
    lastState[p] = digitalRead(p);
    Serial.printf("  GPIO %d\n", p);
  }
  Serial.println("Ready. (Use SHORT taps on the PWR button.)");
  Serial.println("-----------------------------------------");
}

void loop() {
  for (int i = 0; i < numPins; i++) {
    int p = candidatePins[i];
    bool now = digitalRead(p);          // LOW = pressed (active-low)
    if (now == LOW && lastState[p] == HIGH) {
      Serial.printf(">> Pressed: GPIO %d\n", p);
    }
    if (now == HIGH && lastState[p] == LOW) {
      Serial.printf("   released: GPIO %d\n", p);
    }
    lastState[p] = now;
  }
  delay(15);   // simple debounce
}
