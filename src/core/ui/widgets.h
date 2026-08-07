// widgets.h — reusable engine-drawn UI primitives (no art files).
//   UI_SUBMENU_HEADER   title + optional right label + divider rule
//   UI_GAUGE         10-cell segmented bar (dual-coded: fill level + zone colour)
//   UI_CARE_PIPS     2 Good + gate + 3 Bad mistake budget
//   UI_STAGE_INDICATOR  4 lifecycle nodes
#pragma once

#include "core/content/defs.h"
#include "core/content/effect_text.h"   // SpecRow — drawSpecGrid's input
#include "core/model/pet_model.h"
#include "core/render/color.h"
#include "core/render/palette.h"   // palColor — drawHeaderBand's default right colour

namespace mal {

class Framebuffer;

// UI_SUBMENU_HEADER: the band every list and viewer screen opens with — `title`
// at the left margin, `right` (optional) right-aligned opposite it, and the
// divider rule under both, all on layout.h's grid.
//
// It clears the canvas to PAPER first, because opening a screen with this band IS
// what every list and viewer does, and thirty copies of the same clear is how the
// per-screen header helpers drifted apart. A screen that wants a backdrop under
// its band is the growth point that splits the clear back out.
//
// Both colours are the caller's because what a label MEANS varies — ARCH's slot
// count is secondary information (INK_DIM), SHOP's Bits wallet is the thing the
// screen is about (ACCENT), the cache screens tint by rarity, and the wild
// encounter's title is the alarm itself (WARN). Only ever emphasis: the words are
// already there, so the band survives the grayscale gate either way.
void drawHeaderBand(Framebuffer& fb, const char* title,
                    const char* right = nullptr,
                    Rgb565 rightColor = palColor(Pal::INK_DIM),
                    Rgb565 titleColor = palColor(Pal::INK));

// Text in a column narrower than it needs. Draws `s` at (x,y) clipped to `w`; if
// it fits, this is exactly drawText and costs nothing extra.
//
// When it doesn't fit and `scroll` is set, the line travels: it holds at the head
// long enough to read, walks to the tail, holds again, and snaps back — driven by
// `beat`, so it advances on the ~4fps heartbeat like every other motion on the
// device rather than on a timer of its own.
//
// Only the FOCUSED row of a list should scroll. An unfocused overflowing row is
// clipped instead: a screen of lines all travelling at once is unreadable, and
// the cursor is already the thing that says which row the player is asking about.
// The 5x7-era ellipsis is not available — the glyph table is ASCII, which the
// content tests enforce — so the clip is the truncation cue.
void drawTextMarquee(Framebuffer& fb, int x, int y, int w, const char* s,
                     Rgb565 color, int beat, bool scroll);

// Segmented 10-cell gauge in the box (x,y,w,h). `value` 0..100 sets lit cells
// (floor(value/10)). Vitality/Hazard polarity is already baked into `zone`.
// fragRamp=true tints lit cells purple->pink by position instead of by zone.
// pulseOn drives the ~1Hz Critical pulse (caller supplies the phase).
// When fragRamp is set, the frontier cell (the leftover value % 10) breaks
// into a 3x3 sub-grid seeded from `beat`, and a collision "debt" from that
// roll can bleed backward into the previous solid cell as a few dark holes —
// Fragmentation's glitchy, garbled edge instead of a hard stop. Ignored when
// fragRamp is false.
void drawGauge(Framebuffer& fb, int x, int y, int w, int h, int value,
               Zone zone, bool fragRamp, bool pulseOn, int beat = 0);

// Care-mistake budget: 2 Good pips, a gate divider, 3 Bad pips. `mistakes` 0..5.
void drawCarePips(Framebuffer& fb, int x, int y, int mistakes, bool pulseOn);

// 4 lifecycle nodes; `current` is bright + named, past lit, future dim.
void drawStageIndicator(Framebuffer& fb, int x, int y, int w, Stage current);

// UI_PROGRESS_BAR: a single "fills up / counts down" bar, sharing the
// gauge visual language. `t` 0..1 sets the fill; `c` is the fill colour. The
// frame + remaining track read in grayscale (shape channel), so a partial bar
// is legible without colour. Reused by feeding, MAINT processes, the Lockout
// countdown, etc.
// churn=true dithers a few ink-white flecks into the fill's leading edge,
// reseeded per `beat` so they shimmer in place — the same "can't show past its
// own cap" cue as the Fragmentation frontier glitch (drawGauge above), for a
// bar whose backing value can exceed 100% of what it can render (e.g. a
// shield pool bigger than max health).
void drawProgressBar(Framebuffer& fb, int x, int y, int w, int h, float t,
                     Rgb565 c, bool churn = false, int beat = 0);

// UI_CURSOR_ROW: a 5x7 right-pointing triangle marker for a focused list row
// (the 5x7 font has no '>'). Left-anchored, tapering to a point at the right.
void drawRowCursor(Framebuffer& fb, int x, int y, Rgb565 c);

// The derived readout (effect_text.h's specRows) as an aligned grid: label left,
// magnitude RIGHT-aligned in its column, so two rows of a list line their numbers
// up and become comparable at a glance — which a " / "-joined sentence never is.
//
// Packs two cells per line where both clear a column, and gives a wide pair or a
// flag row (SpecRow::flag — a behaviour with no magnitude) the full width. Returns
// the y just past the last line. It never truncates: the caller is responsible for
// handing it enough room, which is what gridLines() below is for.
//
// Consumers: the MODS/ITEMS/TRAIN detail + picker panels (spec_sheet.h).
int drawSpecGrid(Framebuffer& fb, int x, int y, int w, const SpecRow* rows, int n,
                 int lineH, Rgb565 labelColor, Rgb565 valueColor);

// How many lines drawSpecGrid will take for the same arguments — so a screen can
// reserve exactly that and lay out whatever follows, without drawing twice.
int gridLines(int w, const SpecRow* rows, int n);

} // namespace mal
