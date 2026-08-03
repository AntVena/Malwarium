// registry.h — lookup-by-id over one or more merged content sources.
//
// The engine reads content only through here. Sources are added at boot
// (embedded first; an SD source later), so new content packs extend the game
// without touching call sites — the registration seam.
#pragma once

#include <vector>

#include "core/content/content_source.h"

namespace mal {

class ContentRegistry {
public:
    // Merge a content source. Earlier sources win on id collision.
    void addSource(const ContentSource& src) { sources_.push_back(&src); }
    void setAssets(const AssetSource& assets) { assets_ = &assets; }

    const CreatureDef* creature(const char* id) const;
    const EggLineDef* eggLine(const char* id) const;
    // Reverse lookup: the line whose eggCreatureId matches a Boot-Sector creature id
    // (null if none). Used to re-derive an incubating egg's line from the persisted
    // active-pet id, since the line itself isn't independently saved.
    const EggLineDef* eggLineForCreature(const char* creatureId) const;
    const ItemDef* item(const char* id) const;
    const ModDef* mod(const char* id) const;
    const MoveDef* move(const char* id) const;

    // Evolution routing. signalRoute: the Boot->Process / Process->
    // Script hop for `fromId` (null if none). daemonPool: the Script->Daemon
    // weighted pool for `fromId` on the given branch (null if none).
    const SignalRouteDef* signalRoute(const char* fromId) const;
    const DaemonPoolDef* daemonPool(const char* fromId, bool badBranch) const;

    // Every known item def, across all sources (the inventory list builds off
    // this — content stays data-driven, no hardcoded item table in the UI).
    std::vector<const ItemDef*> allItems() const;
    // Every known mod def (the MODS picker builds off this, same rationale).
    std::vector<const ModDef*> allMods() const;
    // Every known move def (the TRAIN move picker builds off this, same rationale).
    std::vector<const MoveDef*> allMoves() const;
    // Every known creature def, across all sources. Roster-wide invariants build
    // off this (e.g. the wild-name / roster-name disjointness gate).
    std::vector<const CreatureDef*> allCreatures() const;
    // Every known egg line, across all sources (the hatch line-select builds off
    // this — a new line is a data row, no UI change).
    std::vector<const EggLineDef*> allEggLines() const;

    // Resolve a creature's sprite via the AssetSource (art may ship with content).
    const SpriteData* creatureSprite(const CreatureDef& c) const;
    const SpriteData* sprite(const char* name) const {
        return assets_ ? assets_->sprite(name) : nullptr;
    }

    // Build the registry (embedded content + assets).
    static ContentRegistry embedded();

private:
    std::vector<const ContentSource*> sources_;
    const AssetSource* assets_ = nullptr;
};

} // namespace mal
