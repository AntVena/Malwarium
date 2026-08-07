// font_glyphs.h — FONT_UI's glyph tables, as one lookup.
//
// The tables themselves are generated (font_glyphs.cpp, tools/gen_font.py) from
// the TTF in assets/fonts/. This header is the whole seam between them and the
// renderer in font.cpp: swap the face, re-run the generator, and nothing that
// DRAWS text has to know.
#pragma once

#include <cstdint>

namespace mal {

// Which cut to read. A face is a table, never a metric: Bold is the regular
// table smeared a column right, so it shares kFontW / kFontH / kFontAdvance and
// costs a layout nothing to switch to. Reserve it for the one thing on a screen
// that outranks the rest — emphasis stops meaning anything once two things claim
// it. Order matches the generated face lookup.
enum class FontFace { Regular, Bold };

// The kFontH rows of `c` in `face`, bit(kFontW-1) = leftmost column; nullptr for
// a glyph the face has no cell for (drawn as blank). Lowercase is folded to
// uppercase — see the note on the definition.
const uint8_t* fontGlyph(char c, FontFace face = FontFace::Regular);

}  // namespace mal
