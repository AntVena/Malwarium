#include "core/app/game/radio_pages.h"

#include "core/ui/cfg_screen.h"
#include "core/ui/layout.h"

namespace mal {

namespace {
HomeNetwork homeNetwork(const RadioHost& host) {
    return host.netProvisioned() ? HomeNetwork::Saved : HomeNetwork::Unsaved;
}
}  // namespace

void RadioPages::open(const RadioHost& host, CfgScreen page) {
    switch (page) {
        case CfgScreen::Audit: pick_ = static_cast<int>(host.auditMode()); break;
        case CfgScreen::PediaAp: pick_ = host.apEnabled() ? 1 : 0; break;
        case CfgScreen::Link: pick_ = host.linkEnabled() ? 1 : 0; break;
        default: break;
    }
}

// Escalating picker: OFF -> SCAN -> SCAN+CAP -> OFF.
CfgMove RadioPages::onAudit(RadioHost& host, const ButtonEvent& ev) {
    return pickerPress(ev, [&] { pick_ = stepRow(pick_, 3, +1); }, [&] {
        host.setAuditMode(static_cast<AuditMode>(pick_));
        return CfgMove::leave();
    });
}

CfgMove RadioPages::onAp(RadioHost& host, const ButtonEvent& ev) {
    return pickerPress(ev, [&] { pick_ ^= 1; }, [&] {
        host.setApEnabled(pick_ != 0);
        // Turning the AP ON leads straight to the QR to connect (also the "see the QR"
        // path when it is already on). OFF has no QR to show.
        if (pick_ == 0) return CfgMove::leave();
        qrPage_ = 0;             // start on the join-network QR
        return CfgMove::show(CfgScreen::PediaQr);
    });
}

CfgMove RadioPages::onLink(RadioHost& host, const ButtonEvent& ev) {
    return pickerPress(ev, [&] { pick_ ^= 1; }, [&] {
        host.setLinkEnabled(pick_ != 0);
        return CfgMove::leave();
    });
}

// Two QR pages: A cycles join-network and the 'Pedia URL; C exits.
CfgMove RadioPages::onPediaQr(RadioHost& host, const ButtonEvent& ev) {
    if (ev.button == Button::A) {
        qrPage_ = stepRow(qrPage_, pediaQrPages(homeNetwork(host)), +1);
        host.markDirty();
    } else if (ev.button == Button::C) {
        return CfgMove::leave();
    }
    return CfgMove::stay();
}

void RadioPages::drawGroup(Framebuffer& fb, const RadioHost& host, int cursor) const {
    CfgRadioView v;
    v.cursor = cursor;
    v.owner = host.radioOwner();
    v.auditLevel = static_cast<int>(host.auditMode());
    v.linkOn = host.linkEnabled();
    v.apOn = host.apEnabled();
    v.updateLive = host.netConnectWanted();
    drawCfgRadio(fb, v);
}

void RadioPages::drawAudit(Framebuffer& fb, const RadioHost& host) const {
    drawAuditMode(fb, pick_, static_cast<int>(host.auditMode()));
}

void RadioPages::drawAp(Framebuffer& fb, const RadioHost& host) const {
    drawApToggle(fb, pick_, host.apEnabled() ? OnOff::On : OnOff::Off);
}

void RadioPages::drawLink(Framebuffer& fb, const RadioHost& host) const {
    drawLinkToggle(fb, pick_, host.linkEnabled() ? OnOff::On : OnOff::Off,
                   host.linkAmbientStarved() ? LinkReach::PeersOnly : LinkReach::Ambient);
}

void RadioPages::drawPediaQr(Framebuffer& fb, const RadioHost& host) const {
    mal::drawPediaQr(fb, qrPage_, homeNetwork(host));
}

}  // namespace mal
