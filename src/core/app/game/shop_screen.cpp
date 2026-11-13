#include "core/app/game/shop_screen.h"

#include <algorithm>

#include "core/app/operator.h"
#include "core/ui/expl_screen.h"
#include "core/ui/layout.h"

namespace mal {

void ShopScreen::onButton(ShopHost& host, const ButtonEvent& ev) {
    // A cycles the selection through every listing (wraps): on a single-listing
    // storefront it does nothing, so a stray Next press never exits the shop.
    host.holdShopOpen();
    if (ev.button == Button::A) {
        row_ = stepRow(row_, host.shopListingCount(), +1);
    } else if (ev.button == Button::B) {
        host.buyShopItem(row_);
    } else if (ev.button == Button::C) {
        host.leaveShop();
    }
}

const char* ShopScreen::statusLine(const ShopHost& host) const {
    if (row_ < 0 || row_ >= host.shopListingCount()) return "";
    const char* why = host.shopRowRefusal(row_);
    return why ? why : "B TO BUY";
}

void ShopScreen::draw(Framebuffer& fb, const ShopHost& host) const {
    // Every listing resolved into plain ShopRowView rows, so expl_screen.cpp's drawShop
    // stays registry-agnostic.
    const int n = std::min(host.shopListingCount(), kMaxShopListings);
    ShopRowView rows[kMaxShopListings];
    for (int i = 0; i < n; ++i) {
        rows[i].name = host.shopListingName(i);
        rows[i].bitsPrice = host.shopListingBitsPrice(i);
        rows[i].stock = host.shopListingStock(i);
        rows[i].costCount = std::min(host.shopListingCostCount(i), kMaxShopCostItems);
        for (int k = 0; k < rows[i].costCount; ++k) {
            rows[i].costName[k] = host.shopListingCostName(i, k);
            rows[i].costQty[k] = host.shopListingCostQty(i, k);
        }
        rows[i].held = host.shopListingHeld(i);
        rows[i].heldCap = host.shopListingHeldCap(i);
    }
    const EffectText description = host.shopListingDescription(row_);   // outlives the draw
    StorefrontView v;
    v.storeName = host.shopStoreName();
    v.walletBits = host.operatorRecord().bits;
    v.rows = rows;
    v.rowCount = n;
    v.cursor = row_;
    v.selectedDescription = description.c_str();
    v.statusLine = statusLine(host);
    v.beat = host.screenBeat();
    drawShop(fb, v);
}

}  // namespace mal
