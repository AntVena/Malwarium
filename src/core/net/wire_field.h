// wire_field.h — fixed-width ASCII string slots, shared by the radio wire formats.
//
// Both peer_link.h (the HELLO beacon) and pvp_link.h (the duel handshake) lay their
// strings out the same way: a fixed-width slot, NUL-padded, written and read byte by
// byte so host and device compilers can never disagree about struct padding. These
// two helpers are that shared convention; use them for any new frame rather than a
// memcpy, so the trust boundary below stays in one place.
//
// UNTRUSTED INPUT. readWireField is where bytes off the radio become a C string
// destined for the framebuffer and for SD files, so it keeps ONLY printable ASCII.
// Dropping offending bytes (rather than rejecting the frame) means a mangled name
// still yields a usable, if shorter, value instead of losing an otherwise-good frame.
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace mal {

// Copy `src` into a fixed-width wire slot, NUL-padding the remainder. Truncates
// rather than overruns, and the pad means two encodes of the same logical value are
// byte-identical (no stale bytes leaking from a previous longer string).
inline void writeWireField(uint8_t* out, const char* src, size_t cap) {
    std::memset(out, 0, cap);
    if (!src) return;
    size_t n = 0;
    while (n < cap - 1 && src[n] != '\0') {
        out[n] = static_cast<uint8_t>(src[n]);
        ++n;
    }
}

// Pull a fixed-width slot into a C string, keeping only printable ASCII. The result
// is always NUL-terminated and always fits `cap`.
inline void readWireField(const uint8_t* in, char* out, size_t cap) {
    size_t w = 0;
    for (size_t i = 0; i < cap - 1; ++i) {
        const uint8_t c = in[i];
        if (c == 0) break;
        if (c >= 0x20 && c < 0x7F) out[w++] = static_cast<char>(c);
    }
    out[w] = '\0';
}

}  // namespace mal
