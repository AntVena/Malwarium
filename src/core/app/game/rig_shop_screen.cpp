#include "core/app/game/rig_shop_screen.h"

#include <algorithm>
#include <cstdio>

#include "core/app/game_internal.h"
#include "core/render/canvas.h"
#include "core/render/font.h"
#include "core/render/palette.h"
#include "core/ui/layout.h"
#include "core/ui/spec_sheet.h"
#include "core/ui/widgets.h"

// The SHOP list is the Rig Shop (game_rig_shop.h): each row is a one-time or tiered
// account upgrade bought with Bits, described by a RigUpgradeDef. The input and the
// render loop are generic over the whole table, so adding a row is a table entry. The
// list overflows its kRigVisibleRows window, so A-cycling past the bottom scrolls.

namespace mal {

namespace {

// Append one row to a readout rigSpec has already built. The info page states what a
// run COSTS right now, a live number off the pet's defrag tally that a static content
// table cannot carry, beside the magnitudes the row itself declares.
void appendSpecRow(SpecRows& s, const char* label, const char* fmt, int v) {
    const int cap = static_cast<int>(sizeof(s.rows) / sizeof(s.rows[0]));
    if (s.count >= cap) return;
    SpecRow& r = s.rows[s.count++];
    std::snprintf(r.label, sizeof(r.label), "%s", label);
    std::snprintf(r.value, sizeof(r.value), fmt, v);
}

// The info page's geometry. The description pages on A, so the renderer and the pager
// agree on how much of it is on screen through these (the shape CREW's detail page uses).
constexpr int kSvcInfoTop = 30;
constexpr int kSvcInfoProseBottom = kActiveH - kHintBandH;

// Every slot the SHOP list shows, in list order: the SERVICES head slot (once the
// player owns a service to switch), then every row with something left to buy.
int shopSlots(const RigShopHost& host, int* out, int max) {
    int n = 0;
    if (host.rigServicesOffered() && n < max) out[n++] = kRigSlotServices;
    for (int i = 0; i < kRigUpgradeCount && n < max; ++i)
        if (host.shopRowOffered(i)) out[n++] = i;
    return n;
}
}  // namespace

void RigShopScreen::enter() {
    view_ = ShopView::Rows;
    slot_ = 0;
    serviceRow_ = 0;
    serviceScroll_ = 0;
}

int RigShopScreen::nextSlot(const RigShopHost& host, int cur, int dir) const {
    // One index space for both kinds of slot: index 0 is the SERVICES head
    // (kRigSlotServices, -1) and index i+1 is row i, so `slot + 1` is the index of any
    // slot and the walk is the same arithmetic for both. A cursor on a slot that has
    // just left the list steps from where it was, so a bought-out row hands the cursor
    // to its neighbour.
    const int total = kRigUpgradeCount + 1;
    for (int step = 1; step <= total; ++step) {
        const int idx = (((cur + 1) + dir * step) % total + total) % total;
        const int slot = idx - 1;
        if (slot == kRigSlotServices ? host.rigServicesOffered() : host.shopRowOffered(slot))
            return slot;
    }
    return cur;
}

void RigShopScreen::stepSlot(const RigShopHost& host, int dir) {
    slot_ = nextSlot(host, slot_, dir);
}

void RigShopScreen::stepService(const RigShopHost& host, int dir) {
    int rows[kRigUpgradeCount];
    const int n = host.rigServiceRows(rows, kRigUpgradeCount);
    if (n > 0) serviceRow_ = ((serviceRow_ + dir) % n + n) % n;
}

void RigShopScreen::onButton(RigShopHost& host, const ButtonEvent& ev) {
    switch (view_) {
        case ShopView::Rows:        onRows(host, ev); break;
        case ShopView::Services:    onServices(host, ev); break;
        case ShopView::ServiceInfo: onServiceInfo(host, ev); break;
    }
}

void RigShopScreen::onRows(RigShopHost& host, const ButtonEvent& ev) {
    if (ev.button == Button::A) {
        slot_ = nextSlot(host, slot_, +1);
    } else if (ev.button == Button::B) {
        if (slot_ == kRigSlotServices) {
            view_ = ShopView::Services;   // the head slot opens the board; it is not for sale
            serviceRow_ = 0;
            host.markDirty();
            return;
        }
        host.buyRigUpgrade(slot_);
        // The purchase may have taken the focused row out of the list. Hop to the next
        // offered one so the cursor never rests on a row nobody drew.
        if (!host.shopRowOffered(slot_)) slot_ = nextSlot(host, slot_, +1);
    } else if (ev.button == Button::C) {
        host.leaveToCarousel();
    }
}

void RigShopScreen::onServices(RigShopHost& host, const ButtonEvent& ev) {
    int rows[kRigUpgradeCount];
    const int n = host.rigServiceRows(rows, kRigUpgradeCount);
    // C backs up ONE view, to the storefront that opened this (the stack CREW's views
    // walk). An empty board leaves the same way.
    if (ev.button == Button::C || n == 0) {
        view_ = ShopView::Rows;
        host.markDirty();
        return;
    }
    if (serviceRow_ >= n) serviceRow_ = 0;
    if (ev.button == Button::A) {
        serviceRow_ = (serviceRow_ + 1) % n;
        host.markDirty();
    } else if (ev.button == Button::B) {
        // B is a tap/hold pair here (the shape the VAULT's bulk-open and the ITEMS filter
        // take): arm on the press; the release flips the switch, the hold opens the info
        // page. How long B is held decides which, so the press itself does nothing.
        host.armHoldB();
    }
}

void RigShopScreen::releaseB(RigShopHost& host) {
    if (view_ != ShopView::Services) return;
    int rows[kRigUpgradeCount];
    const int n = host.rigServiceRows(rows, kRigUpgradeCount);
    if (n == 0) return;
    if (serviceRow_ >= n) serviceRow_ = 0;
    host.toggleRigService(rows[serviceRow_]);
}

SpecRows RigShopScreen::serviceSpec(const RigShopHost& host, int row) {
    SpecRows spec = rigSpec(kRigUpgrades[row], host.rigLevel(row));
    // Disk Maintenance's UPKEEP multiple is half a price: a run costs that multiple of
    // THIS pet's defrag price, which climbs with every defrag the pet has had.
    const int dm = host.diskMaintenanceLevel();
    if (row == kRigRowDiskMaintenance && dm > 0)
        appendSpecRow(spec, "PER RUN", "%d BITS",
                      host.defragCost() * kDiskMaintenanceCostMult[dm - 1]);
    return spec;
}

int RigShopScreen::serviceProseLines(const RigShopHost& host, int row) {
    const int top = kSvcInfoTop + serviceSpec(host, row).count * kLineH + kSpecBlockGap;
    const int room = (kSvcInfoProseBottom - top) / kLineH;
    return room > 0 ? room : 0;
}

void RigShopScreen::openServiceInfo(RigShopHost& host) {
    if (view_ != ShopView::Services) return;
    int rows[kRigUpgradeCount];
    const int n = host.rigServiceRows(rows, kRigUpgradeCount);
    if (n == 0) return;
    if (serviceRow_ >= n) serviceRow_ = 0;
    view_ = ShopView::ServiceInfo;
    serviceScroll_ = 0;      // a page always opens at the top of its description
    host.markDirty();
}

void RigShopScreen::onServiceInfo(RigShopHost& host, const ButtonEvent& ev) {
    // A reader. A pages a description too long for the panel and wraps at the end, so
    // one that fits never moves. The bound is the room the renderer gives it
    // (serviceProseLines), so the two agree about where the last page is.
    if (ev.button == Button::A) {
        int rows[kRigUpgradeCount];
        const int n = host.rigServiceRows(rows, kRigUpgradeCount);
        if (n > 0) {
            const int row = rows[serviceRow_ < n ? serviceRow_ : 0];
            const int room = serviceProseLines(host, row);
            const int lines = proseLineCount(kActiveW - 2 * kMargin,
                                             kRigUpgrades[row].serviceInfo);
            if (room > 0 && lines > room)
                serviceScroll_ = (serviceScroll_ + 1) % (lines - room + 1);
        }
        host.markDirty();
        return;
    }
    view_ = ShopView::Services;
    serviceScroll_ = 0;
    host.markDirty();
}

// --- Rendering -------------------------------------------------------------

void RigShopScreen::draw(Framebuffer& fb, const RigShopHost& host, int beat) const {
    switch (view_) {
        case ShopView::ServiceInfo: drawServiceInfo(fb, host); break;
        case ShopView::Services:    drawServices(fb, host, beat); break;
        case ShopView::Rows:        drawRows(fb, host, beat); break;
    }
}

void RigShopScreen::drawServiceInfo(Framebuffer& fb, const RigShopHost& host) const {
    // SHOP > SERVICES > info: the spec sheet every "what does this do" panel uses
    // (ui/spec_sheet.h). The row's own readout, bounded and never cut, sits over the
    // authored description of what running it costs.
    int rows[kRigUpgradeCount];
    const int n = host.rigServiceRows(rows, kRigUpgradeCount);
    if (n == 0) {
        drawHeaderBand(fb, "SERVICE");
        drawText(fb, kMargin, 40, "NO SERVICES OWNED", palColor(Pal::INK_DIM));
        return;
    }
    const int row = rows[serviceRow_ < n ? serviceRow_ : 0];
    const RigUpgradeDef& def = kRigUpgrades[row];
    const bool on = host.rigFeatureActive(row);
    // The header names the service and states the switch, so the panel underneath is
    // all description: the page is opened to read it.
    drawHeaderBand(fb, def.displayName, host.rigServiceWord(row),
                   on ? palColor(Pal::ACCENT) : palColor(Pal::INK_DIM));

    const SpecRows spec = serviceSpec(host, row);
    SpecSheet sheet;
    sheet.rows = spec.rows;
    sheet.rowCount = spec.count;
    sheet.prose = def.serviceInfo;
    sheet.proseScroll = serviceScroll_;
    drawSpecSheet(fb, kMargin, kSvcInfoTop, kActiveW - 2 * kMargin,
                  kSvcInfoProseBottom, sheet);
    // A pages a description that doesn't fit; with nothing more to show, the band offers
    // only the way out.
    const int room = serviceProseLines(host, row);
    const bool pages =
        room > 0 && proseLineCount(kActiveW - 2 * kMargin, def.serviceInfo) > room;
    drawHintBand(fb, pages ? "A PAGE  C BACK" : "C BACK");
}

void RigShopScreen::drawServices(Framebuffer& fb, const RigShopHost& host, int beat) const {
    // SHOP > SERVICES: one row per OWNED service, each reading its switch. Nothing here
    // is bought, so the header carries no wallet.
    drawHeaderBand(fb, "SERVICES");

    int rows[kRigUpgradeCount];
    const int n = host.rigServiceRows(rows, kRigUpgradeCount);
    if (n == 0) {
        drawText(fb, kMargin, 40, "NO SERVICES OWNED", palColor(Pal::INK_DIM));
        return;
    }
    const int sel = serviceRow_ < n ? serviceRow_ : 0;

    const int scrollTop = listScrollTop(sel, n, kHackerListRows);
    for (int v = 0; v < kHackerListRows && scrollTop + v < n; ++v) {
        const int i = scrollTop + v;
        const int row = rows[i];
        const int rowY = kHackerListTop + v * kHackerListRowH;
        const bool cur = i == sel;
        if (cur) drawRowCursor(fb, 2, rowY + 1, palColor(Pal::ACCENT));
        // A stopped service is stated twice, by the word OFF and by the dim ink, so the
        // list reads right in grayscale.
        const bool on = host.rigFeatureActive(row);
        drawLabelValue(fb, kMargin, rowY, kRigUpgrades[row].displayName,
                       cur ? palColor(Pal::INK) : palColor(Pal::INK_DIM),
                       host.rigServiceWord(row),
                       on ? palColor(Pal::ACCENT) : palColor(Pal::INK_DIM),
                       beat, Overflow::Scroll);
    }
    drawScrollbar(fb, kHackerListTop, kHackerListRows * kHackerListRowH, n, scrollTop,
                  kHackerListRows);
    fb.fillRect(0, kActiveH - 26, kActiveW, 1, palColor(Pal::TRACK));
    // An askable row's B steps to its next position, so the label names that one.
    const int selRow = rows[sel];
    const ServiceMode mode = host.rigServiceMode(selRow);
    const char* bLabel = !kRigUpgrades[selRow].askable
                             ? (host.rigFeatureActive(selRow) ? "B STOP" : "B START")
                         : mode == ServiceMode::On  ? "B SET ASK"
                         : mode == ServiceMode::Ask ? "B SET NO"
                                                    : "B SET YES";
    drawText(fb, kMargin, kActiveH - 20, bLabel, palColor(Pal::ACCENT));
    // The other half of B, stated where the tap is: a held B reads the service out.
    drawText(fb, kActiveW - kMargin - textWidth("HOLD - WHAT IT DOES"),
             kActiveH - 20, "HOLD - WHAT IT DOES", palColor(Pal::INK_DIM));
}

void RigShopScreen::drawRows(Framebuffer& fb, const RigShopHost& host, int beat) const {
    // The storefront. The header carries the Bits wallet.
    char bitsStr[16];
    std::snprintf(bitsStr, sizeof(bitsStr), "%d BITS", host.bits());
    drawHeaderBand(fb, "SHOP", bitsStr, palColor(Pal::ACCENT));

    // Only purchasable rows are listed, behind the SERVICES head slot that owning a
    // service adds.
    int visRows[kRigUpgradeCount + 1];
    const int visN = shopSlots(host, visRows, kRigUpgradeCount + 1);
    if (visN == 0) {
        drawText(fb, kMargin, kRigRowTop, "ALL UPGRADES OWNED", palColor(Pal::INK_DIM));
        return;
    }

    int selPos = 0;   // slot_'s position within visRows, for the scroll math
    for (int i = 0; i < visN; ++i) if (visRows[i] == slot_) { selPos = i; break; }
    const int scrollTop = listScrollTop(selPos, visN, kRigVisibleRows);
    // Only the rows that exist are drawn, so a player who has bought most upgrades gets
    // the slack back for the status line.
    const int visibleRows = std::max(1, std::min(visN, kRigVisibleRows));

    // Each row: name and price on one line, then the row's readout as the shared aligned
    // grid underneath. The price sits beside the NAME because a "NOW -> NEXT" pair is
    // wider than what is left beside a right-aligned "16384 BITS".
    for (int v = 0; v < visibleRows && scrollTop + v < visN; ++v) {
        const int row = visRows[scrollTop + v];
        const int rowY = kRigRowTop + v * kRigRowPitch;
        const bool sel = slot_ == row;
        if (sel) drawRowCursor(fb, 2, rowY + 1, palColor(Pal::ACCENT));

        if (row == kRigSlotServices) {
            // The head slot opens the switchboard and costs nothing. Its value column
            // counts what is RUNNING of what is owned.
            int svcRows[kRigUpgradeCount];
            const int svcN = host.rigServiceRows(svcRows, kRigUpgradeCount);
            char svc[16];
            std::snprintf(svc, sizeof(svc), "%d/%d ON", host.rigServicesRunning(), svcN);
            drawLabelValue(fb, kMargin, rowY, "SERVICES",
                           sel ? palColor(Pal::INK) : palColor(Pal::INK_DIM), svc,
                           palColor(Pal::INK), beat, Overflow::Scroll);
            SpecBuilder head;
            head.flag("SWITCH AUTOMATION");
            drawSpecGrid(fb, kMargin, rowY + kFontH + 5, kActiveW - 2 * kMargin,
                         head.out.rows, head.out.count, kFontH + 3,
                         palColor(Pal::INK_DIM), palColor(Pal::ACCENT));
            if (sel) {
                const int sy = kRigRowTop + visibleRows * kRigRowPitch;
                fb.fillRect(0, sy - 6, kActiveW, 1, palColor(Pal::TRACK));
                drawText(fb, kMargin, sy, "B TO OPEN", palColor(Pal::ACCENT));
            }
            continue;
        }

        const RigUpgradeDef& def = kRigUpgrades[row];
        const int cost = host.rigUpgradeCostFor(row);
        const bool afford = host.bits() >= cost;
        char rhs[16];
        std::snprintf(rhs, sizeof(rhs), "%d BITS", cost);
        // Every row scrolls: several names ("REDUCE EXPLORE FRAG") overflow the column
        // and would otherwise sit cut off until the cursor landed on them.
        drawLabelValue(fb, kMargin, rowY, def.displayName,
                       sel ? palColor(Pal::INK) : palColor(Pal::INK_DIM), rhs,
                       afford ? palColor(Pal::INK) : palColor(Pal::INK_DIM),
                       beat, Overflow::Scroll);

        const SpecRows spec = rigSpec(def, host.rigLevel(row));
        drawSpecGrid(fb, kMargin, rowY + kFontH + 5, kActiveW - 2 * kMargin,
                     spec.rows, spec.count, kFontH + 3, palColor(Pal::INK_DIM),
                     palColor(Pal::ACCENT));

        // The focused row's status line states the buy reason in words (grayscale-safe).
        if (sel) {
            const char* status = afford ? "B TO BUY" : "NOT ENOUGH BITS";
            const int sy = kRigRowTop + visibleRows * kRigRowPitch;
            fb.fillRect(0, sy - 6, kActiveW, 1, palColor(Pal::TRACK));
            drawText(fb, kMargin, sy, status,
                     afford ? palColor(Pal::ACCENT) : palColor(Pal::INK_DIM));
        }
    }

    drawScrollbar(fb, kRigRowTop, kRigVisibleRows * kRigRowPitch, visN, scrollTop,
                  kRigVisibleRows);
}

}  // namespace mal
