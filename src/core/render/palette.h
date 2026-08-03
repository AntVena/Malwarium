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
// Deliberately not in the save blob or a settings row yet: nothing chooses a theme
// but the default, and a stored index would be a promise about a menu that doesn't
// exist. Wiring one is a CFG row reading this.
inline int& palThemeIndex() {
    static int idx = 0;
    return idx;
}

inline void setPalTheme(int i) {
    palThemeIndex() = (i < 0 || i >= kPalThemeCount) ? 0 : i;
}

inline const char* palThemeName() { return kPalThemeNames[palThemeIndex()]; }

inline Rgb565 palColor(Pal p) { return kPalRgb565[palThemeIndex()][static_cast<int>(p)]; }
inline float palLum(Pal p) { return kPalLum[palThemeIndex()][static_cast<int>(p)]; }

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
