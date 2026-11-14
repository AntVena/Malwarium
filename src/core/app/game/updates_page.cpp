#include "core/app/game/updates_page.h"

#include "version.h"
#include "core/ui/cfg_screen.h"
#include "core/ui/layout.h"

namespace mal {

// Leaving does not cancel a job. The job holds the radio, so a download finishes and
// its outcome waits here.
UpdatesPage::Move UpdatesPage::onButton(UpdatesHost& host, const ButtonEvent& ev) {
    if (host.updateJobLive() || host.installStatus().state != InstallState::Idle) {
        if (ev.button != Button::C) return Move::Stay;
        // Only a finished job's report is cleared on the way out; a live one keeps its
        // progress to come back to.
        if (!host.updateJobLive()) host.clearInstallReport();
        return Move::Leave;
    }
    if (confirm_ != UpdateTarget::None) return onConfirm(host, ev);
    return onRows(host, ev);
}

UpdatesPage::Move UpdatesPage::onConfirm(UpdatesHost& host, const ButtonEvent& ev) {
    if (ev.button == Button::A) confirmPick_ ^= 1;      // NO <-> YES
    else if (ev.button == Button::B) {
        // The second yes, and the only path to a write.
        if (confirmPick_ == 1) host.requestUpdateInstall(confirm_);
        confirm_ = UpdateTarget::None;
        confirmPick_ = 0;
    } else if (ev.button == Button::C) {
        confirm_ = UpdateTarget::None;
        confirmPick_ = 0;
    }
    host.markDirty();
    return Move::Stay;
}

UpdatesPage::Move UpdatesPage::onRows(UpdatesHost& host, const ButtonEvent& ev) {
    const UpdateStatus& status = host.updateStatus();
    if (ev.button == Button::A) {
        row_ = stepRow(row_, updateCheckRows(status), +1);
        host.markDirty();
    } else if (ev.button == Button::B) {
        switch (updateCheckRowKind(status, row_)) {
            case UpdateRowKind::Check:
                host.requestUpdateCheck();      // no-ops unless ready
                row_ = 0;                       // a fresh check invalidates the rows
                break;
            case UpdateRowKind::Install:
                confirm_ = updateCheckRowTarget(status, row_);
                confirmPick_ = 0;               // ask before anything is written, on NO
                host.markDirty();
                break;
            case UpdateRowKind::FlashQr:
                // Needs no radio and no network: the code is drawn from an address this
                // device already holds, so it opens even when a check cannot run.
                if (host.updateSourceKnown()) {
                    host.markDirty();
                    return Move::OpenFlashQr;
                }
                break;
        }
    } else if (ev.button == Button::C) {
        return Move::Leave;
    }
    return Move::Stay;
}

// C returns to UPDATES with its cursor still on the row that opened this. The defocus
// and the sleep are held off while it shows (qrScreenActive): a phone takes longer
// than the idle window to read it.
UpdatesPage::Move UpdatesPage::onFlashQr(const ButtonEvent& ev) const {
    return ev.button == Button::C ? Move::Leave : Move::Stay;
}

void UpdatesPage::draw(Framebuffer& fb, const UpdatesHost& host) const {
    const UpdateStatus& status = host.updateStatus();
    const InstallStatus& install = host.installStatus();
    UpdateCheckView checkView;
    checkView.ready = host.updateCheckReady();
    checkView.provisioned = host.netProvisioned();
    checkView.sourceKnown = host.updateSourceKnown();
    checkView.firmwareVersion = kFirmwareVersion;
    checkView.manifestUrl = host.updateManifestUrl();
    checkView.row = row_;
    // The input handler's three faces, in its order: a live or just-finished job outranks
    // the confirm, which outranks the list.
    if (host.updateJobLive() || install.state != InstallState::Idle) {
        if (host.updateJobTarget() == UpdateTarget::None && install.state == InstallState::Idle)
            drawUpdateCheck(fb, status, host.netStatus(), checkView);
        else
            drawUpdateProgress(fb, install, host.netStatus());
    } else if (confirm_ != UpdateTarget::None) {
        const bool fw = confirm_ == UpdateTarget::Firmware;
        drawUpdateConfirm(fb, confirm_, fw ? kFirmwareVersion : host.webBundleVersion(),
                          fw ? status.firmwareVersion : status.webVersion, confirmPick_);
    } else {
        drawUpdateCheck(fb, status, host.netStatus(), checkView);
    }
}

void UpdatesPage::drawFlashQr(Framebuffer& fb, const UpdatesHost& host) const {
    drawUpdateQr(fb, host.updateManifestUrl());
}

}  // namespace mal
