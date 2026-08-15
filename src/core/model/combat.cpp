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
    // Player starts full, unless a gauntlet carries a wounded Health in (—
    // no heal between rounds). Clamp: never above max, never a non-positive start.
    if (carryPlayerHealth >= 0) {
        player_.health = carryPlayerHealth < player_.maxHealth ? carryPlayerHealth
                                                               : player_.maxHealth;
        if (player_.health < 1) player_.health = 1;
    } else {
        player_.health = player_.maxHealth;
    }
    enemy_.health = enemy_.maxHealth;
    stakes_ = stakes;
    outcome_ = Outcome::Ongoing;
    rng_ = seed ? seed : 1u;
    // Initiative and action frequency are speed-driven by the scheduler seeded below
    // (see pickNextActor); a failed pre-fight flee passes forceEnemyFirst to hand the
    // enemy the opening action.
    // Zero-Day Exploit (mod): a one-time gamble rolled HERE (not makePlayerCombatant) —
    // it needs the fight's seeded RNG, which only exists once begin() sets rng_ above.
    // A miss leaves powerMultPct untouched; a hit lasts the whole fight (never re-rolled,
    // never reset — begin() is only called once per fight/round).
    //
    // Rolled for BOTH sides, player first. Only a mod grants this and only a pet carries
    // mods, so in every PVE fight the enemy's magnitude is 0 and its branch never draws —
    // the roll order and the RNG stream are unchanged there. It matters in a linked duel
    // (core/model/pvp_battle.h), where both combatants are real pets and the one that
    // happens to occupy the enemy_ slot must not silently lose its equipped mod.
    auto rollGamble = [this](Combatant& c) {
        const int pct = c.mods.mag(ModEffect::GambleBattlePowerPct);
        if (pct > 0 && static_cast<int>(rng() % 100) < pct)
            c.powerMultPct += c.mods.mag2(ModEffect::GambleBattlePowerPct);
    };
    rollGamble(player_);
    rollGamble(enemy_);
    // Ransom Note (Ransomware): the window exists from the opening bell, not from the
    // ransomer's first turn — a slow pet that gets hit before it ever acts would otherwise
    // have no passive for the part of the fight it most needs one. Same short-circuit as
    // every other per-line roll: a fight with no Ransomware pet draws nothing here and
    // replays identically. `||` rather than `=` so an already-armed window (a caller that
    // hands one in) is never rolled away, matching resolveTurn's don't-re-roll rule.
    player_.ransomArmed = player_.ransomArmed || ransomArmRolls(player_);
    enemy_.ransomArmed = enemy_.ransomArmed || ransomArmRolls(enemy_);
    // Baseline the attack lean AFTER the one-time gamble, so only a mid-fight Phishing
    // siphon (the only thing that moves powerMultPct after this point) shows up as a
    // delta on the combat screen — the gamble is a fixed start-of-fight bonus, not a
    // live "stat state" change the player needs to watch.
    player_.basePowerMultPct = player_.powerMultPct;
    enemy_.basePowerMultPct = enemy_.powerMultPct;
    // Shared Resources (Worm): the match happens BEFORE the speed baseline is captured,
    // so a worm's own "start of fight" speed is the matched one. The stat panel then
    // shows a delta only when the pair have actually MOVED together mid-fight, rather
    // than reporting the opening handshake as a siphon.
    syncWormSpeed();
    player_.baseSpeed = player_.speed;
    enemy_.baseSpeed = enemy_.speed;
    // ...and the third lean, for the same reason: a Net Neutrality reset has to know
    // what the pet's damage cut was before a siphon or an armour rot went to work on it.
    player_.baseDmgReducePct = player_.dmgReducePct;
    enemy_.baseDmgReducePct = enemy_.dmgReducePct;
    // Seed the speed scheduler with empty gauges, then pick the opening actor by speed.
    // forceEnemyFirst (a failed pre-fight flee) is a hard guarantee: the enemy takes a
    // free opening action regardless of speed, and normal speed scheduling resumes after.
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
    lastWormKill_ = {};
}

void Combat::setLast(const char* name, int dmg, bool byPlayer, bool charge,
                     bool ransomed) {
    lastMoveName_ = name;
    lastDamage_ = dmg;
    lastByPlayer_ = byPlayer;
    lastWasCharge_ = charge;
    lastRansomed_ = ransomed;
}

int Combat::chooseMove(Combatant& self) {
    const int n = static_cast<int>(self.moves.size());
    if (n <= 1) return 0;
    // Uniform over the moves != lastMoveIdx — no-consecutive-repeat. Uniform
    // selection makes the attack/defend lean emerge directly from the slot mix
    // more attack slots → more attack rolls, no separate ratio dial.
    const bool excludeLast = self.lastMoveIdx >= 0 && self.lastMoveIdx < n;
    const int span = excludeLast ? n - 1 : n;
    int target = static_cast<int>(rng() % static_cast<uint32_t>(span));
    int seen = 0;
    for (int i = 0; i < n; ++i) {
        if (excludeLast && i == self.lastMoveIdx) continue;
        if (seen == target) return i;
        ++seen;
    }
    return 0;
}

// Net Neutrality's floor (crew Exploit): whether `c`'s stat LEANS are locked against
// being lowered. Every site below that would REDUCE a combatant's power / defence /
// speed / maxHealth asks this one question first, so the next erosion mechanic has an
// answer waiting rather than needing a case of its own.
//
// Health is deliberately not covered: it is the resource the fight is FOUGHT over, not
// a lean, so a lifesteal drinks from it like any attack would.
static bool statsFloored(const Combatant& c) {
    return c.crewExploit.holds(CrewExploitKind::ResetStatsAndFloor);
}

void Combat::applyEffect(Combatant& actor, Combatant& target, const MoveDef* mv,
                         bool byPlayer, int moveIdx) {
    target.mirrorFired = false;
    // Malbeast In The Middle (crew Exploit): while it holds, every SELF-BUFF the enemy
    // casts is copied onto the player as it lands. Answered once here so each of the
    // four buff sites below costs a single line, and false in every fight that hasn't
    // armed it. `byPlayer` names who is CASTING (the Trojan hijack passes the flipped
    // flag with the swapped roles), so this is the enemy side buffing itself and no
    // other reading.
    //
    // Scoped to self-buffs, not to the steal track: a siphon's gain half is the
    // player's OWN stat changing hands, and copying that back would hand the pet a
    // refund rather than a copy of somebody else's advantage.
    const bool mitmCopy =
        !byPlayer && player_.crewExploit.holds(CrewExploitKind::MirrorEnemyBuffs);
    // Feeding-frenzy combo (Phishing steal-attacks only, mv->stealPowerPct > 0):
    // this actor's OWN run of back-to-back steal-attack casts — a mixed kit that
    // slots in a Defend move (e.g. Spoof-Bubble) between them breaks it, restarting
    // at 1. A continuing run permanently banks (run length - 1) flat damage into
    // phishComboBonus, applied below to every future steal-attack hit this fight —
    // so a short early run adds a sliver, a long one snowballs, and it never decays
    // even after this particular run ends (unlike Combat::streakCount_, the
    // turn-order streak driving the render pace, which resets clean each time).
    if (mv->stealPowerPct > 0) {
        actor.phishStreak++;
        if (actor.phishStreak > 1) actor.phishComboBonus += actor.phishStreak - 1;
    } else {
        actor.phishStreak = 0;
    }
    if (mv->kind == MoveDef::Kind::Attack) {
        // Base damage scaled by the actor's branch attack-power lean PLUS any
        // Lockout-track Power the caster has stacked this fight: Bad-branch
        // petware hits harder, Good-branch softer, a stacked wall harder still. Meltdown
        // Core (mod) adds a further comeback bonus while the actor is critically low.
        int mult = actor.powerMultPct + actor.stackPowerBonus;
        const int meltdownPct = actor.mods.mag(ModEffect::LowHealthPowerPct);
        if (meltdownPct > 0 && actor.maxHealth > 0) {
            const int healthPct = actor.health * 100 / actor.maxHealth;
            if (healthPct <= meltdownPct)
                mult += actor.mods.mag2(ModEffect::LowHealthPowerPct);
        }
        int dmg = mv->power * mult / 100;
        // These moves are deliberately low-power and lean on the min-1 penetration
        // floor rather than raw damage, so this flat bonus (above) is what actually
        // makes a sustained frenzy dangerous instead of a long string of 1s.
        if (mv->stealPowerPct > 0) dmg += actor.phishComboBonus;
        // Worm attacker replicas pile onto their parent's swing (wormReplicaDamage).
        // The parent's own attacks are deliberately weak, so on a Worm this is most of
        // the damage — and it is why the line's threat scales with the BOARD rather than
        // with the move rolled. Added before mitigation, so the pile goes through the
        // target's defence like any other damage. Zero on every non-Worm side.
        dmg += wormReplicaDamage(actor);
        // Wild-encounter challenge buff: explore malbeasts hit
        // harder so a win costs real Health. enemyDamageMultPct is 100 for the
        // player, bosses, and Sim dummies, so this is a no-op off the wild path.
        if (!byPlayer) dmg = dmg * actor.enemyDamageMultPct / 100;
        // Worm replication (target side): an attack aimed at a worm picks its victim
        // among the parent and every live replica, weighted so a defender draws hardest
        // (wormTargetPick / content_passives.h). A replica that catches the hit eats it
        // WHOLE — no mitigation, no riders, no overflow to the parent — and dies if
        // overrun, freeing its slot. That is the passive: replication does not make the
        // worm tougher, it makes the worm harder to actually be the one hit.
        //
        // Rolled only when the target actually has replicas out, so no other line ever
        // perturbs the stream. One rng() draw, resolved by a pure function, so both
        // devices of a duel name the same victim from the same seed.
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
                setLast(mv->displayName, dealt, byPlayer, /*charge=*/false);
                return;
            }
        }
        const int baseDmg = dmg;    // pre-mitigation, for the min-1 penetration floor
        const bool crewNegates =
            target.crewExploit.armed(CrewExploitKind::NegateNextHits);
        const bool mirrorArmed = target.mods.armed(ModEffect::RaidMirror);
        if (dmg > 0 && (crewNegates || mirrorArmed)) {
            // A crew Exploit charge / RAID Mirror each negate the whole hit, whatever
            // its size. The Backup Drive is not part of this: it never looks at hits at
            // all, only at a pet that has already gone down (Combat::checkOutcome).
            dmg = 0;
            if (crewNegates) {
                // Crew charges absorb FIRST: the player spent an Exploit use to arm
                // them this fight, so the passive one-shot below stays held for after
                // the charges run out.
                --target.crewExploit.charges;
            } else {
                target.mods.spend(ModEffect::RaidMirror);
            }
            target.mirrorFired = true;
        } else {
            // Target's effective damage cut = passive (Firewall/Defense) + any
            // Cipher-track Defense it stacked, under the never-immune clamp.
            // MBR Wipe's armorPiercePct then ignores a slice of BOTH the % cut and the
            // one-shot brace (a wiped boot sector doesn't care how hard the disk is).
            int reduce = target.dmgReducePct + target.stackDefenseBonus;
            if (reduce > kLevelDmgReduceMaxPct) reduce = kLevelDmgReduceMaxPct;
            if (mv->armorPiercePct > 0)
                reduce = reduce * (100 - mv->armorPiercePct) / 100;
            if (reduce > 0) dmg = dmg * (100 - reduce) / 100;
            // Canary Trap (mod): an EXTRA cut on the first hit this pet takes, stacked
            // on top of the normal reduce above (outside the 85% clamp, and not armor-
            // pierced — the decoy absorbs regardless of the attacker's pierce). Consumed
            // only once an actual hit lands (dmg > 0 here), so a hit already zeroed by
            // reduce/guard elsewhere doesn't burn it before a real one arrives.
            if (ModState* canary = target.mods.find(ModEffect::FirstHitCutPct);
                dmg > 0 && canary && canary->mag > 0 && canary->pending > 0) {
                dmg = dmg * (100 - canary->mag) / 100;
                if (dmg < 0) dmg = 0;
                --canary->pending;
            }
            if (target.guard > 0) {                   // a defend brace (one-shot)
                int brace = target.guard;
                if (mv->armorPiercePct > 0)
                    brace = brace * (100 - mv->armorPiercePct) / 100;
                dmg = dmg > brace ? dmg - brace : 0;
                target.guard = 0;
            }
            // Minimum penetration: a real attack ALWAYS lands at least 1 through pure
            // defensive mitigation (% cut + one-shot guard brace), so no pet becomes an
            // invincible wall a weak/fast attacker can never chip (matters once speed
            // drives an action economy). Scoped to the mitigation branch so RAID Mirror's
            // deliberate full negation (its own branch) still zeroes a hit; the shield
            // pool below still absorbs this 1 (a consumable pool, not a wall).
            if (baseDmg > 0 && dmg < 1) dmg = 1;
        }
        if (dmg < 0) dmg = 0;
        // Prowlware (mod): the FIRST landed damaging hit this fight is multiplied by the
        // used move's Attack-power rank (attackPowerRank). Consumed only on an actual
        // damaging hit (dmg > 0 here, i.e. AFTER mirror/reduce/guard) so a mirrored or
        // fully-mitigated hit doesn't burn it before a real one lands.
        if (dmg > 0 && actor.mods.spend(ModEffect::FirstStrikeRankMult)) {
            const int rank = attackPowerRank(actor.moves, moveIdx);
            if (rank > 1) dmg *= rank;
        }
        // ECC Memory (mod): cap any single incoming hit — after ALL mitigation, so it's a
        // last-resort ceiling on burst (channel detonations, high-power moves). Thorns/
        // Deadman below read the capped value; a fully-mirrored hit (dmg 0) is untouched.
        const int hitCap = target.mods.mag(ModEffect::MaxHitCapPct);
        if (hitCap > 0 && dmg > hitCap) dmg = hitCap;
        // Load Balancer (mod): a big hit (>= threshold) is SPLIT — splitPct% is DEFERRED to
        // the victim's next turn-start (resolveTurn), the rest lands now. It does NOT reduce
        // total damage (unlike ECC), only spreads it — buying a turn to heal / land a KO
        // first; if the debt isn't outrun it lands next turn (can KO on its own). Runs AFTER
        // the ECC cap so the two compose (cap the burst, then spread the remainder). Thorns/
        // Deadman below read the IMMEDIATE portion (so a split hit may not KO now, but the
        // deferred tick can — checked at turn-start where checkOutcome() then catches it).
        if (ModState* lb = target.mods.find(ModEffect::LoadBalance);
            lb && lb->mag > 0 && dmg >= lb->mag && lb->mag2 > 0) {
            const int deferred = dmg * lb->mag2 / 100;
            if (deferred > 0) {
                lb->pending += deferred;       // comes due at the victim's next turn-start
                dmg -= deferred;
            }
        }
        // Obfuscation shield pool (Phishing, poolable second health bar): the shield
        // eats damage first; only the OVERFLOW reaches real Health, and the pool pops
        // (clamps to 0) when overrun. Sits last in the mitigation chain (after % cut /
        // guard / ECC / Load Balancer) so it's the final wall before Health. A hit
        // fully soaked by the shield (dmg -> 0) triggers no on-hit rider below — the
        // same "no damage landed" semantics guard already gives.
        if (dmg > 0 && target.shieldHp > 0) {
            if (target.shieldHp >= dmg) { target.shieldHp -= dmg; dmg = 0; }
            else { dmg -= target.shieldHp; target.shieldHp = 0; }
        }
        // Trojan trap (Trojan line): an incoming attack springs the top armed trap. It
        // deletes trapEvasionPct% of the hit (the survival-in-lieu-of-healing tool),
        // reflects trapReboundPct% of everything the Trojan avoided back through the
        // attacker's CURRENT (rotting) defense — so rebound grows as armor rots — then
        // strips trapArmorRot flat % Defense off the attacker for the rest of the fight.
        // Uses no rng(), so a non-Trojan fight (trojanTrapCount always 0) is untouched.
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
        // Ransom Note (Ransomware passive): while the target's ransom window is armed
        // (rolled at its own turn-start, resolveTurn), the DAMAGE this hit would deal is
        // held hostage instead of taken — banked into ransomPool, with the countdown
        // pushed back out to kRansomHoldTurns of the target's own turns. Last in the
        // chain, so the pool holds exactly what would otherwise have reached Health.
        //
        // Only the number is deferred. `dmg` below still describes a hit that LANDED, so
        // every rider (thorns, the steal track, the stack/stun/DoT riders) fires on impact
        // as it always did — the pet never has to track a held DoT or a held armor shred,
        // and a KO check (Deadman Switch) correctly sees Health that hasn't moved.
        int ransomed = 0;
        if (dmg > 0 && target.ransomArmed) {
            ransomed = dmg;
            target.ransomPool += dmg;
            target.ransomTurnsLeft = kRansomHoldTurns;
            target.ransomArmed = false;   // the window closes on the hit it catches
        }
        // Health is left UNCLAMPED here (and at every other site below that spends it):
        // Combat::checkOutcome owns the floor, because how far past 0 a hit buried the
        // pet is exactly what the Backup Drive's death-save has to weigh before that
        // floor erases it.
        target.health -= dmg - ransomed;
        // Honeytoken (mod): a landed hit chips the ATTACKER back. Mods are player-side,
        // so it only ever reflects onto an enemy that hit the pet — never the other way.
        // Doesn't fire on a fully-mirrored hit.
        const int thorns = target.mods.mag(ModEffect::Thorns);
        if (dmg > 0 && thorns > 0) {
            actor.health -= thorns;
        }
        // Tripwire (mod): same reflect shape as Honeytoken, but only while the pet is
        // critically low — its own ModEffect kind, so it never mixes its conditional
        // accumulation with an always-on Thorns mod equipped alongside it.
        const int condThorns = target.mods.mag(ModEffect::ConditionalThorns);
        if (dmg > 0 && condThorns > 0 && target.maxHealth > 0) {
            const int hpPct = target.health * 100 / target.maxHealth;
            if (hpPct <= target.mods.mag2(ModEffect::ConditionalThorns)) {
                actor.health -= condThorns;
            }
        }
        // Steal track (defs.h has the per-field detail): every non-zero MoveDef
        // steal* field fires on a landed hit, independently and deterministically (no
        // random pick — a move that sets more than one steals more than one).
        // stealPowerPct/stealDefensePct are unconditional; stealSpeedPct/
        // stealCurrentHpPct additionally require the caster's Obfuscation shield to be
        // up (shieldHp > 0) — the Phishing interplay this track is built around: going
        // aggressive with the volatile pair costs the defensive bubble's uptime.
        //
        // A target whose stats are FLOORED (Net Neutrality) has nothing here to give:
        // every branch below is a TRANSFER, so a steal that may not lower the victim
        // does not pay the thief either — the siphon simply comes back empty. That is
        // checked per-branch rather than around the block because stealCurrentHpPct is
        // in here too, and Health is not one of the floored leans.
        if (dmg > 0) {
            const bool floored = statsFloored(target);
            bool powerSiphoned = false;
            if (mv->stealPowerPct > 0 && !floored) {
                const int stolen = target.powerMultPct * mv->stealPowerPct / 100;
                if (stolen > 0) {
                    actor.powerMultPct += stolen;
                    target.powerMultPct -= stolen;
                    if (target.powerMultPct < kStealPowerFloorPct)
                        target.powerMultPct = kStealPowerFloorPct;
                    powerSiphoned = true;
                }
            }
            if (mv->stealDefensePct > 0 && !floored) {
                const int stolen = target.dmgReducePct * mv->stealDefensePct / 100;
                if (stolen > 0) {
                    actor.dmgReducePct += stolen;
                    target.dmgReducePct -= stolen;
                    if (target.dmgReducePct < 0) target.dmgReducePct = 0;
                }
            }
            // Perfect Bite: rolled ONCE per hit, only when this move actually carries a
            // bubble-gated field AND the bubble is up — a fight with neither draws no
            // rng() at all, so replays without the track stay identical. When a move
            // sets BOTH gated fields, a second roll picks which one the bite doubles.
            const bool bubbleUp = actor.shieldHp > 0;
            const bool bubbleGatedMove = mv->stealSpeedPct > 0 || mv->stealCurrentHpPct > 0;
            bool bite = false, biteHitsSpeed = mv->stealSpeedPct > 0;
            if (bubbleUp && bubbleGatedMove) {
                bite = bubbleBiteRolls(actor.stage);
                if (bite && mv->stealSpeedPct > 0 && mv->stealCurrentHpPct > 0)
                    biteHitsSpeed = (rng() % 2 == 0);
            }
            if (bubbleUp && mv->stealSpeedPct > 0 && !floored) {
                int pct = mv->stealSpeedPct;
                // The Phishing Rod only ever scales the BITE half — it amplifies the
                // bonus, not the move's own base siphon.
                if (bite && biteHitsSpeed)
                    pct += mv->stealSpeedPct *
                           (100 + actor.mods.mag(ModEffect::StealAmplifyPct)) / 100;
                // FLOAT, not int: a percentage of the target's CURRENT (already-
                // siphoned) speed — with an int this truncated to 0 the moment speed
                // neared the floor, making repeat speed steals a dead branch in
                // practice. Float precision keeps every landed siphon meaningful.
                const float stolen = target.speed * pct / 100.0f;
                if (stolen > 0.0f) {
                    actor.speed += stolen;
                    target.speed -= stolen;
                    if (target.speed < kStealSpeedFloor) target.speed = kStealSpeedFloor;
                }
            }
            if (bubbleUp && mv->stealCurrentHpPct > 0) {
                int pct = mv->stealCurrentHpPct;
                if (bite && !biteHitsSpeed)
                    pct += mv->stealCurrentHpPct *
                           (100 + actor.mods.mag(ModEffect::StealAmplifyPct)) / 100;
                const int stolen = target.health * pct / 100;   // lifesteal: target's
                if (stolen > 0) {                               // CURRENT health drains
                    target.health -= stolen;                    // straight to the caster
                    actor.health += stolen;
                    if (actor.health > actor.maxHealth) actor.health = actor.maxHealth;
                }
            }
            if (mv->stealMaxHpPct > 0 && !floored) {
                const int stolen = target.maxHealth * mv->stealMaxHpPct / 100;
                if (stolen > 0 && target.maxHealth - stolen >= 1) {
                    target.maxHealth -= stolen;                 // permanent for the fight
                    if (target.health > target.maxHealth) target.health = target.maxHealth;
                    actor.maxHealth += stolen;
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
        // Deadman Switch (mod): if this hit KO'd the pet, it deals a parting blast to the
        // enemy. A mutual KO resolves as a Win (enemy-death priority, checkOutcome) — the
        // clutch payoff of a rare Epic. One shot: cleared so a gauntlet round can't reuse it.
        if (ModState* dead = target.mods.find(ModEffect::DeathBlast);
            target.health <= 0 && dead && dead->mag > 0 && dead->pending > 0) {
            actor.health -= dead->mag;
            --dead->pending;
        }
        setLast(mv->displayName, dmg, byPlayer, false, ransomed > 0);
        // Escalation (crew Exploit): each of the next few LANDED attacks banks its own
        // FINAL damage — after every mitigation, and including a hit the target's ransom
        // pool merely held — as Power for the rest of the fight. Charge-metered, so it
        // spends itself on hits that connected rather than on turns that happened, and
        // uncapped, unlike the Lockout track below: the crew's whole personality is that
        // each charge pays for the bigger swing the next one banks.
        if (dmg > 0 && actor.crewExploit.armed(CrewExploitKind::PowerByDamageDealt)) {
            actor.stackPowerBonus += dmg;
            --actor.crewExploit.charges;
        }
        // Lockout track: landing the hit stacks the caster's Power for the rest
        // of the fight — additive, never reset, capped per move.
        if (mv->stackPowerPct > 0 && actor.stackPowerBonus < mv->stackPowerCap) {
            int gain = mv->stackPowerPct;
            if (actor.stackPowerBonus + gain > mv->stackPowerCap)
                gain = mv->stackPowerCap - actor.stackPowerBonus;
            actor.stackPowerBonus += gain;
            // ...and the copy Malbeast In The Middle takes of it. The cap belongs to the
            // enemy's move and is measured against the enemy's own pile, so it bounds
            // what there is to copy rather than what the player may hold.
            if (mitmCopy) player_.stackPowerBonus += gain;
        }
        // STUN rider (Watchdog-pass THREAT): a landed hit freezes the target's next
        // lockTurns turns. The target's Watchdog Timer (mod) CLAMPS it (a hung process
        // reboots after its clamp's turns). Doesn't stack onto a live stun; a fully-
        // mirrored hit (RAID Mirror negated the whole attack) carries no rider.
        if (mv->lockTurns > 0 && !target.mirrorFired && target.lockedTurnsLeft == 0) {
            int k = mv->lockTurns;
            const int watchdog = target.mods.mag(ModEffect::WatchdogClamp);
            if (watchdog > 0 && k > watchdog) k = watchdog;
            if (k > 0) target.lockedTurnsLeft = k;
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
        // Defend: brace against the next incoming hit. Brace magnitude scales
        // with the caster's Defense stat (defenseMultPct) — leveling Defense thickens
        // the wall's absorb, symmetric to Power→attack.
        //
        // Obfuscation track (Phishing, MoveDef::shieldPool): a shield-pool move POOLS
        // additively into shieldHp (a second health bar — recasting stacks it, and the
        // attack path overflows to Health only once it pops) instead of the one-shot
        // guard brace. Every other Defend move keeps the one-shot guard.
        //
        // Each gain is computed once and then handed to Malbeast In The Middle as well
        // (mitmCopy) — the enemy's brace becomes the player's brace, its pool the
        // player's pool. The copy is additive on whatever the player already had, so a
        // pet that braced this turn keeps its own on top of the one it stole.
        const int braced = mv->power * actor.defenseMultPct / 100;
        if (mv->shieldPool > 0) {
            actor.shieldHp += braced;
            if (mitmCopy) player_.shieldHp += braced;
        } else {
            actor.guard += braced;
            if (mitmCopy) player_.guard += braced;
        }
        // Cipher track: the cast stacks the caster's Defense (% cut) for the
        // fight, capped per move; the attack path clamps the total to 85% (never immune).
        if (mv->stackDefensePct > 0 && actor.stackDefenseBonus < mv->stackDefenseCap) {
            int gain = mv->stackDefensePct;
            if (actor.stackDefenseBonus + gain > mv->stackDefenseCap)
                gain = mv->stackDefenseCap - actor.stackDefenseBonus;
            actor.stackDefenseBonus += gain;
            if (mitmCopy) player_.stackDefenseBonus += gain;
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

bool Combat::ransomArmRolls(const Combatant& c) {
    // The Ransom Note passive belongs to whichever line carries the flag, scaled by the
    // ransomer's stage. The passive check short-circuits BEFORE any rng() draw, so a side
    // without it never perturbs the deterministic stream (a fight with no Ransom Note
    // pet in it replays identically). Boot's 0% is inert (an egg can't fight).
    if (!hasLinePassive(c.linePassives, LinePassive::RansomNote)) return false;
    const int si = stageIndex(c.stage);
    const int pct = (si >= 0 && si < 4) ? kRansomArmPctByStage[si] : 0;
    if (pct <= 0) return false;
    return static_cast<int>(rng() % 100) < pct;
}

bool Combat::bubbleBiteRolls(Stage stage) {
    // Perfect Bite (Phishing steal track): the caller (applyEffect) already checked
    // the bubble is up and the move carries a gated field, so this is purely the
    // stage-scaled chance — short-circuits before the rng() draw at 0% (Boot) so a
    // fight that never reaches the gated condition never perturbs the stream.
    const int si = stageIndex(stage);
    const int pct = (si >= 0 && si < 4) ? kPhishingBiteChancePctByStage[si] : 0;
    if (pct <= 0) return false;
    return static_cast<int>(rng() % 100) < pct;
}

// Whether a side replicates — the Worm line's passive family today. Kept in one place
// because three hooks below ask; each reads the flag its combatant was built with.
static bool replicates(const Combatant& c) {
    return hasLinePassive(c.linePassives, LinePassive::Replication);
}

void Combat::syncWormSpeed() {
    // Shared Resources: the worm's speed IS the opponent's, so the relative speed that
    // deals actions (pickNextActor) is always 1:1 and nothing out-actions a worm.
    //
    // Exactly one worm, or nothing happens. With none there is nobody to match; with two
    // (a duel of worms) each would be assigned the other's value and the pair would swap
    // speeds every tick forever — and they are already in lockstep by definition, since
    // whatever moves one moves the other. Leaving both alone is that fact stated.
    const bool pw = replicates(player_), ew = replicates(enemy_);
    if (pw == ew) return;
    if (pw) player_.speed = enemy_.speed;
    else    enemy_.speed = player_.speed;
}

void Combat::rollWormSpawn(Combatant& actor, const MoveDef* mv) {
    // Slots are the hard cap on replication (kWormReplicaSlots), and a full board simply
    // doesn't roll — so a worm holding three replicas draws no rng here and the stream
    // stays identical to one that never had the chance.
    if (actor.wormReplicaCount >= kWormReplicaSlots) return;
    if (static_cast<int>(rng() % 100) >= mv->replicaSpawnPct) return;
    WormReplica& r = actor.wormReplicas[actor.wormReplicaCount];
    r = WormReplica{};
    r.defender = mv->kind == MoveDef::Kind::Defend;
    if (r.defender) {
        // A defender is a body: real Health, no swing of its own. Its size is banked here
        // from the attackers already out (wormDefenderHealth) — spawn order is the
        // decision the player is making.
        r.maxHealth = wormDefenderHealth(actor, mv->replicaHealthPct);
        r.health = r.maxHealth;
    } else {
        // An attacker is thin — one hit takes it — and pays its way by piling onto the
        // parent's swings (wormReplicaDamage). Its base is a share of the move that made
        // it, scaled by the same attack lean the parent's own damage is scaled by, so a
        // Bad-branch worm's copies hit like it does.
        r.maxHealth = 1;
        r.health = 1;
        r.attack = mv->power * mv->replicaPowerPct / 100 * actor.powerMultPct / 100;
        if (r.attack < 1) r.attack = 1;
    }
    ++actor.wormReplicaCount;
}

int Combat::execOverrideChance(const Combatant& trojan) const {
    // The Execution-Override passive belongs to whichever line carries the flag. The
    // check short-circuits to 0 BEFORE any rng() draw at the call site, so a pet without
    // it never perturbs the deterministic stream. Base chance is low; each armed trap
    // adds its trapPassiveBonusPct, so holding all three traps makes the hijack likely.
    if (!hasLinePassive(trojan.linePassives, LinePassive::ExecOverride)) return 0;
    int pct = kExecOverrideBasePct;
    for (int i = 0; i < trojan.trojanTrapCount; ++i)
        if (trojan.trojanTraps[i]) pct += trojan.trojanTraps[i]->trapPassiveBonusPct;
    return pct;
}

void Combat::resolveTurn(Combatant& actor, Combatant& target, bool byPlayer) {
    // Load Balancer (mod): the deferred half of an earlier big hit comes due at the
    // start of the victim's turn. Applied BEFORE anything else so a fatal debt ends the turn
    // (the actor doesn't get to act) — checkOutcome() after step() then reads the KO. Only
    // the player ever carries deferred damage (mods are player-side); an actor with none
    // (every enemy, most turns) skips this entirely.
    if (ModState* lb = actor.mods.find(ModEffect::LoadBalance); lb && lb->pending > 0) {
        const int due = lb->pending;
        lb->pending = 0;
        actor.health -= due;
        if (actor.health <= 0) {                          // the debt came due, fatally
            setLast("OVERLOAD", due, byPlayer, /*charge=*/false);
            return;
        }
        // Survived it: fall through and act normally (the tick folds silently into this
        // turn — the actual move's popup below overwrites any transient marker anyway).
    }

    // DoT (Faraday-pass THREAT): the corruption an earlier hit planted bites at the start of
    // every one of the victim's turns, independent of any lock (a frozen process still rots).
    // Ticks before the lock burn so a fatal DoT ends the turn even while stunned. Faraday Cage
    // (mod) already cut/negated the magnitude when the DoT was APPLIED (applyEffect).
    if (actor.dotTurnsLeft > 0 && actor.dotPerTurn > 0) {
        actor.dotTurnsLeft--;
        actor.health -= actor.dotPerTurn;
        if (actor.health <= 0) {                          // rotted to death
            setLast("CORRUPTED", actor.dotPerTurn, byPlayer, /*charge=*/false);
            return;
        }
        // Survived the tick: fall through (folds into this turn, like the deferred tick).
    }

    // Ransom Note (Ransomware passive), the WINDOW half: one roll, here, decides whether
    // this side is holding a ransom window open. Rolling per TURN rather than per incoming
    // hit is what keeps a linked duel in step — the window is fixed by the seed before the
    // turn plays out, so it can't depend on how the opponent's speed happens to deal
    // actions inside it. Rolled even on a turn spent paying or stunned.
    //
    // An armed window STAYS open until a hit actually lands in it (applyEffect closes it),
    // rather than lapsing with the turn. A lapsing window is invisible — most fights are
    // short and land only a couple of hits on the pet, so the great majority of successful
    // rolls would catch nothing and the passive would read as not firing at all. Holding it
    // open means a roll that succeeds always gets paid, and the chance below sets how often
    // a hit is ransomed rather than how often a turn is.
    if (!actor.ransomArmed) actor.ransomArmed = ransomArmRolls(actor);

    // ...and the BILL half. The countdown burns one of the RANSOMER's own turns per tick —
    // not one per incoming action — so a fast opponent buys itself more hits inside the
    // window rather than a faster payout. At zero the whole pool lands in one blow, which
    // can absolutely be the thing that kills: that cliff is the price of every hit the pet
    // got to ignore on the way here.
    //
    // Paying COSTS the turn (unlike the DoT/deferred ticks above, which fold silently into
    // it): the pool's whole point is arriving as one legible blow, and a tick that fell
    // through would have its popup overwritten by the same turn's move. It is also the
    // passive's price — the pet bought turns of fighting at untouched Health and settles
    // up with one. Ahead of the stun below, so a frozen pet still pays on schedule.
    if (actor.ransomTurnsLeft > 0 && --actor.ransomTurnsLeft == 0 && actor.ransomPool > 0) {
        const int due = actor.ransomPool;
        actor.ransomPool = 0;
        actor.health -= due;
        setLast("RANSOM DUE", due, byPlayer, /*charge=*/false);
        return;
    }

    // STUN (Watchdog-pass THREAT): a landed hit's lockTurns rider freezes `actor` for a
    // few of its own turns — a pure skip, no delayed fire. The burned turns are lost.
    if (actor.lockedTurnsLeft > 0) {
        actor.lockedTurnsLeft--;
        setLast("STUN LOCK", 0, byPlayer, /*charge=*/true);
        return;
    }

    int moveIdx;
    if (byPlayer && forcedMoveIdx_ >= 0 &&
        forcedMoveIdx_ < static_cast<int>(actor.moves.size())) {
        // The committed override commands the next move — it ignores the lean AND
        // the no-consecutive rule (so it can chain the same move twice), and it
        // resets any in-progress channel (S41b).
        moveIdx = forcedMoveIdx_;
        forcedMoveIdx_ = -1;
        actor.channelMoveIdx = -1;
        actor.channelLeft = 0;
    } else if (actor.channelMoveIdx >= 0) {
        moveIdx = actor.channelMoveIdx;               // committed mid-channel
    } else {
        moveIdx = chooseMove(actor);
    }
    const MoveDef* mv = actor.moves[moveIdx];

    // Execution-Override (Trojan passive): a Trojan (`target`, the side not acting)
    // hijacks the actor's freshly-picked move and runs it back AT them, consuming their
    // turn. execOverrideChance is 0 for a non-Trojan target, so rng() is only drawn when
    // the passive is live (a Ransomware/Phishing fight replays identically). moveIdx = -1
    // so the hijacked cast doesn't index the Trojan's own mod state (e.g. Prowlware).
    //
    // Side-agnostic for the same reason as the Ransom Lock above: no PVE enemy carries a
    // `line`, so this only ever fires for a player Trojan there. In a duel a Trojan on
    // either side gets its passive.
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

    applyEffect(actor, target, mv, byPlayer, moveIdx);
    // Shared Resources (Worm), the replication half: a cast carrying replicaSpawnPct
    // rolls for a copy once it has RESOLVED, so a fresh replica joins the next swing
    // rather than the one that spawned it. One site for both kinds — the move's own
    // `kind` picks which sort appears (rollWormSpawn) — and it still runs on a swing
    // that was redirected into a replica, which is a cast like any other.
    //
    // Deliberately not reachable from the Execution-Override branch above: a hijacked
    // cast is the Trojan's, and a Trojan is not a Worm, so a stolen move replicates for
    // nobody. Guarded on the field AND the line, so no other line draws rng here.
    if (mv->replicaSpawnPct > 0 && replicates(actor)) rollWormSpawn(actor, mv);
    actor.lastMoveIdx = moveIdx;
}

void Combat::checkOutcome() {
    // The one place a pet is judged overwhelmed, and so the one place the Backup Drive's
    // death-save runs. Everything that spends Health leaves it unclamped and this is
    // where it lands, which means the save reads the pet's STATE and never the thing
    // that got it there — a hit, a rotting DoT, a ransom bill coming due and whatever
    // gets added next all arrive here the same way, with no branch of their own.
    //
    // Backup Plan B (crew Exploit) looks at the hole FIRST, for the same reason its
    // crew-mate's negation charges absorb ahead of the RAID Mirror: the player SPENT an
    // Exploit use on this, so a granted one-shot stays held for a later hole. It
    // restores TO half of max (the drive below ADDS half to wherever the pet ended up),
    // and pays the overkill — how far past 0 the blow buried it — back as Power,
    // multiplied by the turns still on its clock. So it is worth most the moment it is
    // armed, and a bigger blow rallies harder: the hit that should have ended the fight
    // is the one that funds the rest of it. One shot, consumed when it fires.
    if (player_.health <= 0 &&
        player_.crewExploit.ticking(CrewExploitKind::DeathSaveRally)) {
        const int overkill = -player_.health;
        player_.health = player_.maxHealth / 2;
        if (player_.health < 1) player_.health = 1;   // a 1-HP pet still gets back up
        player_.stackPowerBonus += overkill * player_.crewExploit.turns;
        player_.crewExploit = CrewExploitState{};
    }
    if (player_.health <= 0) player_.restoreFromBackup();
    // ...and the floor, after the save has had its look at how deep the hole is.
    if (player_.health < 0) player_.health = 0;
    if (enemy_.health < 0) enemy_.health = 0;
    if (enemy_.health <= 0) outcome_ = Outcome::Win;        // win takes priority
    else if (player_.health <= 0) outcome_ = Outcome::Lose;
}

bool Combat::pickNextActor() {
    // Shared Resources (Worm): match a worm's speed to its opponent's before the gauges
    // read it, at EVERY scheduling tick — so a mid-fight change on either side (a
    // Phishing siphon, a Clock-Speed mod) is matched the instant it lands rather than
    // leaving the worm behind until the next fight. No-op without exactly one worm.
    syncWormSpeed();
    // Fill both gauges by their live speed (min 1, so a fully-siphoned pet still acts
    // eventually and the loop always terminates) until a gauge reaches the threshold;
    // that side takes the action and spends one threshold's worth, leaving the other's
    // carry to build toward its next turn. A tie goes to the player. Equal speed makes
    // both cross together every threshold and alternate; a speed edge lets the faster
    // side cross more often, so actions are dealt in proportion to relative speed.
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
    // A destroyed replica is news about THIS turn only, so it is cleared before the turn
    // rather than accumulated — the same one-turn lifetime lastDamage_ and friends have.
    lastWormKill_ = {};
    // Feeding-frenzy streak: continues the running count if the same side is acting
    // again, else restarts it at 1 for the new actor. Computed BEFORE resolveTurn so
    // applyEffect (the Phishing combo bonus) sees this hit's own place in the run.
    if (streakCount_ > 0 && playerTurn_ == streakIsPlayer_) ++streakCount_;
    else { streakCount_ = 1; streakIsPlayer_ = playerTurn_; }
    const bool enemyActed = !playerTurn_;
    if (playerTurn_) resolveTurn(player_, enemy_, /*byPlayer=*/true);
    else resolveTurn(enemy_, player_, /*byPlayer=*/false);
    checkOutcome();
    if (enemyActed) tickCrewExploitClock();
    playerTurn_ = pickNextActor();   // schedule the next actor by relative speed
    return true;
}

void Combat::tickCrewExploitClock() {
    // Backup Plan B is a death-save, so its clock is the INCOMING turns it covers
    // rather than the pet's own — the three turns it promises are the next three the
    // malbeast gets, which is the only count that describes what the player is buying.
    // (Every other turn-metered thing here — a DoT, a stun, a ransom bill — ticks on
    // its victim's turns instead, because those are things the victim is living
    // THROUGH rather than a guard standing over it.)
    //
    // A turn the enemy spent stunned, rotting or paying its own ransom still counts:
    // it was a turn, and the caller ticks after the whole turn has resolved, so every
    // early return inside resolveTurn is counted the same as a swing.
    if (player_.crewExploit.ticking(CrewExploitKind::DeathSaveRally))
        --player_.crewExploit.turns;
}

int Combat::overrideMoveCount() const {
    return static_cast<int>(player_.moves.size());
}

void Combat::openOverride(std::vector<OverrideItem> items, CrewExploit crew) {
    if (overrideUsesLeft_ <= 0 || outcome_ != Outcome::Ongoing) return;
    overrideItems_ = std::move(items);
    crewExploit_ = crew;
    overrideOpen_ = true;
    overridePick_ = 0;
}

void Combat::cycleOverride() {
    if (!overrideOpen_) return;
    const int n = overrideMoveCount() + static_cast<int>(overrideItems_.size()) +
                  overrideCrewRows();
    if (n <= 0) return;
    overridePick_ = (overridePick_ + 1) % n;
}

void Combat::applyCrewExploit() {
    // Each kind meters itself out of the shared CrewExploitState counters; re-arming
    // the same kind stacks. Adding an ability is a case here plus its crewExploitTag().
    // A player belongs to one crew, so `kind` is only ever set to what it already is.
    if (crewExploit_.kind == CrewExploitKind::None) return;
    player_.crewExploit.kind = crewExploit_.kind;
    switch (crewExploit_.kind) {
        case CrewExploitKind::NegateNextHits:
        case CrewExploitKind::PowerByDamageDealt:
            player_.crewExploit.charges += crewExploit_.magnitude;
            break;
        case CrewExploitKind::DeathSaveRally:
            player_.crewExploit.turns += crewExploit_.magnitude;
            break;
        case CrewExploitKind::ResetStatsAndFloor:
            // Snap the three live stat LEANS back to what the pet walked in with — in
            // BOTH directions, so this is a decision about timing rather than a free
            // top-up: fire it drained and it restores, fire it buffed and it costs. The
            // floor (statsFloored) then holds them there for the rest of the fight.
            //
            // Two things it deliberately leaves alone. The earned line-stack tracks
            // (Lockout Power, Cipher Defense) are not leans — they are progress the pet
            // cast for, and nothing erodes them, so there is nothing to reset. And
            // maxHealth already drunk from by a steal is not given back: a pool that has
            // been drained cannot be un-drained, and the floor only stops the next sip.
            player_.powerMultPct = player_.basePowerMultPct;
            player_.speed = player_.baseSpeed;
            player_.dmgReducePct = player_.baseDmgReducePct;
            break;
        case CrewExploitKind::MirrorEnemyBuffs:
            break;      // sticky: being armed IS the effect (applyEffect's mitmCopy)
        case CrewExploitKind::None:
            return;
    }
    // "<TAG> xN" popup — or a bare "<TAG>" for a sticky kind, which counts nothing.
    // dmg=0 so the combat screen's red damage number stays hidden; the readout rides in
    // the move-name slot, and the live counter also shows in the B stat panel — all
    // three go through crewExploitLabel().
    crewExploitLabel(itemPopup_, sizeof(itemPopup_), crewExploit_.kind,
                     crewExploit_.magnitude);
    setLast(itemPopup_, 0, /*byPlayer=*/true, /*charge=*/false);
}

void Combat::commitOverride() {
    if (!overrideOpen_) return;
    const int moves = overrideMoveCount();
    const int items = static_cast<int>(overrideItems_.size());
    if (overridePick_ < moves) {
        forcedMoveIdx_ = overridePick_;         // forces the player's next move
    } else if (overridePick_ >= moves + items) {
        applyCrewExploit();                     // the crew row (last band)
    } else {
        // USE ITEM: patch transient Health here (combat state); the Game consumes
        // the stack + applies the item's own effect off takeCommittedItem().
        const OverrideItem& it = overrideItems_[overridePick_ - moves];
        player_.health += it.heal;
        if (player_.health > player_.maxHealth) player_.health = player_.maxHealth;
        committedItemId_ = it.id;
        // "PATCH +N" popup. dmg=0 so the combat screen's red "-N" damage number
        // stays hidden (a heal isn't damage); the +N rides in the move name.
        // lastMoveName_ borrows this member buffer, so it must outlive the call —
        // it lives on the Combat object.
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
        tickCrewExploitClock();   // the free turn is a turn, and costs the clock one
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
    // Half of max is what the backup holds — added to where the pet actually ended up,
    // not restored TO a fixed level. A pet buried deeper than that is past what a
    // restore can bring back, and stays down; the drive is spent either way, which is
    // the difference the two used states record.
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

// The multiplier one kind of replica takes from the other's live count.
static int wormCrossMult(int otherKindCount) {
    return otherKindCount < kWormReplicaMultFloor ? kWormReplicaMultFloor : otherKindCount;
}

int wormReplicaDamage(const Combatant& c) {
    const int mult = wormCrossMult(wormReplicaCount(c, /*defenders=*/true));
    int total = 0;
    for (int i = 0; i < c.wormReplicaCount; ++i)
        if (!c.wormReplicas[i].defender) total += c.wormReplicas[i].attack * mult;
    return total;
}

int wormDefenderHealth(const Combatant& parent, int pct) {
    const int mult = wormCrossMult(wormReplicaCount(parent, /*defenders=*/false));
    const int hp = parent.maxHealth * pct / 100 * mult;
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
