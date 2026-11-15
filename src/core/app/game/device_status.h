// device_status.h: what the device tier last reported about the hardware (battery, card,
// radio owner, the web bundle on the card) and the one-shot requests the CFG pages leave for it (re-mount the card,
// format it, travel sleep). Runtime only: each is a reading of the hardware now or a
// request a reboot answers, so none of it is saved.
//
// Game owns one, feeds it from its set* entry points and repaints when one reports a
// change. Screens and the platform tiers read it through Game::deviceStatus().
#pragma once

#include <cstdint>
#include <cstring>

#include "tunables.h"
#include "core/device/power_status.h"
#include "core/device/radio_status.h"
#include "core/device/sd_status.h"

namespace mal {

class DeviceStatus {
public:
    // Each setter and request returns whether anything a screen draws changed.

    // The percentage is smoothed and the glyph's fill resolved here, so the CFG BATT
    // line and the Hacker face's glyph never disagree about one battery, and a platform
    // that samples faster than a screen repaints pays for it once. A sample landing on
    // the same number changes nothing, so a free-running sampler cannot drive the ~4fps
    // event-driven redraw.
    bool setPower(const PowerStatus& p) {
        PowerStatus next = p;
        const bool readable = p.present && p.percent >= 0;
        if (readable) {
            next.percent = smoothBatteryPercent(
                power_.present && power_.percent >= 0 ? power_.percent : -1, p.percent);
        }
        const int level = readable ? batteryLevelStable(next.percent, batteryLevel_) : -1;
        const bool changed = next.present != power_.present ||
                             next.charging != power_.charging ||
                             next.percent != power_.percent || level != batteryLevel_;
        power_ = next;
        batteryLevel_ = level;
        return changed;
    }
    // The last SMOOTHED reading.
    const PowerStatus& powerStatus() const { return power_; }
    // Which ICON_SYS_BATTERY frame the reading draws as, or -1 when there is no reading
    // (host builds, or a board with the monitor disabled). It is also the state
    // batteryLevelStable holds a glyph steady with across a band edge.
    int batteryLevel() const { return batteryLevel_; }

    // A card just becoming present (boot mount or a runtime re-check) arms the idle
    // screen's SD-present flash; a fresh reading of a card already present does not.
    void setSd(const SdStatus& s, uint32_t nowMs) {
        if (s.present && !sd_.present) {
            sdRevealAtMs_ = nowMs;
            sdRevealArmed_ = true;
        }
        sd_ = s;
    }
    const SdStatus& sdStatus() const { return sd_; }
    // The flash lasts kSdIconRevealMs after the card became present. The armed guard
    // lets a boot mount at game-ms 0 still reveal.
    bool sdIconVisible(uint32_t nowMs) const {
        return sd_.present && sdRevealArmed_ && (nowMs - sdRevealAtMs_) < kSdIconRevealMs;
    }

    // A card inserted after boot is invisible until the device loop unmounts, remounts
    // and pushes a fresh reading. CFG asks; the device tier acts, then clears it.
    void requestSdRecheck() { sdRecheck_ = true; }
    bool sdRecheckRequested() const { return sdRecheck_; }
    void clearSdRecheck() { sdRecheck_ = false; }

    // FORMAT SD. The operator's yes latches Working; the device tier formats, remounts and
    // reports Done or Failed. Latched rather than pulsed: the device tier may decline a
    // pass (an update job is writing to the card) and be asked again. A reboot mid-format
    // leaves the card for the boot mount to report on.
    void requestSdFormat() { sdFormat_ = SdFormatState::Working; }
    bool sdFormatRequested() const { return sdFormat_ == SdFormatState::Working; }
    void finishSdFormat(bool ok) { sdFormat_ = ok ? SdFormatState::Done : SdFormatState::Failed; }
    void clearSdFormatResult() { sdFormat_ = SdFormatState::Idle; }
    SdFormatState sdFormatState() const { return sdFormat_; }

    // Travel sleep. Latched: the device tier may hold it a moment (an OTA image on trial
    // must commit before a reset is safe). A device that woke from one is not still
    // asking, since the sleep is a reset.
    void requestTravelSleep() { travelSleep_ = true; }
    void clearTravelSleep() { travelSleep_ = false; }
    bool travelSleepRequested() const { return travelSleep_; }

    // Which consent holds the radio, pushed by the arbiter (platform/esp32/radio_arbiter.h)
    // whenever it hands ownership over. None on host.
    bool setRadioOwner(RadioOwner o) {
        if (radioOwner_ == o) return false;
        radioOwner_ = o;
        return true;
    }
    RadioOwner radioOwner() const { return radioOwner_; }

    // The web 'Pedia bundle on the card, read off its marker by the platform tier
    // (platform/esp32/web_bundle.h) and refreshed after an install. Display only: every
    // "is it newer" comparison is made device-side. Empty or null reads as UNKNOWN (no
    // card, or no marker).
    bool setWebBundleVersion(const char* v) {
        const char* src = (v && v[0]) ? v : "UNKNOWN";
        if (std::strcmp(webBundle_, src) == 0) return false;
        std::strncpy(webBundle_, src, sizeof(webBundle_) - 1);
        webBundle_[sizeof(webBundle_) - 1] = '\0';
        return true;
    }
    const char* webBundleVersion() const { return webBundle_; }

private:
    PowerStatus power_;
    int batteryLevel_ = -1;
    SdStatus sd_;
    uint32_t sdRevealAtMs_ = 0;
    bool sdRevealArmed_ = false;
    bool sdRecheck_ = false;
    SdFormatState sdFormat_ = SdFormatState::Idle;
    bool travelSleep_ = false;
    RadioOwner radioOwner_ = RadioOwner::None;
    char webBundle_[16] = "UNKNOWN";
};

}  // namespace mal
