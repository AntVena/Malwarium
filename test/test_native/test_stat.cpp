// test_stat.cpp — native gates for STAT paging, the loadout readout and the effect-text budget.
//
// One slice of the native suite; see test_gates.h for the shared fixtures and
// test_main.cpp for the roster that runs these. A save-vNN gate sits with the
// feature whose field it migrates, not in a migrations pile of its own.
#include "test_gates.h"

// STAT is 5 paged screens — 0 pet VITALS (the landing, + an XP bar), 1 LOADOUT
// (the equipped moves + mods, WITH their effect text), 2 BUFFS (armed item
// buffs), 3 SPECIES (the pet's own lore), 4 AUDIT LOG — cycled by A, and C
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

    // A -> page 1 = LOADOUT (a distinct page).
    g.onButton(press(Button::A));
    Framebuffer p1(kActiveW, kActiveH);
    g.render(p1);
    CHECK(!fbEqual(p0, p1));

    // A -> page 2 = BUFFS (distinct), then page 3 = SPECIES, page 4 = audit
    // log, A -> wraps back to page 0 = VITALS.
    g.onButton(press(Button::A));
    Framebuffer p2(kActiveW, kActiveH);
    g.render(p2);
    CHECK(!fbEqual(p1, p2));
    g.onButton(press(Button::A));   // -> page 3 SPECIES
    g.onButton(press(Button::A));   // -> page 4 AUDIT LOG
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
    // the Quick Jab fallback. So there's a default (isDefault) row, it names Quick Jab and
    // carries its effect, and it appears AFTER the equipped move (in slot order) — NOT as a
    // leading fixture. The first non-header row is the equipped move, not the default.
    const LoadoutRow* firstMove = nullptr;
    const LoadoutRow* defaultRow = nullptr;
    for (const auto& row : rows) {
        if (row.header) continue;
        if (!firstMove) firstMove = &row;
        if (row.isDefault && !defaultRow) defaultRow = &row;
    }
    CHECK(firstMove && !firstMove->isDefault);            // equipped move leads, not the default
    const MoveDef* def = r.move(ml.defaultMove());
    CHECK(defaultRow && def && std::strcmp(defaultRow->label, def->displayName) == 0);
    CHECK(defaultRow && !defaultRow->effect.empty() &&
          std::strcmp(defaultRow->effect.c_str(), effectText(*def).c_str()) == 0);

    // Every UNLOCKED slot yields one row: equipped moves by name+effect, empty slots as
    // the default. Exact row count = 2 headers + (one row per unlocked slot) + mod rows.
    auto containsNameAndEffect = [&](const char* name, const EffectText& effect) {
        for (const auto& row : rows)
            if (!row.header && std::strcmp(row.label, name) == 0 &&
                !row.effect.empty() &&
                std::strcmp(row.effect.c_str(), effect.c_str()) == 0)
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
    CHECK(eggRows.size() == 1 && !eggRows[0].header && eggRows[0].effect.empty());
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
void test_stat_loadout_b_scroll() {
    Game g{StartMode::Hatched};
    enterSubmenuId(g, SubmenuId::Stat);

    // Page 0 (VITALS): B is a no-op.
    Framebuffer v0(kActiveW, kActiveH); g.render(v0);
    g.onButton(press(Button::B));
    Framebuffer v0b(kActiveW, kActiveH); g.render(v0b);
    CHECK(fbEqual(v0, v0b));

    // A -> page 1 (LOADOUT). Confirm the starting loadout really does overflow
    // the visible window (the precondition this test needs).
    g.onButton(press(Button::A));
    auto rows = buildLoadoutRows(g.content(), g.moveLoadout(), g.loadout(),
                                 g.pet()->stage, g.inEggPhase());
    CHECK(loadoutRowsFitting(rows, 0) < static_cast<int>(rows.size()));

    Framebuffer l0(kActiveW, kActiveH); g.render(l0);
    g.onButton(press(Button::B));               // scroll forward one window
    Framebuffer l1(kActiveW, kActiveH); g.render(l1);
    CHECK(!fbEqual(l0, l1));                     // the row window actually moved

    // Keep pressing to the end of the list: every window is a fresh view, and the
    // one past the last wraps home. How many windows that takes depends on how tall
    // the rows are (each is sized to its own prose), so the walk is bounded by the
    // row count rather than assuming a fixed window.
    bool wrapped = false;
    for (int i = 0; i < static_cast<int>(rows.size()) && !wrapped; ++i) {
        g.onButton(press(Button::B));
        Framebuffer ln(kActiveW, kActiveH); g.render(ln);
        wrapped = fbEqual(l0, ln);
    }
    CHECK(wrapped);

    // C -> back to the carousel (resets statPage_/statScroll_); B re-enters
    // the submenu (cursor is still parked on STAT's slot); A -> page 1 confirms
    // the scroll reset (renders identically to the very first page-1 view, l0).
    g.onButton(press(Button::C));
    g.onButton(press(Button::B));
    g.onButton(press(Button::A));
    Framebuffer l3(kActiveW, kActiveH); g.render(l3);
    CHECK(fbEqual(l0, l3));

    // Page 2 (BUFFS) flows rows too, but with nothing armed it has none to scroll,
    // so B is inert there; page 4 (AUDIT LOG) never scrolls at all.
    g.onButton(press(Button::A));                // page 1 -> page 2
    Framebuffer b0(kActiveW, kActiveH); g.render(b0);
    g.onButton(press(Button::B));
    Framebuffer b0b(kActiveW, kActiveH); g.render(b0b);
    CHECK(fbEqual(b0, b0b));

    g.onButton(press(Button::A));                // -> page 3 (SPECIES)
    g.onButton(press(Button::A));                // -> page 4 (AUDIT LOG)
    Framebuffer a0(kActiveW, kActiveH); g.render(a0);
    g.onButton(press(Button::B));
    Framebuffer a0b(kActiveW, kActiveH); g.render(a0b);
    CHECK(fbEqual(a0, a0b));
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

// The permanent upgrade has to be READABLE, or a player has no way to know a pet
// carries it: nothing else reports it — no home-screen icon (it never lapses and asks
// nothing of the operator), no shop row, no timer. The BUFFS page is the whole surface.
void test_buffs_page_lists_the_bandwidth_upgrade() {
    ContentRegistry r = ContentRegistry::embedded();
    // Nothing armed, and not upgraded: the page is empty.
    CHECK(buildBuffRows(r, false, false, false, 0, 1, false, false, 0, false).empty());

    const std::vector<BuffRow> rows =
        buildBuffRows(r, false, false, false, 0, 1, false, false, 0, true);
    CHECK(rows.size() == 1);
    // Named by the DISH that grants it, resolved off the item table rather than a
    // literal here — renaming the dish must not need a test edit to go with it.
    const ItemDef* d = nullptr;
    for (const ItemDef* it : r.allItems())
        for (const ItemEffect& e : it->effects)
            if (e.kind == ItemEffect::Kind::BandwidthRegenBonusMin) d = it;
    CHECK(d && std::strcmp(rows[0].label, d->displayName) == 0);
    CHECK(!rows[0].hasTimer);        // it never lapses, so it carries no countdown
}
