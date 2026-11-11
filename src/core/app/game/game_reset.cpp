// game_reset.cpp: the factory reset, in its two halves: the per-pet state (resetToHatch) and
// everything the device keeps across pets (wipeDeviceProgress).

#include "core/app/game.h"

#include <cstring>

#include "tunables.h"

namespace mal {

void Game::wipeDeviceProgress() {
    // Everything the device knows that OUTLIVES a pet: the 'Pedia's reveal tiers, the
    // operator's rank and discovery history, earned Titles, world progress, every
    // quote's standing, the Rig Shop's account upgrades, the crew allegiance, and every
    // radio consent (out of the box, all three are off). resetToHatch() clears the
    // per-pet half; together they are a device with no history. Not cleared: the device
    // preferences (brightness, UI mode, sound), which are settings rather than progress,
    // and the device-level SD-backed ledgers (core/net/network_ledger.h, peer_ledger.h),
    // which are files on a card the operator can remove. A save wipe has no business
    // deleting them. The SHAKES handshake ledger is SD-backed too but pet-level (it
    // follows handshakesSeen_ below), so it IS cleared here, which truncates its card
    // file.
    seenCreatures_.clear();
    raisedCreatures_.clear();
    malbeastSeenMask_ = 0;
    malbeastDefeatedMask_ = 0;
    for (uint8_t& b : achEarned_) b = 0;
    for (uint8_t& b : achNotified_) b = 0;
    achBannerWire_ = -1;
    achBannerCount_ = 0;
    collectedItems_.clear();
    speciesDives_.clear();
    bossWins_ = 0;
    stackerWins_ = 0;
    for (uint8_t& b : quoteStates_) b = 0;   // every quote back to never played
    for (uint8_t& b : tipsSeen_) b = 0;      // a device with no history is taught again
    hackerRank_ = 0;
    networksSeen_ = 0;
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
    for (int i = 0; i < kMaxRigUpgrades; ++i) rigLevel_[i] = 0;
    crewIndex_ = -1;
    homeNetworkKey_ = 0;
    homeNetworkName_[0] = '\0';
    setAuditMode(AuditMode::Off);      // seals capture with the scan, per the ladder
    setApEnabled(false);
    setLinkEnabled(false);
}

void Game::resetToHatch() {
    // Dev shortcut (host key / device gesture / tests): wipe to a fresh empty
    // save and re-enter the Decryption Hatch. Mirrors the FreshHatch constructor
    // path, so a retest matches a real first boot without a reflash.
    model_ = PetModel();
    inventory_ = Inventory::starting();
    log_ = EventLog{};
    bits_ = kStartBits;
    cursor_ = 0;
    listRow_ = 0;
    detailItem_ = nullptr;
    resetStatReader();
    maintKind_ = MaintKind::Defrag;
    maintVariant_ = 0;
    maintPlayed_ = false;
    processResolved_ = false;
    feedItem_ = nullptr;
    feedFromLockout_ = false;
    lockoutActive_ = false;
    lockoutItemsContext_ = false;
    evolveTo_ = nullptr;
    evolveBeat_ = 0;
    dyingArmed_ = false;
    dyingElapsedMs_ = 0;
    csfBeat_ = 0;
    cfgScreen_ = CfgScreen::SysInfo;
    factoryScope_ = 0;
    bHeld_ = false;
    // A full wipe: clear the persisted save + the rack + lifetime/identity state,
    // so the next boot is a genuine first boot. The scope
    // split (Reset Pet keeps 'Pedia) lands with the SD store: both wipe today.
    rack_.clear();
    records_.clear();
    loadout_ = Loadout::starting();
    moveLoadout_ = MoveLoadout::startingForLine(registry_, nullptr);  // startHatch() below re-seeds it
    loadoutTab_ = LoadoutTab::Hub;
    loadoutHubRow_ = 0;
    arcadeRow_ = 0;
    arcadeRun_ = false;                  // nothing can be mid-cabinet after a wipe
    cryptogramQuote_ = -1;               // ...nor mid-board
    cryptogramPrizeBits_ = 0;
    cryptogramPrize_ = {};
    arcadeDifficulty_ = ArcadeDifficulty::Medium;
    maintDifficulty_ = ArcadeDifficulty::Easy;
    for (int i = 0; i < kArcadeMaxCabinets; ++i) {
        arcadePlays_[i] = 0;
        arcadeWins_[i] = 0;
        arcadeBest_[i] = 0;
    }
    trainRow_ = 0;
    trainScreen_ = TrainScreen::MovePicker;
    moveConfirm_ = false;
    movePendingId_ = nullptr;
    simTier_ = 0;
    combatBeat_ = 0;
    fxBeat_ = 0;
    combatTurnBeat_ = 0;
    combatXp_ = 0;
    combatLevel_ = 0;                    // a fresh egg starts at level 0
    defragCount_ = 0;                    // no defrags on a wiped save
    upgrades_ = PetUpgrades{};           // nothing permanent survives a wipe either
    petFoodsEaten_.clear();
    for (int i = 0; i < kLevelStatCount; ++i) statPoints_[i] = 0;
    lastLevelUpStat_ = -1;
    for (int i = 0; i < kMaxMoveSlots; ++i) slotKinds_[i] = SlotKind::Unset;
    generation_ = 0;
    petsRaised_ = 0;
    uptimeBase_ = 0;
    lifetimeSteps_ = 0;
    std::strncpy(hackerTag_, "NETRUNNER_99", sizeof(hackerTag_) - 1);
    hackerTag_[sizeof(hackerTag_) - 1] = '\0';
    archConfirm_ = false;
    saveDirty_ = false;
    lastSaveMs_ = nowMs_;
    if (store_) store_->clear();
    startHatch();          // lays a fresh egg at idle (persists it: durable, save v9)
    dirty_ = true;
}

}  // namespace mal
