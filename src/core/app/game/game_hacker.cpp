#include "core/app/game.h"

#include <algorithm>
#include <cstdio>

#include "tunables.h"
#include "core/app/game_internal.h"
#include "core/app/game_rig_shop.h"
#include "core/model/hacker_rank.h"
#include "core/render/canvas.h"
#include "core/render/font.h"
#include "core/render/palette.h"
#include "core/render/sprite.h"
#include "core/ui/carousel.h"
#include "core/ui/items_screen.h"
#include "core/ui/layout.h"
#include "core/ui/spec_sheet.h"
#include "core/ui/theme.h"
#include "core/ui/widgets.h"

// game_hacker.cpp — the Hacker face (07, /HACK2/SHOP1).
//
// A+C at the top level flips PET ↔ HACKER (toggleFace). The Hacker face reuses the
// pet-side carousel + submenu contracts wholesale — the same Nav::Idle/Cursor/Submenu
// states and the same A/B/C cursor cycling — so the only bespoke code here is the
// hacker slot roster, the live L2 screens (PROFILE viewer + SHOP list + VAULT), and
// the terminal-reskin centre canvas. MERGE HUB's own L2 screen lives in
// game_merge.cpp (this face only sells the hub itself — the recipes cooked in it are
// won off a Decryptogram), and CREW's (enlist + home network) in game_crew.cpp.
// PEERS (met operators) lives in game_peers.cpp and LINK (1v1 duels) in game_pvp.cpp;
// SCAN renders the inaccessible marker and B is inert.
//
// The SHOP list is the Rig Shop (game_rig_shop.h): each row is a one-time or tiered
// account upgrade bought with Bits, described by a RigUpgradeDef. buyRigUpgrade/the
// render loop below are generic over the whole table, so adding a row is a table
// entry, not new control flow. The list overflows the 3-visible-row window
// (kRigVisibleRows), so A-cycling past the bottom scrolls rather than growing the screen.

namespace mal {

namespace {

// Every slot the SHOP list shows, in list order: the SERVICES head slot (once the
// player owns a service to switch), then every row with something left to buy.
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
// that ISN'T is a MERGE HUB recipe, which is won off a Decryptogram and was never a rig
// row to gate. The single answer the A-cycle, the render loop and buyRigUpgrade all
// defer to, so the list can't show a row the cursor skips, or sell one it never drew.
bool Game::shopRowOffered(int i) const {
    if (i < 0 || i >= kRigUpgradeCount) return false;
    return rigLevel_[i] < kRigUpgrades[i].maxLevel;
}

// --- Face toggle + navigation ---------------------------------------------

void Game::toggleFace() {
    face_ = (face_ == Face::Pet) ? Face::Hacker : Face::Pet;
    // Land on the face's idle/centre canvas. Drop any cursor + sub-state so a
    // flip is a clean top-level context switch, never mid-submenu on the other face.
    nav_ = Nav::Idle;
    // Focus the first ACCESSIBLE hacker slot rather than blindly slot 0 — landing on an
    // inaccessible slot (B inert) would be a bad first impression. Computed from the
    // roster so it survives a reorder. Pet face has no such gating, so it always rests
    // on slot 0.
    cursor_ = 0;
    if (face_ == Face::Hacker) {
        const HackerCarouselSlot* slots = hackerCarouselSlots();
        for (int i = 0; i < kCarouselSlots; ++i)
            if (slots[i].accessible) { cursor_ = i; break; }
    }
    hackerShopRow_ = 0;
    listRow_ = 0;
    detailItem_ = nullptr;
    dirty_ = true;
}

HackerSlotId Game::enteredHackerId() const {
    return hackerCarouselSlots()[cursor_].id;
}

bool Game::hackerSlotAccessible(int slot) const {
    if (slot < 0 || slot >= kCarouselSlots) return false;
    const HackerCarouselSlot& s = hackerCarouselSlots()[slot];
    // MERGE is the one slot whose accessibility isn't fixed at compile time — it
    // flips live the moment the f Merge Hub purchase lands (mirrors
    // drawHackerCarousel's `mergeUnlocked` override).
    if (s.id == HackerSlotId::Merge) return mergeHubUnlocked();
#ifdef MALWARIUM_DEMO
    // SCAN/LINK/PEERS are the radio: a Wi-Fi sniff and a peer link, neither of which
    // a browser has. They stay on the shelf, dimmed, rather than opening onto a
    // screen that could only ever report nothing found (demo_config.h).
    if (s.id == HackerSlotId::Scan || s.id == HackerSlotId::Link ||
        s.id == HackerSlotId::Peers)
        return false;
#endif
    return s.accessible;
}

void Game::enterHackerSubmenu() {
    // B on an inaccessible slot is inert (the slot exists but isn't usable yet).
    if (!hackerSlotAccessible(cursor_)) return;
    nav_ = Nav::Submenu;
    hackerShopRow_ = 0;      // the first PURCHASABLE row; SERVICES sits above it
    shopView_ = ShopView::Rows;
    rigServiceRow_ = 0;
    rigServiceScroll_ = 0;
    hackerVaultRow_ = 0;
    hackerMergeRow_ = 0;
    crewView_ = CrewView::Hub;   // every entry opens at the top of the CREW screen
    crewHubRow_ = 0;
    crewTeamRow_ = 0;
    crewDetail_ = -1;
    crewProseScroll_ = 0;
    crewNetRow_ = 0;
    peerRow_ = 0;
    pvpRow_ = 0;
}

void Game::onHackerSubmenu(const ButtonEvent& ev) {
    switch (enteredHackerId()) {
        case HackerSlotId::Crew:
            onHackerCrew(ev);
            break;
        case HackerSlotId::Shop:
            onHackerShop(ev);
            break;
        case HackerSlotId::Vault:
            onHackerVault(ev);
            break;
        case HackerSlotId::Merge:
            onHackerMerge(ev);
            break;
        case HackerSlotId::Peers:
            onHackerPeers(ev);
            break;
        case HackerSlotId::Link:
            onHackerLink(ev);
            break;
        case HackerSlotId::Profile:
        default:
            // PROFILE is a flat read-only viewer (the STAT analog): nothing to select,
            // C backs out to the hacker carousel. A/B are no-ops.
            if (ev.button == Button::C) nav_ = Nav::Cursor;
            break;
    }
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
    rigServicesOff_ ^= (1u << static_cast<unsigned>(row));
    log_.push(LogEventType::ItemUsed,
              rigFeatureActive(row) ? "SERVICE STARTED" : "SERVICE STOPPED");
    dirty_ = true;
    markSaveDirty();
}

namespace {
// Is this an item the VAULT cashes in? Two Use-actions qualify: a sealed cache opens
// for a reward draw, and a Decryptogram buys a board. Both are spent HERE rather than
// pet-side, because what either pays out is player-level — which is also what
// itemUsable's two gate messages say from the other side.
bool vaultCashable(const ItemDef& d) {
    return d.use == ItemDef::Use::OpenContainer || d.use == ItemDef::Use::PlayCryptogram;
}
}  // namespace

int Game::vaultOwnedRows(const ItemDef** rows, int max) const {
    int n = 0;
    for (const auto& s : inventory_.stacks()) {
        if (s.qty <= 0) continue;
        const ItemDef* d = registry_.item(s.id);
        if (d && vaultCashable(*d) && n < max) rows[n++] = d;
    }
    // Tickets above caches, then richest draw first — a cache's rarity IS its payout
    // tier (content_items.cpp grades bits/draws/pool by it), so the top cache row is
    // always the best thing to open, and a Decryptogram outranks all of them because it
    // is the only row here that is a GAME rather than a draw. Inventory order is
    // insertion order, which says nothing a player cares about.
    std::stable_sort(rows, rows + n, [](const ItemDef* a, const ItemDef* b) {
        const bool at = a->use == ItemDef::Use::PlayCryptogram;
        const bool bt = b->use == ItemDef::Use::PlayCryptogram;
        if (at != bt) return at;
        return a->rarity > b->rarity;
    });
    return n;
}

void Game::cashVaultRow(const ItemDef& d) {
    if (d.use != ItemDef::Use::PlayCryptogram) { openSealedCache(d); return; }
    // Spend the ticket FIRST: the board cannot be walked out of, so there is no path
    // where it is consumed and no board follows, and taking it up front means a reboot
    // mid-quote costs the ticket rather than duplicating it.
    if (inventory_.count(d.id) <= 0) return;
    // Every quote won already? Then there is no first-solve prize left and the board is
    // replayed for Bits — the same deal the arcade cabinet offers, minus the cabinet.
    const int quote = pickQuote(quotesLeftToWin() ? QuotePick::Unsolved
                                                  : QuotePick::SolvedOnly);
    if (quote < 0) return;   // an empty pool: keep the ticket rather than eat it
    inventory_.remove(d.id, 1);
    char buf[28];
    std::snprintf(buf, sizeof(buf), "USED %s", d.displayName);
    log_.push(LogEventType::ItemUsed, buf);
    markSaveDirty();
    startCryptogram(quote, cryptogramExtraReveals(quoteTier(quote)));
}

void Game::onHackerVault(const ButtonEvent& ev) {
    // VAULT — cash in what a pet has no use for, OFF pet ITEMS. Build the list of
    // owned cashable stacks (caches to decrypt, Decryptograms to play); A cycles, C
    // backs to the carousel. B: unowned e resolves the focused row immediately
    // (cashVaultRow); owned, B becomes a tap/hold gesture ON A CACHE ROW — arm here and
    // resolve on the button-RELEASE edge (vaultBulkReleaseB, a short tap cashes just
    // the one) or on the hold crossing kBulkOpenHoldMs (tick() bulk-opens every cache
    // sharing the row's rarity instead).
    const ItemDef* rows[16];
    const int n = vaultOwnedRows(rows, 16);
    if (ev.button == Button::C) { nav_ = Nav::Cursor; return; }
    if (n == 0) return;                                       // empty state: A/B inert
    if (hackerVaultRow_ >= n) hackerVaultRow_ = 0;
    if (ev.button == Button::A) {
        hackerVaultRow_ = (hackerVaultRow_ + 1) % n;
    } else if (ev.button == Button::B) {
        // The hold gesture is a CACHE gesture — "open every cache of this rarity" means
        // nothing on a Decryptogram, and arming it there would swallow the tap that is
        // supposed to start a board. A ticket row therefore always resolves on the press.
        if (bulkOpenUnlocked() &&
            rows[hackerVaultRow_]->use == ItemDef::Use::OpenContainer) {
            bHeld_ = true;
            bDownMs_ = nowMs_;
        } else {
            cashVaultRow(*rows[hackerVaultRow_]);   // a reward draw, or a board
        }
    }
}

void Game::vaultBulkReleaseB() {
    if (!(face_ == Face::Hacker && nav_ == Nav::Submenu &&
          enteredHackerId() == HackerSlotId::Vault && bulkOpenUnlocked()))
        return;   // bHeld_ was armed elsewhere (e.g. the unrelated CFG hold) — no-op
    const ItemDef* rows[16];
    const int n = vaultOwnedRows(rows, 16);
    if (n == 0) return;
    if (hackerVaultRow_ >= n) hackerVaultRow_ = 0;
    cashVaultRow(*rows[hackerVaultRow_]);
    dirty_ = true;
    lastInputMs_ = nowMs_;
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

void Game::drawHackerHome(Framebuffer& fb, int cursor) const {
    fb.clear(palColor(Pal::PAPER));

    // Terminal scanline pass over the living area — the procedural "console" tell
    // (SCREEN_FX budget). Dim horizontal lines every few rows; grayscale-safe
    // (it only darkens, never re-colours, so the readout stays legible).
    for (int y = kLivingTop; y < kLivingBottom; y += 4)
        for (int x = 0; x < kActiveW; ++x)
            fb.blendPixel(x, y, palColor(Pal::TRACK), 90);

    int y = kLivingTop + 8;

    // HackerTag — the operator's handle, framed like a prompt banner.
    char tag[kHackerTagMax + 8];
    std::snprintf(tag, sizeof(tag), "// %s //", hackerTag_);
    drawText(fb, kMargin, y, tag, palColor(Pal::INK));
    y += kFontH + 6;

    // Crew line — the operator's allegiance, or the prompt toward the CREW slot.
    if (const CrewDef* c = activeCrew())
        drawText(fb, kMargin, y, c->displayName, palColor(Pal::INK));
    else
        drawText(fb, kMargin, y, "NO CREW - SEE CREW SLOT", palColor(Pal::INK_DIM));
    y += kFontH + 4;

    // Equipped Title — its real home is the Hacker face. NONE → greyed.
    if (equippedTitle_ >= 0) {
        char t[28];
        std::snprintf(t, sizeof(t), "* %s", equippedTitleName());
        drawText(fb, kMargin, y, t, palColor(Pal::ACCENT));
    } else {
        drawText(fb, kMargin, y, "* NO TITLE", palColor(Pal::INK_DIM));
    }
    y += kFontH + 6;

    fb.fillRect(kMargin, y, kActiveW - 2 * kMargin, 1, palColor(Pal::TRACK));
    y += 6;

    // Rank + NETS/SHAKES — the brag line the PROFILE viewer expands.
    char rank[28];
    std::snprintf(rank, sizeof(rank), "RANK %d %s", hackerRank_,
                  hackerRankTitle(hackerRank_));
    drawText(fb, kMargin, y, rank, palColor(Pal::INK));
    y += kFontH + 4;
    char nets[20];
    std::snprintf(nets, sizeof(nets), "NETS %d", networksSeen_);
    drawText(fb, kMargin, y, nets, palColor(Pal::INK_DIM));
    char shakes[20];
    std::snprintf(shakes, sizeof(shakes), "SHAKES %d", handshakesSeen_);
    drawText(fb, kActiveW - kMargin - textWidth(shakes), y, shakes,
             palColor(Pal::INK_DIM));
    y += kFontH + 4;
    // Queued sightings not yet flushed to SD — the "walk soon" pressure gauge.
    char queued[24];
    std::snprintf(queued, sizeof(queued), "QUEUED %d/%d", pendingNetworks(),
                  kPendingNetworkQueueCap);
    const Rgb565 queuedColor = pendingNetworks() >= kPendingNetworkQueueCap
                                    ? palColor(Pal::WARN)
                                    : palColor(Pal::INK_DIM);
    drawText(fb, kMargin, y, queued, queuedColor);
    y += kFontH + 6;

    // Blinking console cursor — the "idle terminal" motion.
    const bool on = ((beat_ / 2) & 1) == 0;
    drawText(fb, kMargin, y, on ? "> _" : ">", palColor(Pal::ACCENT));

    // Build the inaccessible mask from the same predicate B-dispatch asks, so a slot
    // can never look live and refuse to open (or the reverse).
    unsigned lockedMask = 0;
    for (int i = 0; i < kCarouselSlots; ++i)
        if (!hackerSlotAccessible(i)) lockedMask |= (1u << i);
    drawHackerCarousel(fb, cursor, uiMode_, beat_, mergeHubUnlocked(), lockedMask);
}

void Game::drawHackerSubmenu(Framebuffer& fb) const {
    if (enteredHackerId() == HackerSlotId::Shop && shopView_ == ShopView::ServiceInfo) {
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
        drawHeaderBand(fb, def.displayName, on ? "ON" : "OFF",
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
        return;
    }

    if (enteredHackerId() == HackerSlotId::Shop && shopView_ == ShopView::Services) {
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

        // Windowed on the VAULT's metrics — the same one-line-per-row list shape.
        constexpr int kSvcTop = 34, kSvcRowH = 22, kSvcVisibleRows = 7;
        int scrollTop = 0;
        if (n > kSvcVisibleRows) {
            if (sel >= scrollTop + kSvcVisibleRows) scrollTop = sel - kSvcVisibleRows + 1;
            scrollTop = std::max(0, std::min(scrollTop, n - kSvcVisibleRows));
        }
        for (int v = 0; v < kSvcVisibleRows && scrollTop + v < n; ++v) {
            const int i = scrollTop + v;
            const int row = rows[i];
            const int rowY = kSvcTop + v * kSvcRowH;
            const bool cur = i == sel;
            if (cur) drawRowCursor(fb, 2, rowY + 1, palColor(Pal::ACCENT));
            // A stopped service is stated twice — the word OFF and the dim ink — so the
            // list reads right in grayscale, where the colour alone says nothing.
            const bool on = rigFeatureActive(row);
            drawLabelValue(fb, kMargin, rowY, kRigUpgrades[row].displayName,
                           cur ? palColor(Pal::INK) : palColor(Pal::INK_DIM),
                           on ? "ON" : "OFF",
                           on ? palColor(Pal::ACCENT) : palColor(Pal::INK_DIM),
                           beat_, true);
        }
        if (n > kSvcVisibleRows) {   // UI_SCROLLBAR
            const int barX = kActiveW - 3;
            const int trackH = kSvcVisibleRows * kSvcRowH;
            fb.fillRect(barX, kSvcTop, 2, trackH, palColor(Pal::TRACK));
            const int thumbH = std::max(8, trackH * kSvcVisibleRows / n);
            const int thumbY = kSvcTop + trackH * scrollTop / n;
            fb.fillRect(barX, thumbY, 2, thumbH, palColor(Pal::INK_DIM));
        }
        fb.fillRect(0, kActiveH - 26, kActiveW, 1, palColor(Pal::TRACK));
        drawText(fb, kMargin, kActiveH - 20,
                 rigFeatureActive(rows[sel]) ? "B STOP" : "B START", palColor(Pal::ACCENT));
        // The other half of B, stated where the tap is: a HELD B reads the service out
        // rather than switching it.
        drawText(fb, kActiveW - kMargin - textWidth("HOLD - WHAT IT DOES"),
                 kActiveH - 20, "HOLD - WHAT IT DOES", palColor(Pal::INK_DIM));
        return;
    }

    if (enteredHackerId() == HackerSlotId::Shop) {
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
            drawText(fb, kMargin, 34, "ALL UPGRADES OWNED", palColor(Pal::INK_DIM));
            return;
        }

        int selPos = 0;   // hackerShopRow_'s position within visRows, for scroll math
        for (int i = 0; i < visN; ++i) if (visRows[i] == hackerShopRow_) { selPos = i; break; }

        int scrollTop = 0;
        if (visN > kRigVisibleRows) {
            if (selPos < scrollTop) scrollTop = selPos;
            if (selPos >= scrollTop + kRigVisibleRows) scrollTop = selPos - kRigVisibleRows + 1;
            scrollTop = std::max(0, std::min(scrollTop, visN - kRigVisibleRows));
        }

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
            const int rowY = 34 + v * kRigRowPitch;
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
                               palColor(Pal::INK), beat_, true);
                SpecBuilder head;
                head.flag("SWITCH AUTOMATION");
                drawSpecGrid(fb, kMargin, rowY + kFontH + 5, kActiveW - 2 * kMargin,
                             head.out.rows, head.out.count, kFontH + 3,
                             palColor(Pal::INK_DIM), palColor(Pal::ACCENT));
                if (sel) {
                    const int sy = 34 + visibleRows * kRigRowPitch;
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
                           beat_, true);

            const SpecRows spec = rigSpec(def, level);
            drawSpecGrid(fb, kMargin, rowY + kFontH + 5, kActiveW - 2 * kMargin,
                         spec.rows, spec.count, kFontH + 3, palColor(Pal::INK_DIM),
                         palColor(Pal::ACCENT));

            // Per-selected-row status line — the grayscale-safe buy reason (grammar).
            if (sel) {
                const char* status = afford ? "B TO BUY" : "NOT ENOUGH BITS";
                const int sy = 34 + visibleRows * kRigRowPitch;
                fb.fillRect(0, sy - 6, kActiveW, 1, palColor(Pal::TRACK));
                drawText(fb, kMargin, sy, status,
                         afford ? palColor(Pal::ACCENT) : palColor(Pal::INK_DIM));
            }
        }

        if (visN > kRigVisibleRows) {   // UI_SCROLLBAR
            const int barX = kActiveW - 3;
            const int trackTop = 34;
            const int trackH = kRigVisibleRows * kRigRowPitch;
            fb.fillRect(barX, trackTop, 2, trackH, palColor(Pal::TRACK));
            const int thumbH = std::max(8, trackH * kRigVisibleRows / visN);
            const int thumbY = trackTop + trackH * scrollTop / visN;
            fb.fillRect(barX, thumbY, 2, thumbH, palColor(Pal::INK_DIM));
        }
        return;
    }

    if (enteredHackerId() == HackerSlotId::Vault) {
        // VAULT — cash in what a pet has no use for, OFF pet ITEMS. One row per owned
        // cashable stack (name + count); A cycles, B spends it (cashVaultRow), C exits.
        drawHeaderBand(fb, "VAULT");

        const ItemDef* rows[16];
        const int n = vaultOwnedRows(rows, 16);
        if (n == 0) {
            drawText(fb, kMargin, 40, "NOTHING TO CASH IN", palColor(Pal::INK_DIM));
            return;
        }
        int sel = hackerVaultRow_;
        if (sel >= n) sel = 0;
        // Windowed exactly like the ITEMS list / rig shop, so a bag holding more
        // container kinds than fit scrolls instead of drawing under the hint band.
        constexpr int kVaultRowTop = 34, kVaultRowH = 22, kVaultVisibleRows = 7;
        int scrollTop = 0;
        if (n > kVaultVisibleRows) {
            if (sel >= scrollTop + kVaultVisibleRows)
                scrollTop = sel - kVaultVisibleRows + 1;
            scrollTop = std::max(0, std::min(scrollTop, n - kVaultVisibleRows));
        }
        for (int v = 0; v < kVaultVisibleRows && scrollTop + v < n; ++v) {
            const int i = scrollTop + v;
            const int rowY = kVaultRowTop + v * kVaultRowH;
            const bool s = i == sel;
            if (s) drawRowCursor(fb, 2, rowY + 1, palColor(Pal::ACCENT));
            // The cache glyph takes its rarity's colour. The tint states nothing new:
            // every container's own name carries the tier word ("Epic Cache"), and the
            // list is ordered by it — see core/ui/theme.h.
            if (const SpriteData* icon = itemIcon(registry_, rows[i]->id))
                drawSpriteTinted(fb, *icon, 0, kMargin, rowY - 7,
                                 rarityColor(rows[i]->rarity));
            char line[40];
            std::snprintf(line, sizeof(line), "%s x%d", rows[i]->displayName,
                          inventory_.count(rows[i]->id));
            drawText(fb, kMargin + 26, rowY, line,
                     s ? palColor(Pal::INK) : palColor(Pal::INK_DIM));
        }
        if (n > kVaultVisibleRows) {   // UI_SCROLLBAR
            const int barX = kActiveW - 3;
            const int trackH = kVaultVisibleRows * kVaultRowH;
            fb.fillRect(barX, kVaultRowTop, 2, trackH, palColor(Pal::TRACK));
            const int thumbH = std::max(8, trackH * kVaultVisibleRows / n);
            const int thumbY = kVaultRowTop + trackH * scrollTop / n;
            fb.fillRect(barX, thumbY, 2, thumbH, palColor(Pal::INK_DIM));
        }
        fb.fillRect(0, kActiveH - 26, kActiveW, 1, palColor(Pal::TRACK));
        // B does two different things here, so it says which: a cache is DECRYPTED for
        // a draw, a Decryptogram is PLAYED as a board.
        const bool ticket = rows[sel]->use == ItemDef::Use::PlayCryptogram;
        drawText(fb, kMargin, kActiveH - 20, ticket ? "B PLAY" : "B DECRYPT",
                 palColor(Pal::ACCENT));
        // e hint (exception rule) — only once owned, so an unowned
        // player's VAULT is pixel-identical to before this upgrade existed. Never on a
        // ticket row, which has no bulk gesture to advertise.
        if (bulkOpenUnlocked() && !ticket)
            drawText(fb, kActiveW - kMargin - textWidth("HOLD - OPEN ALL"),
                     kActiveH - 20, "HOLD - OPEN ALL", palColor(Pal::INK_DIM));
        return;
    }

    if (enteredHackerId() == HackerSlotId::Crew) {
        // CREW — its own screen, own file (game_crew.cpp): enlist in a crew + pick
        // the home network membership hangs off. The one sub-screen here that draws
        // its OWN header band: it is four views deep and the title (and its tint)
        // changes with the view, which only the screen knows.
        drawHackerCrew(fb);
        return;
    }

    if (enteredHackerId() == HackerSlotId::Peers) {
        // PEERS — its own screen, own file (game_peers.cpp): the operators this
        // device has met, live ones first.
        drawHeaderBand(fb, "PEERS");
        drawHackerPeers(fb);
        return;
    }

    if (enteredHackerId() == HackerSlotId::Link) {
        // LINK — its own screen, own file (game_pvp.cpp): challenge an operator who is
        // in range right now to a no-stakes 1v1.
        drawHeaderBand(fb, "LINK");
        drawHackerLink(fb);
        return;
    }

    if (enteredHackerId() == HackerSlotId::Merge) {
        // MERGE HUB — its own screen, own file (game_merge.cpp): combines owned
        // ingredient items into a rarer one. The roster outgrew the panel, so the body
        // windows it and the band carries the n/N the scroll would otherwise hide
        // (the same note drawItemsList's header prints, and only when it scrolls).
        char pos[12];
        pos[0] = '\0';
        if (kMergeRecipeCount > kMergeVisibleRows)
            std::snprintf(pos, sizeof(pos), "%d/%d", hackerMergeRow_ + 1,
                          kMergeRecipeCount);
        drawHeaderBand(fb, "MERGE HUB", pos);
        drawHackerMerge(fb);
        return;
    }

    // PROFILE — the read-only operator viewer (the STAT analog).
    drawHeaderBand(fb, "PROFILE");
    char tag[kHackerTagMax + 8];
    std::snprintf(tag, sizeof(tag), "// %s //", hackerTag_);
    drawText(fb, kMargin, 28, tag, palColor(Pal::INK));

    // Crew, then Title — a line each, not a pair sharing one (unaffiliated → greyed).
    // Both are content that grows to 20 characters against a 26-character line, so
    // side by side the widest pair leaves the crew a five-character window to scroll
    // through, which is no way to read the one word that says who you run with.
    const CrewDef* c = activeCrew();
    drawTextMarquee(fb, kMargin, 44, kActiveW - 2 * kMargin,
                    c ? c->displayName : "NO CREW",
                    c ? palColor(Pal::INK) : palColor(Pal::INK_DIM), beat_, true);
    char titleTag[24];
    std::snprintf(titleTag, sizeof(titleTag), "* %s",
                  equippedTitle_ >= 0 ? equippedTitleName() : "NO TITLE");
    drawTextMarquee(fb, kMargin, 58, kActiveW - 2 * kMargin, titleTag,
                    palColor(equippedTitle_ >= 0 ? Pal::ACCENT : Pal::INK_DIM), beat_,
                    true);
    fb.fillRect(0, 68, kActiveW, 1, palColor(Pal::TRACK));

    // The operator's own standing, on one 16px row pitch: rank · nets · shakes ·
    // sigils · species · queued · bandwidth, then the defended network last. Same shape
    // as the identity row above: the value owns the right end and the label yields to
    // it, so the widest rank title on the ladder cannot land on top of its own label.
    //
    // The pitch is 16 rather than the 18 the block used to run at, which is what buys
    // the eighth row without pushing HOME NET off the foot: the face is 8px, so 16 still
    // leaves a full cell of air between rows and the block reads as the same list.
    auto statRow = [&](int ry, const char* label, const char* value, Rgb565 vc) {
        drawLabelValue(fb, kMargin, ry, label, palColor(Pal::INK_DIM), value, vc,
                       beat_, true);
    };
    // "RANK", not "HACKER RANK": the label shares its line with a title of up to 16
    // characters, and the short form is what leaves room for the longest of them. It
    // is also what SYSTEM INFO calls the same readout.
    char rankVal[24];
    std::snprintf(rankVal, sizeof(rankVal), "R%d %s", hackerRank_,
                  hackerRankTitle(hackerRank_));
    statRow(76, "RANK", rankVal, palColor(Pal::ACCENT));
    char nets[12];
    std::snprintf(nets, sizeof(nets), "%d", networksSeen_);
    statRow(92, "NETS", nets, palColor(Pal::INK));
    // SHAKES is UNSPENT/LIFETIME, the same n/N shape QUEUED uses below. A shake is what
    // buys a sigil of the Cant (game_shibboleth.cpp), so the number that matters when
    // standing in front of a guardian is what is left to spend — and the lifetime tally
    // is still the brag it always was, right beside it.
    char shakes[16];
    std::snprintf(shakes, sizeof(shakes), "%d/%d", shakesUnspent(), handshakesSeen_);
    statRow(108, "SHAKES", shakes, palColor(Pal::INK));
    // SIGILS — how much of the guardians' language this device can read. Dim at zero,
    // because none of it is a real state rather than a missing one, and ACCENT once the
    // whole Cant is known: at that point no guardian will ever refuse the pet again.
    char sigils[16];
    std::snprintf(sigils, sizeof(sigils), "%d/%d", sigilsKnown(), kCantSigils);
    statRow(124, "SIGILS", sigils,
            palColor(sigilsKnown() == 0 ? Pal::INK_DIM
                     : sigilsKnown() >= kCantSigils ? Pal::ACCENT : Pal::INK));

    // SPECIES — distinct creatures this device has ever raised (Game::speciesRaised,
    // save v39). A collection stat rather than a radio one, but it belongs on the
    // operator face for the same reason the others do: it outlives every pet.
    char species[12];
    std::snprintf(species, sizeof(species), "%d", speciesRaised());
    statRow(140, "SPECIES", species, palColor(Pal::INK));

    // QUEUED — sightings waiting in RAM for a walk (EXPL Wi-Fi event) to flush
    // them into the SD-backed ledger; nears kPendingNetworkQueueCap = new
    // sightings start dropping, so this is the "walk your pet soon" signal.
    char queued[16];
    std::snprintf(queued, sizeof(queued), "%d/%d", pendingNetworks(),
                  kPendingNetworkQueueCap);
    const Rgb565 queuedColor = pendingNetworks() >= kPendingNetworkQueueCap
                                    ? palColor(Pal::WARN)
                                    : palColor(Pal::INK);
    statRow(156, "QUEUED", queued, queuedColor);

    // BANDWIDTH — the farming pool, shown n/N with its fill bar so the player
    // reads their farm budget on the operator face. The bar level is the grayscale channel.
    char bw[16];
    std::snprintf(bw, sizeof(bw), "%d/%d", bandwidth_, bandwidthMax());
    statRow(172, "BANDWIDTH", bw, palColor(Pal::INK));
    const float t = bandwidthMax() > 0
                        ? static_cast<float>(bandwidth_) / bandwidthMax() : 0.f;
    drawProgressBar(fb, kMargin, 186, kActiveW - 2 * kMargin, 7, t,
                    palColor(Pal::ACCENT));

    // HOME NET — the network this operator defends (set in CREW). Sits with the
    // identity stats because it's a property of the player, not the pet.
    statRow(202, "HOME NET", hasHomeNetwork() ? homeNetworkName_ : "NOT SET",
            hasHomeNetwork() ? palColor(Pal::INK) : palColor(Pal::INK_DIM));
}

}  // namespace mal
