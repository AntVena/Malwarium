#include "core/app/game.h"

#include <cstdio>

#include "tunables.h"
#include "core/content/content_themes.h"   // themeForChip: a chip is inert here, and loot once
#include "core/ui/items_screen.h"

namespace mal {

// Combat-stat labels come from game_internal.h::levelStatName, so the Rollback picker,
// the post-encounter level-up readout, the permanent-grant log line below and the serial
// trace all name a stat the same way.

// --- ITEMS -----------------------------------------------------------------

void Game::openItemDetail(const ItemDef& d) {
    detailItem_ = &d;
    nav_ = Nav::Detail;
}

void Game::returnToLockout() { nav_ = Nav::ModalLockout; }

void Game::itemFilterReleaseB() {
    // Only the screen check here: onButton owns bHeld_, and clears it once after
    // offering the release to all four B gestures. Clearing it in here would disarm
    // the three that had not been asked yet.
    if (!(nav_ == Nav::Submenu && face_ == Face::Pet && enteredId() == SubmenuId::Items))
        return;   // bHeld_ was armed elsewhere (the VAULT / CFG / MOVES holds)
    items_.releaseB(*this);
    dirty_ = true;
    lastInputMs_ = nowMs_;
}

bool Game::itemUsable(const ItemDef& d, const char*& gateMsg) const {
    if (inventory_.count(d.id) <= 0) { gateMsg = "- NONE LEFT -"; return false; }
    // Boot-Sector egg: only quest items are usable (an egg can't be fed or buffed).
    // Quest items still fall through to their own context gate below.
    if (inEggPhase() && d.type != ItemDef::Type::Quest) {
        gateMsg = "USABLE AFTER HATCH"; return false;
    }
    // Warp keys are spent only on the walk (the B warp picker), so a stray Use
    // from ITEMS cannot burn one.
    if (d.walkWarp != ItemDef::WalkWarp::None) {
        gateMsg = "USE ON THE WALK"; return false;
    }
    // Rollback is inert with nothing to shed: a level-0 pet has no earned
    // points, so the picker would be empty. Gate it clearly instead of opening blank.
    if (d.use == ItemDef::Use::Rollback && combatLevel_ <= 0) {
        gateMsg = "NO LEVELS TO ROLL"; return false;
    }
    // Repartition needs a point to move, and level equals the count of earned points,
    // so a level-0 pet has none.
    if (d.use == ItemDef::Use::Repartition && combatLevel_ <= 0) {
        gateMsg = "NO POINTS TO MOVE"; return false;
    }
    // The Boot Accelerator only shortens an egg's incubation, so it does nothing once hatched.
    if (d.use == ItemDef::Use::DecryptEgg && !inEggPhase()) {
        gateMsg = "USABLE ON EGG ONLY"; return false;
    }
    // A palette chip is not spent to apply a theme, and applying one is not a thing done
    // to the pet: holding it EVER is the whole unlock (content_themes.h), and the set is
    // chosen on the CFG row this points at. So Use is inert here and says where to go,
    // the way a sealed cache points at the VAULT.
    if (themeForChip(d.id)) { gateMsg = "SET IN CFG > THEME"; return false; }
    // the Defrag Tool is spent only by a TOOL DEFRAG in MAINT (a guaranteed clean),
    // so a stray Use from ITEMS cannot burn it.
    if (d.use == ItemDef::Use::GuaranteeDefrag) {
        gateMsg = "USE IN MAINT DEFRAG"; return false;
    }
    // Sealed caches are now decrypted from the Hacker VAULT, never
    // pet-side ITEMS. Gate here so the detail action reads OPEN IN VAULT (A+C) and B is inert.
    if (d.use == ItemDef::Use::OpenContainer) {
        gateMsg = "OPEN IN VAULT (A+C)"; return false;
    }
    // The Decryptogram is cashed in at the same VAULT, for the same reason: what it buys
    // is a player-level unlock, not anything done to the pet.
    if (d.use == ItemDef::Use::PlayCryptogram) {
        gateMsg = "CASH IN AT VAULT (A+C)"; return false;
    }
    // A LOCKING device (a soak, a hold) owns the boundary while it is in, so nothing else
    // in the family goes in beside it: no divert, branch override or second soak. The
    // Eject-USB is the one exception, because pulling that device is its job: a port that
    // could only be emptied by the boundary it was refusing to reach would be a trap
    // rather than a decision. Checked ahead of the inert test below so the refusal names
    // the port rather than the state of whatever was being plugged in.
    if (usbPortLocked() && itemIsUsb(d) && !itemEjectsUsbPort(d)) {
        gateMsg = "USB PORT IN USE"; return false;
    }
    // ...and a soak only goes in at a stage its own row reaches: PROCESS for both, plus
    // SCRIPT for the late one (itemSoakReachesScript). A Boot egg has no evolution clock
    // to stretch and a Daemon has no boundary left, so neither is ever a soak's stage.
    if (const int soak = itemEvolveSoakFactor(d); soak > 0) {
        const bool ok = pet_ && !inEggPhase() &&
                        (pet_->stage == Stage::Process ||
                         (pet_->stage == Stage::Script && itemSoakReachesScript(d)));
        if (!ok) {
            gateMsg = itemSoakReachesScript(d) ? "PROCESS OR SCRIPT ONLY"
                                               : "PROCESS STAGE ONLY";
            return false;
        }
    }
    // If the state already holds, say so and keep the item.
    if (itemUseIsInert(d, gateMsg)) return false;
    if (lockoutItemsContext_) {
        if (itemResolvesLockout(d)) return true;
        gateMsg = "- WONT RESOLVE LOCKOUT -";
        return false;
    }
    switch (d.context) {
        case ItemDef::Context::Anytime: return true;
        case ItemDef::Context::LockoutOnly:
            gateMsg = "USABLE IN LOCKOUT ONLY"; return false;
        case ItemDef::Context::PreEncounter:
            gateMsg = "USABLE BEFORE ENCOUNTER"; return false;
    }
    return false;
}

bool Game::lifetimeItemSpent(const ItemDef& d) const {
    // One question, one answer: lifetimeGrantSpent (core/model/pet_upgrades.h) reads the
    // gates off the effect vocabulary, so the ITEMS list, the detail page's once-per-life
    // note and the 'Pedia's pet page can never disagree about what this pet has taken.
    // itemUseIsInert below asks the same thing effect by effect, to decide whether a USE
    // would achieve anything; this asks it about the ITEM, which is what a readout wants.
    return lifetimeGrantSpent(d, petLifetimeGates());
}

bool Game::itemUseIsInert(const ItemDef& d, const char*& why) const {
    // Only the plain consume path applies effects[]; every other Use hands off to
    // its own flow and is gated by itemUsable on its own terms.
    if (d.use != ItemDef::Use::Consume) return false;

    const char* reason = nullptr;
    int armingEffects = 0;
    for (const ItemEffect& e : d.effects) {
        switch (e.kind) {
            case ItemEffect::Kind::None:
                continue;
            // A vitals lever always lands. Feeding a full pet wastes some of
            // the fill, but that is the player's choice. Only a use that does
            // nothing at all is refused.
            case ItemEffect::Kind::Hunger:
            case ItemEffect::Kind::HungerStacking:
            case ItemEffect::Kind::Happy:
            case ItemEffect::Kind::HappyToward50:
            case ItemEffect::Kind::Frag:
                return false;
            case ItemEffect::Kind::RemoveCareMistakeOnce:
                ++armingEffects;
                if (!yubiConsumed_) return false;
                if (!reason) reason = "ALREADY SPENT THIS LIFE";
                break;
            case ItemEffect::Kind::ClearMistakeShieldOnce:
                ++armingEffects;
                if (!shieldItemConsumed_) return false;
                if (!reason) reason = "ALREADY SPENT THIS LIFE";
                break;
            case ItemEffect::Kind::ForceTrojanDivert:
                ++armingEffects;
                if (!forceTrojanDivert_) return false;
                if (!reason) reason = "DIVERT ALREADY ARMED";
                break;
            // The branch-override pair shares ONE slot, so a second device pointing the
            // way the slot already points would buy nothing. Pointing it the other way
            // always does something (the most recently plugged-in device wins), and so
            // does arming it over a care record that agrees today, because the record can
            // change and the override cannot.
            case ItemEffect::Kind::ForceEvolveBranchGood:
                ++armingEffects;
                if (evolveBranchOverride_ != BranchOverride::Good) return false;
                if (!reason) reason = "GOOD BRANCH ARMED";
                break;
            case ItemEffect::Kind::ForceEvolveBranchBad:
                ++armingEffects;
                if (evolveBranchOverride_ != BranchOverride::Bad) return false;
                if (!reason) reason = "BAD BRANCH ARMED";
                break;
            // The two locking devices. Both are unreachable while the port gate above
            // stands (it refuses every USB but the Eject whenever either is armed): they
            // state the same fact on the effect side, so the vocabulary does not depend on
            // the order of two gates.
            case ItemEffect::Kind::ArmEvolveSoak:
            case ItemEffect::Kind::ArmEvolveSoakLate:
                ++armingEffects;
                if (!usbPortLocked()) return false;
                if (!reason) reason = "USB PORT IN USE";
                break;
            case ItemEffect::Kind::ArmEvolveHold:
                ++armingEffects;
                if (!usbPortLocked()) return false;
                if (!reason) reason = "USB PORT IN USE";
                break;
            case ItemEffect::Kind::ClearUsbPort:
                // The Eject is the device a player will actually try with nothing to do.
                // It is meant for a locked port, so on an empty port it is refused and
                // kept.
                ++armingEffects;
                if (usbPortOccupied()) return false;
                if (!reason) reason = "USB PORT EMPTY";
                break;
            case ItemEffect::Kind::ArmCombatShieldBuff:
                // Re-arming only ever REPLACES the deadline, so a second one over
                // a live shield trades an item for nothing. The STAT BUFFS page
                // carries the time remaining.
                ++armingEffects;
                if (!backupShieldArmed()) return false;
                if (!reason) reason = "SHIELD ALREADY ARMED";
                break;
            case ItemEffect::Kind::ArmDeepWebDepthMultiplier:
                // These overwrite rather than stack, so a weaker one after a
                // stronger one is a downgrade paid for with an item.
                ++armingEffects;
                if (e.magnitude > deepWebDepthMultiplier_) return false;
                if (!reason) reason = "STRONGER ONE ARMED";
                break;
            // Both bells are measured against the depth a dive would actually start at
            // (armedDeepWebStartDepth), and not the raw pending field: a live Checkpoint
            // arming is a negative SENTINEL there, so comparing against it would let any
            // bell (including a second Checkpoint, which can only re-arm what is already
            // armed) spend itself over a deeper start.
            case ItemEffect::Kind::SetDeepWebStartDepth:
                ++armingEffects;
                if (e.magnitude > armedDeepWebStartDepth()) return false;
                if (!reason) reason = "DEEPER START ARMED";
                break;
            case ItemEffect::Kind::SetDeepWebStartDepthToBest:
                ++armingEffects;
                if (bestDeepWebDepth_ > armedDeepWebStartDepth()) return false;
                if (!reason) reason = "DEEPER START ARMED";
                break;
            // The pay bonus rides on the arming beside it, which already decides
            // whether this use achieves anything, so it neither counts as an arming
            // nor answers the question on its own.
            case ItemEffect::Kind::DiveStartBonusPct:
            case ItemEffect::Kind::DiveStepBonusPct:
                break;
            case ItemEffect::Kind::CutIncubationMin:
                // The clock is already inside the reveal window, where the cut is
                // floored, so using one would move nothing.
                ++armingEffects;
                if (inEggPhase() && bootHatchRemainMs_ > kHatchRevealMs) return false;
                if (!reason) reason = "SHELL ALREADY CRACKING";
                break;
            case ItemEffect::Kind::ClearReplicationGhost:
                // Nothing to cut loose on a pet with no ghost. Only reachable for a row
                // that carries NOTHING else. Unlinkguine's Hunger fill already
                // answers "not inert" above, which is the intent: it stays an ordinary
                // food, and only its cure half is conditional.
                ++armingEffects;
                if (model_.hasGhost()) return false;
                if (!reason) reason = "NO REPLICATION GHOST";
                break;
            // None of these can achieve nothing. An Epic dish whose permanent grant is
            // already spent is still food: a second helping feeds the pet, so the use is
            // allowed. What a spent grant DOES change is the readout: the 'Pedia and the
            // detail screen say so through itemIsOncePerPetLifetime / lifetimeItemSpent,
            // which is where that fact belongs.
            case ItemEffect::Kind::BandwidthRegenBonusMin:
            case ItemEffect::Kind::StatPointPower:
            case ItemEffect::Kind::StatPointDefense:
            case ItemEffect::Kind::StatPointSpeed:
            case ItemEffect::Kind::StatPointHealth:
            case ItemEffect::Kind::XpRateBonusPct:
            case ItemEffect::Kind::Bandwidth:
                return false;
        }
    }
    // A row with no arming effects at all does its work through a hand-off field
    // (combatHeal, preEncounterXp, a warp key), never inert on this axis.
    if (armingEffects == 0 || !reason) return false;
    why = reason;
    return true;
}

void Game::onItemsDetail(const ButtonEvent& ev) {
    if (ev.button == Button::B) useItem();
    else if (ev.button == Button::C) nav_ = Nav::Submenu;
}

void Game::useItem() {
    if (!detailItem_) return;
    const char* gate = "";
    if (!itemUsable(*detailItem_, gate)) return;   // inert when gated
    const ItemDef& d = *detailItem_;

    // Openable containers (sealed caches) are decrypted from the Hacker VAULT: itemUsable
    // gates them ("OPEN IN VAULT (A+C)"), so useItem never reaches this point for one.
    // openSealedCache lives on and is called from onHackerVault.

    // Rollback: Use opens the stat picker instead of feeding/buffing. The
    // item is only consumed when a shed is confirmed (onRollbackPicker B): cancelling
    // (C) leaves it in the bag. detailItem_ stays set so the shed path can consume it.
    if (d.use == ItemDef::Use::Rollback) { openRollbackPicker(); return; }

    // Repartition: the same hand-off, to the two-step picker that moves a point instead
    // of shedding one. Consumed only on the commit (onRepartitionPicker B at the TO
    // step): backing out of either step leaves it in the bag.
    if (d.use == ItemDef::Use::Repartition) { openRepartitionPicker(); return; }

    // Boot Accelerator: Use shortens the egg's incubation instead of feeding.
    if (d.use == ItemDef::Use::DecryptEgg) { useBootAccelerator(d); return; }

    if (d.type == ItemDef::Type::Food) {
        startFeeding(d, lockoutItemsContext_);
        return;
    }
    // Active quest item used inside a Lockout: resolves it immediately.
    if (lockoutItemsContext_ && itemResolvesLockout(d)) {
        inventory_.remove(d.id, 1);
        char buf[28];
        std::snprintf(buf, sizeof(buf), "USED %s", d.displayName);
        log_.push(LogEventType::ItemUsed, buf);
        resolveLockout();
        return;
    }
    // Buff (normal context): applied immediately, count decrements. Every pet lever
    // (Happiness, the Yubi-Cookie mistake removal, the Restore Point shield) is one of
    // the item's effects[], applied by the one applier; no per-id branching here.
    inventory_.remove(d.id, 1);
    applyItemEffects(d);
    noteCareSignal(DominantSignal::Play);   // a happiness buff reads as play
    char buf[28];
    std::snprintf(buf, sizeof(buf), "USED %s", d.displayName);
    log_.push(LogEventType::ItemUsed, buf);
    markSaveDirty();
    if (inventory_.count(d.id) <= 0) nav_ = Nav::Submenu;  // item left the list
}

void Game::noteLifetimeGrant(const char* what, const char* valueFmt, int magnitude) {
    char value[12];
    std::snprintf(value, sizeof(value), valueFmt, magnitude);
    char buf[28];
    std::snprintf(buf, sizeof(buf), "%s %s FOR LIFE", what, value);
    log_.push(LogEventType::ItemUsed, buf);
}

void Game::applyItemEffects(const ItemDef& d) {
    // Apply the item's on-Use pet levers (its ItemEffect list, which defines
    // everything an item does to the pet). Hunger/Happy accumulate into ONE
    // PetModel::feed so a food's fill lands as a single beat; the rest apply in place.
    // Every magnitude comes off the item row (defs.h): none live in tunables.
    int hunger = 0, happy = 0;
    for (const ItemEffect& e : d.effects) {
        switch (e.kind) {
            case ItemEffect::Kind::None:
                break;
            case ItemEffect::Kind::Hunger:
                hunger += e.magnitude;
                break;
            case ItemEffect::Kind::HungerStacking:
                // Polltatoes: the run counts THIS bite too, so eating one alone still
                // fills magnitude. The counter is reset by the next passive Hunger
                // decay (Game::tickHungerAndAwardXp): a fresh boot starts a fresh
                // run, which keeps a transient combo counter out of the save blob.
                ++stackingFoodRun_;
                hunger += e.magnitude * stackingFoodRun_;
                break;
            case ItemEffect::Kind::Happy:
                happy += e.magnitude;
                break;
            case ItemEffect::Kind::Frag:
                model_.setFragmentation(model_.fragmentation() + e.magnitude);
                break;
            case ItemEffect::Kind::HappyToward50: {
                // The "empty feeling": pull Happiness TOWARD 50% from either side
                // by magnitude, no overshoot. Numbs a too-happy pet as much as it lifts
                // a miserable one, unlike the flat Happy add.
                const int h = model_.happiness();
                if (h > 50) {
                    const int next = h - e.magnitude;
                    model_.setHappiness(next < 50 ? 50 : next);
                } else if (h < 50) {
                    const int next = h + e.magnitude;
                    model_.setHappiness(next > 50 ? 50 : next);
                }
                break;
            }
            case ItemEffect::Kind::RemoveCareMistakeOnce:
                // Yubi-Cookie ("Once per pet"): remove mistakes ONCE, then the
                // per-pet flag latches it; already-spent → no-op.
                if (!yubiConsumed_) {
                    model_.addCareMistake(-e.magnitude);
                    yubiConsumed_ = true;
                }
                break;
            case ItemEffect::Kind::ClearMistakeShieldOnce:
                // Restore Point (save v21): arm the next-mistake shield ONCE per pet;
                // already-spent → no-op.
                if (!shieldItemConsumed_) {
                    mistakeShieldActive_ = true;
                    shieldItemConsumed_ = true;
                }
                break;
            case ItemEffect::Kind::ForceTrojanDivert:
                // Ambig-USB (save v28): arm a guaranteed Trojan divert at the pet's
                // next Process->Script evolution (Game::fireEvolution), replacing the
                // kTrojanDivertPct roll. Re-armable (unlike the once-per-lifetime
                // shields above): buying another does no harm.
                forceTrojanDivert_ = true;
                break;
            case ItemEffect::Kind::ForceEvolveBranchGood:
            case ItemEffect::Kind::ForceEvolveBranchBad:
                // Signed-USB / Bad-USB (save v60): point the next branching evolution at
                // the Good or the Bad successor whatever the care budget says. One slot,
                // so this overwrites. Plugging in the opposite device changes the choice,
                // and the last one in fires. Consumed at the evolution it steers
                // (Game::completeEvolution).
                evolveBranchOverride_ = e.kind == ItemEffect::Kind::ForceEvolveBranchBad
                                            ? BranchOverride::Bad
                                            : BranchOverride::Good;
                break;
            case ItemEffect::Kind::ArmEvolveSoak:
            case ItemEffect::Kind::ArmEvolveSoakLate:
                // Sandbox/Hypervisor-USB (save v60): stretch this stage's evolution dwell
                // by the factor and pay the same factor on every XP award while it runs
                // (Game::evolveDwellMs, Game::addCombatXp). Which of the two Kinds it is
                // decides only WHERE it may go in (itemUsable): the arming is identical,
                // and the doubled Script clock comes from the pet's stage. It cannot
                // overwrite another soak: itemUsable refuses every USB but the Eject
                // while one is armed, so this only ever writes into an empty port.
                // Floored at 1 the way the codec floors it: the factor multiplies a clock
                // AND an XP award, so a row authored at 0 would stall a pet.
                evolveSoakFactor_ = e.magnitude > 1 ? e.magnitude : 1;
                break;
            case ItemEffect::Kind::ArmEvolveHold:
                // Halt-USB (save v60): stop the pet reaching a boundary at all
                // (Game::evolveEligible). Never consumed by an evolution, because none
                // arrives while it is in: only an Eject-USB, a rack swap or a new egg
                // takes it out.
                evolveHold_ = true;
                break;
            case ItemEffect::Kind::ClearUsbPort:
                // Eject-USB: pull whatever is in the port and drop its effect. One call
                // rather than a list here, so a device added to the family is undone by
                // naming it in Game::clearUsbPort and nowhere else.
                clearUsbPort();
                log_.push(LogEventType::ItemUsed, "USB PORT CLEARED");
                break;
            case ItemEffect::Kind::ArmCombatShieldBuff:
                // Backup Drive (save v30): arm the timed combat shield, magnitude
                // MINUTES from now (Game::buildPlayerCombatant reads the deadline).
                // Re-armable: using another one just refreshes the window.
                backupShieldUntilMs_ = lifetimeUptimeMs() + static_cast<uint32_t>(e.magnitude) * 60u * 1000u;
                break;
            case ItemEffect::Kind::ArmDeepWebDepthMultiplier:
                // Deep-Learning Module/Core: overwrites, never stacks. Buying a second one
                // just replaces the multiplier. Armed AHEAD of the dive it applies to
                // (Context::Anytime), so it is cleared only where a DIVE ENDS. The Stop
                // row and a combat loss, both guarded on inDeepWebDive(), and on a new
                // pet (game_lifecycle.cpp). Deliberately NOT tied to exploreStreak_'s
                // reset: the streak resets at both ends of a run, so following it would
                // clear the buff on the way INTO the dive it was bought for.
                deepWebDepthMultiplier_ = e.magnitude;
                diveStepBonusPct_ = 0;   // the row's own DiveStepBonusPct, if any, follows
                break;
            case ItemEffect::Kind::BandwidthRegenBonusMin:
                // Tiramisudo (save v50): the FIRST helping shaves magnitude minutes off
                // this pet's Bandwidth regen for good; later ones latch out here and
                // leave only the row's ordinary food levers, including its +Bandwidth.
                // Granting once is the whole design: the upgrade is per-pet, so the way
                // to have it twice is to raise a second pet, not to eat a second plate.
                if (upgrades_.bandwidthRegenMin == 0) {
                    upgrades_.bandwidthRegenMin = e.magnitude;
                    noteLifetimeGrant("BW REGEN", "-%dMIN", e.magnitude);
                }
                break;
            case ItemEffect::Kind::StatPointPower:
            case ItemEffect::Kind::StatPointDefense:
            case ItemEffect::Kind::StatPointSpeed:
            case ItemEffect::Kind::StatPointHealth: {
                // An Epic dish's off-level point. Granted once per pet PER STAT (four
                // dishes, four stats, four upgrades a pet can hold at most), and latched
                // out after that so the plate keeps feeding without stacking. It lands
                // outside statPoints_ on purpose: the level stays equal to the sum of
                // EARNED points, which is the invariant Rollback sheds against, so
                // nothing here can ever be rolled back off the pet.
                const int stat = statPointEffectIndex(e.kind);
                if (stat >= 0 && upgrades_.statBonus[stat] == 0) {
                    upgrades_.statBonus[stat] = e.magnitude;
                    noteLifetimeGrant(levelStatWord(stat), "+%d", e.magnitude);
                }
                break;
            }
            case ItemEffect::Kind::XpRateBonusPct:
                // Profilerole: raises what every XP source pays this pet, for good.
                // First helping only, same as the grants above.
                if (upgrades_.xpRatePct == 0) {
                    upgrades_.xpRatePct = e.magnitude;
                    noteLifetimeGrant("XP RATE", "+%d%%", e.magnitude);
                }
                break;
            case ItemEffect::Kind::Bandwidth:
                // A top-up of the live pool, capped at the ceiling the rig has bought:
                // the same clamp grantRigLevel applies, because overfilling would give
                // shielded fights the operator has not paid for.
                bandwidth_ += e.magnitude;
                if (bandwidth_ > bandwidthMax()) bandwidth_ = bandwidthMax();
                break;
            case ItemEffect::Kind::SetDeepWebStartDepth:
                // Backdoor/Rootkit/Kernel Bell: arm the NEXT startDeepWebDive() to
                // begin at this fixed depth instead of 0; consumed there.
                pendingDeepWebStartDepth_ = e.magnitude;
                pendingDiveStartBonusPct_ = 0;   // ...and this row's bonus, if any, follows
                break;
            case ItemEffect::Kind::SetDeepWebStartDepthToBest:
                // Checkpoint Bell: arm the next dive to start at THIS PET's own
                // bestDeepWebDepth_, resolved at dive-start (not here) so improving
                // the record between now and then is never stale.
                pendingDeepWebStartDepth_ = kDeepWebStartDepthUseBest;
                pendingDiveStartBonusPct_ = 0;
                break;
            case ItemEffect::Kind::ClearReplicationGhost:
                // Unlinkguine: cut the phantom process off from the pet it copied.
                // Guarded on having a ghost, so the achievement marks curing one. Eating
                // it as ordinary food unlocks nothing.
                if (model_.hasGhost()) {
                    model_.setGhost(false);
                    log_.push(LogEventType::ItemUsed, "GHOST CLEARED");
                    unlockAchievement(ach::kAirGapped);
                }
                break;
            case ItemEffect::Kind::DiveStartBonusPct:
                // Armed with the bell's start depth; goes live at startDeepWebDive.
                pendingDiveStartBonusPct_ = e.magnitude;
                break;
            case ItemEffect::Kind::DiveStepBonusPct:
                diveStepBonusPct_ = e.magnitude;
                break;
            case ItemEffect::Kind::CutIncubationMin: {
                // Boot Accelerator: a flat bite out of the incubation clock, floored at
                // kHatchRevealMs. The last stretch is where the player cracks the shell
                // by hand (hatchRevealReady), so a cut that skipped past it would take
                // that away rather than hand it over.
                const uint32_t cutMs = static_cast<uint32_t>(e.magnitude) * 60u * 1000u;
                const uint32_t left = bootHatchRemainMs_ > cutMs ? bootHatchRemainMs_ - cutMs : 0;
                if (bootHatchRemainMs_ > kHatchRevealMs)
                    bootHatchRemainMs_ = left > kHatchRevealMs ? left : kHatchRevealMs;
                break;
            }
        }
    }
    if (hunger != 0 || happy != 0) model_.feed(hunger, happy);
}

void Game::useBootAccelerator(const ItemDef& d) {
    // The wait, shortened, and nothing else. Every line's hatch minigame is played once,
    // at lay-time, so by the time an egg is sitting there being looked at there is no
    // game left for an item to open. How much it cuts is the row's own CutIncubationMin
    // effect, applied by the one applier. Its own path rather than the buff path below
    // because an egg isn't being played with: no care signal is noted.
    // itemUsable already guaranteed we're in the egg phase.
    inventory_.remove(d.id, 1);
    char buf[28];
    std::snprintf(buf, sizeof(buf), "USED %s", d.displayName);
    log_.push(LogEventType::ItemUsed, buf);
    applyItemEffects(d);
    markSaveDirty();
}

void Game::rollPantrySpoilage() {
    // One roll per held perishable STACK, converting a single unit, so a larder of
    // twenty rots no faster than a larder of one. Collected first because converting
    // mutates the vector the roll is walking.
    struct Turned { const ItemDef* from; const char* into; };
    Turned turned[4] = {};
    int n = 0;
    for (const Inventory::Stack& s : inventory_.stacks()) {
        if (n == 4) break;
        const ItemDef* d = registry_.item(s.id);
        if (!d || !d->spoil.into || d->spoil.pct <= 0) continue;
        rng_ = rng_ * 1664525u + 1013904223u;
        if (static_cast<int>((rng_ >> 16) % 100) >= d->spoil.pct) continue;
        turned[n++] = {d, d->spoil.into};
    }
    for (int i = 0; i < n; ++i) {
        inventory_.remove(turned[i].from->id, 1);
        inventory_.add(turned[i].into, 1);
        // Say so: an item quietly becoming a worse item is the kind of change a player
        // would otherwise only notice as a miscount.
        char buf[28];
        std::snprintf(buf, sizeof(buf), "%s SPOILED", turned[i].from->displayName);
        log_.push(LogEventType::CareMistake, buf);
    }
    if (n > 0) markSaveDirty();
}

void Game::startFeeding(const ItemDef& d, bool fromLockout) {
    inventory_.remove(d.id, 1);
    // Snapshot first: the modal's gauges report the DELTA this bite achieved, and
    // a stat that was already capped has none to show.
    feedBefore_ = {model_.hunger(), model_.fragmentation(), model_.happiness()};
    applyItemEffects(d);                        // Hunger/Happy fill + any frag/happy-pull
    noteCareSignal(DominantSignal::Feeding);   // dominant-signal tally
    // Feeding from the crisis resolves it immediately: disarm the deadline so it
    // can't penalise the pet mid-eat (endFeeding finalises the return to idle).
    if (fromLockout) lockoutActive_ = false;
    char buf[28];
    std::snprintf(buf, sizeof(buf), "FED %s", d.displayName);
    log_.push(LogEventType::ItemUsed, buf);
    // After the FED line, so a favourite found by this bite logs as its consequence.
    markFoodEaten(d);                          // this pet's palate (STAT's FOODS grid)
    feedItem_ = &d;
    feedBeat_ = 0;
    fxBeat_ = 0;              // the bite's own dissolve clock
    feedFromLockout_ = fromLockout;
    rollPantrySpoilage();          // opening the bag is what disturbs the rest of it
    markSaveDirty();
    nav_ = Nav::ModalFeeding;
}

void Game::endFeeding() {
    if (feedFromLockout_) {
        resolveLockout();                 // -> idle
    } else if (detailItem_ && inventory_.count(detailItem_->id) > 0) {
        nav_ = Nav::Detail;               // back to the detail it came from
    } else {
        nav_ = Nav::Submenu;              // item ran out -> back to the list
    }
    feedItem_ = nullptr;
    feedFromLockout_ = false;
}


}  // namespace mal
