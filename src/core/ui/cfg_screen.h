// cfg_screen.h — CFG submenu: a settings list (L2) whose rows open
// read-only viewers / toggles / group screens (L3). Flattens the spec's "System
// Info" + "User Registry" into one list. The hidden Factory Reset is NOT a row
// — it is reached only by a hold-B gesture off System Info.
//
// The list is SIX rows, which is exactly the viewport (kVisibleRows), so it never
// scrolls. Settings that belong to one another are reached through a GROUP screen
// rather than as list peers: DISPLAY holds UI MODE + BRIGHTNESS, RADIO holds the
// three radio toggles (AUDIT / LINK / PEDIA AP) plus the one line that says which
// of them the arbiter has actually granted the radio. A group is not a fourth nav
// level — like the UPDATES screen's check/confirm/progress faces, it is another
// CfgScreen inside the same Nav::Detail, and its children return to it.
//
// Reaching the 'net is NOT a row here. It has no standing setting to hold: UPDATES
// raises the association as part of a job and drops it when the job ends, which is
// why that screen reports the connection instead of one of its own.
#pragma once

#include <cstddef>
#include <cstdint>

#include "core/app/net_status.h"
#include "core/app/power_status.h"
#include "core/app/radio_status.h"
#include "core/app/sd_status.h"
#include "core/app/update_status.h"
#include "core/ui/carousel.h"  // UiMode

namespace mal {

class Framebuffer;
struct SpriteData;

// L3 destinations a CFG row can open. Display and Radio are the two GROUP screens
// (their own children follow them here). ResetHatch is the DEV-only quick reset (an
// immediate action, no L3 screen). FactoryReset is reached only via the hidden
// hold-B gesture, never a list row.
enum class CfgScreen { SysInfo, HackerTag, Titles,
                       Display, UiMode, Brightness,
                       Radio, Audit, Link, PediaAp, PediaQr,
                       Update, UpdateQr, ResetHatch, FactoryReset };

// One CFG settings row: glyph + label + its L3 target. Shared by the top-level
// list and both group screens, so a row reads the same wherever it lives.
struct CfgRow {
    const char* label;
    const SpriteData* icon;
    CfgScreen target;
};

// The CFG row table (static). Includes the DEV "Reset to Hatch" row only when
// DEV_CFG_RESET_ROW is defined (dev_config.h) — release builds omit it. Returns
// the row count and points `out` at the static array.
int cfgRows(const CfgRow*& out);

// The rows of a group screen — Display (UI MODE, BRIGHTNESS) or Radio (PEDIA AP,
// LINK, AUDIT). Any other screen has none, so a caller can ask "is this a group?"
// by testing the count. Returns the count, points `out` at the static array.
int cfgGroupRows(CfgScreen group, const CfgRow*& out);

// The group screen a child returns to on C (or after applying), or the child's
// own value when it isn't inside one — so a radio toggle backs out to RADIO and
// a top-level row backs out to the list. PediaQr's parent is Radio: it is entered
// from PEDIA AP, which has already applied by then.
CfgScreen cfgParentGroup(CfgScreen s);

// Human-readable UI Mode label (shared by the list preview + the toggle screen).
const char* uiModeName(UiMode m);

// Cursor-following scroll offset: the top row index to draw so a list of
// `n` rows showing `visible` at once keeps `cursor` on-screen. Clamped to
// [0, n-visible]; returns 0 when the whole list fits. Pure — unit-tested.
int cfgScrollTop(int cursor, int n, int visible);

// L2 settings list. `hackerTag` drives the HACKERTAG row's value preview,
// `equippedTitle` the TITLE row's (the equipped Title name, or "NONE"), and
// `radioOwner` the RADIO row's — the row previews what the radio is DOING, not
// how many toggles are on, since only one of them can be live at a time.
void drawCfgList(Framebuffer& fb, int cursor, const char* hackerTag,
                 const char* equippedTitle, RadioOwner radioOwner);

// L3 DISPLAY group: UI MODE + BRIGHTNESS, each previewing its live value. B opens
// the focused control, C backs to the list.
void drawCfgDisplay(Framebuffer& fb, int cursor, UiMode uiMode, int brightness);

// L3 RADIO group: the three radio toggles, in the arbiter's priority order with the
// highest first, headed by the one thing no individual toggle screen can report —
// which of them the arbiter has actually put on the air (`owner`). `updateLive`
// draws a fourth, non-selectable row ABOVE them for as long as an update job holds
// the radio: it outranks all three, so the screen would otherwise show a set of
// waiting rows with no winner named.
//
// Switching a row on is a directive, not a preference, so the screen owes the
// operator the difference between what they asked for and what the radio is doing.
// It draws that as one group: a row that is ON is INDENTED under a radio mark
// (filled = on the air, hollow = switched on, waiting behind a row above it), and a
// row that is OFF sits flush left with no mark, outside the group. The value column
// stays what the operator set; the mark is what became of it. Three channels —
// words in the header line, the mark's fill, the indent — none of them colour, so a
// grayscale screenshot still says who has the radio.
//
// `auditLevel` is 0 OFF / 1 SCAN / 2 SCAN+CAP, and its row reads as on-air under
// EITHER arbiter owner the ladder resolves to (Scan or Capture).
void drawCfgRadio(Framebuffer& fb, int cursor, RadioOwner owner, int auditLevel,
                  bool linkOn, bool apOn, bool updateLive);

// L3 System Info viewer, and the device's one status readout. RADIO names the
// arbiter's live owner (`owner`); SD is live from `sd` (device tier feeds a mount
// reading — size in MB when present, else "ABSENT") and A re-checks it in place,
// which is why this screen owns that action rather than a list row the player
// would have to find; the BATT line is live when `power.present` (device tier feeds
// it), else "-". `rankTitle`/`networksSeen` surface Hacker Rank here too (the
// spec's "User Registry," beside the HackerTag). NETS is the lifetime
// unique-network count; SHAKES (the lifetime unique-handshake count,
// `handshakesSeen`) is drawn beneath it ONLY when `captureEnabled` — the pcap
// capture toggle is on. `holdFrac` (0..1) draws the hidden Factory-Reset reveal
// bar while B is held (0 = not holding, no bar).
void drawSysInfo(Framebuffer& fb, uint32_t uptimeMs, UiMode uiMode,
                 const char* fwVersion, const char* rankTitle, int networksSeen,
                 const PowerStatus& power, const SdStatus& sd, bool captureEnabled,
                 int handshakesSeen, const char* equippedTitle, RadioOwner owner,
                 float holdFrac);

// L3 HackerTag arcade editor. `tag` is the working buffer (fixed
// kHackerTagMax cells); `caret` is the focused cell (0..kHackerTagMax — the last
// position is the ⏎ confirm). Renders the cells, the caret highlight, the confirm
// cell, and the editor hint line (A cycle · B next · C delete).
void drawHackerTag(Framebuffer& fb, const char* tag, int caret);

// L3 UI Mode toggle. `pick` is the focused option (cycles A); B applies.
void drawUiModeToggle(Framebuffer& fb, int pick, UiMode current);

// L3 Brightness picker. A discrete backlight-level chooser: `pick` is the
// focused level (0..kBrightnessLevels-1, cycles A), `current` the applied level. Draws
// a level bar + percent so it reads in grayscale (fill height + number, not colour).
// B applies (the device tier then drives the backlight PWM).
void drawBrightness(Framebuffer& fb, int pick, int current);

// L3 Titles picker: equip a zone-completion Title on the HackerTag.
// Rows are NONE + one per sector Title; locked Titles show "LOCKED" and are
// unselectable, the equipped one shows "ACTIVE". `focusSector` is the highlighted
// choice (-1 = the NONE row, else a sector index); `unlockedMask` bit i = sector i
// unlocked; `equippedSector` is the currently-applied Title (-1 = none). Dual-coded
// (LOCKED/ACTIVE text + the row cursor) — grayscale-safe. B applies, C cancels.
void drawTitles(Framebuffer& fb, int focusSector, uint32_t unlockedMask,
                int equippedSector);

// L3 Audit-mode escalating picker. One ordered control:
// 0 OFF (radio idle) / 1 SCAN (passive
// discovery, feeds NETS) / 2 SCAN+CAPTURE (adds passive WPA-handshake -> .pcap).
// Higher = more battery; capture structurally requires scan, so there is no
// scan-off/capture-on option. `current` is the live level, `pick` the focused one
// (cycles A). PASSIVE only — no deauth/injection; carries the authorized-use
// disclaimer since the top level writes captured frames to SD.
void drawAuditMode(Framebuffer& fb, int pick, int current);

// L3 'Pedia local-AP toggle. OFF/ON like the audit toggles;
// names the SSID + landing URL so the player knows what to connect to.
void drawApToggle(Framebuffer& fb, int pick, bool current);

// L3 pet-to-pet LINK toggle. OFF/ON like the AP toggle above, but its copy leads
// with what turning it ON transmits — announcing identity is a different consent
// from the AUDIT ladder's listening, so the screen has to say so rather than let
// the player infer it from the row name.
void drawLinkToggle(Framebuffer& fb, int pick, bool current, bool ambientStarved);

// L3 update check, and the ONLY screen that ever puts this device on a network.
// CHECK NOW, plus an install row per artifact the last check found to be newer.
//
// There is no separate internet consent to arm first: pressing CHECK NOW is the
// directive to go online, and the job hands the radio back when it reaches a
// terminal outcome (Game::netConnectWanted). So the association's own progress is
// reported HERE, as the first step of a job — `net` drives the CONNECTING → FETCHING
// split and a failed join names the setup portal that fixes it.
//
// `ready` says whether a check is even possible — a stored network and a manifest
// source, both of which are SETUP rather than permission; when it isn't, the screen
// names the missing piece instead of offering a button that would no-op.
// `manifestUrl` is shown host-only beside the installed version — where this device
// looks, which a card-less one can read nowhere else.
void drawUpdateCheck(Framebuffer& fb, bool ready, bool provisioned,
                     bool sourceKnown, const UpdateStatus& status,
                     const NetStatus& net, const char* firmwareVersion,
                     const char* manifestUrl, int row);

// How many rows drawUpdateCheck offers, so the input handler and the renderer
// agree on what A cycles through: CHECK NOW, then one install row per artifact
// reported newer, then FLASH OVER USB. Row 0 is always the check and the last row
// is always the flasher, which is why the middle is the only part that varies.
int updateCheckRows(const UpdateStatus& status);

// What B on `row` actually does. Three different acts share one cursor — start a
// check, open an install confirm, open the flasher QR — so both the input handler
// and the renderer ask this instead of deriving it from the index twice and
// eventually disagreeing.
enum class UpdateRowKind : unsigned char { Check, Install, FlashQr };
UpdateRowKind updateCheckRowKind(const UpdateStatus& status, int row);

// Which artifact an INSTALL row selects; None for any other row. Paired with
// updateCheckRowKind above, so a row can never install the artifact next to the
// one it names.
UpdateTarget updateCheckRowTarget(const UpdateStatus& status, int row);

// The second yes. Names what is about to be replaced and what it is replaced with,
// and starts on NO — an install is never one press away from the row that offered
// it. `pick` is 0 = NO, 1 = YES.
void drawUpdateConfirm(Framebuffer& fb, UpdateTarget target, const char* fromVersion,
                       const char* toVersion, int pick);

// A running (or finished) install: which artifact, how far in, and what it cost if
// it failed. Progress is dual-coded — a bar AND the byte counts — so the screen
// still reports in grayscale. `net` names the join an install does for itself,
// which is the phase before any byte arrives.
void drawUpdateProgress(Framebuffer& fb, const InstallStatus& install,
                        const NetStatus& net);

// Where the USB flasher lives, derived from the manifest address this device
// already checks: the publish host serves both, so `.../manifest.json` becomes
// `.../flash/` and nobody has to configure the same host twice. A device pointed
// at a fork, or at a laptop, gets that host's flasher for free.
//
// False (and an empty `out`) when there is no source to derive from, or the
// result wouldn't fit — a device with nowhere to look has no page to offer, and
// the UPDATES screen already says so.
bool updateFlasherUrl(const char* manifestUrl, char* out, size_t cap);

// L3 USB-flasher QR, reached from the UPDATES screen's last row. Encodes the
// address updateFlasherUrl derives from `manifestUrl` and says the two things a
// scanner can't: that the page needs Chrome on a computer, and that A is held
// down while the cable goes in. C exits.
void drawUpdateQr(Framebuffer& fb, const char* manifestUrl);

// L3 'Pedia QR: real, scannable QR pages the player cycles with A, in step order —
// page 0 = join the local AP (WIFI: payload), then the home-network setup page
// while none is stored, then the 'Pedia URL. Reached by turning the AP on
// (PEDIA AP → ON). The AP address is fixed (192.168.4.1), so every code is static.
void drawPediaQr(Framebuffer& fb, int page, bool provisioned);

// How many QR pages the carousel offers, so the input handler and the renderer
// agree on what A wraps at. The setup step drops out once a network is stored.
int pediaQrPages(bool provisioned);

// L3 Factory Reset (hidden). `scope` 0 = Reset Pet, 1 = Wipe Everything
// (A cycles). `holdFrac` (0..1) is the hold-B-to-commit fill (releasing aborts).
void drawFactoryReset(Framebuffer& fb, int scope, float holdFrac);

} // namespace mal
