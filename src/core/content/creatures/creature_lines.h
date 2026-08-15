// creature_lines.h — the roster, one entry per creature FAMILY.
//
// kCreatureLines is the ONE place the roster's families and their order are
// declared: kCreatureLineCount and kCreatureCount are both derived from it, so
// adding a family is "write its line.h, add one line here" and there is no second
// count anywhere to fall out of sync with. `embedded_content.cpp` hands this list
// to the registry, which reads creatures family-by-family
// (ContentRegistry::creature / allCreatures).
//
// Order is presentation only. Unlike the EXPL ladder (areas/area_defs.h), nothing
// is keyed by a creature's POSITION: the registry addresses creatures by id, and a
// save records them by id too (save.h's `seenCreatures`). So a family may be
// spliced in anywhere, or reordered, with no save-version concern — what a reorder
// does change is the order the 'Pedia lists species in, since tools/dump_content.cpp
// walks this list.
//
// See CREATURE_CONTENT_STANDARD.md for the folder shape and how to add a family.
#pragma once

#include "core/content/creatures/phishing/line.h"
#include "core/content/creatures/ransomware/line.h"
#include "core/content/creatures/trojan/line.h"
#include "core/content/creatures/worm/line.h"
#include "core/content/content_achievements.h"
#include "core/content/defs.h"

namespace mal {

inline constexpr CreatureLine kCreatureLines[] = {
    {"ransomware", kRansomwareCreatures, kRansomwareCreatureCount,
     linePassives(LinePassive::RansomNote)},
    {"phishing", kPhishingCreatures, kPhishingCreatureCount},
    // The only line a pet can ARRIVE on mid-raise (the cross-line divert), so it is the
    // only one with an achievement for reaching it.
    {"trojan", kTrojanCreatures, kTrojanCreatureCount,
     linePassives(LinePassive::ExecOverride), ach::kTrojanUnleashed},
    {"worm", kWormCreatures, kWormCreatureCount, linePassives(LinePassive::Replication)},
};
inline constexpr int kCreatureLineCount =
    sizeof(kCreatureLines) / sizeof(kCreatureLines[0]);

// Every creature in the build, across all families. Derived, never authored — a
// family's rows are counted where they are defined.
inline constexpr int totalCreatureCount() {
    int n = 0;
    for (const CreatureLine& l : kCreatureLines) n += l.count;
    return n;
}
inline constexpr int kCreatureCount = totalCreatureCount();

// The family carrying `lineId` (CreatureDef::line), or nullptr if none does. This
// is what asking "how many creatures are on this line" costs now — a family lookup
// rather than a scan of the whole roster comparing strings.
inline const CreatureLine* creatureLine(const char* lineId) {
    if (!lineId) return nullptr;
    for (const CreatureLine& l : kCreatureLines)
        if (std::strcmp(l.id, lineId) == 0) return &l;
    return nullptr;
}

}  // namespace mal
