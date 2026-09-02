#pragma once

// game_achievements.h — how big is the set an achievement counts over?
//
// The split this file exists for: an achievement row's PROGRESS is player state and lives
// on the Game (Game::achValue, game_pedia.cpp), but the TOTAL it is measured against is a
// static property of the build — how many areas ship, how many creatures a line has, how
// many rig levels exist. That total is what `kGoalAll` compares to, and keeping it here,
// pure and Game-free, means:
//
//   * one definition of "all of them", shared by the unlock check and the 'Pedia's prose,
//     so a row that says "all {n} sub-areas" can never disagree with the row that unlocks
//     when you have cleared them;
//   * tools/dump_content.cpp can publish the expanded trigger text without an engine.
//
// Adding a countable series: add its case to achievementSeriesTotal below (0 if the series
// is unbounded, e.g. a Bits or depth ladder — kGoalAll is meaningless for those, and the
// native gate fails a row that tries).
//
// Header-only and inline, sibling to game_internal.h / game_rig_shop.h.

#include "core/app/game_rig_shop.h"            // kRigUpgrades — the rig row/level totals
#include "core/content/areas/area_defs.h"      // kAreaCount / kSubAreasPerArea
#include "core/content/content_achievements.h"
#include "core/content/content_quotes.h"       // quoteCount() — the DECRYPTOGRAM roster
#include "core/content/content_recipes.h"      // kMergeRecipeCount
#include "core/content/content_tables.h"       // kItems
#include "core/content/creatures/creature_lines.h"  // kCreatureCount / creatureLine()
#include "core/content/effect_text.h"
#include "core/model/combat.h"                 // kWildMalbeastCount

namespace mal {

// How many things `series` counts over in this build, or 0 when the series is unbounded
// (a running total with no ceiling — depth, Bits, boss wins, networks). Pure: it reads the
// shipped tables only, never player state.
inline int achievementSeriesTotal(AchSeries series, const char* key, int param) {
    switch (series) {
        case AchSeries::Event:
        case AchSeries::DeepWebDepth:
        case AchSeries::DeepWebDepthLine:
        case AchSeries::BossWins:
        case AchSeries::NetworksSeen:
        case AchSeries::Handshakes:
        case AchSeries::BitsHeld:
        case AchSeries::PetsRaised:
        case AchSeries::StackerWins:
        case AchSeries::ArcadePlays:
        case AchSeries::ArcadeWins:
        case AchSeries::ArcadeLosses:
        case AchSeries::ArcadeCabinetWins:
        case AchSeries::ArcadeCabinetBest:
        case AchSeries::TourneyWins:
        case AchSeries::PvpWins:
        case AchSeries::MergesCooked:
        case AchSeries::PeersMet:
        case AchSeries::RackHeld:
        // Bounded by the rack, but the rack's ceiling is a rig upgrade — player state,
        // which this must not read. Fixed rungs instead, same as RackHeld above.
        case AchSeries::RackTwins:
        case AchSeries::StepsWalked:
            return 0;                                   // unbounded — no "all of them"
        case AchSeries::SpeciesAtDepth:
        case AchSeries::SpeciesRaised:
        case AchSeries::SpeciesSeen:
        // "One of everything, alive, at once" — the same roster the three above count
        // over, asked of the shelf rather than of a lifetime.
        case AchSeries::RackSpecies:
            return kCreatureCount;
        case AchSeries::RackLines:
            return kCreatureLineCount;
        case AchSeries::QuotesSolved:
            return quoteCount();
        case AchSeries::AreasCleared:
        // One Title per area, which is the same set AreasCleared counts over — a Title
        // IS a cleared area's receipt, so the two totals are one fact, not two that
        // happen to agree today.
        case AchSeries::TitlesUnlocked:
            return kAreaCount;
        case AchSeries::SubAreasCleared:
            return kAreaCount * kSubAreasPerArea;
        case AchSeries::MalbeastsDefeated:
            return kWildMalbeastCount;
        // Both ask "how many creatures are on line `key`" — one of what was ever raised,
        // one of what is on the shelf right now — so they share the family's own count.
        case AchSeries::RackLineHeld:
        case AchSeries::LineRaised: {
            // A line IS a family, so "how many creatures are on it" is the family's
            // own count — no roster scan, and no way to miscount a row whose `line`
            // disagrees with the folder it was authored in.
            const CreatureLine* line = creatureLine(key);
            return line ? line->count : 0;
        }
        case AchSeries::FoodsCollected: {
            int n = 0;
            for (int i = 0; i < kItemsCount; ++i)
                if (kItems[i].type == ItemDef::Type::Food) ++n;
            return n;
        }
        case AchSeries::RarityCollected: {
            int n = 0;
            for (int i = 0; i < kItemsCount; ++i)
                if (static_cast<int>(kItems[i].rarity) == param) ++n;
            return n;
        }
        case AchSeries::ItemCollected:
            return 1;
        case AchSeries::RecipesKnown:
            return kMergeRecipeCount;
        case AchSeries::RigRowsOwned:
            return kRigUpgradeCount;
        case AchSeries::RigCappedLevels: {
            int n = 0;
            for (int i = 0; i < kRigUpgradeCount; ++i)
                if (kRigUpgrades[i].maxLevel != kRigLevelUnlimited)
                    n += kRigUpgrades[i].maxLevel;
            return n;
        }
    }
    return 0;
}

// The threshold a row is actually held to: its own `goal`, or the size of the set it
// counts over when the row says kGoalAll. A kGoalAll row on an unbounded series resolves
// to 0, which never unlocks — the native gate rejects that pairing at build time rather
// than letting it ship as an unearnable badge.
inline int achievementGoal(const AchievementDef& d) {
    return d.goal == kGoalAll ? achievementSeriesTotal(d.series, d.key, d.param) : d.goal;
}

// The row's trigger prose with its numbers filled in. The one call every surface should
// use, so the 'Pedia and any future on-device reader can't render it differently.
inline EffectText achievementTrigger(const AchievementDef& d) {
    return effectText(d, achievementGoal(d));
}

}  // namespace mal
