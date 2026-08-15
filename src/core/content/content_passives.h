// content_passives.h — per-LINE combat passive tuning.
//
// A line's signature ability is a bespoke hook in Combat (ransomArmRolls,
// execOverrideChance, syncWormSpeed, the steal-track siphon + bubble-bite in
// applyEffect), gated on Combatant::line or (for the steal track) purely on which
// MoveDef fields a line's rows populate. This file holds the CONSTANTS those hooks
// read — grouped by line, alongside content_moves.cpp's per-move magnitudes, not in
// tunables.h: these numbers only ever move together with a line's move balance, and
// tuning one without the other breaks the passive's math. Cross-cutting engine
// constants that apply the same way regardless of line (kLevelDmgReduceMaxPct,
// kSpeedActionThreshold, ...) stay in tunables.h.
#pragma once

namespace mal {

// --- Ransomware — Ransom Note --------------------------------------------------
// A stage-scaled chance, rolled at the start of each of the ransomer's own turns, ARMS a
// ransom window. The window stays open until a hit lands in it: that hit's DAMAGE is held
// hostage instead of landing, banked into a pool (Combatant::ransomPool) that resets its
// countdown to kRansomHoldTurns of the ransomer's own turns, and the window closes. When
// the countdown runs out the whole pool lands in one blow — so a lucky run buys real
// fighting time at the price of a cliff that can kill outright. Only damage is deferred;
// a hit's riders (DoT, stun, steals, armor rot) apply on impact as usual.
//
// Deciding the window up front (rather than per incoming hit) is what makes the passive
// safe in a linked duel: both devices resolve the same seeded fight from the same
// per-turn roll, with nothing about it depending on how many actions the opponent's
// speed happens to buy inside the window.
//
// Indexed by Stage (Boot/Process/Script/Daemon); Boot's 0% is inert (an egg can't fight).
// Because a window persists until it catches something, this is very nearly "what share of
// the hits the pet takes get held" — which is also what a player perceives, so it is the
// dial to turn if the passive reads too rare or too constant. It can afford to be generous:
// the pool is a DEFERRAL, not a damage cut, and total damage taken is unchanged unless the
// fight ends before the bill comes due.
constexpr int kRansomArmPctByStage[4] = {0, 40, 55, 70};
constexpr int kRansomHoldTurns = 3;   // also sizes the combat screen's blip row

// --- Phishing — steal track + Obfuscation-bubble passives ---------------------
// Floors for the generic per-field siphon in Combat::applyEffect (MoveDef's steal*
// fields): a power siphon can't drag powerMultPct below kStealPowerFloorPct; a speed
// siphon can't drag speed below kStealSpeedFloor (the target keeps some action tempo).
constexpr int kStealPowerFloorPct = 20;
constexpr int kStealSpeedFloor = 1;

// Feed-frenzy: a landed steal-attack while the caster's Obfuscation shield (shieldHp)
// is up heals it this permille of the shield's current HP (min 1) — FLOAT, not int, so
// a small shield still contributes a meaningful heal instead of truncating to 0.
constexpr float kFrenzyHealPermille = 15.0f;   // 0.X% of live shieldHp, min 1

// Perfect Bite: on a landed hit whose move sets stealSpeedPct and/or
// stealCurrentHpPct, these only fire at all while the caster's bubble is up (shieldHp > 0)
constexpr int kPhishingBiteChancePctByStage[4] = {0, 32, 48, 64};

// Frenzy lean: once the bubble has been stacked past the pet's own max Health — the
// point the shield bar starts churning, so the screen has already promised something
// changed — more bubble buys less than a bite does, and Combat::chooseMove starts
// re-rolling Defend picks into Attack ones. The chance ramps with how far past max
// Health the pool was stacked, reaching kPhishFrenzyLeanMaxPct at this multiple of it.
//
// It reads the pool's HIGH-WATER mark (phishShieldPeak), not its live size, so the lean
// does NOT ease off as the bubble is chewed back down — a pet that banked a wall commits
// to spending it. The ratchet releases only when the bubble actually POPS, which is the
// off-ramp: stop bracing, the enemy eventually breaks through, and the pet returns to
// mixed play. Re-casting the bubble before it pops holds the frenzy open, which is what
// makes a committed override (Combat::commitOverride) worth spending on this line.
constexpr int kPhishFrenzyLeanFullMult = 2;    // peak >= this x maxHealth -> full lean
constexpr int kPhishFrenzyLeanMaxPct = 100;

// --- Trojan — Execution-Override + trap cap ------------------------------------
// On an enemy's move-pick a Trojan has kExecOverrideBasePct chance PLUS the sum of
// its armed traps' trapPassiveBonusPct to hijack that move and turn it on the enemy.
// Low base, high with traps held — so maintaining the trap stack IS the way to make
// it fire. kTrojanTrapCap caps how many Trojan Defend traps can be armed at once (a
// few pips) — also sizes Combatant::trojanTraps' fixed array.
constexpr int kExecOverrideBasePct = 8;
constexpr int kTrojanTrapCap       = 3;

// --- Worm — Shared Resources + the replication slots ---------------------------
// Shared Resources has two halves that only make sense together.
//
// SPEED LOCKSTEP: a worm's speed is not its own — it is continuously assigned the
// OPPONENT's (Combat::syncWormSpeed, re-applied at every scheduling tick). Actions are
// dealt by RELATIVE speed (Combat::pickNextActor), so a worm can never be out-actioned:
// whatever buffs or siphons the other side, the gauges stay level and the fight
// alternates. It costs the worm every speed lever in the game — nothing it equips or
// steals can buy it an extra action either. The fiction is the resource share: a worm
// duplicating on your machine runs on your cycles, so the two of you slow down together.
// No constant needed for it; matching exactly IS the passive.
//
// REPLICATION SLOTS: the screen is 224px of active canvas and a replica is drawn art, so
// replication is capped by SLOTS rather than left open. One pool shared by both kinds,
// which is what makes the split a decision — a slot spent on a defender is a slot no
// attacker can use.
constexpr int kWormReplicaSlots = 3;

// A replica's magnitude comes from the OTHER kind's live count, which is the line's whole
// strategy: attackers are worthless without defenders behind them and defenders are thin
// without attackers to guard. The first replica of either kind would multiply by zero, so
// the count floors here — a lone replica is worth exactly its base rather than nothing,
// and every one after it is the real multiplier.
constexpr int kWormReplicaMultFloor = 1;

// Incoming attacks pick their victim among the parent and its replicas by weight
// (wormTargetWeights, combat.h). Defenders draw hardest — a body thrown in front is what
// a defender IS — with attackers next and the parent itself the rarest target, so a full
// board genuinely hides the worm. These are the numbers to turn if the parent reads
// either untouchable or unprotected.
constexpr int kWormTargetWeightParent   = 1;
constexpr int kWormTargetWeightAttacker = 2;
constexpr int kWormTargetWeightDefender = 4;

}  // namespace mal
