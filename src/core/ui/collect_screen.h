// collect_screen.h — STAT's two COLLECTION pages: what this pet has tasted, and what it
// has learned.
//
// Both answer the same question about a different roster — "how much of this have I got,
// and what is left" — and both are per-PET. That is the whole point of them: the device's
// own lifetime tallies (every item ever held, every species raised) live on the Hacker
// PROFILE and in the web 'Pedia, and they say nothing about the creature in front of you.
// A pet is raised one plate and one drop at a time, and until now the only place that
// showed was a website you had to be on the same Wi-Fi to read.
//
//   FOODS   a grid of every dish in the game, in rarity sections, with the ones THIS pet
//           has eaten drawn bright and the rest dimmed. The shapes are already familiar
//           from the ITEMS bag, so a gap in the grid is a dish you can go and find.
//   MOVES   every move this pet could ever learn (the line rule, moveAllowedForLine),
//           its own line's first and the shared pool after, each tagged KNOWN or not.
//
// The MOVES page is built as the same flowed prose rows LOADOUT and TIERS use
// (prose_page.h) with no body text — a name and a tag is exactly one of those rows — so
// it inherits the section windows, the scrollbar and STAT's index anchors for free.
#pragma once

#include <vector>

#include "core/content/defs.h"
#include "core/ui/prose_page.h"

namespace mal {

class Framebuffer;
class ContentRegistry;
class MoveLoadout;

// Cells per grid row. Set by the 20px icon tier against the active canvas at the
// margins the rest of the device uses, not chosen for looks.
inline constexpr int kFoodCols = 8;

// One ROW of the FOODS page: either a rarity SECTION heading, or up to kFoodCols dishes.
//
// A row rather than a cell is the unit because that is what the page scrolls by, exactly
// as the flowed pages scroll by prose rows — and it is what lets a section heading end a
// window the way a ProseRow header does. A grid of two hundred anonymous glyphs is a
// wall; "8/9 COMMON, 0/6 EPIC" over each block of it is a plan.
struct FoodRow {
    const char* section = nullptr;   // non-null = a heading row, and `cells` is empty
    int have = 0;                    // heading rows: tasted / total in that group
    int total = 0;
    const ItemDef* cells[kFoodCols] = {};
    bool eaten[kFoodCols] = {};
    int count = 0;                   // cells used in this row
};

// Every Food in the roster, laid into rows, one rarity group at a time. `eatenSet` is the
// pet's palate (Game::petFoodsEaten_ — borrowed registry pointers), passed in rather than
// asked for, so this page has no opinion about where a palate is kept or how it persists.
//
// Rarity order rather than roster order because rarity is what tells a player whether a
// gap is a shopping trip or a campaign: the commons are a kitchen you can fill
// deliberately, and the epics are the ones a dive or a boss has to hand you.
std::vector<FoodRow> buildFoodRows(const ContentRegistry& reg,
                                   const std::vector<const ItemDef*>& eatenSet);

// How many rows starting at `top` fit one screen, stopping at the next section for the
// reason the prose flow does (prose_page.h).
int foodRowsFitting(const std::vector<FoodRow>& rows, int top);

// The page's window count and which one `scrollTop` opens, for the hint band's "n/m".
int foodWindowCount(const std::vector<FoodRow>& rows);
int foodWindowIndex(const std::vector<FoodRow>& rows, int scrollTop);

// STAT's FOODS page: the windowed grid from buildFoodRows, tasted cells at full ink and
// the rest dimmed. `eaten`/`total` head the page with the score.
void drawFoodsScreen(Framebuffer& fb, const ContentRegistry& reg,
                     const std::vector<FoodRow>& rows, int scrollTop, int eaten,
                     int total, int beat);

// STAT's MOVES page row model: every move `petLine` can hold, its own line's section
// first and the generic pool second, tagged KNOWN where `owned` says the pet has it.
// Moves gated to a later stage are still listed — a move you cannot field yet is exactly
// the kind of thing this page exists to point at — and carry the stage as their tag
// instead, so the page never promises one the pet could field today.
std::vector<ProseRow> buildMoveDexRows(const ContentRegistry& reg,
                                       const MoveLoadout& moves, const char* petLine,
                                       Stage stage);

// The MOVES page's window, the shared flow's own count at this page's row top.
int moveDexRowsFitting(const std::vector<ProseRow>& rows, int top);

// STAT's MOVES page: the windowed roster from buildMoveDexRows. `known`/`total` head the
// page with the score, the way FOODS does.
void drawMoveDexScreen(Framebuffer& fb, const std::vector<ProseRow>& rows, int scrollTop,
                       int known, int total, int beat);

} // namespace mal
