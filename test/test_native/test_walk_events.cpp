// test_walk_events.cpp — native gates for the walk's non-combat events — Wi-Fi, shops, caches and warp keys.
//
// One slice of the native suite; see test_gates.h for the shared fixtures and
// test_main.cpp for the roster that runs these. A save-vNN gate sits with the
// feature whose field it migrates, not in a migrations pile of its own.
#include "test_gates.h"

// Same search, but keeps re-rolling (resolving each non-matching outcome via
// B, or riding out+dismissing an awakened-guardian fight) until the specific
// sub-outcome `want` comes up, leaving the game AT Nav::Wifi with it unresolved
// — so a test can then press B itself and assert the resolution.
static void walkToWifiOutcome(Game& g, Game::WifiOutcome want) {
    for (int tries = 0; tries < 50 && g.nav() != Game::Nav::Wifi; ++tries) {
        walkToWifiEvent(g);
        if (g.nav() != Game::Nav::Wifi) return;   // search exhausted (shouldn't happen)
        if (g.wifiOutcome() == want) return;
        g.onButton(press(Button::B));             // wrong outcome — resolve, keep looking
        if (g.nav() == Game::Nav::Combat) {        // an awakened-guardian miss
            uint32_t t = 0;
            for (int i = 0; i < 400 && g.combat().outcome() == Combat::Outcome::Ongoing; ++i)
                g.tick(t += kHeartbeatMs);
            g.onButton(press(Button::B));          // dismiss -> back to the Walk
        }
    }
}

// The periodic roll can type a Wi-Fi network event (not just Quiet/Loot/Wild)
// a decision-free, self-contained screen rendering the sector header
// unchanged + the rolled sub-outcome's flavor line + a B-continue hint band.
void test_wifi_event_reached() {
    Game g{StartMode::Hatched};
    walkToWifiEvent(g);
    CHECK(g.nav() == Game::Nav::Wifi);
    Framebuffer fb(kActiveW, kActiveH);
    g.render(fb);
    CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));

    // Like Walk/Encounter/Combat, it's an activity screen beyond the 3-deep
    // menu budget — the global 5s auto-defocus must stay suspended while it's
    // up (analogy), not silently drop the player out mid-event.
    g.tick(kAutoDefocusMs + kHeartbeatMs);
    CHECK(g.nav() == Game::Nav::Wifi);
}

// A real network only credits the lifetime networks-seen counter once the EXPL
// Wi-Fi event actually resolves it (never at registerNetwork/scan time), and
// only for a GENUINELY new BSSID — never decreasing, never re-crediting a
// repeat, and never crediting anything when nothing was queued.
void test_wifi_networks_seen_counter() {
    Game g{StartMode::Hatched};
    CHECK(g.networksSeen() == 0);

    // Nothing queued yet: the event resolves to the empty-queue outcome, no credit.
    walkToWifiEvent(g);
    CHECK(g.nav() == Game::Nav::Wifi);
    CHECK(g.networksSeen() == 0);
    g.onButton(press(Button::B));
    if (g.nav() == Game::Nav::Combat) {
        uint32_t t = 0;
        for (int j = 0; j < 400 && g.combat().outcome() == Combat::Outcome::Ongoing; ++j)
            g.tick(t += kHeartbeatMs);
        g.onButton(press(Button::B));
    }

    // Queue a real sighting: the NEXT Wi-Fi event resolves it as genuinely new.
    const uint8_t bssid[6] = {0x30, 0x40, 0x50, 0x60, 0x70, 0x80};
    walkAndCreditNetwork(g, bssid, "Neighbour");
    CHECK(g.networksSeen() == 1);

    // Re-queue the SAME BSSID: the ledger already knows it, so the next event
    // resolves it as a repeat — never a second credit.
    walkAndCreditNetwork(g, bssid, "Neighbour");
    CHECK(g.networksSeen() == 1);
}

// Sleeping guardian -> sneak the cache uncontested: the reward-pool draw
// (Bits, same path as a loot cache) with no fight and no Fragmentation risk.
void test_wifi_sleeping_guardian_grants_loot() {
    Game g{StartMode::Hatched};
    walkToWifiOutcome(g, Game::WifiOutcome::SleepingGuardian);
    CHECK(g.nav() == Game::Nav::Wifi);
    CHECK(g.wifiOutcome() == Game::WifiOutcome::SleepingGuardian);
    const int bits0 = g.bits();
    g.onButton(press(Button::B));
    CHECK(g.nav() == Game::Nav::Idle);
    CHECK(g.bits() == bits0 + kLootBitsReward);
}

// Open cache -> the same straight reward-pool draw, no guardian at all.
void test_wifi_open_cache_grants_loot() {
    Game g{StartMode::Hatched};
    walkToWifiOutcome(g, Game::WifiOutcome::OpenCache);
    const int bits0 = g.bits();
    g.onButton(press(Button::B));
    CHECK(g.nav() == Game::Nav::Idle);
    CHECK(g.bits() == bits0 + kLootBitsReward);
}

// Drive the audit-capture SM to Hot ("broadcasting") — the only signal that
// flags the player as a target. Toggle on (-> Armed) then a captured
// handshake (-> Hot).
static void makeHot(Game& g) {
    g.setAuditCaptureEnabled(true);                  // -> Armed
    const uint8_t bssid[6] = {0xde, 0xad, 0xbe, 0xef, 0x00, 0x01};
    g.registerHandshake(bssid);                      // Armed -> Hot
    CHECK(g.auditCapture().broadcasting());
}

// Awakened guardian routes straight into the shared wild-combat path with LIVE
// stakes ("reuse startWildCombat/Nav::Combat") — no Fight/Flee/Sinkhole
// intro, the guardian is already hostile. A guardian only wakes when the player
// is HOT: drive the SM hot up front, then search. The walk helpers keep
// the game clock low (each resets a local tick base at 0), so nowMs stays well
// under the ~2-min hot window and broadcasting() persists across the whole search.
void test_wifi_awakened_guardian_enters_combat() {
    Game g{StartMode::Hatched};
    makeHot(g);
    walkToWifiOutcome(g, Game::WifiOutcome::AwakenedGuardian);
    CHECK(g.auditCapture().broadcasting());          // still hot at the decision point
    CHECK(g.nav() == Game::Nav::Wifi);
    CHECK(g.wifiOutcome() == Game::WifiOutcome::AwakenedGuardian);
    g.onButton(press(Button::B));
    CHECK(g.nav() == Game::Nav::Combat);
    CHECK(g.combat().stakes() == Combat::Stakes::Live);
}

// The gate: merely DISCOVERING networks never flags the player hot, so a
// non-hot player is never auto-attacked by a Wi-Fi event — over a large sample of
// Wi-Fi events, none awaken a guardian and none route into combat. (Wild-step
// combats reached separately are the expected majority path — not our concern here.)
void test_wifi_never_awakens_when_not_hot() {
    Game g{StartMode::Hatched};
    CHECK(!g.auditCapture().broadcasting());         // not hot: no capture done
    int wifiEvents = 0;
    for (int k = 0; k < 80 && wifiEvents < 40; ++k) {
        walkToWifiEvent(g);                          // robust: reaches a Wi-Fi screen
        if (g.nav() != Game::Nav::Wifi) break;       // search exhausted (shouldn't happen)
        CHECK(!g.auditCapture().broadcasting());     // still not hot
        CHECK(g.wifiOutcome() != Game::WifiOutcome::AwakenedGuardian);
        ++wifiEvents;
        g.onButton(press(Button::B));                // a peaceful outcome -> back to idle
        CHECK(g.nav() != Game::Nav::Combat);         // never a fight from a Wi-Fi event
    }
    CHECK(wifiEvents >= 20);                          // sampled enough to trust the gate
}

// Friendly visit grants a transient ally buff for the next 3 battles (
// ) — the v1 substitute for the doc-07 Met-Pets Roster: it boosts
// the player's combat power (buildPlayerCombatant) rather than naming a
// remembered pet, consumed one battle at a time regardless of outcome.
void test_wifi_friendly_visit_grants_ally_buff() {
    Game g{StartMode::Hatched};
    walkToWifiOutcome(g, Game::WifiOutcome::FriendlyVisit);
    CHECK(g.allyBuffBattlesLeft() == 0);
    g.onButton(press(Button::B));
    CHECK(g.nav() == Game::Nav::Idle);
    CHECK(g.allyBuffBattlesLeft() == kAllyBuffBattles);

    walkToAnyCombat(g);                                  // a REAL battle entry
    CHECK(g.nav() == Game::Nav::Combat);
    CHECK(g.allyBuffBattlesLeft() == kAllyBuffBattles - 1);
}

// Hands-off Wi-Fi reveal: with no button press the rolled outcome auto-
// plays out after the ~3s reveal hold — here a sleeping guardian's free loot — and
// hands back to the idle habitat so exploration keeps running. Zero presses.
void test_wifi_auto_plays_out_after_hold() {
    Game g{StartMode::Hatched};
    walkToWifiOutcome(g, Game::WifiOutcome::SleepingGuardian);
    CHECK(g.nav() == Game::Nav::Wifi);
    const int bits0 = g.bits();
    uint32_t t = g.lifetimeUptimeMs();                   // continue from the game's clock
    // Well within the reveal hold: still parked on the reveal.
    for (int i = 0; i < kExploreRevealHoldBeats / 3; ++i) g.tick(t += kHeartbeatMs);
    CHECK(g.nav() == Game::Nav::Wifi);
    // Past the hold: auto-plays out (stop the instant it leaves the reveal, before the
    // idle auto-stepper can type the next event).
    bool played = false;
    for (int i = 0; i < kExploreRevealHoldBeats + 4 && !played; ++i) {
        g.tick(t += kHeartbeatMs);
        if (g.nav() != Game::Nav::Wifi) played = true;
    }
    CHECK(played);
    CHECK(g.nav() == Game::Nav::Idle);                   // auto-resolved, no presses
    CHECK(g.bits() == bits0 + kLootBitsReward);          // the free loot landed
}

// ===========================================================================
// Backlog / — shops as explore events + the two shop consumables.
// A storefront is a self-contained walk event (like the Wi-Fi event): one item,
// restocked + priced per that area's own shop def (src/core/content/areas/). Byte
// to Eat (Citrus Circuit -> Null Noodles) and Pier-to-Peer (The Pirate Bayou ->
// R007_B33R). The two items break "+Hunger = fills" in opposite ways.
// ===========================================================================

// Bounded search: walk until the periodic roll types a storefront shop event,
// leaving the game AT Nav::Shop. Dismisses any other typed event (Wild via a
// Sinkhole Trap, Wi-Fi via B, boss via ride-out) — robust to the weight mix.
// `stopAt` is the shop nav (Shop or ModShop) this walk is trying to reach; landing
// in the OTHER shop kind just leaves it (C) so the walk keeps stepping instead of
// getting stuck there.
static void walkToShopNav(Game& g, Game::Nav stopAt) {
    g.inventory().add("sinkhole_trap", 60);   // bypass Wild encounters, free
    enterWalk(g);
    uint32_t t = 0;
    for (int i = 0; i < 800 && g.nav() != stopAt; ++i) {
        switch (g.nav()) {
            case Game::Nav::Idle:
                if (g.exploreActive()) pingExplore(g); else enterWalk(g);
                break;
            case Game::Nav::Encounter:
                g.onButton(press(Button::A));   // Fight -> Flee
                g.onButton(press(Button::A));   // Flee -> Sinkhole
                g.onButton(press(Button::B));   // confirm -> back to idle
                break;
            case Game::Nav::Wifi: g.onButton(press(Button::B)); break;
            case Game::Nav::Shop:
            case Game::Nav::ModShop:
                g.onButton(press(Button::C));   // the OTHER shop kind -> leave, keep stepping
                break;
            case Game::Nav::Combat:
                for (int j = 0; j < 800 &&
                        g.combat().outcome() == Combat::Outcome::Ongoing; ++j)
                    g.tick(t += kHeartbeatMs);
                g.onButton(press(Button::B));
                break;
            case Game::Nav::PostEncounter:
                g.onButton(press(Button::B));   // that fight's status readout -> dismiss
                break;
            default: break;
        }
    }
}
static void walkToShop(Game& g) { walkToShopNav(g, Game::Nav::Shop); }
static void walkToModShop(Game& g) { walkToShopNav(g, Game::Nav::ModShop); }

// Buying spends the shop's OWN price in Bits (AreaShopDef::price, distinct from
// the item's own bitsPrice), adds one to the inventory, and draws down the
// per-visit stock; the storefront stays open to buy more, and C leaves back to
// the Walk.
void test_shop_event_buy_decrements() {
    Game g{StartMode::Hatched};
    walkToShop(g);
    CHECK(g.nav() == Game::Nav::Shop);
    const int row = g.shopCursor();
    const char* id = g.shopListingId(row);
    CHECK(id != nullptr && id[0] != '\0');
    const int price = g.shopListingBitsPrice(row);
    CHECK(price > 0);
    const int stock0 = g.shopListingStock(row);       // refilled for this visit
    CHECK(stock0 > 0);
    const int bits0 = g.bits();
    const int inv0 = g.inventory().count(id);
    g.onButton(press(Button::B));                     // buy one
    CHECK(g.bits() == bits0 - price);
    CHECK(g.inventory().count(id) == inv0 + 1);
    CHECK(g.shopListingStock(row) == stock0 - 1);
    CHECK(g.nav() == Game::Nav::Shop);               // still open
    CHECK(g.log().size() >= 1 &&
          g.log().at(0).type == LogEventType::ItemGained);
    g.onButton(press(Button::C));                    // leave -> back to the habitat
    CHECK(g.nav() == Game::Nav::Idle);
}

// Broke gate: with fewer Bits than the price, a buy is inert — no spend, no
// item, no stock draw — and the status line spells out the reason (grayscale).
void test_shop_buy_gated_when_broke() {
    Game g{StartMode::Hatched};
    walkToShop(g);
    CHECK(g.nav() == Game::Nav::Shop);
    const int row = g.shopCursor();
    const char* id = g.shopListingId(row);
    const int price = g.shopListingBitsPrice(row);
    CHECK(id != nullptr && price > 0);
    g.debugSetBits(price - 1);                       // one Bit short
    const int inv0 = g.inventory().count(id);
    const int stock0 = g.shopListingStock(row);
    g.onButton(press(Button::B));                    // try to buy -> inert
    CHECK(g.bits() == price - 1);                    // unchanged
    CHECK(g.inventory().count(id) == inv0);
    CHECK(g.shopListingStock(row) == stock0);
    CHECK(g.nav() == Game::Nav::Shop);
}

// Selling out: buying the whole stock leaves it at 0, after which further
// buys are inert (the stock gate). Wallet is topped up so only stock limits it.
void test_shop_sold_out_after_stock() {
    Game g{StartMode::Hatched};
    walkToShop(g);
    CHECK(g.nav() == Game::Nav::Shop);
    const int row = g.shopCursor();
    const char* id = g.shopListingId(row);
    CHECK(id != nullptr);
    const int stock0 = g.shopListingStock(row);
    g.debugSetBits(g.shopListingBitsPrice(row) * (stock0 + 5));  // plenty to clear the shelf
    for (int i = 0; i < stock0; ++i) g.onButton(press(Button::B));
    CHECK(g.shopListingStock(row) == 0);
    const int bits0 = g.bits();
    const int inv0 = g.inventory().count(id);
    g.onButton(press(Button::B));                       // sold out -> inert
    CHECK(g.bits() == bits0);
    CHECK(g.inventory().count(id) == inv0);
    CHECK(g.shopListingStock(row) == 0);
}

// Arbitrary-length listing cycling: A steps the cursor through EVERY listing (not
// just a 0/1 toggle) and wraps back to 0 — generic over whatever count the current
// area's storefront happens to carry, so a future area with more rows doesn't need
// a new test.
void test_shop_cursor_cycles_all_listings() {
    Game g{StartMode::Hatched};
    walkToShop(g);
    CHECK(g.nav() == Game::Nav::Shop);
    const int n = g.shopListingCount();
    CHECK(n >= 1);
    for (int i = 0; i < n; ++i) {
        CHECK(g.shopCursor() == i);
        g.onButton(press(Button::A));
    }
    CHECK(g.shopCursor() == 0);   // wrapped back to the first listing
}

// Mod shop: buying spends Bits AND every item cost the listing carries (not Bits
// alone), then grants the mod into the loadout's spare pool — a guaranteed buy, not
// a lucky drop.
void test_mod_shop_buy_grants_mod() {
    Game g{StartMode::Hatched};
    walkToModShop(g);
    CHECK(g.nav() == Game::Nav::ModShop);
    CHECK(g.shopIsModShop());
    const int row = g.shopCursor();
    const char* id = g.shopListingId(row);
    CHECK(id != nullptr && id[0] != '\0');
    const int price = g.shopListingBitsPrice(row);
    const int costCount = g.shopListingCostCount(row);
    CHECK(costCount >= 1);   // the mod shop's whole point: an item cost beyond Bits
    for (int k = 0; k < costCount; ++k)
        g.inventory().add(g.shopListingCostId(row, k), g.shopListingCostQty(row, k));
    g.debugSetBits(price);
    g.onButton(press(Button::B));
    CHECK(g.loadout().owns(id));
    CHECK(g.bits() == 0);
    for (int k = 0; k < costCount; ++k)
        CHECK(g.shopListingCostHave(row, k) == 0);   // the cost items were consumed
}

// Item-cost gate: an ample wallet alone isn't enough — a mod-shop listing's item
// cost(s) must also be held, or the buy stays inert (mirrors the Bits-only gate
// test_shop_buy_gated_when_broke, but for the NEW cost axis this feature adds).
void test_mod_shop_buy_gated_by_item_cost() {
    Game g{StartMode::Hatched};
    walkToModShop(g);
    CHECK(g.nav() == Game::Nav::ModShop);
    const int row = g.shopCursor();
    const char* id = g.shopListingId(row);
    CHECK(g.shopListingCostCount(row) >= 1);
    g.debugSetBits(g.shopListingBitsPrice(row) * 10);   // plenty of Bits
    const int bits0 = g.bits();
    const bool ownedBefore = g.loadout().owns(id);
    g.onButton(press(Button::B));                       // no cost items held -> inert
    CHECK(g.bits() == bits0);
    CHECK(g.loadout().owns(id) == ownedBefore);
}

// Dual-coding gate: the shop screen reads in grayscale (the release rule).
void test_shop_grayscale() {
    Game g{StartMode::Hatched};
    walkToShop(g);
    CHECK(g.nav() == Game::Nav::Shop);
    Framebuffer fb(kActiveW, kActiveH);
    g.render(fb);
    CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));
}

// Hands-off shop DECISION: a shop is a real buy/leave choice, so the auto-
// mode holds it ~10s (longer than a reveal) to let a watching player act. A button
// press restarts the hold so an engaged shopper isn't rushed; left untouched past the
// full hold it auto-leaves back to the habitat and exploration continues.
void test_shop_auto_leaves_after_hold() {
    Game g{StartMode::Hatched};
    walkToShop(g);
    CHECK(g.nav() == Game::Nav::Shop);
    uint32_t t = g.lifetimeUptimeMs();                   // continue from the game's clock
    // A reveal-length wait is NOT enough — the decision hold is longer.
    for (int i = 0; i < kExploreRevealHoldBeats + 2; ++i) g.tick(t += kHeartbeatMs);
    CHECK(g.nav() == Game::Nav::Shop);
    // A press restarts the hold — the engaged shopper keeps the shop open.
    g.onButton(press(Button::A));                        // one-item shop: A is a no-op cycle
    for (int i = 0; i < kExploreDecisionHoldBeats - 2; ++i) g.tick(t += kHeartbeatMs);
    CHECK(g.nav() == Game::Nav::Shop);                   // the press extended the window
    // Left alone past the full hold, it auto-leaves (stop the instant it does).
    bool left = false;
    for (int i = 0; i < kExploreDecisionHoldBeats + 4 && !left; ++i) {
        g.tick(t += kHeartbeatMs);
        if (g.nav() != Game::Nav::Shop) left = true;
    }
    CHECK(left);
    CHECK(g.nav() == Game::Nav::Idle);                   // auto-left, no purchase forced
    CHECK(g.exploreActive());                            // explore keeps running
}

// Sealed Cache ----------------------------------------
// A container collected NON-INTERRUPTING on the walk (the walk keeps going, no
// sub-screen), then ALWAYS openable from ITEMS for a rarity-tiered reward draw.
// a find rolls one of four rarity tiers (sealed_cache_<rarity>), so the
// test counts the whole family, not a single base id.
static const char* const kCacheIds[] = {"sealed_cache_common",
                                        "sealed_cache_uncommon",
                                        "sealed_cache_rare", "sealed_cache_epic"};
static int cacheCount(const Game& g) {
    int n = 0;
    for (const char* id : kCacheIds) n += g.inventory().count(id);
    return n;
}
// walkToCache bounded-searches to a find, bypassing Wilds with Sinkhole Traps (like
// walkToShop) and fighting the sector boss out if it forces (combat returns to Walk).
static void walkToCache(Game& g) {
    g.inventory().add("sinkhole_trap", 200);   // bypass Wild encounters, free
    enterWalk(g);
    uint32_t t = 0;
    for (int i = 0; i < 2000 && cacheCount(g) == 0; ++i) {
        switch (g.nav()) {
            case Game::Nav::Idle:
                if (g.exploreActive()) pingExplore(g); else enterWalk(g);
                break;
            case Game::Nav::Encounter:
                g.onButton(press(Button::A));   // Fight -> Flee
                g.onButton(press(Button::A));   // Flee -> Sinkhole
                g.onButton(press(Button::B));   // confirm -> back to idle
                break;
            case Game::Nav::Wifi: g.onButton(press(Button::B)); break;
            case Game::Nav::Shop: g.onButton(press(Button::C)); break;
            case Game::Nav::ModShop: g.onButton(press(Button::C)); break;
            case Game::Nav::Combat:
                for (int j = 0; j < 800 &&
                        g.combat().outcome() == Combat::Outcome::Ongoing; ++j)
                    g.tick(t += kHeartbeatMs);
                g.onButton(press(Button::B));
                break;
            case Game::Nav::PostEncounter:
                g.onButton(press(Button::B));   // that fight's status readout -> dismiss
                break;
            default: break;
        }
    }
}

// The cache find is non-interrupting: it lands a (rarity-tiered) cache in the
// inventory while explore-mode keeps stepping (no sub-screen), and logs the find.
void test_sealed_cache_walk_find() {
    Game g{StartMode::Hatched};
    walkToCache(g);
    CHECK(cacheCount(g) >= 1);
    CHECK(g.nav() == Game::Nav::Idle);             // explore was NOT interrupted
}

// Decrypting a Sealed Cache now happens in the Hacker VAULT, NOT pet-side
// ITEMS. (a) pet-side Use is inert (gated "DECRYPT IN VAULT" — nothing consumed);
// (b) VAULT B decrypts: consumes one container, draws the reward pool (>= flat Bits),
// logs it, reveals the yield (Nav::CacheYield).
void test_sealed_cache_open_grants_reward() {
    // (a) pet-side ITEMS no longer opens caches — Use is inert.
    {
        Game g{StartMode::Hatched};
        g.inventory().add("sealed_cache", 2);
        const int bits0 = g.bits();
        g.debugUseItem("sealed_cache");                  // pet-side Use path
        CHECK(g.inventory().count("sealed_cache") == 2); // NOTHING consumed
        CHECK(g.bits() == bits0);                        // no reward drawn
        CHECK(g.nav() != Game::Nav::CacheYield);         // no yield reveal
    }
    // (b) VAULT decrypt consumes + grants the reward.
    {
        Game g{StartMode::Hatched};
        g.inventory().add("sealed_cache", 2);
        const int bits0 = g.bits();
        // Into the Hacker face, focus VAULT, enter it, press B to decrypt.
        g.onButton({Button::A, true, true});             // PET -> HACKER
        g.onButton(press(Button::A));                    // summon the hacker cursor
        while (hackerCarouselSlots()[g.cursor()].id != HackerSlotId::Vault)
            g.onButton(press(Button::A));
        CHECK(hackerCarouselSlots()[g.cursor()].accessible);  // VAULT is live
        g.onButton(press(Button::B));                    // enter VAULT
        CHECK(g.nav() == Game::Nav::Submenu);
        g.onButton(press(Button::B));                    // decrypt the selected cache
        CHECK(g.inventory().count("sealed_cache") == 1); // one container consumed
        CHECK(g.bits() >= bits0 + kLootBitsReward);      // reward-pool draw paid Bits
        CHECK(g.log().size() >= 1);                      // the open/contents were logged
        CHECK(g.nav() == Game::Nav::CacheYield);         // yield reveal shown
    }
}

// Every rarity-tiered cache is an openable Quest container carrying no vitals/price
// — found, not bought, opened for its draw. Distinct rarities across the family.
void test_sealed_cache_item_shape() {
    ContentRegistry r = ContentRegistry::embedded();
    const ItemDef* d = r.item("sealed_cache");
    CHECK(d != nullptr);
    CHECK(d->use == ItemDef::Use::OpenContainer);
    CHECK(d->type == ItemDef::Type::Quest);          // always openable (egg gate skips Quest)
    CHECK(d->bitsPrice == 0);                         // not sold at a shop
    // The four tiered variants exist and carry ascending rarities.
    const ItemDef* c = r.item("sealed_cache_common");
    const ItemDef* e = r.item("sealed_cache_epic");
    CHECK(c && c->use == ItemDef::Use::OpenContainer && c->rarity == ItemDef::Rarity::Common);
    CHECK(e && e->use == ItemDef::Use::OpenContainer && e->rarity == ItemDef::Rarity::Epic);
    for (const char* id : {"sealed_cache_common", "sealed_cache_uncommon",
                           "sealed_cache_rare", "sealed_cache_epic"}) {
        const ItemDef* t = r.item(id);
        CHECK(t != nullptr && t->use == ItemDef::Use::OpenContainer &&
              t->type == ItemDef::Type::Quest && t->bitsPrice == 0);
    }
}

// opening a tiered cache pays that tier's Bits and drops item(s) from the
// tier's rarity-specific pool. The epic tier is the multi-item drop (2 items), and
// its pool is scarce-utility only (rollback / yubi_cookie / backup_drive) — none of
// the common consumables. Grayscale-independent: assertions are on ids + counts.
void test_cache_epic_pool_and_multidrop() {
    Game g{StartMode::Hatched};
    g.inventory().add("sealed_cache_epic", 1);
    const int bits0 = g.bits();
    // Count the WHOLE epic pool (rollback / yubi_cookie / backup_drive / restore_point /
    // deep_learning_core / zeroday_bell) so the delta is robust to which two members the
    // seeded draw picks.
    auto epicPool = [&] {
        return g.inventory().count("rollback") + g.inventory().count("yubi_cookie") +
               g.inventory().count("backup_drive") + g.inventory().count("restore_point") +
               g.inventory().count("deep_learning_core") + g.inventory().count("zeroday_bell");
    };
    const int items0 = epicPool();
    g.debugOpenCache("sealed_cache_epic");           // decrypt (VAULT path)
    CHECK(g.bits() == bits0 + ContentRegistry::embedded().item("sealed_cache_epic")->cache.bits);  // epic Bits, not the flat loot
    CHECK(epicPool() - items0 == 2);                 // epic drops TWO items
    // Nothing from a lower pool leaked in (epic pool has no common consumables).
    CHECK(g.inventory().count("airgap_snack") == Inventory::starting().count("airgap_snack"));
    CHECK(g.inventory().count("null_noodles") == 0);
}

// The common tier pays the common Bits and drops exactly one consumable from its
// pool — a cheaper draw than epic. Counted over the cache's OWN pool rather than a
// hand-listed set of ids, so adding a row to the pantry can't quietly turn this into
// a test of five specific items that the draw is now allowed to miss.
void test_cache_common_pool_single_drop() {
    Game g{StartMode::Hatched};
    g.inventory().add("sealed_cache_common", 1);
    const int bits0 = g.bits();
    const CacheDef& common = ContentRegistry::embedded().item("sealed_cache_common")->cache;
    auto commonPool = [&] {
        int n = 0;
        for (int i = 0; i < common.poolSize; ++i) n += g.inventory().count(common.pool[i].id);
        return n;
    };
    const int pool0 = commonPool();
    g.debugOpenCache("sealed_cache_common");         // decrypt (VAULT path)
    CHECK(g.bits() == bits0 + ContentRegistry::embedded().item("sealed_cache_common")->cache.bits);
    CHECK(commonPool() - pool0 == 1);                // exactly one common item
}

// Earn-path coverage: EVERY understood item must be obtainable in play, not just
// listed in the registry. An item is earnable if it is on the one-time starting
// shelf, in some container's reward pool (ItemDef::cache.pool), in the walk loot-cache
// pool (kLootPool), shop-buyable (bitsPrice), a container the walk can FIND
// (cache.findWeight > 0) or a warp key, the OUTPUT of a Hacker MERGE HUB recipe
// (game_internal.h kMergeRecipes — its two INPUT ingredients still need their own
// earn path, same as any other item), or handed over by an ACHIEVEMENT reward.
//
// Note the container rule is findWeight, not "is a container": a cache that is only ever
// granted (the Commendation Cache) has to justify itself through the achievement table
// like anything else, so adding an unreachable container fails here.
void test_item_earn_coverage() {
    ContentRegistry r = ContentRegistry::embedded();
    Inventory starting = Inventory::starting();
    auto inCachePool = [&r](const char* id) {
        for (const ItemDef* c : r.allItems())
            for (int i = 0; i < c->cache.poolSize; ++i)
                if (std::strcmp(c->cache.pool[i].id, id) == 0) return true;
        return false;
    };
    auto isMergeOutput = [](const char* id) {
        for (const MergeRecipe& rec : kMergeRecipes)
            if (std::strcmp(rec.outputId, id) == 0) return true;
        return false;
    };
    auto inLootPool = [](const char* id) {
        for (int i = 0; i < kLootPoolCount; ++i)
            if (std::strcmp(kLootPool[i].id, id) == 0) return true;
        return false;
    };
    auto isAchievementReward = [](const char* id) {
        for (int i = 0; i < kAchievementCount; ++i)
            for (const AchievementReward& rw : kAchievements[i].rewards)
                if (rw.kind == AchievementReward::Kind::Item && rw.id &&
                    std::strcmp(rw.id, id) == 0)
                    return true;
        return false;
    };
    for (const ItemDef* it : r.allItems()) {
        // The one deliberate orphan: the untiered original cache. It predates the rarity
        // tiers and is kept ONLY so a save written before them still has an openable
        // container — nothing grants it any more, and nothing should.
        if (std::strcmp(it->id, "sealed_cache") == 0) continue;
        const bool earnable =
            starting.count(it->id) > 0 ||                        // starting shelf
            inCachePool(it->id) ||                               // a container's pool
            inLootPool(it->id) ||                                // walk loot-cache pool
            it->bitsPrice > 0 ||                                 // shop-buyable
            it->cache.findWeight > 0 ||                          // a container the walk drops
            it->walkWarp != ItemDef::WalkWarp::None ||           // warp key found on the walk
            isMergeOutput(it->id) ||                             // Merge Hub recipe output
            isAchievementReward(it->id);                         // an achievement pays it
        if (!earnable) std::printf("  ORPHAN ITEM (no earn path): %s\n", it->id);
        CHECK(earnable);
    }
    // Pin the specifically-placed items to their homes (documents intent).
    CHECK(inCachePool("decrypt_key"));
    CHECK(inCachePool("restore_point"));
    CHECK(isAchievementReward("commend_cache"));
    // The untiered legacy cache is deliberately unfindable now, and so is the earned-only
    // Commendation Cache — the walk must never roll either.
    CHECK(r.item("sealed_cache")->cache.findWeight == 0);
    CHECK(r.item("commend_cache")->cache.findWeight == 0);
}

// opening a cache surfaces a yield reveal (Nav::CacheYield) naming the
// container + what came out; B dismisses it back to the ITEMS list. The reveal's
// item count matches the tier's multi-drop (2 for epic).
void test_cache_yield_reveal_and_dismiss() {
    Game g{StartMode::Hatched};
    g.inventory().add("sealed_cache_epic", 1);
    g.debugOpenCache("sealed_cache_epic");            // decrypt (VAULT path)
    CHECK(g.nav() == Game::Nav::CacheYield);          // reveal is up
    CHECK(g.cacheYieldItemCount() == 2);              // epic yielded two items
    CHECK(g.cacheYieldBits() == ContentRegistry::embedded().item("sealed_cache_epic")->cache.bits);  // and the Bits it paid
    g.onButton(press(Button::B));                     // OK dismisses...
    CHECK(g.nav() == Game::Nav::Submenu);             // ...back to the submenu (VAULT list)
}

// Key warp items -------------------------------------------------
// Two explore-use keys: Access Token warps to the nearest shop, Safe-Mode Key to a
// safe rest. Both are Quest-typed (egg gate skips them), priceless (found, not
// bought), carry no vitals, and are spent ON the walk (walkWarp != None), never
// from the ITEMS use path.
void test_warp_item_shape() {
    ContentRegistry r = ContentRegistry::embedded();
    const ItemDef* tok = r.item("access_token");
    const ItemDef* key = r.item("safe_mode_key");
    CHECK(tok != nullptr);
    CHECK(key != nullptr);
    CHECK(tok->walkWarp == ItemDef::WalkWarp::Shop);
    CHECK(key->walkWarp == ItemDef::WalkWarp::SafeRest);
    CHECK(tok->type == ItemDef::Type::Quest && key->type == ItemDef::Type::Quest);
    CHECK(tok->bitsPrice == 0 && key->bitsPrice == 0);            // found, not sold
    CHECK(tok->use != ItemDef::Use::OpenContainer &&             // not containers
          key->use != ItemDef::Use::OpenContainer);
}

// A warp key held in inventory is INERT from the ITEMS use path ("USE ON THE WALK"),
// so a stray Use can't burn it for nothing — the count is untouched.
void test_warp_key_inert_from_items() {
    Game g{StartMode::Hatched};
    g.inventory().add("access_token", 1);
    g.debugUseItem("access_token");                  // real path: gate -> inert
    CHECK(g.inventory().count("access_token") == 1); // NOT consumed
}

// Using the Access Token WHILE EXPLORING (A+C control chord → B opens the picker,
// B spends it) warps straight to a shop event — consumes the key and lands on
// Nav::Shop (re-homed onto the control chord).
void test_access_token_warps_to_shop() {
    Game g{StartMode::Hatched};
    enterWalk(g);
    g.inventory().add("access_token", 1);
    g.onButton(chordAC());                           // A+C -> control overlay, on PING
    CHECK(g.nav() == Game::Nav::ExploreControl);
    g.onButton(press(Button::A));                    // A -> the WARP row
    g.onButton(press(Button::B));                    // B -> warp picker
    CHECK(g.nav() == Game::Nav::WarpPicker);
    g.onButton(press(Button::B));                    // spend the focused key
    CHECK(g.nav() == Game::Nav::Shop);               // warped to the storefront
    CHECK(g.inventory().count("access_token") == 0); // key consumed
}

// Using the Safe-Mode Key while exploring warps to a safe rest: it de-frags the pet
// by the key ROW's own Frag magnitude, consumes the key, and returns to the idle
// habitat (no combat). Read off the row rather than a constant, so retuning the key
// retunes the gate — the magnitude is the row's to own.
void test_safe_mode_key_warps_to_rest() {
    ContentRegistry reg = ContentRegistry::embedded();
    const ItemDef* key = reg.item("safe_mode_key");
    CHECK(key != nullptr);
    int rowFrag = 0;
    for (const ItemEffect& e : key->effects)
        if (e.kind == ItemEffect::Kind::Frag) rowFrag += e.magnitude;
    CHECK(rowFrag < 0);   // the row de-frags (negative Frag), not fragments

    Game g{StartMode::Hatched};
    enterWalk(g);
    g.model().setFragmentation(60);
    g.inventory().add("safe_mode_key", 1);
    g.onButton(chordAC());                           // A+C -> control overlay, on PING
    g.onButton(press(Button::A));                    // A -> the WARP row
    g.onButton(press(Button::B));                    // B -> warp picker
    CHECK(g.nav() == Game::Nav::WarpPicker);
    g.onButton(press(Button::B));                    // spend the focused key
    CHECK(g.nav() == Game::Nav::Idle);               // safe rest resolves in place
    CHECK(g.model().fragmentation() == 60 + rowFrag);           // rest de-frags
    CHECK(g.inventory().count("safe_mode_key") == 0);          // key consumed
}

// With no warp keys held, the control overlay's B (Warp) is a harmless no-op — it
// opens no picker and stays in the overlay ("else inert").
void test_explore_warp_no_keys_is_noop() {
    Game g{StartMode::Hatched};
    enterWalk(g);
    g.onButton(chordAC());                           // A+C -> control overlay, on PING
    CHECK(g.nav() == Game::Nav::ExploreControl);
    g.onButton(press(Button::A));                    // A -> the WARP row
    g.onButton(press(Button::B));                    // Warp with no key held -> inert
    CHECK(g.nav() == Game::Nav::ExploreControl);     // no picker opened
}

// The Key-item find walk event is NON-INTERRUPTING: it lands a warp
// key in the inventory while the walk stays live. A bounded walk to the first key
// find (bypassing wilds with Sinkhole Traps, fighting a boss out if it forces).
void test_warp_key_walk_find() {
    Game g{StartMode::Hatched};
    g.inventory().add("sinkhole_trap", 200);         // bypass Wild encounters, free
    enterWalk(g);
    uint32_t t = 0;
    auto keysHeld = [&] {
        return g.inventory().count("access_token") +
               g.inventory().count("safe_mode_key");
    };
    for (int i = 0; i < 4000 && keysHeld() == 0; ++i) {
        switch (g.nav()) {
            case Game::Nav::Idle:
                if (g.exploreActive()) pingExplore(g); else enterWalk(g);
                break;
            case Game::Nav::Encounter:
                g.onButton(press(Button::A));        // Fight -> Flee
                g.onButton(press(Button::A));        // Flee -> Sinkhole
                g.onButton(press(Button::B));        // confirm -> back to idle
                break;
            case Game::Nav::Wifi: g.onButton(press(Button::B)); break;
            case Game::Nav::Shop: g.onButton(press(Button::C)); break;
            case Game::Nav::ModShop: g.onButton(press(Button::C)); break;
            case Game::Nav::Combat:
                for (int j = 0; j < 800 &&
                        g.combat().outcome() == Combat::Outcome::Ongoing; ++j)
                    g.tick(t += kHeartbeatMs);
                g.onButton(press(Button::B));
                break;
            case Game::Nav::PostEncounter:
                g.onButton(press(Button::B));   // that fight's status readout -> dismiss
                break;
            default: break;
        }
    }
    CHECK(keysHeld() >= 1);                           // a key was found
    CHECK(g.nav() == Game::Nav::Idle);               // explore was NOT interrupted
}

// Null Noodles: the "empty feeling" food — de-frags, makes the pet HUNGRIER
// (negative eating, the explicit exception to "+Hunger = fills"), and pulls
// Happiness TOWARD 50% from ABOVE (a too-happy pet is numbed down).
void test_null_noodles_effects() {
    Game g{StartMode::Hatched};
    g.inventory().add("null_noodles", 1);
    g.model().setHunger(60);
    g.model().setFragmentation(40);
    g.model().setHappiness(90);                      // above the midpoint
    g.debugUseItem("null_noodles");
    CHECK(g.model().hunger() == 60 - 15);            // negative eating -> hungrier
    CHECK(g.model().fragmentation() == 40 - 15);     // de-frags
    CHECK(g.model().happiness() == 90 - 20);         // pulled toward 50 (item's pull=20)
    CHECK(g.model().happiness() >= 50);              // never overshoots past 50
    CHECK(g.inventory().count("null_noodles") == 0); // consumed
}

// The Null Noodles Happiness pull works from BELOW 50 too — it lifts a miserable
// pet toward the midpoint (the "numb" middle), never past it.
void test_null_noodles_happy_pull_from_below() {
    Game g{StartMode::Hatched};
    g.inventory().add("null_noodles", 2);
    g.model().setHappiness(20);                      // below the midpoint
    g.debugUseItem("null_noodles");
    CHECK(g.model().happiness() == 20 + 20);         // pulled up toward 50 (item's pull=20)
    CHECK(g.model().happiness() <= 50);
    // From just under 50 it clamps AT 50, not past it.
    g.model().setHappiness(45);
    g.debugUseItem("null_noodles");
    CHECK(g.model().happiness() == 50);              // clamped to the midpoint
}

// R007_B33R: fills Hunger, lifts Happiness, but RAISES Fragmentation — the
// straightforwardly indulgent counterpart to Null Noodles.
void test_r007_b33r_effects() {
    Game g{StartMode::Hatched};
    g.inventory().add("r007_b33r", 1);
    g.model().setHunger(50);
    g.model().setHappiness(40);
    g.model().setFragmentation(20);
    g.debugUseItem("r007_b33r");
    CHECK(g.model().hunger() == 50 + 15);            // fills
    CHECK(g.model().happiness() == 40 + 25);         // cheers
    CHECK(g.model().fragmentation() == 20 + 5);      // but fragments
    CHECK(g.inventory().count("r007_b33r") == 0);    // consumed
}
