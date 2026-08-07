// content_tables.h — the embedded content tables, one per entity type.
//
// The data is split across sibling content_*.cpp units (one file per content
// type, so each stays skimmable: content_items / content_mods / content_moves /
// content_evolution). Each table is defined with external
// linkage in its own unit and declared here; `embedded_content.cpp` assembles them
// into the ContentSource the registry reads. `<table>Count` travels with each
// array (sizeof only works in the defining unit).
//
// Adding a content type = a new content_<type>.cpp + one extern pair here + one
// accessor in embedded_content.cpp. Adding a row = edit that one table's unit.
#pragma once

#include "core/content/defs.h"

namespace mal {

// Creatures are NOT here: they are grouped into families, one folder each, and
// listed in creatures/creature_lines.h (kCreatureLines). A family's rows are
// authored together, so the roster is a list of families rather than a table of
// rows — see creatures/CREATURE_CONTENT_STANDARD.md.
extern const EggLineDef      kEggLines[];      extern const int kEggLinesCount;
extern const ItemDef         kItems[];         extern const int kItemsCount;
extern const char* const kDefragToolId;  // "disk_scrubber" — MAINT's TOOL-defrag item id
extern const char* const kBackupDriveId;  // "backup_drive" — the combat-shield buff item id
// The walk loot-cache event's reward pool (Game::grantLootReward). Lives with the item
// table because it is a set of item ids; every CACHE's pool is on its own row's CacheDef.
extern const LootEntry kLootPool[];      extern const int kLootPoolCount;
extern const ModDef          kMods[];          extern const int kModsCount;
// Save capacity for the owned-mod pool, in ModDef::wire numbers. The pool ships as a
// COUNT PER WIRE — a nibble each — so what this bounds is the array's addressable range,
// NOT how many mods exist: a roster twice this long is a content problem, a wire number
// past it is a save problem. Raise it in multiples of 2 (a whole byte of nibbles) with a
// save-version note. The array is length-prefixed like the achievement masks, so a
// longer one loads into an older build's shorter view harmlessly and a shorter one reads
// back as "none held".
inline constexpr int kModWireCap = 128;
extern const MoveDef         kMoves[];         extern const int kMovesCount;
extern const DaemonPoolDef   kDaemonPools[];   extern const int kDaemonPoolsCount;

}  // namespace mal
