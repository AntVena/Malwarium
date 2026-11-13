#include "core/app/game.h"

#include <cstdio>
#include <cstring>

#include "core/content/content_themes.h"

namespace mal {

const ItemDef* Game::rollLootEntry(const LootEntry* pool, int poolSize) {
    // The one weighted walk over a loot pool, shared by every container's payout and
    // by the walk's own loot-cache event. Each row draws at its LootEntry::weight if it
    // names one, else at the item's own itemDropWeight() (which falls back to rarity),
    // so a pool stays a bare id list until an entry genuinely needs to differ. Advances
    // the shared LCG, so a draw stays deterministic under a fixed seed.
    if (!pool || poolSize <= 0) return nullptr;
    auto weightOf = [this](const LootEntry& e) {
        // A PALETTE CHIP is a permanent unlock rather than a stack, so a second copy is
        // worth nothing at all: once its set is in, the chip leaves the pool instead of
        // costing a draw that could have been a prize (content_themes.h).
        if (const ThemeDef* t = themeForChip(e.id))
            if (t->earnedById && hasCollectedItem(t->earnedById)) return 0;
        if (e.weight > 0) return e.weight;
        const ItemDef* d = registry_.item(e.id);
        return d ? itemDropWeight(*d) : 0;
    };
    int total = 0;
    for (int i = 0; i < poolSize; ++i) total += weightOf(pool[i]);
    if (total <= 0) return nullptr;
    rng_ = rng_ * 1664525u + 1013904223u;
    int roll = static_cast<int>((rng_ >> 16) % static_cast<unsigned>(total));
    const LootEntry* hit = &pool[poolSize - 1];
    for (int i = 0; i < poolSize; ++i) {
        const int w = weightOf(pool[i]);
        if (roll < w) { hit = &pool[i]; break; }
        roll -= w;
    }
    return registry_.item(hit->id);
}

const ItemDef* Game::drawCacheItem(const LootEntry* pool, int poolSize) {
    const ItemDef* d = rollLootEntry(pool, poolSize);
    if (!d) return nullptr;
    inventory_.add(d->id, 1);
    char buf[32];
    std::snprintf(buf, sizeof(buf), "FOUND %s", d->displayName);
    log_.push(LogEventType::ItemGained, buf);
    return d;
}

void Game::grantCacheReward(const ItemDef& d, int& bitsOut, const ItemDef** itemsOut,
                            int& itemCountOut, int maxItems) {
    // Everything a cache pays is on its own row (ItemDef::cache):
    // purse, draw count, pool, and whether it also rolls a mod. One uniform payout, so
    // adding a container is a row rather than a branch here.
    const CacheDef& c = d.cache;
    bitsOut = applyCacheBitsBonus(c.bits);   // Enhanced DataMining
    op_.bits += bitsOut;
    itemCountOut = 0;
    for (int i = 0; i < c.draws; ++i) {
        if (c.drawChancePct < 100) {
            rng_ = rng_ * 1664525u + 1013904223u;
            if (static_cast<int>((rng_ >> 16) % 100) >= c.drawChancePct) continue;
        }
        const ItemDef* got = drawCacheItem(c.pool, c.poolSize);
        if (got && itemCountOut < maxItems) itemsOut[itemCountOut++] = got;
    }
    // A rich cache is also a second, non-boss MOD source: rolled globally rarity-weighted
    // (even a tier-4 mod can surface; its rolled equip-level gate holds it until the pet
    // is deep enough). Granted + logged here: the yield reveal shows the items/Bits, the
    // mod lands in MODS.
    if (c.modChancePct > 0) {
        rng_ = rng_ * 1664525u + 1013904223u;
        if (static_cast<int>((rng_ >> 16) % 100) < c.modChancePct)
            if (const char* id = rollAnyModId())
                grantMod(id);
    }
}

void Game::openSealedCache(const ItemDef& d) {
    // opening a container consumes it and draws its reward.
    // the yield (Bits + item names) is stashed and surfaced on Nav::CacheYield.
    inventory_.remove(d.id, 1);
    char opened[36];
    std::snprintf(opened, sizeof(opened), "OPENED %s", d.displayName);
    log_.push(LogEventType::ItemUsed, opened);

    CacheReveal r;
    r.cache = &d;            // ItemDef lives in the static registry: stable pointer
    grantCacheReward(d, r.bits, r.items, r.itemCount, 2);
    vault_.showReveal(r);

    markSaveDirty();
    nav_ = Nav::CacheYield;   // show what came out
}

void Game::openAllCachesOfRarity(const ItemDef& focused) {
    // Bulk-open every copy of the FOCUSED cache in one action. Grouping is by the cache
    // itself, not by rarity: two containers may share a rarity while paying out of
    // completely different pools (an earned Commendation Cache and a found Epic one), and
    // "open all of these" should never quietly spend a prize alongside ordinary loot.
    struct Target { const ItemDef* def; int qty; };
    std::vector<Target> targets;
    for (const auto& s : inventory_.stacks()) {
        if (s.qty <= 0) continue;
        const ItemDef* d = registry_.item(s.id);
        if (!d || d->use != ItemDef::Use::OpenContainer) continue;
        if (std::strcmp(d->id, focused.id) == 0) targets.push_back({d, s.qty});
    }

    BulkReveal r;
    r.cache = &focused;

    for (const Target& t : targets) {
        for (int i = 0; i < t.qty; ++i) {
            inventory_.remove(t.def->id, 1);
            ++r.cachesOpened;
            int oneBits = 0;
            const ItemDef* oneItems[2] = {nullptr, nullptr};
            int oneCount = 0;
            grantCacheReward(*t.def, oneBits, oneItems, oneCount, 2);
            r.bits += oneBits;
            for (int k = 0; k < oneCount; ++k) {
                if (!oneItems[k]) continue;
                bool merged = false;
                for (BulkReveal::Tally& tally : r.tally)
                    if (tally.def == oneItems[k]) { ++tally.count; merged = true; break; }
                if (!merged) r.tally.push_back({oneItems[k], 1});
            }
        }
    }

    char opened[40];
    std::snprintf(opened, sizeof(opened), "BULK OPENED %d %s", r.cachesOpened,
                  rarityName(focused.rarity));
    log_.push(LogEventType::ItemUsed, opened);
    vault_.showBulkReveal(std::move(r));
    markSaveDirty();
    nav_ = Nav::BulkYield;
}

}  // namespace mal
