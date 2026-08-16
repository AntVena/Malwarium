#include "core/model/tournament.h"

#include <cstring>

#include "core/content/content_crews.h"
#include "core/content/defs.h"
#include "core/content/registry.h"
#include "core/model/move_loadout.h"
#include "core/model/pvp_battle.h"
#include "tunables.h"

namespace mal {

namespace {

// The engine's shared LCG, the same constants every other roll in the game advances
// (Game::rng_, combat_factory's variant rolls). Kept local and passed by reference so
// every derivation here is a pure function of its own seed rather than of call order.
inline uint32_t lcg(uint32_t& s) {
    s = s * 1664525u + 1013904223u;
    return s >> 16;               // the high half, like every other consumer
}

// A slot's own stream. Mixed from the run seed and the slot index so that two slots of
// one bracket, and the same slot of two brackets, are uncorrelated — a plain
// `seed + slot` would roll neighbouring slots into near-identical entrants under an
// LCG whose low bits move slowly.
inline uint32_t slotSeed(uint32_t seed, int slot) {
    uint32_t s = seed ^ (static_cast<uint32_t>(slot + 1) * 0x9E3779B9u);
    s = s * 1664525u + 1013904223u;
    s ^= s >> 15;
    return s ? s : 1u;
}

// Roll in [lo, hi] inclusive.
inline int rollRange(uint32_t& s, int lo, int hi) {
    if (hi <= lo) return lo;
    return lo + static_cast<int>(lcg(s) % static_cast<uint32_t>(hi - lo + 1));
}

// The stage an entrant of `level` is fielded at. Bands, not a roll — see
// content_tournament.h.
Stage tourneyStage(int level) {
    if (level < kTourneyScriptLevel) return Stage::Process;
    if (level < kTourneyDaemonLevel) return Stage::Script;
    return Stage::Daemon;
}

// Uniform pick over the rows of `all` that pass `keep`, in ONE pass and constant
// memory (reservoir sampling): the nth candidate replaces the held pick with
// probability 1/n. Used for the species, each move slot and each mod, so none of them
// needs a filtered copy of a content table on an embedded heap.
template <typename T, typename Pred>
const T* reservoirPick(const std::vector<const T*>& all, uint32_t& s, Pred keep) {
    const T* pick = nullptr;
    int seen = 0;
    for (const T* row : all) {
        if (!row || !keep(*row)) continue;
        ++seen;
        if (lcg(s) % static_cast<uint32_t>(seen) == 0) pick = row;
    }
    return pick;
}

// The handle permutation for a run: Fisher-Yates over the whole pool, seeded by the
// run seed alone. Drawing WITHOUT replacement is the point — eight entrants must be
// eight names, and a per-slot roll would collide often enough to be noticed.
int handleIndexFor(uint32_t seed, int slot) {
    if (slot < 0 || slot >= kTourneySlots || kTourneyHandleCount <= 0) return -1;
    uint8_t order[64];
    const int n = kTourneyHandleCount < 64 ? kTourneyHandleCount : 64;
    for (int i = 0; i < n; ++i) order[i] = static_cast<uint8_t>(i);
    uint32_t s = seed ? seed : 1u;
    for (int i = n - 1; i > 0; --i) {
        const int j = static_cast<int>(lcg(s) % static_cast<uint32_t>(i + 1));
        const uint8_t t = order[i];
        order[i] = order[j];
        order[j] = t;
    }
    return order[slot % n];
}

// Whether `id` already sits in the first `count` cells of a fixed id table — the
// no-duplicates check for the move and mod rolls.
bool alreadyPicked(const char (*cells)[kPvpIdCap], int count, const char* id) {
    for (int i = 0; i < count; ++i)
        if (cells[i][0] && std::strcmp(cells[i], id) == 0) return true;
    return false;
}

int healthPctOf(const Combatant& c) {
    const int maxH = c.maxHealth > 0 ? c.maxHealth : 1;
    const int h = c.health > 0 ? c.health : 0;
    return h * 100 / maxH;
}

}  // namespace

int tourneyPlayerSlot(uint32_t seed) {
    uint32_t s = slotSeed(seed, kTourneySlots);   // a stream no entrant slot uses
    return static_cast<int>(lcg(s) % kTourneySlots);
}

TourneyCard tourneyCard(uint32_t seed, int slot) {
    TourneyCard c;
    if (slot < 0 || slot >= kTourneySlots) return c;
    c.handle = tourneyHandleName(handleIndexFor(seed, slot));
    uint32_t s = slotSeed(seed, slot);
    c.level = rollRange(s, kTourneyMinLevel, kTourneyMaxLevel);
    return c;
}

TourneyFighter tourneyEntrant(const ContentRegistry& reg, uint32_t seed, int slot) {
    TourneyFighter f;
    if (slot < 0 || slot >= kTourneySlots) return f;

    // The level is drawn FIRST, off the same stream tourneyCard reads, so the cheap
    // card and the full fighter agree by construction rather than by both being
    // careful. Everything after it draws from the stream that roll left behind.
    uint32_t s = slotSeed(seed, slot);
    const int level = rollRange(s, kTourneyMinLevel, kTourneyMaxLevel);
    f.spec.level = static_cast<uint16_t>(level);
    std::strncpy(f.spec.tag, tourneyHandleName(handleIndexFor(seed, slot)),
                 sizeof(f.spec.tag) - 1);

    // Species: any creature the roster can field at this level's stage. Filtered on
    // `line` as well as stage, because a lineless row is a bare frame rather than
    // petware somebody raised.
    const Stage stage = tourneyStage(level);
    const auto creatures = reg.allCreatures();
    const CreatureDef* pet = reservoirPick(creatures, s, [stage](const CreatureDef& c) {
        return c.stage == stage && c.line != nullptr;
    });
    if (!pet) return f;                       // an empty creatureId reads as "unbuildable"
    std::strncpy(f.spec.creatureId, pet->id, sizeof(f.spec.creatureId) - 1);

    // Stat points, one per level into a random stat — the SAME grant the player's own
    // level-ups make (Game::addCombatXp), so an entrant's stat line is one a raised pet
    // could actually have. Clamped per stat to the wire cell, which only bites at a
    // level far above kTourneyMaxLevel.
    int points[kLevelStatCount] = {0, 0, 0, 0};
    for (int i = 0; i < level; ++i) ++points[lcg(s) % kLevelStatCount];
    for (int i = 0; i < kLevelStatCount && i < 4; ++i)
        f.spec.statPoints[i] = static_cast<uint8_t>(points[i] > 255 ? 255 : points[i]);

    // Moves: one per slot this stage has unlocked, each matching that slot's stamped
    // Attack/Defend kind on THIS creature. LINE-exclusive moves stay inside the line
    // for free — moveAllowedForLine is the same predicate the player's own picker
    // filters on, so a Worm entrant can no more field a Phishing move than a raised
    // Worm could.
    const auto moves = reg.allMoves();
    const int slots = MoveLoadout::slotsForStage(stage);
    for (int i = 0; i < slots && i < kMaxMoveSlots; ++i) {
        const MoveKind want = pet->slotKinds[i];
        const MoveDef* m = reservoirPick(moves, s, [&](const MoveDef& d) {
            return d.kind == want && moveAllowedForLine(d, pet->line) &&
                   moveUnlockedAtStage(d, stage) &&
                   !alreadyPicked(f.spec.moveIds, i, d.id);
        });
        if (m) std::strncpy(f.spec.moveIds[i], m->id, kPvpIdCap - 1);
    }

    // Mods: any three distinct rows this level could actually field. The equip gate is
    // the pet's own (modEquipLevel), so a low-level entrant is held to the same shelf
    // the player is rather than being handed endgame passives to make it interesting.
    const auto mods = reg.allMods();
    for (int i = 0; i < kModSlots; ++i) {
        const ModDef* m = reservoirPick(mods, s, [&](const ModDef& d) {
            return modEquipLevel(d) <= level && !alreadyPicked(f.spec.modIds, i, d.id);
        });
        if (m) std::strncpy(f.spec.modIds[i], m->id, kPvpIdCap - 1);
    }

    // The Exploit, and the moment it commits to using it. Drawn off the same crew
    // roster the player enlists in, so the abilities an operator meets in the arena are
    // the abilities they can go and earn.
    if (const CrewDef* c = crew(static_cast<int>(lcg(s) % kCrewCount))) {
        f.exploit = {c->exploit.name, c->exploit.kind, c->exploit.magnitude};
        f.exploitAtHealthPct =
            kTourneyTriggerPcts[lcg(s) % static_cast<uint32_t>(kTourneyTriggerCount)];
    }
    return f;
}

Combatant makeTourneyCombatant(const ContentRegistry& reg, const TourneyFighter& f) {
    Combatant c = makePvpCombatant(reg, f.spec);
    c.autoExploit = f.exploit;
    c.autoExploitAtHealthPct = f.exploitAtHealthPct;
    return c;
}

bool tourneyResolveMatch(const ContentRegistry& reg, const TourneyFighter& a,
                         const TourneyFighter& b, uint32_t seed) {
    Combat c;
    c.begin(makeTourneyCombatant(reg, a), makeTourneyCombatant(reg, b),
            Combat::Stakes::Safe, seed, /*forceEnemyFirst=*/false,
            /*carryPlayerHealth=*/-1, /*exploitUses=*/0);
    for (int i = 0; i < kTourneyMatchTurnCap && c.outcome() == Combat::Outcome::Ongoing;
         ++i)
        c.step();
    if (c.outcome() == Combat::Outcome::Win) return true;
    if (c.outcome() == Combat::Outcome::Lose) return false;
    return healthPctOf(c.player()) >= healthPctOf(c.enemy());   // called on Health
}

int tourneyOpponentSlot(uint8_t alive, int slot, int round) {
    if (slot < 0 || slot >= kTourneySlots || round < 0 || round >= kTourneyRounds)
        return -1;
    const int start = tourneyBlockStart(slot, round);
    const int size = tourneyBlockSize(round);
    for (int i = start; i < start + size && i < kTourneySlots; ++i)
        if (i != slot && (alive & (1u << i))) return i;
    return -1;
}

uint32_t tourneyMatchSeed(uint32_t seed, int round, int blockStart) {
    uint32_t s = seed ^ (static_cast<uint32_t>(round + 1) * 0x85EBCA6Bu) ^
                 (static_cast<uint32_t>(blockStart + 1) * 0xC2B2AE35u);
    s = s * 1664525u + 1013904223u;
    s ^= s >> 13;
    return s ? s : 1u;
}

void tourneyResolveRound(const ContentRegistry& reg, uint32_t seed, int round,
                         int playerSlot, uint8_t& alive) {
    if (round < 0 || round >= kTourneyRounds) return;
    const int size = tourneyBlockSize(round);
    const int playerBlock = tourneyBlockStart(playerSlot, round);
    for (int start = 0; start < kTourneySlots; start += size) {
        if (start == playerBlock) continue;          // fought on the screen, not here
        // The two survivors of this block. A block holding fewer than two is already
        // resolved (or malformed) and is left exactly as it is.
        int a = -1, b = -1;
        for (int i = start; i < start + size; ++i) {
            if (!(alive & (1u << i))) continue;
            if (a < 0) a = i;
            else if (b < 0) b = i;
        }
        if (a < 0 || b < 0) continue;
        const bool aWon = tourneyResolveMatch(reg, tourneyEntrant(reg, seed, a),
                                              tourneyEntrant(reg, seed, b),
                                              tourneyMatchSeed(seed, round, start));
        alive &= static_cast<uint8_t>(~(1u << (aWon ? b : a)));
    }
}

int tourneyAliveCount(uint8_t alive) {
    int n = 0;
    for (int i = 0; i < kTourneySlots; ++i)
        if (alive & (1u << i)) ++n;
    return n;
}

}  // namespace mal
