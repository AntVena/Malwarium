// radio_pages.h: CFG's RADIO pages: AUDIT, PEDIA AP, PASSIVE LINK, the PEDIA QR behind
// the AP, and what the RADIO group page shows above its rows.
//
// The pages hold the focused choice (seeded from what is live as each opens) and which
// QR step is up. The toggles, the arbiter that decides which radio is on the air and the
// stored home network are the game's, reached through RadioHost.
//
// This header names no core/ui type beyond ui_state.h, so game.h can hold the pages
// (inside CfgMenu) by value.
#pragma once

#include "core/app/game/cfg_page.h"
#include "core/device/radio_status.h"
#include "core/net/audit_capture.h"     // AuditMode

namespace mal {

class Framebuffer;

// What the RADIO pages need from the game: the three toggles, what the arbiter put on
// the air, the update job's claim on it, and whether a home network is stored.
class RadioHost {
public:
    virtual AuditMode auditMode() const = 0;
    virtual void setAuditMode(AuditMode m) = 0;
    virtual bool apEnabled() const = 0;
    virtual void setApEnabled(bool on) = 0;
    virtual bool linkEnabled() const = 0;
    virtual void setLinkEnabled(bool on) = 0;
    virtual bool linkAmbientStarved() const = 0;
    virtual RadioOwner radioOwner() const = 0;
    virtual bool netConnectWanted() const = 0;   // the Update claim the arbiter resolves
    virtual bool netProvisioned() const = 0;
    virtual void markDirty() = 0;

protected:
    ~RadioHost() = default;
};

class RadioPages {
public:
    // Seeds the focus from what is live, so a picker opens on the current setting.
    void open(const RadioHost& host, CfgScreen page);

    CfgMove onAudit(RadioHost& host, const ButtonEvent& ev);
    CfgMove onAp(RadioHost& host, const ButtonEvent& ev);
    CfgMove onLink(RadioHost& host, const ButtonEvent& ev);
    CfgMove onPediaQr(RadioHost& host, const ButtonEvent& ev);

    // The RADIO group page, its rows at `cursor` (the menu's group cursor).
    void drawGroup(Framebuffer& fb, const RadioHost& host, int cursor) const;
    void drawAudit(Framebuffer& fb, const RadioHost& host) const;
    void drawAp(Framebuffer& fb, const RadioHost& host) const;
    void drawLink(Framebuffer& fb, const RadioHost& host) const;
    void drawPediaQr(Framebuffer& fb, const RadioHost& host) const;

    // Which QR step the 'Pedia page shows (pediaQrPages orders them).
    int pediaQrPage() const { return qrPage_; }

private:
    // The open page's focused choice. The pages are never open together, so they share
    // it, and each seeds it on opening.
    int pick_ = 0;
    int qrPage_ = 0;
};

}  // namespace mal
