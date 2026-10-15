#include "core/app/game.h"

#include <cstdio>
#include <cstring>

#include "tunables.h"
#include "core/app/game_rig_shop.h"
#include "core/content/content_tables.h"
#include "core/ui/maint_screen.h"
#include "core/ui/mods_screen.h"

namespace mal {

// --- MAINT -----------------------------------------------------------------

void Game::onMaintList(const ButtonEvent& ev) {
    if (ev.button == Button::A) listRow_ ^= 1;          // toggle the 2 rows
    else if (ev.button == Button::B) {
        maintKind_ = (listRow_ == 0) ? MaintKind::Defrag : MaintKind::Av;
        maintVariant_ = 0;                               // default to QUICK
        maintDifficulty_ = ArcadeDifficulty::Easy;
        nav_ = Nav::Detail;
    } else if (ev.button == Button::C) nav_ = Nav::Cursor;
}

void Game::onMaintAction(const ButtonEvent& ev) {
    // A cycles the payment VARIANT — DEFRAG: Quick / Tool / Played; AV: Quick / Played —
    // and on the PLAYED row it walks MAINT's difficulty dial first, EASY up to HARD,
    // before wrapping back to Quick (and the dial back to EASY). One button for both, the way the arcade cabinet page
    // cycles its dial, so the screen never needs a second gesture to set the stakes.
    // B runs the focused one.
    if (ev.button == Button::A) {
        const int n = maintKind_ == MaintKind::Defrag ? kDefragVariants : kAvVariants;
        const bool played = maintVariant_ == n - 1;
        if (played && maintDifficulty_ != ArcadeDifficulty::Hard) {
            maintDifficulty_ =
                static_cast<ArcadeDifficulty>(static_cast<int>(maintDifficulty_) + 1);
        } else {
            if (played) maintDifficulty_ = ArcadeDifficulty::Easy;
            maintVariant_ = (maintVariant_ + 1) % n;
        }
    } else if (ev.button == Button::B) startMaint();
    else if (ev.button == Button::C) nav_ = Nav::Submenu;
}

void Game::startMaint() {
    maintPlayed_ = false;
    const bool defrag = maintKind_ == MaintKind::Defrag;
    const bool played = maintVariant_ == (defrag ? kDefragVariants : kAvVariants) - 1;
    if (defrag) {
        if (defragGated(model_)) return;                 // nothing to defragment
        // a Defrag costs stage-scaled Bits — inert if the wallet can't
        // cover it. Charged here on RUN (before the roll), so it's the spend, not
        // the outcome, that gates spamming; a failed run still burns the Bits.
        const int cost = defragCost();
        if (bits_ < cost) return;                        // can't afford — inert
        // the TOOL variant additionally spends one Defrag Tool for a
        // GUARANTEED clean (no fail roll). Inert if none held; the QUICK variant is
        // Bits-only with the normal success roll.
        const bool tool = maintVariant_ == 1;
        if (tool && inventory_.count(kDefragToolId) <= 0) return;   // no tool — inert
        bits_ -= cost;
        if (!played) {
            if (tool) {
                inventory_.remove(kDefragToolId, 1);
                maintSuccess_ = true;                    // a Tool defrag never fails
            } else {
                maintSuccess_ = rollMaintSuccess();
            }
        }
    } else {
        if (avGated(model_)) return;                     // inert
        if (!played) maintSuccess_ = rollMaintSuccess();
    }
    // The PLAYED variant buys nothing but the attempt (a Defrag's Bits are already
    // spent; AV is free). It hands off to the process's game instead of the process bar,
    // and rejoins at settlePlayedRun.
    if (played) {
        maintPlayLive_ = beginPlayedGame(maintPlayedGame(maintKind_));
        return;
    }
    processBeat_ = 0;
    processResolved_ = false;
    nav_ = Nav::Process;
}

void Game::beginStackerBoard() {
    // The board itself, with no opinion about who is paying for it — MAINT takes the
    // units, the arcade charges nothing and takes the score.
    rng_ = rng_ * 1664525u + 1013904223u;   // the shared LCG seeds the intruder's column
    stacker_.reset(rng_ >> 8);
    lastStackerStepMs_ = nowMs_;
    closeGameBrief();   // a stale flag from a previous board must not open paused
    nav_ = Nav::Stacker;
}

void Game::onStacker(const ButtonEvent& ev) {
    if (onGameBriefInput(ev)) return;
    if (!stacker_.running()) {                  // parked on the result: any press moves on
        finishStacker();
        return;
    }
    if (ev.button == Button::B) {
        const int hand = stacker_.width();
        stacker_.drop();
        playSound(stacker_.width() < hand ? Sound::Miss : Sound::Point);
    } else if (ev.button == Button::C) {
        // Stop early and bank the board. Since a run that ends in mid-air keeps whatever
        // it locked, quitting has to keep it too — otherwise C would be a button that
        // throws away blocks the player has already earned, which is a trap, not a choice.
        finishStacker();
    }
}

// One beat of the board. Only the intruders' drop ends one, so the ending sounds here
// rather than on a press.
void Game::stepStacker() {
    const bool live = stacker_.running();
    stacker_.step();
    if (live && !stacker_.running()) playSound(stacker_.won() ? Sound::Clear : Sound::Crash);
}

void Game::finishStacker() {
    // The board's own joke, and it belongs to the BOARD rather than to any context:
    // the line that was breached with a single course built.
    if (stacker_.breached() && stacker_.row() == 1)
        unlockAchievement(ach::kStackOverflow);
    // The played-by-hand ladder and the two rows a board's SHAPE earns, for a line
    // held in MAINT — whichever process it was played for. The hand it was held with is
    // the whole record of how the run went: the full starting width means no block ever
    // fell past a wall, and one block means the line was finished a column at a time.
    if (!arcadeRun_ && stacker_.won()) {
        ++stackerWins_;
        const int kept = stacker_.width();
        if (kept >= kStackerStartWidth) unlockAchievement(ach::kPerfectDefrag);
        if (kept == 1) unlockAchievement(ach::kHangingByABit);
    }
    PlayOutcome o;
    o.units = stacker_.blocks();
    o.won = stacker_.won();
    o.score = stacker_.score();
    o.scoreMax = kStackerMaxScore;
    settlePlayedRun(o);
}

void Game::resolveMaint() {
    // A failed Defrag/AV costs +1 care mistake — routed through the SHIELDED path (v21
    // Restore Point covers maint fails too), so PetModel reports the failure and the
    // Game applies the mistake here (the frag +penalty already landed model-side).
    if (maintKind_ == MaintKind::Defrag) {
        const bool failed = model_.applyDefrag(maintSuccess_);
        if (!failed) ++defragCount_;          // this pet's defrag tally
        else {
            addCareMistakeShielded(1);
            log_.push(LogEventType::CareMistake, "FAILED DEFRAG");
            // A botched defrag on an ALREADY-CRITICAL disk forks a Replication Ghost:
            // the write went wrong badly enough to leave a phantom second copy of the
            // pet's process behind. This is the only thing that raises one, and it is
            // deliberately not a dice roll on top of a dice roll — the state the player
            // is in is what decides it, so the way to never see a ghost is to not let
            // Fragmentation reach Critical before defragmenting (or to stop rolling for
            // it: the Tool and Played variants never reach this path — one is bought and
            // the other is played, and neither has a roll to lose). Worm-line only: a
            // self-replicating phantom copy is that line's own failure mode, the way
            // Lockout is Ransomware's — a Phishing or Trojan pet's disk just stays
            // fragmented.
            const bool wormLine = pet_ && pet_->line &&
                                  std::strcmp(pet_->line, "worm") == 0;
            if (wormLine && model_.fragmentation() >= kFragCriticalMin &&
                !model_.hasGhost()) {
                model_.setGhost(true);
                log_.push(LogEventType::CareMistake, "REPLICATION GHOST");
            }
        }
    } else {
        const bool failed = model_.applyAntivirus(maintSuccess_);
        if (failed) { addCareMistakeShielded(1); log_.push(LogEventType::CareMistake, "FAILED AV SCAN"); }
    }
    noteCareSignal(DominantSignal::Maintenance);   // dominant-signal tally
    markSaveDirty();
    processResolved_ = true;
}

void Game::maybeAutoDefrag() {
    // Disk Maintenance (Rig Shop i): silently runs a guaranteed-clean defrag
    // between explore events once Fragmentation crosses the owned tier's
    // threshold. A deeper (pricier) tier both lowers that threshold and raises
    // the per-run Bits cost — the "buy more safety" trade for an older, stronger
    // pet that explores enough to make a hands-off defrag worth paying for.
    const int lvl = rigLevel_[kRigRowDiskMaintenance];
    if (lvl <= 0 || !pet_) return;
    // Owned is not the same as RUNNING: a service the player has stopped on the SHOP's
    // SERVICES screen spends nothing and defrags nothing until it is started again.
    if (!rigFeatureActive(kRigRowDiskMaintenance)) return;
    if (model_.fragmentation() < kDiskMaintenanceThreshold[lvl - 1]) return;
    const int cost = defragCost() * kDiskMaintenanceCostMult[lvl - 1];
    if (bits_ < cost) return;                    // can't afford this run — skip silently
    bits_ -= cost;
    model_.applyDefrag(true);                    // a paid auto-run never fails
    ++defragCount_;
    log_.push(LogEventType::ItemUsed, "AUTO-DEFRAG RAN");
    markSaveDirty();
}

void Game::debugResolveDefrag(bool success) {
    // Deterministic maint outcome for tests: drive the real resolveMaint() seam with a
    // forced success/failure (rollMaintSuccess is otherwise RNG-gated). A failure routes
    // its +1 care mistake through addCareMistakeShielded, so the shield gate is assertable.
    maintKind_ = MaintKind::Defrag;
    maintPlayed_ = false;
    maintSuccess_ = success;
    resolveMaint();
}

int Game::maintFailPct() const {
    const int failPct = 15 + model_.fragmentation() / 4;   // rises with Fragmentation
    return failPct > 60 ? 60 : failPct;
}

bool Game::rollMaintSuccess() {
    rng_ = rng_ * 1664525u + 1013904223u;               // LCG (deterministic seed)
    int roll = static_cast<int>((rng_ >> 16) % 100);
    return roll >= maintFailPct();
}

// MODS — the LOADOUT hub ----------------------------------------

void Game::onLoadoutHub(const ButtonEvent& ev) {
    if (ev.button == Button::A) {
        loadoutHubRow_ = (loadoutHubRow_ + 1) % kLoadoutHubRows;
    } else if (ev.button == Button::B) {
        // Row order is the hub's draw order, which is LoadoutTab's own order past Hub.
        openLoadoutTab(static_cast<LoadoutTab>(loadoutHubRow_ + 1));
    } else if (ev.button == Button::C) {
        nav_ = Nav::Cursor;
    }
}

void Game::openLoadoutTab(LoadoutTab tab) {
    loadoutTab_ = tab;
    switch (tab) {
        case LoadoutTab::Mods:
            listRow_ = 0;
            modConfirm_ = false;
            modDetail_ = false;
            modDetailId_ = nullptr;
            nav_ = Nav::Submenu;
            break;
        case LoadoutTab::Moves:
            trainRow_ = 0;
            trainScreen_ = TrainScreen::MovePicker;
            moveConfirm_ = false;
            movePendingId_ = nullptr;
            nav_ = Nav::Submenu;
            break;
        case LoadoutTab::Practise:
            // The dummy fight has no list of its own to rest on — the hub row IS its
            // L2, so it opens straight into the tier pick.
            trainScreen_ = TrainScreen::SimTier;
            simTier_ = 0;
            nav_ = Nav::Detail;
            break;
        case LoadoutTab::Hub:
            nav_ = Nav::Submenu;
            break;
    }
}

void Game::leaveLoadoutTab() {
    // Also re-park the cursor, because one caller (finishCombat, off PRACTISE) gets
    // here from a full-screen activity rather than from inside the hub.
    loadoutTab_ = LoadoutTab::Hub;
    cursor_ = carouselSlotOf(SubmenuId::Mods);
    nav_ = Nav::Submenu;
}

void Game::onModsList(const ButtonEvent& ev) {
    if (ev.button == Button::A) {
        listRow_ = (listRow_ + 1) % kModSlots;          // next slot, wraps
    } else if (ev.button == Button::B) {                 // open the slot's picker
        modSlot_ = listRow_;
        modPick_ = 0;
        modConfirm_ = false;
        modPendingId_ = nullptr;
        modDetail_ = false;
        modDetailId_ = nullptr;
        nav_ = Nav::Detail;
    } else if (ev.button == Button::C) {
        leaveLoadoutTab();
    }
}

void Game::onModPicker(const ButtonEvent& ev) {
    // The same five arguments drawModPicker draws with, so the cursor indexes the rows
    // the player is looking at — fittable mods first, locked ones after them.
    const auto owned = ownedModList(registry_, loadout_, combatLevel_,
                                    pet_ ? pet_->line : nullptr, modSlot_);
    const int rows = static_cast<int>(owned.size());      // available spares to install

    if (modConfirm_) {                                    // inline overwrite confirm
        if (ev.button == Button::A) modConfirmChoice_ ^= 1;
        else if (ev.button == Button::B) {
            const ModDef* pendMod = modPendingId_ ? registry_.mod(modPendingId_) : nullptr;
            const bool lineOk = !pendMod || !pendMod->requiresLine ||
                (pet_ && pet_->line && std::strcmp(pendMod->requiresLine, pet_->line) == 0);
            if (modConfirmChoice_ == 1 && modPendingId_ && lineOk && pendMod &&
                modEquipLevel(*pendMod) <= combatLevel_) {
                // D3: consumes the new mod AND discards the one already in the slot.
                // (Re-check the equip-LEVEL + hard line gates defensively — commitModEquip
                // already gated both before setting modPendingId_.)
                loadout_.equip(modSlot_, modPendingId_);
                markSaveDirty();
            }
            modConfirm_ = false;
            modPendingId_ = nullptr;
            nav_ = Nav::Submenu;                          // resolved -> back to slots
        } else if (ev.button == Button::C) {
            modConfirm_ = false;                          // cancel -> stay in picker
            modPendingId_ = nullptr;
        }
        return;
    }

    if (ev.button == Button::A) {
        if (rows > 0) modPick_ = (modPick_ + 1) % rows;
    } else if (ev.button == Button::B) {
        if (rows > 0) {                                   // inspect before equipping
            if (modPick_ >= rows) modPick_ = rows - 1;
            modDetailId_ = owned[modPick_]->id;           // open the detail
            modDetail_ = true;
        }
    } else if (ev.button == Button::C) {
        nav_ = Nav::Submenu;
    }
}

// mod detail (mirrors ITEMS list -> detail -> use): B commits the equip,
// C backs to the picker. The equip runs the same overwrite gate as the picker.
void Game::onModDetail(const ButtonEvent& ev) {
    if (ev.button == Button::B) commitModEquip();
    else if (ev.button == Button::C) modDetail_ = false;  // back to the picker
}

// Shared equip-or-confirm for the detail's inspected mod (modDetailId_). An empty
// slot consumes + installs straight away; a slot already holding a DIFFERENT mod
// hands off to the picker's inline overwrite confirm — the "discards {current} —
// permanent" warning, since the overwrite is irreversible. A slot that
// already holds this exact mod is a no-op (don't burn a spare for no change).
void Game::commitModEquip() {
    const char* id = modDetailId_;
    if (!id) return;
    // The equip-LEVEL gate — a mod is held forever but equippable only once the pet is
    // high enough. The gate is the MOD's (modEquipLevel, off its power tier), the same
    // for every copy; a sub-level mod is blocked here, and the detail already shows
    // LOCKED + the requirement.
    const ModDef* md = registry_.mod(id);
    if (md && modEquipLevel(*md) > combatLevel_) return;
    // Niche-flavour pass: the HARD line gate (ModDef::requiresLine) — distinct from the
    // soft `line`/`affinityBonus` bonus every other mod uses. A mod like Phishing Rod or
    // Extortion Ledger simply cannot be installed unless the active pet's line matches;
    // it can still drop and sit in inventory for whenever the player raises that line.
    if (md && md->requiresLine &&
        (!pet_ || !pet_->line || std::strcmp(md->requiresLine, pet_->line) != 0))
        return;
    // One slot per id per pet — already installed elsewhere on this pet is a no-op.
    const int elsewhere = loadout_.slotOf(id);
    if (elsewhere >= 0 && elsewhere != modSlot_) return;
    const char* cur = loadout_.equipped(modSlot_);
    if (cur && std::strcmp(cur, id) == 0) {               // already installed here
        modDetail_ = false;
        nav_ = Nav::Submenu;
    } else if (cur) {                                     // replacing a different mod
        modConfirm_ = true;
        modConfirmChoice_ = 0;                            // default Cancel
        modPendingId_ = id;
        modDetail_ = false;                               // confirm shows on the picker
    } else {                                              // empty slot — consume + install
        loadout_.equip(modSlot_, id);
        markSaveDirty();
        modDetail_ = false;
        nav_ = Nav::Submenu;
    }
}


}  // namespace mal
