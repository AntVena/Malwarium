// layout.h — the UI layout grid: the insets and vertical zones every screen
// lays out against.
//
// assets/VISUAL_LANGUAGE.md §4 describes this grid in prose for whoever is
// drawing art against it; this header is what the code reads. They are one
// decision written twice, and these numbers are the ones that ship.
//
// A screen that wants a different value gives it a NAME OF ITS OWN
// (expl_screen's kTrackRowTop, items_screen's kPickRowH) rather than redeclaring
// one of these in a file-private namespace — that is what keeps "the grid" a
// single answer and a deviation a visible, named choice. Redeclaring one at
// namespace scope inside `mal` is an ambiguity the compiler catches. Beware the
// one hole it does not: a `constexpr int kMargin` declared inside a FUNCTION
// shadows silently.
//
// Consumers: every src/core/ui/*_screen.cpp and every game_*.cpp unit that draws
// a list. The band itself is drawn by widgets.h's drawHeaderBand.
#pragma once

#include "core/render/font.h"  // kFontH — kLineH is derived from it

namespace mal {

// --- Insets ---------------------------------------------------------------

// Text inset from the active canvas edge, and its mirror: a right-aligned label
// sits at kActiveW - kMargin - textWidth(s). One value serves both faces, so a
// pet submenu and a Hacker sub-screen line their text up on the same column.
constexpr int kMargin = 8;

// --- The header band ------------------------------------------------------
//
// Every list and viewer screen opens with the same band: a title on the left at
// kTitleY, an optional right-aligned label opposite it, and a divider rule.

constexpr int kTitleY = 6;
constexpr int kHeaderRule = 22;

// --- List rows ------------------------------------------------------------

constexpr int kRowTop = 26;   // first row, immediately under the header rule
constexpr int kRowH = 28;     // row pitch
constexpr int kRowIcon = 20;  // the 20x20 icon tier (VISUAL_LANGUAGE §3.1)

// Rows that fit between kRowTop and the bottom hint band. A list longer than
// this scrolls rather than growing the screen; the scrollbar geometry in
// items/mods/cfg is all derived from it.
constexpr int kVisibleRows = 6;

// Text line pitch for a block of prose or a stacked multi-line row — one glyph
// height plus leading.
constexpr int kLineH = kFontH + 5;

// --- The Hacker face's second band ----------------------------------------
//
// The Hacker sub-screens (CREW, PEERS, LINK, PROFILE) carry a context line under
// the title — who owns the radio, which network is home — and rule it off before
// their own content starts. It is a real second band rather than a first row:
// the state it reports changes what the rows below it MEAN, so it reads as
// chrome. The pet submenus have no equivalent and start straight at kRowTop.

constexpr int kContextY = 28;
constexpr int kContextRule = 44;

}  // namespace mal
