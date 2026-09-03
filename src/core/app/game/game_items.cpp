#include "core/app/game.h"
#include "core/app/game_internal.h"

#include <cstdio>
#include <cstring>

#include "tunables.h"
#include "core/content/content_tables.h"
#include "core/ui/items_screen.h"

namespace mal {

// Combat-stat labels come from game_internal.h::levelStatName, so the Rollback picker,
// the post-encounter level-up readout, the permanent-grant log line below and the serial
// trace all name a stat the same way.

// --- ITEMS -----------------------------------------------------------------

void Game::onItemsPicker(const ButtonEvent& ev) {
    const auto tiles = buildItemPickerRows(registry_, inventory_);
    const int n = static_cast<int>(tiles.size());
    if (n <= 0) { if (ev.button == Button::C) nav_ = Nav::Cursor; return; }
    if (itemPickRow_ < 0 || itemPickRow_ >= n) itemPickRow_ = 0;

    if (ev.button == Button::A) {
        itemPickRow_ = (itemPickRow_ + 1) % n;
    } else if (ev.button == Button::B) {
        // Commit the tile and drill in. An EMPTY category still opens — the list
        // says "- NO MATCHING ITEMS -" and C walks back, which reads better than a
        // dead button on a row the screen just drew.
        itemFilter_ = tiles[itemPickRow_].filter;
        itemsScreen_ = ItemsScreen::List;
        auto rows = buildInventoryRows(registry_, inventory_, false, itemFilter_);
        listRow_ = firstSelectableRow(rows);
        if (listRow_ < 0) listRow_ = 0;
    } else if (ev.button == Button::C) {
        nav_ = Nav::Cursor;
    }
}

void Game::onItemsList(const ButtonEvent& ev) {
    auto rows = buildInventoryRows(registry_, inventory_, lockoutItemsContext_, itemFilter_);
    if (ev.button == Button::A) {
        // A is the plain step here and holding it repeats (Game::tickListNav) — the
        // filter gesture lives on B below, so nothing has to share this button.
        if (!rows.empty()) listRow_ = stepSelectableRow(rows, listRow_, +1);
    } else if (ev.button == Button::B) {
        // Once the type-tabs upgrade is owned, B becomes a tap/hold gesture — arm here
        // and resolve on the button-RELEASE edge (a short tap -> itemFilterReleaseB
        // opens the focused item, same as always) or on the hold crossing
        // kItemFilterHoldMs (tick() cycles the filter instead). Unowned players see
        // zero change: B still opens the detail immediately on press.
        if (itemTabsUnlocked()) {
            bHeld_ = true;
            bDownMs_ = nowMs_;
        } else {
            openFocusedItem();
        }
    } else if (ev.button == Button::C) {
        // Back one layer: the Lockout modal, else the type-picker that opened this
        // list, else out to the carousel.
        if (lockoutItemsContext_) nav_ = Nav::ModalLockout;
        else if (itemPickerUnlocked()) itemsScreen_ = ItemsScreen::Picker;
        else nav_ = Nav::Cursor;
    }
}

// Open the focused row's detail. A header row has no item behind it, so it is inert —
// the cursor never rests on one, but a list rebuilt under a held button can move.
void Game::openFocusedItem() {
    const auto rows =
        buildInventoryRows(registry_, inventory_, lockoutItemsContext_, itemFilter_);
    if (listRow_ < 0 || listRow_ >= static_cast<int>(rows.size())) return;
    if (rows[listRow_].header) return;
    detailItem_ = rows[listRow_].def;
    nav_ = Nav::Detail;
}

void Game::itemFilterReleaseB() {
    // Only the screen check here — onButton owns bHeld_, and clears it once after
    // offering the release to all four B gestures. Clearing it in here would disarm
    // the three that had not been asked yet.
    if (!(nav_ == Nav::Submenu && face_ == Face::Pet && enteredId() == SubmenuId::Items &&
          itemsScreen_ == ItemsScreen::List && itemTabsUnlocked()))
        return;   // bHeld_ was armed elsewhere (the VAULT / CFG / MOVES holds) — no-op
    openFocusedItem();
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
    // Warp keys are spent only ON the walk (the B warp picker), never from
    // the ITEMS use path — otherwise a stray Use would burn a key for nothing.
    if (d.walkWarp != ItemDef::WalkWarp::None) {
        gateMsg = "USE ON THE WALK"; return false;
    }
    // Rollback is inert with nothing to shed — a level-0 pet has no earned
    // points, so the picker would be empty. Gate it clearly instead of opening blank.
    if (d.use == ItemDef::Use::Rollback && combatLevel_ <= 0) {
        gateMsg = "NO LEVELS TO ROLL"; return false;
    }
    // The Boot Accelerator only shortens an egg's incubation — nothing to do once hatched.
    if (d.use == ItemDef::Use::DecryptEgg && !inEggPhase()) {
        gateMsg = "USABLE ON EGG ONLY"; return false;
    }
    // the Defrag Tool is spent only by a TOOL DEFRAG in MAINT (a guaranteed
    // clean), never from the ITEMS use path — a stray Use would burn it for nothing.
    if (std::strcmp(d.id, kDefragToolId) == 0) {
        gateMsg = "USE IN MAINT DEFRAG"; return false;
    }
    // Sealed caches are now decrypted from the Hacker VAULT, never
    // pet-side ITEMS. Gate here so the detail action reads DECRYPT IN VAULT and B is inert.
    if (d.use == ItemDef::Use::OpenContainer) {
        gateMsg = "DECRYPT IN VAULT"; return false;
    }
    // The Decryptogram is cashed in at the same VAULT, for the same reason: what it buys
    // is a player-level unlock, not anything done to the pet.
    if (d.use == ItemDef::Use::PlayCryptogram) {
        gateMsg = "CASH IN AT VAULT"; return false;
    }
    // A LOCKING device (a soak, a hold) owns the boundary while it is in, so nothing else
    // in the family goes in beside it — not a divert, not a branch override, not a second
    // soak. The Eject-USB is the one exception, because pulling that device is its job: a
    // port that could only be emptied by the boundary it was refusing to reach would be a
    // trap rather than a decision. Checked ahead of the inert test below so the refusal
    // names the port rather than the state of whatever was being plugged in.
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
    // Nothing left for this one to do — say so and keep the item, rather than
    // spending it on a state it already holds.
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
            // A vitals lever always lands. Feeding an already-full pet wastes
            // some of the fill, but that's the player's call to make — only a
            // use that can achieve LITERALLY nothing is worth refusing.
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
            // way the slot already points would buy nothing. Pointing it the OTHER way
            // always does something — that is what "most recently plugged in wins" means
            // — and so does arming it over a care record that happens to agree today,
            // since the record can still change and the override cannot.
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
            // stands (it refuses every USB but the Eject whenever either is armed) — kept
            // as the effect-side statement of the same fact, so the vocabulary answers for
            // itself rather than depending on the order of two gates.
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
                // The undo is the one device whose inert case a player will actually meet:
                // it goes in over a locked port on purpose, so the only time it achieves
                // nothing is on an EMPTY one — and then it is refused and kept rather
                // than spent on a port that is already the way it would leave it.
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
            // (armedDeepWebStartDepth), never the raw pending field: a live Zero-Day
            // arming is a negative SENTINEL there, so comparing against it would let any
            // bell — including a second Zero-Day, which can only re-arm what is already
            // armed — spend itself over a deeper start.
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
            case ItemEffect::Kind::ClearReplicationGhost:
                // Nothing to cut loose on a pet with no ghost. Only reachable for a row
                // that carries NOTHING else — Unlinkguine's Hunger fill already
                // answers "not inert" above, which is the intent: it stays an ordinary
                // food, and only its cure half is conditional.
                ++armingEffects;
                if (model_.hasGhost()) return false;
                if (!reason) reason = "NO REPLICATION GHOST";
                break;
            // None of these can achieve nothing. A permanent grant already spent is the
            // ordinary case for the Epic dish that carries it — a pet that is already
            // rooted, pointed or profiled eats the plate for the food, which is exactly
            // what a second helping is meant to be, so refusing the use would be
            // refusing a meal. What a spent grant DOES change is the readout: the 'Pedia
            // and the detail screen say so through itemIsOncePerPetLifetime /
            // lifetimeItemSpent, which is where that fact belongs.
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
    // (combatHeal, preEncounterXp, a warp key) — never inert on this axis.
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

    // Openable containers (sealed caches) are now decrypted from the Hacker VAULT
    // not here — itemUsable gates them ("DECRYPT IN VAULT"), so useItem never
    // reaches this point for one. openSealedCache lives on and is called from onHackerVault.

    // Rollback: Use opens the stat picker instead of feeding/buffing. The
    // item is only consumed when a shed is confirmed (onRollbackPicker B) — cancelling
    // (C) leaves it in the bag. detailItem_ stays set so the shed path can consume it.
    if (d.use == ItemDef::Use::Rollback) { openRollbackPicker(); return; }

    // Boot Accelerator: Use shortens the egg's incubation instead of feeding.
    if (d.use == ItemDef::Use::DecryptEgg) { useBootAccelerator(d); return; }

    if (d.type == ItemDef::Type::Food) {
        startFeeding(d, lockoutItemsContext_);
        return;
    }
    // Active quest item used inside a Lockout — resolves it immediately.
    if (lockoutItemsContext_ && itemResolvesLockout(d)) {
        inventory_.remove(d.id, 1);
        char buf[28];
        std::snprintf(buf, sizeof(buf), "USED %s", d.displayName);
        log_.push(LogEventType::ItemUsed, buf);
        resolveLockout();
        return;
    }
    // Buff (normal context): applied immediately, count decrements. Every pet lever —
    // Happiness, the Yubi-Cookie mistake removal, the Restore Point shield — is one of
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
    // Apply the item's on-Use pet levers (its ItemEffect list — the single source of
    // truth for what an item does to the pet). Hunger/Happy accumulate into ONE
    // PetModel::feed so a food's fill lands as a single beat; the rest apply in place.
    // Every magnitude comes off the item row (defs.h) — none live in tunables.
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
                // decay (Game::tickHungerAndAwardXp) — a fresh boot starts a fresh
                // run, which is the forgiving reading and keeps a transient combo
                // counter out of the save blob.
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
                // by magnitude, no overshoot — numbs a too-happy pet as much as it lifts
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
                // Yubi-Cookie ("Max 1 per lifecycle"): remove mistakes ONCE, then the
                // per-pet flag latches it — already-spent → no-op.
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
                // shields above) — buying another does no harm.
                forceTrojanDivert_ = true;
                break;
            case ItemEffect::Kind::ForceEvolveBranchGood:
            case ItemEffect::Kind::ForceEvolveBranchBad:
                // Signed-USB / Bad-USB (save v60): point the next branching evolution at
                // the Good or the Bad successor whatever the care budget says. ONE slot,
                // so this overwrites rather than stacks — plugging the opposite device in
                // is how a player changes their mind, and the last one in is the one that
                // fires. Consumed at the evolution it steers (Game::completeEvolution).
                evolveBranchOverride_ = e.kind == ItemEffect::Kind::ForceEvolveBranchBad
                                            ? BranchOverride::Bad
                                            : BranchOverride::Good;
                break;
            case ItemEffect::Kind::ArmEvolveSoak:
            case ItemEffect::Kind::ArmEvolveSoakLate:
                // Sandbox/Hypervisor-USB (save v60): stretch this stage's evolution dwell
                // by the factor and pay the same factor on every XP award while it runs
                // (Game::evolveDwellMs, Game::addCombatXp). Which of the two Kinds it is
                // decides only WHERE it may go in (itemUsable) — the arming is identical,
                // and the doubled Script clock is read off the pet's stage, not off here.
                // It cannot overwrite another soak: itemUsable refuses every USB but the
                // Eject while one is armed, so this only ever writes into an empty port.
                // Floored at 1 the way the codec floors it — the factor multiplies a clock
                // AND an XP award, so a row authored at 0 would stall a pet.
                evolveSoakFactor_ = e.magnitude > 1 ? e.magnitude : 1;
                break;
            case ItemEffect::Kind::ArmEvolveHold:
                // Halt-USB (save v60): stop the pet reaching a boundary at all
                // (Game::evolveEligible). Never consumed by an evolution, because none
                // arrives while it is in — an Eject-USB, a rack swap or a new egg are the
                // only ways out, which is what makes parking a pet a real commitment.
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
                // Re-armable — using another one just refreshes the window.
                backupShieldUntilMs_ = lifetimeUptimeMs() + static_cast<uint32_t>(e.magnitude) * 60u * 1000u;
                break;
            case ItemEffect::Kind::ArmDeepWebDepthMultiplier:
                // Deep-Learning Module/Core: overwrites, never stacks — buying a second one
                // just replaces the multiplier. Armed AHEAD of the dive it applies to
                // (Context::Anytime), so it is cleared only where a DIVE ENDS — the Stop
                // row and a combat loss, both guarded on inDeepWebDive() — and on a new
                // pet (game_lifecycle.cpp). Deliberately NOT tied to exploreStreak_'s
                // reset: the streak resets at both ends of a run, so following it would
                // clear the buff on the way INTO the dive it was bought for.
                deepWebDepthMultiplier_ = e.magnitude;
                break;
            case ItemEffect::Kind::BandwidthRegenBonusMin:
                // Tiramisudo (save v50): the FIRST helping shaves magnitude minutes off
                // this pet's Bandwidth regen for good; later ones latch out here and
                // leave only the row's ordinary food levers — including its +Bandwidth,
                // which is what a second helping is actually for. Granting once is the
                // whole design: the upgrade is per-pet, so the way to have it twice is
                // to raise a second pet, not to eat a second plate.
                if (upgrades_.bandwidthRegenMin == 0) {
                    upgrades_.bandwidthRegenMin = e.magnitude;
                    noteLifetimeGrant("BW REGEN", "-%dMIN", e.magnitude);
                }
                break;
            case ItemEffect::Kind::StatPointPower:
            case ItemEffect::Kind::StatPointDefense:
            case ItemEffect::Kind::StatPointSpeed:
            case ItemEffect::Kind::StatPointHealth: {
                // An Epic dish's off-level point. Granted once per pet PER STAT — four
                // dishes, four stats, four upgrades a pet can hold at most — and latched
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
                // A top-up of the live pool, capped at the ceiling the rig has bought —
                // the same clamp grantRigLevel applies, since overfilling would hand
                // back shielded fights the cap says the operator hasn't paid for.
                bandwidth_ += e.magnitude;
                if (bandwidth_ > bandwidthMax()) bandwidth_ = bandwidthMax();
                break;
            case ItemEffect::Kind::SetDeepWebStartDepth:
                // Backdoor/Rootkit/Kernel Bell: arm the NEXT startDeepWebDive() to
                // begin at this fixed depth instead of 0; consumed there.
                pendingDeepWebStartDepth_ = e.magnitude;
                break;
            case ItemEffect::Kind::SetDeepWebStartDepthToBest:
                // Zero-Day Bell: arm the next dive to start at THIS PET's own
                // bestDeepWebDepth_, resolved at dive-start (not here) so improving
                // the record between now and then is never stale.
                pendingDeepWebStartDepth_ = kDeepWebStartDepthUseBest;
                break;
            case ItemEffect::Kind::ClearReplicationGhost:
                // Unlinkguine: cut the phantom process off from the pet it copied.
                // Guarded on actually HAVING a ghost, so the achievement marks curing one
                // rather than owning the snack — eating it as ordinary food is the common
                // case and must not unlock anything.
                if (model_.hasGhost()) {
                    model_.setGhost(false);
                    log_.push(LogEventType::ItemUsed, "GHOST CLEARED");
                    unlockAchievement(ach::kAirGapped);
                }
                break;
        }
    }
    if (hunger != 0 || happy != 0) model_.feed(hunger, happy);
}

const ItemDef* Game::rollLootEntry(const LootEntry* pool, int poolSize) {
    // The one weighted walk over a loot pool, shared by every container's payout and
    // by the walk's own loot-cache event. Each row draws at its LootEntry::weight if it
    // names one, else at the item's own itemDropWeight() (which falls back to rarity) —
    // so a pool stays a bare id list until an entry genuinely needs to differ. Advances
    // the shared LCG, so a draw stays deterministic under a fixed seed.
    if (!pool || poolSize <= 0) return nullptr;
    auto weightOf = [this](const LootEntry& e) {
        if (e.weight > 0) return e.weight;
        const ItemDef* d = registry_.item(e.id);
        return d ? itemDropWeight(*d) : 0;
    };
    int total = 0;
    for (int i = 0; i < poolSize; ++i) total += weightOf(pool[i]);
    if (total <= 0) return nullptr;
    rng_ = rng_ * 1664525u + 1013904223u;
    int roll = static_cast<int>((rng_ >> 16) % static_cast<unsigned>(total));
    const LootEntry* hit = &pool[poolSize - 1];
    for (int i = 0; i < poolSize; ++i) {
        const int w = weightOf(pool[i]);
        if (roll < w) { hit = &pool[i]; break; }
        roll -= w;
    }
    return registry_.item(hit->id);
}

const ItemDef* Game::drawCacheItem(const LootEntry* pool, int poolSize) {
    const ItemDef* d = rollLootEntry(pool, poolSize);
    if (!d) return nullptr;
    inventory_.add(d->id, 1);
    char buf[32];
    std::snprintf(buf, sizeof(buf), "FOUND %s", d->displayName);
    log_.push(LogEventType::ItemGained, buf);
    return d;
}

void Game::grantCacheReward(const ItemDef& d, int& bitsOut, const ItemDef** itemsOut,
                            int& itemCountOut, int maxItems) {
    // Everything a cache pays is on its own row (ItemDef::cache, content_items.cpp) —
    // purse, draw count, pool, and whether it also rolls a mod. One uniform payout, so
    // adding a container is a row rather than a branch here.
    const CacheDef& c = d.cache;
    bitsOut = applyCacheBitsBonus(c.bits);   // Enhanced DataMining
    bits_ += bitsOut;
    itemCountOut = 0;
    for (int i = 0; i < c.draws; ++i) {
        if (c.drawChancePct < 100) {
            rng_ = rng_ * 1664525u + 1013904223u;
            if (static_cast<int>((rng_ >> 16) % 100) >= c.drawChancePct) continue;
        }
        const ItemDef* got = drawCacheItem(c.pool, c.poolSize);
        if (got && itemCountOut < maxItems) itemsOut[itemCountOut++] = got;
    }
    // A rich cache is also a second, non-boss MOD source: rolled globally rarity-weighted
    // (even a tier-4 mod can surface; its rolled equip-level gate holds it until the pet
    // is deep enough). Granted + logged here — the yield reveal shows the items/Bits, the
    // mod lands in MODS.
    if (c.modChancePct > 0) {
        rng_ = rng_ * 1664525u + 1013904223u;
        if (static_cast<int>((rng_ >> 16) % 100) < c.modChancePct)
            if (const char* id = rollAnyModId())
                grantMod(id);
    }
}

void Game::openSealedCache(const ItemDef& d) {
    // opening a container consumes it and draws its reward.
    // the yield (Bits + item names) is stashed and surfaced on Nav::CacheYield.
    inventory_.remove(d.id, 1);
    char opened[36];
    std::snprintf(opened, sizeof(opened), "OPENED %s", d.displayName);
    log_.push(LogEventType::ItemUsed, opened);

    cacheYieldCache_ = &d;   // ItemDef lives in the static registry — stable pointer
    cacheYieldItemCount_ = 0;
    cacheYieldBits_ = 0;
    grantCacheReward(d, cacheYieldBits_, cacheYieldItems_, cacheYieldItemCount_, 2);

    markSaveDirty();
    nav_ = Nav::CacheYield;   // show what came out
}

void Game::onCacheYield(const ButtonEvent& ev) {
    // Any confirm/back dismisses the reveal to the ITEMS list (the container is
    // already gone; the yield is in the bag + logged).
    if (ev.button == Button::B || ev.button == Button::C) nav_ = Nav::Submenu;
}

void Game::openAllCachesOfRarity(const ItemDef& focused) {
    // Bulk-open every copy of the FOCUSED cache in one action. Grouping is by the cache
    // itself, not by rarity: two containers may share a rarity while paying out of
    // completely different pools (an earned Commendation Cache and a found Epic one), and
    // "open all of these" should never quietly spend a prize alongside ordinary loot.
    struct Target { const ItemDef* def; int qty; };
    std::vector<Target> targets;
    for (const auto& s : inventory_.stacks()) {
        if (s.qty <= 0) continue;
        const ItemDef* d = registry_.item(s.id);
        if (!d || d->use != ItemDef::Use::OpenContainer) continue;
        if (std::strcmp(d->id, focused.id) == 0) targets.push_back({d, s.qty});
    }

    bulkYieldCache_ = &focused;
    bulkYieldCachesOpened_ = 0;
    bulkYieldBits_ = 0;
    bulkYieldTally_.clear();
    bulkYieldRow_ = 0;

    for (const Target& t : targets) {
        for (int i = 0; i < t.qty; ++i) {
            inventory_.remove(t.def->id, 1);
            ++bulkYieldCachesOpened_;
            int oneBits = 0;
            const ItemDef* oneItems[2] = {nullptr, nullptr};
            int oneCount = 0;
            grantCacheReward(*t.def, oneBits, oneItems, oneCount, 2);
            bulkYieldBits_ += oneBits;
            for (int k = 0; k < oneCount; ++k) {
                if (!oneItems[k]) continue;
                bool merged = false;
                for (auto& tally : bulkYieldTally_)
                    if (tally.def == oneItems[k]) { ++tally.count; merged = true; break; }
                if (!merged) bulkYieldTally_.push_back({oneItems[k], 1});
            }
        }
    }

    char opened[40];
    std::snprintf(opened, sizeof(opened), "BULK OPENED %d %s", bulkYieldCachesOpened_,
                  rarityName(focused.rarity));
    log_.push(LogEventType::ItemUsed, opened);
    markSaveDirty();
    nav_ = Nav::BulkYield;
}

void Game::onBulkYield(const ButtonEvent& ev) {
    const int n = static_cast<int>(bulkYieldTally_.size());
    if (ev.button == Button::A) {
        if (n > 0) bulkYieldRow_ = (bulkYieldRow_ + 1) % n;
    } else if (ev.button == Button::B || ev.button == Button::C) {
        nav_ = Nav::Submenu;   // back to the VAULT list the bulk-open came from
    }
}

void Game::useBootAccelerator(const ItemDef& d) {
    // A flat bite out of the incubation clock, and nothing else. Every line's hatch
    // minigame is played once, at lay-time, so by the time an egg is sitting there
    // being looked at there is no game left for an item to open — what is left is the
    // wait, and this shortens it. itemUsable already guaranteed we're in the egg phase.
    //
    // Floored at kHatchRevealMs rather than at zero: the last stretch of the clock is
    // where the player can crack the shell by hand and watch it (hatchRevealReady), so
    // an item that skipped past it would take that away rather than hand it over.
    inventory_.remove(d.id, 1);
    char buf[28];
    std::snprintf(buf, sizeof(buf), "USED %s", d.displayName);
    log_.push(LogEventType::ItemUsed, buf);
    const uint32_t cut = bootHatchRemainMs_ > kBootAcceleratorCutMs
                             ? bootHatchRemainMs_ - kBootAcceleratorCutMs : 0;
    if (bootHatchRemainMs_ > kHatchRevealMs)
        bootHatchRemainMs_ = cut > kHatchRevealMs ? cut : kHatchRevealMs;
    markSaveDirty();
}

int Game::nextEligibleStat(int cur) const {
    // The next stat (wrapping) with ≥1 earned point — A cycles only eligible rows
    // Returns `cur` if it's the only eligible one, or -1 if none are.
    for (int step = 1; step <= kLevelStatCount; ++step) {
        const int i = (cur + step) % kLevelStatCount;
        if (rollbackEligible(i)) return i;
    }
    return rollbackEligible(cur) ? cur : -1;
}

void Game::openRollbackPicker() {
    // Park on the first eligible stat (itemUsable already guaranteed ≥1 point exists,
    // so nextEligibleStat can't return -1 here). detailItem_ (the Rollback item) stays
    // set so a confirmed shed consumes it.
    int first = -1;
    for (int i = 0; i < kLevelStatCount; ++i)
        if (rollbackEligible(i)) { first = i; break; }
    rollbackRow_ = first < 0 ? 0 : first;
    nav_ = Nav::RollbackPicker;
    dirty_ = true;
}

void Game::onRollbackPicker(const ButtonEvent& ev) {
    if (ev.button == Button::A) {
        const int nxt = nextEligibleStat(rollbackRow_);
        if (nxt >= 0) rollbackRow_ = nxt;
    } else if (ev.button == Button::B) {
        // Shed one point of the chosen stat: −1 that stat, −1 level (level == total
        // points), and re-zero the XP bucket so the pet RE-GRINDS that level to
        // re-roll it (the grind is the real cost). Consume the Rollback item + log.
        if (rollbackEligible(rollbackRow_) && detailItem_) {
            --statPoints_[rollbackRow_];
            --combatLevel_;
            combatXp_ = 0;
            inventory_.remove(detailItem_->id, 1);
            char buf[28];
            // Surface the shed explicitly — which stat and by how much (
            // "show the -1 <stat> on a Rollback"). The stat WORD + signed number are
            // the non-colour channel, so it reads in a grayscale log.
            std::snprintf(buf, sizeof(buf), "ROLLBACK %s -1",
                          levelStatName(rollbackRow_));
            log_.push(LogEventType::ItemUsed, buf);
            markSaveDirty();
        }
        nav_ = Nav::Submenu;   // back to the ITEMS list (the item may have run out)
    } else if (ev.button == Button::C) {
        nav_ = Nav::Detail;    // cancel — item untouched, back to its detail
    }
    dirty_ = true;
}

void Game::rollPantrySpoilage() {
    // One roll per held perishable STACK, converting a single unit — so a larder of
    // twenty rots no faster than a larder of one, and a player who hoards isn't punished
    // for it beyond the flavour. Collected first because converting mutates the vector
    // the roll is walking.
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
    // Feeding from the crisis resolves it immediately — disarm the deadline so it
    // can't penalise the pet mid-eat (endFeeding finalises the return to idle).
    if (fromLockout) lockoutActive_ = false;
    char buf[28];
    std::snprintf(buf, sizeof(buf), "FED %s", d.displayName);
    log_.push(LogEventType::ItemUsed, buf);
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
