// game_reset.cpp: the factory reset, in its two halves: the per-pet state (resetToHatch) and
// everything the device keeps across pets (wipeDeviceProgress).

#include "core/app/game.h"

#include <cstring>

#include "tunables.h"

namespace mal {

void Game::wipeDeviceProgress() {
    // Everything the device knows that OUTLIVES a pet: the whole Operator record, earned
    // Titles, world progress, the collected-items and dive shelves, the Cant, and every
    // radio consent (out of the box, all three are off). resetToHatch() clears the
    // per-pet half; together they are a device with no history. Not cleared: the device
    // preferences (brightness, UI mode, sound), which are settings rather than progress,
    // and the device-level SD-backed ledgers (core/net/network_ledger.h, peer_ledger.h),
    // which are files on a card the operator can remove. A save wipe has no business
    // deleting them. The SHAKES handshake ledger is SD-backed too but pet-level (it
    // follows handshakesSeen_ below), so it IS cleared here, which truncates its card
    // file.
    op_ = Operator{};
    raisedCreatures_.clear();
    achBannerWire_ = -1;
    achBannerCount_ = 0;
    collectedItems_.clear();
    speciesDives_.clear();
    handshakesSeen_ = 0;
    handshakeLedger_.clear();
    cantSigils_ = 0;      // the Cant is unlearned again, and the purse it was bought
    shakesSpent_ = 0;     // with is empty: both follow the handshakes they came from
    titlesUnlocked_ = 0;
    equippedTitle_ = -1;
    for (int a = 0; a < kAreaCount; ++a) {
        sectorCleared_[a] = false;
        for (int s = 0; s < kSubAreasPerArea; ++s) {
            subCleared_[a][s] = false;
            subBossUnlocked_[a][s] = false;
        }
    }
    setAuditMode(AuditMode::Off);      // seals capture with the scan, per the ladder
    setApEnabled(false);
    setLinkEnabled(false);
}

void Game::resetToHatch() {
    // Dev shortcut (host key / device gesture / tests): wipe to a fresh empty
    // save and re-enter the Decryption Hatch. Mirrors the FreshHatch constructor
    // path, so a retest matches a real first boot without a reflash.
    // A wipe replaces the whole Pet. The sample mods are the device's first-egg kit,
    // which layEgg keeps.
    pet_ = Pet{};
    pet_.mods = ModSlots::starting();
    inventory_ = Inventory::starting();
    log_ = EventLog{};
    cursor_ = 0;
    listRow_ = 0;
    detailItem_ = nullptr;
    stat_.reset();
    maintKind_ = MaintKind::Defrag;
    maintVariant_ = 0;
    maintPlayed_ = false;
    processResolved_ = false;
    feedItem_ = nullptr;
    feedFromLockout_ = false;
    lockoutItemsContext_ = false;
    evolveTo_ = nullptr;
    evolveBeat_ = 0;
    csfBeat_ = 0;
    cfgScreen_ = CfgScreen::SysInfo;
    factoryScope_ = 0;
    bHeld_ = false;
    // A full wipe: clear the persisted save + the rack + lifetime/identity state,
    // so the next boot is a genuine first boot. The scope
    // split (Reset Pet keeps 'Pedia) lands with the SD store: both wipe today.
    rack_.clear();
    records_.clear();
    loadoutTab_ = LoadoutTab::Hub;
    loadoutHubRow_ = 0;
    arcadeRow_ = 0;
    arcadeRun_ = false;                  // nothing can be mid-cabinet after a wipe
    cryptogramQuote_ = -1;               // ...nor mid-board
    cryptogramPrizeBits_ = 0;
    cryptogramPrize_ = {};
    arcadeDifficulty_ = ArcadeDifficulty::Medium;
    maintDifficulty_ = ArcadeDifficulty::Easy;
    trainRow_ = 0;
    trainScreen_ = TrainScreen::MovePicker;
    moveConfirm_ = false;
    movePendingId_ = nullptr;
    simTier_ = 0;
    combatBeat_ = 0;
    fxBeat_ = 0;
    combatTurnBeat_ = 0;
    lastLevelUpStat_ = -1;
    // RESET PET also restarts this much of the Operator: the tag, the lifetime counters,
    // the wallet, the spare mods and the arcade tallies. The 'Pedia, the rank, the Rig
    // Shop and the achievements stand. WIPE EVERYTHING has already replaced the record.
    const Operator fresh;
    std::memcpy(op_.hackerTag, fresh.hackerTag, sizeof(op_.hackerTag));
    op_.petsRaised = fresh.petsRaised;
    op_.uptimeBase = fresh.uptimeBase;
    op_.lifetimeSteps = fresh.lifetimeSteps;
    op_.bits = fresh.bits;
    op_.modPool = fresh.modPool;
    OperatorTallies& t = op_.tallies;
    std::memcpy(t.arcadePlays, fresh.tallies.arcadePlays, sizeof(t.arcadePlays));
    std::memcpy(t.arcadeWins, fresh.tallies.arcadeWins, sizeof(t.arcadeWins));
    std::memcpy(t.arcadeBest, fresh.tallies.arcadeBest, sizeof(t.arcadeBest));
    archConfirm_ = false;
    saveDirty_ = false;
    lastSaveMs_ = nowMs_;
    if (store_) store_->clear();
    startHatch();          // lays a fresh egg at idle (persists it: durable, save v9)
    dirty_ = true;
}

}  // namespace mal
