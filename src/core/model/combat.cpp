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
    // The frenzy ratchet is "the most bubble this pet has held", so it can never start
    // below a pool the combatant walked in with — a duel round or a gauntlet leg that
    // carries a live shield arms the lean on the same terms a cast would.
    if (player_.phishShieldPeak < player_.shieldHp)
        player_.phishShieldPeak = player_.shieldHp;
    if (enemy_.phishShieldPeak < enemy_.shieldHp)
        enemy_.phishShieldPeak = enemy_.shieldHp;
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
    lastWasStrike_ = false;
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
}

// Uniform pick over `self`'s slots, skipping lastMoveIdx (the no-consecutive-repeat
// rule) unless `allowRepeat`, and — when `attacksOnly` — every Defend slot. Draws
// exactly one rng() when it has anything to choose between. Returns -1 when nothing
// qualifies, which is the caller's cue to keep what it already had rather than force
// an unplayable slot.
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

// The frenzy lean (Phishing) — see combat.h for the contract. 0 for every combatant that
// has never pooled a shield past its own max Health, which is every non-Phishing pet in
// the game, and so the guard that keeps this whole path from drawing rng() in a fight
// without the track.
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
    // A WILDCARD row declares none of the fields below and is still not a pure brace: it
    // has not decided what it is yet, and what it rolls may be a pool, a trap or a spawn.
    // Reading it off its own empty fields would re-roll the metamorphic line off its own
    // defend half every time a brace happened to be standing.
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
    // Measured against the STAGE's body (kMaxHealthByStage), not this pet's own levelled
    // maxHealth. Against the levelled one the conversion decayed as the pet grew: earned
    // max-Health points inflate the denominator while the pool only tracks Defence, so a
    // Health-steered pet at level 60 carries a body of 400-odd and a bubble that reads as
    // nearly nothing. The pool is a statement about how deep the bubble is stacked, and a
    // pet should not get worse at its own line's mechanic for having raised a different stat.
    const int base = kMaxHealthByStage[stageIndex(c.stage)];
    if (base <= 0) return 0;
    // Phishing Rod (mod) scales the siphon ITSELF, not its ceiling — a cap almost no
    // fight reaches is not a bonus. Perfect Bite, which the mod was written for alone, is
    // a roll on top of a condition (bubble up, then the chance lands), so a mod confined
    // to it paid on a fraction of a fraction. The pool siphon is the line's continuous
    // half and pays every turn the bubble is live, which is the floor the mod needed.
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
    // One stat point's worth each, in the vocabulary applyLevelStatPoints already spends.
    // The KIND picks which pair is paid, so a varied kit shapes what the pet becomes
    // rather than every payment landing on the same stat.
    if (kind == MoveKind::Attack) {
        c.powerMultPct += kLevelPowerPctPerPoint * points;
        c.speed += static_cast<float>(kLevelSpeedPerPoint * points);
    } else {
        c.dmgReducePct += kLevelDefensePctPerPoint * points;
        if (c.dmgReducePct > kLevelDmgReduceMaxPct) c.dmgReducePct = kLevelDmgReduceMaxPct;
        c.defenseMultPct += kLevelDefenseBracePctPerPoint * points;
        // The ceiling AND the Health standing in it — a pool cannot be raised from under
        // a fighter without giving it the room it just gained.
        c.maxHealth += kLevelHealthPerPoint * points;
        c.health += kLevelHealthPerPoint * points;
    }
}

uint32_t moveEffectMask(const MoveDef& m) {
    // One bit per rider, in the order MoveDef declares them. Bit POSITIONS are internal —
    // nothing persists this mask — so a new rider appends a bit here and needs no
    // migration, only a row that sets the field it reads.
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
    // A band with no rows hands its weight back to generic rather than being rolled into
    // and found empty — which is what a pool naming a line this build doesn't know, or a
    // line with no rows of this kind (Phishing fields no Defend track), actually is.
    int wGeneric = genericN > 0 ? kWildSourceGenericPct : 0;
    const int wA = lineAN > 0 ? kWildSourceLineAPct : 0;
    const int wB = lineBN > 0 ? kWildSourceLineBPct : 0;
    if (lineAN == 0) wGeneric += kWildSourceLineAPct;
    if (lineBN == 0) wGeneric += kWildSourceLineBPct;
    const int sum = wGeneric + wA + wB;
    if (sum <= 0) return pool.rows[roll % total];   // no weights: fall back to a flat draw
    // ONE draw does both halves — the low bits pick the band, the high bits the row inside
    // it. Two draws would consume two numbers from the seeded stream, and a duel's two
    // devices only stay in step while they take the same count.
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
    // Uniform over the moves != lastMoveIdx — no-consecutive-repeat. Uniform
    // selection makes the attack/defend lean emerge directly from the slot mix
    // more attack slots → more attack rolls, no separate ratio dial.
    int idx = pickSlot(self, /*attacksOnly=*/false, /*allowRepeat=*/false);
    if (idx < 0) return 0;
    // ...with ONE exception, and it is a Phishing state rather than a dial: a bubble
    // stacked past the pet's own max Health has bought more wall than the fight can
    // spend, while a bite still buys something. Past that point a Defend pick is
    // re-rolled into an Attack one, at a chance that ramps with how far the pool was
    // stacked (phishFrenzyLeanPct, content_passives.h).
    //
    // The re-roll ignores no-consecutive-repeat, the same licence a committed override
    // takes: a frenzy is a pet biting single-mindedly, and a two-slot Phishing kit whose
    // only attack was the last move would otherwise be unable to frenzy at all. It can
    // still decline to find an attack (a Defend-only kit), and then the original stands.
    const int leanPct = phishFrenzyLeanPct(self);
    if (leanPct > 0 && self.moves[idx]->kind != MoveDef::Kind::Attack &&
        static_cast<int>(rng() % 100) < leanPct) {
        const int atk = pickSlot(self, /*attacksOnly=*/true, /*allowRepeat=*/true);
        if (atk >= 0) idx = atk;
    }
    // ...and one more, for the same reason in a different costume: a pure-brace Defend
    // (braceOnlyDefend) rolled while this fighter's brace is STILL UP adds to a one-shot
    // pool that already absorbs the next hit, so the turn buys nothing but overkill.
    // Swing instead. Unconditional rather than a chance — there is no state in which
    // re-bracing is the better turn, so there is nothing for a dial to express.
    //
    // Takes the same no-consecutive-repeat licence the frenzy re-roll does, and for the
    // same reason: a defend-heavy kit with a single attack would otherwise be unable to
    // act on this at all, which is exactly the kit the waste falls hardest on. It can
    // still decline to find an attack (a Defend-only kit), and then the original stands.
    if (self.guard > 0 && braceOnlyDefend(*self.moves[idx])) {
        const int atk = pickSlot(self, /*attacksOnly=*/true, /*allowRepeat=*/true);
        if (atk >= 0) idx = atk;
    }
    return idx;
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

// The multiplier one kind of replica takes from the other's count at the moment it
// spawns. Both kinds bank it (wormAttackerDamage / wormDefenderHealth), so it is read
// once per copy and never again.
static int wormCrossMult(int otherKindCount) {
    return otherKindCount < kWormReplicaMultFloor ? kWormReplicaMultFloor : otherKindCount;
}

// Whether a side replicates — the Worm line's passive family today. Kept in one place
// because several hooks ask, from applyEffect down; each reads the flag its combatant
// was built with.
static bool replicates(const Combatant& c) {
    return hasLinePassive(c.linePassives, LinePassive::Replication);
}

void Combat::applyEffect(Combatant& actor, Combatant& target, const MoveDef* mv,
                         bool byPlayer, int moveIdx) {
    target.mirrorFired = false;
    // Malbeast In The Middle (crew Exploit): while it holds, every SELF-BUFF the side
    // OPPOSITE the holder casts is copied onto the holder as it lands. Answered once
    // here so each of the four buff sites below costs a single line, and false in every
    // fight that hasn't armed it. `byPlayer` names who is CASTING (the Trojan hijack
    // passes the flipped flag with the swapped roles), so `mirror` is always the side
    // watching this cast happen and no other reading.
    //
    // Read off whichever side holds it rather than off player_, because a tournament
    // opponent arms its own Exploits (Combatant::autoExploit) and an ability that only
    // ever worked in one seat would be a different ability depending on who drew it.
    //
    // Scoped to self-buffs, not to the steal track: a siphon's gain half is the
    // caster's OWN stat changing hands, and copying that back would hand the holder a
    // refund rather than a copy of somebody else's advantage.
    Combatant& mirror = byPlayer ? enemy_ : player_;
    const bool mitmCopy = mirror.crewExploit.holds(CrewExploitKind::MirrorEnemyBuffs);
    // Feeding-frenzy combo (Phishing steal-attacks only, mv->stealPowerPct > 0): this
    // actor's OWN run of steal-attack casts made WITH THE BUBBLE UP. A continuing run
    // permanently banks (run length - 1) flat damage into phishComboBonus, applied below
    // to every future steal-attack hit this fight — so a short early run adds a sliver,
    // a long one snowballs, and it never decays even after this particular run ends
    // (unlike Combat::streakCount_, the turn-order streak driving the render pace, which
    // resets clean each time).
    //
    // The bubble is the gate, not the interruption. A Defend cast — the very move that
    // raises the bubble — leaves the run standing, because every other rider on this line
    // (stealSpeedPct, stealCurrentHpPct, Perfect Bite, the frenzy heal) already requires
    // shieldHp > 0: a brace that armed four riders while breaking a fifth was the one
    // place the track argued with itself. What breaks the run is SWINGING while exposed.
    //
    // The break is on any Attack, not only a steal-attack: "caught out with the bubble
    // down" is a statement about the pet's exposure, and scoping it to the line's own
    // moves would have let a mixed kit swing generics through the whole exposed stretch
    // with its banked run intact and re-bubble at leisure. Only the ADVANCE is narrowed —
    // a swing that siphons no Power is frenzy-neutral, neither building nor breaking,
    // which is what keeps a heavy off-line hitter a real choice rather than a strict one.
    //
    // Note what the advance keys on: the FIELD, not the line. Any move carrying
    // stealPowerPct feeds this run and collects phishComboBonus below, whoever authored
    // it — which is precisely why the generic boss pool (content_moves.cpp) leaves
    // stealPowerPct at zero on every row and shreds Defense instead. A generic move that
    // set it would hand every Phishing pet a combo engine off-line.
    if (mv->kind == MoveDef::Kind::Attack) {
        if (actor.shieldHp <= 0) {
            actor.phishStreak = 0;    // caught out with the bubble down
        } else if (mv->stealPowerPct > 0) {
            actor.phishStreak++;
            if (actor.phishStreak > 1) actor.phishComboBonus += actor.phishStreak - 1;
        }
    }
    // A SEIZED move swung by its captor hits for the wall behind it (kRansomSeizedWallPct).
    // Asked here, before the damage below is scaled, so the bonus rides the same
    // multipliers everything else does. `moveIdx` naming the seized slot is the whole
    // test: the seized move IS that slot for as long as the ransom runs, so nothing has
    // to compare move pointers to know it is being swung by the pet that took it.
    const bool swingingSeized = mv->kind == MoveDef::Kind::Attack &&
                                actor.ransomSeizure.holding() && moveIdx >= 0 &&
                                moveIdx == actor.ransomSeizure.slot;
    if (mv->kind == MoveDef::Kind::Attack) {
        // Base damage scaled by the actor's branch attack-power lean PLUS any
        // Lockout-track Power the caster has stacked this fight: Bad-branch
        // petware hits harder, Good-branch softer, a stacked wall harder still. Meltdown
        // Core (mod) adds a further comeback bonus while the actor is critically low.
        int mult = actor.powerMultPct + actor.stackPowerBonus;
        // Extortion Ledger (mod), the POWER half — paid while this pet is holding damage it
        // has not settled for. An UNPAID ransom, not a seized move: a seizure needs a full
        // Cipher stack standing under a live window and closes on the next bill, which is
        // a payoff most fights never reach, and a bonus hung on it averages to nothing
        // however large it is. A pool with anything in it is the ordinary state of the
        // line doing its job, so this pays on the turns the pet is actually playing.
        //
        // It also reads as the thing the family IS: what the pool holds is what the pet has
        // taken and not answered for yet, and it hits harder for as long as that is true.
        // ...scaled by how much is on the ledger. The bonus pays at any pool size and grows
        // with what the pet is owed, which is what makes the pool worth CARRYING rather
        // than merely worth having opened.
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
        // These moves are deliberately low-power and lean on the min-1 penetration
        // floor rather than raw damage, so this flat bonus (above) is what actually
        // makes a sustained frenzy dangerous instead of a long string of 1s.
        if (mv->stealPowerPct > 0) dmg += actor.phishComboBonus;
        // The wall, spent. Ransomware's one currency it could never cash in.
        if (swingingSeized && actor.stackDefenseBonus > 0)
            dmg = dmg * (100 + actor.stackDefenseBonus * kRansomSeizedWallPct / 100) / 100;
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
            if (mv->armorPiercePct > 0) {
                // Defence tier 1: pierce exists to make a wall irrelevant, so a committed
                // wall cuts the pierce back before it lands (levelDefensePierceResistPct).
                // Applied to the PIERCE and not to the cut, because what the tier buys is
                // that the cut it already earned stops being routed around.
                int pierce = mv->armorPiercePct * (100 - target.pierceResistPct) / 100;
                if (pierce > 0) reduce = reduce * (100 - pierce) / 100;
            }
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
                if (mv->armorPiercePct > 0) {
                    const int pierce =
                        mv->armorPiercePct * (100 - target.pierceResistPct) / 100;
                    brace = brace * (100 - pierce) / 100;
                }
                const int unspent = brace > dmg ? brace - dmg : 0;
                dmg = dmg > brace ? dmg - brace : 0;
                // Defence tier 2: the remainder an over-sized brace did not need CARRIES
                // instead of being binned (levelDefenseBraceRetainPct). What a committed
                // wall gets is efficiency — the same brace covering more hits — rather
                // than a bigger number, which the % cut's own ceiling has already
                // established it cannot be paid in. Measured against the pre-pierce
                // remainder, so resisting a pierce and retaining its leftovers are one
                // reward and not two.
                target.guard = unspent * target.braceRetainPct / 100;
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
        //
        // Overrunning it also releases the frenzy ratchet (phishShieldPeak): the lean
        // that a stacked pool armed is spent, and the pet goes back to mixed play. Only
        // the POP clears it — a pool merely chewed down keeps the lean, which is what
        // makes the enemy's way out of a frenzy "break the bubble", not "wait".
        if (dmg > 0 && target.shieldHp > 0) {
            // Poisoned data (MoveDef::poolRetaliateDot): reading the decoy costs the reader.
            // Planted on the ATTACKER, before the pool is chewed, so a hit that pops the
            // bubble still poisons — the last read is the one that got through, and it was
            // still a read. Refreshes rather than stacks, like every other DoT.
            if (target.poolDotDamage > 0 && target.poolDotTurns > 0) {
                const int cut = actor.mods.mag(ModEffect::FaradayCut);
                const int per = target.poolDotDamage * (100 - cut) / 100;
                if (per > 0) { actor.dotPerTurn = per; actor.dotTurnsLeft = target.poolDotTurns; }
            }
            if (target.shieldHp >= dmg) { target.shieldHp -= dmg; dmg = 0; }
            else { dmg -= target.shieldHp; target.shieldHp = 0; }
            if (target.shieldHp == 0) target.phishShieldPeak = 0;
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
        // The SEIZURE (RansomSeizure): a full Cipher wall with a live ransom takes the
        // attack that hits it. The pet swings it out of the brace's own slot until the
        // ransom settles. Only an attack is worth taking, and only a hit that landed —
        // a swing the wall shrugged off entirely was never held to anything.
        // ...and never a WILDCARD row. A metamorphic pet's slot resolves to whatever it
        // rolled before this point, so what lands here is an ordinary move and the seizure
        // gets something real — but a pool with nothing in it leaves the row itself, and a
        // row that casts nothing is worth nothing to whoever takes it. The ransomer has no
        // pool of its own to resolve one with, so it would swing an empty slot for the
        // whole hold.
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
                // Holding a move hostage IS a ransom, so it runs the same clock — which is
                // also what gives the seizure its release. A pet that goes on diverting
                // hits keeps resetting that clock and so keeps the move; one that does not
                // hands it back when the hold lapses.
                target.ransomTurnsLeft = kRansomHoldTurns;
            }
            seize.armed = false;
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
            // The pool's bonus applies to whichever gated steal fires, on top of the base
            // and any Perfect-Bite doubling: the bubble is what permits these two AND now
            // what sizes them.
            const int poolBonus = phishPoolSiphonBonusPct(actor);
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
                pct += mv->stealSpeedPct * poolBonus / 100;
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
                pct += mv->stealCurrentHpPct * poolBonus / 100;
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
                    // The pool MOVES: the ceiling and the Health inside it both cross. A
                    // ceiling on its own is not a reward — combat has no heal to climb
                    // into it, so raising `maxHealth` alone hands the caster a number it
                    // can never reach and the move only ever reads as a debuff.
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
        // Deadman Switch (mod): if this hit KO'd the pet, it deals a parting blast to the
        // enemy. A mutual KO resolves as a Win (enemy-death priority, checkOutcome) — the
        // clutch payoff of a rare Epic. One shot: cleared so a gauntlet round can't reuse it.
        if (ModState* dead = target.mods.find(ModEffect::DeathBlast);
            target.health <= 0 && dead && dead->mag > 0 && dead->pending > 0) {
            actor.health -= dead->mag;
            --dead->pending;
        }
        setLast(mv->displayName, dmg, byPlayer, false, ransomed > 0, /*strike=*/true);
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
            // caster's move and is measured against the caster's own pile, so it bounds
            // what there is to copy rather than what the holder may hold.
            if (mitmCopy) mirror.stackPowerBonus += gain;
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
        // (mitmCopy) — the caster's brace becomes the holder's brace, its pool the
        // holder's pool. The copy is additive on whatever the holder already had, so a
        // fighter that braced this turn keeps its own on top of the one it stole.
        const int braced = mv->power * actor.defenseMultPct / 100;
        // Whether this cast is going to put a DEFENDER on the board instead. A Defend's
        // replicaSpawnPct is 100 on every row that has one, so a free slot is the whole
        // of the question — and it has to be asked HERE, before the brace, even though
        // the spawn itself happens after this resolves.
        const bool spawnsDefender = mv->replicaSpawnPct > 0 && replicates(actor) &&
                                    actor.wormReplicaCount < kWormReplicaSlots;
        if (mv->shieldPool > 0) {
            actor.shieldHp += braced;
            // Poisoned data: a pool row may arm a RETALIATION, applied to whoever strikes
            // the bubble (the attack path above). It rides the pool rather than the brace
            // because the pool is what the enemy has to chew through — which is also what
            // makes it a conversion a defend-heavy pet can actually hold, unlike this
            // line's steals, which need its one attack slot.
            if (mv->poolRetaliateDot > 0 && mv->poolRetaliateTurns > 0) {
                actor.poolDotDamage = mv->poolRetaliateDot;
                actor.poolDotTurns = mv->poolRetaliateTurns;
            }
            // Ratchet the frenzy high-water mark on every top-up (chooseMove reads it).
            // Re-casting onto a live pool is therefore how a pet holds a frenzy open past
            // the hits that would otherwise have popped it.
            if (actor.shieldHp > actor.phishShieldPeak)
                actor.phishShieldPeak = actor.shieldHp;
            if (mitmCopy) {
                mirror.shieldHp += braced;
                if (mirror.shieldHp > mirror.phishShieldPeak)
                    mirror.phishShieldPeak = mirror.shieldHp;
            }
        } else if (spawnsDefender) {
            // A Worm's defend does not brace: the BODY it is about to put on the board is
            // the move (rollWormSpawn, after this resolves), and the row's `power` exists
            // only so the turn still does something when every replication slot is full.
            // Bracing as well would pay twice for one cast — and would put a guard on a
            // fighter whose whole defensive story the player reads off the copies standing
            // in front of it.
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
        // ...and once that wall is FULL, the next thing to hit it gets seized rather than
        // absorbed (RansomSeizure). The full wall is the WHOLE condition: requiring a
        // ransom to already be running as well meant two independent things had to coincide,
        // and measured across random legal kits that halved the duty cycle to 27% — a large
        // payoff nobody meets. The seizure starts the ransom clock itself instead, which is
        // the more honest reading anyway: taking something hostage is what begins a ransom.
        //
        // Asked after the stack above so the cast that FILLS the cap is already the one that
        // arms — the pet does not spend a further turn topping up a finished bar.
        // `moveIdx < 0` is a hijacked cast (Execution-Override), which owns no slot to
        // seize into.
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
    // Replication Bus (mod) raises the RATE, never the CAP — the slot check above still
    // runs first, so a full board draws no rng with the mod equipped exactly as it draws
    // none without it, and the deterministic stream a duel replays stays identical.
    const int spawnPct = mv->replicaSpawnPct + actor.mods.mag(ModEffect::ReplicaSpawnPct);
    if (static_cast<int>(rng() % 100) >= spawnPct) return;
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
        r.attack = wormAttackerDamage(actor, mv->power, mv->replicaPowerPct);
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
    // Ring-0 Shim (mod) adds to the same sum the traps do, so a shim rewards a trap build
    // instead of substituting for one. Gated to the line by ModDef::requiresLine at equip
    // time, and the passive check above has already returned for anything that isn't a
    // Trojan — so this can only ever read as 0 on a pet that shouldn't have it.
    pct += trojan.mods.mag(ModEffect::ExecOverridePct);
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
    if (actor.ransomTurnsLeft > 0 && --actor.ransomTurnsLeft == 0) {
        // Settled: whatever was seized goes home. Ahead of the payout's early return, and
        // outside the pool check, because a ransom that caught no damage still ends here
        // and a seizure must not outlive the hold that justified it.
        releaseRansomSeizure(actor);
        if (actor.ransomPool > 0) {
            const int due = actor.ransomPool;
            actor.ransomPool = 0;
            actor.health -= due;
            setLast("RANSOM DUE", due, byPlayer, /*charge=*/false);
            return;
        }
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
        actor.chainSlot = -1;    // commanding a move breaks a chain mid-flight
    } else if (actor.channelMoveIdx >= 0) {
        moveIdx = actor.channelMoveIdx;               // committed mid-channel
    } else {
        moveIdx = chooseMove(actor);
    }
    // A chained move's FOLLOW-UP step, committed by last turn's entry cast. Taken before
    // the roll's answer is used, so the step lands on the very next turn — the entry
    // already spent its turn doing something real, which is what separates a chain from
    // the wind-up it replaces. The step is not one of `moves`, so `moveIdx` stays the
    // ENTRY's slot: every mod and passive that keys off which slot was cast keeps
    // pointing at the slot the player actually equipped.
    const MoveDef* chained = nullptr;
    if (actor.chainSlot >= 0 && actor.chainSlot < static_cast<int>(actor.chainFollow.size())) {
        chained = actor.chainFollow[actor.chainSlot];
        if (chained) moveIdx = actor.chainSlot;
        actor.chainSlot = -1;
    }
    const MoveDef* mv = chained ? chained : actor.moves[moveIdx];

    // METAMORPHIC: a wildcard row does not cast itself — it rolls one of the moves this pet
    // could have been. A single branch and not a loop, so a pool that somehow held another
    // wildcard row would resolve it as an ordinary move rather than re-entering the roll;
    // an empty pool leaves `mv` as the row itself, which is the same fall-through a slot
    // with nothing usable in it already takes. Not reachable from `chained`: a follow-up
    // step is a payload, not a slot, and has no pool of its own.
    const WildPool* wild = nullptr;
    if (!chained && moveIsWildcard(*mv) && moveIdx >= 0 &&
        moveIdx < static_cast<int>(actor.wildPools.size())) {
        wild = &actor.wildPools[moveIdx];
        if (const MoveDef* drawn = wildPick(*wild, rng())) {
            mv = drawn;
            // What the picker's LOCK band offers to commit to. Written on the ROLL rather
            // than on a resolved hit: an operator locks the move they watched come up,
            // and a swing the enemy absorbed came up all the same.
            actor.wildPools[moveIdx].lastRolled = drawn;
        }
    }

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

    // POLYMORPH, and the identity a borrowed row brings with it. Both run BEFORE the cast
    // resolves, so the stats this move swings with already include what absorbing it paid
    // — the fighter has become the thing before it does the thing, which is the whole read
    // — and a Worm row spawns on the same turn it grants Replication rather than the next.
    if (wild) actor.linePassives |= wildBorrowedPassives(*wild, mv);
    polymorphAbsorb(actor, mv);
    // Mutation Engine's axis. Recorded on the CAST, the same moment as the absorb, rather
    // than on whichever riders survived the target's defences — what the mod reads is the
    // range of things this fighter has reached for, and a stun the enemy shrugged off was
    // still a stun it had to shrug off.
    if (actor.polymorphic) {
        const uint32_t before = actor.effectsSeen;
        actor.effectsSeen |= moveEffectMask(*mv);
        // Mutation Engine (mod): a KIND this fighter has not reached for before pays the
        // passive's own stat point, the same way an unlearned move does. An amplifier of
        // Polymorph rather than a percentage beside it — which is also the only shape this
        // tier rewards: what leads the band takes turns or refuses death, and a flat
        // attack-power bonus measured worth nothing there however large it was made.
        if (const int per = actor.mods.mag(ModEffect::PolymorphEffectPct); per > 0) {
            int fresh = 0;
            for (uint32_t d = actor.effectsSeen & ~before; d; d &= d - 1) ++fresh;
            polymorphPay(actor, mv->kind, per * fresh);
        }
    }

    applyEffect(actor, target, mv, byPlayer, moveIdx);
    // Tempo refund (MoveDef::speedRefundPct): hand part of an action's worth of gauge back
    // to whoever just spent a turn bracing. Applied HERE rather than inside applyEffect
    // because what is being refunded is the TURN, and the turn belongs to `actor` — an
    // Execution-Override hijack routes the cast to the other side, which resolves through
    // applyEffect directly and must not pay tempo to a fighter that spent nothing.
    if (mv->kind == MoveDef::Kind::Defend && mv->speedRefundPct > 0) {
        float& gauge = byPlayer ? plGauge_ : enGauge_;
        gauge += kSpeedActionThreshold * mv->speedRefundPct / 100.0f;
        // A refund shortens the wait; it never grants an action outright. Without this a
        // large enough refund would cross the threshold on its own and hand the brace a
        // free follow-up turn, which is a different mechanic from the one being paid for.
        if (gauge >= kSpeedActionThreshold) gauge = kSpeedActionThreshold - 1;
    }
    // Hand the slot to this move's follow-up step, if it has one. Set after the entry has
    // RESOLVED, so a cast that killed the target commits nothing, and read on the actor's
    // next turn above. `chained` guards the obvious loop: a step never chains onward, so a
    // pair is two turns and not a track a fighter can never get off.
    if (!chained && moveIdx >= 0 && moveIdx < static_cast<int>(actor.chainFollow.size()) &&
        actor.chainFollow[moveIdx] && outcome_ == Outcome::Ongoing)
        actor.chainSlot = moveIdx;
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
    //
    // Asked of BOTH sides, because a tournament opponent arms its own Exploits and a
    // rally that only worked in the player's seat would be a different ability
    // depending on who drew the crew. Nothing in a wild/boss/Sim fight ever arms one
    // on the enemy, so those fights draw the same conclusion they always did.
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
    const bool playerActed = playerTurn_;
    // An Exploit this side fires itself SPENDS the turn (see Combatant::autoExploit),
    // so it is asked before the move roll and short-circuits it. Nothing else about the
    // turn changes: the streak above already counted it, and the clock + scheduler
    // below still run, because a turn spent arming is still a turn taken.
    Combatant& actor = playerTurn_ ? player_ : enemy_;
    if (!fireAutoExploit(actor, playerTurn_)) {
        if (playerTurn_) resolveTurn(player_, enemy_, /*byPlayer=*/true);
        else resolveTurn(enemy_, player_, /*byPlayer=*/false);
    }
    checkOutcome();
    // The side that did NOT act is the one a death-save is standing over, so it is the
    // one whose clock this turn came off.
    tickCrewExploitClock(playerActed ? enemy_ : player_);
    playerTurn_ = pickNextActor();   // schedule the next actor by relative speed
    return true;
}

bool Combat::fireAutoExploit(Combatant& actor, bool byPlayer) {
    if (actor.autoExploitFired || !actor.autoExploit.label ||
        actor.autoExploit.kind == CrewExploitKind::None)
        return false;
    // The trigger is a fraction of this fighter's OWN max, so an opponent rolled to
    // wait for trouble waits the same number of proportional hits whatever its Health
    // pool is. A 100% threshold is "open with it" and clears on the first turn.
    const int maxH = actor.maxHealth > 0 ? actor.maxHealth : 1;
    if (actor.health * 100 > actor.autoExploitAtHealthPct * maxH) return false;
    actor.autoExploitFired = true;
    armCrewExploit(actor, actor.autoExploit, byPlayer);
    return true;
}

void Combat::tickCrewExploitClock(Combatant& guarded) {
    // Backup Plan B is a death-save, so its clock is the INCOMING turns it covers
    // rather than the guarded fighter's own — the three turns it promises are the next
    // three its opponent gets, which is the only count that describes what was bought.
    // (Every other turn-metered thing here — a DoT, a stun, a ransom bill — ticks on
    // its victim's turns instead, because those are things the victim is living
    // THROUGH rather than a guard standing over it.)
    //
    // A turn the opponent spent stunned, rotting or paying its own ransom still counts:
    // it was a turn, and the caller ticks after the whole turn has resolved, so every
    // early return inside resolveTurn is counted the same as a swing.
    if (guarded.crewExploit.ticking(CrewExploitKind::DeathSaveRally))
        --guarded.crewExploit.turns;
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
                  overrideLockCount() + overrideCrewRows();
    if (n <= 0) return;
    overridePick_ = (overridePick_ + 1) % n;
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
            self.crewExploit.charges += x.magnitude;
            break;
        case CrewExploitKind::DeathSaveRally:
            self.crewExploit.turns += x.magnitude;
            break;
        case CrewExploitKind::ResetStatsAndFloor:
            // Snap the three live stat LEANS back to what the fighter walked in with —
            // in BOTH directions, so this is a decision about timing rather than a free
            // top-up: fire it drained and it restores, fire it buffed and it costs. The
            // floor (statsFloored) then holds them there for the rest of the fight.
            //
            // Two things it deliberately leaves alone. The earned line-stack tracks
            // (Lockout Power, Cipher Defense) are not leans — they are progress the pet
            // cast for, and nothing erodes them, so there is nothing to reset. And
            // maxHealth already drunk from by a steal is not given back: a pool that has
            // been drained cannot be un-drained, and the floor only stops the next sip.
            self.powerMultPct = self.basePowerMultPct;
            self.speed = self.baseSpeed;
            self.dmgReducePct = self.baseDmgReducePct;
            break;
        case CrewExploitKind::MirrorEnemyBuffs:
            break;      // sticky: being armed IS the effect (applyEffect's mitmCopy)
        case CrewExploitKind::None:
            return;
    }
    // "<TAG> xN" popup — or a bare "<TAG>" for a sticky kind, which counts nothing.
    // dmg=0 so the combat screen's red damage number stays hidden; the readout rides in
    // the move-name slot, and the live counter also shows in the B stat panel — all
    // three go through crewExploitLabel(). `byPlayer` is what puts the popup over the
    // right fighter, so an opponent's own Exploit announces itself on its own row.
    crewExploitLabel(itemPopup_, sizeof(itemPopup_), x.kind, x.magnitude);
    setLast(itemPopup_, 0, byPlayer, /*charge=*/false);
}

void Combat::commitOverride() {
    if (!overrideOpen_) return;
    const int moves = overrideMoveCount();
    const int items = static_cast<int>(overrideItems_.size());
    const int locks = overrideLockCount();
    if (overridePick_ < moves) {
        forcedMoveIdx_ = overridePick_;         // forces the player's next move
    } else if (overridePick_ >= moves + items + locks) {
        applyCrewExploit();                     // the crew row (last band)
    } else if (overridePick_ >= moves + items) {
        // LOCK: the slot stops drawing and keeps what it last rolled. Replacing the row in
        // `moves` rather than flagging it is what makes the slot ORDINARY from here —
        // every reader (the roll, this picker, the KIT page, Prowlware's power ranking)
        // sees the committed move with nothing new taught to any of them.
        //
        // Its own chain step is deliberately not committed: a locked cast is a substituted
        // one, and a substituted move hands nothing on, the same rule wildPick's result
        // already answers to. Resolving one would need the registry, mid-fight.
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
        // The free turn is a turn, and costs the player's clock one — a flee is only
        // ever the player retreating, so the guarded side is never in doubt here.
        tickCrewExploitClock(player_);
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

// Replication Bus (mod): what one copy's banked figure is multiplied by. Applied at SPAWN
// like every other term in a copy's value, so a bus equipped mid-run never reaches back and
// pumps copies that are already standing — a separate thing stays separate.
static int replicaWorthMult(const Combatant& parent) {
    return 100 + parent.mods.mag(ModEffect::ReplicaWorthPct);
}

int wormAttackerDamage(const Combatant& parent, int movePower, int pct) {
    // A share of the move that made it, scaled by the same attack lean the parent's own
    // damage is scaled by — so a Bad-branch worm's copies hit like it does — and by the
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
