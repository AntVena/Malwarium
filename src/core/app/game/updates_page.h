// updates_page.h: CFG's UPDATES page and the flash QR behind it.
//
// Three faces on one page: a job's progress, the yes/no confirm an install row opens,
// and the row list. The page holds the focused row and the target sitting at its
// confirm. The job, what it reports and the radio it holds are the game's, reached
// through UpdatesHost. A press answers with the move it asks of the menu (CfgMove).
//
// This header names no core/ui type beyond ui_state.h, so game.h can hold the page
// (inside CfgMenu) by value.
#pragma once

#include <cstdint>

#include "core/app/game/cfg_page.h"
#include "core/app/game/update_job.h"
#include "core/device/net_status.h"
#include "core/device/update_status.h"

namespace mal {

class DeviceStatus;
class Framebuffer;

// What UPDATES needs from the game: the update job, what it reports, and whether a
// check could run at all.
class UpdatesHost {
public:
    virtual const UpdateJob& updateJob() const = 0;
    virtual bool updateCheckReady() const = 0;
    virtual const NetStatus& netStatus() const = 0;
    virtual bool netProvisioned() const = 0;
    virtual const DeviceStatus& deviceStatus() const = 0;   // the web bundle on the card
    virtual void requestUpdateCheck() = 0;
    virtual void requestUpdateInstall(UpdateTarget t) = 0;
    virtual void clearInstallReport() = 0;
    virtual void markDirty() = 0;

protected:
    ~UpdatesHost() = default;
};

class UpdatesPage {
public:
    CfgMove onButton(UpdatesHost& host, const ButtonEvent& ev);
    void draw(Framebuffer& fb, const UpdatesHost& host) const;
    // The flash QR: one page, so A does nothing and C goes back to the rows.
    CfgMove onFlashQr(const ButtonEvent& ev) const;
    void drawFlashQr(Framebuffer& fb, const UpdatesHost& host) const;

private:
    CfgMove onConfirm(UpdatesHost& host, const ButtonEvent& ev);
    CfgMove onRows(UpdatesHost& host, const ButtonEvent& ev);

    // The focused row, and the target sitting at its yes/no confirm. The confirm
    // clears on the way out; the job does not.
    int row_ = 0;
    UpdateTarget confirm_ = UpdateTarget::None;
    int confirmPick_ = 0;   // 0 = NO, and NO is always where it starts
};

}  // namespace mal
