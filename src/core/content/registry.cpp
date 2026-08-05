#include "core/content/registry.h"

#include <cstring>

namespace mal {

const CreatureDef* ContentRegistry::creature(const char* id) const {
    for (const ContentSource* src : sources_) {
        int n = 0;
        const CreatureDef* arr = src->creatures(n);
        for (int i = 0; i < n; ++i)
            if (std::strcmp(arr[i].id, id) == 0) return &arr[i];
    }
    return nullptr;
}

const EggLineDef* ContentRegistry::eggLine(const char* id) const {
    for (const ContentSource* src : sources_) {
        int n = 0;
        const EggLineDef* arr = src->eggLines(n);
        for (int i = 0; i < n; ++i)
            if (std::strcmp(arr[i].id, id) == 0) return &arr[i];
    }
    return nullptr;
}

const EggLineDef* ContentRegistry::eggLineForCreature(const char* creatureId) const {
    if (!creatureId) return nullptr;
    for (const ContentSource* src : sources_) {
        int n = 0;
        const EggLineDef* arr = src->eggLines(n);
        for (int i = 0; i < n; ++i)
            if (std::strcmp(arr[i].eggCreatureId, creatureId) == 0) return &arr[i];
    }
    return nullptr;
}

const ItemDef* ContentRegistry::item(const char* id) const {
    for (const ContentSource* src : sources_) {
        int n = 0;
        const ItemDef* arr = src->items(n);
        for (int i = 0; i < n; ++i)
            if (std::strcmp(arr[i].id, id) == 0) return &arr[i];
    }
    return nullptr;
}

const ModDef* ContentRegistry::modByWire(int wire) const {
    for (const ContentSource* src : sources_) {
        int n = 0;
        const ModDef* arr = src->mods(n);
        for (int i = 0; i < n; ++i)
            if (arr[i].wire == wire) return &arr[i];
    }
    return nullptr;
}

const ModDef* ContentRegistry::mod(const char* id) const {
    for (const ContentSource* src : sources_) {
        int n = 0;
        const ModDef* arr = src->mods(n);
        for (int i = 0; i < n; ++i)
            if (std::strcmp(arr[i].id, id) == 0) return &arr[i];
    }
    return nullptr;
}

const MoveDef* ContentRegistry::move(const char* id) const {
    for (const ContentSource* src : sources_) {
        int n = 0;
        const MoveDef* arr = src->moves(n);
        for (int i = 0; i < n; ++i)
            if (std::strcmp(arr[i].id, id) == 0) return &arr[i];
    }
    return nullptr;
}

const DaemonPoolDef* ContentRegistry::daemonPool(const char* fromId,
                                                 bool badBranch) const {
    for (const ContentSource* src : sources_) {
        int n = 0;
        const DaemonPoolDef* arr = src->daemonPools(n);
        for (int i = 0; i < n; ++i)
            if (arr[i].badBranch == badBranch &&
                std::strcmp(arr[i].fromId, fromId) == 0)
                return &arr[i];
    }
    return nullptr;
}

std::vector<const ItemDef*> ContentRegistry::allItems() const {
    std::vector<const ItemDef*> out;
    for (const ContentSource* src : sources_) {
        int n = 0;
        const ItemDef* arr = src->items(n);
        for (int i = 0; i < n; ++i) {
            // Earlier sources win on id collision (matches the lookup rule).
            bool dup = false;
            for (const ItemDef* e : out)
                if (std::strcmp(e->id, arr[i].id) == 0) { dup = true; break; }
            if (!dup) out.push_back(&arr[i]);
        }
    }
    return out;
}

std::vector<const ModDef*> ContentRegistry::allMods() const {
    std::vector<const ModDef*> out;
    for (const ContentSource* src : sources_) {
        int n = 0;
        const ModDef* arr = src->mods(n);
        for (int i = 0; i < n; ++i) {
            bool dup = false;
            for (const ModDef* e : out)
                if (std::strcmp(e->id, arr[i].id) == 0) { dup = true; break; }
            if (!dup) out.push_back(&arr[i]);
        }
    }
    return out;
}

std::vector<const MoveDef*> ContentRegistry::allMoves() const {
    std::vector<const MoveDef*> out;
    for (const ContentSource* src : sources_) {
        int n = 0;
        const MoveDef* arr = src->moves(n);
        for (int i = 0; i < n; ++i) {
            bool dup = false;
            for (const MoveDef* e : out)
                if (std::strcmp(e->id, arr[i].id) == 0) { dup = true; break; }
            if (!dup) out.push_back(&arr[i]);
        }
    }
    return out;
}

std::vector<const CreatureDef*> ContentRegistry::allCreatures() const {
    std::vector<const CreatureDef*> out;
    for (const ContentSource* src : sources_) {
        int n = 0;
        const CreatureDef* arr = src->creatures(n);
        for (int i = 0; i < n; ++i) {
            bool dup = false;
            for (const CreatureDef* e : out)
                if (std::strcmp(e->id, arr[i].id) == 0) { dup = true; break; }
            if (!dup) out.push_back(&arr[i]);
        }
    }
    return out;
}

std::vector<const EggLineDef*> ContentRegistry::allEggLines() const {
    std::vector<const EggLineDef*> out;
    for (const ContentSource* src : sources_) {
        int n = 0;
        const EggLineDef* arr = src->eggLines(n);
        for (int i = 0; i < n; ++i) {
            bool dup = false;
            for (const EggLineDef* e : out)
                if (std::strcmp(e->id, arr[i].id) == 0) { dup = true; break; }
            if (!dup) out.push_back(&arr[i]);
        }
    }
    return out;
}

const SpriteData* ContentRegistry::creatureSprite(const CreatureDef& c) const {
    return sprite(c.spriteName);
}

ContentRegistry ContentRegistry::embedded() {
    ContentRegistry r;
    r.addSource(embeddedContent());
    r.setAssets(embeddedAssets());
    return r;
}

} // namespace mal
