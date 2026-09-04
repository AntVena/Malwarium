// test_stat.cpp — native gates for STAT paging, the loadout readout and the effect-text budget.
//
// One slice of the native suite; see test_gates.h for the shared fixtures and
// test_main.cpp for the roster that runs these. A save-vNN gate sits with the
// feature whose field it migrates, not in a migrations pile of its own.
#include "test_gates.h"

// STAT is 6 paged screens — 0 pet VITALS (the landing, + an XP bar), 1 TIERS (the
// investment ladder: which stat tiers this pet holds and what the next costs),
// 2 LOADOUT (the equipped moves + mods, WITH their effect text), 3 BUFFS (armed
// item buffs), 4 SPECIES (the pet's own lore), 5 AUDIT LOG — cycled by A, and C
// backs out. (Device/account stats live on the Hacker PROFILE slot.)
void test_stat_paging_loadout_xp() {
    Game g{StartMode::Hatched};
    g.model().setHunger(100);                        // full gauge -> 10 lit cells
    enterSubmenuId(g, SubmenuId::Stat);
    Framebuffer p0(kActiveW, kActiveH);
    g.render(p0);                                    // page 0 = VITALS (landing)
    CHECK(litCellsGray(p0, 70, 110, 74) == 10);       // full hunger gauge (fresh pet)

    // the XP bar fills with banked XP — a half-level of XP paints
    // the fill region that an empty (0 XP) fresh pet leaves blank on the landing page.
    Game g3{StartMode::Hatched};
    g3.debugAddCombatXp(g3.xpToNextLevel() / 2);
    enterSubmenuId(g3, SubmenuId::Stat);
    Framebuffer p0xp(kActiveW, kActiveH);
    g3.render(p0xp);
    CHECK(regionDiffers(p0, p0xp, 31, 60, 120, 66));  // XP bar fill differs

    // A -> page 1 = TIERS (a distinct page).
    g.onButton(press(Button::A));
    Framebuffer p1(kActiveW, kActiveH);
    g.render(p1);
    CHECK(!fbEqual(p0, p1));

    // A -> page 2 = LOADOUT (distinct again), then BUFFS, SPECIES, audit log,
    // A -> wraps back to page 0 = VITALS.
    g.onButton(press(Button::A));
    Framebuffer p2(kActiveW, kActiveH);
    g.render(p2);
    CHECK(!fbEqual(p1, p2));
    g.onButton(press(Button::A));   // -> page 3 BUFFS
    Framebuffer p3(kActiveW, kActiveH);
    g.render(p3);
    CHECK(!fbEqual(p2, p3));
    g.onButton(press(Button::A));   // -> page 4 SPECIES
    g.onButton(press(Button::A));   // -> page 5 AUDIT LOG
    g.onButton(press(Button::A));   // -> wraps to page 0 VITALS
    Framebuffer wrap(kActiveW, kActiveH);
    g.render(wrap);
    CHECK(fbEqual(p0, wrap));                          // cycle wraps to VITALS
}

// buildLoadoutRows(): the STAT LOADOUT page's row model. Every UNLOCKED slot yields exactly one row carrying BOTH the name
// AND the effect text: the equipped move, or — when the slot is EMPTY — the innate
// Quick Jab fallback tagged isDefault. There is NO standalone default row (Quick Jab is
// not a dedicated fixture; it surfaces only inside an empty slot). An empty MODS loadout
// still gets its section (a "- NONE -" row); an egg collapses to one "- no loadout -" line.
void test_loadout_rows_model() {
    ContentRegistry r = ContentRegistry::embedded();
    MoveLoadout ml = MoveLoadout::starting();   // owns 3, equips packet_storm (slot 0 only)
    Loadout mods = Loadout::starting();         // equips firewall_patch + clock_speed_boost

    auto rows = buildLoadoutRows(r, ml, mods, Stage::Process, /*isEgg=*/false);

    bool sawMovesHeader = false, sawModsHeader = false;
    for (const auto& row : rows) {
        if (row.header && std::strcmp(row.label, "MOVES") == 0) sawMovesHeader = true;
        if (row.header && std::strcmp(row.label, "MODS") == 0) sawModsHeader = true;
    }
    CHECK(sawMovesHeader && sawModsHeader);

    // At Process the pet has 2 slots: slot 0 = packet_storm (equipped), slot 1 = empty →
    // the Quick Jab fallback. So there's a DEFAULT-tagged row, it names Quick Jab and
    // carries its effect, and it appears AFTER the equipped move (in slot order) — NOT as a
    // leading fixture. The first non-header row is the equipped move, not the default.
    auto isDefaultRow = [](const ProseRow& r) {
        return std::strcmp(r.tag, "DEFAULT") == 0;
    };
    const ProseRow* firstMove = nullptr;
    const ProseRow* defaultRow = nullptr;
    for (const auto& row : rows) {
        if (row.header) continue;
        if (!firstMove) firstMove = &row;
        if (isDefaultRow(row) && !defaultRow) defaultRow = &row;
    }
    CHECK(firstMove && !isDefaultRow(*firstMove));        // equipped move leads, not the default
    const MoveDef* def = r.move(ml.defaultMove());
    CHECK(defaultRow && def && std::strcmp(defaultRow->label, def->displayName) == 0);
    CHECK(defaultRow && !defaultRow->body.empty() &&
          std::strcmp(defaultRow->body.c_str(), effectText(*def).c_str()) == 0);

    // Every UNLOCKED slot yields one row: equipped moves by name+effect, empty slots as
    // the default. Exact row count = 2 headers + (one row per unlocked slot) + mod rows.
    auto containsNameAndEffect = [&](const char* name, const EffectText& effect) {
        for (const auto& row : rows)
            if (!row.header && std::strcmp(row.label, name) == 0 &&
                !row.body.empty() &&
                std::strcmp(row.body.c_str(), effect.c_str()) == 0)
                return true;
        return false;
    };
    const int unlocked = MoveLoadout::slotsForStage(Stage::Process);
    for (int i = 0; i < unlocked; ++i) {
        if (const char* id = ml.equipped(i)) {
            const MoveDef* m = r.move(id);
            CHECK(m && containsNameAndEffect(m->displayName, effectText(*m)));
        }
    }
    int equippedModCount = 0;
    for (int i = 0; i < kModSlots; ++i) {
        if (const char* id = mods.equipped(i)) {
            const ModDef* m = r.mod(id);
            CHECK(m && containsNameAndEffect(m->displayName, effectText(*m)));
            ++equippedModCount;
        }
    }
    CHECK(equippedModCount > 0);   // Loadout::starting does equip mods (sample)
    CHECK(static_cast<int>(rows.size()) == 2 + unlocked + equippedModCount);

    // No mods equipped -> a single "- NONE -" row (the section still appears).
    Loadout noMods;   // default-constructed: equipped(i) == nullptr for every slot
    auto rowsNoMods = buildLoadoutRows(r, ml, noMods, Stage::Process, false);
    bool sawNone = false;
    for (const auto& row : rowsNoMods)
        if (!row.header && std::strcmp(row.label, "- NONE -") == 0) sawNone = true;
    CHECK(sawNone);

    // An egg collapses the whole page to one line, no headers.
    auto eggRows = buildLoadoutRows(r, ml, mods, Stage::BootSector, /*isEgg=*/true);
    CHECK(eggRows.size() == 1 && !eggRows[0].header && eggRows[0].body.empty());
    CHECK(std::strcmp(eggRows[0].label, "- NO LOADOUT -") == 0);
}

// Descriptions are TEMPLATES over their own row (core/content/effect_text.h): a
// description writes `{mag}` and the value is substituted from the same row, so a
// balance retune can never leave the prose quoting the old number. That only holds
// while every token a row names actually resolves — an unknown or misspelt token is
// passed through braces and all, which is exactly what this asserts against, across
// every item / mod / move in the shipped tables. It also catches the reverse mistake:
// a bare digit typed into prose that a {token} should have supplied is invisible to
// the expander, so the stat line is checked to be carrying the row's magnitudes
// independently of whatever the prose says.
void test_effect_text_templates_resolve() {
    auto clean = [](const EffectText& t) {
        return std::strchr(t.c_str(), '{') == nullptr &&
               std::strchr(t.c_str(), '}') == nullptr;
    };
    for (int i = 0; i < kItemsCount; ++i) CHECK(clean(effectText(kItems[i])));
    for (int i = 0; i < kModsCount; ++i) CHECK(clean(effectText(kMods[i])));
    for (int i = 0; i < kMovesCount; ++i) CHECK(clean(effectText(kMoves[i])));

    // A worked case end to end: Tripwire's two magnitudes land in the right holes
    // (magnitude = the reflected damage, magnitude2 = the Health threshold — the
    // pair a hand-written sentence is most likely to transpose).
    const ModDef* tw = nullptr;
    for (int i = 0; i < kModsCount; ++i)
        if (std::strcmp(kMods[i].id, "tripwire") == 0) tw = &kMods[i];
    CHECK(tw && tw->magnitude == 10 && tw->magnitude2 == 40);
    CHECK(std::strcmp(effectText(*tw).c_str(),
                      "Below 40% Health, reflects 10 damage to any attacker.") == 0);

    // Every structured magnitude a row carries reaches the player through the stat
    // line whether or not the prose mentions it: the Tor-Tilla Chip's description is
    // pure flavour, so its Happiness 10 has to come from here.
    const ItemDef* chip = nullptr;
    for (int i = 0; i < kItemsCount; ++i)
        if (std::strcmp(kItems[i].id, "tortilla_chip") == 0) chip = &kItems[i];
    CHECK(chip && std::strstr(effectText(*chip).c_str(), "10") == nullptr);
    CHECK(chip && std::strcmp(statLine(*chip).c_str(), "HAPPY +10") == 0);

    // Descriptions are drawn with FONT_UI (core/render/font_glyphs.cpp), whose table
    // stops at ASCII 126 — a typographic dash or ellipsis renders as a run of
    // blanks AND measures 3 characters wide, throwing off every wrap that sizes
    // itself with textWidth(). So the tables stay ASCII-only.
    auto ascii = [](const char* s) {
        for (const char* p = s; *p; ++p)
            if (static_cast<unsigned char>(*p) > 127) return false;
        return true;
    };
    for (int i = 0; i < kItemsCount; ++i) CHECK(ascii(kItems[i].effect));
    for (int i = 0; i < kModsCount; ++i) CHECK(ascii(kMods[i].effect));
    for (int i = 0; i < kMovesCount; ++i) CHECK(ascii(kMoves[i].effect));
    // The stat-tier ladder is drawn with the same font on the same page as the loadout
    // it sits beside (STAT's TIERS), so it lives under the same rule even though it is
    // a core/model table rather than a content one.
    for (int i = 0; i < kLevelStatCount; ++i)
        for (int t = 0; t < kStatTierCount; ++t) {
            const StatTierDef& d = statTier(static_cast<LevelStat>(i), t);
            CHECK(ascii(d.name));
            CHECK(ascii(d.effect));
        }

    // {|token|} renders the magnitude unsigned, for prose that carries the sign.
    ItemDef probe = kItems[0];
    probe.effect = "{hunger} then {|hunger|}";
    probe.effects[0] = {ItemEffect::Kind::Hunger, -15};
    probe.effects[1] = {};
    CHECK(std::strcmp(effectText(probe).c_str(), "-15 then 15") == 0);

    // An unknown token survives as literal braces — which is what makes the sweep
    // above a real gate rather than a formality.
    probe.effect = "{nosuchfield}";
    CHECK(std::strcmp(effectText(probe).c_str(), "{nosuchfield}") == 0);
}

// A content row's panel is the readout GRID plus the prose (ui/spec_sheet.h), and the
// two share one band. The grid is reserved first and never cut, so the prose is what
// overruns — and on the pages with no scroll it loses its tail with no ellipsis and no
// warning. This gates the three bands where that can happen, measured through the
// renderers' own wrap + grid packing so it can't drift from what ships:
//   * ITEMS / MODS detail — grid + prose must fit the band above their footer rows.
//   * an area STOREFRONT — kShopDescLines, much tighter, and only binding on the rows
//     an area actually stocks (a full shop's worst case; a short list gets more).
//   * every move's grid alone must fit the reserve the MOVE detail page sets aside for
//     it (train_screen's kDetailGridLines) — its prose is allowed to overrun, because
//     that page pages it on A rather than truncating.
//   * every RIG SHOP row's readout, swept across its whole tier ladder, must fit the
//     space under its name in a kRigRowPitch row (game_rig_shop.h's rigSpec).
// STAT's LOADOUT and BUFFS pages are deliberately NOT on this list: they budget no
// lines at all, sizing each row to the prose it holds and scrolling what is left
// over (stat_screen.cpp), so there is no number for content to outgrow there.
// Prose is also checked against EffectText's own buffer, which caps it before any
// screen is involved.
// It also catches a readout LABEL that outgrew SpecRow's buffer, which truncates
// silently (a 20-char flag losing its last letter is invisible in review).
void test_effect_text_fits_its_screen_budget() {
    constexpr int kMaxW = kActiveW - 2 * 8;   // both margins, every panel screen
    // Both taken from the screens themselves (layout.h's kLineH, items_screen's own
    // band): a literal here is a number that goes stale the moment the font or the
    // panel moves, and a panel budget that reads high is one that passes an item
    // whose description is being cut on the device.
    const int kDetailPanelLines = itemDetailPanelLines();
    constexpr int kMoveGridReserve = 4;       // train_screen's kDetailGridLines
    auto proseLines = [&](const EffectText& t) {
        return textWrapLines(t.c_str(), kMaxW);
    };
    auto labelsIntact = [](const SpecRows& s) {
        for (int i = 0; i < s.count; ++i) {
            const SpecRow& r = s.rows[i];
            if (std::strlen(r.label) >= sizeof(r.label) - 1) return false;  // truncated
            if (std::strlen(r.value) >= sizeof(r.value) - 1) return false;
            if (textWidth(r.label) + kFontAdvance + textWidth(r.value) > kMaxW)
                return false;                                              // unpackable
        }
        return true;
    };
    auto panelFits = [&](const SpecRows& s, const EffectText& prose) {
        return labelsIntact(s) &&
               gridLines(kMaxW, s.rows, s.count) + proseLines(prose) <= kDetailPanelLines;
    };
    for (int i = 0; i < kItemsCount; ++i)
        CHECK(panelFits(specRows(kItems[i]), effectText(kItems[i])));
    // MODS budgets its panel differently: its floor grows with the readout, so the
    // prose gets a FIXED number of lines whatever the grid costs. Held to that number
    // rather than to the ITEMS band, which would pass a mod whose tail is being cut.
    for (int i = 0; i < kModsCount; ++i) {
        CHECK(labelsIntact(specRows(kMods[i])));
        CHECK(proseLines(effectText(kMods[i])) <= modDetailProseLines());
    }
    for (int i = 0; i < kMovesCount; ++i) {
        const SpecRows s = specRows(kMoves[i]);
        CHECK(labelsIntact(s));
        CHECK(gridLines(kMaxW, s.rows, s.count) <= kMoveGridReserve);
    }

    // Prose is capped by EffectText's own buffer before any screen sees it, so a row
    // that reaches the cap has already lost its tail — catch that at the source rather
    // than wondering why a sentence stops mid-word on the device.
    for (int i = 0; i < kItemsCount; ++i) CHECK(!effectText(kItems[i]).atCap());
    for (int i = 0; i < kModsCount; ++i) CHECK(!effectText(kMods[i]).atCap());
    for (int i = 0; i < kMovesCount; ++i) CHECK(!effectText(kMoves[i]).atCap());

    // The Rig Shop builds its readout the same way (rigSpec), and its values are the
    // longest anyone builds — a "NOW -> NEXT" pair at a deep tier. Sweep every row
    // across its ladder: no truncation, and the grid has to fit the space a SHOP row
    // leaves under its name (kRigRowPitch minus the name line).
    constexpr int kRigRowGridLines = (kRigRowPitch - (kFontH + 5)) / (kFontH + 3);
    for (int i = 0; i < kRigUpgradeCount; ++i) {
        const RigUpgradeDef& d = kRigUpgrades[i];
        // Bandwidth is deliberately uncapped (kRigLevelUnlimited), so bound the sweep
        // at a level far past any real tier ladder rather than iterating a million.
        const int top = d.maxLevel < 300 ? d.maxLevel : 300;
        for (int lvl = 0; lvl <= top; ++lvl) {
            const SpecRows s = rigSpec(d, lvl);
            CHECK(labelsIntact(s));
            CHECK(gridLines(kMaxW, s.rows, s.count) <= kRigRowGridLines);
        }
    }

    ContentRegistry reg;
    reg.addSource(embeddedContent());
    for (int a = 0; a < kAreaCount; ++a) {
        const AreaDef& ar = area(a);
        for (int i = 0; i < ar.shop.listingCount; ++i) {
            const ItemDef* d = reg.item(ar.shop.listings[i].id);
            CHECK(d && proseLines(effectText(*d)) <= kShopDescLines);
        }
        for (int i = 0; i < ar.modShop.listingCount; ++i) {
            const ModDef* m = reg.mod(ar.modShop.listings[i].id);
            CHECK(m && proseLines(effectText(*m)) <= kShopDescLines);
        }
    }
}

// The STAT LOADOUT page's B-scroll: B is a no-op on the pages that don't flow rows
// (0 VITALS / 4 AUDIT LOG), it advances the row window on page 1, and wraps back to
// the top once it runs past the end. The fresh starting loadout (2 moves + 2 mods +
// 2 section headers) already outruns one screen, so no extra setup is needed to
// force a scroll.
//
// B is a tap/hold pair on these pages (the hold opens the INDEX), so the advance
// settles on the RELEASE — every press here is a complete tapB.
void test_stat_loadout_b_scroll() {
    Game g{StartMode::Hatched};
    enterSubmenuId(g, SubmenuId::Stat);

    // Page 0 (VITALS): B is a no-op.
    Framebuffer v0(kActiveW, kActiveH); g.render(v0);
    tapB(g);
    Framebuffer v0b(kActiveW, kActiveH); g.render(v0b);
    CHECK(fbEqual(v0, v0b));

    // A twice -> page 2 (LOADOUT); page 1 (TIERS) flows rows of its own and is
    // walked by its own gate below. Confirm the starting loadout really does
    // overflow the visible window (the precondition this test needs).
    g.onButton(press(Button::A));
    g.onButton(press(Button::A));
    auto rows = buildLoadoutRows(g.content(), g.moveLoadout(), g.loadout(),
                                 g.pet()->stage, g.inEggPhase());
    CHECK(loadoutRowsFitting(rows, 0) < static_cast<int>(rows.size()));

    Framebuffer l0(kActiveW, kActiveH); g.render(l0);
    tapB(g);                                     // scroll forward one window
    Framebuffer l1(kActiveW, kActiveH); g.render(l1);
    CHECK(!fbEqual(l0, l1));                     // the row window actually moved

    // Keep pressing to the end of the list: every window is a fresh view, and the
    // one past the last wraps home. How many windows that takes depends on how tall
    // the rows are (each is sized to its own prose), so the walk is bounded by the
    // row count rather than assuming a fixed window.
    bool wrapped = false;
    for (int i = 0; i < static_cast<int>(rows.size()) && !wrapped; ++i) {
        tapB(g);
        Framebuffer ln(kActiveW, kActiveH); g.render(ln);
        wrapped = fbEqual(l0, ln);
    }
    CHECK(wrapped);

    // C -> back to the carousel (resets statPage_/statScroll_); B re-enters
    // the submenu (cursor is still parked on STAT's slot); A twice -> page 2
    // confirms the scroll reset (renders identically to the first page-2 view, l0).
    tapC(g);
    tapB(g);
    g.onButton(press(Button::A));
    g.onButton(press(Button::A));
    Framebuffer l3(kActiveW, kActiveH); g.render(l3);
    CHECK(fbEqual(l0, l3));

    // Page 3 (BUFFS) flows rows too, but with nothing armed it has none to scroll,
    // so B is inert there; page 5 (AUDIT LOG) never scrolls at all.
    g.onButton(press(Button::A));                // page 2 -> page 3
    Framebuffer b0(kActiveW, kActiveH); g.render(b0);
    tapB(g);
    Framebuffer b0b(kActiveW, kActiveH); g.render(b0b);
    CHECK(fbEqual(b0, b0b));

    g.onButton(press(Button::A));                // -> page 4 (SPECIES)
    g.onButton(press(Button::A));                // -> page 5 (AUDIT LOG)
    Framebuffer a0(kActiveW, kActiveH); g.render(a0);
    tapB(g);
    Framebuffer a0b(kActiveW, kActiveH); g.render(a0b);
    CHECK(fbEqual(a0, a0b));
}

// buildTierRows(): the TIERS page's row model — the investment ladder, read back as the
// player sees it. One heading per stat carrying that stat's point total, then one row per
// rung. The TAG is the whole page: it is the only channel reporting state (the pages have
// to stay legible in grayscale) and the only thing that answers "how far to the next one",
// so it is what this gate reads rather than the pixels.
void test_tier_rows_report_where_the_pet_stands() {
    // A pet with one stat on each side of every interesting boundary: nothing invested,
    // one point short of the first rung, exactly on it, and past the second.
    const int points[kLevelStatCount] = {0, kStatTier1Points - 1, kStatTier1Points,
                                         kStatTier2Points};
    const auto rows = buildTierRows(points);
    // One heading plus one row per rung, per stat — the page draws whatever the shared
    // table holds, so the count is a statement about the table and not about the page.
    CHECK(static_cast<int>(rows.size()) == kLevelStatCount * (1 + kStatTierCount));

    for (int i = 0; i < kLevelStatCount; ++i) {
        const int base = i * (1 + kStatTierCount);
        const ProseRow& head = rows[base];
        CHECK(head.header);
        CHECK(head.body.empty());                    // a heading explains nothing itself
        char expect[16];
        std::snprintf(expect, sizeof(expect), "%d PTS", points[i]);
        CHECK(std::strcmp(head.tag, expect) == 0);   // the stat's own count rides its heading

        const int reached = statTiersReached(points[i]);
        for (int t = 0; t < kStatTierCount; ++t) {
            const ProseRow& row = rows[base + 1 + t];
            CHECK(!row.header);
            CHECK(!row.body.empty());                // every rung says what it does
            CHECK(std::strcmp(row.label, statTier(static_cast<LevelStat>(i), t).name) == 0);
            if (t < reached) {
                CHECK(std::strcmp(row.tag, "HELD") == 0);
            } else if (t == reached) {
                // The rung being climbed names the distance, and it is the SAME distance
                // the shared progress helper reports — a page that did its own arithmetic
                // here is exactly how a screen starts lying about the engine.
                std::snprintf(expect, sizeof(expect), "%d TO GO",
                              statTierPointsToNext(points[i]));
                CHECK(std::strcmp(row.tag, expect) == 0);
            } else {
                std::snprintf(expect, sizeof(expect), "AT %d", statTierPoints(t));
                CHECK(std::strcmp(row.tag, expect) == 0);
            }
            CHECK(row.tag[0] != '\0');               // no rung is left unlabelled
        }
    }

    // The boundary belongs to the rung, read off the page: one point short reads as
    // climbing, exactly on it reads as held.
    CHECK(std::strcmp(rows[1 * (1 + kStatTierCount) + 1].tag, "1 TO GO") == 0);
    CHECK(std::strcmp(rows[2 * (1 + kStatTierCount) + 1].tag, "HELD") == 0);

    // A null point array is not a crash — the page is drawn from whatever Game hands it,
    // and an empty list draws as an empty page rather than reading off nothing.
    CHECK(buildTierRows(nullptr).empty());
}

// The TIERS page reads the stat the way the FIGHT does: total points, an Epic dish's
// off-level grant included. A page counting only earned points would promise a rung the
// engine had already granted, or withhold one it had — and the off-level points are
// precisely the ones that carry a committed pet to the top of the ladder.
void test_tier_page_counts_off_level_points() {
    Game g{StartMode::Hatched};
    g.debugAddCombatXp(1200);                        // some earned points, spread at random
    const int earned = g.levelStatPoint(0);
    const auto before = buildTierRows(nullptr);      // (the null case, again cheaply)
    CHECK(before.empty());

    int points[kLevelStatCount];
    for (int i = 0; i < kLevelStatCount; ++i) points[i] = g.totalStatPoint(i);
    CHECK(points[0] == earned + g.statBonusPoint(0));
    // What the page would draw for POWER's heading is that total, not the earned half.
    const auto rows = buildTierRows(points);
    char expect[16];
    std::snprintf(expect, sizeof(expect), "%d PTS", points[0]);
    CHECK(std::strcmp(rows[0].tag, expect) == 0);
    // ...and the rung it reports is the one the engine would resolve from the same count.
    Combatant c;
    c.maxHealth = 100;
    applyLevelStatPoints(c, points);
    const bool pageSaysHeld = std::strcmp(rows[2].tag, "HELD") == 0;   // POWER's T2 row
    CHECK(pageSaysHeld == (c.piercePct > 0));
}

// The flowed pages TILE their rows: consecutive windows skip nothing and repeat
// nothing, and the walk terminates. This is the invariant the engine's B press and
// the page's own layout have to agree on — they measure the window separately
// (game_core.cpp's statScrollSpan vs. the renderer), and a disagreement shows up as
// a row that can never be read or one that shows twice. Checked on a Daemon holding
// a full kit, the longest list the page can be handed.
void test_stat_prose_windows_tile_the_list() {
    Game g{StartMode::Hatched, "wire_heir"};
    g.debugFillLoadout();
    const auto rows = buildLoadoutRows(g.content(), g.moveLoadout(), g.loadout(),
                                       g.pet()->stage, g.inEggPhase());
    const int total = static_cast<int>(rows.size());
    CHECK(loadoutRowsFitting(rows, 0) < total);   // it really does need scrolling

    int covered = 0;
    for (int top = 0; top < total;) {
        const int n = loadoutRowsFitting(rows, top);
        CHECK(n > 0);            // a zero-row window would never reach the end
        covered += n;
        top += n;
    }
    CHECK(covered == total);     // every row seen exactly once
}

// Release gate 1 (grayscale readability) on every screen that lacked a dedicated
// grayscale test — each must read with colour stripped: ink text + structure.
void test_remaining_screens_grayscale() {
    Framebuffer fb(kActiveW, kActiveH);
    const int W = kActiveW, H = kActiveH;

    // MAINT list.
    { Game g{StartMode::Hatched}; g.model().setFragmentation(40);
      enterSubmenuId(g, SubmenuId::Maint); g.render(fb);
      CHECK(hasDarkInk(fb, 0, 0, W, H)); }

    // MAINT action detail.
    { Game g{StartMode::Hatched}; g.model().setFragmentation(40);
      enterSubmenuId(g, SubmenuId::Maint); g.onButton(press(Button::B));
      CHECK(g.nav() == Game::Nav::Detail);
      g.render(fb); CHECK(hasDarkInk(fb, 0, 0, W, H)); }

    // MAINT process (running) then outcome (resolved) — both render via Process.
    { Game g{StartMode::Hatched}; g.model().setFragmentation(40);
      enterSubmenuId(g, SubmenuId::Maint); g.onButton(press(Button::B));
      g.onButton(press(Button::B));                 // start the non-interruptible run
      CHECK(g.nav() == Game::Nav::Process);
      g.render(fb); CHECK(hasDarkInk(fb, 0, 0, W, H));   // progress bar + label
      uint32_t t = 0;
      for (int i = 0; i < kProcessBeats + 1; ++i) g.tick(t += kHeartbeatMs);
      CHECK(g.nav() == Game::Nav::Process);            // resolved, awaiting dismiss
      g.render(fb); CHECK(hasDarkInk(fb, 0, 0, W, H)); }   // outcome text

    // Item detail.
    { Game g{StartMode::Hatched}; enterSubmenuId(g, SubmenuId::Items);
      g.onButton(press(Button::B)); CHECK(g.nav() == Game::Nav::Detail);
      g.render(fb); CHECK(hasDarkInk(fb, 0, 0, W, H)); }

    // Feeding modal (Use the first food item).
    { Game g{StartMode::Hatched}; enterSubmenuId(g, SubmenuId::Items);
      g.onButton(press(Button::B)); g.onButton(press(Button::B));   // detail -> Use
      CHECK(g.nav() == Game::Nav::ModalFeeding);
      g.render(fb); CHECK(hasDarkInk(fb, 0, 0, W, H)); }

    // Lockout modal.
    { Game g{StartMode::Hatched}; g.model().setHunger(0); g.tick(1);
      CHECK(g.lockoutActive());
      g.render(fb); CHECK(hasDarkInk(fb, 0, 0, W, H)); }

    // Hacker-Log (STAT page 2).
    { Game g{StartMode::Hatched}; enterSubmenuId(g, SubmenuId::Stat);
      g.onButton(press(Button::A));                 // page Vitals -> Log
      g.render(fb); CHECK(hasDarkInk(fb, 0, 0, W, H)); }

    // TRAIN list shell (a submenu with a deferred ·soon· action).
    { Game g{StartMode::Hatched}; enterLoadoutTab(g, 1);
      g.render(fb); CHECK(hasDarkInk(fb, 0, 0, W, H)); }
}

// Event-driven repaint: no free-running loop. Between
// heartbeats a static screen issues no redraw; a heartbeat repaints (idle
// animation); a state change (button) repaints exactly once, then quiesces.
void test_event_driven_repaint() {
    Game g{StartMode::Hatched};
    g.tick(0);                              // consume the initial dirty paint
    CHECK(!g.tick(10));                     // static, mid-heartbeat -> no repaint
    CHECK(!g.tick(20));
    CHECK(g.tick(kHeartbeatMs));            // heartbeat boundary -> animation repaint
    g.onButton(press(Button::A));           // state change (summon cursor)
    CHECK(g.tick(kHeartbeatMs + 10));      // exactly one repaint for the change...
    CHECK(!g.tick(kHeartbeatMs + 20));     // ...then it quiesces
}

// Release gate 2 (legible before the x1.75 upscale) + the creature gates,
// sampled at the logical sprite resolution: each pet's frame 0 must read as a
// clear silhouette (a solid object, neither empty nor a full block) with distinct
// grayscale value steps (shading, not one flat blob).
void test_sprite_grayscale_legibility() {
    ContentRegistry r = ContentRegistry::embedded();
    // The full Ransomware chain (Boot->Process->Script->Daemon) + Pingcub: each
    // sprite — incl. the single-frame Malbear / Daemon-branch frames — must
    // pass the shading/silhouette law.
    const char* ids[] = {"cryptoshell", "paypup", "malbear", "bruinforce",
                         "berserkernel", "pingcub"};
    for (const char* id : ids) {
        const CreatureDef* c = r.creature(id);
        CHECK(c != nullptr);
        const SpriteData* s = c ? r.creatureSprite(*c) : nullptr;
        CHECK(s != nullptr);
        if (!s) continue;
        int opaque = 0;
        bool band[5] = {false, false, false, false, false};   // 5 luminance steps
        for (int y = 0; y < s->h; ++y)
            for (int x = 0; x < s->frameW; ++x) {
                if (spriteAlphaAt(*s, x, y) < 128) continue;   // frame 0
                ++opaque;
                int b = static_cast<int>(luminance(spriteColorAt(*s, x, y)) * 5);
                if (b > 4) b = 4;
                if (b < 0) b = 0;
                band[b] = true;
            }
        const int cells = s->frameW * s->h;
        CHECK(opaque > cells / 10 && opaque < cells * 95 / 100);   // clear silhouette
        int bands = 0;
        for (bool v : band) if (v) ++bands;
        CHECK(bands >= 3);                                          // distinct value steps
    }
}

// ===========================================================================
// Carousel submenus (CFG · ARCH · MODS · TRAIN · EXPL)
// ===========================================================================

// A permanent upgrade has to be READABLE, or a player has no way to know a pet carries
// one: nothing else reports them — no home-screen icon (they never lapse and ask nothing
// of the operator), no shop row, no timer. The BUFFS page is the whole surface, and it
// has to hold ALL of them, since a well-fed pet can be carrying every grant at once.
//
// They also have to READ as permanent, which is what the PERMANENT heading is for: a
// standing grant listed among the things counting down is a standing grant the player
// will expect to lose. The heading is emitted by the first row under it and never over
// an empty section, so this gate reads the rows THROUGH it rather than by index.
void test_buffs_page_lists_the_permanent_upgrades() {
    ContentRegistry r = ContentRegistry::embedded();
    // Nothing armed, and nothing granted: the page is empty.
    CHECK(buildBuffRows(r, false, false, false, 0, 1, false, false, 0,
                        BranchOverride::None, 1, false, PetUpgrades{})
              .empty());

    // The DISH that grants an effect, resolved off the item table rather than named by a
    // literal here — renaming a dish must not need a test edit to go with it.
    auto dishFor = [&](ItemEffect::Kind k) -> const ItemDef* {
        for (const ItemDef* it : r.allItems())
            for (const ItemEffect& e : it->effects)
                if (e.kind == k) return it;
        return nullptr;
    };

    // The buffs (not the headings) of a page, in order — what the reader actually sees
    // listed, with the section signage taken back out.
    auto entries = [](const std::vector<BuffRow>& rows) {
        std::vector<const BuffRow*> out;
        for (const BuffRow& row : rows)
            if (!row.header) out.push_back(&row);
        return out;
    };
    auto hasHeading = [](const std::vector<BuffRow>& rows, const char* label) {
        for (const BuffRow& row : rows)
            if (row.header && std::strcmp(row.label, label) == 0) return true;
        return false;
    };

    PetUpgrades u;
    u.bandwidthRegenMin = 1;
    const std::vector<BuffRow> one =
        buildBuffRows(r, false, false, false, 0, 1, false, false, 0,
                      BranchOverride::None, 1, false, u);
    CHECK(entries(one).size() == 1);
    // The standing grant is filed under PERMANENT, and nothing is armed, so the page
    // carries no ARMED heading over an empty section.
    CHECK(hasHeading(one, "PERMANENT"));
    CHECK(!hasHeading(one, "ARMED"));
    const ItemDef* bw = dishFor(ItemEffect::Kind::BandwidthRegenBonusMin);
    CHECK(bw && std::strcmp(entries(one)[0]->label, bw->displayName) == 0);
    CHECK(!entries(one)[0]->hasTimer);   // it never lapses, so it carries no countdown

    // Everything at once: the regen shave, one row per off-level stat point, the XP rate.
    for (int& b : u.statBonus) b = 1;
    u.xpRatePct = 25;
    const std::vector<BuffRow> all =
        buildBuffRows(r, false, false, false, 0, 1, false, false, 0,
                      BranchOverride::None, 1, false, u);
    CHECK(entries(all).size() == static_cast<size_t>(2 + kLevelStatCount));
    for (const BuffRow& row : all) CHECK(!row.hasTimer);
    // Each stat point is named by its own dish, in stat order.
    const ItemEffect::Kind kStatKinds[kLevelStatCount] = {
        ItemEffect::Kind::StatPointPower, ItemEffect::Kind::StatPointDefense,
        ItemEffect::Kind::StatPointSpeed, ItemEffect::Kind::StatPointHealth};
    for (int i = 0; i < kLevelStatCount; ++i) {
        const ItemDef* d = dishFor(kStatKinds[i]);
        CHECK(d && std::strcmp(entries(all)[1 + i]->label, d->displayName) == 0);
    }
    const ItemDef* xp = dishFor(ItemEffect::Kind::XpRateBonusPct);
    CHECK(xp && std::strcmp(entries(all)[1 + kLevelStatCount]->label,
                            xp->displayName) == 0);
}

// STAT's landing page and its SPECIES page each open with a header that packs a
// creature's NAME against fixed chrome on the right — GEN + stage on one, the line tag
// on the other. Both halves are content: a twelve-letter creature beside METAMORPHIC
// LINE fills the line exactly, and one letter more prints through it.
//
// Both rows now lay the right-hand chrome out first and give the name what is left, so
// a name that outgrows its column scrolls instead of overprinting. That is the safety
// net; this is the budget. A name held to the room its own worst chrome leaves is one a
// player reads at a glance instead of waiting out a marquee for.
//
// Measured through the same arithmetic the two rows use, so it cannot drift from them:
//   * SPECIES is a drawLabelValue pair at kMargin — name + tag <= kActiveW - 3*kMargin.
//   * STAT lays out stage, then GEN beside it with a 6px gap, and the name takes the
//     rest between the margins.
void test_creature_name_headers_pack() {
    const ContentRegistry reg = ContentRegistry::embedded();
    constexpr int kPairRoom = kActiveW - 3 * kMargin;

    for (const CreatureDef* c : reg.allCreatures()) {
        // SPECIES: the widest tag any line can put beside this name is its own.
        char tag[24];
        std::snprintf(tag, sizeof(tag), "%s", c->line ? c->line : "");
        for (char* p = tag; *p; ++p)
            *p = static_cast<char>(std::toupper(static_cast<unsigned char>(*p)));
        CHECK(textWidth(c->displayName) + textWidth(tag) <= kPairRoom);

        // STAT: name against GEN alone, since the stage moved down to the indicator.
        // GEN runs to two digits before a player is likely to see a third.
        const int genX = kActiveW - kMargin - textWidth("GEN 99");
        CHECK(textWidth(c->displayName) <= genX - 2 * kMargin);

        // The stage word the indicator now carries, under the leftmost node — the
        // widest of the four starting from the furthest-left x.
        for (int s = 0; s <= static_cast<int>(Stage::Daemon); ++s)
            CHECK(kMargin + textWidth(stageName(static_cast<Stage>(s))) <=
                  kActiveW - kMargin);
    }
}

// STAT's INDEX: the jump list held-B opens over the six pages. It is the answer to the
// reader's one real cost — a levelled, kitted pet is a lap of six pages and several
// windows of each to look at any one thing — so what it has to prove is that a jump
// LANDS: on the page a row names, and on the SECTION of it the row is anchored to.
//
// The landing itself stays free: entering STAT opens VITALS with no menu in front of it,
// which is what makes hunger and frag a two-press check, and the index is what the rarer
// long jump costs instead.
void test_stat_index_jumps_to_a_page_and_a_section() {
    Game g{StartMode::Hatched};
    g.debugAddCombatXp(400000);              // far enough up the ladder for held rungs
    enterSubmenuId(g, SubmenuId::Stat);

    // The front door: VITALS, unscrolled, no index.
    CHECK(g.statPage() == 0);
    CHECK(g.statScreen() == StatScreen::Page);

    // A tap of B is the window advance, not the index — the two share the key and only
    // the dwell tells them apart.
    uint32_t t = 0;
    g.tick(t);
    g.onButton(press(Button::B));
    g.tick(t += kStatIndexHoldMs / 2);
    g.onButton(lift(Button::B));
    CHECK(g.statScreen() == StatScreen::Page);

    // Held past the dwell, it opens the index — with the cursor already on what was
    // being read, so backing straight out is a no-op rather than a jump.
    g.onButton(press(Button::B));
    g.tick(t += kStatIndexHoldMs + kHeartbeatMs);
    CHECK(g.statScreen() == StatScreen::Index);
    g.onButton(lift(Button::B));
    auto rows = g.statIndexRows();
    CHECK(rows[g.statIndexRow()].page == 0);          // parked on VITALS
    tapC(g);                                          // C leaves the index, not STAT
    CHECK(g.statScreen() == StatScreen::Page);
    CHECK(g.statPage() == 0);
    CHECK(g.nav() == Game::Nav::Submenu);

    // The roster: every page is reachable, and the two flowed ones carry a sub-row per
    // section — one per combat stat, and the loadout's two halves.
    int pages[kStatPages] = {0};
    int subs = 0;
    for (const StatIndexRow& r : rows) {
        CHECK(r.page >= 0 && r.page < kStatPages);
        ++pages[r.page];
        if (r.sub) ++subs;
    }
    for (int i = 0; i < kStatPages; ++i) CHECK(pages[i] >= 1);
    CHECK(subs == kLevelStatCount + 2);

    // Open the LAST stat's section off the index: the page it names, anchored at that
    // stat's own heading — the whole point, since walking there costs several windows.
    int target = -1;
    for (int i = 0; i < static_cast<int>(rows.size()); ++i)
        if (rows[i].sub && rows[i].page == 1) target = i;
    CHECK(target >= 0);
    const int anchor = rows[target].anchor;

    g.onButton(press(Button::B));
    g.tick(t += kStatIndexHoldMs + kHeartbeatMs);
    g.onButton(lift(Button::B));
    CHECK(g.statScreen() == StatScreen::Index);
    while (g.statIndexRow() != target) g.onButton(press(Button::A));
    tapB(g);
    CHECK(g.statScreen() == StatScreen::Page);
    CHECK(g.statPage() == 1);
    CHECK(g.statScroll() == anchor);

    // And the anchor is a HEADING, so the window opens on the section rather than
    // partway into it. Rebuilt from the pet's own points, which is what the page does.
    int points[kLevelStatCount];
    for (int i = 0; i < kLevelStatCount; ++i) points[i] = g.totalStatPoint(i);
    const auto tierRows = buildTierRows(points);
    CHECK(anchor < static_cast<int>(tierRows.size()));
    CHECK(tierRows[anchor].header);

    // Re-entering STAT still lands on the front door, not on the page last read.
    tapC(g);
    CHECK(g.nav() == Game::Nav::Cursor);
    g.onButton(press(Button::B));
    CHECK(g.statPage() == 0);
    CHECK(g.statScroll() == 0);
    CHECK(g.statScreen() == StatScreen::Page);
}

// A flowed page is read a SECTION at a time. The window rule (prose_page.cpp) says a
// heading ends the window before it once that window is at least half full, which is
// what makes each press of the scroll key land on the next group — and what keeps a
// heading from being stranded at the foot of a screen with its own rows on the next one.
// The half-full clause is the other half of it: a section longer than one window leaves
// a tail behind, and breaking after a one-row tail would spend a whole screen on it.
void test_prose_windows_break_on_sections() {
    const int points[kLevelStatCount] = {kStatTier2Points, kStatTier2Points,
                                         kStatTier2Points, kStatTier2Points};
    const auto rows = buildTierRows(points);
    const int total = static_cast<int>(rows.size());

    int windows = 0;
    for (int top = 0; top < total; ++windows) {
        const int shown = tierRowsFitting(rows, top);
        CHECK(shown >= 1);                          // the fit never stalls
        // No window ENDS on a heading: a fence with nothing behind it is not a fence.
        CHECK(!rows[top + shown - 1].header || shown == 1);
        top += shown;
    }
    // Every stat's heading opens a window of its own: the sections here are the whole
    // reason a reader presses the key, and there are four of them.
    CHECK(windows >= kLevelStatCount);
    CHECK(proseWindowCount(rows, 26) == windows);
    CHECK(proseWindowIndex(rows, 0, 26) == 0);
}
