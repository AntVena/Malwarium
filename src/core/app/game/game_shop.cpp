// game_shop.cpp (the EXPL storefronts): what a shop stocks, what it charges, and the
// buy flow (part of the Game unit split, core/app/game.h).
//
// Both storefronts an area owns run through here (its item shop and its mod shop),
// because they differ only in which registry resolves a listing id. The rows
// themselves belong to the area (content/areas/area_defs.h's AreaStorefrontDef);
// a visit to one is a StorefrontVisit (core/model/storefront_visit.h), and this unit
// opens it and is its till: what the player already holds of a row, why a buy is
// refused, and the purchase that spends Bits plus any item costs on the row.
//
// game_explore.cpp owns the walk: stepping, loot rolls, the sub-area boss
// ladder. A shop is reached FROM an explore event and is otherwise independent
// of it: nothing here reads the streak, the boss state or the step counter.
#include "core/app/game.h"

#include <cstdio>
#include <cstring>

#include "core/content/areas/area_defs.h"

namespace mal {

void Game::startShopEvent() {
    // The current sector's ITEM storefront (BYTE TO EAT / PIER-TO-PEER / MOOR-TO-MOOR).
    shopVisit_ = StorefrontVisit::opened(area(walk_.sector).shop, StorefrontVisit::Goods::Items);
    shop_.enter();
    walk_.eventBeat = 0;   // start the hands-off decision hold
    nav_ = Nav::Shop;
}

void Game::startModShopEvent() {
    // The current sector's MOD storefront (CHIP SHOP / PLUNDER PORT / MOOR TO MODS).
    shopVisit_ = StorefrontVisit::opened(area(walk_.sector).modShop, StorefrontVisit::Goods::Mods);
    shop_.enter();
    walk_.eventBeat = 0;
    nav_ = Nav::ModShop;
}

// The row reads are the visit's; with none open they read as empty.
bool Game::shopIsModShop() const { return shopVisit_ && shopVisit_->sellsMods(); }
const char* Game::shopStoreName() const { return shopVisit_ ? shopVisit_->storeName() : ""; }
int Game::shopListingCount() const { return shopVisit_ ? shopVisit_->listingCount() : 0; }
const char* Game::shopListingId(int i) const { return shopVisit_ ? shopVisit_->id(i) : ""; }
const char* Game::shopListingName(int i) const {
    return shopVisit_ ? shopVisit_->name(i, registry_) : "";
}
EffectText Game::shopListingDescription(int i) const {
    return shopVisit_ ? shopVisit_->description(i, registry_) : EffectText{};
}
int Game::shopListingStock(int i) const { return shopVisit_ ? shopVisit_->stock(i) : 0; }
int Game::shopListingBitsPrice(int i) const { return shopVisit_ ? shopVisit_->bitsPrice(i) : 0; }
int Game::shopListingCostCount(int i) const { return shopVisit_ ? shopVisit_->costCount(i) : 0; }
const char* Game::shopListingCostId(int i, int k) const {
    return shopVisit_ ? shopVisit_->costId(i, k) : "";
}
const char* Game::shopListingCostName(int i, int k) const {
    return shopVisit_ ? shopVisit_->costName(i, k, registry_) : "";
}
int Game::shopListingCostQty(int i, int k) const {
    return shopVisit_ ? shopVisit_->costQty(i, k) : 0;
}
int Game::shopListingCostHave(int i, int k) const {
    const char* id = shopListingCostId(i, k);
    return id[0] ? inventory_.count(id) : 0;
}

// --- The till: what the player holds and pays --------------------------------

const ShopListingDef* Game::shopListing(int i) const {
    return shopVisit_ ? shopVisit_->listing(i) : nullptr;
}

int Game::shopListingHeld(int i) const {
    const ShopListingDef* l = shopListing(i);
    if (!l) return 0;
    const char* id = l->id;
    // The MOD count is the un-equipped SPARE pool, the same number the cap bounds
    // (ModPool::grant): an installed mod left the pool, so it does not count
    // against the cap.
    return shopIsModShop() ? op_.modPool.countOf(id) : inventory_.count(id);
}

int Game::shopListingHeldCap(int i) const {
    if (!shopListing(i)) return 0;
    // Only mods have a ceiling: the MOD STORAGE rig row's allowance, the same one every
    // other grant site measures against. An inventory stack has none, so an item row
    // reports 0: the screen then shows no HAVE column, and the buy gate never reports
    // full.
    return shopIsModShop() ? modStorageCap() : 0;
}

const char* Game::shopRowRefusal(int i) const {
    const ShopListingDef* l = shopListing(i);
    if (!l) return "";
    const ShopListingDef& def = *l;
    if (shopVisit_->stock(i) <= 0) return "SOLD OUT";
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
    return shopListing(i) && shopRowRefusal(i) == nullptr;
}

void Game::buyShopItem(int i) {
    if (!shopRowBuyable(i)) return;   // inert: the sign says which reason
    const ShopListingDef& def = *shopListing(i);

    op_.bits -= def.bitsPrice;
    for (const auto& c : def.costs) if (c.itemId) inventory_.remove(c.itemId, c.qty);
    shopVisit_->sell(i);

    char buf[28];
    if (shopVisit_->sellsMods()) {
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
    shopVisit_.reset();
    std::snprintf(walk_.flavor, sizeof(walk_.flavor), "LEFT THE SHOP");
    returnToExplore();
}

}  // namespace mal
