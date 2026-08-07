#include "core/app/game.h"

#include <algorithm>
#include <cstdio>

#include "tunables.h"
#include "core/app/game_internal.h"
#include "core/app/game_rig_shop.h"
#include "core/model/hacker_rank.h"
#include "core/render/canvas.h"
#include "core/render/font5x7.h"
#include "core/render/palette.h"
#include "core/render/sprite.h"
#include "core/ui/carousel.h"
#include "core/ui/items_screen.h"
#include "core/ui/layout.h"
#include "core/ui/theme.h"
#include "core/ui/widgets.h"
#include "generated/assets.h"

// game_hacker.cpp — the Hacker face (07, /HACK2/SHOP1).
//
// A+C at the top level flips PET ↔ HACKER (toggleFace). The Hacker face reuses the
// pet-side carousel + submenu contracts wholesale — the same Nav::Idle/Cursor/Submenu
// states and the same A/B/C cursor cycling — so the only bespoke code here is the
// hacker slot roster, the live L2 screens (PROFILE viewer + SHOP list + VAULT), and
// the terminal-reskin centre canvas. MERGE HUB's own L2 screen lives in
// game_merge.cpp (its rows are declared here via the shared RigRow enum,
// game_rig_shop.h), and CREW's (enlist + home network) in game_crew.cpp.
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

// Is this row on sale right now? Three ways it isn't: it's maxed/owned (nothing
// left to buy), its prerequisite row is still unbought (RigUpgradeDef::requiresRow —
// a recipe with no MERGE HUB to cook it in), or an item it wants the operator to have
// MET is one they've never held (requiresItems — a recipe for a dish they wouldn't
// recognise). The single answer the A-cycle, the render loop and buyRigUpgrade all
// defer to, so the list can't show a row the cursor skips, or sell one it never drew.
bool shopRowOffered(const Game& g, const int* rigLevel, int i) {
    if (i < 0 || i >= kRigUpgradeCount) return false;
    const RigUpgradeDef& d = kRigUpgrades[i];
    if (rigLevel[i] >= d.maxLevel) return false;
    if (d.requiresRow >= 0 && rigLevel[d.requiresRow] <= 0) return false;
    for (const char* need : d.requiresItems)
        if (need && !g.itemCollected(need)) return false;
    return true;
}

// The indices of every offered row, in table order — the SHOP list only ever
// shows rows with something to do.
int visibleShopRows(const Game& g, const int* rigLevel, int* out, int max) {
    int n = 0;
    for (int i = 0; i < kRigUpgradeCount && n < max; ++i)
        if (shopRowOffered(g, rigLevel, i)) out[n++] = i;
    return n;
}

// Step `row` forward through raw table order to the next offered row, wrapping;
// leaves `row` unchanged if nothing is on sale.
int nextVisibleShopRow(const Game& g, const int* rigLevel, int row) {
    int next = row;
    for (int step = 0; step < kRigUpgradeCount; ++step) {
        next = (next + 1) % kRigUpgradeCount;
        if (shopRowOffered(g, rigLevel, next)) return next;
    }
    return row;
}
}  // namespace

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
    return s.accessible;
}

void Game::enterHackerSubmenu() {
    // B on an inaccessible slot is inert (the slot exists but isn't usable yet).
    if (!hackerSlotAccessible(cursor_)) return;
    nav_ = Nav::Submenu;
    hackerShopRow_ = 0;
    hackerVaultRow_ = 0;
    hackerMergeRow_ = 0;
    crewRow_ = 0;
    crewNetPicker_ = false;
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

void Game::onHackerShop(const ButtonEvent& ev) {
    if (ev.button == Button::A) {
        hackerShopRow_ = nextVisibleShopRow(*this, rigLevel_, hackerShopRow_);  // skip unoffered
    } else if (ev.button == Button::B) {
        buyRigUpgrade(hackerShopRow_);
        // That purchase may have just taken the focused row out of the list — hop
        // to the next offered one so the cursor never rests on a row nobody drew.
        // (A purchase can also ADD rows: buying the MERGE HUB reveals its recipes,
        // which land below it, so the hop naturally arrives on the first of them.)
        if (!shopRowOffered(*this, rigLevel_, hackerShopRow_))
            hackerShopRow_ = nextVisibleShopRow(*this, rigLevel_, hackerShopRow_);
    } else if (ev.button == Button::C) {
        nav_ = Nav::Cursor;
    }
}

int Game::vaultOwnedRows(const ItemDef** rows, int max) const {
    int n = 0;
    for (const auto& s : inventory_.stacks()) {
        if (s.qty <= 0) continue;
        const ItemDef* d = registry_.item(s.id);
        if (d && d->use == ItemDef::Use::OpenContainer && n < max) rows[n++] = d;
    }
    // Richest draw first — a cache's rarity IS its payout tier (content_items.cpp
    // grades bits/draws/pool by it), so the top row is always the best thing to open.
    // Inventory order is insertion order, which says nothing a player cares about.
    std::stable_sort(rows, rows + n, [](const ItemDef* a, const ItemDef* b) {
        return a->rarity > b->rarity;
    });
    return n;
}

void Game::onHackerVault(const ButtonEvent& ev) {
    // VAULT — decrypt owned sealed caches here, OFF pet ITEMS. Build the list
    // of owned openable stacks; A cycles, C backs to the carousel. B: unowned e
    // decrypts the focused row immediately (openSealedCache, unchanged behaviour);
    // owned, B becomes a tap/hold gesture — arm here and resolve on the button-RELEASE
    // edge (vaultBulkReleaseB, a short tap opens just the one) or on the hold crossing
    // kBulkOpenHoldMs (tick() bulk-opens every cache sharing the row's rarity instead).
    const ItemDef* rows[16];
    const int n = vaultOwnedRows(rows, 16);
    if (ev.button == Button::C) { nav_ = Nav::Cursor; return; }
    if (n == 0) return;                                       // empty state: A/B inert
    if (hackerVaultRow_ >= n) hackerVaultRow_ = 0;
    if (ev.button == Button::A) {
        hackerVaultRow_ = (hackerVaultRow_ + 1) % n;
    } else if (ev.button == Button::B) {
        if (bulkOpenUnlocked()) {
            bHeld_ = true;
            bDownMs_ = nowMs_;
        } else {
            openSealedCache(*rows[hackerVaultRow_]);          // consumes + reward draw
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
    openSealedCache(*rows[hackerVaultRow_]);
    dirty_ = true;
    lastInputMs_ = nowMs_;
}

void Game::buyRigUpgrade(int row) {
    if (!shopRowOffered(*this, rigLevel_, row)) return;   // MAXED/OWNED, or gated — inert
    const RigUpgradeDef& def = kRigUpgrades[row];
    const int cost = rigUpgradeCostFor(row);
    if (bits_ < cost) return;                       // NOT ENOUGH BITS — inert
    bits_ -= cost;
    ++rigLevel_[row];
    // Bandwidth is the one row with an immediate effect: the freshly-bought pool
    // capacity is available to farm with right away, not just on the next regen tick.
    if (def.effectKind == RigEffectKind::GrantBandwidth) {
        bandwidth_ += def.effectMagnitude;
        if (bandwidth_ > bandwidthMax()) bandwidth_ = bandwidthMax();
    }
    log_.push(LogEventType::ItemGained, def.logText);
    markSaveDirty();
    dirty_ = true;
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

    drawHackerCarousel(fb, cursor, uiMode_, beat_, mergeHubUnlocked());
}

void Game::drawHackerSubmenu(Framebuffer& fb) const {
    if (enteredHackerId() == HackerSlotId::Shop) {
        // SHOP — the Rig Shop (game_rig_shop.h). Header carries the Bits wallet (the
        // storefront grammar). The list iterates kRigUpgrades generically, windowed to
        // kRigVisibleRows (mirrors drawItemsList/drawMovePicker).
        char bitsStr[16];
        std::snprintf(bitsStr, sizeof(bitsStr), "%d BITS", bits_);
        drawHeaderBand(fb, "SHOP", bitsStr, palColor(Pal::ACCENT));

        // Maxed/owned rows have nothing left to buy — filter them out entirely so
        // the list only ever shows purchasable rows (visRows is in table order).
        int visRows[kRigUpgradeCount];
        const int visN = visibleShopRows(*this, rigLevel_, visRows, kRigUpgradeCount);

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
        // is wider than the leftovers next to a right-aligned "16384 BITS" — the two
        // used to overprint each other.
        for (int v = 0; v < visibleRows && scrollTop + v < visN; ++v) {
            const int row = visRows[scrollTop + v];
            const int rowY = 34 + v * kRigRowPitch;
            const bool sel = hackerShopRow_ == row;
            if (sel) drawRowCursor(fb, 2, rowY + 1, palColor(Pal::ACCENT));

            const RigUpgradeDef& def = kRigUpgrades[row];
            const int level = rigLevel_[row];
            const int cost = rigUpgradeCostFor(row);
            const bool afford = bits_ >= cost;

            drawText(fb, kMargin, rowY, def.displayName,
                     sel ? palColor(Pal::INK) : palColor(Pal::INK_DIM));
            char rhs[16];
            std::snprintf(rhs, sizeof(rhs), "%d BITS", cost);
            drawText(fb, kActiveW - kMargin - textWidth(rhs), rowY, rhs,
                     afford ? palColor(Pal::INK) : palColor(Pal::INK_DIM));

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
        // VAULT — decrypt owned sealed caches OFF pet ITEMS. One row per owned
        // openable stack (name + count); A cycles, B decrypts (openSealedCache), C exits.
        drawHeaderBand(fb, "VAULT");

        const ItemDef* rows[16];
        const int n = vaultOwnedRows(rows, 16);
        if (n == 0) {
            drawText(fb, kMargin, 40, "NO SEALED CACHES", palColor(Pal::INK_DIM));
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
        drawText(fb, kMargin, kActiveH - 20, "B DECRYPT", palColor(Pal::ACCENT));
        // e hint (exception rule) — only once owned, so an unowned
        // player's VAULT is pixel-identical to before this upgrade existed.
        if (bulkOpenUnlocked())
            drawText(fb, kActiveW - kMargin - textWidth("HOLD - OPEN ALL"),
                     kActiveH - 20, "HOLD - OPEN ALL", palColor(Pal::INK_DIM));
        return;
    }

    if (enteredHackerId() == HackerSlotId::Crew) {
        // CREW — its own screen, own file (game_crew.cpp): enlist in a crew + pick
        // the home network membership hangs off.
        drawHeaderBand(fb, "CREW");
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
        // MERGE HUB — its own screen, own file (game_merge.cpp): combines two owned
        // ingredient items into a rarer one.
        drawHeaderBand(fb, "MERGE HUB");
        drawHackerMerge(fb);
        return;
    }

    // PROFILE — the read-only operator viewer (the STAT analog).
    drawHeaderBand(fb, "PROFILE");
    char tag[kHackerTagMax + 8];
    std::snprintf(tag, sizeof(tag), "// %s //", hackerTag_);
    drawText(fb, kMargin, 28, tag, palColor(Pal::INK));

    // Crew + Title identity row (unaffiliated → greyed).
    const CrewDef* c = activeCrew();
    drawText(fb, kMargin, 44, c ? c->displayName : "NO CREW",
             c ? palColor(Pal::INK) : palColor(Pal::INK_DIM));
    if (equippedTitle_ >= 0) {
        char t[24];
        std::snprintf(t, sizeof(t), "* %s", equippedTitleName());
        drawText(fb, kActiveW - kMargin - textWidth(t), 44, t, palColor(Pal::ACCENT));
    }
    fb.fillRect(0, 60, kActiveW, 1, palColor(Pal::TRACK));

    // The operator's own standing, on one 18px row pitch: rank · nets · shakes ·
    // species · queued · bandwidth, then the defended network last.
    auto statRow = [&](int ry, const char* label, const char* value, Rgb565 vc) {
        drawText(fb, kMargin, ry, label, palColor(Pal::INK_DIM));
        drawText(fb, kActiveW - kMargin - textWidth(value), ry, value, vc);
    };
    char rankVal[24];
    std::snprintf(rankVal, sizeof(rankVal), "R%d %s", hackerRank_,
                  hackerRankTitle(hackerRank_));
    statRow(70, "HACKER RANK", rankVal, palColor(Pal::ACCENT));
    char nets[12];
    std::snprintf(nets, sizeof(nets), "%d", networksSeen_);
    statRow(88, "NETS", nets, palColor(Pal::INK));
    char shakes[12];
    std::snprintf(shakes, sizeof(shakes), "%d", handshakesSeen_);
    statRow(106, "SHAKES", shakes, palColor(Pal::INK));

    // SPECIES — distinct creatures this device has ever raised (Game::speciesRaised,
    // save v39). A collection stat rather than a radio one, but it belongs on the
    // operator face for the same reason the others do: it outlives every pet.
    char species[12];
    std::snprintf(species, sizeof(species), "%d", speciesRaised());
    statRow(124, "SPECIES", species, palColor(Pal::INK));

    // QUEUED — sightings waiting in RAM for a walk (EXPL Wi-Fi event) to flush
    // them into the SD-backed ledger; nears kPendingNetworkQueueCap = new
    // sightings start dropping, so this is the "walk your pet soon" signal.
    char queued[16];
    std::snprintf(queued, sizeof(queued), "%d/%d", pendingNetworks(),
                  kPendingNetworkQueueCap);
    const Rgb565 queuedColor = pendingNetworks() >= kPendingNetworkQueueCap
                                    ? palColor(Pal::WARN)
                                    : palColor(Pal::INK);
    statRow(142, "QUEUED", queued, queuedColor);

    // BANDWIDTH — the farming pool, shown n/N with its fill bar so the player
    // reads their farm budget on the operator face. The bar level is the grayscale channel.
    char bw[16];
    std::snprintf(bw, sizeof(bw), "%d/%d", bandwidth_, bandwidthMax());
    statRow(160, "BANDWIDTH", bw, palColor(Pal::INK));
    const float t = bandwidthMax() > 0
                        ? static_cast<float>(bandwidth_) / bandwidthMax() : 0.f;
    drawProgressBar(fb, kMargin, 174, kActiveW - 2 * kMargin, 7, t,
                    palColor(Pal::ACCENT));

    // HOME NET — the network this operator defends (set in CREW). Sits with the
    // identity stats because it's a property of the player, not the pet.
    statRow(194, "HOME NET", hasHomeNetwork() ? homeNetworkName_ : "NOT SET",
            hasHomeNetwork() ? palColor(Pal::INK) : palColor(Pal::INK_DIM));
}

}  // namespace mal
