#include "core/model/combat.h"

#include <cstring>
#include <utility>

#include "tunables.h"
#include "core/content/areas/area_defs.h"
#include "core/content/areas/deepweb_dive/area.h"
#include "core/content/registry.h"
#include "core/model/loadout.h"
#include "core/model/move_loadout.h"

namespace mal {

void Combatant::setLine(const ContentRegistry& reg, const char* lineId) {
    line = lineId;
    const CreatureLine* cl = reg.creatureLine(lineId);
    linePassives = cl ? cl->passives : 0;
}

// The wildcard pools (MoveDef::drawLineA/B), built alongside the chain steps and for the
// same reason: the registry is read HERE so the turn engine never has to.
//
// One pool per slot, banded generic-then-A-then-B in a single vector (WildPool). A row is
// eligible when it matches the wildcard's own KIND — an Attack wildcard rolls attacks, so
// the slot typing an operator equipped into still means what it says — and when the pet's
// stage has unlocked it, which is what keeps a Boot pet off the Daemon roster without a
// per-stage table anywhere.
//
// The metamorphic track excludes itself for free: a pool is the generic roster plus two of
// the OTHER lines, and a line's own rows are gated to its line, so nothing here has to
// filter them and no roll can nest.
void buildWildPools(const ContentRegistry& reg, Combatant& c, Stage stage) {
    // Nothing is allocated for a kit holding no wildcard row, which is every fight the
    // metamorphic track is not in. The pools are the only per-fight heap this file would
    // add, and a build that pays for them regardless would charge every pet on the roster
    // for a line it is not on — on a board where an allocation that fails takes the whole
    // device down with it (no exceptions).
    bool any = false;
    for (const MoveDef* m : c.moves)
        if (m && moveIsWildcard(*m)) { any = true; break; }
    if (!any) return;
    c.wildPools.assign(c.moves.size(), WildPool{});
    for (size_t i = 0; i < c.moves.size(); ++i) {
        const MoveDef* w = c.moves[i];
        if (!w || !moveIsWildcard(*w)) continue;
        c.polymorphic = true;
        WildPool& p = c.wildPools[i];
        auto take = [&](const char* line) {
            for (const MoveDef* m : reg.allMoves()) {
                if (m->kind != w->kind || !moveUnlockedAtStage(*m, stage)) continue;
                const bool generic = m->line == nullptr;
                if (line ? (generic || std::strcmp(m->line, line) != 0) : !generic) continue;
                p.rows.push_back(m);
            }
        };
        take(nullptr);                                  // the shared roster
        p.genericEnd = static_cast<int>(p.rows.size());
        if (w->drawLineA) take(w->drawLineA);
        p.lineAEnd = static_cast<int>(p.rows.size());
        if (w->drawLineB) take(w->drawLineB);
        // A borrowed row arrives carrying its line's passive, so the flags are resolved
        // from the same read that found the rows (Combatant::setLine makes this lookup too).
        if (const CreatureLine* la = w->drawLineA ? reg.creatureLine(w->drawLineA) : nullptr)
            p.passivesA = la->passives;
        if (const CreatureLine* lb = w->drawLineB ? reg.creatureLine(w->drawLineB) : nullptr)
            p.passivesB = lb->passives;
    }
}

void resolveChains(const ContentRegistry& reg, Combatant& c) {
    c.chainFollow.assign(c.moves.size(), nullptr);
    for (size_t i = 0; i < c.moves.size(); ++i)
        if (c.moves[i] && c.moves[i]->chainNextId)
            c.chainFollow[i] = reg.chainStep(c.moves[i]->chainNextId);
}

Combatant makePlayerCombatant(const ContentRegistry& reg, const CreatureDef& pet,
                              const MoveLoadout& moves, const Loadout& mods) {
    Combatant c;
    c.name = pet.displayName;
    c.spriteName = pet.spriteName;
    c.creature = &pet;                              // authored clips, for the fight's poses
    c.stage = pet.stage;                            // drives the per-line passive
    c.setLine(reg, pet.line);                       // line identity + its passives, read once
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
            // MULTIPLICATIVE, for the reason applyLevelStatPoints (below) spells out at
            // length about the level bonus: a flat add lands on a base kStagePowerScalePct
            // has already inflated 100 -> 230, so the same row was worth 18% of output on a
            // Process pet and 7.8% on a Daemon — it decayed across exactly the stretch a
            // player spends earning the mod. The prose says "raises attack power by {mag}%"
            // and this is what makes that true at every stage. Identical arithmetic at
            // scale 100, so nothing early moves; only the late decay goes away.
            //
            // The CONDITIONAL power rows (Meltdown Core, Zero-Day, the Ledger's owed half)
            // still add into `mult` at the damage calc and are left that way on purpose:
            // they measure healthy, their magnitudes were picked against that base, and
            // they are read at a different point in the pipeline. Move them only with a
            // sweep in hand — the units are not interchangeable.
            case ModEffect::PowerPct:
                c.powerMultPct = c.powerMultPct * (100 + mag) / 100;
                break;
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
            case ModEffect::ArmorPiercePct:    // DRM Stripper — read at applyEffect
            case ModEffect::RegenPerTurn:      // Trickle Charger — read at resolveTurn
            case ModEffect::FirstStrikeRankMult:
            case ModEffect::FirstHitCutPct:
            case ModEffect::LowHealthPowerPct:
            case ModEffect::GambleBattlePowerPct:
            case ModEffect::ConditionalThorns:
            case ModEffect::StealAmplifyPct:
            case ModEffect::ExecOverridePct:   // Ring-0 Shim — read at execOverrideChance
            case ModEffect::ReplicaSpawnPct:   // Replication Bus — read at rollWormSpawn
            case ModEffect::ExtortionLedger:
                // The STANDING half is a damage cut and lands on the base, under the same
                // never-immune clamp every other cut answers to — and whatever that clamp
                // refuses is paid in max-Health rather than dropped, so the row pays what
                // it prints on a pet that is already at the wall (capOverflowHealth).
                // Only the seizure WINDOW stays live, since it opens and closes mid-fight.
                c.dmgReducePct += mag;
                if (c.dmgReducePct > kLevelDmgReduceMaxPct) {
                    const int over = c.dmgReducePct - kLevelDmgReduceMaxPct;
                    c.dmgReducePct = kLevelDmgReduceMaxPct;
                    const int gain = capOverflowHealth(over, kLevelDefensePctPerPoint);
                    c.maxHealth += gain;
                    c.health += gain;      // as ModEffect::MaxHealth does, and for its reason
                }
                c.mods.apply(m->effectKind, mag, m->magnitude2);
                break;
            case ModEffect::ReplicaWorthPct:     // Replication Bus — read at a copy's spawn
            case ModEffect::PolymorphEffectPct:  // Mutation Engine — read at the turn engine
                c.mods.apply(m->effectKind, mag, m->magnitude2);
                break;
            case ModEffect::AttackCountPowerPct: {  // Botnet Swarm — +mag% power PER Attack move
                int atk = 0;
                for (const MoveDef* cm : c.moves)
                    if (cm->kind == MoveDef::Kind::Attack) ++atk;
                c.powerMultPct = c.powerMultPct * (100 + mag * atk) / 100;  // as PowerPct
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
    resolveChains(reg, c);
    buildWildPools(reg, c, c.stage);
    return c;
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
    //
    // The last field is the creature's own SIGNATURE (CombatEnemy::signatureMoveId): the
    // one move that is ITS, carried past the depth ladder that overwrites everything else
    // in the row. The moveIds beside it are the tier's baseline and survive only at the
    // shallowest sub-area; the signature is what a body says about itself at every depth,
    // and what makes beating this one worth more than beating the other one at the same
    // rung. The rows are in content_moves.cpp under "Wild SIGNATURES".
    if (sectorTier <= 1) {
        static const CombatEnemy kTier1[] = {
            {"GlitchHog", "SPR_MALBEAST_GLITCHHOG", 1, 35, 9, {"quick_jab"}, true,
             "screen_tear"},
            {"Segfault Pup", "SPR_MALBEAST_SEGFAULT_PUP", 1, 30, 10,
             {"quick_jab"}, true, "wild_pointer"},
        };
        return kTier1[variantRoll % 2];
    }
    if (sectorTier == 2) {
        static const CombatEnemy kTier2[] = {
            {"Packet Wraith", "SPR_MALBEAST_PACKET_WRAITH", 2, 55, 12,
             {"quick_jab", "packet_storm"}, true, "dropped_packet"},
            {"Cache Ghoul", "SPR_MALBEAST_CACHE_GHOUL", 2, 50, 13,
             {"quick_jab", "packet_storm"}, true, "stale_read"},
        };
        return kTier2[variantRoll % 2];
    }
    static const CombatEnemy kTier3[] = {
        {"Buffer Wyrm", "SPR_MALBEAST_BUFFER_WYRM", 3, 80, 14,
         {"packet_storm", "fork_bomb"}, true, "coil_overrun"},
        // The apex gets a bigger 64x56 cell than the 56x48 the rest of the roster
        // uses — drawn from its own frame size, so nothing else has to know.
        {"Kernel Leviathan", "SPR_MALBEAST_KERNEL_LEVIATHAN", 3, 85, 13,
         {"packet_storm", "fork_bomb"}, true, "ring_zero"},
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

void applyWildSubAreaRamp(CombatEnemy& e, int areaIdx, int sub) {
    if (areaIdx < 0) areaIdx = 0;
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
    e.level = areaIdx * kSubAreasPerArea + sub;
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
    if (sub > 0) e.moveIds = kLadder[sub];             // sub 0: keep the roster baseline

    // ...and this AREA's own pair, on top of whatever rung the ladder just set. The two
    // levers answer different questions and so compose rather than replace: the ladder is
    // HOW HARD this rung swings, the pair is WHERE the player is. Without it a wild reads
    // only as its tier — the same three moves everywhere, so an encounter in the Bayou and
    // one in the Moors are distinguishable by nothing a fight can show.
    //
    // Depth picks how much of the pair rides. The Attack comes from the first rung, so a
    // zone announces itself immediately; the Defend joins deeper, which is the weighting
    // and also the honest cost — a braced wild is a LONGER fight, not a harder one, and
    // the rung that has already earned more Health and speed is the one that can afford
    // to spend a turn holding. One brace, never two: chooseMove is uniform, and the same
    // reason kMaxBossTeaches caps a boss at one applies to anything that takes turns.
    const AreaDef& a = area(areaIdx);
    if (a.wildAttackMoveId) e.moveIds.push_back(a.wildAttackMoveId);
    if (a.wildDefendMoveId && sub >= kWildAreaDefendSub)
        e.moveIds.push_back(a.wildDefendMoveId);

    // ...and the CREATURE's own, the third lever, riding every rung at every depth. The
    // ladder says how hard this rung swings and the area pair says where the player is;
    // without this the body itself says nothing, and two malbeasts sharing a tier are one
    // fight wearing two sprites. Appended for the same reason the pair is: it composes
    // with the rung rather than replacing it, so the depth ramp the ladder builds is
    // still there underneath.
    //
    // Signatures are authored small (content_moves.cpp) precisely so that adding one
    // cannot reorder the rungs — the ladder is sorted by EFFECTIVE per-turn damage, and a
    // constant appended to every rung alike moves them all the same way.
    if (e.signatureMoveId) e.moveIds.push_back(e.signatureMoveId);
}

std::vector<const char*> deepWebMoveIds(int depth, uint32_t roll) {
    if (depth < 0) depth = 0;
    // Past the gate the deep pool REPLACES the rung — a boss move is the whole point of
    // being this far down, not a rare garnish on the same kit.
    const bool deep = depth >= kDeepWebBossMoveDepth;
    int rung = 0;                                // the deepest rung this depth has opened
    while (rung + 1 < kDeepWebMoveRungTotal && depth >= kDeepWebMoveRungDepths[rung + 1])
        ++rung;
    const char* const* pool = deep ? kDeepWebMovesBoss : kDeepWebMoveRungs[rung];
    const int poolN = deep ? kDeepWebMovesBossCount : kDeepWebMoveRungCounts[rung];
    // Two distinct picks: enough that a dive enemy is not one move on repeat, few enough
    // that Combat::chooseMove's uniform pick still lets each one read. Drawn without
    // replacement so a "pair" is never the same move twice wearing two hats.
    std::vector<const char*> out;
    for (int i = 0; i < 2 && static_cast<int>(out.size()) < poolN; ++i) {
        for (int tries = 0; tries < poolN; ++tries) {
            roll = roll * 1664525u + 1013904223u;
            const char* pick = pool[(roll >> 16) % static_cast<uint32_t>(poolN)];
            bool dup = false;
            for (const char* got : out)
                if (std::strcmp(got, pick) == 0) { dup = true; break; }
            if (!dup) { out.push_back(pick); break; }
        }
    }
    return out;
}

void applyDeepWebScale(CombatEnemy& e, int petLevel, int depth, uint32_t roll) {
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

    // A BUDGET OF POINTS, SPENT AT RANDOM — the same shape a pet's own growth takes (one
    // point per level into one of four stats, game_combat.cpp's addCombatXp), so a dive
    // enemy is a peer built the way the player was built rather than a second curve to
    // reason about. Health used to be the only thing depth moved, which is why a deep
    // enemy was a bigger bag of the same harmless swings: the fix is that Power is now in
    // the same hat as Health.
    //
    // The spread is per-point rather than a fixed split, so a shallow dive throws real
    // variety — a glass cannon, a wall, a blur — while a deep one evens out on its own as
    // the count grows. Nothing has to special-case that; it is just what many rolls do.
    //
    // The budget OUTGROWS the pet on purpose. effLevel's depth half is logarithmic and
    // flattens, so on its own it converges to a fair fight that a good build wins forever.
    // The linear term below is what makes the zone endless in the honest sense: dive far
    // enough and the arithmetic beats you. The streak is the score.
    const int budget = effLevel + depth / kDeepWebDepthPointsPerN;
    int points[kLevelStatCount] = {0, 0, 0, 0};
    for (int i = 0; i < budget; ++i) {
        roll = roll * 1664525u + 1013904223u;
        ++points[(roll >> 16) % kLevelStatCount];
    }
    // ...and what it fights WITH, drawn from the rung this depth has reached. Done here
    // rather than at the call site so "a dive enemy" is one statement: the roster picked
    // the body, the depth picked everything else about it.
    e.moveIds = deepWebMoveIds(depth, roll);
    // ...plus the dive's OWN pair, weighted by depth exactly as an area weights its own
    // (deepweb_dive/area.h): the Attack from the first dive, the Defend once the zone has
    // had a rung's worth of depth to establish itself. Appended rather than folded into
    // the rungs so the dive's two moves survive kDeepWebBossMoveDepth, past which the boss
    // pool replaces the rung and would otherwise take them with it.
    e.moveIds.push_back(kDeepWebWildAttackMoveId);
    if (depth >= kDeepWebWildDefendDepth) e.moveIds.push_back(kDeepWebWildDefendMoveId);

    e.powerMultPct += points[0] * kLevelPowerPctPerPoint;
    // Same diminishing curve and ceiling the pet's Defence answers to — an enemy is not
    // allowed a wall the player could not have built, and makeEnemyCombatant re-clamps.
    e.dmgReducePct += levelDefenseCutPct(points[1]);
    e.speed += points[2] * kLevelSpeedPerPoint;
    e.maxHealth += points[3] * kDeepWebHealthPerLevel;
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

namespace {
// One boss malbeast at a given DEPTH. Health climbs with the area's ladder depth
// (areaTier) + the sub index, so sub 1 opens easy and the signature sub 5 is the wall;
// moves thicken as the sub climbs. Generic frame (no new art).
//
// `sub` is the depth to build AT, which is not always the sub-area being fought: an escort
// round (SubBossRound::rung) is the same fight drawn a rung shallower, and expressing that
// as "run the whole formula at sub-1" is what keeps an escort automatically consistent with
// the boss it guards — there is no second stat curve to keep in step with this one.
CombatEnemy subBossEnemy(const AreaDef& a, int tier, int sub, const char* name,
                         const SubBossDef* teacher, const char* extraMoveId = nullptr) {
    const int health = kSubBossHealthBase + tier * 8 + sub * kSubBossHealthStep;
    const int speed = kSubBossSpeedBase + tier + (sub >= kSubAreasPerArea - 1 ? 2 : 0);
    std::vector<const char*> moves = {"quick_jab"};
    if (sub >= 2) moves.push_back("packet_storm");
    if (sub >= kSubAreasPerArea - 1) {
        moves.push_back("fork_bomb");
        // The signature (sub 4) boss debuts its area's THREAT rider — the telegraphed
        // apex where you'd bring the matching counter-mod, earned in that same area's
        // own loot table (AreaDef::modPoolIds). An escort drops to a shallower `sub` and
        // so never carries it: the rider is the wall's tell, not the doorway's.
        if (a.apexThreatMoveId) moves.push_back(a.apexThreatMoveId);
    }
    // ...and what this boss TEACHES, on top of the depth spine. This is the only reason
    // most of the move roster is reachable at all: a drop is drawn from the defeated
    // enemy's kit, so a move reaches a player exactly when some boss row names it. An
    // escort carries its boss's list too — it is that boss a rung shallower, and the drop
    // filter (not-yet-owned) already stops the extra rounds from paying twice.
    if (teacher)
        for (const char* id : teacher->teaches)
            if (id) moves.push_back(id);
    if (extraMoveId) moves.push_back(extraMoveId);
    return {name, "SPR_PET_CACHEMUTT", tier + 1, health, speed, std::move(moves)};
}
}  // namespace

BossGauntlet subAreaBoss(int areaIdx, int sub) {
    // The sub-area's boss, fought as the rounds its own row spells out — usually one, but
    // a row may author escorts (Castle Rapidscare's pawn ranks) and they run back-to-back
    // on the same carried-Health plumbing (startBossRound/finishBossRound) the area boss
    // already uses. Boss names are a content pool disjoint from the roster + the wild
    // malbeasts (namespace guard, tested), owned by each area's own AreaDef.
    if (areaIdx < 0) areaIdx = 0;
    if (areaIdx >= kAreaCount) areaIdx = kAreaCount - 1;
    if (sub < 0) sub = 0;
    if (sub >= kSubAreasPerArea) sub = kSubAreasPerArea - 1;
    const AreaDef& a = area(areaIdx);
    const SubBossDef& b = a.subBosses[sub];
    const int tier = areaTier(areaIdx);
    BossGauntlet g{b.name, tier + 1, {}};
    for (int r = 0; r < b.roundCount(); ++r) {
        const SubBossRound rd = b.round(r);
        // An escort's rung is a DELTA, so the depth it lands on may sit BELOW the area's
        // first sub-area — which is the point: a doorway boss (sub 0) has nowhere shallower
        // to draw from, and flooring at 0 would silently hand it an escort identical to
        // itself. The formula stays sound below zero (Health is base + tier*8 + depth*step,
        // and the move rungs are all `depth >= n` tests), so the floor only has to keep
        // Health positive: a full ladder-span below is as far as any escort can reach.
        int depth = sub + rd.rung;
        if (depth < -(kSubAreasPerArea - 1)) depth = -(kSubAreasPerArea - 1);
        if (depth >= kSubAreasPerArea) depth = kSubAreasPerArea - 1;
        g.rounds.push_back(subBossEnemy(a, tier, depth, rd.name, &b));
    }
    return g;
}

BossGauntlet areaBoss(int areaIdx) {
    // The five sub-area bosses fought back-to-back. Health carries across
    // the rounds (no heal), so this is the real finale. The apex (round 5) is the
    // signature sub-area boss — the piece curation upgrades first.
    //
    // Each sub-area contributes its boss PROPER — that banner, at its own rung — and none
    // of the escorts around it. So the finale stays exactly five rounds however many
    // escorts a sub-area grows: composing whole sub-gauntlets would balloon the area boss
    // every time one rung gained a round, a cost nobody authoring that rung intends. It is
    // also why this builds the enemy directly instead of picking a round out of
    // subAreaBoss: a round is named for the shape it takes IN ITS OWN fight (THE BACK
    // RANK), and the finale is announcing the boss (THE EIGHT PWNS).
    if (areaIdx < 0) areaIdx = 0;
    if (areaIdx >= kAreaCount) areaIdx = kAreaCount - 1;
    const AreaDef& a = area(areaIdx);
    const int tier = areaTier(areaIdx);
    BossGauntlet g{a.areaBossName, tier + 1, {}};
    for (int s = 0; s < kSubAreasPerArea; ++s) {
        // The area boss's OWN move rides on the LAST round only, so it is a reward for
        // clearing all five stages rather than for beating whichever sub-area sits last —
        // that sub-boss's own fight never carries it (a drop is rolled per round, off the
        // round's own kit, so where the move sits IS what it costs to learn).
        const bool finale = (s == kSubAreasPerArea - 1);
        g.rounds.push_back(subBossEnemy(a, tier, s, a.subBosses[s].name, &a.subBosses[s],
                                        finale ? a.areaBossMoveId : nullptr));
    }
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

// The accelerating pair. Counted the same exact way levelDefenseCutPct counts its bent
// stretch — whole points, one multiply per band — so the two curves are readable against
// each other and neither rounds a band away.
int levelPowerPct(int points) {
    if (points <= 0) return 0;
    const int base = points < kLevelPowerSpecPoints ? points : kLevelPowerSpecPoints;
    const int spec = points - base;
    int pct = base * kLevelPowerPctPerPoint + spec * kLevelPowerPctPerSpecPoint;
    if (pct > kLevelPowerSpecCapPct) pct = kLevelPowerSpecCapPct;
    return pct;
}

int levelHealthBonus(int points) {
    if (points <= 0) return 0;
    const int base = points < kLevelHealthSpecPoints ? points : kLevelHealthSpecPoints;
    const int spec = points - base;
    int hp = base * kLevelHealthPerPoint + spec * kLevelHealthPerSpecPoint;
    if (hp > kLevelHealthSpecCap) hp = kLevelHealthSpecCap;
    return hp;
}

int levelDefensePierceResistPct(int points) {
    return points >= kLevelDefensePierceResistPoints ? kLevelDefensePierceResistPct : 0;
}

int levelDefenseBraceRetainPct(int points) {
    return points >= kLevelDefenseBraceRetainPoints ? kLevelDefenseBraceRetainPct : 0;
}

// The curve before its ceiling — the one thing both answers below are cut from.
int levelDefenseCutRawPct(int points) {
    if (points <= 0) return 0;
    // Full rate up to the soft point, HALF rate after — the diminishing half of a stat
    // that also has a hard ceiling. Integer and exact: the bent stretch is counted in
    // whole points and halved once, rather than halving each point (which would round
    // every one of them down to the same place and quietly stall the curve flat).
    const int full = points < kLevelDefenseSoftPoints ? points : kLevelDefenseSoftPoints;
    const int bent = points - full;
    return full * kLevelDefensePctPerPoint + bent * kLevelDefensePctPerPoint / 2;
}

int levelDefenseCutPct(int points) {
    const int cut = levelDefenseCutRawPct(points);
    return cut > kLevelDefenseCapPct ? kLevelDefenseCapPct : cut;
}

// The pair's other half — see the declaration (combat.h).
int levelDefenseCutOverflowPct(int points) {
    const int cut = levelDefenseCutRawPct(points);
    return cut > kLevelDefenseCapPct ? cut - kLevelDefenseCapPct : 0;
}

// The exchange and why it is this one are on the declaration (combat.h).
int capOverflowHealth(int overflowPct, int perPointPct) {
    if (overflowPct <= 0 || perPointPct <= 0) return 0;
    return overflowPct * kLevelHealthPerPoint / perPointPct;
}

void applyLevelStatPoints(Combatant& c, const int statPoints[4]) {
    if (!statPoints) return;
    // power → +% attack lean, ACCELERATING past its specialisation point (levelPowerPct);
    // defense → +% incoming-damage cut (diminishing past the soft point, its own cap, then
    // the total cut is clamped so defense can never null a hit) AND +% defend-move brace
    // magnitude AND, past their thresholds, the two investment TIERS the % cut cannot be
    // paid in (pierce resist, brace retain); speed → +initiative, the one stat whose value
    // is flat in both directions; max-Health → +HP, accelerating like power.
    // MULTIPLICATIVE, not additive — and this is not stage scaling by the back door. A
    // level bonus added into powerMultPct lands on a base the stage multiplier has already
    // inflated (kStagePowerScalePct runs 100 -> 230), so the same earned point was worth
    // 32% more damage on a Process pet and 14% on a Daemon: the stat quietly decayed
    // across exactly the stretch a player spends earning it. As a percentage OF the pet's
    // own output it is worth the same everywhere, which is what the row always said it
    // was. It scales whatever that output happens to be, mods included — a power bonus
    // applying to your power is the reading every one of those rows already invites.
    c.powerMultPct = c.powerMultPct * (100 + levelPowerPct(statPoints[0])) / 100;
    c.dmgReducePct += levelDefenseCutPct(statPoints[1]);
    c.pierceResistPct = levelDefensePierceResistPct(statPoints[1]);
    c.braceRetainPct = kBraceRetainBasePct + levelDefenseBraceRetainPct(statPoints[1]);
    // Everything the three Defence ceilings refuse, paid into max-Health (capOverflowHealth).
    // Three separate discards and no double count: the curve's own ceiling refuses part
    // of the cut this pet EARNED, the never-immune clamp then refuses part of the total it
    // is added to (mods included — the cut a mod added is already on the stat by now),
    // and the brace cap refuses its own.
    int overflow = levelDefenseCutOverflowPct(statPoints[1]);
    if (c.dmgReducePct > kLevelDmgReduceMaxPct) {
        overflow += c.dmgReducePct - kLevelDmgReduceMaxPct;
        c.dmgReducePct = kLevelDmgReduceMaxPct;
    }
    int braceOverflow = 0;
    int brace = statPoints[1] * kLevelDefenseBracePctPerPoint;
    if (brace > kLevelDefenseBraceCapPct) {
        braceOverflow = brace - kLevelDefenseBraceCapPct;
        brace = kLevelDefenseBraceCapPct;
    }
    c.defenseMultPct += brace;
    c.speed += statPoints[2] * kLevelSpeedPerPoint;
    c.maxHealth += levelHealthBonus(statPoints[3]);
    c.maxHealth += capOverflowHealth(overflow, kLevelDefensePctPerPoint);
    c.maxHealth += capOverflowHealth(braceOverflow, kLevelDefenseBracePctPerPoint);
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
    c.powerMultPct = spec.powerMultPct;
    // Held to the same never-immune clamp the player's own defence answers to, rather
    // than trusted from the spec: a rolled dive enemy (applyDeepWebScale) can spend an
    // arbitrary pile of points here, and an enemy nobody can hurt is the same broken
    // fight as a pet nobody can hurt.
    c.dmgReducePct = spec.dmgReducePct > kLevelDmgReduceMaxPct ? kLevelDmgReduceMaxPct
                                                               : spec.dmgReducePct;
    if (spec.isWild) {                              // wild-encounter challenge buff
        c.maxHealth = c.maxHealth * kWildEnemyHealthPct / 100;
        c.health = c.maxHealth;
        c.enemyDamageMultPct = kWildEnemyDamagePct;
    }
    for (const char* id : spec.moveIds)
        if (const MoveDef* m = reg.move(id)) c.moves.push_back(m);
    if (c.moves.empty())                            // never actionless
        if (const MoveDef* d = reg.move("quick_jab")) c.moves.push_back(d);
    resolveChains(reg, c);
    buildWildPools(reg, c, c.stage);
    return c;
}

} // namespace mal
