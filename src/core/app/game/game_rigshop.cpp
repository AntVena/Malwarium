#include "core/app/game.h"

#include "core/app/game_rig_shop.h"

// game_rigshop.cpp: the Rig Shop account (purchases, grants, service switches) and the
// RigShopHost methods Game implements for the SHOP screen (game/rig_shop_screen.cpp).
// The account stays on Game because other features buy and grant rows too: a
// Decryptogram prize, an item, the arcade's gating.

namespace mal {

// Is this row on sale right now? One way it isn't: it's maxed/owned, with nothing left
// to buy. Every row in the table is an ordinary purchase — the one Hacker-face unlock
// that ISN'T is a MERGE HUB recipe, which is won off a Decryptogram and is not a rig
// row to gate. The single answer the A-cycle, the render loop and buyRigUpgrade all
// defer to, so the list can't show a row the cursor skips, or sell one it never drew.
bool Game::shopRowOffered(int i) const {
    if (i < 0 || i >= kRigUpgradeCount) return false;
    return rigLevel_[i] < kRigUpgrades[i].maxLevel;
}

void Game::rigServiceReleaseB() {
    if (!(face_ == Face::Hacker && nav_ == Nav::Submenu &&
          enteredHackerId() == HackerSlotId::Shop))
        return;   // bHeld_ was armed elsewhere (the VAULT or CFG holds)
    rigShop_.releaseB(*this);
}

void Game::leaveToCarousel() { nav_ = Nav::Cursor; }

void Game::armHoldB() {
    bHeld_ = true;
    bDownMs_ = nowMs_;
}

void Game::markDirty() { dirty_ = true; }

void Game::toggleRigService(int row) {
    if (!rigServiceOwned(row)) return;   // nothing bought here has a switch
    const uint32_t bit = 1u << static_cast<unsigned>(row);
    if (kRigUpgrades[row].askable) {
        // Three positions, stepped in the order the board reads them.
        switch (rigServiceMode(row)) {
            case ServiceMode::On:  rigServicesAsk_ |= bit; break;
            case ServiceMode::Ask: rigServicesAsk_ &= ~bit; rigServicesOff_ |= bit; break;
            case ServiceMode::Off: rigServicesOff_ &= ~bit; break;
        }
        log_.push(LogEventType::ItemUsed, rigServiceMode(row) == ServiceMode::On  ? "SERVICE SET YES"
                                        : rigServiceMode(row) == ServiceMode::Ask ? "SERVICE SET ASK"
                                                                                  : "SERVICE SET NO");
        dirty_ = true;
        markSaveDirty();
        return;
    }
    rigServicesOff_ ^= bit;
    log_.push(LogEventType::ItemUsed,
              rigFeatureActive(row) ? "SERVICE STARTED" : "SERVICE STOPPED");
    dirty_ = true;
    markSaveDirty();
}

void Game::buyRigUpgrade(int row) {
    if (!shopRowOffered(row)) return;   // MAXED/OWNED, or gated — inert
    const RigUpgradeDef& def = kRigUpgrades[row];
    const int cost = rigUpgradeCostFor(row);
    if (bits_ < cost) return;                       // NOT ENOUGH BITS — inert
    bits_ -= cost;
    grantRigLevel(row, 1);
    dirty_ = true;
}

void Game::grantRigLevel(int row, int levels) {
    if (row < 0 || row >= kRigUpgradeCount || levels <= 0) return;
    const RigUpgradeDef& def = kRigUpgrades[row];
    for (int i = 0; i < levels; ++i) {
        ++rigLevel_[row];
        // Bandwidth is the one row with an immediate effect: the freshly-owned pool
        // capacity is available to farm with right away, not just on the next regen
        // tick. Applied per level so a multi-level grant tops up by the whole amount.
        if (def.effectKind == RigEffectKind::GrantBandwidth) {
            bandwidth_ += def.effectMagnitude;
            if (bandwidth_ > bandwidthMax()) bandwidth_ = bandwidthMax();
        }
    }
    log_.push(LogEventType::ItemGained, def.logText);
    markSaveDirty();
}

}  // namespace mal
