#include "core/model/storefront_visit.h"

#include "core/content/registry.h"

namespace mal {

StorefrontVisit StorefrontVisit::opened(const AreaStorefrontDef& front, Goods goods) {
    StorefrontVisit v;
    v.front_ = &front;
    v.goods_ = goods;
    for (int i = 0; i < front.listingCount && i < kMaxShopListings; ++i)
        v.stock_[i] = front.listings[i].stock;
    return v;
}

const ShopListingDef* StorefrontVisit::listing(int i) const {
    return i >= 0 && i < front_->listingCount ? &front_->listings[i] : nullptr;
}

bool StorefrontVisit::costInRange(int i, int k) const {
    return listing(i) && k >= 0 && k < kMaxShopCostItems &&
           front_->listings[i].costs[k].itemId != nullptr;
}

const char* StorefrontVisit::id(int i) const {
    const ShopListingDef* l = listing(i);
    return l ? l->id : "";
}

const char* StorefrontVisit::name(int i, const ContentRegistry& reg) const {
    const ShopListingDef* l = listing(i);
    if (!l) return "";
    if (sellsMods()) {
        const ModDef* m = reg.mod(l->id);
        return m ? m->displayName : l->id;
    }
    const ItemDef* it = reg.item(l->id);
    return it ? it->displayName : l->id;
}

EffectText StorefrontVisit::description(int i, const ContentRegistry& reg) const {
    const ShopListingDef* l = listing(i);
    if (!l) return {};
    if (sellsMods()) {
        const ModDef* m = reg.mod(l->id);
        return m ? effectText(*m) : EffectText{};
    }
    const ItemDef* it = reg.item(l->id);
    return it ? effectText(*it) : EffectText{};
}

int StorefrontVisit::stock(int i) const { return listing(i) ? stock_[i] : 0; }

int StorefrontVisit::bitsPrice(int i) const {
    const ShopListingDef* l = listing(i);
    return l ? l->bitsPrice : 0;
}

int StorefrontVisit::costCount(int i) const {
    const ShopListingDef* l = listing(i);
    if (!l) return 0;
    int n = 0;
    for (const auto& c : l->costs) if (c.itemId) ++n;
    return n;
}

const char* StorefrontVisit::costId(int i, int k) const {
    return costInRange(i, k) ? front_->listings[i].costs[k].itemId : "";
}

const char* StorefrontVisit::costName(int i, int k, const ContentRegistry& reg) const {
    if (!costInRange(i, k)) return "";
    const char* id = front_->listings[i].costs[k].itemId;
    const ItemDef* it = reg.item(id);
    return it ? it->displayName : id;
}

int StorefrontVisit::costQty(int i, int k) const {
    return costInRange(i, k) ? front_->listings[i].costs[k].qty : 0;
}

void StorefrontVisit::sell(int i) {
    if (listing(i)) --stock_[i];
}

}  // namespace mal
