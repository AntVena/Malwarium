// shop_screen.h: an explore storefront (Nav::Shop / Nav::ModShop), as a feature that owns
// its own state.
//
// The storefront list with the focused row's description and the till's answer. The
// screen holds its cursor. The visit (which storefront, its per-visit stock, item or
// mod) and the till are the game's (game_shop.cpp), read through ShopHost's listing
// accessors, which resolve a row's id through whichever registry the visit uses.
#pragma once

#include "core/content/effect_text.h"   // EffectText: a listing's resolved description
#include "core/device/platform.h"

namespace mal {

class Framebuffer;
struct Operator;

// What a storefront needs from the game. Game implements it; a test can too. Every `i`
// is bounds-clamped to "" / 0 for an out-of-range row.
class ShopHost {
public:
    virtual const Operator& operatorRecord() const = 0;   // the wallet
    virtual int screenBeat() const = 0;
    virtual const char* shopStoreName() const = 0;
    virtual int shopListingCount() const = 0;
    virtual const char* shopListingName(int i) const = 0;
    virtual EffectText shopListingDescription(int i) const = 0;
    virtual int shopListingStock(int i) const = 0;         // remaining this visit
    virtual int shopListingBitsPrice(int i) const = 0;
    virtual int shopListingCostCount(int i) const = 0;
    virtual const char* shopListingCostName(int i, int k) const = 0;
    virtual int shopListingCostQty(int i, int k) const = 0;
    virtual int shopListingHeld(int i) const = 0;
    virtual int shopListingHeldCap(int i) const = 0;
    // Why row `i` cannot be bought, in the words the sign shows, or null when it can.
    // The till asks the same question, so the two cannot disagree.
    virtual const char* shopRowRefusal(int i) const = 0;

    virtual void buyShopItem(int i) = 0;   // inert when shopRowRefusal answers
    virtual void leaveShop() = 0;          // C: back to the walk
    // Any press restarts the hands-off hold, so an engaged shopper is not auto-left.
    virtual void holdShopOpen() = 0;

protected:
    ~ShopHost() = default;
};

class ShopScreen {
public:
    void enter() { row_ = 0; }
    void onButton(ShopHost& host, const ButtonEvent& ev);
    void draw(Framebuffer& fb, const ShopHost& host) const;
    // The till's answer for the focused row, spelled out so it survives grayscale:
    // "SOLD OUT" / "STORAGE FULL" / "NOT ENOUGH BITS" / "NOT ENOUGH ITEMS" / "B TO BUY".
    const char* statusLine(const ShopHost& host) const;

    int row() const { return row_; }

private:
    int row_ = 0;
};

}  // namespace mal
