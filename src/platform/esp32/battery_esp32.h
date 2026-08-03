// battery_esp32.h — device-tier battery/charge reader for the CFG "BATT" line.
//
// Reads the pack voltage off BAT_ADC (PIN_BATTERY_ADC = GPIO1, behind the
// board's 3:1 divider) and the ETA6098 charge-status pin (PIN_CHARGE_STAT =
// GPIO3, open-drain: LOW = actively charging). Produces a portable PowerStatus
// the engine renders. All the SoC math lives in core/app/power_status.h so it's
// unit-tested on the host; this file is just the GPIO glue.
//
// Compiles to an absent reading (present=false → "-") when the battery monitor
// is disabled or the ADC pin is unset, so it's safe on any board.
#pragma once

#include <Arduino.h>

#include "config.h"
#include "core/app/power_status.h"

namespace mal {

inline void beginBattery() {
#if BATTERY_MONITOR_ENABLED && defined(PIN_CHARGE_STAT) && (PIN_CHARGE_STAT >= 0)
    // STAT is open-drain; pull it up so "not charging" reads a clean HIGH.
    pinMode(PIN_CHARGE_STAT, INPUT_PULLUP);
#endif
}

inline PowerStatus readPowerStatus() {
    PowerStatus s;
#if BATTERY_MONITOR_ENABLED && (PIN_BATTERY_ADC >= 0)
    s.present = true;
    // analogReadMilliVolts is calibrated per-chip; scale by the divider to get
    // the true pack voltage, then map to a rough SoC (shared portable helper).
    const int packMv =
        static_cast<int>(analogReadMilliVolts(PIN_BATTERY_ADC) * BATTERY_ADC_DIVIDER);
    s.percent = batteryPercentFromMilliVolts(packMv);
#  if defined(PIN_CHARGE_STAT) && (PIN_CHARGE_STAT >= 0)
    s.charging = (digitalRead(PIN_CHARGE_STAT) == LOW);  // ETA6098: LOW = charging
#  endif
#endif
    return s;
}

} // namespace mal
