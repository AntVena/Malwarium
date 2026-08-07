// font_glyphs.h — FONT_UI's glyph table, as one lookup.
//
// The table itself is generated (font_glyphs.cpp, tools/gen_font.py) from the
// TTF in assets/fonts/. This header is the whole seam between it and the
// renderer in font.cpp: swap the face, re-run the generator, and nothing that
// DRAWS text has to know.
#pragma once

#include <cstdint>

namespace mal {

// The kFontH rows of `c`, bit(kFontW-1) = leftmost column; nullptr for a glyph
// the face has no cell for (drawn as blank). Lowercase is folded to uppercase —
// see the note on the definition.
const uint8_t* fontGlyph(char c);

}  // namespace mal
