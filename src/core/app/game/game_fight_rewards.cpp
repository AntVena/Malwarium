// What a fight pays and costs: the XP curve and its grants, the battle-fatigue tax, and
// the settle that turns a wild or Sim outcome into Bits, XP, drops or Fragmentation.
// Boss rounds settle in game_bosses.cpp and share applyLossFrag.
#include "core/app/game.h"

#include <cstring>

#include "tunables.h"
#include "core/content/areas/area_defs.h"   // kAreaCount, kSubAreasPerArea
#include "core/model/lcg.h"

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
        lcgAdvance(rng_);      // deterministic stat roll
        const int stat = static_cast<int>((rng_ >> 16) % kLevelStatCount);
        ++pet_.statPoints[stat];
        pet_.lastLevelUpStat = stat;
    }
    if (pet_.level > levelBefore) playSound(Sound::LevelUp);
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
    lcgAdvance(rng_);
    // c: the Reduce-Frag-TRIGGER rig upgrade lowers the trigger CHANCE by the
    // purchased tier (tier 0 = the unchanged baseline; tiers 1..6 use the reduced table).
    const int trigTier = fragTriggerTier();
    const int chance = (trigTier == 0)
                           ? kBattleFatigueChancePct
                           : kFragTriggerReducedPct[trigTier - 1];
    if (static_cast<int>((rng_ >> 16) % 100) >= chance) return;
    lcgAdvance(rng_);
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
    const bool fought = fight_.combat.outcome() == Combat::Outcome::Win ||
                        fight_.combat.lostOrDrawn();   // a draw (the turn limit) is a loss
    // The STARTING values, before anything below moves them: finishCombat reads the
    // before/after pair to drive Nav::PostEncounter for a wild fight, and the stat
    // diff names what this fight's XP levelled up.
    fight_.result.bwBefore = bandwidth_.level();
    fight_.result.fragBefore = pet_.model.fragmentation();
    int statBefore[kLevelStatCount];
    for (int i = 0; i < kLevelStatCount; ++i) statBefore[i] = pet_.statPoints[i];

    // Bandwidth is a per-fight FRAGMENTATION SHIELD: any resolved live fight (a flee is
    // not one, and Sim is exempt) spends a charge to skip the battle-fatigue tax, so
    // Bits sunk into the SHOP's Bandwidth upgrade buy a longer farm. A dry pool is the
    // "defrag or come home" signal. Bosses resolve in finishBossRound and never get here.
    const bool shielded = !safe && fought && bandwidth_.spendOne();
    fight_.result.shielded = shielded;
    if (!safe && fought && !shielded) applyBattleFatigue();

    switch (fight_.combat.outcome()) {
        case Combat::Outcome::Win:
            if (safe) paySimWin();
            else payWildWin(shielded);
            break;
        case Combat::Outcome::Lose:
        case Combat::Outcome::Draw:
            if (!safe) chargeWildLoss();
            break;
        case Combat::Outcome::Fled:
        case Combat::Outcome::Ongoing:
            break;                                 // flee: no reward, no penalty
    }

    fight_.result.bwAfter = bandwidth_.level();
    fight_.result.fragAfter = pet_.model.fragmentation();
    for (int i = 0; i < kLevelStatCount; ++i)
        fight_.result.statGain[i] = pet_.statPoints[i] - statBefore[i];
    fight_.result.levelAfter = pet_.level;
    markSaveDirty();
}

// Sim-Battle is a drill: a small flat payout, and nothing logged.
void Game::paySimWin() {
    int bits = kSimBitsReward;
    bits += sumEquippedModMagnitude(registry_, pet_.mods, ModEffect::PostBattleBits,
                                    pet_.creature ? pet_.creature->line : nullptr);
    op_.bits += applyCombatBitsBonus(bits);   // Scraping Cluster Expansion
    pet_.model.feed(0, kSimHappyReward);
    addFightXp(applyCombatXpBonus(kSimXpReward));  // Well-Fed XP Boost
}

void Game::payWildWin(bool shielded) {
    // randInt(R, R²) keyed to the opponent's stage-rank R. The malbeast's difficulty
    // pips (1/2/3) stand in for its stage, so R = diffPips+1. diffPips does not move
    // with depth, so the dive and the crawl scale the roll by their own depth curves.
    int bits = normalBitsReward(fight_.combat.enemy().diffPips + 1, rng_);
    if (inDeepWebDive())
        bits = bits * deepWebDepthBitsPct(walk_.streak) / 100
                    * (100 + diveRewardBonusPct()) / 100;   // DIVE PAY
    else if (inDarkWebCrawl())
        bits = bits * darkWebDepthBitsPct(walk_.streak) / 100;
    bits += sumEquippedModMagnitude(registry_, pet_.mods, ModEffect::PostBattleBits,
                                    pet_.creature ? pet_.creature->line : nullptr);
    op_.bits += applyCombatBitsBonus(bits);   // Scraping Cluster Expansion

    // The zone's base XP scaled by the enemy's level against the pet's: punching up pays
    // more, farming shallow a trickle. The CRAWL's base is a multiple of the flat one
    // (wildWinXpBase), and DIVE PAY adds on top inside a dive, where each win has to be
    // worth the shallow ones the shorter, harder run skipped.
    const int winXp = wildWinXp(wildWinXpBase(walk_.sector),
                                fight_.combat.enemy().level, pet_.level) *
                      (100 + diveRewardBonusPct()) / 100;
    addFightXp(applyCombatXpBonus(winXp));  // Well-Fed XP Boost

    rollWildWinDrops(shielded);
    log_.push(LogEventType::CombatWon, "WON BATTLE");
}

// Re-farming a CLEARED sub-area pays full non-Bits loot while Bandwidth shielded the
// fight. Unshielded, drop chances decay per re-farm win toward a floor and the per-sub
// count advances (saved since v15) until Bandwidth regenerates. Bits and XP stay full,
// so a done area is always a viable training ground.
int Game::wildDropScalePct(bool shielded) {
    const bool refarm = walk_.sector >= 0 && walk_.sector < kAreaCount &&
                        walk_.sub >= 0 && walk_.sub < kSubAreasPerArea &&
                        op_.world.subCleared[walk_.sector][walk_.sub];
    if (!refarm || shielded) return 100;
    uint16_t& c = pet_.armed.subRefarmCount[walk_.sector][walk_.sub];
    const int pct = refarmDropScalePct(c);
    if (c < kRefarmCountCap) ++c;
    return pct;
}

void Game::rollWildWinDrops(bool shielded) {
    const int dropScalePct = wildDropScalePct(shielded);
    // What a wild win drops is the AREA's business: each area row carries its own wild
    // drop table, including whichever Merge Hub ingredient it makes farmable there.
    if (rollD100() < kWildItemDropPct * dropScalePct / 100) {
        const AreaLootTable t = areaWildLootTable(walk_.sector);
        drawCacheItem(t.rows, t.count);
    }
    rollEnemyMoveDrop(fight_.combat.enemy(), kWildMoveDropPct * dropScalePct / 100);
    // The endless zones are the tier-4 mod source. The equip-LEVEL gate still applies,
    // so a lucky drop is held until the pet is deep enough to field it.
    if (walk_.sector == kDeepWebSector || walk_.sector == kDarkWebSector) {
        if (rollD100() < kModDeepWebDropPct)
            if (const char* id = rollAreaModId(walk_.sector))
                grantMod(id);
    }
}

// A live loss corrupts (scaled by the Bad-branch multiplier). It is not a care mistake.
// A lost gauntlet (finishBossRound) pays the same.
void Game::applyLossFrag() {
    const int frag = kWildLossFrag * fight_.combat.player().fragMultPct / 100;
    pet_.model.setFragmentation(pet_.model.fragmentation() + frag);
}

void Game::chargeWildLoss() {
    applyLossFrag();
    log_.push(LogEventType::CombatLost, "LOST BATTLE");
}

}  // namespace mal
