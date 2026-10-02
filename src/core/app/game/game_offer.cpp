#include "core/app/game.h"

#include <cstdio>

#include "core/app/game_rig_shop.h"

#include "core/content/effect_text.h"
#include "core/render/canvas.h"
#include "core/render/font.h"
#include "core/render/framebuffer.h"
#include "core/render/palette.h"
#include "core/ui/layout.h"
#include "core/ui/widgets.h"

// game_offer.cpp — the ITEM OFFER: a rare-moment item, put in front of the player at
// the moment it matters.
//
// Some items do nothing for days and then matter for one press: the bells and
// Deep-Learning devices only at the top of a DeepWeb Dive, the Boot Accelerator only
// while an egg incubates. Left in the bag they are forgotten at exactly that press, so
// the game asks instead. What is offered is decided by the item's own row (its effect
// kinds, its Use) and by itemUsable, the same gate the ITEMS screen uses — so an item
// that would be refused there (a shallower bell over a deeper one, an accelerator on a
// shell that is already cracking) is never offered here either.

namespace mal {

bool Game::offerRelevant(const ItemDef& d) const {
    switch (offerFor_) {
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
                    return bestDeepWebDepth_ > 0;
            }
            return false;
    }
    return false;
}

std::vector<const ItemDef*> Game::offerItems() const {
    // Inventory order, so the rows are stable while the list is open and testable.
    std::vector<const ItemDef*> out;
    for (const auto& s : inventory_.stacks()) {
        if (s.qty <= 0) continue;
        const ItemDef* d = registry_.item(s.id);
        const char* gate = "";
        if (d && offerRelevant(*d) && itemUsable(*d, gate)) out.push_back(d);
    }
    return out;
}

void Game::offerThenDive() {
    // AUTO DIVE GEAR decides how the dive's items are handled once it is owned: YES
    // arms them without a screen, NO skips them without one, ASK is the DIVE PREP list.
    // A rig without the row is asked, which is how a player meets the items at all.
    if (!deepWebUnlocked()) return;
    if (rigServiceOwned(kRigRowAutoDiveGear)) {
        switch (rigServiceMode(kRigRowAutoDiveGear)) {
            case ServiceMode::On:  autoUseDiveGear(); startDeepWebDive(); return;
            case ServiceMode::Off: startDeepWebDive(); return;
            case ServiceMode::Ask: break;
        }
    }
    openItemOffer(OfferFor::Dive);
}

void Game::autoUseDiveGear() {
    // The deepest start a held bell would give (a Checkpoint's is this pet's record),
    // and the strongest depth multiplier a held device would arm — one of each, which is
    // everything a dive can use. offerItems has already dropped anything that would be
    // refused, such as a bell shallower than the start already armed.
    offerFor_ = OfferFor::Dive;
    const ItemDef* bell = nullptr;
    const ItemDef* device = nullptr;
    int deepest = 0, strongest = 1;
    for (const ItemDef* d : offerItems())
        for (const ItemEffect& e : d->effects) {
            const int depth = e.kind == ItemEffect::Kind::SetDeepWebStartDepth ? e.magnitude
                            : e.kind == ItemEffect::Kind::SetDeepWebStartDepthToBest
                                ? bestDeepWebDepth_ : 0;
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
    offerFor_ = f;
    if (f == OfferFor::Egg) eggOfferMade_ = true;   // asked once per egg, answered or not
    if (offerItems().empty()) { finishItemOffer(); return; }
    offerBackNav_ = nav_;
    offerRow_ = 0;
    nav_ = Nav::ItemOffer;
    dirty_ = true;
}

void Game::finishItemOffer() {
    switch (offerFor_) {
        case OfferFor::Dive: startDeepWebDive(); break;   // sets nav_ itself
        case OfferFor::Egg:  nav_ = Nav::Idle; break;
    }
    dirty_ = true;
}

void Game::onItemOffer(const ButtonEvent& ev) {
    // Rebuilt every press: using an item can take its row away (the last of a stack,
    // or a bell that makes every shallower bell pointless), so a cursor kept across
    // presses would point at the wrong row.
    const auto items = offerItems();
    const int rows = static_cast<int>(items.size()) + 1;   // + the carry-on row
    if (offerRow_ >= rows) offerRow_ = rows - 1;
    if (ev.button == Button::A) {
        offerRow_ = (offerRow_ + 1) % rows;
    } else if (ev.button == Button::B) {
        if (offerRow_ == rows - 1) { finishItemOffer(); return; }
        useOfferedItem(*items[offerRow_]);
        // Nothing else left worth using: carry straight on rather than leave the
        // player on a list that only says "go".
        if (offerItems().empty()) { finishItemOffer(); return; }
        offerRow_ = 0;
    } else if (ev.button == Button::C) {
        // Backing out of a dive offer cancels the dive (nothing has started); backing
        // out of the egg's is a "not now", and the latch keeps it from asking again.
        nav_ = offerFor_ == OfferFor::Dive ? offerBackNav_ : Nav::Idle;
    }
    dirty_ = true;
}

void Game::drawItemOfferScreen(Framebuffer& fb) const {
    const bool dive = offerFor_ == OfferFor::Dive;
    drawHeaderBand(fb, dive ? "DIVE PREP" : "EGG");
    drawText(fb, kMargin, 28, dive ? "USE ONE BEFORE YOU DIVE?" : "SPEED UP THE EGG?",
             palColor(Pal::INK_DIM));

    const auto items = offerItems();
    const int rows = static_cast<int>(items.size()) + 1;
    const int cursor = offerRow_ < rows ? offerRow_ : rows - 1;
    // Compact rows, windowed: there are six dive items, and the focused row's
    // description below is what tells the player which one they want.
    constexpr int kTop = 44;
    constexpr int kPitch = kFontH + 6;
    constexpr int kMaxRows = 5;
    int scrollTop = cursor >= kMaxRows ? cursor - kMaxRows + 1 : 0;
    for (int v = 0; v < kMaxRows && scrollTop + v < rows; ++v) {
        const int r = scrollTop + v;
        const int y = kTop + v * kPitch;
        if (r == cursor) {
            fb.fillRect(4, y - 3, kActiveW - 8, kPitch, palColor(Pal::TRACK));
            drawRowCursor(fb, kMargin, y, palColor(Pal::ACCENT));
        }
        char label[32];
        if (r < static_cast<int>(items.size()))
            std::snprintf(label, sizeof(label), "%s x%d", items[r]->displayName,
                          inventory_.count(items[r]->id));
        else
            std::snprintf(label, sizeof(label), "%s", dive ? "DIVE NOW" : "NOT NOW");
        drawText(fb, 24, y, label, palColor(r == cursor ? Pal::INK : Pal::INK_DIM));
    }

    // The focused item's own description — the row says what it is, this says what it
    // does — under a rule, down to the hint band.
    const int descTop = kTop + kMaxRows * kPitch + 6;
    if (cursor < static_cast<int>(items.size())) {
        const EffectText t = effectText(*items[cursor]);
        const int maxLines = (kActiveH - kHintBandH - descTop) / kLineH;
        drawTextWrapped(fb, kMargin, descTop, kActiveW - 2 * kMargin, t.c_str(),
                        palColor(Pal::INK_DIM), kLineH, maxLines);
    } else {
        drawTextWrapped(fb, kMargin, descTop, kActiveW - 2 * kMargin,
                        dive ? "Start the dive with nothing armed." :
                               "Leave the egg to hatch on its own clock.",
                        palColor(Pal::INK_DIM), kLineH, 3);
    }
    drawHintBand(fb, cursor == rows - 1 ? "A NEXT  B GO  C BACK"
                                        : "A NEXT  B USE  C BACK");
}

}  // namespace mal
