#include "core/app/game.h"

#include <cstdio>
#include <cstring>

#include "tunables.h"
#include "core/app/game_rig_shop.h"
#include "core/content/content_tables.h"
#include "core/render/canvas.h"
#include "core/render/font5x7.h"
#include "core/render/palette.h"
#include "core/render/sprite.h"
#include "core/ui/carousel.h"
#include "core/ui/combat_screen.h"
#include "core/ui/items_screen.h"
#include "core/ui/maint_screen.h"
#include "core/ui/modals.h"
#include "core/ui/stat_screen.h"
#include "core/ui/train_screen.h"
#include "generated/assets.h"

namespace mal {

// --- MAINT -----------------------------------------------------------------

void Game::onMaintList(const ButtonEvent& ev) {
    if (ev.button == Button::A) listRow_ ^= 1;          // toggle the 2 rows
    else if (ev.button == Button::B) {
        maintKind_ = (listRow_ == 0) ? MaintKind::Defrag : MaintKind::Av;
        defragVariant_ = 0;                              // default to QUICK
        nav_ = Nav::Detail;
    } else if (ev.button == Button::C) nav_ = Nav::Cursor;
}

void Game::onMaintAction(const ButtonEvent& ev) {
    // on the Defrag screen, A cycles the payment VARIANT — Quick (Bits, may
    // fail) / Tool (an item, guaranteed) / Stacker (the minigame, guaranteed if you
    // clear it) — and B runs the focused one. AV has no variants (A is inert there).
    if (ev.button == Button::A && maintKind_ == MaintKind::Defrag)
        defragVariant_ = (defragVariant_ + 1) % kDefragVariants;
    else if (ev.button == Button::B) startMaint();
    else if (ev.button == Button::C) nav_ = Nav::Submenu;
}

void Game::startMaint() {
    if (maintKind_ == MaintKind::Defrag) {
        if (defragGated(model_)) return;                 // nothing to defragment
        // a Defrag costs stage-scaled Bits — inert if the wallet can't
        // cover it. Charged here on RUN (before the roll), so it's the spend, not
        // the outcome, that gates spamming; a failed run still burns the Bits.
        const int cost = defragCost();
        if (bits_ < cost) return;                        // can't afford — inert
        // the TOOL variant additionally spends one Defrag Tool for a
        // GUARANTEED clean (no fail roll). Inert if none held; the QUICK variant is
        // Bits-only with the normal success roll.
        const bool tool = defragVariant_ == 1;
        if (tool && inventory_.count(kDefragToolId) <= 0) return;   // no tool — inert
        bits_ -= cost;
        // the STACKER variant buys nothing but the attempt: the Bits are spent
        // here like every other run, and the outcome is then PLAYED rather than rolled
        // or bought. It hands off to its own screen instead of the process bar, and
        // rejoins at finishStacker().
        if (defragVariant_ == kDefragVariantStacker) {
            startStackerDefrag();
            return;
        }
        if (tool) {
            inventory_.remove(kDefragToolId, 1);
            maintSuccess_ = true;                        // a Tool defrag never fails
        } else {
            maintSuccess_ = rollMaintSuccess();
        }
    } else {
        if (avGated(model_)) return;                     // inert
        maintSuccess_ = rollMaintSuccess();
    }
    processBeat_ = 0;
    processResolved_ = false;
    nav_ = Nav::Process;
}

void Game::startStackerDefrag() {
    // The Bits are already spent (startMaint charges before branching), so from here the
    // run owes the player only an outcome. C abandons it and forfeits them, exactly as a
    // failed Quick defrag forfeits them — the spend is what gates spamming, not the result.
    stacker_.reset();
    lastStackerStepMs_ = nowMs_;
    nav_ = Nav::Stacker;
}

void Game::onStacker(const ButtonEvent& ev) {
    if (!stacker_.running()) {                  // parked on the result: any press moves on
        finishStacker();
        return;
    }
    if (ev.button == Button::B) {
        stacker_.drop();
    } else if (ev.button == Button::C) {
        // Abandon. The Bits are gone, but nothing else happened — no clean, and no care
        // mistake either, because walking away isn't a botched defrag.
        nav_ = Nav::Detail;
    }
}

void Game::finishStacker() {
    // Hand the played result back to the shared maint outcome path, so a cleared board
    // reduces Fragmentation and bumps the tally through exactly the same seam a Tool
    // defrag does, and a lost board takes the same care mistake a failed Quick one does.
    maintKind_ = MaintKind::Defrag;
    maintSuccess_ = stacker_.won();
    if (maintSuccess_) {
        ++stackerWins_;                          // the played ladder, player-level
        // The two rows a board's SHAPE earns. The top row's surviving width is the whole
        // record of how the run went: the full starting width means nothing was ever
        // shaved, and one block means the last lock had a single column to land on.
        // Read here rather than inside Stacker because the model has no idea achievements
        // exist — it answers "how wide", and this is what that width is worth.
        const int kept = stacker_.rowWidth(kStackerRows - 1);
        if (kept >= kStackerStartWidth) unlockAchievement(ach::kPerfectDefrag);
        if (kept == 1) unlockAchievement(ach::kHangingByABit);
    }
    processBeat_ = kProcessBeats;                // the work is done; show the outcome
    processResolved_ = false;
    nav_ = Nav::Process;
    resolveMaint();
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
            // it: the Tool and Stacker variants can't fail).
            if (model_.fragmentation() >= kFragCriticalMin && !model_.hasGhost()) {
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
    maintSuccess_ = success;
    resolveMaint();
}

bool Game::rollMaintSuccess() {
    rng_ = rng_ * 1664525u + 1013904223u;               // LCG (deterministic seed)
    int roll = static_cast<int>((rng_ >> 16) % 100);
    int failPct = 15 + model_.fragmentation() / 4;      // rises with Fragmentation
    if (failPct > 60) failPct = 60;
    return roll >= failPct;
}

// MODS ----------------------------------------------------------

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
        nav_ = Nav::Cursor;
    }
}

void Game::onModPicker(const ButtonEvent& ev) {
    const auto owned = ownedModList(registry_, loadout_);
    const int rows = static_cast<int>(owned.size());      // available spares to install

    if (modConfirm_) {                                    // inline overwrite confirm
        if (ev.button == Button::A) modConfirmChoice_ ^= 1;
        else if (ev.button == Button::B) {
            const ModDef* pendMod = modPendingId_ ? registry_.mod(modPendingId_) : nullptr;
            const bool lineOk = !pendMod || !pendMod->requiresLine ||
                (pet_ && pet_->line && std::strcmp(pendMod->requiresLine, pet_->line) == 0);
            if (modConfirmChoice_ == 1 && modPendingId_ && lineOk &&
                loadout_.reqLevelFor(modPendingId_) <= combatLevel_) {
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
// permanent" warning (D3), since the overwrite is irreversible. A slot that
// already holds this exact mod is a no-op (don't burn a spare for no change).
void Game::commitModEquip() {
    const char* id = modDetailId_;
    if (!id) return;
    // the rolled equip-LEVEL gate — a mod is held forever but equippable only
    // once the pet is high enough. reqLevelFor is the best (lowest) held copy's gate; a
    // sub-level mod is blocked here (the detail already shows LOCKED + the requirement).
    if (loadout_.reqLevelFor(id) > combatLevel_) return;
    // Niche-flavour pass: the HARD line gate (ModDef::requiresLine) — distinct from the
    // soft `line`/`affinityBonus` bonus every other mod uses. A mod like Phishing Rod or
    // Extortion Ledger simply cannot be installed unless the active pet's line matches;
    // it can still drop and sit in inventory for whenever the player raises that line.
    const ModDef* md = registry_.mod(id);
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
