// power_status.h — portable power/battery status the platform tier feeds in.
//
// Reading the battery ADC and charge-status pin is device-tier work (only the
// ESP32 layer can touch GPIO), but the *shape* of the reading and the SoC math
// live here so they're shared and unit-testable on the native host. The device
// layer fills a PowerStatus and hands it to Game::setPowerStatus(); the CFG
// "BATT" line renders it. Host/no-battery builds leave present=false → "-".
#pragma once

namespace mal {

struct PowerStatus {
    bool present = false;   // is a real battery-monitor reading available?
    bool charging = false;  // external power in AND actively charging (CHG_STAT low)
    int  percent = -1;      // 0..100 estimate, or -1 when unknown/absent
};

// Rough 1S Li-ion state-of-charge from pack millivolts: 3.30 V ≈ empty,
// 4.20 V ≈ full, linear + clamped. Deliberately simple for v1 — a proper
// non-linear discharge curve is a later refinement; this is monotonic and
// good enough to read "how full" at a glance. Pure + portable so it's tested
// natively (test_battery_percent_from_mv), then reused by the device reader.
inline int batteryPercentFromMilliVolts(int packMilliVolts) {
    constexpr int kEmptyMv = 3300;
    constexpr int kFullMv  = 4200;
    int pct = (packMilliVolts - kEmptyMv) * 100 / (kFullMv - kEmptyMv);
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    return pct;
}

} // namespace mal
