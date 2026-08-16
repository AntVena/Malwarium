#include "core/app/game.h"

#include <cstdio>
#include <cstring>

#include "tunables.h"
#include "core/app/game_internal.h"   // backupDriveAchievement — declared, defined here
#include "core/ui/expl_screen.h"
#include "core/ui/train_screen.h"

namespace mal {

namespace {
// Sum the magnitude of every EQUIPPED mod with a given effect kind, folding
// in a line-affinity bonus when the pet's line matches. Used for the non-Combatant-field
// mod effects that makePlayerCombatant's loop can't apply itself: the post-battle Bits
// bonus (Packet Sniffer, PostBattleBits) and the battle-fatigue Frag cut (Heat Sink,
// FatigueFragCut).
int sumEquippedModMagnitude(const ContentRegistry& reg, const Loadout& load,
                            ModEffect kind, const char* petLine) {
    int total = 0;
    for (int i = 0; i < kModSlots; ++i)
        if (const char* id = load.equipped(i))
            if (const ModDef* m = reg.mod(id))
                if (m->effectKind == kind) {
                    int mag = m->magnitude;
                    if (m->line && petLine && std::strcmp(m->line, petLine) == 0)
                        mag += m->affinityBonus;
                    total += mag;
                }
    return total;
}
}  // namespace

// MOVES + the combat activity ---------------------------

void Game::onTrainList(const ButtonEvent& ev) {
    // The MOVES slot list. A steps the unlocked slots (skipping the locked ones), B
    // opens the focused slot's move picker, C backs to the LOADOUT hub. An egg has no
    // unlocked slot at all, so `sel` can legitimately be 0 — a state MOVES is
    // unreachable in (eggSlotLocked), but the wrap has to survive it regardless.
    const Stage st = pet_ ? pet_->stage : Stage::BootSector;
    const int sel = loadoutSelectableCount(st);
    if (ev.button == Button::A) {
        trainRow_ = sel > 0 ? (trainRow_ + 1) % sel : 0;
    } else if (ev.button == Button::B) {
        if (sel <= 0) return;
        trainScreen_ = TrainScreen::MovePicker;
        moveSlot_ = trainRow_;
        movePick_ = 0;
        moveShowAll_ = false;
        moveConfirm_ = false;
        movePendingId_ = nullptr;
        nav_ = Nav::Detail;
    } else if (ev.button == Button::C) {
        leaveLoadoutTab();
    }
}

void Game::onTrainDetail(const ButtonEvent& ev) {
    if (trainScreen_ == TrainScreen::MoveDetail) onMoveDetail(ev);
    else onMovePicker(ev);
}

const MoveDef* Game::focusedPickerMove() const {
    const auto owned = ownedMoveList(registry_, moveLoadout_, slotRequiredKind(moveSlot_),
                                     pet_ ? pet_->stage : Stage::BootSector,
                                     pet_ ? pet_->line : nullptr, moveSlot_, moveShowAll_);
    if (movePick_ <= 0 || movePick_ - 1 >= static_cast<int>(owned.size())) return nullptr;
    return owned[movePick_ - 1];
}

void Game::onMoveDetail(const ButtonEvent& ev) {
    // Standard A/B/C, no exceptions to spell out: A pages the prose when there is more
    // of it than fits (A cycles — reading on is the natural "next" here), B equips, C
    // returns to the picker with the cursor where it was.
    const MoveDef* m = focusedPickerMove();
    if (ev.button == Button::C || !m) {
        trainScreen_ = TrainScreen::MovePicker;
        return;
    }
    if (ev.button == Button::A) {
        const int total = moveProseLines(*m);
        const int shown = moveDetailProseLines();
        if (total <= shown) return;                 // it all fits — nothing to page
        moveProseScroll_ += shown;
        if (moveProseScroll_ >= total) moveProseScroll_ = 0;   // wraps to the top
        return;
    }
    if (ev.button != Button::B) return;
    // A move the pet hasn't evolved into yet can't be equipped — the page states the
    // gate, and B is inert so the player stays and reads it.
    if (pet_ && !moveUnlockedAtStage(*m, pet_->stage)) return;
    // Line-gating: a defensive backstop — a pet can never equip another line's
    // exclusive move (drops already keep them un-owned, so this only fires if a
    // wrong-line move is ever granted some other way).
    if (pet_ && !moveAllowedForLine(*m, pet_->line)) return;
    const char* cur = moveLoadout_.equipped(moveSlot_);
    if (cur && std::strcmp(cur, m->id) != 0) {
        // Replacing a DIFFERENT move: hand back to the picker, which owns the inline
        // overwrite confirm (drawMovePicker / onMovePicker) rather than duplicating it.
        moveConfirm_ = true;
        moveConfirmChoice_ = 0;
        movePendingId_ = m->id;
        trainScreen_ = TrainScreen::MovePicker;
        return;
    }
    moveLoadout_.equip(moveSlot_, m->id);
    noteCareSignal(DominantSignal::Training);
    markSaveDirty();
    trainScreen_ = TrainScreen::MovePicker;
    nav_ = Nav::Submenu;
}

void Game::onMovePicker(const ButtonEvent& ev) {
    // Mirrors the MODS picker: owned moves + an unequip option; a move lives
    // in one slot (equipping it elsewhere moves it); overwriting a DIFFERENT move
    // opens the inline confirm. The innate default can't be slotted. #12: the
    // slot is type-locked, so the candidate list is already kind-filtered — there's
    // nothing left to reject on kind by the time B is pressed. The default (non-
    // showAll) list also drops anything else the pet can't equip here right now
    // (locked by stage, wrong line, already equipped elsewhere) — declutters the
    // menu to just the live options, per ownedMoveList.
    const auto owned = ownedMoveList(registry_, moveLoadout_, slotRequiredKind(moveSlot_),
                                     pet_ ? pet_->stage : Stage::BootSector,
                                     pet_ ? pet_->line : nullptr, moveSlot_, moveShowAll_);
    const int rows = 1 + static_cast<int>(owned.size());   // [0] = unequip

    if (moveConfirm_) {
        if (ev.button == Button::A) moveConfirmChoice_ ^= 1;
        else if (ev.button == Button::B) {
            if (moveConfirmChoice_ == 1 && movePendingId_) {
                moveLoadout_.equip(moveSlot_, movePendingId_);
                noteCareSignal(DominantSignal::Training);
                markSaveDirty();
            }
            moveConfirm_ = false;
            movePendingId_ = nullptr;
            nav_ = Nav::Submenu;
        } else if (ev.button == Button::C) {
            moveConfirm_ = false;
            movePendingId_ = nullptr;
        }
        return;
    }

    if (ev.button == Button::A) {
        movePick_ = (movePick_ + 1) % rows;   // steps, and holding it repeats
    } else if (ev.button == Button::B) {
        // B is the tap/hold pair here (the ITEMS list's shape): arm now and resolve on
        // the RELEASE edge (moveFilterReleaseB — a short tap unequips or drills into
        // the focused move) or on the hold crossing kMoveFilterHoldMs, where tick()
        // reveals the full roster instead.
        bHeld_ = true;
        bDownMs_ = nowMs_;
    } else if (ev.button == Button::C) {
        nav_ = Nav::Submenu;
    }
}

void Game::moveFilterReleaseB() {
    // Screen check only — onButton owns bHeld_ (see itemFilterReleaseB).
    if (!(nav_ == Nav::Detail && loadoutTab_ == LoadoutTab::Moves &&
          trainScreen_ == TrainScreen::MovePicker && !moveConfirm_))
        return;   // bHeld_ was armed elsewhere (the ITEMS / VAULT / CFG holds) — no-op
    if (movePick_ == 0) {                              // unequip this slot
        moveLoadout_.unequip(moveSlot_);
        markSaveDirty();
        nav_ = Nav::Submenu;
    } else {
        // A real move row DRILLS DOWN to its own entry rather than equipping from
        // the list — the same list -> B -> detail -> B -> act shape ITEMS uses. It's
        // what lets the picker stay a comparison view (the aligned readout) and the
        // detail page be the reader, instead of one screen failing at both.
        trainScreen_ = TrainScreen::MoveDetail;
        moveProseScroll_ = 0;
    }
    dirty_ = true;
    lastInputMs_ = nowMs_;
}

void Game::onSimTier(const ButtonEvent& ev) {
    // PRACTISE's only screen, so C leaves the whole page rather than stepping back a
    // level — there is no list underneath it, only the hub row that opened it.
    if (ev.button == Button::A) simTier_ = (simTier_ + 1) % kSimDummyTiers;
    else if (ev.button == Button::B) startSimBattle();
    else if (ev.button == Button::C) leaveLoadoutTab();
}

const char* backupDriveAchievement(Combatant::BackupUse used, Combat::Outcome outcome) {
    // Overwhelmed is read FIRST because a mutual KO resolves as a Win: the pet still
    // didn't get up, and what the player watched was the restore failing.
    if (used == Combatant::BackupUse::None) return nullptr;
    if (used == Combatant::BackupUse::Overwhelmed) return ach::kShatteredPlatter;
    if (outcome == Combat::Outcome::Lose) return ach::kNeededMoreBackup;
    if (outcome == Combat::Outcome::Win) return ach::kBackUpAndDriven;
    return nullptr;   // Fled: the run isn't over, so neither is the story
}

void Game::settleBackupDrive() {
    // Every path out of a fight lands here (applyCombatResult for wild/Sim, and every
    // boss round), so a spent drive is settled in exactly one place. It is spent for the
    // rest of its 1h window regardless of time remaining — win or lose, the buff doesn't
    // outlive the death it answered.
    if (combat_.player().backupUse == Combatant::BackupUse::None) return;
    backupShieldUntilMs_ = 0;
    if (const char* id = backupDriveAchievement(combat_.player().backupUse,
                                                combat_.outcome()))
        unlockAchievement(id);
}

Combatant Game::buildPlayerCombatant() {
    // The friendly-visit ally buff — a transient combat modifier
    // consumed one per battle, applied at every real entry into combat (Sim +
    // wild). debugStartCombat is the dev/test hook and stays raw/deterministic.
    Combatant p = makePlayerCombatant(registry_, *pet_, moveLoadout_, loadout_);
    // Backup Drive's timed death-save (save v30): armed while lifetimeUptimeMs() is
    // still under the buff's deadline. Consumption is read back in applyCombatResult /
    // finishBossRound (Combatant::itemShieldFired) to clear backupShieldUntilMs_ early.
    if (backupShieldUntilMs_ != 0 && lifetimeUptimeMs() < backupShieldUntilMs_)
        p.itemShield = true;
    // Per-pet level stat points: additive into the combat maths, stacking with the
    // branch multipliers and mods. Shared with the duel path's remote-fighter rebuild
    // (applyLevelStatPoints, core/model/combat.h) so the two can't drift apart.
    applyLevelStatPoints(p, statPoints_);
    if (allyBuffBattlesLeft_ > 0) {
        p.powerMultPct += kAllyBuffPowerPct;
        --allyBuffBattlesLeft_;
    }
    return p;
}

void Game::startSimBattle() {
    if (!pet_) return;
    Combatant p = buildPlayerCombatant();
    // A Sim Battle has no stakes — losing costs nothing — so there is no death to save
    // the pet from and no reason to spend a Rare item's shield on training. Stripped
    // here rather than in buildPlayerCombatant, which doesn't know the stakes.
    p.itemShield = false;
    CombatEnemy dummy = simDummy(simTier_);
    applySimDummyLevelScale(dummy, combatLevel_);
    Combatant e = makeEnemyCombatant(registry_, dummy);
    rng_ = rng_ * 1664525u + 1013904223u;     // advance for a fresh deterministic seed
    combat_.begin(p, e, Combat::Stakes::Safe, rng_, /*forceEnemyFirst=*/false,
                  /*carryPlayerHealth=*/-1, exploitUsesPerBattle());
    combatBeat_ = 0;
    combatTurnBeat_ = 0;
    nav_ = Nav::Combat;
    dirty_ = true;
}

void Game::onCombat(const ButtonEvent& ev) {
    // Once resolved, the result beat waits for B/C to return to the caller.
    if (combat_.outcome() != Combat::Outcome::Ongoing) {
        if (ev.button == Button::B || ev.button == Button::C) finishCombat();
        return;
    }
    // A linked duel is running the SAME fight on two devices with no turn-by-turn
    // traffic (game_pvp.cpp), so anything that pauses or ends this copy of it would
    // desync the pair: the Exploit picker stops the clock for a human, and fleeing ends
    // a fight the opponent is still playing. Both are inert for the duration, which is
    // why beginPvpBattle grants zero Exploit uses — this is the input half of that.
    const bool duel = pvpFighting();

    // A+C opens the Exploit picker — the chord arrives here because
    // onButton bypasses its early-out in the combat state. The picker lists the
    // pet's moves, any combat-usable items from the live inventory, and — while the
    // player belongs to one — their crew's signature Exploit (game_crew.cpp).
    if (ev.chordAC) {
        if (!duel) combat_.openOverride(combatItemOptions(), crewExploitOption());
        return;
    }
    if (combat_.overrideOpen()) {                 // standard A/B/C inside the picker
        if (ev.button == Button::A) combat_.cycleOverride();
        else if (ev.button == Button::B) {
            combat_.commitOverride();
            // If a USE-ITEM was committed, Combat has already patched Health; the
            // Game now consumes the stack + applies the item's out-of-combat effect.
            if (const char* id = combat_.takeCommittedItem()) consumeCombatItem(id);
        } else if (ev.button == Button::C) combat_.cancelOverride();
        return;
    }
    // B toggles the stat panel — a live buff/debuff readout that does NOT pause the fight.
    if (ev.button == Button::B) { combatStatsOpen_ = !combatStatsOpen_; return; }
    // Auto-play: A fast-forwards the current beat, C flees / quits. A fast-forward is
    // safe in a duel — it only advances THIS screen's playback of an already-decided
    // fight — but the flee isn't (see `duel` above).
    // ...and a Compo match is the second fight with no exit: forfeiting a bracket
    // mid-match would let a losing draw be replayed, which is the whole stake gone.
    // The way out of the arena is the bracket screen, before a match starts.
    const bool noExit = duel || combatCaller_ == CombatCaller::Tourney;
    if (ev.button == Button::A) advanceCombatTurn();
    else if (ev.button == Button::C && !noExit) combat_.flee();
}

int Game::combatBeatsForTurn() const {
    // combat_ already tracks the same-actor streak for its own combo-damage bonus
    // (Combat::streakCount_) — reuse it here rather than a second copy. streakCount()/
    // streakIsPlayer() describe the LAST resolved hit; playerTurnNext() names who's
    // about to act, so if that matches, the upcoming hit extends the run by one.
    // Ramps the wait down from kCombatBeatsPerTurn, floored at kCombatMinBeatsPerTurn.
    const bool nextIsPlayer = combat_.playerTurnNext();
    const int upcoming = (combat_.streakCount() > 0 && nextIsPlayer == combat_.streakIsPlayer())
                              ? combat_.streakCount() + 1
                              : 1;
    const int beats = kCombatBeatsPerTurn - (upcoming - 1);
    return beats < kCombatMinBeatsPerTurn ? kCombatMinBeatsPerTurn : beats;
}

void Game::advanceCombatTurn() {
    combat_.step();
    combatTurnBeat_ = 0;
    combatHitBeat_ = 0;   // restart the impact-punch decay for drawCombat
}

int Game::xpToNextLevel() const {
    // Geometric curve: round(kLevelXpBase * (kLevelXpGrowthPct/100)^level).
    // Integer accumulate so it matches on host + device (no float): each level scales
    // the running need by the growth% with round-to-nearest. level 0→100, 1→110,
    // 2→121, 3→133, 4→146…
    long long need = kLevelXpBase;
    for (int i = 0; i < combatLevel_; ++i)
        need = (need * kLevelXpGrowthPct + 50) / 100;
    return static_cast<int>(need);
}

void Game::addCombatXp(int xp) {
    if (xp <= 0) return;
    combatXp_ += xp;
    // Spend the XP bucket into as many levels as it covers (a big lump can cross more
    // than one boundary). Each level-up grants +1 to a random combat stat, so
    // level always equals the sum of earned points (the Rollback invariant).
    while (combatXp_ >= xpToNextLevel()) {
        combatXp_ -= xpToNextLevel();
        ++combatLevel_;
        rng_ = rng_ * 1664525u + 1013904223u;      // deterministic stat roll
        const int stat = static_cast<int>((rng_ >> 16) % kLevelStatCount);
        ++statPoints_[stat];
        lastLevelUpStat_ = stat;                    // most-recent level-up stat (tell)
    }
    markSaveDirty();
}

std::vector<OverrideItem> Game::combatItemOptions() const {
    // The Exploit picker's USE-ITEM section: every owned stack whose ItemDef
    // marks a combatHeal. Order follows the inventory so it's stable/testable.
    std::vector<OverrideItem> out;
    for (const auto& s : inventory_.stacks()) {
        if (s.qty <= 0) continue;
        const ItemDef* d = registry_.item(s.id);
        if (d && d->combatHeal > 0)
            out.push_back({d->id, d->displayName, d->combatHeal});
    }
    return out;
}

void Game::consumeCombatItem(const char* id) {
    const ItemDef* d = registry_.item(id);
    if (!d) return;
    inventory_.remove(id, 1);
    // Interpretation "1 and 2": the item both patched combat Health (done in
    // Combat) AND applies its own out-of-combat effect to the persistent pet — the same
    // effects[] levers a normal Use would pull.
    applyItemEffects(*d);
    char buf[28];
    std::snprintf(buf, sizeof(buf), "USED %s", d->displayName);
    log_.push(LogEventType::ItemUsed, buf);
    markSaveDirty();
}

int Game::refarmDropScalePct(int refarmCount) {
    if (refarmCount < 0) refarmCount = 0;
    int scale = 100 - refarmCount * kRefarmDropDecayStep;
    if (scale < kRefarmDropFloorPct) scale = kRefarmDropFloorPct;
    return scale;
}

void Game::applyBattleFatigue() {
    // the per-fight fragmentation tax. A kBattleFatigueChancePct roll; on a
    // hit, add random(min..max) Fragmentation. This is the whole "fatigue" mechanic —
    // there's no separate meter, the pet just degrades from fighting and must be
    // defragged (MAINT). Deterministic off the shared LCG. (A future purchasable
    // upgrade could scale the chance/magnitude down before this rolls.)
    rng_ = rng_ * 1664525u + 1013904223u;
    // c: the Reduce-Frag-TRIGGER rig upgrade lowers the trigger CHANCE by the
    // purchased tier (tier 0 = the unchanged baseline; tiers 1..6 use the reduced table).
    const int trigTier = fragTriggerTier();
    const int chance = (trigTier == 0)
                           ? kBattleFatigueChancePct
                           : kFragTriggerReducedPct[trigTier - 1];
    if (static_cast<int>((rng_ >> 16) % 100) >= chance) return;
    rng_ = rng_ * 1664525u + 1013904223u;
    // b: the Reduce-Explore-Frag rig upgrade shaves the frag AMOUNT cap by the
    // purchased tier (floored at the min so a hit still costs ≥1 Frag).
    int effMax = kBattleFatigueFragMax - static_cast<int>(fragAmountTier());
    if (effMax < kBattleFatigueFragMin) effMax = kBattleFatigueFragMin;
    const int span = effMax - kBattleFatigueFragMin + 1;
    int add = kBattleFatigueFragMin +
              static_cast<int>((rng_ >> 16) % static_cast<uint32_t>(span));
    // Heat Sink (mod): dampen the fatigue-Frag tax by its cut %. A big cut can
    // zero out a small tick — the point of the mod is exactly to make farming gentler.
    int cut = sumEquippedModMagnitude(registry_, loadout_, ModEffect::FatigueFragCut,
                                      pet_ ? pet_->line : nullptr);
    if (cut > 100) cut = 100;
    if (cut > 0) add = add * (100 - cut) / 100;
    if (add <= 0) return;
    model_.setFragmentation(model_.fragmentation() + add);
}

void Game::applyCombatResult() {
    settleBackupDrive();
    const bool safe = combat_.stakes() == Combat::Stakes::Safe;
    const bool won = combat_.outcome() == Combat::Outcome::Win;
    const bool lost = combat_.outcome() == Combat::Outcome::Lose;
    // Post-encounter status readout snapshot: the STARTING
    // bandwidth/fragmentation, before anything below moves them. finishCombat()
    // reads the before/after pair (+ postEncShielded_ below) to drive
    // Nav::PostEncounter for a WILD fight; harmless bookkeeping for Sim/boss (they
    // never route to that overlay, so nothing reads it there).
    postEncBwBefore_ = bandwidth_;
    postEncFragBefore_ = model_.fragmentation();
    // Snapshot the pre-reward stat points so the readout can name what this fight's
    // XP levelled up. The reward path below calls addCombatXp, which may cross
    // one or more level boundaries; diffing after (part 2) yields the per-stat gain.
    int statBefore[kLevelStatCount];
    for (int i = 0; i < kLevelStatCount; ++i) statBefore[i] = statPoints_[i];
    // Are we farming an already-CLEARED sub-area? (re-farm context — drives the
    // diminishing-returns loot decay below, once the Bandwidth shield runs dry.)
    const bool refarm =
        exploreSector_ >= 0 && exploreSector_ < kExplSectors &&
        exploreSub_ >= 0 && exploreSub_ < kExplSubAreas &&
        subCleared_[exploreSector_][exploreSub_];
    // a resolved WILD (live, non-boss) fight fatigues the pet — a per-fight
    // frag (corruption) tax, win or lose (boss rounds never reach here; safe Sim is
    // exempt). A flee (Fled) isn't a fought battle, so it's excluded.
    // (D2): Bandwidth is a per-fight FRAGMENTATION SHIELD for
    // exploration. ANY resolved wild fight (win OR loss) spends 1 charge to skip the
    // battle-fatigue frag tax entirely — so a stocked pool lets the pet farm longer with
    // less risk, and Bits sunk into the Hacker SHOP's Bandwidth upgrade buy real safety.
    // Once the pool hits 0 the tax bites again (the "defrag or come home" signal). This
    // covers first-clear grinding and DeepWeb dives too, not just cleared-sub re-farming.
    // Bosses resolve in finishBossRound() and never reach here.
    const bool bandwidthShielded = !safe && (won || lost) && bandwidth_ > 0;
    if (bandwidthShielded) --bandwidth_;               // spend one charge for the shield
    postEncShielded_ = bandwidthShielded;
    if (!safe && (won || lost) && !bandwidthShielded)
        applyBattleFatigue();                          // no charge left → the pet corrupts
    switch (combat_.outcome()) {
        case Combat::Outcome::Win: {
            if (safe) {
                // Sim-Battle: a fixed small flat payout — a few Bits
                // (Packet Sniffer boosts loot), a little combat XP, a small
                // Happiness bump. NOT logged — it's a drill.
                int bits = kSimBitsReward;
                bits += sumEquippedModMagnitude(registry_, loadout_,
                            ModEffect::PostBattleBits, pet_ ? pet_->line : nullptr);
                bits_ += applyCombatBitsBonus(bits);   // Scraping Cluster Expansion
                model_.feed(0, kSimHappyReward);
                addCombatXp(applyCombatXpBonus(kSimXpReward));  // Well-Fed XP Boost
            } else {
                // Wild win: Bits + XP + a chance of an item drop
                // from the shared reward pool. Full drop tables are a later
                // content/balance pass; this seeds the mechanic.
                // payout: randInt(R, R²) keyed to the opponent's stage-rank R.
                // The wild malbeast's difficulty pips (1/2/3) stand in for its
                // stage tier, so R = diffPips+1 → Process 2..4, Script 3..9,
                // Daemon 4..16.
                int bits = normalBitsReward(encounterEnemy_.diffPips + 1, rng_);
                // depth ramp, Bits half: mirrors the XP bonus below onto
                // Bits, since diffPips (and so the base roll) doesn't move with depth
                // on its own — otherwise a deep dive would pay the same Bits as a
                // fresh one despite the tougher fight.
                if (inDeepWebDive())
                    bits = bits * deepWebDepthBitsPct(exploreStreak_) / 100;
                bits += sumEquippedModMagnitude(registry_, loadout_,
                            ModEffect::PostBattleBits, pet_ ? pet_->line : nullptr);
                bits_ += applyCombatBitsBonus(bits);   // Scraping Cluster Expansion
                // scale the XP by how the enemy's depth level compares to
                // the pet's — punching up pays more, farming shallow pays a trickle.
                addCombatXp(applyCombatXpBonus(wildWinXp(kWildWinXpReward, encounterEnemy_.level,
                                      combatLevel_)));  // Well-Fed XP Boost
                // re-farm loot decay: farming an already-CLEARED sub-area pays
                // FULL non-Bits loot while the Bandwidth shield covered this fight (the
                // decay count stays frozen). Once the pool is dry (this fight was NOT
                // shielded), the diminishing-returns curve kicks in: drop chances decay per
                // re-farm win toward a floor and the per-sub count advances (saturating;
                // persists at save v15) until Bandwidth regenerates over real time (tick()).
                // Bits + XP (above) stay full throughout, so a done area is always a viable
                // training ground. First-clear grinding / bosses / Sim / the DeepWeb Dive
                // never take this branch (only cleared-sub farming does). The Bandwidth
                // charge itself was already spent at the shield site above — it now drains
                // on every fight, not just re-farms.
                int dropScalePct = 100;
                if (refarm && !bandwidthShielded) {
                    uint16_t& c = subRefarmCount_[exploreSector_][exploreSub_];
                    dropScalePct = refarmDropScalePct(c);
                    if (c < kRefarmCountCap) ++c;
                }
                const int itemDropPct = kWildItemDropPct * dropScalePct / 100;
                const int moveDropPct = kWildMoveDropPct * dropScalePct / 100;
                rng_ = rng_ * 1664525u + 1013904223u;
                if (static_cast<int>((rng_ >> 16) % 100) < itemDropPct) {
                    // What a wild win pays is the AREA's business: each row under
                    // content/areas/ carries its own wild drop table, including whichever
                    // Merge Hub ingredient it makes farmable here. Drawn by the same
                    // weighted walk every other pool uses.
                    const AreaLootTable t = areaWildLootTable(exploreSector_);
                    drawCacheItem(t.rows, t.count);
                }
                // A separate, independent move-drop roll, mirroring the item roll above.
                rollEnemyMoveDrop(combat_.enemy(), moveDropPct);
                // the endless DeepWeb Dive is the tier-4 mod source — a rare
                // roll on a win drops one of its permanent endgame mods (Deadman Switch /
                // RAID Mirror). The rolled equip-LEVEL gate still applies, so a lucky drop
                // is held until the pet is deep enough to field it.
                if (exploreSector_ == kDeepWebSector) {
                    rng_ = rng_ * 1664525u + 1013904223u;
                    if (static_cast<int>((rng_ >> 16) % 100) < kModDeepWebDropPct)
                        if (const char* id = rollAreaModId(kDeepWebSector))
                            grantMod(id);
                }
                log_.push(LogEventType::CombatWon, "WON BATTLE");
            }
            break;
        }
        case Combat::Outcome::Lose:
            // Live stakes: +Fragmentation (×Bad-branch multiplier) + COMBAT_LOST
            // NOT a care mistake. Safe (Sim-Battle): no Frag, no mistake.
            if (!safe) {
                const int frag = kWildLossFrag * combat_.player().fragMultPct / 100;
                model_.setFragmentation(model_.fragmentation() + frag);
                log_.push(LogEventType::CombatLost, "LOST BATTLE");
            }
            break;
        case Combat::Outcome::Fled:
        case Combat::Outcome::Ongoing:
            break;                                 // flee: no reward, no penalty
    }
    // Post-encounter status readout snapshot, part 2: the ENDING values, now that
    // the switch above has applied whatever this outcome caused.
    postEncBwAfter_ = bandwidth_;
    postEncFragAfter_ = model_.fragmentation();
    for (int i = 0; i < kLevelStatCount; ++i)
        postEncStatGain_[i] = statPoints_[i] - statBefore[i];
    postEncLevelAfter_ = combatLevel_;
    markSaveDirty();
}

void Game::finishCombat() {
    combatStatsOpen_ = false;   // clear the stat panel so the next fight opens closed
    // A duel has its own result path (a banner on the LINK screen and a log line, and
    // deliberately nothing else — no stakes means no reward and no penalty), so it
    // hands off before applyCombatResult() the same way a boss round does.
    if (pvpFighting()) { finishPvpBattle(); return; }
    // A boss/gauntlet round has its own result path (advance/clear/fail + the
    // lump), NOT the wild loot rolls — hand off before applyCombatResult().
    if (combatCaller_ == CombatCaller::Boss) { finishBossRound(); return; }
    // A Compo match advances a bracket instead of paying a fight (game_tourney.cpp):
    // Safe stakes, no loot rolls, and nothing but the title pays. Same hand-off shape.
    if (combatCaller_ == CombatCaller::Tourney) { finishTourneyMatch(); return; }
    applyCombatResult();
    // Return to the caller — never a fixed layer. Sim-Battle returns to the
    // LOADOUT hub; a wild encounter returns to the idle habitat (explore-mode),
    // not the EXPL list or the carousel.
    if (combatCaller_ == CombatCaller::Wild) {
        // Explore win-streak: a win advances the streak (10 in a row →
        // unlock the sector's boss, unless already cleared); a loss cancels the mode
        // and resets the streak. That's the whole risk model.
        if (combat_.outcome() == Combat::Outcome::Win) {
            // 'Pedia malbeast "defeated": a WON live wild fight
            // (never a boss round — those return via finishBossRound() above before
            // ever reaching here, and Sim never reaches this branch either).
            if (const int idx = wildMalbeastIndex(encounterEnemy_.name);
                idx >= 0 && !(malbeastDefeatedMask_ & (1u << idx))) {
                malbeastDefeatedMask_ |= static_cast<uint16_t>(1u << idx);
                markSaveDirty();
            }
            if (exploreActive_) {
                // In the endless DeepWeb Dive an armed Deep-Learning Module/Core advances
                // the streak (= depth) by more than one step per win, so a
                // blitzing endgame pet catches back up to a genuine struggle faster.
                // Every other area always advances by exactly 1.
                exploreStreak_ += inDeepWebDive() ? deepWebDepthMultiplier_ : 1;
                // 10 wins in a row unlock the ARMED sub-area's boss, unless
                // it's already cleared (re-farming a done sub-area never re-arms it).
                if (exploreStreak_ >= kExploreStreakToBoss &&
                    exploreSector_ >= 0 && exploreSector_ < kExplSectors &&
                    exploreSub_ >= 0 && exploreSub_ < kExplSubAreas &&
                    !subCleared_[exploreSector_][exploreSub_])
                    subBossUnlocked_[exploreSector_][exploreSub_] = true;
                // In the endless DeepWeb Dive the streak IS the DEPTH. Two records get
                // it: this PET's own (read back by the Zero-Day Bell) and its SPECIES'
                // (which outlives the pet, and is what the depth achievements — device
                // best, best per line, and how many species have been taken deep — are
                // all counted off).
                if (inDeepWebDive()) {
                    if (exploreStreak_ > bestDeepWebDepth_) bestDeepWebDepth_ = exploreStreak_;
                    recordSpeciesDive(pet_, exploreStreak_);
                }
            }
        } else if (combat_.outcome() == Combat::Outcome::Lose) {
            // A loss ends the run. Same rule as the Stop row: only a DIVE ending spends
            // the depth multiplier, and inDeepWebDive() is derived from exploreActive_,
            // so it has to be read first.
            if (inDeepWebDive()) deepWebDepthMultiplier_ = 1;
            exploreActive_ = false;
            exploreStreak_ = 0;
        }
        // Post-encounter status readout: a FOUGHT battle (win or
        // loss) parks on the BANDWIDTH/FRAG readout before the habitat — a flee
        // never reaches finishCombat() with those stakes touched, so it skips
        // straight back (returnToExplore also surfaces an awakened-guardian
        // rank-up).
        if (combat_.outcome() == Combat::Outcome::Win ||
            combat_.outcome() == Combat::Outcome::Lose) {
            nav_ = Nav::PostEncounter;
            postEncounterDeadlineMs_ = nowMs_ + kPostEncounterMs;
        } else {
            returnToExplore();
        }
    } else {
        // Sim-Battle came from PRACTISE, so it goes back to the LOADOUT hub with that
        // row still focused — the fight is over, not the menu.
        trainScreen_ = TrainScreen::MovePicker;
        leaveLoadoutTab();
    }
    dirty_ = true;
    persistSave();
}

void Game::debugStartCombat(bool live, bool lethal) {
    if (!pet_) return;
    Combatant p = makePlayerCombatant(registry_, *pet_, moveLoadout_, loadout_);
    CombatEnemy spec = lethal
        // An unbeatable enemy: huge Health + an overwhelming move so a loss is
        // deterministic (exercises the live-vs-safe stakes contract).
        ? CombatEnemy{"Lethal", "SPR_PET_CACHEMUTT", 3, 9999, 99, {"packet_storm"}}
        : simDummy(simTier_);
    if (lethal) {
        // Give the lethal enemy a one-shot kill regardless of move power.
        p.maxHealth = 1;
    }
    Combatant e = makeEnemyCombatant(registry_, spec);
    rng_ = rng_ * 1664525u + 1013904223u;
    combat_.begin(p, e, live ? Combat::Stakes::Live : Combat::Stakes::Safe, rng_,
                  /*forceEnemyFirst=*/false, /*carryPlayerHealth=*/-1,
                  exploitUsesPerBattle());
    combatCaller_ = CombatCaller::Sim;   // the dev hook always returns to the hub
    combatBeat_ = 0;
    combatTurnBeat_ = 0;
    nav_ = Nav::Combat;
    dirty_ = true;
}

void Game::debugSetNetworksSeen(int n) {
    if (n < 0) n = 0;
    networksSeen_ = n;   // unbounded (XP model) — no ledger/pool to keep in sync now
    hackerRank_ = hackerRankForNetworksSeen(networksSeen_);   // silent — no celebration
}

void Game::debugUseItem(const char* id) {
    const ItemDef* d = registry_.item(id);
    if (!d) return;
    detailItem_ = d;
    lockoutItemsContext_ = false;   // normal (non-Lockout) use context
    useItem();                      // real path: gate -> effect -> consume -> log
}

void Game::debugOpenCache(const char* id) {
    // Sealed caches decrypt from the Hacker VAULT, not pet-side ITEMS — this
    // seam drives the same real reward path (openSealedCache) onHackerVault uses.
    const ItemDef* d = registry_.item(id);
    if (d) openSealedCache(*d);
}

void Game::debugSetBits(int n) { bits_ = n < 0 ? 0 : n; }

void Game::debugFillLoadout() {
    if (!pet_) return;
    // Moves: the first owned move that matches the slot's stamped kind and isn't
    // already sitting in an earlier slot (equipping a move MOVES it, so reusing one
    // would empty the slot behind us and leave the page shorter than it should be).
    const int slots = MoveLoadout::slotsForStage(pet_->stage);
    for (int s = 0; s < slots; ++s)
        for (const char* id : moveLoadout_.owned()) {
            const MoveDef* m = registry_.move(id);
            if (!m || m->kind != slotRequiredKind(s)) continue;
            if (moveLoadout_.slotOf(id) >= 0) continue;
            moveLoadout_.equip(s, m->id);
            break;
        }
    // Mods: any three distinct rows, placed directly (setEquipped, no spare consumed).
    int filled = 0;
    for (const ModDef* d : registry_.allMods()) {
        if (filled >= kModSlots) break;
        if (loadout_.slotOf(d->id) >= 0) continue;
        loadout_.setEquipped(filled++, d->id);
    }
    markSaveDirty();
}


void Game::startEncounter() {
    rng_ = rng_ * 1664525u + 1013904223u;                    // roster variant roll
    if (inDeepWebDive()) {
        // the endless zone draws the endgame (tier-3) roster and scales it to
        // the PET's level, ramped by the dive's current win-streak (exploreStreak_) so
        // diving deeper gradually punches the pet up (more XP, tougher enemy) instead of
        // sitting at flat parity forever.
        encounterEnemy_ = wildMalbeast(3, rng_ >> 16);
        rng_ = rng_ * 1664525u + 1013904223u;                // the dive's own stat/kit roll
        applyDeepWebScale(encounterEnemy_, combatLevel_, exploreStreak_, rng_);
    } else {
        encounterEnemy_ = wildMalbeast(explSectorTier(exploreSector_), rng_ >> 16);
        applyWildSubAreaRamp(encounterEnemy_, exploreSector_, exploreSub_);  // depth ramp
    }
    // 'Pedia malbeast "seen": the wild roll itself is the
    // glimpse, regardless of what happens next (fight/flee/sinkhole/auto-resolve).
    // wildMalbeastIndex returns -1 for anything outside the fixed 6-entry roster.
    if (const int idx = wildMalbeastIndex(encounterEnemy_.name);
        idx >= 0 && !(malbeastSeenMask_ & (1u << idx))) {
        malbeastSeenMask_ |= static_cast<uint16_t>(1u << idx);
        markSaveDirty();
    }
    encounterChoice_ = 0;
    encounterSinkhole_ = inventory_.count("sinkhole_trap") > 0;
    // Auto-explore is a hands-off background mode: a wild encounter resolves
    // itself — there's no human at the intro to press Fight, and Flee is pointless
    // since a loss already cancels the mode + zeroes the streak (finishCombat). A
    // held Sinkhole Trap still auto-bypasses for XP (its whole purpose);
    // otherwise the fight auto-starts. The Fight/Flee/Sinkhole intro stands only for a
    // (currently unused) manual encounter path.
    if (exploreActive_) {
        if (encounterSinkhole_) resolveSinkhole();
        else startWildCombat(/*forceEnemyFirst=*/false);
        return;
    }
    nav_ = Nav::Encounter;
}

void Game::onEncounter(const ButtonEvent& ev) {
    const int options = encounterSinkhole_ ? 3 : 2;
    if (ev.button == Button::A) {
        encounterChoice_ = (encounterChoice_ + 1) % options;
    } else if (ev.button == Button::B) {
        if (encounterChoice_ == 0) startWildCombat(/*forceEnemyFirst=*/false);
        else if (encounterChoice_ == 1) resolveFlee();
        else resolveSinkhole();
    } else if (ev.button == Button::C) {
        resolveFlee();                              // flee shortcut
    }
}

void Game::resolveFlee() {
    rng_ = rng_ * 1664525u + 1013904223u;
    const bool escaped = static_cast<int>((rng_ >> 16) % 100) < kFleeChancePct;
    if (escaped) {
        std::snprintf(exploreFlavor_, sizeof(exploreFlavor_), "GOT AWAY");
        returnToExplore();
    } else {
        // A failed flee forces the fight, enemy first (retreat isn't free).
        startWildCombat(/*forceEnemyFirst=*/true);
    }
}

void Game::resolveSinkhole() {
    // The bypass XP lump is the Sinkhole Trap's own preEncounterXp (defs.h row), not a
    // global tunable — one item's magnitude lives on the item.
    const ItemDef* d = registry_.item("sinkhole_trap");
    const int xp = d ? d->preEncounterXp : 0;
    inventory_.remove("sinkhole_trap", 1);
    addCombatXp(xp);
    log_.push(LogEventType::ItemUsed, "USED SINKHOLE TRAP");
    std::snprintf(exploreFlavor_, sizeof(exploreFlavor_), "SINKHOLE +%d XP", xp);
    // Every other XP grant (finishCombat/finishBossRound) persists immediately rather
    // than leaving it to the debounced autosave — a level-up is exactly the kind of
    // progress that must survive a reboot moments later, not just eventually. This is
    // the one XP source that bypasses combat entirely, so it needs its own persist.
    persistSave();
    returnToExplore();
}

void Game::startWildCombat(bool forceEnemyFirst) {
    if (!pet_) return;
    Combatant p = buildPlayerCombatant();
    Combatant e = makeEnemyCombatant(registry_, encounterEnemy_);
    rng_ = rng_ * 1664525u + 1013904223u;
    combat_.begin(p, e, Combat::Stakes::Live, rng_, forceEnemyFirst,
                  /*carryPlayerHealth=*/-1, exploitUsesPerBattle());
    combatCaller_ = CombatCaller::Wild;
    combatBeat_ = 0;
    combatTurnBeat_ = 0;
    nav_ = Nav::Combat;
    dirty_ = true;
}

}  // namespace mal
