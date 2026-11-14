#include "core/app/game.h"

#include <cstdio>

#include "core/app/game_rig_shop.h"

// game_offer.cpp (the ITEM OFFER): a rare-moment item, put in front of the player at
// the moment it matters.
//
// Some items do nothing for days and then matter for one press: the bells and
// Deep-Learning devices only at the top of a DeepWeb Dive, the Boot Accelerator only
// while an egg incubates. Left in the bag they are forgotten at exactly that press, so
// the game asks instead. What is offered is decided by the item's own row (its effect
// kinds, its Use) and by itemUsable, the same gate the ITEMS screen uses, so an item
// that would be refused there (a shallower bell over a deeper one, an accelerator on a
// shell that is already cracking) is never offered here either.

namespace mal {

bool Game::offerRelevant(const ItemDef& d, OfferFor f) const {
    switch (f) {
        case OfferFor::Egg:
            return d.use == ItemDef::Use::DecryptEgg;
        case OfferFor::Dive:
            for (const ItemEffect& e : d.effects) {
                if (e.kind == ItemEffect::Kind::SetDeepWebStartDepth ||
                    e.kind == ItemEffect::Kind::ArmDeepWebDepthMultiplier)
                    return true;
                // A Checkpoint Bell on a pet that has never been down would start the
                // dive at 0: the item spent for nothing, so it isn't put forward.
                if (e.kind == ItemEffect::Kind::SetDeepWebStartDepthToBest)
                    return pet_.bestDeepWebDepth > 0;
            }
            return false;
    }
    return false;
}

std::vector<const ItemDef*> Game::offerItems(OfferFor f) const {
    // Inventory order, so the rows are stable while the list is open and testable.
    std::vector<const ItemDef*> out;
    for (const auto& s : inventory_.stacks()) {
        if (s.qty <= 0) continue;
        const ItemDef* d = registry_.item(s.id);
        const char* gate = "";
        if (d && offerRelevant(*d, f) && itemUsable(*d, gate)) out.push_back(d);
    }
    return out;
}

void Game::offerThenDive() {
    // AUTO DIVE GEAR decides how the dive's items are handled once it is owned: YES
    // arms them without a screen, NO skips them without one, ASK is the DIVE PREP list.
    // A rig without the row is asked, which is how a player meets the items at all.
    if (!deepWebUnlocked()) return;
    if (op_.rig.serviceOwned(kRigRowAutoDiveGear)) {
        switch (op_.rig.serviceMode(kRigRowAutoDiveGear)) {
            case ServiceMode::On:  autoUseDiveGear(); startDeepWebDive(); return;
            case ServiceMode::Off: startDeepWebDive(); return;
            case ServiceMode::Ask: break;
        }
    }
    openItemOffer(OfferFor::Dive);
}

void Game::autoUseDiveGear() {
    // The deepest start a held bell would give (a Checkpoint's is this pet's record),
    // and the strongest depth multiplier a held device would arm. A dive uses at most
    // one of each. offerItems has already dropped anything that would be refused, such
    // as a bell shallower than the start already armed.
    const ItemDef* bell = nullptr;
    const ItemDef* device = nullptr;
    int deepest = 0, strongest = 1;
    for (const ItemDef* d : offerItems(OfferFor::Dive))
        for (const ItemEffect& e : d->effects) {
            const int depth = e.kind == ItemEffect::Kind::SetDeepWebStartDepth ? e.magnitude
                            : e.kind == ItemEffect::Kind::SetDeepWebStartDepthToBest
                                ? pet_.bestDeepWebDepth : 0;
            if (depth > deepest) { deepest = depth; bell = d; }
            if (e.kind == ItemEffect::Kind::ArmDeepWebDepthMultiplier &&
                e.magnitude > strongest) {
                strongest = e.magnitude;
                device = d;
            }
        }
    if (bell) useOfferedItem(*bell);
    if (device) useOfferedItem(*device);
}

void Game::useOfferedItem(const ItemDef& d) {
    if (d.use == ItemDef::Use::DecryptEgg) { useBootAccelerator(d); return; }
    inventory_.remove(d.id, 1);
    applyItemEffects(d);
    char buf[28];
    std::snprintf(buf, sizeof(buf), "USED %s", d.displayName);
    log_.push(LogEventType::ItemUsed, buf);
    markSaveDirty();
}

void Game::openItemOffer(OfferFor f) {
    if (f == OfferFor::Egg) pet_.eggOfferMade = true;   // asked once per egg, answered or not
    if (offerItems(f).empty()) { finishItemOffer(f); return; }
    offerBackNav_ = nav_;
    itemOffer_.open(f);
    nav_ = Nav::ItemOffer;
    dirty_ = true;
}

void Game::finishItemOffer(OfferFor f) {
    switch (f) {
        case OfferFor::Dive: startDeepWebDive(); break;   // sets nav_ itself
        case OfferFor::Egg:  nav_ = Nav::Idle; break;
    }
    dirty_ = true;
}

void Game::leaveItemOffer(OfferFor f) {
    // Backing out of a dive offer cancels the dive (nothing has started); backing out of
    // the egg's is a "not now", and the latch keeps it from asking again.
    nav_ = f == OfferFor::Dive ? offerBackNav_ : Nav::Idle;
}

}  // namespace mal
