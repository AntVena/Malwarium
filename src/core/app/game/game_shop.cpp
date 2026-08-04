// game_shop.cpp — the EXPL storefronts: what a shop stocks, what it charges, and the
// buy flow (part of the Game unit split, core/app/game.h).
//
// Both storefronts an area owns run through here — its item shop and its mod shop —
// because the difference between them is which registry resolves a listing id, not
// how a shop behaves. The rows themselves belong to the area
// (content/areas/area_defs.h's AreaStorefrontDef); this unit owns the visit: the
// per-visit stock that refills on entry, the accessors the screen reads a row
// through, and the purchase that spends Bits plus any item costs on the row.
//
// Split out of game_explore.cpp, which owns the walk — stepping, loot rolls, the
// sub-area boss ladder. A shop is reached FROM an explore event and is otherwise
// independent of it: nothing here reads the streak, the boss state or the step
// counter.
#include "core/app/game.h"

#include <cstdio>
#include <cstring>

#include "tunables.h"
#include "core/content/areas/area_defs.h"
#include "core/content/content_tables.h"

namespace mal {

namespace {
// Bounds-clamped: an out-of-range row (or no storefront entered yet) reads as
// empty/zero rather than reading past the array — callers loop 0..listingCount()
// which already stays in range, but the accessors stay safe on their own.
bool shopRowInRange(const AreaStorefrontDef* front, int i) {
    return front && i >= 0 && i < front->listingCount;
}
bool shopCostInRange(const AreaStorefrontDef* front, int i, int k) {
    return shopRowInRange(front, i) && k >= 0 && k < kMaxShopCostItems &&
           front->listings[i].costs[k].itemId != nullptr;
}
}  // namespace

void Game::startShopEvent() {
    // The current sector's ITEM storefront (BYTE TO EAT / PIER-TO-PEER /
    // MOOR-TO-MOOR). Refill this visit's per-listing stock from the area's OWN rows.
    shopFront_ = &area(exploreSector_).shop;
    shopIsModShop_ = false;
    for (int i = 0; i < shopFront_->listingCount && i < kMaxShopListings; ++i)
        shopStockLeft_[i] = shopFront_->listings[i].stock;
    shopCursor_ = 0;
    exploreEventBeat_ = 0;   // start the hands-off decision hold
    nav_ = Nav::Shop;
}

void Game::startModShopEvent() {
    // The current sector's MOD storefront (CHIP SHOP / PLUNDER PORT / MOOR TO MODS)
    // — same shape as startShopEvent, just resolving through the area's modShop and
    // the mod registry (shopIsModShop_ picks that at render/buy time).
    shopFront_ = &area(exploreSector_).modShop;
    shopIsModShop_ = true;
    for (int i = 0; i < shopFront_->listingCount && i < kMaxShopListings; ++i)
        shopStockLeft_[i] = shopFront_->listings[i].stock;
    shopCursor_ = 0;
    exploreEventBeat_ = 0;
    nav_ = Nav::ModShop;
}

const char* Game::shopListingId(int i) const {
    return shopRowInRange(shopFront_, i) ? shopFront_->listings[i].id : "";
}

const char* Game::shopListingName(int i) const {
    if (!shopRowInRange(shopFront_, i)) return "";
    const char* id = shopFront_->listings[i].id;
    if (shopIsModShop_) {
        const ModDef* m = registry_.mod(id);
        return m ? m->displayName : id;
    }
    const ItemDef* it = registry_.item(id);
    return it ? it->displayName : id;
}

EffectText Game::shopListingDescription(int i) const {
    if (!shopRowInRange(shopFront_, i)) return {};
    const char* id = shopFront_->listings[i].id;
    if (shopIsModShop_) {
        const ModDef* m = registry_.mod(id);
        return m ? effectText(*m) : EffectText{};
    }
    const ItemDef* it = registry_.item(id);
    return it ? effectText(*it) : EffectText{};
}

int Game::shopListingStock(int i) const {
    return shopRowInRange(shopFront_, i) ? shopStockLeft_[i] : 0;
}
int Game::shopListingBitsPrice(int i) const {
    return shopRowInRange(shopFront_, i) ? shopFront_->listings[i].bitsPrice : 0;
}
int Game::shopListingCostCount(int i) const {
    if (!shopRowInRange(shopFront_, i)) return 0;
    int n = 0;
    for (const auto& c : shopFront_->listings[i].costs) if (c.itemId) ++n;
    return n;
}
const char* Game::shopListingCostId(int i, int k) const {
    return shopCostInRange(shopFront_, i, k) ? shopFront_->listings[i].costs[k].itemId : "";
}
const char* Game::shopListingCostName(int i, int k) const {
    if (!shopCostInRange(shopFront_, i, k)) return "";
    const ItemDef* it = registry_.item(shopFront_->listings[i].costs[k].itemId);
    return it ? it->displayName : shopFront_->listings[i].costs[k].itemId;
}
int Game::shopListingCostQty(int i, int k) const {
    return shopCostInRange(shopFront_, i, k) ? shopFront_->listings[i].costs[k].qty : 0;
}
int Game::shopListingCostHave(int i, int k) const {
    return shopCostInRange(shopFront_, i, k)
               ? inventory_.count(shopFront_->listings[i].costs[k].itemId)
               : 0;
}

const char* Game::shopStatusLine() const {
    // Spelled out so the buy-availability reason survives grayscale (dual-coding).
    if (!shopRowInRange(shopFront_, shopCursor_)) return "";
    const ShopListingDef& def = shopFront_->listings[shopCursor_];
    if (shopStockLeft_[shopCursor_] <= 0) return "SOLD OUT";
    if (bits_ < def.bitsPrice) return "NOT ENOUGH BITS";
    for (const auto& c : def.costs)
        if (c.itemId && inventory_.count(c.itemId) < c.qty) return "NOT ENOUGH ITEMS";
    return "B TO BUY";
}

void Game::buyShopItem() {
    if (!shopRowInRange(shopFront_, shopCursor_)) return;
    const int i = shopCursor_;
    if (shopStockLeft_[i] <= 0) return;
    const ShopListingDef& def = shopFront_->listings[i];
    if (bits_ < def.bitsPrice) return;
    for (const auto& c : def.costs)
        if (c.itemId && inventory_.count(c.itemId) < c.qty) return;  // inert — a cost is short

    bits_ -= def.bitsPrice;
    for (const auto& c : def.costs) if (c.itemId) inventory_.remove(c.itemId, c.qty);
    --shopStockLeft_[i];

    char buf[28];
    if (shopIsModShop_) {
        // A shop buy is a guaranteed acquisition, not a lucky drop — grant at
        // reqLevel 0 (freely equippable) rather than rolling grantRolledMod's
        // per-instance gate.
        const ModDef* m = registry_.mod(def.id);
        loadout_.grant(def.id, /*reqLevel=*/0);
        std::snprintf(buf, sizeof(buf), "BOUGHT %s", m ? m->displayName : def.id);
    } else {
        inventory_.add(def.id, 1);
        const ItemDef* it = registry_.item(def.id);
        std::snprintf(buf, sizeof(buf), "BOUGHT %s", it ? it->displayName : def.id);
    }
    log_.push(LogEventType::ItemGained, buf);
    markSaveDirty();
}

void Game::onShop(const ButtonEvent& ev) {
    // B buys the SELECTED row (while stock + wallet + item costs allow); C leaves
    // back to the idle habitat. A cycles the selection through every listing
    // (wraps) — a no-op, not an exit, on a single-listing storefront so a stray
    // "Next" press can't drop the player out mid-shop. Any press restarts the
    // hands-off hold so an engaged shopper isn't auto-left.
    exploreEventBeat_ = 0;
    if (!shopFront_) return;
    if (ev.button == Button::A) {
        if (shopFront_->listingCount > 0)
            shopCursor_ = (shopCursor_ + 1) % shopFront_->listingCount;
    } else if (ev.button == Button::B) {
        buyShopItem();
    } else if (ev.button == Button::C) {
        std::snprintf(exploreFlavor_, sizeof(exploreFlavor_), "LEFT THE SHOP");
        returnToExplore();
    }
}

}  // namespace mal
