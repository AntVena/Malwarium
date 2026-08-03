// color.h — RGB565 helpers shared across the render layer.
#pragma once

#include <cstdint>

namespace mal {

using Rgb565 = uint16_t;

constexpr Rgb565 rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return static_cast<Rgb565>(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

// Unpack to 8-bit channels (expanded back to the full 0..255 range).
constexpr uint8_t r8(Rgb565 c) { uint8_t v = (c >> 11) & 0x1F; return (v << 3) | (v >> 2); }
constexpr uint8_t g8(Rgb565 c) { uint8_t v = (c >> 5) & 0x3F; return (v << 2) | (v >> 4); }
constexpr uint8_t b8(Rgb565 c) { uint8_t v = c & 0x1F;        return (v << 3) | (v >> 2); }

// Perceptual luminance 0..1 (Rec.709). Used by the grayscale-readability gate.
inline float luminance(Rgb565 c) {
    return (0.2126f * r8(c) + 0.7152f * g8(c) + 0.0722f * b8(c)) / 255.0f;
}

// Alpha-blend src over dst with 8-bit coverage.
inline Rgb565 blend(Rgb565 dst, Rgb565 src, uint8_t a) {
    if (a == 0) return dst;
    if (a == 255) return src;
    int ia = 255 - a;
    int r = (r8(src) * a + r8(dst) * ia) / 255;
    int g = (g8(src) * a + g8(dst) * ia) / 255;
    int b = (b8(src) * a + b8(dst) * ia) / 255;
    return rgb565(static_cast<uint8_t>(r), static_cast<uint8_t>(g),
                  static_cast<uint8_t>(b));
}

} // namespace mal
