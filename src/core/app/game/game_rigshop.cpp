#include "core/app/game.h"

#include <algorithm>
#include <cstdio>

#include "core/app/game_internal.h"
#include "core/app/game_rig_shop.h"
#include "core/render/canvas.h"
#include "core/render/font.h"
#include "core/render/palette.h"
#include "core/ui/layout.h"
#include "core/ui/spec_sheet.h"
#include "core/ui/widgets.h"

// game_rigshop.cpp — the Hacker face's SHOP slot (game_hacker.cpp dispatches to it).
//
// The SHOP list is the Rig Shop (game_rig_shop.h): each row is a one-time or tiered
// account upgrade bought with Bits, described by a RigUpgradeDef. buyRigUpgrade/the
// render loop below are generic over the whole table, so adding a row is a table
// entry, not new control flow. The list overflows the 3-visible-row window
// (kRigVisibleRows), so A-cycling past the bottom scrolls rather than growing the screen.
// Behind the list's head slot sit the other two views: the SERVICES switchboard and one
// service's info page (Game::ShopView).

namespace mal {

namespace {

// Append one row to a readout rigSpec has already built. The SERVICES info page says
// what a run COSTS right now — a live number off the pet's own defrag tally, which a
// static content table cannot carry — beside the magnitudes the row itself declares.
void appendSpecRow(SpecRows& s, const char* label, const char* fmt, int v) {
    const int cap = static_cast<int>(sizeof(s.rows) / sizeof(s.rows[0]));
    if (s.count >= cap) return;
    SpecRow& r = s.rows[s.count++];
    std::snprintf(r.label, sizeof(r.label), "%s", label);
    std::snprintf(r.value, sizeof(r.value), fmt, v);
}

// The SERVICES info page's geometry. The description is the elastic half and pages on
// A, so the renderer and the pager have to agree on how much of it is on screen — these
// are what they agree through (the shape CREW's detail page uses).
constexpr int kSvcInfoTop = 30;
constexpr int kSvcInfoProseBottom = kActiveH - kHintBandH;

// Every slot the SHOP list shows, in list order: the SERVICES head slot (once the
// player owns a service to switch), then every row with something left to buy.
int shopSlots(const Game& g, int* out, int max) {
    int n = 0;
    if (g.rigServicesOffered() && n < max) out[n++] = kRigSlotServices;
    for (int i = 0; i < kRigUpgradeCount && n < max; ++i)
        if (g.shopRowOffered(i)) out[n++] = i;
    return n;
}
}  // namespace

// Is this row on sale right now? One way it isn't: it's maxed/owned, with nothing left
// to buy. Every row in the table is an ordinary purchase — the one Hacker-face unlock
// that ISN'T is a MERGE HUB recipe, which is won off a Decryptogram and is not a rig
// row to gate. The single answer the A-cycle, the render loop and buyRigUpgrade all
// defer to, so the list can't show a row the cursor skips, or sell one it never drew.
bool Game::shopRowOffered(int i) const {
    if (i < 0 || i >= kRigUpgradeCount) return false;
    return rigLevel_[i] < kRigUpgrades[i].maxLevel;
}

int Game::nextShopSlot(int cur, int dir) const {
    // One index space for both kinds of slot: index 0 is the SERVICES head
    // (kRigSlotServices, -1) and index i+1 is row i, so `slot + 1` is the index of any
    // slot and the walk is the same arithmetic for both. A cursor sitting on a slot
    // that has just left the list still steps from where it was, so a bought-out row
    // hands the cursor to its neighbour rather than to the top.
    const int total = kRigUpgradeCount + 1;
    for (int step = 1; step <= total; ++step) {
        const int idx = (((cur + 1) + dir * step) % total + total) % total;
        const int slot = idx - 1;
        if (slot == kRigSlotServices ? rigServicesOffered() : shopRowOffered(slot))
            return slot;
    }
    return cur;
}

void Game::onHackerShop(const ButtonEvent& ev) {
    // The SHOP is three screens behind one slot; whichever is up owns the buttons.
    if (shopView_ == ShopView::ServiceInfo) { onRigServiceInfo(ev); return; }
    if (shopView_ == ShopView::Services) { onRigServices(ev); return; }
    if (ev.button == Button::A) {
        hackerShopRow_ = nextShopSlot(hackerShopRow_, +1);   // skip unoffered
    } else if (ev.button == Button::B) {
        if (hackerShopRow_ == kRigSlotServices) {
            shopView_ = ShopView::Services;   // the head slot is a door, not a purchase
            rigServiceRow_ = 0;
            dirty_ = true;
            return;
        }
        buyRigUpgrade(hackerShopRow_);
        // That purchase may have just taken the focused row out of the list — hop
        // to the next offered one so the cursor never rests on a row nobody drew.
        if (!shopRowOffered(hackerShopRow_))
            hackerShopRow_ = nextShopSlot(hackerShopRow_, +1);
    } else if (ev.button == Button::C) {
        nav_ = Nav::Cursor;
    }
}

void Game::onRigServices(const ButtonEvent& ev) {
    int rows[kRigUpgradeCount];
    const int n = rigServiceRows(rows, kRigUpgradeCount);
    // C backs up ONE view, to the storefront that opened this — the same stack CREW's
    // views walk. An empty board (nothing owned any more) leaves the same way.
    if (ev.button == Button::C || n == 0) {
        shopView_ = ShopView::Rows;
        dirty_ = true;
        return;
    }
    if (rigServiceRow_ >= n) rigServiceRow_ = 0;
    if (ev.button == Button::A) {
        rigServiceRow_ = (rigServiceRow_ + 1) % n;
        dirty_ = true;
    } else if (ev.button == Button::B) {
        // B is a tap/hold pair here (the shape the VAULT's bulk-open and the ITEMS
        // filter take): ARM on the press, and let the release flip the switch or the
        // hold open the info page. Nothing happens on the press itself, because the
        // two things B can mean are decided by how long it is held.
        bHeld_ = true;
        bDownMs_ = nowMs_;
    }
}

void Game::rigServiceReleaseB() {
    if (!(face_ == Face::Hacker && nav_ == Nav::Submenu &&
          enteredHackerId() == HackerSlotId::Shop && shopView_ == ShopView::Services))
        return;   // bHeld_ was armed elsewhere (the VAULT / CFG holds) — no-op
    int rows[kRigUpgradeCount];
    const int n = rigServiceRows(rows, kRigUpgradeCount);
    if (n == 0) return;
    if (rigServiceRow_ >= n) rigServiceRow_ = 0;
    toggleRigService(rows[rigServiceRow_]);
}

SpecRows Game::rigServiceSpec(int row) const {
    SpecRows spec = rigSpec(kRigUpgrades[row], rigLevel_[row]);
    // Disk Maintenance's UPKEEP multiple is only half of a price: what a run costs is
    // that multiple of THIS pet's defrag price, which climbs with every defrag the pet
    // has had. The multiple is on the row; only the game knows the rest.
    if (row == kRigRowDiskMaintenance && diskMaintenanceLevel() > 0)
        appendSpecRow(spec, "PER RUN", "%d BITS",
                      defragCost() * kDiskMaintenanceCostMult[diskMaintenanceLevel() - 1]);
    return spec;
}

int Game::rigServiceProseLines(int row) const {
    const int top = kSvcInfoTop + rigServiceSpec(row).count * kLineH + kSpecBlockGap;
    const int room = (kSvcInfoProseBottom - top) / kLineH;
    return room > 0 ? room : 0;
}

void Game::openRigServiceInfo() {
    int rows[kRigUpgradeCount];
    const int n = rigServiceRows(rows, kRigUpgradeCount);
    if (n == 0) return;
    if (rigServiceRow_ >= n) rigServiceRow_ = 0;
    shopView_ = ShopView::ServiceInfo;
    rigServiceScroll_ = 0;      // a page always opens at the top of its description
    dirty_ = true;
}

void Game::onRigServiceInfo(const ButtonEvent& ev) {
    // A reader: A pages a description too long for the panel and WRAPS at the end, so
    // one that fits never moves and one that doesn't always comes back to its start.
    // The bound is the room the renderer will give it (rigServiceProseLines), not a
    // guess — the two would otherwise disagree about where the last page is.
    if (ev.button == Button::A) {
        int rows[kRigUpgradeCount];
        const int n = rigServiceRows(rows, kRigUpgradeCount);
        if (n > 0) {
            const int row = rows[rigServiceRow_ < n ? rigServiceRow_ : 0];
            const int room = rigServiceProseLines(row);
            const int lines = proseLineCount(kActiveW - 2 * kMargin,
                                             kRigUpgrades[row].serviceInfo);
            if (room > 0 && lines > room)
                rigServiceScroll_ = (rigServiceScroll_ + 1) % (lines - room + 1);
        }
        dirty_ = true;
        return;
    }
    shopView_ = ShopView::Services;
    rigServiceScroll_ = 0;
    dirty_ = true;
}

void Game::toggleRigService(int row) {
    if (!rigServiceOwned(row)) return;   // nothing bought here has a switch
    const uint32_t bit = 1u << static_cast<unsigned>(row);
    if (kRigUpgrades[row].askable) {
        // Three positions, stepped in the order the board reads them.
        switch (rigServiceMode(row)) {
            case ServiceMode::On:  rigServicesAsk_ |= bit; break;
            case ServiceMode::Ask: rigServicesAsk_ &= ~bit; rigServicesOff_ |= bit; break;
            case ServiceMode::Off: rigServicesOff_ &= ~bit; break;
        }
        log_.push(LogEventType::ItemUsed, rigServiceMode(row) == ServiceMode::On  ? "SERVICE SET YES"
                                        : rigServiceMode(row) == ServiceMode::Ask ? "SERVICE SET ASK"
                                                                                  : "SERVICE SET NO");
        dirty_ = true;
        markSaveDirty();
        return;
    }
    rigServicesOff_ ^= bit;
    log_.push(LogEventType::ItemUsed,
              rigFeatureActive(row) ? "SERVICE STARTED" : "SERVICE STOPPED");
    dirty_ = true;
    markSaveDirty();
}

void Game::buyRigUpgrade(int row) {
    if (!shopRowOffered(row)) return;   // MAXED/OWNED, or gated — inert
    const RigUpgradeDef& def = kRigUpgrades[row];
    const int cost = rigUpgradeCostFor(row);
    if (bits_ < cost) return;                       // NOT ENOUGH BITS — inert
    bits_ -= cost;
    grantRigLevel(row, 1);
    dirty_ = true;
}

void Game::grantRigLevel(int row, int levels) {
    if (row < 0 || row >= kRigUpgradeCount || levels <= 0) return;
    const RigUpgradeDef& def = kRigUpgrades[row];
    for (int i = 0; i < levels; ++i) {
        ++rigLevel_[row];
        // Bandwidth is the one row with an immediate effect: the freshly-owned pool
        // capacity is available to farm with right away, not just on the next regen
        // tick. Applied per level so a multi-level grant tops up by the whole amount.
        if (def.effectKind == RigEffectKind::GrantBandwidth) {
            bandwidth_ += def.effectMagnitude;
            if (bandwidth_ > bandwidthMax()) bandwidth_ = bandwidthMax();
        }
    }
    log_.push(LogEventType::ItemGained, def.logText);
    markSaveDirty();
}

// --- Rendering -------------------------------------------------------------

void Game::drawRigShop(Framebuffer& fb) const {
    switch (shopView_) {
        case ShopView::ServiceInfo: drawRigServiceInfo(fb); break;
        case ShopView::Services:    drawRigServices(fb); break;
        case ShopView::Rows:        drawRigShopRows(fb); break;
    }
}

void Game::drawRigServiceInfo(Framebuffer& fb) const {
    // SHOP > SERVICES > info — the spec sheet every "what does this do" panel uses
    // (ui/spec_sheet.h): the row's own readout, bounded and never cut, over the
    // authored description of what running it costs. Reached by HOLDING B on the
    // board, so a player about to throw a switch can read the terms first.
    int rows[kRigUpgradeCount];
    const int n = rigServiceRows(rows, kRigUpgradeCount);
    if (n == 0) {
        drawHeaderBand(fb, "SERVICE");
        drawText(fb, kMargin, 40, "NO SERVICES OWNED", palColor(Pal::INK_DIM));
        return;
    }
    const int row = rows[rigServiceRow_ < n ? rigServiceRow_ : 0];
    const RigUpgradeDef& def = kRigUpgrades[row];
    const bool on = rigFeatureActive(row);
    // The header names the service and states the switch, so the panel underneath is
    // all description — the page is opened to READ, and a name line inside it would
    // cost a line of the thing being read.
    drawHeaderBand(fb, def.displayName, rigServiceWord(row),
                   on ? palColor(Pal::ACCENT) : palColor(Pal::INK_DIM));

    const SpecRows spec = rigServiceSpec(row);
    SpecSheet sheet;
    sheet.rows = spec.rows;
    sheet.rowCount = spec.count;
    sheet.prose = def.serviceInfo;
    sheet.proseScroll = rigServiceScroll_;
    drawSpecSheet(fb, kMargin, kSvcInfoTop, kActiveW - 2 * kMargin,
                  kSvcInfoProseBottom, sheet);
    // A pages a description that doesn't fit; with nothing more to show, the band
    // offers only the way out — so the hint never advertises a page that isn't there.
    const int room = rigServiceProseLines(row);
    const bool pages =
        room > 0 && proseLineCount(kActiveW - 2 * kMargin, def.serviceInfo) > room;
    drawHintBand(fb, pages ? "A PAGE  C BACK" : "C BACK");
}

void Game::drawRigServices(Framebuffer& fb) const {
    // SHOP > SERVICES — the switchboard behind the list's head slot. One row per
    // OWNED service (rigServiceRows), each reading ON or OFF; A cycles, B flips the
    // focused one, C returns to the storefront. Nothing here is bought, so the
    // header carries no wallet: a switch costs nothing to throw.
    drawHeaderBand(fb, "SERVICES");

    int rows[kRigUpgradeCount];
    const int n = rigServiceRows(rows, kRigUpgradeCount);
    if (n == 0) {
        drawText(fb, kMargin, 40, "NO SERVICES OWNED", palColor(Pal::INK_DIM));
        return;
    }
    int sel = rigServiceRow_;
    if (sel >= n) sel = 0;

    const int scrollTop = listScrollTop(sel, n, kHackerListRows);
    for (int v = 0; v < kHackerListRows && scrollTop + v < n; ++v) {
        const int i = scrollTop + v;
        const int row = rows[i];
        const int rowY = kHackerListTop + v * kHackerListRowH;
        const bool cur = i == sel;
        if (cur) drawRowCursor(fb, 2, rowY + 1, palColor(Pal::ACCENT));
        // A stopped service is stated twice — the word OFF and the dim ink — so the
        // list reads right in grayscale, where the colour alone says nothing.
        const bool on = rigFeatureActive(row);
        drawLabelValue(fb, kMargin, rowY, kRigUpgrades[row].displayName,
                       cur ? palColor(Pal::INK) : palColor(Pal::INK_DIM),
                       rigServiceWord(row),
                       on ? palColor(Pal::ACCENT) : palColor(Pal::INK_DIM),
                       beat_, Overflow::Scroll);
    }
    drawScrollbar(fb, kHackerListTop, kHackerListRows * kHackerListRowH, n, scrollTop,
                  kHackerListRows);
    fb.fillRect(0, kActiveH - 26, kActiveW, 1, palColor(Pal::TRACK));
    // An askable row's B steps to its next position, so the label names that one.
    const int selRow = rows[sel];
    const char* bLabel = !kRigUpgrades[selRow].askable
                             ? (rigFeatureActive(selRow) ? "B STOP" : "B START")
                         : rigServiceMode(selRow) == ServiceMode::On  ? "B SET ASK"
                         : rigServiceMode(selRow) == ServiceMode::Ask ? "B SET NO"
                                                                      : "B SET YES";
    drawText(fb, kMargin, kActiveH - 20, bLabel, palColor(Pal::ACCENT));
    // The other half of B, stated where the tap is: a HELD B reads the service out
    // rather than switching it.
    drawText(fb, kActiveW - kMargin - textWidth("HOLD - WHAT IT DOES"),
             kActiveH - 20, "HOLD - WHAT IT DOES", palColor(Pal::INK_DIM));
}

void Game::drawRigShopRows(Framebuffer& fb) const {
    // SHOP — the Rig Shop (game_rig_shop.h). Header carries the Bits wallet (the
    // storefront grammar). The list iterates kRigUpgrades generically, windowed to
    // kRigVisibleRows (mirrors drawItemsList/drawMovePicker).
    char bitsStr[16];
    std::snprintf(bitsStr, sizeof(bitsStr), "%d BITS", bits_);
    drawHeaderBand(fb, "SHOP", bitsStr, palColor(Pal::ACCENT));

    // Maxed/owned rows have nothing left to buy — filter them out entirely so the
    // list only ever shows purchasable rows, behind the SERVICES head slot that
    // owning a service adds (shopSlots is in list order).
    int visRows[kRigUpgradeCount + 1];
    const int visN = shopSlots(*this, visRows, kRigUpgradeCount + 1);

    if (visN == 0) {
        drawText(fb, kMargin, kRigRowTop, "ALL UPGRADES OWNED", palColor(Pal::INK_DIM));
        return;
    }

    int selPos = 0;   // hackerShopRow_'s position within visRows, for scroll math
    for (int i = 0; i < visN; ++i) if (visRows[i] == hackerShopRow_) { selPos = i; break; }

    const int scrollTop = listScrollTop(selPos, visN, kRigVisibleRows);

    // ADAPTIVE window (mirrors drawMovePicker / drawShop): only the rows that
    // exist are drawn, so a late-game player who has bought most upgrades gets the
    // slack back for the status line rather than reserving it empty.
    const int visibleRows = std::max(1, std::min(visN, kRigVisibleRows));

    // Each row: name + price on one line, then the row's own readout as the shared
    // aligned grid underneath (rigSpec + drawSpecGrid). The price moves up beside
    // the NAME rather than sharing the readout's line, because a "NOW -> NEXT" pair
    // is wider than the leftovers next to a right-aligned "16384 BITS", so the two
    // would overprint.
    for (int v = 0; v < visibleRows && scrollTop + v < visN; ++v) {
        const int row = visRows[scrollTop + v];
        const int rowY = kRigRowTop + v * kRigRowPitch;
        const bool sel = hackerShopRow_ == row;
        if (sel) drawRowCursor(fb, 2, rowY + 1, palColor(Pal::ACCENT));

        if (row == kRigSlotServices) {
            // The head slot: a door onto the switchboard, priced in nothing. Its
            // value column counts what is RUNNING out of what is owned, which is
            // the one number a player wants before deciding to open it.
            int svcRows[kRigUpgradeCount];
            const int svcN = rigServiceRows(svcRows, kRigUpgradeCount);
            char svc[16];
            std::snprintf(svc, sizeof(svc), "%d/%d ON", rigServicesRunning(), svcN);
            drawLabelValue(fb, kMargin, rowY, "SERVICES",
                           sel ? palColor(Pal::INK) : palColor(Pal::INK_DIM), svc,
                           palColor(Pal::INK), beat_, Overflow::Scroll);
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
        const int level = rigLevel_[row];
        const int cost = rigUpgradeCostFor(row);
        const bool afford = bits_ >= cost;

        char rhs[16];
        std::snprintf(rhs, sizeof(rhs), "%d BITS", cost);
        // Marquee unconditionally, not just when focused: several rows
        // ("REDUCE EXPLORE FRAG", "REDUCE FRAG TRIGGER") overflow the name
        // column and, scrolling only on selection, would sit permanently cut
        // off mid-word until a player happened to land the cursor on them.
        drawLabelValue(fb, kMargin, rowY, def.displayName,
                       sel ? palColor(Pal::INK) : palColor(Pal::INK_DIM), rhs,
                       afford ? palColor(Pal::INK) : palColor(Pal::INK_DIM),
                       beat_, Overflow::Scroll);

        const SpecRows spec = rigSpec(def, level);
        drawSpecGrid(fb, kMargin, rowY + kFontH + 5, kActiveW - 2 * kMargin,
                     spec.rows, spec.count, kFontH + 3, palColor(Pal::INK_DIM),
                     palColor(Pal::ACCENT));

        // Per-selected-row status line — the grayscale-safe buy reason (grammar).
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
