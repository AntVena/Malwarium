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
        if (d && offerRelevant(*d, f) && itemUsable(*d, ItemUseFrom::Bag, gate)) out.push_back(d);
    }
    return out;
}

void Game::offerThenDive() {
    // AUTO DIVE GEAR at YES or NO answers DIVE PREP without a screen. ASK, and a rig
    // without the row, get DIVE PREP, which is how a player meets the loadout at all.
    if (!deepWebUnlocked()) return;
    if (op_.rig.serviceOwned(kRigRowAutoDiveGear) &&
        op_.rig.serviceMode(kRigRowAutoDiveGear) != ServiceMode::Ask) {
        startGearedDive();
        return;
    }
    openDivePrep();
}

void Game::startGearedDive() {
    // AUTO DIVE GEAR's position when owned: YES arms the best of each, NO arms nothing.
    // At ASK, or unowned, the standing loadout is the answer DIVE PREP last gave.
    if (op_.rig.serviceOwned(kRigRowAutoDiveGear)) {
        switch (op_.rig.serviceMode(kRigRowAutoDiveGear)) {
            case ServiceMode::On:
                armDivePick({DivePickKind::Best, nullptr}, DiveSlot::Bell);
                armDivePick({DivePickKind::Best, nullptr}, DiveSlot::Device);
                startDeepWebDive();
                return;
            case ServiceMode::Off: startDeepWebDive(); return;
            case ServiceMode::Ask: break;
        }
    }
    diveWithLoadout();
}

void Game::openDivePrep() {
    divePrep_.open(nav_);
    nav_ = Nav::DivePrep;
    dirty_ = true;
}

void Game::diveWithLoadout() {
    armDivePick(op_.dive.bell, DiveSlot::Bell);
    armDivePick(op_.dive.device, DiveSlot::Device);
    startDeepWebDive();   // sets nav_ itself
    dirty_ = true;
}

void Game::leaveDivePrep(Nav backTo) {
    // Nothing has started and nothing is spent; a pick changed here still stands.
    nav_ = backTo;
    dirty_ = true;
}

void Game::armDivePick(const DivePick& p, DiveSlot s) {
    if (const ItemDef* d = resolveDivePick(p, s)) useOfferedItem(*d);
}

int Game::diveSlotValue(const ItemEffect& e, DiveSlot s) const {
    switch (s) {
        case DiveSlot::Bell:
            if (e.kind == ItemEffect::Kind::SetDeepWebStartDepth) return e.magnitude;
            if (e.kind == ItemEffect::Kind::SetDeepWebStartDepthToBest) return pet_.bestDeepWebDepth;
            return 0;
        case DiveSlot::Device:
            return e.kind == ItemEffect::Kind::ArmDeepWebDepthMultiplier ? e.magnitude : 0;
    }
    return 0;
}

std::vector<const ItemDef*> Game::diveSlotItems(DiveSlot s) const {
    std::vector<const ItemDef*> out;
    for (const ItemDef* d : offerItems(OfferFor::Dive))
        for (const ItemEffect& e : d->effects)
            if (diveSlotValue(e, s) > 0) { out.push_back(d); break; }
    return out;
}

const ItemDef* Game::bestDiveItem(DiveSlot s) const {
    // The deepest start a held bell would give (a Checkpoint's is this pet's record), or
    // the strongest depth multiplier a held device would arm. diveSlotItems has already
    // dropped anything that would be refused, such as a bell shallower than the start
    // already armed.
    const ItemDef* best = nullptr;
    int top = 0;
    for (const ItemDef* d : diveSlotItems(s))
        for (const ItemEffect& e : d->effects)
            if (const int v = diveSlotValue(e, s); v > top) { top = v; best = d; }
    return best;
}

const ItemDef* Game::resolveDivePick(const DivePick& p, DiveSlot s) const {
    switch (p.kind) {
        case DivePickKind::None: return nullptr;
        case DivePickKind::Best: return bestDiveItem(s);
        case DivePickKind::Item:
            for (const ItemDef* d : diveSlotItems(s))
                if (d == p.item) return d;
            return nullptr;
    }
    return nullptr;
}

void Game::cycleDivePick(DiveSlot s) {
    // NONE, BEST, then each item the bag holds for the slot, in inventory order. A pick
    // whose item has run out is off the cycle, so it steps back to NONE.
    std::vector<DivePick> cycle = {{DivePickKind::None, nullptr}, {DivePickKind::Best, nullptr}};
    for (const ItemDef* d : diveSlotItems(s)) cycle.push_back({DivePickKind::Item, d});
    DivePick& cur = op_.dive.pick(s);
    size_t at = cycle.size() - 1;
    for (size_t i = 0; i < cycle.size(); ++i)
        if (cycle[i].kind == cur.kind && cycle[i].item == cur.item) at = i;
    cur = cycle[(at + 1) % cycle.size()];
    markSaveDirty();
    dirty_ = true;
}

void Game::toggleAutoRedive() {
    op_.dive.autoRedive = !op_.dive.autoRedive;
    markSaveDirty();
    dirty_ = true;
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
    itemOffer_.open(f, nav_);
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

void Game::leaveItemOffer(OfferFor f, Nav backTo) {
    // Backing out of a dive offer cancels the dive (nothing has started); backing out of
    // the egg's is a "not now", and the latch keeps it from asking again.
    nav_ = f == OfferFor::Dive ? backTo : Nav::Idle;
}

}  // namespace mal
