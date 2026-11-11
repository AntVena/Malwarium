#include "core/content/registry.h"

#include <cstring>

namespace mal {

void ContentRegistry::addSource(const ContentSource& src) {
    sources_.push_back(&src);
    int n = 0;
    const CreatureLine* lines = src.creatureLines(n);
    lines_.merge(lines, n);
    for (int l = 0; l < n; ++l) creatures_.merge(lines[l].rows, lines[l].count);
    const EggLineDef* eggs = src.eggLines(n);
    eggLines_.merge(eggs, n);
    const ItemFamily* families = src.itemFamilies(n);
    for (int f = 0; f < n; ++f) items_.merge(families[f].rows, families[f].count);
    const ModDef* mods = src.mods(n);
    mods_.merge(mods, n);
    const MoveDef* moves = src.moves(n);
    moves_.merge(moves, n);
    const MoveDef* steps = src.chainSteps(n);
    chainSteps_.merge(steps, n);
}

const CreatureDef* ContentRegistry::creature(const char* id) const {
    return creatures_.find(id);
}

const CreatureLine* ContentRegistry::creatureLine(const char* lineId) const {
    return lines_.find(lineId);
}

const EggLineDef* ContentRegistry::eggLine(const char* id) const {
    return eggLines_.find(id);
}

const EggLineDef* ContentRegistry::eggLineForCreature(const char* creatureId) const {
    if (!creatureId) return nullptr;
    for (const EggLineDef* e : eggLines_.rows())
        if (std::strcmp(e->eggCreatureId, creatureId) == 0) return e;
    return nullptr;
}

const ItemDef* ContentRegistry::item(const char* id) const { return items_.find(id); }

const ModDef* ContentRegistry::modByWire(int wire) const {
    for (const ModDef* m : mods_.rows())
        if (m->wire == wire) return m;
    return nullptr;
}

const ModDef* ContentRegistry::mod(const char* id) const { return mods_.find(id); }

const MoveDef* ContentRegistry::move(const char* id) const { return moves_.find(id); }

const MoveDef* ContentRegistry::chainStep(const char* id) const {
    return chainSteps_.find(id);
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
