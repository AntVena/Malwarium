#include "core/app/game/hardware_pages.h"

#include <cstdio>

#include "tunables.h"
#include "version.h"
#include "core/app/operator.h"
#include "core/app/settings.h"
#include "core/model/hacker_rank.h"
#include "core/ui/cfg_screen.h"

namespace mal {

void HardwarePages::open(HardwareHost& host, CfgScreen page) {
    switch (page) {
        case CfgScreen::Travel: pick_ = 0; break;              // always opens on NO
        case CfgScreen::FormatSd:                              // ...likewise, and a result
            pick_ = 0;                                         // left on screen is spent
            if (host.sdFormatState() != SdFormatState::Working) host.clearSdFormatResult();
            break;
        default: break;
    }
}

// A re-checks the card (a card inserted after boot is picked up, and the result lands
// on this page's SD line). B arms the hidden hold (CfgMenu::holdB).
CfgMove HardwarePages::onSysInfo(HardwareHost& host, const ButtonEvent& ev) {
    if (ev.button == Button::A) host.requestSdRecheck();
    else if (ev.button == Button::B) host.armHoldB();
    else if (ev.button == Button::C) return CfgMove::leave();
    return CfgMove::stay();
}

// Once the request is latched every button is inert: the device tier is already landing
// a save and powering the panel down, and a press that half-cancelled that would leave
// the operator guessing which state they are in.
CfgMove HardwarePages::onTravel(HardwareHost& host, const ButtonEvent& ev) {
    if (host.travelSleepRequested()) return CfgMove::stay();
    CfgMove move = CfgMove::stay();
    if (ev.button == Button::A) pick_ ^= 1;
    else if (ev.button == Button::B) {
        if (pick_ == 1) host.requestTravelSleep();
        else move = CfgMove::leave();
    } else if (ev.button == Button::C) move = CfgMove::leave();
    host.markDirty();
    return move;
}

// Working: every button is inert, as travel's are once latched, since nothing pressed
// here could stop a format. A result is read, then any button leaves. YES on a busy
// card is refused by the request itself.
CfgMove HardwarePages::onFormatSd(HardwareHost& host, const ButtonEvent& ev) {
    const SdFormatState st = host.sdFormatState();
    if (st == SdFormatState::Working) return CfgMove::stay();
    CfgMove move = CfgMove::stay();
    if (st != SdFormatState::Idle) {
        host.clearSdFormatResult();
        move = CfgMove::leave();
    } else if (ev.button == Button::A) {
        pick_ ^= 1;
    } else if (ev.button == Button::B) {
        if (pick_ == 0) move = CfgMove::leave();
        else host.requestSdFormat();
    } else if (ev.button == Button::C) {
        move = CfgMove::leave();
    }
    host.markDirty();
    return move;
}

void HardwarePages::drawSysInfo(Framebuffer& fb, const HardwareHost& host) const {
    const Operator& op = host.operatorRecord();
    // RANK reads "R{n} {title}" so the endless numeric rank shows beside its title, the
    // same "R%d %s" form as STAT.
    char rankLine[28];
    std::snprintf(rankLine, sizeof(rankLine), "R%d %s", op.hackerRank,
                  hackerRankTitle(op.hackerRank));
    SysInfoView v;
    v.owner = host.radioOwner();
    v.sd = host.sdStatus();
    v.power = host.powerStatus();
    v.uptimeMs = host.uptimeMs();
    v.fwVersion = kFirmwareVersion;
    v.uiMode = host.settingsRecord().uiMode;
    v.rankTitle = rankLine;
    v.networksSeen = op.networksSeen;
    v.equippedTitle = host.equippedTitleName();
    v.captureEnabled = host.captureEnabled();
    v.handshakesSeen = op.cant.handshakesSeen;
    if (const int32_t held = host.heldBMs(); held >= 0)
        v.holdFrac = static_cast<float>(held) / kFactoryRevealMs;
    mal::drawSysInfo(fb, v);
}

// Two faces, like UPDATES: the question, then the notice that replaces it for as long as
// the device is still on its way down.
void HardwarePages::drawTravel(Framebuffer& fb, const HardwareHost& host) const {
    if (host.travelSleepRequested()) drawTravelSleeping(fb);
    else drawTravelConfirm(fb, pick_);
}

void HardwarePages::drawFormatSd(Framebuffer& fb, const HardwareHost& host) const {
    if (host.sdFormatState() == SdFormatState::Idle)
        drawSdFormatConfirm(fb, pick_, host.sdStatus(), host.sdCardUse());
    else
        drawSdFormatStatus(fb, host.sdFormatState(), host.sdStatus());
}

}  // namespace mal
