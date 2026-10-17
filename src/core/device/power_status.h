// power_status.h — portable power/battery status the platform tier feeds in.
//
// Reading the battery ADC and charge-status pin is device-tier work (only the
// ESP32 layer can touch GPIO), but the *shape* of the reading and the SoC math
// live here so they're shared and unit-testable on the native host. The device
// layer fills a PowerStatus and hands it to Game::setPowerStatus(); the CFG
// "BATT" line and the Hacker face's battery glyph render it. Host/no-battery
// builds leave present=false → "-" (and no glyph).
//
// Two of the three helpers below exist because a raw ADC reading is noisier than
// the thing it measures: the pack sags while the radio transmits, so consecutive
// samples of an unchanged battery differ by several points. smoothBatteryPercent
// settles that into one number, and batteryLevelStable keeps the glyph from
// flickering between two fills when that number sits on a band edge.
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

// The reading at or below which a battery is worth doing something about. Shared so
// the CFG "BATT" line and the Hacker face's glyph raise the alarm at the same number
// rather than each picking one — the emphasis is decoration either way, repeating what
// the empty shell and the printed percentage already say.
inline constexpr int kBatteryLowPct = 15;

// --- The glyph's fill levels ------------------------------------------------
//
// ICON_SYS_BATTERY is a sheet of this many frames, frame index == fill level:
// 0 is an empty shell and kBatteryLevels-1 is a full one. The level IS the
// non-colour channel the dual-coding gate asks for — a grayscale shot still
// counts the bars — so the tint a screen picks may only repeat what the fill and
// the printed percentage already say.
inline constexpr int kBatteryLevels = 5;

// The lowest percentage that still reads as each level. The bands are deliberately
// uneven: the bottom one is narrow because "nearly flat" is the reading a player
// acts on, and the top one is wide because the difference between 90% and 100% is
// not a thing anybody does anything about.
inline constexpr int kBatteryLevelFloorPct[kBatteryLevels] = {0, 10, 35, 60, 85};

// How far past a band edge the percentage must travel before the glyph follows it.
// Without this a reading resting on an edge alternates fills every sample, which
// reads as a fault rather than as a battery.
inline constexpr int kBatteryLevelHysteresisPct = 3;

// Which fill a percentage reads as, ignoring whatever is on screen now.
inline int batteryLevel(int percent) {
    int lvl = 0;
    for (int i = kBatteryLevels - 1; i > 0; --i) {
        if (percent >= kBatteryLevelFloorPct[i]) { lvl = i; break; }
    }
    return lvl;
}

// Which fill to DRAW, given what is drawn now. `shown` is the level currently on
// screen, or -1 when nothing is (first reading, or the glyph was absent) — then the
// fresh level lands as-is, since there is nothing to flicker against. Otherwise a
// move only happens once the percentage clears the edge it would cross by the
// hysteresis margin, so a reading hovering on a band edge holds the fill it has.
inline int batteryLevelStable(int percent, int shown) {
    const int fresh = batteryLevel(percent);
    if (shown < 0 || shown >= kBatteryLevels) return fresh;
    if (fresh == shown) return shown;
    if (fresh > shown)  // rising: clear the floor of the level above by the margin
        return percent >= kBatteryLevelFloorPct[shown + 1] + kBatteryLevelHysteresisPct
                   ? fresh : shown;
    // falling: drop below this level's own floor by the margin
    return percent < kBatteryLevelFloorPct[shown] - kBatteryLevelHysteresisPct
               ? fresh : shown;
}

// Fold a fresh sample into the running reading — a quarter-weight EMA, so a lone
// noisy sample moves the displayed percentage by a few points rather than all the
// way, and a real change still settles within a handful of samples. `prev` is the
// last smoothed value, or -1 for no history (the first sample after a boot or a
// wake lands as-is, because there is nothing yet for it to be noise against).
//
// The rounding guard is load-bearing, not a nicety: a plain integer EMA cannot
// close the last point of a gap — (50*3 + 51)/4 truncates back to 50 forever — so
// a reading one point away from the truth would never arrive at it.
inline int smoothBatteryPercent(int prev, int sample) {
    if (prev < 0 || prev > 100) return sample;
    const int blended = (prev * 3 + sample + 2) / 4;
    if (blended == prev && sample != prev) return prev + (sample > prev ? 1 : -1);
    return blended;
}

} // namespace mal
