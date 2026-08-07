#include "core/app/game.h"

#include <cstdio>
#include <cstring>

#include "tunables.h"
#include "version.h"
#include "core/render/canvas.h"
#include "core/render/font.h"
#include "core/render/palette.h"
#include "core/render/sprite.h"
#include "core/ui/carousel.h"
#include "core/ui/combat_screen.h"
#include "core/ui/items_screen.h"
#include "core/ui/maint_screen.h"
#include "core/ui/modals.h"
#include "core/ui/stat_screen.h"
#include "core/ui/train_screen.h"
#include "generated/assets.h"

namespace mal {

// CFG -----------------------------------------------------------

// Open an L3 CFG screen, seeding whatever picker it opens on so the focus always
// starts at the applied value. The one seam both the top-level list and a group
// screen enter through, so a row behaves identically wherever it is reached from.
void Game::enterCfgScreen(CfgScreen target) {
    cfgScreen_ = target;
    switch (target) {
        case CfgScreen::Device:
        case CfgScreen::Radio:
            cfgGroupRow_ = 0;
            break;
        case CfgScreen::Travel: cfgTravelPick_ = 0; break;   // always opens on NO
        case CfgScreen::UiMode: cfgUiPick_ = static_cast<int>(uiMode_); break;
        case CfgScreen::Brightness: cfgBrightPick_ = brightness_; break;   // the applied level
        case CfgScreen::Titles: cfgTitlePick_ = equippedTitle_; break;     // the equipped one
        case CfgScreen::Audit: cfgAuditPick_ = static_cast<int>(auditMode()); break;
        case CfgScreen::PediaAp: cfgApPick_ = apEnabled_ ? 1 : 0; break;
        case CfgScreen::Link: cfgLinkPick_ = linkEnabled_ ? 1 : 0; break;
        case CfgScreen::HackerTag: enterHackerTagEditor(); break;
        default: break;
    }
    bHeld_ = false;
    nav_ = Nav::Detail;
}

// Leave the current L3 — back to its group screen if it has one (a radio toggle
// returns to RADIO, not to the top of CFG), otherwise out to the list. Applying a
// setting exits the same way, so B and C land in the same place.
void Game::leaveCfgScreen() {
    const CfgScreen parent = cfgParentGroup(cfgScreen_);
    if (parent == cfgScreen_) { nav_ = Nav::Submenu; return; }
    // Land the group's cursor on the row just left, so a return trip resumes where
    // the operator was rather than at the top.
    const CfgRow* rows = nullptr;
    const int n = cfgGroupRows(parent, rows);
    for (int i = 0; i < n; ++i)
        if (rows[i].target == cfgScreen_) { cfgGroupRow_ = i; break; }
    cfgScreen_ = parent;
    bHeld_ = false;
    dirty_ = true;
}

void Game::onCfgList(const ButtonEvent& ev) {
    const CfgRow* rows = nullptr;
    const int n = cfgRows(rows);
    if (ev.button == Button::A) {
        listRow_ = (listRow_ + 1) % n;
    } else if (ev.button == Button::B) {
        const CfgScreen target = rows[listRow_].target;
        if (target == CfgScreen::ResetHatch) {
            resetToHatch();                  // DEV quick reset — one press (dev_config.h)
            return;
        }
        enterCfgScreen(target);
    } else if (ev.button == Button::C) {
        nav_ = Nav::Cursor;
    }
}

// A group screen (DEVICE / RADIO): a row list like the L2 above it, one level in.
// A walks it, B opens the focused setting, C returns to the CFG list.
void Game::onCfgGroup(const ButtonEvent& ev) {
    const CfgRow* rows = nullptr;
    const int n = cfgGroupRows(cfgScreen_, rows);
    if (n <= 0) { leaveCfgScreen(); return; }
    if (cfgGroupRow_ >= n) cfgGroupRow_ = 0;
    if (ev.button == Button::A) cfgGroupRow_ = (cfgGroupRow_ + 1) % n;
    else if (ev.button == Button::B) enterCfgScreen(rows[cfgGroupRow_].target);
    else if (ev.button == Button::C) leaveCfgScreen();
}

void Game::onCfgDetail(const ButtonEvent& ev) {
    switch (cfgScreen_) {
        case CfgScreen::Device:
        case CfgScreen::Radio:
            onCfgGroup(ev);
            break;
        case CfgScreen::Travel:
            // Once the request is latched every button is inert: the device tier is
            // already landing a save and powering the panel down, and a press that
            // half-cancelled that would leave the operator guessing which of the two
            // states they are in. A ordinarily cycles NO/YES, B commits the focused
            // one, C backs out to the group with nothing asked for.
            if (travelSleepRequested_) break;
            if (ev.button == Button::A) cfgTravelPick_ ^= 1;
            else if (ev.button == Button::B) {
                if (cfgTravelPick_ == 1) requestTravelSleep();
                else leaveCfgScreen();
            } else if (ev.button == Button::C) leaveCfgScreen();
            dirty_ = true;
            break;
        case CfgScreen::SysInfo:
            // A re-checks the card: ask the device tier to unmount + remount so one
            // inserted after boot is picked up. The result lands on this screen's own
            // SD line (+ the idle SD-present icon), which is why the action lives
            // here. B arms the hidden hold-to-reveal (the timer fires in tick); C
            // backs to the list.
            if (ev.button == Button::A) requestSdRecheck();
            else if (ev.button == Button::B) { bHeld_ = true; bDownMs_ = nowMs_; }
            else if (ev.button == Button::C) leaveCfgScreen();
            break;
        case CfgScreen::HackerTag: {
            // On-device arcade editor: A cycles the focused cell's char, B
            // advances the caret (and at the ⏎ confirm cell, saves), C deletes the
            // previous char (or backs out from the first cell).
            static const char* kAlphabet =
                "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_";
            if (ev.button == Button::A) {
                if (editCaret_ < kHackerTagMax) {
                    const char cur = editTag_[editCaret_];
                    const char* pos = std::strchr(kAlphabet, cur);
                    int idx = pos ? static_cast<int>(pos - kAlphabet) : -1;
                    idx = (idx + 1) % static_cast<int>(std::strlen(kAlphabet));
                    editTag_[editCaret_] = kAlphabet[idx];
                }
            } else if (ev.button == Button::B) {
                if (editCaret_ >= kHackerTagMax) saveHackerTag();  // ⏎ confirm
                else ++editCaret_;
            } else if (ev.button == Button::C) {
                if (editCaret_ == 0) leaveCfgScreen();          // back out
                else { --editCaret_; editTag_[editCaret_] = '_'; } // delete prev
            }
            break;
        }
        case CfgScreen::UiMode:
            if (ev.button == Button::A) cfgUiPick_ = (cfgUiPick_ + 1) % 3;
            else if (ev.button == Button::B) {        // apply + back out
                setUiMode(static_cast<UiMode>(cfgUiPick_));
                leaveCfgScreen();
            } else if (ev.button == Button::C) leaveCfgScreen();  // no change
            break;
        case CfgScreen::Brightness:
            // A cycles the level (wrapping); B applies (setBrightness persists +
            // marks the frame dirty, so the device tier re-reads brightness() and
            // drives the backlight); C backs out with no change.
            if (ev.button == Button::A)
                cfgBrightPick_ = (cfgBrightPick_ + 1) % kBrightnessLevels;
            else if (ev.button == Button::B) {        // apply + back out
                setBrightness(cfgBrightPick_);
                leaveCfgScreen();
            } else if (ev.button == Button::C) leaveCfgScreen();  // no change
            break;
        case CfgScreen::Titles:
            // A cycles the focus over the selectable choices only — NONE (-1) plus
            // each UNLOCKED sector Title, skipping locked ones. B equips the focus;
            // C backs out without changing the equip.
            if (ev.button == Button::A) cfgTitlePick_ = nextSelectableTitle(cfgTitlePick_);
            else if (ev.button == Button::B) {        // apply + back out
                equippedTitle_ = cfgTitlePick_;       // -1 (NONE) or an unlocked sector
                markSaveDirty();
                leaveCfgScreen();
            } else if (ev.button == Button::C) leaveCfgScreen();  // no change
            break;
        case CfgScreen::Audit:
            // Escalating picker: A cycles OFF -> SCAN -> SCAN+CAP -> OFF; B applies.
            if (ev.button == Button::A) cfgAuditPick_ = (cfgAuditPick_ + 1) % 3;
            else if (ev.button == Button::B) {        // apply + back out
                setAuditMode(static_cast<AuditMode>(cfgAuditPick_));
                leaveCfgScreen();
            } else if (ev.button == Button::C) leaveCfgScreen();  // no change
            break;
        case CfgScreen::PediaAp:
            if (ev.button == Button::A) cfgApPick_ ^= 1;             // OFF <-> ON
            else if (ev.button == Button::B) {
                setApEnabled(cfgApPick_ != 0);
                // Turning the AP ON leads straight to the QR to connect (also the
                // "see the QR" path when it's already on). Turning it OFF has no QR
                // to show, so it just backs out to the list.
                if (cfgApPick_ != 0) {
                    cfgScreen_ = CfgScreen::PediaQr;
                    pediaQrPage_ = 0;             // start on the join-network QR
                } else {
                    leaveCfgScreen();
                }
            } else if (ev.button == Button::C) leaveCfgScreen();  // no change
            break;
        case CfgScreen::Link:
            if (ev.button == Button::A) cfgLinkPick_ ^= 1;           // OFF <-> ON
            else if (ev.button == Button::B) {
                setLinkEnabled(cfgLinkPick_ != 0);
                leaveCfgScreen();
            } else if (ev.button == Button::C) leaveCfgScreen();  // no change
            break;
        case CfgScreen::Update:
            // Three faces on one screen: the row list, the yes/no confirm an
            // install row opens, and a job's progress. Leaving deliberately does
            // NOT cancel a job — it holds the radio itself, so a download runs to
            // the end and the outcome is waiting here on the way back.
            if (updateJobLive() || installStatus_.state != InstallState::Idle) {
                if (ev.button == Button::C) {
                    // Only a finished job's report is cleared on the way out; a live
                    // one keeps its progress to come back to.
                    if (!updateJobLive()) installStatus_ = InstallStatus{};
                    leaveCfgScreen();
                }
                break;
            }
            if (updateConfirm_ != UpdateTarget::None) {
                if (ev.button == Button::A) updateConfirmPick_ ^= 1;      // NO <-> YES
                else if (ev.button == Button::B) {
                    // The second yes, and the only path to a write.
                    if (updateConfirmPick_ == 1) requestUpdateInstall(updateConfirm_);
                    updateConfirm_ = UpdateTarget::None;
                    updateConfirmPick_ = 0;
                } else if (ev.button == Button::C) {
                    updateConfirm_ = UpdateTarget::None;
                    updateConfirmPick_ = 0;
                }
                dirty_ = true;
                break;
            }
            if (ev.button == Button::A) {
                updateRow_ = (updateRow_ + 1) % updateCheckRows(updateStatus_);
                dirty_ = true;
            } else if (ev.button == Button::B) {
                switch (updateCheckRowKind(updateStatus_, updateRow_)) {
                    case UpdateRowKind::Check:
                        requestUpdateCheck();       // no-ops unless ready
                        updateRow_ = 0;             // a fresh check invalidates the rows
                        break;
                    case UpdateRowKind::Install:
                        updateConfirm_ = updateCheckRowTarget(updateStatus_, updateRow_);
                        updateConfirmPick_ = 0;     // ask before anything is written,
                        dirty_ = true;              // ...starting on NO
                        break;
                    case UpdateRowKind::FlashQr:
                        // Needs no radio and no network — the code is drawn from an
                        // address this device already holds, so it opens even when a
                        // check cannot run, which is when it is most wanted.
                        if (updateSourceKnown_) {
                            cfgScreen_ = CfgScreen::UpdateQr;
                            dirty_ = true;
                        }
                        break;
                }
            } else if (ev.button == Button::C) leaveCfgScreen();
            break;
        case CfgScreen::UpdateQr:
            // One page, so A does nothing here. C returns to UPDATES with its cursor
            // still on the row that opened this. The 5s auto-defocus and the screen
            // sleep are both held off while it shows (qrScreenActive) — this code is
            // read by a phone, which takes longer than the usual idle window.
            if (ev.button == Button::C) leaveCfgScreen();
            break;
        case CfgScreen::PediaQr:
            // Two QR pages: A cycles join-network <-> 'Pedia-URL; C exits. The 5s
            // auto-defocus + the device screen-sleep are both suspended here
            // (qrScreenActive), and resume once C exits — scanning/connecting takes
            // longer than the usual idle window.
            if (ev.button == Button::A) {
                pediaQrPage_ = (pediaQrPage_ + 1) % pediaQrPages(netProvisioned_);
                dirty_ = true;
            } else if (ev.button == Button::C) leaveCfgScreen();
            break;
        case CfgScreen::FactoryReset:
            if (ev.button == Button::A) factoryScope_ ^= 1;          // cycle scope
            else if (ev.button == Button::B) { bHeld_ = true; bDownMs_ = nowMs_; }  // arm hold-to-commit
            else if (ev.button == Button::C) nav_ = Nav::Submenu;    // safe abort
            break;
        case CfgScreen::ResetHatch:
            break;  // never an L3 screen — handled inline in onCfgList
    }
}

void Game::executeFactoryReset() {
    // The two scopes differ in exactly what the screen promises. RESET PET lays a
    // fresh egg over the same operator — resetToHatch() leaves every player-level
    // tally standing, which IS "keeps 'Pedia progress". WIPE EVERYTHING clears that
    // standing state first, so the reset that follows lands on a device with no
    // history. Order matters: the wipe runs BEFORE resetToHatch, so the fresh egg it
    // lays is tallied as raised (a wiped device still knows its own current pet).
    if (factoryScope_ == 1) wipeDeviceProgress();
    resetToHatch();
}

void Game::cycleUiMode() {
    uiMode_ = static_cast<UiMode>((static_cast<int>(uiMode_) + 1) % 3);
    dirty_ = true;
}

void Game::setBrightness(int level) {
    if (level < 0) level = 0;
    if (level >= kBrightnessLevels) level = kBrightnessLevels - 1;
    if (level == brightness_) return;
    brightness_ = level;
    dirty_ = true;
    markSaveDirty();   // a persisted CFG pref (save v14) — survives a reboot
}

void Game::drawCfg(Framebuffer& fb) const {
    switch (cfgScreen_) {
        case CfgScreen::SysInfo: {
            const float hf = bHeld_
                ? static_cast<float>(nowMs_ - bDownMs_) / kFactoryRevealMs
                : 0.0f;
            // RANK reads "R{n} {title}" so the endless NUMERIC rank shows alongside the
            // (threshold-unlocked, growable) title — same "R%d %s" form as STAT.
            char rankLine[28];
            std::snprintf(rankLine, sizeof(rankLine), "R%d %s", hackerRank_,
                          hackerRankTitle(hackerRank_));
            drawSysInfo(fb, nowMs_, uiMode_, kFirmwareVersion,
                       rankLine, networksSeen_, power_,
                       sdStatus_, auditCapture_.enabled(), handshakesSeen_,
                       equippedTitleName(), radioOwner_, hf);
            break;
        }
        case CfgScreen::Device:
            drawCfgDevice(fb, cfgGroupRow_, uiMode_, brightness_); break;
        case CfgScreen::Travel:
            // Two faces, like the UPDATES screen: the question, then the notice that
            // replaces it for as long as the device is still on its way down.
            if (travelSleepRequested_) drawTravelSleeping(fb);
            else drawTravelConfirm(fb, cfgTravelPick_);
            break;
        case CfgScreen::Radio:
            drawCfgRadio(fb, cfgGroupRow_, radioOwner_,
                         static_cast<int>(auditMode()), linkEnabled_, apEnabled_,
                         updateJobLive());
            break;
        case CfgScreen::HackerTag: drawHackerTag(fb, editTag_, editCaret_); break;
        case CfgScreen::Titles:
            drawTitles(fb, cfgTitlePick_, titlesUnlocked_, equippedTitle_, beat_); break;
        case CfgScreen::UiMode: drawUiModeToggle(fb, cfgUiPick_, uiMode_); break;
        case CfgScreen::Brightness: drawBrightness(fb, cfgBrightPick_, brightness_); break;
        case CfgScreen::Audit:
            drawAuditMode(fb, cfgAuditPick_, static_cast<int>(auditMode())); break;
        case CfgScreen::PediaAp: drawApToggle(fb, cfgApPick_, apEnabled_); break;
        case CfgScreen::Link:
            drawLinkToggle(fb, cfgLinkPick_, linkEnabled_, linkAmbientStarved());
            break;
        case CfgScreen::Update:
            // Same three faces as the input handler, in the same order — a live or
            // just-finished job outranks the confirm, which outranks the list.
            if (updateJobLive() || installStatus_.state != InstallState::Idle) {
                if (updateJobTarget() == UpdateTarget::None &&
                    installStatus_.state == InstallState::Idle) {
                    drawUpdateCheck(fb, updateCheckReady(), netProvisioned_,
                                    updateSourceKnown_, updateStatus_, netStatus_,
                                    kFirmwareVersion, updateManifestUrl_, updateRow_);
                } else {
                    drawUpdateProgress(fb, installStatus_, netStatus_);
                }
            } else if (updateConfirm_ != UpdateTarget::None) {
                const bool fw = updateConfirm_ == UpdateTarget::Firmware;
                drawUpdateConfirm(fb, updateConfirm_,
                                  fw ? kFirmwareVersion : webBundleVersion_,
                                  fw ? updateStatus_.firmwareVersion
                                     : updateStatus_.webVersion,
                                  updateConfirmPick_);
            } else {
                drawUpdateCheck(fb, updateCheckReady(), netProvisioned_,
                                updateSourceKnown_, updateStatus_, netStatus_,
                                kFirmwareVersion, updateManifestUrl_, updateRow_);
            }
            break;
        case CfgScreen::UpdateQr: drawUpdateQr(fb, updateManifestUrl_); break;
        case CfgScreen::PediaQr: drawPediaQr(fb, pediaQrPage_, netProvisioned_); break;
        case CfgScreen::FactoryReset: {
            const float hf = bHeld_
                ? static_cast<float>(nowMs_ - bDownMs_) / kFactoryCommitMs
                : 0.0f;
            drawFactoryReset(fb, factoryScope_, hf);
            break;
        }
        case CfgScreen::ResetHatch: break;
    }
}

// HackerTag editor --------------------------------------------

void Game::enterHackerTagEditor() {
    // Seed the working buffer from the live tag, padding to the fixed cell count
    // with '_' (a valid alphabet char). Caret starts at the first cell.
    for (int i = 0; i < kHackerTagMax; ++i) {
        const char c = hackerTag_[i];
        editTag_[i] = (c == '\0') ? '_' : c;
    }
    editTag_[kHackerTagMax] = '\0';
    editCaret_ = 0;
}

void Game::saveHackerTag() {
    std::strncpy(hackerTag_, editTag_, sizeof(hackerTag_) - 1);
    hackerTag_[sizeof(hackerTag_) - 1] = '\0';
    markSaveDirty();
    nav_ = Nav::Submenu;
}

// Web 'Pedia rename ('s "one safe write") — the same charset the on-device
// arcade editor cycles through, validated up front so a bad POST never mutates
// the live tag.
bool Game::setHackerTag(const char* s) {
    if (!s) return false;
    const size_t len = std::strlen(s);
    if (len == 0 || len > static_cast<size_t>(kHackerTagMax)) return false;
    for (size_t i = 0; i < len; ++i) {
        const char c = s[i];
        const bool ok = (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
        if (!ok) return false;
    }
    std::strncpy(hackerTag_, s, sizeof(hackerTag_) - 1);
    hackerTag_[sizeof(hackerTag_) - 1] = '\0';
    markSaveDirty();
    return true;
}


}  // namespace mal
