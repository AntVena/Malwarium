// ascii.h: upper-casing for the two places names are spelled in capitals. FONT_UI draws
// only upper-case letters, and the atlas names every sheet in them, so a lower-case id
// is raised before it is shown or looked up.
#pragma once

#include <cstddef>

namespace mal {

constexpr char asciiUpper(char c) {
    return (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c;
}

// Copy `s` into `out` (at most outSize - 1 characters) upper-cased, always terminated.
inline void asciiUpperCopy(char* out, size_t outSize, const char* s) {
    if (outSize == 0) return;
    size_t i = 0;
    for (const char* p = s; p && *p && i + 1 < outSize; ++p, ++i) out[i] = asciiUpper(*p);
    out[i] = '\0';
}

} // namespace mal
