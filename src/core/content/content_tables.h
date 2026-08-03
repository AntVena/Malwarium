// content_tables.h — the embedded content tables, one per entity type.
//
// The data is split across sibling content_*.cpp units (one file per content
// type, so each stays skimmable: content_creatures / content_items / content_mods
// / content_moves / content_evolution). Each table is defined with external
// linkage in its own unit and declared here; `embedded_content.cpp` assembles them
// into the ContentSource the registry reads. `<table>Count` travels with each
// array (sizeof only works in the defining unit).
//
// Adding a content type = a new content_<type>.cpp + one extern pair here + one
// accessor in embedded_content.cpp. Adding a row = edit that one table's unit.
#pragma once

#include "core/content/defs.h"

namespace mal {

extern const CreatureDef    kCreatures[];     extern const int kCreaturesCount;
extern const EggLineDef      kEggLines[];      extern const int kEggLinesCount;
extern const ItemDef         kItems[];         extern const int kItemsCount;
extern const char* const kDefragToolId;  // "disk_scrubber" — MAINT's TOOL-defrag item id
extern const char* const kBackupDriveId;  // "backup_drive" — the combat-shield buff item id
// The walk loot-cache event's reward pool (Game::grantLootReward). Lives with the item
// table because it is a set of item ids; every CACHE's pool is on its own row's CacheDef.
extern const LootEntry kLootPool[];      extern const int kLootPoolCount;
extern const ModDef          kMods[];          extern const int kModsCount;
extern const MoveDef         kMoves[];         extern const int kMovesCount;
extern const SignalRouteDef  kSignalRoutes[];  extern const int kSignalRoutesCount;
extern const DaemonPoolDef   kDaemonPools[];   extern const int kDaemonPoolsCount;

}  // namespace mal
