#include "core/app/game.h"

#include "core/app/game_rig_shop.h"

// game_rigshop.cpp: buying, granting and switching on the Rig Shop account
// (Operator::rig), and the RigShopHost methods Game implements for the SHOP screen
// (game/rig_shop_screen.cpp). These stay on Game because a purchase moves more than the
// account: the wallet, the live Bandwidth pool, the log and the save.

namespace mal {

void Game::leaveToCarousel() { nav_ = Nav::Cursor; }

void Game::armHoldB() {
    bHeld_ = true;
    bDownMs_ = nowMs_;
}

void Game::markDirty() { dirty_ = true; }

void Game::toggleRigService(int row) {
    if (!op_.rig.toggleService(row)) return;   // nothing bought here has a switch
    const char* line = "SERVICE STOPPED";
    if (kRigUpgrades[row].askable) {
        const ServiceMode m = op_.rig.serviceMode(row);
        line = m == ServiceMode::On  ? "SERVICE SET YES"
             : m == ServiceMode::Ask ? "SERVICE SET ASK"
                                     : "SERVICE SET NO";
    } else if (op_.rig.featureActive(row)) {
        line = "SERVICE STARTED";
    }
    log_.push(LogEventType::ItemUsed, line);
    dirty_ = true;
    markSaveDirty();
}

void Game::buyRigUpgrade(int row) {
    if (!op_.rig.offered(row)) return;   // MAXED/OWNED, or gated: inert
    const int cost = op_.rig.costFor(row);
    if (op_.bits < cost) return;   // NOT ENOUGH BITS: inert
    op_.bits -= cost;
    grantRigLevel(row, 1);
    dirty_ = true;
}

void Game::grantRigLevel(int row, int levels) {
    if (row < 0 || row >= kRigUpgradeCount || levels <= 0) return;
    const RigUpgradeDef& def = kRigUpgrades[row];
    for (int i = 0; i < levels; ++i) {
        ++op_.rig.level[row];
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
