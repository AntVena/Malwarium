// update_job.h: the operator-started update check or install, from the press that starts
// it to the terminal status the device tier reports, plus where this device looks for one.
// Runtime only: where to look is device-tier (platform/esp32/update_source.h) and a
// check's answer is stale the moment the device is off, so none of it is saved.
//
// Game owns one and starts, cancels and feeds it (setUpdateStatus, setInstallStatus,
// setNetStatus), because a job also raises the connection it needs. Everything else reads
// it through Game::updateJob().
#pragma once

#include <cstdint>
#include <cstring>

#include "core/device/update_status.h"

namespace mal {

class UpdateJob {
public:
    // True while a check OR an install is in flight. Latched rather than derived from
    // the screen, because a job outlives the screen that started it: this is what holds
    // the radio (Game::netConnectWanted) and what platform/esp32/net_update.h services,
    // and nothing else releases the radio, so it must always reach a terminal outcome.
    bool live() const { return live_; }
    // What the live job is doing: None is a check, anything else an install.
    UpdateTarget target() const { return target_; }
    const UpdateStatus& status() const { return status_; }
    const InstallStatus& install() const { return install_; }

    // Whether this device knows WHERE to check. A device with no source isn't offered a
    // check it cannot perform.
    bool sourceKnown() const { return sourceKnown_; }
    // The manifest address. Display only (the engine never fetches), but readable so the
    // 'Pedia's user section can show what it is about to replace. Empty means no source.
    const char* manifestUrl() const { return manifestUrl_; }

    // Each setter returns whether anything changed.
    bool setSourceKnown(bool on) {
        if (sourceKnown_ == on) return false;
        sourceKnown_ = on;
        return true;
    }
    bool setManifestUrl(const char* url) {
        const char* src = url ? url : "";
        if (std::strcmp(manifestUrl_, src) == 0) return false;
        std::strncpy(manifestUrl_, src, sizeof(manifestUrl_) - 1);
        manifestUrl_[sizeof(manifestUrl_) - 1] = '\0';
        sourceKnown_ = manifestUrl_[0] != '\0';
        return true;
    }

    // Start a check. False if a job is already running (it is not restarted).
    bool startCheck() {
        if (live_) return false;
        live_ = true;
        target_ = UpdateTarget::None;
        status_ = UpdateStatus{};
        status_.state = UpdateState::Checking;
        return true;
    }

    // THE SECOND YES. Refuses any target the last check didn't report as newer, so an
    // install can never run against a stale screen or a version the device never
    // confirmed exists.
    bool startInstall(UpdateTarget t) {
        if (live_) return false;
        if (t == UpdateTarget::Firmware && !status_.firmwareNewer) return false;
        if (t == UpdateTarget::Web && !status_.webNewer) return false;
        if (t == UpdateTarget::None) return false;
        live_ = true;
        target_ = t;
        install_ = InstallStatus{};
        install_.state = InstallState::Downloading;
        install_.target = t;
        return true;
    }

    // End a running job without an answer (consent withdrawn, or the operator
    // cancelled). A completed job's result is left alone: the screen is showing it.
    bool cancel() {
        if (!live_) return false;
        if (target_ == UpdateTarget::None) status_ = UpdateStatus{};
        else install_ = InstallStatus{};
        end();
        return true;
    }

    // The connection the job raised never came up: there is nothing to fetch over it,
    // and the screen has to say which step gave up.
    void failNoNetwork() {
        if (!live_) return;
        if (target_ == UpdateTarget::None) {
            status_.state = UpdateState::Failed;
            status_.fail = UpdateFail::NoNetwork;
        } else {
            install_.state = InstallState::Failed;
            install_.fail = InstallFail::NoNetwork;
        }
        end();
    }

    // What the device tier reports. A terminal state ends the job and hands the radio
    // back; Checking (or an install still under way) keeps it. A finished install's
    // status stands so the screen can report what happened.
    void report(const UpdateStatus& s) {
        status_ = s;
        if (s.state != UpdateState::Checking) end();
    }
    void report(const InstallStatus& s) {
        install_ = s;
        if (s.state == InstallState::Done || s.state == InstallState::Failed) end();
    }

    void clearInstallReport() { install_ = InstallStatus{}; }

private:
    // One job ends the same way whatever it was.
    void end() {
        live_ = false;
        target_ = UpdateTarget::None;
    }

    bool sourceKnown_ = false;
    // Sized to match UpdateSource::kUrlCap and UpdateArtifact::url: the manifest's
    // address and the addresses inside it come off the same host, so a URL one can hold
    // and another can't would be a trap.
    char manifestUrl_[160] = {0};
    bool live_ = false;
    UpdateTarget target_ = UpdateTarget::None;
    UpdateStatus status_;
    InstallStatus install_;
};

}  // namespace mal
