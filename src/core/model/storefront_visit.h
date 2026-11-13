// storefront_visit.h: one visit to an area storefront, as a run.
//
// An area owns two counters (its item shop and its mod shop, AreaStorefrontDef), and a
// visit is one of them stocked afresh: every row's ShopListingDef::stock, spent down
// by what is bought and gone when the visit ends. The visit answers what a row is and
// costs; whether the player can pay, and the paying, are the till's (game_shop.cpp),
// which reads the wallet and the bag.
#pragma once

#include <cstdint>

#include "core/content/areas/area_defs.h"
#include "core/content/effect_text.h"

namespace mal {

class ContentRegistry;

class StorefrontVisit {
public:
    // Which registry resolves a listing's id.
    enum class Goods : uint8_t { Items, Mods };

    static StorefrontVisit opened(const AreaStorefrontDef& front, Goods goods);

    const char* storeName() const { return front_->name; }
    int listingCount() const { return front_->listingCount; }
    bool sellsMods() const { return goods_ == Goods::Mods; }
    // Row `i`, or null out of range. Every row read below is bounds-clamped the same way,
    // to "" or 0, so a caller can loop 0..listingCount() without checking.
    const ShopListingDef* listing(int i) const;

    const char* id(int i) const;
    const char* name(int i, const ContentRegistry& reg) const;
    // The goods' own effect text with its magnitudes substituted in.
    EffectText description(int i, const ContentRegistry& reg) const;
    int stock(int i) const;   // left this visit
    int bitsPrice(int i) const;
    int costCount(int i) const;   // the non-empty extra item costs
    const char* costId(int i, int k) const;
    const char* costName(int i, int k, const ContentRegistry& reg) const;
    int costQty(int i, int k) const;

    // One of row `i` off the shelf, once the till has taken payment.
    void sell(int i);

private:
    bool costInRange(int i, int k) const;

    const AreaStorefrontDef* front_ = nullptr;
    Goods goods_ = Goods::Items;
    int stock_[kMaxShopListings] = {};
};

}  // namespace mal
