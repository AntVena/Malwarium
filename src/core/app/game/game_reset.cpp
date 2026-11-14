// game_reset.cpp: the factory reset, in its two halves: the per-pet state (resetToHatch) and
// everything the device keeps across pets (wipeDeviceProgress).

#include "core/app/game.h"

#include "tunables.h"

namespace mal {

void Game::wipeDeviceProgress() {
    // Everything the device knows that OUTLIVES a pet is the Operator record, and every
    // radio consent (out of the box, all three are off). resetToHatch() clears the
    // per-pet half; together they are a device with no history. Not cleared: the device
    // preferences (brightness, UI mode, sound), which are settings rather than progress,
    // and the SD-backed network and peer ledgers (core/net/network_ledger.h,
    // peer_ledger.h), which are files on a card the operator can remove. The SHAKES
    // handshake ledger is cleared, truncating its card file, because it is the dedup set
    // for the record's handshake count.
    op_ = Operator{};
    handshakeLedger_.clear();
    achBannerWire_ = -1;
    achBannerCount_ = 0;
    setAuditMode(AuditMode::Off);      // seals capture with the scan, per the ladder
    setApEnabled(false);
    setLinkEnabled(false);
}

void Game::resetToHatch() {
    // RESET PET (also the dev shortcut: host key / device gesture / tests): replace the
    // Pet record with a fresh egg and leave the Operator alone. A new pet, same player:
    // the tag, the wallet, the spare mods, the ARCH rack, the tallies, the 'Pedia and the
    // rank all stand. WIPE EVERYTHING replaces the Operator first (wipeDeviceProgress).
    // The sample mods are the device's first-egg kit, which layEgg keeps.
    pet_ = Pet{};
    pet_.mods = ModSlots::starting();
    inventory_ = Inventory::starting();
    log_ = EventLog{};
    cursor_ = 0;
    stat_.reset();
    maintScreen_ = MaintScreen{};
    maint_.reset();
    feeding_.reset();
    lockoutItemsContext_ = false;
    evolve_ = EvolveScreen{};
    csf_ = CsfScreen{};
    cfg_.reset();
    bHeld_ = false;
    loadout_.reset();
    arcadeScreen_.reset();
    arcade_.reset();                     // nothing can be mid-cabinet after a wipe
    cryptogramRun_.reset();              // ...nor mid-board
    fight_ = Fight{};                    // nothing can be mid-fight after a reset
    gauntlet_.reset();
    fxBeat_ = 0;
    lastLevelUpStat_ = -1;
    arch_.reset();
    saveDirty_ = false;
    lastSaveMs_ = nowMs_;
    if (store_) store_->clear();
    startHatch();          // lays a fresh egg at idle (persists it: durable, save v9)
    dirty_ = true;
}

}  // namespace mal
