// shop_screen.h: an explore storefront (Nav::Shop / Nav::ModShop), as a feature that owns
// its own state.
//
// The storefront list with the focused row's description and the till's answer. The
// screen holds its cursor. The visit (which storefront, its per-visit stock, item or
// mod) and the till are the game's (game_shop.cpp): the screen reads the visit itself
// and is handed its rows whole, resolved through whichever registry the visit uses.
#pragma once

#include "core/content/effect_text.h"   // EffectText: a listing's resolved description
#include "core/device/platform.h"

namespace mal {

class Framebuffer;
class StorefrontVisit;
struct Operator;
struct ShopRowView;

// What a storefront needs from the game. Game implements it; a test can too. Every `i`
// is bounds-clamped to "" / 0 for an out-of-range row.
class ShopHost {
public:
    virtual const Operator& operatorRecord() const = 0;   // the wallet
    virtual int screenBeat() const = 0;
    // The visit on screen (core/model/storefront_visit.h), or null with none.
    virtual const StorefrontVisit* storefrontVisit() const = 0;
    // Every row of the visit, resolved for drawing (names through the visit's registry,
    // and what the shopper already holds of each), up to `max`. Returns the count.
    virtual int shopRows(ShopRowView* out, int max) const = 0;
    virtual EffectText shopListingDescription(int i) const = 0;
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
    // A step is activity, so it holds the storefront open like any press.
    void stepList(ShopHost& host, int dir);
    void draw(Framebuffer& fb, const ShopHost& host) const;
    // The till's answer for the focused row, spelled out so it survives grayscale:
    // "SOLD OUT" / "STORAGE FULL" / "NOT ENOUGH BITS" / "NOT ENOUGH ITEMS" / "CAN BUY".
    // A verdict rather than a prompt: the hint band already says B BUY.
    const char* statusLine(const ShopHost& host) const;

    int row() const { return row_; }

private:
    int row_ = 0;
};

}  // namespace mal
