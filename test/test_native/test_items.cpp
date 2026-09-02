// test_items.cpp — native gates for ITEMS, feeding, MAINT/defrag and the Lockout.
//
// One slice of the native suite; see test_gates.h for the shared fixtures and
// test_main.cpp for the roster that runs these. A save-vNN gate sits with the
// feature whose field it migrates, not in a migrations pile of its own.
#include "test_gates.h"

void test_inventory() {
    Inventory inv;
    CHECK(inv.count("dyno_nuggets") == 0 && !inv.has("dyno_nuggets"));
    inv.add("dyno_nuggets", 2);
    inv.add("dyno_nuggets");                          // default +1
    CHECK(inv.count("dyno_nuggets") == 3 && inv.has("dyno_nuggets"));
    CHECK(inv.remove("dyno_nuggets", 1));
    CHECK(inv.count("dyno_nuggets") == 2);
    CHECK(!inv.remove("dyno_nuggets", 5));            // insufficient -> no-op
    CHECK(inv.count("dyno_nuggets") == 2);

    Inventory s = Inventory::starting();
    CHECK(s.count("dyno_nuggets") == 3 && s.count("backup_drive") == 1);
    CHECK(s.count("boot_accelerator") == 1);         // egg accelerator
    // The Decryptogram is FOUND, never issued — it buys a quote board, and the starting
    // kit does not hand one over.
    CHECK(s.count("decryptogram") == 0);
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
    Inventory inv = Inventory::starting();           // 6 items across 4 groups
    auto rows = buildInventoryRows(r, inv, false);

    int headers = 0, items = 0;
    for (const auto& row : rows) (row.header ? headers : items)++;
    // FOOD/INGREDIENTS/BUFFS/QUEST: tortilla_chip is both a snack and a Nachos
    // ingredient, so it splits the starting shelf's Food stack into its own group
    // alongside dyno_nuggets's.
    CHECK(items == 6 && headers == 4);
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

    const int q0 = g.inventory().count("dyno_nuggets");
    g.onButton(press(Button::B));                    // Use -> feeding modal
    CHECK(g.nav() == Game::Nav::ModalFeeding);
    CHECK(g.model().hunger() == 80);                 // 40 + 40 restore
    CHECK(g.inventory().count("dyno_nuggets") == q0 - 1);
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
    tapC(g);
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
    tapC(g);                      // action -> MAINT list
    tapC(g);                      // list -> carousel
    tapC(g);                      // carousel -> idle habitat

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
    g.inventory().add("yubi_cookie", 1);
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

    // Same rule off a different state: the Restore Point's shield is once per lifetime,
    // so the second one is refused instead of vanishing for nothing.
    g.inventory().add("restore_point", 2);
    g.debugUseItem("restore_point");
    const int spent = g.inventory().count("restore_point");
    g.debugUseItem("restore_point");
    CHECK(g.inventory().count("restore_point") == spent);

    // ...and the line the other way. The Yubi-Cookie carries the same once-per-lifetime
    // mistake wipe, but it is a FOOD: a spent cookie is still a cookie, so the second one
    // is EATEN for its Hunger and Happiness rather than refused. Nothing that fills a
    // vital can ever be inert (Game::itemUseIsInert).
    g.model().setCareMistakes(2);
    g.inventory().add("yubi_cookie", 2);
    g.debugUseItem("yubi_cookie");
    CHECK(g.model().careMistakes() == 1);            // the once-per-life half fired
    g.debugUseItem("yubi_cookie");
    CHECK(g.model().careMistakes() == 1);            // ...once, and only once
    CHECK(g.inventory().count("yubi_cookie") == 0);  // but both were eaten
}

// --- The USB port: the branch overrides and the soak pair ---------------------
//
// Four devices that all steer the pet's next evolution, and the rules they share are
// what these gates hold: ONE branch at a time, most recently plugged in wins, and a soak
// locks the port against the whole family until the boundary it stretched arrives.

// Bad-USB overrules a spotless care record; Signed-USB overrules a ruined one. The
// branch is what the item says, not what the raise earned — which is the whole item.
void test_branch_override_usbs_overrule_the_care_record() {
    // Perfect care, Bad-USB in the port -> the BAD daemon.
    { Game g{StartMode::Hatched, "malbear"};
      CHECK(g.model().careBranch() == CareBranch::Good);
      // Two, so that spending one leaves the ITEMS list standing and the pet at idle —
      // debugUseItem drives the real Use path, which walks back out of an emptied row.
      g.inventory().add("bad_usb", 2);
      g.debugUseItem("bad_usb");
      CHECK(g.inventory().count("bad_usb") == 1);              // consumed on Use
      CHECK(g.evolveBranchOverride() == BranchOverride::Bad);
      CHECK(g.effectiveCareBranch() == CareBranch::Bad);       // ...whatever the record says
      CHECK(g.model().careBranch() == CareBranch::Good);       // which is untouched
      g.debugTriggerEvolution();
      uint32_t t = 0; advanceToReveal(g, t);
      g.onButton(press(Button::B));
      CHECK(g.pet() && std::strcmp(g.pet()->id, "berserkernel") == 0);
      // Spent at the boundary it steered: the next stage reads the care budget again.
      CHECK(g.evolveBranchOverride() == BranchOverride::None); }

    // ...and the inverse, off a care record that had already lost the good branch.
    { Game g{StartMode::Hatched, "malbear"};
      g.model().setCareMistakes(kCareGoodMax + 1);
      CHECK(g.model().careBranch() == CareBranch::Bad);
      g.inventory().add("signed_usb", 2);
      g.debugUseItem("signed_usb");
      CHECK(g.effectiveCareBranch() == CareBranch::Good);
      g.debugTriggerEvolution();
      uint32_t t = 0; advanceToReveal(g, t);
      g.onButton(press(Button::B));
      CHECK(g.pet() && std::strcmp(g.pet()->id, "bruinforce") == 0); }

    // Neither reaches around a DYING pet: 5/5 is Critical System Failure, not a branch,
    // so an armed override answers Dying like everything else and the pet is still on
    // its way to the failure the neglect earned.
    { Game g{StartMode::Hatched, "malbear"};
      g.inventory().add("signed_usb", 1);
      g.debugUseItem("signed_usb");
      CHECK(g.evolveBranchOverride() == BranchOverride::Good);
      g.model().setCareMistakes(kCareDying);
      CHECK(g.effectiveCareBranch() == CareBranch::Dying); }
}

// ONE slot. Plugging the opposite device in replaces what was there — the most recently
// used is the one that fires — and plugging in the SAME direction twice buys nothing, so
// the second is refused and stays in the bag (Game::itemUseIsInert).
void test_branch_override_holds_one_device_at_a_time() {
    Game g{StartMode::Hatched, "malbear"};
    g.inventory().add("bad_usb", 2);
    g.inventory().add("signed_usb", 1);

    g.debugUseItem("bad_usb");
    CHECK(g.evolveBranchOverride() == BranchOverride::Bad);
    // A second Bad-USB over a slot already pointing Bad: nothing to achieve, kept.
    g.debugUseItem("bad_usb");
    CHECK(g.inventory().count("bad_usb") == 1);
    // The opposite device always does something — it takes the slot.
    g.debugUseItem("signed_usb");
    CHECK(g.evolveBranchOverride() == BranchOverride::Good);
    CHECK(g.inventory().count("signed_usb") == 0);
    // ...and now the Bad-USB has work to do again, so it spends.
    g.debugUseItem("bad_usb");
    CHECK(g.evolveBranchOverride() == BranchOverride::Bad);
    CHECK(g.inventory().count("bad_usb") == 0);
}

// The soak's two halves are ONE number: the stage's evolution dwell runs x{soak} longer
// and every XP award pays x{soak} more. Same pet, arriving later and further along.
void test_soak_usb_stretches_the_clock_and_pays_the_xp() {
    Game g{StartMode::Hatched, "paypup"};             // a Process pet
    CHECK(g.pet() && g.pet()->stage == Stage::Process);
    const uint32_t plain = g.evolveRemainMs();
    CHECK(plain == kEvolveProcessToScriptMs);

    g.inventory().add("sandbox_usb", 1);
    g.debugUseItem("sandbox_usb");
    CHECK(g.evolveSoakFactor() == 2);
    CHECK(g.evolveRemainMs() == plain * 2);           // the wait...

    const int lvl0 = g.combatLevel(), xp0 = g.combatXp();
    g.debugAddCombatXp(10);
    const int soaked = g.combatXp() - xp0;
    CHECK(lvl0 == g.combatLevel());                   // no level crossed, so the bank is the award
    CHECK(soaked == 20);                              // ...and the pay, by the same factor

    // The Epic device is the same mechanic twice as deep, and the ladder is asserted off
    // the ROWS rather than off two names here: whatever carries a soak is a USB, and the
    // deepest one on the shelf is twice the shallowest.
    int shallow = 0, deep = 0;
    for (const ItemDef* d : ContentRegistry::embedded().allItems()) {
        const int f = itemEvolveSoakFactor(*d);
        if (f <= 0) continue;
        CHECK(itemIsUsb(*d));
        if (!shallow || f < shallow) shallow = f;
        if (f > deep) deep = f;
    }
    CHECK(shallow == 2 && deep == 2 * shallow);
}

// A soak holds the port SHUT. Nothing else in the family goes in until it is spent at
// the boundary it stretched — not a divert, not a branch override, not a second soak.
void test_soak_usb_locks_the_port_until_the_boundary() {
    Game g{StartMode::Hatched, "paypup"};
    g.inventory().add("sandbox_usb", 2);   // two, so the ITEMS list survives the spend
    for (const char* id : {"hypervisor_usb", "ambig_usb", "bad_usb", "signed_usb"})
        g.inventory().add(id, 1);
    g.debugUseItem("sandbox_usb");
    CHECK(g.evolveSoakFactor() == 2);

    // Every other USB is refused and KEPT — including the deeper soak, which is what
    // makes the choice of factor a decision about the stage rather than a shopping list.
    for (const char* id : {"hypervisor_usb", "ambig_usb", "bad_usb", "signed_usb"}) {
        g.debugUseItem(id);
        CHECK(g.inventory().count(id) == 1);
    }
    CHECK(g.evolveSoakFactor() == 2);
    CHECK(g.evolveBranchOverride() == BranchOverride::None);

    // The boundary empties the port: the next stage counts down at its own pace, and
    // the family is usable again.
    g.debugTriggerEvolution();
    uint32_t t = 0; advanceToReveal(g, t);
    g.onButton(press(Button::B));
    CHECK(g.evolveSoakFactor() == 1);
    g.debugUseItem("bad_usb");
    CHECK(g.evolveBranchOverride() == BranchOverride::Bad);
}

// The Epic soak reaches one stage further than the Rare one, and pays for the reach at
// the till rather than in the benefit: on a Script the XP is the same x4 the row promises
// and the clock is DOUBLE that. A Script's boundary is the branch a whole raise was aimed
// at, so stretching the last stage before an ending is the expensive thing to do.
void test_late_soak_reaches_script_at_double_the_clock() {
    // The Rare one does not reach it at all.
    { Game g{StartMode::Hatched, "malbear"};              // a Script pet
      CHECK(g.pet()->stage == Stage::Script);
      g.inventory().add("sandbox_usb", 1);
      g.debugUseItem("sandbox_usb");
      CHECK(g.inventory().count("sandbox_usb") == 1);     // refused, kept
      CHECK(g.evolveSoakFactor() == 1); }

    // On a PROCESS pet the Epic one is an ordinary x4: clock and XP move together.
    { Game g{StartMode::Hatched, "paypup"};
      g.inventory().add("hypervisor_usb", 2);
      g.debugUseItem("hypervisor_usb");
      CHECK(g.evolveSoakFactor() == 4);
      CHECK(g.evolveRemainMs() == kEvolveProcessToScriptMs * 4);
      const int xp0 = g.combatXp();
      g.debugAddCombatXp(10);
      CHECK(g.combatXp() - xp0 == 40); }

    // On a SCRIPT pet it goes in, pays the same x4 XP, and charges x8 on the clock.
    { Game g{StartMode::Hatched, "malbear"};
      CHECK(g.evolveRemainMs() == kEvolveScriptToDaemonMs);
      g.inventory().add("hypervisor_usb", 2);
      g.debugUseItem("hypervisor_usb");
      CHECK(g.evolveSoakFactor() == 4);
      CHECK(g.evolveRemainMs() == kEvolveScriptToDaemonMs * 8);
      const int xp0 = g.combatXp();
      g.debugAddCombatXp(10);
      CHECK(g.combatXp() - xp0 == 40); }   // the BENEFIT is unchanged — only the wait grew
}

// The Halt-USB stops the pet evolving outright: not a longer wait but no arrival, which
// is what parks a pet at a stage on purpose. It is the one device a boundary never
// consumes, because none comes while it is in.
void test_halt_usb_stops_evolution_outright() {
    Game g{StartMode::Hatched, "paypup"};
    g.inventory().add("halt_usb", 2);
    g.debugUseItem("halt_usb");
    CHECK(g.evolveHoldArmed());
    // STAT's own EVOLVE readout says MAX, the same thing it says at a terminus and true
    // in the same sense — the BUFFS page and the habitat badge carry the WHY.
    CHECK(!g.hasNextEvolution());
    CHECK(g.evolveRemainMs() == 0);

    // Wait out the whole dwell and then some: the boundary never fires and the pet is
    // still the Process it was parked as. Vitals are topped up along the way, since five
    // days of unattended decay would end the run in a Lockout rather than at the boundary
    // this gate is about.
    uint32_t t = 0;
    for (int i = 0; i < 40; ++i) {
        g.model().setHunger(90);
        g.model().setHappiness(90);
        g.model().setCareMistakes(0);
        g.tick(t += kEvolveProcessToScriptMs / 8);
    }
    CHECK(g.nav() == Game::Nav::Idle);
    CHECK(g.pet() && std::strcmp(g.pet()->id, "paypup") == 0);
    CHECK(g.evolveHoldArmed());          // ...and nothing consumed it

    // Pull it, and the boundary that was waiting all along fires on the next beat.
    g.inventory().add("eject_usb", 2);
    g.debugUseItem("eject_usb");
    CHECK(!g.evolveHoldArmed());
    CHECK(g.hasNextEvolution());
    g.tick(t += kHeartbeatMs);
    CHECK(g.nav() == Game::Nav::ModalEvolve);
}

// The Eject-USB is the family's undo: it goes in over a LOCKED port (which nothing else
// does) and drops whatever was armed, whichever device armed it. On an EMPTY port it
// achieves nothing, so it is refused and kept rather than spent.
void test_eject_usb_pulls_whatever_is_armed() {
    Game g{StartMode::Hatched, "paypup"};
    g.inventory().add("eject_usb", 4);

    // Empty port: refused, kept.
    CHECK(!g.usbPortOccupied());
    g.debugUseItem("eject_usb");
    CHECK(g.inventory().count("eject_usb") == 4);

    // Over a swappable device (a branch override).
    g.inventory().add("bad_usb", 2);
    g.debugUseItem("bad_usb");
    CHECK(g.usbPortOccupied() && !g.usbPortLocked());
    g.debugUseItem("eject_usb");
    CHECK(g.evolveBranchOverride() == BranchOverride::None);
    CHECK(g.inventory().count("eject_usb") == 3);

    // Over a LOCKING device — the case that makes it more than a convenience, since a
    // soak refuses every other USB in the family while it runs.
    g.inventory().add("sandbox_usb", 2);
    g.debugUseItem("sandbox_usb");
    CHECK(g.usbPortLocked());
    g.debugUseItem("bad_usb");                       // still refused...
    CHECK(g.evolveBranchOverride() == BranchOverride::None);
    g.debugUseItem("eject_usb");                     // ...but the eject is not
    CHECK(g.evolveSoakFactor() == 1);
    CHECK(!g.usbPortOccupied());
    CHECK(g.evolveRemainMs() == kEvolveProcessToScriptMs);
    // ...and now the port takes anything again.
    g.debugUseItem("bad_usb");
    CHECK(g.evolveBranchOverride() == BranchOverride::Bad);
}

// A device is plugged into the RIG, not frozen into the creature — so a rack swap empties
// the port rather than handing the incoming pet an ending the outgoing one paid for, or
// carrying a Process-only soak into a stage its own gate forbids.
void test_usb_port_empties_on_a_rack_swap() {
    Game g{StartMode::Hatched, "paypup"};
    g.inventory().add("sandbox_usb", 2);
    g.debugUseItem("sandbox_usb");
    CHECK(g.evolveSoakFactor() == 2);

    // Store the soaked Paypup (a fresh egg lands), then deploy it straight back out —
    // driven through ARCH the way a player would, since that is the seam being tested.
    enterSubmenuId(g, SubmenuId::Arch);
    g.onButton(press(Button::B));                          // open active record (Store)
    g.onButton(press(Button::B));                          // Store -> confirm (default Cancel)
    g.onButton(press(Button::A));                          // Cancel -> Confirm
    g.onButton(press(Button::B));                          // commit Store
    pickFirstEggLine(g);
    CHECK(g.rackCount() == 1 && g.inEggPhase());
    enterSubmenuId(g, SubmenuId::Arch);
    g.onButton(press(Button::A));                          // focus the stored row
    g.onButton(press(Button::B));                          // open stored record (Deploy)
    g.onButton(press(Button::B));                          // Deploy -> confirm
    g.onButton(press(Button::A));                          // -> Confirm
    g.onButton(press(Button::B));                          // commit Deploy
    CHECK(g.pet() && std::strcmp(g.pet()->id, "paypup") == 0);
    CHECK(g.evolveSoakFactor() == 1);
    CHECK(g.evolveBranchOverride() == BranchOverride::None);
    CHECK(g.evolveRemainMs() == kEvolveProcessToScriptMs);
}

// A soak is a decision about the PROCESS stage, so it only goes in there — an egg has no
// evolution clock to stretch, and a Script's next boundary is the branch itself.
void test_soak_usb_is_process_stage_only() {
    { Game g{StartMode::Hatched, "malbear"};          // a Script pet
      CHECK(g.pet() && g.pet()->stage == Stage::Script);
      g.inventory().add("sandbox_usb", 1);
      g.debugUseItem("sandbox_usb");
      CHECK(g.inventory().count("sandbox_usb") == 1);  // refused, kept
      CHECK(g.evolveSoakFactor() == 1); }

    // The branch overrides are NOT stage-gated: arming one on a Process pet is how a
    // player commits to an ending a stage before the branch is reached.
    { Game g{StartMode::Hatched, "paypup"};
      g.inventory().add("bad_usb", 1);
      g.debugUseItem("bad_usb");
      CHECK(g.evolveBranchOverride() == BranchOverride::Bad); }
}

// Both slots are per-pet state that rides the save, and both come back clamped: an
// unknown branch number reads as no override, and the factor is floored at 1 — it
// multiplies the evolution clock AND every XP award, so a 0 on the wire would otherwise
// hand back a pet that can neither evolve nor learn.
void test_save_v60_usb_port_roundtrip() {
    SaveData a;
    std::strcpy(a.activeId, "paypup");
    a.generation = 1;
    a.evolveBranchOverride = static_cast<uint8_t>(BranchOverride::Bad);
    a.evolveSoakFactor = 4;
    SaveData out;
    CHECK(deserializeSave(serializeSave(a), out));
    CHECK(out.evolveBranchOverride == static_cast<uint8_t>(BranchOverride::Bad));
    CHECK(out.evolveSoakFactor == 4);

    // A blob that never carried the tail describes an empty port, not a stalled pet.
    SaveData fresh;
    CHECK(fresh.evolveBranchOverride == 0);
    CHECK(fresh.evolveSoakFactor == 1);

    // ...and the round trip through the live game, over a real store: an armed port
    // survives a power cycle, because a soak the player is halfway through paying for
    // must not be handed back by a reboot.
    MemSaveStore store;
    {
        Game g{StartMode::Hatched, "paypup", &store};
        g.inventory().add("hypervisor_usb", 1);
        g.inventory().add("bad_usb", 1);
        g.debugUseItem("hypervisor_usb");
        g.debugUseItem("bad_usb");                                 // refused: the port is shut
        CHECK(g.evolveSoakFactor() == 4);
        CHECK(g.evolveBranchOverride() == BranchOverride::None);
        g.tick(kSaveAutosaveMs + kHeartbeatMs);
    }
    {
        Game g{StartMode::Hatched, "paypup", &store};
        CHECK(g.evolveSoakFactor() == 4);
        CHECK(g.inventory().count("bad_usb") == 1);   // refused, so still in the bag
    }
}

// No Bits path reaches a recipe. The SHOP sells the MERGE HUB and nothing that
// happens inside it — a recipe is won off a Decryptogram, so the only thing a full
// wallet can do about cooking is buy the kitchen.
void test_recipes_are_not_for_sale_at_any_price() {
    Game g{StartMode::Hatched};
    g.debugSetBits(99999);
    for (int i = 0; i < kRigUpgradeCount; ++i)
        CHECK(std::strncmp(kRigUpgrades[i].id, "recipe_", 7) != 0);
    // Buy out the whole storefront; the kitchen stays empty.
    for (int i = 0; i < kRigUpgradeCount; ++i) g.debugBuyRigRow(i);
    CHECK(g.mergeHubUnlocked());
    for (int i = 0; i < kMergeRecipeCount; ++i) CHECK(!g.debugRecipeOwned(i));
}

// A recipe can't be handed over before there is anywhere to cook it: the prize ladder
// steps over one whose MERGE HUB hasn't been built, and comes back to it later.
void test_recipes_wait_on_the_merge_hub() {
    Game g{StartMode::Hatched};
    for (int i = 0; i < kMergeRecipeCount; ++i) CHECK(!g.debugRecipeGrantable(i));
    CHECK(!g.mergeHubUnlocked());

    g.debugSetBits(99999);
    g.debugBuyMergeHub();
    CHECK(g.mergeHubUnlocked());
    CHECK(g.debugRecipeGrantable(0));   // the hub is what it was waiting on
}

// The second gate (MergeRecipe::requiresItems): the Browns recipes want the operator
// to have HELD both dishes, not just to own the hub — Moor-to-Moor stocks them, so the
// ladder won't teach the method for a dish nobody has met. It's an ever-held record
// (Game::itemCollected), so meeting a dish and then eating it still counts — which is
// the whole reason the gate reads that and not the current bag.
void test_browns_recipes_wait_on_meeting_both_dishes() {
    Game g{StartMode::Hatched};
    g.debugSetBits(99999);
    g.debugBuyMergeHub();

    int brownsRecipe = -1;
    for (int i = 0; i < kMergeRecipeCount; ++i)
        if (std::strcmp(kMergeRecipes[i].outputId, "hashed_browns") == 0) brownsRecipe = i;
    CHECK(brownsRecipe >= 0);

    // The collected-items record is folded in by the achievement sweep, which is
    // throttled — walk the clock past its interval rather than by a millisecond.
    // Game::tick takes an ABSOLUTE timestamp, so the cursor has to advance.
    uint32_t now = 0;
    auto settle = [&] { now += kAchSweepIntervalMs + 1; g.tick(now); };

    CHECK(!g.debugRecipeGrantable(brownsRecipe));   // never met either dish

    // One of the two isn't enough: the row names both.
    g.inventory().add("hashed_browns", 1);
    settle();
    CHECK(g.itemCollected("hashed_browns"));
    CHECK(!g.debugRecipeGrantable(brownsRecipe));

    g.inventory().add("salted_hashed_browns", 1);
    settle();
    CHECK(g.itemCollected("salted_hashed_browns"));
    // Eat the evidence — a met dish stays met.
    g.inventory().remove("hashed_browns", 1);
    g.inventory().remove("salted_hashed_browns", 1);
    CHECK(g.debugRecipeGrantable(brownsRecipe));
}

// The recipes a device knows survive a power cycle — they are player-level, like the
// rig levels beside them, and a lifecycle doesn't cost the operator their cooking.
void test_recipes_persist() {
    MemSaveStore store;
    {
        Game g{StartMode::Hatched, "malbear", &store};
        g.debugWinRecipe(0);
        g.debugWinRecipe(3);
        g.tick(kSaveAutosaveMs + kHeartbeatMs);   // autosave
    }
    Game g2(StartMode::Hatched, "paypup", &store);
    CHECK(g2.debugRecipeOwned(0));
    CHECK(g2.debugRecipeOwned(3));
    CHECK(!g2.debugRecipeOwned(1));
    CHECK(!g2.debugRecipeOwned(2));
}

// v51 — the owned-recipe set outgrew the u32 it used to be. A recipe whose wire is at
// or past 32 is exactly the case the old field could not express, so it is the one worth
// round-tripping; and the u32 still has to carry the low wires, because a device that
// rolls back to a v31..v50 build must keep the recipes that build can name.
void test_recipes_past_the_legacy_mask_persist() {
    int lowIdx = -1, highIdx = -1;
    for (int i = 0; i < kMergeRecipeCount; ++i) {
        if (kMergeRecipes[i].wire < 32 && lowIdx < 0) lowIdx = i;
        if (kMergeRecipes[i].wire >= 32 && highIdx < 0) highIdx = i;
    }
    CHECK(lowIdx >= 0 && highIdx >= 0);          // the table has both sides of the line

    MemSaveStore store;
    {
        Game g{StartMode::Hatched, "malbear", &store};
        g.debugWinRecipe(lowIdx);
        g.debugWinRecipe(highIdx);
        g.tick(kSaveAutosaveMs + kHeartbeatMs);
    }
    Game g2(StartMode::Hatched, "paypup", &store);
    CHECK(g2.debugRecipeOwned(lowIdx));
    CHECK(g2.debugRecipeOwned(highIdx));         // the bit the u32 had no room for

    // What the blob itself says: the full set in the bitset, the low wires mirrored into
    // the legacy field, and nothing above wire 31 pretending to fit there.
    SaveData d{};
    CHECK(deserializeSave(store.bytes(), d));
    const int lowWire = kMergeRecipes[lowIdx].wire;
    CHECK((d.recipesUnlocked & (1u << lowWire)) != 0);
    CHECK(static_cast<int>(d.recipeOwned.size()) == kMergeRecipeWireBytes);
    const int highWire = kMergeRecipes[highIdx].wire;
    CHECK((d.recipeOwned[highWire / 8] & (1u << (highWire % 8))) != 0);
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
    g.debugWinRecipe(idx);

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

// TIRAMISUDO IS THE ONE FOOD THAT UPGRADES THE PET EATING IT. The first helping
// permanently shortens this pet's Bandwidth regen; every helping after it is an
// ordinary (very good) pudding that tops the pool up instead. Both halves are asserted
// here because the second is what stops the upgrade from being farmable.
void test_tiramisudo_upgrades_once_then_feeds() {
    Game g{StartMode::Hatched};
    CHECK(!g.bandwidthRegenUpgraded());
    CHECK(g.bandwidthRegenMinutes() == kBandwidthRegenMinutesPerPoint);

    g.inventory().add("tiramisudo", 2);
    g.debugUseItem("tiramisudo");
    CHECK(g.bandwidthRegenUpgraded());
    // The shave is the magnitude on the item's own row, not a number this test knows.
    const ItemDef* d = ContentRegistry::embedded().item("tiramisudo");
    CHECK(d);
    int shave = 0;
    for (const ItemEffect& e : d->effects)
        if (e.kind == ItemEffect::Kind::BandwidthRegenBonusMin) shave = e.magnitude;
    CHECK(shave > 0);
    CHECK(g.bandwidthRegenMinutes() == kBandwidthRegenMinutesPerPoint - shave);

    // A second helping doesn't stack the upgrade — the pet already has root.
    const uint32_t after = g.bandwidthRegenMinutes();
    g.debugUseItem("tiramisudo");
    CHECK(g.bandwidthRegenMinutes() == after);
}

// The shave is real time, not a readout: the regen loop hands back a point sooner for
// an upgraded pet than the shared interval would.
void test_tiramisudo_regen_actually_runs_faster() {
    Game g{StartMode::Hatched};
    g.debugSpendBandwidth(1);
    const int low = g.bandwidth();
    CHECK(low < g.bandwidthMax());

    g.inventory().add("tiramisudo", 1);
    g.debugUseItem("tiramisudo");
    // Wind the clock to just past the UPGRADED interval but short of the shared one,
    // so only a pet that was actually upgraded has regenerated by now. (The pudding's
    // own +1 Bandwidth already landed above, hence measuring from here.)
    const int before = g.bandwidth();
    const uint32_t upgraded = g.bandwidthRegenMinutes() * 60u * 1000u;
    CHECK(upgraded < kBandwidthRegenMinutesPerPoint * 60u * 1000u);
    g.tick(upgraded + 1000);
    CHECK(g.bandwidth() > before || before == g.bandwidthMax());
}

// The upgrade belongs to the CREATURE, so the rack keeps it: a pet stored and deployed
// again comes back rooted, and the pet that took its place does not inherit it.
void test_tiramisudo_upgrade_survives_the_rack() {
    Game g{StartMode::Hatched, "paypup"};
    g.inventory().add("tiramisudo", 1);
    g.debugUseItem("tiramisudo");
    CHECK(g.bandwidthRegenUpgraded());
    // Using a food parks on the feeding modal; the ARCH walk below starts from the
    // carousel, so back out first. Bounded, so a nav change can't hang the gate.
    for (int i = 0; i < 4 && g.nav() != Game::Nav::Idle; ++i)
        tapC(g);

    g.debugSeedRack("cryptoshell");
    enterSubmenuId(g, SubmenuId::Arch);
    g.onButton(press(Button::A));      // focus the stored pet
    g.onButton(press(Button::B));      // open its record (Deploy)
    g.onButton(press(Button::B));      // -> confirm
    g.onButton(press(Button::A));      // -> Confirm
    g.onButton(press(Button::B));      // commit: CryptoShell in, Paypup frozen
    CHECK(g.pet() && std::strcmp(g.pet()->id, "cryptoshell") == 0);
    CHECK(!g.bandwidthRegenUpgraded());          // the incoming pet was never fed one

    // Swap back: Paypup returns with what it ate.
    enterSubmenuId(g, SubmenuId::Arch);
    g.onButton(press(Button::A));
    g.onButton(press(Button::B));
    g.onButton(press(Button::B));
    g.onButton(press(Button::A));
    g.onButton(press(Button::B));
    CHECK(g.pet() && std::strcmp(g.pet()->id, "paypup") == 0);
    CHECK(g.bandwidthRegenUpgraded());
}

// v50 — the upgrade round-trips for the active pet AND for a pet on the shelf.
void test_save_v50_bandwidth_regen_upgrade() {
    SaveData a;
    std::strcpy(a.activeId, "paypup");
    a.generation = 1;
    a.bandwidthRegenBonusMin = 1;
    SaveStoredPet stored;
    std::strcpy(stored.id, "cryptoshell");
    stored.bandwidthRegenBonusMin = 1;
    a.rack.push_back(stored);
    a.rack.push_back(SaveStoredPet{});          // an un-upgraded shelf-mate

    SaveData out;
    CHECK(deserializeSave(serializeSave(a), out));
    CHECK(out.bandwidthRegenBonusMin == 1);
    CHECK(out.rack.size() == 2);
    CHECK(out.rack[0].bandwidthRegenBonusMin == 1);
    CHECK(out.rack[1].bandwidthRegenBonusMin == 0);
}

// THE EPIC TIER — the five dishes beyond Tiramisudo that permanently upgrade the pet
// eating them (core/model/pet_upgrades.h). What is asserted here is the shape they all
// share: the grant lands OFF the level, it lands once, and a second helping is a meal.
//
// The dishes are found BY EFFECT rather than named, so retiring or renaming one is a
// content edit and not a test edit — what the gate is protecting is the mechanic.
namespace {

// The item whose row carries `k`, or nullptr. Every Epic grant is on exactly one row,
// which test_every_permanent_grant_is_one_epic_dish below is the gate on.
const ItemDef* dishGranting(ItemEffect::Kind k) {
    for (const ItemDef* d : ContentRegistry::embedded().allItems())
        for (const ItemEffect& e : d->effects)
            if (e.kind == k) return d;
    return nullptr;
}

int countLogLinesContaining(const Game& g, const char* needle) {
    int n = 0;
    for (int i = 0; i < g.log().size(); ++i)
        if (std::strstr(g.log().at(i).text, needle)) ++n;
    return n;
}

const ItemEffect::Kind kStatGrantKinds[kLevelStatCount] = {
    ItemEffect::Kind::StatPointPower, ItemEffect::Kind::StatPointDefense,
    ItemEffect::Kind::StatPointSpeed, ItemEffect::Kind::StatPointHealth};

}  // namespace

// A granted point is NOT a level. It shows up in the pet's total, it does not move
// combatLevel, and it never appears in statPoints_ — which is the whole reason a
// Rollback cannot reach it (test_rollback_cannot_shed_a_granted_point below).
void test_epic_dish_grants_an_off_level_stat_point() {
    for (int stat = 0; stat < kLevelStatCount; ++stat) {
        const ItemDef* d = dishGranting(kStatGrantKinds[stat]);
        CHECK(d);
        if (!d) continue;
        // Epic is the tier the permanent grants are reserved to, and the dish has to be
        // COOKED — nothing sells one, which is what makes the recipe the payoff.
        CHECK(d->rarity == ItemDef::Rarity::Epic);
        CHECK(d->type == ItemDef::Type::Food);

        Game g{StartMode::Hatched, "paypup"};
        CHECK(g.statBonusPoint(stat) == 0);
        g.inventory().add(d->id, 2);
        g.debugUseItem(d->id);
        CHECK(g.statBonusPoint(stat) == 1);
        // The moment is written down — a once-per-pet upgrade the operator can scroll
        // back to, not just a "FED <dish>" line like every other plate.
        const int loggedGrants = countLogLinesContaining(g, "FOR LIFE");
        CHECK(loggedGrants == 1);
        CHECK(g.totalStatPoint(stat) == 1);
        CHECK(g.levelStatPoint(stat) == 0);        // not an EARNED point...
        CHECK(g.combatLevel() == 0);               // ...so the level has not moved
        // A second plate does not stack the grant — the pet already has the point —
        // but it is still EATEN rather than refused, which is what stops the dish from
        // becoming a dead row the moment it has paid out (Game::itemUseIsInert).
        CHECK(g.inventory().count(d->id) == 1);
        g.debugUseItem(d->id);
        CHECK(g.statBonusPoint(stat) == 1);
        CHECK(g.inventory().count(d->id) == 0);
        CHECK(countLogLinesContaining(g, "FOR LIFE") == loggedGrants);  // granted once
    }
}

// The point is real where it matters: a fighter built from a pet that ate the dish is
// stronger than the same pet that did not, by exactly the one point.
void test_granted_stat_point_reaches_combat() {
    const ItemDef* d = dishGranting(ItemEffect::Kind::StatPointHealth);
    CHECK(d);
    if (!d) return;
    Game g0{StartMode::Hatched, "paypup"};
    enterSimBattle(g0);
    const int baseHp = g0.combat().player().maxHealth;

    Game g1{StartMode::Hatched, "paypup"};
    g1.inventory().add(d->id, 1);
    g1.debugUseItem(d->id);
    for (int i = 0; i < 4 && g1.nav() != Game::Nav::Idle; ++i) tapC(g1);
    enterSimBattle(g1);
    CHECK(g1.combat().player().maxHealth == baseHp + kLevelHealthPerPoint);
}

// THE PROTECTION. A Rollback sheds an EARNED point and takes the level down with it;
// the granted point is on neither axis, so the pet keeps it all the way down to level 0
// — which is the floor the picker itself refuses to go under.
void test_rollback_cannot_shed_a_granted_point() {
    const ItemDef* d = dishGranting(ItemEffect::Kind::StatPointPower);
    CHECK(d);
    if (!d) return;
    Game g{StartMode::Hatched, "paypup"};
    g.inventory().add(d->id, 1);
    g.debugUseItem(d->id);
    for (int i = 0; i < 4 && g.nav() != Game::Nav::Idle; ++i) tapC(g);
    CHECK(g.statBonusPoint(0) == 1);

    // Earn exactly one level, then roll it back off.
    g.debugAddCombatXp(kLevelXpBase);
    CHECK(g.combatLevel() == 1);
    g.inventory().add("rollback", 1);
    g.debugUseItem("rollback");
    CHECK(g.nav() == Game::Nav::RollbackPicker);
    g.onButton(press(Button::B));
    CHECK(g.combatLevel() == 0);                 // every earned point is gone
    int earned = 0;
    for (int i = 0; i < kLevelStatCount; ++i) earned += g.levelStatPoint(i);
    CHECK(earned == 0);
    CHECK(g.statBonusPoint(0) == 1);             // ...and the granted one is untouched
    CHECK(g.totalStatPoint(0) == 1);

    // With nothing earned left the picker is inert, so there is no second shed that
    // could reach the grant by going negative.
    g.inventory().add("rollback", 1);
    g.debugUseItem("rollback");
    CHECK(g.nav() != Game::Nav::RollbackPicker);
    CHECK(g.inventory().count("rollback") == 1);  // not consumed
}

// Profilerole's rate is applied at addCombatXp, the one door every XP source comes
// through, so an upgraded pet banks more from the same award.
void test_epic_dish_grants_a_permanent_xp_rate() {
    const ItemDef* d = dishGranting(ItemEffect::Kind::XpRateBonusPct);
    CHECK(d);
    if (!d) return;
    CHECK(d->rarity == ItemDef::Rarity::Epic);
    int rate = 0;
    for (const ItemEffect& e : d->effects)
        if (e.kind == ItemEffect::Kind::XpRateBonusPct) rate = e.magnitude;
    CHECK(rate > 0);

    Game g{StartMode::Hatched, "paypup"};
    CHECK(g.xpRateBonusPct() == 0);
    g.inventory().add(d->id, 2);
    g.debugUseItem(d->id);
    CHECK(g.xpRateBonusPct() == rate);
    // A lump short of the first level, so the whole award stays visible in the bucket.
    const int award = kLevelXpBase / 2;
    g.debugAddCombatXp(award);
    CHECK(g.combatLevel() == 0);
    CHECK(g.combatXp() == award + award * rate / 100);
    CHECK(g.combatXp() > award);
    // Second helping: the rate is already the pet's, so it does not compound.
    g.debugUseItem(d->id);
    CHECK(g.xpRateBonusPct() == rate);
}

// The grants belong to the CREATURE, so the rack keeps them and a new egg does not
// inherit them — the same deal Tiramisudo's regen shave gets, asserted over the pair
// that share a save tail with it.
void test_granted_upgrades_survive_the_rack_and_reset_on_a_new_egg() {
    const ItemDef* pw = dishGranting(ItemEffect::Kind::StatPointPower);
    const ItemDef* xp = dishGranting(ItemEffect::Kind::XpRateBonusPct);
    CHECK(pw && xp);
    if (!pw || !xp) return;

    Game g{StartMode::Hatched, "paypup"};
    g.inventory().add(pw->id, 1);
    g.inventory().add(xp->id, 1);
    g.debugUseItem(pw->id);
    for (int i = 0; i < 4 && g.nav() != Game::Nav::Idle; ++i) tapC(g);
    g.debugUseItem(xp->id);
    for (int i = 0; i < 4 && g.nav() != Game::Nav::Idle; ++i) tapC(g);
    CHECK(g.statBonusPoint(0) == 1 && g.xpRateBonusPct() > 0);

    g.debugSeedRack("cryptoshell");
    auto deployFirstStored = [&] {
        enterSubmenuId(g, SubmenuId::Arch);
        g.onButton(press(Button::A));
        g.onButton(press(Button::B));
        g.onButton(press(Button::B));
        g.onButton(press(Button::A));
        g.onButton(press(Button::B));
    };
    deployFirstStored();
    CHECK(g.pet() && std::strcmp(g.pet()->id, "cryptoshell") == 0);
    CHECK(g.statBonusPoint(0) == 0 && g.xpRateBonusPct() == 0);  // never fed either
    deployFirstStored();
    CHECK(g.pet() && std::strcmp(g.pet()->id, "paypup") == 0);
    CHECK(g.statBonusPoint(0) == 1 && g.xpRateBonusPct() > 0);   // came back with them

    // A new egg is a new pet, so it starts with nothing granted.
    g.resetToHatch();
    pickFirstEggLine(g);
    for (int i = 0; i < kLevelStatCount; ++i) CHECK(g.statBonusPoint(i) == 0);
    CHECK(g.xpRateBonusPct() == 0);
}

// v57 — the off-level points and the XP rate round-trip for the active pet AND for a
// pet on the shelf, positionally matched the way the v50 tail beside them is.
void test_save_v57_permanent_grants() {
    SaveData a;
    std::strcpy(a.activeId, "paypup");
    a.generation = 1;
    for (int i = 0; i < kLevelStatCount; ++i) a.statBonus[i] = i;
    a.xpRateBonusPct = 25;
    SaveStoredPet stored;
    std::strcpy(stored.id, "cryptoshell");
    stored.statBonus[2] = 1;
    stored.xpRateBonusPct = 25;
    a.rack.push_back(stored);
    a.rack.push_back(SaveStoredPet{});          // an ungranted shelf-mate

    SaveData out;
    CHECK(deserializeSave(serializeSave(a), out));
    for (int i = 0; i < kLevelStatCount; ++i) CHECK(out.statBonus[i] == i);
    CHECK(out.xpRateBonusPct == 25);
    CHECK(out.rack.size() == 2);
    CHECK(out.rack[0].statBonus[2] == 1);
    CHECK(out.rack[0].xpRateBonusPct == 25);
    for (int i = 0; i < kLevelStatCount; ++i) CHECK(out.rack[1].statBonus[i] == 0);
    CHECK(out.rack[1].xpRateBonusPct == 0);
}

// One grant, one dish, one Epic row. The mechanic's scarcity IS the design — a second
// row carrying the same grant would give a player two chances at a once-per-pet upgrade
// — so the roster is held to it here rather than by everyone remembering.
void test_every_permanent_grant_is_one_epic_dish() {
    const ItemEffect::Kind kGrants[] = {
        ItemEffect::Kind::BandwidthRegenBonusMin, ItemEffect::Kind::StatPointPower,
        ItemEffect::Kind::StatPointDefense,       ItemEffect::Kind::StatPointSpeed,
        ItemEffect::Kind::StatPointHealth,        ItemEffect::Kind::XpRateBonusPct};
    for (ItemEffect::Kind k : kGrants) {
        int rows = 0;
        for (const ItemDef* d : ContentRegistry::embedded().allItems()) {
            bool carries = false;
            for (const ItemEffect& e : d->effects)
                if (e.kind == k) { carries = true; CHECK(e.magnitude > 0); }
            if (!carries) continue;
            ++rows;
            CHECK(d->rarity == ItemDef::Rarity::Epic);
            CHECK(itemIsOncePerPetLifetime(*d));    // the 'Pedia's pet page reads this
            // Cooked, never bought: the recipe is the payoff, so no shop lists a price.
            CHECK(d->bitsPrice == 0);
            const int r = recipeIndexByOutput(d->id);
            CHECK(r >= 0);
            if (r < 0) continue;
            // THE THROTTLE. A recipe is permanent once won and has no cooldown, so the
            // only thing limiting how many permanent upgrades a player can hand out is
            // what the dish is made OF. Every Epic method must therefore want at least
            // one ingredient off the scarce shelf (content_items.cpp's
            // kRareStapleWalkWeight) — without this a six-Common recipe would make the
            // whole tier farmable in an afternoon.
            int scarce = 0;
            for (const RecipeInput& in : kMergeRecipes[r].inputs) {
                if (!in.id) continue;
                const ItemDef* ing = ContentRegistry::embedded().item(in.id);
                CHECK(ing);
                if (ing && ing->rarity >= ItemDef::Rarity::Rare) scarce += in.qty;
            }
            CHECK(scarce >= 1);
        }
        CHECK(rows == 1);
    }
}

// A row that grants something once per pet has to SAY whether this pet has had it —
// nothing else on the ITEMS screens does. The Epic dishes stay perfectly usable once
// spent (they are still food), so there is no gate message to read it off, and the BUFFS
// page only lists what a pet HAS, never what a plate in the bag would still give it.
// lifetimeMark is the one answer both surfaces draw from, so it is what this holds.
void test_lifetime_mark_says_whether_this_pet_has_had_it() {
    Game g{StartMode::Hatched, "paypup"};
    const ContentRegistry& r = ContentRegistry::embedded();

    // An ordinary food has nothing to spend: NEITHER state, so the row draws no mark at
    // all. "No grant" reading the same as "grant already taken" is the failure here.
    const ItemDef* plain = r.item("dyno_nuggets");
    CHECK(plain);
    CHECK(lifetimeMark(*plain, g.petLifetimeGates()) == LifetimeMark::None);

    // Every row carrying a once-per-pet grant reads Unspent on a fresh pet, and flips to
    // Spent for that row alone once it is eaten. Swept over the whole set rather than one
    // example, because each grant is gated by a different flag.
    for (const ItemDef* d : r.allItems()) {
        if (!itemIsOncePerPetLifetime(*d)) continue;
        Game p{StartMode::Hatched, "paypup"};
        p.model().addCareMistake(2);          // something for the Yubi-Cookie to shave
        CHECK(lifetimeMark(*d, p.petLifetimeGates()) == LifetimeMark::Unspent);
        p.inventory().add(d->id, 1);
        p.debugUseItem(d->id);
        CHECK(lifetimeMark(*d, p.petLifetimeGates()) == LifetimeMark::Spent);
        // ...and only that row: a pet that ate one Epic dish has not spent the others.
        for (const ItemDef* other : r.allItems()) {
            if (other == d || !itemIsOncePerPetLifetime(*other)) continue;
            CHECK(lifetimeMark(*other, p.petLifetimeGates()) == LifetimeMark::Unspent);
        }
    }

    // The mark is per-PET, like the grants behind it: a new egg has had none of them.
    const ItemDef* dish = nullptr;
    for (const ItemDef* d : r.allItems())
        for (const ItemEffect& e : d->effects)
            if (e.kind == ItemEffect::Kind::StatPointPower) dish = d;
    CHECK(dish);
    if (!dish) return;
    g.inventory().add(dish->id, 1);
    g.debugUseItem(dish->id);
    CHECK(lifetimeMark(*dish, g.petLifetimeGates()) == LifetimeMark::Spent);
    g.resetToHatch();
    pickFirstEggLine(g);
    CHECK(lifetimeMark(*dish, g.petLifetimeGates()) == LifetimeMark::Unspent);
}

// v54 — the first ITEM id the rename table ever carried (`renamedIds`, save.cpp).
// Both id-bearing item fields are swept, and they fail differently if one is missed:
// `items` is the stack the operator is holding, so a miss there loses the food; and
// `collectedItems` is the ever-held set the earn-path achievements read, so a miss
// there silently un-collects an item the player had already met. A pre-v54 blob is
// rewritten as it is read, which is why the assertions are on what came BACK.
void test_save_v54_renames_the_snack_item_id() {
    SaveData a;
    std::strcpy(a.activeId, "paypup");
    a.items.push_back(SaveStack{"airgap_snack", 3});
    a.items.push_back(SaveStack{"tortilla_chip", 1});   // a neighbour must not move
    SaveId collected;
    std::strcpy(collected.id, "airgap_snack");
    a.collectedItems.push_back(collected);

    // Stamp it back to the last version that still WROTE the old id. The row only
    // applies below its sinceVersion, so a v54 blob must be left alone — which the
    // second half of this test is for.
    auto blob = serializeSave(a);
    const uint16_t before = 53;
    blob[4] = static_cast<uint8_t>(before);
    blob[5] = static_cast<uint8_t>(before >> 8);

    SaveData out;
    CHECK(deserializeSave(blob, out));
    CHECK(out.items.size() == 2);
    CHECK(std::strcmp(out.items[0].id, "dyno_nuggets") == 0);
    CHECK(out.items[0].qty == 3);                       // the count rides across intact
    CHECK(std::strcmp(out.items[1].id, "tortilla_chip") == 0);
    CHECK(out.collectedItems.size() == 1);
    CHECK(std::strcmp(out.collectedItems[0].id, "dyno_nuggets") == 0);

    // A CURRENT blob carries the new id already and must round-trip untouched — a
    // rename row that also fires at or above its own version would be rewriting
    // history it already wrote.
    SaveData b;
    b.items.push_back(SaveStack{"dyno_nuggets", 1});
    SaveData back;
    CHECK(deserializeSave(serializeSave(b), back));
    CHECK(back.items.size() == 1);
    CHECK(std::strcmp(back.items[0].id, "dyno_nuggets") == 0);
}
