#include "core/app/game/shop_screen.h"

#include "core/app/operator.h"
#include "core/model/storefront_visit.h"
#include "core/ui/expl_screen.h"
#include "core/ui/layout.h"

namespace mal {

void ShopScreen::onButton(ShopHost& host, const ButtonEvent& ev) {
    // A cycles the selection through every listing (wraps): on a single-listing
    // storefront it does nothing, so a stray Next press never exits the shop.
    host.holdShopOpen();
    if (ev.button == Button::A) {
        stepList(host, +1);
    } else if (ev.button == Button::B) {
        host.buyShopItem(row_);
    } else if (ev.button == Button::C) {
        host.leaveShop();
    }
}

void ShopScreen::stepList(ShopHost& host, int dir) {
    host.holdShopOpen();
    const StorefrontVisit* visit = host.storefrontVisit();
    row_ = stepRow(row_, visit ? visit->listingCount() : 0, dir);
}

const char* ShopScreen::statusLine(const ShopHost& host) const {
    const StorefrontVisit* visit = host.storefrontVisit();
    if (!visit || row_ < 0 || row_ >= visit->listingCount()) return "";
    const char* why = host.shopRowRefusal(row_);
    return why ? why : "CAN BUY";
}

void ShopScreen::draw(Framebuffer& fb, const ShopHost& host) const {
    // Plain rows, so expl_screen.cpp's drawShop stays registry-agnostic.
    ShopRowView rows[kMaxShopListings];
    const int n = host.shopRows(rows, kMaxShopListings);
    const StorefrontVisit* visit = host.storefrontVisit();
    const EffectText description = host.shopListingDescription(row_);   // outlives the draw
    StorefrontView v;
    v.storeName = visit ? visit->storeName() : "";
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
