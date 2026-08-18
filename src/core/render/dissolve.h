// dissolve.h — the scatter every "a sprite comes apart" effect is derived from.
//
// FX_ABSORB (absorb.h) and FX_SHRED (shred.h) both need a stable pseudo-random value
// per source pixel or per source row: it is what makes a departure order, a slide
// direction or a flight arc a fixed property of the thing coming apart rather than
// state someone has to store. Sharing it also means the two effects scatter the same
// way, so a screen that can play either does not look like it swapped renderers.
//
// The property to protect: an effect built on this is a pure function of the source
// pixel and the sweep position. It allocates nothing, stores nothing, and draws the
// same frame twice for the same progress — which is what lets a ~4fps event-driven
// repaint run it and lets the frame gates dump it.
#pragma once

#include <cstdint>

namespace mal {

inline uint32_t dissolveHash(int a, int b) {
    uint32_t h = static_cast<uint32_t>(a) * 374761393u +
                 static_cast<uint32_t>(b) * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

} // namespace mal
