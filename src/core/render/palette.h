// palette.h — engine-facing access to the PAL_CORE role tokens.
#pragma once

#include "core/render/color.h"
#include "generated/pal_core.h"

namespace mal {

// The active theme — one index into the generated [theme][token] tables.
//
// Every colour on the device is fetched through palColor() below, so this single
// int is the whole theme switch: point it at another row and the entire interface
// restyles on the next repaint, with no drawing call anywhere aware it happened.
// That is the property to protect. A new theme (a colourblind-friendly set, a
// high-contrast set) is a block in assets/PAL_CORE.json naming only the tokens it
// changes, plus whatever chooses this index — never a sweep through the screens.
//
// What chooses it is CFG · DEVICE · THEME, which stores the pick by NAME (save
// v64) rather than by this index: the index is the JSON's row order, so a theme
// inserted above another would silently restyle every device that had chosen the
// one below it. Game::setThemePick is the only caller — nothing else moves the
// index, so the applied theme and the saved one cannot drift apart.
inline int& palThemeIndex() {
    static int idx = 0;
    return idx;
}

inline void setPalTheme(int i) {
    palThemeIndex() = (i < 0 || i >= kPalThemeCount) ? 0 : i;
}

inline const char* palThemeName() { return kPalThemeNames[palThemeIndex()]; }

// A named theme's index, or 0 (base) for a name no set answers to — which is what
// a save written by a firmware that shipped a theme this one doesn't have carries.
// Falling back to base is the honest read of it: the device knows the pick is not
// available, and base is the set every build has.
inline int palThemeByName(const char* name) {
    if (!name || !*name) return 0;
    for (int i = 0; i < kPalThemeCount; ++i) {
        const char* a = kPalThemeNames[i];
        const char* b = name;
        while (*a && *a == *b) { ++a; ++b; }
        if (!*a && !*b) return i;
    }
    return 0;
}

inline Rgb565 palColor(Pal p) { return kPalRgb565[palThemeIndex()][static_cast<int>(p)]; }
inline float palLum(Pal p) { return kPalLum[palThemeIndex()][static_cast<int>(p)]; }

// The same two lookups against a theme that is NOT the active one. Only two kinds of
// caller want this and both are about themes themselves: the CFG picker, which paints
// a row of swatches in the set it is offering rather than describing it in words, and
// the gate that walks every theme asserting the ladders hold in all of them. Drawing a
// screen still goes through palColor() above — a screen that reached for a theme index
// would be a screen with a colour opinion, which is the thing this file exists to stop.
inline Rgb565 palColorIn(int theme, Pal p) {
    if (theme < 0 || theme >= kPalThemeCount) theme = 0;
    return kPalRgb565[theme][static_cast<int>(p)];
}

inline float palLumIn(int theme, Pal p) {
    if (theme < 0 || theme >= kPalThemeCount) theme = 0;
    return kPalLum[theme][static_cast<int>(p)];
}

// Fragmentation ramp: purple (frag-lo) -> pink (frag-hi) by t in [0,1].
// Fragmentation has its own cross-menu identity, not the calm/warn/hot zones.
inline Rgb565 fragRamp(float t) {
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    Rgb565 lo = palColor(Pal::FRAG_LO);
    Rgb565 hi = palColor(Pal::FRAG_HI);
    auto mix = [t](uint8_t a, uint8_t b) {
        return static_cast<uint8_t>(a + (b - a) * t);
    };
    return rgb565(mix(r8(lo), r8(hi)), mix(g8(lo), g8(hi)), mix(b8(lo), b8(hi)));
}

} // namespace mal
