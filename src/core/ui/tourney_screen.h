// tourney_screen.h — ROCK THE DOCK's arena screen: the shape of the bracket, and
// the harbour it is fought on.
//
// The run's rules, its state machine and its copy are elsewhere
// (core/model/tournament.h, game_tourney.cpp, content_tournament.h). What lives here
// is the part that is purely a PICTURE of them: where each entrant's row sits, where
// the tree's ties attach, whether the foot has room to stand two fighters up, and the
// dock itself.
//
// THE FIELD IS THE TREE. Eight rows in bracket order, one per original slot, with the
// ties drawn in a gutter to their left: round 0 in the column nearest the names, each
// later round one column further out, so the tree grows toward the edge exactly as a
// printed bracket does. A tie attaches at its block's ANCHOR — the midpoint of its two
// halves' anchors, recursively — which is what makes the drawn tree agree with the
// pairing maths instead of approximating it.
//
// AND IT EMPTIES. A knocked-out entrant does not keep a full row: it collapses to a
// stub, and the field shortens every time somebody goes out. That is the whole reason
// the foot can hold two 56x48 fighters by the time the operator reaches a semi-final —
// the room is paid for by the entrants who are no longer in the draw. The FOCUSED row
// is the one exception: it always stands at full height, so walking the list can still
// read a name that has been struck out.
//
// THE FOOT IS PINNED, THE FIELD IS NOT. Everything at the bottom — the deck line, the
// fighters' feet, the gesture hint — sits at a fixed y, and the field grows down
// toward it. Fighters therefore always stand on the same plank, which is the only way
// a drawn scene and a variable-height list can share a screen. What varies is the gap
// between them, and the gap is the harbour showing through.
#pragma once

#include <cstdint>

#include "core/content/content_tournament.h"
#include "core/render/canvas.h"   // kActiveW / kActiveH — the foot is pinned to them
#include "core/render/font.h"
#include "core/ui/layout.h"
#include "core/ui/widgets.h"   // kHintBandH — the foot the layout is pinned to

namespace mal {

class Framebuffer;
struct SpriteData;

// --- The field ---------------------------------------------------------------

// The first entrant row, straight under the header band. There is no second context
// band here even though this screen reports state: the round is the band's own right
// label, and how much of the field is left is the FIELD — struck rows are countable,
// so a "n LEFT" readout would be the same answer written twice, at the price of the
// 22 rows that let a Daemon-sized opponent stand at the foot.
constexpr int kDockSlotTop = kRowTop;
// A row still in the draw, and one that is out. The live pitch is tighter than a
// list row anywhere else on the device (layout.h's kRowH) because eight entrants
// have to be readable AT ONCE — the bracket's shape is only legible if none of it
// is scrolled away — and the out pitch is what buys the foot its fighters.
constexpr int kDockLiveRowH = 12;
constexpr int kDockOutRowH = 6;

// The tie gutter: one column per round, the FIRST round nearest the names and each
// later one a column further left, so the tree opens toward the edge as the field
// narrows. It starts inboard of the margin to leave the row cursor its own lane —
// a caret sharing a column with a bracket line is two marks the eye has to separate
// on the one screen that is already dense.
constexpr int kDockTieColW = 5;
constexpr int kDockGutterW = kTourneyRounds * kDockTieColW;
constexpr int kDockTieX0 = kMargin + 2;
constexpr int kDockTextX = kDockTieX0 + kDockGutterW + 3;
// The x of round `r`'s column of ties.
constexpr int dockTieX(int round) {
    return kDockTieX0 + (kTourneyRounds - 1 - round) * kDockTieColW;
}

// The height of one row. `cursor` is the focused slot: a row that is out but focused
// stands at full height anyway, because a name the operator has parked on is a name
// they are asking to read.
inline int dockRowH(uint8_t alive, int slot, int cursor) {
    const bool alive_ = (alive & (1u << slot)) != 0;
    return (alive_ || slot == cursor) ? kDockLiveRowH : kDockOutRowH;
}

// The top of `slot`'s row. Passing kTourneySlots gives the foot of the whole field,
// which is what dockFieldBottom is.
inline int dockRowY(uint8_t alive, int slot, int cursor) {
    int y = kDockSlotTop;
    for (int i = 0; i < slot; ++i) y += dockRowH(alive, i, cursor);
    return y;
}
inline int dockFieldBottom(uint8_t alive, int cursor) {
    return dockRowY(alive, kTourneySlots, cursor);
}

// The vertical centre of `slot`'s row — where its tie arm attaches. A live row centres
// on its TEXT rather than on its box, so an arm meets the middle of the name it names.
inline int dockRowMidY(uint8_t alive, int slot, int cursor) {
    const int h = dockRowH(alive, slot, cursor);
    return dockRowY(alive, slot, cursor) + (h == kDockOutRowH ? h / 2 : kFontH / 2);
}

// Where a block's tie attaches: a single slot answers with its own row centre, and a
// block with the midpoint of its two halves. This is the whole bracket-drawing rule,
// and it is recursive for the same reason the bracket is — a semi-final's arm has to
// land between the two quarter-finals that feed it, wherever those have drifted to.
inline int dockBlockAnchorY(uint8_t alive, int start, int size, int cursor) {
    if (size <= 1) return dockRowMidY(alive, start, cursor);
    const int half = size / 2;
    return (dockBlockAnchorY(alive, start, half, cursor) +
            dockBlockAnchorY(alive, start + half, half, cursor)) / 2;
}

// --- The foot ----------------------------------------------------------------

// A fighter is a creature cell drawn at 1/1, like the combat stage's — the arena
// previews the bout in the bout's own scale, so the operator is looking at the same
// silhouette they are about to see swing. It is SEATED, not boxed: a cell runs
// anywhere from 56x48 to the 128x64 ceiling (assets/README.md), so the drawing is
// bottom-anchored on the plank and right-anchored on its own content band, and what
// gives is the width of the column of copy beside it.
constexpr int kDockCellH = 64;   // the tallest cell a creature may ship at
// The plank the fighters stand on, and the deck surface just behind their feet.
constexpr int kDockFeetY = 188;
constexpr int kDockDeckY = 184;
// The highest a fighter's head can reach, which is what the field has to clear.
constexpr int kDockFighterY = kDockFeetY - kDockCellH;
// The two gestures that are extras (a hold, a chord) sit as a dim line ABOVE the hint
// band rather than as more words inside it — the same split the ITEMS list uses.
constexpr int kDockGestureY = 192;

// The copy beside the fighter: four lines from the left margin, in whatever width the
// drawing leaves. The WIDTH is measured rather than fixed, because a cell's width is
// the creature's business — and every line travels if it overruns (drawTextMarquee),
// so a wide opponent costs legibility rather than costing the layout.
//
// Top-anchored to the fighter's own band, and tighter than layout.h's kLineH, for one
// reason: what the block does NOT reach down to is the only harbour this screen gets.
// A 224px canvas holding a list of eight, a 96px creature and four lines of copy has
// about twenty rows left over, and they are spent on the waterline.
constexpr int kDockCardLines = 4;
constexpr int kDockCardLineH = kFontH + 3;
constexpr int kDockCardTop = kDockFighterY;
constexpr int kDockCardBottom = kDockCardTop + (kDockCardLines - 1) * kDockCardLineH +
                                kFontH;
// Where the same block goes with NOBODY standing beside it — the two verdicts, and a
// field too full to seat a fighter. Nothing to align with, so it sits at the foot
// instead of at the top of a band that is not there.
constexpr int kDockTextCardTop =
    kDockGestureY - 4 - kDockCardLines * kDockCardLineH;

static_assert(kDockTextCardTop + (kDockCardLines - 1) * kDockCardLineH + kFontH <=
                  kDockGestureY,
              "the fallback card must clear the gesture hint line");
static_assert(kDockGestureY + kFontH <= kActiveH - kHintBandH,
              "the gesture hint line must clear the hint band");
static_assert(kDockSlotTop + kTourneySlots * kDockLiveRowH + 6 <= kDockTextCardTop,
              "a field with nobody out yet must still clear the card it falls back to");
static_assert(kDockCardBottom < kDockDeckY,
              "the fighter's copy must clear the deck it is standing on");

// The field's bottom with the cursor parked on whichever row makes it TALLEST. The
// foot is laid out against THIS rather than against where the cursor actually is, so
// stepping the list never makes the card below it change shape underfoot.
int dockFieldBottomMax(uint8_t alive);

// Is there room under the field to stand a fighter up? Answered against the TALLEST
// cell rather than against the one about to be drawn, so which opponent the bracket
// deals never decides what shape the screen is. False falls back to the text card,
// which says the same things in the same order and simply has no face.
bool dockFaceoffFits(uint8_t alive);

// Where a fighter's drawing goes: right-anchored so its CONTENT band ends on the
// right margin, and bottom-anchored so its feet land on the plank. Seating by the
// content band rather than by the cell is what stops a creature padded to one side
// of its sheet standing that padding's width off its own mark.
int dockSeatX(const SpriteData& s, bool mirror);
int dockSeatY(const SpriteData& s);

// The width left for the copy beside a seated fighter — the gap from the left margin
// to the near edge of its drawing. Answers the full width when there is no drawing.
int dockCardW(const SpriteData* s, bool mirror);

// --- The tree ------------------------------------------------------------------

// The ties, drawn behind the rows: one bracket per live pairing per round, each
// attached at its block's anchor. `round` is the round being fought — its ties draw
// in `ink` and every other round's in `ink-dim`, so the pairings being settled RIGHT
// NOW are the ones the eye lands on. `playerSlot`'s own path through the tree is
// drawn a pixel thicker, which is the operator's road to the final stated as a shape
// rather than as a second use of the focus colour.
void drawDockTies(Framebuffer& fb, uint8_t alive, int cursor, int round,
                  int playerSlot);

// --- The two readers -----------------------------------------------------------

// SCOUT and BRIEF share the band but not their top: the scout sheet seats a portrait
// of whoever it describes under the band, and the briefing has no picture to seat.
// Both the flow that DRAWS a page and the engine that steps its scroll read these, so
// what the reader sees and what B pages past can never be two different pages
// (ui/prose_page.h's proseRowsFitting).
constexpr int kDockSubY = 30;
constexpr int kDockBriefTop = 46;
// The scout portrait is bottom-anchored on this line for the same reason the fighter
// is bottom-anchored on the plank: a cell's height is the creature's business, and a
// portrait that started at a fixed top would leave a different gap under every one.
constexpr int kDockPortraitFootY = kDockSubY + kDockCellH;
constexpr int kDockScoutTop = kDockPortraitFootY + 12;

// --- The harbour --------------------------------------------------------------

// The backdrop: night over the Bayou, the far shore, the water, and the dock the
// bracket is fought on. Clears the canvas — this is the screen's BACKGROUND pass
// (core/render/RENDER_PIPELINE.md), so it runs before the band and everything else
// composes onto it.
//
// Composed out of core/render/scene.h's primitives against a SceneGround whose FLOOR is
// this header's own kDockDeckY — the screen knows where a fighter's feet go, and the
// scene is told. That header carries the rest of the reasoning, including why a
// backdrop behind text is drawn from palette tokens rather than shipped as a sheet.
void drawDockScene(Framebuffer& fb, int beat);

}  // namespace mal
