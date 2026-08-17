// cfg_screen.cpp — the CFG settings list and its L3 viewers/toggles/groups.
//
// The UPDATES screens and the QR pages are the same CFG surface but a different
// kind of screen — they run a job rather than read and write local state — and
// live in update_screen.cpp. Both units are declared by cfg_screen.h.
#include "core/ui/cfg_screen.h"

#include <algorithm>
#include <cstdio>

#include "tunables.h"
#include "core/render/canvas.h"
#include "core/render/font.h"
#include "core/render/framebuffer.h"
#include "core/render/palette.h"
#include "core/render/sprite.h"
#include "core/ui/expl_screen.h"  // sectorTitle, kExplSectors
#include "core/ui/layout.h"
#include "core/ui/widgets.h"
#include "dev_config.h"
#include "generated/assets.h"

namespace mal {

namespace {

// A simple key/value info line for the read-only viewers.
void infoLine(Framebuffer& fb, int y, const char* key, const char* val,
              Rgb565 valColor) {
    drawText(fb, kMargin, y, key, palColor(Pal::INK_DIM));
    drawText(fb, kMargin + 48, y, val, valColor);
}

// One settings row — icon, label, and an optional right-aligned value preview.
// Shared by the top-level list and both group screens so a row looks the same
// wherever it is drawn. `valColor` is the caller's, because what a value's
// emphasis MEANS is the screen's business, not the row's.
void settingsRow(Framebuffer& fb, int y, const CfgRow& row, bool focused,
                 const char* val, Rgb565 valColor) {
    if (focused) {
        fb.fillRect(4, y + 2, kActiveW - 8, kRowH - 4, palColor(Pal::TRACK));
        drawRowCursor(fb, 8, y + (kRowH - 7) / 2, palColor(Pal::ACCENT));
    }
    if (row.icon)
        drawSprite(fb, *row.icon, 0, 16, y + (kRowH - kRowIcon) / 2);
    drawText(fb, 40, y + (kRowH - kFontH) / 2, row.label, palColor(Pal::INK));
    if (val)
        drawText(fb, kActiveW - kMargin - textWidth(val),
                 y + (kRowH - kFontH) / 2, val, valColor);
}

// A 7x7 radio button. Filled = this one is on the air; hollow = switched on and
// waiting behind something above it. Two shapes, not two colours — the pair has to
// read apart in a grayscale screenshot, which a filled/unfilled disc does and an
// accent/ink disc would not.
void drawRadioMark(Framebuffer& fb, int x, int y, bool filled, Rgb565 c) {
    static const int kInset[7] = {2, 1, 0, 0, 0, 1, 2};
    for (int row = 0; row < 7; ++row) {
        const int x0 = x + kInset[row];
        const int w = 7 - 2 * kInset[row];
        if (filled || row == 0 || row == 6) {
            fb.fillRect(x0, y + row, w, 1, c);
        } else {  // hollow: the two rim pixels only
            fb.fillRect(x0, y + row, 1, 1, c);
            fb.fillRect(x0 + w - 1, y + row, 1, 1, c);
        }
    }
}

// One RADIO row. A row that is switched ON is INDENTED under a radio mark, so the
// rows in contention read as one group and the filled mark inside that group says
// which of them actually has the radio. A row that is off sits flush left with no
// mark — it is outside the group, not a member of it that happens to be losing.
// `val` is what the operator set the row to; the mark is what the arbiter did with
// it, and the two never restate each other.
//
// No row icon, unlike every other CFG list: the mark IS this screen's glyph and it
// wants the gutter. The rows would otherwise draw one duplicate of the SYSTEM INFO
// glyph and one 28px carousel sprite in a 20px slot that overruns its own label,
// and the labels say more than either. A 20px radio-family set would earn the
// column back.
void radioRow(Framebuffer& fb, int y, const CfgRow& row, bool focused, bool on,
              bool live, const char* val) {
    const int textY = y + (kRowH - kFontH) / 2;
    if (focused) {
        fb.fillRect(4, y + 2, kActiveW - 8, kRowH - 4, palColor(Pal::TRACK));
        drawRowCursor(fb, 8, textY, palColor(Pal::ACCENT));
    }
    if (on) drawRadioMark(fb, 18, textY, live, palColor(live ? Pal::ACCENT : Pal::INK));
    drawText(fb, on ? 32 : 18, textY, row.label, palColor(Pal::INK));
    drawText(fb, kActiveW - kMargin - textWidth(val), textY, val,
             palColor(on ? Pal::INK : Pal::INK_DIM));
}

} // namespace

const char* uiModeName(UiMode m) {
    switch (m) {
        case UiMode::IconsLabel: return "ICONS+LABEL";
        case UiMode::IconsOnly: return "ICONS ONLY";
        case UiMode::TextOnly: return "TEXT ONLY";
    }
    return "?";
}

int cfgRows(const CfgRow*& out) {
    // Six rows = exactly kVisibleRows, so the release list never scrolls: anything
    // that would be a seventh belongs in one of the two group screens below. The
    // DEV reset row is appended only in dev builds (and is the one thing that does
    // push the list into scrolling — a dev build, deliberately).
    static const CfgRow kRows[] = {
        {"SYSTEM INFO", &ASSET_ICON_CFG_SYSINFO, CfgScreen::SysInfo},
        {"HACKERTAG", &ASSET_ICON_CFG_TAG, CfgScreen::HackerTag},
        {"TITLE", &ASSET_ICON_CFG_TITLE, CfgScreen::Titles},
        {"DEVICE", &ASSET_ICON_CFG_UIMODE, CfgScreen::Device},
        {"RADIO", &ASSET_ICON_CFG_RADIO, CfgScreen::Radio},
        {"UPDATES", &ASSET_ICON_CFG_UPDATE, CfgScreen::Update},
        // SD RECHECK is not a row: it acts on the SD line it reports through, so it
        // is the A press on System Info. PEDIA QR isn't one either — it's reached
        // from PEDIA AP (turning the AP ON opens the QR to connect).
#ifdef DEV_CFG_RESET_ROW
        {"RESET TO HATCH", &ASSET_ICON_CFG, CfgScreen::ResetHatch},
#endif
    };
    out = kRows;
    return static_cast<int>(sizeof(kRows) / sizeof(kRows[0]));
}

int cfgGroupRows(CfgScreen group, const CfgRow*& out) {
    // The device itself, rather than the world in it: how it presents, and whether
    // it is running at all. None of the three is worth a top-level row, and
    // BRIGHTNESS reads as neighbour to both — it is the panel's setting and the
    // largest battery lever short of switching the device off, which is the row
    // under it.
    static const CfgRow kDevice[] = {
        {"UI MODE", &ASSET_ICON_CFG_UIMODE, CfgScreen::UiMode},
        {"BRIGHTNESS", &ASSET_ICON_CFG_UIMODE, CfgScreen::Brightness},
        {"TRAVEL MODE", &ASSET_ICON_CFG_TRAVEL, CfgScreen::Travel},
    };
    // The three radio TOGGLES, listed in the arbiter's own priority order, highest
    // first — so "the one nearest the top wins" is a rule the reader can check
    // against the screen instead of being told. PEDIA AP hosts, LINK transmits,
    // AUDIT listens. PASSIVE LINK is named for the half it is: consent to be heard,
    // against the Hacker face's LINK slot, which is the duel that consent enables.
    // Each is still its own affirmative choice on its own screen:
    // grouping them says they contend for one radio, not that switching one on
    // switches another.
    //
    // A running update job outranks all three and is NOT here, because it is not a
    // toggle — nothing about it is set in advance. drawCfgRadio draws it in above
    // them for exactly as long as it holds the radio.
    static const CfgRow kRadio[] = {
        {"PEDIA AP", &ASSET_ICON_SYS_WIFI, CfgScreen::PediaAp},
        {"PASSIVE LINK", &ASSET_ICON_CREW, CfgScreen::Link},
        {"AUDIT", &ASSET_ICON_CFG_SYSINFO, CfgScreen::Audit},
    };
    if (group == CfgScreen::Device) {
        out = kDevice;
        return static_cast<int>(sizeof(kDevice) / sizeof(kDevice[0]));
    }
    if (group == CfgScreen::Radio) {
        out = kRadio;
        return static_cast<int>(sizeof(kRadio) / sizeof(kRadio[0]));
    }
    out = nullptr;
    return 0;
}

CfgScreen cfgParentGroup(CfgScreen s) {
    switch (s) {
        case CfgScreen::UiMode:
        case CfgScreen::Brightness:
        case CfgScreen::Travel:
            return CfgScreen::Device;
        case CfgScreen::Audit:
        case CfgScreen::Link:
        case CfgScreen::PediaAp:
        case CfgScreen::PediaQr:
            return CfgScreen::Radio;
        // Not a group child — UPDATES is a plain L3 screen — but backing out to it
        // is what leaves the cursor on the row that opened the code.
        case CfgScreen::UpdateQr:
            return CfgScreen::Update;
        default:
            return s;
    }
}

void drawCfgList(Framebuffer& fb, int cursor, const char* hackerTag,
                 const char* equippedTitle, RadioOwner radioOwner) {
    drawHeaderBand(fb, "CFG");
    const CfgRow* rows = nullptr;
    const int n = cfgRows(rows);

    // Scroll a cursor-following window when the list is taller than the viewport
    // (mirrors items_screen). Every CFG row is selectable, so no header-skipping.
    // The release table fits exactly, so this only ever engages in a dev build.
    const int scrollTop = listScrollTop(cursor, n, kVisibleRows);

    for (int v = 0; v < kVisibleRows && scrollTop + v < n; ++v) {
        const int i = scrollTop + v;
        const char* val = nullptr;
        if (rows[i].target == CfgScreen::HackerTag) val = hackerTag;
        else if (rows[i].target == CfgScreen::Titles) val = equippedTitle;
        else if (rows[i].target == CfgScreen::Radio) val = radioOwnerName(radioOwner);
        settingsRow(fb, kRowTop + v * kRowH, rows[i], i == cursor, val,
                    palColor(Pal::INK_DIM));
    }

    if (n > kVisibleRows) {  // slim scrollbar (UI_SCROLLBAR), matching items_screen
        const int barX = kActiveW - 3;
        const int trackH = kVisibleRows * kRowH;
        fb.fillRect(barX, kRowTop, 2, trackH, palColor(Pal::TRACK));
        const int thumbH = std::max(8, trackH * kVisibleRows / n);
        const int thumbY = kRowTop + trackH * scrollTop / n;
        fb.fillRect(barX, thumbY, 2, thumbH, palColor(Pal::INK_DIM));
    }
    drawHintBand(fb, "A CYCLE  B OPEN  C BACK");
}

void drawCfgDevice(Framebuffer& fb, int cursor, UiMode uiMode, int brightness) {
    drawHeaderBand(fb, "DEVICE");
    const CfgRow* rows = nullptr;
    const int n = cfgGroupRows(CfgScreen::Device, rows);

    char brightBuf[8];
    std::snprintf(brightBuf, sizeof(brightBuf), "%d%%", brightnessPercent(brightness));
    for (int i = 0; i < n; ++i) {
        const char* val = nullptr;   // TRAVEL MODE is an action: no value to preview
        if (rows[i].target == CfgScreen::UiMode) val = uiModeName(uiMode);
        else if (rows[i].target == CfgScreen::Brightness) val = brightBuf;
        settingsRow(fb, kRowTop + i * kRowH, rows[i], i == cursor, val,
                    palColor(Pal::INK_DIM));
    }
    drawHintBand(fb, "A NEXT  B OPEN  C BACK");
}

void drawTravelConfirm(Framebuffer& fb, int pick) {
    drawHeaderBand(fb, "TRAVEL MODE?");

    // What it costs and what it buys, in that order. "Nothing ages" is the whole
    // point of the mode and the reason it is not the same as switching the device
    // off mid-lifecycle, so it leads.
    drawText(fb, kMargin, 30, "THE DEVICE SLEEPS UNTIL", palColor(Pal::INK_DIM));
    drawText(fb, kMargin, 42, "YOU WAKE IT.", palColor(Pal::INK_DIM));
    drawText(fb, kMargin, 60, "NOTHING AGES WHILE IT IS", palColor(Pal::INK));
    drawText(fb, kMargin, 72, "ASLEEP - NO HUNGER, NO", palColor(Pal::INK));
    drawText(fb, kMargin, 84, "DECAY, NO GROWTH.", palColor(Pal::INK));

    // The one instruction a dark device cannot give, so it is given here and
    // repeated on the frame that follows (drawTravelSleeping).
    drawText(fb, kMargin, 104, "WAKE: HOLD B+C TOGETHER", palColor(Pal::ACCENT));

    static const char* kOpts[2] = {"NO", "YES, SLEEP"};
    for (int i = 0; i < 2; ++i) {
        const int y = 126 + i * 20;
        if (i == pick) {
            fb.fillRect(4, y - 2, kActiveW - 8, 18, palColor(Pal::TRACK));
            drawRowCursor(fb, 8, y + 3, palColor(Pal::ACCENT));
        }
        drawText(fb, 24, y + 3, kOpts[i],
                 palColor(i == 1 ? Pal::WARN : Pal::INK));
    }
    drawHintBand(fb, "A CHOOSE  B CONFIRM  C BACK");
}

void drawTravelSleeping(Framebuffer& fb) {
    drawHeaderBand(fb, "TRAVEL MODE");
    drawText(fb, kMargin, 80, "GOING TO SLEEP...", palColor(Pal::INK));
    drawText(fb, kMargin, 104, "HOLD B+C TOGETHER", palColor(Pal::ACCENT));
    drawText(fb, kMargin, 116, "TO WAKE ME UP.", palColor(Pal::ACCENT));
}

void drawCfgRadio(Framebuffer& fb, int cursor, RadioOwner owner, int auditLevel,
                  bool linkOn, bool apOn, bool updateLive) {
    drawHeaderBand(fb, "RADIO");
    const CfgRow* rows = nullptr;
    const int n = cfgGroupRows(CfgScreen::Radio, rows);

    // The line the group exists for. Several rows can be switched on at once while
    // exactly one of them is on the air, so the screen names that one in words
    // before any mark has to be decoded — and names it even when it is nobody,
    // because "the radio is doing nothing" is a state the operator asked for too.
    char now[40];
    std::snprintf(now, sizeof(now), "ON AIR: %s",
                  owner == RadioOwner::None ? "NOBODY" : radioOwnerName(owner));
    drawText(fb, kMargin, 30, now,
             palColor(owner == RadioOwner::None ? Pal::INK_DIM : Pal::ACCENT));

    // A running update job outranks every toggle below, so it is drawn where its
    // priority puts it: above them, in the group, marked like them. It appears only
    // while it holds the radio and it is not a row the cursor can reach — there is
    // nothing to open, because nothing about it was set in advance. Without it the
    // screen would show three rows waiting on a winner it never named.
    int top = 44;
    if (updateLive) {
        static const CfgRow kJob = {"UPDATE", nullptr, CfgScreen::Update};
        radioRow(fb, top, kJob, false, true, owner == RadioOwner::Update, "RUNNING");
        top += kRowH;
    }

    for (int i = 0; i < n; ++i) {
        const char* val = "OFF";
        bool on = false;
        bool live = false;
        switch (rows[i].target) {
            case CfgScreen::PediaAp:
                val = apOn ? "ON" : "OFF";
                on = apOn;
                live = owner == RadioOwner::Ap;
                break;
            case CfgScreen::Link:
                val = linkOn ? "ON" : "OFF";
                on = linkOn;
                live = owner == RadioOwner::Link;
                break;
            // One row, two arbiter owners: the ladder's top rung runs as Capture and
            // its lower one as Scan, and either of them means this row is the one
            // holding the radio.
            case CfgScreen::Audit:
                val = auditLevel >= 2 ? "SCAN+CAP" : auditLevel == 1 ? "SCAN" : "OFF";
                on = auditLevel > 0;
                live = owner == RadioOwner::Scan || owner == RadioOwner::Capture;
                break;
            default: break;
        }
        radioRow(fb, top + i * kRowH, rows[i], i == cursor, on, live, val);
    }

    // What the indent and the two marks mean. An operator who switched two rows on
    // is not looking at a bug, and this is where the screen says so — once, rather
    // than on each of the three screens the rows open.
    drawRadioMark(fb, kMargin, 158, true, palColor(Pal::ACCENT));
    drawText(fb, kMargin + 11, 158, "ON AIR", palColor(Pal::INK_DIM));
    drawRadioMark(fb, 76, 158, false, palColor(Pal::INK));
    drawText(fb, 87, 158, "WAITING", palColor(Pal::INK_DIM));
    drawText(fb, kMargin, 172, "NEAREST THE TOP WINS", palColor(Pal::INK_DIM));
    drawHintBand(fb, "A NEXT  B OPEN  C BACK");
}

void drawSysInfo(Framebuffer& fb, uint32_t uptimeMs, UiMode uiMode,
                 const char* fwVersion, const char* rankTitle, int networksSeen,
                 const PowerStatus& power, const SdStatus& sd, bool captureEnabled,
                 int handshakesSeen, const char* equippedTitle, RadioOwner owner,
                 float holdFrac) {
    drawHeaderBand(fb, "SYSTEM INFO");

    // RADIO: which contender the arbiter has actually granted, named in
    // words. Four toggles resolve to one owner, and this is the only screen that
    // reports the resolution rather than an intent.
    infoLine(fb, 34, "RADIO", radioOwnerName(owner),
             owner == RadioOwner::None ? palColor(Pal::INK_DIM) : palColor(Pal::INK));

    // SD is live: the device tier feeds a real mount reading. Present -> capacity
    // ("nnMB", the number is the non-colour channel), or "OK" if size is unknown;
    // absent (host / no card / bad card) -> dual-coded "ABSENT".
    char sdBuf[16];
    const char* sdVal = "ABSENT";
    Rgb565 sdColor = palColor(Pal::INK_DIM);
    if (sd.present) {
        if (sd.sizeMB > 0) {
            std::snprintf(sdBuf, sizeof(sdBuf), "%uMB",
                          static_cast<unsigned>(sd.sizeMB));
            sdVal = sdBuf;
        } else {
            sdVal = "OK";
        }
        sdColor = palColor(Pal::INK);
    }
    infoLine(fb, 50, "SD", sdVal, sdColor);

    // BATT: live once the device tier feeds a reading. "CHG nn%" while charging
    // (external power in), "nn%" on battery, "-" when absent (host / no monitor).
    // The number is the non-colour channel; a low pack just adds HOT emphasis.
    char batt[16];
    const char* battVal = "-";
    Rgb565 battColor = palColor(Pal::INK_DIM);
    if (power.present && power.percent >= 0) {
        if (power.charging) {
            std::snprintf(batt, sizeof(batt), "CHG %d%%", power.percent);
            battColor = palColor(Pal::INK);
        } else {
            std::snprintf(batt, sizeof(batt), "%d%%", power.percent);
            battColor = power.percent <= 15 ? palColor(Pal::HOT) : palColor(Pal::INK);
        }
        battVal = batt;
    }
    infoLine(fb, 66, "BATT", battVal, battColor);

    char up[12];
    const uint32_t secs = uptimeMs / 1000u;
    std::snprintf(up, sizeof(up), "%u:%02u", secs / 60u,
                  static_cast<unsigned>(secs % 60u));
    infoLine(fb, 82, "UP", up, palColor(Pal::INK));
    infoLine(fb, 98, "FW", fwVersion, palColor(Pal::INK));
    infoLine(fb, 114, "UI", uiModeName(uiMode), palColor(Pal::INK));

    // Hacker Rank: the "User Registry" progression tier, beside the
    // HackerTag list row.
    infoLine(fb, 130, "RANK", rankTitle, palColor(Pal::INK));
    char nets[16];
    std::snprintf(nets, sizeof(nets), "%d", networksSeen);
    infoLine(fb, 146, "NETS", nets, palColor(Pal::INK));

    // TITLE: the equipped zone-completion Title, part of the User Registry
    // beside RANK. "NONE" when nothing is equipped — dual-coded (text), grayscale-safe.
    infoLine(fb, 162, "TITLE", equippedTitle, palColor(Pal::INK));

    // SHAKES: lifetime unique WPA handshakes captured. Only meaningful
    // when the pcap capture toggle is on, so it's drawn beneath TITLE only then.
    // Deduped by BSSID engine-side, so revisiting a known network never re-credits.
    if (captureEnabled) {
        char shakes[16];
        std::snprintf(shakes, sizeof(shakes), "%d", handshakesSeen);
        infoLine(fb, 178, "SHAKES", shakes, palColor(Pal::INK));
    }

    // The hidden Factory-Reset reveal: holding B fills a bar. Drawn only
    // while held so the gesture stays undocumented on a resting screen.
    if (holdFrac > 0.0f) {
        drawText(fb, kMargin, 168, "...", palColor(Pal::HOT));
        drawProgressBar(fb, kMargin, 180, kActiveW - 2 * kMargin, 10, holdFrac,
                        palColor(Pal::HOT));
    }

    // A re-checks the card (it lives here rather than on a list row because the SD
    // line above IS its result — insert a card, press A, watch the line change) and
    // C backs out. B is deliberately unnamed: it arms the hidden reveal above, and
    // naming it in the band would be the same as writing it on the screen.
    drawHintBand(fb, "A RECHECKS SD  C BACK");
}

void drawHackerTag(Framebuffer& fb, const char* tag, int caret) {
    drawHeaderBand(fb, "HACKERTAG");

    // Arcade entry grid: one cell per character, caret highlighted, a trailing ⏎
    // confirm cell. A/B/C carry editor meanings, so a hint band is shown.
    const int n = kHackerTagMax;
    const int cellW = 16, cellH = 22, gap = 1;
    const int gridW = (n + 1) * cellW + n * gap;      // +1 for the confirm cell
    const int x0 = (kActiveW - gridW) / 2;
    const int y = 70;
    for (int i = 0; i < n; ++i) {
        const int cx = x0 + i * (cellW + gap);
        const bool focus = (i == caret);
        if (focus) fb.fillRect(cx, y, cellW, cellH, palColor(Pal::ACCENT));
        else fb.fillRect(cx, y + cellH - 2, cellW, 2, palColor(Pal::TRACK));
        char ch[2] = {tag[i], '\0'};
        drawText(fb, cx + (cellW - textWidth(ch)) / 2, y + (cellH - kFontH) / 2, ch,
                 focus ? palColor(Pal::PAPER) : palColor(Pal::INK));
    }
    // The confirm cell (caret == kHackerTagMax).
    const int cx = x0 + n * (cellW + gap);
    const bool okFocus = (caret >= n);
    if (okFocus) fb.fillRect(cx, y, cellW, cellH, palColor(Pal::ACCENT));
    drawText(fb, cx + (cellW - textWidth("OK")) / 2, y + (cellH - kFontH) / 2, "OK",
             okFocus ? palColor(Pal::PAPER) : palColor(Pal::INK_DIM));

    // Live preview of the tag.
    drawText(fb, kMargin, 110, tag, palColor(Pal::INK));
    // B's meaning changes at the confirm cell (advance vs. save), so the band
    // reads that back rather than a caveat printed above it.
    drawHintBand(fb, caret >= n ? "A CYCLE  B SAVE  C DELETE"
                               : "A CYCLE  B NEXT  C DELETE");
}

void drawUiModeToggle(Framebuffer& fb, int pick, UiMode current) {
    drawHeaderBand(fb, "UI MODE");
    const UiMode modes[3] = {UiMode::IconsLabel, UiMode::IconsOnly,
                             UiMode::TextOnly};
    for (int i = 0; i < 3; ++i) {
        const int y = 40 + i * 24;
        if (i == pick) {
            fb.fillRect(4, y - 2, kActiveW - 8, 20, palColor(Pal::TRACK));
            drawRowCursor(fb, 8, y + 4, palColor(Pal::ACCENT));
        }
        const bool isCurrent = modes[i] == current;
        drawText(fb, 24, y + 4, uiModeName(modes[i]),
                 isCurrent ? palColor(Pal::ACCENT) : palColor(Pal::INK));
        if (isCurrent)
            drawText(fb, kActiveW - kMargin - textWidth("ACTIVE"), y + 4,
                     "ACTIVE", palColor(Pal::ACCENT));
    }
    drawHintBand(fb, "A CYCLE  B APPLY  C BACK");
}

void drawBrightness(Framebuffer& fb, int pick, int current) {
    drawHeaderBand(fb, "BRIGHTNESS");
    if (pick < 0) pick = 0;
    if (pick >= kBrightnessLevels) pick = kBrightnessLevels - 1;

    // A row of level bars whose FILL HEIGHT encodes the level — grayscale-safe (the
    // focused bar + the percent read carry meaning without colour). The focused level
    // is the tall accent bar; levels at/below it are filled, above it are empty tracks.
    const int n = kBrightnessLevels;
    const int barW = 20, gap = 6, maxH = 90;
    const int gridW = n * barW + (n - 1) * gap;
    const int x0 = (kActiveW - gridW) / 2;
    const int baseY = 130;
    for (int i = 0; i < n; ++i) {
        const int bx = x0 + i * (barW + gap);
        const int h = maxH * (i + 1) / n;
        const int by = baseY - h;
        // Empty track behind every bar (so the step ladder is visible even unfilled).
        fb.fillRect(bx, baseY - maxH, barW, maxH, palColor(Pal::TRACK));
        if (i <= pick)
            fb.fillRect(bx, by, barW, h,
                        i == pick ? palColor(Pal::ACCENT) : palColor(Pal::INK_DIM));
        // Mark the currently-APPLIED level with a baseline pip (so pick vs applied
        // both read before B is pressed).
        if (i == current)
            fb.fillRect(bx, baseY + 3, barW, 3, palColor(Pal::INK));
    }

    char pct[12];
    std::snprintf(pct, sizeof(pct), "%d%%", brightnessPercent(pick));
    drawText(fb, (kActiveW - textWidth(pct)) / 2, 30, pct, palColor(Pal::INK));
    drawHintBand(fb, "A LEVEL  B APPLY  C BACK");
}

void drawTitles(Framebuffer& fb, int focusSector, uint32_t unlockedMask,
                int equippedSector, int beat) {
    drawHeaderBand(fb, "TITLE");
    drawText(fb, kMargin, 30, "EARNED BY CLEARING ZONES.", palColor(Pal::INK_DIM));

    // Row 0 is NONE (sector -1); rows 1..kExplSectors are each sector's Title.
    // A locked Title is greyed + tagged LOCKED (and is not a valid focus/equip);
    // the equipped one is tagged ACTIVE. The row cursor marks the current focus.
    for (int r = 0; r <= kExplSectors; ++r) {
        const int sector = r - 1;                    // -1 = NONE
        const int y = 52 + r * 22;
        const bool locked = sector >= 0 && (unlockedMask & (1u << sector)) == 0;
        const bool focus = sector == focusSector;
        const bool equipped = sector == equippedSector;
        if (focus) {
            fb.fillRect(4, y - 2, kActiveW - 8, 20, palColor(Pal::TRACK));
            drawRowCursor(fb, 8, y + 4, palColor(Pal::ACCENT));
        }
        const char* name = sector < 0 ? "NONE" : sectorTitle(sector);
        const Rgb565 c = locked ? palColor(Pal::INK_DIM)
                       : equipped ? palColor(Pal::ACCENT)
                                  : palColor(Pal::INK);
        const char* tag = locked ? "LOCKED" : equipped ? "ACTIVE" : "";
        drawLabelValue(fb, 24, y + 4, name, c, tag,
                       locked ? palColor(Pal::INK_DIM) : palColor(Pal::ACCENT),
                       beat, focusSector == sector);
    }
    drawHintBand(fb, "A NEXT  B EQUIP  C BACK");
}

void drawAuditMode(Framebuffer& fb, int pick, int current) {
    drawHeaderBand(fb, "AUDIT");

    // The escalating ladder is the whole point of the screen — each step spells out
    // what it does + its battery cost, so raising it is an informed, affirmative
    // choice. PASSIVE only (no deauth/injection); the top step writes to SD.
    drawText(fb, kMargin, 30, "MORE = MORE BATTERY.", palColor(Pal::INK_DIM));
    drawText(fb, kMargin, 42, "PASSIVE / AUTHORIZED USE.", palColor(Pal::WARN));

    // label + one-line effect/cost hint, ordered by escalation.
    static const char* kOpts[3] = {"OFF", "SCAN", "SCAN + CAPTURE"};
    static const char* kHints[3] = {"RADIO IDLE",
                                    "HEAR NETS, +RANK",
                                    "+ HANDSHAKE -> PCAP"};
    for (int i = 0; i < 3; ++i) {
        const int y = 74 + i * 28;
        if (i == pick) {
            fb.fillRect(4, y - 2, kActiveW - 8, 26, palColor(Pal::TRACK));
            drawRowCursor(fb, 8, y + 4, palColor(Pal::ACCENT));
        }
        const bool isCurrent = (i == current);
        drawText(fb, 24, y, kOpts[i],
                 isCurrent ? palColor(Pal::ACCENT) : palColor(Pal::INK));
        drawText(fb, 24, y + 12, kHints[i], palColor(Pal::INK_DIM));
        if (isCurrent)
            drawText(fb, kActiveW - kMargin - textWidth("ACTIVE"), y,
                     "ACTIVE", palColor(Pal::ACCENT));
    }
    drawHintBand(fb, "A CYCLE  B APPLY  C BACK");
}

void drawApToggle(Framebuffer& fb, int pick, bool current) {
    drawHeaderBand(fb, "PEDIA AP");

    // The 'Pedia local Access Point: hosts a small Wi-Fi network + web page (no
    // internet). Naming the SSID + URL here is the whole affordance — connect,
    // then browse. The SSID is a device value (config.h), so kept generic here.
    drawText(fb, kMargin, 30, "HOSTS A LOCAL WI-FI + PAGE.", palColor(Pal::INK_DIM));
    drawText(fb, kMargin, 42, "JOIN 'MALWARIUM' NETWORK.", palColor(Pal::INK_DIM));
    drawText(fb, kMargin, 54, "ON SHOWS A QR TO CONNECT.", palColor(Pal::ACCENT));

    static const char* kOpts[2] = {"OFF", "ON"};
    for (int i = 0; i < 2; ++i) {
        const int y = 80 + i * 24;
        if (i == pick) {
            fb.fillRect(4, y - 2, kActiveW - 8, 20, palColor(Pal::TRACK));
            drawRowCursor(fb, 8, y + 4, palColor(Pal::ACCENT));
        }
        const bool isCurrent = (i == 1) == current;
        drawText(fb, 24, y + 4, kOpts[i],
                 isCurrent ? palColor(Pal::ACCENT) : palColor(Pal::INK));
        if (isCurrent)
            drawText(fb, kActiveW - kMargin - textWidth("ACTIVE"), y + 4,
                     "ACTIVE", palColor(Pal::ACCENT));
    }
    drawHintBand(fb, "A TOGGLE  B APPLY  C BACK");
}

void drawLinkToggle(Framebuffer& fb, int pick, bool current, bool ambientStarved) {
    drawHeaderBand(fb, "PASSIVE LINK");

    // The consent this row asks for is the whole affordance, so it is spelled out
    // rather than implied: it TRANSMITS. It is deliberately not folded into the
    // AUDIT ladder, whose escalation is about how hard the radio LISTENS — this one
    // is about what the device says out loud, which is a different question.
    drawText(fb, kMargin, 30, "MEET NEARBY MALWARIUMS.", palColor(Pal::INK_DIM));
    drawText(fb, kMargin, 42, "BROADCASTS YOUR TAG,", palColor(Pal::WARN));
    drawText(fb, kMargin, 54, "PET AND CREW TO ANYONE.", palColor(Pal::WARN));

    static const char* kOpts[2] = {"OFF", "ON"};
    for (int i = 0; i < 2; ++i) {
        const int y = 80 + i * 24;
        if (i == pick) {
            fb.fillRect(4, y - 2, kActiveW - 8, 20, palColor(Pal::TRACK));
            drawRowCursor(fb, 8, y + 4, palColor(Pal::ACCENT));
        }
        const bool isCurrent = (i == 1) == current;
        drawText(fb, 24, y + 4, kOpts[i],
                 isCurrent ? palColor(Pal::ACCENT) : palColor(Pal::INK));
        if (isCurrent)
            drawText(fb, kActiveW - kMargin - textWidth("ACTIVE"), y + 4,
                     "ACTIVE", palColor(Pal::ACCENT));
    }
    // Naming the cost keeps the choice honest: an unassociated listener can't use
    // Wi-Fi modem sleep, so this is a real draw whenever the radio window is open.
    drawText(fb, kMargin, 134, "ON COSTS BATTERY.", palColor(Pal::INK_DIM));
    // Ambient discovery borrows the audit scan's radio window, so with the scan off
    // this toggle only does anything while PEERS is open. Say so rather than let the
    // setting look dead — and rather than arming the scan on the player's behalf,
    // which would take a listening consent they didn't give.
    if (ambientStarved) {
        drawText(fb, kMargin, 146, "AUDIT SCAN IS OFF:", palColor(Pal::WARN));
        drawText(fb, kMargin, 158, "ONLY MEETS ON PEERS SCREEN", palColor(Pal::WARN));
    }
    drawHintBand(fb, "A TOGGLE  B APPLY  C BACK");
}

void drawFactoryReset(Framebuffer& fb, int scope, float holdFrac) {
    drawHeaderBand(fb, "FACTORY RESET");
    const char* scopeName = scope == 0 ? "RESET PET" : "WIPE EVERYTHING";
    char line[28];
    std::snprintf(line, sizeof(line), "SCOPE: %s", scopeName);
    drawText(fb, kMargin, 40, line, palColor(Pal::INK));
    drawText(fb, kMargin, 56,
             scope == 0 ? "KEEPS PEDIA PROGRESS." : "WIPES PET + PEDIA.",
             palColor(Pal::INK_DIM));

    // Hold-B to commit; the fill bar IS the affordance (a screen that repurposes
    // B surfaces its own hint). Releasing early aborts.
    drawText(fb, kMargin, 150, "HOLD B TO WIPE", palColor(Pal::HOT));
    drawProgressBar(fb, kMargin, 164, kActiveW - 2 * kMargin, 12, holdFrac,
                    palColor(Pal::HOT));
    // B is deliberately absent from the band: it is a HOLD, not a tap, and the
    // HOT line + fill bar above are its affordance. Naming it here in the same
    // voice as every tap in the app would invite one.
    drawHintBand(fb, "A SCOPE  C BACK");
}

} // namespace mal
