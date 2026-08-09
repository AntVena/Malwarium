#include "core/app/game.h"

#include <cstdio>
#include <cstring>

#include "tunables.h"
#include "core/render/canvas.h"
#include "core/render/font.h"
#include "core/render/palette.h"
#include "core/render/sprite.h"
#include "core/ui/expl_screen.h"
#include "generated/assets.h"

namespace mal {

// --- Care-mistake shield (v21) ---------------------------------------------

void Game::addCareMistakeShielded(int n) {
    // The single DRY seam for Game-level POSITIVE care mistakes. If a Restore-Point
    // shield is armed, the next positive increment is BLOCKED and the shield consumed;
    // negative adjustments (e.g. Yubi-Cookie's −1) pass through untouched.
    if (n > 0 && mistakeShieldActive_) {
        mistakeShieldActive_ = false;   // consume the shield, block this one mistake
        markSaveDirty();
        return;
    }
    model_.addCareMistake(n);
}

// --- Lockout ---------------------------------------------------------------

void Game::fireLockout() {
    // Hunger hit 0: charge the "went hungry" care mistake (split budget) — one
    // half of a fully-failed Lockout's 2-mistake cost; the other +1 lands on expiry.
    // Shielded like every Game-level positive mistake.
    addCareMistakeShielded(kWentHungryMistakes);
    log_.push(LogEventType::CareMistake, "went hungry");
    // A crisis modal preempts AND cancels explore-mode — the streak resets.
    exploreActive_ = false;
    exploreStreak_ = 0;
    deepWebDepthMultiplier_ = 1;
    lockoutActive_ = true;
    lockoutDeadlineMs_ = nowMs_ + kLockoutDurationMs;
    lockoutPayOption_ = false;
    lockoutItemsContext_ = false;
    nav_ = Nav::ModalLockout;
}

void Game::expireLockout() {
    addCareMistakeShielded(kLockoutExpiryMistakes);
    model_.setHappiness(model_.happiness() - kLockoutHappyPenalty);
    model_.setHunger(kLockoutRecoveryHunger);
    char buf[28];
    std::snprintf(buf, sizeof(buf), "HAPPY -%d EXPIRED", kLockoutHappyPenalty);
    log_.push(LogEventType::CareMistake, buf);
    lockoutActive_ = false;
    lockoutItemsContext_ = false;
    nav_ = Nav::Idle;
    detailItem_ = nullptr;
    statPage_ = 0;
    loadoutScroll_ = 0;
    markSaveDirty();
}

void Game::resolveLockout() {
    // 'Pedia SURVIVED_LOCKOUT: the RESOLVED path only (feed /
    // pay-Bits / item-use) — every call site funnels through here, never through
    // expireLockout()'s failure path.
    unlockAchievement(ach::kSurvivedLockout);
    lockoutActive_ = false;
    lockoutItemsContext_ = false;
    if (model_.hunger() == 0) model_.setHunger(kLockoutRecoveryHunger);
    nav_ = Nav::Idle;
    listRow_ = 0;
    detailItem_ = nullptr;
    statPage_ = 0;
    loadoutScroll_ = 0;
    markSaveDirty();
}

// --- Decryption Hatch ------------------------------------------------------

void Game::startHatch() {
    // a fresh save LAYS THE EGG at idle. Line-select
    // precursor: when more than one egg line is unlocked, first present the
    // choice (Ransomware vs Phishing); layEgg() runs on the player's B. With a single
    // line the modal auto-skips — this keeps the historical straight-to-egg boot.
    const auto lines = availableEggLines();   // only UNLOCKED lines (Phishing gates on DEEPWEB_DEPTH_8)
    if (lines.size() > 1) {
        installPet(nullptr);         // empty save while choosing (no egg laid yet)
        hatchLine_ = nullptr;
        lineSelectRow_ = 0;
        nav_ = Nav::ModalLineSelect;
        dirty_ = true;
        return;
    }
    layEgg(lines.empty() ? registry_.eggLine("ransomware") : lines.front());
}

void Game::layEgg(const EggLineDef* line) {
    // Commit a chosen line (or the sole line via startHatch's auto-skip): lay its
    // Boot-Sector egg at idle. The egg IS the Boot-Sector creature, on-screen and
    // interactable, incubating over kBootHatchMs. It counts as a new pet immediately
    // (the egg is laid now; generation = its ordinal).
    hatchLine_ = line ? line : registry_.eggLine("ransomware");
    installPet(registry_.creature(hatchLine_ ? hatchLine_->eggCreatureId : "cryptoshell"));
    model_ = PetModel();                 // fresh Boot-Sector vitals (frozen while egg)
    for (int& t : signalTally_) t = 0;   // Boot-stage dominant signal starts fresh
    // creature levels are PER-PET — a newly laid egg begins the level system
    // fresh (the opposite of Titles/Rank, which survive a pet's death). This is the
    // single "new egg" chokepoint (fresh boot, Store, CSF, resetToHatch), so resetting
    // here covers them all; evolution keeps the level (it doesn't pass through here).
    combatXp_ = 0;
    combatLevel_ = 0;
    for (int i = 0; i < kLevelStatCount; ++i) statPoints_[i] = 0;
    lastLevelUpStat_ = -1;
    // Move-slot rework #12: the stamped Attack/Defend layout is per-pet too — a
    // fresh egg starts every slot Unset so its own raise re-derives the layout
    // from scratch (same new-egg chokepoint as the level reset above).
    for (int i = 0; i < kMaxMoveSlots; ++i) slotKinds_[i] = SlotKind::Unset;
    // The equipped/owned move loadout is per-pet too (owned moves are earned by
    // THIS pet's raise) — reset alongside the level/slot-kind state above so
    // a new egg never inherits a prior pet's (possibly wrong-line, wrong-stage)
    // equipped moves, which combat would then silently drop to Quick Jab for. A
    // fresh egg starts owning every move native to ITS OWN line — a line's moves
    // are its nature, not a lucky drop; the generic pool is wild-encounter-only.
    moveLoadout_ = MoveLoadout::startingForLine(registry_, hatchLine_ ? hatchLine_->id : nullptr);
    // Installed mod SLOTS are per-pet hardware, like the move loadout above — a
    // fresh egg starts with nothing installed. The SPARE pool is untouched: mods
    // are found like items, shared across whichever pet is currently active. Skip
    // on the very first egg (petsRaised_ == 0 here, pre-increment below) so
    // Game::Game()/resetToHatch's starting sample loadout survives a fresh boot.
    if (petsRaised_ > 0) loadout_.resetSlots();
    // Per-pet care-mistake shield state (save v21): a fresh egg carries no shield and
    // has neither once-per-lifetime item consumed. This is the single new-egg chokepoint.
    mistakeShieldActive_ = false;
    shieldItemConsumed_ = false;
    yubiConsumed_ = false;
    forceTrojanDivert_ = false;
    backupShieldUntilMs_ = 0;
    // This pet's own DeepWeb Dive record (save v35) and any armed depth-multiplier/
    // start-depth Pass — a fresh egg has never dived and carries nothing armed.
    bestDeepWebDepth_ = 0;
    deepWebDepthMultiplier_ = 1;
    pendingDeepWebStartDepth_ = -1;
    // the re-farm diminishing-returns curve is PER-PET (not per-device) —
    // a fresh pet finds every cleared area an undepleted training ground again. This
    // is the single new-egg chokepoint, so zeroing here covers fresh boot / Store /
    // CSF / resetToHatch; the pet-swap path (archDeployStored) resets it too.
    for (auto& row : subRefarmCount_) for (auto& c : row) c = 0;
    exploreActive_ = false;              // an egg can't explore; clear any live mode
    exploreStreak_ = 0;
    bootHatchRemainMs_ = kBootHatchMs;   // the incubation clock (decrypt gate)
    hatchArmed_ = false;
    hatchDeadlineMs_ = 0;
    hatchPressMs_ = 0;
    lastModelMs_ = nowMs_;
    stageEnteredMs_ = nowMs_;            // Boot-Sector in-stage clock starts now
    ++petsRaised_;                       // lifetime egg count; this pet's generation = its ordinal
    generation_ = petsRaised_;
    // The generation ladder is swept off petsRaised_ (AchSeries::PetsRaised), so
    // incrementing it here is the whole trigger — and it fires the moment the NEW egg is
    // laid, matching "raise N pets across lifecycles" rather than waiting on this one to
    // also finish hatching.
    nav_ = Nav::Idle;
    dirty_ = true;
    persistSave();                       // a laid egg survives an immediate reboot
    startHatchGame(hatchLine_);          // some lines play for the hatch bonus right now
}

void Game::startHatchGame(const EggLineDef* line) {
    // The applier for EggLineDef::hatchGame — the one place a line's hatch shape turns
    // into behaviour. Called with the egg already laid and parked at idle, so a kind
    // that does nothing here simply leaves the player there.
    if (!line) return;
    switch (line->hatchGame) {
        case HatchGame::Decrypt:
            // Nothing to open now: the Decryption Hatch is offered later, once the
            // incubation clock reaches its second half (openDecryptMinigame).
            break;
        case HatchGame::Clutch:
            startEggPick(kEggPickRounds);
            break;
        case HatchGame::Isolation:
            // The goal is the WHOLE incubation clock, priced in bytes — so "clean"
            // means the run that hatched the egg by itself, and WORM_WHISPERER has
            // something exact to test. Rounded up, so a clock that isn't a whole
            // number of minutes still has to be finished rather than merely reached.
            startIsolation(static_cast<int>(
                (bootHatchRemainMs_ + kIsolationDotMs - 1) / kIsolationDotMs));
            break;
    }
}

void Game::openDecryptMinigame() {
    // Only reachable in the second half of incubation (hatchMinigameReady). Arm the
    // decrypt modal here (on entry) — the egg-at-idle boot no longer routes through
    // ModalHatch, so the tick-time auto-arm never fires for it.
    if (!hatchMinigameReady()) return;
    // This is the Decrypt line's game specifically. A line that hatches some other way
    // (Clutch plays once, at lay-time) must never land here, so the guard sits at the
    // entry point rather than at each caller — idle's B, the Decryptogram, and anything
    // added later all get it for free. Such an egg just runs its incubation clock down.
    if (hatchLine_ && hatchLine_->hatchGame != HatchGame::Decrypt) return;
    hatchArmed_ = true;
    hatchDeadlineMs_ = nowMs_ + kHatchDurationMs;
    hatchPressMs_ = 0;
    nav_ = Nav::ModalHatch;
    dirty_ = true;
}

void Game::hatchPress() {
    if (!hatchArmed_) return;                 // armed on entry
    hatchPressMs_ = nowMs_;                    // drives the render jiggle
    if (hatchDeadlineMs_ > nowMs_ + kHatchPressReductionMs)
        hatchDeadlineMs_ -= kHatchPressReductionMs;
    else
        hatchDeadlineMs_ = nowMs_;            // enough presses: due now
    if (nowMs_ >= hatchDeadlineMs_) completeHatch();
}

void Game::accelerateEggHatch(uint32_t ms) {
    if (!inEggPhase()) return;
    bootHatchRemainMs_ = bootHatchRemainMs_ > ms ? bootHatchRemainMs_ - ms : 0;
    // Reaching 0 by acceleration auto-hatches on the next tick (see update()); we
    // don't force the modal here so a walk/network step never yanks the screen away.
}

const CreatureDef* Game::rollHatchProcess(const EggLineDef* line) {
    // Egg -> Process is a RANDOM equal-weight draw across the line's Process pool
    // every Process creature whose `line` matches the egg line's id
    // is a candidate. One member -> a deterministic pick; the ransomware egg's pool is
    // {Paypup, Conkittenate}, so a CryptoShell egg hatches one of the two at random.
    const char* lineId = line ? line->id : nullptr;
    std::vector<const CreatureDef*> pool;
    for (const CreatureDef* c : registry_.allCreatures())
        if (c->stage == Stage::Process && c->line && lineId &&
            std::strcmp(c->line, lineId) == 0 && hatchProcessUnlocked(c))
            pool.push_back(c);
    if (pool.empty()) {
        // Fallback for a lineless/legacy egg with no pool: the egg's linear successor.
        const CreatureDef* egg =
            registry_.creature(line ? line->eggCreatureId : "cryptoshell");
        return egg && egg->evolvesToId ? registry_.creature(egg->evolvesToId) : nullptr;
    }
    rng_ = rng_ * 1664525u + 1013904223u;   // advance the shared LCG (deterministic draw)
    return pool[(rng_ >> 16) % pool.size()];
}

void Game::completeHatch() {
    // The egg hatches straight to PROCESS (redesign): the Boot-Sector shell
    // and the egg share a sprite, so revealing it looked unhatched — instead the
    // decrypt jumps to the first real petware. Target = a RANDOM Process from the egg
    // line's pool (rollHatchProcess), so the same egg can hatch different species.
    const CreatureDef* egg = registry_.creature(hatchLine_ ? hatchLine_->eggCreatureId
                                                           : "cryptoshell");
    const CreatureDef* next = rollHatchProcess(hatchLine_);
    installPet(next ? next : egg); // fall back to the egg if content is missing
    model_ = PetModel();          // fresh Process vitals (unfreeze the raising loop)
    for (int& t : signalTally_) t = 0;  // Process-stage dominant signal starts fresh
    defragCount_ = 0;             // a brand-new pet has never been defragged
    stampSlotKinds();             // #12: lock this pet's newly-unlocked slot(s)
    // The Game constructor pre-equips slot 1 with a Defend move (MoveLoadout::
    // starting()) before any creature has stamped a kind onto it — this is the
    // first stamp slot 1 ever gets, so validate now rather than carry a possible
    // mismatch silently into the raise.
    enforceSlotKindInvariant();
    bootHatchRemainMs_ = 0;       // no longer an egg
    hatchArmed_ = false;
    lastModelMs_ = nowMs_;        // post-hatch decay starts now (no jump)
    stageEnteredMs_ = nowMs_;     // Process in-stage clock starts at hatch
    // FIRST_BRUTE_FORCE: the first successfully-decrypted egg. Idempotent — every later
    // hatch (a new egg after death/Store) re-fires harmlessly. The full-line rows need no
    // call: installPet() has already tallied the species, and the sweep picks it up.
    unlockAchievement(ach::kFirstBruteForce);
    nav_ = Nav::Idle;
    dirty_ = true;
    persistSave();                // a freshly hatched pet survives an immediate reboot
}

const SpriteData* Game::hatchEggSprite() const {
    if (!hatchLine_) return nullptr;
    const CreatureDef* egg = registry_.creature(hatchLine_->eggCreatureId);
    return egg ? registry_.creatureSprite(*egg) : nullptr;
}

float Game::hatchProgress() const {
    if (!hatchArmed_ || hatchDeadlineMs_ <= nowMs_) return hatchArmed_ ? 1.0f : 0.0f;
    const uint32_t remain = hatchDeadlineMs_ - nowMs_;
    float frac = 1.0f - static_cast<float>(remain) / static_cast<float>(kHatchDurationMs);
    if (frac < 0.0f) frac = 0.0f;
    if (frac > 1.0f) frac = 1.0f;
    return frac;
}

int Game::hatchCrackFrame() const {
    const SpriteData* egg = hatchEggSprite();
    const int frames = egg ? egg->frames : 4;
    int f = static_cast<int>(hatchProgress() * frames);
    if (f >= frames) f = frames - 1;
    if (f < 0) f = 0;
    return f;
}

// Evolution boundary (stub) -----------------------------------

DominantSignal Game::dominantSignal() const {
    // Argmax of the per-stage interaction tally. `>=` lets the highest index win
    // among equal maxima, so a tie leans toward Balanced (the last enum value);
    // an all-zero tally (nothing's happened yet) is Balanced outright.
    int best = 0;
    DominantSignal winner = DominantSignal::Balanced;
    for (int i = 0; i < kNumDominantSignals; ++i) {
        if (signalTally_[i] >= best) {
            best = signalTally_[i];
            winner = static_cast<DominantSignal>(i);
        }
    }
    return best > 0 ? winner : DominantSignal::Balanced;
}

void Game::noteCareSignal(DominantSignal s) {
    signalTally_[static_cast<int>(s)]++;
}

const char* Game::evolutionTargetId() const {
    if (!pet_) return nullptr;
    // Script->Daemon: a weighted pool for this care-branch, when the Script has one.
    // 0-2 mistakes -> Good, 3-4 -> Bad; 5/5 (Dying) never reaches here (evolveEligible
    // gates it out -> Critical System Failure). Single-entry pools today, so the pick
    // is deterministic (entries[0]); the weighted draw lands with the real roster.
    const bool bad = model_.careBranch() == CareBranch::Bad;
    if (const DaemonPoolDef* pool = registry_.daemonPool(pet_->id, bad)) {
        if (pool->count > 0) return pool->entries[0].daemonId;
    }
    // Everything else is on the creature's own row: a care branch when it defines
    // both successors, otherwise the linear hop (null at a terminus).
    if (pet_->evolvesToGoodId && pet_->evolvesToBadId)
        return bad ? pet_->evolvesToBadId : pet_->evolvesToGoodId;
    return pet_->evolvesToId;
}

namespace {
// The dwell required in the CURRENT stage before its evolution gate opens
// (Process and Script are the only stages that reach this — Boot exits via
// the Decryption Hatch, Daemon is a terminus with no successor).
uint32_t evolveStageDurationMs(Stage current) {
    return current == Stage::Script ? kEvolveScriptToDaemonMs : kEvolveProcessToScriptMs;
}
}  // namespace

bool Game::evolveEligible() const {
    // Walk the chain on time-in-stage + not-Dying + a real successor. Boot->Script
    // are linear; Script->Daemon branches (evolutionTargetId picks Good/Bad from the
    // care budget). The Daemon successors are termini (no successor), so they never
    // qualify. 5/5 routes to Critical System Failure, not evolution.
    // An unhatched egg never takes the normal Boot->Process boundary — the
    // Decryption Hatch IS that transition now (redesign), so completeHatch
    // owns it; the egg-at-idle incubation clock, not time-in-stage, gates it.
    if (inEggPhase()) return false;
    const char* next = evolutionTargetId();
    if (!next || !registry_.creature(next)) return false;      // terminus / unknown
    if (model_.careMistakes() >= kCareDying) return false;
    return nowMs_ - stageEnteredMs_ >= evolveStageDurationMs(pet_->stage);  // time-in-stage gate
}

bool Game::hasNextEvolution() const {
    if (inEggPhase()) return true;                    // the hatch is the next boundary
    const char* next = evolutionTargetId();
    return next && registry_.creature(next);          // false at a Daemon terminus
}

uint32_t Game::evolveRemainMs() const {
    if (inEggPhase()) return bootHatchRemainMs_;       // egg -> hatch countdown
    if (!hasNextEvolution()) return 0;                 // terminus: nothing left
    const uint32_t duration = evolveStageDurationMs(pet_->stage);
    const uint32_t elapsed = nowMs_ - stageEnteredMs_;
    return elapsed >= duration ? 0 : duration - elapsed;
}

bool Game::evolveRevealed() const {
    return evolveBeat_ >= kEvoHoldBeats + kEvoFlashBeats;
}

void Game::fireEvolution() {
    const char* id = evolutionTargetId();
    const CreatureDef* next = id ? registry_.creature(id) : nullptr;
    if (!next) return;
    // Cross-line Trojan infiltration: a Process pet with a divert target has a
    // kTrojanDivertPct chance to become a Trojan instead of its normal successor. Rolled
    // once here — fireEvolution runs once per boundary, whereas evolutionTargetId is
    // const and called repeatedly, so the roll can't live there. Same LCG as the hatch draw.
    // An armed Ambig-USB (forceTrojanDivert_, save v28) skips the roll and guarantees
    // the divert instead; consumed here either way, even if there's no divert target
    // to trigger (the flag doesn't linger onto the pet's next lifecycle).
    if (pet_ && pet_->stage == Stage::Process) {
        const bool forced = forceTrojanDivert_;
        forceTrojanDivert_ = false;
        if (pet_->evolvesToTrojanId) {
            rng_ = rng_ * 1664525u + 1013904223u;
            const bool rolled = (rng_ >> 16) % 100 < static_cast<uint32_t>(kTrojanDivertPct);
            if (forced || rolled)
                if (const CreatureDef* t = registry_.creature(pet_->evolvesToTrojanId))
                    next = t;
        }
    }
    evolveTo_ = next;           // pet_ holds the OLD sprite through the cinematic
    evolveBeat_ = 0;
    nav_ = Nav::ModalEvolve;
    dirty_ = true;
}

void Game::completeEvolution() {
    if (evolveTo_) {
        // 'Pedia achievements, evaluated BEFORE the swap: model_ still holds the care
        // budget that decided which branch fired. The branch NOT taken is deliberately
        // NOT marked seen — seeing means having FACED a species in combat
        // (Game::markCreatureSeen), and a sibling revealed by the evolution cinematic
        // was never fought.
        if (evolveTo_->stage == Stage::Daemon) {
            if (model_.careMistakes() == 0) unlockAchievement(ach::kFlawlessRun);
            if (model_.careBranch() == CareBranch::Bad)
                unlockAchievement(ach::kGoneRogue);
        }
        // A cross-line divert (or any line change) makes the pet a member of a NEW line:
        // its old line's moves no longer apply, so re-seed the loadout to the new line's
        // kit and let the new creature's slot layout govern (reset + re-stamp). The first
        // Trojan divert unlocks the family.
        const char* oldLine = pet_ ? pet_->line : nullptr;
        const char* newLine = evolveTo_->line;
        const bool lineChanged =
            (oldLine == nullptr) != (newLine == nullptr) ||
            (oldLine && newLine && std::strcmp(oldLine, newLine) != 0);
        installPet(evolveTo_);   // commit the swap; the stage indicator advances
        if (lineChanged) {
            moveLoadout_ = MoveLoadout::startingForLine(registry_, pet_->line);
            for (SlotKind& k : slotKinds_) k = SlotKind::Unset;
            stampSlotKinds();
            enforceSlotKindInvariant();
            if (newLine && std::strcmp(newLine, "trojan") == 0)
                unlockAchievement(ach::kTrojanUnleashed);
        }
    }
    evolveTo_ = nullptr;
    stageEnteredMs_ = nowMs_;          // restart the in-stage clock for the next boundary
    for (int& t : signalTally_) t = 0; // next stage's dominant signal starts fresh
    stampSlotKinds();                  // #12: lock this pet's newly-unlocked slot(s)
    nav_ = Nav::Idle;
    dirty_ = true;
    persistSave();                     // a structural change — persist immediately
    // Not logged in v1.
}

void Game::debugTriggerEvolution() {
    const char* id = evolutionTargetId();
    if (nav_ == Nav::Idle && id && registry_.creature(id)) fireEvolution();
}

// --- Move-slot rework (#12): per-pet Attack/Defend slot typing -------------

void Game::stampSlotKinds() {
    if (!pet_) return;
    const int unlocked = MoveLoadout::slotsForStage(pet_->stage);
    for (int i = 0; i < unlocked && i < kMaxMoveSlots; ++i) {
        if (slotKinds_[i] != SlotKind::Unset) continue;   // locked in — never rewritten
        slotKinds_[i] = pet_->slotKinds[i] == MoveKind::Defend ? SlotKind::Defend
                                                                : SlotKind::Attack;
    }
}

void Game::enforceSlotKindInvariant() {
    if (!pet_) return;
    const int unlocked = MoveLoadout::slotsForStage(pet_->stage);
    for (int i = 0; i < unlocked && i < kMaxMoveSlots; ++i) {
        const char* id = moveLoadout_.equipped(i);
        if (!id) continue;
        const MoveDef* m = registry_.move(id);
        if (m && m->kind != slotRequiredKind(i)) moveLoadout_.unequip(i);
    }
}

// Critical System Failure ---------------------------------------

bool Game::csfRevealed() const { return csfBeat_ >= kCsfHoldBeats; }

void Game::fireCSF() {
    // The dying state's ageing window expired — terminal. CSF is the top modal
    // priority, so it clears any lower-priority modal state it preempts —
    // including explore-mode.
    exploreActive_ = false;
    exploreStreak_ = 0;
    deepWebDepthMultiplier_ = 1;
    lockoutActive_ = false;
    lockoutItemsContext_ = false;
    evolveTo_ = nullptr;
    dyingArmed_ = false;
    dyingElapsedMs_ = 0;
    csfBeat_ = 0;
    nav_ = Nav::ModalCSF;
    dirty_ = true;
}

void Game::acknowledgeCSF() {
    // The pet becomes a permanent [CORRUPTED] ARCH record (greyed, NO rack slot,
    // ) and the active save is vacated -> the Decryption Hatch. Not
    // logged in v1; the record is the lasting trace.
    if (pet_) {
        SaveRecord rec;
        std::strncpy(rec.id, pet_->id, kSaveIdCap - 1);
        rec.status = static_cast<uint8_t>(RecordStatus::Corrupted);
        rec.generation = generation_;
        records_.push_back(rec);
    }
    csfBeat_ = 0;
    dyingArmed_ = false;
    dyingElapsedMs_ = 0;
    startHatch();      // pet_ = nullptr, nav_ = ModalHatch, re-arms on the next tick
    persistSave();     // the record + vacated active survive an immediate reboot
}

// --- Dev reset -------------------------------------------------------------

void Game::wipeDeviceProgress() {
    // Everything the device knows that OUTLIVES a pet: the 'Pedia's reveal tiers,
    // the operator's rank and discovery history, earned Titles, world progress, the
    // Rig Shop's account upgrades, the crew allegiance, and every radio consent
    // (out of the box, all three are off). resetToHatch() clears the per-pet half;
    // together they are a device with no history. Deliberately NOT cleared: the
    // screen preferences (brightness / UI mode), which are how the operator likes
    // their hardware rather than anything they earned, and the SD-backed ledgers
    // (core/net/network_ledger.h, peer_ledger.h), which are files on a card the
    // operator can remove — a save wipe has no business deleting them.
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
    hackerRank_ = 0;
    networksSeen_ = 0;
    handshakesSeen_ = 0;
    seenHandshakeBssids_.clear();
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
    // path so a retest is identical to a real first boot — no reflash needed.
    model_ = PetModel();
    inventory_ = Inventory::starting();
    log_ = EventLog{};
    bits_ = kStartBits;
    cursor_ = 0;
    listRow_ = 0;
    detailItem_ = nullptr;
    statPage_ = 0;
    loadoutScroll_ = 0;
    maintKind_ = MaintKind::Defrag;
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
    // split (Reset Pet keeps 'Pedia) lands with the SD store — both wipe today.
    rack_.clear();
    records_.clear();
    loadout_ = Loadout::starting();
    moveLoadout_ = MoveLoadout::startingForLine(registry_, nullptr);  // startHatch() below re-seeds it
    loadoutTab_ = LoadoutTab::Hub;
    loadoutHubRow_ = 0;
    arcadeRow_ = 0;
    arcadeRun_ = false;                  // nothing can be mid-cabinet after a wipe
    arcadeDifficulty_ = ArcadeDifficulty::Medium;
    for (int i = 0; i < kArcadeMaxCabinets; ++i) { arcadePlays_[i] = 0; arcadeWins_[i] = 0; }
    trainRow_ = 0;
    trainScreen_ = TrainScreen::MovePicker;
    moveConfirm_ = false;
    movePendingId_ = nullptr;
    simTier_ = 0;
    combatBeat_ = 0;
    combatTurnBeat_ = 0;
    combatXp_ = 0;
    combatLevel_ = 0;                    // a fresh egg starts at level 0
    defragCount_ = 0;                    // no defrags on a wiped save
    for (int i = 0; i < kLevelStatCount; ++i) statPoints_[i] = 0;
    lastLevelUpStat_ = -1;
    for (int i = 0; i < kMaxMoveSlots; ++i) slotKinds_[i] = SlotKind::Unset;  // #12
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
    startHatch();          // lays a fresh egg at idle (persists it — durable, save v9)
    dirty_ = true;
}

// Zone-completion Titles -----------------------------------

void Game::unlockTitle(int sector) {
    // Grant the sector's Title (player-level). Idempotent — re-clearing a sector
    // (or a migrated save that already had it) never double-logs. Auto-equip the
    // first Title earned so the reward is visible without a trip to CFG.
    if (sector < 0 || sector >= kAreaCount) return;
    if (titlesUnlocked_ & (1u << sector)) return;      // already earned
    titlesUnlocked_ |= (1u << sector);
    if (equippedTitle_ < 0) equippedTitle_ = sector;   // first Title auto-equips
    log_.push(LogEventType::CombatWon, "TITLE UNLOCKED");
    markSaveDirty();
}

int Game::titlesUnlockedCount() const {
    int n = 0;
    for (int i = 0; i < kAreaCount; ++i)
        if (titlesUnlocked_ & (1u << i)) ++n;
    return n;
}

const char* Game::equippedTitleName() const {
    return titleUnlocked(equippedTitle_) ? sectorTitle(equippedTitle_) : "NONE";
}

int Game::nextSelectableTitle(int cur) const {
    // Selectable ring: NONE (-1), then each unlocked sector in ascending order.
    // Walk forward from `cur`, wrapping through -1, to the next selectable value.
    for (int step = 0; step < kAreaCount + 1; ++step) {
        cur = (cur + 1 > kAreaCount - 1) ? -1 : cur + 1;   // ... , n-1, -1, 0, ...
        if (cur < 0 || titleUnlocked(cur)) return cur;
    }
    return -1;   // nothing unlocked → only NONE is selectable
}

}  // namespace mal
