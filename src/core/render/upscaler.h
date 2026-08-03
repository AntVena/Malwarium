// upscaler.h — whole-canvas integer-ratio nearest-neighbour upscale.
//
// The engine renders at 128 logical and upscales the entire canvas by num/den
// (7/4 = x1.75 -> 224) with NO downscaling, preserving clean logical-pixel
// boundaries (05).
#pragma once

#include <vector>

#include "core/render/color.h"
#include "core/render/framebuffer.h"

namespace mal {

inline int scaledExtent(int n, int num, int den) { return n * num / den; }

// Upscale `src` into `out` (resized to src*num/den). Pixel (ox,oy) samples
// source (ox*den/num, oy*den/num) — a deterministic 2,2,2,1 expansion for 7/4.
inline void upscale(const Framebuffer& src, std::vector<Rgb565>& out,
                    int num, int den) {
    const int sw = src.width(), sh = src.height();
    const int dw = scaledExtent(sw, num, den);
    const int dh = scaledExtent(sh, num, den);
    out.resize(static_cast<size_t>(dw) * dh);
    for (int oy = 0; oy < dh; ++oy) {
        int sy = oy * den / num;
        const Rgb565* srow = src.data() + static_cast<size_t>(sy) * sw;
        Rgb565* orow = out.data() + static_cast<size_t>(oy) * dw;
        for (int ox = 0; ox < dw; ++ox) orow[ox] = srow[ox * den / num];
    }
}

} // namespace mal
