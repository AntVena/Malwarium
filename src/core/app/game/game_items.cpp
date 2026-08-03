#include "core/app/game.h"
#include "core/app/game_internal.h"

#include <cstdio>
#include <cstring>

#include "tunables.h"
#include "core/content/content_tables.h"
#include "core/render/canvas.h"
#include "core/render/font5x7.h"
#include "core/render/palette.h"
#include "core/render/sprite.h"
#include "core/ui/carousel.h"
#include "core/ui/combat_screen.h"
#include "core/ui/items_screen.h"
#include "core/ui/maint_screen.h"
#include "core/ui/modals.h"
#include "core/ui/stat_screen.h"
#include "core/ui/train_screen.h"
#include "generated/assets.h"

namespace mal {

// Combat-stat labels now live in game_internal.h::levelStatName so the
// Rollback picker, the post-encounter level-up readout, and the serial trace all
// name a stat the same way.

// --- ITEMS -----------------------------------------------------------------

void Game::onItemsPicker(const ButtonEvent& ev) {
    const auto tiles = buildItemPickerRows(registry_, inventory_);
    const int n = static_cast<int>(tiles.size());
    if (n <= 0) { if (ev.button == Button::C) nav_ = Nav::Cursor; return; }
    if (itemPickRow_ < 0 || itemPickRow_ >= n) itemPickRow_ = 0;

    if (ev.button == Button::A) {
        itemPickRow_ = (itemPickRow_ + 1) % n;
    } else if (ev.button == Button::B) {
        // Commit the tile and drill in. An EMPTY category still opens — the list
        // says "- NO MATCHING ITEMS -" and C walks back, which reads better than a
        // dead button on a row the screen just drew.
        itemFilter_ = tiles[itemPickRow_].filter;
        itemsScreen_ = ItemsScreen::List;
        auto rows = buildInventoryRows(registry_, inventory_, false, itemFilter_);
        listRow_ = firstSelectableRow(rows);
        if (listRow_ < 0) listRow_ = 0;
    } else if (ev.button == Button::C) {
        nav_ = Nav::Cursor;
    }
}

void Game::onItemsList(const ButtonEvent& ev) {
    auto rows = buildInventoryRows(registry_, inventory_, lockoutItemsContext_, itemFilter_);
    if (ev.button == Button::A) {
        // d: once the ITEMS type-tabs upgrade is owned, A becomes a tap/hold
        // gesture — arm here and resolve on the button-RELEASE edge (a short tap ->
        // itemFilterReleaseA steps the cursor, same as always) or on the hold
        // crossing kItemFilterHoldMs (tick() cycles the filter instead). Unowned
        // players see zero change: A still fires the step immediately on press.
        if (itemTabsUnlocked()) {
            aHeld_ = true;
            aDownMs_ = nowMs_;
        } else if (!rows.empty()) {
            listRow_ = stepSelectableRow(rows, listRow_, +1);
        }
    } else if (ev.button == Button::B) {
        if (listRow_ >= 0 && listRow_ < static_cast<int>(rows.size()) &&
            !rows[listRow_].header) {
            detailItem_ = rows[listRow_].def;
            nav_ = Nav::Detail;
        }
    } else if (ev.button == Button::C) {
        // Back one layer: the Lockout modal, else the type-picker that opened this
        // list, else out to the carousel.
        if (lockoutItemsContext_) nav_ = Nav::ModalLockout;
        else if (itemPickerUnlocked()) itemsScreen_ = ItemsScreen::Picker;
        else nav_ = Nav::Cursor;
    }
}

void Game::itemFilterReleaseA() {
    if (!aHeld_) return;   // tick() already fired the hold-cycle and cleared this
    aHeld_ = false;
    if (!(nav_ == Nav::Submenu && face_ == Face::Pet && enteredId() == SubmenuId::Items &&
          itemsScreen_ == ItemsScreen::List && itemTabsUnlocked()))
        return;   // context changed mid-hold (shouldn't happen in practice) — no-op
    auto rows = buildInventoryRows(registry_, inventory_, lockoutItemsContext_, itemFilter_);
    if (!rows.empty()) listRow_ = stepSelectableRow(rows, listRow_, +1);
    dirty_ = true;
    lastInputMs_ = nowMs_;
}

bool Game::itemUsable(const ItemDef& d, const char*& gateMsg) const {
    if (inventory_.count(d.id) <= 0) { gateMsg = "- NONE LEFT -"; return false; }
    // Boot-Sector egg: only quest items are usable (an egg can't be fed or buffed).
    // Quest items still fall through to their own context gate below.
    if (inEggPhase() && d.type != ItemDef::Type::Quest) {
        gateMsg = "USABLE AFTER HATCH"; return false;
    }
    // Warp keys are spent only ON the walk (the B warp picker), never from
    // the ITEMS use path — otherwise a stray Use would burn a key for nothing.
    if (d.walkWarp != ItemDef::WalkWarp::None) {
        gateMsg = "USE ON THE WALK"; return false;
    }
    // Rollback is inert with nothing to shed — a level-0 pet has no earned
    // points, so the picker would be empty. Gate it clearly instead of opening blank.
    if (d.use == ItemDef::Use::Rollback && combatLevel_ <= 0) {
        gateMsg = "NO LEVELS TO ROLL"; return false;
    }
    // Decryptogram only readies an egg's decrypt — nothing to do once hatched.
    if (d.use == ItemDef::Use::DecryptEgg && !inEggPhase()) {
        gateMsg = "USABLE ON EGG ONLY"; return false;
    }
    // the Defrag Tool is spent only by a TOOL DEFRAG in MAINT (a guaranteed
    // clean), never from the ITEMS use path — a stray Use would burn it for nothing.
    if (std::strcmp(d.id, kDefragToolId) == 0) {
        gateMsg = "USE IN MAINT DEFRAG"; return false;
    }
    // Sealed caches are now decrypted from the Hacker VAULT, never
    // pet-side ITEMS. Gate here so the detail action reads DECRYPT IN VAULT and B is inert.
    if (d.use == ItemDef::Use::OpenContainer) {
        gateMsg = "DECRYPT IN VAULT"; return false;
    }
    // Nothing left for this one to do — say so and keep the item, rather than
    // spending it on a state it already holds.
    if (itemUseIsInert(d, gateMsg)) return false;
    if (lockoutItemsContext_) {
        if (itemResolvesLockout(d)) return true;
        gateMsg = "- WONT RESOLVE LOCKOUT -";
        return false;
    }
    switch (d.context) {
        case ItemDef::Context::Anytime: return true;
        case ItemDef::Context::LockoutOnly:
            gateMsg = "USABLE IN LOCKOUT ONLY"; return false;
        case ItemDef::Context::PreEncounter:
            gateMsg = "USABLE BEFORE ENCOUNTER"; return false;
    }
    return false;
}

bool Game::itemUseIsInert(const ItemDef& d, const char*& why) const {
    // Only the plain consume path applies effects[]; every other Use hands off to
    // its own flow and is gated by itemUsable on its own terms.
    if (d.use != ItemDef::Use::Consume) return false;

    const char* reason = nullptr;
    int armingEffects = 0;
    for (const ItemEffect& e : d.effects) {
        switch (e.kind) {
            case ItemEffect::Kind::None:
                continue;
            // A vitals lever always lands. Feeding an already-full pet wastes
            // some of the fill, but that's the player's call to make — only a
            // use that can achieve LITERALLY nothing is worth refusing.
            case ItemEffect::Kind::Hunger:
            case ItemEffect::Kind::HungerStacking:
            case ItemEffect::Kind::Happy:
            case ItemEffect::Kind::HappyToward50:
            case ItemEffect::Kind::Frag:
                return false;
            case ItemEffect::Kind::RemoveCareMistakeOnce:
                ++armingEffects;
                if (!yubiConsumed_) return false;
                if (!reason) reason = "ALREADY SPENT THIS LIFE";
                break;
            case ItemEffect::Kind::ClearMistakeShieldOnce:
                ++armingEffects;
                if (!shieldItemConsumed_) return false;
                if (!reason) reason = "ALREADY SPENT THIS LIFE";
                break;
            case ItemEffect::Kind::ForceTrojanDivert:
                ++armingEffects;
                if (!forceTrojanDivert_) return false;
                if (!reason) reason = "DIVERT ALREADY ARMED";
                break;
            case ItemEffect::Kind::ArmCombatShieldBuff:
                // Re-arming only ever REPLACES the deadline, so a second one over
                // a live shield trades an item for nothing. The STAT BUFFS page
                // carries the time remaining.
                ++armingEffects;
                if (!backupShieldArmed()) return false;
                if (!reason) reason = "SHIELD ALREADY ARMED";
                break;
            case ItemEffect::Kind::ArmDeepWebDepthMultiplier:
                // These overwrite rather than stack, so a weaker one after a
                // stronger one is a downgrade paid for with an item.
                ++armingEffects;
                if (e.magnitude > deepWebDepthMultiplier_) return false;
                if (!reason) reason = "STRONGER ONE ARMED";
                break;
            case ItemEffect::Kind::SetDeepWebStartDepth:
                ++armingEffects;
                if (e.magnitude > pendingDeepWebStartDepth_) return false;
                if (!reason) reason = "DEEPER START ARMED";
                break;
            case ItemEffect::Kind::SetDeepWebStartDepthToBest:
                ++armingEffects;
                if (bestDeepWebDepth_ > pendingDeepWebStartDepth_) return false;
                if (!reason) reason = "DEEPER START ARMED";
                break;
        }
    }
    // A row with no arming effects at all does its work through a hand-off field
    // (combatHeal, preEncounterXp, a warp key) — never inert on this axis.
    if (armingEffects == 0 || !reason) return false;
    why = reason;
    return true;
}

void Game::onItemsDetail(const ButtonEvent& ev) {
    if (ev.button == Button::B) useItem();
    else if (ev.button == Button::C) nav_ = Nav::Submenu;
}

void Game::useItem() {
    if (!detailItem_) return;
    const char* gate = "";
    if (!itemUsable(*detailItem_, gate)) return;   // inert when gated
    const ItemDef& d = *detailItem_;

    // Openable containers (sealed caches) are now decrypted from the Hacker VAULT
    // not here — itemUsable gates them ("DECRYPT IN VAULT"), so useItem never
    // reaches this point for one. openSealedCache lives on and is called from onHackerVault.

    // Rollback: Use opens the stat picker instead of feeding/buffing. The
    // item is only consumed when a shed is confirmed (onRollbackPicker B) — cancelling
    // (C) leaves it in the bag. detailItem_ stays set so the shed path can consume it.
    if (d.use == ItemDef::Use::Rollback) { openRollbackPicker(); return; }

    // Decryptogram: Use readies the egg's decrypt NOW instead of feeding.
    if (d.use == ItemDef::Use::DecryptEgg) { useDecryptogram(d); return; }

    if (d.type == ItemDef::Type::Food) {
        startFeeding(d, lockoutItemsContext_);
        return;
    }
    // Active quest item used inside a Lockout — resolves it immediately.
    if (lockoutItemsContext_ && itemResolvesLockout(d)) {
        inventory_.remove(d.id, 1);
        char buf[28];
        std::snprintf(buf, sizeof(buf), "USED %s", d.displayName);
        log_.push(LogEventType::ItemUsed, buf);
        resolveLockout();
        return;
    }
    // Buff (normal context): applied immediately, count decrements. Every pet lever —
    // Happiness, the Yubi-Cookie mistake removal, the Restore Point shield — is one of
    // the item's effects[], applied by the one applier; no per-id branching here.
    inventory_.remove(d.id, 1);
    applyItemEffects(d);
    noteCareSignal(DominantSignal::Play);   // a happiness buff reads as play
    char buf[28];
    std::snprintf(buf, sizeof(buf), "USED %s", d.displayName);
    log_.push(LogEventType::ItemUsed, buf);
    markSaveDirty();
    if (inventory_.count(d.id) <= 0) nav_ = Nav::Submenu;  // item left the list
}

void Game::applyItemEffects(const ItemDef& d) {
    // Apply the item's on-Use pet levers (its ItemEffect list — the single source of
    // truth for what an item does to the pet). Hunger/Happy accumulate into ONE
    // PetModel::feed so a food's fill lands as a single beat; the rest apply in place.
    // Every magnitude comes off the item row (defs.h) — none live in tunables.
    int hunger = 0, happy = 0;
    for (const ItemEffect& e : d.effects) {
        switch (e.kind) {
            case ItemEffect::Kind::None:
                break;
            case ItemEffect::Kind::Hunger:
                hunger += e.magnitude;
                break;
            case ItemEffect::Kind::HungerStacking:
                // Polltatoes: the run counts THIS bite too, so eating one alone still
                // fills magnitude. The counter is reset by the next passive Hunger
                // decay (Game::tickHungerAndAwardXp) — a fresh boot starts a fresh
                // run, which is the forgiving reading and keeps a transient combo
                // counter out of the save blob.
                ++stackingFoodRun_;
                hunger += e.magnitude * stackingFoodRun_;
                break;
            case ItemEffect::Kind::Happy:
                happy += e.magnitude;
                break;
            case ItemEffect::Kind::Frag:
                model_.setFragmentation(model_.fragmentation() + e.magnitude);
                break;
            case ItemEffect::Kind::HappyToward50: {
                // The "empty feeling": pull Happiness TOWARD 50% from either side
                // by magnitude, no overshoot — numbs a too-happy pet as much as it lifts
                // a miserable one, unlike the flat Happy add.
                const int h = model_.happiness();
                if (h > 50) {
                    const int next = h - e.magnitude;
                    model_.setHappiness(next < 50 ? 50 : next);
                } else if (h < 50) {
                    const int next = h + e.magnitude;
                    model_.setHappiness(next > 50 ? 50 : next);
                }
                break;
            }
            case ItemEffect::Kind::RemoveCareMistakeOnce:
                // Yubi-Cookie ("Max 1 per lifecycle"): remove mistakes ONCE, then the
                // per-pet flag latches it — already-spent → no-op.
                if (!yubiConsumed_) {
                    model_.addCareMistake(-e.magnitude);
                    yubiConsumed_ = true;
                }
                break;
            case ItemEffect::Kind::ClearMistakeShieldOnce:
                // Restore Point (save v21): arm the next-mistake shield ONCE per pet;
                // already-spent → no-op.
                if (!shieldItemConsumed_) {
                    mistakeShieldActive_ = true;
                    shieldItemConsumed_ = true;
                }
                break;
            case ItemEffect::Kind::ForceTrojanDivert:
                // Ambig-USB (save v28): arm a guaranteed Trojan divert at the pet's
                // next Process->Script evolution (Game::fireEvolution), replacing the
                // kTrojanDivertPct roll. Re-armable (unlike the once-per-lifetime
                // shields above) — buying another does no harm.
                forceTrojanDivert_ = true;
                break;
            case ItemEffect::Kind::ArmCombatShieldBuff:
                // Backup Drive (save v30): arm the timed combat shield, magnitude
                // MINUTES from now (Game::buildPlayerCombatant reads the deadline).
                // Re-armable — using another one just refreshes the window.
                backupShieldUntilMs_ = lifetimeUptimeMs() + static_cast<uint32_t>(e.magnitude) * 60u * 1000u;
                break;
            case ItemEffect::Kind::ArmDeepWebDepthMultiplier:
                // Deep-Learning Module/Core: overwrites, never stacks — buying a second one
                // just replaces the multiplier. Cleared back to 1 wherever
                // exploreStreak_ resets to 0 (game_explore.cpp/game_combat.cpp/
                // game_lifecycle.cpp).
                deepWebDepthMultiplier_ = e.magnitude;
                break;
            case ItemEffect::Kind::SetDeepWebStartDepth:
                // Backdoor/Rootkit/Kernel Bell: arm the NEXT startDeepWebDive() to
                // begin at this fixed depth instead of 0; consumed there.
                pendingDeepWebStartDepth_ = e.magnitude;
                break;
            case ItemEffect::Kind::SetDeepWebStartDepthToBest:
                // Zero-Day Bell: arm the next dive to start at THIS PET's own
                // bestDeepWebDepth_, resolved at dive-start (not here) so improving
                // the record between now and then is never stale.
                pendingDeepWebStartDepth_ = kDeepWebStartDepthUseBest;
                break;
        }
    }
    if (hunger != 0 || happy != 0) model_.feed(hunger, happy);
}

const ItemDef* Game::rollLootEntry(const LootEntry* pool, int poolSize) {
    // The one weighted walk over a loot pool, shared by every container's payout and
    // by the walk's own loot-cache event. Each row draws at its LootEntry::weight if it
    // names one, else at the item's own itemDropWeight() (which falls back to rarity) —
    // so a pool stays a bare id list until an entry genuinely needs to differ. Advances
    // the shared LCG, so a draw stays deterministic under a fixed seed.
    if (!pool || poolSize <= 0) return nullptr;
    auto weightOf = [this](const LootEntry& e) {
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
    // Everything a cache pays is on its own row (ItemDef::cache, content_items.cpp) —
    // purse, draw count, pool, and whether it also rolls a mod. One uniform payout, so
    // adding a container is a row rather than a branch here.
    const CacheDef& c = d.cache;
    bitsOut = applyCacheBitsBonus(c.bits);   // Enhanced DataMining
    bits_ += bitsOut;
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
    // is deep enough). Granted + logged here — the yield reveal shows the items/Bits, the
    // mod lands in MODS.
    if (c.modChancePct > 0) {
        rng_ = rng_ * 1664525u + 1013904223u;
        if (static_cast<int>((rng_ >> 16) % 100) < c.modChancePct)
            if (const char* id = rollAnyModId())
                grantRolledMod(id);
    }
}

void Game::openSealedCache(const ItemDef& d) {
    // opening a container consumes it and draws its reward.
    // the yield (Bits + item names) is stashed and surfaced on Nav::CacheYield.
    inventory_.remove(d.id, 1);
    char opened[36];
    std::snprintf(opened, sizeof(opened), "OPENED %s", d.displayName);
    log_.push(LogEventType::ItemUsed, opened);

    cacheYieldCache_ = &d;   // ItemDef lives in the static registry — stable pointer
    cacheYieldItemCount_ = 0;
    cacheYieldBits_ = 0;
    grantCacheReward(d, cacheYieldBits_, cacheYieldItems_, cacheYieldItemCount_, 2);

    markSaveDirty();
    nav_ = Nav::CacheYield;   // show what came out
}

void Game::onCacheYield(const ButtonEvent& ev) {
    // Any confirm/back dismisses the reveal to the ITEMS list (the container is
    // already gone; the yield is in the bag + logged).
    if (ev.button == Button::B || ev.button == Button::C) nav_ = Nav::Submenu;
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

    bulkYieldCache_ = &focused;
    bulkYieldCachesOpened_ = 0;
    bulkYieldBits_ = 0;
    bulkYieldTally_.clear();
    bulkYieldRow_ = 0;

    for (const Target& t : targets) {
        for (int i = 0; i < t.qty; ++i) {
            inventory_.remove(t.def->id, 1);
            ++bulkYieldCachesOpened_;
            int oneBits = 0;
            const ItemDef* oneItems[2] = {nullptr, nullptr};
            int oneCount = 0;
            grantCacheReward(*t.def, oneBits, oneItems, oneCount, 2);
            bulkYieldBits_ += oneBits;
            for (int k = 0; k < oneCount; ++k) {
                if (!oneItems[k]) continue;
                bool merged = false;
                for (auto& tally : bulkYieldTally_)
                    if (tally.def == oneItems[k]) { ++tally.count; merged = true; break; }
                if (!merged) bulkYieldTally_.push_back({oneItems[k], 1});
            }
        }
    }

    char opened[40];
    std::snprintf(opened, sizeof(opened), "BULK OPENED %d %s", bulkYieldCachesOpened_,
                  rarityName(focused.rarity));
    log_.push(LogEventType::ItemUsed, opened);
    markSaveDirty();
    nav_ = Nav::BulkYield;
}

void Game::onBulkYield(const ButtonEvent& ev) {
    const int n = static_cast<int>(bulkYieldTally_.size());
    if (ev.button == Button::A) {
        if (n > 0) bulkYieldRow_ = (bulkYieldRow_ + 1) % n;
    } else if (ev.button == Button::B || ev.button == Button::C) {
        nav_ = Nav::Submenu;   // back to the VAULT list the bulk-open came from
    }
}

void Game::useDecryptogram(const ItemDef& d) {
    // consume the Decryptogram and ready the egg's decrypt immediately. Default
    // effect = jump straight to the decrypt-ready point (second incubation half), then
    // open the hatch minigame so the player decrypts NOW instead of waiting it out.
    // itemUsable already guaranteed we're in the egg phase.
    inventory_.remove(d.id, 1);
    char buf[28];
    std::snprintf(buf, sizeof(buf), "USED %s", d.displayName);
    log_.push(LogEventType::ItemUsed, buf);
    if (bootHatchRemainMs_ > kBootHatchMs / 2) bootHatchRemainMs_ = kBootHatchMs / 2;
    markSaveDirty();
    // Arms the modal (hatchMinigameReady() now holds) for a Decrypt-line egg; on a line
    // that hatches another way this no-ops and the clock cut above IS the whole effect.
    openDecryptMinigame();
}

int Game::nextEligibleStat(int cur) const {
    // The next stat (wrapping) with ≥1 earned point — A cycles only eligible rows
    // Returns `cur` if it's the only eligible one, or -1 if none are.
    for (int step = 1; step <= kLevelStatCount; ++step) {
        const int i = (cur + step) % kLevelStatCount;
        if (rollbackEligible(i)) return i;
    }
    return rollbackEligible(cur) ? cur : -1;
}

void Game::openRollbackPicker() {
    // Park on the first eligible stat (itemUsable already guaranteed ≥1 point exists,
    // so nextEligibleStat can't return -1 here). detailItem_ (the Rollback item) stays
    // set so a confirmed shed consumes it.
    int first = -1;
    for (int i = 0; i < kLevelStatCount; ++i)
        if (rollbackEligible(i)) { first = i; break; }
    rollbackRow_ = first < 0 ? 0 : first;
    nav_ = Nav::RollbackPicker;
    dirty_ = true;
}

void Game::onRollbackPicker(const ButtonEvent& ev) {
    if (ev.button == Button::A) {
        const int nxt = nextEligibleStat(rollbackRow_);
        if (nxt >= 0) rollbackRow_ = nxt;
    } else if (ev.button == Button::B) {
        // Shed one point of the chosen stat: −1 that stat, −1 level (level == total
        // points), and re-zero the XP bucket so the pet RE-GRINDS that level to
        // re-roll it (the grind is the real cost). Consume the Rollback item + log.
        if (rollbackEligible(rollbackRow_) && detailItem_) {
            --statPoints_[rollbackRow_];
            --combatLevel_;
            combatXp_ = 0;
            inventory_.remove(detailItem_->id, 1);
            char buf[28];
            // Surface the shed explicitly — which stat and by how much (
            // "show the -1 <stat> on a Rollback"). The stat WORD + signed number are
            // the non-colour channel, so it reads in a grayscale log.
            std::snprintf(buf, sizeof(buf), "ROLLBACK %s -1",
                          levelStatName(rollbackRow_));
            log_.push(LogEventType::ItemUsed, buf);
            markSaveDirty();
        }
        nav_ = Nav::Submenu;   // back to the ITEMS list (the item may have run out)
    } else if (ev.button == Button::C) {
        nav_ = Nav::Detail;    // cancel — item untouched, back to its detail
    }
    dirty_ = true;
}

void Game::rollPantrySpoilage() {
    // One roll per held perishable STACK, converting a single unit — so a larder of
    // twenty rots no faster than a larder of one, and a player who hoards isn't punished
    // for it beyond the flavour. Collected first because converting mutates the vector
    // the roll is walking.
    struct Turned { const ItemDef* from; const char* into; };
    Turned turned[4] = {};
    int n = 0;
    for (const Inventory::Stack& s : inventory_.stacks()) {
        if (n == 4) break;
        const ItemDef* d = registry_.item(s.id);
        if (!d || !d->spoil.into || d->spoil.pct <= 0) continue;
        rng_ = rng_ * 1664525u + 1013904223u;
        if (static_cast<int>((rng_ >> 16) % 100) >= d->spoil.pct) continue;
        turned[n++] = {d, d->spoil.into};
    }
    for (int i = 0; i < n; ++i) {
        inventory_.remove(turned[i].from->id, 1);
        inventory_.add(turned[i].into, 1);
        // Say so: an item quietly becoming a worse item is the kind of change a player
        // would otherwise only notice as a miscount.
        char buf[28];
        std::snprintf(buf, sizeof(buf), "%s SPOILED", turned[i].from->displayName);
        log_.push(LogEventType::CareMistake, buf);
    }
    if (n > 0) markSaveDirty();
}

void Game::startFeeding(const ItemDef& d, bool fromLockout) {
    inventory_.remove(d.id, 1);
    // Snapshot first: the modal's gauges report the DELTA this bite achieved, and
    // a stat that was already capped has none to show.
    feedBefore_ = {model_.hunger(), model_.fragmentation(), model_.happiness()};
    applyItemEffects(d);                        // Hunger/Happy fill + any frag/happy-pull
    noteCareSignal(DominantSignal::Feeding);   // dominant-signal tally
    // Feeding from the crisis resolves it immediately — disarm the deadline so it
    // can't penalise the pet mid-eat (endFeeding finalises the return to idle).
    if (fromLockout) lockoutActive_ = false;
    char buf[28];
    std::snprintf(buf, sizeof(buf), "FED %s", d.displayName);
    log_.push(LogEventType::ItemUsed, buf);
    feedItem_ = &d;
    feedBeat_ = 0;
    feedFromLockout_ = fromLockout;
    rollPantrySpoilage();          // opening the bag is what disturbs the rest of it
    markSaveDirty();
    nav_ = Nav::ModalFeeding;
}

void Game::endFeeding() {
    if (feedFromLockout_) {
        resolveLockout();                 // -> idle
    } else if (detailItem_ && inventory_.count(detailItem_->id) > 0) {
        nav_ = Nav::Detail;               // back to the detail it came from
    } else {
        nav_ = Nav::Submenu;              // item ran out -> back to the list
    }
    feedItem_ = nullptr;
    feedFromLockout_ = false;
}


}  // namespace mal
