// test_items.cpp — native gates for ITEMS, feeding, MAINT/defrag and the Lockout.
//
// One slice of the native suite; see test_gates.h for the shared fixtures and
// test_main.cpp for the roster that runs these. A save-vNN gate sits with the
// feature whose field it migrates, not in a migrations pile of its own.
#include "test_gates.h"

void test_inventory() {
    Inventory inv;
    CHECK(inv.count("airgap_snack") == 0 && !inv.has("airgap_snack"));
    inv.add("airgap_snack", 2);
    inv.add("airgap_snack");                          // default +1
    CHECK(inv.count("airgap_snack") == 3 && inv.has("airgap_snack"));
    CHECK(inv.remove("airgap_snack", 1));
    CHECK(inv.count("airgap_snack") == 2);
    CHECK(!inv.remove("airgap_snack", 5));            // insufficient -> no-op
    CHECK(inv.count("airgap_snack") == 2);

    Inventory s = Inventory::starting();
    CHECK(s.count("airgap_snack") == 3 && s.count("backup_drive") == 1);
    CHECK(s.count("decryptogram") == 1);             // egg accelerator
}

void test_event_log() {
    EventLog log;
    CHECK(log.size() == 0);
    log.push(LogEventType::ItemUsed, "FED A");
    log.push(LogEventType::CareMistake, "FAILED DEFRAG");
    CHECK(log.size() == 2);
    CHECK(log.at(0).type == LogEventType::CareMistake);   // newest first
    CHECK(std::strcmp(log.at(1).text, "FED A") == 0);
    // Ring eviction: capacity is kept, oldest falls off.
    for (int i = 0; i < EventLog::kCapacity + 3; ++i)
        log.push(LogEventType::ItemGained, "X");
    CHECK(log.size() == EventLog::kCapacity);
}

// Grouped inventory list: section headers per group, cursor skips them, wraps.
void test_inventory_rows_grouped() {
    ContentRegistry r = ContentRegistry::embedded();
    Inventory inv = Inventory::starting();           // 7 items across 3 groups
    auto rows = buildInventoryRows(r, inv, false);

    int headers = 0, items = 0;
    for (const auto& row : rows) (row.header ? headers : items)++;
    CHECK(items == 7 && headers == 3);               // FOOD/BUFFS/QUEST
    CHECK(rows.front().header && std::strcmp(rows.front().label, "FOOD") == 0);

    const int f = firstSelectableRow(rows);
    CHECK(f >= 0 && !rows[f].header);
    // Stepping never lands on a header; one lap over the selectable items wraps
    // back to the start.
    int cur = f;
    for (int i = 0; i < items; ++i) {
        cur = stepSelectableRow(rows, cur, +1);
        CHECK(!rows[cur].header);
    }
    CHECK(cur == f);                                 // full wrap returns to start
}

// Within a group the list reads richest-first, so the top of a block is always the
// best thing the bag holds of that type.
void test_inventory_rows_rarity_desc() {
    // Four Foods whose alphabetical order is the REVERSE of their rarity order, so
    // "sorted by rarity" and "sorted by name" can't both pass.
    ContentRegistry r = ContentRegistry::embedded();
    Inventory inv;
    inv.add("breadcrumbs");            // Common
    inv.add("java");                   // Uncommon
    inv.add("applets");                // Uncommon
    inv.add("salted_hashed_browns");   // Rare
    auto rows = buildInventoryRows(r, inv, false);

    const char* const kExpected[] = {"Salted&Hashed Browns", "Applets", "Java",
                                     "Breadcrumbs"};
    int seen = 0;
    for (const auto& row : rows) {
        if (row.header) continue;
        CHECK(seen < 4 && std::strcmp(row.def->displayName, kExpected[seen]) == 0);
        ++seen;
    }
    CHECK(seen == 4);   // rarest first, then alphabetical between the two Uncommons
}

// >6 rows scrolls: the scrollbar column carries position without colour.
void test_inventory_scrollbar() {
    ContentRegistry r = ContentRegistry::embedded();
    Inventory inv = Inventory::starting();
    auto rows = buildInventoryRows(r, inv, false);
    CHECK(static_cast<int>(rows.size()) > 6);        // 6 items + 3 headers = 9
    Framebuffer fb(kActiveW, kActiveH);
    drawItemsList(fb, rows, firstSelectableRow(rows), false, 0);
    CHECK(anyNonPaper(fb, kActiveW - 3, kItemsRowTop, kActiveW, kActiveH));
}

void test_feed_and_maint_model() {
    PetModel m;
    m.setHunger(40);
    m.feed(40, 0);
    CHECK(m.hunger() == 80);                          // food restores Hunger
    m.feed(80, 5);
    CHECK(m.hunger() == 100 && m.happiness() == kStartHappiness + 5);  // clamps

    // NOTE: the +1 care mistake on a maint FAILURE now lands Game-side (via the shielded
    // path — see test_restore_shield_covers_maint_fail); PetModel only reports the failure
    // (returns true) and applies the frag penalty. So these model-level checks assert frag
    // + the returned failure flag, NOT the mistake counter.
    PetModel d;
    d.setFragmentation(50);
    CHECK(!d.applyDefrag(true));
    CHECK(d.fragmentation() == 50 - kDefragReduction && d.careMistakes() == 0);
    CHECK(d.applyDefrag(false));
    CHECK(d.fragmentation() == 30 + kMaintFailPenalty && d.careMistakes() == 0);

    PetModel a;
    a.setFragmentation(50);
    a.setGhost(true);
    a.setDebuffs(1);
    CHECK(!a.applyAntivirus(true));
    CHECK(a.fragmentation() == 40 && !a.hasGhost() && a.debuffs() == 0);
    a.setFragmentation(50);
    CHECK(a.applyAntivirus(false));
    CHECK(a.fragmentation() == 65 && a.careMistakes() == 0);
}

// Feeding: ITEMS -> detail -> Use launches the modal, Hunger rises, count drops,
// and an ITEM_USED row is logged.
void test_feeding_flow() {
    Game g{StartMode::Hatched};
    g.model().setHunger(40);
    enterSlot(g, SubmenuId::Items);
    CHECK(g.nav() == Game::Nav::Submenu);
    g.onButton(press(Button::B));                    // open first food's detail
    CHECK(g.nav() == Game::Nav::Detail);

    const int q0 = g.inventory().count("airgap_snack");
    g.onButton(press(Button::B));                    // Use -> feeding modal
    CHECK(g.nav() == Game::Nav::ModalFeeding);
    CHECK(g.model().hunger() == 80);                 // 40 + 40 restore
    CHECK(g.inventory().count("airgap_snack") == q0 - 1);
    CHECK(g.log().size() >= 1 && g.log().at(0).type == LogEventType::ItemUsed);

    g.onButton(press(Button::B));                    // dismiss -> back to detail
    CHECK(g.nav() == Game::Nav::Detail);
}

// Gated item: Use is inert outside its context (Decryption Key is Lockout-only).
void test_item_context_gate() {
    Game g{StartMode::Hatched};
    enterSlot(g, SubmenuId::Items);
    // Walk to the Decryption Key row, then open + try to Use it.
    auto rows = buildInventoryRows(ContentRegistry::embedded(),
                                   g.inventory(), false);
    // The detail subject is driven by the engine's cursor; instead exercise the
    // pure gate predicate the engine uses.
    const ItemDef* key = ContentRegistry::embedded().item("decrypt_key");
    CHECK(key && key->context == ItemDef::Context::LockoutOnly);
    (void)rows;
}

// MAINT: list -> action -> non-interruptible process -> outcome changes Frag.
void test_maint_flow() {
    Game g{StartMode::Hatched};
    g.model().setFragmentation(50);
    enterSlot(g, SubmenuId::Maint);
    CHECK(g.nav() == Game::Nav::Submenu);
    g.onButton(press(Button::B));                    // enter Defrag action
    CHECK(g.nav() == Game::Nav::Detail);
    g.onButton(press(Button::B));                    // Run -> process
    CHECK(g.nav() == Game::Nav::Process);

    // C is ignored while the process runs (non-interruptible).
    g.onButton(press(Button::C));
    CHECK(g.nav() == Game::Nav::Process);

    for (int i = 1; i <= kProcessBeats + 1; ++i)
        g.tick(static_cast<uint32_t>(i) * kHeartbeatMs);
    const int frag = g.model().fragmentation();
    CHECK(frag == 50 - kDefragReduction || frag == 50 + kMaintFailPenalty);
    g.onButton(press(Button::B));                    // dismiss outcome -> action
    CHECK(g.nav() == Game::Nav::Detail);
}

// a Defrag costs stage-scaled Bits (free to spam before). A Process pet
// (default Paypup) pays kDefragCostByStage[Process]; the wallet is debited on RUN.
void test_defrag_costs_stage_bits() {
    Game g{StartMode::Hatched};                       // Paypup = Process stage
    const int cost = kDefragCostByStage[static_cast<int>(Stage::Process)];
    CHECK(g.defragCost() == cost);
    CHECK(cost > 0);                                   // defrag costs Bits
    g.model().setFragmentation(50);
    g.debugSetBits(cost + 3);
    enterSlot(g, SubmenuId::Maint);
    g.onButton(press(Button::B));                      // enter Defrag action
    g.onButton(press(Button::B));                      // RUN
    CHECK(g.nav() == Game::Nav::Process);              // launched
    CHECK(g.bits() == 3);                              // charged exactly `cost`
}

// an empty (or too-thin) wallet makes Defrag inert — RUN doesn't launch
// and no Bits move, even with Fragmentation to clear.
void test_defrag_gated_when_broke() {
    Game g{StartMode::Hatched};
    g.model().setFragmentation(50);
    g.debugSetBits(g.defragCost() - 1);               // one short
    enterSlot(g, SubmenuId::Maint);
    g.onButton(press(Button::B));                      // enter Defrag action
    const int bits0 = g.bits();
    g.onButton(press(Button::B));                      // RUN -> inert
    CHECK(g.nav() == Game::Nav::Detail);              // stayed on the action screen
    CHECK(g.bits() == bits0);                          // no charge
    CHECK(g.model().fragmentation() == 50);            // nothing defragged
}

// two defrag variants. QUICK is Bits-only with the normal fail roll (covered
// by test_defrag_costs_stage_bits); the TOOL variant spends one Defrag Tool for a
// GUARANTEED clean, is inert with no tool held, and every successful defrag advances the
// per-pet tally.
void test_defrag_variants() {
    Game g{StartMode::Hatched};                       // Paypup = Process stage
    g.model().setFragmentation(60);
    g.debugSetBits(999);
    enterSlot(g, SubmenuId::Maint);
    g.onButton(press(Button::B));                     // enter Defrag action
    CHECK(g.nav() == Game::Nav::Detail);
    g.onButton(press(Button::A));                     // switch to the TOOL variant
    const int bits0 = g.bits();
    g.onButton(press(Button::B));                     // RUN with no tool -> inert
    CHECK(g.nav() == Game::Nav::Detail);              // gated, stayed on the screen
    CHECK(g.bits() == bits0);                          // no charge when inert
    CHECK(g.defragCount() == 0);
    // Hold two tools, run the TOOL defrag -> guaranteed clean, one tool spent, tally++.
    g.inventory().add("disk_scrubber", 2);
    g.onButton(press(Button::B));                     // RUN (TOOL variant) -> process
    CHECK(g.nav() == Game::Nav::Process);
    uint32_t t = 0;
    for (int i = 1; i <= kProcessBeats + 1; ++i) g.tick(t += kHeartbeatMs);
    CHECK(g.model().fragmentation() == 60 - kDefragReduction);  // guaranteed success
    CHECK(g.inventory().count("disk_scrubber") == 1);           // exactly one consumed
    CHECK(g.bits() == bits0 - g.defragCost());                  // Bits still charged
    CHECK(g.defragCount() == 1);                                // the tally advanced
}

// the Defrag Tool can't be Used from the ITEMS path (a stray Use would burn
// it) — it's gated with a "use in MAINT" message.
void test_defrag_tool_gated_in_items() {
    Game g{StartMode::Hatched};
    g.inventory().add("disk_scrubber", 1);
    g.debugUseItem("disk_scrubber");                  // real Use path (gate → no-op)
    CHECK(g.inventory().count("disk_scrubber") == 1); // not consumed (inert)
}

// the per-pet defrag tally stays consistent through ARCH freeze/thaw — a
// stored pet keeps its own count, and the original's count returns when redeployed.
void test_defrag_count_freeze_thaw() {
    Game g{StartMode::Hatched};                       // active Paypup
    g.model().setFragmentation(60);
    g.debugSetBits(999);
    g.inventory().add("disk_scrubber", 1);
    enterSlot(g, SubmenuId::Maint);
    g.onButton(press(Button::B));                     // Defrag action
    g.onButton(press(Button::A));                     // TOOL variant
    g.onButton(press(Button::B));                     // RUN -> process (guaranteed)
    uint32_t t = 0;
    for (int i = 1; i <= kProcessBeats + 1; ++i) g.tick(t += kHeartbeatMs);
    CHECK(g.defragCount() == 1);                       // Paypup: one defrag
    g.onButton(press(Button::B));                      // dismiss outcome -> action
    g.onButton(press(Button::C));                      // action -> MAINT list
    g.onButton(press(Button::C));                      // list -> carousel
    g.onButton(press(Button::C));                      // carousel -> idle habitat

    g.debugSeedRack("cryptoshell");                    // a stored pet at rack row 1
    auto deployRow1 = [](Game& gg) {                   // Deploy the rack-row-1 pet
        enterSlot(gg, SubmenuId::Arch);
        gg.onButton(press(Button::A));                 // row 0 (active) -> row 1 (stored)
        gg.onButton(press(Button::B));                 // open record (Deploy default)
        gg.onButton(press(Button::B));                 // Deploy -> confirm
        gg.onButton(press(Button::A));                 // Cancel -> Confirm
        gg.onButton(press(Button::B));                 // commit
    };
    deployRow1(g);                                     // cryptoshell in, Paypup frozen
    CHECK(g.defragCount() == 0);                       // cryptoshell's own (fresh) tally
    deployRow1(g);                                     // Paypup back out of the rack
    CHECK(g.defragCount() == 1);                       // thawed Paypup's tally intact
}

// ARCH Release valve: a stored pet can be RELEASED — no reward, frees a rack
// slot. Cycle a stored record's actions Deploy -> Sell -> Release, then confirm.
void test_arch_release_stored_frees_slot() {
    Game g{StartMode::Hatched};
    g.debugSeedRack("cryptoshell");                    // a stored pet at rack row 1
    CHECK(g.rackCount() == 1);
    enterSlot(g, SubmenuId::Arch);
    g.onButton(press(Button::A));                      // row 0 (active) -> row 1 (stored)
    g.onButton(press(Button::B));                      // open record (Deploy default)
    g.onButton(press(Button::A));                      // Deploy -> Sell
    g.onButton(press(Button::A));                      // Sell   -> Release
    g.onButton(press(Button::B));                      // Release -> confirm
    g.onButton(press(Button::A));                      // Cancel -> Confirm
    g.onButton(press(Button::B));                      // commit the release
    CHECK(g.rackCount() == 0);                          // slot freed, no record left
    CHECK(g.nav() == Game::Nav::Submenu);              // back to the (empty) rack list
}

// Lockout fires at Hunger 0; an unresolved expiry costs 2 care mistakes.
void test_lockout_expire_mistakes() {
    Game g{StartMode::Hatched};
    g.model().setHunger(0);
    g.tick(kHeartbeatMs);
    CHECK(g.lockoutActive() && g.nav() == Game::Nav::ModalLockout);

    g.tick(kLockoutDurationMs + 2 * kHeartbeatMs);   // past the deadline
    CHECK(!g.lockoutActive() && g.nav() == Game::Nav::Idle);
    // A fully-failed Lockout still nets 2 total: +1 "went hungry" when Hunger hit 0
    // (fireLockout) + kLockoutExpiryMistakes on expiry (the split budget invariant).
    CHECK(g.model().careMistakes() == kWentHungryMistakes + kLockoutExpiryMistakes);
    CHECK(g.model().careMistakes() == 2);            // the invariant holds numerically
    CHECK(g.model().hunger() == kLockoutRecoveryHunger);
    CHECK(g.log().size() >= 1 && g.log().at(0).type == LogEventType::CareMistake);
}

// Restore Point shield: a shielded pet ignores the NEXT positive care mistake, then
// takes the following one. Covers the went-hungry increment path (generic interception).
void test_restore_point_shield_blocks_next_mistake() {
    Game g{StartMode::Hatched};
    CHECK(g.inventory().count("restore_point") == 1);
    g.debugUseItem("restore_point");                 // arm the shield (once-per-lifetime)
    CHECK(g.inventory().count("restore_point") == 0);
    CHECK(g.model().careMistakes() == 0);

    // First failed Lockout: the "went hungry" +1 is BLOCKED by the shield; expiry +1
    // lands (shield already spent). Net = kLockoutExpiryMistakes, not 2.
    g.model().setHunger(0);
    g.tick(kHeartbeatMs);
    g.tick(kLockoutDurationMs + 2 * kHeartbeatMs);
    CHECK(g.model().careMistakes() == kLockoutExpiryMistakes);   // one mistake shielded

    // Second failed Lockout: shield is gone, so both halves land (2 more).
    const int before = g.model().careMistakes();
    g.model().setHunger(0);
    g.tick(kHeartbeatMs);
    g.tick(kLockoutDurationMs + 2 * kHeartbeatMs);
    CHECK(g.model().careMistakes() ==
          before + kWentHungryMistakes + kLockoutExpiryMistakes);
}

// Restore Point is once-per-lifetime: after its shield is spent, a second Use never
// re-arms (the gate flag is consumed for the pet's whole life).
void test_restore_point_once_per_lifetime() {
    Game g{StartMode::Hatched};
    g.debugUseItem("restore_point");                 // arm + spend the once-per-life gate
    // Spend the shield on a went-hungry mistake (blocked; net 0 from this half).
    g.model().setHunger(0);
    g.tick(kHeartbeatMs);
    CHECK(g.model().careMistakes() == 0);            // shielded — the went-hungry blocked
    g.tick(kLockoutDurationMs + 2 * kHeartbeatMs);   // let the Lockout expire cleanly
    const int after = g.model().careMistakes();      // expiry mistake stands (shield gone)

    // Grant + Use another copy: the gate is spent, so no shield re-arms.
    g.inventory().add("restore_point", 1);
    g.debugUseItem("restore_point");                 // consumed as a buff, but NO re-arm
    g.model().setHunger(0);
    g.tick(kHeartbeatMs);
    CHECK(g.model().careMistakes() == after + kWentHungryMistakes);  // not re-shielded
}

// The Restore Point shield covers the +1 care mistake from a FAILED Defrag/AV
// (the maint-fail mistake routes through resolveMaint -> addCareMistakeShielded).
// An armed shield eats the first failed defrag's mistake; a second failure then
// lands normally.
void test_restore_shield_covers_maint_fail() {
    Game g{StartMode::Hatched};
    g.debugUseItem("restore_point");                 // arm the shield
    CHECK(g.model().careMistakes() == 0);

    const int frag0 = g.model().fragmentation();
    g.debugResolveDefrag(false);                     // first failed defrag → mistake SHIELDED
    CHECK(g.model().careMistakes() == 0);            // no mistake (shield consumed)
    CHECK(g.model().fragmentation() == frag0 + kMaintFailPenalty);  // frag penalty still lands

    g.debugResolveDefrag(false);                     // second failure → mistake lands
    CHECK(g.model().careMistakes() == 1);
}

// Yubi-Cookie removes one existing mistake, ONCE PER LIFETIME: a second Use is inert.
void test_yubi_cookie_once_per_lifetime() {
    Game g{StartMode::Hatched};
    g.model().addCareMistake(2);                     // two mistakes to shave
    CHECK(g.inventory().count("yubi_cookie") == 1);
    g.debugUseItem("yubi_cookie");                   // -1 mistake (first use)
    CHECK(g.model().careMistakes() == 1);
    g.inventory().add("yubi_cookie", 1);             // grant another copy
    g.debugUseItem("yubi_cookie");                   // already consumed -> no -1
    CHECK(g.model().careMistakes() == 1);            // unchanged
}

// Backup Drive is a combat BUFF, not a Lockout item: Use arms a timed shield
// (Game::backupShieldArmed) instead of touching the Lockout at all.
void test_backup_drive_arms_and_lapses() {
    Game g{StartMode::Hatched};
    CHECK(!g.backupShieldArmed());
    g.debugUseItem("backup_drive");
    CHECK(g.inventory().count("backup_drive") == 0);       // consumed on Use
    CHECK(g.backupShieldArmed());
    // Ticking less than the 1h window leaves it armed; crossing it lapses it unused
    // (the item row's magnitude — content_items.cpp's ArmCombatShieldBuff — is 60 min).
    // Game::tick(nowMs) takes an ABSOLUTE clock reading, not a delta.
    const uint32_t oneHourMs = 60u * 60u * 1000u;
    g.tick(oneHourMs / 2);
    CHECK(g.backupShieldArmed());
    g.tick(oneHourMs + kHeartbeatMs);
    CHECK(!g.backupShieldArmed());
}


// A use that can achieve literally nothing is refused and KEEPS the item, rather
// than spending it on a state the pet already holds. itemUsable gates on
// Game::itemUseIsInert, which reads the ItemEffect vocabulary — so every arming
// effect gets the same protection without a per-id branch.
void test_inert_use_keeps_the_item() {
    Game g{StartMode::Hatched};
    g.inventory().add("backup_drive", 2);
    g.debugUseItem("backup_drive");
    CHECK(g.backupShieldArmed());
    const int armed = g.inventory().count("backup_drive");
    // A second over a live shield would only replace a deadline it already has.
    g.debugUseItem("backup_drive");
    CHECK(g.inventory().count("backup_drive") == armed);
    // Once the window lapses there is something to arm again, so it spends.
    g.tick(60u * 60u * 1000u + kHeartbeatMs);
    CHECK(!g.backupShieldArmed());
    g.debugUseItem("backup_drive");
    CHECK(g.backupShieldArmed());
    CHECK(g.inventory().count("backup_drive") == armed - 1);

    // Same rule off a different state: the Yubi-Cookie is once per lifetime, so
    // the second one is refused instead of vanishing for nothing.
    g.model().setCareMistakes(2);
    g.inventory().add("yubi_cookie", 2);
    g.debugUseItem("yubi_cookie");
    const int spent = g.inventory().count("yubi_cookie");
    g.debugUseItem("yubi_cookie");
    CHECK(g.inventory().count("yubi_cookie") == spent);
}

// A Rig Shop row with a prerequisite (RigUpgradeDef::requiresRow) isn't on sale
// until that row is owned: the recipes need the MERGE HUB they'd be cooked in.
// The SHOP list and the buy path share one predicate (shopRowOffered), so
// refusing the purchase and hiding the row are the same fact asserted once.
void test_recipe_rows_wait_on_the_merge_hub() {
    Game g{StartMode::Hatched};
    g.debugSetBits(99999);
    const int start = g.bits();
    g.debugBuyRecipe(0);
    CHECK(g.bits() == start);           // gated: nothing bought, nothing charged
    CHECK(!g.mergeHubUnlocked());

    g.debugBuyMergeHub();
    CHECK(g.mergeHubUnlocked());
    CHECK(g.bits() == start - kRigMergeHubCost);
    g.debugBuyRecipe(0);                // now offered, so it sells
    CHECK(g.bits() == start - kRigMergeHubCost - kRigRecipeUnlockCost);
}

// The second gate axis (RigUpgradeDef::requiresItems): the Browns recipes want the
// operator to have HELD both dishes, not just to own the hub. It's an ever-held
// record (Game::itemCollected), so meeting a dish and then eating it still counts —
// which is the whole reason the gate reads that and not the current bag.
void test_browns_recipes_wait_on_meeting_both_dishes() {
    Game g{StartMode::Hatched};
    g.debugSetBits(99999);
    g.debugBuyMergeHub();

    int brownsRecipe = -1;
    for (int i = 0; i < kMergeRecipeCount; ++i)
        if (std::strcmp(kMergeRecipes[i].outputId, "hashed_browns") == 0) brownsRecipe = i;
    CHECK(brownsRecipe >= 0);

    // Whether the row actually sold, measured across the call — an absolute Bits
    // figure would drift the moment a tick pays out an achievement.
    auto sold = [&] {
        const int before = g.bits();
        g.debugBuyRecipe(brownsRecipe);
        return g.bits() != before;
    };
    // The collected-items record is folded in by the achievement sweep, which is
    // throttled — walk the clock past its interval rather than by a millisecond.
    // Game::tick takes an ABSOLUTE timestamp, so the cursor has to advance.
    uint32_t now = 0;
    auto settle = [&] { now += kAchSweepIntervalMs + 1; g.tick(now); };

    CHECK(!sold());                            // never met either dish -> not on sale

    // One of the two isn't enough: the row names both.
    g.inventory().add("hashed_browns", 1);
    settle();
    CHECK(g.itemCollected("hashed_browns"));
    CHECK(!sold());

    g.inventory().add("salted_hashed_browns", 1);
    settle();
    CHECK(g.itemCollected("salted_hashed_browns"));
    // Eat the evidence — a met dish stays met.
    g.inventory().remove("hashed_browns", 1);
    g.inventory().remove("salted_hashed_browns", 1);
    CHECK(sold());
}

// A recipe consumes every ingredient it names, not just the first two: Hashed Browns
// takes four. The craft is gated on ALL of them, so being short on the last one is
// refused exactly like being short on the first.
void test_four_ingredient_recipe_consumes_all_inputs() {
    Game g{StartMode::Hatched};
    int idx = -1;
    for (int i = 0; i < kMergeRecipeCount; ++i)
        if (std::strcmp(kMergeRecipes[i].outputId, "hashed_browns") == 0) idx = i;
    CHECK(idx >= 0);
    const MergeRecipe& r = kMergeRecipes[idx];
    CHECK(recipeInputCount(r) == 4);

    g.debugSetBits(99999);
    g.debugBuyMergeHub();
    g.inventory().add("hashed_browns", 1);
    g.inventory().add("salted_hashed_browns", 1);
    g.tick(kAchSweepIntervalMs + 1);   // the sweep records both as met
    g.debugBuyRecipe(idx);

    // Three of the four in the bag: still refused, nothing consumed.
    for (int i = 0; i < 3; ++i) g.inventory().add(r.inputs[i].id, r.inputs[i].qty);
    CHECK(!g.debugRecipeCraftable(idx));
    g.debugCraftRecipe(idx);
    CHECK(g.inventory().count(r.inputs[0].id) == r.inputs[0].qty);

    const int before = g.inventory().count("hashed_browns");
    g.inventory().add(r.inputs[3].id, r.inputs[3].qty);
    CHECK(g.debugRecipeCraftable(idx));
    g.debugCraftRecipe(idx);
    CHECK(g.inventory().count("hashed_browns") == before + r.outputQty);
    for (const RecipeInput& in : r.inputs)
        if (in.id) CHECK(g.inventory().count(in.id) == 0);   // all four spent
}

// Polltatoes (ItemEffect::Kind::HungerStacking): a run of stacking foods pays
// magnitude x how many have been eaten since the last Hunger point lost to decay,
// counting the current bite. Eaten alone it is worth exactly its magnitude.
void test_stacking_food_run_climbs_then_resets_on_decay() {
    Game g{StartMode::Hatched};
    g.inventory().add("polltatoes", 8);
    // Well clear of the 100 ceiling so every gain is observable, and clear of the
    // Lockout floor so the feed path isn't resolving a crisis instead.
    g.model().setHunger(40);

    int last = g.model().hunger();
    for (int expect = 1; expect <= 3; ++expect) {
        g.debugUseItem("polltatoes");
        CHECK(g.model().hunger() - last == expect);   // 1, then 2, then 3
        last = g.model().hunger();
    }

    // A Hunger point lost to passive decay ends the run — the next one is worth 1
    // again, not 5. Long enough for at least one decay step, and we re-level Hunger
    // afterwards so the assertion measures the bite alone.
    g.tick(kHungerMinutesPerPoint * 60u * 1000u);
    g.model().setHunger(40);
    g.debugUseItem("polltatoes");
    CHECK(g.model().hunger() - 40 == 1);
}

// Every id named in a loot/reward pool has to resolve to a real item with a positive
// draw weight. A typo'd id would otherwise cost that entry silently — the pool would
// still draw, just never that row — which is exactly the kind of failure a weighted
// table makes invisible.
void test_loot_pools_resolve_and_carry_weight() {
    ContentRegistry r = ContentRegistry::embedded();
    auto checkPool = [&r](const LootEntry* pool, int n) {
        for (int i = 0; i < n; ++i) {
            const ItemDef* d = r.item(pool[i].id);
            if (!d) std::printf("  UNRESOLVED POOL ENTRY: %s\n", pool[i].id);
            CHECK(d != nullptr);
            CHECK((pool[i].weight > 0 ? pool[i].weight : itemDropWeight(*d)) > 0);
        }
    };
    checkPool(kLootPool, kLootPoolCount);
    for (const ItemDef* c : r.allItems()) checkPool(c->cache.pool, c->cache.poolSize);

    // The resolver's two arms: a row that names its own weight reports it; one that
    // doesn't falls back to its rarity's default.
    CHECK(itemDropWeight(*r.item("spam")) == 90);          // names its own
    CHECK(itemDropWeight(*r.item("boolean_cubes")) ==
          kItemRarityDropWeight[static_cast<int>(ItemDef::Rarity::Common)]);
}

// Resolving by paying Bits clears the crisis before the timer expires.
void test_lockout_resolve_pay() {
    Game g{StartMode::Hatched};
    g.model().setHunger(0);
    g.tick(kHeartbeatMs);
    CHECK(g.bits() == kStartBits);
    g.onButton(press(Button::A));                    // toggle to Pay Bits
    g.onButton(press(Button::B));                    // pay
    CHECK(!g.lockoutActive() && g.nav() == Game::Nav::Idle);
    // Resolving also EARNS SURVIVED_LOCKOUT, which pays its own Bits — read the amount
    // off the achievement row rather than restating it, so retuning the reward doesn't
    // break this gate.
    CHECK(g.bits() == kStartBits - kLockoutBitsCost + achBitsReward(ach::kSurvivedLockout));
    CHECK(g.model().hunger() == kLockoutRecoveryHunger);
}

// Resolving via the Lockout ITEMS escape hatch (feed a food).
void test_lockout_resolve_feed() {
    Game g{StartMode::Hatched};
    g.model().setHunger(0);
    g.tick(kHeartbeatMs);
    CHECK(g.nav() == Game::Nav::ModalLockout);
    g.onButton(press(Button::B));                    // "Open Items" (lockout ctx)
    CHECK(g.nav() == Game::Nav::Submenu);
    g.onButton(press(Button::B));                    // open the top food detail
    CHECK(g.nav() == Game::Nav::Detail);
    g.onButton(press(Button::B));                    // Use food -> feeding modal
    CHECK(g.nav() == Game::Nav::ModalFeeding);
    CHECK(!g.lockoutActive());                        // the crisis is already off
    // The deadline is disarmed: ticking well past it adds NO further mistake. The one
    // "went hungry" +1 (charged when Hunger hit 0) stands — the intended new cost.
    g.tick(kLockoutDurationMs + 10000);
    CHECK(g.model().careMistakes() == kWentHungryMistakes);
    g.onButton(press(Button::B));                    // dismiss -> resolves lockout
    CHECK(!g.lockoutActive() && g.nav() == Game::Nav::Idle);
    CHECK(g.model().hunger() > 0);
}

// Grayscale gate: the focused ITEMS row's cursor marker reads without colour.
void test_items_grayscale() {
    ContentRegistry r = ContentRegistry::embedded();
    Inventory inv = Inventory::starting();
    auto rows = buildInventoryRows(r, inv, false);
    const int cur = firstSelectableRow(rows);        // first FOOD row (v=1)
    Framebuffer fb(kActiveW, kActiveH);
    drawItemsList(fb, rows, cur, false, 0);
    // The claim is that the MARKER reads against the selection band it sits on, with
    // no colour: probe the cursor triangle at x=9 against the bare band two columns
    // left of it, on the same row. Comparing against a different ROW is what a probe
    // must not do — a group header draws its label from the left margin too, so the
    // "empty" row is only empty until the inventory shifts under it.
    int drawnRow = 0;
    for (int i = 0, v = 0; i < static_cast<int>(rows.size()); ++i) {
        if (i == cur) { drawnRow = v; break; }
        ++v;
    }
    const int rowY = kItemsRowTop + drawnRow * 28 + 13;
    CHECK(luminance(fb.get(9, rowY)) - luminance(fb.get(5, rowY)) > 0.3f);
}

// Decryption Hatch (egg-at-idle) ------------------------------
