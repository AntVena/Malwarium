#include "core/app/game/vault_screen.h"

#include <algorithm>
#include <cstdio>

#include "tunables.h"
#include "core/app/game_internal.h"
#include "core/content/registry.h"
#include "core/model/inventory.h"
#include "core/render/canvas.h"
#include "core/render/font.h"
#include "core/render/palette.h"
#include "core/ui/items_screen.h"   // itemIcon
#include "core/ui/layout.h"
#include "core/ui/theme.h"
#include "core/ui/widgets.h"

namespace mal {

namespace {
// Is this an item the VAULT cashes in? Two Use-actions qualify: a sealed cache opens
// for a reward draw, and a Decryptogram buys a board. Both are spent here because
// their payout is player-level; itemUsable's two gate messages point here.
bool vaultCashable(const ItemDef& d) {
    return d.use == ItemDef::Use::OpenContainer || d.use == ItemDef::Use::PlayCryptogram;
}
}  // namespace

int VaultScreen::rows(const VaultHost& host, const ItemDef** out, int max) {
    int n = 0;
    for (const auto& s : host.inventory().stacks()) {
        if (s.qty <= 0) continue;
        const ItemDef* d = host.content().item(s.id);
        if (d && vaultCashable(*d) && n < max) out[n++] = d;
    }
    // Tickets above caches, then richest draw first: a cache's rarity IS its payout
    // tier (its row grades bits/draws/pool by it), so the top cache row is
    // always the best thing to open, and a Decryptogram outranks all of them because it
    // is the only row here that is a GAME rather than a draw. Inventory order is
    // insertion order, which says nothing a player cares about.
    std::stable_sort(out, out + n, [](const ItemDef* a, const ItemDef* b) {
        const bool at = a->use == ItemDef::Use::PlayCryptogram;
        const bool bt = b->use == ItemDef::Use::PlayCryptogram;
        if (at != bt) return at;
        return a->rarity > b->rarity;
    });
    return n;
}

const ItemDef* VaultScreen::focused(const VaultHost& host) {
    const ItemDef* r[kMaxRows];
    const int n = rows(host, r, kMaxRows);
    if (n == 0) return nullptr;
    if (row_ >= n) row_ = 0;
    return r[row_];
}

void VaultScreen::stepList(const VaultHost& host, int dir) {
    const ItemDef* r[kMaxRows];
    row_ = stepRow(row_, rows(host, r, kMaxRows), dir);
}

void VaultScreen::onButton(VaultHost& host, const ButtonEvent& ev) {
    if (ev.button == Button::C) { host.leaveToCarousel(); return; }
    const ItemDef* d = focused(host);
    if (!d) return;                                           // empty state: A/B inert
    if (ev.button == Button::A) {
        stepList(host, +1);
    } else if (ev.button == Button::B) {
        // The hold opens every cache of one rarity, so it is armed only on cache rows. On
        // a Decryptogram it would swallow the tap that starts a board. A ticket row
        // therefore always resolves on the press.
        if (host.bulkOpenUnlocked() && d->use == ItemDef::Use::OpenContainer) host.armHoldB();
        else host.cashVaultRow(*d);                           // a reward draw, or a board
    }
}

bool VaultScreen::holdB(VaultHost& host, uint32_t heldMs) {
    if (!host.bulkOpenUnlocked() || heldMs < kBulkOpenHoldMs) return false;
    // Caches only: the row can move under a held B, and a Decryptogram cannot be
    // bulk-opened.
    if (const ItemDef* d = focused(host); d && d->use == ItemDef::Use::OpenContainer)
        host.openAllCachesOfRarity(*d);
    return true;
}

void VaultScreen::releaseB(VaultHost& host) {
    if (!host.bulkOpenUnlocked()) return;
    if (const ItemDef* d = focused(host)) host.cashVaultRow(*d);
}

void VaultScreen::draw(Framebuffer& fb, const VaultHost& host) const {
    drawHeaderBand(fb, "VAULT");

    const ItemDef* r[kMaxRows];
    const int n = rows(host, r, kMaxRows);
    if (n == 0) {
        drawText(fb, kMargin, 40, "NOTHING TO CASH IN", palColor(Pal::INK_DIM));
        return;
    }
    const int sel = row_ < n ? row_ : 0;
    const int scrollTop = listScrollTop(sel, n, kHackerListRows);
    for (int v = 0; v < kHackerListRows && scrollTop + v < n; ++v) {
        const int i = scrollTop + v;
        const int rowY = kHackerListTop + v * kHackerListRowH;
        const bool s = i == sel;
        if (s) drawRowCursor(fb, 2, rowY + 1, palColor(Pal::ACCENT));
        // The cache glyph takes its rarity's colour. The tint is extra: every
        // container's name carries the tier word ("Epic Cache"), and the list is
        // ordered by it. See core/ui/theme.h.
        if (const SpriteData* icon = itemIcon(host.content(), r[i]->id))
            drawSpriteTinted(fb, *icon, 0, kMargin, rowY - 7, rarityColor(r[i]->rarity));
        char line[40];
        std::snprintf(line, sizeof(line), "%s x%d", r[i]->displayName,
                      host.inventory().count(r[i]->id));
        drawText(fb, kMargin + 26, rowY, line,
                 s ? palColor(Pal::INK) : palColor(Pal::INK_DIM));
    }
    drawScrollbar(fb, kHackerListTop, kHackerListRows * kHackerListRowH, n, scrollTop,
                  kHackerListRows);
    // B does two different things here, so it says which: a cache is DECRYPTED for
    // a draw, a Decryptogram is PLAYED as a board.
    const bool ticket = r[sel]->use == ItemDef::Use::PlayCryptogram;
    // The bulk-open hint (exception rule, VISUAL_LANGUAGE §4.2), only once owned, so
    // an unowned player is never shown a gesture they don't have. Never on a ticket
    // row, which has no bulk gesture to advertise.
    drawHackerHint(fb, host.bulkOpenUnlocked() && !ticket ? "HOLD - OPEN ALL" : nullptr,
                   ticket ? "B PLAY" : "B DECRYPT");
}

void VaultScreen::onReveal(VaultHost& host, const ButtonEvent& ev) {
    if (ev.button == Button::B || ev.button == Button::C) {
        reveal_.reset();
        host.closeDetail();
    }
}

void VaultScreen::onBulkReveal(VaultHost& host, const ButtonEvent& ev) {
    if (!bulk_) return;
    if (ev.button == Button::A) {
        bulk_->row = stepRow(bulk_->row, static_cast<int>(bulk_->tally.size()), +1);
    } else if (ev.button == Button::B || ev.button == Button::C) {
        bulk_.reset();
        host.closeDetail();
    }
}

void VaultScreen::drawReveal(Framebuffer& fb, const VaultHost& host) const {
    if (!reveal_ || !reveal_->cache) return;
    const CacheReveal& r = *reveal_;
    YieldItem items[2];
    for (int i = 0; i < r.itemCount && i < 2; ++i) {
        items[i].def = r.items[i];
        items[i].icon = items[i].def ? itemIcon(host.content(), items[i].def->id) : nullptr;
    }
    drawCacheYield(fb, *r.cache, r.bits, items, r.itemCount);
}

void VaultScreen::drawBulkReveal(Framebuffer& fb, const VaultHost& host) const {
    if (!bulk_ || !bulk_->cache) return;
    const BulkReveal& r = *bulk_;
    BulkYieldRow rows[kMaxRows];
    const int n = std::min(static_cast<int>(r.tally.size()), kMaxRows);
    for (int i = 0; i < n; ++i) {
        rows[i].def = r.tally[i].def;
        rows[i].icon = r.tally[i].def ? itemIcon(host.content(), r.tally[i].def->id) : nullptr;
        rows[i].count = r.tally[i].count;
    }
    BulkYieldView v;
    v.cachesOpened = r.cachesOpened;
    v.bits = r.bits;
    v.rows = rows;
    v.count = n;
    v.cursor = r.row;
    v.beat = host.screenBeat();
    drawBulkYield(fb, *r.cache, v);
}

}  // namespace mal
