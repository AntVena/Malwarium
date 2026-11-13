#include "core/app/game.h"

#include <cstdio>
#include <cstring>

#include "tunables.h"
#include "core/content/areas/darkweb_crawl/area.h"
#include "core/app/game_internal.h"   // backupDriveAchievement: declared, defined here
#include "core/ui/combat_screen.h"
#include "core/ui/expl_screen.h"

namespace mal {

namespace {
// Sum the magnitude of every EQUIPPED mod with a given effect kind, folding
// in a line-affinity bonus when the pet's line matches. Used for the non-Combatant-field
// mod effects that makePlayerCombatant's loop can't apply itself: the post-battle Bits
// bonus (Packet Sniffer, PostBattleBits) and the battle-fatigue Frag cut (Heat Sink,
// FatigueFragCut).
int sumEquippedModMagnitude(const ContentRegistry& reg, const ModSlots& load,
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

// The combat activity ---------------------------

// The rule for what a MIRROR is, in the one place both seams can reach it. Compared by
// species id rather than by anything the fight computed: two pets of one species are the
// mirror whatever their levels, kits or branches did, and that is what the operator sees.
// Cheap and idempotent, so a caller fires it on every bout without guarding.
void Game::noteMirrorMatch(const char* opponentCreatureId) {
    if (!opponentCreatureId || !pet_.creature || !pet_.creature->id) return;
    if (std::strcmp(pet_.creature->id, opponentCreatureId) == 0)
        unlockAchievement(ach::kHashCollision);
}

const char* backupDriveAchievement(Combatant::BackupUse used, Combat::Outcome outcome) {
    // Overwhelmed is read FIRST because a mutual KO resolves as a Win: the pet still
    // didn't get up, and what the player watched was the restore failing.
    if (used == Combatant::BackupUse::None) return nullptr;
    if (used == Combatant::BackupUse::Overwhelmed) return ach::kShatteredPlatter;
    if (outcome == Combat::Outcome::Lose || outcome == Combat::Outcome::Draw)
        return ach::kNeededMoreBackup;               // a draw is a loss
    if (outcome == Combat::Outcome::Win) return ach::kBackUpAndDriven;
    return nullptr;   // Fled: the run isn't over, so neither is the story
}

void Game::settleBackupDrive() {
    // Every path out of a fight lands here (applyCombatResult for wild/Sim, and every
    // boss round), so a spent drive is settled in exactly one place. It is spent for the
    // rest of its 1h window, win or lose.
    if (fight_.combat.player().backupUse == Combatant::BackupUse::None) return;
    pet_.armed.backupShieldUntilMs = 0;
    if (const char* id = backupDriveAchievement(fight_.combat.player().backupUse,
                                                fight_.combat.outcome()))
        unlockAchievement(id);
}

Combatant Game::buildPlayerCombatant() {
    // In the DARKWEB CRAWL the Silk Lode's apex rider is simply the weather: every fight
    // runs with the A+C picker reordered and drawn in the Cant, so what the pet can still
    // use is exactly what the device can still read. -1 is the "for the whole fight"
    // sentinel (Combatant::scrambleTurns) rather than a big number, because a big number
    // is an assumption about how long a fight lasts and this is not one.
    // Crib Sheet still answers it, which is why the ward is checked here and not only in
    // applyEffect: a mod that refused a three-turn rider but not the zone built out of it
    // would be selling half of what it says.
    // The friendly-visit ally buff: a transient combat modifier
    // consumed one per battle, applied at every real entry into combat (Sim +
    // wild). debugStartCombat is the dev/test hook and stays raw/deterministic.
    Combatant p = makePlayerCombatant(registry_, *pet_.creature, pet_.moves, pet_.mods);
    // Backup Drive's timed death-save (save v30): armed while lifetimeUptimeMs() is
    // still under the buff's deadline. Consumption is read back in applyCombatResult /
    // finishBossRound (Combatant::itemShieldFired) to clear pet_.armed.backupShieldUntilMs early.
    if (pet_.armed.backupShieldUntilMs != 0 && lifetimeUptimeMs() < pet_.armed.backupShieldUntilMs)
        p.itemShield = true;
    if (inDarkWebCrawl() && p.mods.mag(ModEffect::ScrambleWard) <= 0)
        p.scrambleTurns = kScrambleWholeFight;
    // Per-pet stat points: additive into the combat maths, stacking with the branch
    // multipliers and mods. Shared with the duel path's remote-fighter rebuild
    // (applyLevelStatPoints, core/model/combat.h) so the two can't drift apart. Earned
    // and granted points go in as one total. An Epic dish's off-level point counts
    // fully in a fight; only the level and the Rollback picker tell them apart.
    int points[kLevelStatCount];
    for (int i = 0; i < kLevelStatCount; ++i) points[i] = pet_.totalStatPoint(i);
    applyLevelStatPoints(p, points);
    if (allyBuffBattlesLeft_ > 0) {
        p.powerMultPct += kAllyBuffPowerPct;
        --allyBuffBattlesLeft_;
    }
    return p;
}

void Game::startSimBattle() {
    if (!pet_.creature) return;
    Combatant p = buildPlayerCombatant();
    // A Sim Battle has no stakes (losing costs nothing), so there is no death to save
    // the pet from and no reason to spend a Rare item's shield on training. Stripped
    // here rather than in buildPlayerCombatant, which doesn't know the stakes.
    p.itemShield = false;
    CombatEnemy dummy = simDummy(simTier_);
    applySimDummyLevelScale(dummy, pet_.level);
    Combatant e = makeEnemyCombatant(registry_, dummy);
    rng_ = rng_ * 1664525u + 1013904223u;     // advance for a fresh deterministic seed
    fight_ = Fight::started(FightCaller::Sim);
    fight_.combat.begin(p, e, Combat::Stakes::Safe, rng_, /*forceEnemyFirst=*/false,
                        /*carryPlayerHealth=*/-1, exploitUsesPerBattle());
    fxBeat_ = 0;
    nav_ = Nav::Combat;
    dirty_ = true;
    playSound(Sound::CombatStart);
}

// --- FightScreenHost: the Combat screen's way back into the game --------------

const SpriteData* Game::petSprite() const {
    return pet_.creature ? registry_.creatureSprite(*pet_.creature) : nullptr;
}

void Game::openExploitPicker() {
    // The pet's Cant goes in with the picker, because it decides how much of a
    // scrambled move the pet can read (MoveDef::scrambleTurns) and Combat has no
    // other source for it. The sigils are the device's, not the fight's.
    fight_.combat.openOverride(combatItemOptions(), crewExploitOption(), op_.cant.sigils);
}

void Game::stepGuardianFlock() { guardianFlock_.step(guardianFlockMood()); }

int Game::combatBeatsForTurn() const {
    // Combat already tracks the same-actor streak for its own combo-damage bonus
    // (Combat::streakCount_): this reuses it. streakCount()/ streakIsPlayer() describe
    // the LAST resolved hit; playerTurnNext() names who's about to act, so if that
    // matches, the upcoming hit extends the run by one. Ramps the wait down from
    // kCombatBeatsPerTurn, floored at kCombatMinBeatsPerTurn.
    const Combat& c = fight_.combat;
    const bool nextIsPlayer = c.playerTurnNext();
    const int upcoming = (c.streakCount() > 0 && nextIsPlayer == c.streakIsPlayer())
                              ? c.streakCount() + 1
                              : 1;
    const int beats = kCombatBeatsPerTurn - (upcoming - 1);
    return beats < kCombatMinBeatsPerTurn ? kCombatMinBeatsPerTurn : beats;
}

void Game::advanceCombatTurn() {
    fight_.combat.step();
    fight_.turnBeat = 0;
    fight_.hitBeat = 0;   // restart the impact-punch decay for drawCombat
    playCombatTurnSound();
    // FX_CAMO deliberately takes nothing from here. The colours the pet is wearing are a
    // reading of its LIVE cast, eased every anim tick (tick(), camoAdvance), so this
    // step changes them only by changing what the pet holds. The rival's turn never
    // touches them.
}

bool Game::exploreFightLive() const {
    return walk_.active && nav_ == Nav::Combat &&
           (fight_.caller == FightCaller::Wild || fight_.caller == FightCaller::Boss);
}

void Game::playCombatTurnSound() {
    const Combat& c = fight_.combat;
    switch (c.outcome()) {
        case Combat::Outcome::Ongoing: break;
        case Combat::Outcome::Fled: playSound(Sound::Fled); return;
        default: playSound(Sound::Knockout); return;
    }
    // Nobody is watching an auto-explore fight swing by swing; a cue every turn would
    // only be noise from a device sitting in a pocket. The start and the KO still sound.
    if (exploreFightLive()) return;
    if (c.lastWasStunned()) playSound(Sound::Stunned);
    // Only a swing gets a hit cue: the passive ticks and wind-ups would make every turn
    // beep, and a shielded or ransomed swing that moved no Health reads as a block.
    else if (!c.lastWasStrike()) return;
    else if (c.lastDamage() == 0 || c.lastRansomed()) playSound(Sound::Blocked);
    else playSound(c.lastByPlayer() ? Sound::HitDealt : Sound::HitTaken);
}

int Game::xpForLevel(int level) const {
    // Geometric curve: round(kLevelXpBase * (kLevelXpGrowthPct/100)^level).
    // Integer accumulate so it matches on host + device (no float): each level scales
    // the running need by the growth% with round-to-nearest. level 0→100, 1→110,
    // 2→121, 3→133, 4→146…
    long long need = kLevelXpBase;
    for (int i = 0; i < level; ++i)
        need = (need * kLevelXpGrowthPct + 50) / 100;
    return static_cast<int>(need);
}

int Game::xpToNextLevel() const { return xpForLevel(pet_.level); }

int Game::scaleXp(int xp) const {
    // The armed Sandbox/Hypervisor-USB soak is the OTHER half of the trade it makes: the
    // stage's evolution dwell runs this many times longer (Game::evolveDwellMs) and every
    // XP award pays this many times more. Applied first, as a multiplier on the raw
    // award, so the rates below still read as a percentage of what the source actually
    // paid rather than of a number one item already scaled.
    if (pet_.usb.soakFactor > 1) xp *= pet_.usb.soakFactor;
    // Profilerole's permanent rate and the pet's Bond add up into one percentage, so
    // neither compounds on the other. Applied at the one door every XP source comes
    // through, so they raise combat, solved boards and passive farming alike. Rounded
    // down, so a rate only adds.
    const int rateBp = pet_.upgrades.xpRatePct * 100 + bondTotalBp(pet_.upgrades);
    return xp + static_cast<int>(static_cast<int64_t>(xp) * rateBp / 10000);
}

void Game::addFightXp(int xp) {
    if (xp <= 0) return;
    // Bond counts the award as the fight paid it, before the pet's own rates, so Bond
    // never feeds on itself.
    addBond(pet_.upgrades, BondSource::Adventure, xp * kBondAdventureBpPerXp);
    addCombatXp(xp);
}

void Game::addCombatXp(int xp) {
    if (xp <= 0) return;
    pet_.xp += scaleXp(xp);
    // Spend the XP bucket into as many levels as it covers (a big lump can cross more
    // than one boundary). Each level-up grants +1 to a random combat stat, so
    // level always equals the sum of earned points (the Rollback invariant).
    const int levelBefore = pet_.level;
    while (pet_.xp >= xpToNextLevel()) {
        pet_.xp -= xpToNextLevel();
        ++pet_.level;
        rng_ = rng_ * 1664525u + 1013904223u;      // deterministic stat roll
        const int stat = static_cast<int>((rng_ >> 16) % kLevelStatCount);
        ++pet_.statPoints[stat];
        lastLevelUpStat_ = stat;                    // most-recent level-up stat (tell)
    }
    if (pet_.level > levelBefore) playSound(Sound::LevelUp);
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
    // Combat) AND applies its own out-of-combat effect to the persistent pet. The same
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
    // the per-fight fragmentation tax. A kBattleFatigueChancePct roll; on a hit, add
    // random(min..max) Fragmentation. This is the fatigue mechanic. There is no
    // separate meter: fighting fragments the pet, and MAINT's defrag repairs it.
    // Deterministic off the shared LCG. (A future purchasable upgrade could scale
    // the chance/magnitude down before this rolls.)
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
    // Heat Sink (mod): dampen the fatigue-Frag tax by its cut %. A big cut can zero a
    // small tick, which is the mod's purpose: gentler farming.
    int cut = sumEquippedModMagnitude(registry_, pet_.mods, ModEffect::FatigueFragCut,
                                      pet_.creature ? pet_.creature->line : nullptr);
    if (cut > 100) cut = 100;
    if (cut > 0) add = add * (100 - cut) / 100;
    if (add <= 0) return;
    pet_.model.setFragmentation(pet_.model.fragmentation() + add);
}

void Game::applyCombatResult() {
    settleBackupDrive();
    const bool safe = fight_.combat.stakes() == Combat::Stakes::Safe;
    const bool won = fight_.combat.outcome() == Combat::Outcome::Win;
    const bool lost = fight_.combat.lostOrDrawn();   // a draw (the turn limit) is a loss
    const bool fought = won || lost;
    // Post-encounter status readout snapshot: the STARTING
    // bandwidth/fragmentation, before anything below moves them. finishCombat()
    // reads the before/after pair (+ the shield flag below) to drive
    // Nav::PostEncounter for a WILD fight; harmless bookkeeping for Sim/boss (they
    // never route to that overlay, so nothing reads it there).
    fight_.result.bwBefore = bandwidth_;
    fight_.result.fragBefore = pet_.model.fragmentation();
    // Snapshot the pre-reward stat points so the readout can name what this fight's
    // XP levelled up. The reward path below calls addCombatXp, which may cross
    // one or more level boundaries; diffing after (part 2) yields the per-stat gain.
    int statBefore[kLevelStatCount];
    for (int i = 0; i < kLevelStatCount; ++i) statBefore[i] = pet_.statPoints[i];
    // Are we farming an already-CLEARED sub-area? (re-farm context: drives the
    // diminishing-returns loot decay below, once the Bandwidth shield runs dry.)
    const bool refarm =
        walk_.sector >= 0 && walk_.sector < kExplSectors &&
        walk_.sub >= 0 && walk_.sub < kExplSubAreas &&
        op_.world.subCleared[walk_.sector][walk_.sub];
    // a resolved WILD (live, non-boss) fight fatigues the pet: a per-fight frag
    // (corruption) tax, win or lose (boss rounds never reach here; safe Sim is exempt). A
    // flee (Fled) isn't a fought battle, so it's excluded. Bandwidth is a per-fight
    // FRAGMENTATION SHIELD for exploration. Any resolved wild fight (win or loss) spends
    // 1 charge to skip the battle-fatigue frag tax entirely, so a stocked pool lets the
    // pet farm longer with less risk, and Bits sunk into the Hacker SHOP's Bandwidth
    // upgrade buy real safety. Once the pool hits 0 the tax bites again (the "defrag or
    // come home" signal). This covers first-clear grinding and DeepWeb dives too, not
    // just cleared-sub re-farming. Bosses resolve in finishBossRound() and never reach
    // here.
    const bool bandwidthShielded = !safe && fought && bandwidth_ > 0;
    if (bandwidthShielded) --bandwidth_;               // spend one charge for the shield
    fight_.result.shielded = bandwidthShielded;
    if (!safe && fought && !bandwidthShielded)
        applyBattleFatigue();                          // no charge left → the pet corrupts
    switch (fight_.combat.outcome()) {
        case Combat::Outcome::Win: {
            if (safe) {
                // Sim-Battle: a fixed small flat payout. A few Bits
                // (Packet Sniffer boosts loot), a little combat XP, a small
                // Happiness bump. NOT logged: it's a drill.
                int bits = kSimBitsReward;
                bits += sumEquippedModMagnitude(registry_, pet_.mods,
                            ModEffect::PostBattleBits, pet_.creature ? pet_.creature->line : nullptr);
                op_.bits += applyCombatBitsBonus(bits);   // Scraping Cluster Expansion
                pet_.model.feed(0, kSimHappyReward);
                addFightXp(applyCombatXpBonus(kSimXpReward));  // Well-Fed XP Boost
            } else {
                // Wild win: Bits + XP + a chance of an item drop
                // from the shared reward pool. Full drop tables are a later
                // content/balance pass; this seeds the mechanic.
                // payout: randInt(R, R²) keyed to the opponent's stage-rank R.
                // The wild malbeast's difficulty pips (1/2/3) stand in for its
                // stage tier, so R = diffPips+1 → Process 2..4, Script 3..9,
                // Daemon 4..16.
                int bits = normalBitsReward(fight_.combat.enemy().diffPips + 1, rng_);
                // depth ramp, Bits half: mirrors the XP bonus below onto Bits,
                // because diffPips (and so the base roll) does not move with depth,
                // and a deep dive would otherwise pay the same Bits as a fresh one.
                if (inDeepWebDive())
                    bits = bits * deepWebDepthBitsPct(walk_.streak) / 100
                                * (100 + diveRewardBonusPct()) / 100;   // DIVE PAY
                else if (inDarkWebCrawl())
                    bits = bits * darkWebDepthBitsPct(walk_.streak) / 100;
                bits += sumEquippedModMagnitude(registry_, pet_.mods,
                            ModEffect::PostBattleBits, pet_.creature ? pet_.creature->line : nullptr);
                op_.bits += applyCombatBitsBonus(bits);   // Scraping Cluster Expansion
                // scale the ZONE's base XP by how the enemy's depth level compares to
                // the pet's: punching up pays more, farming shallow pays a trickle. The
                // base is the flat one everywhere but the CRAWL, which pays a multiple of
                // it because its runs end long before a depth curve is worth anything
                // (wildWinXpBase → kDarkWebXpPct, darkweb_crawl/area.h).
                // An armed bell or Deep-Learning device adds its DIVE PAY on top (zero
                // outside a dive): the run it buys is shorter and harder, so each win in
                // it has to be worth more than the shallow ones it skipped.
                const int winXp = wildWinXp(wildWinXpBase(walk_.sector),
                                            fight_.combat.enemy().level, pet_.level) *
                                  (100 + diveRewardBonusPct()) / 100;
                addFightXp(applyCombatXpBonus(winXp));  // Well-Fed XP Boost
                // re-farm loot decay: farming an already-CLEARED sub-area pays FULL non-Bits
                // loot while the Bandwidth shield covered this fight (the decay count stays
                // frozen). Once the pool is dry (this fight was NOT shielded), the
                // diminishing-returns curve kicks in: drop chances decay per re-farm win
                // toward a floor and the per-sub count advances (saturating; persists at
                // save v15) until Bandwidth regenerates over real time (tick()). Bits + XP
                // (above) stay full throughout, so a done area is always a viable training
                // ground. First-clear grinding / bosses / Sim / the DeepWeb Dive never take
                // this branch (only cleared-sub farming does). The Bandwidth charge was
                // already spent at the shield site above, on every fight.
                int dropScalePct = 100;
                if (refarm && !bandwidthShielded) {
                    uint16_t& c = pet_.armed.subRefarmCount[walk_.sector][walk_.sub];
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
                    const AreaLootTable t = areaWildLootTable(walk_.sector);
                    drawCacheItem(t.rows, t.count);
                }
                // A separate, independent move-drop roll, mirroring the item roll above.
                rollEnemyMoveDrop(fight_.combat.enemy(), moveDropPct);
                // the endless DeepWeb Dive is the tier-4 mod source: a rare
                // roll on a win drops one of its permanent endgame mods (Deadman Switch /
                // RAID Mirror). The rolled equip-LEVEL gate still applies, so a lucky drop
                // is held until the pet is deep enough to field it.
                if (walk_.sector == kDeepWebSector || walk_.sector == kDarkWebSector) {
                    rng_ = rng_ * 1664525u + 1013904223u;
                    if (static_cast<int>((rng_ >> 16) % 100) < kModDeepWebDropPct)
                        if (const char* id = rollAreaModId(walk_.sector))
                            grantMod(id);
                }
                log_.push(LogEventType::CombatWon, "WON BATTLE");
            }
            break;
        }
        case Combat::Outcome::Lose:
        case Combat::Outcome::Draw:                // a draw (the turn limit) is a loss
            // Live stakes: +Fragmentation (×Bad-branch multiplier) + COMBAT_LOST
            // NOT a care mistake. Safe (Sim-Battle): no Frag, no mistake.
            if (!safe) {
                const int frag = kWildLossFrag * fight_.combat.player().fragMultPct / 100;
                pet_.model.setFragmentation(pet_.model.fragmentation() + frag);
                log_.push(LogEventType::CombatLost, "LOST BATTLE");
            }
            break;
        case Combat::Outcome::Fled:
        case Combat::Outcome::Ongoing:
            break;                                 // flee: no reward, no penalty
    }
    // Post-encounter status readout snapshot, part 2: the ENDING values, now that
    // the switch above has applied whatever this outcome caused.
    fight_.result.bwAfter = bandwidth_;
    fight_.result.fragAfter = pet_.model.fragmentation();
    for (int i = 0; i < kLevelStatCount; ++i)
        fight_.result.statGain[i] = pet_.statPoints[i] - statBefore[i];
    fight_.result.levelAfter = pet_.level;
    markSaveDirty();
}

void Game::finishCombat() {
    fightScreen_.shutPanel();   // the next fight opens with the panel shut
    if (fight_.combat.outcome() == Combat::Outcome::Win) playSound(Sound::BattleWin);
    else if (fight_.combat.lostOrDrawn()) playSound(Sound::BattleLose);
    // A duel has its own result path (a banner on the LINK screen and a log line; a
    // duel has no stakes, so no reward and no penalty), so it hands off before
    // applyCombatResult() the same way a boss round does.
    if (pvpFighting()) { finishPvpBattle(); return; }
    // A boss/gauntlet round has its own result path (advance/clear/fail + the
    // lump) with no wild loot rolls: hand off before applyCombatResult().
    if (fight_.caller == FightCaller::Boss) { finishBossRound(); return; }
    // An arena bout advances a bracket instead of paying a fight (game_tourney.cpp):
    // Safe stakes, no loot rolls, and nothing but the title pays. Same hand-off shape.
    if (fight_.caller == FightCaller::Tourney) { finishTourneyMatch(); return; }
    applyCombatResult();
    // Return to the caller, never a fixed layer. Sim-Battle returns to the
    // LOADOUT hub; a wild encounter returns to the idle habitat (explore-mode),
    // not the EXPL list or the carousel.
    if (fight_.caller == FightCaller::Wild) {
        // Explore win-streak: a win advances the streak (10 in a row →
        // unlock the sector's boss, unless already cleared); a loss cancels the mode
        // and resets the streak. That's the whole risk model.
        if (fight_.combat.outcome() == Combat::Outcome::Win) ++away_.won;
        else if (fight_.combat.lostOrDrawn()) ++away_.lost;
        if (fight_.combat.outcome() == Combat::Outcome::Win) {
            // 'Pedia malbeast "defeated": a WON live wild fight
            // (never a boss round. Those return via finishBossRound() above before
            // ever reaching here, and Sim never reaches this branch either).
            if (const int idx = wildMalbeastIndex(fight_.combat.enemy().name);
                idx >= 0 && !(op_.reveals.malbeastDefeatedMask & (1u << idx))) {
                op_.reveals.malbeastDefeatedMask |= static_cast<uint16_t>(1u << idx);
                markSaveDirty();
            }
            if (walk_.active) {
                // In the endless DeepWeb Dive an armed Deep-Learning Module/Core advances
                // the streak (= depth) by more than one step per win, so a
                // blitzing endgame pet catches back up to a genuine struggle faster.
                // Every other area always advances by exactly 1.
                walk_.streak += inDeepWebDive() ? pet_.armed.deepWebDepthMultiplier : 1;
                // 10 wins in a row unlock the ARMED sub-area's boss, unless
                // it's already cleared (re-farming a done sub-area never re-arms it).
                if (walk_.streak >= kExploreStreakToBoss &&
                    walk_.sector >= 0 && walk_.sector < kExplSectors &&
                    walk_.sub >= 0 && walk_.sub < kExplSubAreas &&
                    !op_.world.subCleared[walk_.sector][walk_.sub])
                    op_.world.subBossUnlocked[walk_.sector][walk_.sub] = true;
                // In the endless DeepWeb Dive the streak IS the DEPTH. Two records get
                // it: the pet's own (read back by the Checkpoint Bell) and its species'
                // record, which outlives the pet and feeds the depth achievements
                // (device best, best per line, and how many species have been taken
                // deep).
                if (inDeepWebDive()) {
                    if (walk_.streak > pet_.bestDeepWebDepth) pet_.bestDeepWebDepth = walk_.streak;
                    recordSpeciesDive(pet_.creature, walk_.streak);
                }
                // The crawl keeps its own record and pays the thing it exists to pay: a
                // SIGIL every kDarkWebSigilEveryN wins. It is credited as a spent shake
                // rather than as a free letter, so the two doors onto the Cant meet in one
                // ledger and the aerial's captures are never devalued by the zone.
                if (inDarkWebCrawl()) {
                    if (walk_.streak > pet_.bestDarkWebDepth) pet_.bestDarkWebDepth = walk_.streak;
                    if (kDarkWebSigilEveryN > 0 && walk_.streak > 0 &&
                        walk_.streak % kDarkWebSigilEveryN == 0 &&
                        sigilsKnown() < kCantSigils) {
                        op_.cant.sigils = learnSigil(op_.cant.sigils);
                        log_.push(LogEventType::ItemGained, "LEARNED A SIGIL");
                    }
                }
            }
        } else if (fight_.combat.lostOrDrawn()) {
            // A loss ends the run: a draw is one. Same rule as the Stop row: only a DIVE ending spends
            // the depth multiplier, and inDeepWebDive() is derived from walk_.active,
            // so it has to be read first.
            if (inEndlessZone()) clearDiveDepthBuffs();
            walk_.end();
        }
        // Post-encounter status readout: a FOUGHT battle (win or loss, a draw
        // being a loss) parks on the BANDWIDTH/FRAG readout before the habitat. A
        // flee never reaches finishCombat() with those stakes touched, so it skips
        // straight back (returnToExplore also surfaces an awakened-guardian
        // rank-up).
        if (fight_.combat.outcome() == Combat::Outcome::Win || fight_.combat.lostOrDrawn()) {
            nav_ = Nav::PostEncounter;
            fight_.result.readoutUntilMs = nowMs_ + kPostEncounterMs;
        } else {
            returnToExplore();
        }
    } else {
        // Sim-Battle came from PRACTISE, so it returns to the LOADOUT hub with that
        // row still focused.
        trainScreen_ = TrainScreen::MovePicker;
        leaveLoadoutTab();
    }
    dirty_ = true;
    persistSave();
}

bool Game::combatCanTeach() const {
    // Only a fight against something the world owns. A duel or arena bout fights another
    // operator's pet and a Sim fights a dummy. Neither rolls a drop, so neither shows
    // anything to win.
    return !pvpFighting() &&
           (fight_.caller == FightCaller::Wild || fight_.caller == FightCaller::Boss);
}

bool Game::combatOutroEligible() const {
    // ...and WON, which is the only thing the dissolve adds to the question above: the
    // KIT panel answers it live, mid-fight, off the same gate.
    return combatCanTeach() && fight_.combat.outcome() == Combat::Outcome::Win;
}

bool Game::combatDissolveRunning() const {
    return combatOutroEligible() && fxBeat_ < kAbsorbTotalBeats;
}

uint32_t Game::rivalTeachableMoveMask() const {
    // This is the roll's own filter (moveIsTeachable), because a mark here promises that
    // winning could teach the move. Anything the filter rejects (an owned move, the
    // innate jab, another line's exclusive) stays unmarked. A rival whose whole kit is
    // rejected returns 0, and shreds, which is the truth.
    if (!combatCanTeach()) return 0;
    // The rival BY ROLE is Combat's enemy_ in every fight that reaches here: the one
    // seating that differs (a duel guest, core/ui/combat_screen.h's CombatSides) is a
    // duel, which combatCanTeach has already refused.
    uint32_t mask = 0;
    const std::vector<const MoveDef*>& kit = fight_.combat.enemy().moves;
    // 32 is far above any kit the game builds (a boss carries the depth spine, its
    // area's rider and at most kMaxBossTeaches), so the bound only guards the shift.
    for (size_t i = 0; i < kit.size() && i < 32; ++i)
        if (moveIsTeachable(kit[i])) mask |= 1u << i;
    return mask;
}

void Game::debugStartCombat(bool live, bool lethal) {
    if (!pet_.creature) return;
    Combatant p = makePlayerCombatant(registry_, *pet_.creature, pet_.moves, pet_.mods);
    CombatEnemy spec = lethal
        // An unbeatable enemy: huge Health + an overwhelming move so a loss is
        // deterministic (exercises the live-vs-safe stakes contract).
        ? CombatEnemy{"Lethal", "SPR_PET_CACHEMUTT", 3, 9999, 99, {"packet_storm"}}
        : simDummy(simTier_);
    if (lethal) {
        // Give the lethal enemy a one-shot kill regardless of move power.
        p.maxHealth = 1;
    }
    beginDebugCombat(p, spec, live);
}

void Game::debugStartCombatVs(const CombatEnemy& spec, bool live) {
    if (!pet_.creature) return;
    Combatant p = makePlayerCombatant(registry_, *pet_.creature, pet_.moves, pet_.mods);
    beginDebugCombat(p, spec, live);
}

void Game::beginDebugCombat(const Combatant& p, const CombatEnemy& spec, bool live) {
    Combatant e = makeEnemyCombatant(registry_, spec);
    rng_ = rng_ * 1664525u + 1013904223u;
    fight_ = Fight::started(FightCaller::Sim);   // the dev hook always returns to the hub
    fight_.combat.begin(p, e, live ? Combat::Stakes::Live : Combat::Stakes::Safe, rng_,
                        /*forceEnemyFirst=*/false, /*carryPlayerHealth=*/-1,
                        exploitUsesPerBattle());
    fxBeat_ = 0;
    nav_ = Nav::Combat;
    dirty_ = true;
}

void Game::debugSetNetworksSeen(int n) {
    if (n < 0) n = 0;
    op_.networksSeen = n;   // unbounded (XP model): no ledger/pool to keep in sync now
    op_.hackerRank = hackerRankForNetworksSeen(op_.networksSeen);   // silent: no celebration
}

void Game::debugUseItem(const char* id) {
    const ItemDef* d = registry_.item(id);
    if (!d) return;
    detailItem_ = d;
    lockoutItemsContext_ = false;   // normal (non-Lockout) use context
    useItem();                      // real path: gate -> effect -> consume -> log
}

void Game::debugOpenCache(const char* id) {
    // Sealed caches decrypt from the Hacker VAULT: this seam drives the same real
    // reward path (openSealedCache) onHackerVault uses.
    const ItemDef* d = registry_.item(id);
    if (d) openSealedCache(*d);
}

void Game::debugSetBits(int n) { op_.bits = n < 0 ? 0 : n; }

void Game::debugFillLoadout() {
    if (!pet_.creature) return;
    // Moves: the first owned move that matches the slot's stamped kind and isn't
    // already sitting in an earlier slot (equipping a move MOVES it, so reusing one
    // would empty the slot behind us and leave the page shorter than it should be).
    const int slots = MoveLoadout::slotsForStage(pet_.creature->stage);
    for (int s = 0; s < slots; ++s)
        for (const char* id : pet_.moves.owned()) {
            const MoveDef* m = registry_.move(id);
            if (!m || m->kind != slotRequiredKind(s)) continue;
            if (pet_.moves.slotOf(id) >= 0) continue;
            pet_.moves.equip(s, m->id);
            break;
        }
    // Mods: any three distinct rows, placed directly (setEquipped, no spare consumed).
    int filled = 0;
    for (const ModDef* d : registry_.allMods()) {
        if (filled >= kModSlots) break;
        if (pet_.mods.slotOf(d->id) >= 0) continue;
        pet_.mods.setEquipped(filled++, d->id);
    }
    markSaveDirty();
}


// The wild a walk meets here, at full strength for where it was met: the zone's own pair
// and its scaling. The Wi-Fi guardian (game_net.cpp) rolls through this too, so it is the
// same creature a walk in the same place would meet.
CombatEnemy Game::rollWild() {
    CombatEnemy wild;
    rng_ = rng_ * 1664525u + 1013904223u;                    // roster variant roll
    if (inEndlessZone()) {
        // Both endless zones field a tier-3 pair (the crawl its own, kDarkWebWilds) and
        // scale it to the PET's level, ramped by the run's current win-streak
        // (walk_.streak) so pushing deeper gradually punches the pet up instead of
        // sitting at flat parity forever. The dive's curve has a foothold and spends a
        // fraction of a level's stat points; the crawl's has neither.
        wild = inDarkWebCrawl() ? wildMalbeastOf(kDarkWebWilds, 3, rng_ >> 16)
                                : wildMalbeast(3, rng_ >> 16);
        rng_ = rng_ * 1664525u + 1013904223u;                // the zone's stat/kit roll
        if (inDarkWebCrawl())
            applyDarkWebScale(wild, pet_.level, walk_.streak, rng_);
        else
            applyDeepWebScale(wild, pet_.level, walk_.streak, rng_);
    } else {
        wild = wildMalbeastIn(walk_.sector, rng_ >> 16);     // the area's pair
        applyWildSubAreaRamp(wild, walk_.sector, walk_.sub);  // depth ramp
    }
    // 'Pedia malbeast "seen": the wild roll itself is the
    // glimpse, regardless of what happens next (fight/flee/sinkhole/auto-resolve).
    // wildMalbeastIndex returns -1 for anything outside the wild roster.
    if (const int idx = wildMalbeastIndex(wild.name);
        idx >= 0 && !(op_.reveals.malbeastSeenMask & (1u << idx))) {
        op_.reveals.malbeastSeenMask |= static_cast<uint16_t>(1u << idx);
        markSaveDirty();
    }
    return wild;
}

void Game::startEncounter() {
    // THE FIRST ENCOUNTER IN A ZONE IS WHERE ITS CHAPTER FIRES (game_story.cpp). Before
    // the roll, because the chapter is about arriving and has to come before the first
    // malbeast. fireStory answers false for a zone with nothing authored or a chapter
    // already read, in which case this is an ordinary encounter and nothing here knows a
    // story exists; when it answers true the reader has the screen and calls back into
    // this function once it is done, by which time the chapter is marked read and the
    // gate falls straight through.
    if (fireStory(walk_.sector, StoryBeat::AreaIntro, StoryThen::Encounter)) return;
    const CombatEnemy wild = rollWild();
    // Auto-explore is a hands-off background mode: a wild encounter resolves
    // itself. There's no human at the intro to press Fight, and Flee is pointless
    // since a loss already cancels the mode + zeroes the streak (finishCombat). A
    // held Sinkhole Trap still auto-bypasses for XP (its whole purpose);
    // otherwise the fight auto-starts. The Fight/Flee/Sinkhole intro stands only for a
    // (currently unused) manual encounter path.
    if (walk_.active) {
        if (const ItemDef* bypass = heldEncounterBypass()) resolveSinkhole(*bypass);
        else fightWild(wild);
        return;
    }
    encounter_.open(wild);
    nav_ = Nav::Encounter;
}

void Game::fightWild(const CombatEnemy& wild) {
    startWildCombat(wild, /*forceEnemyFirst=*/false);
}

void Game::resolveFlee(const CombatEnemy& wild) {
    rng_ = rng_ * 1664525u + 1013904223u;
    const bool escaped = static_cast<int>((rng_ >> 16) % 100) < kFleeChancePct;
    if (escaped) {
        std::snprintf(walk_.flavor, sizeof(walk_.flavor), "GOT AWAY");
        playSound(Sound::Fled);
        returnToExplore();
    } else {
        // A failed flee forces the fight, enemy first (retreat isn't free).
        startWildCombat(wild, /*forceEnemyFirst=*/true);
    }
}

const ItemDef* Game::heldEncounterBypass() const {
    for (const ItemDef* d : registry_.allItems())
        if (d->preEncounterXp > 0 && inventory_.count(d->id) > 0) return d;
    return nullptr;
}

void Game::resolveSinkhole(const ItemDef& d) {
    const int xp = d.preEncounterXp;
    inventory_.remove(d.id, 1);
    addCombatXp(xp);
    char used[36];
    std::snprintf(used, sizeof(used), "USED %s", d.displayName);
    log_.push(LogEventType::ItemUsed, used);
    std::snprintf(walk_.flavor, sizeof(walk_.flavor), "%s +%d XP", d.displayName, xp);
    // Persist now: the debounced autosave can lose a level-up to a reboot. Every other XP
    // grant (finishCombat, finishBossRound) does the same, and this source skips combat,
    // so it needs its own call.
    persistSave();
    returnToExplore();
}

void Game::startWildCombat(const CombatEnemy& wild, bool forceEnemyFirst) {
    if (!pet_.creature) return;
    Combatant p = buildPlayerCombatant();
    Combatant e = makeEnemyCombatant(registry_, wild);
    rng_ = rng_ * 1664525u + 1013904223u;
    fight_ = Fight::started(FightCaller::Wild);
    fight_.combat.begin(p, e, Combat::Stakes::Live, rng_, forceEnemyFirst,
                        /*carryPlayerHealth=*/-1, exploitUsesPerBattle());
    fxBeat_ = 0;
    nav_ = Nav::Combat;
    dirty_ = true;
    playSound(Sound::CombatStart);
}

}  // namespace mal
