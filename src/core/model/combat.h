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

// The per-fight state of an ARMED crew Exploit — one block on the Combatant instead of
// a fresh field per ability, so the crew roster (content_crews.h) can grow without
// widening Combatant. Only one Exploit can be armed at a time (you belong to one crew),
// and most abilities meter out as either a charge count or a turn count, so those two
// counters cover the vocabulary; re-arming the same kind stacks onto them. Kept apart
// from the mod passives' ModStateSet (mod_state.h) because it is keyed by a different
// content vocabulary. Transient — wiped with the Combatant each fight.
struct CrewExploitState {
    CrewExploitKind kind = CrewExploitKind::None;
    int charges = 0;    // charge-metered kinds (NegateNextHits, PowerByDamageDealt)
    int turns = 0;      // duration-metered kinds (DeathSaveRally)
    // The three metering shapes, asked as three questions. `armed` is the charge half
    // ("is there a charge left to spend?"), `ticking` the turn half, and `holds` the
    // STICKY case, where being the armed kind at all is the whole condition.
    bool armed(CrewExploitKind k) const { return kind == k && charges > 0; }
    bool ticking(CrewExploitKind k) const { return kind == k && turns > 0; }
    bool holds(CrewExploitKind k) const { return kind == k; }
    // Whether the armed Exploit still has anything left to do — the one question the
    // combat screen's readout asks, so a spent charge-metered Exploit stops being
    // reported without anything having to clear `kind` behind it.
    bool live() const {
        return kind != CrewExploitKind::None &&
               (charges > 0 || turns > 0 || crewExploitIsSticky(kind));
    }
    // The number that prints beside the tag: whichever counter this kind meters out of,
    // and 0 for a sticky kind (which crewExploitLabel then renders without an "xN").
    int count() const { return charges > 0 ? charges : (turns > 0 ? turns : 0); }
};

// An Exploit as it is OFFERED to a fighter — the name it is announced under, the
// mechanic, and the one number that mechanic meters. Declared here, above Combatant,
// because a combatant carries one it may fire on its own initiative (`autoExploit`
// below) as well as one a human may commit from the picker (Combat::openOverride).
//
// The player's copy comes from their crew (content_crews.h); an AI's is rolled for it
// by whatever built the fighter. A default-constructed one (label == nullptr) means
// "no Exploit": no picker row is offered, and nothing ever fires on its own.
struct CrewExploit {
    const char* label = nullptr;
    CrewExploitKind kind = CrewExploitKind::None;
    int magnitude = 0;
};

// A move the Ransomware line is holding hostage. One block rather than a field per part,
// for the same reason CrewExploitState is one: the passive is a single mechanic with several
// halves, and a fighter whose halves disagree is not a state the fight should be able to
// reach.
//
// Cipher stacks a damage cut and, on its own, nothing else — which is why the line's
// defend-heavy pets measured worst of the four despite having the roster's deepest wall. A
// wall that only survives is not a win condition. So once the Cipher stack is FULL and a
// ransom is already running, the next brace stops merely absorbing: it SEIZES the attack
// that hits it, and the pet swings that move from the seized slot until the ransom comes
// due, at which point it hands it back. The line's defence becomes its offence, and it does
// so with the line's own vocabulary — take the thing, hold it, return it on payment.
//
// The seized move goes into `Combatant::moves` in place of the brace, so the roll, the
// Exploit picker and the readouts all see it without any of them learning what a seizure
// is. `heldMove` is what to give back. `heldFollow` is that move's chain step, parked for
// the same reason: what was seized is a payload, not the toolkit around it, so a seized
// chain entry commits no follow-up.
struct RansomSeizure {
    bool armed = false;                   // a full Cipher stack met a live ransom
    int slot = -1;                        // which of the pet's own slots is occupied
    const MoveDef* heldMove = nullptr;    // ...and what is waiting to come back to it
    const MoveDef* heldFollow = nullptr;
    bool holding() const { return heldMove != nullptr; }
};

// One live copy a Worm has replicated into a replication slot (Combatant::wormReplicas).
// NOT a Combatant: a replica never takes a turn of its own, never rolls a move and never
// appears in the speed scheduler — it is a piece of its parent's state that happens to
// be drawn separately. That is a deliberate ceiling, because the turn order is what both
// devices in a linked duel resolve independently (core/model/pvp_battle.h): a third
// actor with its own initiative would be a third thing for their two RNG streams to
// disagree about.
//
// So a replica does exactly two things — it SOAKS an incoming attack aimed at it
// (wormTargetWeights below), and, if it is an attacker, it PILES onto its parent's own
// swings (wormReplicaDamage below). Transient (per-fight) like every other combat field.
struct WormReplica {
    bool defender = false;  // spawned by a Defend move (soaks); else an attacker (piles on)
    int health = 0;
    int maxHealth = 0;
    // An attacker's damage per parent swing, banked whole at spawn; 0 on a defender.
    // Whole means whole: the parent's attack lean AND the defenders standing when it
    // spawned are already in this number (wormAttackerDamage), so nothing that happens
    // to the parent or the board afterwards moves it. A copy is a separate thing, and a
    // separate thing does not get stronger because a later one arrived.
    int attack = 0;
};

// One wildcard slot's draw pool (MoveDef::drawLineA/B), resolved when the Combatant is
// built so the turn engine never needs a registry.
//
// THREE BANDS IN ONE VECTOR rather than three vectors: `rows` holds the generic entries
// first, then line A's, then line B's, and the two indices below cut it. The roll picks a
// band by weight and then an index inside it (content_passives.h), so the shape the roll
// needs is two integers instead of three allocations per slot.
//
// Each line's own passive flags ride along, resolved from the same registry read that
// found its rows — which is what lets a cast GRANT them without Combat ever looking a line
// id up. A line the build does not know contributes no rows and no flags, and its weight
// falls back to generic rather than being drawn into an empty band.
struct WildPool {
    std::vector<const MoveDef*> rows;
    int genericEnd = 0;              // rows[0, genericEnd)        — the shared roster
    int lineAEnd = 0;                // rows[genericEnd, lineAEnd) — line A's track
                                      // rows[lineAEnd, size)       — line B's track
    LinePassives passivesA = 0;
    LinePassives passivesB = 0;
    // What this slot rolled on its last turn, which is the move the picker's LOCK band
    // offers to commit to. Null until the slot has actually fired once: an operator locks
    // something they SAW work, never a pick shown to them ahead of time.
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
    // Initiative (Clock-Speed Boost raises it). FLOAT (not int): a Phishing
    // speed siphon steals a % of the target's CURRENT speed each hit, and integer
    // truncation was crushing that to 0 the moment speed dropped near the floor —
    // float precision keeps every siphon meaningful instead of rounding it away.
    float speed = 0;
    int powerMultPct = 100;     // attack-power lean (Good/Bad branch)
    int basePowerMultPct = 100; // powerMultPct at fight start (after the gamble roll),
                                // captured in begin() — the combat screen diffs the
                                // live lean against it to SHOW a Phishing siphon
                                // (steal shifts powerMultPct; nothing else does mid-fight)
    float baseSpeed = 0;        // speed at fight start, captured in begin(); the stat
                                // panel diffs live speed against it to show a siphon
    int fragMultPct = 100;      // loss-Frag multiplier (Bad-branch hook)
    int enemyDamageMultPct = 100;  // wild-encounter offense buff (challenge pass);
                                    // 100 = neutral (player + bosses + Sim dummies)
    std::vector<const MoveDef*> moves;

    int defenseMultPct = 100;   // scales DEFEND-move brace magnitude (Defense stat +
                                // Cipher scaling); 100 = neutral (enemies, Sim).

    // Line identity — drives the per-line passive (e.g. Ransom
    // Lock) and is read once at fight start, same timing as MODS.
    Stage stage = Stage::BootSector;
    const char* line = nullptr;
    // The passives that line carries, resolved off its row ONCE when the combatant is
    // built (makePlayerCombatant) — same timing as MODS, and the reason the turn engine
    // never names a line. Empty for a combatant with no line (every PVE enemy today).
    // Set it through setLine(), never on its own: the id and the flags are two halves of
    // one fact, and a combatant whose flags disagree with its id is a fight that reads
    // its passives off one line and its move affinity off another.
    LinePassives linePassives = 0;

    // Adopt a line — the id move affinity matches on, plus the passives its family row
    // carries — in one call. An unknown (or null) line clears both.
    void setLine(const ContentRegistry& reg, const char* lineId);

    // The creature this combatant IS, when it is one — the fight reads its authored
    // clips (CreatureDef::clips) to pose the sprite while swinging or being hit.
    // A registry row outlives any fight, so this is a borrow, not ownership.
    // Null for a combatant built from a sprite-named spec rather than a creature
    // (makeEnemyCombatant), which the combat screen reads as "no authored poses" and
    // draws on row 0 — the single row every stand-in sheet ships.
    const CreatureDef* creature = nullptr;

    // Equipped MOD passives, keyed by the ModEffect that declares them (mod_state.h).
    // Effects that simply move a base stat (PowerPct, MaxHealth, …) are already folded
    // into the fields above by makePlayerCombatant; this holds the ones that stay live
    // for the fight — caps, thresholds, one-shots.
    ModStateSet mods;

    // Transient defend state.
    // Defence's investment tiers, resolved once by applyLevelStatPoints. Both are 0 for
    // any fighter that has not committed to the stat, which is every enemy and most pets.
    int pierceResistPct = 0;    // cuts an incoming attack's own armorPiercePct
    // % of an unspent brace that carries to the next hit. Starts at the baseline every
    // fighter gets (enemies included, which is why it is a default and not something only
    // applyLevelStatPoints sets) and Defence investment adds to it.
    int braceRetainPct = kBraceRetainBasePct;
    int dmgReducePct = 0;       // Firewall Patch / TPM Chip — % incoming damage cut
    int baseDmgReducePct = 0;   // dmgReducePct at fight start, captured in begin() —
                                // the third of the three live stat LEANS (with
                                // basePowerMultPct and baseSpeed) that a Phishing
                                // siphon or a Trojan trap's armour rot erodes, and so
                                // the third of the three Net Neutrality snaps back
    bool mirrorFired = false;   // set the turn a hit is fully negated (a brief flash);
                                 // also suppresses that attack's stun/DoT riders
    bool itemShield = false;    // Backup Drive's timed buff — a DEATH-SAVE, not a hit
                                 // negator: every hit lands in full, and the drive is
                                 // read only once the pet is already down
                                 // (restoreFromBackup below). Nothing like the RAID
                                 // Mirror mod, which spends itself on the first hit of
                                 // any size — the two never touch. Armed by the Game off
                                 // an item buff, so it lives here rather than in `mods`
                                 // (which the mod table owns)
    // What the drive DID, held for the rest of the fight (unlike mirrorFired, not reset
    // per-turn). Game reads it once the fight ends: any value but None burns the buff's
    // save-side timer early, and the three values are the three ways a fight can go
    // after a drive was spent, which is what the achievements are cut from. One field
    // rather than a fired/worked pair, because "spent but didn't work" is a state of the
    // same fact and two bools could disagree about it.
    enum class BackupUse : uint8_t {
        None,          // no drive was armed, or it was never needed
        Restored,      // spent, and the pet got back up
        Overwhelmed,   // spent, and half of max still wasn't enough — the pet went down
    };
    BackupUse backupUse = BackupUse::None;
    // The armed crew Exploit, if any (see CrewExploitState). Its charges are spent
    // BEFORE the RAID Mirror mod so that one-shot stays held for after the charges run
    // out. Armed either by a human at the picker (commitOverride) or by this side
    // firing its own (autoExploit, below) — the two paths converge on one applier, so
    // an ability behaves the same whoever pulled the trigger.
    CrewExploitState crewExploit;

    // --- The Exploit this side fires on its OWN initiative ------------------------
    // What a fighter with nobody at the buttons carries: a rolled Exploit plus the
    // moment it commits to using it. The TOURNAMENT arena is what has these (an
    // opponent there is petware with a kit and an Exploit, not a malbeast); every
    // other PVE enemy leaves `autoExploit.label` null and never draws for it.
    //
    // Firing COSTS the turn it happens on, exactly like a human spending their one
    // Exploit use costs the move they'd otherwise have forced — so an opponent that
    // opens with its Exploit really has skipped a swing to do it. That also gives the
    // fire its own beat on the combat screen, which is the only way the player sees it
    // happen at all.
    CrewExploit autoExploit;
    // The Health it waits for, as a % of its own max: 100 fires on its first turn,
    // 30 waits until it is in real trouble. Checked on this side's own turn, so a
    // fighter killed before its next turn never gets to spend it.
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

    // Feeding-frenzy combo (Phishing steal-attacks only, see Combat::applyEffect):
    // phishStreak counts this combatant's OWN run of steal-attack casts made WITH THE
    // BUBBLE UP (shieldHp > 0); phishComboBonus is the flat damage those casts have
    // permanently banked this fight. Unlike stackPowerBonus (a % mult, capped,
    // per-move-defined), this is flat damage, uncapped, and grows by the run length
    // itself — early runs add a sliver, a long one snowballs.
    //
    // Casting the bubble HOLDS the run rather than breaking it: the same shieldHp that
    // gates stealSpeedPct/stealCurrentHpPct and Perfect Bite gates the combo, so the
    // whole line answers to one question ("is the bubble up?") instead of the brace
    // being simultaneously required by three riders and fatal to a fourth. The run
    // breaks on a steal-attack cast made with the bubble DOWN — caught out mid-frenzy.
    int phishStreak = 0;
    int phishComboBonus = 0;

    // High-water mark of shieldHp since the pool last popped, driving the frenzy lean
    // in Combat::chooseMove (content_passives.h sizes it). Ratchets up as the bubble is
    // stacked and is cleared ONLY when the pool is overrun, never when it merely shrinks.
    int phishShieldPeak = 0;

    // STUN (a landed hit's lockTurns rider) — set ON THE VICTIM. While
    // lockedTurnsLeft > 0 the victim burns its turn doing nothing, then it lifts.
    int lockedTurnsLeft = 0;
    // ...and what a landed one leaves behind: one resist point per turn it froze, shed
    // one per turn this fighter gets to act (Combat::resolveTurn). The next stun rolls
    // against the pile (Combat::stunLands, kLockResistStepPct), so being chain-stunned is
    // the thing that buys a way out of it. Transient (per-fight), like lockedTurnsLeft.
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

    // POLYMORPH's memory: the distinct moves this fighter has cast, which is the only
    // question the passive asks ("is this one new?"). A fixed array rather than a set,
    // for the reason trojanTraps is one — a fight must not reach the heap — and its cap is
    // technical: past kPolymorphAbsorbCap a cast simply stops being recorded and stops
    // paying, which a fight would have to run absurdly long to reach.
    const MoveDef* absorbed[kPolymorphAbsorbCap] = {};
    int absorbedCount = 0;
    // The OTHER axis, and the one Mutation Engine (ModEffect::PolymorphEffectPct) is paid on:
    // which distinct EFFECT KINDS this fighter has cast, as a bitmask (moveEffectMask).
    // A mask rather than a list because the vocabulary is fixed and small — the question
    // is "has a stun happened at all", never which move brought it.
    uint32_t effectsSeen = 0;
    // Whether this fighter plays the metamorphic game at all — set when any of its slots
    // holds a wildcard row. The passive gates on THIS rather than on a line id, the way
    // Perfect Bite gates on a live bubble: the engine never learns a line's name, and a
    // pet that has equipped none of the line's rows is not running its passive either.
    bool polymorphic = false;

    // Chained moves (MoveDef::chainNextId). Parallel to `moves`: chainFollow[i] is the
    // step slot i hands off to, or nullptr for an ordinary move. Resolved once when the
    // Combatant is built (resolveChains) so the turn engine never needs a registry.
    std::vector<const MoveDef*> chainFollow;
    // The slot whose follow-up step is COMMITTED to this fighter's next turn, or -1. It
    // bypasses the move roll the same way a channel does, so the step lands on the very
    // next turn and the no-consecutive rule never gets a say — a chain that had to wait
    // for a second random roll of its own slot would almost never complete.
    //
    // Cleared by anything that interrupts the fighter: an Exploit override commanding a
    // different move, and death. That is the whole of "the chain broke" — the pet simply
    // rolls the entry again next time the slot comes up.
    int chainSlot = -1;

    // Backup Drive's death-save (itemShield): burn the armed drive and add half of max
    // Health back. Called from ONE place, Combat::checkOutcome, on a combatant that has
    // just been judged overwhelmed — so it reads this pet's state and knows nothing
    // about what put it there. A save that had to recognise each damage source would
    // owe every future one a branch of its own.
    //
    // Half of max is what the drive holds, so it does not guarantee survival: added to
    // a deep enough hole it still leaves the pet under 0, and the pet dies. Enemies
    // never carry a drive (only Game::buildPlayerCombatant arms one), so this is a
    // no-op on every enemy Combatant.
    void restoreFromBackup();
};

// Prowlware's multiplier for `moves[moveIdx]`: the rank of that move's Attack power
// among the DISTINCT Attack powers in `moves`, ascending (weakest tier = 1, strongest =
// N, ties sharing a rank). 0 for a Defend move or an out-of-range slot, so a kit with
// one attack tier ranks 1 everywhere and pays nothing — the mod only rewards a genuine
// power spread. Pure, so it's directly assertable.
int attackPowerRank(const std::vector<const MoveDef*>& moves, int moveIdx);

// --- Worm replication, the pure half ------------------------------------------------
// Each of these is a total function of a Combatant's own replica array, so the balance
// they encode is assertable directly rather than only through a resolved fight.

// How many of `c`'s live replicas are of the given kind.
int wormReplicaCount(const Combatant& c, bool defenders);

// Whether `m`'s ENTIRE contribution is the one-shot `guard` brace. A Defend row may also
// pool a shield, arm a trap, spawn a defender or stack the Cipher cut, and each of those
// is worth a turn whatever the brace situation is; a row carrying none of them does
// nothing but add to `guard`.
//
// That matters because `guard` is one-shot: the whole pool absorbs the next hit and is
// then zeroed, with any magnitude past that hit's damage discarded. So casting a
// pure-brace Defend onto a brace that is ALREADY up spends a turn to buy overkill on a
// single hit, and Combat::chooseMove re-rolls off it for the same reason it re-rolls off
// a Defend during a frenzy. Pure, so the classification is assertable without a fight.
bool braceOnlyDefend(const MoveDef& m);

// The odds a stun rider aimed at `c` right now would actually freeze it: 100 while `c`
// holds no lock resistance, falling kLockResistStepPct per resist point it has banked
// and never past kLockResistFloorPct. A total function of the combatant, so the combat
// screen reads out the same number Combat::stunLands is rolling against.
int stunLandPct(const Combatant& c);

// The Phishing pool siphon, 0..kPhishPoolSiphonMaxPct: what the LIVE Obfuscation pool adds
// to the magnitude of this pet's two bubble-gated steals, as a percentage of their own
// authored value. Scales with the pool against the pet's own max Health, so a bubble worth
// its whole body doubles the bite. 0 with no bubble up, which is every pet off the line.
//
// The line's conversion from defence to offence: the bubble stops being a wall it hides
// behind and becomes the size of what it takes. A total function of the combatant, so the
// combat screen can show the same number the engine is acting on.
int phishPoolSiphonBonusPct(const Combatant& c);

// The Phishing frenzy lean, 0..kPhishFrenzyLeanMaxPct: how strongly an over-stacked
// Obfuscation bubble pushes `c` off bracing and onto biting (Combat::chooseMove reads
// it to re-roll Defend picks). A total function of the combatant, so the combat SCREEN
// can draw the same state the engine is acting on rather than re-deriving a lookalike.
// 0 for every combatant that has never pooled a shield past its own max Health.
int phishFrenzyLeanPct(const Combatant& c);

// What one ATTACKER is worth, at the moment it spawns: its share of the move that made
// it, times the parent's attack lean, times the defenders already standing (floored at
// kWormReplicaMultFloor, so a board with no cover still pays it its base).
//
// The cross-multiplier is read HERE and never again, which is what makes spawn ORDER the
// decision the line is played on: cover first and the teeth that follow are worth more,
// teeth first and they are worth their base forever. Multiplying live instead would
// reach backwards and pump copies that are already on the board, which is the one way a
// copy would stop behaving like the separate thing it otherwise is — it has its own
// Health, its own damage and its own chance of being hit, and nothing that happens to
// the parent after it spawns reaches it.
int wormAttackerDamage(const Combatant& parent, int movePower, int pct);

// The damage `c`'s ATTACKER replicas add to one of its parent's swings — the sum of what
// each banked when it spawned, and nothing more.
int wormReplicaDamage(const Combatant& c);

// A DEFENDER's Health at the moment it spawns: `pct` of the parent's maxHealth times the
// live ATTACKER count (same floor). Banked at spawn rather than recomputed, because a
// defender's Health is a pool being chipped and cannot be restated once a hit has come
// out of it — so the order is a real decision: attackers first, then a defender that
// inherits their number.
int wormDefenderHealth(const Combatant& parent, int pct);

// The per-target draw weights an incoming attack picks its victim from: index 0 is the
// PARENT and index 1+i is replica i, so the returned vector is always
// 1 + c.wormReplicaCount long. Weights come from content_passives.h; the shape is
// exposed so a test can assert the distribution without resolving a fight.
std::vector<int> wormTargetWeights(const Combatant& c);

// The GRUDGE, 0..kLedgerGrudgeMaxPct: how much Extortion Ledger's power bonus is scaled up
// by what this pet is currently holding unsettled, as a percentage of that bonus. Measured
// against the stage's own body, so it says how DEEP the pool is rather than how levelled the
// pet is. 0 for a fighter carrying no ransom, which is every pet off the line.
//
// A total function of the combatant, so a test can assert the curve without resolving a
// fight and the combat screen can draw the same number the engine is multiplying by.
int ledgerGrudgePct(const Combatant& c);

// --- Polymorph, the pure half ---------------------------------------------------------

// Has `c` already cast `m` this fight? The whole of what the passive asks, and the reason
// `absorbed` is a list rather than a counter. Pure, so a test asserts the distinct rule
// without resolving a fight.
bool polymorphHasAbsorbed(const Combatant& c, const MoveDef* m);

// Record `m` as cast and pay for it, if it is new to `c` and `c` plays this game at all.
// Returns true when it actually paid, which is what the combat screen's popup reads.
//
// The payout MUTATES the fighter's live stats rather than being derived on read, and that
// is deliberate: `Combat::begin` captures basePowerMultPct/baseSpeed/baseDmgReducePct, and
// the stat panel renders live-against-base as a signed delta — so moving the real field is
// what makes an absorbed stack show up on a screen that already knows how to draw it. A
// derived bonus would be invisible there without a fourth channel teaching it the same
// number twice.
//
// max-Health is the one that could not be derived even in principle: it is a pool being
// chipped, and a pool cannot be restated once a hit has already come out of it. Both the
// ceiling and the Health inside it rise, which is what `MaxHealth` mods already do.
bool polymorphAbsorb(Combatant& c, const MoveDef* m);

// Pay `points` stat points' worth in `kind`'s currency — Attack buys Power and Speed,
// Defend buys Defense, brace magnitude and max-Health. The one place Polymorph and its
// amplifier spend, so a mod that adds to the passive cannot drift from what the passive
// itself pays.
void polymorphPay(Combatant& c, MoveKind kind, int points);

// Which distinct EFFECT KINDS `m` carries, as a bitmask — one bit per rider a row can
// declare, so two different moves that both do nothing but damage share a mask of 0 while
// one loaded row can set several bits at once.
//
// This is the axis Mutation Engine pays on, and it is deliberately not the one Polymorph
// counts: a kit of plain swings feeds the passive perfectly well and feeds the mod
// nothing. Derived entirely from fields a row already declares, so no move authoring
// changes and no table has to be kept in step with the roster.
//
// The steal track spends a bit per STAT rather than one for the track, because a siphon
// that moves Speed and one that moves max-Health are different things happening to the
// fighter on the other side — which is the reading the mod is named for.
uint32_t moveEffectMask(const MoveDef& m);

// How many distinct effect kinds `c` has cast this fight — popcount over effectsSeen.
// Pure, so the mod's magnitude is assertable without resolving a fight.
int polymorphEffectCount(const Combatant& c);

// Which row a wildcard slot casts this turn: `roll` is the caller's own rng draw, taken
// once, and the pool's bands are weighted by content_passives.h's kWildSource*Pct. Returns
// nullptr for an empty pool, which the caller reads as "cast the wildcard row itself".
//
// Pure and total, so the whole weighting is assertable against a swept roll rather than
// only through a resolved fight — and so both devices of a duel, drawing the same number
// from the same seeded stream, land on the same row.
const MoveDef* wildPick(const WildPool& pool, uint32_t roll);

// The passives a cast of `m` grants, given the pool of the slot that cast it: line A's
// flags if `m` belongs to line A, line B's if it belongs to line B, and none otherwise.
// A string compare against two ids rather than a registry lookup, which is what keeps the
// grant on the cast path without Combat learning what a registry is.
LinePassives wildBorrowedPassives(const WildPool& pool, const MoveDef* m);

// Resolve `roll` (any uint32_t — the caller's own rng draw) against wormTargetWeights:
// returns -1 for the parent, else the index into c.wormReplicas that eats the hit.
// Pure, so the same roll always names the same victim on both devices of a duel.
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
    // The one move that is THIS creature's, whatever depth it was met at. Carried beside
    // moveIds rather than in it because the depth ladder REPLACES that list
    // (applyWildSubAreaRamp): a kit written into the row survives only the shallowest
    // sub-area, which is why six wild bodies read as three tiers and nothing else.
    //
    // It is what makes one malbeast worth farming over another. A win teaches out of the
    // beaten enemy's whole kit (rollEnemyMoveDrop, game_explore.cpp), so a signature is a
    // legible reason to hunt a particular creature — and being generic (MoveDef::line
    // null) it drops to whatever the player hatched.
    const char* signatureMoveId = nullptr;
    int level = 0;                      // depth level (global sub-area rung);
                                        // set by applyWildSubAreaRamp, drives the
                                        // level-difference XP scaling (wildWinXp). 0 =
                                        // unranked (Sim dummies, bosses use their own).
    // The other two of the four stats a PET levels. Absent until the DeepWeb dive needed
    // to roll a full stat spread: every authored enemy is a Health/speed/moves statement
    // and leans on its move rows for offence, which is exactly why a dive enemy could
    // never hit harder no matter how deep it got. Defaults match what makeEnemyCombatant
    // used to hard-code, so no authored row changes meaning by their arrival.
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
// band's rows are always one contiguous run and every flat index the commit path reads
// (Combat::commitOverride) falls inside exactly one of them.
//
// They exist as a NAVIGATION level because only one of them is bounded by design: a
// fighter has at most kMaxMoveSlots moves and a crew Exploit is one row, but the item
// band is every combat-usable stack in the bag and the lock band is every wildcard slot
// that has fired. Walked as one flat list those bury the short bands behind a lap of
// the long one, on a screen with room for ten rows.
enum class OverrideBand : uint8_t { Move, Item, Lock, Crew };
constexpr int kOverrideBands = 4;

// The band's own name — the level-1 row, and the header over its rows at level 2.
const char* overrideBandName(OverrideBand b);

// A Worm replica destroyed by the turn that just resolved. Replicas are packed out of
// Combatant::wormReplicas the instant they die, so by the time anything looks at the
// board the copy is simply gone — and a death that leaves no trace is the one moment of
// the line's whole mechanic the player never gets to see. This is that trace: enough for
// the combat screen to play the glyph's dissolve frames over the freed slot, and nothing
// more. Overwritten by the next resolved turn (Combat::step), never accumulated.
struct WormKill {
    bool happened = false;
    bool onPlayer = false;   // whose board lost the copy (Combat's slot, not the seat)
    bool defender = false;   // which of the two glyphs dissolves
};

class Combat {
public:
    enum class Stakes { Live, Safe };       // live = +Frag on loss; safe = nothing
    enum class Outcome { Ongoing, Win, Lose, Fled };

    // Build a battle. The enemy always resets to full Health; the player normally
    // does too, EXCEPT when `carryPlayerHealth >= 0` — then the player STARTS at
    // that value (clamped to maxHealth). That is the boss-gauntlet carry (
    // ): consecutive rounds run with no heal between, so a round-2 boss
    // inherits the Health the pet limped out of round 1 with. `seed` makes
    // resolution deterministic. `forceEnemyFirst` overrides the speed-based
    // initiative roll — the wild-encounter "failed flee" penalty:
    // retreating isn't free.
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
    // The band is empty for every fighter that is not running Polymorph, so no other line
    // ever sees a row here — and it is empty in a DUEL for a different reason that needs
    // no code: the picker never opens at all (exploitUses is 0, core/model/pvp_battle.h).
    // Building toward a plan is a thing this line does in the field and not across a link.
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
    // A picker with ONE band opens straight at level 2: there is nothing to choose
    // between, so an early-game pet with no items, no fired wildcard and no crew walks
    // the same single list it always has. leaveOverrideBand answers false there, which
    // is how the caller knows C means cancel rather than back.
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
    // move reaching its target (or one of its replicas), landed or fully absorbed. False
    // for everything else a turn can be: a defend, an item, a crew Exploit, a wind-up
    // turn that only charged, and the passive ticks (a DoT, a ransom bill, a stun) that
    // move a fighter's own Health with nobody swinging at all.
    //
    // The combat screen's directional cues read this rather than lastDamage(), because
    // the two questions differ in both directions: a shielded swing deals 0 and is still
    // an attack, and a ransom bill coming due deals plenty and is not one.
    bool lastWasStrike() const { return lastWasStrike_; }

    // How many strikes this fight has resolved, either side, counting from its start.
    // The combat screen walks its strike mark's pair off this (ui/combat_screen.cpp), so
    // no two swings IN A ROW draw the same frame — which is what the player is actually
    // watching, a sequence of blows rather than one fighter's private history.
    //
    // Derived from the resolved turn (setLast) and never from a clock, so both devices of
    // a duel land on the same frame with nothing sent between them.
    int strikeCount() const { return strikeCount_; }
    // The Worm replica the last resolved turn destroyed, if any (see WormKill).
    const WormKill& lastWormKill() const { return lastWormKill_; }
    // Consecutive same-actor turns (a lopsided speed edge — a Phishing speed siphon —
    // pays one side extra actions in a row). 0 before the first turn; 1 on a fresh
    // actor's first hit; increments each turn that side keeps acting; resets to 1 the
    // instant the other side gets a turn. Drives both the feeding-frenzy render pacing
    // (Game::combatBeatsForTurn) and the Phishing steal-attack combo bonus (applyEffect)
    // off the SAME count, so a burst that looks faster also hits harder.
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
    // Offer `c` the turn that just resolved, so whichever turn-metered crew Exploit it
    // holds can burn one off its own clock — `actedThisTurn` says which side of the turn
    // `c` was on, because the two kinds count opposite clocks (see the definition).
    // Called for BOTH fighters from the two places a turn resolves — step() and a failed
    // flee — and always AFTER checkOutcome, so the turn that actually needed a death-save
    // is paid out at the count it was armed with rather than one short.
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
    // Whether a stun rider beats `target`'s built-up lock resistance (Combatant::lockResist,
    // kLockResistStepPct). Answers true with no rng draw at all on an unresisted target —
    // the common case, and the one that keeps an unchained fight's stream deterministic.
    bool stunLands(const Combatant& target);
    // Shared Resources (Worm), the speed half: assign a worm side the OPPONENT's live
    // speed. Called from every scheduling point (begin + pickNextActor) rather than once
    // at fight start, so a mid-fight speed change on either side is matched immediately.
    // No rng, and no effect on a fight without exactly one Worm in it.
    void syncWormSpeed();
    // Shared Resources (Worm), the replication half: `mv` has just been cast by `actor`
    // and carries replicaSpawnPct, so roll for a spawn into a free replication slot. The
    // move's own kind decides which sort spawns (Attack -> attacker, Defend -> defender)
    // and both magnitudes are read off `mv`. The caller checks replicaSpawnPct > 0
    // first, so no rng is drawn for a line that doesn't replicate.
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
// (tunables.h) — the mirror image of levelDefenseCutPct's bend below — and both cap. Pure
// and total, so the curve is asserted directly rather than through a resolved fight.
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
// (kLevelDefensePctPerPoint) up to kLevelDefenseSoftPoints, HALF rate past it, hard-capped
// at kLevelDefenseCapPct — the diminishing half of the one stat with a ceiling, so the
// last points before that ceiling stop being the best purchase in the game. Pure +
// deterministic, so it is unit-tested directly rather than through a fight. Shared with
// the DeepWeb dive's rolled enemies, which are held to the same curve the pet is.
int levelDefenseCutPct(int points);
// What that curve's own ceiling REFUSED, in percentage points — the discard
// kLevelDefenseCapPct makes, and the only place the uncapped curve is visible. Paired
// with the function above rather than folded into it so the cut stays a pure, total
// answer to "what is this pet's Defence worth" and nothing downstream has to learn a
// second output to keep working.
int levelDefenseCutOverflowPct(int points);

// OVERFLOW: what a bonus the caps refused is worth instead, in max-Health.
//
// A clamp is a promise about the CEILING, not about the bonus. A pet already at the
// never-immune cut, the level-Defence ceiling or the brace cap earns literally nothing
// from the next Defence point, mod or absorbed move, and no screen says so — the row
// still reads as if it paid. The Epics make it easy to reach: Extortion Ledger alone
// adds 35 points of cut to a line that also stacks Cipher. So the discard is paid into
// max-Health, the one pool nothing caps.
//
// At the level table's OWN exchange rate: `perPointPct` is what one stat point bought of
// the clamped stat and kLevelHealthPerPoint is what the same point buys of max-Health, so
// what arrives is exactly what that investment was worth spent the other way. There is
// nothing new to tune, and overflowing can never be worth MORE than not overflowing.
//
// This is about the discard, not the ceiling: every cap stays exactly where it is, and
// the read-side clamps that keep a fighter killable are untouched. It pays a pet's own
// EARNED bonuses — level points, mods, Polymorph — and not a spec-built enemy, which is
// described rather than rewarded and whose budget the zone already tunes.
int capOverflowHealth(int overflowPct, int perPointPct);
// Build an enemy Combatant from a spec.
Combatant makeEnemyCombatant(const ContentRegistry& reg, const CombatEnemy& spec);

// Sim-Battle practice dummies. A tougher tier = a better test + more
// growth, still zero risk. Tier count + stats are content/balance.
constexpr int kSimDummyTiers = 2;
const char* simDummyName(int tier);
CombatEnemy simDummy(int tier);

// Scales a dummy to the pet's current level (the DeepWeb "match the pet" trick,
// reused at a gentler rate) — the Basic/Hardened tiers stay a
// fair practice target as the pet grows instead of trailing further behind every
// level. Mutates `e` in place; stamps `level`/`hasLevel` so the combat screen shows
// a real number instead of "???".
void applySimDummyLevelScale(CombatEnemy& e, int petLevel);

// Wild-encounter malbeasts, difficulty-scaled by sector tier (1..3).
// Each tier rolls among 2 variants — the full per-sector table
// (more variants, distinct art) is still a later content/balance pass; this
// closes the "exactly one fixed enemy per tier" gap. `variantRoll` is
// caller-owned (the shared Game LCG, same pattern as every other roll in this
// file) so the pick stays deterministic under a fixed seed; default 0 keeps
// the single-enemy behaviour for callers that don't roll (existing
// tests). Same no-new-art convention as simDummy — reuses SPR_PET_CACHEMUTT.
CombatEnemy wildMalbeast(int sectorTier, uint32_t variantRoll = 0);

// The fixed wild-malbeast roster (save v25 'Pedia reveal state) —
// index = bit position in Game's malbeastSeen/malbeastDefeated masks. Ids are the
// slugged (lowercase, non-alnum -> '_') form of each CombatEnemy::name above, and
// match the 'Pedia catalog's malbeast entry ids (tools/gen_pedia_data.py).
constexpr int kWildMalbeastCount = 6;
extern const char* const kWildMalbeastIds[kWildMalbeastCount];  // "glitchhog" .. "kernel_leviathan"

// Resolve a CombatEnemy::name (e.g. "GlitchHog", "Segfault Pup") to its roster index
// by slugging (lowercase, non-alnum -> '_') and matching against kWildMalbeastIds.
// Returns -1 for anything not in the 6 (sub-area/area bosses, Sim dummies, the debug
// "Lethal" enemy) — so only the WILD malbeast path ever sets a seen/defeated bit.
int wildMalbeastIndex(const char* enemyName);

// Within-sub-area / between-area wild difficulty ramp (balance).
// wildMalbeast() gives the sub-0 BASELINE for a sector; this thickens that spec as
// the player pushes DEEPER — through a sector's sub-areas AND between sectors — so
// the early sub-areas stay winnable for a fresh pet while the later ones (and later
// areas) gate on a stronger/evolved one ("steep/gated" curve). Moves are the first
// lever (fully data-driven, applied here); enemy mods, raw-stat scaling, and
// evolution-tier substitution layer on top of this as the ramp is tuned. `areaIdx`
// (0-based sector) and `sub` (0..kSubAreasPerArea-1) fold into one depth rung.
// It ALSO stamps an explicit `level` (a global depth rung, +1 per
// sub-area and across areas) and applies the stat half of a level-up (Health per
// sub, speed at the deeper rungs) so deeper wilds are meaner AND worth ranking.
//
// The other thing it does is the only per-AREA statement in an otherwise tier-keyed
// path: it reads that area's own wild pair off its row (AreaDef::wildAttackMoveId /
// wildDefendMoveId) and adds it to the kit — the Attack at every rung, the Defend from
// kWildAreaDefendSub. wildMalbeast() cannot express that, since it is keyed by tier and
// three tiers are shared across five areas; this is where an area gets to be itself.
void applyWildSubAreaRamp(CombatEnemy& e, int areaIdx, int sub);

// level-difference XP scaling. A wild win's XP is the
// flat base scaled by how the ENEMY's level compares to the PET's: each level the
// enemy is ABOVE the pet adds kWildXpPerLevelDiffPct%, each level BELOW subtracts it,
// clamped to [kWildXpDiffMinPct, kWildXpDiffMaxPct]. Rewards punching up (challenge),
// taxes farming low-level sub-areas — but never zero (a floored trickle). Pure +
// deterministic so it's unit-tested directly; result is at least 1.
int wildWinXp(int baseXp, int enemyLevel, int petLevel);

// the DEEPWEB DIVE endless-zone scaler. Takes an endgame
// (tier-3) wild `e` and scales it to the PET's level: stamps `e.level = petLevel +
// kDeepWebEnemyLevelOffset` (parity at depth=0 → wildWinXp pays full base XP) and
// thickens Health/speed per pet level so the fight tracks the pet's own stat growth
// instead of trivialising as it levels. `depth` is the dive's current win-streak; it
// adds a `floorLog2(depth+1) * kDeepWebDepthLevelPerLog2` bonus effective level on top
// of `petLevel` before every scale above is applied — so diving deeper gradually
// punches the pet up (more XP via wildWinXp's level-diff bonus) and thickens the enemy
// to match, logarithmically (fast early ramp, flattens at deep streaks — no endless-
// zone runaway). Mutates `e` in place. `petLevel`/`depth` clamp at 0.
// `roll` is caller-owned (the shared Game LCG, same pattern as wildMalbeast's variantRoll)
// so a dive enemy is deterministic under a fixed seed like every other roll in the engine.
void applyDeepWebScale(CombatEnemy& e, int petLevel, int depth = 0, uint32_t roll = 0);

// What a DIVE enemy knows at `depth` — two distinct ids drawn from that depth's rung
// (deepweb_dive/area.h documents the rungs and the boss-move gate). Split out from
// applyDeepWebScale so the depth→kit rule can be tested without building an enemy.
std::vector<const char*> deepWebMoveIds(int depth, uint32_t roll);

// depth ramp, Bits half: the DEEPWEB DIVE's Bits payout (normalBitsReward,
// keyed to diffPips/stage-rank) doesn't see the level bonus applyDeepWebScale grants
// XP through, so without this a deep dive would pay flat Bits forever. Mirrors the
// same logarithmic curve directly onto Bits: returns a percentage (100 = unchanged)
// the caller multiplies the rolled Bits by — 100 + floorLog2(depth+1) *
// kDeepWebDepthBitsPctPerLog2, clamped to kDeepWebDepthBitsMaxPct. Pure + deterministic
// so it's unit-tested directly. `depth` clamps at 0.
int deepWebDepthBitsPct(int depth);

// A boss run. `rounds` is the ordered gauntlet: a single boss is a gauntlet
// of length 1; a multi-boss run fights them back-to-back with Health carried across
// `stageRank` is the opponent's stage-rank R used by the Bits
// payout (Process 2, Script 3, Daemon 4); `name` banners the confrontation. Enemies
// reuse the generic malbeast frame (no new art), same convention as wildMalbeast.
struct BossGauntlet {
    const char* name;
    int stageRank;
    std::vector<CombatEnemy> rounds;
};

// A SUB-AREA boss: a strong malbeast, unlocked by a 10-win streak and fought
// manually. `area` (0..kExplSectors-1) picks the roster + tier; `sub`
// (0..kSubAreasPerArea-1) scales the Health/speed climb (sub 4 = the signature apex).
// Usually one round — but a row may author escorts (AreaDef's SubBossDef::rounds), and
// those run back-to-back on the same carried-Health plumbing the area boss uses, so a
// caller never has to know which shape it got. Boss names are a pool disjoint from the
// roster + wild malbeasts (namespace guard).
BossGauntlet subAreaBoss(int area, int sub);

// The AREA boss: a 5-stage gauntlet of the area's five sub-area bosses
// fought back-to-back (carried Health, no heal between). Unlocked once all five
// sub-areas are cleared; beating it clears the area (→ next area) + grants the Title.
// Always exactly kSubAreasPerArea rounds — each sub-area's boss PROPER, never the escorts
// that boss may have in its own fight.
BossGauntlet areaBoss(int area);

// combat Bits payout, keyed to the opponent's stage-rank R. A NORMAL opponent
// pays a random integer in [R, R²]; a BOSS rolls that range R times and sums (a
// gauntlet accrues one boss roll per round and pays the lump at the end). `rng` is
// caller-owned (the shared LCG) and advanced in place so payouts stay deterministic
// under a fixed seed, same pattern as every other roll in the engine.
int normalBitsReward(int stageRank, uint32_t& rng);
int bossBitsReward(int stageRank, uint32_t& rng);

} // namespace mal
