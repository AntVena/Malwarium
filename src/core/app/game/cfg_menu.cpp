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

namespace {
constexpr const char* kTagAlphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_";

// The kBackgrounds row a scene is, as the picker counts them: 0 for AUTO, else 1 + row.
int backgroundRow(SceneId s) {
    const BackgroundDef* b = backgroundFor(s);
    int row = 0;
    if (b)
        for (int i = 0; i < kBackgroundCount; ++i)
            if (kBackgrounds[i].wire == b->wire) row = i + 1;
    return row;
}

HomeNetwork homeNetwork(const CfgHost& host) {
    return host.netProvisioned() ? HomeNetwork::Saved : HomeNetwork::Unsaved;
}
}  // namespace

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
    const Settings& s = host.settingsRecord();
    page_ = target;
    switch (target) {
        case CfgScreen::Device:
        case CfgScreen::Radio: groupRow_ = 0; break;
        case CfgScreen::Travel: pick_ = 0; break;              // always opens on NO
        case CfgScreen::FormatSd:                              // ...likewise, and a result
            pick_ = 0;                                         // left on screen is spent
            if (host.sdFormatState() != SdFormatState::Working) host.clearSdFormatResult();
            break;
        case CfgScreen::UiMode: pick_ = static_cast<int>(s.uiMode); break;
        case CfgScreen::Tips: tipsRow_ = 0; break;
        case CfgScreen::Brightness: pick_ = s.brightness; break;
        case CfgScreen::Sound: pick_ = static_cast<int>(s.soundMode); break;
        case CfgScreen::Volume: pick_ = s.volume; break;
        case CfgScreen::Theme: pick_ = host.themeRow(); break;
        case CfgScreen::Titles: pick_ = host.operatorRecord().world.equippedTitle; break;
        case CfgScreen::Background: pick_ = backgroundRow(host.operatorRecord().backgroundPick); break;
        case CfgScreen::Audit: pick_ = static_cast<int>(host.auditMode()); break;
        case CfgScreen::PediaAp: pick_ = host.apEnabled() ? 1 : 0; break;
        case CfgScreen::Link: pick_ = host.linkEnabled() ? 1 : 0; break;
        case CfgScreen::HackerTag: {
            // The tag padded to the fixed cell count with '_', a valid alphabet char.
            const char* live = host.operatorRecord().hackerTag;
            for (int i = 0; i < kHackerTagMax; ++i) tag_[i] = live[i] == '\0' ? '_' : live[i];
            tag_[kHackerTagMax] = '\0';
            caret_ = 0;
            break;
        }
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
            host.setCareAlerts(!host.settingsRecord().careAlerts);
        else
            openPage(host, rows[groupRow_].target);
    } else if (ev.button == Button::C) {
        leavePage(host);
    }
}

// --- L3: the pages ------------------------------------------------------------

// The shape every value picker shares: A steps the focus, B applies it and backs out, C
// backs out with nothing changed. `apply` returns whether to back out, so a row that
// refuses (a locked set) or moves on to a page of its own (the AP's QR) stays put.
template <class Step, class Apply>
void CfgMenu::onPicker(CfgHost& host, const ButtonEvent& ev, Step step, Apply apply) {
    if (ev.button == Button::A) step();
    else if (ev.button == Button::B) { if (apply()) leavePage(host); }
    else if (ev.button == Button::C) leavePage(host);
}

void CfgMenu::onDetail(CfgHost& host, const ButtonEvent& ev) {
    switch (page_) {
        case CfgScreen::Device:
        case CfgScreen::Radio:      onGroup(host, ev); break;
        case CfgScreen::Travel:     onTravel(host, ev); break;
        case CfgScreen::FormatSd:   onFormatSd(host, ev); break;
        case CfgScreen::SysInfo:
            // A re-checks the card (a card inserted after boot is picked up, and the
            // result lands on this page's SD line). B arms the hidden hold (holdB).
            if (ev.button == Button::A) host.requestSdRecheck();
            else if (ev.button == Button::B) host.armHoldB();
            else if (ev.button == Button::C) leavePage(host);
            break;
        case CfgScreen::HackerTag:  onHackerTag(host, ev); break;
        case CfgScreen::Tips:       onTips(host, ev); break;
        case CfgScreen::UiMode:
            onPicker(host, ev, [&] { pick_ = stepRow(pick_, 3, +1); },
                     [&] { host.setUiMode(static_cast<UiMode>(pick_)); return true; });
            break;
        case CfgScreen::Brightness:
            onPicker(host, ev, [&] { pick_ = stepRow(pick_, kBrightnessLevels, +1); },
                     [&] { host.setBrightness(pick_); return true; });
            break;
        case CfgScreen::Sound:
            onPicker(host, ev, [&] { pick_ = stepRow(pick_, kSoundModeCount, +1); },
                     [&] { host.setSoundMode(static_cast<SoundMode>(pick_)); return true; });
            break;
        case CfgScreen::Volume:
            onPicker(host, ev,
                     [&] {
                         pick_ = stepRow(pick_, kVolumeLevels, +1);
                         host.previewVolume(pick_);
                     },
                     [&] { host.setVolume(pick_); return true; });
            break;
        case CfgScreen::Theme:
            // A walks every row, locked ones included, as BACKGROUND does: the line under
            // the header says where the focused set comes from, which is how an operator
            // learns there is more to find. A locked row refuses and the page stays, its
            // LOCKED tag being the answer. Applying is instant and total, so the page the
            // operator lands back on is the confirmation.
            onPicker(host, ev, [&] { pick_ = stepRow(pick_, kThemeCount, +1); },
                     [&] { return host.setThemePick(palThemeByName(kThemes[pick_].palette)); });
            break;
        case CfgScreen::Titles:
            // The focus walks NONE (-1) and each unlocked sector Title, skipping locked ones.
            onPicker(host, ev, [&] { pick_ = host.nextSelectableTitle(pick_); },
                     [&] { host.equipTitle(pick_); return true; });
            break;
        case CfgScreen::Background:
            // A walks every row, locked ones included: the line under the header names how
            // the focused row is earned, which is how the page teaches the economy.
            onPicker(host, ev, [&] { pick_ = stepRow(pick_, kBackgroundCount + 1, +1); },
                     [&] {
                         return host.setBackgroundPick(
                             pick_ == 0 ? SceneId::None : kBackgrounds[pick_ - 1].scene);
                     });
            break;
        case CfgScreen::Audit:
            // Escalating picker: OFF -> SCAN -> SCAN+CAP -> OFF.
            onPicker(host, ev, [&] { pick_ = stepRow(pick_, 3, +1); },
                     [&] { host.setAuditMode(static_cast<AuditMode>(pick_)); return true; });
            break;
        case CfgScreen::PediaAp:
            onPicker(host, ev, [&] { pick_ ^= 1; },
                     [&] {
                         host.setApEnabled(pick_ != 0);
                         // Turning the AP ON leads straight to the QR to connect (also the
                         // "see the QR" path when it is already on). OFF has no QR to show.
                         if (pick_ == 0) return true;
                         page_ = CfgScreen::PediaQr;
                         qrPage_ = 0;             // start on the join-network QR
                         return false;
                     });
            break;
        case CfgScreen::Link:
            onPicker(host, ev, [&] { pick_ ^= 1; },
                     [&] { host.setLinkEnabled(pick_ != 0); return true; });
            break;
        case CfgScreen::Update:     follow(host, updates_.onButton(host, ev)); break;
        case CfgScreen::UpdateQr:   follow(host, updates_.onFlashQr(ev)); break;
        case CfgScreen::PediaQr:    onPediaQr(host, ev); break;
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

// Once the request is latched every button is inert: the device tier is already landing
// a save and powering the panel down, and a press that half-cancelled that would leave
// the operator guessing which state they are in.
void CfgMenu::onTravel(CfgHost& host, const ButtonEvent& ev) {
    if (host.travelSleepRequested()) return;
    if (ev.button == Button::A) pick_ ^= 1;
    else if (ev.button == Button::B) {
        if (pick_ == 1) host.requestTravelSleep();
        else leavePage(host);
    } else if (ev.button == Button::C) leavePage(host);
    host.markDirty();
}

// Working: every button is inert, as travel's are once latched, since nothing pressed
// here could stop a format. A result is read, then any button leaves. YES is refused
// while an update job runs: it is writing to this card.
void CfgMenu::onFormatSd(CfgHost& host, const ButtonEvent& ev) {
    const SdFormatState st = host.sdFormatState();
    if (st == SdFormatState::Working) return;
    if (st != SdFormatState::Idle) {
        host.clearSdFormatResult();
        leavePage(host);
    } else if (ev.button == Button::A) {
        pick_ ^= 1;
    } else if (ev.button == Button::B) {
        if (pick_ == 0) leavePage(host);
        else if (!host.updateJobLive()) host.requestSdFormat();
    } else if (ev.button == Button::C) {
        leavePage(host);
    }
    host.markDirty();
}

// An arcade editor: A cycles the focused cell's char, B advances the caret (and at the
// confirm cell, saves), C deletes the previous char (or backs out from the first cell).
void CfgMenu::onHackerTag(CfgHost& host, const ButtonEvent& ev) {
    if (ev.button == Button::A) {
        if (caret_ < kHackerTagMax) {
            const char* pos = std::strchr(kTagAlphabet, tag_[caret_]);
            const int idx = pos ? static_cast<int>(pos - kTagAlphabet) : -1;
            tag_[caret_] = kTagAlphabet[(idx + 1) % static_cast<int>(std::strlen(kTagAlphabet))];
        }
    } else if (ev.button == Button::B) {
        if (caret_ >= kHackerTagMax) {             // the confirm cell
            host.setHackerTag(tag_);
            host.closeDetail();
        } else {
            ++caret_;
        }
    } else if (ev.button == Button::C) {
        if (caret_ == 0) leavePage(host);
        else { --caret_; tag_[caret_] = '_'; }
    }
}

void CfgMenu::onTips(CfgHost& host, const ButtonEvent& ev) {
    const std::vector<Tip> tips = host.replayableTips();
    const int n = static_cast<int>(tips.size());
    if (ev.button == Button::A && n > 0) {
        tipsRow_ = stepRow(tipsRow_, n, +1);
        host.markDirty();
    } else if (ev.button == Button::B && n > 0) {
        if (tipsRow_ >= n) tipsRow_ = 0;
        host.openTip(tips[tipsRow_]);
    } else if (ev.button == Button::C) {
        leavePage(host);
    }
}

// What the UPDATES page asked of the menu. Its flash QR is a page of the menu's like any
// other, whose parent is UPDATES, so leaving it lands back on the rows.
void CfgMenu::follow(CfgHost& host, UpdatesPage::Move move) {
    switch (move) {
        case UpdatesPage::Move::Stay: break;
        case UpdatesPage::Move::Leave: leavePage(host); break;
        case UpdatesPage::Move::OpenFlashQr: page_ = CfgScreen::UpdateQr; break;
    }
}

// Two QR pages: A cycles join-network and the 'Pedia URL; C exits.
void CfgMenu::onPediaQr(CfgHost& host, const ButtonEvent& ev) {
    if (ev.button == Button::A) {
        qrPage_ = stepRow(qrPage_, pediaQrPages(homeNetwork(host)), +1);
        host.markDirty();
    } else if (ev.button == Button::C) {
        leavePage(host);
    }
}

// --- Drawing ------------------------------------------------------------------

void CfgMenu::drawDetail(Framebuffer& fb, const CfgHost& host) const {
    const Settings& s = host.settingsRecord();
    switch (page_) {
        case CfgScreen::SysInfo: drawSysInfoPage(fb, host); break;
        case CfgScreen::Device: drawDevicePage(fb, host); break;
        case CfgScreen::Radio: drawRadioPage(fb, host); break;
        case CfgScreen::Travel:
            // Two faces, like UPDATES: the question, then the notice that replaces it for
            // as long as the device is still on its way down.
            if (host.travelSleepRequested()) drawTravelSleeping(fb);
            else drawTravelConfirm(fb, pick_);
            break;
        case CfgScreen::FormatSd:
            if (host.sdFormatState() == SdFormatState::Idle)
                drawSdFormatConfirm(fb, pick_, host.sdStatus(),
                                    host.updateJobLive() ? SdCardUse::Busy : SdCardUse::Free);
            else
                drawSdFormatStatus(fb, host.sdFormatState(), host.sdStatus());
            break;
        case CfgScreen::HackerTag: drawHackerTag(fb, tag_, caret_); break;
        case CfgScreen::Titles: {
            const OperatorWorld& w = host.operatorRecord().world;
            drawTitles(fb, pick_, w.titlesUnlocked, w.equippedTitle, host.screenBeat());
            break;
        }
        case CfgScreen::Background: drawBackgroundPage(fb, host); break;
        case CfgScreen::UiMode: drawUiModeToggle(fb, pick_, s.uiMode); break;
        case CfgScreen::Tips: drawTipsPage(fb, host); break;
        case CfgScreen::Brightness: drawBrightness(fb, pick_, s.brightness); break;
        case CfgScreen::Sound: drawSoundMode(fb, pick_, s.soundMode); break;
        case CfgScreen::Volume: drawVolume(fb, pick_, s.volume); break;
        case CfgScreen::Theme:
            drawThemePicker(fb, pick_, host.themeRow(), host.themesUnlockedMask());
            break;
        case CfgScreen::Audit: drawAuditMode(fb, pick_, static_cast<int>(host.auditMode())); break;
        case CfgScreen::PediaAp:
            drawApToggle(fb, pick_, host.apEnabled() ? OnOff::On : OnOff::Off);
            break;
        case CfgScreen::Link:
            drawLinkToggle(fb, pick_, host.linkEnabled() ? OnOff::On : OnOff::Off,
                           host.linkAmbientStarved() ? LinkReach::PeersOnly : LinkReach::Ambient);
            break;
        case CfgScreen::Update: updates_.draw(fb, host); break;
        case CfgScreen::UpdateQr: updates_.drawFlashQr(fb, host); break;
        case CfgScreen::PediaQr: drawPediaQr(fb, qrPage_, homeNetwork(host)); break;
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

void CfgMenu::drawSysInfoPage(Framebuffer& fb, const CfgHost& host) const {
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
    drawSysInfo(fb, v);
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

void CfgMenu::drawRadioPage(Framebuffer& fb, const CfgHost& host) const {
    CfgRadioView v;
    v.cursor = groupRow_;
    v.owner = host.radioOwner();
    v.auditLevel = static_cast<int>(host.auditMode());
    v.linkOn = host.linkEnabled();
    v.apOn = host.apEnabled();
    v.updateLive = host.updateJobLive();
    drawCfgRadio(fb, v);
}

void CfgMenu::drawBackgroundPage(Framebuffer& fb, const CfgHost& host) const {
    // Ownership is derived, so the mask is built for the draw rather than held: one bit
    // per row, in the table's own order.
    uint32_t owned = 0;
    for (int i = 0; i < kBackgroundCount; ++i)
        if (host.backgroundOwned(kBackgrounds[i].scene)) owned |= 1u << i;
    drawBackgrounds(fb, pick_, owned, backgroundRow(host.operatorRecord().backgroundPick),
                           host.screenBeat());
}

void CfgMenu::drawTipsPage(Framebuffer& fb, const CfgHost& host) const {
    fb.clear(palColor(Pal::PAPER));
    drawHeaderBand(fb, "TIPS");
    const std::vector<Tip> tips = host.replayableTips();
    const int n = static_cast<int>(tips.size());
    if (n == 0) {
        const char* none = "- NONE SHOWN YET -";
        drawText(fb, (kActiveW - textWidth(none)) / 2, kActiveH / 2, none,
                 palColor(Pal::INK_DIM));
        drawHintBand(fb, "C BACK");
        return;
    }
    const int cursor = tipsRow_ < n ? tipsRow_ : 0;
    const int scrollTop = listScrollTop(cursor, n, kVisibleRows);
    for (int v = 0; v < kVisibleRows && scrollTop + v < n; ++v) {
        const int i = scrollTop + v;
        const int y = kRowTop + v * kRowH;
        if (i == cursor) drawRowSelection(fb, y, kRowH);
        drawText(fb, 22, y + (kRowH - kFontH) / 2, TipCardScreen::title(tips[i]),
                 i == cursor ? palColor(Pal::ACCENT) : palColor(Pal::INK));
    }
    drawHintBand(fb, "A NEXT  B READ  C BACK");
}

}  // namespace mal
