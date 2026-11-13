// cfg_menu.h: the CFG slot's screens, as a feature that owns its own state.
//
// The CFG list (L2), its two group screens (DEVICE, RADIO) and every L3 behind them: the
// value pickers, the travel and SD-format confirms, System Info with its hidden Factory
// Reset, the HackerTag editor, TIPS, UPDATES and the two QR pages. The screen holds which
// page is open, the cursors, the open page's focused choice, the tag being typed and
// the UPDATES row and confirm. The settings, the radios and the jobs are the game's,
// reached through CfgHost.
//
// This header names no core/ui type beyond ui_state.h, so game.h can hold a CfgMenu by
// value.
#pragma once

#include <cstdint>
#include <vector>

#include "tunables.h"                   // kHackerTagMax
#include "core/app/operator.h"          // Tip
#include "core/device/platform.h"
#include "core/device/power_status.h"
#include "core/device/radio_status.h"
#include "core/device/sd_status.h"
#include "core/device/update_status.h"
#include "core/device/net_status.h"
#include "core/render/scene_id.h"
#include "core/audio/sound.h"           // SoundMode
#include "core/net/audit_capture.h"     // AuditMode
#include "core/ui/ui_state.h"           // CfgScreen, UiMode

namespace mal {

class Framebuffer;
struct Settings;

// What a Factory Reset clears, the choice A cycles on its page.
enum class FactoryScope : uint8_t { ResetPet, WipeEverything };

// What CFG needs from the game. Game implements it; a test can too.
class CfgHost {
public:
    virtual const Settings& settingsRecord() const = 0;
    virtual const Operator& operatorRecord() const = 0;
    virtual int screenBeat() const = 0;
    virtual uint32_t uptimeMs() const = 0;
    // How long B has been held on this page, or -1 with it up.
    virtual int32_t heldBMs() const = 0;

    // What the pages show.
    virtual RadioOwner radioOwner() const = 0;
    virtual const SdStatus& sdStatus() const = 0;
    virtual const PowerStatus& powerStatus() const = 0;
    virtual SdFormatState sdFormatState() const = 0;
    virtual bool travelSleepRequested() const = 0;
    virtual bool captureEnabled() const = 0;
    virtual const char* equippedTitleName() const = 0;
    virtual const char* themeName() const = 0;
    virtual int themeRow() const = 0;
    virtual uint32_t themesUnlockedMask() const = 0;
    virtual int nextSelectableTitle(int cur) const = 0;
    virtual bool backgroundOwned(SceneId s) const = 0;
    virtual AuditMode auditMode() const = 0;
    virtual bool apEnabled() const = 0;
    virtual bool linkEnabled() const = 0;
    virtual bool linkAmbientStarved() const = 0;
    virtual bool netProvisioned() const = 0;
    virtual bool updateSourceKnown() const = 0;
    virtual bool updateCheckReady() const = 0;
    virtual bool updateJobLive() const = 0;
    virtual UpdateTarget updateJobTarget() const = 0;
    virtual const UpdateStatus& updateStatus() const = 0;
    virtual const InstallStatus& installStatus() const = 0;
    virtual const NetStatus& netStatus() const = 0;
    virtual const char* updateManifestUrl() const = 0;
    virtual const char* webBundleVersion() const = 0;
    // The tips seen so far, in the order TIPS lists them.
    virtual std::vector<Tip> replayableTips() const = 0;

    // What the pages change. A setter returning bool refuses a locked choice.
    virtual void setUiMode(UiMode m) = 0;
    virtual void setBrightness(int level) = 0;
    virtual void setSoundMode(SoundMode m) = 0;
    virtual void previewVolume(int level) = 0;
    virtual void setVolume(int level) = 0;
    virtual bool setThemePick(int paletteIndex) = 0;
    virtual void equipTitle(int sector) = 0;          // -1 for NONE
    virtual bool setBackgroundPick(SceneId s) = 0;
    virtual void setAuditMode(AuditMode m) = 0;
    virtual void setApEnabled(bool on) = 0;
    virtual void setLinkEnabled(bool on) = 0;
    virtual void setCareAlerts(bool on) = 0;
    virtual bool setHackerTag(const char* tag) = 0;   // false on a tag it refuses
    virtual void requestTravelSleep() = 0;
    virtual void requestSdFormat() = 0;
    virtual void clearSdFormatResult() = 0;
    virtual void requestSdRecheck() = 0;
    virtual void requestUpdateCheck() = 0;
    virtual void requestUpdateInstall(UpdateTarget t) = 0;
    virtual void clearInstallReport() = 0;
    virtual void resetToHatch() = 0;
    virtual void executeFactoryReset(FactoryScope scope) = 0;
    virtual void openTip(Tip t) = 0;                  // read, then back to TIPS
    virtual void armHoldB() = 0;
    virtual void disarmHoldB() = 0;
    virtual void markDirty() = 0;
    virtual void openDetail() = 0;
    virtual void closeDetail() = 0;
    virtual void leaveToCarousel() = 0;

protected:
    ~CfgHost() = default;
};

class CfgMenu {
public:
    void enter() { row_ = 0; }
    void reset() { *this = CfgMenu{}; }

    // L2: the CFG list.
    void onButton(CfgHost& host, const ButtonEvent& ev);
    void stepList(const CfgHost& host, int dir);
    void draw(Framebuffer& fb, const CfgHost& host) const;
    // L3: the open page. A group page is a list too.
    void onDetail(CfgHost& host, const ButtonEvent& ev);
    bool detailIsList() const { return page_ == CfgScreen::Device || page_ == CfgScreen::Radio; }
    void stepGroup(int dir);
    void drawDetail(Framebuffer& fb, const CfgHost& host) const;
    // System Info's hidden hold reveals Factory Reset, whose own hold commits it.
    bool holdB(CfgHost& host, uint32_t heldMs);
    void releaseB(CfgHost&) {}

    CfgScreen page() const { return page_; }
    // Which QR step the 'Pedia page shows (pediaQrPages orders them).
    int pediaQrPage() const { return qrPage_; }

private:
    void openPage(CfgHost& host, CfgScreen target);
    void leavePage(CfgHost& host);
    void onGroup(CfgHost& host, const ButtonEvent& ev);
    template <class Step, class Apply>
    void onPicker(CfgHost& host, const ButtonEvent& ev, Step step, Apply apply);
    void onTravel(CfgHost& host, const ButtonEvent& ev);
    void onFormatSd(CfgHost& host, const ButtonEvent& ev);
    void onHackerTag(CfgHost& host, const ButtonEvent& ev);
    void onTips(CfgHost& host, const ButtonEvent& ev);
    void onUpdate(CfgHost& host, const ButtonEvent& ev);
    void onUpdateRows(CfgHost& host, const ButtonEvent& ev);
    void onUpdateConfirm(CfgHost& host, const ButtonEvent& ev);
    void onPediaQr(CfgHost& host, const ButtonEvent& ev);
    void drawSysInfoPage(Framebuffer& fb, const CfgHost& host) const;
    void drawDevicePage(Framebuffer& fb, const CfgHost& host) const;
    void drawRadioPage(Framebuffer& fb, const CfgHost& host) const;
    void drawBackgroundPage(Framebuffer& fb, const CfgHost& host) const;
    void drawTipsPage(Framebuffer& fb, const CfgHost& host) const;
    void drawUpdatePage(Framebuffer& fb, const CfgHost& host) const;

    int row_ = 0;                          // the CFG list
    CfgScreen page_ = CfgScreen::SysInfo;  // the open L3
    // A group page's cursor, kept apart from row_ so backing out of a group lands on
    // the row that opened it.
    int groupRow_ = 0;
    // The open page's focused choice: a picker's row, a confirm's NO/YES, the Factory
    // Reset scope. Only one page is open at a time, and each seeds it on opening.
    int pick_ = 0;
    int qrPage_ = 0;
    int tipsRow_ = 0;
    // The HackerTag editor: the tag being typed, and the caret (kHackerTagMax is the
    // confirm cell).
    char tag_[kHackerTagMax + 1] = {0};
    int caret_ = 0;
    // UPDATES: the focused row, and the target sitting at its yes/no confirm. Screen
    // state: the confirm clears on the way out, the job does not.
    int updateRow_ = 0;
    UpdateTarget updateConfirm_ = UpdateTarget::None;
    int updateConfirmPick_ = 0;   // 0 = NO, and NO is always where it starts
};

}  // namespace mal
