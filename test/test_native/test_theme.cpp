// test_theme.cpp — native gates for the PAL_CORE themes: the ladders every set has to
// hold, the table that offers them, and what unlocks each.
//
// One slice of the native suite; see test_gates.h for the shared fixtures and
// test_main.cpp for the roster that runs these. A save-vNN gate sits with the feature
// whose field it migrates, not in a migrations pile of its own.
//
// Why this file exists at all: every other grayscale gate renders in whatever theme
// happens to be active, which is the base set, so it says nothing about the other rows
// of the table. A theme that broke a ladder would ship green — and the screens it broke
// would be exactly the ones with no words on them to fall back on.
#include "test_gates.h"

#include "core/content/content_themes.h"  // kThemes — the sets offered, and their unlocks
#include "core/model/chromatophore.h"     // kChromaSkins — the camo ladder's length
#include "core/model/disk_decryption.h"   // kDecryptionColours — and the code ladder's
#include "core/ui/layout.h"               // kMargin — the picker row's own text budget

namespace {

// Every ladder below is stated WITHOUT A POLARITY, and that is the point of this file
// rather than an accident of how it is written. A light theme (`daylight`, `dot-matrix`)
// runs every one of them the other way up: its paper is the bright end, its status tints
// are dark because they are read on white, and its decryption five descend from a rung
// just off the page instead of climbing to one. None of that is a different KIND of
// legibility — the separations are identical — so a gate phrased as "ascending, and the
// top clears 0.8" would fail a set that is exactly as readable as the base one, and go
// on to reject the whole idea of a light theme on a technicality about which end is up.
//
// Luminance is read off the RGB565 the panel is actually sent, not the authored hex,
// because that quantisation is what a player sees — and 5 bits of blue is enough to
// close a margin an author measured in 8.
float themeLum(int theme, Pal p) { return luminance(palColorIn(theme, p)); }

}  // namespace

// Every theme, against every ladder the base set is already held to. The thresholds are
// the ones the per-screen gates use (test_decryption_grayscale, test_chroma_grayscale,
// test_palette_luminance_ordered) rather than a stricter set of this file's own: a theme
// is not a lesser citizen, and a bar only this gate enforced would be a second, quieter
// standard for the same screens.
void test_theme_invariants() {
    CHECK(kPalThemeCount >= 1);
    for (int t = 0; t < kPalThemeCount; ++t) {
        CHECK(kPalThemeNames[t] != nullptr && kPalThemeNames[t][0] != '\0');
        for (int u = t + 1; u < kPalThemeCount; ++u)   // a picker row needs its own name
            CHECK(std::strcmp(kPalThemeNames[t], kPalThemeNames[u]) != 0);
        // The palette name is what the save carries, so it has to fit the blob's cell —
        // a name too long would come back truncated and resolve to a different set.
        CHECK(std::strlen(kPalThemeNames[t]) < static_cast<size_t>(kSaveIdCap));

        auto lum = [t](Pal p) { return themeLum(t, p); };
        const float paper = lum(Pal::PAPER), ink = lum(Pal::INK);
        const float near = std::fmin(ink, paper), far = std::fmax(ink, paper);

        // Status: calm > warn > hot, monotonic, so a gauge's band reads as a grey.
        CHECK(lum(Pal::HOT) < lum(Pal::WARN));
        CHECK(lum(Pal::WARN) < lum(Pal::CALM));
        // ...and all three are drawn ON paper, as fills and as text tints, so each has
        // to stand off it whichever end of the range the page is.
        CHECK(std::fabs(lum(Pal::CALM) - paper) > 0.2f);
        CHECK(std::fabs(lum(Pal::WARN) - paper) > 0.2f);
        CHECK(std::fabs(lum(Pal::HOT) - paper) > 0.2f);

        // Ink on paper is the highest-contrast pair on the device — text lives on it.
        CHECK(far - near > 0.5f);
        // ...and the dim tier is a THIRD value, not a synonym for either: a gated row
        // has to read as text and still read as gated.
        CHECK(lum(Pal::INK_DIM) > near + 0.1f);
        CHECK(lum(Pal::INK_DIM) < far - 0.1f);
        // The focus hue is drawn on paper too — a cursor box that sank into the page
        // would take the one channel that says where you are.
        CHECK(std::fabs(lum(Pal::ACCENT) - paper) > 0.25f);

        // The decryption five, whose only non-colour channel is this ladder: nothing is
        // written on a cell, so a theme that collapsed two rungs would make the board
        // unplayable in grayscale without failing anything else. Monotonic in whichever
        // direction the set runs it, evenly enough spaced that no two rungs meet, with
        // the rung NEAREST the page clear of it and the far one actually using the range.
        const Pal five[kDecryptionColours] = {Pal::DECRYPTION_PURPLE, Pal::DECRYPTION_BLUE,
                                              Pal::DECRYPTION_GREEN, Pal::DECRYPTION_ORANGE,
                                              Pal::DECRYPTION_WHITE};
        float nearest = lum(five[0]), farthest = lum(five[0]), prev = lum(five[0]);
        for (int i = 1; i < kDecryptionColours; ++i) {
            const float l = lum(five[i]);
            CHECK(l - prev > 0.12f);                 // authored low-to-high, and separated
            prev = l;
            if (std::fabs(l - paper) < std::fabs(nearest - paper)) nearest = l;
            if (std::fabs(l - paper) > std::fabs(farthest - paper)) farthest = l;
        }
        CHECK(std::fabs(nearest - paper) > 0.08f);
        CHECK(std::fabs(farthest - paper) > 0.55f);

        // The camo three, on the same footing and for the same reason — and this one is
        // the tightest fit any set has to make: three rungs 0.15 apart, both ends clear
        // of ink and paper by 0.15, inside whatever range the theme left between them.
        const Pal skins[kChromaSkins] = {Pal::CAMO_KELP, Pal::CAMO_SILT, Pal::CAMO_BLOOM};
        float lo = lum(skins[0]), hi = lum(skins[0]);
        prev = lum(skins[0]);
        for (int i = 1; i < kChromaSkins; ++i) {
            const float l = lum(skins[i]);
            CHECK(l - prev > 0.15f);
            prev = l;
            lo = std::fmin(lo, l);
            hi = std::fmax(hi, l);
        }
        CHECK(lo > near + 0.15f);
        CHECK(hi < far - 0.15f);

        // The held-notice plate: `paper` is drawn as TEXT on it, so the two have to be
        // a legible pair wherever the theme puts them — and it is not `warn`, which is
        // the distinction the token exists to carry.
        CHECK(std::fabs(lum(Pal::NOTICE_HOLD) - paper) > 0.3f);
        CHECK(std::fabs(lum(Pal::NOTICE_HOLD) - lum(Pal::WARN)) > 0.05f);

        // The rarity ramp is read by POSITION, but the top tier still has to be the one
        // that leaps off the page — which on a light theme means darker, not brighter.
        CHECK(std::fabs(lum(Pal::RARITY_EPIC) - paper) >
              std::fabs(lum(Pal::RARITY_COMMON) - paper) + 0.05f);
    }
}

// The content table and the generated palette are two lists that have to name the same
// sets. A theme authored in PAL_CORE.json and forgotten in content_themes.h would be a
// set nobody could ever pick; a row naming a set the JSON doesn't have would be a picker
// row that drew the base palette and lied about it.
void test_theme_table_covers_the_palette() {
    CHECK(kThemeCount == kPalThemeCount);
    for (int i = 0; i < kThemeCount; ++i) {
        const ThemeDef& t = kThemes[i];
        // palThemeByName falls back to 0 for a name it doesn't know, so "resolves to 0"
        // is only honest for the row that IS base — every other row must resolve to a
        // set of its own.
        const int p = palThemeByName(t.palette);
        CHECK(p == 0 ? std::strcmp(t.palette, kPalThemeNames[0]) == 0 : true);
        for (int j = i + 1; j < kThemeCount; ++j)
            CHECK(palThemeByName(kThemes[j].palette) != p);

        // The name and the earned-by line are the only two things a locked row tells an
        // operator, so both have to fit the row they are drawn on.
        CHECK(t.name && *t.name && t.earnedBy && *t.earnedBy);
        CHECK(22 + textWidth(t.name) < kActiveW - kMargin - textWidth("LOCKED") - kFontW);
        CHECK(kMargin + textWidth(t.earnedBy) <= kActiveW - kMargin);

        if (t.source != ThemeSource::Chip) continue;
        // A chip row's item has to exist, has to be worth finding (Rare or Epic — a
        // permanent unlock is not a Common), and has to actually be handed out by
        // something: an unlock nothing drops is a row that can never be reached.
        const ItemDef* d = ContentRegistry::embedded().item(t.earnedById);
        CHECK(d != nullptr);
        if (!d) continue;
        CHECK(d->rarity == ItemDef::Rarity::Rare || d->rarity == ItemDef::Rarity::Epic);
        CHECK(themeForChip(d->id) == &t);          // ...and the reverse lookup agrees
        // A shelf of six identical blanks would be six items nobody could tell apart:
        // itemIcon derives ICON_ITEM_<ID>, so a chip added without a recipe in
        // tools/gen_item_icons.py fails here rather than on the ITEMS row.
        CHECK(itemIcon(ContentRegistry::embedded(), d->id) != nullptr);
        bool pooled = false;
        for (int k = 0; k < kItemsCount && !pooled; ++k)
            for (int e = 0; e < kItems[k].cache.poolSize; ++e)
                if (std::strcmp(kItems[k].cache.pool[e].id, d->id) == 0) { pooled = true; break; }
        CHECK(pooled);
    }
    // Every device has at least one set that is not behind a draw, and the accessible
    // ones are among them: a legibility setting you have to be lucky to own is not one.
    CHECK(kThemes[0].source == ThemeSource::Start);
    for (int i = 0; i < kThemeCount; ++i)
        if (std::strcmp(kThemes[i].palette, "high-contrast") == 0)
            CHECK(kThemes[i].source == ThemeSource::Start);
}

// The picker moves the palette index, and nothing else does: applying a theme restyles
// the live interface, and the frame is marked dirty because every pixel of it changed
// even though no state a screen reads did.
void test_cfg_theme_picker_applies() {
    Game g{StartMode::Hatched};
    CHECK(g.themePick() == 0);                     // a fresh device is the base set
    CHECK(palThemeIndex() == 0);
    CHECK(g.themeRow() == 0);
    CHECK(std::strcmp(g.themeName(), kPalThemeNames[0]) == 0);

    // Row 1 is the other set every device has, so it is the one a fresh save can apply.
    const int row = 1;
    CHECK(g.themeRowUnlocked(row));
    const int palette = palThemeByName(kThemes[row].palette);

    enterCfgTarget(g, CfgScreen::Theme);           // CFG -> DEVICE -> THEME
    CHECK(g.nav() == Game::Nav::Detail);
    CHECK(g.cfgScreen() == CfgScreen::Theme);

    g.onButton(press(Button::A));                  // focus the next set...
    CHECK(g.themePick() == 0);                     // ...which is not applying it
    CHECK(palThemeIndex() == 0);
    g.onButton(press(Button::B));                  // apply -> back to DEVICE
    CHECK(g.cfgScreen() == CfgScreen::Device);
    CHECK(g.themePick() == palette);
    CHECK(palThemeIndex() == palette);             // the whole interface, restyled
    CHECK(g.themeRow() == row);
    CHECK(std::strcmp(g.themeName(), kThemes[row].palette) == 0);

    // C leaves the pick where it was, like every other CFG picker.
    enterCfgTarget(g, CfgScreen::Theme);
    g.onButton(press(Button::A));
    tapC(g);
    CHECK(g.themePick() == palette);
    CHECK(palThemeIndex() == palette);

    // A wraps rather than stopping at the end of the table.
    enterCfgTarget(g, CfgScreen::Theme);
    for (int i = 0; i < kThemeCount; ++i) g.onButton(press(Button::A));
    g.onButton(press(Button::B));
    CHECK(g.themePick() == palette);               // all the way round, back to itself

    setPalTheme(0);                                // leave the process in the base set
}

// A locked set is walked onto and refused — the picker's A steps over every row so the
// line under the header can say where each one comes from, and B is what says no.
void test_cfg_theme_picker_refuses_a_locked_set() {
    Game g{StartMode::Hatched};
    int locked = -1;
    for (int i = 0; i < kThemeCount && locked < 0; ++i)
        if (!g.themeRowUnlocked(i)) locked = i;
    CHECK(locked > 0);                             // a fresh device has some to earn

    enterCfgTarget(g, CfgScreen::Theme);
    for (int i = 0; i < locked; ++i) g.onButton(press(Button::A));
    g.onButton(press(Button::B));                  // ...and B does nothing at all
    CHECK(g.cfgScreen() == CfgScreen::Theme);      // the screen stays, no modal
    CHECK(g.themePick() == 0);
    CHECK(palThemeIndex() == 0);

    // The engine refuses it from the other side too, so nothing that skips the picker
    // can hand the device a set it hasn't earned.
    CHECK(!g.setThemePick(palThemeByName(kThemes[locked].palette)));
    CHECK(g.themePick() == 0);
    setPalTheme(0);
}

// The chip is the unlock, and HOLDING it ever is what counts: the set stays once the bag
// is empty again, because what was earned is the ROM having been read.
void test_theme_chip_unlocks_its_set() {
    Game g{StartMode::Hatched};
    int chipRow = -1;
    for (int i = 0; i < kThemeCount && chipRow < 0; ++i)
        if (kThemes[i].source == ThemeSource::Chip) chipRow = i;
    CHECK(chipRow >= 0);
    const ThemeDef& t = kThemes[chipRow];
    CHECK(!g.themeRowUnlocked(chipRow));
    CHECK((g.themesUnlockedMask() & (1u << chipRow)) == 0);

    g.inventory().add(t.earnedById, 1);
    g.tick(kHeartbeatMs);                          // the sweep folds the bag into the shelf
    CHECK(g.hasCollectedItem(t.earnedById));
    CHECK(g.themeRowUnlocked(chipRow));
    CHECK((g.themesUnlockedMask() & (1u << chipRow)) != 0);

    // ...and now it applies.
    const int palette = palThemeByName(t.palette);
    CHECK(g.setThemePick(palette));
    CHECK(palThemeIndex() == palette);

    // Spend the chip and the set stays: the unlock is the ever-held shelf, not the bag.
    g.inventory().remove(t.earnedById, 1);
    CHECK(g.inventory().count(t.earnedById) == 0);
    CHECK(g.themeRowUnlocked(chipRow));
    setPalTheme(0);
}

// The chips are real loot — a commendation actually pays them — and then they stop
// being loot rather than costing one a real prize: a chip already held drops to weight
// zero, so a device that has every set draws from the pool it started with.
void test_theme_chips_drop_then_leave_the_pool() {
    {   // They come out of the box the achievements pay.
        Game g{StartMode::Hatched};
        int found = 0;
        for (int i = 0; i < 200 && found == 0; ++i) {
            g.inventory().add("commend_cache", 1);
            g.debugOpenCache("commend_cache");
            tapC(g);                                   // dismiss the yield
            for (int r = 0; r < kThemeCount; ++r)
                if (kThemes[r].source == ThemeSource::Chip &&
                    g.inventory().count(kThemes[r].earnedById) > 0) ++found;
        }
        CHECK(found > 0);
    }
    {   // ...and once every set is in, two hundred more never waste a draw on one.
        Game g{StartMode::Hatched};
        for (int r = 0; r < kThemeCount; ++r)
            if (kThemes[r].source == ThemeSource::Chip)
                g.inventory().add(kThemes[r].earnedById, 1);
        g.tick(kHeartbeatMs);                          // fold the bag into the shelf
        for (int r = 0; r < kThemeCount; ++r)
            if (kThemes[r].source == ThemeSource::Chip)
                g.inventory().remove(kThemes[r].earnedById, 1);

        for (int i = 0; i < 200; ++i) {
            g.inventory().add("commend_cache", 1);
            g.debugOpenCache("commend_cache");
            tapC(g);
        }
        for (int r = 0; r < kThemeCount; ++r)
            if (kThemes[r].source == ThemeSource::Chip)
                CHECK(g.inventory().count(kThemes[r].earnedById) == 0);
    }
    setPalTheme(0);
}

// A theme survives a reboot, and survives its own row moving. The blob stores the NAME
// (save v64), so what a device restores is the set it was told to draw in rather than
// whatever now sits at that index — and a name this build has no set for falls back to
// base rather than to nothing.
void test_theme_persists_by_name() {
    MemSaveStore store;
    const int row = 1;                             // the second set every device has
    const int palette = palThemeByName(kThemes[row].palette);
    {
        Game g(StartMode::Hatched, "paypup", &store);
        enterCfgTarget(g, CfgScreen::Theme);
        for (int i = 0; i < row; ++i) g.onButton(press(Button::A));
        g.onButton(press(Button::B));
        CHECK(g.themePick() == palette);
        g.tick(kSaveAutosaveMs + kHeartbeatMs);    // persist the pick
    }
    setPalTheme(0);                                // a cold boot starts in the base set
    {
        Game g(StartMode::Hatched, "paypup", &store);
        CHECK(g.themePick() == palette);
        CHECK(palThemeIndex() == palette);         // ...and the load APPLIED it
    }

    // What the blob actually carries is the name, so it reads the same however the
    // table is ordered — and an unknown one is base, the set every build has.
    SaveData d;
    CHECK(deserializeSave(store.bytes(), d));
    CHECK(std::strcmp(d.theme, kThemes[row].palette) == 0);
    CHECK(palThemeByName(d.theme) == palette);
    CHECK(palThemeByName("a-set-this-build-does-not-ship") == 0);
    CHECK(palThemeByName("") == 0);
    CHECK(palThemeByName(nullptr) == 0);

    // A pre-v64 blob names no theme at all, which is the same answer.
    d.theme[0] = '\0';
    std::vector<uint8_t> blob = serializeSave(d);
    SaveData back;
    CHECK(deserializeSave(blob, back));
    CHECK(palThemeByName(back.theme) == 0);
    setPalTheme(0);
}

// A save that names a set this DEVICE has not unlocked lands on base. It cannot happen
// through the picker, which refuses one; it can happen to a blob moved between devices,
// and the answer is the set every build has rather than a theme nobody earned.
void test_theme_load_falls_back_when_locked() {
    MemSaveStore store;
    int chipRow = -1;
    for (int i = 0; i < kThemeCount && chipRow < 0; ++i)
        if (kThemes[i].source == ThemeSource::Chip) chipRow = i;
    CHECK(chipRow >= 0);
    {
        Game g(StartMode::Hatched, "paypup", &store);
        g.tick(kSaveAutosaveMs + kHeartbeatMs);
    }
    SaveData d;
    CHECK(deserializeSave(store.bytes(), d));
    std::snprintf(d.theme, sizeof(d.theme), "%s", kThemes[chipRow].palette);
    CHECK(store.save(serializeSave(d)));

    Game g(StartMode::Hatched, "paypup", &store);
    CHECK(g.themePick() == 0);
    CHECK(palThemeIndex() == 0);
    setPalTheme(0);
}

// The picker draws each set in ITSELF — the swatch strips are the only thing on the
// screen that isn't in the live theme — and still reads with the colour taken away.
void test_theme_picker_previews_each_set() {
    Framebuffer fb(kActiveW, kActiveH);
    Game g{StartMode::Hatched};
    enterCfgTarget(g, CfgScreen::Theme);

    // Walk the whole table a row at a time; every set's own paper has to appear on the
    // screen while its row is on it. The rows are painted from palColorIn, so a picker
    // that had quietly gone back through palColor would draw identical strips and fail.
    for (int r = 0; r < kThemeCount; ++r) {
        g.render(fb);
        CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));   // the names + the row cursor
        const Rgb565 want = palColorIn(palThemeByName(kThemes[r].palette), Pal::PAPER);
        bool found = false;
        for (int y = 0; y < kActiveH && !found; ++y)
            for (int x = 0; x < kActiveW; ++x)
                if (fb.get(x, y) == want) { found = true; break; }
        CHECK(found);                                      // the focused row is on screen
        g.onButton(press(Button::A));
    }
    setPalTheme(0);
}
