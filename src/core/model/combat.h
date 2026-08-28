// combat.h — the shared autonomous combat engine.
//
// Battles run themselves: on its turn each combatant ROLLS one of its moves off
// its attack/defend lean (the lean IS the slot mix) and never repeats the
// same move twice in a row. The human's agency is the META layer — the
// loadout, the evolution-unlocked slots, the MODS passives, and ONE Exploit
// override per battle (A+C) that commands the next move and may break the
// no-consecutive rule. Health is transient (resets each fight, never
// persisted; max scales with Stage). Resolution is deterministic from a
// seed so tests + replays are reproducible.
//
// The engine is shared: Sim-Battle (PRACTISE, safe stakes) drives it now; EXPL
// wild encounters (live stakes) reuse it later — the only difference is the
// `stakes` flag (whether a loss touches Fragmentation).
#pragma once

#include <cstdint>
#include <vector>

#include "core/content/content_crews.h"     // CrewExploitKind — the crew Exploit vocabulary
#include "core/content/content_passives.h"  // kTrojanTrapCap sizes trojanTraps below;
                                             // kPolymorphAbsorbCap sizes absorbed;
                                             // kRansomHoldTurns sizes the ransom countdown;
                                             // kWormReplicaSlots sizes wormReplicas
#include "core/content/defs.h"
#include "core/model/mod_state.h"           // ModStateSet — the equipped mods' per-fight state

namespace mal {

class ContentRegistry;
class Loadout;        // MODS — combat passives
class MoveLoadout;    // equipped moves

// The per-fight state of an ARMED crew Exploit — one block on the Combatant rather than a
// field per ability, so the crew roster (content_crews.h) can grow without widening
// Combatant. One Exploit at a time; re-arming the same kind stacks onto its counter.
// Transient, wiped with the Combatant each fight.
struct CrewExploitState {
    CrewExploitKind kind = CrewExploitKind::None;
    int charges = 0;    // charge-metered kinds (NegateNextHits, PowerByDamageDealt)
    int turns = 0;      // duration-metered kinds (DeathSaveRally)
    // The three metering shapes: charge, turn, and STICKY (being the armed kind is the
    // whole condition).
    bool armed(CrewExploitKind k) const { return kind == k && charges > 0; }
    bool ticking(CrewExploitKind k) const { return kind == k && turns > 0; }
    bool holds(CrewExploitKind k) const { return kind == k; }
    // Whether the armed Exploit still has anything left to do — what the combat screen's
    // readout asks, so a spent Exploit stops reporting without anything clearing `kind`.
    bool live() const {
        return kind != CrewExploitKind::None &&
               (charges > 0 || turns > 0 || crewExploitIsSticky(kind));
    }
    // The number beside the tag: whichever counter this kind meters out of, and 0 for a
    // sticky kind, which crewExploitLabel then renders without an "xN".
    int count() const { return charges > 0 ? charges : (turns > 0 ? turns : 0); }
};

// An Exploit as OFFERED to a fighter — the name it is announced under, the mechanic, and
// the one number that mechanic meters. A combatant carries one it may fire on its own
// initiative (`autoExploit`) as well as one a human may commit from the picker
// (Combat::openOverride). The player's comes from their crew (content_crews.h); an AI's is
// rolled by whatever built the fighter. label == nullptr means "no Exploit".
struct CrewExploit {
    const char* label = nullptr;
    CrewExploitKind kind = CrewExploitKind::None;
    int magnitude = 0;
};

// A move the Ransomware line is holding hostage. Once the Cipher stack is FULL, the next
// brace stops merely absorbing: it SEIZES the attack that hits it, and the pet swings that
// move from the seized slot until the ransom comes due and it hands it back. The line's
// wall becomes its win condition, in the line's own vocabulary.
//
// The seized move goes into `Combatant::moves` in place of the brace, so the roll, the
// Exploit picker and the readouts all see it without learning what a seizure is.
// `heldFollow` parks the chain step: what was seized is a payload, not the toolkit around
// it, so a seized chain entry commits no follow-up.
struct RansomSeizure {
    bool armed = false;                   // a full Cipher stack met a live ransom
    int slot = -1;                        // which of the pet's own slots is occupied
    const MoveDef* heldMove = nullptr;    // ...and what is waiting to come back to it
    const MoveDef* heldFollow = nullptr;
    bool holding() const { return heldMove != nullptr; }
};

// One live copy a Worm has replicated into a replication slot (Combatant::wormReplicas).
// NOT a Combatant: a replica never takes a turn, rolls a move or enters the speed
// scheduler — it is a piece of its parent's state that happens to be drawn separately. A
// third actor with its own initiative would be a third thing for a linked duel's two RNG
// streams to disagree about (core/model/pvp_battle.h).
//
// So a replica does two things: it SOAKS an attack aimed at it (wormTargetWeights) and, if
// an attacker, PILES onto its parent's swings (wormReplicaDamage). Transient, per-fight.
struct WormReplica {
    bool defender = false;  // spawned by a Defend move (soaks); else an attacker (piles on)
    int health = 0;
    int maxHealth = 0;
    // An attacker's damage per parent swing, banked whole at spawn; 0 on a defender. The
    // parent's attack lean and the defenders standing at that moment are already in this
    // number (wormAttackerDamage), so nothing afterwards moves it.
    int attack = 0;
};

// One wildcard slot's draw pool (MoveDef::drawLineA/B), resolved when the Combatant is
// built so the turn engine never needs a registry.
//
// THREE BANDS IN ONE VECTOR: `rows` holds the generic entries first, then line A's, then
// line B's, cut by the two indices below — two integers per slot instead of three
// allocations. The roll picks a band by weight, then an index inside it
// (content_passives.h). Each line's passive flags ride along from the same registry read,
// which is what lets a cast GRANT them without Combat looking a line id up. An unknown line
// contributes no rows and its weight falls back to generic.
struct WildPool {
    std::vector<const MoveDef*> rows;
    int genericEnd = 0;              // rows[0, genericEnd)        — the shared roster
    int lineAEnd = 0;                // rows[genericEnd, lineAEnd) — line A's track
                                      // rows[lineAEnd, size)       — line B's track
    LinePassives passivesA = 0;
    LinePassives passivesB = 0;
    // What this slot rolled on its last turn — the move the picker's LOCK band offers.
    // Null until the slot has fired once: an operator locks something they SAW.
    const MoveDef* lastRolled = nullptr;
    bool empty() const { return rows.empty(); }
};

// A combat participant. moves[0] is always the innate DEFAULT (so a pet is never
// actionless); moves[1..] are the unlocked equipped slot moves. Mod
// passives (Firewall / Clock-Speed / RAID Mirror) ride on the player side.
struct Combatant {
    const char* name = "";
    const char* spriteName = "";  // SPR_PET_* idle frame reused for the combatant
    int diffPips = 0;            // UI_DIFFICULTY_PIPS (enemy); 0 = player row
    int level = 0;                // depth level, carried from CombatEnemy
                                   // (0 unless hasLevel — wild encounters only)
    bool hasLevel = false;        // false = unranked (Sim dummy / boss) — combat
                                   // screen shows "???" instead of a number
    int maxHealth = 0;
    int health = 0;
    // Initiative (Clock-Speed Boost raises it). FLOAT because a Phishing siphon steals a %
    // of CURRENT speed, which int arithmetic truncates to 0 once speed nears the floor.
    float speed = 0;
    int powerMultPct = 100;     // attack-power lean (Good/Bad branch)
    int basePowerMultPct = 100; // powerMultPct at fight start, after the gamble roll; the
                                // combat screen diffs the live lean against it to show a siphon
    float baseSpeed = 0;        // speed at fight start, diffed the same way
    int fragMultPct = 100;      // loss-Frag multiplier (Bad-branch hook)
    int enemyDamageMultPct = 100;  // wild-encounter offense buff (challenge pass);
                                    // 100 = neutral (player + bosses + Sim dummies)
    std::vector<const MoveDef*> moves;

    int defenseMultPct = 100;   // scales DEFEND-move brace magnitude (Defense stat +
                                // Cipher scaling); 100 = neutral (enemies, Sim).

    // Line identity — what move affinity matches on. Read once at fight start, same
    // timing as MODS.
    Stage stage = Stage::BootSector;
    const char* line = nullptr;
    // The passives that line carries, resolved off its row once at build time
    // (makePlayerCombatant), which is why the turn engine never names a line. Empty for a
    // combatant with no line. Always set through setLine(): the id and the flags are two
    // halves of one fact and must not be able to disagree.
    LinePassives linePassives = 0;

    // Adopt a line — the id and the passives its family row carries — in one call. An
    // unknown or null line clears both.
    void setLine(const ContentRegistry& reg, const char* lineId);

    // The creature this combatant IS, when it is one — the fight reads its authored clips
    // (CreatureDef::clips) to pose the sprite. A borrow; a registry row outlives any fight.
    // Null for a combatant built from a sprite-named spec (makeEnemyCombatant), which the
    // combat screen draws on row 0, the single row every stand-in sheet ships.
    const CreatureDef* creature = nullptr;

    // Equipped MOD passives, keyed by the ModEffect that declares them (mod_state.h).
    // Effects that simply move a base stat (PowerPct, MaxHealth, …) are already folded
    // into the fields above by makePlayerCombatant; this holds the ones that stay live
    // for the fight — caps, thresholds, one-shots.
    ModStateSet mods;

    // Transient defend state. The two Defence investment tiers are resolved once by
    // applyLevelStatPoints, and are 0 for any fighter that has not committed to the stat.
    int pierceResistPct = 0;    // cuts an incoming attack's own armorPiercePct
    // % of an unspent brace that carries to the next hit. Every fighter starts on the
    // baseline (enemies included, hence the default); Defence investment adds to it.
    int braceRetainPct = kBraceRetainBasePct;
    int dmgReducePct = 0;       // Firewall Patch / TPM Chip — % incoming damage cut
    int baseDmgReducePct = 0;   // dmgReducePct at fight start; the third live stat LEAN,
                                // with basePowerMultPct and baseSpeed
    bool mirrorFired = false;   // set the turn a hit is fully negated (a brief flash);
                                 // also suppresses that attack's stun/DoT riders
    bool itemShield = false;    // Backup Drive's timed buff — a DEATH-SAVE, not a hit
                                 // negator: every hit lands in full and the drive is read
                                 // only once the pet is down (restoreFromBackup). Armed by
                                 // the Game off an item buff, so it lives here rather than
                                 // in `mods`, which the mod table owns
    // What the drive DID, held for the rest of the fight. Game reads it at the end: any
    // value but None burns the buff's save-side timer early. One field rather than a
    // fired/worked pair, so "spent but didn't work" cannot disagree with itself.
    enum class BackupUse : uint8_t {
        None,          // no drive was armed, or it was never needed
        Restored,      // spent, and the pet got back up
        Overwhelmed,   // spent, and half of max still wasn't enough — the pet went down
    };
    BackupUse backupUse = BackupUse::None;
    // The armed crew Exploit (see CrewExploitState). Its charges are spent BEFORE the RAID
    // Mirror mod, so that one-shot stays held for after they run out. Armed either by a
    // human at the picker (commitOverride) or by this side firing its own (autoExploit) —
    // both paths converge on one applier.
    CrewExploitState crewExploit;

    // --- The Exploit this side fires on its OWN initiative ------------------------
    // What a fighter with nobody at the buttons carries: a rolled Exploit plus the moment
    // it commits. The TOURNAMENT arena is what has these; every other PVE enemy leaves
    // `autoExploit.label` null and never draws for it. Firing COSTS the turn, exactly as a
    // human's one Exploit use costs the move they'd have forced — which is also what gives
    // it a beat on the combat screen.
    CrewExploit autoExploit;
    // The Health it waits for, as a % of its own max: 100 fires on its first turn, 30 waits
    // for real trouble. Checked on this side's own turn, so a fighter killed first never
    // gets to spend it.
    int autoExploitAtHealthPct = 100;
    bool autoExploitFired = false;
    int guard = 0;              // pending mitigation from a defend move (one-shot)
    // Poisoned data (MoveDef::poolRetaliateDot): what a strike on this pet's live bubble
    // plants on the striker. Armed by the pool cast, and read on the attack path.
    int poolDotDamage = 0;
    int poolDotTurns = 0;
    int shieldHp = 0;           // Obfuscation shield pool (Phishing) — a poolable
                                // second health bar; absorbs before real Health, stacks
                                // on recast, pops when overrun. Transient (per-fight).
    int dotPerTurn = 0;            // active DoT on this combatant: damage per turn-start...
    int dotTurnsLeft = 0;          //   ...for this many more turn-starts (0 = none)

    // Line-move stacking buffs (transient, wiped each fight). Lockout-track hits
    // grow stackPowerBonus (added to the effective attack mult); Cipher-track casts
    // grow stackDefenseBonus (added to the effective damage cut, under the 85% clamp).
    int stackPowerBonus = 0;
    int stackDefenseBonus = 0;

    // Feeding-frenzy combo (Combat::applyEffect): phishStreak counts this combatant's run
    // of steal-attack casts made with the bubble up; phishComboBonus is the flat damage
    // those casts have permanently banked this fight. Unlike stackPowerBonus it is flat,
    // uncapped, and grows by the run length itself. Casting the bubble HOLDS the run — the
    // whole line answers to one question, "is the bubble up?" — and swinging while exposed
    // breaks it.
    int phishStreak = 0;
    int phishComboBonus = 0;

    // High-water mark of shieldHp since the pool last popped, driving the frenzy lean
    // in Combat::chooseMove (content_passives.h sizes it). Ratchets up as the bubble is
    // stacked and is cleared ONLY when the pool is overrun, never when it merely shrinks.
    int phishShieldPeak = 0;

    // STUN (a landed hit's lockTurns rider) — set ON THE VICTIM. While
    // lockedTurnsLeft > 0 the victim burns its turn doing nothing, then it lifts.
    int lockedTurnsLeft = 0;
    // ...and what a landed one leaves behind: one resist point per turn it froze, shed one
    // per turn this fighter gets to act (Combat::resolveTurn). The next stun rolls against
    // the pile (Combat::stunLands), so being chain-stunned buys the way out of it.
    int lockResist = 0;

    // Ransom Note (Ransomware passive) — all three live on the RANSOMER (the pet that
    // owns the passive), never the attacker. `ransomArmed` is re-rolled at the start of
    // every one of this side's turns (Combat::ransomArmRolls) and holds for the window
    // until its next turn; while it's up, incoming hits divert their damage into
    // `ransomPool` and reset `ransomTurnsLeft` to kRansomHoldTurns. The countdown ticks
    // one per turn of THIS side (not per incoming action), and the pool lands whole when
    // it hits 0. Transient (per-fight), like every other combat field here.
    int ransomPool = 0;
    int ransomTurnsLeft = 0;
    bool ransomArmed = false;
    // The seizure half of the same passive (see RansomSeizure). Lives on the RANSOMER too.
    RansomSeizure ransomSeizure;

    // Trojan traps (Trojan line) — a stack of armed Trojan Defend moves. Each incoming
    // enemy attack TRIGGERS the top trap (evasion + rebound + armor-rot, applyEffect);
    // armed traps also raise this side's Execution-Override hijack chance
    // (execOverrideChance). Transient (per-fight); only ever set on a Trojan pet.
    const MoveDef* trojanTraps[kTrojanTrapCap] = {};
    int trojanTrapCount = 0;

    // Worm replicas (Worm line) — the live occupants of this side's replication slots,
    // spawned by its own moves' replicaSpawnPct (see WormReplica above for what one is
    // and what it can do). Packed: a replica killed by a redirected hit is swapped out
    // and the count drops, freeing its slot for the next spawn. Transient (per-fight);
    // only ever non-empty on a Worm pet.
    WormReplica wormReplicas[kWormReplicaSlots] = {};
    int wormReplicaCount = 0;

    // Per-fight turn state (no-consecutive + channel wind-up).
    int lastMoveIdx = -1;
    int channelMoveIdx = -1;    // mid-channel move (-1 = not channelling)
    int channelLeft = 0;        // turns until the channel detonates

    // --- Metamorphic: the wildcard pools, and what has been absorbed ----------------
    // One pool per SLOT (parallel to `moves`, like chainFollow), because each wildcard row
    // names its own pair of lines — a kit holding two of them is holding two different
    // pools, and merging them would quietly hand every row the union of what its neighbour
    // reaches. A slot whose move is not a wildcard leaves an empty pool, which is also what
    // makes `wildPools` safe to index with any moveIdx.
    std::vector<WildPool> wildPools;

    // POLYMORPH's memory: the distinct moves this fighter has cast, which is all the
    // passive asks. A fixed array rather than a set because a fight must not reach the
    // heap; past the cap a cast simply stops being recorded and stops paying.
    const MoveDef* absorbed[kPolymorphAbsorbCap] = {};
    int absorbedCount = 0;
    // The other axis, and what Mutation Engine (ModEffect::PolymorphEffectPct) pays on:
    // which distinct effect KINDS this fighter has cast, as a bitmask (moveEffectMask).
    uint32_t effectsSeen = 0;
    // Whether this fighter plays the metamorphic game at all — set when any slot holds a
    // wildcard row. The passive gates on this rather than on a line id, so the engine never
    // learns a line's name and a pet equipping none of the line's rows runs no passive.
    bool polymorphic = false;

    // Chained moves (MoveDef::chainNextId). Parallel to `moves`: chainFollow[i] is the
    // step slot i hands off to, or nullptr for an ordinary move. Resolved once when the
    // Combatant is built (resolveChains) so the turn engine never needs a registry.
    std::vector<const MoveDef*> chainFollow;
    // The slot whose follow-up step is COMMITTED to this fighter's next turn, or -1. It
    // bypasses the move roll the way a channel does, so the step lands on the very next
    // turn; a chain waiting for a second random roll would almost never complete. Cleared
    // by an Exploit override or death, after which the pet just rolls the entry again.
    int chainSlot = -1;

    // Backup Drive's death-save (itemShield): burn the armed drive and add half of max
    // Health back. Called from one place, Combat::checkOutcome, on a combatant just judged
    // overwhelmed — so it reads state and knows nothing about what put the pet there. Half
    // of max does not guarantee survival: a deep enough hole still leaves it under 0. Only
    // Game::buildPlayerCombatant arms one, so this is a no-op on every enemy.
    void restoreFromBackup();
};

// Prowlware's multiplier for `moves[moveIdx]`: the rank of that move's Attack power among
// the DISTINCT Attack powers in `moves`, ascending, ties sharing a rank. 0 for a Defend
// move or an out-of-range slot, so a kit with one attack tier pays nothing — the mod
// rewards a genuine power spread.
int attackPowerRank(const std::vector<const MoveDef*>& moves, int moveIdx);

// --- Worm replication, the pure half ------------------------------------------------
// Total functions of a Combatant's own replica array, so the balance they encode is
// assertable without resolving a fight.

// How many of `c`'s live replicas are of the given kind.
int wormReplicaCount(const Combatant& c, bool defenders);

// Whether `m`'s ENTIRE contribution is the one-shot `guard` brace — a Defend row that
// pools a shield, arms a trap, spawns a defender or stacks the Cipher cut is worth a turn
// whatever the brace situation. `guard` is one-shot and discards overkill, so re-casting a
// pure brace onto a live one buys nothing, and Combat::chooseMove re-rolls off it.
bool braceOnlyDefend(const MoveDef& m);

// The odds a stun rider aimed at `c` right now would freeze it: 100 while it holds no lock
// resistance, falling kLockResistStepPct per banked point and never past
// kLockResistFloorPct. The combat screen reads out what Combat::stunLands rolls against.
int stunLandPct(const Combatant& c);

// The Phishing pool siphon, 0..kPhishPoolSiphonMaxPct: what the LIVE Obfuscation pool adds
// to this pet's two bubble-gated steals, as a percentage of their authored value. Scaled
// against the stage body, so a bubble worth a whole body doubles the bite. 0 with no bubble
// up. This is the line's conversion from defence to offence — the bubble sizes what it takes.
int phishPoolSiphonBonusPct(const Combatant& c);

// The Phishing frenzy lean, 0..kPhishFrenzyLeanMaxPct: how strongly an over-stacked
// Obfuscation bubble pushes `c` off bracing and onto biting (Combat::chooseMove re-rolls
// Defend picks against it). 0 until a shield has been pooled past max Health.
int phishFrenzyLeanPct(const Combatant& c);

// What one ATTACKER is worth at the moment it spawns: its share of the move that made it,
// times the parent's attack lean, times the defenders already standing (floored at
// kWormReplicaMultFloor). The cross-multiplier is read here and never again, which is what
// makes spawn ORDER the decision the line is played on — cover first and the teeth that
// follow are worth more, teeth first and they are worth their base forever.
int wormAttackerDamage(const Combatant& parent, int movePower, int pct);

// The damage `c`'s ATTACKER replicas add to one of its parent's swings — the sum of what
// each banked when it spawned, and nothing more.
int wormReplicaDamage(const Combatant& c);

// A DEFENDER's Health at spawn: `pct` of the parent's maxHealth times the live ATTACKER
// count (same floor). Banked rather than recomputed — a defender's Health is a pool being
// chipped and cannot be restated once a hit has come out of it.
int wormDefenderHealth(const Combatant& parent, int pct);

// The per-target draw weights an incoming attack picks its victim from: index 0 is the
// PARENT and index 1+i is replica i, so the vector is always 1 + c.wormReplicaCount long.
// Weights come from content_passives.h.
std::vector<int> wormTargetWeights(const Combatant& c);

// The GRUDGE, 0..kLedgerGrudgeMaxPct: how much Extortion Ledger's power bonus is scaled by
// what this pet holds unsettled, as a percentage of that bonus. Measured against the stage
// body, so it says how DEEP the pool is rather than how levelled the pet is.
int ledgerGrudgePct(const Combatant& c);

// --- Polymorph, the pure half ---------------------------------------------------------

// Has `c` already cast `m` this fight? The whole of what the passive asks, and the reason
// `absorbed` is a list rather than a counter.
bool polymorphHasAbsorbed(const Combatant& c, const MoveDef* m);

// Record `m` as cast and pay for it, if it is new to `c` and `c` plays this game at all.
// Returns true when it actually paid, which the combat screen's popup reads.
//
// The payout MUTATES the fighter's live stats rather than being derived on read: begin()
// captures basePowerMultPct/baseSpeed/baseDmgReducePct and the stat panel draws
// live-against-base, so moving the real field is what makes an absorbed stack visible.
// max-Health could not be derived in any case — a pool being chipped cannot be restated.
bool polymorphAbsorb(Combatant& c, const MoveDef* m);

// Pay `points` stat points' worth in `kind`'s currency — Attack buys Power and Speed,
// Defend buys Defense, brace magnitude and max-Health. The one place Polymorph and its
// amplifier spend, so a mod that adds to the passive cannot drift from what the passive
// itself pays.
void polymorphPay(Combatant& c, MoveKind kind, int points);

// Which distinct EFFECT KINDS `m` carries, as a bitmask — one bit per rider a row can
// declare, so two plain-damage moves share a mask of 0 while one loaded row sets several
// bits. The axis Mutation Engine pays on, deliberately not the one Polymorph counts: a kit
// of plain swings feeds the passive and feeds the mod nothing. Derived from fields a row
// already declares, so no table has to be kept in step with the roster. The steal track
// spends a bit per STAT, a Speed siphon and a max-Health siphon being different things.
uint32_t moveEffectMask(const MoveDef& m);

// How many distinct effect kinds `c` has cast this fight — popcount over effectsSeen.
int polymorphEffectCount(const Combatant& c);

// Which row a wildcard slot casts this turn: `roll` is the caller's own rng draw, taken
// once, and the bands are weighted by content_passives.h's kWildSource*Pct. Returns nullptr
// for an empty pool, which the caller reads as "cast the wildcard row itself". Pure, so
// both devices of a duel drawing the same number land on the same row.
const MoveDef* wildPick(const WildPool& pool, uint32_t roll);

// The passives a cast of `m` grants, given the pool of the slot that cast it: line A's
// flags if `m` belongs to line A, line B's if it belongs to line B, and none otherwise.
// A string compare against two ids rather than a registry lookup, which is what keeps the
// grant on the cast path without Combat learning what a registry is.
LinePassives wildBorrowedPassives(const WildPool& pool, const MoveDef* m);

// Resolve `roll` (the caller's own rng draw) against wormTargetWeights: -1 for the parent,
// else the index into c.wormReplicas that eats the hit. Pure, so the same roll names the
// same victim on both devices of a duel.
int wormTargetPick(const Combatant& c, uint32_t roll);

// Enemy archetype spec — what the engine needs to build an enemy Combatant.
// Sim-Battle Dummy tiers use it now; sector malbeasts reuse the shape for EXPL.
struct CombatEnemy {
    const char* name;
    const char* spriteName;             // SPR_PET_* reused for the enemy frame
    int diffPips;
    int maxHealth;
    int speed;
    std::vector<const char*> moveIds;   // resolved against the registry
    bool isWild = false;                // EXPL wild malbeast → the challenge buff
                                        // (kWildEnemy*Pct) applies; bosses/Sim don't
    // The one move that is THIS creature's, whatever depth it was met at. Beside moveIds
    // rather than in it because the depth ladder REPLACES that list (applyWildSubAreaRamp),
    // so a kit written into the row would survive only the shallowest sub-area. A win
    // teaches out of the beaten enemy's whole kit (rollEnemyMoveDrop, game_explore.cpp), so
    // a signature is the legible reason to hunt one creature over another; being generic
    // (MoveDef::line null) it drops to whatever the player hatched.
    const char* signatureMoveId = nullptr;
    int level = 0;                      // depth level (global sub-area rung);
                                        // set by applyWildSubAreaRamp, drives the
                                        // level-difference XP scaling (wildWinXp). 0 =
                                        // unranked (Sim dummies, bosses use their own).
    // The other two of the four stats a PET levels, so a scaled enemy (the DeepWeb dive)
    // can hit harder rather than leaning entirely on its move rows. The defaults are the
    // neutral values, which is what every authored Health/speed/moves row wants.
    int powerMultPct = 100;             // attack lean, same units as Combatant's
    int dmgReducePct = 0;               // % incoming-damage cut, same units + same clamp
    bool hasLevel = false;              // true once applyWildSubAreaRamp / applyDeepWebScale
                                        // has stamped `level`; Sim dummies + bosses never set
                                        // this, so the combat screen renders "???" for them
                                        // instead of a misleading "Lv 0".
};

// An item offered in the Exploit picker's USE-ITEM section.
// The Game builds these from the live inventory (items with combatHeal>0) when the
// picker opens; Combat applies the Health patch itself (it's combat state) and
// reports the committed id back so the Game can consume the stack + apply the
// item's own out-of-combat effect. Pointers are borrowed content ids (not copied).
struct OverrideItem {
    const char* id;      // stable content id
    const char* label;   // display name for the picker row
    int heal;            // transient combat Health restored on use
};

// The BANDS of the Exploit picker, in the order their rows sit in the flat list, so a
// band's rows are one contiguous run and every flat index Combat::commitOverride reads
// falls inside exactly one of them. They exist as a NAVIGATION level because the item and
// lock bands are unbounded — walked as one flat list they bury the short bands behind a lap
// of the long one, on a screen with room for ten rows.
enum class OverrideBand : uint8_t { Move, Item, Lock, Crew };
constexpr int kOverrideBands = 4;

// The band's own name — the level-1 row, and the header over its rows at level 2.
const char* overrideBandName(OverrideBand b);

// A Worm replica destroyed by the turn that just resolved. Replicas are packed out of
// Combatant::wormReplicas the instant they die, so without this trace the death would leave
// nothing on screen. Enough for the combat screen to play the glyph's dissolve frames over
// the freed slot. Overwritten by the next resolved turn, never accumulated.
struct WormKill {
    bool happened = false;
    bool onPlayer = false;   // whose board lost the copy (Combat's slot, not the seat)
    bool defender = false;   // which of the two glyphs dissolves
};

class Combat {
public:
    enum class Stakes { Live, Safe };       // live = +Frag on loss; safe = nothing
    enum class Outcome { Ongoing, Win, Lose, Fled };

    // Build a battle. The enemy always resets to full Health, and so does the player
    // unless `carryPlayerHealth >= 0` — the boss-gauntlet carry, where consecutive rounds
    // run with no heal between. `seed` makes resolution deterministic. `forceEnemyFirst`
    // overrides the speed-based initiative roll: the failed-flee penalty.
    void begin(const Combatant& player, const Combatant& enemy, Stakes stakes,
               uint32_t seed, bool forceEnemyFirst = false,
               int carryPlayerHealth = -1, int exploitUses = 1);

    // Advance one action beat (one actor's turn). No-op once resolved or while the
    // override picker is open (the fight pauses for the human). Returns true if a
    // turn actually resolved.
    bool step();

    // The A+C Exploit override — N uses per battle --------
    // The rows are one flat list in band order (OverrideBand): the player's moves
    // first, then any combat-usable items the Game supplied, then the metamorphic LOCK
    // rows, then the crew Exploit row (if any). A pick < moveCount forces that move; a
    // pick inside the item band uses that item; a lock row freezes its slot; the last
    // row fires the crew Exploit. Committing any of them spends one Exploit use.
    //
    // The flat index is what a commit reads, but it is NOT how the cursor walks — the
    // picker is two levels (overrideAtBands below), and `overridePick` names the row
    // under the cursor at both of them.
    bool overrideReady() const { return overrideUsesLeft_ > 0; }
    int overrideUsesLeft() const { return overrideUsesLeft_; }
    int overrideUsesTotal() const { return overrideUsesTotal_; }
    bool overrideOpen() const { return overrideOpen_; }
    // open — does NOT spend. `crew` defaults to "no crew" (no extra row).
    void openOverride(std::vector<OverrideItem> items = {}, CrewExploit crew = {});
    void cycleOverride();       // A inside the picker → next entry (moves, items, crew)
    int overridePick() const { return overridePick_; }
    int overrideMoveCount() const;                // picker rows that are moves
    const std::vector<OverrideItem>& overrideItems() const { return overrideItems_; }
    // The crew row this picker is offering (label == nullptr when there is none) and
    // whether it contributes a row at all — the render walks the same three bands.
    const CrewExploit& overrideCrew() const { return crewExploit_; }
    int overrideCrewRows() const { return crewExploit_.label ? 1 : 0; }
    // --- The metamorphic LOCK band -------------------------------------------
    // Rows for every wildcard slot that has already fired once. Committing one spends the
    // Exploit use to freeze that slot on the move it last rolled: the slot stops drawing
    // and becomes an ordinary one for the rest of the fight.
    //
    // Empty for every fighter not running Polymorph, and empty in a duel because the
    // picker never opens there at all (exploitUses is 0, core/model/pvp_battle.h).
    int overrideLockCount() const;
    // The player slot the i-th lock row refers to, or -1.
    int overrideLockSlot(int i) const;
    // The move that row would commit to (its slot's last roll), or nullptr.
    const MoveDef* overrideLockMove(int i) const;
    // --- The picker's two levels ---------------------------------------------
    // LEVEL 1 is the bands this fight actually has — never more than four rows,
    // whatever the bag holds. LEVEL 2 is the rows inside the chosen band, and the only
    // level whose length is unbounded, which is why the screen windows it.
    //
    // A picker with ONE band opens straight at level 2. leaveOverrideBand answers false
    // there, which is how the caller knows C means cancel rather than back.
    bool overrideAtBands() const { return overrideAtBands_; }
    int overrideBandCount() const;                    // bands holding at least one row
    OverrideBand overrideBandAt(int i) const;         // the i-th band that is present
    int overrideBandPick() const { return overrideBandPick_; }
    int overrideBandRows(OverrideBand b) const;       // rows that band contributes
    int overrideBandFirst(OverrideBand b) const;      // its first flat index
    OverrideBand overrideBandOf(int flatPick) const;  // the band a flat row sits in
    // B at level 1 → descend onto that band's rows. False when already at level 2, so
    // the caller falls through to commitOverride and B keeps one meaning per level.
    bool enterOverrideBand();
    // C at level 2 → back to the band list, landing on the band just left. False when
    // there is no level 1 to return to, so the caller cancels the picker instead.
    bool leaveOverrideBand();
    void commitOverride();      // B → force the chosen move / use the item / fire the crew
                                 // Exploit; spends one use either way
    void cancelOverride();      // C → close the picker, no spend
    // A committed item's id (set by commitOverride, applied Health-side already);
    // returns it once then clears, so the Game consumes the stack + out-of-combat
    // effect exactly once. nullptr when the last commit was a move (or none).
    const char* takeCommittedItem();

    // Flee / quit. Safe → immediate quit (Fled). Live → escape roll; a fail
    // gives the enemy a free turn. `seedFlee` keeps the roll deterministic.
    void flee();

    // --- Inspectors (render + tests) ---------------------------------------
    const Combatant& player() const { return player_; }
    const Combatant& enemy() const { return enemy_; }
    Outcome outcome() const { return outcome_; }
    // Execution-Override (Trojan passive): the % chance `trojan` hijacks an enemy move.
    // 0 for a non-Trojan side (checked before any rng() draw at the call site); else
    // kExecOverrideBasePct + the armed traps' bonuses. Public so it's directly assertable.
    int execOverrideChance(const Combatant& trojan) const;
    Stakes stakes() const { return stakes_; }
    bool playerActsFirst() const { return playerFirst_; }
    bool playerTurnNext() const { return playerTurn_; }
    const char* lastMoveName() const { return lastMoveName_; }
    int lastDamage() const { return lastDamage_; }
    bool lastByPlayer() const { return lastByPlayer_; }
    bool lastWasCharge() const { return lastWasCharge_; }
    // True when lastDamage() was HELD by the target's ransom pool instead of taken —
    // the hit landed in full, its riders applied, but no Health moved. The combat
    // screen captions the popup differently for it, so a still Health bar under a
    // damage number reads as the passive working rather than as a stuck gauge.
    bool lastRansomed() const { return lastRansomed_; }
    // Whether the last resolved turn was one fighter SWINGING AT THE OTHER — an attack
    // reaching its target, landed or fully absorbed. False for a defend, an item, a crew
    // Exploit, a wind-up, and the passive ticks. The combat screen's directional cues read
    // this rather than lastDamage(): a shielded swing deals 0 and is still an attack, and a
    // ransom bill deals plenty and is not one.
    bool lastWasStrike() const { return lastWasStrike_; }

    // How many strikes this fight has resolved, either side. The combat screen walks its
    // strike mark's pair off this (ui/combat_screen.cpp) so no two swings in a row draw the
    // same frame. Derived from the resolved turn (setLast), never from a clock, so both
    // devices of a duel land on the same frame with nothing sent between them.
    int strikeCount() const { return strikeCount_; }
    // The Worm replica the last resolved turn destroyed, if any (see WormKill).
    const WormKill& lastWormKill() const { return lastWormKill_; }
    // Consecutive same-actor turns; 0 before the first turn, and reset to 1 the instant the
    // other side acts. Drives the feeding-frenzy render pacing (Game::combatBeatsForTurn)
    // and the steal-attack combo bonus (applyEffect) off the SAME count, so a burst that
    // looks faster also hits harder.
    int streakCount() const { return streakCount_; }
    bool streakIsPlayer() const { return streakIsPlayer_; }
    // The enemy's active channel (UI_MOVE_CHANNEL cue), or nullptr if none.
    const MoveDef* enemyChannel() const;
    int enemyChannelLeft() const { return enemy_.channelLeft; }

private:
    uint32_t rng();
    int chooseMove(Combatant& self);                 // autonomous roll (lean + no-repeat)
    int pickSlot(const Combatant& self, bool attacksOnly, bool allowRepeat);  // -1 = none
    void resolveTurn(Combatant& actor, Combatant& target, bool byPlayer);
    // The single applier for every CrewExploitKind — a new crew ability is one enum
    // entry (content_crews.h) plus one case here, never a per-crew branch elsewhere.
    // `self` is the side arming it, so the human's commit and an AI firing its own run
    // the identical code; `byPlayer` only captions the popup.
    void armCrewExploit(Combatant& self, const CrewExploit& x, bool byPlayer);
    // The commit-path wrapper: arm the picker's crew row onto the player.
    void applyCrewExploit();
    // `actor` is about to take its turn — if it carries an unfired autoExploit whose
    // Health trigger has come, arm it and report true, which SPENDS the turn. False
    // (the common case) leaves the turn to a move.
    bool fireAutoExploit(Combatant& actor, bool byPlayer);
    // Offer `c` the turn that just resolved, so a turn-metered crew Exploit can burn one
    // off its clock; `actedThisTurn` says which side of the turn `c` was on, the two kinds
    // counting opposite clocks. Called for both fighters and always AFTER checkOutcome, so
    // the turn that needed a death-save pays out at the count it was armed with.
    void tickCrewExploitClock(Combatant& c, bool actedThisTurn);
    // `moveIdx` is the actor's slot index for `mv` (into actor.moves) — needed by
    // Prowlware to rank that move's Attack power (attackPowerRank).
    void applyEffect(Combatant& actor, Combatant& target, const MoveDef* mv,
                     bool byPlayer, int moveIdx);
    // Hand a seized move back and restore what it displaced (RansomSeizure). Called the
    // turn the ransom settles, and inert on a fighter holding nothing — so every path that
    // ends a ransom can call it without first asking whether there was a seizure.
    void releaseRansomSeizure(Combatant& c);
    // Ransom Note (Ransomware): whether `c`'s ransom window is armed for the turn it is
    // about to take. Rolled once per turn at turn-start, never per incoming hit.
    bool ransomArmRolls(const Combatant& c);
    // Perfect Bite (Phishing steal track): stage-scaled chance, rolled only while the
    // caster's Obfuscation shield is up, to double whichever of stealSpeedPct/
    // stealCurrentHpPct lands this hit (see applyEffect).
    bool bubbleBiteRolls(Stage stage);
    // Whether a stun rider beats `target`'s built-up lock resistance. Answers true with no
    // rng draw on an unresisted target, which is the common case.
    bool stunLands(const Combatant& target);
    // Shared Resources (Worm), the speed half: assign a worm side the OPPONENT's live
    // speed. Called from every scheduling point (begin + pickNextActor) rather than once
    // at fight start, so a mid-fight speed change on either side is matched immediately.
    // No rng, and no effect on a fight without exactly one Worm in it.
    void syncWormSpeed();
    // Shared Resources (Worm), the replication half: roll `mv`'s replicaSpawnPct for a
    // spawn into a free slot. The move's kind decides which sort spawns and both magnitudes
    // are read off `mv`. The caller checks replicaSpawnPct > 0 first, so no rng is drawn
    // for a line that doesn't replicate.
    void rollWormSpawn(Combatant& actor, const MoveDef* mv);
    // Advance the speed gauges and return whether the player takes the next action.
    // Reads live speed, so a mid-fight speed change (siphon, buff) shifts the tempo at
    // once. Deterministic (no rng), so replays stay reproducible.
    bool pickNextActor();
    void setLast(const char* name, int dmg, bool byPlayer, bool charge,
                 bool ransomed = false, bool strike = false);
    void checkOutcome();

    Combatant player_, enemy_;
    Stakes stakes_ = Stakes::Safe;
    Outcome outcome_ = Outcome::Ongoing;
    bool playerFirst_ = true;
    bool playerTurn_ = true;
    uint32_t rng_ = 1;
    float plGauge_ = 0;         // speed-accumulator scheduler (see pickNextActor):
    float enGauge_ = 0;         // each side's gauge, filled by its speed, spent per action
    int streakCount_ = 0;       // see streakCount() above
    bool streakIsPlayer_ = true;

    int overrideUsesLeft_ = 1;      // Exploit uses remaining this fight
    int overrideUsesTotal_ = 1;     // allowance at fight start (for the pip readout)
    bool overrideOpen_ = false;
    bool overrideAtBands_ = false;              // cursor on the band list, not on rows
    int overrideBandPick_ = 0;                  // index into the PRESENT bands
    int overridePick_ = 0;          // index into [moves..., items...]
    int forcedMoveIdx_ = -1;        // committed override → the player's next move
    std::vector<OverrideItem> overrideItems_;   // combat-usable items this picker
    CrewExploit crewExploit_;                   // the crew row this picker offers (if any)
    const char* committedItemId_ = nullptr;     // set when an item is committed
    char itemPopup_[16] = "";                   // backing store for the PATCH / crew popup

    const char* lastMoveName_ = "";
    int lastDamage_ = 0;
    bool lastByPlayer_ = false;
    bool lastWasCharge_ = false;
    bool lastRansomed_ = false;
    bool lastWasStrike_ = false;
    int strikeCount_ = 0;
    WormKill lastWormKill_;
};

// Fill `c.chainFollow` from each equipped move's MoveDef::chainNextId, resolved against
// the chain-step table. Called by every Combatant builder — the player's pet, a PVE
// enemy and a duel/arena fighter — because a chain is a property of the MOVE and so
// belongs to whoever is holding it. Idempotent; safe to call after `moves` is final.
void resolveChains(const ContentRegistry& reg, Combatant& c);

// Build the wildcard draw pools for every slot of `c` holding a metamorphic row, resolved
// against `stage` (see the definition). Sets Combatant::polymorphic when any slot does.
void buildWildPools(const ContentRegistry& reg, Combatant& c, Stage stage);

// Build the player Combatant from live game state (active pet + loadouts + mods).
Combatant makePlayerCombatant(const ContentRegistry& reg, const CreatureDef& pet,
                              const MoveLoadout& moves, const Loadout& mods);

// Fold a pet's earned per-level stat points into an already-built Combatant, in the
// order power / defense / speed / maxHealth. Additive on top of the branch multipliers
// and mods, and it refills Health so the raised maximum starts full.
//
// A free function rather than part of makePlayerCombatant because two callers must
// produce byte-identical results from it: the Game building its own pet
// (game_combat.cpp) and makePvpCombatant rebuilding a REMOTE pet from its wire spec
// (core/model/pvp_battle.h). A duel's two devices resolve the same seeded fight only
// while both sides' stats agree exactly, so this arithmetic lives in exactly one place.
void applyLevelStatPoints(Combatant& c, const int statPoints[4]);

// The level-Power % bonus for `points` earned Power points, and the flat max-Health bonus
// for `points` earned max-Health points. Both ACCELERATE past their specialisation point
// (tunables.h) — the mirror image of levelDefenseCutPct's bend below — and both cap.
int levelPowerPct(int points);
int levelHealthBonus(int points);

// Defence's two investment tiers, each a total function of the earned Defence points.
// They exist because the stat's own % cut is bent and capped, so past a point it can only
// be paid in a different kind of thing (tunables.h explains which and why).
//
// pierce resist: the % an attack's own armorPiercePct is cut by before it is applied.
// brace retain: what Defence investment ADDS to the baseline share of an unspent one-shot
// brace that carries to the next hit (kBraceRetainBasePct). 0 below the threshold, which is
// every pet that has not committed — such a pet still keeps the baseline.
int levelDefensePierceResistPct(int points);
int levelDefenseBraceRetainPct(int points);

// The Defence stat's % incoming-damage cut, for `points` earned Defence points. Full rate
// up to kLevelDefenseSoftPoints, half rate past it, hard-capped at kLevelDefenseCapPct.
// Shared with the DeepWeb dive's rolled enemies, held to the same curve the pet is.
int levelDefenseCutPct(int points);
// What that ceiling REFUSED, in percentage points — the only place the uncapped curve is
// visible. Paired with the function above rather than folded into it, so the cut stays a
// single total answer to "what is this pet's Defence worth".
int levelDefenseCutOverflowPct(int points);

// OVERFLOW: what a bonus the caps refused is worth instead, in max-Health — the one pool
// nothing caps. A pet already at the never-immune cut, the level-Defence ceiling or the
// brace cap earns nothing from the next Defence point, mod or absorbed move, and no screen
// says so.
//
// Paid at the level table's own exchange rate: `perPointPct` is what one stat point bought
// of the clamped stat, so what arrives is that investment spent the other way. Nothing new
// to tune, and overflowing is never worth MORE than not overflowing. Every cap stays where
// it is. Pays a pet's EARNED bonuses only, never a spec-built enemy.
int capOverflowHealth(int overflowPct, int perPointPct);
// Build an enemy Combatant from a spec.
Combatant makeEnemyCombatant(const ContentRegistry& reg, const CombatEnemy& spec);

// Sim-Battle practice dummies. A tougher tier = a better test + more
// growth, still zero risk. Tier count + stats are content/balance.
constexpr int kSimDummyTiers = 2;
const char* simDummyName(int tier);
CombatEnemy simDummy(int tier);

// Scales a dummy to the pet's current level (the DeepWeb "match the pet" trick at a gentler
// rate), so the Basic/Hardened tiers stay a fair practice target as the pet grows. Mutates
// `e`; stamps `level`/`hasLevel` so the combat screen shows a number instead of "???".
void applySimDummyLevelScale(CombatEnemy& e, int petLevel);

// Wild-encounter malbeasts, difficulty-scaled by sector tier (1..3); each tier rolls among
// 2 variants. `variantRoll` is caller-owned (the shared Game LCG) so the pick is
// deterministic under a fixed seed; 0 picks the first variant.
CombatEnemy wildMalbeast(int sectorTier, uint32_t variantRoll = 0);

// The fixed wild-malbeast roster — index = bit position in Game's
// malbeastSeen/malbeastDefeated masks. Ids are the slugged (lowercase, non-alnum -> '_')
// form of each CombatEnemy::name, matching the 'Pedia catalog (tools/gen_pedia_data.py).
constexpr int kWildMalbeastCount = 6;
extern const char* const kWildMalbeastIds[kWildMalbeastCount];  // "glitchhog" .. "kernel_leviathan"

// Resolve a CombatEnemy::name (e.g. "GlitchHog", "Segfault Pup") to its roster index
// by slugging (lowercase, non-alnum -> '_') and matching against kWildMalbeastIds.
// Returns -1 for anything not in the 6 (sub-area/area bosses, Sim dummies, the debug
// "Lethal" enemy) — so only the WILD malbeast path ever sets a seen/defeated bit.
int wildMalbeastIndex(const char* enemyName);

// Within-sub-area / between-area wild difficulty ramp. wildMalbeast() gives a sector's
// sub-0 baseline; this thickens it as the player pushes deeper, so early sub-areas stay
// winnable for a fresh pet while later ones gate on a stronger pet. `areaIdx` and `sub`
// fold into one depth rung. Stamps an explicit `level` (+1 per sub-area and across areas)
// and applies the stat half of a level-up, so deeper wilds are meaner AND worth ranking.
//
// It is also the only per-AREA statement in an otherwise tier-keyed path: it reads the
// area's own wild pair off its row (AreaDef::wildAttackMoveId / wildDefendMoveId) — the
// Attack at every rung, the Defend from kWildAreaDefendSub. wildMalbeast() is keyed by
// tier, and three tiers are shared across five areas, so this is where an area is itself.
void applyWildSubAreaRamp(CombatEnemy& e, int areaIdx, int sub);

// Level-difference XP scaling: a wild win's flat base scaled by how the ENEMY's level
// compares to the PET's — kWildXpPerLevelDiffPct% per level either way, clamped to
// [kWildXpDiffMinPct, kWildXpDiffMaxPct]. Rewards punching up, taxes farming shallow
// sub-areas, and never pays zero. Result is at least 1.
int wildWinXp(int baseXp, int enemyLevel, int petLevel);

// The DEEPWEB DIVE endless-zone scaler: takes an endgame (tier-3) wild `e` and scales it to
// the PET's level, stamping `e.level = petLevel + kDeepWebEnemyLevelOffset` (parity at
// depth 0, so wildWinXp pays full base XP) and thickening Health/speed per pet level so the
// fight tracks the pet's growth. `depth` is the dive's win-streak, adding
// `floorLog2(depth+1) * kDeepWebDepthLevelPerLog2` effective levels before those scales —
// a fast early ramp that flattens, so a deep streak never runs away. Mutates `e`;
// `petLevel`/`depth` clamp at 0. `roll` is caller-owned (the shared Game LCG).
void applyDeepWebScale(CombatEnemy& e, int petLevel, int depth = 0, uint32_t roll = 0);

// What a DIVE enemy knows at `depth` — two distinct ids drawn from that depth's rung
// (deepweb_dive/area.h documents the rungs and the boss-move gate). Split out from
// applyDeepWebScale so the depth→kit rule can be tested without building an enemy.
std::vector<const char*> deepWebMoveIds(int depth, uint32_t roll);

// The depth ramp's Bits half. normalBitsReward is keyed to diffPips/stage-rank and cannot
// see the level bonus applyDeepWebScale grants XP through, so a deep dive would otherwise
// pay flat Bits forever. Mirrors the same logarithmic curve onto Bits: returns a percentage
// (100 = unchanged) the caller multiplies the rolled Bits by, clamped to
// kDeepWebDepthBitsMaxPct. `depth` clamps at 0.
int deepWebDepthBitsPct(int depth);

// A boss run. `rounds` is the ordered gauntlet — a single boss is a gauntlet of length 1;
// a multi-boss run fights them back-to-back with Health carried across. `stageRank` is the
// opponent's stage-rank R used by the Bits payout (Process 2, Script 3, Daemon 4); `name`
// banners the confrontation.
struct BossGauntlet {
    const char* name;
    int stageRank;
    std::vector<CombatEnemy> rounds;
};

// A SUB-AREA boss: a strong malbeast, unlocked by a 10-win streak and fought manually.
// `area` picks the roster + tier; `sub` scales the Health/speed climb (sub 4 = the
// signature apex). Usually one round, but a row may author escorts (SubBossDef::rounds)
// which run back-to-back on the area boss's carried-Health plumbing, so a caller never has
// to know which shape it got.
BossGauntlet subAreaBoss(int area, int sub);

// The AREA boss: a gauntlet of the area's five sub-area bosses fought back-to-back with
// carried Health. Unlocked once all five sub-areas are cleared; beating it clears the area
// and grants the Title.
// Always exactly kSubAreasPerArea rounds — each sub-area's boss PROPER, never the escorts
// that boss may have in its own fight.
BossGauntlet areaBoss(int area);

// Combat Bits payout, keyed to the opponent's stage-rank R. A NORMAL opponent pays a random
// integer in [R, R²]; a BOSS rolls that range R times and sums, a gauntlet accruing one
// roll per round and paying the lump at the end. `rng` is caller-owned and advanced in place.
int normalBitsReward(int stageRank, uint32_t& rng);
int bossBitsReward(int stageRank, uint32_t& rng);

} // namespace mal
