// widgets.h — reusable engine-drawn UI primitives (no art files).
//   UI_SUBMENU_HEADER   title + optional right label + divider rule
//   UI_HINT_BAND     centred control hint in a filled strip at the canvas foot
//   UI_GAUGE         10-cell segmented bar (dual-coded: fill level + zone colour)
//   UI_CARE_PIPS     2 Good + gate + 3 Bad mistake budget
//   UI_STAGE_INDICATOR  4 lifecycle nodes
#pragma once

#include "core/content/defs.h"
#include "core/content/effect_text.h"   // SpecRow — drawSpecGrid's input
#include "core/model/pet_model.h"
#include "core/render/color.h"
#include "core/render/font_glyphs.h"   // FontFace — drawTextMarquee's cut
#include "core/render/palette.h"   // palColor — drawHeaderBand's default right colour

namespace mal {

class Framebuffer;

// UI_SUBMENU_HEADER: the band every list and viewer screen opens with — `title`
// at the left margin, `right` (optional) right-aligned opposite it, and the
// divider rule under both, all on layout.h's grid.
//
// It clears the canvas to PAPER first, because opening a screen with this band IS
// what every list and viewer does, and thirty copies of the same clear is how the
// per-screen header helpers drifted apart. drawHeaderBandOver below is the band
// WITHOUT that clear, for the screen that has already painted something under it —
// ROCK THE DOCK's dock scene (ui/tourney_screen.h) is the one, and the two share a
// body so the title can never sit at a different height on a screen with a backdrop.
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

// The same band drawn ONTO whatever is already on the canvas. Same title, same
// right label, same rule, same grid — only the clear is missing.
void drawHeaderBandOver(Framebuffer& fb, const char* title,
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
//
// `face` rides along because a row's NAME is often both the heading of its row and the
// thing too long for it — a title-weight label that overflows still has to travel, and
// dropping to Regular to get that is a weight change nobody asked for.
void drawTextMarquee(Framebuffer& fb, int x, int y, int w, const char* s,
                     Rgb565 color, int beat, bool scroll,
                     FontFace face = FontFace::Regular);

// A one-line label/value pair: `value` right-aligned against the right margin, `label`
// running from `x` and yielding to it — scrolling when `scroll`, clipped otherwise.
//
// Nearly every readout and list row on the device is this shape, and drawn as two
// independent drawText calls it fails the same way every time: the label grows, the
// two overlap, and the row still "renders". Going through here makes the gap a
// property of the pair instead of an assumption about the copy.
void drawLabelValue(Framebuffer& fb, int x, int y, const char* label, Rgb565 labelCol,
                    const char* value, Rgb565 valueCol, int beat, bool scroll);

// --- The fragmentation dither -----------------------------------------------
//
// Fragmentation is the one stat drawn as something still HAPPENING rather than as
// a level: its pixels are rolled fresh from `beat`, so the pattern reshuffles every
// repaint and the disk reads as actively garbling itself. Both screens that show
// fragmentation go through these two calls so they say it the same way — the FRAG
// gauge's frontier cell (drawGauge below) and Disk Decryption's played rows
// (decryption_screen.h). Anything else that grows a fragmentation read should join
// them here rather than rolling its own dither.
//
// Re-rolling is not decoration. Rolls collide, so the sub-cells lit by ONE roll are a
// sample of the damage rather than a count of it — 4 rolls land as anywhere from 1 to
// 4 blocks. Held still, a screen shows the player a single draw and invites them to
// read it as the number; re-rolled per beat, the samples average back to the true
// figure over the frames they watch.

// Fills `lit` with `draws` rolls into a 3x3 grid, from `seed`. Rolls collide, so
// fewer than `draws` sub-cells light: the shortfall is RETURNED as the collision
// debt, which the gauge spends bleeding holes backward into the cell behind the
// frontier. Salt `seed` per site (cell index, row) or neighbouring dithers shuffle
// in lockstep and read as one flashing block rather than as noise.
int fragDitherPattern(bool lit[9], int draws, uint32_t seed);

// Paints a 3x3 sub-grid of `c` inside (x,y,w,h) per `lit` (row-major), absorbing
// any rounding at the far edge so the sub-cells tile the box exactly.
void drawSubGrid(Framebuffer& fb, int x, int y, int w, int h, const bool lit[9],
                 Rgb565 c);

// Segmented 10-cell gauge in the box (x,y,w,h). `value` 0..100 sets lit cells
// (floor(value/10)). Vitality/Hazard polarity is already baked into `zone`.
// fragRamp=true tints lit cells purple->pink by position instead of by zone.
// pulseOn drives the ~1Hz Critical pulse (caller supplies the phase).
// When fragRamp is set, the frontier cell (the leftover value % 10) breaks
// into a 3x3 sub-grid seeded from `beat`, and a collision "debt" from that
// roll can bleed backward into the previous solid cell as a few dark holes —
// Fragmentation's glitchy, garbled edge instead of a hard stop. Ignored when
// fragRamp is false.
// `fill`, when given, replaces the zone's colour for the lit cells — for a gauge whose
// quantity carries no danger meaning of its own and so has no zone to take one from.
// The combat screen's rival Health is the case: the same amount of it is good news or
// bad depending on which side of the fight you are reading it from, so it is drawn in a
// neutral ink while the pet's own gauge beneath keeps the danger ramp. The SHAPE channel
// is untouched either way, which is what keeps both readable in grayscale.
void drawGauge(Framebuffer& fb, int x, int y, int w, int h, int value,
               Zone zone, bool fragRamp, bool pulseOn, int beat = 0,
               const Rgb565* fill = nullptr);

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

// UI_HINT_BAND: the control hint, centred in a filled strip pinned to the foot of
// the canvas. The fill is what makes it a BAND rather than one more row — a hint
// set as bare text a row-pitch under a list reads as another entry in the list, and
// dimming it is no help to a reader who is scanning shape rather than words.
//
// Pinned to the bottom rather than following the content, so it lands in the same
// place whether the list above it is full or half empty. Consumers: the list and
// picker screens that end in one (EXPL, SHOP, TITLE).
//
// The height is named because a screen that FLOWS its content (STAT's prose pages
// size each row to the text it holds) has to subtract the band from the room it
// has before it lays anything out — the band is drawn last but occupies its strip
// from the start, and a page that discovers it after filling the screen has
// already overprinted it.
constexpr int kHintBandH = 16;
void drawHintBand(Framebuffer& fb, const char* hint);

// The derived readout (effect_text.h's specRows) as an aligned grid: label left,
// magnitude RIGHT-aligned in its column, so two rows of a list line their numbers
// up and become comparable at a glance — which a " / "-joined sentence never is.
//
// Packs two cells per line where both clear a column, and gives a wide pair or a
// flag row (SpecRow::flag — a behaviour with no magnitude) the full width. Returns
// the y just past the last line. It never truncates: the caller is responsible for
// handing it enough room, which is what gridLines() below is for.
//
// Consumers: the MODS/ITEMS/MOVES detail + picker panels (spec_sheet.h).
int drawSpecGrid(Framebuffer& fb, int x, int y, int w, const SpecRow* rows, int n,
                 int lineH, Rgb565 labelColor, Rgb565 valueColor);

// How many lines drawSpecGrid will take for the same arguments — so a screen can
// reserve exactly that and lay out whatever follows, without drawing twice.
int gridLines(int w, const SpecRow* rows, int n);

} // namespace mal
