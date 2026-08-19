// content_source.h — the seam behind the registry.
//
// EmbeddedContentSource ships content compiled in. A future
// SdContentSource (JSON on the SD card) implements the same
// interface and is merged in at boot — no engine changes. AssetSource does the
// same for sprite art (embedded now, card-backed later).
#pragma once

#include "core/content/defs.h"
#include "core/render/sprite.h"

namespace mal {

struct ContentSource {
    virtual ~ContentSource() = default;
    // Creatures arrive grouped into families (CreatureLine), not as one flat
    // roster: a family's rows are authored together and a source that ships one
    // ships all of it. The registry flattens on read, so nothing downstream cares.
    virtual const CreatureLine* creatureLines(int& count) const = 0;
    virtual const EggLineDef* eggLines(int& count) const = 0;
    virtual const ItemDef* items(int& count) const = 0;
    virtual const ModDef* mods(int& count) const = 0;
    virtual const MoveDef* moves(int& count) const = 0;
    // Chained-move follow-up steps. DEFAULTED rather than pure: a pack that ships no
    // chains is complete without one, and every reader already treats "no step by that
    // id" as an ordinary move.
    virtual const MoveDef* chainSteps(int& count) const {
        count = 0;
        return nullptr;
    }
    // Script->Daemon weighted pools. A source with no multi-Daemon content
    // returns count 0 (nullptr) — the registry then falls through to the next
    // source / the per-creature evolvesTo* fields.
    virtual const DaemonPoolDef* daemonPools(int& count) const = 0;
};

struct AssetSource {
    virtual ~AssetSource() = default;
    // Resolve a sprite by asset name; nullptr if unknown.
    virtual const SpriteData* sprite(const char* name) const = 0;
};

// Content + art, compiled in.
const ContentSource& embeddedContent();
const AssetSource& embeddedAssets();

} // namespace mal
