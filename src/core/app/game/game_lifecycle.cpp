#include "core/app/game.h"

#include <cstdio>
#include <cstring>

#include "tunables.h"
#include "core/render/sprite.h"
#include "core/ui/expl_screen.h"

namespace mal {

// --- Care-mistake shield (v21) ---------------------------------------------

void Game::addCareMistakeShielded(int n) {
    // The single DRY seam for Game-level POSITIVE care mistakes. If a Restore-Point
    // shield is armed, the next positive increment is BLOCKED and the shield consumed;
    // negative adjustments (e.g. Yubi-Cookie's −1) pass through untouched.
    if (n > 0 && pet_.grants.shieldActive) {
        pet_.grants.shieldActive = false;   // consume the shield, block this one mistake
        markSaveDirty();
        return;
    }
    if (n > 0) away_.errors += n;
    pet_.model.addCareMistake(n);
}

// --- Lockout ---------------------------------------------------------------

void Game::fireLockout() {
    // Hunger hit 0: charge the "went hungry" care mistake (split budget). One
    // half of a fully-failed Lockout's 2-mistake cost; the other +1 lands on expiry.
    // Shielded like every Game-level positive mistake.
    addCareMistakeShielded(kWentHungryMistakes);
    log_.push(LogEventType::CareMistake, "went hungry");
    ++away_.hungry;
    // A crisis modal preempts AND cancels explore-mode: the streak resets.
    exploreActive_ = false;
    exploreStreak_ = 0;
    clearDiveDepthBuffs();
    pet_.armed.lockoutActive = true;
    pet_.armed.lockoutDeadlineMs = nowMs_ + kLockoutDurationMs;
    pet_.armed.lockoutReminded = false;
    playSound(Sound::Lockout);
    // Open on whichever path can actually resolve it: feeding, unless the bag has
    // nothing to feed and the wallet can cover the Bits instead.
    lockoutPayOption_ = !lockoutFoodHeld() && bits_ >= kLockoutBitsCost;
    lockoutItemsContext_ = false;
    nav_ = Nav::ModalLockout;
}

void Game::expireLockout() {
    ++away_.missed;
    addCareMistakeShielded(kLockoutExpiryMistakes);
    pet_.model.setHappiness(pet_.model.happiness() - kLockoutHappyPenalty);
    pet_.model.setHunger(kLockoutRecoveryHunger);
    char buf[28];
    std::snprintf(buf, sizeof(buf), "HAPPY -%d EXPIRED", kLockoutHappyPenalty);
    log_.push(LogEventType::CareMistake, buf);
    pet_.armed.lockoutActive = false;
    lockoutItemsContext_ = false;
    nav_ = Nav::Idle;
    detailItem_ = nullptr;
    resetStatReader();
    markSaveDirty();
}

void Game::resolveLockout() {
    // 'Pedia SURVIVED_LOCKOUT: the RESOLVED path only (feed /
    // pay-Bits / item-use). Every call site funnels through here, never through
    // expireLockout()'s failure path.
    unlockAchievement(ach::kSurvivedLockout);
    pet_.armed.lockoutActive = false;
    lockoutItemsContext_ = false;
    if (pet_.model.hunger() == 0) pet_.model.setHunger(kLockoutRecoveryHunger);
    nav_ = Nav::Idle;
    listRow_ = 0;
    detailItem_ = nullptr;
    resetStatReader();
    markSaveDirty();
}

// --- Decryption Hatch ------------------------------------------------------

void Game::startHatch() {
    // a fresh save LAYS THE EGG at idle. Line-select precursor: when more than one
    // egg line is unlocked, first present the choice (Ransomware vs Phishing);
    // layEgg() runs on the player's B. With a single line the modal skips itself and
    // boot goes straight to the egg.
    const auto lines = availableEggLines();   // only UNLOCKED lines (Phishing gates on DEEPWEB_DEPTH_8)
    if (lines.size() > 1) {
        installPet(nullptr);         // empty save while choosing (no egg laid yet)
        pet_.line = nullptr;
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
    pet_.line = line ? line : registry_.eggLine("ransomware");
    installPet(registry_.creature(pet_.line ? pet_.line->eggCreatureId : "cryptoshell"));
    pet_.model = PetModel();                 // fresh Boot-Sector vitals (frozen while egg)
    // creature levels are per pet, so a newly laid egg starts at level 0. Titles and
    // Rank survive a pet's death. This is the single "new egg" chokepoint (fresh boot,
    // Store, CSF, resetToHatch), so resetting here covers them all; evolution keeps
    // the level (it doesn't pass through here).
    pet_.xp = 0;
    pet_.level = 0;
    for (int i = 0; i < kLevelStatCount; ++i) pet_.statPoints[i] = 0;
    lastLevelUpStat_ = -1;
    // The stamped Attack/Defend layout is per-pet too: a
    // fresh egg starts every slot Unset so its own raise re-derives the layout
    // from scratch (same new-egg chokepoint as the level reset above).
    for (int i = 0; i < kMaxMoveSlots; ++i) pet_.slotKinds[i] = SlotKind::Unset;
    // The equipped/owned move loadout is per-pet too (owned moves are earned by
    // this pet's raise). It resets with the level and slot kinds above, so a new
    // egg never inherits a prior pet's moves, which could be the wrong line or
    // stage and would fall back to Quick Jab in combat. A fresh egg owns every
    // move native to its own line. The generic pool comes only from wild
    // encounters.
    pet_.moves = MoveLoadout::startingForLine(registry_, pet_.line ? pet_.line->id : nullptr);
    // Installed mod SLOTS are per-pet hardware, like the move loadout above: a
    // fresh egg starts with nothing installed. The SPARE pool is untouched: mods
    // are found like items, shared across whichever pet is currently active. Skip
    // on the very first egg (petsRaised_ == 0 here, pre-increment below) so
    // Game::Game()/resetToHatch's starting sample loadout survives a fresh boot.
    if (petsRaised_ > 0) pet_.mods.resetSlots();
    // Per-pet care-mistake shield state (save v21): a fresh egg carries no shield and
    // has neither once-per-lifetime item consumed. This is the single new-egg chokepoint.
    pet_.grants.shieldActive = false;
    pet_.grants.shieldSpent = false;
    pet_.grants.yubiSpent = false;
    pet_.armed.trojanDivert = false;
    // ...and nothing in the USB port: a fresh egg carries neither a forced branch, a soak
    // nor a hold, however many devices the last pet was raised on. pet_.armed.trojanDivert is
    // cleared just above, so this is the same emptying the Eject-USB does.
    pet_.usb.branch = BranchOverride::None;
    pet_.usb.soakFactor = 1;
    pet_.usb.hold = false;
    pet_.armed.backupShieldUntilMs = 0;
    // ...and none of the permanent Epic-dish upgrades: every one of them roots a PET, so
    // a new egg starts back at the shared regen interval, with no off-level stat points
    // and no XP rate, however many plates the last one ate.
    pet_.upgrades = PetUpgrades{};
    // ...and an empty plate. The palate belongs to the pet. collectedItems_ keeps
    // everything the device has held; this set starts over, so each pet fills STAT's
    // FOODS grid from empty.
    pet_.foodsEaten.clear();
    // This pet's own DeepWeb Dive record (save v35) and any armed depth-multiplier/
    // start-depth Pass: a fresh egg has never dived and carries nothing armed.
    pet_.bestDeepWebDepth = 0;
    clearDiveDepthBuffs();
    pet_.armed.deepWebStartDepth = -1;
    pet_.armed.pendingDiveStartBonusPct = 0;
    // the re-farm diminishing-returns curve is PER-PET (not per-device):
    // a fresh pet finds every cleared area an undepleted training ground again. This
    // is the single new-egg chokepoint, so zeroing here covers fresh boot / Store /
    // CSF / resetToHatch; the pet-swap path (archDeployStored) resets it too.
    for (auto& row : pet_.armed.subRefarmCount) for (auto& c : row) c = 0;
    exploreActive_ = false;              // an egg can't explore; clear any live mode
    exploreStreak_ = 0;
    pet_.bootHatchRemainMs = kBootHatchMs;   // the incubation clock (decrypt gate)
    pet_.eggOfferMade = false;               // this egg hasn't been offered an accelerator
    lastModelMs_ = nowMs_;
    pet_.stageEnteredMs = nowMs_;            // Boot-Sector in-stage clock starts now
    ++petsRaised_;                       // lifetime egg count; this pet's generation = its ordinal
    pet_.generation = petsRaised_;
    // The generation ladder is swept off petsRaised_ (AchSeries::PetsRaised), so
    // incrementing it here is the trigger, and it fires when the new egg is laid without
    // waiting for the hatch.
    nav_ = Nav::Idle;
    dirty_ = true;
    persistSave();                       // a laid egg survives an immediate reboot
    startHatchGame(pet_.line);          // some lines play for the hatch bonus right now
}

void Game::startHatchGame(const EggLineDef* line) {
    // The applier for EggLineDef::hatchGame, where a line's hatch shape becomes
    // behaviour. Called with the egg already laid and parked at idle, so a kind that
    // does nothing here simply leaves the player there.
    if (!line) return;
    switch (line->hatchGame) {
        case HatchGame::Decrypt:
            // The hatch board plays by the standard rules: no duplicate colours in the
            // key, and the feedback stays anonymous. Both are the arcade's dials, and
            // neither moves here.
            startDecryption(/*allowDuplicates=*/false, /*easyHints=*/false);
            break;
        case HatchGame::Clutch:
            startEggPick(kEggPickRounds);
            break;
        case HatchGame::Isolation:
            // The goal is the whole incubation clock, priced in bytes, so a clean
            // run is one that hatched the egg by itself, which WORM_WHISPERER tests.
            // Rounded up, so a clock that isn't a whole number of minutes still has
            // to be finished rather than merely reached.
            startIsolation(static_cast<int>(
                (pet_.bootHatchRemainMs + kIsolationDotMs - 1) / kIsolationDotMs));
            break;
        case HatchGame::Chroma:
            // The board as shipped: the window shrinks round by round and the water
            // never changes under the pet mid-window. Switching is only on the
            // arcade's HARD setting, because a hatch is a player's first minute with
            // a new line.
            startChroma(kChromaRounds, kChromaWindowMs, /*switching=*/false);
            break;
    }
    // The first egg of each line lays straight onto a board the player has never
    // seen, so that once the board opens paused under its own RULES page, led by
    // what the egg stakes (game_onboard.cpp). Every later egg of the line skips it.
    const Tip tip = line->hatchGame == HatchGame::Clutch    ? Tip::HatchClutch
                  : line->hatchGame == HatchGame::Isolation ? Tip::HatchIsolation
                  : line->hatchGame == HatchGame::Chroma    ? Tip::HatchChroma
                                                            : Tip::HatchDecrypt;
    if (!tipSeen(tip)) {
        markTipSeen(tip);
        openGameBrief();
        gameBriefIntro_ = gameBriefOpen_;
    }
}

void Game::accelerateEggHatch(uint32_t ms) {
    if (!inEggPhase()) return;
    pet_.bootHatchRemainMs = pet_.bootHatchRemainMs > ms ? pet_.bootHatchRemainMs - ms : 0;
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
    // The egg hatches straight to Process. The Boot-Sector shell and the egg share a
    // sprite, so revealing the shell would look unhatched. Target = a RANDOM Process
    // from the egg line's pool (rollHatchProcess), so the same egg can hatch
    // different species.
    const CreatureDef* egg = registry_.creature(pet_.line ? pet_.line->eggCreatureId
                                                           : "cryptoshell");
    const CreatureDef* next = rollHatchProcess(pet_.line);
    installPet(next ? next : egg); // fall back to the egg if content is missing
    pet_.model = PetModel();          // fresh Process vitals (unfreeze the raising loop)
    pet_.defragCount = 0;             // a brand-new pet has never been defragged
    stampSlotKinds();             // lock this pet's newly-unlocked slot(s)
    // The Game constructor pre-equips slot 1 with a Defend move (MoveLoadout::
    // starting()) before any creature has stamped a kind onto it. This is the
    // first stamp slot 1 ever gets, so validate now rather than carry a possible
    // mismatch silently into the raise.
    enforceSlotKindInvariant();
    pet_.bootHatchRemainMs = 0;       // no longer an egg
    // The device's first pet hatches into the Caution band (kStartFragmentation), so
    // MAINT's "!" invites a defrag from its first minute, and a QUICK one can fail into
    // a care ERROR before the player knows what one is. One Defrag Tool makes that first
    // defrag a sure thing; MAINT lists it as the GUARANTEED row, one press of A away.
    if (petsRaised_ == 1) {
        inventory_.add(kDefragToolId, 1);
        log_.push(LogEventType::ItemGained, "GOT A DEFRAG TOOL");
    }
    lastModelMs_ = nowMs_;        // post-hatch decay starts now (no jump)
    pet_.stageEnteredMs = nowMs_;     // Process in-stage clock starts at hatch
    away_.hatched = pet_.creature;
    nav_ = Nav::Idle;
    dirty_ = true;
    playSound(Sound::Hatch);
    persistSave();                // a freshly hatched pet survives an immediate reboot
}

const SpriteData* Game::hatchEggSprite() const {
    if (!pet_.line) return nullptr;
    const CreatureDef* egg = registry_.creature(pet_.line->eggCreatureId);
    return egg ? registry_.creatureSprite(*egg) : nullptr;
}

CareBranch Game::effectiveCareBranch() const {
    // A Dying pet has no branch: 5/5 is Critical System Failure, which no USB
    // overrides. Everything else answers to whatever is in the override slot.
    const CareBranch earned = pet_.model.careBranch();
    if (earned == CareBranch::Dying) return earned;
    switch (pet_.usb.branch) {
        case BranchOverride::Good: return CareBranch::Good;
        case BranchOverride::Bad:  return CareBranch::Bad;
        case BranchOverride::None: break;
    }
    return earned;
}

const char* Game::evolutionTargetId() const {
    if (!pet_.creature) return nullptr;
    // Script->Daemon: a weighted pool for this care-branch, when the Script has one.
    // 0-2 mistakes -> Good, 3-4 -> Bad; 5/5 (Dying) never reaches here (evolveEligible
    // gates it out -> Critical System Failure). Single-entry pools today, so the pick
    // is deterministic (entries[0]); the weighted draw lands with the real roster. The
    // branch is read through effectiveCareBranch(), so a Bad-USB/Signed-USB in the
    // port decides it instead, including on this pool, the branch the player spent the
    // stage earning.
    const bool bad = effectiveCareBranch() == CareBranch::Bad;
    if (const DaemonPoolDef* pool = registry_.daemonPool(pet_.creature->id, bad)) {
        if (pool->count > 0) return pool->entries[0].daemonId;
    }
    // Everything else is on the creature's own row: a care branch when it defines
    // both successors, otherwise the linear hop (null at a terminus).
    if (pet_.creature->evolvesToGoodId && pet_.creature->evolvesToBadId)
        return bad ? pet_.creature->evolvesToBadId : pet_.creature->evolvesToGoodId;
    return pet_.creature->evolvesToId;
}

namespace {
// The dwell required in the CURRENT stage before its evolution gate opens
// (Process and Script are the only stages that reach this: Boot exits via
// the Decryption Hatch, Daemon is a terminus with no successor).
uint32_t evolveStageDurationMs(Stage current) {
    return current == Stage::Script ? kEvolveScriptToDaemonMs : kEvolveProcessToScriptMs;
}
}  // namespace

uint32_t Game::evolveDwellMs() const {
    // The soak multiplies the WAIT, which is the half of the trade the player feels;
    // addCombatXp multiplies the other half. On a SCRIPT the wait costs double what the
    // XP pays: only the late soak (Hypervisor-USB) plugs in there at all, and a Script's
    // boundary is the branch the whole raise was aimed at, so stretching the last stage
    // before an ending is deliberately the more expensive thing to do. The stage is read
    // live, which is safe because a soak is spent at the boundary it stretched, so the
    // stage cannot change while it is in.
    //
    // Nothing overflows: the longest stage is 32h, the deepest soak x8 (the Hypervisor-USB,
    // items/rare_kit.h), doubled on a Script = x16, so 512h in ms. Under half of what a
    // u32 holds.
    const Stage stage = pet_.creature ? pet_.creature->stage : Stage::Process;
    const uint32_t base = evolveStageDurationMs(stage);
    const int factor = pet_.usb.soakFactor < 1 ? 1 : pet_.usb.soakFactor;
    const int scale = (factor > 1 && stage == Stage::Script) ? 2 * factor : factor;
    return base * static_cast<uint32_t>(scale);
}

bool Game::evolveEligible() const {
    // Walk the chain on time-in-stage + not-Dying + a real successor. Boot->Script
    // are linear; Script->Daemon branches (evolutionTargetId picks Good/Bad from the
    // care budget). The Daemon successors are termini (no successor), so they never
    // qualify. 5/5 routes to Critical System Failure, not evolution. An unhatched
    // egg never takes the normal Boot->Process boundary: the Decryption Hatch is
    // that transition, so completeHatch owns it; the egg-at-idle incubation clock,
    // not time-in-stage, gates it.
    if (inEggPhase()) return false;
    // A Halt-USB in the port blocks the boundary entirely, which parks a pet at its
    // stage. Checked before the clock, because a held pet's clock is not the reason it
    // isn't evolving.
    if (pet_.usb.hold) return false;
    const char* next = evolutionTargetId();
    if (!next || !registry_.creature(next)) return false;      // terminus / unknown
    if (pet_.model.careMistakes() >= kCareDying) return false;
    return nowMs_ - pet_.stageEnteredMs >= evolveDwellMs();          // time-in-stage gate
}

bool Game::hasNextEvolution() const {
    if (inEggPhase()) return true;                    // the hatch is the next boundary
    // A held pet has no next boundary for as long as the hold is in, so STAT's EVOLVE
    // row reads MAX, as it does at a terminus. WHY it reads MAX is on the BUFFS page and
    // on the habitat's armed-buff strip, which is where every other armed state already
    // explains itself.
    if (pet_.usb.hold) return false;
    const char* next = evolutionTargetId();
    return next && registry_.creature(next);          // false at a Daemon terminus
}

uint32_t Game::evolveRemainMs() const {
    if (inEggPhase()) return pet_.bootHatchRemainMs;       // egg -> hatch countdown
    if (!hasNextEvolution()) return 0;                 // terminus: nothing left
    const uint32_t duration = evolveDwellMs();
    const uint32_t elapsed = nowMs_ - pet_.stageEnteredMs;
    return elapsed >= duration ? 0 : duration - elapsed;
}

bool Game::evolveRevealed() const {
    return evolveBeat_ >= kEvoHoldBeats + kEvoFlashBeats;
}

void Game::fireEvolution() {
    const char* id = evolutionTargetId();
    const CreatureDef* next = id ? registry_.creature(id) : nullptr;
    if (!next) return;
    // Cross-line Trojan infiltration: a pet whose row names a divert target has a
    // kTrojanDivertPct chance to become a Trojan instead of its normal successor. Rolled
    // once here: fireEvolution runs once per boundary, whereas evolutionTargetId is
    // const and called repeatedly, so the roll can't live there. Same LCG as the hatch draw.
    //
    // The ROW is the gate, not the stage. A `stage == Process` test here would fit only a
    // roster where every divert is Phishlet's and a Trojan is by definition a Script you
    // got instead of your own. The Worm line diverts one boundary later (Rootgrub trades
    // its Daemon for one, not its Script), so a stage test would need a second mechanism
    // for it. Nothing is needed in its place: a creature with no successor never reaches
    // fireEvolution at all, and a row that must not divert says so by leaving
    // evolvesToTrojanId null.
    //
    // An armed Ambig-USB (pet_.armed.trojanDivert, save v28) skips the roll and guarantees
    // the divert instead; consumed here either way, even if there's no divert target
    // to trigger (the flag doesn't linger onto the pet's next lifecycle).
    if (pet_.creature) {
        const bool forced = pet_.armed.trojanDivert;
        pet_.armed.trojanDivert = false;
        if (pet_.creature->evolvesToTrojanId) {
            rng_ = rng_ * 1664525u + 1013904223u;
            const bool rolled = (rng_ >> 16) % 100 < static_cast<uint32_t>(kTrojanDivertPct);
            if (forced || rolled) {
                // A divert that lands on a Daemon is a BRANCH, read off the same care
                // budget the normal Script->Daemon hop reads (evolutionTargetId). The
                // divert takes the pet's line away from it; it does not also take away
                // the one thing the player spent the whole stage deciding.
                const char* to = pet_.creature->evolvesToTrojanId;
                if (pet_.creature->evolvesToTrojanBadId && effectiveCareBranch() == CareBranch::Bad)
                    to = pet_.creature->evolvesToTrojanBadId;
                if (const CreatureDef* t = registry_.creature(to))
                    next = t;
            }
        }
    }
    evolveTo_ = next;           // pet_.creature holds the OLD sprite through the cinematic
    evolveBeat_ = 0;
    nav_ = Nav::ModalEvolve;
    dirty_ = true;
    playSound(Sound::Evolve);
}

void Game::completeEvolution() {
    if (evolveTo_) away_.evolved = evolveTo_;
    if (evolveTo_) {
        // 'Pedia achievements, evaluated BEFORE the swap: pet_.model still holds the care
        // budget that decided which branch fired. The branch not taken is not marked
        // seen: seen means faced in combat (Game::markCreatureSeen), and the evolution
        // cinematic's sibling was never fought. Flawless reads the raw mistake count, so
        // no device can make a run flawless. Gone Rogue is about the DAEMON that came
        // out, so it reads effectiveCareBranch(): a pet that ended up on the bad line
        // went rogue whether a Bad-USB steered it there or five months of neglect did.
        if (evolveTo_->stage == Stage::Daemon) {
            if (pet_.model.careMistakes() == 0) unlockAchievement(ach::kFlawlessRun);
            if (effectiveCareBranch() == CareBranch::Bad)
                unlockAchievement(ach::kGoneRogue);
        }
        // A cross-line divert (or any line change) makes the pet a member of a NEW line:
        // its old line's moves no longer apply, so re-seed the loadout to the new line's
        // kit and let the new creature's slot layout govern (reset + re-stamp). The first
        // Trojan divert unlocks the family.
        const char* oldLine = pet_.creature ? pet_.creature->line : nullptr;
        const char* newLine = evolveTo_->line;
        const bool lineChanged =
            (oldLine == nullptr) != (newLine == nullptr) ||
            (oldLine && newLine && std::strcmp(oldLine, newLine) != 0);
        installPet(evolveTo_);   // commit the swap; the stage indicator advances
        if (lineChanged) {
            pet_.moves = MoveLoadout::startingForLine(registry_, pet_.creature->line);
            for (SlotKind& k : pet_.slotKinds) k = SlotKind::Unset;
            stampSlotKinds();
            enforceSlotKindInvariant();
            // Arriving on a line can be an achievement in its own right, and the line's
            // row is what says so (CreatureLine::raisedAchievement): the Trojan divert
            // is the only arrival recognised today, but this seam never learns that.
            if (const CreatureLine* cl = registry_.creatureLine(newLine))
                if (cl->raisedAchievement) unlockAchievement(cl->raisedAchievement);
        }
    }
    evolveTo_ = nullptr;
    // The USB port empties at the boundary it was steering. The branch override is
    // consumed HERE rather than in fireEvolution (where the Ambig-USB's divert flag goes)
    // because the achievements above still had to see which branch fired; the soak is
    // consumed here too, so the next stage counts down at its own pace and pays its own
    // XP. Both are per-pet state and clear again on a new egg (layEgg).
    pet_.usb.branch = BranchOverride::None;
    pet_.usb.soakFactor = 1;
    // The hold is deliberately NOT cleared here: it is the one device a boundary never
    // reaches, so reaching one means it was never in.
    pet_.stageEnteredMs = nowMs_;          // restart the in-stage clock for the next boundary
    stampSlotKinds();                  // lock this pet's newly-unlocked slot(s)
    nav_ = Nav::Idle;
    dirty_ = true;
    persistSave();                     // a structural change: persist immediately
    // Not logged in v1.
}

void Game::debugTriggerEvolution() {
    const char* id = evolutionTargetId();
    if (nav_ == Nav::Idle && id && registry_.creature(id)) fireEvolution();
}

// --- Move-slot rework: per-pet Attack/Defend slot typing -------------

void Game::stampSlotKinds() {
    if (!pet_.creature) return;
    const int unlocked = MoveLoadout::slotsForStage(pet_.creature->stage);
    for (int i = 0; i < unlocked && i < kMaxMoveSlots; ++i) {
        if (pet_.slotKinds[i] != SlotKind::Unset) continue;   // locked in, never rewritten
        pet_.slotKinds[i] = pet_.creature->slotKinds[i] == MoveKind::Defend ? SlotKind::Defend
                                                                : SlotKind::Attack;
    }
    // A pet always fights with everything it's earned: an unlocked slot that's
    // still empty (never equipped, or a slot whose only candidate unlocked AFTER
    // it was stamped; e.g. a second Attack slot stamped at Process, still empty,
    // gets filled the moment Script unlocks a second owned Attack move) falls back
    // to the strongest owned+unlocked move of its stamped kind instead of sitting
    // on the generic Quick Jab. Walked in slot order, live-checking slotOf() as it
    // goes, so two empty same-kind slots never both grab the same move.
    for (int i = 0; i < unlocked && i < kMaxMoveSlots; ++i) {
        if (pet_.moves.equipped(i) || pet_.slotKinds[i] == SlotKind::Unset) continue;
        const MoveDef::Kind kind = slotRequiredKind(i);
        for (const MoveDef* m : registry_.allMoves()) {
            if (!pet_.moves.owns(m->id) || m->kind != kind) continue;
            if (pet_.moves.isInnate(m->id)) continue;       // not slottable (equip refuses)
            if (!moveUnlockedAtStage(*m, pet_.creature->stage)) continue;
            if (!moveAllowedForLine(*m, pet_.creature->line)) continue;
            if (pet_.moves.slotOf(m->id) >= 0) continue;    // already filling another slot
            pet_.moves.equip(i, m->id);
            break;
        }
    }
}

void Game::enforceSlotKindInvariant() {
    if (!pet_.creature) return;
    const int unlocked = MoveLoadout::slotsForStage(pet_.creature->stage);
    for (int i = 0; i < unlocked && i < kMaxMoveSlots; ++i) {
        const char* id = pet_.moves.equipped(i);
        if (!id) continue;
        const MoveDef* m = registry_.move(id);
        if (m && m->kind != slotRequiredKind(i)) pet_.moves.unequip(i);
    }
}

// Critical System Failure ---------------------------------------

bool Game::csfRevealed() const { return csfBeat_ >= kCsfHoldBeats; }

void Game::fireCSF() {
    // The dying state's ageing window expired: terminal. CSF is the top modal
    // priority, so it clears any lower-priority modal state it preempts,
    // including explore-mode.
    exploreActive_ = false;
    exploreStreak_ = 0;
    clearDiveDepthBuffs();
    pet_.armed.lockoutActive = false;
    lockoutItemsContext_ = false;
    evolveTo_ = nullptr;
    pet_.dyingArmed = false;
    pet_.dyingElapsedMs = 0;
    csfBeat_ = 0;
    nav_ = Nav::ModalCSF;
    dirty_ = true;
    playSound(Sound::PetLost);
}

void Game::acknowledgeCSF() {
    // The pet becomes a permanent [CORRUPTED] ARCH record (greyed, NO rack slot,
    // ) and the active save is vacated -> the Decryption Hatch. Not
    // logged in v1; the record is the lasting trace.
    if (pet_.creature) {
        SaveRecord rec;
        std::strncpy(rec.id, pet_.creature->id, kSaveIdCap - 1);
        rec.status = static_cast<uint8_t>(RecordStatus::Corrupted);
        rec.generation = pet_.generation;
        records_.push_back(rec);
    }
    csfBeat_ = 0;
    pet_.dyingArmed = false;
    pet_.dyingElapsedMs = 0;
    startHatch();      // pet_.creature = nullptr -> line-select, then a fresh egg
    persistSave();     // the record + vacated active survive an immediate reboot
}

}  // namespace mal
