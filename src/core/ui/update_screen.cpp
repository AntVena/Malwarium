// update_screen.cpp — the software-update and QR flows of the CFG surface,
// declared alongside the settings screens in cfg_screen.h.
//
// It sits apart from cfg_screen.cpp because it is a different KIND of screen. The
// settings screens read and write state this device already holds; these ones run
// a job — join a network, fetch a manifest, weigh a verdict, install and verify —
// and every face here (check, confirm, progress, the two QR pages) is a step of
// that job or a way into it. The two halves share the layout grid (core/ui/layout.h)
// and nothing else.
//
// The QR pages ride along because both of them exist to get a phone or a computer
// to a URL this device cannot open, and they share blitQr.
#include "core/ui/cfg_screen.h"

#include <cstdio>
#include <cstring>

#include "core/render/canvas.h"
#include "core/render/font.h"
#include "core/render/framebuffer.h"
#include "core/render/palette.h"
#include "core/render/qrcodegen.h"  // real QR encode for the AP + flasher URLs
#include "core/ui/layout.h"
#include "core/ui/widgets.h"

namespace mal {

namespace {

// The host part of `url` ("https://box.local:8000/dist/manifest.json" ->
// "box.local:8000"), truncated with a trailing "…" if it still won't fit. An empty
// or scheme-less address reports "NOT SET", so the field never renders as a blank
// the reader has to interpret. `out` is always NUL-terminated.
void urlHost(const char* url, char* out, size_t cap) {
    const char* s = url ? url : "";
    const char* sep = std::strstr(s, "//");
    const char* host = sep ? sep + 2 : s;
    if (!*host) { std::snprintf(out, cap, "NOT SET"); return; }
    size_t n = 0;
    while (host[n] && host[n] != '/' && n < cap - 1) ++n;
    std::memcpy(out, host, n);
    out[n] = '\0';
    if (host[n] && host[n] != '/' && n >= 3) std::strcpy(out + n - 3, "...");
}

}  // namespace

// The install rows exist only for what a check actually found. No check, no rows
// — there is nothing to install and nothing to name. Split out because both the
// row count and the row→kind mapping need the same number and must never differ.
static int updateInstallRows(const UpdateStatus& status) {
    if (status.state != UpdateState::Available) return 0;
    return (status.firmwareNewer ? 1 : 0) + (status.webNewer ? 1 : 0);
}

int updateCheckRows(const UpdateStatus& status) {
    return 1 + updateInstallRows(status) + 1;   // check · installs · flasher
}

UpdateRowKind updateCheckRowKind(const UpdateStatus& status, int row) {
    if (row <= 0) return UpdateRowKind::Check;
    if (row <= updateInstallRows(status)) return UpdateRowKind::Install;
    return UpdateRowKind::FlashQr;              // the last row, always
}

UpdateTarget updateCheckRowTarget(const UpdateStatus& status, int row) {
    if (updateCheckRowKind(status, row) != UpdateRowKind::Install)
        return UpdateTarget::None;
    int n = 0;
    if (status.firmwareNewer && ++n == row) return UpdateTarget::Firmware;
    if (status.webNewer && ++n == row) return UpdateTarget::Web;
    return UpdateTarget::None;
}

bool updateFlasherUrl(const char* manifestUrl, char* out, size_t cap) {
    if (!out || cap == 0) return false;
    out[0] = '\0';
    const char* s = manifestUrl ? manifestUrl : "";
    const size_t len = std::strlen(s);
    if (len == 0) return false;

    // Replace the manifest's own filename with the flasher's directory. The last
    // '/' AFTER the scheme separator is what divides the two — searching from the
    // host start rather than from the beginning is what stops "https://host" (no
    // path at all) from being cut at the "//".
    const char* sep = std::strstr(s, "//");
    const size_t hostStart = sep ? static_cast<size_t>(sep - s) + 2 : 0;
    size_t keep = 0;                       // bytes of `s` to copy, slash included
    for (size_t i = len; i > hostStart; --i)
        if (s[i - 1] == '/') { keep = i; break; }

    static const char kLeaf[] = "flash/";
    // A bare host keeps all of itself and earns the separating slash it lacks.
    const size_t need = (keep ? keep : len) + (keep ? 0 : 1) + sizeof(kLeaf);
    if (need > cap) return false;

    if (keep) {
        std::memcpy(out, s, keep);
        std::strcpy(out + keep, kLeaf);
    } else {
        std::memcpy(out, s, len);
        out[len] = '/';
        std::strcpy(out + len + 1, kLeaf);
    }
    return true;
}

void drawUpdateCheck(Framebuffer& fb, bool ready, bool provisioned,
                     bool sourceKnown, const UpdateStatus& status,
                     const NetStatus& net, const char* firmwareVersion,
                     const char* manifestUrl, int row) {
    drawHeaderBand(fb, "UPDATES");

    // What is on this device right now, so the verdict below has something to be
    // newer THAN. The web bundle's own version is only known once a check has read
    // the card's marker, so it appears with the result rather than here.
    drawText(fb, kMargin, 28, "INSTALLED", palColor(Pal::INK_DIM));


    // WHERE it looks, beside what it would replace. The full manifest URL is set
    // from the 'Pedia and is far too long for this panel, so this shows the host —
    // the part that answers "is this device still pointed at my publish server?".
    // A card-less device has no 'Pedia to read it from, which is the whole reason
    // it has to be legible here.
    drawText(fb, kActiveW - kMargin - textWidth("FROM"), 28, "FROM",
             palColor(Pal::INK_DIM));
    char host[24];
    urlHost(manifestUrl, host, sizeof(host));
    drawLabelValue(fb, kMargin, 40, firmwareVersion, palColor(Pal::INK), host,
                   sourceKnown ? palColor(Pal::INK) : palColor(Pal::INK_DIM), 0, false);

    // Row 0 is always the check, the last is always the flasher, and between them
    // sit what the check found. An install row names the artifact AND the version
    // it would bring, so the confirm that follows is never the first time the
    // operator sees what they are agreeing to.
    //
    // The flasher row is the one that does not depend on a network — it is the
    // answer to "the check can't run" and to "an update needs the cable", so it is
    // live whenever this device knows a host at all. That makes `sourceKnown`
    // rather than `ready` what lights it.
    const bool busy = status.state == UpdateState::Checking;
    const int rows = updateCheckRows(status);
    for (int i = 0; i < rows; ++i) {
        const int y = 56 + i * 18;
        const UpdateRowKind kind = updateCheckRowKind(status, i);
        const bool live = (kind == UpdateRowKind::FlashQr) ? sourceKnown : ready;
        if (i == row) {
            fb.fillRect(4, y - 2, kActiveW - 8, 18, palColor(Pal::TRACK));
            drawRowCursor(fb, 8, y + 3, palColor(live ? Pal::ACCENT : Pal::INK_DIM));
        }
        if (kind == UpdateRowKind::Check) {
            drawText(fb, 24, y + 3, busy ? "CHECKING..." : "CHECK NOW",
                     palColor(ready && !busy ? Pal::INK : Pal::INK_DIM));
        } else if (kind == UpdateRowKind::FlashQr) {
            drawText(fb, 24, y + 3, "FLASH OVER USB",
                     palColor(live ? Pal::INK : Pal::INK_DIM));
            drawText(fb, kActiveW - kMargin - textWidth("SHOW QR"), y + 3, "SHOW QR",
                     palColor(Pal::INK_DIM));
        } else {
            const UpdateTarget t = updateCheckRowTarget(status, i);
            const char* label = (t == UpdateTarget::Firmware) ? "FIRMWARE" : "PEDIA SITE";
            const char* ver = (t == UpdateTarget::Firmware) ? status.firmwareVersion
                                                            : status.webVersion;
            drawText(fb, 24, y + 3, label, palColor(Pal::INK));
            drawText(fb, kActiveW - kMargin - textWidth(ver), y + 3, ver,
                     palColor(Pal::ACCENT));
        }
    }

    // The verdict sits under the rows, not at a fixed line: only the Available
    // state grows the list past three, and it is also the only state whose verdict
    // is a single line, so the block below never runs into the standing promise.
    const int verdictY = std::max(118, 56 + rows * 18 + 8);

    // Why a check can't run, most-blocking first — each names the screen that
    // fixes it, since none of them is fixable from here. Both are SETUP: there is
    // no "turn the internet on" step to forget, because pressing CHECK NOW is what
    // grants the connection and finishing the job is what gives it back.
    if (!ready) {
        if (!provisioned) {
            drawText(fb, kMargin, verdictY, "NO NETWORK SAVED:", palColor(Pal::WARN));
            drawText(fb, kMargin, verdictY + 12, "SET ONE UP FROM PEDIA AP", palColor(Pal::WARN));
        } else if (!sourceKnown) {
            drawText(fb, kMargin, verdictY, "NO UPDATE SOURCE SET:", palColor(Pal::WARN));
            drawText(fb, kMargin, verdictY + 12, "THIS BUILD HAS NOWHERE", palColor(Pal::WARN));
            drawText(fb, kMargin, verdictY + 24, "TO LOOK.", palColor(Pal::WARN));
        }
    } else {
        // The verdict. Every state names itself in words, so the screen reads the
        // same in grayscale as in colour.
        switch (status.state) {
            case UpdateState::Idle:
                drawText(fb, kMargin, verdictY, "NOT CHECKED YET.", palColor(Pal::INK_DIM));
                break;
            // A check is two steps and they fail for different reasons, so the screen
            // separates them: joining the network is reported here because a job on
            // this screen is the only thing that ever asks for one.
            case UpdateState::Checking:
                if (net.state == NetState::Connecting) {
                    drawText(fb, kMargin, verdictY, "JOINING YOUR NETWORK...",
                             palColor(Pal::WARN));
                } else {
                    drawText(fb, kMargin, verdictY, "FETCHING THE LIST...",
                             palColor(Pal::WARN));
                    if (net.state == NetState::Online && net.ip[0])
                        drawText(fb, kMargin, verdictY + 12, net.ip, palColor(Pal::INK_DIM));
                }
                break;
            case UpdateState::UpToDate:
                drawText(fb, kMargin, verdictY, "UP TO DATE.", palColor(Pal::ACCENT));
                break;
            case UpdateState::Available:
                // The rows above already say what and which version; this says the
                // one thing they can't — that pressing B does not commit anything.
                drawText(fb, kMargin, verdictY, "B ON A ROW TO INSTALL.", palColor(Pal::INK_DIM));
                break;
            case UpdateState::Failed: {
                drawText(fb, kMargin, verdictY, "CHECK FAILED:", palColor(Pal::WARN));
                const char* why = "UNKNOWN.";
                // A join that never came up is the one failure with somewhere to go
                // fix it, so it says where — the stored credentials are only ever
                // written from the setup portal.
                const char* fix = nullptr;
                switch (status.fail) {
                    case UpdateFail::NoSource:   why = "NOWHERE TO LOOK."; break;
                    case UpdateFail::NoNetwork:
                        why = "COULD NOT JOIN.";
                        fix = "CHECK SETUP FROM PEDIA AP";
                        break;
                    case UpdateFail::NoManifest: why = "NO LIST PUBLISHED."; break;
                    case UpdateFail::Malformed:  why = "THE LIST MADE NO SENSE."; break;
                    case UpdateFail::None:       break;
                }
                drawText(fb, kMargin, verdictY + 12, why, palColor(Pal::WARN));
                if (fix) drawText(fb, kMargin, verdictY + 24, fix, palColor(Pal::WARN));
                break;
            }
        }
    }

    // The standing promise, on the screen where it matters: looking is not
    // installing, and the operator gets asked again before anything is written.
    drawText(fb, kMargin, 164, "NOTHING INSTALLS WITHOUT", palColor(Pal::INK_DIM));
    drawText(fb, kMargin, 176, "ASKING YOU FIRST.", palColor(Pal::INK_DIM));
}

void drawUpdateConfirm(Framebuffer& fb, UpdateTarget target, const char* fromVersion,
                       const char* toVersion, int pick) {
    drawHeaderBand(fb, "INSTALL?");

    const bool fw = target == UpdateTarget::Firmware;
    drawText(fb, kMargin, 30, fw ? "FIRMWARE" : "PEDIA SITE", palColor(Pal::INK));

    // From → to on separate lines: the operator is agreeing to a REPLACEMENT, and
    // seeing only the new version hides half of what that means.
    drawText(fb, kMargin, 48, "NOW", palColor(Pal::INK_DIM));
    drawText(fb, 64, 48, fromVersion, palColor(Pal::INK));
    drawText(fb, kMargin, 60, "NEW", palColor(Pal::INK_DIM));
    drawText(fb, 64, 60, toVersion, palColor(Pal::ACCENT));

    // What it actually costs, per target, in plain words.
    if (fw) {
        drawText(fb, kMargin, 80, "DOWNLOADS, CHECKS ITSELF,", palColor(Pal::INK_DIM));
        drawText(fb, kMargin, 92, "THEN RESTARTS.", palColor(Pal::INK_DIM));
        drawText(fb, kMargin, 104, "YOUR PET IS NOT TOUCHED.", palColor(Pal::INK_DIM));
    } else {
        drawText(fb, kMargin, 80, "REPLACES THE SITE ON THE", palColor(Pal::INK_DIM));
        drawText(fb, kMargin, 92, "SD CARD. NEEDS A CARD.", palColor(Pal::INK_DIM));
        drawText(fb, kMargin, 104, "YOUR PET IS NOT TOUCHED.", palColor(Pal::INK_DIM));
    }

    // NO first, and NO is where the cursor starts — an install is never one press
    // away from the row that offered it.
    static const char* kOpts[2] = {"NO", "YES, INSTALL"};
    for (int i = 0; i < 2; ++i) {
        const int y = 126 + i * 20;
        if (i == pick) {
            fb.fillRect(4, y - 2, kActiveW - 8, 18, palColor(Pal::TRACK));
            drawRowCursor(fb, 8, y + 3, palColor(Pal::ACCENT));
        }
        drawText(fb, 24, y + 3, kOpts[i],
                 palColor(i == 1 ? Pal::WARN : Pal::INK));
    }
    drawHintBand(fb, "A CHOOSE  B CONFIRM  C CANCEL");
}

void drawUpdateProgress(Framebuffer& fb, const InstallStatus& install,
                        const NetStatus& net) {
    drawHeaderBand(fb, "INSTALLING");

    const bool fw = install.target == UpdateTarget::Firmware;
    drawText(fb, kMargin, 30, fw ? "FIRMWARE" : "PEDIA SITE", palColor(Pal::INK));

    // The phase, named. Which one it is IS the safety story — nothing is written
    // where it will be used until the digest has already matched. An install raises
    // its own association, so joining is the phase before any byte arrives — and it
    // is named rather than shown as a download stuck at zero.
    const char* phase = "";
    switch (install.state) {
        case InstallState::Idle:        phase = "WAITING..."; break;
        case InstallState::Downloading:
            phase = net.state == NetState::Connecting ? "JOINING YOUR NETWORK..."
                                                      : "DOWNLOADING...";
            break;
        case InstallState::Verifying:   phase = "CHECKING IT ARRIVED WHOLE"; break;
        case InstallState::Installing:  phase = "WRITING..."; break;
        case InstallState::Done:        phase = "DONE."; break;
        case InstallState::Failed:      phase = "STOPPED."; break;
    }
    const bool bad = install.state == InstallState::Failed;
    drawText(fb, kMargin, 50, phase,
             palColor(bad ? Pal::WARN
                          : install.state == InstallState::Done ? Pal::ACCENT : Pal::INK));

    // Dual-coded progress: a filled bar AND the counts, so the bar is never the
    // only thing carrying how far along this is.
    if (install.total > 0) {
        const int w = kActiveW - 2 * kMargin;
        const uint32_t recv = install.received < install.total ? install.received
                                                               : install.total;
        const int fill = static_cast<int>(
            (static_cast<uint64_t>(recv) * static_cast<uint64_t>(w)) / install.total);
        fb.fillRect(kMargin, 72, w, 10, palColor(Pal::TRACK));
        if (fill > 0) fb.fillRect(kMargin, 72, fill, 10, palColor(Pal::ACCENT));

        char counts[32];
        std::snprintf(counts, sizeof(counts), "%luK OF %luK",
                      static_cast<unsigned long>(recv / 1024),
                      static_cast<unsigned long>(install.total / 1024));
        drawText(fb, kMargin, 88, counts, palColor(Pal::INK_DIM));
    }

    if (install.state == InstallState::Failed) {
        const char* why = "UNKNOWN.";
        switch (install.fail) {
            case InstallFail::NoNetwork:   why = "LOST THE CONNECTION."; break;
            case InstallFail::Truncated:   why = "THE DOWNLOAD CUT SHORT."; break;
            case InstallFail::Corrupt:     why = "IT ARRIVED DAMAGED."; break;
            case InstallFail::NoRoom:      why = "NOT ENOUGH ROOM."; break;
            case InstallFail::WriteFailed: why = "COULD NOT WRITE IT."; break;
            case InstallFail::None:        break;
        }
        drawText(fb, kMargin, 112, why, palColor(Pal::WARN));
        // The reassurance that makes a failure boring: a half-download is never
        // installed, so the device is still running exactly what it was.
        drawText(fb, kMargin, 132, "NOTHING WAS CHANGED.", palColor(Pal::INK_DIM));
        drawText(fb, kMargin, 144, "SAFE TO TRY AGAIN.", palColor(Pal::INK_DIM));
    } else if (install.state == InstallState::Done) {
        if (install.restartNeeded)
            drawText(fb, kMargin, 112, "RESTARTING...", palColor(Pal::ACCENT));
        else
            drawText(fb, kMargin, 112, "THE SITE IS UP TO DATE.", palColor(Pal::ACCENT));
    } else {
        drawText(fb, kMargin, 112, "KEEP THE DEVICE POWERED.", palColor(Pal::WARN));
        drawText(fb, kMargin, 132, "YOU CAN LEAVE THIS SCREEN;", palColor(Pal::INK_DIM));
        drawText(fb, kMargin, 144, "IT KEEPS GOING.", palColor(Pal::INK_DIM));
    }
    drawHintBand(fb, "C BACK");
}

// Encode `text` and blit a real, scannable QR centred at the given top-y (Nayuki
// qrcodegen, MIT). Light field (INK ≈ white) incl. the quiet zone + dark modules
// (PAPER ≈ black) = a standard dark-on-light code (inverse of the dark UI) that
// real scanners read, and grayscale-safe by construction (max luminance contrast).
// Returns false only if encoding failed (not expected for these short strings).
static bool blitQr(Framebuffer& fb, const char* text, int oy) {
    // Stack buffers, sized for the longest payload any caller can hand over — a
    // stored update source is capped at 160 characters and the flasher address is
    // derived from one, which is what sets the ceiling (~2×408B). The short fixed
    // AP payloads still encode at a low version and simply draw larger, since the
    // scale below is chosen from the module count rather than the cap.
    constexpr int kMaxVer = 10;
    uint8_t qr[qrcodegen_BUFFER_LEN_FOR_VERSION(kMaxVer)];
    uint8_t tmp[qrcodegen_BUFFER_LEN_FOR_VERSION(kMaxVer)];
    if (!qrcodegen_encodeText(text, tmp, qr, qrcodegen_Ecc_MEDIUM,
                              qrcodegen_VERSION_MIN, kMaxVer,
                              qrcodegen_Mask_AUTO, /*boostEcl=*/true))
        return false;
    const int n = qrcodegen_getSize(qr);              // modules per side
    const int quiet = 4;                              // spec-recommended light border
    const int total = n + 2 * quiet;
    const int avail = 124;                             // px budget for the whole code
    int scale = avail / total; if (scale < 1) scale = 1;
    const int dim = total * scale;
    const int ox = (kActiveW - dim) / 2;
    fb.fillRect(ox, oy, dim, dim, palColor(Pal::INK));
    for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x)
            if (qrcodegen_getModule(qr, x, y))
                fb.fillRect(ox + (x + quiet) * scale, oy + (y + quiet) * scale,
                            scale, scale, palColor(Pal::PAPER));
    return true;
}

int pediaQrPages(bool provisioned) { return provisioned ? 2 : 3; }

void drawPediaQr(Framebuffer& fb, int page, bool provisioned) {
    // Static QR pages, in the order the steps actually happen (the AP address is
    // fixed at 192.168.4.1): join the AP, then — only while no home network is
    // stored — set one up, then open the 'Pedia. A cycles, C exits (game_config).
    // The join SSID matches config.h AP_SSID (default "Malwarium"); an open AP →
    // T:nopass.
    //
    // The setup step earns its place in the middle because it is the second thing
    // that happens on a fresh device and the phone is already on the AP by then.
    // It DISAPPEARS once a network is stored rather than sitting there forever as a
    // step nobody needs twice — and A always walks past it either way, so it never
    // stands between the operator and the 'Pedia.
    const int pages = pediaQrPages(provisioned);
    // With setup gone, the 'Pedia is page 1; with it present, page 2. Naming the
    // step rather than the index keeps the two layouts from drifting apart.
    const bool join = (page == 0);
    const bool setup = !provisioned && page == 1;

    drawHeaderBand(fb, join ? "QR: JOIN WI-FI" : setup ? "QR: SET UP WI-FI" : "QR: OPEN PEDIA");

    const char* payload = join  ? "WIFI:S:Malwarium;T:nopass;;"
                        : setup ? "http://192.168.4.1/setup"
                                : "http://192.168.4.1/pedia";
    const int oy = 28;
    const int dim = 124;                               // matches blitQr's budget
    if (blitQr(fb, payload, oy)) {
        const char* l1 = join  ? "SCAN TO JOIN 'MALWARIUM'"
                       : setup ? "SCAN TO SET UP INTERNET"
                               : "SCAN TO OPEN PEDIA";
        const char* l2 = join  ? "OPEN WI-FI (NO PASSWORD)"
                       : setup ? "TYPE YOUR WI-FI PASSWORD"
                               : "192.168.4.1/PEDIA";
        drawText(fb, (kActiveW - textWidth(l1)) / 2, oy + dim + 6, l1,
                 palColor(Pal::INK));
        drawText(fb, (kActiveW - textWidth(l2)) / 2, oy + dim + 18, l2,
                 palColor(Pal::INK_DIM));
    } else {
        drawText(fb, (kActiveW - textWidth("QR ENCODE FAILED")) / 2, oy + 60,
                 "QR ENCODE FAILED", palColor(Pal::WARN));
    }

    // Page indicator + controls hint (dual-coded: the active page is the bright
    // dot AND named in the header). "1/3 · A NEXT · C EXIT".
    char hint[24];
    std::snprintf(hint, sizeof(hint), "%d/%d  A NEXT  C EXIT", page + 1, pages);
    drawText(fb, (kActiveW - textWidth(hint)) / 2, oy + dim + 32, hint,
             palColor(Pal::ACCENT));
}

void drawUpdateQr(Framebuffer& fb, const char* manifestUrl) {
    drawHeaderBand(fb, "QR: FLASH BY USB");

    char url[192];
    if (!updateFlasherUrl(manifestUrl, url, sizeof(url))) {
        // Reachable only defensively — the row that opens this screen is dead
        // without a source — but a blank panel would be the worst way to say so.
        drawText(fb, kMargin, 40, "NO UPDATE SOURCE SET:", palColor(Pal::WARN));
        drawText(fb, kMargin, 52, "THIS DEVICE KNOWS NO", palColor(Pal::WARN));
        drawText(fb, kMargin, 64, "HOST TO POINT YOU AT.", palColor(Pal::WARN));
        drawHintBand(fb, "C BACK");
        return;
    }

    const int oy = 28;
    const int dim = 124;                               // matches blitQr's budget
    if (blitQr(fb, url, oy)) {
        // The two things a scanned page cannot tell someone standing here with the
        // device in one hand: which browser it needs, and what to do to the device
        // before the cable goes in. The phone that scans this is not the machine
        // that flashes, which is the part that surprises people.
        const char* l1 = "OPEN IN CHROME ON A PC";
        const char* l2 = "HOLD A WHILE PLUGGING IN";
        drawText(fb, (kActiveW - textWidth(l1)) / 2, oy + dim + 6, l1, palColor(Pal::INK));
        drawText(fb, (kActiveW - textWidth(l2)) / 2, oy + dim + 18, l2, palColor(Pal::INK));
    } else {
        drawText(fb, (kActiveW - textWidth("QR ENCODE FAILED")) / 2, oy + 60,
                 "QR ENCODE FAILED", palColor(Pal::WARN));
    }

    drawText(fb, (kActiveW - textWidth("C EXIT")) / 2, oy + dim + 32, "C EXIT",
             palColor(Pal::ACCENT));
}

}  // namespace mal
