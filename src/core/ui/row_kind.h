// row_kind.h: what a row of a list screen is, shared by every list that has headings.
//
// A row is an entry the cursor can land on, a heading that names the group under it, or
// a notice: a heading that tells the player something they must act on, drawn in full
// INK. One value rather than a header flag and a notice flag, so a row cannot claim to be
// a notice without being a heading, and no reader has to know which pairs are legal.
#pragma once

#include <cstdint>

namespace mal {

enum class RowKind : uint8_t { Item, Header, Notice };

// Headers and notices are both headings: drawn as a rule, never selectable, and the
// first row of the group under them.
inline bool isHeading(RowKind k) { return k != RowKind::Item; }

}  // namespace mal
