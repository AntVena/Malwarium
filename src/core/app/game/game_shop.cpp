// game_shop.cpp (the EXPL storefronts): what a shop stocks, what it charges, and the
// buy flow (part of the Game unit split, core/app/game.h).
//
// Both storefronts an area owns run through here (its item shop and its mod shop),
// because they differ only in which registry resolves a listing id. The rows
// themselves belong to the area (content/areas/area_defs.h's AreaStorefrontDef);
// this unit owns the visit: the per-visit stock that refills on entry, the
// accessors the screen reads a row through, and the purchase that spends Bits plus
// any item costs on the row.
//
// game_explore.cpp owns the walk: stepping, loot rolls, the sub-area boss
// ladder. A shop is reached FROM an explore event and is otherwise independent
// of it: nothing here reads the streak, the boss state or the step counter.
#include "core/app/game.h"

#include <cstdio>
#include <cstring>

#include "core/content/areas/area_defs.h"

namespace mal {

namespace {
// Bounds-clamped: an out-of-range row (or no storefront entered yet) reads as
// empty or zero. Callers loop 0..listingCount() which already stays in range,
// but the accessors stay safe on their own.
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
    shopFront_ = &area(walk_.sector).shop;
    shopIsModShop_ = false;
    for (int i = 0; i < shopFront_->listingCount && i < kMaxShopListings; ++i)
        shopStockLeft_[i] = shopFront_->listings[i].stock;
    shop_.enter();
    walk_.eventBeat = 0;   // start the hands-off decision hold
    nav_ = Nav::Shop;
}

void Game::startModShopEvent() {
    // The current sector's MOD storefront (CHIP SHOP / PLUNDER PORT / MOOR TO MODS):
    // same shape as startShopEvent, just resolving through the area's modShop and
    // the mod registry (shopIsModShop_ picks that at render/buy time).
    shopFront_ = &area(walk_.sector).modShop;
    shopIsModShop_ = true;
    for (int i = 0; i < shopFront_->listingCount && i < kMaxShopListings; ++i)
        shopStockLeft_[i] = shopFront_->listings[i].stock;
    shop_.enter();
    walk_.eventBeat = 0;
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

int Game::shopListingHeld(int i) const {
    if (!shopRowInRange(shopFront_, i)) return 0;
    const char* id = shopFront_->listings[i].id;
    // The MOD count is the un-equipped SPARE pool, the same number the cap bounds
    // (ModPool::grant): an installed mod left the pool, so it does not count
    // against the cap.
    return shopIsModShop_ ? op_.modPool.countOf(id) : inventory_.count(id);
}

int Game::shopListingHeldCap(int i) const {
    if (!shopRowInRange(shopFront_, i)) return 0;
    // Only mods have a ceiling: the MOD STORAGE rig row's allowance, the same one every
    // other grant site measures against. An inventory stack has none, so an item row
    // reports 0: the screen then shows no HAVE column, and the buy gate never reports
    // full.
    return shopIsModShop_ ? modStorageCap() : 0;
}

const char* Game::shopRowRefusal(int i) const {
    if (!shopRowInRange(shopFront_, i)) return "";
    const ShopListingDef& def = shopFront_->listings[i];
    if (shopStockLeft_[i] <= 0) return "SOLD OUT";
    // Storage before the wallet: a full mod pool is the one refusal Bits cannot answer,
    // so naming a price the player could meet would send them off to earn Bits that
    // still would not buy this. The buy is refused up front, because ModSlots::grant
    // would return false after the Bits were spent.
    const int cap = shopListingHeldCap(i);
    if (cap > 0 && shopListingHeld(i) >= cap) return "STORAGE FULL";
    if (op_.bits < def.bitsPrice) return "NOT ENOUGH BITS";
    for (const auto& c : def.costs)
        if (c.itemId && inventory_.count(c.itemId) < c.qty) return "NOT ENOUGH ITEMS";
    return nullptr;
}

bool Game::shopRowBuyable(int i) const {
    return shopRowInRange(shopFront_, i) && shopRowRefusal(i) == nullptr;
}

void Game::buyShopItem(int i) {
    if (!shopRowBuyable(i)) return;   // inert: the sign says which reason
    const ShopListingDef& def = shopFront_->listings[i];

    op_.bits -= def.bitsPrice;
    for (const auto& c : def.costs) if (c.itemId) inventory_.remove(c.itemId, c.qty);
    --shopStockLeft_[i];

    char buf[28];
    if (shopIsModShop_) {
        // A shop buy runs through the same storage cap a drop does: the row is
        // storage-gated at the till (shopRowRefusal's STORAGE FULL), so reaching here
        // means there was room for it.
        const ModDef* m = registry_.mod(def.id);
        op_.modPool.grant(def.id, modStorageCap());
        std::snprintf(buf, sizeof(buf), "BOUGHT %s", m ? m->displayName : def.id);
    } else {
        inventory_.add(def.id, 1);
        const ItemDef* it = registry_.item(def.id);
        std::snprintf(buf, sizeof(buf), "BOUGHT %s", it ? it->displayName : def.id);
    }
    log_.push(LogEventType::ItemGained, buf);
    markSaveDirty();
}

void Game::leaveShop() {
    std::snprintf(walk_.flavor, sizeof(walk_.flavor), "LEFT THE SHOP");
    returnToExplore();
}

}  // namespace mal
