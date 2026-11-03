// registry.h — lookup-by-id over one or more merged content sources.
//
// The engine reads content only through here. Sources are added at boot
// (embedded first; an SD source later), so new content packs extend the game
// without touching call sites — the registration seam.
#pragma once

#include <algorithm>
#include <cstring>
#include <vector>

#include "core/content/content_source.h"

namespace mal {

class ContentRegistry {
public:
    // Merge a content source. Earlier sources win on id collision. The merge runs here,
    // once per source, so every lookup and every all*() list reads the merged tables.
    void addSource(const ContentSource& src);
    void setAssets(const AssetSource& assets) { assets_ = &assets; }

    const CreatureDef* creature(const char* id) const;
    // The FAMILY carrying `lineId` (CreatureDef::line), or nullptr. This is how the
    // line-level facts — the combat passives it carries, the achievement for arriving
    // on it — are read, so neither the turn engine nor the evolution seam has to know a
    // line by name.
    const CreatureLine* creatureLine(const char* lineId) const;
    const EggLineDef* eggLine(const char* id) const;
    // Reverse lookup: the line whose eggCreatureId matches a Boot-Sector creature id
    // (null if none). Used to re-derive an incubating egg's line from the persisted
    // active-pet id, since the line itself isn't independently saved.
    const EggLineDef* eggLineForCreature(const char* creatureId) const;
    const ItemDef* item(const char* id) const;
    const ModDef* mod(const char* id) const;
    const MoveDef* move(const char* id) const;
    // A chained move's FOLLOW-UP step by id, or nullptr. Kept apart from move() on
    // purpose: a step is not a move a pet can hold, so nothing that enumerates the
    // roster (allMoves, the equip picker, the drop roll) should ever see one. The only
    // caller is the Combatant builder, resolving MoveDef::chainNextId once at build time.
    const MoveDef* chainStep(const char* id) const;

    // Evolution routing. daemonPool: the Script->Daemon weighted pool for
    // `fromId` on the given branch (null if none — the caller then reads the
    // creature's own evolvesTo* fields). Every other hop is on the creature row.
    const DaemonPoolDef* daemonPool(const char* fromId, bool badBranch) const;

    // The all*() lists return a copy in merge order (source order, first id wins). A
    // copy keeps `for (x : ContentRegistry::embedded().allItems())` safe, where a
    // reference would outlive the temporary registry it points into.

    // Every known item def, across all sources (the inventory list builds off
    // this — content stays data-driven, no hardcoded item table in the UI).
    std::vector<const ItemDef*> allItems() const { return items_.rows(); }
    // Every known mod def (the MODS picker builds off this, same rationale).
    std::vector<const ModDef*> allMods() const { return mods_.rows(); }
    // The mod carrying save wire number `wire` (ModDef::wire), or nullptr — the lookup
    // the save's owned-mod pool needs, since a blob names a mod by number and not by id.
    // A number belonging to a RETIRED row resolves to nullptr, which is exactly what
    // should happen: its copies are dropped on load rather than resurrected.
    const ModDef* modByWire(int wire) const;
    // Every known move def (the MOVES picker builds off this, same rationale).
    std::vector<const MoveDef*> allMoves() const { return moves_.rows(); }
    // Every known FAMILY, across all sources, in roster order. What a caller asking
    // about the lines themselves rather than about one line by id needs — the habitat's
    // ambient colour drift walks it to find the families a pet is not (idleCamoSprite).
    std::vector<const CreatureLine*> allCreatureLines() const { return lines_.rows(); }
    // Every known creature def, across all sources. Roster-wide invariants build
    // off this (e.g. the wild-name / roster-name disjointness gate).
    std::vector<const CreatureDef*> allCreatures() const { return creatures_.rows(); }
    // Every known egg line, across all sources (the hatch line-select builds off
    // this — a new line is a data row, no UI change).
    std::vector<const EggLineDef*> allEggLines() const { return eggLines_.rows(); }

    // Resolve a creature's sprite via the AssetSource (art may ship with content).
    const SpriteData* creatureSprite(const CreatureDef& c) const;
    // A null NAME is a real answer and not a caller's mistake: a row may legitimately
    // have no sheet — an area guardian is drawn as a flock instead (CombatEnemy::isSwarm,
    // core/render/swarm.h) — and every draw path already treats a null sheet as an empty
    // standard seat. The guard is here rather than at those callers because the lookup
    // below is a strcmp against the asset table, which a null would walk straight into.
    const SpriteData* sprite(const char* name) const {
        return (assets_ && name) ? assets_->sprite(name) : nullptr;
    }

    // Build the registry (embedded content + assets).
    static ContentRegistry embedded();

private:
    // One merged content table: rows in merge order, plus the same rows sorted by id
    // for binary-search lookup. Every row type here carries a `const char* id`.
    template <typename T>
    class IdTable {
    public:
        // Add `n` rows, skipping any whose id is already present (earlier wins).
        void merge(const T* arr, int n) {
            for (int i = 0; i < n; ++i) {
                const T* row = &arr[i];
                auto at = lowerBound(row->id);
                if (at != byId_.end() && std::strcmp((*at)->id, row->id) == 0) continue;
                byId_.insert(at, row);
                rows_.push_back(row);
            }
        }
        const T* find(const char* id) const {
            if (!id) return nullptr;
            auto at = lowerBound(id);
            return (at != byId_.end() && std::strcmp((*at)->id, id) == 0) ? *at : nullptr;
        }
        const std::vector<const T*>& rows() const { return rows_; }

    private:
        typename std::vector<const T*>::const_iterator lowerBound(const char* id) const {
            return std::lower_bound(byId_.begin(), byId_.end(), id,
                                    [](const T* r, const char* k) {
                                        return std::strcmp(r->id, k) < 0;
                                    });
        }
        std::vector<const T*> rows_;
        std::vector<const T*> byId_;
    };

    std::vector<const ContentSource*> sources_;   // daemonPool still scans these raw
    const AssetSource* assets_ = nullptr;
    IdTable<CreatureLine> lines_;
    IdTable<CreatureDef> creatures_;
    IdTable<EggLineDef> eggLines_;
    IdTable<ItemDef> items_;
    IdTable<ModDef> mods_;
    IdTable<MoveDef> moves_;
    IdTable<MoveDef> chainSteps_;
};

} // namespace mal
