#include "core/app/game/cfg_menu.h"

#include <cstdio>
#include <cstring>

#include "version.h"
#include "core/app/settings.h"
#include "core/app/game/tip_card_screen.h"   // TipCardScreen::title
#include "core/content/content_backgrounds.h"
#include "core/content/content_themes.h"
#include "core/model/hacker_rank.h"
#include "core/render/canvas.h"
#include "core/render/framebuffer.h"
#include "core/render/font.h"
#include "core/render/palette.h"
#include "core/ui/cfg_screen.h"
#include "core/ui/layout.h"
#include "core/ui/widgets.h"

namespace mal {

// --- L2: the CFG list ---------------------------------------------------------

void CfgMenu::stepList(const CfgHost&, int dir) {
    const CfgRow* rows = nullptr;
    row_ = stepRow(row_, cfgRows(rows), dir);
}

void CfgMenu::onButton(CfgHost& host, const ButtonEvent& ev) {
    const CfgRow* rows = nullptr;
    const int n = cfgRows(rows);
    if (ev.button == Button::A) {
        row_ = stepRow(row_, n, +1);
    } else if (ev.button == Button::B) {
        const CfgScreen target = rows[row_].target;
        if (target == CfgScreen::ResetHatch) {
            host.resetToHatch();             // DEV quick reset: one press (dev_config.h)
            return;
        }
        openPage(host, target);
    } else if (ev.button == Button::C) {
        host.leaveToCarousel();
    }
}

void CfgMenu::draw(Framebuffer& fb, const CfgHost& host) const {
    drawCfgList(fb, row_, host.operatorRecord().hackerTag, host.equippedTitleName(),
                host.radioOwner());
}

// --- Moving between pages -----------------------------------------------------

// Open a page, seeding its focus at the applied value. The one seam the list and a
// group page both enter through, so a row behaves the same wherever it is reached from.
void CfgMenu::openPage(CfgHost& host, CfgScreen target) {
    page_ = target;
    switch (target) {
        case CfgScreen::Device:
        case CfgScreen::Radio: groupRow_ = 0; break;
        case CfgScreen::Travel:
        case CfgScreen::FormatSd: hardware_.open(host, target); break;
        case CfgScreen::UiMode:
        case CfgScreen::Brightness:
        case CfgScreen::Sound:
        case CfgScreen::Volume: settings_.open(host, target); break;
        case CfgScreen::Tips: tips_.open(); break;
        case CfgScreen::Theme:
        case CfgScreen::Titles:
        case CfgScreen::Background:
        case CfgScreen::HackerTag: looks_.open(host, target); break;
        case CfgScreen::Audit:
        case CfgScreen::PediaAp:
        case CfgScreen::Link: radio_.open(host, target); break;
        default: break;
    }
    host.disarmHoldB();
    host.openDetail();
}

// Back to the page's group if it has one (a radio toggle returns to RADIO), else out to
// the list. Applying a setting leaves the same way, so B and C land in the same place.
void CfgMenu::leavePage(CfgHost& host) {
    const CfgScreen parent = cfgParentGroup(page_);
    if (parent == page_) { host.closeDetail(); return; }
    // The group's cursor lands on the row just left, so a return trip resumes there.
    const CfgRow* rows = nullptr;
    const int n = cfgGroupRows(parent, rows);
    for (int i = 0; i < n; ++i)
        if (rows[i].target == page_) { groupRow_ = i; break; }
    page_ = parent;
    host.disarmHoldB();
    host.markDirty();
}

void CfgMenu::stepGroup(int dir) {
    const CfgRow* rows = nullptr;
    groupRow_ = stepRow(groupRow_, cfgGroupRows(page_, rows), dir);
}

// A group page (DEVICE / RADIO): a row list one level in. A switch flips where it
// stands; every other row opens its own page.
void CfgMenu::onGroup(CfgHost& host, const ButtonEvent& ev) {
    const CfgRow* rows = nullptr;
    const int n = cfgGroupRows(page_, rows);
    if (n <= 0) { leavePage(host); return; }
    if (groupRow_ >= n) groupRow_ = 0;
    if (ev.button == Button::A) {
        groupRow_ = stepRow(groupRow_, n, +1);
    } else if (ev.button == Button::B) {
        if (rows[groupRow_].target == CfgScreen::CareAlerts)
            settings_.flipCareAlerts(host);
        else
            openPage(host, rows[groupRow_].target);
    } else if (ev.button == Button::C) {
        leavePage(host);
    }
}

// --- L3: the pages ------------------------------------------------------------

void CfgMenu::onDetail(CfgHost& host, const ButtonEvent& ev) {
    switch (page_) {
        case CfgScreen::Device:
        case CfgScreen::Radio:      onGroup(host, ev); break;
        case CfgScreen::SysInfo:    follow(host, hardware_.onSysInfo(host, ev)); break;
        case CfgScreen::Travel:     follow(host, hardware_.onTravel(host, ev)); break;
        case CfgScreen::FormatSd:   follow(host, hardware_.onFormatSd(host, ev)); break;
        case CfgScreen::HackerTag:  follow(host, looks_.onHackerTag(host, ev)); break;
        case CfgScreen::Tips:       follow(host, tips_.onButton(host, ev)); break;
        case CfgScreen::UiMode:     follow(host, settings_.onUiMode(host, ev)); break;
        case CfgScreen::Brightness: follow(host, settings_.onBrightness(host, ev)); break;
        case CfgScreen::Sound:      follow(host, settings_.onSound(host, ev)); break;
        case CfgScreen::Volume:     follow(host, settings_.onVolume(host, ev)); break;
        case CfgScreen::Theme:      follow(host, looks_.onTheme(host, ev)); break;
        case CfgScreen::Titles:     follow(host, looks_.onTitles(host, ev)); break;
        case CfgScreen::Background: follow(host, looks_.onBackground(host, ev)); break;
        case CfgScreen::Audit:      follow(host, radio_.onAudit(host, ev)); break;
        case CfgScreen::PediaAp:    follow(host, radio_.onAp(host, ev)); break;
        case CfgScreen::Link:       follow(host, radio_.onLink(host, ev)); break;
        case CfgScreen::PediaQr:    follow(host, radio_.onPediaQr(host, ev)); break;
        case CfgScreen::Update:     follow(host, updates_.onButton(host, ev)); break;
        case CfgScreen::UpdateQr:   follow(host, updates_.onFlashQr(ev)); break;
        case CfgScreen::FactoryReset:
            if (ev.button == Button::A) pick_ ^= 1;                  // cycle scope
            else if (ev.button == Button::B) host.armHoldB();         // arm hold-to-commit
            else if (ev.button == Button::C) host.closeDetail();      // safe abort
            break;
        case CfgScreen::ResetHatch:
        case CfgScreen::CareAlerts:
            break;  // never pages: the list and the DEVICE group act on them in place
    }
}

// The hidden Factory Reset: a held B reveals the reset page from System Info, then
// commits the wipe there. Each is a deliberate hold of several seconds.
bool CfgMenu::holdB(CfgHost& host, uint32_t heldMs) {
    if (page_ == CfgScreen::SysInfo && heldMs >= kFactoryRevealMs) {
        page_ = CfgScreen::FactoryReset;
        pick_ = 0;
        return true;
    }
    if (page_ == CfgScreen::FactoryReset && heldMs >= kFactoryCommitMs) {
        host.executeFactoryReset(static_cast<FactoryScope>(pick_));
        return true;
    }
    return false;
}

// What a page asked of the menu. A page it shows (a QR behind a row) is a page of the
// menu's like any other, whose parent is the page that showed it, so leaving it lands
// back there.
void CfgMenu::follow(CfgHost& host, CfgMove move) {
    switch (move.kind) {
        case CfgMove::Kind::Stay: break;
        case CfgMove::Kind::Leave: leavePage(host); break;
        case CfgMove::Kind::Show: page_ = move.page; break;
    }
}

// --- Drawing ------------------------------------------------------------------

void CfgMenu::drawDetail(Framebuffer& fb, const CfgHost& host) const {
    switch (page_) {
        case CfgScreen::SysInfo: hardware_.drawSysInfo(fb, host); break;
        case CfgScreen::Device: drawDevicePage(fb, host); break;
        case CfgScreen::Radio: radio_.drawGroup(fb, host, groupRow_); break;
        case CfgScreen::Travel: hardware_.drawTravel(fb, host); break;
        case CfgScreen::FormatSd: hardware_.drawFormatSd(fb, host); break;
        case CfgScreen::HackerTag: looks_.drawHackerTag(fb); break;
        case CfgScreen::Titles: looks_.drawTitles(fb, host); break;
        case CfgScreen::Background: looks_.drawBackground(fb, host); break;
        case CfgScreen::UiMode: settings_.drawUiMode(fb, host); break;
        case CfgScreen::Tips: tips_.draw(fb, host); break;
        case CfgScreen::Brightness: settings_.drawBrightness(fb, host); break;
        case CfgScreen::Sound: settings_.drawSound(fb, host); break;
        case CfgScreen::Volume: settings_.drawVolume(fb, host); break;
        case CfgScreen::Theme: looks_.drawTheme(fb, host); break;
        case CfgScreen::Audit: radio_.drawAudit(fb, host); break;
        case CfgScreen::PediaAp: radio_.drawAp(fb, host); break;
        case CfgScreen::Link: radio_.drawLink(fb, host); break;
        case CfgScreen::Update: updates_.draw(fb, host); break;
        case CfgScreen::UpdateQr: updates_.drawFlashQr(fb, host); break;
        case CfgScreen::PediaQr: radio_.drawPediaQr(fb, host); break;
        case CfgScreen::FactoryReset: {
            const int32_t held = host.heldBMs();
            const float hf = held >= 0 ? static_cast<float>(held) / kFactoryCommitMs : 0.0f;
            drawFactoryReset(fb, pick_, hf);
            break;
        }
        case CfgScreen::ResetHatch:
        case CfgScreen::CareAlerts:
            break;
    }
}

void CfgMenu::drawDevicePage(Framebuffer& fb, const CfgHost& host) const {
    const Settings& s = host.settingsRecord();
    const SceneId bg = host.operatorRecord().backgroundPick;
    CfgDeviceView v;
    v.cursor = groupRow_;
    v.uiMode = s.uiMode;
    v.brightness = s.brightness;
    v.soundMode = s.soundMode;
    v.volume = s.volume;
    v.theme = host.themeName();
    // The row previews the choice, not the place being drawn: AUTO stays AUTO however
    // the pet moves, which is the difference between the two.
    v.background = bg == SceneId::None ? "AUTO" : backgroundFor(bg)->name;
    v.careAlerts = s.careAlerts;
    drawCfgDevice(fb, v);
}

}  // namespace mal
