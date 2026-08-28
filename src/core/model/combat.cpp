#include "core/model/combat.h"

#include <cstdio>
#include <cstring>
#include <utility>

#include "tunables.h"

namespace mal {

uint32_t Combat::rng() {
    rng_ = rng_ * 1664525u + 1013904223u;   // LCG (deterministic, matches MAINT roll)
    return rng_ >> 16;
}

void Combat::begin(const Combatant& player, const Combatant& enemy, Stakes stakes,
                   uint32_t seed, bool forceEnemyFirst, int carryPlayerHealth,
                   int exploitUses) {
    player_ = player;
    enemy_ = enemy;
    // Full Health unless a gauntlet carries a wounded one in — no heal between rounds.
    if (carryPlayerHealth >= 0) {
        player_.health = carryPlayerHealth < player_.maxHealth ? carryPlayerHealth
                                                               : player_.maxHealth;
        if (player_.health < 1) player_.health = 1;
    } else {
        player_.health = player_.maxHealth;
    }
    enemy_.health = enemy_.maxHealth;
    // The frenzy ratchet is "the most bubble this pet has held", so a carried-in shield
    // arms the lean on the same terms a cast would.
    if (player_.phishShieldPeak < player_.shieldHp)
        player_.phishShieldPeak = player_.shieldHp;
    if (enemy_.phishShieldPeak < enemy_.shieldHp)
        enemy_.phishShieldPeak = enemy_.shieldHp;
    stakes_ = stakes;
    outcome_ = Outcome::Ongoing;
    rng_ = seed ? seed : 1u;
    // Zero-Day Exploit (mod): rolled here rather than in makePlayerCombatant because it
    // needs the fight's seeded RNG. Both sides, player first — in PVE the enemy's
    // magnitude is 0 and its branch draws nothing, but a linked duel
    // (core/model/pvp_battle.h) has a real pet in the enemy_ slot.
    auto rollGamble = [this](Combatant& c) {
        const int pct = c.mods.mag(ModEffect::GambleBattlePowerPct);
        if (pct > 0 && static_cast<int>(rng() % 100) < pct)
            c.powerMultPct += c.mods.mag2(ModEffect::GambleBattlePowerPct);
    };
    rollGamble(player_);
    rollGamble(enemy_);
    // Ransom Note (Ransomware): the window is armed from the opening bell, not from the
    // ransomer's first turn. `||` so a caller-supplied armed window is never rolled away.
    player_.ransomArmed = player_.ransomArmed || ransomArmRolls(player_);
    enemy_.ransomArmed = enemy_.ransomArmed || ransomArmRolls(enemy_);
    // The three stat baselines the combat screen shows deltas against, and that Net
    // Neutrality resets to. Captured after the one-time gamble and after syncWormSpeed()
    // so only genuine mid-fight movement reads as a delta.
    player_.basePowerMultPct = player_.powerMultPct;
    enemy_.basePowerMultPct = enemy_.powerMultPct;
    syncWormSpeed();
    player_.baseSpeed = player_.speed;
    enemy_.baseSpeed = enemy_.speed;
    player_.baseDmgReducePct = player_.dmgReducePct;
    enemy_.baseDmgReducePct = enemy_.dmgReducePct;
    // Empty gauges, then pick the opening actor by speed. forceEnemyFirst (a failed
    // pre-fight flee) overrides that once; speed scheduling resumes after.
    plGauge_ = 0;
    enGauge_ = 0;
    streakCount_ = 0;
    streakIsPlayer_ = true;
    playerTurn_ = forceEnemyFirst ? false : pickNextActor();
    playerFirst_ = playerTurn_;
    overrideUsesLeft_ = exploitUses < 0 ? 0 : exploitUses;
    overrideUsesTotal_ = overrideUsesLeft_;
    overrideOpen_ = false;
    overridePick_ = 0;
    forcedMoveIdx_ = -1;
    overrideItems_.clear();
    committedItemId_ = nullptr;
    lastMoveName_ = "";
    lastDamage_ = 0;
    lastByPlayer_ = false;
    lastWasCharge_ = false;
    lastRansomed_ = false;
    lastWasStrike_ = false;
    strikeCount_ = 0;
    lastWormKill_ = {};
}

void Combat::setLast(const char* name, int dmg, bool byPlayer, bool charge,
                     bool ransomed, bool strike) {
    lastMoveName_ = name;
    lastDamage_ = dmg;
    lastByPlayer_ = byPlayer;
    lastWasCharge_ = charge;
    lastRansomed_ = ransomed;
    lastWasStrike_ = strike;
    // Every resolved turn funnels through here, so the strike count lives here rather
    // than at each call site that can swing.
    if (strike) strikeCount_++;
}

// Uniform pick over `self`'s slots, skipping lastMoveIdx (no-consecutive-repeat) unless
// `allowRepeat`, and every Defend slot when `attacksOnly`. Draws exactly one rng() when
// it has a choice. Returns -1 when nothing qualifies — keep what you had.
int Combat::pickSlot(const Combatant& self, bool attacksOnly, bool allowRepeat) {
    const int n = static_cast<int>(self.moves.size());
    const bool excludeLast =
        !allowRepeat && self.lastMoveIdx >= 0 && self.lastMoveIdx < n;
    auto eligible = [&](int i) {
        if (excludeLast && i == self.lastMoveIdx) return false;
        return !attacksOnly || self.moves[i]->kind == MoveDef::Kind::Attack;
    };
    int span = 0;
    for (int i = 0; i < n; ++i)
        if (eligible(i)) ++span;
    if (span <= 0) return -1;
    const int target = static_cast<int>(rng() % static_cast<uint32_t>(span));
    int seen = 0;
    for (int i = 0; i < n; ++i) {
        if (!eligible(i)) continue;
        if (seen == target) return i;
        ++seen;
    }
    return -1;
}

// The frenzy lean (Phishing) — contract in combat.h. 0 unless a shield has been pooled
// past max Health, which is also the guard keeping this path from drawing rng() in a
// fight without the track.
int phishFrenzyLeanPct(const Combatant& c) {
    if (c.phishShieldPeak <= c.maxHealth || c.maxHealth <= 0) return 0;
    const int span = c.maxHealth * (kPhishFrenzyLeanFullMult - 1);
    if (span <= 0) return kPhishFrenzyLeanMaxPct;
    const int pct = (c.phishShieldPeak - c.maxHealth) * 100 / span;
    return pct > kPhishFrenzyLeanMaxPct ? kPhishFrenzyLeanMaxPct : pct;
}

void Combat::releaseRansomSeizure(Combatant& c) {
    RansomSeizure& seize = c.ransomSeizure;
    if (!seize.holding()) return;
    const int slot = seize.slot;
    if (slot >= 0 && slot < static_cast<int>(c.moves.size())) {
        c.moves[slot] = seize.heldMove;
        if (slot < static_cast<int>(c.chainFollow.size()))
            c.chainFollow[slot] = seize.heldFollow;
    }
    seize = RansomSeizure{};
}

bool braceOnlyDefend(const MoveDef& m) {
    // A WILDCARD declares none of the fields below and still isn't a pure brace — what it
    // rolls may be a pool, a trap or a spawn.
    if (moveIsWildcard(m)) return false;
    return m.kind == MoveDef::Kind::Defend && m.stackDefensePct == 0 && m.shieldPool == 0 &&
           m.trapArm == 0 && m.replicaSpawnPct == 0;
}

int ledgerGrudgePct(const Combatant& c) {
    if (c.ransomPool <= 0) return 0;
    const int base = kMaxHealthByStage[stageIndex(c.stage)];
    if (base <= 0) return 0;
    const int pct = c.ransomPool * kLedgerGrudgeFullPct / base;
    return pct > kLedgerGrudgeMaxPct ? kLedgerGrudgeMaxPct : pct;
}

int phishPoolSiphonBonusPct(const Combatant& c) {
    if (c.shieldHp <= 0) return 0;
    // Against the STAGE body, not this pet's levelled maxHealth: earned max-Health points
    // would inflate the denominator while the pool only tracks Defence, so a Health-steered
    // pet would get worse at its own line's mechanic.
    const int base = kMaxHealthByStage[stageIndex(c.stage)];
    if (base <= 0) return 0;
    // Phishing Rod (mod) scales the siphon itself, not its ceiling — the cap is almost
    // never reached, so scaling it would pay nothing.
    int pct = c.shieldHp * kPhishPoolSiphonFullPct / base;
    pct = pct * (100 + c.mods.mag(ModEffect::StealAmplifyPct)) / 100;
    return pct > kPhishPoolSiphonMaxPct ? kPhishPoolSiphonMaxPct : pct;
}

// --- Polymorph, the pure half ---------------------------------------------------------

bool polymorphHasAbsorbed(const Combatant& c, const MoveDef* m) {
    if (!m) return false;
    for (int i = 0; i < c.absorbedCount; ++i)
        if (c.absorbed[i] == m) return true;
    return false;
}

bool polymorphAbsorb(Combatant& c, const MoveDef* m) {
    if (!c.polymorphic || !m) return false;
    if (c.absorbedCount >= kPolymorphAbsorbCap) return false;   // technical bound only
    if (polymorphHasAbsorbed(c, m)) return false;               // a repeat pays nothing
    c.absorbed[c.absorbedCount++] = m;
    polymorphPay(c, m->kind, 1);
    return true;
}

void polymorphPay(Combatant& c, MoveKind kind, int points) {
    if (points <= 0) return;
    // One stat point's worth each, in applyLevelStatPoints' vocabulary. The KIND picks
    // which pair is paid, so a varied kit shapes what the pet becomes.
    if (kind == MoveKind::Attack) {
        c.powerMultPct += kLevelPowerPctPerPoint * points;
        c.speed += static_cast<float>(kLevelSpeedPerPoint * points);
    } else {
        c.dmgReducePct += kLevelDefensePctPerPoint * points;
        int gain = kLevelHealthPerPoint * points;
        // A payment onto a full wall would otherwise be worth nothing; what the clamp
        // refuses becomes Health instead (capOverflowHealth, combat.h), as at level-up.
        if (c.dmgReducePct > kLevelDmgReduceMaxPct) {
            gain += capOverflowHealth(c.dmgReducePct - kLevelDmgReduceMaxPct,
                                      kLevelDefensePctPerPoint);
            c.dmgReducePct = kLevelDmgReduceMaxPct;
        }
        c.defenseMultPct += kLevelDefenseBracePctPerPoint * points;
        // Ceiling and current together — raising max under a fighter must hand it the room.
        c.maxHealth += gain;
        c.health += gain;
    }
}

uint32_t moveEffectMask(const MoveDef& m) {
    // One bit per rider, in MoveDef declaration order. Nothing persists this mask, so a
    // new rider just appends a bit here.
    uint32_t mask = 0;
    int bit = 0;
    auto set = [&](bool on) { if (on) mask |= 1u << bit; ++bit; };
    set(m.channelTurns > 1);
    set(m.armorPiercePct > 0);
    set(m.lockTurns > 0);
    set(m.dotDamage > 0 && m.dotTurns > 0);
    set(m.stealPowerPct > 0);
    set(m.stealDefensePct > 0);
    set(m.stealSpeedPct > 0);
    set(m.stealCurrentHpPct > 0);
    set(m.stealMaxHpPct > 0);
    set(m.shieldPool > 0);
    set(m.trapArm > 0);
    set(m.replicaSpawnPct > 0);
    set(m.stackPowerPct > 0);
    set(m.stackDefensePct > 0);
    set(m.chainNextId != nullptr);
    set(m.speedRefundPct > 0);
    set(m.poolRetaliateDot > 0);
    return mask;
}

int polymorphEffectCount(const Combatant& c) {
    int n = 0;
    for (uint32_t m = c.effectsSeen; m; m &= m - 1) ++n;   // popcount, no <bit> needed
    return n;
}

const MoveDef* wildPick(const WildPool& pool, uint32_t roll) {
    if (pool.rows.empty()) return nullptr;
    const int total = static_cast<int>(pool.rows.size());
    const int genericN = pool.genericEnd;
    const int lineAN = pool.lineAEnd - pool.genericEnd;
    const int lineBN = total - pool.lineAEnd;
    // An empty band hands its weight back to generic rather than being rolled into and
    // found empty — a pool naming an unknown line, or a line with no rows of this kind.
    int wGeneric = genericN > 0 ? kWildSourceGenericPct : 0;
    const int wA = lineAN > 0 ? kWildSourceLineAPct : 0;
    const int wB = lineBN > 0 ? kWildSourceLineBPct : 0;
    if (lineAN == 0) wGeneric += kWildSourceLineAPct;
    if (lineBN == 0) wGeneric += kWildSourceLineBPct;
    const int sum = wGeneric + wA + wB;
    if (sum <= 0) return pool.rows[roll % total];   // no weights: fall back to a flat draw
    // ONE draw does both halves — low bits pick the band, high bits the row. A second
    // draw would desync a duel's two devices, which only stay in step on equal draw counts.
    int band = static_cast<int>(roll % static_cast<uint32_t>(sum));
    const uint32_t within = roll / static_cast<uint32_t>(sum);
    if (band < wGeneric && genericN > 0) return pool.rows[within % genericN];
    band -= wGeneric;
    if (band < wA && lineAN > 0) return pool.rows[pool.genericEnd + (within % lineAN)];
    if (lineBN > 0) return pool.rows[pool.lineAEnd + (within % lineBN)];
    if (genericN > 0) return pool.rows[within % genericN];
    return pool.rows[within % total];
}

LinePassives wildBorrowedPassives(const WildPool& pool, const MoveDef* m) {
    if (!m || !m->line) return 0;
    for (int i = pool.genericEnd; i < pool.lineAEnd; ++i)
        if (pool.rows[i]->line && std::strcmp(pool.rows[i]->line, m->line) == 0)
            return pool.passivesA;
    for (size_t i = static_cast<size_t>(pool.lineAEnd); i < pool.rows.size(); ++i)
        if (pool.rows[i]->line && std::strcmp(pool.rows[i]->line, m->line) == 0)
            return pool.passivesB;
    return 0;
}

int Combat::chooseMove(Combatant& self) {
    const int n = static_cast<int>(self.moves.size());
    if (n <= 1) return 0;
    // Uniform over the moves != lastMoveIdx. The attack/defend lean then emerges from the
    // slot mix itself — more attack slots, more attack rolls — with no separate dial.
    int idx = pickSlot(self, /*attacksOnly=*/false, /*allowRepeat=*/false);
    if (idx < 0) return 0;
    // Two exceptions re-roll a Defend into an Attack. Both allow a repeat, so a kit with
    // one attack can still act on them; both leave the original standing if no attack
    // qualifies. First: a bubble stacked past max Health has bought more wall than the
    // fight can spend, at a chance ramping with the stack (phishFrenzyLeanPct).
    const int leanPct = phishFrenzyLeanPct(self);
    if (leanPct > 0 && self.moves[idx]->kind != MoveDef::Kind::Attack &&
        static_cast<int>(rng() % 100) < leanPct) {
        const int atk = pickSlot(self, /*attacksOnly=*/true, /*allowRepeat=*/true);
        if (atk >= 0) idx = atk;
    }
    // Second: a pure brace re-cast while this fighter's brace is still up buys nothing but
    // overkill. Unconditional — re-bracing is never the better turn.
    if (self.guard > 0 && braceOnlyDefend(*self.moves[idx])) {
        const int atk = pickSlot(self, /*attacksOnly=*/true, /*allowRepeat=*/true);
        if (atk >= 0) idx = atk;
    }
    return idx;
}

// Net Neutrality's floor (crew Exploit): whether `c`'s stat LEANS are locked against being
// lowered. Every site that would reduce power / defence / speed / maxHealth asks this
// first. Health is not covered — it is the resource the fight is fought over, not a lean.
static bool statsFloored(const Combatant& c) {
    return c.crewExploit.holds(CrewExploitKind::ResetStatsAndFloor);
}

// The multiplier one kind of replica takes from the other's count as it spawns. Both kinds
// bank it (wormAttackerDamage / wormDefenderHealth), so it is read once per copy.
static int wormCrossMult(int otherKindCount) {
    return otherKindCount < kWormReplicaMultFloor ? kWormReplicaMultFloor : otherKindCount;
}

// Whether a side replicates — the Worm line's passive family. Several hooks ask, from
// applyEffect down.
static bool replicates(const Combatant& c) {
    return hasLinePassive(c.linePassives, LinePassive::Replication);
}

void Combat::applyEffect(Combatant& actor, Combatant& target, const MoveDef* mv,
                         bool byPlayer, int moveIdx) {
    target.mirrorFired = false;
    // Malbeast In The Middle (crew Exploit): while it holds, every SELF-BUFF the OPPOSITE
    // side casts is copied onto the holder as it lands. `byPlayer` names who is CASTING
    // (the Trojan hijack passes the flipped flag), so `mirror` is always the watching side.
    // Read off whichever side holds it — a tournament opponent arms its own Exploits
    // (Combatant::autoExploit). Self-buffs only: copying a siphon's gain half would refund
    // the holder its own stat rather than copy someone else's advantage.
    Combatant& mirror = byPlayer ? enemy_ : player_;
    const bool mitmCopy = mirror.crewExploit.holds(CrewExploitKind::MirrorEnemyBuffs);
    // Feeding-frenzy combo: this actor's run of steal-attack casts made with the bubble up.
    // A continuing run permanently banks (run length - 1) flat damage into phishComboBonus,
    // paid below on every later steal-attack hit this fight; it never decays.
    //
    // Advancing keys on the FIELD (stealPowerPct), not the line — which is why the generic
    // boss pool leaves stealPowerPct at zero and shreds Defense instead. Breaking keys on
    // ANY attack swung while exposed, so a mixed kit cannot swing generics through the
    // exposed stretch with its run intact. A Defend cast leaves the run standing.
    if (mv->kind == MoveDef::Kind::Attack) {
        if (actor.shieldHp <= 0) {
            actor.phishStreak = 0;    // caught out with the bubble down
        } else if (mv->stealPowerPct > 0) {
            actor.phishStreak++;
            if (actor.phishStreak > 1) actor.phishComboBonus += actor.phishStreak - 1;
        }
    }
    // A SEIZED move swung by its captor hits for the wall behind it (kRansomSeizedWallPct).
    // Asked before scaling so the bonus rides the same multipliers. The seized move IS
    // that slot while the ransom runs, so `moveIdx` alone identifies it.
    const bool swingingSeized = mv->kind == MoveDef::Kind::Attack &&
                                actor.ransomSeizure.holding() && moveIdx >= 0 &&
                                moveIdx == actor.ransomSeizure.slot;
    if (mv->kind == MoveDef::Kind::Attack) {
        // Base damage scaled by the actor's branch attack-power lean plus any Lockout-track
        // Power stacked this fight. Meltdown Core (mod) adds a comeback bonus while low.
        int mult = actor.powerMultPct + actor.stackPowerBonus;
        // Extortion Ledger (mod), the POWER half — keyed on an unpaid ransom pool rather
        // than on a seizure, which most fights never reach. Scaled by what is owed, so the
        // pool is worth carrying rather than merely worth opening.
        if (actor.ransomPool > 0) {
            const int owed = actor.mods.mag2(ModEffect::ExtortionLedger);
            mult += owed * (100 + ledgerGrudgePct(actor)) / 100;
        }
        const int meltdownPct = actor.mods.mag(ModEffect::LowHealthPowerPct);
        if (meltdownPct > 0 && actor.maxHealth > 0) {
            const int healthPct = actor.health * 100 / actor.maxHealth;
            if (healthPct <= meltdownPct)
                mult += actor.mods.mag2(ModEffect::LowHealthPowerPct);
        }
        int dmg = mv->power * mult / 100;
        // Steal-attacks are deliberately low-power and lean on the min-1 penetration floor,
        // so the banked flat bonus is what makes a sustained frenzy dangerous.
        if (mv->stealPowerPct > 0) dmg += actor.phishComboBonus;
        // The wall, spent. Ransomware's one currency it could never cash in.
        if (swingingSeized && actor.stackDefenseBonus > 0)
            dmg = dmg * (100 + actor.stackDefenseBonus * kRansomSeizedWallPct / 100) / 100;
        // Worm attacker replicas pile onto the parent's swing before mitigation, so it goes
        // through the target's defence like any other damage. The parent's own attacks are
        // weak, so the line's threat scales with the board rather than the move rolled.
        dmg += wormReplicaDamage(actor);
        // Wild-encounter challenge buff. enemyDamageMultPct is 100 for the player, bosses
        // and Sim dummies, so this is a no-op off the wild path.
        if (!byPlayer) dmg = dmg * actor.enemyDamageMultPct / 100;
        // Worm replication (target side): the hit picks a victim among the parent and every
        // live replica, weighted so a defender draws hardest (wormTargetPick). A replica
        // eats it WHOLE — no mitigation, no riders, no overflow — and dies if overrun.
        // Replication makes the worm harder to be the one hit, not tougher.
        //
        // One rng() draw, only when replicas are out, so no other line perturbs the stream.
        if (target.wormReplicaCount > 0) {
            const int victim = wormTargetPick(target, rng());
            if (victim >= 0) {
                WormReplica& r = target.wormReplicas[victim];
                const int dealt = dmg < r.health ? dmg : r.health;
                r.health -= dealt;
                if (r.health <= 0) {   // packed: the last replica fills the freed slot
                    // Recorded before the pack erases it — the copy is about to stop
                    // existing, and the screen needs to know it ever did (WormKill).
                    lastWormKill_ = {/*happened=*/true, /*onPlayer=*/!byPlayer,
                                     r.defender};
                    target.wormReplicas[victim] =
                        target.wormReplicas[target.wormReplicaCount - 1];
                    target.wormReplicas[--target.wormReplicaCount] = WormReplica{};
                }
                setLast(mv->displayName, dealt, byPlayer, /*charge=*/false,
                        /*ransomed=*/false, /*strike=*/true);
                return;
            }
        }
        const int baseDmg = dmg;    // pre-mitigation, for the min-1 penetration floor
        const bool crewNegates =
            target.crewExploit.armed(CrewExploitKind::NegateNextHits);
        const bool mirrorArmed = target.mods.armed(ModEffect::RaidMirror);
        if (dmg > 0 && (crewNegates || mirrorArmed)) {
            // A crew Exploit charge or RAID Mirror negates the whole hit, whatever its size.
            dmg = 0;
            if (crewNegates) {
                // Crew charges absorb first — the player spent an Exploit use to arm them,
                // so the passive one-shot stays held for after they run out.
                --target.crewExploit.charges;
            } else {
                target.mods.spend(ModEffect::RaidMirror);
            }
            target.mirrorFired = true;
        } else {
            // Effective cut = passive Defense + stacked Cipher-track Defense, under the
            // never-immune clamp. Pierce (the move's own, then the mod's) is applied
            // multiplicatively rather than summed, so however many stack the defender keeps
            // a defence — two 50% pierces are 75%, never 100. Defence tier 1 cuts each
            // pierce back before it lands, so the cut already earned stops being routed
            // around (levelDefensePierceResistPct).
            const auto pierced = [&](int value, int piercePct) {
                if (piercePct <= 0 || value <= 0) return value;
                const int p = piercePct * (100 - target.pierceResistPct) / 100;
                return p > 0 ? value * (100 - p) / 100 : value;
            };
            const int modPierce = actor.mods.mag(ModEffect::ArmorPiercePct);
            int reduce = target.dmgReducePct + target.stackDefenseBonus;
            if (reduce > kLevelDmgReduceMaxPct) reduce = kLevelDmgReduceMaxPct;
            reduce = pierced(reduce, mv->armorPiercePct);
            reduce = pierced(reduce, modPierce);
            if (reduce > 0) dmg = dmg * (100 - reduce) / 100;
            // Canary Trap (mod): an extra cut on the first hit, outside the 85% clamp and
            // never pierced. Consumed only when a hit actually lands (dmg > 0).
            if (ModState* canary = target.mods.find(ModEffect::FirstHitCutPct);
                dmg > 0 && canary && canary->mag > 0 && canary->pending > 0) {
                dmg = dmg * (100 - canary->mag) / 100;
                if (dmg < 0) dmg = 0;
                --canary->pending;
            }
            if (target.guard > 0) {                   // a defend brace (one-shot)
                // Pierced by the same pair in the same order: a row that ignores a wall
                // ignores a brace too (defs.h).
                int brace = pierced(target.guard, mv->armorPiercePct);
                brace = pierced(brace, modPierce);
                const int unspent = brace > dmg ? brace - dmg : 0;
                dmg = dmg > brace ? dmg - brace : 0;
                // Defence tier 2: an over-sized brace's remainder carries instead of being
                // binned (levelDefenseBraceRetainPct) — the wall buys efficiency, not a
                // bigger number, since the % cut's ceiling rules that out. Measured against
                // the pre-pierce remainder, so pierce-resist and retention pay once, not twice.
                target.guard = unspent * target.braceRetainPct / 100;
            }
            // Minimum penetration: an attack always lands at least 1 through pure
            // mitigation, so no pet becomes a wall a weak attacker can never chip. Scoped
            // to this branch so RAID Mirror's deliberate negation still zeroes a hit; the
            // shield pool below still absorbs this 1, being a consumable pool not a wall.
            if (baseDmg > 0 && dmg < 1) dmg = 1;
        }
        if (dmg < 0) dmg = 0;
        // Prowlware (mod): the first landed damaging hit is multiplied by the move's
        // attackPowerRank. Consumed after mitigation, so a mirrored hit doesn't burn it.
        if (dmg > 0 && actor.mods.spend(ModEffect::FirstStrikeRankMult)) {
            const int rank = attackPowerRank(actor.moves, moveIdx);
            if (rank > 1) dmg *= rank;
        }
        // ECC Memory (mod): a last-resort ceiling on any single hit, after all mitigation.
        // Thorns/Deadman below read the capped value.
        const int hitCap = target.mods.mag(ModEffect::MaxHitCapPct);
        if (hitCap > 0 && dmg > hitCap) dmg = hitCap;
        // Load Balancer (mod): a hit at or over the threshold is split — splitPct% deferred
        // to the victim's next turn-start (resolveTurn), the rest lands now. It spreads
        // damage rather than reducing it, buying a turn to heal or land a KO. After the ECC
        // cap so the two compose. Thorns/Deadman read only the immediate portion.
        if (ModState* lb = target.mods.find(ModEffect::LoadBalance);
            lb && lb->mag > 0 && dmg >= lb->mag && lb->mag2 > 0) {
            const int deferred = dmg * lb->mag2 / 100;
            if (deferred > 0) {
                lb->pending += deferred;       // comes due at the victim's next turn-start
                dmg -= deferred;
            }
        }
        // Obfuscation shield pool (Phishing): a second health bar, last in the mitigation
        // chain. Only the overflow reaches Health, and a hit fully soaked triggers no
        // on-hit rider below. Popping it (not merely chewing it down) releases the frenzy
        // ratchet, so the way out of a frenzy is "break the bubble", not "wait".
        if (dmg > 0 && target.shieldHp > 0) {
            // Poisoned data (MoveDef::poolRetaliateDot): planted on the attacker before the
            // pool is chewed, so a hit that pops the bubble still poisons. Refreshes rather
            // than stacks, like every other DoT.
            if (target.poolDotDamage > 0 && target.poolDotTurns > 0) {
                const int cut = actor.mods.mag(ModEffect::FaradayCut);
                const int per = target.poolDotDamage * (100 - cut) / 100;
                if (per > 0) { actor.dotPerTurn = per; actor.dotTurnsLeft = target.poolDotTurns; }
            }
            if (target.shieldHp >= dmg) { target.shieldHp -= dmg; dmg = 0; }
            else { dmg -= target.shieldHp; target.shieldHp = 0; }
            if (target.shieldHp == 0) target.phishShieldPeak = 0;
        }
        // Trojan trap: an incoming attack springs the top armed trap — delete
        // trapEvasionPct% of the hit, reflect trapReboundPct% of everything avoided through
        // the attacker's CURRENT (rotting) defense, then strip trapArmorRot flat % Defense
        // for the rest of the fight. So rebound grows as the armor rots. Uses no rng().
        if (baseDmg > 0 && target.trojanTrapCount > 0) {
            const MoveDef* trap = target.trojanTraps[--target.trojanTrapCount];
            target.trojanTraps[target.trojanTrapCount] = nullptr;
            if (trap->trapEvasionPct > 0) dmg = dmg * (100 - trap->trapEvasionPct) / 100;
            if (dmg < 0) dmg = 0;
            const int mitigated = baseDmg - dmg;          // total the Trojan avoided
            if (trap->trapReboundPct > 0 && mitigated > 0) {
                int rebound = mitigated * trap->trapReboundPct / 100;
                int r = actor.dmgReducePct;               // through the attacker's defense
                if (r > kLevelDmgReduceMaxPct) r = kLevelDmgReduceMaxPct;
                if (r > 0) rebound = rebound * (100 - r) / 100;
                if (rebound > 0) {
                    actor.health -= rebound;
                }
            }
            if (trap->trapArmorRot > 0 && !statsFloored(actor)) {   // rot armor for next time
                actor.dmgReducePct -= trap->trapArmorRot;
                if (actor.dmgReducePct < 0) actor.dmgReducePct = 0;
            }
        }
        // Ransom Note (Ransomware passive): with the window armed, the damage is banked
        // into ransomPool instead of taken, and the countdown resets to kRansomHoldTurns.
        // Last in the chain, so the pool holds exactly what would have reached Health.
        // Only the NUMBER is deferred — `dmg` below still describes a landed hit, so every
        // rider fires on impact and a KO check sees Health that hasn't moved.
        int ransomed = 0;
        if (dmg > 0 && target.ransomArmed) {
            ransomed = dmg;
            target.ransomPool += dmg;
            target.ransomTurnsLeft = kRansomHoldTurns;
            target.ransomArmed = false;   // the window closes on the hit it catches
        }
        // The SEIZURE (RansomSeizure): a full Cipher wall with a live ransom takes the
        // attack that hit it and swings it from the brace's own slot until the ransom
        // settles. Only a landed attack, and never an unresolved WILDCARD row — the
        // ransomer has no pool to resolve one with, so it would swing an empty slot.
        if (dmg > 0 && target.ransomSeizure.armed && mv->kind == MoveDef::Kind::Attack &&
            !moveIsWildcard(*mv)) {
            RansomSeizure& seize = target.ransomSeizure;
            const int slot = seize.slot;
            if (slot >= 0 && slot < static_cast<int>(target.moves.size())) {
                seize.heldMove = target.moves[slot];
                seize.heldFollow = slot < static_cast<int>(target.chainFollow.size())
                                       ? target.chainFollow[slot]
                                       : nullptr;
                target.moves[slot] = mv;
                if (slot < static_cast<int>(target.chainFollow.size()))
                    target.chainFollow[slot] = nullptr;   // the payload, not the toolkit
                // The seizure runs the ransom clock, which is also its release: a pet that
                // keeps diverting hits keeps the move, one that stops hands it back.
                target.ransomTurnsLeft = kRansomHoldTurns;
            }
            seize.armed = false;
        }
        // Health is left UNCLAMPED here and at every site below that spends it:
        // Combat::checkOutcome owns the floor, because how far past 0 a hit buried the pet
        // is what the Backup Drive's death-save weighs before that floor erases it.
        target.health -= dmg - ransomed;
        // Honeytoken (mod): a landed hit chips the attacker back. Mods are player-side, so
        // it only ever reflects onto an enemy that hit the pet.
        const int thorns = target.mods.mag(ModEffect::Thorns);
        if (dmg > 0 && thorns > 0) {
            actor.health -= thorns;
        }
        // Tripwire (mod): Honeytoken's shape, but only while the pet is critically low.
        // Its own ModEffect kind so it never pools with an always-on Thorns alongside it.
        const int condThorns = target.mods.mag(ModEffect::ConditionalThorns);
        if (dmg > 0 && condThorns > 0 && target.maxHealth > 0) {
            const int hpPct = target.health * 100 / target.maxHealth;
            if (hpPct <= target.mods.mag2(ModEffect::ConditionalThorns)) {
                actor.health -= condThorns;
            }
        }
        if (dmg > 0) applyStealTrack(actor, target, *mv);
        // Deadman Switch (mod): a hit that KO'd the pet deals a parting blast. A mutual KO
        // resolves as a Win (enemy-death priority, checkOutcome). One shot per fight.
        if (ModState* dead = target.mods.find(ModEffect::DeathBlast);
            target.health <= 0 && dead && dead->mag > 0 && dead->pending > 0) {
            actor.health -= dead->mag;
            --dead->pending;
        }
        setLast(mv->displayName, dmg, byPlayer, false, ransomed > 0, /*strike=*/true);
        // Escalation (crew Exploit): each of the next few landed attacks banks its own
        // final damage as Power for the rest of the fight. Charge-metered, so it spends on
        // hits that connected rather than turns that happened, and uncapped unlike the
        // Lockout track below — each charge pays for the bigger swing the next one banks.
        if (dmg > 0 && actor.crewExploit.armed(CrewExploitKind::PowerByDamageDealt)) {
            actor.stackPowerBonus += dmg;
            --actor.crewExploit.charges;
        }
        // Protection Racket (crew Exploit): a landed hit hands kCrewRakePct of its final
        // damage back as Health. Turn-metered rather than charge-metered — the clock is the
        // holder's own turns (tickCrewExploitClock), so a turn that lands nothing still
        // costs one, which is what makes arming it early a decision.
        if (dmg > 0 && actor.crewExploit.ticking(CrewExploitKind::LeechOnHit)) {
            const int rake = dmg * kCrewRakePct / 100;
            actor.health += rake > 0 ? rake : 1;   // a hit that landed always pays something
            if (actor.health > actor.maxHealth) actor.health = actor.maxHealth;
        }
        // Lockout track: landing the hit stacks the caster's Power for the rest
        // of the fight — additive, never reset, capped per move.
        if (mv->stackPowerPct > 0 && actor.stackPowerBonus < mv->stackPowerCap) {
            int gain = mv->stackPowerPct;
            if (actor.stackPowerBonus + gain > mv->stackPowerCap)
                gain = mv->stackPowerCap - actor.stackPowerBonus;
            actor.stackPowerBonus += gain;
            // The cap is measured against the caster's own pile, so it bounds what there is
            // to copy rather than what the MITM holder may hold.
            if (mitmCopy) mirror.stackPowerBonus += gain;
        }
        // STUN rider: a landed hit freezes the target's next lockTurns turns. The target's
        // Watchdog Timer (mod) clamps it. Doesn't stack onto a live stun, and a fully
        // mirrored hit carries no rider. A repeat stun must beat the target's accumulated
        // lock resistance (stunLands); resistance banks the CLAMPED turns, so a Watchdog
        // pet trades some of the pile for the shorter lock.
        if (mv->lockTurns > 0 && !target.mirrorFired && target.lockedTurnsLeft == 0) {
            int k = mv->lockTurns;
            const int watchdog = target.mods.mag(ModEffect::WatchdogClamp);
            if (watchdog > 0 && k > watchdog) k = watchdog;
            if (k > 0 && stunLands(target)) {
                target.lockedTurnsLeft = k;
                target.lockResist += k;
            }
        }
        // DoT rider (Faraday-pass THREAT): a landed hit plants corruption — dotDamage/turn for
        // dotTurns of the target's upcoming turn-starts. The target's Faraday Cage (mod) cuts
        // the magnitude (100 = immune → nothing planted). Refreshes, not stacks.
        if (mv->dotDamage > 0 && mv->dotTurns > 0 && !target.mirrorFired) {
            int per = mv->dotDamage;
            const int faradayCut = target.mods.mag(ModEffect::FaradayCut);
            if (faradayCut > 0) per = per * (100 - faradayCut) / 100;
            if (per > 0) { target.dotPerTurn = per; target.dotTurnsLeft = mv->dotTurns; }
        }
    } else {
        // Defend: brace against the next hit, scaled by the caster's Defense stat
        // (defenseMultPct), symmetric to Power→attack. An Obfuscation row
        // (MoveDef::shieldPool) pools additively into shieldHp instead — a second health
        // bar that recasting stacks. Each gain is also handed to Malbeast In The Middle
        // (mitmCopy), additively on whatever the holder already had.
        const int braced = mv->power * actor.defenseMultPct / 100;
        // Asked before the brace even though the spawn happens after this resolves. A
        // Defend's replicaSpawnPct is 100 on every row that has one, so a free slot is the
        // whole of the question.
        const bool spawnsDefender = mv->replicaSpawnPct > 0 && replicates(actor) &&
                                    actor.wormReplicaCount < kWormReplicaSlots;
        if (mv->shieldPool > 0) {
            actor.shieldHp += braced;
            // Poisoned data: a pool row may arm a retaliation against whoever strikes the
            // bubble (the attack path above). Rides the pool rather than the brace, so a
            // defend-heavy pet can hold it without spending its one attack slot.
            if (mv->poolRetaliateDot > 0 && mv->poolRetaliateTurns > 0) {
                actor.poolDotDamage = mv->poolRetaliateDot;
                actor.poolDotTurns = mv->poolRetaliateTurns;
            }
            // Ratchet the frenzy high-water mark (chooseMove reads it), so re-casting onto
            // a live pool is how a pet holds a frenzy open past the hits that would pop it.
            if (actor.shieldHp > actor.phishShieldPeak)
                actor.phishShieldPeak = actor.shieldHp;
            if (mitmCopy) {
                mirror.shieldHp += braced;
                if (mirror.shieldHp > mirror.phishShieldPeak)
                    mirror.phishShieldPeak = mirror.shieldHp;
            }
        } else if (spawnsDefender) {
            // A Worm's defend does not brace: the body it puts on the board (rollWormSpawn,
            // after this resolves) IS the move. The row's `power` exists only so the turn
            // still does something when every replication slot is full.
        } else {
            actor.guard += braced;
            if (mitmCopy) mirror.guard += braced;
        }
        // Cipher track: the cast stacks the caster's Defense (% cut) for the
        // fight, capped per move; the attack path clamps the total to 85% (never immune).
        if (mv->stackDefensePct > 0 && actor.stackDefenseBonus < mv->stackDefenseCap) {
            int gain = mv->stackDefensePct;
            if (actor.stackDefenseBonus + gain > mv->stackDefenseCap)
                gain = mv->stackDefenseCap - actor.stackDefenseBonus;
            actor.stackDefenseBonus += gain;
            if (mitmCopy) mirror.stackDefenseBonus += gain;
        }
        // Once that wall is FULL, the next thing to hit it is seized rather than absorbed
        // (RansomSeizure). The full wall is the whole condition — the seizure starts the
        // ransom clock itself rather than requiring one to already be running, which would
        // need two independent things to coincide. Asked after the stack above so the cast
        // that fills the cap is the one that arms. `moveIdx < 0` is a hijacked cast
        // (Execution-Override), which owns no slot to seize into.
        if (mv->stackDefensePct > 0 && moveIdx >= 0 &&
            actor.stackDefenseBonus >= mv->stackDefenseCap && !actor.ransomSeizure.holding()) {
            actor.ransomSeizure.armed = true;
            actor.ransomSeizure.slot = moveIdx;
        }
        // Trojan trap (Trojan line): a trap move ARMS a trap (its power is 0, so the guard
        // line above is a no-op) that stacks up to kTrojanTrapCap and springs on the enemy's
        // next hit (attack path above). When the pile is full the oldest trap drops.
        if (mv->trapArm > 0) {
            if (actor.trojanTrapCount >= kTrojanTrapCap) {
                for (int i = 1; i < kTrojanTrapCap; ++i)
                    actor.trojanTraps[i - 1] = actor.trojanTraps[i];
                actor.trojanTrapCount = kTrojanTrapCap - 1;
            }
            actor.trojanTraps[actor.trojanTrapCount++] = mv;
        }
        setLast(mv->displayName, 0, byPlayer, false);
    }
}

// The steal track, and the frenzy heal that hangs off it. Lifted whole out of
// applyEffect so the mitigation chain there reads as one thing: every branch below is
// a landed hit paying out, and the caller has already decided that a hit landed.
// Draws rng() only through Perfect Bite, at the point applyEffect always drew it.
void Combat::applyStealTrack(Combatant& actor, Combatant& target, const MoveDef& mv) {
    // Every non-zero steal* field fires, independently — a move setting more than one
    // steals more than one. stealPowerPct/stealDefensePct are unconditional;
    // stealSpeedPct/stealCurrentHpPct also need the caster's Obfuscation bubble up, so
    // going aggressive with the volatile pair costs bubble uptime. Per-field detail is on
    // MoveDef (defs.h).
    //
    // Every branch is a TRANSFER, so a floored target (Net Neutrality) pays the thief
    // nothing. Checked per-branch, not around the block, because stealCurrentHpPct is in
    // here and Health is not one of the floored leans.
    const bool floored = statsFloored(target);
    bool powerSiphoned = false;
    if (mv.stealPowerPct > 0 && !floored) {
        const int stolen = target.powerMultPct * mv.stealPowerPct / 100;
        if (stolen > 0) {
            actor.powerMultPct += stolen;
            target.powerMultPct -= stolen;
            if (target.powerMultPct < kStealPowerFloorPct)
                target.powerMultPct = kStealPowerFloorPct;
            powerSiphoned = true;
        }
    }
    if (mv.stealDefensePct > 0 && !floored) {
        const int stolen = target.dmgReducePct * mv.stealDefensePct / 100;
        if (stolen > 0) {
            actor.dmgReducePct += stolen;
            target.dmgReducePct -= stolen;
            if (target.dmgReducePct < 0) target.dmgReducePct = 0;
        }
    }
    // Perfect Bite: rolled once per hit, only when the move carries a bubble-gated
    // field and the bubble is up, so a fight without the track draws no rng(). A
    // move setting both gated fields takes a second roll for which one doubles.
    const bool bubbleUp = actor.shieldHp > 0;
    const bool bubbleGatedMove = mv.stealSpeedPct > 0 || mv.stealCurrentHpPct > 0;
    bool bite = false, biteHitsSpeed = mv.stealSpeedPct > 0;
    if (bubbleUp && bubbleGatedMove) {
        bite = bubbleBiteRolls(actor.stage);
        if (bite && mv.stealSpeedPct > 0 && mv.stealCurrentHpPct > 0)
            biteHitsSpeed = (rng() % 2 == 0);
    }
    // The pool bonus applies to whichever gated steal fires, on top of the base and
    // any Perfect-Bite doubling — the bubble both permits these two and sizes them.
    const int poolBonus = phishPoolSiphonBonusPct(actor);
    if (bubbleUp && mv.stealSpeedPct > 0 && !floored) {
        int pct = mv.stealSpeedPct;
        // The Phishing Rod only ever scales the BITE half — it amplifies the
        // bonus, not the move's own base siphon.
        if (bite && biteHitsSpeed)
            pct += mv.stealSpeedPct *
                   (100 + actor.mods.mag(ModEffect::StealAmplifyPct)) / 100;
        // FLOAT: a percentage of the target's already-siphoned speed truncates to 0
        // in int arithmetic once speed nears the floor, killing repeat steals.
        pct += mv.stealSpeedPct * poolBonus / 100;
        const float stolen = target.speed * pct / 100.0f;
        if (stolen > 0.0f) {
            actor.speed += stolen;
            target.speed -= stolen;
            if (target.speed < kStealSpeedFloor) target.speed = kStealSpeedFloor;
        }
    }
    if (bubbleUp && mv.stealCurrentHpPct > 0) {
        int pct = mv.stealCurrentHpPct;
        if (bite && !biteHitsSpeed)
            pct += mv.stealCurrentHpPct *
                   (100 + actor.mods.mag(ModEffect::StealAmplifyPct)) / 100;
        pct += mv.stealCurrentHpPct * poolBonus / 100;
        const int stolen = target.health * pct / 100;   // lifesteal: target's
        if (stolen > 0) {                               // CURRENT health drains
            target.health -= stolen;                    // straight to the caster
            actor.health += stolen;
            if (actor.health > actor.maxHealth) actor.health = actor.maxHealth;
        }
    }
    if (mv.stealMaxHpPct > 0 && !floored) {
        const int stolen = target.maxHealth * mv.stealMaxHpPct / 100;
        if (stolen > 0 && target.maxHealth - stolen >= 1) {
            target.maxHealth -= stolen;                 // permanent for the fight
            if (target.health > target.maxHealth) target.health = target.maxHealth;
            // The pool MOVES — ceiling and the Health inside it both cross. Combat
            // has no heal to climb into a raised ceiling, so a bare maxHealth gain
            // would read as a pure debuff on the victim and nothing for the caster.
            actor.maxHealth += stolen;
            actor.health += stolen;
        }
    }
    // Feed-frenzy: a landed POWER siphon from inside an Obfuscation bubble
    // devours a sliver of the shield as healing (0.75% of its HP, at least 1),
    // so a stacked shield both tanks and sustains. No shield up -> no heal.
    if (powerSiphoned && actor.shieldHp > 0) {
        int heal = actor.shieldHp * kFrenzyHealPermille / 1000;
        if (heal < 1) heal = 1;
        actor.health += heal;
        if (actor.health > actor.maxHealth) actor.health = actor.maxHealth;
    }
}


bool Combat::ransomArmRolls(const Combatant& c) {
    // Scaled by the ransomer's stage. The passive check short-circuits before any rng()
    // draw, so a side without it never perturbs the deterministic stream.
    if (!hasLinePassive(c.linePassives, LinePassive::RansomNote)) return false;
    const int si = stageIndex(c.stage);
    const int pct = (si >= 0 && si < 4) ? kRansomArmPctByStage[si] : 0;
    if (pct <= 0) return false;
    return static_cast<int>(rng() % 100) < pct;
}

bool Combat::bubbleBiteRolls(Stage stage) {
    // applyEffect has already checked the bubble and the gated field, so this is purely the
    // stage-scaled chance. Short-circuits before the draw at 0% (Boot).
    const int si = stageIndex(stage);
    const int pct = (si >= 0 && si < 4) ? kPhishingBiteChancePctByStage[si] : 0;
    if (pct <= 0) return false;
    return static_cast<int>(rng() % 100) < pct;
}

int stunLandPct(const Combatant& c) {
    if (c.lockResist <= 0) return 100;
    const int pct = 100 - c.lockResist * kLockResistStepPct;
    return pct < kLockResistFloorPct ? kLockResistFloorPct : pct;
}

bool Combat::stunLands(const Combatant& target) {
    // Nothing to beat, no draw: the first stun of a chain always lands, and a fight with
    // no chain-stunning never draws here.
    if (target.lockResist <= 0) return true;
    return static_cast<int>(rng() % 100) < stunLandPct(target);
}

void Combat::syncWormSpeed() {
    // Shared Resources: the worm's speed IS the opponent's, so pickNextActor always deals
    // actions 1:1 and nothing out-actions a worm. Exactly one worm, or nothing happens —
    // two are already in lockstep, and matching each to the other would swap forever.
    const bool pw = replicates(player_), ew = replicates(enemy_);
    if (pw == ew) return;
    if (pw) player_.speed = enemy_.speed;
    else    enemy_.speed = player_.speed;
}

void Combat::rollWormSpawn(Combatant& actor, const MoveDef* mv) {
    // Slots are the hard cap; a full board doesn't roll, so it draws no rng.
    if (actor.wormReplicaCount >= kWormReplicaSlots) return;
    // Replication Bus (mod) raises the RATE, never the cap — the slot check above still
    // runs first, so an equipped mod never changes the rng draw count.
    const int spawnPct = mv->replicaSpawnPct + actor.mods.mag(ModEffect::ReplicaSpawnPct);
    if (static_cast<int>(rng() % 100) >= spawnPct) return;
    WormReplica& r = actor.wormReplicas[actor.wormReplicaCount];
    r = WormReplica{};
    r.defender = mv->kind == MoveDef::Kind::Defend;
    if (r.defender) {
        // A defender is a body: real Health, no swing. Size banked from the attackers
        // already out (wormDefenderHealth), so spawn order is the player's decision.
        r.maxHealth = wormDefenderHealth(actor, mv->replicaHealthPct);
        r.health = r.maxHealth;
    } else {
        // An attacker is thin — one hit takes it — and pays by piling onto the parent's
        // swings (wormReplicaDamage), scaled by the same attack lean the parent uses.
        r.maxHealth = 1;
        r.health = 1;
        r.attack = wormAttackerDamage(actor, mv->power, mv->replicaPowerPct);
    }
    ++actor.wormReplicaCount;
}

int Combat::execOverrideChance(const Combatant& trojan) const {
    // Returns 0 before any rng() draw at the call site, so a pet without the passive never
    // perturbs the stream. Base chance is low; each armed trap adds its
    // trapPassiveBonusPct, so holding all three traps makes the hijack likely.
    if (!hasLinePassive(trojan.linePassives, LinePassive::ExecOverride)) return 0;
    int pct = kExecOverrideBasePct;
    for (int i = 0; i < trojan.trojanTrapCount; ++i)
        if (trojan.trojanTraps[i]) pct += trojan.trojanTraps[i]->trapPassiveBonusPct;
    // Ring-0 Shim (mod) adds to the same sum the traps do, so it rewards a trap build
    // rather than substituting for one.
    pct += trojan.mods.mag(ModEffect::ExecOverridePct);
    return pct;
}

void Combat::resolveTurn(Combatant& actor, Combatant& target, bool byPlayer) {
    // Load Balancer (mod): the deferred half of an earlier big hit comes due at the start of
    // the victim's turn, before anything else, so a fatal debt ends the turn without the
    // actor getting to act — checkOutcome() after step() then reads the KO.
    if (ModState* lb = actor.mods.find(ModEffect::LoadBalance); lb && lb->pending > 0) {
        const int due = lb->pending;
        lb->pending = 0;
        actor.health -= due;
        if (actor.health <= 0) {                          // the debt came due, fatally
            setLast("OVERLOAD", due, byPlayer, /*charge=*/false);
            return;
        }
    }

    // DoT: corruption an earlier hit planted bites at the start of every one of the
    // victim's turns, independent of any lock — a frozen process still rots. Ticks before
    // the lock burn so a fatal DoT ends the turn even while stunned. Faraday Cage (mod)
    // already cut the magnitude when the DoT was applied (applyEffect).
    if (actor.dotTurnsLeft > 0 && actor.dotPerTurn > 0) {
        actor.dotTurnsLeft--;
        actor.health -= actor.dotPerTurn;
        if (actor.health <= 0) {
            setLast("CORRUPTED", actor.dotPerTurn, byPlayer, /*charge=*/false);
            return;
        }
    }

    // Regen (mod): last of the three turn-start ticks, and only on a fighter that survived
    // the other two. A heal running first would cancel the rot before it bit, which is
    // Faraday Cage's job alone — coming last, regen recovers from the fight and never from
    // the tick currently killing you. No popup of its own; the climbing Health bar is the
    // feedback, and this turn's move would overwrite the slot anyway.
    if (const int regen = actor.mods.mag(ModEffect::RegenPerTurn); regen > 0 && actor.health > 0) {
        actor.health += regen;
        if (actor.health > actor.maxHealth) actor.health = actor.maxHealth;
    }

    // Ransom Note, the WINDOW half. Rolled per TURN rather than per incoming hit, so a
    // linked duel stays in step: the window is fixed by the seed before the turn plays out
    // and can't depend on how the opponent's speed deals actions inside it. An armed window
    // stays open until a hit lands in it (applyEffect closes it) rather than lapsing — so
    // the chance sets how often a HIT is ransomed, not how often a turn is.
    if (!actor.ransomArmed) actor.ransomArmed = ransomArmRolls(actor);

    // ...and the BILL half. The countdown burns one of the RANSOMER's own turns per tick,
    // not one per incoming action, so a fast opponent buys more hits inside the window
    // rather than a faster payout. At zero the whole pool lands in one blow, which can kill.
    // Paying costs the turn (unlike the ticks above, which fold into it): the pool's point
    // is arriving as one legible blow. Ahead of the stun, so a frozen pet still pays.
    if (actor.ransomTurnsLeft > 0 && --actor.ransomTurnsLeft == 0) {
        // Outside the pool check: a ransom that caught no damage still ends here, and a
        // seizure must not outlive the hold that justified it.
        releaseRansomSeizure(actor);
        if (actor.ransomPool > 0) {
            const int due = actor.ransomPool;
            actor.ransomPool = 0;
            actor.health -= due;
            setLast("RANSOM DUE", due, byPlayer, /*charge=*/false);
            return;
        }
    }

    // STUN: a landed hit's lockTurns rider freezes `actor` for a few of its own turns —
    // a pure skip, no delayed fire.
    if (actor.lockedTurnsLeft > 0) {
        actor.lockedTurnsLeft--;
        setLast("STUN LOCK", 0, byPlayer, /*charge=*/true);
        return;
    }
    // Only a turn spent FIGHTING sheds a resist point — one burned to the lock or a ransom
    // bill returned above — so resistance grows through a chain and drains once it breaks.
    if (actor.lockResist > 0) actor.lockResist--;

    int moveIdx;
    if (byPlayer && forcedMoveIdx_ >= 0 &&
        forcedMoveIdx_ < static_cast<int>(actor.moves.size())) {
        // A committed override ignores the lean and the no-consecutive rule, so it can
        // chain the same move twice, and it resets any in-progress channel.
        moveIdx = forcedMoveIdx_;
        forcedMoveIdx_ = -1;
        actor.channelMoveIdx = -1;
        actor.channelLeft = 0;
        actor.chainSlot = -1;    // commanding a move breaks a chain mid-flight
    } else if (actor.channelMoveIdx >= 0) {
        moveIdx = actor.channelMoveIdx;               // committed mid-channel
    } else {
        moveIdx = chooseMove(actor);
    }
    // A chained move's follow-up step, committed by last turn's entry cast. Overrides the
    // roll so the step lands on the very next turn — the entry already spent its turn doing
    // something real, which is what separates a chain from a wind-up. `moveIdx` stays the
    // ENTRY's slot, so everything keying off the cast slot still points at what was equipped.
    const MoveDef* chained = nullptr;
    if (actor.chainSlot >= 0 && actor.chainSlot < static_cast<int>(actor.chainFollow.size())) {
        chained = actor.chainFollow[actor.chainSlot];
        if (chained) moveIdx = actor.chainSlot;
        actor.chainSlot = -1;
    }
    const MoveDef* mv = chained ? chained : actor.moves[moveIdx];

    // METAMORPHIC: a wildcard row does not cast itself — it rolls one of the moves this pet
    // could have been. One branch, not a loop, so a nested wildcard resolves as an ordinary
    // move; an empty pool leaves `mv` as the row itself. Unreachable from `chained`, which
    // is a payload rather than a slot and has no pool.
    const WildPool* wild = nullptr;
    if (!chained && moveIsWildcard(*mv) && moveIdx >= 0 &&
        moveIdx < static_cast<int>(actor.wildPools.size())) {
        wild = &actor.wildPools[moveIdx];
        if (const MoveDef* drawn = wildPick(*wild, rng())) {
            mv = drawn;
            // What the picker's LOCK band offers. Written on the roll, not on a resolved
            // hit — an operator locks the move they watched come up.
            actor.wildPools[moveIdx].lastRolled = drawn;
        }
    }

    // Execution-Override (Trojan passive): `target`, the side NOT acting, hijacks the
    // actor's freshly-picked move and runs it back at them, consuming their turn.
    // execOverrideChance is 0 for a non-Trojan target, so rng() is only drawn when the
    // passive is live. moveIdx = -1 keeps the hijacked cast out of the Trojan's mod state.
    if (actor.channelMoveIdx < 0) {
        const int hijackPct = execOverrideChance(target);
        if (hijackPct > 0 && static_cast<int>(rng() % 100) < hijackPct) {
            applyEffect(target, actor, mv, !byPlayer, /*moveIdx=*/-1);
            actor.lastMoveIdx = moveIdx;              // the actor still "used" it
            return;
        }
    }

    if (mv->channelTurns > 1) {
        if (actor.channelMoveIdx != moveIdx) {        // begin the wind-up
            actor.channelMoveIdx = moveIdx;
            actor.channelLeft = mv->channelTurns - 1;
            actor.lastMoveIdx = moveIdx;
            setLast(mv->displayName, 0, byPlayer, /*charge=*/true);
            return;
        }
        if (actor.channelLeft > 0) {                  // still charging
            actor.channelLeft--;
            if (actor.channelLeft > 0) {
                setLast(mv->displayName, 0, byPlayer, /*charge=*/true);
                return;
            }
        }
        actor.channelMoveIdx = -1;                    // detonate this turn
    }

    // POLYMORPH, and the identity a borrowed row brings with it. Both run BEFORE the cast
    // resolves, so this move swings with the stats absorbing it just paid, and a Worm row
    // spawns on the same turn it grants Replication.
    if (wild) actor.linePassives |= wildBorrowedPassives(*wild, mv);
    polymorphAbsorb(actor, mv);
    // Mutation Engine's axis, recorded on the CAST rather than on whichever riders survived
    // the target's defences: what it reads is the range of things this fighter reached for.
    if (actor.polymorphic) {
        const uint32_t before = actor.effectsSeen;
        actor.effectsSeen |= moveEffectMask(*mv);
        // Mutation Engine (mod): a rider KIND this fighter has not reached for before pays
        // the passive's own stat point, the same way an unlearned move does — an amplifier
        // of Polymorph rather than a flat bonus beside it.
        if (const int per = actor.mods.mag(ModEffect::PolymorphEffectPct); per > 0) {
            int fresh = 0;
            for (uint32_t d = actor.effectsSeen & ~before; d; d &= d - 1) ++fresh;
            polymorphPay(actor, mv->kind, per * fresh);
        }
    }

    applyEffect(actor, target, mv, byPlayer, moveIdx);
    // Tempo refund (MoveDef::speedRefundPct): part of an action's gauge back to whoever
    // just spent a turn bracing. Here rather than inside applyEffect because what is
    // refunded is the TURN, which belongs to `actor` — an Execution-Override hijack calls
    // applyEffect directly and must not pay tempo to a fighter that spent nothing.
    if (mv->kind == MoveDef::Kind::Defend && mv->speedRefundPct > 0) {
        float& gauge = byPlayer ? plGauge_ : enGauge_;
        gauge += kSpeedActionThreshold * mv->speedRefundPct / 100.0f;
        // A refund shortens the wait, never grants an action outright.
        if (gauge >= kSpeedActionThreshold) gauge = kSpeedActionThreshold - 1;
    }
    // Hand the slot to this move's follow-up step. Set after the entry RESOLVED, so a cast
    // that killed the target commits nothing. `chained` guards the loop: a step never
    // chains onward, so a pair is two turns and not a track a fighter can't get off.
    if (!chained && moveIdx >= 0 && moveIdx < static_cast<int>(actor.chainFollow.size()) &&
        actor.chainFollow[moveIdx] && outcome_ == Outcome::Ongoing)
        actor.chainSlot = moveIdx;
    // Replication: a cast carrying replicaSpawnPct rolls for a copy once RESOLVED, so a
    // fresh replica joins the next swing rather than the one that spawned it. One site for
    // both kinds — the move's `kind` picks which appears. Guarded on the field AND the
    // line, so no other line draws rng here.
    if (mv->replicaSpawnPct > 0 && replicates(actor)) rollWormSpawn(actor, mv);
    actor.lastMoveIdx = moveIdx;

    // FAILOVER (crew Exploit): one charge runs THIS cast a second time against the same
    // target, on the spot. The CAST and not the turn — sitting here rather than around
    // resolveTurn's caller means the turn-start ticks, the tempo refund and the chain
    // hand-off stay bought once, so holding a spare never costs a second DoT tick. What
    // repeats is the swing and the roll hanging off it, rollWormSpawn included.
    //
    // Every early return above (stunned, mid-channel, hijacked) leaves the charge unspent,
    // there having been no cast to stand in for. One extra pass, never a loop.
    if (actor.crewExploit.armed(CrewExploitKind::SpareFailover) && target.health > 0 &&
        outcome_ == Outcome::Ongoing) {
        --actor.crewExploit.charges;
        applyEffect(actor, target, mv, byPlayer, moveIdx);
        if (mv->replicaSpawnPct > 0 && replicates(actor)) rollWormSpawn(actor, mv);
    }
}

void Combat::checkOutcome() {
    // The one place a pet is judged overwhelmed, and so the one place a death-save runs.
    // Everything that spends Health leaves it unclamped and lands here, so a save reads the
    // pet's STATE rather than the thing that got it there.
    //
    // Backup Plan B (crew Exploit) looks at the hole FIRST — the player spent an Exploit
    // use on it, so a granted one-shot stays held for a later hole. It restores TO half of
    // max (the Backup Drive below ADDS half to wherever the pet ended up) and pays the
    // overkill back as Power, multiplied by the turns still on its clock: a bigger blow
    // rallies harder. One shot. Asked of both sides, since a tournament opponent arms its
    // own Exploits.
    auto rallySave = [](Combatant& c) {
        if (c.health > 0 || !c.crewExploit.ticking(CrewExploitKind::DeathSaveRally))
            return;
        const int overkill = -c.health;
        c.health = c.maxHealth / 2;
        if (c.health < 1) c.health = 1;              // a 1-HP fighter still gets back up
        c.stackPowerBonus += overkill * c.crewExploit.turns;
        c.crewExploit = CrewExploitState{};
    };
    rallySave(player_);
    rallySave(enemy_);
    if (player_.health <= 0) player_.restoreFromBackup();
    // ...and the floor, after the save has had its look at how deep the hole is.
    if (player_.health < 0) player_.health = 0;
    if (enemy_.health < 0) enemy_.health = 0;
    if (enemy_.health <= 0) outcome_ = Outcome::Win;        // win takes priority
    else if (player_.health <= 0) outcome_ = Outcome::Lose;
}

bool Combat::pickNextActor() {
    // Re-matched at EVERY scheduling tick, so a mid-fight speed change on either side is
    // matched the instant it lands.
    syncWormSpeed();
    // Fill both gauges by live speed (min 1, so a fully-siphoned pet still acts and the
    // loop terminates) until one reaches the threshold; that side acts and spends one
    // threshold's worth, leaving the other's carry to build. Ties go to the player.
    // Actions are therefore dealt in proportion to relative speed.
    const float ps = player_.speed < 1 ? 1 : player_.speed;
    const float es = enemy_.speed < 1 ? 1 : enemy_.speed;
    for (int i = 0; i <= kSpeedActionThreshold; ++i) {
        plGauge_ += ps;
        enGauge_ += es;
        const bool pr = plGauge_ >= kSpeedActionThreshold;
        const bool er = enGauge_ >= kSpeedActionThreshold;
        if (pr || er) {
            const bool playerActs = (pr && er) ? plGauge_ >= enGauge_ : pr;
            if (playerActs) plGauge_ -= kSpeedActionThreshold;
            else            enGauge_ -= kSpeedActionThreshold;
            return playerActs;
        }
    }
    return true;   // unreachable: with speed >= 1 a gauge crosses within threshold ticks
}

bool Combat::step() {
    if (outcome_ != Outcome::Ongoing || overrideOpen_) return false;
    // One-turn lifetime, like lastDamage_ and friends.
    lastWormKill_ = {};
    // Feeding-frenzy streak, computed BEFORE resolveTurn so applyEffect's combo bonus sees
    // this hit's own place in the run.
    if (streakCount_ > 0 && playerTurn_ == streakIsPlayer_) ++streakCount_;
    else { streakCount_ = 1; streakIsPlayer_ = playerTurn_; }
    const bool playerActed = playerTurn_;
    // An Exploit a side fires itself SPENDS the turn (Combatant::autoExploit), so it is
    // asked before the move roll and short-circuits it. The clock and scheduler below
    // still run — a turn spent arming is still a turn taken.
    Combatant& actor = playerTurn_ ? player_ : enemy_;
    if (!fireAutoExploit(actor, playerTurn_)) {
        if (playerTurn_) resolveTurn(player_, enemy_, /*byPlayer=*/true);
        else resolveTurn(enemy_, player_, /*byPlayer=*/false);
    }
    checkOutcome();
    // Both sides are offered the turn; each kind takes only the clock it answers to.
    tickCrewExploitClock(playerActed ? player_ : enemy_, /*actedThisTurn=*/true);
    tickCrewExploitClock(playerActed ? enemy_ : player_, /*actedThisTurn=*/false);
    playerTurn_ = pickNextActor();   // schedule the next actor by relative speed
    return true;
}

bool Combat::fireAutoExploit(Combatant& actor, bool byPlayer) {
    if (actor.autoExploitFired || !actor.autoExploit.label ||
        actor.autoExploit.kind == CrewExploitKind::None)
        return false;
    // A fraction of this fighter's OWN max, so the wait is the same number of proportional
    // hits whatever its Health pool. A 100% threshold means "open with it".
    const int maxH = actor.maxHealth > 0 ? actor.maxHealth : 1;
    if (actor.health * 100 > actor.autoExploitAtHealthPct * maxH) return false;
    actor.autoExploitFired = true;
    armCrewExploit(actor, actor.autoExploit, byPlayer);
    return true;
}

void Combat::tickCrewExploitClock(Combatant& c, bool actedThisTurn) {
    // Two turn-metered kinds on opposite clocks. Backup Plan B is a death-save, so its
    // clock is the INCOMING turns it covers — three turns means the opponent's next three.
    // Protection Racket rides the holder's own swings, so it burns a turn when the holder
    // acts: three of yours is a promise about a number of swings.
    //
    // A turn spent stunned, rotting or paying still counts on either clock — the caller
    // ticks after the whole turn resolved, so resolveTurn's early returns count as swings.
    if (!actedThisTurn && c.crewExploit.ticking(CrewExploitKind::DeathSaveRally))
        --c.crewExploit.turns;
    if (actedThisTurn && c.crewExploit.ticking(CrewExploitKind::LeechOnHit))
        --c.crewExploit.turns;
}

int Combat::overrideMoveCount() const {
    return static_cast<int>(player_.moves.size());
}

int Combat::overrideLockCount() const {
    int n = 0;
    for (const WildPool& p : player_.wildPools)
        if (p.lastRolled) ++n;
    return n;
}

int Combat::overrideLockSlot(int i) const {
    if (i < 0) return -1;
    for (size_t s = 0; s < player_.wildPools.size(); ++s) {
        if (!player_.wildPools[s].lastRolled) continue;
        if (i-- == 0) return static_cast<int>(s);
    }
    return -1;
}

const MoveDef* Combat::overrideLockMove(int i) const {
    const int slot = overrideLockSlot(i);
    return slot < 0 ? nullptr : player_.wildPools[slot].lastRolled;
}

const char* overrideBandName(OverrideBand b) {
    switch (b) {
        case OverrideBand::Move: return "MOVES";
        case OverrideBand::Item: return "ITEMS";
        case OverrideBand::Lock: return "LOCK";
        case OverrideBand::Crew: return "CREW";
    }
    return "";
}

int Combat::overrideBandRows(OverrideBand b) const {
    switch (b) {
        case OverrideBand::Move: return overrideMoveCount();
        case OverrideBand::Item: return static_cast<int>(overrideItems_.size());
        case OverrideBand::Lock: return overrideLockCount();
        case OverrideBand::Crew: return overrideCrewRows();
    }
    return 0;
}

int Combat::overrideBandFirst(OverrideBand b) const {
    int first = 0;
    for (int i = 0; i < static_cast<int>(b); ++i)
        first += overrideBandRows(static_cast<OverrideBand>(i));
    return first;
}

int Combat::overrideBandCount() const {
    int n = 0;
    for (int b = 0; b < kOverrideBands; ++b)
        if (overrideBandRows(static_cast<OverrideBand>(b)) > 0) ++n;
    return n;
}

OverrideBand Combat::overrideBandAt(int i) const {
    for (int b = 0; b < kOverrideBands; ++b) {
        const OverrideBand band = static_cast<OverrideBand>(b);
        if (overrideBandRows(band) <= 0) continue;
        if (i-- == 0) return band;
    }
    return OverrideBand::Move;
}

OverrideBand Combat::overrideBandOf(int flatPick) const {
    int end = 0;
    for (int b = 0; b < kOverrideBands; ++b) {
        end += overrideBandRows(static_cast<OverrideBand>(b));
        if (flatPick < end) return static_cast<OverrideBand>(b);
    }
    return OverrideBand::Move;
}

void Combat::openOverride(std::vector<OverrideItem> items, CrewExploit crew) {
    if (overrideUsesLeft_ <= 0 || outcome_ != Outcome::Ongoing) return;
    overrideItems_ = std::move(items);
    crewExploit_ = crew;
    overrideOpen_ = true;
    overrideBandPick_ = 0;
    // One band is not a choice, so skip the band list rather than spend a press on it.
    overrideAtBands_ = overrideBandCount() > 1;
    overridePick_ = overrideBandFirst(overrideBandAt(0));
}

void Combat::cycleOverride() {
    if (!overrideOpen_) return;
    if (overrideAtBands_) {
        const int n = overrideBandCount();
        if (n <= 0) return;
        overrideBandPick_ = (overrideBandPick_ + 1) % n;
        // The flat cursor follows the highlighted band, so a commit target is defined at
        // both levels and descending never has to invent one.
        overridePick_ = overrideBandFirst(overrideBandAt(overrideBandPick_));
        return;
    }
    // Inside a band the walk wraps WITHIN it — crossing into the neighbour is what the
    // band level is for.
    const OverrideBand band = overrideBandOf(overridePick_);
    const int first = overrideBandFirst(band);
    const int rows = overrideBandRows(band);
    if (rows <= 0) return;
    overridePick_ = first + (overridePick_ - first + 1) % rows;
}

bool Combat::enterOverrideBand() {
    if (!overrideOpen_ || !overrideAtBands_) return false;
    overrideAtBands_ = false;
    overridePick_ = overrideBandFirst(overrideBandAt(overrideBandPick_));
    return true;
}

bool Combat::leaveOverrideBand() {
    if (!overrideOpen_ || overrideAtBands_ || overrideBandCount() <= 1) return false;
    const OverrideBand band = overrideBandOf(overridePick_);
    overrideAtBands_ = true;
    overrideBandPick_ = 0;
    for (int i = 0; i < overrideBandCount(); ++i)
        if (overrideBandAt(i) == band) { overrideBandPick_ = i; break; }
    return true;
}

void Combat::applyCrewExploit() { armCrewExploit(player_, crewExploit_, /*byPlayer=*/true); }

void Combat::armCrewExploit(Combatant& self, const CrewExploit& x, bool byPlayer) {
    // Each kind meters itself out of the shared CrewExploitState counters; re-arming
    // the same kind stacks. Adding an ability is a case here plus its crewExploitTag().
    // A fighter carries one Exploit, so `kind` is only ever set to what it already is.
    if (x.kind == CrewExploitKind::None) return;
    self.crewExploit.kind = x.kind;
    switch (x.kind) {
        case CrewExploitKind::NegateNextHits:
        case CrewExploitKind::PowerByDamageDealt:
        case CrewExploitKind::SpareFailover:
            self.crewExploit.charges += x.magnitude;
            break;
        case CrewExploitKind::DeathSaveRally:
        case CrewExploitKind::LeechOnHit:
            self.crewExploit.turns += x.magnitude;
            break;
        case CrewExploitKind::ResetStatsAndFloor:
            // Snap the three live stat LEANS back to the walk-in values, in BOTH
            // directions — fire it drained and it restores, fire it buffed and it costs —
            // then statsFloored holds them there. The earned line-stack tracks (Lockout
            // Power, Cipher Defense) are progress, not leans, and nothing erodes them;
            // maxHealth already drunk from by a steal is not given back either.
            self.powerMultPct = self.basePowerMultPct;
            self.speed = self.baseSpeed;
            self.dmgReducePct = self.baseDmgReducePct;
            break;
        case CrewExploitKind::MirrorEnemyBuffs:
            break;      // sticky: being armed IS the effect (applyEffect's mitmCopy)
        case CrewExploitKind::None:
            return;
    }
    // "<TAG> xN" popup — bare "<TAG>" for a sticky kind. dmg=0 keeps the red damage number
    // hidden; the readout rides in the move-name slot. `byPlayer` puts it over the right
    // fighter, so an opponent's Exploit announces itself on its own row.
    crewExploitLabel(itemPopup_, sizeof(itemPopup_), x.kind, x.magnitude);
    setLast(itemPopup_, 0, byPlayer, /*charge=*/false);
}

void Combat::commitOverride() {
    // Inert at the band level: nothing there is a row to spend a use on, and B's
    // meaning at that level is enterOverrideBand.
    if (!overrideOpen_ || overrideAtBands_) return;
    const int moves = overrideMoveCount();
    const int items = static_cast<int>(overrideItems_.size());
    const int locks = overrideLockCount();
    if (overridePick_ < moves) {
        forcedMoveIdx_ = overridePick_;         // forces the player's next move
    } else if (overridePick_ >= moves + items + locks) {
        applyCrewExploit();                     // the crew row (last band)
    } else if (overridePick_ >= moves + items) {
        // LOCK: the slot stops drawing and keeps what it last rolled. Replacing the row in
        // `moves` rather than flagging it makes the slot ordinary, so every reader sees the
        // committed move with nothing new taught to it. The chain step is not committed —
        // a substituted move hands nothing on, and resolving one would need the registry.
        const int slot = overrideLockSlot(overridePick_ - moves - items);
        if (slot >= 0 && slot < static_cast<int>(player_.moves.size())) {
            player_.moves[slot] = player_.wildPools[slot].lastRolled;
            player_.wildPools[slot] = WildPool{};   // drawing stops; nothing left to offer
            player_.chainFollow[slot] = nullptr;
        }
    } else {
        // USE ITEM: patch transient Health here (combat state); the Game consumes
        // the stack + applies the item's own effect off takeCommittedItem().
        const OverrideItem& it = overrideItems_[overridePick_ - moves];
        player_.health += it.heal;
        if (player_.health > player_.maxHealth) player_.health = player_.maxHealth;
        committedItemId_ = it.id;
        // "PATCH +N" popup. dmg=0 keeps the red damage number hidden. lastMoveName_ borrows
        // this member buffer, which is why it lives on the Combat object.
        std::snprintf(itemPopup_, sizeof(itemPopup_), "PATCH +%d", it.heal);
        setLast(itemPopup_, 0, /*byPlayer=*/true, /*charge=*/false);
    }
    if (overrideUsesLeft_ > 0) --overrideUsesLeft_;   // spent (a pip greys out)
    overrideOpen_ = false;
}

void Combat::cancelOverride() { overrideOpen_ = false; }   // no spend

const char* Combat::takeCommittedItem() {
    const char* id = committedItemId_;
    committedItemId_ = nullptr;
    return id;
}

void Combat::flee() {
    if (outcome_ != Outcome::Ongoing) return;
    if (stakes_ == Stakes::Safe) {                  // Sim-Battle C = quit, always
        outcome_ = Outcome::Fled;
        return;
    }
    // Wild: an escape roll; a fail gives the enemy a free turn (retreat isn't free).
    if (static_cast<int>(rng() % 100) < kFleeChancePct) {
        outcome_ = Outcome::Fled;
    } else {
        resolveTurn(enemy_, player_, /*byPlayer=*/false);
        checkOutcome();
        // The free turn is a turn; both clocks read it as step() would.
        tickCrewExploitClock(enemy_, /*actedThisTurn=*/true);
        tickCrewExploitClock(player_, /*actedThisTurn=*/false);
    }
}

const MoveDef* Combat::enemyChannel() const {
    if (enemy_.channelMoveIdx < 0 ||
        enemy_.channelMoveIdx >= static_cast<int>(enemy_.moves.size()))
        return nullptr;
    return enemy_.moves[enemy_.channelMoveIdx];
}

void Combatant::restoreFromBackup() {
    if (!itemShield) return;
    itemShield = false;
    // Half of max, ADDED to where the pet ended up rather than restored TO a fixed level.
    // A pet buried deeper stays down, and the drive is spent either way — which is the
    // difference the two BackupUse states record.
    health += maxHealth / 2;
    backupUse = health > 0 ? BackupUse::Restored : BackupUse::Overwhelmed;
}

int attackPowerRank(const std::vector<const MoveDef*>& moves, int moveIdx) {
    if (moveIdx < 0 || moveIdx >= kMaxMoveSlots ||
        moveIdx >= static_cast<int>(moves.size()))
        return 0;
    if (moves[moveIdx]->kind != MoveDef::Kind::Attack) return 0;
    // Collect the DISTINCT Attack powers, sort ascending, and report where this move's
    // power lands. Ties share a rank, so a kit with one attack tier is rank 1 throughout.
    int distinctPowers[kMaxMoveSlots];
    int distinctCount = 0;
    for (const MoveDef* cm : moves) {
        if (cm->kind != MoveDef::Kind::Attack) continue;
        bool seen = false;
        for (int k = 0; k < distinctCount; ++k)
            if (distinctPowers[k] == cm->power) { seen = true; break; }
        if (!seen && distinctCount < kMaxMoveSlots) distinctPowers[distinctCount++] = cm->power;
    }
    for (int a = 1; a < distinctCount; ++a) {          // small insertion sort, ascending
        const int key = distinctPowers[a];
        int b = a - 1;
        while (b >= 0 && distinctPowers[b] > key) { distinctPowers[b + 1] = distinctPowers[b]; --b; }
        distinctPowers[b + 1] = key;
    }
    for (int k = 0; k < distinctCount; ++k)
        if (distinctPowers[k] == moves[moveIdx]->power) return k + 1;
    return 0;
}

// --- Worm replication, the pure half (see combat.h for what each one is for) --------

int wormReplicaCount(const Combatant& c, bool defenders) {
    int n = 0;
    for (int i = 0; i < c.wormReplicaCount; ++i)
        if (c.wormReplicas[i].defender == defenders) ++n;
    return n;
}

// Replication Bus (mod): what one copy's banked figure is multiplied by. Applied at SPAWN,
// so a bus equipped mid-run never pumps copies already standing.
static int replicaWorthMult(const Combatant& parent) {
    return 100 + parent.mods.mag(ModEffect::ReplicaWorthPct);
}

int wormAttackerDamage(const Combatant& parent, int movePower, int pct) {
    // A share of the move that made it, scaled by the parent's own attack lean and by the
    // defenders standing at this moment. Banked, exactly as a defender's Health is.
    const int mult = wormCrossMult(wormReplicaCount(parent, /*defenders=*/true));
    int dmg = movePower * pct / 100 * parent.powerMultPct / 100 * mult;
    dmg = dmg * replicaWorthMult(parent) / 100;
    return dmg < 1 ? 1 : dmg;
}

int wormReplicaDamage(const Combatant& c) {
    int total = 0;
    for (int i = 0; i < c.wormReplicaCount; ++i)
        if (!c.wormReplicas[i].defender) total += c.wormReplicas[i].attack;
    return total;
}

int wormDefenderHealth(const Combatant& parent, int pct) {
    const int mult = wormCrossMult(wormReplicaCount(parent, /*defenders=*/false));
    const int hp = parent.maxHealth * pct / 100 * mult * replicaWorthMult(parent) / 100;
    return hp < 1 ? 1 : hp;   // a defender that spawns with no Health is not a body
}

std::vector<int> wormTargetWeights(const Combatant& c) {
    std::vector<int> w;
    w.reserve(1 + c.wormReplicaCount);
    w.push_back(kWormTargetWeightParent);
    for (int i = 0; i < c.wormReplicaCount; ++i)
        w.push_back(c.wormReplicas[i].defender ? kWormTargetWeightDefender
                                               : kWormTargetWeightAttacker);
    return w;
}

int wormTargetPick(const Combatant& c, uint32_t roll) {
    const std::vector<int> w = wormTargetWeights(c);
    int total = 0;
    for (int x : w) total += x;
    if (total <= 0) return -1;
    int cursor = static_cast<int>(roll % static_cast<uint32_t>(total));
    for (int i = 0; i < static_cast<int>(w.size()); ++i) {
        cursor -= w[i];
        if (cursor < 0) return i - 1;   // index 0 is the parent, so shift down by one
    }
    return -1;
}

} // namespace mal
