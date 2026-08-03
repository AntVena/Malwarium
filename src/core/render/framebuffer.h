// framebuffer.h — the logical RGB565 canvas the engine draws into (128x128).
#pragma once

#include <cstdint>
#include <vector>

#include "core/render/color.h"

namespace mal {

class Framebuffer {
public:
    Framebuffer(int w, int h) : w_(w), h_(h), px_(static_cast<size_t>(w) * h) {}

    int width() const { return w_; }
    int height() const { return h_; }

    Rgb565* data() { return px_.data(); }
    const Rgb565* data() const { return px_.data(); }

    bool inBounds(int x, int y) const { return x >= 0 && y >= 0 && x < w_ && y < h_; }

    void clear(Rgb565 c) {
        for (auto& p : px_) p = c;
    }

    void set(int x, int y, Rgb565 c) {
        if (inBounds(x, y)) px_[static_cast<size_t>(y) * w_ + x] = c;
    }

    // Blend src over the existing pixel with 8-bit coverage.
    void blendPixel(int x, int y, Rgb565 src, uint8_t a) {
        if (!inBounds(x, y)) return;
        auto& dst = px_[static_cast<size_t>(y) * w_ + x];
        dst = blend(dst, src, a);
    }

    Rgb565 get(int x, int y) const {
        return inBounds(x, y) ? px_[static_cast<size_t>(y) * w_ + x] : 0;
    }

    void fillRect(int x, int y, int w, int h, Rgb565 c) {
        for (int yy = y; yy < y + h; ++yy)
            for (int xx = x; xx < x + w; ++xx) set(xx, yy, c);
    }

private:
    int w_, h_;
    std::vector<Rgb565> px_;
};

} // namespace mal
