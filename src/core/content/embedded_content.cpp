// embedded_content.cpp — the content pack + art, compiled in.
//
// This is the FIRST ContentSource/AssetSource implementation. It holds no content
// itself: each table lives in its own content_<type>.cpp unit (see content_tables.h)
// so every table stays small and skimmable. This file just ASSEMBLES those tables into
// the ContentSource the registry reads. An SD-backed source is a parallel implementation
// merged at boot — not a change to anything that reads the registry.
#include <cstring>

#include "core/content/content_source.h"
#include "core/content/content_tables.h"
#include "generated/assets.h"

namespace mal {

namespace {

class EmbeddedContent : public ContentSource {
public:
    const CreatureDef* creatures(int& count) const override {
        count = kCreaturesCount;
        return kCreatures;
    }
    const EggLineDef* eggLines(int& count) const override {
        count = kEggLinesCount;
        return kEggLines;
    }
    const ItemDef* items(int& count) const override {
        count = kItemsCount;
        return kItems;
    }
    const ModDef* mods(int& count) const override {
        count = kModsCount;
        return kMods;
    }
    const MoveDef* moves(int& count) const override {
        count = kMovesCount;
        return kMoves;
    }
    const SignalRouteDef* signalRoutes(int& count) const override {
        count = kSignalRoutesCount;
        return kSignalRoutes;
    }
    const DaemonPoolDef* daemonPools(int& count) const override {
        count = kDaemonPoolsCount;
        return kDaemonPools;
    }
};

class EmbeddedAssets : public AssetSource {
public:
    const SpriteData* sprite(const char* name) const override {
        for (int i = 0; i < kAssetCount; ++i)
            if (std::strcmp(kAssetTable[i].name, name) == 0)
                return kAssetTable[i].data;
        return nullptr;
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
