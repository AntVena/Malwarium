// updates_page.h: CFG's UPDATES page and the flash QR behind it.
//
// Three faces on one page: a job's progress, the yes/no confirm an install row opens,
// and the row list. The page holds the focused row and the target sitting at its
// confirm. The job, what it reports and the radio it holds are the game's, reached
// through UpdatesHost. Where the page sits in the CFG menu is the menu's business, so a
// press answers with the move it asks of the menu rather than making it.
//
// This header names no core/ui type, so game.h can hold the page (inside CfgMenu) by
// value.
#pragma once

#include <cstdint>

#include "core/device/net_status.h"
#include "core/device/platform.h"
#include "core/device/update_status.h"

namespace mal {

class Framebuffer;

// What UPDATES needs from the game: the update job, what it reports, and whether a
// check could run at all.
class UpdatesHost {
public:
    virtual bool updateSourceKnown() const = 0;
    virtual bool updateCheckReady() const = 0;
    virtual bool updateJobLive() const = 0;
    virtual UpdateTarget updateJobTarget() const = 0;
    virtual const UpdateStatus& updateStatus() const = 0;
    virtual const InstallStatus& installStatus() const = 0;
    virtual const NetStatus& netStatus() const = 0;
    virtual bool netProvisioned() const = 0;
    virtual const char* updateManifestUrl() const = 0;
    virtual const char* webBundleVersion() const = 0;
    virtual void requestUpdateCheck() = 0;
    virtual void requestUpdateInstall(UpdateTarget t) = 0;
    virtual void clearInstallReport() = 0;
    virtual void markDirty() = 0;

protected:
    ~UpdatesHost() = default;
};

class UpdatesPage {
public:
    // What a press asks of the menu the page sits in.
    enum class Move : uint8_t { Stay, Leave, OpenFlashQr };

    Move onButton(UpdatesHost& host, const ButtonEvent& ev);
    void draw(Framebuffer& fb, const UpdatesHost& host) const;
    // The flash QR: one page, so A does nothing and C goes back to the rows.
    Move onFlashQr(const ButtonEvent& ev) const;
    void drawFlashQr(Framebuffer& fb, const UpdatesHost& host) const;

private:
    Move onConfirm(UpdatesHost& host, const ButtonEvent& ev);
    Move onRows(UpdatesHost& host, const ButtonEvent& ev);

    // The focused row, and the target sitting at its yes/no confirm. The confirm
    // clears on the way out; the job does not.
    int row_ = 0;
    UpdateTarget confirm_ = UpdateTarget::None;
    int confirmPick_ = 0;   // 0 = NO, and NO is always where it starts
};

}  // namespace mal
