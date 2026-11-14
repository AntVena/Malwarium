#include "core/app/game.h"

#include <cstdio>
#include <cstring>

#include "tunables.h"
#include "core/app/game_rig_shop.h"
#include "core/content/content_tables.h"
#include "core/ui/maint_screen.h"   // defragGated, avGated
#include "core/ui/mods_screen.h"

namespace mal {

// --- MAINT -----------------------------------------------------------------

void Game::startMaint(MaintKind kind, int variant, ArcadeDifficulty dial) {
    const bool defrag = kind == MaintKind::Defrag;
    const bool played = variant == (defrag ? kDefragVariants : kAvVariants) - 1;
    bool success = false;
    if (defrag) {
        if (defragGated(pet_.model)) return;                 // nothing to defragment
        // a Defrag costs stage-scaled Bits: inert if the wallet can't
        // cover it. Charged here on RUN (before the roll), so it's the spend, not
        // the outcome, that gates spamming; a failed run still burns the Bits.
        const int cost = defragCost();
        if (op_.bits < cost) return;                        // can't afford: inert
        // the TOOL variant additionally spends one Defrag Tool for a
        // GUARANTEED clean (no fail roll). Inert if none held; the QUICK variant is
        // Bits-only with the normal success roll.
        const bool tool = variant == 1;
        if (tool && inventory_.count(kDefragToolId) <= 0) return;   // no tool: inert
        op_.bits -= cost;
        if (!played) {
            if (tool) {
                inventory_.remove(kDefragToolId, 1);
                success = true;                          // a Tool defrag never fails
            } else {
                success = rollMaintSuccess();
            }
        }
    } else {
        if (avGated(pet_.model)) return;                     // inert
        if (!played) success = rollMaintSuccess();
    }
    maint_ = MaintRun::started(kind, played, dial);
    maint_->success = success;
    // The PLAYED variant buys nothing but the attempt (a Defrag's Bits are already
    // spent; AV is free). It hands off to the process's game instead of the process bar,
    // and rejoins at settlePlayedRun.
    if (played) {
        if (!beginPlayedGame(maintPlayedGame(kind))) maint_.reset();
        return;
    }
    nav_ = Nav::Process;
}

void Game::dismissMaint() {
    maint_.reset();
    nav_ = Nav::Detail;
}

void Game::beginStackerBoard() {
    // The board itself, independent of who pays for it: MAINT takes the units, the
    // arcade charges nothing and takes the score. The shared LCG seeds the intruders'
    // columns; the dial sets what the ground costs.
    rng_ = rng_ * 1664525u + 1013904223u;
    stacker_.reset(rng_ >> 8, kStackerGroundCalls[static_cast<int>(runDifficulty())]);
    lastStackerStepMs_ = nowMs_;
    closeGameBrief();   // a stale flag from a previous board must not open paused
    nav_ = Nav::Stacker;
}

// One beat of the board. Only the intruders' drop takes a block or ends a run, so those
// sound here rather than on a press.
void Game::stepStacker() {
    const bool live = stacker_.running();
    const int destroyed = stacker_.destroyed();
    stacker_.step();
    if (live && !stacker_.running()) playSound(stacker_.won() ? Sound::Clear : Sound::Crash);
    else if (stacker_.destroyed() > destroyed) playSound(Sound::Blocked);
}

void Game::finishStacker() {
    // The board's own joke, and it belongs to the BOARD rather than to any context:
    // the line that was breached with a single course built.
    if (stacker_.breached() && stacker_.row() == 1)
        unlockAchievement(ach::kStackOverflow);
    // The played-by-hand ladder and the two rows a board's SHAPE earns, for a line
    // held in MAINT: whichever process it was played for. The hand it was held with is
    // the whole record of how the run went: the full starting width means no block ever
    // fell past a wall, and one block means the line was finished a column at a time.
    if (!inArcadeRun() && stacker_.won()) {
        ++op_.tallies.stackerWins;
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
    // A failed Defrag/AV costs +1 care mistake: routed through the SHIELDED path (v21
    // Restore Point covers maint fails too), so PetModel reports the failure and the
    // Game applies the mistake here (the frag +penalty already landed model-side).
    // Only a rolled or bought run reaches here, and startMaint opened it.
    MaintRun& run = *maint_;
    if (run.kind == MaintKind::Defrag) {
        const bool failed = pet_.model.applyDefrag(run.success);
        if (!failed) ++pet_.defragCount;          // this pet's defrag tally
        else {
            addCareMistakeShielded(1);
            log_.push(LogEventType::CareMistake, "FAILED DEFRAG");
            // A botched defrag on an ALREADY-CRITICAL disk forks a Replication Ghost:
            // the write went wrong badly enough to leave a phantom second copy of the
            // pet's process behind. This is the only thing that raises one, and it is
            // deliberately not a dice roll on top of a dice roll. The player's state
            // decides it: to avoid a ghost, defragment before Fragmentation reaches
            // Critical (or to stop rolling for it: the Tool and Played variants never
            // reach this path. One is bought and the other is played, and neither has a
            // roll to lose). Worm-line only: a self-replicating phantom copy is that
            // line's failure mode, as Lockout is Ransomware's. A Phishing or Trojan
            // pet's disk just stays fragmented.
            const bool wormLine = pet_.creature && pet_.creature->line &&
                                  std::strcmp(pet_.creature->line, "worm") == 0;
            if (wormLine && pet_.model.fragmentation() >= kFragCriticalMin &&
                !pet_.model.hasGhost()) {
                pet_.model.setGhost(true);
                log_.push(LogEventType::CareMistake, "REPLICATION GHOST");
            }
        }
    } else {
        const bool failed = pet_.model.applyAntivirus(run.success);
        if (failed) { addCareMistakeShielded(1); log_.push(LogEventType::CareMistake, "FAILED AV SCAN"); }
    }
    markSaveDirty();
    run.resolved = true;
}

void Game::maybeAutoDefrag() {
    // Disk Maintenance (Rig Shop i): silently runs a guaranteed-clean defrag
    // between explore events once Fragmentation crosses the owned tier's
    // threshold. A deeper (pricier) tier both lowers that threshold and raises
    // the per-run Bits cost: which suits an older pet that explores enough to
    // make a hands-off defrag worth paying for.
    const int lvl = op_.rig.level[kRigRowDiskMaintenance];
    if (lvl <= 0 || !pet_.creature) return;
    // Owned is not the same as RUNNING: a service the player has stopped on the SHOP's
    // SERVICES screen spends nothing and defrags nothing until it is started again.
    if (!op_.rig.featureActive(kRigRowDiskMaintenance)) return;
    if (pet_.model.fragmentation() < kDiskMaintenanceThreshold[lvl - 1]) return;
    const int cost = defragCost() * kDiskMaintenanceCostMult[lvl - 1];
    if (op_.bits < cost) return;                    // can't afford this run: skip silently
    op_.bits -= cost;
    pet_.model.applyDefrag(true);                    // a paid auto-run never fails
    ++pet_.defragCount;
    log_.push(LogEventType::ItemUsed, "AUTO-DEFRAG RAN");
    markSaveDirty();
}

void Game::debugResolveDefrag(bool success) {
    // Deterministic maint outcome for tests: drive the real resolveMaint() seam with a
    // forced success/failure (rollMaintSuccess is otherwise RNG-gated). A failure routes
    // its +1 care mistake through addCareMistakeShielded, so the shield gate is assertable.
    maint_ = MaintRun::started(MaintKind::Defrag, /*played=*/false, maintScreen_.difficulty());
    maint_->success = success;
    resolveMaint();
    maint_.reset();
}

int Game::maintFailPct() const {
    const int failPct = 15 + pet_.model.fragmentation() / 4;   // rises with Fragmentation
    return failPct > 60 ? 60 : failPct;
}

bool Game::rollMaintSuccess() {
    rng_ = rng_ * 1664525u + 1013904223u;               // LCG (deterministic seed)
    int roll = static_cast<int>((rng_ >> 16) % 100);
    return roll >= maintFailPct();
}

// LOADOUT: the rules its screen asks, and the way back to its hub ------------

void Game::leaveLoadoutTab() {
    // Re-parks the cursor too: the caller (finishCombat, off PRACTISE) gets here from a
    // full-screen activity rather than from inside the hub.
    loadout_.showHub();
    cursor_ = carouselSlotOf(SubmenuId::Mods);
    nav_ = Nav::Submenu;
}

bool Game::modEquippable(int slot, const char* id) const {
    if (!id) return false;
    const ModDef* md = registry_.mod(id);
    // A mod is held forever but equippable only once the pet is high enough. The gate is
    // the mod's (modEquipLevel, off its power tier), the same for every copy.
    if (md && modEquipLevel(*md) > pet_.level) return false;
    // The hard line gate (ModDef::requiresLine), distinct from the soft affinity bonus:
    // such a mod can drop and wait in the pool for a pet of its line.
    if (md && md->requiresLine &&
        (!pet_.creature || !pet_.creature->line ||
         std::strcmp(md->requiresLine, pet_.creature->line) != 0))
        return false;
    // One slot per id per pet.
    const int elsewhere = pet_.mods.slotOf(id);
    return elsewhere < 0 || elsewhere == slot;
}

void Game::installMod(int slot, const char* id) {
    if (!registry_.mod(id) || !modEquippable(slot, id)) return;
    pet_.mods.install(slot, id, op_.modPool);
    markSaveDirty();
}

void Game::equipMove(int slot, const char* id) {
    pet_.moves.equip(slot, id);
    markSaveDirty();
}

void Game::unequipMove(int slot) {
    pet_.moves.unequip(slot);
    markSaveDirty();
}

}  // namespace mal
