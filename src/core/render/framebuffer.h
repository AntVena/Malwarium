// framebuffer.h — the logical RGB565 canvas the engine draws into (128x128).
#pragma once

#include <cstdint>
#include <vector>

#include "core/render/color.h"

namespace mal {

class Framebuffer {
public:
    Framebuffer(int w, int h)
        : w_(w), h_(h), cx1_(w), cy1_(h), px_(static_cast<size_t>(w) * h) {}

    int width() const { return w_; }
    int height() const { return h_; }

    Rgb565* data() { return px_.data(); }
    const Rgb565* data() const { return px_.data(); }

    bool inBounds(int x, int y) const { return x >= 0 && y >= 0 && x < w_ && y < h_; }

    // The clip window: while one is set, every WRITE outside it is dropped, so a
    // primitive that would overrun the column it was given stops at the edge
    // instead. Reads are unaffected — get() reports what is on the canvas, which
    // is what the grayscale gates inspect.
    //
    // It exists for text that is longer than its column (widgets.h's
    // drawTextMarquee): the alternative is every caller measuring and truncating,
    // which puts the same rule in a dozen places and drops whole glyphs where a
    // clip drops columns.
    void setClip(int x, int y, int w, int h) {
        cx0_ = x; cy0_ = y; cx1_ = x + w; cy1_ = y + h;
    }
    void clearClip() { cx0_ = cy0_ = 0; cx1_ = w_; cy1_ = h_; }

    void clear(Rgb565 c) {
        for (auto& p : px_) p = c;
    }

    void set(int x, int y, Rgb565 c) {
        if (writable(x, y)) px_[static_cast<size_t>(y) * w_ + x] = c;
    }

    // Blend src over the existing pixel with 8-bit coverage.
    void blendPixel(int x, int y, Rgb565 src, uint8_t a) {
        if (!writable(x, y)) return;
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
    bool writable(int x, int y) const {
        return inBounds(x, y) && x >= cx0_ && x < cx1_ && y >= cy0_ && y < cy1_;
    }

    int w_, h_;
    int cx0_ = 0, cy0_ = 0, cx1_, cy1_;
    std::vector<Rgb565> px_;
};

} // namespace mal
