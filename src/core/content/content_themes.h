// content_themes.h — the colour sets an operator can OWN and choose between, and what
// each one is unlocked by.
//
// A theme is a PAL_CORE set (assets/PAL_CORE.json's `themes` block) offered as a
// possession. This table is the half of that which is content: the order they are
// listed in on the CFG picker, what each is CALLED where a person reads it, and the one
// line that says how it is come by. Which hexes it is made of stays in the palette, and
// the rule that decides whether it is unlocked yet stays in Game — because that rule is
// a question about a save.
//
// It is the same split content_backgrounds.h makes for places, and for the same reason:
// the render layer should not know what an operator has earned, and the content layer
// should not know what a colour is.
//
// OWNERSHIP IS NOT STORED. A chip theme is unlocked by something the save already
// records — that this DEVICE has, at some point, held the palette chip that carries it
// (Game::hasCollectedItem, the ever-held set the 'Pedia's shelf is drawn from). So the
// chip can be lost, sold or spent and the set stays: what was earned is the ROM having
// been read, not the object still being in the bag. A parallel unlock bitmask would be
// a second copy of a fact already on the blob, and the failure mode of a second copy is
// that it disagrees with the first.
//
// THE PALETTE NAME IS THE SAVE. `palette` is the generated set's own name
// (kPalThemeNames, from the JSON key) and it is what the blob stores — never this
// table's row order, and never the palette's, either of which would re-point somebody's
// chosen theme at its neighbour the moment a set was added in the middle.
//
// Every generated set has exactly one row here and every row names a set that exists;
// test_theme_table_covers_the_palette asserts both, so a theme authored in the JSON and
// forgotten here is a build failure rather than a set nobody can ever pick.
#pragma once

#include <cstdint>

namespace mal {

// How the picker says a set is come by, and how Game decides whether it is.
//
// Two kinds, and the split is deliberate: a set that changes how LEGIBLE the device is
// can never be something you have to be lucky to own, so the accessible ones are Start
// and everything that is a matter of taste is loot.
enum class ThemeSource : uint8_t {
    Start,   // available from the first boot
    Chip,    // the palette chip `earnedById` names has been held at some point
};

struct ThemeDef {
    // The generated set's name — the JSON key, kPalThemeNames' entry, and what the save
    // carries. Resolved to a palette index by palThemeByName.
    const char* palette;
    const char* name;       // what a person reads on the picker
    // The one line under the header while this row is focused. It names where the set
    // comes FROM, which for every chip today is the same place: the container an
    // achievement pays out. A found ROM is not a threshold to be told about, so unlike a
    // background's line there is no deed here to name — the deed is finishing any ladder.
    const char* earnedBy;
    ThemeSource source;
    // Chip: the item id whose ever-held-ness unlocks this set (content_items.cpp).
    // Others: unused. The gate checks the id resolves and that the item is actually in a
    // container's pool, so a set can never be unlockable by an object nothing hands out.
    const char* earnedById = nullptr;
};

// Listed as an operator meets them: the two every device has, then the six a
// commendation pays out, in the order they were authored.
inline constexpr ThemeDef kThemes[] = {
    {"base", "STANDARD", "YOURS FROM THE START", ThemeSource::Start},
    // Never behind a draw. It is the set that exists so the device can be READ — by
    // someone who needs the contrast, or by anyone at all in bright sun — and an
    // accessibility setting you have to be lucky to own is not one.
    {"high-contrast", "HIGH CONTRAST", "YOURS FROM THE START", ThemeSource::Start},

    {"synthwave", "SYNTHWAVE", "LOOT: A COMMENDATION CACHE", ThemeSource::Chip,
     "sunset_rom"},
    {"terminal", "TERMINAL", "LOOT: A COMMENDATION CACHE", ThemeSource::Chip,
     "phosphor_tube"},
    {"amber", "AMBER", "LOOT: A COMMENDATION CACHE", ThemeSource::Chip,
     "amber_tube"},
    {"daylight", "DAYLIGHT", "LOOT: A COMMENDATION CACHE", ThemeSource::Chip,
     "daylight_filter"},
    {"dot-matrix", "DOT MATRIX", "LOOT: A COMMENDATION CACHE", ThemeSource::Chip,
     "pocket_lcd"},
    {"night-vision", "NIGHT VISION", "LOOT: A COMMENDATION CACHE", ThemeSource::Chip,
     "redshift_lens"},
};
inline constexpr int kThemeCount = sizeof(kThemes) / sizeof(kThemes[0]);

// The row a palette chip carries, or nullptr for an item that is not one. The one
// question the ITEMS gate and the loot roll both ask — a chip is inert in the bag and
// stops being loot once its set is unlocked, and both of those start here.
inline const ThemeDef* themeForChip(const char* itemId) {
    if (!itemId) return nullptr;
    for (const ThemeDef& t : kThemes) {
        if (t.source != ThemeSource::Chip || !t.earnedById) continue;
        const char* a = t.earnedById;
        const char* b = itemId;
        while (*a && *a == *b) { ++a; ++b; }
        if (!*a && !*b) return &t;
    }
    return nullptr;
}

}  // namespace mal
