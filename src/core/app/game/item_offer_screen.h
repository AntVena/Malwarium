// item_offer_screen.h: the ITEM OFFER (Nav::ItemOffer), a rare-moment item put in front of
// the player at the moment it matters.
//
// A list of the held items that would do something right now, plus a last row that
// carries on without them. B on an item uses it and keeps the list open (a bell and a
// device both go in before a dive); B on the last row carries on; C backs out. The
// screen holds what it is offering for and its cursor. Which items qualify, using one
// and carrying on are the game's (game_offer.cpp), through ItemOfferHost.
#pragma once

#include <cstdint>
#include <vector>

#include "core/device/platform.h"

namespace mal {

class Framebuffer;
struct ItemDef;

// The moment an offer is for: the top of a DeepWeb Dive (the bells and Deep-Learning
// devices) or an egg that has started incubating (the Boot Accelerator).
enum class OfferFor : uint8_t { Dive, Egg };

// What the offer needs from the game. Game implements it; a test can too.
class ItemOfferHost {
public:
    // The held items worth offering for `f` right now, in inventory order.
    virtual std::vector<const ItemDef*> offerItems(OfferFor f) const = 0;
    virtual int heldCount(const char* itemId) const = 0;
    virtual void useOfferedItem(const ItemDef& item) = 0;
    virtual void finishItemOffer(OfferFor f) = 0;   // carry on with what was paused
    virtual void leaveItemOffer(OfferFor f) = 0;    // C: back out without carrying on

protected:
    ~ItemOfferHost() = default;
};

class ItemOfferScreen {
public:
    void open(OfferFor f) { for_ = f; row_ = 0; }
    void onButton(ItemOfferHost& host, const ButtonEvent& ev);
    void draw(Framebuffer& fb, const ItemOfferHost& host) const;

private:
    OfferFor for_ = OfferFor::Dive;
    int row_ = 0;
};

}  // namespace mal
