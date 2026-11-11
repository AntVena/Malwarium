// embedded_content.cpp: the content pack + art, compiled in.
//
// This is the FIRST ContentSource/AssetSource implementation. It holds no content
// itself: each table lives in its own content_<type>.cpp unit (see content_tables.h)
// so every table stays small and skimmable. This file just ASSEMBLES those tables into
// the ContentSource the registry reads. An SD-backed source is a parallel implementation
// merged at boot, not a change to anything that reads the registry.
#include <algorithm>
#include <cstring>

#include "core/content/content_source.h"
#include "core/content/content_tables.h"
#include "core/content/creatures/creature_lines.h"
#include "core/content/items/item_families.h"
#include "generated/assets.h"

namespace mal {

namespace {

class EmbeddedContent : public ContentSource {
public:
    const CreatureLine* creatureLines(int& count) const override {
        count = kCreatureLineCount;
        return kCreatureLines;
    }
    const EggLineDef* eggLines(int& count) const override {
        count = kEggLinesCount;
        return kEggLines;
    }
    const ItemFamily* itemFamilies(int& count) const override {
        count = kItemFamilyCount;
        return kItemFamilies;
    }
    const ModDef* mods(int& count) const override {
        count = kModsCount;
        return kMods;
    }
    const MoveDef* moves(int& count) const override {
        count = kMovesCount;
        return kMoves;
    }
    const MoveDef* chainSteps(int& count) const override {
        count = kChainStepsCount;
        return kChainSteps;
    }
    const DaemonPoolDef* daemonPools(int& count) const override {
        count = kDaemonPoolsCount;
        return kDaemonPools;
    }
};

class EmbeddedAssets : public AssetSource {
public:
    const SpriteData* sprite(const char* name) const override {
        const AssetEntry* end = kAssetTable + kAssetCount;
        const AssetEntry* it = std::lower_bound(
            kAssetTable, end, name, [](const AssetEntry& e, const char* n) {
                return std::strcmp(e.name, n) < 0;
            });
        return (it != end && std::strcmp(it->name, name) == 0) ? it->data : nullptr;
    }
};

} // namespace

const ContentSource& embeddedContent() {
    static const EmbeddedContent inst;
    return inst;
}
const AssetSource& embeddedAssets() {
    static const EmbeddedAssets inst;
    return inst;
}

} // namespace mal
