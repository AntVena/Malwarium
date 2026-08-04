#include "core/model/combat.h"

#include <cstdio>
#include <cstring>
#include <utility>

#include "tunables.h"
#include "core/content/areas/area_defs.h"
#include "core/content/areas/deepweb_dive/area.h"
#include "core/content/registry.h"
#include "core/model/loadout.h"
#include "core/model/move_loadout.h"

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
    player_.baseSpeed = player_.speed;
    enemy_.baseSpeed = enemy_.speed;
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

void Combat::applyEffect(Combatant& actor, Combatant& target, const MoveDef* mv,
                         bool byPlayer, int moveIdx) {
    target.mirrorFired = false;
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
        // Wild-encounter challenge buff: explore malbeasts hit
        // harder so a win costs real Health. enemyDamageMultPct is 100 for the
        // player, bosses, and Sim dummies, so this is a no-op off the wild path.
        if (!byPlayer) dmg = dmg * actor.enemyDamageMultPct / 100;
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
            if (trap->trapArmorRot > 0) {                  // rot armor for next time
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
        if (dmg > 0) {
            bool powerSiphoned = false;
            if (mv->stealPowerPct > 0) {
                const int stolen = target.powerMultPct * mv->stealPowerPct / 100;
                if (stolen > 0) {
                    actor.powerMultPct += stolen;
                    target.powerMultPct -= stolen;
                    if (target.powerMultPct < kStealPowerFloorPct)
                        target.powerMultPct = kStealPowerFloorPct;
                    powerSiphoned = true;
                }
            }
            if (mv->stealDefensePct > 0) {
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
            if (bubbleUp && mv->stealSpeedPct > 0) {
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
            if (mv->stealMaxHpPct > 0) {
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
        // Lockout track: landing the hit stacks the caster's Power for the rest
        // of the fight — additive, never reset, capped per move.
        if (mv->stackPowerPct > 0 && actor.stackPowerBonus < mv->stackPowerCap) {
            actor.stackPowerBonus += mv->stackPowerPct;
            if (actor.stackPowerBonus > mv->stackPowerCap)
                actor.stackPowerBonus = mv->stackPowerCap;
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
        if (mv->shieldPool > 0)
            actor.shieldHp += mv->power * actor.defenseMultPct / 100;
        else
            actor.guard += mv->power * actor.defenseMultPct / 100;
        // Cipher track: the cast stacks the caster's Defense (% cut) for the
        // fight, capped per move; the attack path clamps the total to 85% (never immune).
        if (mv->stackDefensePct > 0 && actor.stackDefenseBonus < mv->stackDefenseCap) {
            actor.stackDefenseBonus += mv->stackDefensePct;
            if (actor.stackDefenseBonus > mv->stackDefenseCap)
                actor.stackDefenseBonus = mv->stackDefenseCap;
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
    // The Ransom Note passive belongs to the Ransomware line only, scaled by the
    // ransomer's stage. The line check short-circuits BEFORE any rng() draw, so a
    // non-Ransomware side never perturbs the deterministic stream (a fight with no
    // Ransomware pet in it replays identically). Boot's 0% is inert (an egg can't fight).
    if (!c.line || std::strcmp(c.line, "ransomware") != 0) return false;
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

int Combat::execOverrideChance(const Combatant& trojan) const {
    // The Execution-Override passive belongs to the Trojan line only. The line check
    // short-circuits to 0 BEFORE any rng() draw at the call site, so a non-Trojan pet
    // never perturbs the deterministic stream. Base chance is low; each armed trap adds
    // its trapPassiveBonusPct, so holding all three traps makes the hijack likely.
    if (!trojan.line || std::strcmp(trojan.line, "trojan") != 0) return 0;
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
    actor.lastMoveIdx = moveIdx;
}

void Combat::checkOutcome() {
    // The one place a pet is judged overwhelmed, and so the one place the Backup Drive's
    // death-save runs. Everything that spends Health leaves it unclamped and this is
    // where it lands, which means the save reads the pet's STATE and never the thing
    // that got it there — a hit, a rotting DoT, a ransom bill coming due and whatever
    // gets added next all arrive here the same way, with no branch of their own.
    if (player_.health <= 0) player_.restoreFromBackup();
    // ...and the floor, after the save has had its look at how deep the hole is.
    if (player_.health < 0) player_.health = 0;
    if (enemy_.health < 0) enemy_.health = 0;
    if (enemy_.health <= 0) outcome_ = Outcome::Win;        // win takes priority
    else if (player_.health <= 0) outcome_ = Outcome::Lose;
}

bool Combat::pickNextActor() {
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
    // Feeding-frenzy streak: continues the running count if the same side is acting
    // again, else restarts it at 1 for the new actor. Computed BEFORE resolveTurn so
    // applyEffect (the Phishing combo bonus) sees this hit's own place in the run.
    if (streakCount_ > 0 && playerTurn_ == streakIsPlayer_) ++streakCount_;
    else { streakCount_ = 1; streakIsPlayer_ = playerTurn_; }
    if (playerTurn_) resolveTurn(player_, enemy_, /*byPlayer=*/true);
    else resolveTurn(enemy_, player_, /*byPlayer=*/false);
    checkOutcome();
    playerTurn_ = pickNextActor();   // schedule the next actor by relative speed
    return true;
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
    switch (crewExploit_.kind) {
        case CrewExploitKind::NegateNextHits:
            player_.crewExploit.kind = crewExploit_.kind;
            player_.crewExploit.charges += crewExploit_.magnitude;
            break;
        case CrewExploitKind::None:
            return;
    }
    // "<TAG> xN" popup. dmg=0 so the combat screen's red damage number stays hidden;
    // the readout rides in the move-name slot, and the live counter also shows in the
    // B stat panel — all three read the same crewExploitTag().
    std::snprintf(itemPopup_, sizeof(itemPopup_), "%s x%d",
                  crewExploitTag(crewExploit_.kind), crewExploit_.magnitude);
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
    }
}

const MoveDef* Combat::enemyChannel() const {
    if (enemy_.channelMoveIdx < 0 ||
        enemy_.channelMoveIdx >= static_cast<int>(enemy_.moves.size()))
        return nullptr;
    return enemy_.moves[enemy_.channelMoveIdx];
}

Combatant makePlayerCombatant(const ContentRegistry& reg, const CreatureDef& pet,
                              const MoveLoadout& moves, const Loadout& mods) {
    Combatant c;
    c.name = pet.displayName;
    c.spriteName = pet.spriteName;
    c.stage = pet.stage;                            // drives the per-line passive
    c.line = pet.line;                              // line-gating identity carried into combat
    c.maxHealth = kMaxHealthByStage[stageIndex(pet.stage)];
    c.health = c.maxHealth;
    c.speed = kCombatBaseSpeed;
    // branch lean, scaled by the per-stage offensive multiplier so an
    // evolved pet's output keeps pace with tier-scaled enemy Health (4–8-exchange
    // target). The two compose: branch% × stage% / 100.
    c.powerMultPct = pet.powerMultPct * kStagePowerScalePct[stageIndex(pet.stage)] / 100;
    c.fragMultPct = pet.fragMultPct;                // branch loss-Frag multiplier
    // Move-slot rework #11: the default (Quick Jab) is a PER-SLOT fallback now, not
    // an always-additive extra. One pool entry per UNLOCKED slot — that slot's
    // equipped move if it's present and unlocked at this stage, else the default
    // filling the gap ('s evolution gate reuses this same fallback: an
    // equipped-but-not-yet-unlocked move is inert, exactly as before, it just now
    // yields the default for that slot instead of nothing). A fully-kitted pet
    // therefore never rolls Quick Jab; an unequipped pet rolls nothing else.
    const int slots = MoveLoadout::slotsForStage(pet.stage);
    for (int i = 0; i < slots; ++i) {
        const char* id = moves.equipped(i);
        const MoveDef* m = id ? reg.move(id) : nullptr;
        if (m && moveUnlockedAtStage(*m, pet.stage)) {
            c.moves.push_back(m);
        } else if (const MoveDef* d = reg.move(moves.defaultMove())) {
            c.moves.push_back(d);
        }
    }
    // Mod passives read at fight start, DATA-DRIVEN off ModDef — one
    // loop over the equipped slots instead of a hardcoded `if` per mod. A `line`
    // affinity ADDS its bonus when the pet's line matches (never locks —). Fight-
    // start effects land on the base stats here; ones that stay live for the fight go
    // into c.mods for applyEffect/resolveTurn to read; post-battle ones (Bits/Frag) are
    // read by the Game (game_combat) off the same ModDef.
    for (int i = 0; i < kModSlots; ++i) {
        const char* id = mods.equipped(i);
        if (!id) continue;
        const ModDef* m = reg.mod(id);
        if (!m) continue;
        int mag = m->magnitude;
        if (m->line && c.line && std::strcmp(m->line, c.line) == 0) mag += m->affinityBonus;
        switch (m->effectKind) {
            case ModEffect::PowerPct:     c.powerMultPct += mag; break;
            case ModEffect::DamageCutPct: c.dmgReducePct += mag; break;
            case ModEffect::MaxHealth:
                c.maxHealth += mag; c.health += mag;
                // Cold Storage: a bulk buffer costs a little boot speed. Safe to fold
                // into the shared MaxHealth case (unlike Thorns/ConditionalThorns) since
                // both effects are pure linear accumulation — no combine ambiguity if a
                // second MaxHealth mod (magnitude2 == 0) rides alongside it.
                if (m->magnitude2 > 0) c.speed -= m->magnitude2;
                break;
            case ModEffect::Speed:
                c.speed += mag;
                if (m->magnitude2 > 0)    // small attack-power COST (Overclock tradeoff)
                    c.powerMultPct = c.powerMultPct * (100 - m->magnitude2) / 100;
                break;
            // Passives that stay live for the fight: folded into c.mods by their own
            // ModRule (mod_state.cpp), which is where a second copy of the same kind is
            // reconciled. The hooks that read them are in applyEffect/resolveTurn.
            case ModEffect::RaidMirror:
            case ModEffect::Thorns:
            case ModEffect::DeathBlast:
            case ModEffect::MaxHitCapPct:
            case ModEffect::LoadBalance:
            case ModEffect::WatchdogClamp:
            case ModEffect::FaradayCut:
            case ModEffect::FirstStrikeRankMult:
            case ModEffect::FirstHitCutPct:
            case ModEffect::LowHealthPowerPct:
            case ModEffect::GambleBattlePowerPct:
            case ModEffect::ConditionalThorns:
            case ModEffect::StealAmplifyPct:
                c.mods.apply(m->effectKind, mag, m->magnitude2);
                break;
            case ModEffect::AttackCountPowerPct: {  // Botnet Swarm — +mag% power PER Attack move
                int atk = 0;
                for (const MoveDef* cm : c.moves)
                    if (cm->kind == MoveDef::Kind::Attack) ++atk;
                c.powerMultPct += mag * atk;
                break;
            }
            case ModEffect::DefendCountCutPct: {  // Air-Gap Ward — +mag% cut PER Defend move
                int def = 0;
                for (const MoveDef* cm : c.moves)
                    if (cm->kind == MoveDef::Kind::Defend) ++def;
                c.dmgReducePct += mag * def;
                break;
            }
            case ModEffect::PostBattleBits:  // read post-battle by the Game (Packet Sniffer)
            case ModEffect::FatigueFragCut:  // read at the frag tax (Heat Sink)
            case ModEffect::None:
                break;
        }
    }
    // ECC Memory and Load Balancer both declare their magnitude as a % of max Health;
    // resolve each to the flat number the damage pipeline compares a hit against. Runs
    // after the equip loop so a MaxHealth mod in another slot is already folded in.
    if (ModState* ecc = c.mods.find(ModEffect::MaxHitCapPct); ecc && ecc->mag > 0) {
        ecc->mag = c.maxHealth * ecc->mag / 100;
        if (ecc->mag < 1) ecc->mag = 1;   // always a real cap, never a 0 that reads as uncapped
    }
    if (ModState* lb = c.mods.find(ModEffect::LoadBalance); lb && lb->mag > 0) {
        if (lb->mag2 > 0) {
            lb->mag = c.maxHealth * lb->mag / 100;
            if (lb->mag < 1) lb->mag = 1;
        } else {
            lb->mag = 0;                  // no deferred share declared → nothing to split
        }
    }
    return c;
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

const char* simDummyName(int tier) {
    return tier <= 0 ? "Basic Dummy" : "Hardened Dummy";
}

CombatEnemy simDummy(int tier) {
    // Safe practice targets. Both tiers share the one dummy sprite — they are the
    // same prop, and the tier reads off the level/stats rows. Stats/tiers are balance.
    if (tier <= 0)
        return {"Basic Dummy", "SPR_DUMMY", 1, 30, 8, {"quick_jab"}};
    return {"Hardened Dummy", "SPR_DUMMY", 2, 55, 11,
            {"quick_jab", "packet_storm"}};
}

CombatEnemy wildMalbeast(int sectorTier, uint32_t variantRoll) {
    // A seed roster keyed by sector tier, 2 variants per tier rolled uniformly.
    // Each wild carries its OWN SPR_MALBEAST_* frame, so a player can tell one
    // encounter from another at a glance rather than meeting six recolours of the
    // same dog. The names here are the source of truth for kWildMalbeastIds below,
    // which the 'Pedia's seen/defeated masks are keyed on.
    // Base stats are the design values; the wild challenge buff (kWildEnemy*Pct)
    // is applied uniformly in makeEnemyCombatant off the isWild flag, so the
    // tables stay readable and bosses/Sim (which reuse the shape) are unaffected.
    if (sectorTier <= 1) {
        static const CombatEnemy kTier1[] = {
            {"GlitchHog", "SPR_MALBEAST_GLITCHHOG", 1, 35, 9, {"quick_jab"}, true},
            {"Segfault Pup", "SPR_MALBEAST_SEGFAULT_PUP", 1, 30, 10,
             {"quick_jab"}, true},
        };
        return kTier1[variantRoll % 2];
    }
    if (sectorTier == 2) {
        static const CombatEnemy kTier2[] = {
            {"Packet Wraith", "SPR_MALBEAST_PACKET_WRAITH", 2, 55, 12,
             {"quick_jab", "packet_storm"}, true},
            {"Cache Ghoul", "SPR_MALBEAST_CACHE_GHOUL", 2, 50, 13,
             {"quick_jab", "packet_storm"}, true},
        };
        return kTier2[variantRoll % 2];
    }
    static const CombatEnemy kTier3[] = {
        {"Buffer Wyrm", "SPR_MALBEAST_BUFFER_WYRM", 3, 80, 14,
         {"packet_storm", "fork_bomb"}, true},
        // The apex gets a bigger 64x56 cell than the 56x48 the rest of the roster
        // uses — drawn from its own frame size, so nothing else has to know.
        {"Kernel Leviathan", "SPR_MALBEAST_KERNEL_LEVIATHAN", 3, 85, 13,
         {"packet_storm", "fork_bomb"}, true},
    };
    return kTier3[variantRoll % 2];
}

// The fixed wild-malbeast roster (combat.h) — slugged ids matching the wildMalbeast()
// display names above, in bit-position order for Game's seen/defeated masks.
const char* const kWildMalbeastIds[kWildMalbeastCount] = {
    "glitchhog", "segfault_pup", "packet_wraith", "cache_ghoul", "buffer_wyrm",
    "kernel_leviathan",
};

namespace {
// Lowercase + non-alnum -> '_', one-for-one (no collapsing) — matches exactly how
// kWildMalbeastIds above was derived from each CombatEnemy::name.
void slugify(const char* s, char* out, size_t outSize) {
    size_t i = 0;
    for (const char* p = s; *p && i + 1 < outSize; ++p) {
        char c = *p;
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))) c = '_';
        out[i++] = c;
    }
    out[i] = '\0';
}
}  // namespace

int wildMalbeastIndex(const char* enemyName) {
    if (!enemyName) return -1;
    char slug[32];
    slugify(enemyName, slug, sizeof(slug));
    for (int i = 0; i < kWildMalbeastCount; ++i)
        if (std::strcmp(slug, kWildMalbeastIds[i]) == 0) return i;
    return -1;
}

void applyWildSubAreaRamp(CombatEnemy& e, int area, int sub) {
    if (area < 0) area = 0;
    if (sub < 0) sub = 0;
    if (sub >= kSubAreasPerArea) sub = kSubAreasPerArea - 1;

    // the explicit LEVEL + its stat half. Level is a GLOBAL depth rung:
    // +1 per sub-area AND +kSubAreasPerArea across areas, so it climbs monotonically
    // as the player pushes deeper. It's what wildWinXp() compares against the pet's
    // level to reward punching up / tax farming shallow. The STAT bump is the "rolled
    // level-up stats" half: the between-AREA jump already rides the tier roster in
    // wildMalbeast() (tier-2 wilds carry far more Health), so here we only thicken
    // WITHIN the sector — Health per sub (soaks longer) + speed at the deepest rungs
    // (swings sooner). Applied for every sub (including 0, a no-op stat-wise).
    e.level = area * kSubAreasPerArea + sub;
    e.hasLevel = true;
    e.maxHealth += sub * kWildSubAreaHealthStep;
    if (sub >= kSubAreasPerArea - 2) ++e.speed;

    // The moveset ladder — the first (moves-only) lever of the ramp, keyed by the
    // SUB-AREA index (the within-sector climb). The BETWEEN-sector step is already
    // carried by the tier roster in wildMalbeast() (tier-2 wilds have far more Health
    // + speed, so they soak longer and swing more often), so moves only need to make
    // each sub-area meaner than the last. Rungs are ordered by EFFECTIVE per-turn
    // damage, since the enemy rotates its kit and gets only a few turns before it
    // dies — what bites is the AVERAGE swing, not the theoretical top move. That last
    // point matters: fork_bomb's 26 is spread over a 2-turn windup (~13 eff/turn), so
    // adding it to a set of single-turn hitters LOWERS the average and makes the fight
    // EASIER. So the apex is the two hardest single-turn hitters (buffer_overflow 20 +
    // rootkit_strike 24); fork_bomb stays a boss/late-tier flavor move, not the wild
    // ceiling. quick_jab(6) < packet_storm(12) < buffer_overflow(20) < rootkit_strike
    // (24). Sub 0 keeps the roster baseline; each rung raises the average from there.
    static const std::vector<const char*> kLadder[kSubAreasPerArea] = {
        {},                                                          // sub 0: baseline
        {"quick_jab", "packet_storm"},                              // sub 1  avg  9
        {"quick_jab", "packet_storm", "buffer_overflow"},           // sub 2  avg 13
        {"packet_storm", "buffer_overflow"},                        // sub 3  avg 16
        {"buffer_overflow", "rootkit_strike"},                      // sub 4  avg 22 (apex)
    };
    if (sub <= 0) return;                              // baseline: keep roster moves
    e.moveIds = kLadder[sub];
}

void applyDeepWebScale(CombatEnemy& e, int petLevel, int depth) {
    if (petLevel < 0) petLevel = 0;
    if (depth < 0) depth = 0;
    // depth (the dive's current win-streak) folds in as a logarithmic bonus "effective
    // level" on top of the pet's own — so a fresh dive is still today's parity fight,
    // but pushing deeper gradually punches the pet up (wildWinXp's diff bonus, since
    // enemyLevel now exceeds petLevel) and thickens Health/speed to match, without an
    // endless zone runaway-scaling (floorLog2 flattens the curve at deep streaks).
    const int effLevel = petLevel + kDeepWebDepthLevelPerLog2 * floorLog2(depth + 1);
    // Moves stay the tier-3 endgame kit (set by wildMalbeast(3)); the isWild challenge
    // buff still applies in makeEnemyCombatant. Bits payout (diffPips-keyed, not
    // level-keyed) doesn't ride this scale — see deepWebDepthBitsPct below.
    e.level = effLevel + kDeepWebEnemyLevelOffset;
    e.hasLevel = true;
    e.maxHealth += effLevel * kDeepWebHealthPerLevel;
    if (kDeepWebSpeedPerNLevels > 0) e.speed += effLevel / kDeepWebSpeedPerNLevels;
}

int deepWebDepthBitsPct(int depth) {
    if (depth < 0) depth = 0;
    int pct = 100 + floorLog2(depth + 1) * kDeepWebDepthBitsPctPerLog2;
    if (pct > kDeepWebDepthBitsMaxPct) pct = kDeepWebDepthBitsMaxPct;
    return pct;
}

void applySimDummyLevelScale(CombatEnemy& e, int petLevel) {
    if (petLevel < 0) petLevel = 0;
    e.level = petLevel;
    e.hasLevel = true;
    e.maxHealth += petLevel * kSimDummyHealthPerLevel;
    if (kSimDummySpeedPerNLevels > 0) e.speed += petLevel / kSimDummySpeedPerNLevels;
}

BossGauntlet subAreaBoss(int areaIdx, int sub) {
    // One strong malbeast. Health climbs with the area's LADDER DEPTH (areaTier)
    // + the sub index
    // so sub 1 opens easy and the signature sub 5 is the wall; moves thicken as the
    // sub climbs. Returned as a length-1 gauntlet so the carried-Health round plumbing
    // (startBossRound/finishBossRound) drives it unchanged. Generic frame (no new art).
    // Boss names are a content pool disjoint from the roster + the wild malbeasts
    // (namespace guard, tested), owned by each area's own AreaDef.
    if (areaIdx < 0) areaIdx = 0;
    if (areaIdx >= kAreaCount) areaIdx = kAreaCount - 1;
    if (sub < 0) sub = 0;
    if (sub >= kSubAreasPerArea) sub = kSubAreasPerArea - 1;
    const AreaDef& a = area(areaIdx);
    const int tier = areaTier(areaIdx);
    const int health = kSubBossHealthBase + tier * 8 + sub * kSubBossHealthStep;
    const int speed = kSubBossSpeedBase + tier + (sub >= kSubAreasPerArea - 1 ? 2 : 0);
    std::vector<const char*> moves = {"quick_jab"};
    if (sub >= 2) moves.push_back("packet_storm");
    if (sub >= kSubAreasPerArea - 1) {
        moves.push_back("fork_bomb");
        // The signature (sub 4) boss debuts its area's THREAT rider — the telegraphed
        // apex where you'd bring the matching counter-mod, earned in that same area's
        // own loot table (AreaDef::modPoolIds).
        if (a.apexThreatMoveId) moves.push_back(a.apexThreatMoveId);
    }
    CombatEnemy e{a.subBossNames[sub], "SPR_PET_CACHEMUTT", tier + 1, health,
                  speed, std::move(moves)};
    return {a.subBossNames[sub], tier + 1, {e}};
}

BossGauntlet areaBoss(int areaIdx) {
    // The five sub-area bosses fought back-to-back. Health carries across
    // the rounds (no heal), so this is the real finale. The apex (round 5) is the
    // signature sub-area boss — the piece curation upgrades first.
    if (areaIdx < 0) areaIdx = 0;
    if (areaIdx >= kAreaCount) areaIdx = kAreaCount - 1;
    const AreaDef& a = area(areaIdx);
    BossGauntlet g{a.areaBossName, areaTier(areaIdx) + 1, {}};
    for (int s = 0; s < kSubAreasPerArea; ++s)
        g.rounds.push_back(subAreaBoss(areaIdx, s).rounds[0]);
    return g;
}

// Bits payout. randInt(R, R²) — one uniform draw in the inclusive range.
int normalBitsReward(int stageRank, uint32_t& rng) {
    const int R = stageRank < 1 ? 1 : stageRank;
    const int lo = R, hi = R * R;                 // Process 2..4, Script 3..9, ...
    const int span = hi - lo + 1;                 // ≥1 (hi≥lo since R≥1)
    rng = rng * 1664525u + 1013904223u;
    return lo + static_cast<int>((rng >> 16) % static_cast<uint32_t>(span));
}

// A boss rolls the normal range R times and sums (Process boss 4..8, Script boss
// 9..27, Daemon boss 16..64). A gauntlet calls this once per round and banks the
// lump for the end (game orchestration).
int bossBitsReward(int stageRank, uint32_t& rng) {
    const int R = stageRank < 1 ? 1 : stageRank;
    int total = 0;
    for (int i = 0; i < R; ++i) total += normalBitsReward(R, rng);
    return total;
}

// level-difference XP scaling. See combat.h for the contract.
int wildWinXp(int baseXp, int enemyLevel, int petLevel) {
    const int diff = enemyLevel - petLevel;              // >0 = punching up
    int pct = 100 + diff * kWildXpPerLevelDiffPct;
    if (pct < kWildXpDiffMinPct) pct = kWildXpDiffMinPct; // floor (never zero)
    if (pct > kWildXpDiffMaxPct) pct = kWildXpDiffMaxPct; // ceiling (cap the bonus)
    int xp = baseXp * pct / 100;
    return xp < 1 ? 1 : xp;                              // always at least a trickle
}

void applyLevelStatPoints(Combatant& c, const int statPoints[4]) {
    if (!statPoints) return;
    // power → +% attack lean; defense → +% incoming-damage cut (its own cap, then the
    // total cut is clamped so defense can never null a hit) AND +% defend-move brace
    // magnitude (symmetric to power→attack, uncapped: braces are one-shot and cost a
    // turn); speed → +initiative; max-Health → +HP.
    c.powerMultPct += statPoints[0] * kLevelPowerPctPerPoint;
    int defAdd = statPoints[1] * kLevelDefensePctPerPoint;
    if (defAdd > kLevelDefenseCapPct) defAdd = kLevelDefenseCapPct;
    c.dmgReducePct += defAdd;
    if (c.dmgReducePct > kLevelDmgReduceMaxPct) c.dmgReducePct = kLevelDmgReduceMaxPct;
    c.defenseMultPct += statPoints[1] * kLevelDefenseBracePctPerPoint;
    c.speed += statPoints[2] * kLevelSpeedPerPoint;
    c.maxHealth += statPoints[3] * kLevelHealthPerPoint;
    c.health = c.maxHealth;
}

Combatant makeEnemyCombatant(const ContentRegistry& reg, const CombatEnemy& spec) {
    Combatant c;
    c.name = spec.name;
    c.spriteName = spec.spriteName;
    c.diffPips = spec.diffPips;
    c.level = spec.level;
    c.hasLevel = spec.hasLevel;
    c.maxHealth = spec.maxHealth;
    c.health = spec.maxHealth;
    c.speed = spec.speed;
    if (spec.isWild) {                              // wild-encounter challenge buff
        c.maxHealth = c.maxHealth * kWildEnemyHealthPct / 100;
        c.health = c.maxHealth;
        c.enemyDamageMultPct = kWildEnemyDamagePct;
    }
    for (const char* id : spec.moveIds)
        if (const MoveDef* m = reg.move(id)) c.moves.push_back(m);
    if (c.moves.empty())                            // never actionless
        if (const MoveDef* d = reg.move("quick_jab")) c.moves.push_back(d);
    return c;
}

} // namespace mal
