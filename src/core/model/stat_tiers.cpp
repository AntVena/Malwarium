#include "core/model/stat_tiers.h"

#include <cstdio>
#include <cstring>

namespace mal {

namespace {

// The grid, [stat][tier]. Rows are written in the order the STAT page lists them, and
// the page walks this table rather than carrying a layout of its own.
//
// Every threshold reads kStatTier*Points rather than a literal: the ladder being uniform
// is the feature, and a row that could name its own number is a row that could break it.
const StatTierDef kTiers[kLevelStatCount][kStatTierCount] = {
    // POWER — the accelerating band, then the two ways past a wall.
    {{"OVERCLOCK", kStatTier1Points, kLevelPowerPctPerSpecPoint, kLevelPowerPctPerPoint,
      "Every Power point past this one leans your attack {mag}% instead of {mag2}%."},
     {"RING ZERO", kStatTier2Points, kLevelPowerPiercePct, 0,
      "Your hits run under the wall: they ignore {mag}% of the target's damage cut."},
     {"GUARD SMASH", kStatTier3Points, kLevelPowerGuardSmashPct, 0,
      "A braced target absorbs {mag}% less of your hit. Its turn spent, your problem gone."}},

    // DEFENSE — the % cut bends here, so each rung is paid in something else.
    {{"HARDENING", kStatTier1Points, kLevelDefensePierceResistPct, 0,
      "Armour-piercing loses {mag}% of its bite before it reaches you. The cut you earned "
      "stops being routed around."},
     {"WRITE-BACK", kStatTier2Points, kLevelDefenseBraceRetainPct, 0,
      "An over-sized brace stops being wasted: {mag}% more of what it did not need carries "
      "to the next hit."},
     {"BACKSCATTER", kStatTier3Points, kLevelDefenseBackscatterPct, 0,
      "Your wall pays out. {mag}% of the damage it absorbs is dealt back to whoever swung."}},

    // SPEED — three kinds of tempo, each worth something in a different fight.
    {{"FIRST STRIKE", kStatTier1Points, kLevelSpeedFirstStrikeMult, 0,
      "Win the opening and make it count: if yours is the fight's first landed hit, it "
      "lands {mag}x."},
     {"PRIORITY BOOST", kStatTier2Points, kLevelSpeedUnderdogPerPoint, kLevelSpeedPerPoint,
      "While your Speed points trail the enemy's, each is worth up to {mag} initiative "
      "instead of {mag2} — enough to draw level with them, never to pass them."},
     {"ADRENALINE", kStatTier3Points, kLevelSpeedAdrenalinePerStep,
      kLevelSpeedAdrenalineStepPct,
      "+{mag} initiative for every {mag2}% of your max Health already gone. The worse it "
      "goes, the faster you act."}},

    // MAX-HEALTH — the accelerating band, then two ways to spend the pool twice.
    {{"EXPANSION", kStatTier1Points, kLevelHealthPerSpecPoint, kLevelHealthPerPoint,
      "Every max-Health point past this one is worth {mag} Health instead of {mag2}."},
     {"SCRUBBING", kStatTier2Points, kLevelHealthScrubPct, 0,
      "You heal {mag}% of max Health at the start of each of your turns — after the rot "
      "bites, never before it."},
     {"FAILOVER", kStatTier3Points, 0, 0,
      "Once per fight, the hit that would end you leaves you standing on 1 Health. Spent "
      "before a Backup Drive, so the drive keeps its charge."}},
};

// Substitute this table's two tokens. Deliberately NOT effect_text.cpp's expander: that
// one is keyed to the content-row types (ItemDef/ModDef/MoveDef) and lives a layer up,
// and reaching for it would point core/model at a table it has no other business in. Two
// tokens is the whole vocabulary here, so the cost of its own is a dozen lines.
void expandTier(EffectText& out, const StatTierDef& d) {
    int w = 0;
    for (const char* p = d.effect; *p && w < EffectText::kMaxProse;) {
        int value = 0;
        int skip = 0;
        if (std::strncmp(p, "{mag}", 5) == 0) { value = d.mag; skip = 5; }
        else if (std::strncmp(p, "{mag2}", 6) == 0) { value = d.mag2; skip = 6; }
        if (skip == 0) { out.buf[w++] = *p++; continue; }
        w += std::snprintf(out.buf + w, EffectText::kMaxProse + 1 - w, "%d", value);
        if (w > EffectText::kMaxProse) w = EffectText::kMaxProse;
        p += skip;
    }
    out.buf[w] = '\0';
}

// Clamp a tier index onto the table. A caller asking for a rung that does not exist has
// a bug, but handing it row 0 keeps that bug a wrong sentence rather than a crash on a
// device with no way to report one.
int clampTier(int tier) {
    if (tier < 0) return 0;
    return tier >= kStatTierCount ? kStatTierCount - 1 : tier;
}

int clampStat(LevelStat stat) {
    const int i = static_cast<int>(stat);
    return (i < 0 || i >= kLevelStatCount) ? 0 : i;
}

}  // namespace

const StatTierDef& statTier(LevelStat stat, int tier) {
    return kTiers[clampStat(stat)][clampTier(tier)];
}

int statTierPoints(int tier) {
    // The ladder itself, and the only place its three rungs are enumerated. Read off
    // POWER's column because every column carries the same thresholds — a gate asserts
    // that, so which one is asked is arbitrary rather than a hidden assumption.
    return kTiers[0][clampTier(tier)].points;
}

int statTiersReached(int points) {
    int reached = 0;
    while (reached < kStatTierCount && points >= statTierPoints(reached)) ++reached;
    return reached;
}

int statTierPointsToNext(int points) {
    const int reached = statTiersReached(points);
    if (reached >= kStatTierCount) return 0;     // topped out — nothing left to owe
    const int owed = statTierPoints(reached) - points;
    return owed > 0 ? owed : 0;                  // negatives are not a refund
}

EffectText statTierText(LevelStat stat, int tier) {
    EffectText out;
    expandTier(out, statTier(stat, tier));
    return out;
}

}  // namespace mal
