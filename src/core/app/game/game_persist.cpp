#include "core/app/game.h"

#include <cstdio>
#include <cstring>

#include "tunables.h"
#include "core/content/content_backgrounds.h"
#include "core/content/content_tournament.h"
#include "core/render/palette.h"   // palThemeByName: the saved theme is resolved by name

namespace mal {

// Persistence ----------------------------------------

// Every collection below is reserved to the exact count before it is filled. The
// count is always known from the live container, and a vector that grows into place
// instead allocates a whole doubling ladder, which costs nothing in total but leaves
// the free heap in pieces, and the size of the largest PIECE is what persistSave's
// guard reads. Reserving is what keeps building a save from being the thing that
// makes the save look unaffordable.
//
// One section per restore* below, in the same order, so each pair reads side by side.
// Every capture is a read, so unlike applySave the order carries no dependency.
SaveData Game::captureSave() const {
    SaveData d;
    captureBag(d);
    captureHistory(d);
    captureRadio(d);
    captureOperator(d);
    captureTourneyRun(d);
    captureActivePet(d);
    captureSettings(d);
    return d;
}

void Game::captureBag(SaveData& d) const {
    d.items.reserve(inventory_.stacks().size());
    for (const auto& s : inventory_.stacks()) {
        SaveStack ss;
        std::strncpy(ss.id, s.id, kSaveIdCap - 1);
        ss.qty = s.qty;
        d.items.push_back(ss);
    }
}

void Game::captureHistory(SaveData& d) const {
    // Log, chronological oldest-first (at(0) is newest), so a restore replays it.
    d.log.reserve(static_cast<size_t>(log_.size()));
    for (int i = log_.size() - 1; i >= 0; --i) {
        const LogEntry& e = log_.at(i);
        SaveLogEntry se;
        se.type = static_cast<uint8_t>(e.type);
        std::strncpy(se.text, e.text, kSaveTextCap - 1);
        d.log.push_back(se);
    }
}

// The radio toggles persist as intent only: hot/cooldown timers are runtime-only (no RTC).
void Game::captureRadio(SaveData& d) const {
    d.netScanEnabled = netScanEnabled_ ? 1 : 0;       // v5
    d.apEnabled = apEnabled_ ? 1 : 0;                 // v20
    // v37: only the LINK consent bit. The met operators live in the SD-backed
    // PeerLedger (core/net/peer_ledger.h).
    d.linkEnabled = linkEnabled_;
    d.auditCaptureEnabled = auditCapture_.enabled() ? 1 : 0;   // v6

    // v7: the real-radio SHAKES dedup ledger + the lifetime handshake count, so
    // unique-credit survives a reboot (never re-count on return). The card holds the
    // full rotating history; the save stores only the most-recent
    // kHandshakeDedupSaveCap keys as the card-less fallback (newest at the back).
    const std::vector<uint64_t>& shakeKeys = handshakeLedger_.keys();
    const size_t keep = shakeKeys.size() < static_cast<size_t>(kHandshakeDedupSaveCap)
                            ? shakeKeys.size()
                            : static_cast<size_t>(kHandshakeDedupSaveCap);
    d.seenHandshakeBssids.assign(shakeKeys.end() - keep, shakeKeys.end());
}

void Game::captureOperator(SaveData& d) const { d.op = op_.saved(registry_, nowMs_); }

void Game::captureTourneyRun(SaveData& d) const {
    // v53: ROCK THE DOCK's run in play. Four bytes plus a bitmask, because every entrant is
    // derived from the seed (core/model/tournament.h) rather than written down.
    d.tourneySeed = tourneySeed_;
    d.tourneyAlive = tourneyAlive_;
    d.tourneyRound = tourneyRound_;
    d.tourneyPhase = static_cast<uint8_t>(tourneyPhase_);
}

void Game::captureActivePet(SaveData& d) const {
    d.active = pet_.stored(nowMs_);
    d.activeArmed = pet_.armed.saved();
}

// Device-level CFG preferences: what the device looks and sounds like, which no pet owns.
void Game::captureSettings(SaveData& d) const {
    d.brightness = brightness_;                       // v14
    d.uiMode = static_cast<uint8_t>(uiMode_);         // v67
    d.careAlerts = careAlerts_ ? 1 : 0;
    d.soundMode = static_cast<uint8_t>(soundMode_);   // v68
    d.volume = static_cast<uint8_t>(volume_);
    d.exploreSound = exploreSound_ ? 1 : 0;           // v71
    // v64: the chosen PAL_CORE theme, stored by NAME (see save.h's version note).
    std::snprintf(d.theme, sizeof(d.theme), "%s", themeName());
}

void Game::restoreBag(const SaveData& d) {
    // Inventory: resolve each saved id to the registry's stable pointer (the
    // borrowed-id contract); drop ids no longer present in content.
    inventory_ = Inventory{};
    for (const auto& s : d.items) {
        const ItemDef* def = registry_.item(s.id);
        if (def && s.qty > 0) inventory_.add(def->id, s.qty);
    }
}

void Game::restoreHistory(const SaveData& d) {
    // Event log: replay oldest-first into a fresh ring.
    log_ = EventLog{};
    for (const auto& e : d.log)
        log_.push(static_cast<LogEventType>(e.type), e.text);
}

void Game::restoreRadio(const SaveData& d) {
    // Audit-mode passive-scan toggle (v5). A pre-v5 blob defaults it OFF (never
    // silently arm the radio on a migrated save).
    netScanEnabled_ = d.netScanEnabled != 0;

    // 'Pedia local-AP toggle (v20). A pre-v20 blob defaults it OFF (never silently
    // stand up an AP on a migrated save).
    apEnabled_ = d.apEnabled != 0;

    // Pet-to-pet LINK toggle (v37). A pre-v37 blob defaults it OFF, so an upgrade
    // never starts broadcasting the operator's identity. Unlike capture/scan there
    // is no invariant to normalize: LINK is independent of the audit ladder, since
    // listening and announcing are separate consents.
    linkEnabled_ = d.linkEnabled;

    // The v38 slot is deliberately not read back: it held a standing internet opt-in,
    // and this device now goes online only for the duration of a job somebody started
    // (see Game::netConnectWanted). A save carrying the old bit lands off the 'net.

    // Audit-capture toggle (v6). A pre-v6 blob defaults it OFF. Rebuild the policy
    // SM from the persisted intent. The hot/cooldown clock starts fresh (no RTC,
    // like Lockout): a reboot clears any in-flight PENALTY window. The 5/5 CSF
    // recovery window is the one exception (Pet::dyingElapsedMs).
    auditCapture_ = AuditCapture{};
    auditCapture_.setEnabled(d.auditCaptureEnabled != 0, nowMs_);

    // Normalize the escalating-audit invariant on load: a legacy save from before
    // the single AuditMode control could hold capture-on with scan-off (the two
    // toggles were independent). Capture depends on the scan, so pull scan up to
    // match rather than run capture without its discovery feed.
    if (auditCapture_.enabled() && !netScanEnabled_) netScanEnabled_ = true;

    // Audit SHAKES dedup ledger (v7), so a reboot never re-credits a handshake already
    // counted. A pre-v7 blob carries an empty ledger (dedup rebuilds from scratch
    // that session). (v33 dropped the NETS/seenBssids set; see save.h.)
    handshakeLedger_.seed(d.seenHandshakeBssids);
}

// The player across every pet, through the record's own mapping. A Store-vacated save (no
// active pet) still carries all of it. What follows the load reads state outside the
// record: the bag, the active pet's save, and the background a raised species brings.
void Game::restoreOperator(const SaveData& d) {
    op_ = Operator::loaded(d.op, d.active.equippedMods, registry_);
    sweepCollectedItems();     // fold in whatever the bag holds: seeds a pre-v40 save
    if (d.op.speciesDiveIds.empty() && d.active.bestDeepWebDepth > 0)
        // Pre-v40: one pet's own record is the only dive history that was ever kept, so
        // credit it to that pet's species. Deeper dives by earlier pets were never
        // recorded, so they cannot be recovered.
        op_.shelves.recordDive(registry_.creature(d.active.id), d.active.bestDeepWebDepth);
    // Every species the rack or the records still point at was raised to get there, so
    // it belongs in the tally whatever the blob says. That rebuilds a pre-v39 save, which
    // carries no tally at all. The active pet is covered by installPet.
    for (const SaveStoredPet& p : op_.arch.rack) markCreatureRaised(p.id);
    for (const SaveRecord& r : op_.arch.records) markCreatureRaised(r.id);
    achBannerWire_ = -1;
    achBannerCount_ = 0;
    // Bandwidth isn't persisted (it refills on reboot), so re-seed the live pool to the
    // restored cap and a loaded save boots with a full farm pool.
    bandwidth_ = bandwidthMax();
}

// v53: the arena run. A blob written before the arena existed reads back a zero seed,
// which IS "no bracket in play"; a phase byte naming a value this build does not have
// falls back to Ready, so a stale save resumes a fightable run rather than a screen
// stuck on a verdict it cannot dismiss.
void Game::restoreTourneyRun(const SaveData& d) {
    tourneySeed_ = d.tourneySeed;
    tourneyAlive_ = d.tourneyAlive;
    tourneyRound_ = d.tourneyRound < kTourneyRounds
                        ? d.tourneyRound : static_cast<uint8_t>(kTourneyRounds - 1);
    tourneyPhase_ = d.tourneyPhase <= static_cast<uint8_t>(TourneyPhase::Champion)
                        ? static_cast<TourneyPhase>(d.tourneyPhase)
                        : TourneyPhase::Ready;
}

// The active pet, through the same mapping as a rack Deploy, then what the save keeps of
// its armed part. Runs after restoreOperator, which replaces the raised-species tally
// that installPet adds to.
void Game::restoreActivePet(const SaveData& d) {
    pet_ = Pet::thawed(d.active, registry_, nowMs_);
    pet_.armed = PetArmed::loaded(d.activeArmed);
    installPet(pet_.creature);   // a Store-vacated save has none
    if (pet_.creature) {
        lastModelMs_ = nowMs_;
        // Backfill any slot the v24 tail didn't cover (a pre-v24 blob leaves
        // ALL of them Unset here), then drop any restored equip that no longer
        // matches its slot's stamped kind: the invariant is enforced on load,
        // and a wrong-typed equip is dropped.
        stampSlotKinds();
        enforceSlotKindInvariant();
    }
}

// CFG preferences. Runs after restoreOperator because setThemePick validates the
// theme against the collected-items shelf: applied any earlier, an earned set reads
// as locked and the load falls back to base.
void Game::restoreSettings(const SaveData& d) {
    // Screen-brightness level (v14). A pre-v14 blob carries kBrightnessDefault
    // (brightest). Clamp defensively so a corrupt value can't drive the backlight
    // out of range.
    brightness_ = d.brightness;
    // v67: UI MODE and CARE ALERTS. A mode number this build has no mode for reads as
    // the default rather than as a cast past the enum's end.
    uiMode_ = d.uiMode <= static_cast<uint8_t>(UiMode::TextOnly) ? static_cast<UiMode>(d.uiMode)
                                                                 : UiMode::IconsLabel;
    careAlerts_ = d.careAlerts != 0;
    // v68
    soundMode_ = d.soundMode < kSoundModeCount ? static_cast<SoundMode>(d.soundMode)
                                               : SoundMode::Off;
    volume_ = d.volume < kVolumeLevels ? d.volume : kVolumeLevels - 1;
    exploreSound_ = d.exploreSound != 0;   // v71
    if (brightness_ < 0) brightness_ = 0;
    if (brightness_ >= kBrightnessLevels) brightness_ = kBrightnessLevels - 1;

    // The chosen theme (v64), by name. An empty name (every pre-v64 blob), and a name
    // this build ships no set for both resolve to base, and setThemePick applies it:
    // the palette index and this member move together, so the first repaint after a
    // load uses the saved set.
    setThemePick(palThemeByName(d.theme));
}

// The chosen background, validated rather than trusted: one the operator does not own is
// not a state this game should be able to boot into, so it lands on AUTO. Ownership is
// derived from the raised species, the cleared areas and the bracket tally, and the first
// of those is rebuilt from the rack, the records and the active pet, so this runs last.
// Asked any earlier, every earned background would read as unowned.
void Game::settleBackgroundPick() {
    if (!backgroundOwned(op_.backgroundPick)) op_.backgroundPick = SceneId::None;
}

void Game::applySave(const SaveData& d) {
    // Order matters where a step validates against an earlier one: the collected shelf
    // folds in the bag, installPet tallies into the raised species, and the settings and
    // background validate against what everything above restored.
    restoreBag(d);
    restoreHistory(d);
    restoreRadio(d);
    restoreOperator(d);
    restoreTourneyRun(d);
    restoreActivePet(d);
    restoreSettings(d);
    settleBackgroundPick();
    lastSaveMs_ = nowMs_;
    saveDirty_ = false;
}

void Game::persistSave() {
    if (!store_) return;
    // An explicit persistSave() ASSERTS there is something worth writing, so it says so
    // before anything below can turn it away. Every guard past this point leaves the
    // flag set, so the next tick tries again (the contract kSaveHeapFloorBytes
    // documents). The ten "persist immediately" call sites (a laid egg, a hatch, a
    // retire) rely on this. The autosave path reaches here only when it already wanted
    // a write; a landed one clears the flag on the way out regardless.
    saveDirty_ = true;
    // Defer rather than risk the write. A save fires off a timer, so it can land while
    // something else holds the heap: on device, the audit capture arming is the
    // measured case. Leaving saveDirty_ set means the next tick tries again, so the
    // only cost is that the write lands a few seconds later; attempting it without
    // room costs a device reset (setHeapProbe).
    //
    // TWO CHECKS, because the write allocates in two stages. captureSave() builds the
    // SaveData's own vectors, and those can spend the headroom the first check
    // approved, so the second check runs after the SaveData exists. It is the one
    // standing between the allocator and everything below it.
    if (!saveHasHeadroom()) { noteSaveDeferred(); return; }
    const SaveData captured = captureSave();
    // The second check is the one whose firing is otherwise unobservable: the SaveData
    // whose allocation triggered it is freed on the way out, so any sampler looking at
    // the heap afterwards sees a healthy number and concludes nothing happened.
    // Counting it is the only evidence that the guard works rather than that the
    // device merely got lucky.
    if (!saveHasHeadroom()) { noteSaveDeferred(); return; }
    // Size the buffer when this save needs more of it than the last one left behind:
    // the first write after boot, and any later one whose blob has outgrown it. Dropped
    // before it is re-reserved so the peak is the new buffer alone and never both at
    // once, and reached only from behind kSaveGrowHeapFloorBytes, which saveHeapFloor()
    // demands precisely because this line is here. Every other save skips it and writes
    // into memory this object already holds.
    if (saveBlob_.capacity() < saveBlobTargetBytes()) {
        std::vector<uint8_t>().swap(saveBlob_);
        saveBlob_.reserve(saveBlobTargetBytes());
    }
    serializeSaveInto(captured, saveBlob_);
    lastBlobSize_ = saveBlob_.size();   // the buffer got sized and filled either way
    // A store that refused the write leaves the blob on flash older than RAM. After a
    // reboot the progress is gone, and a pet just hatched is replaced by the one
    // before it. Treating it as a write that landed is what makes it silent: the flag
    // would clear, nothing would retry, and saveNow() would tell the travel-sleep
    // path it was safe to power down on a save that never happened. So the flag stays
    // set. lastSaveMs_ still advances, which paces the retries at kSaveDebounceMs
    // instead of every beat. A short write is usually transient.
    if (!store_->save(saveBlob_)) {
        ++saveWritesFailed_;
        lastSaveMs_ = nowMs_;
        return;
    }
    savesDeferred_ = 0;
    saveWritesFailed_ = 0;
    lastSaveMs_ = nowMs_;
    saveDirty_ = false;
}

bool Game::saveNow() {
    if (!store_) return true;   // nothing to lose: no store, no state to strand
    // Unconditional, because the state is about to leave RAM. Passive decay moves the
    // model without marking itself, and persistSave's own opening line covers that.
    // The return value is the flag persistSave clears ONLY on a write that landed, so
    // false means the state is still only in RAM, whether the heap guard turned the
    // write away or the store refused it. Either way the caller must not power down.
    persistSave();
    return !saveDirty_;
}

}  // namespace mal
